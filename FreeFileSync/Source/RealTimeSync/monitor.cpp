// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "monitor.h"
#include <zen/file_access.h>
#include <zen/dir_watcher.h>
#include <zen/thread.h>
#include "../base/resolve_path.h"
//#include "../library/db_file.h"     //SYNC_DB_FILE_ENDING -> complete file too much of a dependency; file ending too little to decouple into single header
//#include "../library/lock_holder.h" //LOCK_FILE_ENDING
//TEMP_FILE_ENDING

using namespace zen;


namespace
{
const std::chrono::seconds FOLDER_EXISTENCE_CHECK_INTERVAL(1);


std::set<Zstring, LessFilePath> getFormattedDirs(const std::vector<Zstring>& folderPathPhrases) //throw FileError
{
    std::set<Zstring, LessFilePath> folderPaths; //make unique

    for (const Zstring& phrase : folderPathPhrases)
    {
        //hopefully clear enough now: https://freefilesync.org/forum/viewtopic.php?t=4302
        auto checkProtocol = [&](const Zstring& protoName)
        {
            if (startsWith(trimCpy(phrase), protoName + Zstr(":"), CmpAsciiNoCase()))
                throw FileError(replaceCpy(_("The %x protocol does not support directory monitoring:"), L"%x", utfTo<std::wstring>(protoName)) + L"\n\n" + fmtPath(phrase));
        };
        checkProtocol(Zstr("FTP"));  //
        checkProtocol(Zstr("SFTP")); //throw FileError
        checkProtocol(Zstr("MTP"));  //

        //make unique: no need to resolve duplicate phrases more than once! (consider "[volume name]" syntax) -> shouldn't this be already buffered by OS?
        folderPaths.insert(fff::getResolvedFilePath(phrase));
    }

    return folderPaths;
}


//wait until all directories become available (again) + logs in network share
std::set<Zstring, LessFilePath> waitForMissingDirs(const std::vector<Zstring>& folderPathPhrases, //throw FileError
                                                   const std::function<void(const Zstring& folderPath)>& requestUiRefresh, std::chrono::milliseconds cbInterval)
{
    for (;;)
    {
        //support specifying volume by name => call getResolvedFilePath() repeatedly
        std::set<Zstring, LessFilePath> folderPaths = getFormattedDirs(folderPathPhrases); //throw FileError

        std::vector<std::pair<Zstring, std::future<bool>>> futureInfo;
        //start all folder checks asynchronously (non-existent network path may block)
        for (const Zstring& folderPath : folderPaths)
            futureInfo.emplace_back(folderPath, runAsync([folderPath]
        {
            //2. check dir availability
            return dirAvailable(folderPath);
        }));

        bool allAvailable = true;

        for (auto& item : futureInfo)
        {
            const Zstring& folderPath = item.first;
            std::future<bool>& ftDirAvailable = item.second;

            for (;;)
            {
                while (ftDirAvailable.wait_for(cbInterval) != std::future_status::ready)
                    requestUiRefresh(folderPath); //throw X

                if (ftDirAvailable.get())
                    break;

                //wait until folder is available: do not needlessly poll all others again!
                allAvailable = false;

                //wait some time...
                const auto delayUntil = std::chrono::steady_clock::now() + FOLDER_EXISTENCE_CHECK_INTERVAL;
                for (auto now = std::chrono::steady_clock::now(); now < delayUntil; now = std::chrono::steady_clock::now())
                {
                    requestUiRefresh(folderPath); //throw X
                    std::this_thread::sleep_for(cbInterval);
                }

                ftDirAvailable = runAsync([folderPath] { return dirAvailable(folderPath); });
            }
        }
        if (allAvailable) //only return when all folders were found on *first* try!
            return folderPaths;
    }
}


//wait until changes are detected or if a directory is not available (anymore)
struct WaitResult
{
    enum ChangeType
    {
        ITEM_CHANGED,
        FOLDER_UNAVAILABLE //1. not existing or 2. can't access
    };

    explicit WaitResult(const DirWatcher::Entry& changeEntry) : type(ITEM_CHANGED), changedItem(changeEntry) {}
    explicit WaitResult(const Zstring& folderPath) : type(FOLDER_UNAVAILABLE), missingFolderPath(folderPath) {}

    ChangeType type;
    DirWatcher::Entry changedItem; //for type == ITEM_CHANGED: file or directory
    Zstring missingFolderPath;     //for type == FOLDER_UNAVAILABLE
};


WaitResult waitForChanges(const std::set<Zstring, LessFilePath>& folderPaths, //throw FileError
                          const std::function<void(bool readyForSync)>& requestUiRefresh, std::chrono::milliseconds cbInterval)
{
    assert(std::all_of(folderPaths.begin(), folderPaths.end(), [](const Zstring& folderPath) { return dirAvailable(folderPath); }));
    if (folderPaths.empty()) //pathological case, but we have to check else this function will wait endlessly
        throw FileError(_("A folder input field is empty.")); //should have been checked by caller!

    std::vector<std::pair<Zstring, std::unique_ptr<DirWatcher>>> watches;

    for (const Zstring& folderPath : folderPaths)
        try
        {
            watches.emplace_back(folderPath, std::make_unique<DirWatcher>(folderPath)); //throw FileError
        }
        catch (FileError&)
        {
            if (!dirAvailable(folderPath)) //folder not existing or can't access
                return WaitResult(folderPath);
            throw;
        }

    auto lastCheckTime = std::chrono::steady_clock::now();
    for (;;)
    {
        const bool checkDirNow = [&] //checking once per sec should suffice
        {
            const auto now = std::chrono::steady_clock::now();
            if (now > lastCheckTime + FOLDER_EXISTENCE_CHECK_INTERVAL)
            {
                lastCheckTime = now;
                return true;
            }
            return false;
        }();

        for (const auto& item : watches)
        {
            const Zstring& folderPath = item.first;
            DirWatcher& watcher       = *item.second;

            //IMPORTANT CHECK: DirWatcher has problems detecting removal of top watched directories!
            if (checkDirNow)
                if (!dirAvailable(folderPath)) //catch errors related to directory removal, e.g. ERROR_NETNAME_DELETED
                    return WaitResult(folderPath);
            try
            {
                std::vector<DirWatcher::Entry> changedItems = watcher.getChanges([&] { requestUiRefresh(false /*readyForSync*/); /*throw X*/ },
                                                                                 cbInterval); //throw FileError
                erase_if(changedItems, [](const DirWatcher::Entry& e)
                {
                    return
                        endsWith(e.itemPath, Zstr(".ffs_tmp"))  || //sync.8ea2.ffs_tmp
                        endsWith(e.itemPath, Zstr(".ffs_lock")) || //sync.ffs_lock, sync.Del.ffs_lock
                        endsWith(e.itemPath, Zstr(".ffs_db"));     //sync.ffs_db
                    //no need to ignore temporary recycle bin directory: this must be caused by a file deletion anyway
                });

                if (!changedItems.empty())
                    return WaitResult(changedItems[0]); //directory change detected
            }
            catch (FileError&)
            {
                if (!dirAvailable(folderPath)) //a benign(?) race condition with FileError
                    return WaitResult(folderPath);
                throw;
            }
        }

        std::this_thread::sleep_for(cbInterval);
        requestUiRefresh(true /*readyForSync*/); //throw X: may start sync at this presumably idle time
    }
}


inline
std::wstring getActionName(DirWatcher::ActionType type)
{
    switch (type)
    {
        case DirWatcher::ACTION_CREATE:
            return L"CREATE";
        case DirWatcher::ACTION_UPDATE:
            return L"UPDATE";
        case DirWatcher::ACTION_DELETE:
            return L"DELETE";
    }
    assert(false);
    return L"ERROR";
}

struct ExecCommandNowException {};
}


void rts::monitorDirectories(const std::vector<Zstring>& folderPathPhrases, std::chrono::seconds delay,
                             const std::function<void(const Zstring& itemPath, const std::wstring& actionName)>& executeExternalCommand,
                             const std::function<void(const Zstring* missingFolderPath)>& requestUiRefresh,
                             const std::function<void(const std::wstring& msg         )>& reportError,
                             std::chrono::milliseconds cbInterval)
{
    assert(!folderPathPhrases.empty());
    if (folderPathPhrases.empty())
        return;

    for (;;)
        try
        {
            std::set<Zstring, LessFilePath> folderPaths = waitForMissingDirs(folderPathPhrases, [&](const Zstring& folderPath) { requestUiRefresh(&folderPath); }, cbInterval); //throw FileError

            //schedule initial execution (*after* all directories have arrived)
            auto nextExecTime = std::chrono::steady_clock::now() + delay;

            for (;;) //command executions
            {
                DirWatcher::Entry lastChangeDetected;
                try
                {
                    for (;;) //detected changes
                    {
                        const WaitResult res = waitForChanges(folderPaths, [&](bool readyForSync) //throw FileError, ExecCommandNowException
                        {
                            requestUiRefresh(nullptr);

                            if (readyForSync && std::chrono::steady_clock::now() >= nextExecTime)
                                throw ExecCommandNowException(); //abort wait and start sync
                        }, cbInterval);
                        switch (res.type)
                        {
                            case WaitResult::ITEM_CHANGED:
                                lastChangeDetected = res.changedItem;
                                break;

                            case WaitResult::FOLDER_UNAVAILABLE: //don't execute the command before all directories are available!
                                lastChangeDetected = DirWatcher::Entry{ DirWatcher::ACTION_UPDATE, res.missingFolderPath};
                                folderPaths = waitForMissingDirs(folderPathPhrases, [&](const Zstring& folderPath) { requestUiRefresh(&folderPath); }, cbInterval); //throw FileError
                                break;
                        }
                        nextExecTime = std::chrono::steady_clock::now() + delay;
                    }
                }
                catch (ExecCommandNowException&) {}

                executeExternalCommand(lastChangeDetected.itemPath, getActionName(lastChangeDetected.action));
                nextExecTime = std::chrono::steady_clock::time_point::max();
            }
        }
        catch (const FileError& e)
        {
            reportError(e.toString());
        }
}

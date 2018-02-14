// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "monitor.h"
#include <ctime>
#include <set>
#include <zen/file_access.h>
#include <zen/dir_watcher.h>
#include <zen/thread.h>
#include <zen/basic_math.h>
#include <wx/utils.h>
#include "../lib/resolve_path.h"
//#include "../library/db_file.h"     //SYNC_DB_FILE_ENDING -> complete file too much of a dependency; file ending too little to decouple into single header
//#include "../library/lock_holder.h" //LOCK_FILE_ENDING
//TEMP_FILE_ENDING

using namespace zen;


namespace
{
const std::chrono::seconds FOLDER_EXISTENCE_CHECK_INTERVAL(1);


std::vector<Zstring> getFormattedDirs(const std::vector<Zstring>& folderPathPhrases) //throw FileError
{
    std::set<Zstring, LessFilePath> folderPaths; //make unique
    for (const Zstring& phrase : std::set<Zstring, LessFilePath>(folderPathPhrases.begin(), folderPathPhrases.end()))
    {
        //hopefully clear enough now: https://www.freefilesync.org/forum/viewtopic.php?t=4302
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

    return { folderPaths.begin(), folderPaths.end() };
}


//wait until changes are detected or if a directory is not available (anymore)
struct WaitResult
{
    enum ChangeType
    {
        CHANGE_DETECTED,
        CHANGE_DIR_UNAVAILABLE //not existing or can't access
    };

    WaitResult(const zen::DirWatcher::Entry& changedItem) : type(CHANGE_DETECTED), changedItem_(changedItem) {}
    WaitResult(const Zstring& folderPath) : type(CHANGE_DIR_UNAVAILABLE), folderPath_(folderPath) {}

    ChangeType type;
    zen::DirWatcher::Entry changedItem_; //for type == CHANGE_DETECTED: file or directory
    Zstring folderPath_;                 //for type == CHANGE_DIR_UNAVAILABLE
};


WaitResult waitForChanges(const std::vector<Zstring>& folderPathPhrases, //throw FileError
                          const std::function<void(bool readyForSync)>& requestUiRefresh, std::chrono::milliseconds cbInterval)
{
    const std::vector<Zstring> folderPaths = getFormattedDirs(folderPathPhrases); //throw FileError
    if (folderPaths.empty()) //pathological case, but we have to check else this function will wait endlessly
        throw FileError(_("A folder input field is empty.")); //should have been checked by caller!

    //detect when volumes are removed/are not available anymore
    std::vector<std::pair<Zstring, std::shared_ptr<DirWatcher>>> watches;

    for (const Zstring& folderPath : folderPaths)
    {
        try
        {
            //a non-existent network path may block, so check existence asynchronously!
            auto ftDirAvailable = runAsync([=] { return dirAvailable(folderPath); });

            while (ftDirAvailable.wait_for(cbInterval) != std::future_status::ready)
                if (requestUiRefresh) requestUiRefresh(false /*readyForSync*/); //throw X

            if (!ftDirAvailable.get()) //folder not existing or can't access
                return WaitResult(folderPath);

            watches.emplace_back(folderPath, std::make_shared<DirWatcher>(folderPath)); //throw FileError
        }
        catch (FileError&)
        {
            if (!dirAvailable(folderPath)) //folder not existing or can't access
                return WaitResult(folderPath);
            throw;
        }
    }

    auto lastCheckTime = std::chrono::steady_clock::now();
    for (;;)
    {
        const bool checkDirNow = [&]() -> bool //checking once per sec should suffice
        {
            const auto now = std::chrono::steady_clock::now();

            if (numeric::dist(now, lastCheckTime) > FOLDER_EXISTENCE_CHECK_INTERVAL) //handle potential chrono wrap-around!
            {
                lastCheckTime = now;
                return true;
            }
            return false;
        }();


        for (auto it = watches.begin(); it != watches.end(); ++it)
        {
            const Zstring& folderPath = it->first;
            DirWatcher& watcher = *(it->second);

            //IMPORTANT CHECK: dirwatcher has problems detecting removal of top watched directories!
            if (checkDirNow)
                if (!dirAvailable(folderPath)) //catch errors related to directory removal, e.g. ERROR_NETNAME_DELETED
                    return WaitResult(folderPath);
            try
            {
                std::vector<DirWatcher::Entry> changedItems = watcher.getChanges([&] { requestUiRefresh(false /*readyForSync*/); /*throw X*/ },
                                                                                 cbInterval); //throw FileError

                //remove to be ignored changes
                erase_if(changedItems, [](const DirWatcher::Entry& e)
                {
                    return
                        endsWith(e.filePath, Zstr(".ffs_tmp"))  || //sync.8ea2.ffs_tmp
                        endsWith(e.filePath, Zstr(".ffs_lock")) || //sync.ffs_lock, sync.Del.ffs_lock
                        endsWith(e.filePath, Zstr(".ffs_db"));     //sync.ffs_db
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


//wait until all directories become available (again) + logs in network share
void waitForMissingDirs(const std::vector<Zstring>& folderPathPhrases, //throw FileError
                        const std::function<void(const Zstring& folderPath)>& requestUiRefresh, std::chrono::milliseconds cbInterval)
{
    for (;;)
    {
        bool allAvailable = true;
        //support specifying volume by name => call getResolvedFilePath() repeatedly
        for (const Zstring& folderPath : getFormattedDirs(folderPathPhrases)) //throw FileError
        {
            auto ftDirAvailable = runAsync([=]
            {
                //2. check dir availability
                return dirAvailable(folderPath);
            });
            while (ftDirAvailable.wait_for(cbInterval) != std::future_status::ready)
                if (requestUiRefresh) requestUiRefresh(folderPath); //throw X

            if (!ftDirAvailable.get())
            {
                allAvailable = false;
                //wait some time...
                const auto delayUntil = std::chrono::steady_clock::now() + FOLDER_EXISTENCE_CHECK_INTERVAL;
                for (auto now = std::chrono::steady_clock::now(); now < delayUntil; now = std::chrono::steady_clock::now())
                {
                    if (requestUiRefresh) requestUiRefresh(folderPath); //throw X
                    std::this_thread::sleep_for(cbInterval);
                }
                break;
            }
        }
        if (allAvailable)
            return;
    }
}


inline
wxString toString(DirWatcher::ActionType type)
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
    return L"ERROR";
}

struct ExecCommandNowException {};
}


void rts::monitorDirectories(const std::vector<Zstring>& folderPathPhrases, size_t delay, MonitorCallback& cb, std::chrono::milliseconds cbInterval)
{
    if (folderPathPhrases.empty())
    {
        assert(false);
        return;
    }

    auto execMonitoring = [&] //throw FileError
    {
        cb.setPhase(MonitorCallback::MONITOR_PHASE_WAITING);
        waitForMissingDirs(folderPathPhrases, [&](const Zstring& folderPath) { cb.requestUiRefresh(); }, cbInterval); //throw FileError
        cb.setPhase(MonitorCallback::MONITOR_PHASE_ACTIVE);

        //schedule initial execution (*after* all directories have arrived, which could take some time which we don't want to include)
        time_t nextExecTime = std::time(nullptr) + delay;

        for (;;) //loop over command invocations
        {
            DirWatcher::Entry lastChangeDetected;
            try
            {
                for (;;) //loop over detected changes
                {
                    //wait for changes (and for all directories to become available)
                    WaitResult res = waitForChanges(folderPathPhrases, [&](bool readyForSync) //throw FileError, ExecCommandNowException
                    {
                        if (readyForSync)
                            if (nextExecTime <= std::time(nullptr))
                                throw ExecCommandNowException(); //abort wait and start sync
                        cb.requestUiRefresh();
                    }, cbInterval);
                    switch (res.type)
                    {
                        case WaitResult::CHANGE_DIR_UNAVAILABLE: //don't execute the command before all directories are available!
                            cb.setPhase(MonitorCallback::MONITOR_PHASE_WAITING);
                            waitForMissingDirs(folderPathPhrases, [&](const Zstring& folderPath) { cb.requestUiRefresh(); }, cbInterval); //throw FileError
                            cb.setPhase(MonitorCallback::MONITOR_PHASE_ACTIVE);
                            break;

                        case WaitResult::CHANGE_DETECTED:
                            lastChangeDetected = res.changedItem_;
                            break;
                    }
                    nextExecTime = std::time(nullptr) + delay;
                }
            }
            catch (ExecCommandNowException&) {}

            ::wxSetEnv(L"change_path", utfTo<wxString>(lastChangeDetected.filePath)); //some way to output what file changed to the user
            ::wxSetEnv(L"change_action", toString(lastChangeDetected.action));        //

            //execute command
            cb.executeExternalCommand();
            nextExecTime = std::numeric_limits<time_t>::max();
        }
    };

    for (;;)
        try
        {
            execMonitoring(); //throw FileError
        }
        catch (const FileError& e)
        {
            cb.reportError(e.toString());
        }
}

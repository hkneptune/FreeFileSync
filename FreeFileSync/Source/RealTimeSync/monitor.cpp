// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "monitor.h"
#include <zen/file_access.h>
#include <zen/dir_watcher.h>
#include <zen/thread.h>
#include <zen/resolve_path.h>
//#include "../library/db_file.h"     //SYNC_DB_FILE_ENDING -> complete file too much of a dependency; file ending too little to decouple into single header
//#include "../library/lock_holder.h" //LOCK_FILE_ENDING
//TEMP_FILE_ENDING

using namespace zen;


namespace
{
constexpr std::chrono::seconds FOLDER_EXISTENCE_CHECK_INTERVAL(1);


//wait until all directories become available (again) + logs in network share
std::set<Zstring, LessNativePath> waitForMissingDirs(const std::vector<Zstring>& folderPathPhrases, //throw FileError
                                                     const std::function<void(const Zstring& folderPath)>& requestUiUpdate, std::chrono::milliseconds cbInterval)
{
    //early failure! check for unsupported folder paths:
    for (const char* protoName : {"ftp", "sftp", "mtp", "gdrive"})
        for (const Zstring& phrase : folderPathPhrases)
            //hopefully clear enough now: https://freefilesync.org/forum/viewtopic.php?t=4302
            if (startsWithAsciiNoCase(trimCpy(phrase), std::string(protoName) + ':'))
                throw FileError(replaceCpy(_("The %x protocol does not support directory monitoring:"), L"%x", utfTo<std::wstring>(protoName)) + L"\n\n" + fmtPath(phrase));

    for (;;)
    {
        struct FolderInfo
        {
            Zstring folderPathPhrase;
            std::future<bool> folderAvailable;
        };
        std::map<Zstring /*folderPath*/, FolderInfo, LessNativePath> folderInfos;

        for (const Zstring& phrase : folderPathPhrases)
        {
            const Zstring& folderPath = getResolvedFilePath(phrase);

            //start all folder checks asynchronously (non-existent network path may block)
            if (!folderInfos.contains(folderPath))
                folderInfos[folderPath] = { phrase, runAsync([folderPath]
                {
                    try
                    {
                        getItemType(folderPath); //throw FileError
                        return true;
                    }
                    catch (FileError&) { return false; }
                })
            };
        }

        std::set<Zstring, LessNativePath> availablePaths;
        std::set<Zstring, LessNativePath> missingPathPhrases;
        for (auto& [folderPath, folderInfo] : folderInfos)
        {
            std::future<bool>& folderAvailable = folderInfo.folderAvailable;

            while (folderAvailable.wait_for(cbInterval) == std::future_status::timeout)
                requestUiUpdate(folderPath); //throw X

            if (folderAvailable.get())
                availablePaths.insert(folderPath);
            else
                missingPathPhrases.insert(folderInfo.folderPathPhrase);
        }
        if (missingPathPhrases.empty())
            return availablePaths; //only return when all folders were found on *first* try!


        auto delayUntil = std::chrono::steady_clock::now() + FOLDER_EXISTENCE_CHECK_INTERVAL;

        for (const Zstring& folderPathPhrase : missingPathPhrases)
            for (;;)
            {
                //support specifying volume by name => call getResolvedFilePath() repeatedly
                const Zstring folderPath = getResolvedFilePath(folderPathPhrase);

                //wait some time...
                for (auto now = std::chrono::steady_clock::now(); now < delayUntil; now = std::chrono::steady_clock::now())
                {
                    requestUiUpdate(folderPath); //throw X
                    std::this_thread::sleep_for(cbInterval);
                }

                std::future<bool> folderAvailable = runAsync([folderPath]
                {
                    try
                    {
                        getItemType(folderPath); //throw FileError
                        return true;
                    }
                    catch (FileError&) { return false; }
                });

                while (folderAvailable.wait_for(cbInterval) == std::future_status::timeout)
                    requestUiUpdate(folderPath); //throw X

                if (folderAvailable.get())
                    break;
                //else: wait until folder is available: do not needlessly poll existing folders again!
                delayUntil = std::chrono::steady_clock::now() + FOLDER_EXISTENCE_CHECK_INTERVAL;
            }
    }
}


//wait until changes are detected or if a directory is not available (anymore)
DirWatcher::Change waitForChanges(const std::set<Zstring, LessNativePath>& folderPaths, //throw FileError
                                  const std::function<void(bool readyForSync)>& requestUiUpdate, std::chrono::milliseconds cbInterval)
{
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
            try { getItemType(folderPath); } //throw FileError
            catch (FileError&)
            {
                assert(false); //why "unavailable"!? violating waitForChanges() precondition!
                return {DirWatcher::ChangeType::baseFolderUnavailable, folderPath};
            }

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

        for (const auto& [folderPath, watcher] : watches)
        {
            //IMPORTANT CHECK: DirWatcher has problems detecting removal of top watched directories!
            if (checkDirNow)
                try //catch errors related to directory removal, e.g. ERROR_NETNAME_DELETED
                {
                    getItemType(folderPath); //throw FileError
                }
                catch (FileError&) { return {DirWatcher::ChangeType::baseFolderUnavailable, folderPath}; }

            try
            {
                std::vector<DirWatcher::Change> changes = watcher->fetchChanges([&] { requestUiUpdate(false /*readyForSync*/); /*throw X*/ },
                                                                                cbInterval); //throw FileError

                //give precedence to ChangeType::baseFolderUnavailable
                for (const DirWatcher::Change& change : changes)
                    if (change.type == DirWatcher::ChangeType::baseFolderUnavailable)
                        return change;

                std::erase_if(changes, [](const DirWatcher::Change& e)
                {
                    return
                        endsWith(e.itemPath, Zstr(".ffs_tmp"))  || //sync.8ea2.ffs_tmp
                        endsWith(e.itemPath, Zstr(".ffs_lock")) || //sync.ffs_lock, sync.Del.ffs_lock
                        endsWith(e.itemPath, Zstr(".ffs_db"));     //sync.ffs_db
                    //no need to ignore temporary recycle bin directory: this must be caused by a file deletion anyway
                });

                if (!changes.empty())
                    return changes[0];
            }
            catch (FileError&)
            {
                try { getItemType(folderPath); } //throw FileError
                catch (FileError&) { return {DirWatcher::ChangeType::baseFolderUnavailable, folderPath}; }

                throw;
            }
        }

        std::this_thread::sleep_for(cbInterval);
        requestUiUpdate(true /*readyForSync*/); //throw X: may start sync at this presumably idle time
    }
}


std::wstring getChangeTypeName(DirWatcher::ChangeType type)
{
    switch (type)
    {
        case DirWatcher::ChangeType::create:
            return L"Create";
        case DirWatcher::ChangeType::update:
            return L"Update";
        case DirWatcher::ChangeType::remove:
            return L"Delete";
        case DirWatcher::ChangeType::baseFolderUnavailable:
            return L"Base Folder Unavailable";
    }
    assert(false);
    return L"Error";
}

struct ExecCommandNowException {};
}


void rts::monitorDirectories(const std::vector<Zstring>& folderPathPhrases, std::chrono::seconds delay,
                             const std::function<void(const Zstring& itemPath, const std::wstring& actionName)>& executeExternalCommand /*throw FileError*/,
                             const std::function<void(const Zstring* missingFolderPath)>& requestUiUpdate,
                             const std::function<void(const std::wstring& msg         )>& reportError,
                             std::chrono::milliseconds cbInterval)
{
    assert(!folderPathPhrases.empty());
    if (folderPathPhrases.empty())
        return;

    for (;;)
        try
        {
            std::set<Zstring, LessNativePath> folderPaths = waitForMissingDirs(folderPathPhrases, [&](const Zstring& folderPath) { requestUiUpdate(&folderPath); }, cbInterval); //throw FileError

            //schedule initial execution (*after* all directories have arrived)
            auto nextExecTime = std::chrono::steady_clock::now() + delay;

            for (;;) //command executions
            {
                DirWatcher::Change lastChangeDetected;
                try
                {
                    for (;;) //detected changes
                    {
                        lastChangeDetected = waitForChanges(folderPaths, [&](bool readyForSync) //throw FileError, ExecCommandNowException
                        {
                            requestUiUpdate(nullptr);

                            if (readyForSync && std::chrono::steady_clock::now() >= nextExecTime)
                                throw ExecCommandNowException(); //abort wait and start sync
                        }, cbInterval);

                        if (lastChangeDetected.type == DirWatcher::ChangeType::baseFolderUnavailable)
                            //don't execute the command before all directories are available!
                            folderPaths = waitForMissingDirs(folderPathPhrases, [&](const Zstring& folderPath) { requestUiUpdate(&folderPath); }, cbInterval); //throw FileError

                        nextExecTime = std::chrono::steady_clock::now() + delay;
                    }
                }
                catch (ExecCommandNowException&) {}

                try
                {
                    executeExternalCommand(lastChangeDetected.itemPath, getChangeTypeName(lastChangeDetected.type)); //throw FileError
                }
                catch (const FileError& e) { reportError(e.toString()); }

                nextExecTime = std::chrono::steady_clock::time_point::max();
            }
        }
        catch (const FileError& e)
        {
            reportError(e.toString());
        }
}

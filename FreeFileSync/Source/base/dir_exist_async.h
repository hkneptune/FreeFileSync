// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef DIR_EXIST_ASYNC_H_0817328167343215806734213
#define DIR_EXIST_ASYNC_H_0817328167343215806734213

#include <zen/thread.h>
#include <zen/file_error.h>
#include <zen/basic_math.h>
#include "process_callback.h"
#include "../afs/abstract.h"


namespace fff
{
const int DEFAULT_FOLDER_ACCESS_TIME_OUT_SEC = 20; //consider CD-ROM insert or hard disk spin up time from sleep

namespace
{
//directory existence checking may hang for non-existent network drives => run asynchronously and update UI!
//- check existence of all directories in parallel! (avoid adding up search times if multiple network drives are not reachable)
//- add reasonable time-out time!
//- avoid checking duplicate entries => std::set
struct FolderStatus
{
    std::set<AbstractPath> existing;
    std::set<AbstractPath> notExisting;
    std::map<AbstractPath, zen::FileError> failedChecks;
};


FolderStatus getFolderStatusNonBlocking(const std::set<AbstractPath>& folderPaths,
                                        bool allowUserInteraction, ProcessCallback& procCallback  /*throw X*/)
{
    using namespace zen;

    //aggregate folder paths that are on the same root device: see parallel_scan.h
    std::map<AfsDevice, std::set<AbstractPath>> perDevicePaths;

    for (const AbstractPath& folderPath : folderPaths)
        if (!AFS::isNullPath(folderPath)) //skip empty folders
            perDevicePaths[folderPath.afsDevice].insert(folderPath);

    std::vector<std::pair<AbstractPath, std::future<bool>>> futureDetails;

    std::vector<ThreadGroup<std::packaged_task<bool()>>> perDeviceThreads;
    for (const auto& [afsDevice, deviceFolderPaths] : perDevicePaths)
    {
        perDeviceThreads.emplace_back(1,           "DirExist: " + utfTo<std::string>(AFS::getDisplayPath(AbstractPath(afsDevice, AfsPath()))));
        auto& threadGroup = perDeviceThreads.back();
        threadGroup.detach(); //don't wait on threads hanging longer than "folderAccessTimeout"

        //1. login to network share, connect with Google Drive, etc.
        std::shared_future<void> ftAuth = runAsync([afsDevice /*clang bug*/= afsDevice, allowUserInteraction] { AFS::authenticateAccess(afsDevice, allowUserInteraction); /*throw FileError*/ });

        for (const AbstractPath& folderPath : deviceFolderPaths)
        {
            std::packaged_task<bool()> pt([folderPath, ftAuth]
            {
                ftAuth.get(); //throw FileError

                /* 2. check dir existence:

                   CAVEAT: the case-sensitive semantics of AFS::itemStillExists() do not fit here!
                        BUT: its implementation happens to be okay for our use:
                    Assume we have a case-insensitive path match:
                    => AFS::itemStillExists() first checks AFS::getItemType()
                    => either succeeds (fine) or fails because of 1. not existing or 2. access error
                    => if the subsequent case-sensitive folder search also doesn't find the folder: only a problem in case 2
                    => FFS tries to create the folder during sync and fails with I. access error (fine) or II. already existing (obscures the previous "access error") */
                return static_cast<bool>(AFS::itemStillExists(folderPath)); //throw FileError
                //consider ItemType::FILE a failure instead? Meanwhile: return "false" IFF nothing (of any type) exists
            });
            auto fut = pt.get_future();
            threadGroup.run(std::move(pt));

            futureDetails.emplace_back(folderPath, std::move(fut));
        }
    }

    //don't wait (almost) endlessly like Win32 would on non-existing network shares:
    const auto startTime = std::chrono::steady_clock::now();

    FolderStatus output;

    for (auto& [folderPath, future] : futureDetails)
    {
        const std::wstring& displayPathFmt = fmtPath(AFS::getDisplayPath(folderPath));

        procCallback.reportStatus(replaceCpy(_("Searching for folder %x..."), L"%x", displayPathFmt)); //throw X

        const int deviceTimeOut = AFS::getAccessTimeout(folderPath); //0 if no timeout in force
        const auto timeoutTime = startTime + std::chrono::seconds(deviceTimeOut > 0 ? deviceTimeOut : DEFAULT_FOLDER_ACCESS_TIME_OUT_SEC);

        while (std::chrono::steady_clock::now() < timeoutTime &&
               future.wait_for(UI_UPDATE_INTERVAL / 2) != std::future_status::ready)
            procCallback.requestUiRefresh(); //throw X

        if (!isReady(future))
            output.failedChecks.emplace(folderPath, FileError(replaceCpy(_("Timeout while searching for folder %x."), L"%x", displayPathFmt)));
        else
            try
            {
                //call future::get() only *once*! otherwise: undefined behavior!
                if (future.get()) //throw FileError
                    output.existing.emplace(folderPath);
                else
                    output.notExisting.insert(folderPath);
            }
            catch (const FileError& e) { output.failedChecks.emplace(folderPath, e); }
    }
    return output;
}
}
}

#endif //DIR_EXIST_ASYNC_H_0817328167343215806734213

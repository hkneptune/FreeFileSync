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
#include "../fs/abstract.h"


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

    std::map<AbstractPath, AbstractPath> normalizedPathsEx; //get rid of folder aliases (e.g. path differing in case)
};

AbstractPath getNormalizedPath(const FolderStatus& status, const AbstractPath& folderPath)
{
    auto it = status.normalizedPathsEx.find(folderPath);
    return it == status.normalizedPathsEx.end() ? folderPath : it->second;
}


FolderStatus getFolderStatusNonBlocking(const std::set<AbstractPath>& folderPaths, const std::map<AfsDevice, size_t>& deviceParallelOps,
                                        bool allowUserInteraction, ProcessCallback& procCallback  /*throw X*/)
{
    using namespace zen;

    //aggregate folder paths that are on the same root device: see parallel_scan.h
    std::map<AfsDevice, std::set<AbstractPath>> perDevicePaths;

    for (const AbstractPath& folderPath : folderPaths)
        if (!AFS::isNullPath(folderPath)) //skip empty folders
            perDevicePaths[folderPath.afsDevice].insert(folderPath);

    std::vector<std::pair<AbstractPath, std::future<std::optional<AFS::FileId>>>> futureDetails;

    std::vector<ThreadGroup<std::packaged_task<std::optional<AFS::FileId>()>>> perDeviceThreads;
    for (const auto& [afsDevice, deviceFolderPaths] : perDevicePaths)
    {
        const size_t parallelOps = getDeviceParallelOps(deviceParallelOps, afsDevice);

        perDeviceThreads.emplace_back(parallelOps, "DirExist: " + utfTo<std::string>(AFS::getDisplayPath(AbstractPath(afsDevice, AfsPath()))));
        auto& threadGroup = perDeviceThreads.back();
        threadGroup.detach(); //don't wait on threads hanging longer than "folderAccessTimeout"

        for (const AbstractPath& folderPath : deviceFolderPaths)
        {
            std::packaged_task<std::optional<AFS::FileId>()> pt([folderPath, allowUserInteraction]() -> std::optional<AFS::FileId>
            {
                //1. login to network share, open FTP connection, etc.
                AFS::connectNetworkFolder(folderPath, allowUserInteraction); //throw FileError

                //2. check dir existence (...by doing something useful and getting the file ID)
                std::exception_ptr fidError;
                try
                {
                    const AFS::FileId fileId = AFS::getFileId(folderPath); //throw FileError
                    if (!fileId.empty()) //=> folder exists
                        return fileId;
                }
                catch (FileError&) { fidError = std::current_exception(); }
                //else: error or fileId not available, e.g. FTP, SFTP

                /* CAVEAT: the case-sensitive semantics of AFS::itemStillExists() do not fit here!
                        BUT: its implementation happens to be okay for our use:
                    Assume we have a case-insensitive path match:
                    => AFS::itemStillExists() first checks AFS::getItemType()
                    => either succeeds (fine) or fails because of 1. not existing or 2. access error
                    => the subsequent folder search reports "no folder": only a problem in case 2
                    => FFS tries to create the folder during sync and fails with I. access error (fine) or II. already existing (obscures the previous "access error") */
                if (!AFS::itemStillExists(folderPath)) //throw FileError
                    return {};

                if (fidError)
                    std::rethrow_exception(fidError);
                else
                    return AFS::FileId();
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
    std::map<AFS::FileId, AbstractPath> exFoldersById;

    for (auto& [folderPath, future] : futureDetails)
    {
        const std::wstring& displayPathFmt = fmtPath(AFS::getDisplayPath(folderPath));

        procCallback.reportStatus(replaceCpy(_("Searching for folder %x..."), L"%x", displayPathFmt)); //throw X

        const int deviceTimeOut = AFS::geAccessTimeout(folderPath); //0 if no timeout in force
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
                if (std::optional<AFS::FileId> folderInfo = future.get()) //throw FileError
                {
                    output.existing.emplace(folderPath);

                    //find folder aliases (e.g. path differing in case)
                    const AFS::FileId fileId = *folderInfo;
                    if (!fileId.empty())
                        exFoldersById.emplace(fileId, folderPath);

                    output.normalizedPathsEx.emplace(folderPath, fileId.empty() ? folderPath : exFoldersById.find(fileId)->second);
                }
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

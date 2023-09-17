// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef DIR_EXIST_ASYNC_H_0817328167343215806734213
#define DIR_EXIST_ASYNC_H_0817328167343215806734213

#include <zen/thread.h>
#include "process_callback.h"
#include "../afs/abstract.h"


namespace fff
{
namespace
{
struct FolderStatus
{
    std::set<AbstractPath> existing;
    std::set<AbstractPath> notExisting;
    std::map<AbstractPath, zen::FileError> failedChecks;
};
//- directory existence checking may hang for non-existent network drives => run asynchronously and update UI!
//- check existence of all directories in parallel! (avoid adding up search times if multiple network drives are not reachable)
//- authenticateAccess() better be integrated into folder existence check: if fails, why bother to go on with the folder!?
//- probably don't need timeout: https://freefilesync.org/forum/viewtopic.php?t=7350#p36817
//   => benefit: waits until user login completed in AFS::authenticateAccess()
FolderStatus getFolderStatusParallel(const std::set<AbstractPath>& folderPaths,
                                     bool authenticateAccess, const AFS::RequestPasswordFun& requestPassword /*throw X*/,
                                     PhaseCallback& cb /*throw X*/) //throw X
{
    using namespace zen;

    //aggregate folder paths that are on the same root device: see parallel_scan.h
    std::map<AfsDevice, std::set<AbstractPath>> perDevicePaths;

    for (const AbstractPath& folderPath : folderPaths)
        if (!AFS::isNullPath(folderPath)) //skip empty folders
            perDevicePaths[folderPath.afsDevice].insert(folderPath);
    //----------------------------------------------------------------------

    std::vector<std::pair<AbstractPath, std::future<bool>>> futFoldersExist;

    struct AsyncPrompt
    {
        std::wstring msg;
        std::wstring lastErrorMsg;
        std::promise<Zstring> promPassword;
    };
    auto protPromptsPending = authenticateAccess && requestPassword ? std::make_shared<Protected<RingBuffer<AsyncPrompt>>>() : nullptr;

    //----------------------------------------------------------------------
    std::vector<ThreadGroup<std::packaged_task<bool()>>> deviceThreadGroups;
    //----------------------------------------------------------------------

    for (const auto& [device, deviceFolderPaths] : perDevicePaths)
    {
        deviceThreadGroups.emplace_back(1,           Zstr("DirExist: ") + utfTo<Zstring>(AFS::getDisplayPath(AbstractPath(device, AfsPath()))));
        deviceThreadGroups.back().detach(); //don't wait on hanging threads if user cancels

        //1. login to network share, connect with Google Drive, etc.
        std::shared_future<void> futAuth;
        if (authenticateAccess)
        {
            AFS::RequestPasswordFun threadRequestPassword; //throw std::future_error
            if (requestPassword)
                threadRequestPassword = [promptsPendingWeak = std::weak_ptr(protPromptsPending)](const std::wstring& msg, const std::wstring& lastErrorMsg)
            {
                std::future<Zstring> futPassword;
                if (auto protPromptsPending2 = promptsPendingWeak.lock()) //[!] not owned by worker thread!
                    protPromptsPending2->access([&](RingBuffer<AsyncPrompt>& promptsPending)
                {
                    promptsPending.push_back(AsyncPrompt{msg, lastErrorMsg, {}});
                    futPassword = promptsPending.back().promPassword.get_future();
                });
                return futPassword.get(); //throw std::future_error -> if std::promise<Zstring> destroyed before password was set
            };

            futAuth = runAsync([device /*clang bug*/= device, threadRequestPassword]
            {
                setCurrentThreadName(Zstr("Auth: ") + utfTo<Zstring>(AFS::getDisplayPath(AbstractPath(device, AfsPath()))));
                AFS::authenticateAccess(device, threadRequestPassword); //throw FileError, std::future_error
            });
        }

        for (const AbstractPath& folderPath : deviceFolderPaths)
        {
            std::packaged_task<bool()> pt([folderPath, futAuth]
            {
                if (futAuth.valid())
                    futAuth.get(); //throw FileError, std::future_error

                /* 2. check dir existence:

                   CAVEAT: the case-sensitive semantics of AFS::itemExists() do not fit here!
                        BUT: its implementation happens to be okay for our use:
                    Assume we have a case-insensitive path match:
                    => AFS::itemExists() first checks AFS::getItemType()
                    => either succeeds (fine) or fails because of 1. not existing or 2. access error
                    => if the subsequent case-sensitive folder search also doesn't find the folder: only a problem in case 2
                    => FFS tries to create the folder during sync and fails with I. access error (fine) or II. already existing (obscures the previous "access error") */
                return AFS::itemExists(folderPath); //throw FileError; return "false" IFF nothing (of any type) exists

                //check for ItemType::file? too pedantic?
                //    throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getDisplayPath(folderPath))),
                //                    replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(getItemName(folderPath))));
            });
            auto futIsExisting = pt.get_future();
            deviceThreadGroups.back().run(std::move(pt));

            futFoldersExist.emplace_back(folderPath, std::move(futIsExisting));
        }
    }
    //----------------------------------------------------------------------

    FolderStatus output;

    for (auto& [folderPath, futFolderExists] : futFoldersExist)
    {
        cb.updateStatus(replaceCpy(_("Searching for folder %x..."), L"%x", fmtPath(AFS::getDisplayPath(folderPath)))); //throw X

        while (futFolderExists.wait_for(UI_UPDATE_INTERVAL / 2) == std::future_status::timeout)
        {
            cb.requestUiUpdate(); //throw X

            //marshal password prompt callback from current thread (probably main) to worker threads
            //=> polling delay doesn't matter because user interaction is required
            if (protPromptsPending)
                protPromptsPending->access([&](RingBuffer<AsyncPrompt>& promptsPending)
            {
                //call back while holding Protected<> lock!? device authentication threads blocking doesn't matter because prompts are serialized to GUI anyway
                if (!promptsPending.empty())
                {
                    assert(requestPassword); //... in this context
                    const Zstring password = requestPassword(promptsPending.front().msg, promptsPending.front().lastErrorMsg); //throw X
                    promptsPending.front().promPassword.set_value(password);
                    promptsPending.pop_front();
                }
            });
        }

        try
        {
            //call future::get() only *once*! otherwise: undefined behavior!
            if (futFolderExists.get()) //throw FileError, (std::future_error)
                output.existing.insert(folderPath);
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

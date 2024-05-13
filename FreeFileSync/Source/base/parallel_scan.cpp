// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "parallel_scan.h"
#include <chrono>
//#include <zen/file_error.h>
#include <zen/thread.h>
#include <zen/scope_guard.h>

using namespace zen;
using namespace fff;


namespace
{
const int FOLDER_TRAVERSAL_LEVEL_MAX = 100;

/* PERF NOTE

    ---------------------------------------------
    |Test case: Reading from two different disks|
    ---------------------------------------------
    Windows 7:
                1st(unbuffered) |2nd (OS buffered)
                ----------------------------------
    1 Thread:          57s      |        8s
    2 Threads:         39s      |        7s

    ---------------------------------------------------
    |Test case: Reading two directories from same disk|
    ---------------------------------------------------
    Windows 7:                                           Windows XP:
                1st(unbuffered) |2nd (OS buffered)                   1st(unbuffered) |2nd (OS buffered)
                ----------------------------------                   ----------------------------------
    1 Thread:          41s      |        13s             1 Thread:          45s      |        13s
    2 Threads:         42s      |        11s             2 Threads:         38s      |         8s

    => Traversing does not take any advantage of file locality so that even multiple threads operating on the same disk impose no performance overhead! (even faster on XP)    */

class AsyncCallback
{
public:
    AsyncCallback(size_t threadsToFinish, std::chrono::milliseconds cbInterval) : threadsToFinish_(threadsToFinish), cbInterval_(cbInterval) {}

    //blocking call: context of worker thread
    AFS::TraverserCallback::HandleError reportError(const AFS::TraverserCallback::ErrorInfo& errorInfo) //throw ThreadStopRequest
    {
        assert(!runningOnMainThread());
        std::unique_lock dummy(lockRequest_);
        interruptibleWait(conditionReadyForNewRequest_, dummy, [this] { return !errorRequest_ && !errorResponse_; }); //throw ThreadStopRequest

        errorRequest_ = errorInfo;
        conditionNewRequest.notify_all();

        interruptibleWait(conditionHaveResponse_, dummy, [this] { return static_cast<bool>(errorResponse_); }); //throw ThreadStopRequest

        AFS::TraverserCallback::HandleError rv = *errorResponse_;

        errorRequest_  = std::nullopt;
        errorResponse_ = std::nullopt;

        dummy.unlock(); //optimization for condition_variable::notify_all()
        conditionReadyForNewRequest_.notify_all(); //instead of notify_one(); work around bug: https://svn.boost.org/trac/boost/ticket/7796

        return rv;
    }

    //context of main thread
    void waitUntilDone(std::chrono::milliseconds duration, const TravErrorCb& onError, const TravStatusCb& onStatusUpdate) //throw X
    {
        assert(runningOnMainThread());
        for (;;)
        {
            const std::chrono::steady_clock::time_point callbackTime = std::chrono::steady_clock::now() + duration;

            for (std::unique_lock dummy(lockRequest_) ;;) //process all errors without delay
            {
                const bool rv = conditionNewRequest.wait_until(dummy, callbackTime, [this] { return (errorRequest_ && !errorResponse_) || (threadsToFinish_ == 0); });
                if (!rv) //time-out + condition not met
                    break;

                if (errorRequest_ && !errorResponse_)
                {
                    assert(threadsToFinish_ != 0);
                    switch (onError({errorRequest_->msg, errorRequest_->failTime, errorRequest_->retryNumber})) //throw X
                    {
                        case PhaseCallback::ignore:
                            errorResponse_ = AFS::TraverserCallback::HandleError::ignore;
                            break;

                        case PhaseCallback::retry:
                            errorResponse_ = AFS::TraverserCallback::HandleError::retry;
                            break;
                    }
                    conditionHaveResponse_.notify_all(); //instead of notify_one(); work around bug: https://svn.boost.org/trac/boost/ticket/7796
                }
                if (threadsToFinish_ == 0)
                {
                    dummy.unlock();
                    onStatusUpdate(getStatusLine(), itemsScanned_); //throw X; one last call for accurate stat-reporting!
                    return;
                }
            }

            //call member functions outside of mutex scope:
            onStatusUpdate(getStatusLine(), itemsScanned_); //throw X
        }
    }

    //perf optimization: comparison phase is 7% faster by avoiding needless std::wstring construction for reportCurrentFile()
    bool mayReportCurrentFile(int threadIdx, std::chrono::steady_clock::time_point& lastReportTime) const
    {
        if (threadIdx != notifyingThreadIdx_) //only one thread at a time may report status: the first in sequential order
            return false;

        const auto now = std::chrono::steady_clock::now();
        if (now > lastReportTime + cbInterval_) //perform ui updates not more often than necessary
        {
            lastReportTime = now; //keep "lastReportTime" at worker thread level to avoid locking!
            return true;
        }
        return false;
    }

    void reportCurrentFile(const std::wstring& filePath) //context of worker thread
    {
        assert(!runningOnMainThread());
        std::lock_guard dummy(lockCurrentStatus_);
        currentFile_ = filePath;
    }

    void incItemsScanned() { ++itemsScanned_; } //perf: irrelevant! scanning is almost entirely file I/O bound, not CPU bound! => no prob having multiple threads poking at the same variable!

    void notifyWorkBegin(int threadIdx, const size_t parallelOps)
    {
        std::lock_guard dummy(lockCurrentStatus_);

        [[maybe_unused]] const auto [it, inserted] = activeThreadIdxs_.emplace(threadIdx, parallelOps);
        assert(inserted);

        notifyingThreadIdx_ = activeThreadIdxs_.begin()->first;
    }

    void notifyWorkEnd(int threadIdx)
    {
        {
            std::lock_guard dummy(lockCurrentStatus_);

            [[maybe_unused]] const size_t no = activeThreadIdxs_.erase(threadIdx);
            assert(no == 1);

            notifyingThreadIdx_ = activeThreadIdxs_.empty() ? 0 : activeThreadIdxs_.begin()->first;
        }
        {
            std::lock_guard dummy(lockRequest_);
            assert(threadsToFinish_ > 0);
            if (--threadsToFinish_ == 0)
                conditionNewRequest.notify_all(); //perf: should unlock mutex before notify!? (insignificant)
        }
    }

private:
    std::wstring getStatusLine() //context of main thread, call repreatedly
    {
        assert(runningOnMainThread());

        size_t parallelOpsTotal = 0;
        std::wstring filePath;
        {
            std::lock_guard dummy(lockCurrentStatus_);
            parallelOpsTotal = activeThreadIdxs_.size();
            filePath = currentFile_;
        }
        if (parallelOpsTotal >= 2)
            return L'[' + _P("1 thread", "%x threads", parallelOpsTotal) + L"] " + filePath;
        else
            return filePath;
    }

    //---- main <-> worker communication channel ----
    std::mutex lockRequest_;
    std::condition_variable conditionReadyForNewRequest_;
    std::condition_variable conditionNewRequest;
    std::condition_variable conditionHaveResponse_;
    std::optional<AFS::TraverserCallback::ErrorInfo  > errorRequest_;
    std::optional<AFS::TraverserCallback::HandleError> errorResponse_;
    size_t threadsToFinish_; //can't use activeThreadIdxs_.size() which is locked by different mutex!
    //also note: activeThreadIdxs_.size() may be 0 during worker thread construction!

    //---- status updates ----
    std::mutex lockCurrentStatus_; //different lock for status updates so that we're not blocked by other threads reporting errors
    std::wstring currentFile_;
    std::map<int /*threadIdx*/, size_t /*parallelOps*/> activeThreadIdxs_;

    std::atomic<int> notifyingThreadIdx_{0}; //CAVEAT: do NOT use boost::thread::id: https://svn.boost.org/trac/boost/ticket/5754
    const std::chrono::milliseconds cbInterval_;

    //---- status updates II (lock-free) ----
    std::atomic<int> itemsScanned_{0}; //std:atomic is uninitialized by default!
};

//-------------------------------------------------------------------------------------------------

struct TraverserConfig
{
    const AbstractPath baseFolderPath;  //thread-safe like an int! :)
    const FilterRef filter;
    const SymLinkHandling handleSymlinks;

    std::unordered_map<Zstring, Zstringc>& failedDirReads;
    std::unordered_map<Zstring, Zstringc>& failedItemReads;

    AsyncCallback& acb;
    const int threadIdx;
    std::chrono::steady_clock::time_point& lastReportTime; //thread-level
};


class DirCallback : public AFS::TraverserCallback
{
public:
    DirCallback(TraverserConfig& cfg,
                Zstring&& parentRelPathPf, //postfixed with FILE_NAME_SEPARATOR (or empty!)
                FolderContainer& output,
                int level) :
        cfg_(cfg),
        parentRelPathPf_(std::move(parentRelPathPf)),
        output_(output),
        level_(level) {} //MUST NOT use cfg_ during construction! see BaseDirCallback()

    virtual void                               onFile   (const AFS::FileInfo&    fi) override; //
    virtual std::shared_ptr<TraverserCallback> onFolder (const AFS::FolderInfo&  fi) override; //throw ThreadStopRequest
    virtual HandleLink                         onSymlink(const AFS::SymlinkInfo& li) override; //

    HandleError reportDirError (const ErrorInfo& errorInfo)                          override  { return reportError(errorInfo, Zstring()); } //throw ThreadStopRequest
    HandleError reportItemError(const ErrorInfo& errorInfo, const Zstring& itemName) override  { return reportError(errorInfo, itemName);  } //

private:
    HandleError reportError(const ErrorInfo& errorInfo, const Zstring& itemName /*optional*/); //throw ThreadStopRequest

    TraverserConfig& cfg_;
    const Zstring parentRelPathPf_;
    FolderContainer& output_;
    const int level_;
};


class BaseDirCallback : public DirCallback
{
public:
    BaseDirCallback(const DirectoryKey& baseFolderKey, DirectoryValue& output,
                    AsyncCallback& acb, int threadIdx, std::chrono::steady_clock::time_point& lastReportTime) :
        DirCallback(travCfg_ /*not yet constructed!!!*/, Zstring(), output.folderCont, 0 /*level*/),
        travCfg_
    {
        baseFolderKey.folderPath,
        baseFolderKey.filter,
        baseFolderKey.handleSymlinks,
        output.failedFolderReads,
        output.failedItemReads,
        acb,
        threadIdx,
        lastReportTime,
    }
    {
        if (acb.mayReportCurrentFile(threadIdx, lastReportTime))
            acb.reportCurrentFile(AFS::getDisplayPath(baseFolderKey.folderPath)); //just in case first directory access is blocking
    }

private:
    TraverserConfig travCfg_;
};


void DirCallback::onFile(const AFS::FileInfo& fi) //throw ThreadStopRequest
{
    interruptionPoint(); //throw ThreadStopRequest

    const Zstring& relPath = parentRelPathPf_ + fi.itemName;

    //update status information no matter if item is excluded or not!
    if (cfg_.acb.mayReportCurrentFile(cfg_.threadIdx, cfg_.lastReportTime))
        cfg_.acb.reportCurrentFile(AFS::getDisplayPath(AFS::appendRelPath(cfg_.baseFolderPath, relPath)));

    //------------------------------------------------------------------------------------
    //apply filter before processing (use relative name!)
    if (!cfg_.filter.ref().passFileFilter(relPath))
        return;
    //note: sync.ffs_db database and lock files are excluded via path filter!

    output_.addFile(fi.itemName,
    {
        .modTime = fi.modTime,
        .fileSize = fi.fileSize,
        .filePrint = fi.filePrint,
        .isFollowedSymlink = fi.isFollowedSymlink,
    });

    cfg_.acb.incItemsScanned(); //add 1 element to the progress indicator
}


std::shared_ptr<AFS::TraverserCallback> DirCallback::onFolder(const AFS::FolderInfo& fi) //throw ThreadStopRequest
{
    interruptionPoint(); //throw ThreadStopRequest

    Zstring relPath = parentRelPathPf_ + fi.itemName;

    //update status information no matter if item is excluded or not!
    if (cfg_.acb.mayReportCurrentFile(cfg_.threadIdx, cfg_.lastReportTime))
        cfg_.acb.reportCurrentFile(AFS::getDisplayPath(AFS::appendRelPath(cfg_.baseFolderPath, relPath)));

    //------------------------------------------------------------------------------------
    //apply filter before processing (use relative name!)
    bool childItemMightMatch = true;
    const bool passFilter = cfg_.filter.ref().passDirFilter(relPath, &childItemMightMatch);
    if (!passFilter && !childItemMightMatch)
        return nullptr; //do NOT traverse subdirs
    //else: ensure directory filtering is applied later to exclude actually filtered directories!!!

    FolderContainer& subFolder = output_.addFolder(fi.itemName, {.isFollowedSymlink = fi.isFollowedSymlink});
    if (passFilter)
        cfg_.acb.incItemsScanned(); //add 1 element to the progress indicator

    //------------------------------------------------------------------------------------
    if (level_ > FOLDER_TRAVERSAL_LEVEL_MAX) //Win32 traverser: stack overflow approximately at level 1000
        //check after FolderContainer::addFolder()
        for (size_t retryNumber = 0;; ++retryNumber)
            switch (reportItemError({replaceCpy(_("Cannot read directory %x."), L"%x", AFS::getDisplayPath(AFS::appendRelPath(cfg_.baseFolderPath, relPath))) +
                                     L"\n\n" L"Endless recursion.", std::chrono::steady_clock::now(), retryNumber}, fi.itemName)) //throw ThreadStopRequest
            {
                case AFS::TraverserCallback::HandleError::retry:
                    break;
                case AFS::TraverserCallback::HandleError::ignore:
                    return nullptr;
            }

    return std::make_shared<DirCallback>(cfg_, std::move(relPath += FILE_NAME_SEPARATOR), subFolder, level_ + 1);
}


DirCallback::HandleLink DirCallback::onSymlink(const AFS::SymlinkInfo& si) //throw ThreadStopRequest
{
    interruptionPoint(); //throw ThreadStopRequest

    const Zstring& relPath = parentRelPathPf_ + si.itemName;

    //update status information no matter if item is excluded or not!
    if (cfg_.acb.mayReportCurrentFile(cfg_.threadIdx, cfg_.lastReportTime))
        cfg_.acb.reportCurrentFile(AFS::getDisplayPath(AFS::appendRelPath(cfg_.baseFolderPath, relPath)));

    switch (cfg_.handleSymlinks)
    {
        case SymLinkHandling::exclude:
            return HandleLink::skip;

        case SymLinkHandling::asLink:
            if (cfg_.filter.ref().passFileFilter(relPath)) //always use file filter: Link type may not be "stable" on Linux!
            {
                output_.addLink(si.itemName, {.modTime = si.modTime});
                cfg_.acb.incItemsScanned(); //add 1 element to the progress indicator
            }
            return HandleLink::skip;

        case SymLinkHandling::follow:
            //filter symlinks before trying to follow them: handle user-excluded broken symlinks!
            //since we don't know yet what type the symlink will resolve to, only do this when both filter variants agree:
            if (!cfg_.filter.ref().passFileFilter(relPath))
            {
                bool childItemMightMatch = true;
                if (!cfg_.filter.ref().passDirFilter(relPath, &childItemMightMatch))
                    if (!childItemMightMatch)
                        return HandleLink::skip;
            }
            return HandleLink::follow;
    }

    assert(false);
    return HandleLink::skip;
}


DirCallback::HandleError DirCallback::reportError(const ErrorInfo& errorInfo, const Zstring& itemName /*optional*/) //throw ThreadStopRequest
{
    const HandleError handleErr = cfg_.acb.reportError(errorInfo); //throw ThreadStopRequest
    switch (handleErr)
    {
        case HandleError::ignore:
            if (itemName.empty())
                cfg_.failedDirReads.emplace(beforeLast(parentRelPathPf_, FILE_NAME_SEPARATOR, IfNotFoundReturn::none), utfTo<Zstringc>(errorInfo.msg));
            else
                cfg_.failedItemReads.emplace(parentRelPathPf_ + itemName, utfTo<Zstringc>(errorInfo.msg));
            break;

        case HandleError::retry:
            break;
    }
    return handleErr;
}
}


std::map<DirectoryKey, DirectoryValue> fff::parallelDeviceTraversal(const std::set<DirectoryKey>& foldersToRead,
                                                                    const TravErrorCb& onError, const TravStatusCb& onStatusUpdate,
                                                                    std::chrono::milliseconds cbInterval)
{
    std::map<DirectoryKey, DirectoryValue> output;

    //aggregate folder paths that are on the same root device:
    // => one worker thread *per device*: avoid excessive parallelism
    // => parallel folder traversal considers "parallel file operations" as specified by user
    // => (S)FTP: avoid hitting connection limits inadvertently
    std::map<AfsDevice, std::set<DirectoryKey>> perDeviceFolders;

    for (const DirectoryKey& key : foldersToRead)
        perDeviceFolders[key.folderPath.afsDevice].insert(key);

    //communication channel used by threads
    AsyncCallback acb(perDeviceFolders.size() /*threadsToFinish*/, cbInterval); //manage life time: enclose InterruptibleThread's!!!

    std::vector<InterruptibleThread> worker;
    ZEN_ON_SCOPE_SUCCESS( for (InterruptibleThread& wt : worker) wt.join(); ); //no stop needed in success case => preempt ~InterruptibleThread()
    ZEN_ON_SCOPE_FAIL( for (InterruptibleThread& wt : worker) wt.requestStop(); ); //stop *all* at the same time before join!

    //init worker threads
    for (const auto& [afsDevice, dirKeys] : perDeviceFolders)
    {
        const int threadIdx = static_cast<int>(worker.size());
        Zstring threadName = Zstr("Compare[") + numberTo<Zstring>(threadIdx + 1) + Zstr('/') + numberTo<Zstring>(perDeviceFolders.size()) + Zstr("] ") +
                             utfTo<Zstring>(AFS::getDisplayPath({afsDevice, AfsPath()}));

        const size_t parallelOps = 1;
        std::map<DirectoryKey, DirectoryValue*> workload;

        for (const DirectoryKey& key : dirKeys)
            workload.emplace(key, &output[key]); //=> DirectoryValue* unshared for lock-free worker-thread access

        worker.emplace_back([afsDevice /*clang bug*/= afsDevice, workload, threadIdx, &acb, parallelOps, threadName = std::move(threadName)]() mutable
        {
            setCurrentThreadName(threadName);

            acb.notifyWorkBegin(threadIdx, parallelOps);
            ZEN_ON_SCOPE_EXIT(acb.notifyWorkEnd(threadIdx));

            std::chrono::steady_clock::time_point lastReportTime; //keep thread-local!

            AFS::TraverserWorkload travWorkload;

            for (auto& [folderKey, folderVal] : workload)
            {
                assert(folderKey.folderPath.afsDevice == afsDevice);
                travWorkload.emplace_back(folderKey.folderPath.afsPath, std::make_shared<BaseDirCallback>(folderKey, *folderVal, acb, threadIdx, lastReportTime));
            }
            AFS::traverseFolderRecursive(afsDevice, travWorkload, parallelOps); //throw ThreadStopRequest
        });
    }
    acb.waitUntilDone(cbInterval, onError, onStatusUpdate); //throw X

    return output;
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "parallel_scan.h"
#include <chrono>
#include <zen/file_error.h>
#include <zen/basic_math.h>
#include <zen/thread.h>
#include <zen/scope_guard.h>
//#include "db_file.h"
//#include "lock_holder.h"

using namespace zen;
using namespace fff;


namespace
{
/*
#ifdef ZEN_WIN

struct DiskInfo
{
    DiskInfo() :
        driveType(DRIVE_UNKNOWN),
        diskID(-1) {}

    UINT driveType;
    int diskID; // -1 if id could not be determined, this one is filled if driveType == DRIVE_FIXED or DRIVE_REMOVABLE;
};

inline
bool operator<(const DiskInfo& lhs, const DiskInfo& rhs)
{
    if (lhs.driveType != rhs.driveType)
        return lhs.driveType < rhs.driveType;

    if (lhs.diskID < 0 || rhs.diskID < 0)
        return false;
    //consider "same", reason: one volume may be uniquely associated with one disk, while the other volume is associated to the same disk AND another one!
    //volume <-> disk is 0..N:1..N

    return lhs.diskID < rhs.diskID ;
}


DiskInfo retrieveDiskInfo(const Zstring& itemPath)
{
    std::vector<wchar_t> volName(std::max(pathName.size(), static_cast<size_t>(10000)));

    DiskInfo output;

    //full pathName need not yet exist!
    if (!::GetVolumePathName(itemPath.c_str(),                    //__in  LPCTSTR lpszFileName,
                             &volName[0],                         //__out LPTSTR lpszVolumePathName,
                             static_cast<DWORD>(volName.size()))) //__in  DWORD cchBufferLength
        return output;

    const Zstring rootPathName = &volName[0];

    output.driveType = ::GetDriveType(rootPathName.c_str());

    if (output.driveType == DRIVE_NO_ROOT_DIR) //these two should be the same error category
        output.driveType = DRIVE_UNKNOWN;

    if (output.driveType != DRIVE_FIXED && output.driveType != DRIVE_REMOVABLE)
        return output; //no reason to get disk ID

    //go and find disk id:

    //format into form: "\\.\C:"
    Zstring volnameFmt = rootPathName;
    if (endsWith(volnameFmt, FILE_NAME_SEPARATOR))
        volnameFmt.pop_back();
    volnameFmt = L"\\\\.\\" + volnameFmt;

    HANDLE hVolume = ::CreateFile(volnameFmt.c_str(),
                                  0,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr,
                                  OPEN_EXISTING,
                                  0,
                                  nullptr);
    if (hVolume == INVALID_HANDLE_VALUE)
        return output;
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hVolume));

    std::vector<std::byte> buffer(sizeof(VOLUME_DISK_EXTENTS) + sizeof(DISK_EXTENT)); //reserve buffer for at most one disk! call below will then fail if volume spans multiple disks!

    DWORD bytesReturned = 0;
    if (!::DeviceIoControl(hVolume,                              //_In_         HANDLE hDevice,
                           IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, //_In_         DWORD dwIoControlCode,
                           nullptr,                              //_In_opt_     LPVOID lpInBuffer,
                           0,                                    //_In_         DWORD nInBufferSize,
                           &buffer[0],                           //_Out_opt_    LPVOID lpOutBuffer,
                           static_cast<DWORD>(buffer.size()),    //_In_         DWORD nOutBufferSize,
                           &bytesReturned,                       //_Out_opt_    LPDWORD lpBytesReturned
                           nullptr))                             //_Inout_opt_  LPOVERLAPPED lpOverlapped
        return output;

    const VOLUME_DISK_EXTENTS& volDisks = *reinterpret_cast<VOLUME_DISK_EXTENTS*>(&buffer[0]);

    if (volDisks.NumberOfDiskExtents != 1)
        return output;

    output.diskID = volDisks.Extents[0].DiskNumber;

    return output;
}
#endif
*/

/*
PERF NOTE

--------------------------------------------
|Testcase: Reading from two different disks|
--------------------------------------------
Windows 7:
            1st(unbuffered) |2nd (OS buffered)
            ----------------------------------
1 Thread:          57s      |        8s
2 Threads:         39s      |        7s

--------------------------------------------------
|Testcase: Reading two directories from same disk|
--------------------------------------------------
Windows 7:                                        Windows XP:
            1st(unbuffered) |2nd (OS buffered)                   1st(unbuffered) |2nd (OS buffered)
            ----------------------------------                   ----------------------------------
1 Thread:          41s      |        13s             1 Thread:          45s      |        13s
2 Threads:         42s      |        11s             2 Threads:         38s      |         8s

=> Traversing does not take any advantage of file locality so that even multiple threads operating on the same disk impose no performance overhead! (even faster on XP)

std::vector<std::set<DirectoryKey>> separateByDistinctDisk(const std::set<DirectoryKey>& dirkeys)
{
    //use one thread per physical disk:
    using DiskKeyMapping = std::map<DiskInfo, std::set<DirectoryKey>>;
    DiskKeyMapping tmp;
    std::for_each(dirkeys.begin(), dirkeys.end(),
    [&](const DirectoryKey& key) { tmp[retrieveDiskInfo(key.dirpathFull_)].insert(key); });

    std::vector<std::set<DirectoryKey>> buckets;
    std::transform(tmp.begin(), tmp.end(), std::back_inserter(buckets),
    [&](const DiskKeyMapping::value_type& diskToKey) { return diskToKey.second; });
    return buckets;
}
*/

//------------------------------------------------------------------------------------------

class AsyncCallback //actor pattern
{
public:
    AsyncCallback(size_t threadsToFinish, std::chrono::milliseconds cbInterval) : threadsToFinish_(threadsToFinish), cbInterval_(cbInterval) {}

    //blocking call: context of worker thread
    AFS::TraverserCallback::HandleError reportError(const std::wstring& msg, size_t retryNumber) //throw ThreadInterruption
    {
        assert(!runningMainThread());
        std::unique_lock<std::mutex> dummy(lockRequest_);
        interruptibleWait(conditionReadyForNewRequest_, dummy, [this] { return !errorRequest_ && !errorResponse_; }); //throw ThreadInterruption

        errorRequest_ = std::make_pair(msg, retryNumber);
        conditionNewRequest.notify_all();

        interruptibleWait(conditionHaveResponse_, dummy, [this] { return static_cast<bool>(errorResponse_); }); //throw ThreadInterruption

        AFS::TraverserCallback::HandleError rv = *errorResponse_;

        errorRequest_  = NoValue();
        errorResponse_ = NoValue();

        dummy.unlock(); //optimization for condition_variable::notify_all()
        conditionReadyForNewRequest_.notify_all(); //instead of notify_one(); workaround bug: https://svn.boost.org/trac/boost/ticket/7796

        return rv;
    }

    //context of main thread
    void waitUntilDone(std::chrono::milliseconds duration, const TravErrorCb& onError, const TravStatusCb& onStatusUpdate) //throw X
    {
        assert(runningMainThread());
        for (;;)
        {
            const std::chrono::steady_clock::time_point callbackTime = std::chrono::steady_clock::now() + duration;

            for (std::unique_lock<std::mutex> dummy(lockRequest_) ;;) //process all errors without delay
            {
                const bool rv = conditionNewRequest.wait_until(dummy, callbackTime, [this] { return (errorRequest_ && !errorResponse_) || (threadsToFinish_ == 0); });
                if (!rv) //time-out + condition not met
                    break;

                if (errorRequest_ && !errorResponse_)
                {
                    assert(threadsToFinish_ != 0);
                    errorResponse_ = onError(errorRequest_->first, errorRequest_->second); //throw X
                    conditionHaveResponse_.notify_all(); //instead of notify_one(); workaround bug: https://svn.boost.org/trac/boost/ticket/7796
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

        const auto now = std::chrono::steady_clock::now(); //0 on error

        //perform ui updates not more often than necessary + handle potential chrono wrap-around!
        if (numeric::dist(now, lastReportTime) > cbInterval_)
        {
            lastReportTime = now; //keep "lastReportTime" at worker thread level to avoid locking!
            return true;
        }
        return false;
    }

    void reportCurrentFile(const std::wstring& filePath) //context of worker thread
    {
        assert(!runningMainThread());
        std::lock_guard<std::mutex> dummy(lockCurrentStatus_);
        currentFile_ = filePath;
    }

    void incItemsScanned() { ++itemsScanned_; } //perf: irrelevant! scanning is almost entirely file I/O bound, not CPU bound! => no prob having multiple threads poking at the same variable!

    void notifyWorkBegin(int threadIdx, const size_t parallelOps)
    {
        std::lock_guard<std::mutex> dummy(lockCurrentStatus_);

        const auto it = activeThreadIdxs_.emplace(threadIdx, parallelOps);
        assert(it.second);
        (void)it;

        notifyingThreadIdx_ = activeThreadIdxs_.begin()->first;
    }

    void notifyWorkEnd(int threadIdx)
    {
        {
            std::lock_guard<std::mutex> dummy(lockCurrentStatus_);

            const size_t no = activeThreadIdxs_.erase(threadIdx);
            assert(no == 1);
            (void)no;

            notifyingThreadIdx_ = activeThreadIdxs_.empty() ? 0 : activeThreadIdxs_.begin()->first;
        }
        {
            std::lock_guard<std::mutex> dummy(lockRequest_);
            assert(threadsToFinish_ > 0);
            if (--threadsToFinish_ == 0)
                conditionNewRequest.notify_all(); //perf: should unlock mutex before notify!? (insignificant)
        }
    }

private:
    std::wstring getStatusLine() //context of main thread, call repreatedly
    {
        assert(runningMainThread());

        size_t parallelOpsTotal = 0;
        std::wstring filePath;
        {
            std::lock_guard<std::mutex> dummy(lockCurrentStatus_);

            for (const auto& item : activeThreadIdxs_)
                parallelOpsTotal += item.second;

            filePath = currentFile_;
        }
        if (parallelOpsTotal >= 2)
            return L"[" + _P("1 thread", "%x threads", parallelOpsTotal) + L"] " + filePath;
        else
            return filePath;
    }

    //---- main <-> worker communication channel ----
    std::mutex lockRequest_;
    std::condition_variable conditionReadyForNewRequest_;
    std::condition_variable conditionNewRequest;
    std::condition_variable conditionHaveResponse_;
    Opt<std::pair<std::wstring, size_t>>     errorRequest_; //error message + retry number
    Opt<AFS::TraverserCallback::HandleError> errorResponse_;
    size_t threadsToFinish_; //can't use activeThreadIdxs_.size() which is locked by different mutex!
    //also note: activeThreadIdxs_.size() may be 0 during worker thread construction!

    //---- status updates ----
    std::mutex lockCurrentStatus_; //different lock for status updates so that we're not blocked by other threads reporting errors
    std::wstring currentFile_;
    std::map<int /*threadIdx*/, size_t /*parallelOps*/> activeThreadIdxs_;

    std::atomic<int> notifyingThreadIdx_ { 0 }; //CAVEAT: do NOT use boost::thread::id: https://svn.boost.org/trac/boost/ticket/5754
    const std::chrono::milliseconds cbInterval_;

    //---- status updates II (lock-free) ----
    std::atomic<int> itemsScanned_{ 0 }; //std:atomic is uninitialized by default!
};

//-------------------------------------------------------------------------------------------------

struct TraverserConfig
{
    const AbstractPath baseFolderPath;  //thread-safe like an int! :)
    const HardFilter::FilterRef filter; //always bound!
    const SymLinkHandling handleSymlinks;

    std::map<Zstring, std::wstring, LessFilePath>& failedDirReads;
    std::map<Zstring, std::wstring, LessFilePath>& failedItemReads;

    AsyncCallback& acb;
    const int threadIdx;
    std::chrono::steady_clock::time_point& lastReportTime; //thread-level
};


class DirCallback : public AFS::TraverserCallback
{
public:
    DirCallback(TraverserConfig& cfg,
                const Zstring& parentRelPathPf, //postfixed with FILE_NAME_SEPARATOR!
                FolderContainer& output,
                int level) :
        cfg_(cfg),
        parentRelPathPf_(parentRelPathPf),
        output_(output),
        level_(level) {} //MUST NOT use cfg_ during construction! see BaseDirCallback()

    virtual void                               onFile   (const AFS::FileInfo&    fi) override; //
    virtual std::shared_ptr<TraverserCallback> onFolder (const AFS::FolderInfo&  fi) override; //throw ThreadInterruption
    virtual HandleLink                         onSymlink(const AFS::SymlinkInfo& li) override; //

    HandleError reportDirError (const std::wstring& msg, size_t retryNumber)                          override { return reportError(msg, retryNumber, Zstring()); } //throw ThreadInterruption
    HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName) override { return reportError(msg, retryNumber, itemName);  } //

private:
    HandleError reportError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName /*optional*/); //throw ThreadInterruption

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
        lastReportTime
    }
    {
        if (acb.mayReportCurrentFile(threadIdx, lastReportTime))
            acb.reportCurrentFile(AFS::getDisplayPath(baseFolderKey.folderPath)); //just in case first directory access is blocking
    }

private:
    TraverserConfig travCfg_;
};


void DirCallback::onFile(const AFS::FileInfo& fi) //throw ThreadInterruption
{
    interruptionPoint(); //throw ThreadInterruption

    const Zstring fileRelPath = parentRelPathPf_ + fi.itemName;

    warn_static("why call reportCurrentFile() per file at all? should be sufficient to do per folder only!")

    //update status information no matter whether item is excluded or not!
    if (cfg_.acb.mayReportCurrentFile(cfg_.threadIdx, cfg_.lastReportTime))
        cfg_.acb.reportCurrentFile(AFS::getDisplayPath(AFS::appendRelPath(cfg_.baseFolderPath, fileRelPath)));

    //------------------------------------------------------------------------------------
    //apply filter before processing (use relative name!)
    if (!cfg_.filter->passFileFilter(fileRelPath))
        return;

    //sync.ffs_db database and lock files are excluded via filter!

    //    std::string fileId = details.fileSize >=  1024 * 1024U ? util::retrieveFileID(filepath) : std::string();
    /*
    Perf test Windows 7, SSD, 350k files, 50k dirs, files > 1MB: 7000
        regular:            6.9s
        ID per file:       43.9s
        ID per file > 1MB:  7.2s
        ID per dir:         8.4s

        Linux: retrieveFileID takes about 50% longer in VM! (avoidable because of redundant stat() call!)
    */

    output_.addSubFile(fi.itemName, FileAttributes(fi.modTime, fi.fileSize, fi.fileId, fi.symlinkInfo != nullptr));

    cfg_.acb.incItemsScanned(); //add 1 element to the progress indicator
}


std::shared_ptr<AFS::TraverserCallback> DirCallback::onFolder(const AFS::FolderInfo& fi) //throw ThreadInterruption
{
    interruptionPoint(); //throw ThreadInterruption

    const Zstring& folderRelPath = parentRelPathPf_ + fi.itemName;

    //update status information no matter whether item is excluded or not!
    if (cfg_.acb.mayReportCurrentFile(cfg_.threadIdx, cfg_.lastReportTime))
        cfg_.acb.reportCurrentFile(AFS::getDisplayPath(AFS::appendRelPath(cfg_.baseFolderPath, folderRelPath)));

    //------------------------------------------------------------------------------------
    //apply filter before processing (use relative name!)
    bool childItemMightMatch = true;
    const bool passFilter = cfg_.filter->passDirFilter(folderRelPath, &childItemMightMatch);
    if (!passFilter && !childItemMightMatch)
        return nullptr; //do NOT traverse subdirs
    //else: attention! ensure directory filtering is applied later to exclude actually filtered directories

    FolderContainer& subFolder = output_.addSubFolder(fi.itemName, fi.symlinkInfo != nullptr);
    if (passFilter)
        cfg_.acb.incItemsScanned(); //add 1 element to the progress indicator

    //------------------------------------------------------------------------------------
    if (level_ > 100) //Win32 traverser: stack overflow approximately at level 1000
        //check after FolderContainer::addSubFolder()
        for (size_t retryNumber = 0;; ++retryNumber)
            switch (reportItemError(replaceCpy(_("Cannot read directory %x."), L"%x", AFS::getDisplayPath(AFS::appendRelPath(cfg_.baseFolderPath, folderRelPath))) +
                                    L"\n\n" L"Endless recursion.", retryNumber, fi.itemName)) //throw ThreadInterruption
            {
                case AbstractFileSystem::TraverserCallback::ON_ERROR_RETRY:
                    break;
                case AbstractFileSystem::TraverserCallback::ON_ERROR_CONTINUE:
                    return nullptr;
            }

    return std::make_shared<DirCallback>(cfg_, folderRelPath + FILE_NAME_SEPARATOR, subFolder, level_ + 1);
}


DirCallback::HandleLink DirCallback::onSymlink(const AFS::SymlinkInfo& si) //throw ThreadInterruption
{
    interruptionPoint(); //throw ThreadInterruption

    const Zstring& linkRelPath = parentRelPathPf_ + si.itemName;

    //update status information no matter whether item is excluded or not!
    if (cfg_.acb.mayReportCurrentFile(cfg_.threadIdx, cfg_.lastReportTime))
        cfg_.acb.reportCurrentFile(AFS::getDisplayPath(AFS::appendRelPath(cfg_.baseFolderPath, linkRelPath)));

    switch (cfg_.handleSymlinks)
    {
        case SymLinkHandling::EXCLUDE:
            return LINK_SKIP;

        case SymLinkHandling::DIRECT:
            if (cfg_.filter->passFileFilter(linkRelPath)) //always use file filter: Link type may not be "stable" on Linux!
            {
                output_.addSubLink(si.itemName, LinkAttributes(si.modTime));
                cfg_.acb.incItemsScanned(); //add 1 element to the progress indicator
            }
            return LINK_SKIP;

        case SymLinkHandling::FOLLOW:
            //filter symlinks before trying to follow them: handle user-excluded broken symlinks!
            //since we don't know yet what type the symlink will resolve to, only do this when both filter variants agree:
            if (!cfg_.filter->passFileFilter(linkRelPath))
            {
                bool childItemMightMatch = true;
                if (!cfg_.filter->passDirFilter(linkRelPath, &childItemMightMatch))
                    if (!childItemMightMatch)
                        return LINK_SKIP;
            }
            return LINK_FOLLOW;
    }

    assert(false);
    return LINK_SKIP;
}


DirCallback::HandleError DirCallback::reportError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName /*optional*/) //throw ThreadInterruption
{
    switch (cfg_.acb.reportError(msg, retryNumber)) //throw ThreadInterruption
    {
        case ON_ERROR_CONTINUE:
            if (itemName.empty())
                cfg_.failedDirReads.emplace(beforeLast(parentRelPathPf_, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE), msg);
            else
                cfg_.failedItemReads.emplace(parentRelPathPf_ + itemName, msg);
            return ON_ERROR_CONTINUE;

        case ON_ERROR_RETRY:
            return ON_ERROR_RETRY;
    }
    assert(false);
    return ON_ERROR_CONTINUE;
}
}


void fff::parallelDeviceTraversal(const std::set<DirectoryKey>& foldersToRead,
                                  std::map<DirectoryKey, DirectoryValue>& output,
                                  const std::map<AbstractPath, size_t>& deviceParallelOps,
                                  const TravErrorCb& onError, const TravStatusCb& onStatusUpdate,
                                  std::chrono::milliseconds cbInterval)
{
    output.clear();

    //aggregate folder paths that are on the same root device:
    // => one worker thread *per device*: avoid excessive parallelism
    // => parallel folder traversal considers "parallel file operations" as specified by user
    // => (S)FTP: avoid hitting connection limits inadvertently
    std::map<AbstractPath, std::set<DirectoryKey>> perDeviceFolders;

    for (const DirectoryKey& key : foldersToRead)
        perDeviceFolders[AFS::getRootPath(key.folderPath)].insert(key);

    //communication channel used by threads
    AsyncCallback acb(perDeviceFolders.size() /*threadsToFinish*/, cbInterval); //manage life time: enclose InterruptibleThread's!!!

    std::vector<InterruptibleThread> worker;
    ZEN_ON_SCOPE_EXIT( for (InterruptibleThread& wt : worker) wt.join     (); );
    ZEN_ON_SCOPE_FAIL( for (InterruptibleThread& wt : worker) wt.interrupt(); ); //interrupt all first, then join

    //init worker threads
    for (const auto& item : perDeviceFolders)
    {
        const AbstractPath& rootPath = item.first;
        const int threadIdx = static_cast<int>(worker.size());
        const size_t parallelOps = getDeviceParallelOps(deviceParallelOps, rootPath);

        std::map<DirectoryKey, DirectoryValue*> workload;

        for (const DirectoryKey& key : item.second)
            workload.emplace(key, &output[key]); //=> DirectoryValue* unshared for lock-free worker-thread access

        worker.emplace_back([rootPath, workload, threadIdx, &acb, parallelOps]() mutable
        {
            setCurrentThreadName(("Comp Worker[" + numberTo<std::string>(threadIdx) + "]").c_str());

            acb.notifyWorkBegin(threadIdx, parallelOps);
            ZEN_ON_SCOPE_EXIT(acb.notifyWorkEnd(threadIdx));

            std::chrono::steady_clock::time_point lastReportTime; //keep thread-local!

            AFS::TraverserWorkload travWorkload;

            for (auto& wl : workload)
            {
                const std::vector<Zstring> relPath = split(AFS::getRootRelativePath(wl.first.folderPath), FILE_NAME_SEPARATOR, SplitType::SKIP_EMPTY);
                assert(AFS::getRootPath(wl.first.folderPath) == rootPath);
                travWorkload.emplace_back(relPath, std::make_shared<BaseDirCallback>(wl.first, *wl.second, acb, threadIdx, lastReportTime));
            }
            AFS::traverseFolderRecursive(rootPath, travWorkload, parallelOps); //throw ThreadInterruption
        });
    }

    acb.waitUntilDone(cbInterval, onError, onStatusUpdate); //throw X
}

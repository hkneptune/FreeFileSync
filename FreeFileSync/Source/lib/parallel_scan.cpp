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
#include <zen/fixed_list.h>
#include "db_file.h"
#include "lock_holder.h"

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

    std::vector<char> buffer(sizeof(VOLUME_DISK_EXTENTS) + sizeof(DISK_EXTENT)); //reserve buffer for at most one disk! call below will then fail if volume spans multiple disks!

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
using BasicWString = Zbase<wchar_t>; //thread-safe string class for UI texts


class AsyncCallback //actor pattern
{
public:
    AsyncCallback(std::chrono::milliseconds cbInterval) : cbInterval_(cbInterval) {}

    //blocking call: context of worker thread
    FillBufferCallback::HandleError reportError(const std::wstring& msg, size_t retryNumber) //throw ThreadInterruption
    {
        std::unique_lock<std::mutex> dummy(lockErrorInfo_);
        interruptibleWait(conditionCanReportError_, dummy, [this] { return !errorInfo_ && !errorResponse_; }); //throw ThreadInterruption

        errorInfo_ = std::make_unique<std::pair<BasicWString, size_t>>(copyStringTo<BasicWString>(msg), retryNumber);

        interruptibleWait(conditionGotResponse_, dummy, [this] { return static_cast<bool>(errorResponse_); }); //throw ThreadInterruption

        FillBufferCallback::HandleError rv = *errorResponse_;

        errorInfo_    .reset();
        errorResponse_.reset();

        dummy.unlock(); //optimization for condition_variable::notify_all()
        conditionCanReportError_.notify_all(); //instead of notify_one(); workaround bug: https://svn.boost.org/trac/boost/ticket/7796

        return rv;
    }

    //context of main thread, call repreatedly
    void processErrors(FillBufferCallback& callback)
    {
        std::unique_lock<std::mutex> dummy(lockErrorInfo_);
        if (errorInfo_.get() && !errorResponse_.get())
        {
            FillBufferCallback::HandleError rv = callback.reportError(copyStringTo<std::wstring>(errorInfo_->first), errorInfo_->second); //throw!
            errorResponse_ = std::make_unique<FillBufferCallback::HandleError>(rv);

            dummy.unlock(); //optimization for condition_variable::notify_all()
            conditionGotResponse_.notify_all(); //instead of notify_one(); workaround bug: https://svn.boost.org/trac/boost/ticket/7796
        }
    }

    void incrementNotifyingThreadId() { ++notifyingThreadID_; } //context of main thread

    //perf optimization: comparison phase is 7% faster by avoiding needless std::wstring contstruction for reportCurrentFile()
    bool mayReportCurrentFile(int threadID, std::chrono::steady_clock::time_point& lastReportTime) const
    {
        if (threadID != notifyingThreadID_) //only one thread at a time may report status
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

    void reportCurrentFile(const std::wstring& filepath) //context of worker thread
    {
        std::lock_guard<std::mutex> dummy(lockCurrentStatus_);
        currentFile_ = copyStringTo<BasicWString>(filepath);
    }

    std::wstring getCurrentStatus() //context of main thread, call repreatedly
    {
        std::wstring filepath;
        {
            std::lock_guard<std::mutex> dummy(lockCurrentStatus_);
            filepath = copyStringTo<std::wstring>(currentFile_);
        }

        if (filepath.empty())
            return std::wstring();

        std::wstring statusText = copyStringTo<std::wstring>(textScanning_);

        const long activeCount = activeWorker_;
        if (activeCount >= 2)
            statusText += L" [" + _P("1 thread", "%x threads", activeCount) + L"]";

        statusText += L" ";
        statusText += filepath;
        return statusText;
    }

    void incItemsScanned() { ++itemsScanned_; } //perf: irrelevant! scanning is almost entirely file I/O bound, not CPU bound! => no prob having multiple threads poking at the same variable!
    long getItemsScanned() const { return itemsScanned_; }

    void incActiveWorker() { ++activeWorker_; }
    void decActiveWorker() { --activeWorker_; }
    long getActiveWorker() const { return activeWorker_; }

private:
    //---- error handling ----
    std::mutex lockErrorInfo_;
    std::condition_variable conditionCanReportError_;
    std::condition_variable conditionGotResponse_;
    std::unique_ptr<std::pair<BasicWString, size_t>> errorInfo_; //error message + retry number
    std::unique_ptr<FillBufferCallback::HandleError> errorResponse_;

    //---- status updates ----
    std::atomic<int> notifyingThreadID_ { 0 }; //CAVEAT: do NOT use boost::thread::id: https://svn.boost.org/trac/boost/ticket/5754

    std::mutex lockCurrentStatus_; //use a different lock for current file: continue traversing while some thread may process an error
    BasicWString currentFile_;
    const std::chrono::milliseconds cbInterval_;

    const BasicWString textScanning_ { copyStringTo<BasicWString>(_("Scanning:")) }; //this one is (currently) not shared and could be made a std::wstring, but we stay consistent and use thread-safe variables in this class only!

    //---- status updates II (lock free) ----
    std::atomic<int> itemsScanned_{ 0 }; //std:atomic is uninitialized by default!
    std::atomic<int> activeWorker_{ 0 }; //
};

//-------------------------------------------------------------------------------------------------

struct TraverserConfig
{
public:
    TraverserConfig(int threadID,
                    const AbstractPath& baseFolderPath,
                    const HardFilter::FilterRef& filter,
                    SymLinkHandling handleSymlinks,
                    std::map<Zstring, std::wstring, LessFilePath>& failedFolderReads,
                    std::map<Zstring, std::wstring, LessFilePath>& failedItemReads,
                    AsyncCallback& acb) :
        baseFolderPath_(baseFolderPath),
        filter_(filter),
        handleSymlinks_(handleSymlinks),
        failedDirReads_ (failedFolderReads),
        failedItemReads_(failedItemReads),
        acb_(acb),
        threadID_(threadID) {}

    const AbstractPath baseFolderPath_;
    const HardFilter::FilterRef filter_; //always bound!
    const SymLinkHandling handleSymlinks_;

    std::map<Zstring, std::wstring, LessFilePath>& failedDirReads_;
    std::map<Zstring, std::wstring, LessFilePath>& failedItemReads_;

    AsyncCallback& acb_;
    const int threadID_;
    std::chrono::steady_clock::time_point lastReportTime_;
};


class DirCallback : public AFS::TraverserCallback
{
public:
    DirCallback(TraverserConfig& config,
                const Zstring& parentRelPathPf, //postfixed with FILE_NAME_SEPARATOR!
                FolderContainer& output,
                int level) :
        cfg(config),
        parentRelPathPf_(parentRelPathPf),
        output_(output),
        level_(level) {}

    virtual void                               onFile   (const FileInfo&    fi) override; //
    virtual std::unique_ptr<TraverserCallback> onFolder (const FolderInfo&  fi) override; //throw ThreadInterruption
    virtual HandleLink                         onSymlink(const SymlinkInfo& li) override; //

    HandleError reportDirError (const std::wstring& msg, size_t retryNumber)                          override; //throw ThreadInterruption
    HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName) override; //

private:
    TraverserConfig& cfg;
    const Zstring parentRelPathPf_;
    FolderContainer& output_;
    const int level_;
};


void DirCallback::onFile(const FileInfo& fi) //throw ThreadInterruption
{
    interruptionPoint(); //throw ThreadInterruption

    //do not list the database file(s) sync.ffs_db, sync.x64.ffs_db, etc. or lock files
    if (endsWith(fi.itemName, SYNC_DB_FILE_ENDING) ||
        endsWith(fi.itemName, LOCK_FILE_ENDING))
        return;

    const Zstring fileRelPath = parentRelPathPf_ + fi.itemName;

    //update status information no matter whether item is excluded or not!
    if (cfg.acb_.mayReportCurrentFile(cfg.threadID_, cfg.lastReportTime_))
        cfg.acb_.reportCurrentFile(AFS::getDisplayPath(AFS::appendRelPath(cfg.baseFolderPath_, fileRelPath)));

    //------------------------------------------------------------------------------------
    //apply filter before processing (use relative name!)
    if (!cfg.filter_->passFileFilter(fileRelPath))
        return;

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

    cfg.acb_.incItemsScanned(); //add 1 element to the progress indicator
}


std::unique_ptr<AFS::TraverserCallback> DirCallback::onFolder(const FolderInfo& fi) //throw ThreadInterruption
{
    interruptionPoint(); //throw ThreadInterruption

    const Zstring& folderRelPath = parentRelPathPf_ + fi.itemName;

    //update status information no matter whether item is excluded or not!
    if (cfg.acb_.mayReportCurrentFile(cfg.threadID_, cfg.lastReportTime_))
        cfg.acb_.reportCurrentFile(AFS::getDisplayPath(AFS::appendRelPath(cfg.baseFolderPath_, folderRelPath)));

    //------------------------------------------------------------------------------------
    //apply filter before processing (use relative name!)
    bool childItemMightMatch = true;
    const bool passFilter = cfg.filter_->passDirFilter(folderRelPath, &childItemMightMatch);
    if (!passFilter && !childItemMightMatch)
        return nullptr; //do NOT traverse subdirs
    //else: attention! ensure directory filtering is applied later to exclude actually filtered directories

    FolderContainer& subFolder = output_.addSubFolder(fi.itemName, fi.symlinkInfo != nullptr);
    if (passFilter)
        cfg.acb_.incItemsScanned(); //add 1 element to the progress indicator

    //------------------------------------------------------------------------------------
    if (level_ > 100) //Win32 traverser: stack overflow approximately at level 1000
        //check after FolderContainer::addSubFolder()
        if (!tryReportingItemError([&] //throw ThreadInterruption
    {
        throw FileError(replaceCpy(_("Cannot read directory %x."), L"%x", AFS::getDisplayPath(AFS::appendRelPath(cfg.baseFolderPath_, folderRelPath))), L"Endless recursion.");
        }, *this, fi.itemName))
    return nullptr;

    return std::make_unique<DirCallback>(cfg, folderRelPath + FILE_NAME_SEPARATOR, subFolder, level_ + 1);
}


DirCallback::HandleLink DirCallback::onSymlink(const SymlinkInfo& si) //throw ThreadInterruption
{
    interruptionPoint(); //throw ThreadInterruption

    const Zstring& linkRelPath = parentRelPathPf_ + si.itemName;

    //update status information no matter whether item is excluded or not!
    if (cfg.acb_.mayReportCurrentFile(cfg.threadID_, cfg.lastReportTime_))
        cfg.acb_.reportCurrentFile(AFS::getDisplayPath(AFS::appendRelPath(cfg.baseFolderPath_, linkRelPath)));

    switch (cfg.handleSymlinks_)
    {
        case SymLinkHandling::EXCLUDE:
            return LINK_SKIP;

        case SymLinkHandling::DIRECT:
            if (cfg.filter_->passFileFilter(linkRelPath)) //always use file filter: Link type may not be "stable" on Linux!
            {
                output_.addSubLink(si.itemName, LinkAttributes(si.modTime));
                cfg.acb_.incItemsScanned(); //add 1 element to the progress indicator
            }
            return LINK_SKIP;

        case SymLinkHandling::FOLLOW:
            //filter symlinks before trying to follow them: handle user-excluded broken symlinks!
            //since we don't know yet what type the symlink will resolve to, only do this when both variants agree:
            if (!cfg.filter_->passFileFilter(linkRelPath))
            {
                bool childItemMightMatch = true;
                if (!cfg.filter_->passDirFilter(linkRelPath, &childItemMightMatch))
                    if (!childItemMightMatch)
                        return LINK_SKIP;
            }
            return LINK_FOLLOW;
    }

    assert(false);
    return LINK_SKIP;
}


DirCallback::HandleError DirCallback::reportDirError(const std::wstring& msg, size_t retryNumber) //throw ThreadInterruption
{
    switch (cfg.acb_.reportError(msg, retryNumber)) //throw ThreadInterruption
    {
        case FillBufferCallback::ON_ERROR_CONTINUE:
            cfg.failedDirReads_[beforeLast(parentRelPathPf_, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE)] = msg;
            return ON_ERROR_CONTINUE;

        case FillBufferCallback::ON_ERROR_RETRY:
            return ON_ERROR_RETRY;
    }
    assert(false);
    return ON_ERROR_CONTINUE;
}


DirCallback::HandleError DirCallback::reportItemError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName) //throw ThreadInterruption
{
    switch (cfg.acb_.reportError(msg, retryNumber)) //throw ThreadInterruption
    {
        case FillBufferCallback::ON_ERROR_CONTINUE:
            cfg.failedItemReads_[parentRelPathPf_ + itemName] =  msg;
            return ON_ERROR_CONTINUE;

        case FillBufferCallback::ON_ERROR_RETRY:
            return ON_ERROR_RETRY;
    }
    assert(false);
    return ON_ERROR_CONTINUE;
}

//------------------------------------------------------------------------------------------

class WorkerThread
{
public:
    WorkerThread(int threadID,
                 const std::shared_ptr<AsyncCallback>& acb,
                 const AbstractPath& baseFolderPath,  //always bound!
                 const HardFilter::FilterRef& filter, //
                 SymLinkHandling handleSymlinks,
                 DirectoryValue& dirOutput) :
        acb_(acb),
        outputContainer_(dirOutput.folderCont),
        travCfg_(threadID,
                 baseFolderPath,
                 filter,
                 handleSymlinks, //shared by all(!) instances of DirCallback while traversing a folder hierarchy
                 dirOutput.failedFolderReads,
                 dirOutput.failedItemReads,
                 *acb_) {}

    void operator()() //thread entry
    {
        setCurrentThreadName("Folder Traverser");

        acb_->incActiveWorker();
        ZEN_ON_SCOPE_EXIT(acb_->decActiveWorker());

        if (acb_->mayReportCurrentFile(travCfg_.threadID_, travCfg_.lastReportTime_))
            acb_->reportCurrentFile(AFS::getDisplayPath(travCfg_.baseFolderPath_)); //just in case first directory access is blocking

        DirCallback cb(travCfg_, Zstring(), outputContainer_, 0);

        AFS::traverseFolder(travCfg_.baseFolderPath_, cb); //throw ThreadInterruption
    }

private:
    std::shared_ptr<AsyncCallback> acb_;
    FolderContainer& outputContainer_;
    TraverserConfig travCfg_;
};
}


void fff::fillBuffer(const std::set<DirectoryKey>& keysToRead, //in
                     std::map<DirectoryKey, DirectoryValue>& buf, //out
                     FillBufferCallback& callback,
                     std::chrono::milliseconds cbInterval)
{
    buf.clear();

    FixedList<InterruptibleThread> worker;

    ZEN_ON_SCOPE_FAIL
    (
        for (InterruptibleThread& wt : worker)
        wt.interrupt(); //interrupt all first, then join
        for (InterruptibleThread& wt : worker)
            if (wt.joinable()) //= precondition of thread::join(), which throws an exception if violated!
                wt.join();     //in this context it is possible a thread is *not* joinable anymore due to the thread::try_join_for() below!
            );

    auto acb = std::make_shared<AsyncCallback>(cbInterval);

    //init worker threads
    for (const DirectoryKey& key : keysToRead)
    {
        assert(buf.find(key) == buf.end());
        DirectoryValue& dirOutput = buf[key];

        const int threadId = static_cast<int>(worker.size());
        worker.emplace_back(WorkerThread(threadId,
                                         acb,
                                         key.folderPath, //AbstractPath is thread-safe like an int! :)
                                         key.filter,
                                         key.handleSymlinks,
                                         dirOutput));
    }

    //wait until done
    for (InterruptibleThread& wt : worker)
    {
        do
        {
            //update status
            callback.reportStatus(acb->getCurrentStatus(), acb->getItemsScanned()); //throw!

            //process errors
            acb->processErrors(callback);
        }
        while (!wt.tryJoinFor(cbInterval));

        acb->incrementNotifyingThreadId(); //process info messages of one thread at a time only
    }
}

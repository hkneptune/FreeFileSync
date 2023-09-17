// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "dir_lock.h"
#include <memory>
#include <unordered_map>
#include <zen/crc.h>
#include <zen/sys_error.h>
#include <zen/thread.h>
#include <zen/scope_guard.h>
#include <zen/guid.h>
#include <zen/file_access.h>
#include <zen/file_io.h>
#include <zen/sys_info.h>

    #include <iostream> //std::cerr
    #include <fcntl.h>  //open()
    #include <unistd.h> //close()
    #include <signal.h> //kill()

using namespace zen;
using namespace fff;


namespace
{
constexpr std::chrono::seconds EMIT_LIFE_SIGN_INTERVAL   (5); //show life sign;
constexpr std::chrono::seconds POLL_LIFE_SIGN_INTERVAL   (4); //poll for life sign;
constexpr std::chrono::seconds DETECT_ABANDONED_INTERVAL(30); //assume abandoned lock;

const char LOCK_FILE_DESCR[] = "FreeFileSync";
const int LOCK_FILE_VERSION = 3; //2020-02-07
const int ABANDONED_LOCK_LEVEL_MAX = 10;
}


Zstring fff::impl::getAbandonedLockFileName(const Zstring& lockFileName) //throw SysError
{
    Zstring fileName = lockFileName;
    int level = 0;

    //recursive abandoned locks!? (almost) impossible, except for file system bugs: https://freefilesync.org/forum/viewtopic.php?t=6568
    const Zstring tmp = afterFirst(fileName, Zstr("Delete."), IfNotFoundReturn::none); //e.g. Delete.1.sync.ffs_lock
    if (!tmp.empty())
    {
        const Zstring levelStr = beforeFirst(tmp, Zstr('.'), IfNotFoundReturn::none);
        if (!levelStr.empty() && std::all_of(levelStr.begin(), levelStr.end(), [](Zchar c) { return zen::isDigit(c); }))
        {
            fileName = afterFirst(tmp, Zstr('.'), IfNotFoundReturn::none);
            level = stringTo<int>(levelStr) + 1;

            if (level >= ABANDONED_LOCK_LEVEL_MAX)
                throw SysError(L"Endless recursion.");
        }
    }

    return Zstr("Delete.") + numberTo<Zstring>(level) + Zstr(".") + fileName; //preserve lock file extension!
}


namespace
{
//worker thread
class LifeSigns
{
public:
    LifeSigns(const Zstring& lockFilePath) : lockFilePath_(lockFilePath)
    {
    }

    void operator()() const //throw ThreadStopRequest
    {
        const std::optional<Zstring> parentDirPath = getParentFolderPath(lockFilePath_);
        setCurrentThreadName(Zstr("DirLock: ") + (parentDirPath ? *parentDirPath : Zstr("")));

        for (;;)
        {
            interruptibleSleep(EMIT_LIFE_SIGN_INTERVAL); //throw ThreadStopRequest
            emitLifeSign(); //noexcept
        }
    }

private:
    //try to append one byte...
    void emitLifeSign() const //noexcept
    {
        try
        {
#if 1
            const int fdLockFile = ::open(lockFilePath_.c_str(), O_WRONLY | O_APPEND | O_CLOEXEC);
            if (fdLockFile == -1)
                THROW_LAST_SYS_ERROR("open");
            ZEN_ON_SCOPE_EXIT(::close(fdLockFile));

#else //alternative using lseek => no apparent benefit https://freefilesync.org/forum/viewtopic.php?t=7553#p25505
            const int fdLockFile = ::open(lockFilePath_.c_str(), O_WRONLY | O_CLOEXEC);
            if (fdLockFile == -1)
                THROW_LAST_SYS_ERROR("open");
            ZEN_ON_SCOPE_EXIT(::close(fdLockFile));

            if (const off_t offset = ::lseek(fdLockFile, 0, SEEK_END);
                offset == -1)
                THROW_LAST_SYS_ERROR("lseek");
#endif
            const ssize_t bytesWritten = ::write(fdLockFile, " ", 1); //writes *up to* count bytes
            if (bytesWritten <= 0)
            {
                if (bytesWritten == 0) //comment in safe-read.c suggests to treat this as an error due to buggy drivers
                    errno = ENOSPC;
                THROW_LAST_SYS_ERROR("write");
            }
            ASSERT_SYSERROR(bytesWritten == 1); //better safe than sorry
        }
        catch (const SysError& e)
        {
            logExtraError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(lockFilePath_)) + L"\n\n" + e.toString());
        }
    }

    const Zstring lockFilePath_; //thread-local!
};


    using ProcessId = pid_t;
    using SessionId = pid_t;

//return ppid on Windows, sid on Linux/Mac, "no value" if process corresponding to "processId" is not existing
std::optional<SessionId> getSessionId(ProcessId processId) //throw FileError
{
    if (::kill(processId, 0) != 0) //sig == 0: no signal sent, just existence check
        return {};

    const pid_t procSid = ::getsid(processId); //NOT to be confused with "login session", e.g. not stable on OS X!!!
    if (procSid < 0) //pids are never negative, empiric proof: https://linux.die.net/man/2/wait
        THROW_LAST_FILE_ERROR(_("Cannot get process information."), "getsid");

    return procSid;
}


struct LockInformation //throw FileError
{
    std::string lockId; //16 byte GUID - a universal identifier for this lock (no matter what the path is, considering symlinks, distributed network, etc.)

    //identify local computer
    std::string computerName; //format: HostName.DomainName
    std::string userId;

    //identify running process
    SessionId sessionId = 0; //Windows: parent process id; Linux/macOS: session of the process, NOT the user
    ProcessId processId = 0;
};


LockInformation getLockInfoFromCurrentProcess() //throw FileError
{
    LockInformation lockInfo =
    {
        .lockId = generateGUID(),
        .userId = utfTo<std::string>(getLoginUser()), //throw FileError
    };

    const std::string osName = "Linux";

    //wxGetFullHostName() is a performance killer and can hang for some users, so don't touch!
    std::vector<char> buf(10000);
    if (::gethostname(buf.data(), buf.size()) != 0)
        THROW_LAST_FILE_ERROR(_("Cannot get process information."), "gethostname");
    lockInfo.computerName = osName + ' ' + buf.data() + '.';

    if (::getdomainname(buf.data(), buf.size()) != 0)
        THROW_LAST_FILE_ERROR(_("Cannot get process information."), "getdomainname");
    lockInfo.computerName += buf.data(); //can be "(none)"!

    lockInfo.processId = ::getpid(); //never fails

    std::optional<SessionId> sessionIdTmp = getSessionId(lockInfo.processId); //throw FileError
    if (!sessionIdTmp)
        throw FileError(_("Cannot get process information."), L"no session id found"); //should not happen?
    lockInfo.sessionId = *sessionIdTmp;

    return lockInfo;
}


std::string serialize(const LockInformation& lockInfo)
{
    MemoryStreamOut streamOut;
    writeArray(streamOut, LOCK_FILE_DESCR, sizeof(LOCK_FILE_DESCR));
    writeNumber<int32_t>(streamOut, LOCK_FILE_VERSION);

    static_assert(sizeof(lockInfo.processId) <= sizeof(uint64_t)); //ensure cross-platform compatibility!
    static_assert(sizeof(lockInfo.sessionId) <= sizeof(uint64_t)); //

    writeContainer(streamOut, lockInfo.lockId);
    writeContainer(streamOut, lockInfo.computerName);
    writeContainer(streamOut, lockInfo.userId);
    writeNumber<uint64_t>(streamOut, lockInfo.sessionId);
    writeNumber<uint64_t>(streamOut, lockInfo.processId);

    writeNumber<uint32_t>(streamOut, getCrc32(streamOut.ref()));
    writeArray(streamOut, "x", 1); //sentinel: mark logical end with a non-space character
    return streamOut.ref();
}


LockInformation unserialize(const std::string& byteStream) //throw SysError
{
    MemoryStreamIn streamIn(byteStream);

    char formatDescr[sizeof(LOCK_FILE_DESCR)] = {};
    readArray(streamIn, &formatDescr, sizeof(formatDescr)); //throw SysErrorUnexpectedEos

    if (!std::equal(std::begin(formatDescr), std::end(formatDescr), std::begin(LOCK_FILE_DESCR)))
        throw SysError(_("File content is corrupted.") + L" (invalid header)");

    const int version = readNumber<int32_t>(streamIn); //throw SysErrorUnexpectedEos
    if (version != LOCK_FILE_VERSION)
        throw SysError(_("Unsupported data format.") + L' ' + replaceCpy(_("Version: %x"), L"%x", numberTo<std::wstring>(version)));

    //--------------------------------------------------------------------
    //catch data corruption ASAP + don't rely on std::bad_alloc for consistency checking
    const size_t posEnd = byteStream.rfind('x'); //skip blanks (+ unrelated corrupted data e.g. nulls!)
    if (posEnd == std::string::npos)
        throw SysErrorUnexpectedEos();

    const std::string_view byteStreamTrm = makeStringView(byteStream.begin(), posEnd);

    MemoryStreamOut crcStreamOut;
    writeNumber<uint32_t>(crcStreamOut, getCrc32(byteStreamTrm.begin(), byteStreamTrm.end() - sizeof(uint32_t)));

    if (!endsWith(byteStreamTrm, crcStreamOut.ref()))
        throw SysError(_("File content is corrupted.") + L" (invalid checksum)");
    //--------------------------------------------------------------------

    LockInformation lockInfo = {};
    lockInfo.lockId       = readContainer<std::string>(streamIn); //
    lockInfo.computerName = readContainer<std::string>(streamIn); //SysErrorUnexpectedEos
    lockInfo.userId       = readContainer<std::string>(streamIn); //
    lockInfo.sessionId    = static_cast<SessionId>(readNumber<uint64_t>(streamIn)); //[!] conversion
    lockInfo.processId    = static_cast<ProcessId>(readNumber<uint64_t>(streamIn)); //[!] conversion
    return lockInfo;
}


LockInformation retrieveLockInfo(const Zstring& lockFilePath) //throw FileError
{
    const std::string byteStream = getFileContent(lockFilePath, nullptr /*notifyUnbufferedIO*/); //throw FileError
    try
    {
        return unserialize(byteStream); //throw SysError
    }
    catch (const SysError& e)
    {
        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(lockFilePath)), e.toString());
    }
}


inline
std::string retrieveLockId(const Zstring& lockFilePath) //throw FileError
{
    return retrieveLockInfo(lockFilePath).lockId; //throw FileError
}


enum class ProcessStatus
{
    notRunning,
    running,
    itsUs,
    noIdea,
};

ProcessStatus getProcessStatus(const LockInformation& lockInfo) //throw FileError
{
    const LockInformation localInfo = getLockInfoFromCurrentProcess(); //throw FileError

    if (lockInfo.computerName != localInfo.computerName ||
        lockInfo.userId != localInfo.userId) //another user may run a session right now!
        return ProcessStatus::noIdea; //lock owned by different computer in this network

    if (lockInfo.sessionId == localInfo.sessionId &&
        lockInfo.processId == localInfo.processId) //obscure, but possible: deletion failed or a lock file is "stolen" and put back while the program is running
        return ProcessStatus::itsUs;

    if (std::optional<SessionId> sessionId = getSessionId(lockInfo.processId)) //throw FileError
        return *sessionId == lockInfo.sessionId ? ProcessStatus::running : ProcessStatus::notRunning;
    return ProcessStatus::notRunning;
}


DEFINE_NEW_FILE_ERROR(ErrorFileNotExisting)
uint64_t getLockFileSize(const Zstring& filePath) //throw FileError, ErrorFileNotExisting
{
    struct stat fileInfo = {};
    if (::stat(filePath.c_str(), &fileInfo) == 0)
        return fileInfo.st_size;

    if (errno == ENOENT)
        throw ErrorFileNotExisting(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), formatSystemError("stat", errno));
    THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), "stat");
}


void waitOnDirLock(const Zstring& lockFilePath, const DirLockCallback& notifyStatus /*throw X*/, std::chrono::milliseconds cbInterval) //throw FileError
{
    std::wstring infoMsg = _("Waiting while directory is in use:") + L' ' + fmtPath(lockFilePath);

    if (notifyStatus) notifyStatus(std::wstring(infoMsg)); //throw X

    //convenience optimization only: if we know the owning process crashed, we needn't wait DETECT_ABANDONED_INTERVAL sec
    bool lockOwnderDead = false;
    std::string originalLockId; //empty if it cannot be retrieved
    try
    {
        const LockInformation& lockInfo = retrieveLockInfo(lockFilePath); //throw FileError

        infoMsg += SPACED_DASH + _("Username:") +  L' ' + utfTo<std::wstring>(lockInfo.userId);

        originalLockId = lockInfo.lockId;
        switch (getProcessStatus(lockInfo)) //throw FileError
        {
            case ProcessStatus::itsUs: //since we've already passed LockAdmin, the lock file seems abandoned ("stolen"?) although it's from this process
            case ProcessStatus::notRunning:
                lockOwnderDead = true;
                break;
            case ProcessStatus::running:
            case ProcessStatus::noIdea:
                break;
        }
    }
    catch (FileError&) {} //logfile may be only partly written -> this is no error!
    //------------------------------------------------------------------------------

    uint64_t fileSizeOld = 0;
    auto lastLifeSign = std::chrono::steady_clock::now();

    for (;;)
    {
        uint64_t fileSizeNew = 0;
        try
        {
            fileSizeNew = getLockFileSize(lockFilePath); //throw FileError, ErrorFileNotExisting
        }
        catch (ErrorFileNotExisting&) { return; } //what we are waiting for...

        const auto lastCheckTime = std::chrono::steady_clock::now();

        if (fileSizeNew != fileSizeOld) //received life sign from lock
        {
            fileSizeOld  = fileSizeNew;
            lastLifeSign = lastCheckTime;
        }

        if (lockOwnderDead || //no need to wait any longer...
            lastCheckTime >= lastLifeSign + DETECT_ABANDONED_INTERVAL)
        {
            const Zstring lockFileName = [&]
            {
                try
                {
                    return fff::impl::getAbandonedLockFileName(getItemName(lockFilePath)); //throw SysError
                }
                catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete file %x."), L"%x", fmtPath(lockFilePath)), e.toString()); }
            }();

            DirLock guardDeletion(*getParentFolderPath(lockFilePath), lockFileName, notifyStatus, cbInterval); //throw FileError

            //now that the lock is in place check existence again: meanwhile another process may have deleted and created a new lock!
            std::string currentLockId;
            try { currentLockId = retrieveLockId(lockFilePath); /*throw FileError*/ }
            catch (FileError&) {}

            if (currentLockId != originalLockId)
                return; //another process has placed a new lock, leave scope: the wait for the old lock is technically over...

            try
            {
                if (getLockFileSize(lockFilePath) != fileSizeOld) //throw FileError, ErrorFileNotExisting
                    return; //late life sign (or maybe even a different lock if retrieveLockId() failed!)
            }
            catch (ErrorFileNotExisting&) { return; } //what we are waiting for anyway...

            removeFilePlain(lockFilePath); //throw FileError
            return;
        }

        //wait some time...
        const auto delayUntil = std::chrono::steady_clock::now() + POLL_LIFE_SIGN_INTERVAL;
        for (auto now = std::chrono::steady_clock::now(); now < delayUntil; now = std::chrono::steady_clock::now())
        {
            if (notifyStatus)
            {
                //one signal missed: it's likely this is an abandoned lock => show countdown
                if (lastCheckTime >= lastLifeSign + EMIT_LIFE_SIGN_INTERVAL + std::chrono::seconds(1))
                {
                    const int remainingSeconds = std::max(0, static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(DETECT_ABANDONED_INTERVAL - (now - lastLifeSign)).count()));
                    notifyStatus(infoMsg + SPACED_DASH + _("Lock file apparently abandoned...") + L' ' + _P("1 sec", "%x sec", remainingSeconds)); //throw X
                }
                else
                    notifyStatus(std::wstring(infoMsg)); //throw X; emit a message in any case (might clear other one)
            }
            std::this_thread::sleep_for(cbInterval);
        }
    }
}


void releaseLock(const Zstring& lockFilePath) { removeFilePlain(lockFilePath); } //throw FileError


bool tryLock(const Zstring& lockFilePath) //throw FileError
{
    //important: we want the lock file to have exactly the permissions specified
    //=> yes, disabling umask() is messy (per-process!), but fchmod() may not be supported: https://freefilesync.org/forum/viewtopic.php?t=8096
    const mode_t oldMask = ::umask(0); //always succeeds
    ZEN_ON_SCOPE_EXIT(::umask(oldMask));

    const mode_t lockFileMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; //0666

    //O_EXCL contains a race condition on NFS file systems: https://linux.die.net/man/2/open
    const int hFile = ::open(lockFilePath.c_str(),                    //const char* pathname
                             O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, //int flags
                             lockFileMode);                           //mode_t mode
    if (hFile == -1)
    {
        if (errno == EEXIST)
            return false;

        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(lockFilePath)), "open");
    }
    FileOutputPlain fileOut(hFile, lockFilePath); //pass handle ownership

    //write housekeeping info: user, process info, lock GUID
    const std::string byteStream = serialize(getLockInfoFromCurrentProcess()); //throw FileError

    unbufferedSave(byteStream, [&](const void* buffer, size_t bytesToWrite)
    {
        return fileOut.tryWrite(buffer, bytesToWrite); //throw FileError; may return short! CONTRACT: bytesToWrite > 0
    },
    fileOut.getBlockSize());

    fileOut.close(); //throw FileError
    return true;
}
}


class DirLock::SharedDirLock
{
public:
    SharedDirLock(const Zstring& lockFilePath, const DirLockCallback& notifyStatus, std::chrono::milliseconds cbInterval) : //throw FileError
        lockFilePath_(lockFilePath)
    {
        if (notifyStatus) notifyStatus(replaceCpy(_("Creating file %x"), L"%x", fmtPath(lockFilePath))); //throw X

        while (!::tryLock(lockFilePath)) //throw FileError
        {
            ::waitOnDirLock(lockFilePath, notifyStatus, cbInterval); //throw FileError
        }

        lifeSignthread_ = InterruptibleThread(LifeSigns(lockFilePath));
    }

    ~SharedDirLock()
    {
        lifeSignthread_.requestStop(); //thread lifetime is subset of this instances's life
        lifeSignthread_.join();

        try
        {
            ::releaseLock(lockFilePath_); //throw FileError
        }
        catch (const FileError& e) { logExtraError(e.toString()); } //inform user about remnant lock files *somehow*!
    }

private:
    SharedDirLock           (const DirLock&) = delete;
    SharedDirLock& operator=(const DirLock&) = delete;

    const Zstring lockFilePath_;
    InterruptibleThread lifeSignthread_;
};


class DirLock::LockAdmin //administrate all locks held by this process to avoid deadlock by recursion
{
public:
    static LockAdmin& instance()
    {
        static LockAdmin inst;
        return inst;
    }

    //create or retrieve a SharedDirLock
    std::shared_ptr<SharedDirLock> retrieve(const Zstring& lockFilePath, const DirLockCallback& notifyStatus, std::chrono::milliseconds cbInterval) //throw FileError
    {
        assert(runningOnMainThread()); //function is not thread-safe!

        tidyUp();

        //optimization: check if we already own a lock for this path
        if (auto itGuid = guidByPath_.find(lockFilePath);
            itGuid != guidByPath_.end())
            if (const std::shared_ptr<SharedDirLock>& activeLock = getActiveLock(itGuid->second)) //returns null-lock if not found
                return activeLock; //SharedDirLock is still active -> enlarge circle of shared ownership

        try //check based on lock GUID, deadlock prevention: "lockFilePath" may be an alternative name for a lock already owned by this process
        {
            const std::string lockId = retrieveLockId(lockFilePath); //throw FileError
            if (const std::shared_ptr<SharedDirLock>& activeLock = getActiveLock(lockId)) //returns null-lock if not found
            {
                guidByPath_[lockFilePath] = lockId; //found an alias for one of our active locks
                return activeLock;
            }
        }
        catch (FileError&) {} //catch everything, let SharedDirLock constructor deal with errors, e.g. 0-sized/corrupted lock files

        //lock not owned by us => create a new one
        auto newLock = std::make_shared<SharedDirLock>(lockFilePath, notifyStatus, cbInterval); //throw FileError
        const std::string& newLockGuid = retrieveLockId(lockFilePath); //throw FileError

        guidByPath_[lockFilePath] = newLockGuid; //update registry
        locksByGuid_[newLockGuid] = newLock;     //

        return newLock;
    }

private:
    LockAdmin() {}
    LockAdmin           (const LockAdmin&) = delete;
    LockAdmin& operator=(const LockAdmin&) = delete;

    using UniqueId = std::string;

    std::shared_ptr<SharedDirLock> getActiveLock(const UniqueId& lockId) //returns null if none found
    {
        auto it = locksByGuid_.find(lockId);
        return it != locksByGuid_.end() ? it->second.lock() : nullptr; //try to get shared_ptr; throw()
    }

    void tidyUp() //remove obsolete entries
    {
        std::erase_if(locksByGuid_, [](const auto& v) { return !v.second.lock(); });
        std::erase_if(guidByPath_, [&](const auto& v) { return !locksByGuid_.contains(v.second); });
    }

    std::unordered_map<Zstring, UniqueId> guidByPath_;                      //lockFilePath |-> GUID; n:1; locks can be referenced by a lockFilePath or alternatively a GUID
    std::unordered_map<UniqueId, std::weak_ptr<SharedDirLock>> locksByGuid_; //GUID |-> "shared lock ownership"; 1:1
};


DirLock::DirLock(const Zstring& folderPath, const Zstring& fileName, const DirLockCallback& notifyStatus, std::chrono::milliseconds cbInterval) //throw FileError
{
    sharedLock_ = LockAdmin::instance().retrieve(appendPath(folderPath, fileName), notifyStatus, cbInterval); //throw FileError
}

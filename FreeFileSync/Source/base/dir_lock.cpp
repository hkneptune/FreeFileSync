// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************
#include "dir_lock.h"
#include <map>
#include <memory>
#include <zen/sys_error.h>
#include <zen/thread.h>
#include <zen/scope_guard.h>
#include <zen/guid.h>
#include <zen/file_access.h>
#include <zen/file_io.h>

    #include <fcntl.h>    //open()
    #include <sys/stat.h> //
    #include <unistd.h> //getsid()
    #include <signal.h> //kill()
    #include <pwd.h> //getpwuid_r()

using namespace zen;
using namespace fff;


namespace
{
const std::chrono::seconds EMIT_LIFE_SIGN_INTERVAL   (5); //show life sign;
const std::chrono::seconds POLL_LIFE_SIGN_INTERVAL   (4); //poll for life sign;
const std::chrono::seconds DETECT_ABANDONED_INTERVAL(30); //assume abandoned lock;

const char LOCK_FORMAT_DESCR[] = "FreeFileSync";
const int LOCK_FORMAT_VER = 2; //lock file format version


//worker thread
class LifeSigns
{
public:
    LifeSigns(const Zstring& lockFilePath) : lockFilePath_(lockFilePath)
    {
    }

    void operator()() const //throw ThreadInterruption
    {
        const std::optional<Zstring> parentDirPath = getParentFolderPath(lockFilePath_);
        setCurrentThreadName(("DirLock: " + (parentDirPath ? utfTo<std::string>(*parentDirPath) : "")).c_str());

        for (;;)
        {
            interruptibleSleep(EMIT_LIFE_SIGN_INTERVAL); //throw ThreadInterruption
            emitLifeSign(); //noexcept
        }
    }

private:
    //try to append one byte...
    void emitLifeSign() const //noexcept
    {
        const int fileHandle = ::open(lockFilePath_.c_str(), O_WRONLY | O_APPEND | O_CLOEXEC);
        if (fileHandle == -1)
            return;
        ZEN_ON_SCOPE_EXIT(::close(fileHandle));

        /*const ssize_t bytesWritten =*/ ::write(fileHandle, " ", 1);
    }

    const Zstring lockFilePath_; //thread-local!
};


Zstring abandonedLockDeletionName(const Zstring& lockFilePath) //make sure to NOT change file ending!
{
    const size_t pos = lockFilePath.rfind(FILE_NAME_SEPARATOR); //search from end
    return pos == Zstring::npos ? Zstr("Del.") + lockFilePath :
           Zstring(lockFilePath.c_str(), pos + 1) + //include path separator
           Zstr("Del.") +
           afterLast(lockFilePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL);
}




    using ProcessId = pid_t;
    using SessionId = pid_t;

//return ppid on Windows, sid on Linux/Mac, "no value" if process corresponding to "processId" is not existing
std::optional<SessionId> getSessionId(ProcessId processId) //throw FileError
{
    if (::kill(processId, 0) != 0) //sig == 0: no signal sent, just existence check
        return {};

    const pid_t procSid = ::getsid(processId); //NOT to be confused with "login session", e.g. not stable on OS X!!!
    if (procSid < 0) //pids are never negative, empiric proof: https://linux.die.net/man/2/wait
        THROW_LAST_FILE_ERROR(_("Cannot get process information."), L"getsid");

    return procSid;
}


struct LockInformation //throw FileError
{
    std::string lockId; //16 byte GUID - a universal identifier for this lock (no matter what the path is, considering symlinks, distributed network, etc.)

    //identify local computer
    std::string computerName; //format: HostName.DomainName
    std::string userId;

    //identify running process
    SessionId sessionId = 0; //Windows: parent process id; Linux/OS X: session of the process, NOT the user
    ProcessId processId = 0;
};


LockInformation getLockInfoFromCurrentProcess() //throw FileError
{
    LockInformation lockInfo = {};
    lockInfo.lockId = generateGUID();

    //wxGetFullHostName() is a performance killer and can hang for some users, so don't touch!

    lockInfo.processId = ::getpid(); //never fails

    std::vector<char> buffer(10000);
    if (::gethostname(&buffer[0], buffer.size()) != 0)
        THROW_LAST_FILE_ERROR(_("Cannot get process information."), L"gethostname");
    lockInfo.computerName = "Linux."; //distinguish linux/windows lock files
    lockInfo.computerName += &buffer[0];

    if (::getdomainname(&buffer[0], buffer.size()) != 0)
        THROW_LAST_FILE_ERROR(_("Cannot get process information."), L"getdomainname");
    lockInfo.computerName += ".";
    lockInfo.computerName += &buffer[0];

    const uid_t userIdNo = ::getuid(); //never fails

    //the id alone is not very distinctive, e.g. often 1000 on Ubuntu => add name
    buffer.resize(std::max<long>(buffer.size(), ::sysconf(_SC_GETPW_R_SIZE_MAX))); //::sysconf may return long(-1)
    struct passwd buffer2 = {};
    struct passwd* pwsEntry = nullptr;
    if (::getpwuid_r(userIdNo, &buffer2, &buffer[0], buffer.size(), &pwsEntry) != 0) //getlogin() is deprecated and not working on Ubuntu at all!!!
        THROW_LAST_FILE_ERROR(_("Cannot get process information."), L"getpwuid_r");
    if (!pwsEntry)
        throw FileError(_("Cannot get process information."), L"no login found"); //should not happen?

    lockInfo.userId = numberTo<std::string>(userIdNo) + "(" + pwsEntry->pw_name + ")"; //follow Linux naming convention "1000(zenju)"

    std::optional<SessionId> sessionIdTmp = getSessionId(lockInfo.processId); //throw FileError
    if (!sessionIdTmp)
        throw FileError(_("Cannot get process information."), L"no session id found"); //should not happen?
    lockInfo.sessionId = *sessionIdTmp;

    return lockInfo;
}


LockInformation unserialize(MemoryStreamIn<ByteArray>& stream) //throw UnexpectedEndOfStreamError
{
    char tmp[sizeof(LOCK_FORMAT_DESCR)] = {};
    readArray(stream, &tmp, sizeof(tmp));                    //file format header
    const int lockFileVersion = readNumber<int32_t>(stream); //

    if (!std::equal(std::begin(tmp), std::end(tmp), std::begin(LOCK_FORMAT_DESCR)) ||
        lockFileVersion != LOCK_FORMAT_VER)
        throw UnexpectedEndOfStreamError(); //well, not really...!?

    LockInformation lockInfo = {};
    lockInfo.lockId       = readContainer<std::string>(stream); //
    lockInfo.computerName = readContainer<std::string>(stream); //UnexpectedEndOfStreamError
    lockInfo.userId       = readContainer<std::string>(stream); //
    lockInfo.sessionId    = static_cast<SessionId>(readNumber<uint64_t>(stream)); //[!] conversion
    lockInfo.processId    = static_cast<ProcessId>(readNumber<uint64_t>(stream)); //[!] conversion
    return lockInfo;
}


void serialize(const LockInformation& lockInfo, MemoryStreamOut<ByteArray>& stream)
{
    writeArray(stream, LOCK_FORMAT_DESCR, sizeof(LOCK_FORMAT_DESCR));
    writeNumber<int32_t>(stream, LOCK_FORMAT_VER);

    static_assert(sizeof(lockInfo.processId) <= sizeof(uint64_t)); //ensure cross-platform compatibility!
    static_assert(sizeof(lockInfo.sessionId) <= sizeof(uint64_t)); //

    writeContainer(stream, lockInfo.lockId);
    writeContainer(stream, lockInfo.computerName);
    writeContainer(stream, lockInfo.userId);
    writeNumber<uint64_t>(stream, lockInfo.sessionId);
    writeNumber<uint64_t>(stream, lockInfo.processId);
}


LockInformation retrieveLockInfo(const Zstring& lockFilePath) //throw FileError
{
    MemoryStreamIn<ByteArray> memStreamIn(loadBinContainer<ByteArray>(lockFilePath, nullptr /*notifyUnbufferedIO*/)); //throw FileError
    try
    {
        return unserialize(memStreamIn); //throw UnexpectedEndOfStreamError
    }
    catch (UnexpectedEndOfStreamError&)
    {
        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(lockFilePath)), L"Unexpected end of stream.");
    }
}


inline
std::string retrieveLockId(const Zstring& lockFilePath) //throw FileError
{
    return retrieveLockInfo(lockFilePath).lockId; //throw FileError
}


enum class ProcessStatus
{
    NOT_RUNNING,
    RUNNING,
    ITS_US,
    CANT_TELL,
};

ProcessStatus getProcessStatus(const LockInformation& lockInfo) //throw FileError
{
    const LockInformation localInfo = getLockInfoFromCurrentProcess(); //throw FileError

    if (lockInfo.computerName != localInfo.computerName ||
        lockInfo.userId != localInfo.userId) //another user may run a session right now!
        return ProcessStatus::CANT_TELL; //lock owned by different computer in this network

    if (lockInfo.sessionId == localInfo.sessionId &&
        lockInfo.processId == localInfo.processId) //obscure, but possible: deletion failed or a lock file is "stolen" and put back while the program is running
        return ProcessStatus::ITS_US;

    if (std::optional<SessionId> sessionId = getSessionId(lockInfo.processId)) //throw FileError
        return *sessionId == lockInfo.sessionId ? ProcessStatus::RUNNING : ProcessStatus::NOT_RUNNING;
    return ProcessStatus::NOT_RUNNING;
}


DEFINE_NEW_FILE_ERROR(ErrorFileNotExisting);
uint64_t getLockFileSize(const Zstring& filePath) //throw FileError, ErrorFileNotExisting
{
    struct ::stat fileInfo = {};
    if (::stat(filePath.c_str(), &fileInfo) == 0)
        return fileInfo.st_size;

    if (errno == ENOENT)
        throw ErrorFileNotExisting(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), formatSystemError(L"stat", errno));
    THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), L"stat");
}


void waitOnDirLock(const Zstring& lockFilePath, const DirLockCallback& notifyStatus /*throw X*/, std::chrono::milliseconds cbInterval) //throw FileError
{
    std::wstring infoMsg = _("Waiting while directory is locked:") + L' ' + fmtPath(lockFilePath);

    if (notifyStatus) notifyStatus(infoMsg); //throw X

    //convenience optimization only: if we know the owning process crashed, we needn't wait DETECT_ABANDONED_INTERVAL sec
    bool lockOwnderDead = false;
    std::string originalLockId; //empty if it cannot be retrieved
    try
    {
        const LockInformation& lockInfo = retrieveLockInfo(lockFilePath); //throw FileError

        infoMsg += L" | " + _("Lock owner:") +  L' ' + utfTo<std::wstring>(lockInfo.userId);

        originalLockId = lockInfo.lockId;
        switch (getProcessStatus(lockInfo)) //throw FileError
        {
            case ProcessStatus::ITS_US: //since we've already passed LockAdmin, the lock file seems abandoned ("stolen"?) although it's from this process
            case ProcessStatus::NOT_RUNNING:
                lockOwnderDead = true;
                break;
            case ProcessStatus::RUNNING:
            case ProcessStatus::CANT_TELL:
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
            DirLock guardDeletion(abandonedLockDeletionName(lockFilePath), notifyStatus, cbInterval); //throw FileError

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
            catch (ErrorFileNotExisting&) { return; } //what we are waiting for...

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
                    notifyStatus(infoMsg + L" | " + _("Detecting abandoned lock...") + L' ' + _P("1 sec", "%x sec", remainingSeconds)); //throw X
                }
                else
                    notifyStatus(infoMsg); //throw X; emit a message in any case (might clear other one)
            }
            std::this_thread::sleep_for(cbInterval);
        }
    }
}


void releaseLock(const Zstring& lockFilePath) //noexcept
{
    try
    {
        removeFilePlain(lockFilePath); //throw FileError
    }
    catch (FileError&) {}
}


bool tryLock(const Zstring& lockFilePath) //throw FileError
{
    const mode_t oldMask = ::umask(0); //important: we want the lock file to have exactly the permissions specified
    ZEN_ON_SCOPE_EXIT(::umask(oldMask));

    //O_EXCL contains a race condition on NFS file systems: http://linux.die.net/man/2/open
    const int hFile = ::open(lockFilePath.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC,
                             S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH); //0666
    if (hFile == -1)
    {
        if (errno == EEXIST)
            return false;

        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(lockFilePath)), L"open");
    }
    ZEN_ON_SCOPE_FAIL(try { removeFilePlain(lockFilePath); }
    catch (FileError&) {});
    FileOutput fileOut(hFile, lockFilePath, nullptr /*notifyUnbufferedIO*/); //pass handle ownership

    //write housekeeping info: user, process info, lock GUID
    MemoryStreamOut<ByteArray> streamOut;
    serialize(getLockInfoFromCurrentProcess(), streamOut); //throw FileError

    fileOut.write(&*streamOut.ref().begin(), streamOut.ref().size()); //throw FileError, (X)
    fileOut.finalize();                                               //
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
        lifeSignthread_.interrupt(); //thread lifetime is subset of this instances's life
        lifeSignthread_.join();

        ::releaseLock(lockFilePath_); //noexcept
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
        assert(runningMainThread()); //function is not thread-safe!

        tidyUp();

        //optimization: check if we already own a lock for this path
        auto itGuid = guidByPath_.find(lockFilePath);
        if (itGuid != guidByPath_.end())
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
        eraseIf(locksByGuid_, [](const auto& v) { return !v.second.lock(); });
        eraseIf(guidByPath_, [&](const auto& v) { return locksByGuid_.find(v.second) == locksByGuid_.end(); });
    }

    std::map<Zstring, UniqueId> guidByPath_;                      //lockFilePath |-> GUID; n:1; locks can be referenced by a lockFilePath or alternatively a GUID
    std::map<UniqueId, std::weak_ptr<SharedDirLock>> locksByGuid_; //GUID |-> "shared lock ownership"; 1:1
};


DirLock::DirLock(const Zstring& lockFilePath, const DirLockCallback& notifyStatus, std::chrono::milliseconds cbInterval) //throw FileError
{
    sharedLock_ = LockAdmin::instance().retrieve(lockFilePath, notifyStatus, cbInterval); //throw FileError
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "file_io.h"
    #include <sys/stat.h>
    #include <fcntl.h>  //open
    #include <unistd.h> //close, read, write

using namespace zen;


size_t FileBase::getBlockSize() //throw FileError
{
    if (blockSizeBuf_ == 0)
    {
        /*  - statfs::f_bsize  - "optimal transfer block size"
            - stat::st_blksize - "blocksize for file system I/O. Writing in smaller chunks may cause an inefficient read-modify-rewrite."

            e.g. local disk: f_bsize  4096   st_blksize  4096
                 USB memory: f_bsize 32768   st_blksize 32768     */
        const auto st_blksize = getStatBuffered().st_blksize; //throw FileError
        if (st_blksize > 0)             //st_blksize is signed!
            blockSizeBuf_ = st_blksize; //

        blockSizeBuf_ = std::max(blockSizeBuf_, defaultBlockSize);
        //ha, convergent evolution! https://github.com/coreutils/coreutils/blob/master/src/ioblksize.h#L74
    }
    return blockSizeBuf_;
}


const struct stat& FileBase::getStatBuffered() //throw FileError
{
    if (!statBuf_)
        try
        {
            if (hFile_ == invalidFileHandle)
                throw SysError(L"Contract error: getStatBuffered() called after close().");

            struct stat fileInfo = {};
            if (::fstat(hFile_, &fileInfo) != 0)
                THROW_LAST_SYS_ERROR("fstat");
            statBuf_ = std::move(fileInfo);
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath_)), e.toString()); }

    return *statBuf_;
}


FileBase::~FileBase()
{
    if (hFile_ != invalidFileHandle)
        try
        {
            close(); //throw FileError
        }
        catch (const FileError& e) { logExtraError(e.toString()); }
}


void FileBase::close() //throw FileError
{
    try
    {
        if (hFile_ == invalidFileHandle)
            throw SysError(L"Contract error: close() called more than once.");
        if (::close(hFile_) != 0)
            THROW_LAST_SYS_ERROR("close");
        hFile_ = invalidFileHandle; //do NOT set on error! => ~FileOutputPlain() still wants to (try to) delete the file!
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), e.toString()); }
}

//----------------------------------------------------------------------------------------------------

namespace
{
    std::pair<FileBase::FileHandle, struct stat>
openHandleForRead(const Zstring& filePath) //throw FileError, ErrorFileLocked
{
    try
    {
        //caveat: check for file types that block during open(): character device, block device, named pipe
        struct stat fileInfo = {};
        if (::stat(filePath.c_str(), &fileInfo) != 0) //follows symlinks
            THROW_LAST_SYS_ERROR("stat");

        if (!S_ISREG(fileInfo.st_mode) &&
            !S_ISDIR(fileInfo.st_mode) && //open() will fail with "EISDIR: Is a directory" => nice
            !S_ISLNK(fileInfo.st_mode)) //?? shouldn't be possible after successful stat()
        {
            const std::wstring typeName = [m = fileInfo.st_mode]
            {
                std::wstring name =
                S_ISCHR (m) ? L"character device" : //e.g. /dev/null
                S_ISBLK (m) ? L"block device" :     //e.g. /dev/sda1
                S_ISFIFO(m) ? L"FIFO, named pipe" :
                S_ISSOCK(m) ? L"socket" : L""; //doesn't block but open() error is unclear: "ENXIO: No such device or address"
                if (!name.empty())
                    name += L", ";
                return name + printNumber<std::wstring>(L"0%06o", m & S_IFMT);
            }();
            throw SysError(_("Unsupported item type.") + L" [" + typeName + L']');
        }

        //don't use O_DIRECT: https://yarchive.net/comp/linux/o_direct.html
        const int fdFile = ::open(filePath.c_str(), O_RDONLY | O_CLOEXEC);
        if (fdFile == -1) //don't check "< 0" -> docu seems to allow "-2" to be a valid file handle
            THROW_LAST_SYS_ERROR("open");
        return {fdFile /*pass ownership*/, fileInfo};
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(filePath)), e.toString()); }
}
}


FileInputPlain::FileInputPlain(const Zstring& filePath) :
    FileInputPlain(openHandleForRead(filePath), filePath) {} //throw FileError, ErrorFileLocked


FileInputPlain::FileInputPlain(const std::pair<FileBase::FileHandle, struct stat>& fileDetails, const Zstring& filePath) :
    FileInputPlain(fileDetails.first, filePath)
{
    setStatBuffered(fileDetails.second);
}


FileInputPlain::FileInputPlain(FileHandle handle, const Zstring& filePath) :
    FileBase(handle, filePath)
{
    //optimize read-ahead on input file:
    if (::posix_fadvise(getHandle(), 0 /*offset*/, 0 /*len*/, POSIX_FADV_SEQUENTIAL) != 0) //"len == 0" means "end of the file"
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(filePath)), "posix_fadvise(POSIX_FADV_SEQUENTIAL)");

    /*  - POSIX_FADV_SEQUENTIAL is like POSIX_FADV_NORMAL, but with twice the read-ahead buffer size
        - POSIX_FADV_NOREUSE "since  kernel  2.6.18 this flag is a no-op" WTF!?
        - POSIX_FADV_DONTNEED may be used to clear the OS file system cache (offset and len must be page-aligned!)
            => does nothing, unless data was already written to disk: https://insights.oetiker.ch/linux/fadvise/
        - POSIX_FADV_WILLNEED: issue explicit read-ahead; almost the same as readahead(), but with weaker error checking
          https://unix.stackexchange.com/questions/681188/difference-between-posix-fadvise-and-readahead

          clear file system cache manually:     sync; echo 3 > /proc/sys/vm/drop_caches              */

}


//may return short, only 0 means EOF! =>  CONTRACT: bytesToRead > 0!
size_t FileInputPlain::tryRead(void* buffer, size_t bytesToRead) //throw FileError, ErrorFileLocked
{
    if (bytesToRead == 0) //"read() with a count of 0 returns zero" => indistinguishable from end of file! => check!
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
    assert(bytesToRead % getBlockSize() == 0);
    try
    {
        ssize_t bytesRead = 0;
        do
        {
            bytesRead = ::read(getHandle(), buffer, bytesToRead);
        }
        while (bytesRead < 0 && errno == EINTR); //Compare copy_reg() in copy.c: ftp://ftp.gnu.org/gnu/coreutils/coreutils-8.23.tar.xz
        //EINTR is not checked on macOS' copyfile: https://opensource.apple.com/source/copyfile/copyfile-173.40.2/copyfile.c.auto.html
        //read() on macOS: https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man2/read.2.html
        //if ::read is interrupted (EINTR) right in the middle, it will return successfully with "bytesRead < bytesToRead"

        if (bytesRead < 0)
            THROW_LAST_SYS_ERROR("read");

        ASSERT_SYSERROR(makeUnsigned(bytesRead) <= bytesToRead); //better safe than sorry
        return bytesRead; //"zero indicates end of file"
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getFilePath())), e.toString()); }
}

//----------------------------------------------------------------------------------------------------

namespace
{
FileBase::FileHandle openHandleForWrite(const Zstring& filePath) //throw FileError, ErrorTargetExisting
{
    try
    {
        //checkForUnsupportedType(filePath); -> not needed, open() + O_WRONLY should fail fast

        const mode_t lockFileMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; //0666 => umask will be applied implicitly!

        //O_EXCL contains a race condition on NFS file systems: https://linux.die.net/man/2/open
        const int fdFile = ::open(filePath.c_str(), //const char* pathname
                                  O_CREAT |         //int flags
                                  /*access == FileOutput::ACC_OVERWRITE ? O_TRUNC : */ O_EXCL | O_WRONLY  | O_CLOEXEC,
                                  lockFileMode);    //mode_t mode
        if (fdFile == -1)
        {
            const int ec = errno; //copy before making other system calls!
            if (ec == EEXIST)
                throw ErrorTargetExisting(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(filePath)), formatSystemError("open", ec));

            THROW_LAST_SYS_ERROR("open");
        }
        return fdFile; //pass ownership
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(filePath)), e.toString()); }
}
}


FileOutputPlain::FileOutputPlain(const Zstring& filePath) :
    FileOutputPlain(openHandleForWrite(filePath), filePath) {} //throw FileError, ErrorTargetExisting


FileOutputPlain::FileOutputPlain(FileHandle handle, const Zstring& filePath) :
    FileBase(handle, filePath)
{
}


FileOutputPlain::~FileOutputPlain()
{

    if (getHandle() != invalidFileHandle) //not finalized => clean up garbage
        try
        {
            //"deleting while handle is open" == FILE_FLAG_DELETE_ON_CLOSE
            if (::unlink(getFilePath().c_str()) != 0)
                THROW_LAST_SYS_ERROR("unlink");
        }
        catch (const SysError& e)
        {
            logExtraError(replaceCpy(_("Cannot delete file %x."), L"%x", fmtPath(getFilePath())) + L"\n\n" + e.toString());
        }
}


void FileOutputPlain::reserveSpace(uint64_t expectedSize) //throw FileError
{
    //NTFS: "If you set the file allocation info [...] the file contents will be forced into nonresident data, even if it would have fit inside the MFT."
    if (expectedSize < 1024) //https://docs.microsoft.com/en-us/archive/blogs/askcore/the-four-stages-of-ntfs-file-growth
        return;

    try
    {
#if 0 /*  fallocate(FALLOC_FL_KEEP_SIZE):
            - perf: no real benefit (in a quick and dirty local test)
            - breaks Btrfs compression: https://freefilesync.org/forum/viewtopic.php?t=10356
            - apparently not even used by cp: https://github.com/coreutils/coreutils/blob/17479ef60c8edbd2fe8664e31a7f69704f0cd221/src/copy.c#LL1234C5-L1234C5      */

        //don't use ::posix_fallocate which uses horribly inefficient fallback if FS doesn't support it (EOPNOTSUPP) and changes files size!
        //FALLOC_FL_KEEP_SIZE => allocate only, file size is NOT changed!
        if (::fallocate(getHandle(),         //int fd
                        FALLOC_FL_KEEP_SIZE, //int mode
                        0,                   //off_t offset
                        expectedSize) != 0)  //off_t len
            if (errno != EOPNOTSUPP) //possible, unlike with posix_fallocate()
                THROW_LAST_SYS_ERROR("fallocate");
#endif

    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), e.toString()); }
}


//may return short! CONTRACT: bytesToWrite > 0
size_t FileOutputPlain::tryWrite(const void* buffer, size_t bytesToWrite) //throw FileError
{
    if (bytesToWrite == 0)
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
    assert(bytesToWrite % getBlockSize() == 0 || bytesToWrite < getBlockSize());
    try
    {
        ssize_t bytesWritten = 0;
        do
        {
            bytesWritten = ::write(getHandle(), buffer, bytesToWrite);
        }
        while (bytesWritten < 0 && errno == EINTR);
        //write() on macOS: https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man2/write.2.html
        //if ::write() is interrupted (EINTR) right in the middle, it will return successfully with "bytesWritten < bytesToWrite"!

        if (bytesWritten <= 0)
        {
            if (bytesWritten == 0) //comment in safe-read.c suggests to treat this as an error due to buggy drivers
                errno = ENOSPC;

            THROW_LAST_SYS_ERROR("write");
        }

        ASSERT_SYSERROR(makeUnsigned(bytesWritten) <= bytesToWrite); //better safe than sorry
        return bytesWritten;
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), e.toString()); }
}

//----------------------------------------------------------------------------------------------------

std::string zen::getFileContent(const Zstring& filePath, const IoCallback& notifyUnbufferedIO /*throw X*/) //throw FileError, X
{
    FileInputPlain fileIn(filePath); //throw FileError, ErrorFileLocked

    return unbufferedLoad<std::string>([&](void* buffer, size_t bytesToRead)
    {
        const size_t bytesRead = fileIn.tryRead(buffer, bytesToRead); //throw FileError; may return short, only 0 means EOF! =>  CONTRACT: bytesToRead > 0!
        if (notifyUnbufferedIO) notifyUnbufferedIO(bytesRead); //throw X!
        return bytesRead;
    },
    fileIn.getBlockSize()); //throw FileError, X
}


void zen::setFileContent(const Zstring& filePath, const std::string_view byteStream, const IoCallback& notifyUnbufferedIO /*throw X*/) //throw FileError, X
{
    const Zstring tmpFilePath = getPathWithTempName(filePath);

    FileOutputPlain tmpFile(tmpFilePath); //throw FileError, (ErrorTargetExisting)

    tmpFile.reserveSpace(byteStream.size()); //throw FileError

    unbufferedSave(byteStream, [&](const void* buffer, size_t bytesToWrite)
    {
        const size_t bytesWritten = tmpFile.tryWrite(buffer, bytesToWrite); //throw FileError; may return short! CONTRACT: bytesToWrite > 0
        if (notifyUnbufferedIO) notifyUnbufferedIO(bytesWritten); //throw X!
        return bytesWritten;
    },
    tmpFile.getBlockSize()); //throw FileError, X

    tmpFile.close(); //throw FileError
    //take over ownership:
    ZEN_ON_SCOPE_FAIL( try { removeFilePlain(tmpFilePath); }
    catch (const FileError& e) { logExtraError(e.toString()); });

    //operation finished: move temp file transactionally
    moveAndRenameItem(tmpFilePath, filePath, true /*replaceExisting*/); //throw FileError, (ErrorMoveUnsupported), (ErrorTargetExisting)
}

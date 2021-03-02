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


FileBase::~FileBase()
{
    if (hFile_ != invalidFileHandle)
        try
        {
            close(); //throw FileError
        }
        catch (FileError&) { assert(false); }
}


void FileBase::close() //throw FileError
{
    if (hFile_ == invalidFileHandle)
        throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), L"Contract error: close() called more than once.");
    ZEN_ON_SCOPE_EXIT(hFile_ = invalidFileHandle);

    if (::close(hFile_) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), "close");
}

//----------------------------------------------------------------------------------------------------

namespace
{
FileBase::FileHandle openHandleForRead(const Zstring& filePath) //throw FileError, ErrorFileLocked
{
    //caveat: check for file types that block during open(): character device, block device, named pipe
    struct stat fileInfo = {};
    if (::stat(filePath.c_str(), &fileInfo) == 0) //follows symlinks
    {
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
            throw FileError(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(filePath)),
                            _("Unsupported item type.") + L" [" + typeName + L']');
        }
    }
    //else: let ::open() fail for errors like "not existing"

    //don't use O_DIRECT: https://yarchive.net/comp/linux/o_direct.html
    const int fdFile = ::open(filePath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fdFile == -1) //don't check "< 0" -> docu seems to allow "-2" to be a valid file handle
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(filePath)), "open");
    return fdFile; //pass ownership
}
}


FileInput::FileInput(FileHandle handle, const Zstring& filePath, const IoCallback& notifyUnbufferedIO) :
    FileBase(handle, filePath), notifyUnbufferedIO_(notifyUnbufferedIO) {}


FileInput::FileInput(const Zstring& filePath, const IoCallback& notifyUnbufferedIO) :
    FileBase(openHandleForRead(filePath), filePath), //throw FileError, ErrorFileLocked
    notifyUnbufferedIO_(notifyUnbufferedIO)
{
    //optimize read-ahead on input file:
    if (::posix_fadvise(getHandle(), 0, 0, POSIX_FADV_SEQUENTIAL) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(filePath)), "posix_fadvise(POSIX_FADV_SEQUENTIAL)");

}


size_t FileInput::tryRead(void* buffer, size_t bytesToRead) //throw FileError, ErrorFileLocked; may return short, only 0 means EOF!
{
    if (bytesToRead == 0) //"read() with a count of 0 returns zero" => indistinguishable from end of file! => check!
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));
    assert(bytesToRead == getBlockSize());

    ssize_t bytesRead = 0;
    do
    {
        bytesRead = ::read(getHandle(), buffer, bytesToRead);
    }
    while (bytesRead < 0 && errno == EINTR); //Compare copy_reg() in copy.c: ftp://ftp.gnu.org/gnu/coreutils/coreutils-8.23.tar.xz
    //EINTR is not checked on macOS' copyfile: https://opensource.apple.com/source/copyfile/copyfile-146/copyfile.c.auto.html
    //read() on macOS: https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man2/read.2.html

    if (bytesRead < 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getFilePath())), "read");
    if (static_cast<size_t>(bytesRead) > bytesToRead) //better safe than sorry
        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getFilePath())), formatSystemError("ReadFile", L"", L"Buffer overflow."));

    //if ::read is interrupted (EINTR) right in the middle, it will return successfully with "bytesRead < bytesToRead"

    return bytesRead; //"zero indicates end of file"
}


size_t FileInput::read(void* buffer, size_t bytesToRead) //throw FileError, ErrorFileLocked, X; return "bytesToRead" bytes unless end of stream!
{
    /*
        FFS 8.9-9.5 perf issues on macOS: https://freefilesync.org/forum/viewtopic.php?t=4808
            app-level buffering is essential to optimize random data sizes; e.g. "export file list":
                => big perf improvement on Windows, Linux. No significant improvement on macOS in tests
            impact on stream-based file copy:
                => no drawback vs block-wise copy loop on Linux, HOWEVER: big perf issue on macOS!

        Possible cause of macOS perf issue unclear:
            - getting rid of std::vector::resize() and std::vector::erase() "fixed" the problem
                => costly zero-initializing memory? problem with inlining? QOI issue of std:vector on clang/macOS?
            - replacing std::copy() with memcpy() also *seems* to have improved speed "somewhat"
    */

    const size_t blockSize = getBlockSize();
    assert(memBuf_.size() >= blockSize);
    assert(bufPos_ <= bufPosEnd_ && bufPosEnd_ <= memBuf_.size());

    auto       it    = static_cast<std::byte*>(buffer);
    const auto itEnd = it + bytesToRead;
    for (;;)
    {
        const size_t junkSize = std::min(static_cast<size_t>(itEnd - it), bufPosEnd_ - bufPos_);
        std::memcpy(it, &memBuf_[0] + bufPos_ /*caveat: vector debug checks*/, junkSize);
        bufPos_ += junkSize;
        it      += junkSize;

        if (it == itEnd)
            break;
        //--------------------------------------------------------------------
        const size_t bytesRead = tryRead(&memBuf_[0], blockSize); //throw FileError, ErrorFileLocked; may return short, only 0 means EOF! => CONTRACT: bytesToRead > 0
        bufPos_ = 0;
        bufPosEnd_ = bytesRead;

        if (notifyUnbufferedIO_) notifyUnbufferedIO_(bytesRead); //throw X

        if (bytesRead == 0) //end of file
            break;
    }
    return it - static_cast<std::byte*>(buffer);
}

//----------------------------------------------------------------------------------------------------

namespace
{
FileBase::FileHandle openHandleForWrite(const Zstring& filePath) //throw FileError, ErrorTargetExisting
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
        const std::wstring errorMsg = replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(filePath));
        const std::wstring errorDescr = formatSystemError("open", ec);

        if (ec == EEXIST)
            throw ErrorTargetExisting(errorMsg, errorDescr);
        //if (ec == ENOENT) throw ErrorTargetPathMissing(errorMsg, errorDescr);

        throw FileError(errorMsg, errorDescr);
    }
    return fdFile; //pass ownership
}
}


FileOutput::FileOutput(FileHandle handle, const Zstring& filePath, const IoCallback& notifyUnbufferedIO) :
    FileBase(handle, filePath), notifyUnbufferedIO_(notifyUnbufferedIO)
{
}


FileOutput::FileOutput(const Zstring& filePath, const IoCallback& notifyUnbufferedIO) :
    FileBase(openHandleForWrite(filePath), filePath), notifyUnbufferedIO_(notifyUnbufferedIO) {} //throw FileError, ErrorTargetExisting


FileOutput::~FileOutput()
{

    if (getHandle() != invalidFileHandle) //not finalized => clean up garbage
    {
        //"deleting while handle is open" == FILE_FLAG_DELETE_ON_CLOSE
        if (::unlink(getFilePath().c_str()) != 0)
            assert(false);
    }
}


size_t FileOutput::tryWrite(const void* buffer, size_t bytesToWrite) //throw FileError; may return short! CONTRACT: bytesToWrite > 0
{
    if (bytesToWrite == 0)
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));
    assert(bytesToWrite <= getBlockSize());

    ssize_t bytesWritten = 0;
    do
    {
        bytesWritten = ::write(getHandle(), buffer, bytesToWrite);
    }
    while (bytesWritten < 0 && errno == EINTR);
    //write() on macOS: https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man2/write.2.html

    if (bytesWritten <= 0)
    {
        if (bytesWritten == 0) //comment in safe-read.c suggests to treat this as an error due to buggy drivers
            errno = ENOSPC;

        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), "write");
    }
    if (bytesWritten > static_cast<ssize_t>(bytesToWrite)) //better safe than sorry
        throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), formatSystemError("write", L"", L"Buffer overflow."));

    //if ::write() is interrupted (EINTR) right in the middle, it will return successfully with "bytesWritten < bytesToWrite"!
    return bytesWritten;
}


void FileOutput::write(const void* buffer, size_t bytesToWrite) //throw FileError, X
{
    const size_t blockSize = getBlockSize();
    assert(memBuf_.size() >= blockSize);
    assert(bufPos_ <= bufPosEnd_ && bufPosEnd_ <= memBuf_.size());

    auto       it    = static_cast<const std::byte*>(buffer);
    const auto itEnd = it + bytesToWrite;
    for (;;)
    {
        if (memBuf_.size() - bufPos_ < blockSize) //support memBuf_.size() > blockSize to reduce memmove()s, but perf test shows: not really needed!
            // || bufPos_ == bufPosEnd_) -> not needed while memBuf_.size() == blockSize
        {
            std::memmove(&memBuf_[0], &memBuf_[0] + bufPos_, bufPosEnd_ - bufPos_);
            bufPosEnd_ -= bufPos_;
            bufPos_ = 0;
        }

        const size_t junkSize = std::min(static_cast<size_t>(itEnd - it), blockSize - (bufPosEnd_ - bufPos_));
        std::memcpy(&memBuf_[0] + bufPosEnd_ /*caveat: vector debug checks*/, it, junkSize);
        bufPosEnd_ += junkSize;
        it         += junkSize;

        if (it == itEnd)
            return;
        //--------------------------------------------------------------------
        const size_t bytesWritten = tryWrite(&memBuf_[bufPos_], blockSize); //throw FileError; may return short! CONTRACT: bytesToWrite > 0
        bufPos_ += bytesWritten;
        if (notifyUnbufferedIO_) notifyUnbufferedIO_(bytesWritten); //throw X!
    }
}


void FileOutput::flushBuffers() //throw FileError, X
{
    assert(bufPosEnd_ - bufPos_ <= getBlockSize());
    assert(bufPos_ <= bufPosEnd_ && bufPosEnd_ <= memBuf_.size());
    while (bufPos_ != bufPosEnd_)
    {
        const size_t bytesWritten = tryWrite(&memBuf_[bufPos_], bufPosEnd_ - bufPos_); //throw FileError; may return short! CONTRACT: bytesToWrite > 0
        bufPos_ += bytesWritten;
        if (notifyUnbufferedIO_) notifyUnbufferedIO_(bytesWritten); //throw X!
    }
}


void FileOutput::finalize() //throw FileError, X
{
    flushBuffers(); //throw FileError, X
    close();        //throw FileError
    //~FileBase() calls this one, too, but we want to propagate errors if any
}


void FileOutput::reserveSpace(uint64_t expectedSize) //throw FileError
{
    //NTFS: "If you set the file allocation info [...] the file contents will be forced into nonresident data, even if it would have fit inside the MFT."
    if (expectedSize < 1024) //https://www.sciencedirect.com/topics/computer-science/master-file-table
        return;

    //don't use ::posix_fallocate which uses horribly inefficient fallback if FS doesn't support it (EOPNOTSUPP) and changes files size!
    //FALLOC_FL_KEEP_SIZE => allocate only, file size is NOT changed!
    if (::fallocate(getHandle(),         //int fd
                    FALLOC_FL_KEEP_SIZE, //int mode
                    0,                   //off_t offset
                    expectedSize) != 0)  //off_t len
        if (errno != EOPNOTSUPP) //possible, unlike with posix_fallocate()
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getFilePath())), "fallocate");

}


std::string zen::getFileContent(const Zstring& filePath, const IoCallback& notifyUnbufferedIO /*throw X*/) //throw FileError, X
{
    FileInput streamIn(filePath, notifyUnbufferedIO); //throw FileError, ErrorFileLocked
    return bufferedLoad<std::string>(streamIn); //throw FileError, X
}


void zen::setFileContent(const Zstring& filePath, const std::string& byteStream, const IoCallback& notifyUnbufferedIO /*throw X*/) //throw FileError, X
{
    TempFileOutput fileOut(filePath, notifyUnbufferedIO); //throw FileError
    if (!byteStream.empty())
    {
        //preallocate disk space & reduce fragmentation
        fileOut.reserveSpace(byteStream.size()); //throw FileError
        fileOut.write(&byteStream[0], byteStream.size()); //throw FileError, X
    }
    fileOut.commit(); //throw FileError, X
}

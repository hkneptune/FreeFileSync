// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_IO_H_89578342758342572345
#define FILE_IO_H_89578342758342572345

#include "file_access.h"
//#include "serialize.h"
#include "crc.h"
#include "guid.h"


namespace zen
{
    const char LINE_BREAK[] = "\n"; //since OS X Apple uses newline, too

/* OS-buffered file IO optimized for
    - sequential read/write accesses
    - better error reporting
    - long path support
    - follows symlinks                     */
class FileBase
{
public:
    using FileHandle = int;
    static const int invalidFileHandle = -1;

    FileHandle getHandle() { return hFile_; }

    //Windows: use 64kB ?? https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-2000-server/cc938632%28v=technet.10%29
    //macOS, Linux: use st_blksize?
    static size_t getBlockSize() { return 128 * 1024; };

    const Zstring& getFilePath() const { return filePath_; }

protected:
    FileBase(FileHandle handle, const Zstring& filePath) : hFile_(handle), filePath_(filePath) {}
    ~FileBase();

    void close(); //throw FileError -> optional, but good place to catch errors when closing stream!

private:
    FileBase           (const FileBase&) = delete;
    FileBase& operator=(const FileBase&) = delete;

    FileHandle hFile_ = invalidFileHandle;
    const Zstring filePath_;
};

//-----------------------------------------------------------------------------------------------

class FileInput : public FileBase
{
public:
    FileInput(                   const Zstring& filePath, const IoCallback& notifyUnbufferedIO /*throw X*/); //throw FileError, ErrorFileLocked
    FileInput(FileHandle handle, const Zstring& filePath, const IoCallback& notifyUnbufferedIO /*throw X*/); //takes ownership!

    size_t read(void* buffer, size_t bytesToRead); //throw FileError, ErrorFileLocked, X; return "bytesToRead" bytes unless end of stream!

private:
    size_t tryRead(void* buffer, size_t bytesToRead); //throw FileError, ErrorFileLocked; may return short, only 0 means EOF! =>  CONTRACT: bytesToRead > 0!

    const IoCallback notifyUnbufferedIO_; //throw X

    std::vector<std::byte> memBuf_ = std::vector<std::byte>(getBlockSize());
    size_t bufPos_   = 0;
    size_t bufPosEnd_= 0;
};


class FileOutput : public FileBase
{
public:
    FileOutput(                   const Zstring& filePath, const IoCallback& notifyUnbufferedIO /*throw X*/); //throw FileError, ErrorTargetExisting
    FileOutput(FileHandle handle, const Zstring& filePath, const IoCallback& notifyUnbufferedIO /*throw X*/); //takes ownership!
    ~FileOutput();

    void reserveSpace(uint64_t expectedSize); //throw FileError

    void write(const void* buffer, size_t bytesToWrite); //throw FileError, X
    void flushBuffers();                                 //throw FileError, X
    //caveat: does NOT flush OS or hard disk buffers like e.g. FlushFileBuffers()!

    void finalize(); /*= flushBuffers() + close()*/      //throw FileError, X

private:
    size_t tryWrite(const void* buffer, size_t bytesToWrite); //throw FileError; may return short! CONTRACT: bytesToWrite > 0

    IoCallback notifyUnbufferedIO_; //throw X
    std::vector<std::byte> memBuf_ = std::vector<std::byte>(getBlockSize());
    size_t bufPos_    = 0;
    size_t bufPosEnd_ = 0;
};
//-----------------------------------------------------------------------------------------------
//native stream I/O convenience functions:

class TempFileOutput
{
public:
    TempFileOutput( const Zstring& filePath, const IoCallback& notifyUnbufferedIO /*throw X*/) : //throw FileError
        filePath_(filePath),
        tmpFile_(tmpFilePath_, notifyUnbufferedIO) {} //throw FileError, (ErrorTargetExisting)

    void reserveSpace(uint64_t expectedSize) { tmpFile_.reserveSpace(expectedSize); } //throw FileError

    void write(const void* buffer, size_t bytesToWrite) { tmpFile_.write(buffer, bytesToWrite); } //throw FileError, X

    FileOutput& refTempFile() { return tmpFile_; }

    void commit() //throw FileError, X
    {
        tmpFile_.finalize(); //throw FileError, X

        //take ownership:
        ZEN_ON_SCOPE_FAIL( try { removeFilePlain(tmpFilePath_); /*throw FileError*/ }
        catch (FileError&) {});

        //operation finished: move temp file transactionally
        moveAndRenameItem(tmpFilePath_, filePath_, true /*replaceExisting*/); //throw FileError, (ErrorMoveUnsupported), (ErrorTargetExisting)
    }

private:
    //generate (hopefully) unique file name to avoid clashing with unrelated tmp file
    const Zstring filePath_;
    const Zstring shortGuid_ = printNumber<Zstring>(Zstr("%04x"), static_cast<unsigned int>(getCrc16(generateGUID())));
    const Zstring tmpFilePath_ = filePath_ + Zstr('.') + shortGuid_ + Zstr(".tmp");
    FileOutput tmpFile_;
};


[[nodiscard]] std::string getFileContent(const Zstring& filePath, const IoCallback& notifyUnbufferedIO /*throw X*/); //throw FileError, X

//overwrites if existing + transactional! :)
void setFileContent(const Zstring& filePath, const std::string& bytes, const IoCallback& notifyUnbufferedIO /*throw X*/); //throw FileError, X
}

#endif //FILE_IO_H_89578342758342572345

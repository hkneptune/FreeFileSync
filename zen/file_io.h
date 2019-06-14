// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_IO_H_89578342758342572345
#define FILE_IO_H_89578342758342572345

#include "file_error.h"
#include "serialize.h"


namespace zen
{
    const char LINE_BREAK[] = "\n"; //since OS X Apple uses newline, too

/*
OS-buffered file IO optimized for
    - sequential read/write accesses
    - better error reporting
    - long path support
    - follows symlinks
    */
class FileBase
{
public:
    const Zstring& getFilePath() const { return filePath_; }

    using FileHandle = int;

    FileHandle getHandle() { return fileHandle_; }

    //Windows: use 64kB ?? https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-2000-server/cc938632%28v=technet.10%29
    //Linux: use st_blksize?
    //macOS: use f_iosize?
    static size_t getBlockSize() { return 128 * 1024; };

protected:
    FileBase(FileHandle handle, const Zstring& filePath) : fileHandle_(handle), filePath_(filePath) {}
    ~FileBase();

    void close(); //throw FileError -> optional, but good place to catch errors when closing stream!
    static const FileHandle invalidHandleValue_;

private:
    FileBase           (const FileBase&) = delete;
    FileBase& operator=(const FileBase&) = delete;

    FileHandle fileHandle_ = invalidHandleValue_;
    const Zstring filePath_;
};

//-----------------------------------------------------------------------------------------------

class FileInput : public FileBase
{
public:
    FileInput(                   const Zstring& filePath, const IOCallback& notifyUnbufferedIO /*throw X*/); //throw FileError, ErrorFileLocked
    FileInput(FileHandle handle, const Zstring& filePath, const IOCallback& notifyUnbufferedIO /*throw X*/); //takes ownership!

    size_t read(void* buffer, size_t bytesToRead); //throw FileError, ErrorFileLocked, X; return "bytesToRead" bytes unless end of stream!

private:
    size_t tryRead(void* buffer, size_t bytesToRead); //throw FileError, ErrorFileLocked; may return short, only 0 means EOF! =>  CONTRACT: bytesToRead > 0!

    const IOCallback notifyUnbufferedIO_; //throw X

    std::vector<std::byte> memBuf_ = std::vector<std::byte>(getBlockSize());
    size_t bufPos_   = 0;
    size_t bufPosEnd_= 0;
};


class FileOutput : public FileBase
{
public:
    enum AccessFlag
    {
        ACC_OVERWRITE,
        ACC_CREATE_NEW
    };
    FileOutput(AccessFlag access, const Zstring& filePath, const IOCallback& notifyUnbufferedIO /*throw X*/); //throw FileError, ErrorTargetExisting
    FileOutput(FileHandle handle, const Zstring& filePath, const IOCallback& notifyUnbufferedIO /*throw X*/); //takes ownership!
    ~FileOutput();

    void preAllocateSpaceBestEffort(uint64_t expectedSize); //throw FileError

    void write(const void* buffer, size_t bytesToWrite); //throw FileError, X
    void flushBuffers();                                 //throw FileError, X
    void finalize(); /*= flushBuffers() + close()*/      //throw FileError, X

private:
    size_t tryWrite(const void* buffer, size_t bytesToWrite); //throw FileError; may return short! CONTRACT: bytesToWrite > 0

    IOCallback notifyUnbufferedIO_; //throw X

    std::vector<std::byte> memBuf_ = std::vector<std::byte>(getBlockSize());
    size_t bufPos_    = 0;
    size_t bufPosEnd_ = 0;
};

//-----------------------------------------------------------------------------------------------

//native stream I/O convenience functions:

template <class BinContainer> inline
BinContainer loadBinContainer(const Zstring& filePath, const IOCallback& notifyUnbufferedIO /*throw X*/) //throw FileError, X
{
    FileInput streamIn(filePath, notifyUnbufferedIO); //throw FileError, ErrorFileLocked
    return bufferedLoad<BinContainer>(streamIn); //throw FileError, X
}


template <class BinContainer> inline
void saveBinContainer(const Zstring& filePath, const BinContainer& buffer, const IOCallback& notifyUnbufferedIO /*throw X*/) //throw FileError, X
{
    FileOutput fileOut(FileOutput::ACC_OVERWRITE, filePath, notifyUnbufferedIO); //throw FileError, (ErrorTargetExisting)
    if (!buffer.empty())
    {
        /*snake oil?*/ fileOut.preAllocateSpaceBestEffort(buffer.size()); //throw FileError
        fileOut.write(&*buffer.begin(), buffer.size()); //throw FileError, X
    }
    fileOut.finalize();                                 //throw FileError, X
}
}

#endif //FILE_IO_H_89578342758342572345

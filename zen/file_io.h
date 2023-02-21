// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_IO_H_89578342758342572345
#define FILE_IO_H_89578342758342572345

#include "file_access.h"
#include "serialize.h"
#include "crc.h"
#include "guid.h"


namespace zen
{
    const char LINE_BREAK[] = "\n"; //since OS X Apple uses newline, too

/*  OS-buffered file I/O:
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

    const Zstring& getFilePath() const { return filePath_; }

    size_t getBlockSize(); //throw FileError

    static constexpr size_t defaultBlockSize = 256 * 1024;

    void close(); //throw FileError -> good place to catch errors when closing stream, otherwise called in ~FileBase()!

    const struct stat& getStatBuffered(); //throw FileError

protected:
    FileBase(FileHandle handle, const Zstring& filePath) : hFile_(handle), filePath_(filePath) {}
    ~FileBase();

    void setStatBuffered(const struct stat& fileInfo) { statBuf_ = fileInfo; }

private:
    FileBase           (const FileBase&) = delete;
    FileBase& operator=(const FileBase&) = delete;

    FileHandle hFile_ = invalidFileHandle;
    const Zstring filePath_;
    size_t blockSizeBuf_ = 0;
    std::optional<struct stat> statBuf_;
};

//-----------------------------------------------------------------------------------------------

class FileInputPlain : public FileBase
{
public:
    FileInputPlain(                   const Zstring& filePath); //throw FileError, ErrorFileLocked
    FileInputPlain(FileHandle handle, const Zstring& filePath); //takes ownership!

    //may return short, only 0 means EOF! CONTRACT: bytesToRead > 0!
    size_t tryRead(void* buffer, size_t bytesToRead); //throw FileError, ErrorFileLocked

private:
    FileInputPlain(const std::pair<FileBase::FileHandle, struct stat>& fileDetails, const Zstring& filePath);
};


class FileOutputPlain : public FileBase
{
public:
    FileOutputPlain(                   const Zstring& filePath); //throw FileError, ErrorTargetExisting
    FileOutputPlain(FileHandle handle, const Zstring& filePath); //takes ownership!
    ~FileOutputPlain();

    //preallocate disk space & reduce fragmentation
    void reserveSpace(uint64_t expectedSize); //throw FileError

    //may return short! CONTRACT: bytesToWrite > 0
    size_t tryWrite(const void* buffer, size_t bytesToWrite); //throw FileError

    //close() when done, or else file is considered incomplete and will be deleted!

private:
};

//--------------------------------------------------------------------

namespace impl
{
inline
auto makeTryRead(FileInputPlain& fip, const IoCallback& notifyUnbufferedIO /*throw X*/)
{
    return [&](void* buffer, size_t bytesToRead)
    {
        const size_t bytesRead = fip.tryRead(buffer, bytesToRead); //throw FileError, ErrorFileLocked; may return short, only 0 means EOF! =>  CONTRACT: bytesToRead > 0!
        if (notifyUnbufferedIO) notifyUnbufferedIO(bytesRead); //throw X
        return bytesRead;
    };
}


inline
auto makeTryWrite(FileOutputPlain& fop, const IoCallback& notifyUnbufferedIO /*throw X*/)
{
    return [&](const void* buffer, size_t bytesToWrite)
    {
        const size_t bytesWritten = fop.tryWrite(buffer, bytesToWrite); //throw FileError
        if (notifyUnbufferedIO) notifyUnbufferedIO(bytesWritten); //throw X
        return bytesWritten;
    };
}
}

//--------------------------------------------------------------------

class FileInputBuffered
{
public:
    FileInputBuffered(const Zstring& filePath, const IoCallback& notifyUnbufferedIO /*throw X*/) : //throw FileError, ErrorFileLocked
        fileIn_(filePath), //throw FileError, ErrorFileLocked
        notifyUnbufferedIO_(notifyUnbufferedIO) {}

    //return "bytesToRead" bytes unless end of stream!
    size_t read(void* buffer, size_t bytesToRead) { return streamIn_.read(buffer, bytesToRead); } //throw FileError, ErrorFileLocked, X

private:
    FileInputPlain fileIn_;
    const IoCallback notifyUnbufferedIO_; //throw X

    BufferedInputStream<FunctionReturnTypeT<decltype(&impl::makeTryRead)>>
    streamIn_{impl::makeTryRead(fileIn_, notifyUnbufferedIO_), fileIn_.getBlockSize()}; //throw FileError
};


class FileOutputBuffered
{
public:
    FileOutputBuffered(const Zstring& filePath, const IoCallback& notifyUnbufferedIO /*throw X*/) : //throw FileError, ErrorTargetExisting
        fileOut_(filePath), //throw FileError, ErrorTargetExisting
        notifyUnbufferedIO_(notifyUnbufferedIO) {}

    void write(const void* buffer, size_t bytesToWrite) { streamOut_.write(buffer, bytesToWrite); } //throw FileError, X

    void finalize() //throw FileError, X
    {
        streamOut_.flushBuffer(); //throw FileError, X
        fileOut_.close(); //throw FileError
    }

private:
    FileOutputPlain fileOut_;
    const IoCallback notifyUnbufferedIO_; //throw X

    BufferedOutputStream<FunctionReturnTypeT<decltype(&impl::makeTryWrite)>>
    streamOut_{impl::makeTryWrite(fileOut_, notifyUnbufferedIO_), fileOut_.getBlockSize()}; //throw FileError
};
//-----------------------------------------------------------------------------------------------

//stream I/O convenience functions:

inline
Zstring getPathWithTempName(const Zstring& filePath) //generate (hopefully) unique file name
{
    const Zstring shortGuid_ = printNumber<Zstring>(Zstr("%04x"), static_cast<unsigned int>(getCrc16(generateGUID())));
    return filePath + Zstr('.') + shortGuid_ + Zstr(".tmp");
}

[[nodiscard]] std::string getFileContent(const Zstring& filePath, const IoCallback& notifyUnbufferedIO /*throw X*/); //throw FileError, X

//overwrites if existing + transactional! :)
void setFileContent(const Zstring& filePath, const std::string_view bytes, const IoCallback& notifyUnbufferedIO /*throw X*/); //throw FileError, X
}

#endif //FILE_IO_H_89578342758342572345

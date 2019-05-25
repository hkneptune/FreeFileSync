// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "binary.h"
#include <vector>
#include <chrono>

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


namespace
{
/*
1. there seems to be no perf improvement possible when using file mappings instad of ::ReadFile() calls on Windows:
    => buffered   access: same perf
    => unbuffered access: same perf on USB stick, file mapping 30% slower on local disk

2. Tests on Win7 x64 show that buffer size does NOT matter if files are located on different physical disks!

Impact of buffer size when files are on same disk:

    buffer  MB/s
    ------------
      64    10
     128    19
     512    40
    1024    48
    2048    56
    4096    56
    8192    56
*/
const size_t BLOCK_SIZE_MAX =  16 * 1024 * 1024;


struct StreamReader
{
    StreamReader(const AbstractPath& filePath, const IOCallback& notifyUnbufferedIO) : //throw FileError
        stream_(AFS::getInputStream(filePath, notifyUnbufferedIO)), //throw FileError, ErrorFileLocked
        defaultBlockSize_(stream_->getBlockSize()),
        dynamicBlockSize_(defaultBlockSize_) { assert(defaultBlockSize_ > 0); }

    void appendChunk(std::vector<std::byte>& buffer) //throw FileError, X
    {
        assert(!eof_);
        if (eof_) return;

        buffer.resize(buffer.size() + dynamicBlockSize_);

        const auto startTime = std::chrono::steady_clock::now();
        const size_t bytesRead = stream_->read(&*(buffer.end() - dynamicBlockSize_), dynamicBlockSize_); //throw FileError, ErrorFileLocked, X; return "bytesToRead" bytes unless end of stream!
        const auto stopTime = std::chrono::steady_clock::now();

        buffer.resize(buffer.size() - dynamicBlockSize_ + bytesRead); //caveat: unsigned arithmetics

        if (bytesRead < dynamicBlockSize_)
        {
            eof_ = true;
            return;
        }

        size_t proposedBlockSize = 0;
        const auto loopTime = stopTime - startTime;

        if (loopTime >= std::chrono::milliseconds(100))
            lastDelayViolation_ = stopTime;

        //avoid "flipping back": e.g. DVD-ROMs read 32MB at once, so first read may be > 500 ms, but second one will be 0ms!
        if (stopTime >= lastDelayViolation_ + std::chrono::seconds(2))
        {
            lastDelayViolation_ = stopTime;
            proposedBlockSize = dynamicBlockSize_ * 2;
        }
        if (loopTime > std::chrono::milliseconds(500))
            proposedBlockSize = dynamicBlockSize_ / 2;

        if (defaultBlockSize_ <= proposedBlockSize && proposedBlockSize <= BLOCK_SIZE_MAX)
            dynamicBlockSize_ = proposedBlockSize;
    }

    bool isEof() const { return eof_; }

private:
    const std::unique_ptr<AFS::InputStream> stream_;
    const size_t defaultBlockSize_;
    size_t dynamicBlockSize_;
    std::chrono::steady_clock::time_point lastDelayViolation_ = std::chrono::steady_clock::now();
    bool eof_ = false;
};
}


bool fff::filesHaveSameContent(const AbstractPath& filePath1, const AbstractPath& filePath2, const IOCallback& notifyUnbufferedIO /*throw X*/) //throw FileError, X
{
    int64_t totalUnbufferedIO = 0;

    StreamReader reader1(filePath1, IOCallbackDivider(notifyUnbufferedIO, totalUnbufferedIO)); //throw FileError
    StreamReader reader2(filePath2, IOCallbackDivider(notifyUnbufferedIO, totalUnbufferedIO)); //

    StreamReader* readerLow  = &reader1;
    StreamReader* readerHigh = &reader2;

    std::vector<std::byte> bufferLow;
    std::vector<std::byte> bufferHigh;

    for (;;)
    {
        readerLow->appendChunk(bufferLow); //throw FileError, X

        if (bufferLow.size() > bufferHigh.size())
        {
            bufferLow.swap(bufferHigh);
            std::swap(readerLow, readerHigh);
        }

        if (!std::equal(bufferLow. begin(), bufferLow.end(),
                        bufferHigh.begin()))
            return false;

        if (readerLow->isEof())
        {
            if (bufferLow.size() < bufferHigh.size())
                return false;
            if (readerHigh->isEof())
                break;
            //bufferLow.swap(bufferHigh); not needed
            std::swap(readerLow, readerHigh);
        }

        //don't let sliding buffer grow too large
        bufferHigh.erase(bufferHigh.begin(), bufferHigh.begin() + bufferLow.size());
        bufferLow.clear();
    }

    if (totalUnbufferedIO % 2 != 0)
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    return true;
}

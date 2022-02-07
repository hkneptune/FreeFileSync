// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STREAM_BUFFER_H_08492572089560298
#define STREAM_BUFFER_H_08492572089560298

#include <condition_variable>
#include "ring_buffer.h"
#include "string_tools.h"


namespace zen
{
/*   implement streaming API on top of libcurl's icky callback-based design
        => support copying arbitrarily-large files: https://freefilesync.org/forum/viewtopic.php?t=4471
        => maximum performance through async processing (prefetching + output buffer!)
        => cost per worker thread creation ~ 1/20 ms                                         */
class AsyncStreamBuffer
{
public:
    explicit AsyncStreamBuffer(size_t bufferSize) : bufSize_(bufferSize) { ringBuf_.reserve(bufferSize); }

    //context of input thread, blocking
    //return "bytesToRead" bytes unless end of stream!
    size_t read(void* buffer, size_t bytesToRead) //throw <write error>
    {
        if (bytesToRead == 0) //"read() with a count of 0 returns zero" => indistinguishable from end of file! => check!
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));

        auto       it    = static_cast<std::byte*>(buffer);
        const auto itEnd = it + bytesToRead;

        for (std::unique_lock dummy(lockStream_); it != itEnd;)
        {
            assert(!errorRead_);
            conditionBytesWritten_.wait(dummy, [this] { return errorWrite_ || !ringBuf_.empty() || eof_; });

            if (errorWrite_)
                std::rethrow_exception(errorWrite_); //throw <write error>

            const size_t junkSize = std::min(static_cast<size_t>(itEnd - it), ringBuf_.size());
            ringBuf_.extract_front(it, it + junkSize);
            it += junkSize;

            conditionBytesRead_.notify_all();

            if (eof_) //end of file
                break;
        }

        const size_t bytesRead = it - static_cast<std::byte*>(buffer);
        totalBytesRead_ += bytesRead;
        return bytesRead;
    }

    //context of output thread, blocking
    void write(const void* buffer, size_t bytesToWrite) //throw <read error>
    {
        totalBytesWritten_ += bytesToWrite; //bytes already processed as far as raw FTP access is concerned

        auto       it    = static_cast<const std::byte*>(buffer);
        const auto itEnd = it + bytesToWrite;

        for (std::unique_lock dummy(lockStream_); it != itEnd;)
        {
            assert(!eof_ && !errorWrite_);
            /*  => can't use InterruptibleThread's interruptibleWait() :(
                -> AsyncStreamBuffer is used for input and output streaming
                => both AsyncStreamBuffer::write()/read() would have to implement interruptibleWait()
                => one of these usually called from main thread
                => but interruptibleWait() cannot be called from main thread!          */
            conditionBytesRead_.wait(dummy, [this] { return errorRead_ || ringBuf_.size() < bufSize_; });

            if (errorRead_)
                std::rethrow_exception(errorRead_); //throw <read error>

            const size_t junkSize = std::min(static_cast<size_t>(itEnd - it), bufSize_ - ringBuf_.size());
            ringBuf_.insert_back(it, it + junkSize);
            it += junkSize;

            conditionBytesWritten_.notify_all();
        }
    }

    //context of output thread
    void closeStream()
    {
        {
            std::lock_guard dummy(lockStream_);
            assert(!eof_ && !errorWrite_);
            eof_ = true;
        }
        conditionBytesWritten_.notify_all();
    }

    //context of input thread
    void setReadError(const std::exception_ptr& error)
    {
        {
            std::lock_guard dummy(lockStream_);
            assert(!errorRead_);
            if (!errorRead_)
                errorRead_ = error;
        }
        conditionBytesRead_.notify_all();
    }

    //context of output thread
    void setWriteError(const std::exception_ptr& error)
    {
        {
            std::lock_guard dummy(lockStream_);
            assert(!errorWrite_);
            if (!errorWrite_)
                errorWrite_ = error;
        }
        conditionBytesWritten_.notify_all();
    }

    //context of *output* thread
    void checkReadErrors() //throw <read error>
    {
        std::lock_guard dummy(lockStream_);
        if (errorRead_)
            std::rethrow_exception(errorRead_); //throw <read error>
    }

#if 0 //function not needed: when EOF is reached (without errors), reading is done => no further error can occur!
    void checkWriteErrors() //throw <write error>
    {
        std::lock_guard dummy(lockStream_);
        if (errorWrite_)
            std::rethrow_exception(errorWrite_); //throw <write error>
    }
#endif

    uint64_t getTotalBytesWritten() const { return totalBytesWritten_; }
    uint64_t getTotalBytesRead   () const { return totalBytesRead_; }

private:
    AsyncStreamBuffer           (const AsyncStreamBuffer&) = delete;
    AsyncStreamBuffer& operator=(const AsyncStreamBuffer&) = delete;

    const size_t bufSize_;
    std::mutex lockStream_;
    RingBuffer<std::byte> ringBuf_; //prefetch/output buffer
    bool eof_ = false;
    std::exception_ptr errorWrite_;
    std::exception_ptr errorRead_;
    std::condition_variable conditionBytesWritten_;
    std::condition_variable conditionBytesRead_;

    std::atomic<uint64_t> totalBytesWritten_{0}; //std:atomic is uninitialized by default!
    std::atomic<uint64_t> totalBytesRead_   {0}; //
};
}

#endif //STREAM_BUFFER_H_08492572089560298

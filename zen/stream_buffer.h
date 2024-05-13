// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STREAM_BUFFER_H_08492572089560298
#define STREAM_BUFFER_H_08492572089560298

#include <thread>
//#include <condition_variable>
#include "ring_buffer.h"
#include "string_tools.h"
//#include "thread.h"


namespace zen
{
/*  implement streaming API on top of libcurl's icky callback-based design
        + curl uses READBUFFER_SIZE download buffer size, but returns via a retarded sendf.c::chop_write() writing in small junks of CURL_MAX_WRITE_SIZE (16 kB)
        => support copying arbitrarily-large files: https://freefilesync.org/forum/viewtopic.php?t=4471
        => maximum performance through async processing (prefetching + output buffer!)
        => cost per worker thread creation ~ 1/20 ms                                         */
class AsyncStreamBuffer
{
public:
    explicit AsyncStreamBuffer(size_t capacity) { ringBuf_.reserve(capacity); }

    //context of input thread, blocking
    size_t read(void* buffer, size_t bytesToRead) //throw <write error>; return "bytesToRead" bytes unless end of stream!
    {
        std::unique_lock dummy(lockStream_);
        const auto bufStart = buffer;

        while (bytesToRead > 0)
        {
            const size_t bytesRead = tryReadImpl(dummy, buffer, bytesToRead); //throw <write error>
            if (bytesRead == 0) //end of file
                break;
            conditionBytesRead_.notify_all();
            buffer = static_cast<std::byte*>(buffer) + bytesRead;
            bytesToRead -= bytesRead;
        }
        return static_cast<std::byte*>(buffer) -
               static_cast<std::byte*>(bufStart);
    }

    //context of input thread, blocking
    size_t tryRead(void* buffer, size_t bytesToRead) //throw <write error>; may return short; only 0 means EOF! CONTRACT: bytesToRead > 0!
    {
        size_t bytesRead = 0;
        {
            std::unique_lock dummy(lockStream_);
            bytesRead = tryReadImpl(dummy, buffer, bytesToRead);
        }
        if (bytesRead > 0)
            conditionBytesRead_.notify_all(); //...*outside* the lock
        return bytesRead;
    }

    //context of output thread, blocking
    void write(const void* buffer, size_t bytesToWrite) //throw <read error>
    {
        std::unique_lock dummy(lockStream_);
        while (bytesToWrite > 0)
        {
            const size_t bytesWritten = tryWriteWhileImpl(dummy, buffer, bytesToWrite); //throw <read error>
            conditionBytesWritten_.notify_all();
            buffer = static_cast<const std::byte*>(buffer) + bytesWritten;
            bytesToWrite -= bytesWritten;
        }
    }

    //context of output thread, blocking
    size_t tryWrite(const void* buffer, size_t bytesToWrite) //throw <read error>; may return short! CONTRACT: bytesToWrite > 0
    {
        size_t bytesWritten = 0;
        {
            std::unique_lock dummy(lockStream_);
            bytesWritten = tryWriteWhileImpl(dummy, buffer, bytesToWrite);
        }
        conditionBytesWritten_.notify_all(); //...*outside* the lock
        return bytesWritten;
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
            assert(error && !errorRead_);
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
            assert(error && !errorWrite_);
            if (!errorWrite_)
                errorWrite_ = error;
        }
        conditionBytesWritten_.notify_all();
    }

#if 0
    //function not needed: when writing is completed successfully, no further error can occur!
    //  => caveat: writing is NOT done (yet) when closeStream() is called!
    //context of *output* thread
    void checkReadErrors() //throw <read error>
    {
        std::lock_guard dummy(lockStream_);
        if (errorRead_)
            std::rethrow_exception(errorRead_); //throw <read error>
    }

    //function not needed: when EOF is reached (without errors), reading is done => no further error can occur!
    //context of *input* thread
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

    //context of input thread, blocking
    size_t tryReadImpl(std::unique_lock<std::mutex>& ul, void* buffer, size_t bytesToRead) //throw <write error>; may return short; only 0 means EOF! CONTRACT: bytesToRead > 0!
    {
        if (bytesToRead == 0) //"read() with a count of 0 returns zero" => indistinguishable from end of file! => check!
            throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

        assert(isLocked(lockStream_));
        assert(!errorRead_);

        conditionBytesWritten_.wait(ul, [this] { return errorWrite_ || !ringBuf_.empty() || eof_; });

        if (errorWrite_)
            std::rethrow_exception(errorWrite_); //throw <write error>

        const size_t junkSize = std::min(bytesToRead, ringBuf_.size());
        ringBuf_.extract_front(static_cast<std::byte*>(buffer),
                               static_cast<std::byte*>(buffer)+ junkSize);
        totalBytesRead_ += junkSize;
        return junkSize;
    }

    //context of output thread, blocking
    size_t tryWriteWhileImpl(std::unique_lock<std::mutex>& ul, const void* buffer, size_t bytesToWrite) //throw <read error>; may return short! CONTRACT: bytesToWrite > 0
    {
        if (bytesToWrite == 0)
            throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

        assert(isLocked(lockStream_));
        assert(!eof_ && !errorWrite_);
        /*  => can't use InterruptibleThread's interruptibleWait() :(
            -> AsyncStreamBuffer is used for input and output streaming
            => both AsyncStreamBuffer::write()/read() would have to implement interruptibleWait()
            => one of these usually called from main thread
            => but interruptibleWait() cannot be called from main thread!          */
        conditionBytesRead_.wait(ul, [this] { return errorRead_ || ringBuf_.size() < ringBuf_.capacity(); });

        if (errorRead_)
            std::rethrow_exception(errorRead_); //throw <read error>

        const size_t junkSize = std::min(bytesToWrite, ringBuf_.capacity() - ringBuf_.size());

        ringBuf_.insert_back(static_cast<const std::byte*>(buffer),
                             static_cast<const std::byte*>(buffer) + junkSize);
        totalBytesWritten_ += junkSize;
        return junkSize;
    }

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

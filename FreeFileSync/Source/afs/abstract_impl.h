// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef IMPL_HELPER_H_873450978453042524534234
#define IMPL_HELPER_H_873450978453042524534234

#include "abstract.h"
#include <zen/thread.h>
#include <zen/ring_buffer.h>


namespace fff
{
template <class Function> inline //return ignored error message if available
std::wstring tryReportingDirError(Function cmd /*throw FileError*/, AbstractFileSystem::TraverserCallback& cb /*throw X*/)
{
    for (size_t retryNumber = 0;; ++retryNumber)
        try
        {
            cmd(); //throw FileError
            return std::wstring();
        }
        catch (const zen::FileError& e)
        {
            assert(!e.toString().empty());
            switch (cb.reportDirError(e.toString(), retryNumber)) //throw X
            {
                case AbstractFileSystem::TraverserCallback::ON_ERROR_CONTINUE:
                    return e.toString();
                case AbstractFileSystem::TraverserCallback::ON_ERROR_RETRY:
                    break; //continue with loop
            }
        }
}

template <class Command> inline
bool tryReportingItemError(Command cmd, AbstractFileSystem::TraverserCallback& callback, const Zstring& itemName) //throw X, return "true" on success, "false" if error was ignored
{
    for (size_t retryNumber = 0;; ++retryNumber)
        try
        {
            cmd(); //throw FileError
            return true;
        }
        catch (const zen::FileError& e)
        {
            switch (callback.reportItemError(e.toString(), retryNumber, itemName)) //throw X
            {
                case AbstractFileSystem::TraverserCallback::ON_ERROR_RETRY:
                    break;
                case AbstractFileSystem::TraverserCallback::ON_ERROR_CONTINUE:
                    return false;
            }
        }
}
//==========================================================================================

/*   implement streaming API on top of libcurl's icky callback-based design
        => support copying arbitrarily-large files: https://freefilesync.org/forum/viewtopic.php?t=4471
        => maximum performance through async processing (prefetching + output buffer!)
        => cost per worker thread creation ~ 1/20 ms                                         */
class AsyncStreamBuffer
{
public:
    AsyncStreamBuffer(size_t bufferSize) : bufSize_(bufferSize) { ringBuf_.reserve(bufferSize); }

    //context of input thread, blocking
    //return "bytesToRead" bytes unless end of stream!
    size_t read(void* buffer, size_t bytesToRead) //throw <write error>
    {
        if (bytesToRead == 0) //"read() with a count of 0 returns zero" => indistinguishable from end of file! => check!
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + zen::numberTo<std::string>(__LINE__));

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
    zen::RingBuffer<std::byte> ringBuf_; //prefetch/output buffer
    bool eof_ = false;
    std::exception_ptr errorWrite_;
    std::exception_ptr errorRead_;
    std::condition_variable conditionBytesWritten_;
    std::condition_variable conditionBytesRead_;

    std::atomic<uint64_t> totalBytesWritten_{ 0 }; //std:atomic is uninitialized by default!
    std::atomic<uint64_t> totalBytesRead_   { 0 }; //
};

//==========================================================================================

//Google Drive/MTP happily create duplicate files/folders with the same names, without failing
//=> however, FFS's "check if already exists after failure" idiom *requires* failure
//=> best effort: serialize access (at path level) so that GdriveFileState existence check and file/folder creation act as a single operation
template <class NativePath>
class PathAccessLocker
{
    struct BlockInfo
    {
        std::mutex m;
        bool itemInUse = false; //protected by mutex!
        /* can we get rid of BlockType::fail and save "bool itemInUse" "somewhere else"?
            Google Drive => put dummy entry in GdriveFileState? problem: there is no fail-free removal: accessGlobalFileState() can throw!
            MTP          => no (buffered) state                                                   */
    };
public:
    PathAccessLocker() {}

    //how to handle *other* access attempts while holding the lock:
    enum class BlockType
    {
        otherWait,
        otherFail
    };

    class Lock
    {
    public:
        Lock(const NativePath& nativePath, BlockType blockType) : blockType_(blockType) //throw SysError
        {
            using namespace zen;

            if (const std::shared_ptr<PathAccessLocker> pal = getGlobalInstance())
                pal->pathLocks_.access([&](std::map<NativePath, std::weak_ptr<BlockInfo>>& pathLocks)
            {
                //clean up obsolete entries
                std::erase_if(pathLocks, [](const auto& v) { return !v.second.lock(); });

                //get or create:
                std::weak_ptr<BlockInfo>& weakPtr = pathLocks[nativePath];
                blockInfo_ = weakPtr.lock();
                if (!blockInfo_)
                    weakPtr = blockInfo_ = std::make_shared<BlockInfo>();
            });
            else
                throw SysError(L"PathAccessLocker::Lock() function call not allowed during init/shutdown.");

            blockInfo_->m.lock();

            if (blockInfo_->itemInUse)
            {
                blockInfo_->m.unlock();
                throw SysError(replaceCpy(_("The item %x is currently in use."), L"%x", fmtPath(getItemName(nativePath))));
            }

            if (blockType == BlockType::otherFail)
            {
                blockInfo_->itemInUse = true;
                blockInfo_->m.unlock();
            }
        }

        ~Lock()
        {
            if (blockType_ == BlockType::otherFail)
            {
                blockInfo_->m.lock();
                blockInfo_->itemInUse = false;
            }

            blockInfo_->m.unlock();
        }

    private:
        Lock           (const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;

        const BlockType blockType_; //[!] needed: we can't instead check "itemInUse" (without locking first)
        std::shared_ptr<BlockInfo> blockInfo_;
    };

private:
    PathAccessLocker           (const PathAccessLocker&) = delete;
    PathAccessLocker& operator=(const PathAccessLocker&) = delete;

    static std::shared_ptr<PathAccessLocker> getGlobalInstance();
    static Zstring getItemName(const NativePath& nativePath);

    zen::Protected<std::map<NativePath, std::weak_ptr<BlockInfo>>> pathLocks_;
};

}

#endif //IMPL_HELPER_H_873450978453042524534234

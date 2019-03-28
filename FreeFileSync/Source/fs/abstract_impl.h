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
inline
AfsPath sanitizeRootRelativePath(Zstring relPath)
{
    if constexpr (FILE_NAME_SEPARATOR != Zstr('/' )) replace(relPath, Zstr('/'),  FILE_NAME_SEPARATOR);
    if constexpr (FILE_NAME_SEPARATOR != Zstr('\\')) replace(relPath, Zstr('\\'), FILE_NAME_SEPARATOR);
    trim(relPath, true, true, [](Zchar c) { return c == FILE_NAME_SEPARATOR; });
    return AfsPath(std::move(relPath));
}


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

//==========================================================================================

/*
implement streaming API on top of libcurl's icky callback-based design
    => support copying arbitrarily-large files: https://freefilesync.org/forum/viewtopic.php?t=4471
    => maximum performance through async processing (prefetching + output buffer!)
    => cost per worker thread creation ~ 1/20 ms
*/
class AsyncStreamBuffer
{
public:
    AsyncStreamBuffer(size_t bufferSize) : bufferSize_(bufferSize) { ringBuf_.reserve(bufferSize); }

    //context of output thread, blocking
    void write(const void* buffer, size_t bytesToWrite) //throw <read error>
    {
        totalBytesWritten_ += bytesToWrite; //bytes already processed as far as raw FTP access is concerned

        auto       it    = static_cast<const std::byte*>(buffer);
        const auto itEnd = it + bytesToWrite;

        for (std::unique_lock dummy(lockStream_); it != itEnd;)
        {
            //=> can't use InterruptibleThread's interruptibleWait() :(
            //   -> AsyncStreamBuffer is used for input and output streaming
            //    => both AsyncStreamBuffer::write()/read() would have to implement interruptibleWait()
            //    => one of these usually called from main thread
            //    => but interruptibleWait() cannot be called from main thread!
            conditionBytesRead_.wait(dummy, [this] { return errorRead_ || ringBuf_.size() < bufferSize_; });

            if (errorRead_)
                std::rethrow_exception(errorRead_); //throw <read error>

            const size_t junkSize = std::min(static_cast<size_t>(itEnd - it), bufferSize_ - ringBuf_.size());
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
            assert(!eof_);
            eof_ = true;
        }
        conditionBytesWritten_.notify_all();
    }

    //context of input thread, blocking
    //return "bytesToRead" bytes unless end of stream!
    size_t read(void* buffer, size_t bytesToRead) //throw <write error>
    {
        auto       it    = static_cast<std::byte*>(buffer);
        const auto itEnd = it + bytesToRead;

        for (std::unique_lock dummy(lockStream_); it != itEnd;)
        {
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

    // -> function not needed: when EOF is reached (without errors), reading is done => no further error can occur!
    //void checkWriteErrors() //throw <write error>
    //{
    //    std::lock_guard dummy(lockStream_);
    //    if (errorWrite_)
    //        std::rethrow_exception(errorWrite_); //throw <write error>
    //}

    uint64_t getTotalBytesWritten() const { return totalBytesWritten_; }
    uint64_t getTotalBytesRead   () const { return totalBytesRead_; }

private:
    AsyncStreamBuffer           (const AsyncStreamBuffer&) = delete;
    AsyncStreamBuffer& operator=(const AsyncStreamBuffer&) = delete;

    const size_t bufferSize_;
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

template <class Context, class Function>
struct Task
{
    Function getResult; //throw FileError
    /* [[no_unique_address]] */ Context ctx;
};


template <class Context, class Function>
struct TaskResult
{
    Task<Context, Function>  wi;
    std::exception_ptr       error;  //mutually exclusive
    decltype(wi.getResult()) value;  //
};

enum class SchedulerStatus
{
    HAVE_RESULT,
    FINISHED,
};

template <class Context, class... Functions> //avoid std::function memory alloc + virtual calls
class TaskScheduler
{
public:
    TaskScheduler(size_t threadCount, const std::string& groupName) :
        threadGroup_(zen::ThreadGroup<std::function<void()>>(threadCount, groupName)) {}

    ~TaskScheduler() { threadGroup_ = {}; } //TaskScheduler must out-live threadGroup! (captured "this")

    //context of controlling thread, non-blocking:
    template <class Function>
    void run(Task<Context, Function>&& wi, bool insertFront = false)
    {
        threadGroup_->run([this, wi = std::move(wi)]
        {
            try {         this->returnResult<Function>({ wi, nullptr, wi.getResult() }); } //throw FileError
            catch (...) { this->returnResult<Function>({ wi, std::current_exception(), {} }); }
        }, insertFront);

        std::lock_guard dummy(lockResult_);
        ++resultsPending_;
    }

    //context of controlling thread, blocking:
    SchedulerStatus getResults(std::tuple<std::vector<TaskResult<Context, Functions>>...>& results)
    {
        std::apply([](auto&... r) { (..., r.clear()); }, results);

        std::unique_lock dummy(lockResult_);

        auto resultsReady = [&]
        {
            bool ready = false;
            std::apply([&ready](const auto&... r) { ready = (... || !r.empty()); }, results_);
            return ready;
        };

        if (!resultsReady() && resultsPending_ == 0)
            return SchedulerStatus::FINISHED;

        conditionNewResult_.wait(dummy, [&resultsReady] { return resultsReady(); });

        results.swap(results_); //reuse memory + avoid needless item-level mutex locking
        return SchedulerStatus::HAVE_RESULT;
    }

private:
    TaskScheduler           (const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    //context of worker threads, non-blocking:
    template <class Function>
    void returnResult(TaskResult<Context, Function>&& r)
    {
        {
            std::lock_guard dummy(lockResult_);

            std::get<std::vector<TaskResult<Context, Function>>>(results_).push_back(std::move(r));
            --resultsPending_;
        }
        conditionNewResult_.notify_all();
    }

    std::optional<zen::ThreadGroup<std::function<void()>>> threadGroup_;

    std::mutex lockResult_;
    size_t resultsPending_ = 0;
    std::tuple<std::vector<TaskResult<Context, Functions>>...> results_;
    std::condition_variable conditionNewResult_;
};


struct TravContext
{
    Zstring errorItemName; //empty if all items affected
    size_t errorRetryCount = 0;
    std::shared_ptr<AbstractFileSystem::TraverserCallback> cb; //call by controlling thread only! => don't require traverseFolderParallel() callbacks to be thread-safe!
};


template <class... Functions>
class GenericDirTraverser
{
public:
    using Function1 = zen::GetFirstOfT<Functions...>;

    GenericDirTraverser(std::vector<Task<TravContext, Function1>>&& initialTasks /*throw X*/, size_t parallelOps, const std::string& threadGroupName) :
        scheduler_(parallelOps, threadGroupName)
    {
        //set the initial work load
        for (auto& item : initialTasks)
            scheduler_.template run<Function1>(std::move(item));

        //run loop
        std::tuple<std::vector<TaskResult<TravContext, Functions>>...> results; //avoid per-getNextResults() memory allocations (=> swap instead!)

        while (scheduler_.getResults(results) == SchedulerStatus::HAVE_RESULT)
            std::apply([&](auto&... r) { (..., this->evalResultList(r)); }, results); //throw X
    }

private:
    GenericDirTraverser           (const GenericDirTraverser&) = delete;
    GenericDirTraverser& operator=(const GenericDirTraverser&) = delete;

    template <class Function>
    void evalResultList(std::vector<TaskResult<TravContext, Function>>& results /*throw X*/)
    {
        for (TaskResult<TravContext, Function>& result : results)
            evalResult(result); //throw X
    }

    template <class Function>
    void evalResult(TaskResult<TravContext, Function>& result /*throw X*/);

    //specialize!
    template <class Function>
    void evalResultValue(const typename Function::Result& r, std::shared_ptr<AbstractFileSystem::TraverserCallback>& cb /*throw X*/);

    TaskScheduler<TravContext, Functions...> scheduler_;
};


template <class... Functions>
template <class Function>
void GenericDirTraverser<Functions...>::evalResult(TaskResult<TravContext, Function>& result /*throw X*/)
{
    auto& cb = result.wi.ctx.cb;
    try
    {
        if (result.error)
            std::rethrow_exception(result.error); //throw FileError
    }
    catch (const zen::FileError& e)
    {
        switch (result.wi.ctx.errorItemName.empty() ?
                cb->reportDirError (e.toString(), result.wi.ctx.errorRetryCount) : //throw X
                cb->reportItemError(e.toString(), result.wi.ctx.errorRetryCount, result.wi.ctx.errorItemName)) //throw X
        {
            case AbstractFileSystem::TraverserCallback::ON_ERROR_RETRY:
                //user expects that the task is retried immediately => we can't do much about other errors already waiting in the queue, but at least *prepend* to the work load!
                scheduler_.template run<Function>({ std::move(result.wi.getResult), TravContext{ result.wi.ctx.errorItemName, result.wi.ctx.errorRetryCount + 1, cb }},
                                                  true /*insertFront*/);
                return;

            case AbstractFileSystem::TraverserCallback::ON_ERROR_CONTINUE:
                return;
        }
    }
    evalResultValue<Function>(result.value, cb); //throw X
}
}

#endif //IMPL_HELPER_H_873450978453042524534234

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef IMPL_HELPER_H_873450978453042524534234
#define IMPL_HELPER_H_873450978453042524534234

#include "abstract.h"
#include <zen/thread.h>


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

    ~TaskScheduler() { threadGroup_ = zen::NoValue(); } //TaskScheduler must out-live threadGroup! (captured "this")

    //context of controlling thread, non-blocking:
    template <class Function>
    void run(Task<Context, Function>&& wi)
    {
        threadGroup_->run([this, wi = std::move(wi)]
        {
            try {         this->returnResult<Function>({ wi, nullptr, wi.getResult() }); } //throw FileError
            catch (...) { this->returnResult<Function>({ wi, std::current_exception(), {} }); }
        });

        std::lock_guard<std::mutex> dummy(lockResult_);
        ++resultsPending_;
    }

    //context of controlling thread, blocking:
    SchedulerStatus getResults(std::tuple<std::vector<TaskResult<Context, Functions>>...>& results)
    {
        std::apply([](auto&... r) { (..., r.clear()); }, results);

        std::unique_lock<std::mutex> dummy(lockResult_);

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
            std::lock_guard<std::mutex> dummy(lockResult_);

            std::get<std::vector<TaskResult<Context, Function>>>(results_).push_back(std::move(r));
            --resultsPending_;
        }
        conditionNewResult_.notify_all();
    }

    zen::Opt<zen::ThreadGroup<std::function<void()>>> threadGroup_;

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

    GenericDirTraverser(std::vector<Task<TravContext, Function1>>&& initialTasks, size_t parallelOps, const std::string& threadGroupName) :
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
    void evalResultList(std::vector<TaskResult<TravContext, Function>>& results) //throw X
    {
        for (TaskResult<TravContext, Function>& result : results)
            evalResult(result); //throw X
    }

    template <class Function>
    void evalResult(TaskResult<TravContext, Function>& result); //throw X

    //specialize!
    template <class Function>
    void evalResultValue(const typename Function::Result& r, std::shared_ptr<AbstractFileSystem::TraverserCallback>& cb); //throw X

    TaskScheduler<TravContext, Functions...> scheduler_;
};


template <class... Functions>
template <class Function>
void GenericDirTraverser<Functions...>::evalResult(TaskResult<TravContext, Function>& result) //throw X
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
                scheduler_.template run<Function>({ std::move(result.wi.getResult), TravContext{ result.wi.ctx.errorItemName, result.wi.ctx.errorRetryCount + 1, cb }});
                return;

            case AbstractFileSystem::TraverserCallback::ON_ERROR_CONTINUE:
                return;
        }
    }
    evalResultValue<Function>(result.value, cb); //throw X
}
}

#endif //IMPL_HELPER_H_873450978453042524534234

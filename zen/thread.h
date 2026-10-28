// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************
#pragma once

#include <thread>
#include <future>
#include <functional>
#include "ring_buffer.h"
#include "zstring.h"

namespace zen
{
class InterruptionStatus;

class InterruptibleThread
{
public:
    InterruptibleThread() {}
    InterruptibleThread           (InterruptibleThread&&    ) noexcept = default;
    InterruptibleThread& operator=(InterruptibleThread&& tmp) noexcept //don't use swap() but end stdThread_ life time immediately
    {
        if (joinable())
        {
            requestStop();
            join();
        }
        intStatus_ = std::move(tmp.intStatus_);
        stdThread_ = std::move(tmp.stdThread_);
        return *this;
    }

    template <class Function>
    explicit InterruptibleThread(Function&& f);

    ~InterruptibleThread()
    {
        if (joinable())
        {
            requestStop();
            join();
        }
    }

    bool joinable () const { return stdThread_.joinable(); }
    void requestStop();
    void join     () { stdThread_.join(); }
    void detach   () { stdThread_.detach(); }

private:
    std::shared_ptr<InterruptionStatus> intStatus_ = std::make_shared<InterruptionStatus>();
    std::thread stdThread_;
};


class ThreadStopRequest {};

//context of worker thread:
void interruptionPoint(); //throw ThreadStopRequest

template <class Predicate>
void interruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lock, Predicate pred); //throw ThreadStopRequest

template <class Rep, class Period>
void interruptibleSleep(const std::chrono::duration<Rep, Period>& relTime); //throw ThreadStopRequest

void setCurrentThreadName(const Zstring& threadName);


bool runningOnMainThread();

//------------------------------------------------------------------------------------------

/*  std::async replacement without crappy semantics:
        1. guaranteed to run asynchronously
        2. does not follow C++11 [futures.async], Paragraph 5, where std::future waits for thread in destructor

    Example:
            Zstring dirPath = ...
            auto ft = zen::runAsync([=]{ return zen::dirExists(dirPath); });
            if (ft.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready && ft.get())
                //dir existing                                                                               */
template <class Function>
auto runAsync(Function&& fun);

//wait for all with a time limit: return true if *all* results are available!
//TODO: use std::when_all when available
template <class InputIterator, class Duration>
bool waitForAllTimed(InputIterator first, InputIterator last, const Duration& wait_duration);

template<typename T> inline
bool isReady(const std::future<T>& f) { assert(f.valid()); return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
//------------------------------------------------------------------------------------------

//wait until first job is successful or all failed
//TODO: use std::when_any when available
template <class T>
class AsyncFirstResult
{
public:
    AsyncFirstResult();

    template <class Fun>
    void addJob(Fun&& f); //f must return a std::optional<T> containing a value if successful

    template <class Duration>
    bool timedWait(const Duration& duration) const; //true: "get()" is ready, false: time elapsed

    //return first value or none if all jobs failed; blocks until result is ready!
    std::optional<T> get() const; //may be called only once!

private:
    class AsyncResult;
    std::shared_ptr<AsyncResult> asyncResult_;
    size_t jobsTotal_ = 0;
};

//------------------------------------------------------------------------------------------

//value associated with mutex and guaranteed protected access:
//TODO: use std::synchronized_value when available https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rconc-mutex
template <class T>
class Protected
{
public:
    Protected() {}

    template <class T2>
    explicit Protected(T2&& value) : value_(std::forward<T2>(value)) {}

    template <class Function>
    auto access(Function&& fun) //let's not use decltype(auto): may return reference to internals?
    {
        std::lock_guard dummy(lockValue_);
        return std::forward<Function>(fun)(value_);
    }

private:
    Protected           (const Protected&) = delete;
    Protected& operator=(const Protected&) = delete;

    std::mutex lockValue_;
    T value_{};
};

//------------------------------------------------------------------------------------------

template <class Function>
class ThreadGroup
{
public:
    ThreadGroup(const Zstring& groupName, size_t threadCountMax = std::max(std::thread::hardware_concurrency(), 1U)) :
         threadCountMax_(threadCountMax), groupName_(groupName)
    { if (threadCountMax == 0) throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!"); }

    ThreadGroup           (ThreadGroup&& tmp) noexcept = default; //noexcept *required* to support move for reallocations in std::vector and std::swap!!!
    ThreadGroup& operator=(ThreadGroup&& tmp) noexcept = default; //don't use swap() but end worker_ life time immediately

    ~ThreadGroup()
    {
        for (InterruptibleThread& w : worker_)
            w.requestStop(); //similar, but not the same as ~InterruptibleThread: stop *all* at the same time before each join!

        if (detach_) //detach() without requestStop() doesn't make sense
            for (InterruptibleThread& w : worker_)
                w.detach();
    }

    //context of controlling OR worker thread, non-blocking:
    void run(Function&& wi /*should throw ThreadStopRequest when needed*/, bool insertFront = false)
    {
        {
            std::lock_guard dummy(workLoad_.ref().lock);

            if (insertFront)
                workLoad_.ref().tasks.push_front(std::move(wi));
            else
                workLoad_.ref().tasks.push_back(std::move(wi));
            const size_t tasksPending = ++(workLoad_.ref().tasksPending);

            if (worker_.size() < std::min(tasksPending, threadCountMax_))
                addWorkerThread();
        }
        workLoad_.ref().conditionNewTask.notify_all();
    }

    //context of controlling thread, blocking:
    void wait()
    {
        //perf: no difference in xBRZ test case compared to std::condition_variable-based implementation
        auto promDone = std::make_shared<std::promise<void>>(); //
        std::future<void> futDone = promDone->get_future();

        notifyWhenDone([promDone] { promDone->set_value(); }); //std::function doesn't support construction involving move-only types!
        //use reference? => potential lifetime issue, e.g. promise object theoretically might be accessed inside set_value() after future gets signalled

        futDone.get();
    }

    //non-blocking wait()-alternative: context of controlling thread:
    void notifyWhenDone(const std::function<void()>& onCompletion /*noexcept! runs on worker thread!*/)
    {
        std::unique_lock dummy(workLoad_.ref().lock);

        if (workLoad_.ref().tasksPending == 0)
        {
            dummy.unlock();
            onCompletion();
        }
        else
            workLoad_.ref().onCompletionCallbacks.push_back(onCompletion);
    }

    //context of controlling thread:
    void detach() { detach_ = true; } //not expected to also interrupt!

private:
    ThreadGroup           (const ThreadGroup&) = delete;
    ThreadGroup& operator=(const ThreadGroup&) = delete;

    void addWorkerThread()
    {
        Zstring threadName = groupName_ + Zstr('[') + numberTo<Zstring>(worker_.size() + 1) + Zstr('/') + numberTo<Zstring>(threadCountMax_) + Zstr(']');

        worker_.emplace_back([sharedWorkLoad = workLoad_, threadName = std::move(threadName)] mutable //don't capture "this"! consider detach() and move operations
        {
            setCurrentThreadName(threadName);
            WorkLoad& workLoad = sharedWorkLoad.ref();

            std::unique_lock dummy(workLoad.lock);
            for (;;)
            {
                interruptibleWait(workLoad.conditionNewTask, dummy, [&tasks = workLoad.tasks] { return !tasks.empty(); }); //throw ThreadStopRequest

                Function task = std::move(workLoad.tasks.    front()); //noexcept thanks to move
                /**/                      workLoad.tasks.pop_front();  //

                dummy.unlock();
                task(); //throw ThreadStopRequest?
                dummy.lock();

                if (--workLoad.tasksPending == 0)
                    if (!workLoad.onCompletionCallbacks.empty())
                    {
                        std::vector<std::function<void()>> callbacks = std::exchange(workLoad.onCompletionCallbacks, {});

                        dummy.unlock();
                        for (const auto& cb : callbacks)
                            cb(); //noexcept!
                        dummy.lock();
                    }
            }
        });
    }

    struct WorkLoad
    {
        std::mutex lock;
        RingBuffer<Function> tasks; //FIFO! :)
        size_t tasksPending = 0;
        std::condition_variable conditionNewTask;
        std::vector<std::function<void()>> onCompletionCallbacks;
    };

    std::vector<InterruptibleThread> worker_;
    SharedRef<WorkLoad> workLoad_ = makeSharedRef<WorkLoad>();
    bool detach_ = false;
    size_t threadCountMax_;
    Zstring groupName_;
};


template <class T, class Function> inline
auto parallelTransform(const std::vector<T>& items, Function&& fun /*throw X*/, const Zstring& threadGroupName) //throw X
{
    using Result = decltype(fun(std::declval<const T&>()));

    std::vector<std::optional<Result>> results(items.size());

    std::mutex lockFinished;
    bool allDone = false;
    std::exception_ptr firstException;
    std::condition_variable conditionFinished;

    auto makeTask = [&](size_t i)
    {
        return [&, i]
        {
            try
            {
                results[i] = fun(items[i]); //throw X
            }
            catch (...)
            {
                {
                    std::lock_guard<std::mutex> dummy(lockFinished);
                    if (!firstException)
                        firstException = std::current_exception();
                }
                conditionFinished.notify_one();
            }
        };
    };

    //lifetime: create *after* dependent variables
    ThreadGroup<decltype(makeTask(0))> threadGroup(threadGroupName);

    for (size_t i = 0; i < items.size(); ++i)
        threadGroup.run(makeTask(i));

    threadGroup.notifyWhenDone([&]
    {
        {
            std::lock_guard<std::mutex> dummy(lockFinished);
            allDone = true;
        }
        conditionFinished.notify_one();
    });

    {
        std::unique_lock<std::mutex> dummy(lockFinished);
        conditionFinished.wait(dummy, [&] { return allDone || firstException; });

        if (firstException)
            std::rethrow_exception(firstException); //throw X => ~ThreadGroup cancels outstanding tasks
    }

    std::vector<Result> out;
    out.reserve(items.size());

    for (std::optional<Result>& r : results)
        out.emplace_back(std::move(*r));

    return out;
}








//###################### implementation ######################

namespace impl
{
//thread_local with non-POD seems to cause memory leaks on VS 14 => pointer only is fine:
inline thread_local InterruptionStatus* threadLocalInterruptionStatus = nullptr;
}


template <class Function> inline
InterruptibleThread::InterruptibleThread(Function&& f)
{
    stdThread_ = std::thread(
    [f = std::forward<Function>(f),
     intStatus = intStatus_] mutable
    {
        assert(!impl::threadLocalInterruptionStatus);
        impl::threadLocalInterruptionStatus = intStatus.get();
        ZEN_ON_SCOPE_EXIT(impl::threadLocalInterruptionStatus = nullptr);

        try
        {
            f(); //throw ThreadStopRequest
        }
        catch (ThreadStopRequest&) {}
    });
}


class InterruptionStatus
{
public:
    //context of InterruptibleThread instance:
    void requestStop()
    {
        stopRequested_ = true;

        {
            std::lock_guard dummy(lockStopRequest_); //needed! makes sure the following signal is not lost!
            //usually we'd make "stopRequested_" non-atomic, but this is already given due to interruptibleWait() handling
        }
        conditionStopRequested_.notify_all();

        std::lock_guard dummy(lockConditionPtr_);
        if (activeCondition_)
            activeCondition_->notify_all(); //signal may get lost!
        //alternative design locking the cv's mutex (after lockConditionPtr_) would dead lock
        //when worker thread acquires locks in opposite order in interruptibleWait(): 1. cv's mutex 2. lockConditionPtr_
    }

    //context of worker thread:
    bool stopRequested() const { return stopRequested_; }

    //context of worker thread:
    template <class Predicate>
    void interruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lock, Predicate pred) //throw ThreadStopRequest
    {
        setActiveCondition(&cv);
        ZEN_ON_SCOPE_EXIT(setActiveCondition(nullptr));

        //"stopRequested_" is not protected by cv's mutex => signal may get lost!!! e.g. after condition was checked but before the wait begins
        //=> add artifical time out to mitigate! CPU: 0.25% vs 0% for longer time out! => 2026-01-30: not even visible in Process Explorer anymore

        //std::condition_variable_any to the rescue? (using an additional internal mutex = "slower"?)
        //see "C++ Concurrency in Action 9.2.4 Interrupting a wait on std::condition_variable_any"
        //https://github.com/josuttis/jthread/blob/0fa8d394254886c555d6faccd0a3de819b7d47f8/source/condition_variable_any2.hpp#L87

        while (!cv.wait_for(lock, std::chrono::milliseconds(1), [&] { return this->stopRequested_ || pred(); }))
            ;

        if (stopRequested_)
            throw ThreadStopRequest();
    }

    //context of worker thread:
    template <class Rep, class Period>
    void interruptibleSleep(const std::chrono::duration<Rep, Period>& relTime) //throw ThreadStopRequest
    {
        std::unique_lock lock(lockStopRequest_);
        if (conditionStopRequested_.wait_for(lock, relTime, [this] { return static_cast<bool>(this->stopRequested_); }))
            throw ThreadStopRequest();
    }

private:
    void setActiveCondition(std::condition_variable* cv)
    {
        std::lock_guard dummy(lockConditionPtr_);
        activeCondition_ = cv;
    }

    std::atomic<bool> stopRequested_{false}; //std::atomic is uninitialized by default!!!
    //"The default constructor is trivial: no initialization takes place other than zero initialization of static and thread-local objects."

    std::condition_variable* activeCondition_ = nullptr;
    std::mutex lockConditionPtr_; //serialize pointer access (only!)

    std::condition_variable conditionStopRequested_;
    std::mutex lockStopRequest_;
};


inline
void InterruptibleThread::requestStop() { intStatus_->requestStop(); }


//context of worker thread:
inline
void interruptionPoint() //throw ThreadStopRequest
{
    assert(impl::threadLocalInterruptionStatus);
    if (impl::threadLocalInterruptionStatus && impl::threadLocalInterruptionStatus->stopRequested())
        throw ThreadStopRequest();
}


//context of worker thread:
template <class Predicate> inline
void interruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lock, Predicate pred) //throw ThreadStopRequest
{
    assert(impl::threadLocalInterruptionStatus);
    if (impl::threadLocalInterruptionStatus)
        impl::threadLocalInterruptionStatus->interruptibleWait(cv, lock, pred);
    else
        cv.wait(lock, pred);
}


//context of worker thread:
template <class Rep, class Period> inline
void interruptibleSleep(const std::chrono::duration<Rep, Period>& relTime) //throw ThreadStopRequest
{
    assert(impl::threadLocalInterruptionStatus);
    if (impl::threadLocalInterruptionStatus)
        impl::threadLocalInterruptionStatus->interruptibleSleep(relTime);
    else
        std::this_thread::sleep_for(relTime);
}

//------------------------------------------------------------------------------------------

template <class Function> inline
auto runAsync(Function&& fun)
{
    using ResultType = decltype(fun());

    std::packaged_task<ResultType()> pt(std::forward<Function>(fun));
    auto fut = pt.get_future();
    std::thread(std::move(pt)).detach();
    return fut;
}


template <class InputIterator, class Duration> inline
bool waitForAllTimed(InputIterator first, InputIterator last, const Duration& duration)
{
    const std::chrono::steady_clock::time_point stopTime = std::chrono::steady_clock::now() + duration;
    for (; first != last; ++first)
        if (first->wait_until(stopTime) == std::future_status::timeout)
            return false;
    return true;
}


template <class T>
class AsyncFirstResult<T>::AsyncResult
{
public:
    //context: worker threads
    void reportFinished(std::optional<T>&& result)
    {
        {
            std::lock_guard dummy(lockResult_);
            ++jobsFinished_;
            if (!result_)
                result_ = std::move(result);
        }
        conditionJobDone_.notify_all(); //better notify all, considering bugs like: https://svn.boost.org/trac/boost/ticket/7796
    }

    //context: main thread
    template <class Duration>
    bool waitForResult(size_t jobsTotal, const Duration& duration)
    {
        std::unique_lock dummy(lockResult_);
        return conditionJobDone_.wait_for(dummy, duration, [&] { return this->jobDone(jobsTotal); });
    }

    std::optional<T> getResult(size_t jobsTotal)
    {
        std::unique_lock dummy(lockResult_);
        conditionJobDone_.wait(dummy, [&] { return this->jobDone(jobsTotal); });
        return std::move(result_);
    }

private:
    bool jobDone(size_t jobsTotal) const { return result_ || (jobsFinished_ >= jobsTotal); } //call while locked!

    std::mutex lockResult_;
    std::optional<T> result_; //our condition is: "have result" or "jobsFinished_ == jobsTotal"
    size_t jobsFinished_ = 0; //
    std::condition_variable conditionJobDone_;
};



template <class T> inline
AsyncFirstResult<T>::AsyncFirstResult() : asyncResult_(std::make_shared<AsyncResult>()) {}


template <class T>
template <class Fun> inline
void AsyncFirstResult<T>::addJob(Fun&& f) //f must return a std::optional<T> containing a value on success
{
    //uncaught exception? => let the app crash at throw location: requires InterruptibleThread, not std::thread!
    InterruptibleThread t([asyncResult = asyncResult_, f = std::forward<Fun>(f)] { asyncResult->reportFinished(f()); });
    ++jobsTotal_;
    t.detach();
}


template <class T>
template <class Duration> inline
bool AsyncFirstResult<T>::timedWait(const Duration& duration) const { return asyncResult_->waitForResult(jobsTotal_, duration); }


template <class T> inline
std::optional<T> AsyncFirstResult<T>::get() const { return asyncResult_->getResult(jobsTotal_); }
}

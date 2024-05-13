// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef THREAD_H_7896323423432235246427
#define THREAD_H_7896323423432235246427

#include <thread>
#include <future>
#include <functional>
#include "ring_buffer.h"
#include "zstring.h"


namespace zen
{
class InterruptionStatus;

//migrate towards https://en.cppreference.com/w/cpp/thread/jthread
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
        stdThread_ = std::move(tmp.stdThread_);
        intStatus_ = std::move(tmp.intStatus_);
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
    std::thread stdThread_;
    std::shared_ptr<InterruptionStatus> intStatus_ = std::make_shared<InterruptionStatus>();
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
    explicit Protected(T& value) : value_(value) {}
    //Protected(T&& tmp ) : value_(std::move(tmp)) {} <- wait until needed

    template <class Function>
    auto access(Function fun) //-> decltype(fun(std::declval<T&>()))
    {
        std::lock_guard dummy(lockValue_);
        return fun(value_);
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
    ThreadGroup(size_t threadCountMax, const Zstring& groupName) : threadCountMax_(threadCountMax), groupName_(groupName)
    { if (threadCountMax == 0) throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!"); }

    ThreadGroup           (ThreadGroup&& tmp) noexcept = default; //noexcept *required* to support move for reallocations in std::vector and std::swap!!!
    ThreadGroup& operator=(ThreadGroup&& tmp) noexcept = default; //don't use swap() but end worker_ life time immediately

    ~ThreadGroup()
    {
        for (InterruptibleThread& w : worker_)
            w.requestStop(); //similar, but not the same as ~InterruptibleThread: stop *all* at the same time before join!

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

        worker_.emplace_back([workLoad_ /*clang bug*/= workLoad_ /*share ownership!*/, threadName = std::move(threadName)]() mutable //don't capture "this"! consider detach() and move operations
        {
            setCurrentThreadName(threadName);
            WorkLoad& workLoad = workLoad_.ref();

            std::unique_lock dummy(workLoad.lock);
            for (;;)
            {
                interruptibleWait(workLoad.conditionNewTask, dummy, [&tasks = workLoad.tasks] { return !tasks.empty(); }); //throw ThreadStopRequest

                Function task = std::move(workLoad.tasks.    front()); //noexcept thanks to move
                /**/                      workLoad.tasks.pop_front();  //

                dummy.unlock();
                task(); //throw ThreadStopRequest?
                dummy.lock();

                if (--(workLoad.tasksPending) == 0)
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








//###################### implementation ######################

namespace impl
{
template <class Function> inline
auto runAsync(Function&& fun, std::true_type /*copy-constructible*/)
{
    using ResultType = decltype(fun());

    //note: std::packaged_task does NOT support move-only function objects!
    std::packaged_task<ResultType()> pt(std::forward<Function>(fun));
    auto fut = pt.get_future();
    std::thread(std::move(pt)).detach(); //we have to explicitly detach since C++11: [thread.thread.destr] ~thread() calls std::terminate() if joinable()!!!
    return fut;
}


template <class Function> inline
auto runAsync(Function&& fun, std::false_type /*copy-constructible*/)
{
    //support move-only function objects!
    auto sharedFun = std::make_shared<Function>(std::forward<Function>(fun));
    return runAsync([sharedFun] { return (*sharedFun)(); }, std::true_type());
}
}


template <class Function> inline
auto runAsync(Function&& fun)
{
    return impl::runAsync(std::forward<Function>(fun), std::is_copy_constructible<Function>());
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
    size_t jobsFinished_ = 0; //
    std::optional<T> result_; //our condition is: "have result" or "jobsFinished_ == jobsTotal"
    std::condition_variable conditionJobDone_;
};



template <class T> inline
AsyncFirstResult<T>::AsyncFirstResult() : asyncResult_(std::make_shared<AsyncResult>()) {}


template <class T>
template <class Fun> inline
void AsyncFirstResult<T>::addJob(Fun&& f) //f must return a std::optional<T> containing a value on success
{
    std::thread t([asyncResult = this->asyncResult_, f = std::forward<Fun>(f)] { asyncResult->reportFinished(f()); });
    ++jobsTotal_;
    t.detach(); //we have to be explicit since C++11: [thread.thread.destr] ~thread() calls std::terminate() if joinable()!!!
}


template <class T>
template <class Duration> inline
bool AsyncFirstResult<T>::timedWait(const Duration& duration) const { return asyncResult_->waitForResult(jobsTotal_, duration); }


template <class T> inline
std::optional<T> AsyncFirstResult<T>::get() const { return asyncResult_->getResult(jobsTotal_); }

//------------------------------------------------------------------------------------------

class InterruptionStatus
{
public:
    //context of InterruptibleThread instance:
    void requestStop()
    {
        stopRequested_ = true;

        {
            std::lock_guard dummy(lockSleep_); //needed! makes sure the following signal is not lost!
            //usually we'd make "interrupted" non-atomic, but this is already given due to interruptibleWait() handling
        }
        conditionSleepInterruption_.notify_all();

        std::lock_guard dummy(lockConditionPtr_);
        if (activeCondition_)
            activeCondition_->notify_all(); //signal may get lost!
        //alternative design locking the cv's mutex here could be dangerous: potential for dead lock!
    }

    //context of worker thread:
    void throwIfStopped() //throw ThreadStopRequest
    {
        if (stopRequested_)
            throw ThreadStopRequest();
    }

    //context of worker thread:
    template <class Predicate>
    void interruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lock, Predicate pred) //throw ThreadStopRequest
    {
        setConditionVar(&cv);
        ZEN_ON_SCOPE_EXIT(setConditionVar(nullptr));

        //"stopRequested_" is not protected by cv's mutex => signal may get lost!!! e.g. after condition was checked but before the wait begins
        //=> add artifical time out to mitigate! CPU: 0.25% vs 0% for longer time out!
        while (!cv.wait_for(lock, std::chrono::milliseconds(1), [&] { return this->stopRequested_ || pred(); }))
            ;

        throwIfStopped(); //throw ThreadStopRequest
    }

    //context of worker thread:
    template <class Rep, class Period>
    void interruptibleSleep(const std::chrono::duration<Rep, Period>& relTime) //throw ThreadStopRequest
    {
        std::unique_lock lock(lockSleep_);
        if (conditionSleepInterruption_.wait_for(lock, relTime, [this] { return static_cast<bool>(this->stopRequested_); }))
            throw ThreadStopRequest();
    }

private:
    void setConditionVar(std::condition_variable* cv)
    {
        std::lock_guard dummy(lockConditionPtr_);
        activeCondition_ = cv;
    }

    std::atomic<bool> stopRequested_{false}; //std::atomic is uninitialized by default!!!
    //"The default constructor is trivial: no initialization takes place other than zero initialization of static and thread-local objects."

    std::condition_variable* activeCondition_ = nullptr;
    std::mutex lockConditionPtr_; //serialize pointer access (only!)

    std::condition_variable conditionSleepInterruption_;
    std::mutex lockSleep_;
};


namespace impl
{
//thread_local with non-POD seems to cause memory leaks on VS 14 => pointer only is fine:
inline thread_local InterruptionStatus* threadLocalInterruptionStatus = nullptr;
}


//context of worker thread:
inline
void interruptionPoint() //throw ThreadStopRequest
{
    assert(impl::threadLocalInterruptionStatus);
    if (impl::threadLocalInterruptionStatus)
        impl::threadLocalInterruptionStatus->throwIfStopped(); //throw ThreadStopRequest
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


template <class Function> inline
InterruptibleThread::InterruptibleThread(Function&& f)
{
    stdThread_ = std::thread([f = std::forward<Function>(f),
                                intStatus = this->intStatus_]() mutable
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


inline
void InterruptibleThread::requestStop() { intStatus_->requestStop(); }
}

#endif //THREAD_H_7896323423432235246427

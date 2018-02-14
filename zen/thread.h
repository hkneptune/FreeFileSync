// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef THREAD_H_7896323423432235246427
#define THREAD_H_7896323423432235246427

#include <thread>
#include <future>
#include "scope_guard.h"
#include "type_traits.h"
#include "optional.h"
    #include <sys/prctl.h>


namespace zen
{
class InterruptionStatus;

class InterruptibleThread
{
public:
    InterruptibleThread() {}
    InterruptibleThread           (InterruptibleThread&& tmp) = default;
    InterruptibleThread& operator=(InterruptibleThread&& tmp) = default;

    template <class Function>
    InterruptibleThread(Function&& f);

    bool joinable () const { return stdThread_.joinable(); }
    void interrupt();
    void join     () { stdThread_.join(); }
    void detach   () { stdThread_.detach(); }

    template <class Rep, class Period>
    bool tryJoinFor(const std::chrono::duration<Rep, Period>& relTime)
    {
        if (threadCompleted_.wait_for(relTime) == std::future_status::ready)
        {
            stdThread_.join(); //runs thread-local destructors => this better be fast!!!
            return true;
        }
        return false;
    }

private:
    std::thread stdThread_;
    std::shared_ptr<InterruptionStatus> intStatus_{ std::make_shared<InterruptionStatus>() };
    std::future<void> threadCompleted_;
};

//context of worker thread:
void interruptionPoint(); //throw ThreadInterruption

template<class Predicate>
void interruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lock, Predicate pred); //throw ThreadInterruption

template <class Rep, class Period>
void interruptibleSleep(const std::chrono::duration<Rep, Period>& relTime); //throw ThreadInterruption

void setCurrentThreadName(const char* threadName);

uint64_t getThreadId(); //simple integer thread id, unlike boost::thread::id: https://svn.boost.org/trac/boost/ticket/5754

//------------------------------------------------------------------------------------------

/*
std::async replacement without crappy semantics:
    1. guaranteed to run asynchronously
    2. does not follow C++11 [futures.async], Paragraph 5, where std::future waits for thread in destructor

Example:
        Zstring dirpath = ...
        auto ft = zen::runAsync([=]{ return zen::dirExists(dirpath); });
        if (ft.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready && ft.get())
            //dir exising
*/
template <class Function>
auto runAsync(Function&& fun);

//wait for all with a time limit: return true if *all* results are available!
template<class InputIterator, class Duration>
bool wait_for_all_timed(InputIterator first, InputIterator last, const Duration& wait_duration);

template<typename T> inline
bool isReady(const std::future<T>& f) { return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
//------------------------------------------------------------------------------------------

//wait until first job is successful or all failed: substitute until std::when_any is available
template <class T>
class GetFirstResult
{
public:
    GetFirstResult();

    template <class Fun>
    void addJob(Fun&& f); //f must return a zen::Opt<T> containing a value if successful

    template <class Duration>
    bool timedWait(const Duration& duration) const; //true: "get()" is ready, false: time elapsed

    //return first value or none if all jobs failed; blocks until result is ready!
    Opt<T> get() const; //may be called only once!

private:
    class AsyncResult;
    std::shared_ptr<AsyncResult> asyncResult_;
    size_t jobsTotal_ = 0;
};

//------------------------------------------------------------------------------------------

//value associated with mutex and guaranteed protected access:
template <class T>
class Protected
{
public:
    Protected() {}
    Protected(const T& value) : value_(value) {}

    template <class Function>
    void access(Function fun)
    {
        std::lock_guard<std::mutex> dummy(lockValue_);
        fun(value_);
    }

private:
    Protected           (const Protected&) = delete;
    Protected& operator=(const Protected&) = delete;

    std::mutex lockValue_;
    T value_{};
};









//###################### implementation ######################

namespace impl
{
template <class Function> inline
auto runAsync(Function&& fun, TrueType /*copy-constructible*/)
{
    using ResultType = decltype(fun());

    //note: std::packaged_task does NOT support move-only function objects!
    std::packaged_task<ResultType()> pt(std::forward<Function>(fun));
    auto fut = pt.get_future();
    std::thread(std::move(pt)).detach(); //we have to explicitly detach since C++11: [thread.thread.destr] ~thread() calls std::terminate() if joinable()!!!
    return fut;
}


template <class Function> inline
auto runAsync(Function&& fun, FalseType /*copy-constructible*/)
{
    //support move-only function objects!
    auto sharedFun = std::make_shared<Function>(std::forward<Function>(fun));
    return runAsync([sharedFun] { return (*sharedFun)(); }, TrueType());
}
}


template <class Function> inline
auto runAsync(Function&& fun)
{
    return impl::runAsync(std::forward<Function>(fun), StaticBool<std::is_copy_constructible<Function>::value>());
}


template<class InputIterator, class Duration> inline
bool wait_for_all_timed(InputIterator first, InputIterator last, const Duration& duration)
{
    const std::chrono::steady_clock::time_point stopTime = std::chrono::steady_clock::now() + duration;
    for (; first != last; ++first)
        if (first->wait_until(stopTime) != std::future_status::ready)
            return false; //time elapsed
    return true;
}


template <class T>
class GetFirstResult<T>::AsyncResult
{
public:
    //context: worker threads
    void reportFinished(Opt<T>&& result)
    {
        {
            std::lock_guard<std::mutex> dummy(lockResult_);
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
        std::unique_lock<std::mutex> dummy(lockResult_);
        return conditionJobDone_.wait_for(dummy, duration, [&] { return this->jobDone(jobsTotal); });
    }

    Opt<T> getResult(size_t jobsTotal)
    {
        std::unique_lock<std::mutex> dummy(lockResult_);
        conditionJobDone_.wait(dummy, [&] { return this->jobDone(jobsTotal); });

        return std::move(result_);
    }

private:
    bool jobDone(size_t jobsTotal) const { return result_ || (jobsFinished_ >= jobsTotal); } //call while locked!


    std::mutex lockResult_;
    size_t jobsFinished_ = 0; //
    Opt<T> result_;          //our condition is: "have result" or "jobsFinished_ == jobsTotal"
    std::condition_variable conditionJobDone_;
};



template <class T> inline
GetFirstResult<T>::GetFirstResult() : asyncResult_(std::make_shared<AsyncResult>()) {}


template <class T>
template <class Fun> inline
void GetFirstResult<T>::addJob(Fun&& f) //f must return a zen::Opt<T> containing a value on success
{
    std::thread t([asyncResult = this->asyncResult_, f = std::forward<Fun>(f)] { asyncResult->reportFinished(f()); });
    ++jobsTotal_;
    t.detach(); //we have to be explicit since C++11: [thread.thread.destr] ~thread() calls std::terminate() if joinable()!!!
}


template <class T>
template <class Duration> inline
bool GetFirstResult<T>::timedWait(const Duration& duration) const { return asyncResult_->waitForResult(jobsTotal_, duration); }


template <class T> inline
Opt<T> GetFirstResult<T>::get() const { return asyncResult_->getResult(jobsTotal_); }

//------------------------------------------------------------------------------------------

//thread_local with non-POD seems to cause memory leaks on VS 14 => pointer only is fine:
    #define ZEN_THREAD_LOCAL_SPECIFIER __thread


class ThreadInterruption {};


class InterruptionStatus
{
public:
    //context of InterruptibleThread instance:
    void interrupt()
    {
        interrupted_ = true;

        {
            std::lock_guard<std::mutex> dummy(lockSleep_); //needed! makes sure the following signal is not lost!
            //usually we'd make "interrupted" non-atomic, but this is already given due to interruptibleWait() handling
        }
        conditionSleepInterruption_.notify_all();

        std::lock_guard<std::mutex> dummy(lockConditionPtr_);
        if (activeCondition_)
            activeCondition_->notify_all(); //signal may get lost!
        //alternative design locking the cv's mutex here could be dangerous: potential for dead lock!
    }

    //context of worker thread:
    void checkInterruption() //throw ThreadInterruption
    {
        if (interrupted_)
            throw ThreadInterruption();
    }

    //context of worker thread:
    template<class Predicate>
    void interruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lock, Predicate pred) //throw ThreadInterruption
    {
        setConditionVar(&cv);
        ZEN_ON_SCOPE_EXIT(setConditionVar(nullptr));

        //"interrupted" is not protected by cv's mutex => signal may get lost!!! => add artifical time out to mitigate! CPU: 0.25% vs 0% for longer time out!
        while (!cv.wait_for(lock, std::chrono::milliseconds(1), [&] { return this->interrupted_ || pred(); }))
            ;

        checkInterruption(); //throw ThreadInterruption
    }

    //context of worker thread:
    template <class Rep, class Period>
    void interruptibleSleep(const std::chrono::duration<Rep, Period>& relTime) //throw ThreadInterruption
    {
        std::unique_lock<std::mutex> lock(lockSleep_);
        if (conditionSleepInterruption_.wait_for(lock, relTime, [this] { return static_cast<bool>(this->interrupted_); }))
            throw ThreadInterruption();
    }

private:
    void setConditionVar(std::condition_variable* cv)
    {
        std::lock_guard<std::mutex> dummy(lockConditionPtr_);
        activeCondition_ = cv;
    }

    std::atomic<bool> interrupted_{ false }; //std:atomic is uninitialized by default!!!
    //"The default constructor is trivial: no initialization takes place other than zero initialization of static and thread-local objects."

    std::condition_variable* activeCondition_ = nullptr;
    std::mutex lockConditionPtr_; //serialize pointer access (only!)

    std::condition_variable conditionSleepInterruption_;
    std::mutex lockSleep_;
};


namespace impl
{
inline
InterruptionStatus*& refThreadLocalInterruptionStatus()
{
    static ZEN_THREAD_LOCAL_SPECIFIER InterruptionStatus* threadLocalInterruptionStatus = nullptr;
    return threadLocalInterruptionStatus;
}
}

//context of worker thread:
inline
void interruptionPoint() //throw ThreadInterruption
{
    assert(impl::refThreadLocalInterruptionStatus());
    if (impl::refThreadLocalInterruptionStatus())
        impl::refThreadLocalInterruptionStatus()->checkInterruption(); //throw ThreadInterruption
}


//context of worker thread:
template<class Predicate> inline
void interruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lock, Predicate pred) //throw ThreadInterruption
{
    assert(impl::refThreadLocalInterruptionStatus());
    if (impl::refThreadLocalInterruptionStatus())
        impl::refThreadLocalInterruptionStatus()->interruptibleWait(cv, lock, pred);
    else
        cv.wait(lock, pred);
}

//context of worker thread:
template <class Rep, class Period> inline
void interruptibleSleep(const std::chrono::duration<Rep, Period>& relTime) //throw ThreadInterruption
{
    assert(impl::refThreadLocalInterruptionStatus());
    if (impl::refThreadLocalInterruptionStatus())
        impl::refThreadLocalInterruptionStatus()->interruptibleSleep(relTime);
    else
        std::this_thread::sleep_for(relTime);
}


template <class Function> inline
InterruptibleThread::InterruptibleThread(Function&& f)
{
    std::promise<void> pFinished;
    threadCompleted_ = pFinished.get_future();

    stdThread_ = std::thread([f = std::forward<Function>(f),
                                intStatus = this->intStatus_,
                                pFinished = std::move(pFinished)]() mutable
    {
        assert(!impl::refThreadLocalInterruptionStatus());
        impl::refThreadLocalInterruptionStatus() = intStatus.get();
        ZEN_ON_SCOPE_EXIT(impl::refThreadLocalInterruptionStatus() = nullptr);
        ZEN_ON_SCOPE_EXIT(pFinished.set_value());

        try
        {
            f(); //throw ThreadInterruption
        }
        catch (ThreadInterruption&) {}
    });
}


inline
void InterruptibleThread::interrupt() { intStatus_->interrupt(); }




inline
void setCurrentThreadName(const char* threadName)
{
    ::prctl(PR_SET_NAME, threadName, 0, 0, 0);

}


inline
uint64_t getThreadId()
{
    //obviously "gettid()" is not available on Ubuntu/Debian/Suse => use the OpenSSL approach:
    static_assert(sizeof(uint64_t) >= sizeof(void*), "");
    return reinterpret_cast<uint64_t>(static_cast<void*>(&errno));

}
}

#endif //THREAD_H_7896323423432235246427

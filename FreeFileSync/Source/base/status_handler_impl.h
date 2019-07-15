// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STATUS_HANDLER_IMPL_H_07682758976
#define STATUS_HANDLER_IMPL_H_07682758976

#include <zen/file_error.h>
#include <zen/thread.h>
#include "process_callback.h"


namespace fff
{
class AsyncCallback //actor pattern
{
public:
    AsyncCallback() {}

    //non-blocking: context of worker thread (and main thread, see reportStats())
    void updateDataProcessed(int itemsDelta, int64_t bytesDelta) //noexcept!!
    {
        itemsDeltaProcessed_ += itemsDelta;
        bytesDeltaProcessed_ += bytesDelta;
    }
    void updateDataTotal(int itemsDelta, int64_t bytesDelta) //noexcept!!
    {
        itemsDeltaTotal_ += itemsDelta;
        bytesDeltaTotal_ += bytesDelta;
    }

    //context of worker thread
    void reportStatus(const std::wstring& msg) //throw ThreadInterruption
    {
        assert(!zen::runningMainThread());
        {
            std::lock_guard dummy(lockCurrentStatus_);
            if (ThreadStatus* ts = getThreadStatus()) //call while holding "lockCurrentStatus_" lock!!
                ts->statusMsg = msg;
            else assert(false);
        }
        zen::interruptionPoint(); //throw ThreadInterruption
    }

    //blocking call: context of worker thread
    //=> indirect support for "pause": reportInfo() is called under singleThread lock,
    //   so all other worker threads will wait when coming out of parallel I/O (trying to lock singleThread)
    void reportInfo(const std::wstring& msg) //throw ThreadInterruption
    {
        reportStatus(msg); //throw ThreadInterruption
        logInfo     (msg); //
    }

    //blocking call: context of worker thread
    void logInfo(const std::wstring& msg) //throw ThreadInterruption
    {
        assert(!zen::runningMainThread());
        std::unique_lock dummy(lockRequest_);
        zen::interruptibleWait(conditionReadyForNewRequest_, dummy, [this] { return !logInfoRequest_; }); //throw ThreadInterruption

        logInfoRequest_ = /*std::move(taskPrefix) + */ msg;

        dummy.unlock(); //optimization for condition_variable::notify_all()
        conditionNewRequest.notify_all();
    }

    //blocking call: context of worker thread
    ProcessCallback::Response reportError(const std::wstring& msg, size_t retryNumber) //throw ThreadInterruption
    {
        assert(!zen::runningMainThread());
        std::unique_lock dummy(lockRequest_);
        zen::interruptibleWait(conditionReadyForNewRequest_, dummy, [this] { return !errorRequest_ && !errorResponse_; }); //throw ThreadInterruption

        errorRequest_ = ErrorInfo({ /*std::move(taskPrefix) + */ msg, retryNumber });
        conditionNewRequest.notify_all();

        zen::interruptibleWait(conditionHaveResponse_, dummy, [this] { return static_cast<bool>(errorResponse_); }); //throw ThreadInterruption

        ProcessCallback::Response rv = *errorResponse_;

        errorRequest_  = {};
        errorResponse_ = {};

        dummy.unlock(); //optimization for condition_variable::notify_all()
        conditionReadyForNewRequest_.notify_all(); //=> spurious wake-up for AsyncCallback::logInfo()
        return rv;
    }

    //context of main thread
    void waitUntilDone(std::chrono::milliseconds duration, ProcessCallback& cb) //throw X
    {
        assert(zen::runningMainThread());
        for (;;)
        {
            const std::chrono::steady_clock::time_point callbackTime = std::chrono::steady_clock::now() + duration;

            for (std::unique_lock dummy(lockRequest_) ;;) //process all errors without delay
            {
                const bool rv = conditionNewRequest.wait_until(dummy, callbackTime, [this] { return (errorRequest_ && !errorResponse_) || logInfoRequest_ || finishNowRequest_; });
                if (!rv) //time-out + condition not met
                    break;

                if (errorRequest_ && !errorResponse_)
                {
                    assert(!finishNowRequest_);
                    errorResponse_ = cb.reportError(errorRequest_->msg, errorRequest_->retryNumber); //throw X
                    conditionHaveResponse_.notify_all(); //instead of notify_one(); workaround bug: https://svn.boost.org/trac/boost/ticket/7796
                }
                if (logInfoRequest_)
                {
                    cb.logInfo(*logInfoRequest_);
                    logInfoRequest_ = {};
                    conditionReadyForNewRequest_.notify_all(); //=> spurious wake-up for AsyncCallback::reportError()
                }
                if (finishNowRequest_)
                {
                    dummy.unlock(); //call member functions outside of mutex scope:
                    reportStats(cb); //one last call for accurate stat-reporting!
                    return;
                }
            }

            //call member functions outside of mutex scope:
            cb.reportStatus(getCurrentStatus()); //throw X
            reportStats(cb);
        }
    }

    void notifyTaskBegin(size_t prio) //noexcept
    {
        assert(!zen::runningMainThread());
        const uint64_t threadId = zen::getThreadId();
        std::lock_guard dummy(lockCurrentStatus_);
        assert(!getThreadStatus());

        //const size_t taskIdx = [&]() -> size_t
        //{
        //    auto it = std::find(usedIndexNums_.begin(), usedIndexNums_.end(), false);
        //    if (it != usedIndexNums_.end())
        //    {
        //        *it = true;
        //        return it - usedIndexNums_.begin();
        //    }

        //    usedIndexNums_.push_back(true);
        //    return usedIndexNums_.size() - 1;
        //}();

        if (statusByPriority_.size() < prio + 1)
            statusByPriority_.resize(prio + 1);

        statusByPriority_[prio].push_back({ threadId, /*taskIdx,*/ std::wstring() });
    }

    void notifyTaskEnd() //noexcept
    {
        assert(!zen::runningMainThread());
        const uint64_t threadId = zen::getThreadId();
        std::lock_guard dummy(lockCurrentStatus_);

        for (std::vector<ThreadStatus>& sbp : statusByPriority_)
            for (ThreadStatus& ts : sbp)
                if (ts.threadId == threadId)
                {
                    //usedIndexNums_[ts.taskIdx] = false;
                    std::swap(ts, sbp.back());
                    sbp.pop_back();
                    return;
                }
        assert(false);
    }

    void notifyAllDone() //noexcept
    {
        std::lock_guard dummy(lockRequest_);
        assert(!finishNowRequest_);
        finishNowRequest_ = true;
        conditionNewRequest.notify_all(); //perf: should unlock mutex before notify!? (insignificant)
    }

private:
    AsyncCallback           (const AsyncCallback&) = delete;
    AsyncCallback& operator=(const AsyncCallback&) = delete;

    struct ThreadStatus
    {
        uint64_t threadId = 0;
        //size_t   taskIdx = 0; //nice human-readable task id for GUI
        std::wstring statusMsg;
    };

    ThreadStatus* getThreadStatus() //call while holding "lockCurrentStatus_" lock!!
    {
        assert(!zen::runningMainThread());
        const uint64_t threadId = zen::getThreadId();

        for (std::vector<ThreadStatus>& sbp : statusByPriority_)
            for (ThreadStatus& ts : sbp) //thread count is (hopefully) small enough so that linear search won't hurt perf
                if (ts.threadId == threadId)
                    return &ts;
        return nullptr;
    }

#if 0 //maybe not that relevant after all!?
    std::wstring getTaskPrefix() //call *outside* of "lockCurrentStatus_" lock!!
    {
        const size_t taskIdx = [&]
        {
            std::lock_guard dummy(lockCurrentStatus_);
            const ThreadStatus* ts = getThreadStatus(); //call while holding "lockCurrentStatus_" lock!!
            return ts ? ts->taskIdx : static_cast<size_t>(-2);
        }();
        return totalThreadCount_ > 1 ? L"[" + zen::numberTo<std::wstring>(taskIdx + 1) + L"] " : L"";
    }
#endif

    //context of main thread
    void reportStats(ProcessCallback& cb)
    {
        assert(zen::runningMainThread());

        const std::pair<int, int64_t> deltaProcessed(itemsDeltaProcessed_, bytesDeltaProcessed_);
        if (deltaProcessed.first != 0 || deltaProcessed.second != 0)
        {
            updateDataProcessed   (-deltaProcessed.first, -deltaProcessed.second); //careful with these atomics: don't just set to 0
            cb.updateDataProcessed( deltaProcessed.first,  deltaProcessed.second); //noexcept!!
        }
        const std::pair<int, int64_t> deltaTotal(itemsDeltaTotal_, bytesDeltaTotal_);
        if (deltaTotal.first != 0 || deltaTotal.second != 0)
        {
            updateDataTotal   (-deltaTotal.first, -deltaTotal.second);
            cb.updateDataTotal( deltaTotal.first,  deltaTotal.second); //noexcept!!
        }
    }

    //context of main thread, call repreatedly
    std::wstring getCurrentStatus()
    {
        assert(zen::runningMainThread());

        int parallelOpsTotal = 0;
        std::wstring statusMsg;
        {
            std::lock_guard dummy(lockCurrentStatus_);

            for (const auto& sbp : statusByPriority_)
                parallelOpsTotal += sbp.empty() ? 0 : 1;
            statusMsg = [&]
            {
                for (const std::vector<ThreadStatus>& sbp : statusByPriority_)
                    for (const ThreadStatus& ts : sbp)
                        if (!ts.statusMsg.empty())
                            return ts.statusMsg;
                return std::wstring();
            }();
        }
        if (parallelOpsTotal >= 2)
            return L"[" + _P("1 thread", "%x threads", parallelOpsTotal) + L"] " + statusMsg;
        else
            return statusMsg;
    }

    struct ErrorInfo
    {
        std::wstring msg;
        size_t retryNumber = 0;
    };

    //---- main <-> worker communication channel ----
    std::mutex lockRequest_;
    std::condition_variable conditionReadyForNewRequest_;
    std::condition_variable conditionNewRequest;
    std::condition_variable conditionHaveResponse_;
    std::optional<ErrorInfo>                 errorRequest_;
    std::optional<ProcessCallback::Response> errorResponse_;
    std::optional<std::wstring>              logInfoRequest_;
    bool finishNowRequest_ = false;

    //---- status updates ----
    std::mutex lockCurrentStatus_; //different lock for status updates so that we're not blocked by other threads reporting errors
    std::vector<std::vector<ThreadStatus>> statusByPriority_;
    //give status messages priority according to their folder pair (e.g. first folder pair has prio 0) => visualize (somewhat) natural processing order

    //std::vector<char/*bool*/> usedIndexNums_; //keep info for human-readable task index numbers

    //---- status updates II (lock-free) ----
    std::atomic<int>     itemsDeltaProcessed_{ 0 }; //
    std::atomic<int64_t> bytesDeltaProcessed_{ 0 }; //std:atomic is uninitialized by default!
    std::atomic<int>     itemsDeltaTotal_    { 0 }; //
    std::atomic<int64_t> bytesDeltaTotal_    { 0 }; //
};


//manage statistics reporting for a single item of work
template <class Callback = ProcessCallback>
class ItemStatReporter
{
public:
    ItemStatReporter(int itemsExpected, int64_t bytesExpected, Callback& cb) :
        itemsExpected_(itemsExpected),
        bytesExpected_(bytesExpected),
        cb_(cb) {}

    ~ItemStatReporter()
    {
        const bool scopeFail = std::uncaught_exceptions() > exeptionCount_;
        if (scopeFail)
            cb_.updateDataTotal(itemsReported_, bytesReported_); //=> unexpected increase of total workload
        else
            //update statistics to consider the real amount of data, e.g. more than the "file size" for ADS streams,
            //less for sparse and compressed files,  or file changed in the meantime!
            cb_.updateDataTotal(itemsReported_ - itemsExpected_, bytesReported_ - bytesExpected_); //noexcept!
    }

    void reportStatus(const std::wstring& msg) { cb_.reportStatus(msg); } //throw ThreadInterruption

    void reportDelta(int itemsDelta, int64_t bytesDelta) //nothrow!
    {
        cb_.updateDataProcessed(itemsDelta, bytesDelta); //nothrow!
        itemsReported_ += itemsDelta;
        bytesReported_ += bytesDelta;

        //special rule: avoid temporary statistics mess up, even though they are corrected anyway below:
        if (itemsReported_ > itemsExpected_)
        {
            cb_.updateDataTotal(itemsReported_ - itemsExpected_, 0);
            itemsReported_ = itemsExpected_;
        }
        if (bytesReported_ > bytesExpected_)
        {
            cb_.updateDataTotal(0, bytesReported_ - bytesExpected_); //=> everything above "bytesExpected" adds to both "processed" and "total" data
            bytesReported_ = bytesExpected_;
        }
    }

private:
    int itemsReported_ = 0;
    int64_t bytesReported_ = 0;
    const int itemsExpected_;
    const int64_t bytesExpected_;
    Callback& cb_;
    const int exeptionCount_ = std::uncaught_exceptions();
};

using AsyncItemStatReporter = ItemStatReporter<AsyncCallback>;

//=====================================================================================================================

template <class Function, class Callback> inline //return ignored error message if available
std::wstring tryReportingError(Function cmd /*throw FileError*/, Callback& cb /*throw X*/)
{
    for (size_t retryNumber = 0;; ++retryNumber)
        try
        {
            cmd(); //throw FileError
            return std::wstring();
        }
        catch (zen::FileError& e)
        {
            assert(!e.toString().empty());
            switch (cb.reportError(e.toString(), retryNumber)) //throw X
            {
                case ProcessCallback::IGNORE_ERROR:
                    return e.toString();
                case ProcessCallback::RETRY:
                    break; //continue with loop
            }
        }
}

//=====================================================================================================================
struct ParallelContext
{
    const AbstractPath& itemPath;
    AsyncCallback& acb;
};
using ParallelWorkItem = std::function<void(ParallelContext& ctx)> /*throw ThreadInterruption*/;


namespace
{
void massParallelExecute(const std::vector<std::pair<AbstractPath, ParallelWorkItem>>& workload,
                         const std::string& threadGroupName,
                         ProcessCallback& callback /*throw X*/) //throw X
{
    using namespace zen;

    std::map<AfsDevice, std::vector<const std::pair<AbstractPath, ParallelWorkItem>*>> perDeviceWorkload;
    for (const auto& item : workload)
        perDeviceWorkload[item.first.afsDevice].push_back(&item);

    if (perDeviceWorkload.empty())
        return; //[!] otherwise AsyncCallback::notifyAllDone() is never called!

    AsyncCallback acb;                                            //manage life time: enclose ThreadGroup's!!!
    std::atomic<int> activeDeviceCount(perDeviceWorkload.size()); //

    //---------------------------------------------------------------------------------------------------------
    std::map<AfsDevice, ThreadGroup<std::function<void()>>> deviceThreadGroups; //worker threads live here...
    //---------------------------------------------------------------------------------------------------------

    for (const auto& [afsDevice, wl] : perDeviceWorkload)
    {
        const size_t statusPrio = deviceThreadGroups.size();

        auto& threadGroup = deviceThreadGroups.emplace(afsDevice, ThreadGroup<std::function<void()>>(
                                                           1,
                                                           threadGroupName + " " + utfTo<std::string>(AFS::getDisplayPath(AbstractPath(afsDevice, AfsPath()))))).first->second;

        for (const std::pair<AbstractPath, ParallelWorkItem>* item : wl)
            threadGroup.run([&acb, statusPrio, &itemPath = item->first, &task = item->second]
        {
            acb.notifyTaskBegin(statusPrio);
            ZEN_ON_SCOPE_EXIT(acb.notifyTaskEnd());

            ParallelContext pctx{ itemPath, acb };
            task(pctx); //throw ThreadInterruption
        });

        threadGroup.notifyWhenDone([&acb, &activeDeviceCount] /*noexcept! runs on worker thread!*/
        {
            if (--activeDeviceCount == 0)
                acb.notifyAllDone(); //noexcept
        });
    }

    acb.waitUntilDone(UI_UPDATE_INTERVAL / 2 /*every ~50 ms*/, callback); //throw X
}
}

//=====================================================================================================================

template <class Function> inline
auto parallelScope(Function&& fun, std::mutex& singleThread) //throw X
{
    singleThread.unlock();
    ZEN_ON_SCOPE_EXIT(singleThread.lock());

    return fun(); //throw X
}
}

#endif //STATUS_HANDLER_IMPL_H_07682758976

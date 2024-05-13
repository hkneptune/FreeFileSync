// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ASYNC_TASK_H_839147839170432143214321
#define ASYNC_TASK_H_839147839170432143214321

//#include <functional>
#include <zen/thread.h>
#include <zen/scope_guard.h>
#include <zen/stl_tools.h>
#include <wx/timer.h>


namespace zen
{
/* Run a task in an async thread, but process result in GUI event loop
   -------------------------------------------------------------------
    1. put AsyncGuiQueue instance inside a dialog:
        AsyncGuiQueue guiQueue;

    2. schedule async task and synchronous continuation:
        guiQueue.processAsync(evalAsync, evalOnGui);

    Alternative: wxWidgets' inter-thread communication (wxEvtHandler::QueueEvent) https://wiki.wxwidgets.org/Inter-Thread_and_Inter-Process_communication
            => don't bother, probably too many MT race conditions lurking around                          */

namespace impl
{
struct Task
{
    virtual ~Task() {}
    virtual bool resultReady   () const = 0;
    virtual void evaluateResult()       = 0;
};


template <class ResultType, class Fun>
class ConcreteTask : public Task
{
public:
    template <class Fun2>
    ConcreteTask(std::future<ResultType>&& asyncResult, Fun2&& evalOnGui) :
        asyncResult_(std::move(asyncResult)), evalOnGui_(std::forward<Fun2>(evalOnGui)) {}

    bool resultReady   () const override { return isReady(asyncResult_); }
    void evaluateResult()       override
    {
        evalResult(std::is_same<ResultType, void>());
    }

private:
    void evalResult(std::false_type /*void result type*/) { evalOnGui_(asyncResult_.get()); }
    void evalResult(std::true_type  /*void result type*/) { asyncResult_.get(); evalOnGui_(); }

    std::future<ResultType> asyncResult_;
    Fun evalOnGui_; //keep "evalOnGui" strictly separated from async thread: in particular do not copy in thread!
};


class AsyncTasks
{
public:
    AsyncTasks() {}

    template <class Fun, class Fun2>
    void add(Fun&& evalAsync, Fun2&& evalOnGui)
    {
        using ResultType = decltype(evalAsync());
        tasks_.push_back(std::make_unique<ConcreteTask<ResultType, std::decay_t<Fun2>>>(zen::runAsync(std::forward<Fun>(evalAsync)), std::forward<Fun2>(evalOnGui)));
    }
    //equivalent to "evalOnGui(evalAsync())"
    //  -> evalAsync: the usual thread-safety requirements apply!
    //  -> evalOnGui: no thread-safety concerns, but must only reference variables with greater-equal lifetime than the AsyncTask instance!

    void evalResults() //call from gui thread repreatedly
    {
        if (!inRecursion_) //prevent implicit recursion, e.g. if we're called from an idle event and spawn another one within the callback below
        {
            inRecursion_ = true;
            ZEN_ON_SCOPE_EXIT(inRecursion_ = false);

            std::vector<std::unique_ptr<Task>> readyTasks; //Reentrancy; access to AsyncTasks::add is not protected! => evaluate outside eraseIf

            eraseIf(tasks_, [&](std::unique_ptr<Task>& task)
            {
                if (task->resultReady())
                {
                    readyTasks.push_back(std::move(task));
                    return true;
                }
                return false;
            });

            for (std::unique_ptr<Task>& task : readyTasks)
                task->evaluateResult();
        }
    }

    bool empty() const { return tasks_.empty(); }

private:
    AsyncTasks           (const AsyncTasks&) = delete;
    AsyncTasks& operator=(const AsyncTasks&) = delete;

    bool inRecursion_ = false;
    std::vector<std::unique_ptr<Task>> tasks_;
};
}


class AsyncGuiQueue : private wxEvtHandler
{
public:
    explicit AsyncGuiQueue(int pollingMs = 50) :
        pollingMs_(pollingMs) { timer_.Bind(wxEVT_TIMER, [this](wxTimerEvent& event) { onTimerEvent(event); }); }

    template <class Fun, class Fun2>
    void processAsync(Fun&& evalAsync, Fun2&& evalOnGui)
    {
        asyncTasks_.add(std::forward<Fun >(evalAsync),
                        std::forward<Fun2>(evalOnGui));
        if (!timer_.IsRunning())
            timer_.Start(pollingMs_ /*unit: [ms]*/);
    }

private:
    void onTimerEvent(wxEvent& event) //schedule and run long-running tasks asynchronously
    {
        asyncTasks_.evalResults(); //process results on GUI queue
        if (asyncTasks_.empty())
            timer_.Stop();
    }

    const int pollingMs_;
    impl::AsyncTasks asyncTasks_;
    wxTimer timer_; //don't use wxWidgets' idle handling => repeated idle requests/consumption hogs 100% cpu!
};

}

#endif //ASYNC_TASK_H_839147839170432143214321

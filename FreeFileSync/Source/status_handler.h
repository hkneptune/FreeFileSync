// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STATUS_HANDLER_H_81704805908341534
#define STATUS_HANDLER_H_81704805908341534

#include <thread>
#include <functional>
//#include <zen/error_log.h>
#include "base/process_callback.h"
#include "return_codes.h"

namespace fff
{
bool uiUpdateDue(bool force = false); //test if a specific amount of time is over

/*  Updating GUI is fast! time per call to ProcessCallback::forceUiRefresh()
    - Comparison       0.025 ms
    - Synchronization  0.74 ms (despite complex graph control!)               */

//Exception class used to abort the "compare" and "sync" process
class CancelProcess {};


enum class CancelReason
{
    user,
    firstError,
};

//GUI may want to abort process
struct CancelCallback
{
    virtual ~CancelCallback() {}
    virtual void userRequestCancel() = 0;
};


struct ProgressStats
{
    int     items = 0;
    int64_t bytes = 0;

    bool operator==(const ProgressStats&) const = default;
};


//common statistics "everybody" needs
struct Statistics
{
    virtual ~Statistics() {}

    virtual ProcessPhase currentPhase() const = 0;

    virtual ProgressStats getCurrentStats() const = 0;
    virtual ProgressStats getTotalStats  () const = 0;

    struct ErrorStats
    {
        int errorCount;
        int warningCount;
    };
    virtual ErrorStats getErrorStats() const = 0;

    virtual std::optional<CancelReason> taskCancelled() const = 0;
    virtual const std::wstring& currentStatusText() const = 0;
};


struct ProcessSummary
{
    std::chrono::system_clock::time_point startTime;
    TaskResult result = TaskResult::cancelled;
    std::vector<std::wstring> jobNames; //may be empty
    ProgressStats statsProcessed;
    ProgressStats statsTotal;
    std::chrono::milliseconds totalTime{};
};


//partial callback implementation with common functionality for "batch", "GUI/Compare" and "GUI/Sync"
class StatusHandler : public ProcessCallback, public CancelCallback, public Statistics
{
public:
    //StatusHandler() {}

    //implement parts of ProcessCallback
    void initNewPhase(int itemsTotal, int64_t bytesTotal, ProcessPhase phase) override //(throw X)
    {
        assert((itemsTotal < 0) == (bytesTotal < 0));
        currentPhase_ = phase;
        statsCurrent_ = {};
        statsTotal_ = {itemsTotal, bytesTotal};
    }

    void updateDataProcessed(int itemsDelta, int64_t bytesDelta) override { updateData(statsCurrent_, itemsDelta, bytesDelta); } //note: these methods MUST NOT throw in order
    void updateDataTotal    (int itemsDelta, int64_t bytesDelta) override { updateData(statsTotal_,   itemsDelta, bytesDelta); } //to allow usage within destructors!

    void requestUiUpdate(bool force) final //throw CancelProcess
    {
        if (uiUpdateDue(force))
        {
            const bool abortRequestedBefore = static_cast<bool>(cancelRequested_);

            forceUiUpdateNoThrow();

            //triggered by userRequestCancel()
            // => sufficient to evaluate occasionally when uiUpdateDue()!
            // => refresh *before* throwing: support requestUiUpdate() during destruction
            if (cancelRequested_)
            {
                if (!abortRequestedBefore)
                    forceUiUpdateNoThrow(); //immediately show the "Stop requested..." status after user clicked cancel
                throw CancelProcess();
            }
        }
    }

    virtual void forceUiUpdateNoThrow() = 0; //noexcept

    void updateStatus(std::wstring&& msg) final //throw CancelProcess
    {
        //assert(!msg.empty()); -> possible, e.g. start of parallel scan
        statusText_ = std::move(msg); //update *before* running operations that can throw
        requestUiUpdate(false /*force*/); //throw CancelProcess
    }

    [[noreturn]] void cancelProcessNow(CancelReason reason)
    {
        if (!cancelRequested_ || reason == CancelReason::user) //CancelReason::user overwrites CancelReason::firstError
            cancelRequested_ = reason;

        forceUiUpdateNoThrow(); //flush GUI to show new cancelled state
        throw CancelProcess();
    }

    //implement CancelCallback
    void userRequestCancel() final
    {
        cancelRequested_ = CancelReason::user; //may overwrite CancelReason::firstError
    } //called from GUI code: this does NOT call cancelProcessNow() immediately, but later when we're out of the C GUI call stack
    //=> don't call forceUiUpdateNoThrow() here!

    //implement Statistics
    ProcessPhase currentPhase() const final { return currentPhase_; }

    ProgressStats getCurrentStats() const override { return statsCurrent_; }
    ProgressStats getTotalStats  () const override { return statsTotal_; }

    const std::wstring& currentStatusText() const override { return statusText_; }

    std::optional<CancelReason> taskCancelled() const override { return cancelRequested_; }

private:
    void updateData(ProgressStats& stats, int itemsDelta, int64_t bytesDelta)
    {
        assert(stats.items >= 0);
        assert(stats.bytes >= 0);
        stats.items += itemsDelta;
        stats.bytes += bytesDelta;
    }

    ProcessPhase currentPhase_ = ProcessPhase::none;
    ProgressStats statsCurrent_;
    ProgressStats statsTotal_ {-1, -1};
    std::wstring statusText_;

    std::optional<CancelReason> cancelRequested_;
};


void delayAndCountDown(std::chrono::steady_clock::time_point delayUntil, const std::function<void(const std::wstring& timeRemMsg)>& notifyStatus);
}

#endif //STATUS_HANDLER_H_81704805908341534

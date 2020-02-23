// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STATUS_HANDLER_H_81704805908341534
#define STATUS_HANDLER_H_81704805908341534

#include <vector>
#include <chrono>
#include <thread>
#include <string>
#include <zen/i18n.h>
#include <zen/basic_math.h>
#include "process_callback.h"
#include "return_codes.h"


namespace fff
{
bool uiUpdateDue(bool force = false); //test if a specific amount of time is over

/*
Updating GUI is fast!
    time per single call to ProcessCallback::forceUiRefresh()
    - Comparison       0.025 ms
    - Synchronization  0.74 ms (despite complex graph control!)
*/

//Exception class used to abort the "compare" and "sync" process
class AbortProcess {};


enum class AbortTrigger
{
    user,
    program,
};

//GUI may want to abort process
struct AbortCallback
{
    virtual ~AbortCallback() {}
    virtual void userRequestAbort() = 0;
};


struct ProgressStats
{
    int     items = 0;
    int64_t bytes = 0;
};
inline bool operator==(const ProgressStats& lhs, const ProgressStats& rhs) { return lhs.items == rhs.items && lhs.bytes == rhs.bytes; }


//common statistics "everybody" needs
struct Statistics
{
    virtual ~Statistics() {}

    virtual ProcessPhase currentPhase() const = 0;

    virtual ProgressStats getStatsCurrent() const = 0;
    virtual ProgressStats getStatsTotal  () const = 0;

    virtual std::optional<AbortTrigger> getAbortStatus() const = 0;
    virtual const std::wstring& currentStatusText() const = 0;
};


struct ProcessSummary
{
    std::chrono::system_clock::time_point startTime;
    SyncResult resultStatus = SyncResult::aborted;
    std::vector<std::wstring> jobNames; //may be empty
    ProgressStats statsProcessed;
    ProgressStats statsTotal;
    std::chrono::milliseconds totalTime{};
};


//partial callback implementation with common functionality for "batch", "GUI/Compare" and "GUI/Sync"
class StatusHandler : public ProcessCallback, public AbortCallback, public Statistics
{
public:
    //StatusHandler() {}

    //implement parts of ProcessCallback
    void initNewPhase(int itemsTotal, int64_t bytesTotal, ProcessPhase phase) override //(throw X)
    {
        assert((itemsTotal < 0) == (bytesTotal < 0));
        currentPhase_ = phase;
        statsCurrent_ = {};
        statsTotal_ = { itemsTotal, bytesTotal };
    }

    void updateDataProcessed(int itemsDelta, int64_t bytesDelta) override { updateData(statsCurrent_, itemsDelta, bytesDelta); } //note: these methods MUST NOT throw in order
    void updateDataTotal    (int itemsDelta, int64_t bytesDelta) override { updateData(statsTotal_,   itemsDelta, bytesDelta); } //to allow usage within destructors!

    void requestUiUpdate(bool force) final //throw AbortProcess
    {
        if (uiUpdateDue(force))
        {
            const bool abortRequestedBefore = static_cast<bool>(abortRequested_);

            forceUiUpdateNoThrow();

            //triggered by userRequestAbort()
            // => sufficient to evaluate occasionally when uiUpdateDue()!
            // => refresh *before* throwing: support requestUiUpdate() during destruction
            if (abortRequested_)
            {
                if (!abortRequestedBefore)
                    forceUiUpdateNoThrow(); //just once to immediately show the "Stop requested..." status after user clicks cancel
                throw AbortProcess();
            }
        }
    }

    virtual void forceUiUpdateNoThrow() = 0; //noexcept

    void updateStatus(const std::wstring& msg) final //throw AbortProcess
    {
        //assert(!msg.empty()); -> possible, e.g. start of parallel scan
        statusText_ = msg; //update *before* running operations that can throw
        requestUiUpdate(false /*force*/); //throw AbortProcess
    }

    [[noreturn]] void abortProcessNow(AbortTrigger trigger)
    {
        if (!abortRequested_ || trigger == AbortTrigger::user) //AbortTrigger::USER overwrites AbortTrigger::program
            abortRequested_ = trigger;

        forceUiUpdateNoThrow(); //flush GUI to show new cancelled state
        throw AbortProcess();
    }

    //implement AbortCallback
    void userRequestAbort() final
    {
        abortRequested_ = AbortTrigger::user; //may overwrite AbortTrigger::program
    } //called from GUI code: this does NOT call abortProcessNow() immediately, but later when we're out of the C GUI call stack
    //=> don't call forceUiUpdateNoThrow() here!

    //implement Statistics
    ProcessPhase currentPhase() const final { return currentPhase_; }

    ProgressStats getStatsCurrent() const override { return statsCurrent_; }
    ProgressStats getStatsTotal  () const override { return statsTotal_; }

    const std::wstring& currentStatusText() const override { return statusText_; }

    std::optional<AbortTrigger> getAbortStatus() const override { return abortRequested_; }

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
    ProgressStats statsTotal_ { -1, -1 };
    std::wstring statusText_;

    std::optional<AbortTrigger> abortRequested_;
};

//------------------------------------------------------------------------------------------

inline
void delayAndCountDown(const std::wstring& operationName, std::chrono::seconds delay, const std::function<void(const std::wstring& msg)>& notifyStatus)
{
    assert(notifyStatus && !zen::endsWith(operationName, L"."));

    const auto delayUntil = std::chrono::steady_clock::now() + delay;
    for (auto now = std::chrono::steady_clock::now(); now < delayUntil; now = std::chrono::steady_clock::now())
    {
        const auto timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(delayUntil - now).count();
        if (notifyStatus)
            notifyStatus(operationName + L"... " + _P("1 sec", "%x sec", numeric::integerDivideRoundUp(timeMs, 1000)));

        std::this_thread::sleep_for(UI_UPDATE_INTERVAL / 2);
    }
}
}

#endif //STATUS_HANDLER_H_81704805908341534

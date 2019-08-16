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
bool updateUiIsAllowed(); //test if a specific amount of time is over

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
    USER,
    PROGRAM,
};

//gui may want to abort process
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

    virtual ProcessCallback::Phase currentPhase() const = 0;

    virtual ProgressStats getStatsCurrent(ProcessCallback::Phase phase) const = 0;
    virtual ProgressStats getStatsTotal  (ProcessCallback::Phase phase) const = 0;

    virtual std::optional<AbortTrigger> getAbortStatus() const = 0;
    virtual const std::wstring& currentStatusText() const = 0;
};


struct ProcessSummary
{
    std::chrono::system_clock::time_point startTime;
    SyncResult finalStatus = SyncResult::aborted;
    std::wstring jobName; //may be empty
    ProgressStats statsProcessed;
    ProgressStats statsTotal;
    std::chrono::milliseconds totalTime{};
};


//partial callback implementation with common functionality for "batch", "GUI/Compare" and "GUI/Sync"
class StatusHandler : public ProcessCallback, public AbortCallback, public Statistics
{
public:
    StatusHandler()
    {
        updateData(statsTotal_, -1, -1);
    }

    //implement parts of ProcessCallback
    void initNewPhase(int itemsTotal, int64_t bytesTotal, Phase phase) override //(throw X)
    {
        assert((itemsTotal < 0) == (bytesTotal < 0));
        currentPhase_ = phase;
        refStats(statsTotal_, currentPhase_) = { itemsTotal, bytesTotal };
    }

    void updateDataProcessed(int itemsDelta, int64_t bytesDelta) override { updateData(statsCurrent_, itemsDelta, bytesDelta); } //note: these methods MUST NOT throw in order
    void updateDataTotal    (int itemsDelta, int64_t bytesDelta) override { updateData(statsTotal_,   itemsDelta, bytesDelta); } //to allow usage within destructors!

    void requestUiRefresh() override final //throw AbortProcess
    {
        if (updateUiIsAllowed())
            forceUiRefresh(); //throw AbortProcess
    }

    void forceUiRefresh() override final //throw AbortProcess
    {
        const bool abortRequestedBefore = static_cast<bool>(abortRequested_);

        forceUiRefreshNoThrow();

        //triggered by userRequestAbort()
        // => sufficient to evaluate occasionally when updateUiIsAllowed()!
        // => refresh *before* throwing: support requestUiRefresh() during destruction
        if (abortRequested_)
        {
            if (!abortRequestedBefore)
                forceUiRefreshNoThrow(); //just once to immediately show the "Stop requested..." status after user clicks cancel
            throw AbortProcess();
        }
    }

    virtual void forceUiRefreshNoThrow() = 0; //noexcept

    void reportStatus(const std::wstring& text) override final //throw AbortProcess
    {
        //assert(!text.empty()); -> possible, e.g. start of parallel scan
        statusText_ = text; //update text *before* running operations that can throw
        requestUiRefresh(); //throw AbortProcess
    }

    [[noreturn]] void abortProcessNow() override
    {
        if (!abortRequested_) abortRequested_ = AbortTrigger::PROGRAM;
        forceUiRefreshNoThrow();
        throw AbortProcess();
    }

    [[noreturn]] void userAbortProcessNow()
    {
        abortRequested_ = AbortTrigger::USER; //may overwrite AbortTrigger::PROGRAM
        forceUiRefreshNoThrow(); //flush GUI to show new abort state
        throw AbortProcess();
    }

    //implement AbortCallback
    void userRequestAbort() override final
    {
        abortRequested_ = AbortTrigger::USER; //may overwrite AbortTrigger::PROGRAM
    } //called from GUI code: this does NOT call abortProcessNow() immediately, but later when we're out of the C GUI call stack
    //=> don't call forceUiRefreshNoThrow() here

    //implement Statistics
    Phase currentPhase() const override final { return currentPhase_; }

    ProgressStats getStatsCurrent(ProcessCallback::Phase phase) const override { return refStats(statsCurrent_, phase); }
    ProgressStats getStatsTotal  (ProcessCallback::Phase phase) const override { return refStats(statsTotal_,   phase); }

    const std::wstring& currentStatusText() const override { return statusText_; }

    std::optional<AbortTrigger> getAbortStatus() const override { return abortRequested_; }

private:
    void updateData(std::vector<ProgressStats>& num, int itemsDelta, int64_t bytesDelta)
    {
        auto& st = refStats(num, currentPhase_);
        assert(st.items >= 0);
        assert(st.bytes >= 0);
        st.items += itemsDelta;
        st.bytes += bytesDelta;
    }

    static const ProgressStats& refStats(const std::vector<ProgressStats>& num, Phase phase)
    {
        switch (phase)
        {
            case PHASE_SCANNING:
                return num[0];
            case PHASE_COMPARING_CONTENT:
                return num[1];
            case PHASE_SYNCHRONIZING:
                return num[2];
            case PHASE_NONE:
                break;
        }
        return num[3]; //dummy entry!
    }

    static ProgressStats& refStats(std::vector<ProgressStats>& num, Phase phase) { return const_cast<ProgressStats&>(refStats(static_cast<const std::vector<ProgressStats>&>(num), phase)); }

    Phase currentPhase_ = PHASE_NONE;
    std::vector<ProgressStats> statsCurrent_ = std::vector<ProgressStats>(4); //init with phase count
    std::vector<ProgressStats> statsTotal_   = std::vector<ProgressStats>(4); //
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

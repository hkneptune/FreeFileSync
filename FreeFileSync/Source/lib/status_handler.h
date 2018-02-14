// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STATUS_HANDLER_H_81704805908341534
#define STATUS_HANDLER_H_81704805908341534

#include "../process_callback.h"
#include <vector>
#include <chrono>
#include <thread>
#include <string>
#include <zen/i18n.h>
#include <zen/basic_math.h>


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


//common statistics "everybody" needs
struct Statistics
{
    virtual ~Statistics() {}

    virtual ProcessCallback::Phase currentPhase() const = 0;

    virtual int getItemsCurrent(ProcessCallback::Phase phaseId) const = 0;
    virtual int getItemsTotal  (ProcessCallback::Phase phaseId) const = 0;

    virtual int64_t getBytesCurrent(ProcessCallback::Phase phaseId) const = 0;
    virtual int64_t getBytesTotal  (ProcessCallback::Phase phaseId) const = 0;

    virtual zen::Opt<AbortTrigger> getAbortStatus() const  = 0;
    virtual const std::wstring& currentStatusText() const = 0;
};


//partial callback implementation with common functionality for "batch", "GUI/Compare" and "GUI/Sync"
class StatusHandler : public ProcessCallback, public AbortCallback, public Statistics
{
public:
    StatusHandler() : numbersCurrent_(4),   //init with phase count
        numbersTotal_  (4) {} //

    //implement parts of ProcessCallback
    void initNewPhase(int itemsTotal, int64_t bytesTotal, Phase phaseId) override //may throw
    {
        currentPhase_ = phaseId;
        refNumbers(numbersTotal_, currentPhase_) = { itemsTotal, bytesTotal };
    }

    void updateProcessedData(int itemsDelta, int64_t bytesDelta) override { updateData(numbersCurrent_, itemsDelta, bytesDelta); } //note: these methods MUST NOT throw in order
    void updateTotalData    (int itemsDelta, int64_t bytesDelta) override { updateData(numbersTotal_,   itemsDelta, bytesDelta); } //to allow usage within destructors!

    void requestUiRefresh() override final //throw X
    {
        if (updateUiIsAllowed())
            forceUiRefresh(); //throw X
    }

    void forceUiRefresh() override final //throw X
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

    void reportStatus(const std::wstring& text) override final //throw X
    {
        //assert(!text.empty()); -> possible: start of parallel scan

        statusText_ = text; //update text *before* running operations that can throw
        requestUiRefresh(); //throw X
    }

    void reportInfo(const std::wstring& text) override //throw X
    {
        assert(!text.empty());
        statusText_ = text;
        requestUiRefresh(); //throw X
        //log text in derived class
    }

    void abortProcessNow() override
    {
        if (!abortRequested_) abortRequested_ = AbortTrigger::PROGRAM;
        forceUiRefreshNoThrow();
        throw AbortProcess();
    }

    void userAbortProcessNow()
    {
        abortRequested_ = AbortTrigger::USER; //may overwrite AbortTrigger::PROGRAM
        forceUiRefreshNoThrow();
        throw AbortProcess();
    }

    //implement AbortCallback
    void userRequestAbort() override final
    {
        abortRequested_ = AbortTrigger::USER; //may overwrite AbortTrigger::PROGRAM
    } //called from GUI code: this does NOT call abortProcessNow() immediately, but later when we're out of the C GUI call stack

    //implement Statistics
    Phase currentPhase() const override final { return currentPhase_; }

    int getItemsCurrent(Phase phaseId) const override {                                    return refNumbers(numbersCurrent_, phaseId).items; }
    int getItemsTotal  (Phase phaseId) const override { assert(phaseId != PHASE_SCANNING); return refNumbers(numbersTotal_,   phaseId).items; }

    int64_t getBytesCurrent(Phase phaseId) const override { assert(phaseId != PHASE_SCANNING); return refNumbers(numbersCurrent_, phaseId).bytes; }
    int64_t getBytesTotal  (Phase phaseId) const override { assert(phaseId != PHASE_SCANNING); return refNumbers(numbersTotal_,   phaseId).bytes; }

    const std::wstring& currentStatusText() const override { return statusText_; }

    zen::Opt<AbortTrigger> getAbortStatus() const override { return abortRequested_; }

private:
    struct StatNumber
    {
        int     items = 0;
        int64_t bytes = 0;
    };
    using StatNumbers = std::vector<StatNumber>;

    void updateData(StatNumbers& num, int itemsDelta, int64_t bytesDelta)
    {
        auto& st = refNumbers(num, currentPhase_);
        st.items += itemsDelta;
        st.bytes += bytesDelta;
    }

    static const StatNumber& refNumbers(const StatNumbers& num, Phase phaseId)
    {
        switch (phaseId)
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
        assert(false);
        return num[3]; //dummy entry!
    }

    static StatNumber& refNumbers(StatNumbers& num, Phase phaseId) { return const_cast<StatNumber&>(refNumbers(static_cast<const StatNumbers&>(num), phaseId)); }

    Phase currentPhase_ = PHASE_NONE;
    StatNumbers numbersCurrent_;
    StatNumbers numbersTotal_;
    std::wstring statusText_;

    zen::Opt<AbortTrigger> abortRequested_;
};

//------------------------------------------------------------------------------------------

inline
void delayAndCountDown(const std::wstring& operationName, size_t delayInSec, const std::function<void(const std::wstring& msg)>& notifyStatus)
{
    assert(notifyStatus && !zen::endsWith(operationName, L"."));

    const auto delayUntil = std::chrono::steady_clock::now() + std::chrono::seconds(delayInSec);
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

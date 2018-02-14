// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PERF_H_83947184145342652456
#define PERF_H_83947184145342652456

#include <chrono>
#include "deprecate.h"
#include "scope_guard.h"

    #include <iostream>


//############# two macros for quick performance measurements ###############
#define PERF_START zen::PerfTimer perfTest;
#define PERF_STOP  perfTest.showResult();
//###########################################################################

namespace zen
{
class PerfTimer
{
public:
    ZEN_DEPRECATE PerfTimer() {}

    ~PerfTimer() { if (!resultShown_) showResult(); }

    void pause()
    {
        if (!paused_)
        {
            paused_ = true;
            elapsedUntilPause_ += std::chrono::steady_clock::now() - startTime_; //ignore potential ::QueryPerformanceCounter() wrap-around!
        }
    }

    void resume()
    {
        if (paused_)
        {
            paused_ = false;
            startTime_ = std::chrono::steady_clock::now();
        }
    }

    void restart()
    {
        paused_ = false;
        startTime_ = std::chrono::steady_clock::now();
        elapsedUntilPause_ = std::chrono::nanoseconds::zero();
    }

    int64_t timeMs() const
    {
        auto elapsedTotal = elapsedUntilPause_;
        if (!paused_)
            elapsedTotal += std::chrono::steady_clock::now() - startTime_;

        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTotal).count();
    }

    void showResult()
    {
        const bool wasRunning = !paused_;
        if (wasRunning) pause(); //don't include call to MessageBox()!
        ZEN_ON_SCOPE_EXIT(if (wasRunning) resume());

        std::clog << "Perf: duration: " << timeMs() << " ms\n";
        resultShown_ = true;
    }

private:
    bool resultShown_ = false;
    bool paused_      = false;
    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now(); //uses ::QueryPerformanceCounter()
    std::chrono::nanoseconds elapsedUntilPause_{}; //std::chrono::duration is uninitialized by default! WTF! When will this stupidity end???
};
}

#endif //PERF_H_83947184145342652456

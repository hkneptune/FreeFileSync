// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PERF_H_83947184145342652456
#define PERF_H_83947184145342652456

#include <chrono>
#include "scope_guard.h"
#include "string_tools.h"

    #include <iostream>


//############# two macros for quick performance measurements ###############
#define PERF_START zen::PerfTimer perfTest;
#define PERF_STOP  perfTest.showResult();
//###########################################################################

/* Example: Aggregated function call time:

    static zen::PerfTimer timer;
    timer.resume();
    ZEN_ON_SCOPE_EXIT(timer.pause());
*/

namespace zen
{
//issue with wxStopWatch? https://freefilesync.org/forum/viewtopic.php?t=1426
// => wxStopWatch implementation uses QueryPerformanceCounter: https://github.com/wxWidgets/wxWidgets/blob/17d72a48ffd4d8ff42eed070ac48ee2de50ceabd/src/common/stopwatch.cpp
// => whatever the problem was, it's almost certainly not caused by QueryPerformanceCounter():
//      MSDN: "How often does QPC roll over? Not less than 100 years from the most recent system boot"
//      https://msdn.microsoft.com/en-us/library/windows/desktop/dn553408#How_often_does_QPC_roll_over_
//
// => using the system clock is problematic: https://freefilesync.org/forum/viewtopic.php?t=5280
//
//    std::chrono::system_clock wraps ::GetSystemTimePreciseAsFileTime()
//    std::chrono::steady_clock wraps ::QueryPerformanceCounter()
class StopWatch
{
public:
    bool isPaused() const { return paused_; }

    void pause()
    {
        if (!paused_)
        {
            paused_ = true;
            elapsedUntilPause_ += std::chrono::steady_clock::now() - startTime_;
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

    std::chrono::nanoseconds elapsed() const
    {
        auto elapsedTotal = elapsedUntilPause_;
        if (!paused_)
            elapsedTotal += std::chrono::steady_clock::now() - startTime_;
        return elapsedTotal;
    }

private:
    bool paused_ = false;
    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
    std::chrono::nanoseconds elapsedUntilPause_{}; //std::chrono::duration is uninitialized by default! WTF! When will this stupidity end???
};


class PerfTimer
{
public:
    [[deprecated]] PerfTimer() {}

    ~PerfTimer() { if (!resultShown_) showResult(); }

    void showResult()
    {
        const bool wasRunning = !watch_.isPaused();
        if (wasRunning) watch_.pause(); //don't include call to MessageBox()!
        ZEN_ON_SCOPE_EXIT(if (wasRunning) watch_.resume());

        const int64_t timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(watch_.elapsed()).count();
        const std::string msg = numberTo<std::string>(timeMs) + " ms";
        std::clog << "Perf: duration: " << msg << "\n";
        resultShown_ = true;
    }

private:
    StopWatch watch_;
    bool resultShown_ = false;
};
}

#endif //PERF_H_83947184145342652456

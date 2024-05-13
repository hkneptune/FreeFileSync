// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PERF_H_83947184145342652456
#define PERF_H_83947184145342652456

#include <chrono>
//#include "scope_guard.h"
#include "string_tools.h"

    #include <iostream>


//############# two macros for quick performance measurements ###############
#define PERF_START zen::PerfTimer perfTest;
#define PERF_STOP  perfTest.showResult();
//###########################################################################

/* Example: Aggregated function call time:

    static zen::PerfTimer perfTest(true); //startPaused
    perfTest.resume();
    ZEN_ON_SCOPE_EXIT(perfTest.pause());                      */

namespace zen
{
/* issue with wxStopWatch? https://freefilesync.org/forum/viewtopic.php?t=1426
     - wxStopWatch implementation uses QueryPerformanceCounter: https://github.com/wxWidgets/wxWidgets/blob/17d72a48ffd4d8ff42eed070ac48ee2de50ceabd/src/common/stopwatch.cpp
     - whatever the problem was, it's almost certainly not caused by QueryPerformanceCounter():
          MSDN: "How often does QPC roll over? Not less than 100 years from the most recent system boot"
          https://docs.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps#general-faq-about-qpc-and-tsc

     - using the system clock is problematic: https://freefilesync.org/forum/viewtopic.php?t=5280

       std::chrono::system_clock wraps ::GetSystemTimePreciseAsFileTime()
       std::chrono::steady_clock wraps ::QueryPerformanceCounter()                       */
class StopWatch
{
public:
    explicit StopWatch(bool startPaused = false)
    {
        if (startPaused)
            startTime_ = {};
    }

    bool isPaused() const { return startTime_ == std::chrono::steady_clock::time_point{}; }

    void pause()
    {
        if (!isPaused())
            elapsedUntilPause_ += std::chrono::steady_clock::now() - std::exchange(startTime_, {});
    }

    void resume()
    {
        if (isPaused())
            startTime_ = std::chrono::steady_clock::now();
    }

    std::chrono::nanoseconds elapsed() const
    {
        auto elapsedTotal = elapsedUntilPause_;
        if (!isPaused())
            elapsedTotal += std::chrono::steady_clock::now() - startTime_;
        return elapsedTotal;
    }

private:
    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
    std::chrono::nanoseconds elapsedUntilPause_{}; //std::chrono::duration is uninitialized by default! WTF! When will this stupidity end!
};


class PerfTimer
{
public:
    [[deprecated]] explicit PerfTimer(bool startPaused = false) : watch_(startPaused) {}

    ~PerfTimer() { if (!resultShown_) showResult(); }

    void pause () { watch_.pause(); }
    void resume() { watch_.resume(); }

    void showResult()
    {
        const int64_t timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(watch_.elapsed()).count();
        const std::string msg = numberTo<std::string>(timeMs) + " ms";
        std::clog << "Perf: duration: " << msg + '\n';
        resultShown_ = true;

        watch_ = StopWatch(watch_.isPaused());
    }

private:
    StopWatch watch_;
    bool resultShown_ = false;
};
}

#endif //PERF_H_83947184145342652456

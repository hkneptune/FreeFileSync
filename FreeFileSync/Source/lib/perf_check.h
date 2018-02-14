// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PERF_CHECK_H_87804217589312454
#define PERF_CHECK_H_87804217589312454

#include <map>
#include <chrono>
#include <string>
#include <zen/optional.h>


namespace fff
{
class PerfCheck
{
public:
    PerfCheck(std::chrono::milliseconds windowSizeRemTime,
              std::chrono::milliseconds windowSizeSpeed);

    void addSample(std::chrono::nanoseconds timeElapsed, int itemsCurrent, double dataCurrent);

    zen::Opt<double> getRemainingTimeSec(double dataRemaining) const;
    zen::Opt<std::wstring> getBytesPerSecond() const; //for window
    zen::Opt<std::wstring> getItemsPerSecond() const; //

private:
    struct Record
    {
        int    items = 0;
        double bytes = 0;
    };

    std::tuple<double, int, double> getBlockDeltas(std::chrono::milliseconds windowSize) const;

    const std::chrono::milliseconds windowSizeRemTime_;
    const std::chrono::milliseconds windowSizeSpeed_;
    const std::chrono::milliseconds windowMax_;

    std::map<std::chrono::nanoseconds, Record> samples_;
};
}

#endif //PERF_CHECK_H_87804217589312454

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PERF_CHECK_H_87804217589312454
#define PERF_CHECK_H_87804217589312454

#include <chrono>
#include <optional>
#include <string>
#include <zen/ring_buffer.h>


namespace fff
{
class SpeedTest
{
public:
    explicit SpeedTest(std::chrono::milliseconds windowSize) : windowSize_(windowSize) {}

    void addSample(std::chrono::nanoseconds timeElapsed, int itemsCurrent, int64_t bytesCurrent);

    std::optional<double> getRemainingSec(int itemsRemaining, int64_t bytesRemaining) const;
    std::optional<double> getBytesPerSec() const;
    std::optional<double> getItemsPerSec() const;

    std::wstring getBytesPerSecFmt() const; //empty if not (yet) available
    std::wstring getItemsPerSecFmt() const; //

    void clear() { samples_.clear(); }

private:
    struct Sample
    {
        std::chrono::nanoseconds timeElapsed{}; //std::chrono::duration is uninitialized by default! WTF
        int     items = 0;
        int64_t bytes = 0;
    };

    const std::chrono::milliseconds windowSize_;
    zen::RingBuffer<Sample> samples_;
};
}

#endif //PERF_CHECK_H_87804217589312454

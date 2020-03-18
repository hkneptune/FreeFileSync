// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "perf_check.h"
#include <zen/basic_math.h>
#include <zen/i18n.h>
#include <zen/format_unit.h>

using namespace zen;
using namespace fff;


PerfCheck::PerfCheck(std::chrono::milliseconds windowSizeRemTime,
                     std::chrono::milliseconds windowSizeSpeed) :
    windowSizeRemTime_(windowSizeRemTime),
    windowSizeSpeed_  (windowSizeSpeed),
    windowMax_(std::max(windowSizeRemTime, windowSizeSpeed)) {}


void PerfCheck::addSample(std::chrono::nanoseconds timeElapsed, int itemsCurrent, double bytesCurrent)
{
    samples_.insert(samples_.end(), { timeElapsed, { itemsCurrent, bytesCurrent }}); //use fact that time is monotonously ascending

    //remove all records earlier than "now - windowMax"
    auto it = samples_.upper_bound(timeElapsed - windowMax_);
    if (it != samples_.begin())
        samples_.erase(samples_.begin(), --it); //keep one point before newBegin in order to handle "measurement holes"
}


std::tuple<double /*timeDelta*/, int /*itemsDelta*/, double /*bytesDelta*/> PerfCheck::getBlockDeltas(std::chrono::milliseconds windowSize) const
{
    if (samples_.empty()) return {};

    auto itBack = samples_.rbegin();
    //find start of records "window"
    auto itFront = samples_.upper_bound(itBack->first - windowSize);
    if (itFront != samples_.begin())
        --itFront; //one point before window begin in order to handle "measurement holes"

    const double timeDelta = std::chrono::duration<double>(itBack->first - itFront->first).count();
    const int    itemsDelta = itBack->second.items - itFront->second.items;
    const double bytesDelta = itBack->second.bytes - itFront->second.bytes;

    return { timeDelta, itemsDelta, bytesDelta };
}


std::optional<double> PerfCheck::getRemainingTimeSec(double bytesRemaining) const
{
    const auto [timeDelta, itemsDelta, bytesDelta] = getBlockDeltas(windowSizeRemTime_);

    //objects model logical operations *NOT* disk accesses, so we better play safe and use "bytes" only!

    if (!numeric::isNull(bytesDelta)) //sign(dataRemaining) != sign(bytesDelta) usually an error, so show it!
        return bytesRemaining * timeDelta / bytesDelta;

    return {};
}


std::optional<std::wstring> PerfCheck::getBytesPerSecond() const
{
    const auto [timeDelta, itemsDelta, bytesDelta] = getBlockDeltas(windowSizeSpeed_);

    if (!numeric::isNull(timeDelta))
        return replaceCpy(_("%x/sec"), L"%x", formatFilesizeShort(numeric::round(bytesDelta / timeDelta)));

    return {};
}


std::optional<std::wstring> PerfCheck::getItemsPerSecond() const
{
    const auto [timeDelta, itemsDelta, bytesDelta] = getBlockDeltas(windowSizeSpeed_);

    if (!numeric::isNull(timeDelta))
        return replaceCpy(_("%x/sec"), L"%x", replaceCpy(_("%x items"), L"%x", formatTwoDigitPrecision(itemsDelta / timeDelta)));

    return {};
}


/*
class for calculation of remaining time:
----------------------------------------
"filesize |-> time" is an affine linear function f(x) = z_1 + z_2 x

For given n measurements, sizes x_0, ..., x_n and times f_0, ..., f_n, the function f (as a polynom of degree 1) can be lineary approximated by

z_1 = (r - s * q / p) / ((n + 1) - s * s / p)
z_2 = (q - s * z_1) / p = (r - (n + 1) z_1) / s

with
p := x_0^2 + ... + x_n^2
q := f_0 x_0 + ... + f_n x_n
r := f_0 + ... + f_n
s := x_0 + ... + x_n

=> the time to process N files with amount of data D is:    N * z_1 + D * z_2

Problem:
--------
Times f_0, ..., f_n can be very small so that precision of the PC clock is poor.
=> Times have to be accumulated to enhance precision:
Copying of m files with sizes x_i and times f_i (i = 1, ..., m) takes sum_i f(x_i) := m * z_1 + z_2 * sum x_i = sum f_i
With X defined as the accumulated sizes and F the accumulated times this gives: (in theory...)
m * z_1 + z_2 * X = F   <=>
z_1 + z_2 * X / m = F / m

=> we obtain a new (artificial) measurement with size X / m and time F / m to be used in the linear approximation above


Statistics::Statistics(int totalObjectCount, double totalDataAmount, unsigned recordCount) :
        itemsTotal(totalObjectCount),
        bytesTotal(totalDataAmount),
        recordsMax(recordCount),
        objectsLast(0),
        dataLast(0),
        timeLast(wxGetLocalTimeMillis()),
        z1_current(0),
        z2_current(0),
        dummyRecordPresent(false) {}


wxString Statistics::getRemainingTime(int objectsCurrent, double dataCurrent)
{
    //add new measurement point
    const int m = objectsCurrent - objectsLast;
    if (m != 0)
    {
        objectsLast = objectsCurrent;

        const double X = dataCurrent - dataLast;
        dataLast = dataCurrent;

        const int64_t timeCurrent = wxGetLocalTimeMillis();
        const double F = (timeCurrent - timeLast).ToDouble();
        timeLast = timeCurrent;

        record newEntry;
        newEntry.x_i = X / m;
        newEntry.f_i = F / m;

        //remove dummy record
        if (dummyRecordPresent)
        {
            measurements.pop_back();
            dummyRecordPresent = false;
        }

        //insert new record
        measurements.push_back(newEntry);
        if (measurements.size() > recordsMax)
            measurements.pop_front();
    }
    else //dataCurrent increased without processing new objects:
    {    //modify last measurement until m != 0
        const double X = dataCurrent - dataLast; //do not set dataLast, timeLast variables here, but write dummy record instead
        if (!isNull(X))
        {
            const int64_t timeCurrent = wxGetLocalTimeMillis();
            const double F = (timeCurrent - timeLast).ToDouble();

            record modifyEntry;
            modifyEntry.x_i = X;
            modifyEntry.f_i = F;

            //insert dummy record
            if (!dummyRecordPresent)
            {
                measurements.push_back(modifyEntry);
                if (measurements.size() > recordsMax)
                    measurements.pop_front();
                dummyRecordPresent = true;
            }
            else //modify dummy record
                measurements.back() = modifyEntry;
        }
    }

    //calculate remaining time based on stored measurement points
    double p = 0;
    double q = 0;
    double r = 0;
    double s = 0;
    for (const record& rec : measurements)
    {
        const double x_i = rec.x_i;
        const double f_i = rec.f_i;
        p += x_i * x_i;
        q += f_i * x_i;
        r += f_i;
        s += x_i;
    }

    if (!isNull(p))
    {
        const double n   = measurements.size();
        const double tmp = (n - s * s / p);

        if (!isNull(tmp) && !isNull(s))
        {
            const double z1 = (r - s * q / p) / tmp;
            const double z2 = (r - n * z1) / s;    //not (n + 1) here, since n already is the number of measurements

            //refresh current values for z1, z2
            z1_current = z1;
            z2_current = z2;
        }
    }

    return formatRemainingTime((itemsTotal - objectsCurrent) * z1_current + (bytesTotal - dataCurrent) * z2_current);
}
*/

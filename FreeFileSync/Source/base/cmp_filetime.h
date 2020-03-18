// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef CMP_FILETIME_H_032180451675845
#define CMP_FILETIME_H_032180451675845

#include <ctime>
#include <algorithm>


namespace fff
{
inline
bool sameFileTime(time_t lhs, time_t rhs, int tolerance, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    if (tolerance < 0) //:= unlimited tolerance by convention!
        return true;

    if (lhs < rhs)
        std::swap(lhs, rhs);

    if (rhs > std::numeric_limits<time_t>::max() - tolerance) //protect against overflow!
        return true;

    if (lhs <= rhs + tolerance)
        return true;

    for (const unsigned int minutes : ignoreTimeShiftMinutes)
    {
        assert(minutes > 0);
        const int shiftSec = static_cast<int>(minutes) * 60;

        time_t low  = rhs;
        time_t high = lhs;

        if (low <= std::numeric_limits<time_t>::max() - shiftSec) //protect against overflow!
            low += shiftSec;
        else
            high -= shiftSec;

        if (high < low)
            std::swap(high, low);

        if (low > std::numeric_limits<time_t>::max() - tolerance) //protect against overflow!
            return true;

        if (high <= low + tolerance)
            return true;
    }

    return false;
}

//---------------------------------------------------------------------------------------------------------------

enum class TimeResult
{
    EQUAL,
    LEFT_NEWER,
    RIGHT_NEWER,
    LEFT_INVALID,
    RIGHT_INVALID
};


inline
TimeResult compareFileTime(time_t lhs, time_t rhs, int tolerance, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    //number of seconds since Jan 1st 1970 + 1 year (needn't be too precise)
    static const time_t oneYearFromNow = std::time(nullptr) + 365 * 24 * 3600;

    if (sameFileTime(lhs, rhs, tolerance, ignoreTimeShiftMinutes)) //last write time may differ by up to 2 seconds (NTFS vs FAT32)
        return TimeResult::EQUAL;

    //check for erroneous dates
    if (lhs < 0 || lhs > oneYearFromNow) //earlier than Jan 1st 1970 or more than one year in future
        return TimeResult::LEFT_INVALID;

    if (rhs < 0 || rhs > oneYearFromNow)
        return TimeResult::RIGHT_INVALID;

    //regular time comparison
    if (lhs < rhs)
        return TimeResult::RIGHT_NEWER;
    else
        return TimeResult::LEFT_NEWER;
}
}

#endif //CMP_FILETIME_H_032180451675845

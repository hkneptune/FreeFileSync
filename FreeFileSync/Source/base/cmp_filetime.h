// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef CMP_FILETIME_H_032180451675845
#define CMP_FILETIME_H_032180451675845

#include <ctime>
//#include <algorithm>


namespace fff
{
inline
bool sameFileTime(time_t lhs, time_t rhs, /*unsigned*/ int tolerance, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    assert(tolerance >= 0);

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
    equal,
    leftNewer,
    rightNewer,
    leftInvalid,
    rightInvalid
};


//number of seconds since Jan 1st 1970 + 1 year (needn't be too precise)
inline const time_t oneYearFromNow = std::time(nullptr) + 365 * 24 * 3600;


inline
TimeResult compareFileTime(time_t lhs, time_t rhs, unsigned int tolerance, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    assert(oneYearFromNow != 0);
    if (sameFileTime(lhs, rhs, tolerance, ignoreTimeShiftMinutes)) //last write time may differ by up to 2 seconds (NTFS vs FAT32)
        return TimeResult::equal;

    //check for erroneous dates
    if (lhs < 0 || lhs > oneYearFromNow) //earlier than Jan 1st 1970 or more than one year in future
        return TimeResult::leftInvalid;

    if (rhs < 0 || rhs > oneYearFromNow)
        return TimeResult::rightInvalid;

    //regular time comparison
    if (lhs < rhs)
        return TimeResult::rightNewer;
    else
        return TimeResult::leftNewer;
}
}

#endif //CMP_FILETIME_H_032180451675845

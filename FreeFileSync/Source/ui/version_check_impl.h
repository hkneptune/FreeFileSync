// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef VERSION_ID_HEADER_2348769284769242
#define VERSION_ID_HEADER_2348769284769242

#include <ctime>
#include <zen/basic_math.h>
#include "../version/version.h"


namespace fff
{
inline
time_t getVersionCheckInactiveId()
{
    //use current version to calculate a changing number for the inactive state near UTC begin, in order to always check for updates after installing a new version
    //=> convert version into 11-based *unique* number (this breaks lexicographical version ordering, but that's irrelevant!)
    int id = 0;
    const char* first = ffsVersion;
    const char* last = first + zen::strLength(ffsVersion);
    std::for_each(first, last, [&](char c)
    {
        id *= 11;
        if ('0' <= c && c <= '9')
            id += c - '0';
        else
        {
            assert(c == FFS_VERSION_SEPARATOR);
            id += 10;
        }
    });
    assert(0 < id && id < 3600 * 24 * 365); //as long as value is within a year after UTC begin (1970) there's no risk to clash with *current* time
    return id;
}


inline
time_t getVersionCheckCurrentTime()
{
    return std::time(nullptr);
}


inline
bool shouldRunAutomaticUpdateCheck(time_t lastUpdateCheck)
{
    if (lastUpdateCheck == getVersionCheckInactiveId())
        return false;

    const time_t now = std::time(nullptr);
    return numeric::dist(now, lastUpdateCheck) >= 7 * 24 * 3600; //check weekly
}
}

#endif //VERSION_ID_HEADER_2348769284769242

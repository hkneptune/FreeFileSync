// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FMT_UNIT_8702184019487324
#define FMT_UNIT_8702184019487324

#include <string>
#include <optional>


namespace zen
{
    const int bytesPerKilo = 1000;
std::wstring formatFilesizeShort(int64_t filesize);
std::wstring formatRemainingTime(double timeInSec);
std::wstring formatProgressPercent(double fraction /*[0, 1]*/, int decPlaces = 0 /*[0, 9]*/); //rounded down!
std::wstring formatUtcToLocalTime(time_t utcTime); //like Windows Explorer would...

std::wstring formatTwoDigitPrecision  (double value); //format with fixed number of digits
std::wstring formatThreeDigitPrecision(double value); //(unless value is too large)

std::wstring formatNumber(int64_t n); //format integer number including thousands separator



enum class WeekDay
{
    monday,
    tuesday,
    wednesday,
    thursday,
    friday,
    saturday,
    sunday,
};
WeekDay getFirstDayOfWeek();

namespace impl { WeekDay getFirstDayOfWeekImpl(); } //throw SysError
}

#endif

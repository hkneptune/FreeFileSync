// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FMT_UNIT_8702184019487324
#define FMT_UNIT_8702184019487324

#include <string>
#include <cstdint>
#include "string_tools.h"


namespace zen
{
std::wstring formatFilesizeShort(int64_t filesize);
std::wstring formatRemainingTime(double timeInSec);
std::wstring formatFraction(double fraction); //within [0, 1]
std::wstring formatUtcToLocalTime(time_t utcTime); //like Windows Explorer would...

std::wstring formatTwoDigitPrecision  (double value); //format with fixed number of digits
std::wstring formatThreeDigitPrecision(double value); //(unless value is too large)

std::wstring formatNumber(int64_t n); //format integer number including thousands separator

}

#endif

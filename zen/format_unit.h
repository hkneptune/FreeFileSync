// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FMT_UNIT_8702184019487324
#define FMT_UNIT_8702184019487324

#include <string>
#include <cstdint>
#include "optional.h"
#include "string_tools.h"


namespace zen
{
std::wstring formatFilesizeShort(int64_t filesize);
std::wstring formatRemainingTime(double timeInSec);
std::wstring formatFraction(double fraction); //within [0, 1]
std::wstring formatUtcToLocalTime(int64_t utcTime); //like Windows Explorer would...

std::wstring formatTwoDigitPrecision  (double value); //format with fixed number of digits
std::wstring formatThreeDigitPrecision(double value); //(unless value is too large)

template <class NumberType>
std::wstring formatNumber(NumberType number); //format integer number including thousands separator











//--------------- inline impelementation -------------------------------------------
namespace impl
{
std::wstring includeNumberSeparator(const std::wstring& number);
}

template <class NumberType> inline
std::wstring formatNumber(NumberType number)
{
    static_assert(IsInteger<NumberType>::value, "");
    return impl::includeNumberSeparator(zen::numberTo<std::wstring>(number));
}
}

#endif

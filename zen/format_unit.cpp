// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "format_unit.h"
#include <ctime>
#include <cstdio>
#include "basic_math.h"
#include "i18n.h"
#include "time.h"
#include "globals.h"

    #include <clocale> //thousands separator
    #include "utf.h"   //

using namespace zen;


std::wstring zen::formatTwoDigitPrecision(double value)
{
    //print two digits: 0,1 | 1,1 | 11
    if (numeric::abs(value) < 9.95) //9.99 must not be formatted as "10.0"
        return printNumber<std::wstring>(L"%.1f", value);
    return numberTo<std::wstring>(numeric::round(value));
}


std::wstring zen::formatThreeDigitPrecision(double value)
{
    //print three digits: 0,01 | 0,11 | 1,11 | 11,1 | 111
    if (numeric::abs(value) < 9.995) //9.999 must not be formatted as "10.00"
        return printNumber<std::wstring>(L"%.2f", value);
    if (numeric::abs(value) < 99.95) //99.99 must not be formatted as "100.0"
        return printNumber<std::wstring>(L"%.1f", value);
    return numberTo<std::wstring>(numeric::round(value));
}


std::wstring zen::formatFilesizeShort(int64_t size)
{
    //if (size < 0) return _("Error"); -> really?

    if (numeric::abs(size) <= 999)
        return _P("1 byte", "%x bytes", static_cast<int>(size));

    double sizeInUnit = static_cast<double>(size);

    auto formatUnit = [&](const std::wstring& unitTxt) { return replaceCpy(unitTxt, L"%x", formatThreeDigitPrecision(sizeInUnit)); };

    sizeInUnit /= 1024;
    if (numeric::abs(sizeInUnit) < 999.5)
        return formatUnit(_("%x KB"));

    sizeInUnit /= 1024;
    if (numeric::abs(sizeInUnit) < 999.5)
        return formatUnit(_("%x MB"));

    sizeInUnit /= 1024;
    if (numeric::abs(sizeInUnit) < 999.5)
        return formatUnit(_("%x GB"));

    sizeInUnit /= 1024;
    if (numeric::abs(sizeInUnit) < 999.5)
        return formatUnit(_("%x TB"));

    sizeInUnit /= 1024;
    return formatUnit(_("%x PB"));
}


namespace
{
enum UnitRemTime
{
    URT_SEC,
    URT_MIN,
    URT_HOUR,
    URT_DAY
};


std::wstring formatUnitTime(int val, UnitRemTime unit)
{
    switch (unit)
    {
        case URT_SEC:
            return _P("1 sec", "%x sec", val);
        case URT_MIN:
            return _P("1 min", "%x min", val);
        case URT_HOUR:
            return _P("1 hour", "%x hours", val);
        case URT_DAY:
            return _P("1 day", "%x days", val);
    }
    assert(false);
    return _("Error");
}


template <int M, int N>
std::wstring roundToBlock(double timeInHigh,
                          UnitRemTime unitHigh, const int (&stepsHigh)[M],
                          int unitLowPerHigh,
                          UnitRemTime unitLow, const int (&stepsLow)[N])
{
    assert(unitLowPerHigh > 0);
    const double granularity = 0.1;
    const double timeInLow = timeInHigh * unitLowPerHigh;
    const int blockSizeLow = granularity * timeInHigh < 1 ?
                             numeric::nearMatch(granularity * timeInLow,  std::begin(stepsLow),  std::end(stepsLow)):
                             numeric::nearMatch(granularity * timeInHigh, std::begin(stepsHigh), std::end(stepsHigh)) * unitLowPerHigh;
    const int roundedtimeInLow = static_cast<int>(numeric::round(timeInLow / blockSizeLow) * blockSizeLow);

    std::wstring output = formatUnitTime(roundedtimeInLow / unitLowPerHigh, unitHigh);
    if (unitLowPerHigh > blockSizeLow)
        output += L" " + formatUnitTime(roundedtimeInLow % unitLowPerHigh, unitLow);
    return output;
};
}


std::wstring zen::formatRemainingTime(double timeInSec)
{
    const int steps10[] = { 1, 2, 5, 10 };
    const int steps24[] = { 1, 2, 3, 4, 6, 8, 12, 24 };
    const int steps60[] = { 1, 2, 5, 10, 15, 20, 30, 60 };

    //determine preferred unit
    double timeInUnit = timeInSec;
    if (timeInUnit <= 60)
        return roundToBlock(timeInUnit, URT_SEC, steps60, 1, URT_SEC, steps60);

    timeInUnit /= 60;
    if (timeInUnit <= 60)
        return roundToBlock(timeInUnit, URT_MIN, steps60, 60, URT_SEC, steps60);

    timeInUnit /= 60;
    if (timeInUnit <= 24)
        return roundToBlock(timeInUnit, URT_HOUR, steps24, 60, URT_MIN, steps60);

    timeInUnit /= 24;
    return roundToBlock(timeInUnit, URT_DAY, steps10, 24, URT_HOUR, steps24);
    //note: for 10% granularity steps10 yields a valid blocksize only up to timeInUnit == 100!
    //for larger time sizes this results in a finer granularity than expected: 10 days -> should not be a problem considering "usual" remaining time for synchronization
}


//std::wstring zen::fractionToString1Dec(double fraction)
//{
//    return printNumber<std::wstring>(L"%.1f", fraction * 100.0) + L'%'; //no need to internationalize fraction!?
//}


std::wstring zen::formatFraction(double fraction)
{
    return printNumber<std::wstring>(L"%.2f", fraction * 100.0) + L'%'; //no need to internationalize fraction!?
}




std::wstring zen::impl::includeNumberSeparator(const std::wstring& number)
{
    //we have to include thousands separator ourselves; this doesn't work for all countries (e.g india), but is better than nothing

    //::setlocale (LC_ALL, ""); -> implicitly called by wxLocale
    const lconv* localInfo = ::localeconv(); //always bound according to doc
    const std::wstring& thousandSep = zen::utfTo<std::wstring>(localInfo->thousands_sep);

    // THOUSANDS_SEPARATOR = std::use_facet<std::numpunct<wchar_t>>(std::locale("")).thousands_sep(); - why not working?
    // DECIMAL_POINT       = std::use_facet<std::numpunct<wchar_t>>(std::locale("")).decimal_point();

    std::wstring output(number);
    size_t i = output.size();
    for (;;)
    {
        if (i <= 3)
            break;
        i -= 3;
        if (!isDigit(output[i - 1])) //stop on +, - signs
            break;
        output.insert(i, thousandSep);
    }
    return output;
}


std::wstring zen::formatUtcToLocalTime(int64_t utcTime)
{
    auto errorMsg = [&] { return _("Error") + L" (time_t: " + numberTo<std::wstring>(utcTime) + L")"; };

    TimeComp loc = getLocalTime(utcTime);

    std::wstring dateString = formatTime<std::wstring>(L"%x  %X", loc);
    return !dateString.empty() ? dateString : errorMsg();
}



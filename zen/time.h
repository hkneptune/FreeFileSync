// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TIME_H_8457092814324342453627
#define TIME_H_8457092814324342453627

#include <ctime>
#include "basic_math.h"
#include "zstring.h"


namespace zen
{
struct TimeComp //replaces std::tm and SYSTEMTIME
{
    int year   = 0; // -
    int month  = 0; //1-12
    int day    = 0; //1-31
    int hour   = 0; //0-23
    int minute = 0; //0-59
    int second = 0; //0-60 (including leap second)

    bool operator==(const TimeComp&) const = default;
};

TimeComp getUtcTime(time_t utc); //convert time_t (UTC) to UTC time components, returns TimeComp() on error
TimeComp getUtcTime(); //utc = std::time()
std::pair<time_t, bool /*success*/> utcToTimeT(const TimeComp& tc); //convert UTC time components to time_t (UTC)

TimeComp getLocalTime(time_t utc); //convert time_t (UTC) to local time components, returns TimeComp() on error
TimeComp getLocalTime(); //utc = std::time()
std::pair<time_t, bool /*success*/> localToTimeT(const TimeComp& tc); //convert local time components to time_t (UTC)

TimeComp getCompileTime(); //returns TimeComp() on error

//----------------------------------------------------------------------------------------------------------------------------------
/* format (current) date and time; example:
            formatTime(Zstr("%Y|%m|%d")); -> "2011|10|29"
            formatTime(formatDateTag);    -> "2011-10-29"
            formatTime(formatTimeTag);    -> "17:55:34"                       */
Zstring formatTime(const Zchar* format, const TimeComp& tc = getLocalTime()); //format as specified by "std::strftime", returns empty string on error

//the "format" parameter of formatTime() is partially specialized with the following type tags:
const Zchar* const formatDateTag     = Zstr("%x"); //locale-dependent date representation: e.g. 8/23/2001
const Zchar* const formatTimeTag     = Zstr("%X"); //locale-dependent time representation: e.g. 2:55:02 PM
const Zchar* const formatDateTimeTag = Zstr("%c"); //locale-dependent date and time:       e.g. 8/23/2001 2:55:02 PM

const Zchar* const formatIsoDateTag     = Zstr("%Y-%m-%d");          //e.g. 2001-08-23
const Zchar* const formatIsoTimeTag     = Zstr("%H:%M:%S");          //e.g. 14:55:02
const Zchar* const formatIsoDateTimeTag = Zstr("%Y-%m-%d %H:%M:%S"); //e.g. 2001-08-23 14:55:02

//----------------------------------------------------------------------------------------------------------------------------------
//example: parseTime("%Y-%m-%d %H:%M:%S",  "2001-08-23 14:55:02");
//         parseTime(formatIsoDateTimeTag, "2001-08-23 14:55:02");
template <class String, class String2>
TimeComp parseTime(const String& format, const String2& str); //similar to ::strptime()
//----------------------------------------------------------------------------------------------------------------------------------

//format: [-][[d.]HH:]MM:SS    e.g. -1.23:45:67
Zstring formatTimeSpan(int64_t timeInSec, bool hourOptional = false);











//############################ implementation ##############################
namespace impl
{
inline
std::tm toClibTimeComponents(const TimeComp& tc)
{
    assert(1 <= tc.month  && tc.month  <= 12 &&
           1 <= tc.day    && tc.day    <= 31 &&
           0 <= tc.hour   && tc.hour   <= 23 &&
           0 <= tc.minute && tc.minute <= 59 &&
           0 <= tc.second && tc.second <= 61);

    return
    {
        .tm_sec   = tc.second,      //0-60 (including leap second)
        .tm_min   = tc.minute,      //0-59
        .tm_hour  = tc.hour,        //0-23
        .tm_mday  = tc.day,         //1-31
        .tm_mon   = tc.month - 1,   //0-11
        .tm_year  = tc.year - 1900, //years since 1900
        .tm_isdst = -1,             //> 0 if DST is active, == 0 if DST is not active, < 0 if the information is not available
        //.tm_wday
        //.tm_yday
    };
}

inline
TimeComp toZenTimeComponents(const std::tm& ctc)
{
    return
    {
        .year   = ctc.tm_year + 1900,
        .month  = ctc.tm_mon + 1,
        .day    = ctc.tm_mday,
        .hour   = ctc.tm_hour,
        .minute = ctc.tm_min,
        .second = ctc.tm_sec,
    };
}


/*
inline
bool isValid(const std::tm& t)
{
     -> not enough! MSCRT has different limits than the C standard which even seem to change with different versions:
        _VALIDATE_RETURN((( timeptr->tm_sec >=0 ) && ( timeptr->tm_sec <= 59 ) ), EINVAL, FALSE)
        _VALIDATE_RETURN(( timeptr->tm_year >= -1900 ) && ( timeptr->tm_year <= 8099 ), EINVAL, FALSE)
    -> also std::mktime does *not* help here at all!

    auto inRange = [](int value, int minVal, int maxVal) { return minVal <= value && value <= maxVal; };

    //https://www.cplusplus.com/reference/clibrary/ctime/tm/
    return inRange(t.tm_sec,  0, 61) &&
           inRange(t.tm_min,  0, 59) &&
           inRange(t.tm_hour, 0, 23) &&
           inRange(t.tm_mday, 1, 31) &&
           inRange(t.tm_mon,  0, 11) &&
           //tm_year
           inRange(t.tm_wday, 0, 6) &&
           inRange(t.tm_yday, 0, 365);
    //tm_isdst
};
*/
}


constexpr auto daysPer400Years = 100 * (4 * 365 /*usual days per year*/ + 1 /*including leap day*/) - 3 /*no leap days for centuries, except if divisible by 400 */;
constexpr auto secsPer400Years = 3600LL * 24 * daysPer400Years;


inline
TimeComp getUtcTime(time_t utc)
{
    //Windows: gmtime_s() only works for years [1970, 3001]
    //=> map into working 400-year range [1970, 2370)
    //   bonus: avoid asking for bugs for time_t(-1)
    const int cycles400 = static_cast<int>(numeric::intDivFloor(utc, secsPer400Years));
    utc -= secsPer400Years * cycles400;

    std::tm ctc = {};
    if (::gmtime_r(&utc, &ctc) == nullptr) //Linux, macOS: apparently NO limits (tested years 0 to 10.000!)
        return TimeComp();

    ctc.tm_year += 400 * cycles400;

    return impl::toZenTimeComponents(ctc);
}


inline
TimeComp getUtcTime()
{
    const time_t utc = std::time(nullptr); //returns -1 on error
    if (utc == -1)
        return TimeComp();

    return getUtcTime(utc);
}


inline
TimeComp getLocalTime(time_t utc)
{
    const int cycles400 = static_cast<int>(numeric::intDivFloor(utc, secsPer400Years));
    utc -= secsPer400Years * cycles400;

    std::tm ctc = {};
    if (::localtime_r(&utc, &ctc) == nullptr)
        return TimeComp();

    ctc.tm_year += 400 * cycles400;

    return impl::toZenTimeComponents(ctc);
}


inline
TimeComp getLocalTime()
{
    const time_t utc = std::time(nullptr); //returns -1 on error
    if (utc == -1)
        return TimeComp();

    return getLocalTime(utc);
}


inline
std::pair<time_t, bool /*success*/> utcToTimeT(const TimeComp& tc)
{
    if (tc == TimeComp())
        return {};

    std::tm ctc = impl::toClibTimeComponents(tc);
    ctc.tm_isdst = 0; //"Zero (0) to indicate that standard time is in effect" => unused by _mkgmtime, but take no chances

    /*  Windows: _mkgmtime() only works for years [1970, 3001]
        macOS: timegm() requires tm_year >= 1900; apparently no upper limit (tested until year 10.000!)
        Linux, 64-bit: apparently NO limits (tested years 0 to 10.000!)
               32-bit: timegm() only works for years [1902, 2038] => sucks to be on 32-bit! :>

        => map into working 400-year range [1970, 2370)
           bonus: disambiguate -1 error code from time_t(-1)          */
    const int cycles400 = numeric::intDivFloor(ctc.tm_year + 1900 - 1970, 400);
    ctc.tm_year -= 400 * cycles400;

    const time_t utc = ::timegm(&ctc);
    if (utc == -1)
        return {};

    assert(utc >= 0);
    return {utc + secsPer400Years * cycles400, true};
}


inline
std::pair<time_t, bool /*success*/> localToTimeT(const TimeComp& tc) //convert local time components to time_t (UTC)
{
    if (tc == TimeComp())
        return {};

    std::tm ctc = impl::toClibTimeComponents(tc);

    const int cycles400 = numeric::intDivFloor(ctc.tm_year + 1900 - 1971/*[!]*/, 400); //see utcToTimeT()
    //1971: ensures resulting time_t >= 0 after time zone, DST adaption, or std::mktime will fail on Windows!
    ctc.tm_year -= 400 * cycles400;

    const time_t locTime = std::mktime(&ctc);
    if (locTime == -1)
        return {};

    assert(locTime > 0);
    return {locTime + secsPer400Years * cycles400, true};
}


inline
TimeComp getCompileTime()
{
    //https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
    char compileTime[] = __DATE__ " " __TIME__; //e.g. "Aug  1 2017 01:32:26"
    if (compileTime[4] == ' ') //day is space-padded, but %d expects zero-padding
        compileTime[4] = '0';

    return parseTime("%b %d %Y %H:%M:%S", compileTime);
}




inline
Zstring formatTime(const Zchar* format, const TimeComp& tc)
{
    if (tc == TimeComp()) //failure code from getLocalTime()
        return Zstring();

    std::tm ctc = impl::toClibTimeComponents(tc);
    std::mktime(&ctc); //unfortunately std::strftime() needs all elements of "struct tm" filled, e.g. tm_wday, tm_yday
    //note: although std::mktime() explicitly expects "local time", calculating weekday and day of year *should* be time-zone and DST independent

    Zstring buf(256, Zstr('\0'));
    //strftime() craziness on invalid input:
    //  VS 2010: CRASH unless "_invalid_parameter_handler" is set: https://docs.microsoft.com/en-us/cpp/c-runtime-library/parameter-validation
    //  GCC: returns 0, apparently no crash. Still, considering some clib maintainer's comments, we should expect the worst!
    //  Windows: avoid char-based strftime() which uses ANSI encoding! (e.g. Greek letters for AM/PM)
    const size_t charsWritten = std::strftime(buf.data(), buf.size(), format, &ctc);
    buf.resize(charsWritten);
    return buf;
}


template <class String, class String2>
TimeComp parseTime(const String& format, const String2& str)
{
    using CharType = GetCharTypeT<String>;
    static_assert(std::is_same_v<CharType, GetCharTypeT<String2>>);

    const CharType*       itStr = strBegin(str);
    const CharType* const strLast = itStr + strLength(str);

    auto extractNumber = [&](int& result, size_t digitCount)
    {
        if (strLast - itStr < makeSigned(digitCount))
            return false;

        if (!std::all_of(itStr, itStr + digitCount, isDigit<CharType>))
            return false;

        result = zen::stringTo<int>(makeStringView(itStr, digitCount));
        itStr += digitCount;
        return true;
    };

    TimeComp output;

    const CharType*       itFmt = strBegin(format);
    const CharType* const fmtLast = itFmt + strLength(format);

    for (; itFmt != fmtLast; ++itFmt)
    {
        const CharType fmt = *itFmt;

        if (fmt == '%')
        {
            ++itFmt;
            if (itFmt == fmtLast)
                return TimeComp();

            switch (*itFmt)
            {
                case 'Y':
                    if (!extractNumber(output.year, 4))
                        return TimeComp();
                    break;
                case 'm':
                    if (!extractNumber(output.month, 2))
                        return TimeComp();
                    break;
                case 'b': //abbreviated month name: Jan-Dec
                {
                    if (strLast - itStr < 3)
                        return TimeComp();

                    const char* months[] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
                    auto itMonth = std::find_if(std::begin(months), std::end(months), [&](const char* month)
                    {
                        return equalAsciiNoCase(makeStringView(itStr, 3), month);
                    });
                    if (itMonth == std::end(months))
                        return TimeComp();

                    output.month = 1 + static_cast<int>(itMonth - std::begin(months));
                    itStr += 3;
                }
                break;
                case 'd':
                    if (!extractNumber(output.day, 2))
                        return TimeComp();
                    break;
                case 'H':
                    if (!extractNumber(output.hour, 2))
                        return TimeComp();
                    break;
                case 'M':
                    if (!extractNumber(output.minute, 2))
                        return TimeComp();
                    break;
                case 'S':
                    if (!extractNumber(output.second, 2))
                        return TimeComp();
                    break;
                default:
                    return TimeComp();
            }
        }
        else if (isWhiteSpace(fmt)) //single whitespace in format => skip 0..n whitespace chars
        {
            while (itStr != strLast && isWhiteSpace(*itStr))
                ++itStr;
        }
        else
        {
            if (itStr == strLast || *itStr != fmt)
                return TimeComp();
            ++itStr;
        }
    }

    if (itStr != strLast)
        return TimeComp();

    return output;
}


inline
Zstring formatTimeSpan(int64_t timeInSec, bool hourOptional)
{
    Zstring timespanStr;

    if (timeInSec < 0)
    {
        timeInSec = -timeInSec; //need to fix LLONG_MIN?
        timespanStr = Zstr('-');
    }

    //check *before* subtracting days!
    const Zchar* timeSpanFmt = hourOptional && timeInSec < 3600 ? Zstr("%M:%S") : formatIsoTimeTag;

    const int secsPerDay = 24 * 3600;
    const int64_t days = numeric::intDivFloor(timeInSec, secsPerDay);
    if (days > 0)
    {
        timeInSec -= days * secsPerDay;
        timespanStr += numberTo<Zstring>(days) + Zstr("."); //don't need zen::formatNumber(), do we?
    }

    //format time span as if absolute UTC time
    const TimeComp& tc = getUtcTime(timeInSec); //returns TimeComp() on error
    timespanStr += formatTime(timeSpanFmt, tc); //returns empty string on error

    return timespanStr;
}
}

#endif //TIME_H_8457092814324342453627

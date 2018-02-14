// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TIME_H_8457092814324342453627
#define TIME_H_8457092814324342453627

#include <ctime>
#include "string_tools.h"


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
};
inline bool operator==(const TimeComp& lhs, const TimeComp& rhs)
{
    return lhs.year == rhs.year && lhs.month == rhs.month  && lhs.day == rhs.day && lhs.hour == rhs.hour && lhs.minute == rhs.minute && lhs.second == rhs.second;
}
inline bool operator!=(const TimeComp& lhs, const TimeComp& rhs) { return !(lhs == rhs); }

TimeComp getLocalTime(time_t utc = std::time(nullptr)); //convert time_t (UTC) to local time components, returns TimeComp() on error
time_t   localToTimeT(const TimeComp& tc);              //convert local time components to time_t (UTC), returns -1 on error

TimeComp getUtcTime(time_t utc = std::time(nullptr)); //convert time_t (UTC) to UTC time components, returns TimeComp() on error
time_t   utcToTimeT(const TimeComp& tc);              //convert UTC time components to time_t (UTC), returns -1 on error

TimeComp getCompileTime(); //returns TimeComp() on error

//----------------------------------------------------------------------------------------------------------------------------------

/*
format (current) date and time; example:
        formatTime<std::wstring>(L"%Y|%m|%d"); -> "2011|10|29"
        formatTime<std::wstring>(FORMAT_DATE); -> "2011-10-29"
        formatTime<std::wstring>(FORMAT_TIME); -> "17:55:34"
*/
template <class String, class String2>
String formatTime(const String2& format, const TimeComp& tc = getLocalTime()); //format as specified by "std::strftime", returns empty string on failure

//the "format" parameter of formatTime() is partially specialized with the following type tags:
const struct FormatDateTag     {} FORMAT_DATE      = {}; //%x - locale dependent date representation: e.g. 08/23/01
const struct FormatTimeTag     {} FORMAT_TIME      = {}; //%X - locale dependent time representation: e.g. 14:55:02
const struct FormatDateTimeTag {} FORMAT_DATE_TIME = {}; //%c - locale dependent date and time:       e.g. Thu Aug 23 14:55:02 2001

const struct FormatIsoDateTag     {} FORMAT_ISO_DATE      = {}; //%Y-%m-%d          - e.g. 2001-08-23
const struct FormatIsoTimeTag     {} FORMAT_ISO_TIME      = {}; //%H:%M:%S          - e.g. 14:55:02
const struct FormatIsoDateTimeTag {} FORMAT_ISO_DATE_TIME = {}; //%Y-%m-%d %H:%M:%S - e.g. 2001-08-23 14:55:02

//----------------------------------------------------------------------------------------------------------------------------------

/*
example: parseTime("%Y-%m-%d %H:%M:%S",  "2001-08-23 14:55:02");
         parseTime(FORMAT_ISO_DATE_TIME, "2001-08-23 14:55:02");
*/
template <class String, class String2>
TimeComp parseTime(const String& format, const String2& str); //similar to ::strptime()

//----------------------------------------------------------------------------------------------------------------------------------













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

    std::tm ctc = {};
    ctc.tm_year  = tc.year - 1900; //years since 1900
    ctc.tm_mon   = tc.month - 1;   //0-11
    ctc.tm_mday  = tc.day;         //1-31
    ctc.tm_hour  = tc.hour;        //0-23
    ctc.tm_min   = tc.minute;      //0-59
    ctc.tm_sec   = tc.second;      //0-60 (including leap second)
    ctc.tm_isdst = -1;             //> 0 if DST is active, == 0 if DST is not active, < 0 if the information is not available
    //ctc.tm_wday
    //ctc.tm_yday
    return ctc;
}

inline
TimeComp toZenTimeComponents(const std::tm& ctc)
{
    TimeComp tc;
    tc.year   = ctc.tm_year + 1900;
    tc.month  = ctc.tm_mon + 1;
    tc.day    = ctc.tm_mday;
    tc.hour   = ctc.tm_hour;
    tc.minute = ctc.tm_min;
    tc.second = ctc.tm_sec;
    return tc;
}


template <class T>
struct GetFormat; //get default time formats as char* or wchar_t*

template <>
struct GetFormat<FormatDateTag> //%x - locale dependent date representation: e.g. 08/23/01
{
    const char*    format(char)    const { return  "%x"; }
    const wchar_t* format(wchar_t) const { return L"%x"; }
};

template <>
struct GetFormat<FormatTimeTag> //%X - locale dependent time representation: e.g. 14:55:02
{
    const char*    format(char)    const { return  "%X"; }
    const wchar_t* format(wchar_t) const { return L"%X"; }
};

template <>
struct GetFormat<FormatDateTimeTag> //%c - locale dependent date and time:       e.g. Thu Aug 23 14:55:02 2001
{
    const char*    format(char)    const { return  "%c"; }
    const wchar_t* format(wchar_t) const { return L"%c"; }
};

template <>
struct GetFormat<FormatIsoDateTag> //%Y-%m-%d - e.g. 2001-08-23
{
    const char*    format(char)    const { return  "%Y-%m-%d"; }
    const wchar_t* format(wchar_t) const { return L"%Y-%m-%d"; }
};

template <>
struct GetFormat<FormatIsoTimeTag> //%H:%M:%S - e.g. 14:55:02
{
    const char*    format(char)    const { return  "%H:%M:%S"; }
    const wchar_t* format(wchar_t) const { return L"%H:%M:%S"; }
};

template <>
struct GetFormat<FormatIsoDateTimeTag> //%Y-%m-%d %H:%M:%S - e.g. 2001-08-23 14:55:02
{
    const char*    format(char)    const { return  "%Y-%m-%d %H:%M:%S"; }
    const wchar_t* format(wchar_t) const { return L"%Y-%m-%d %H:%M:%S"; }
};


//strftime() craziness on invalid input:
//  VS 2010: CRASH unless "_invalid_parameter_handler" is set: https://msdn.microsoft.com/en-us/library/ksazx244.aspx
//  GCC: returns 0, apparently no crash. Still, considering some clib maintainer's comments, we should expect the worst!
inline
size_t strftimeWrap_impl(char* buffer, size_t bufferSize, const char* format, const std::tm* timeptr)
{
    return std::strftime(buffer, bufferSize, format, timeptr);
}


inline
size_t strftimeWrap_impl(wchar_t* buffer, size_t bufferSize, const wchar_t* format, const std::tm* timeptr)
{
    return std::wcsftime(buffer, bufferSize, format, timeptr);
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

    //http://www.cplusplus.com/reference/clibrary/ctime/tm/
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

template <class CharType> inline
size_t strftimeWrap(CharType* buffer, size_t bufferSize, const CharType* format, const std::tm* timeptr)
{
    return strftimeWrap_impl(buffer, bufferSize, format, timeptr);
}


struct UserDefinedFormatTag {};
struct PredefinedFormatTag  {};

template <class String, class String2> inline
String formatTime(const String2& format, const TimeComp& tc, UserDefinedFormatTag) //format as specified by "std::strftime", returns empty string on failure
{
    using CharType = typename GetCharType<String>::Type;
    std::tm ctc = toClibTimeComponents(tc);
    std::mktime(&ctc); // unfortunately std::strftime() needs all elements of "struct tm" filled, e.g. tm_wday, tm_yday
    //note: although std::mktime() explicitly expects "local time", calculating weekday and day of year *should* be time-zone and DST independent

    CharType buffer[256] = {};
    const size_t charsWritten = strftimeWrap(buffer, 256, strBegin(format), &ctc);
    return String(buffer, charsWritten);
}


template <class String, class FormatType> inline
String formatTime(FormatType, const TimeComp& tc, PredefinedFormatTag)
{
    using CharType = typename GetCharType<String>::Type;
    return formatTime<String>(GetFormat<FormatType>().format(CharType()), tc, UserDefinedFormatTag());
}
}


inline
TimeComp getLocalTime(time_t utc)
{
    if (utc == -1) //failure code from std::time(nullptr)
        return TimeComp();

    std::tm ctc = {};
    if (::localtime_r(&utc, &ctc) == nullptr)
        return TimeComp();

    return impl::toZenTimeComponents(ctc);
}


inline
TimeComp getUtcTime(time_t utc)
{
    if (utc == -1) //failure code from std::time(nullptr)
        return TimeComp();

    std::tm ctc = {};
    if (::gmtime_r(&utc, &ctc) == nullptr)
        return TimeComp();

    return impl::toZenTimeComponents(ctc);
}


inline
time_t localToTimeT(const TimeComp& tc) //returns -1 on error
{
    if (tc == TimeComp())
        return -1;

    std::tm ctc = impl::toClibTimeComponents(tc);
    return std::mktime(&ctc);
}


inline
time_t utcToTimeT(const TimeComp& tc) //returns -1 on error
{
    if (tc == TimeComp())
        return -1;

    std::tm ctc = impl::toClibTimeComponents(tc);
    ctc.tm_isdst = 0; //"Zero (0) to indicate that standard time is in effect" => unused by _mkgmtime, but take no chances
    return ::timegm(&ctc);
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


template <class String, class String2> inline
String formatTime(const String2& format, const TimeComp& tc)
{
    if (tc == TimeComp()) //failure code from getLocalTime()
        return String();

    using FormatTag = typename SelectIf<
                      IsSameType<String2, FormatDateTag       >::value ||
                      IsSameType<String2, FormatTimeTag       >::value ||
                      IsSameType<String2, FormatDateTimeTag   >::value ||
                      IsSameType<String2, FormatIsoDateTag    >::value ||
                      IsSameType<String2, FormatIsoTimeTag    >::value ||
                      IsSameType<String2, FormatIsoDateTimeTag>::value, impl::PredefinedFormatTag, impl::UserDefinedFormatTag>::Type;

    return impl::formatTime<String>(format, tc, FormatTag());
}


namespace impl
{
template <class String, class String2>
TimeComp parseTime(const String& format, const String2& str, UserDefinedFormatTag)
{
    using CharType = typename GetCharType<String>::Type;
    static_assert(IsSameType<CharType, typename GetCharType<String2>::Type>::value, "");

    const CharType*       itStr = strBegin(str);
    const CharType* const strLast = itStr + strLength(str);

    auto extractNumber = [&](int& result, size_t digitCount) -> bool
    {
        if (strLast - itStr < makeSigned(digitCount))
            return false;

        if (std::any_of(itStr, itStr + digitCount, [](CharType c) { return !isDigit(c); }))
        return false;

        result = zen::stringTo<int>(StringRef<const CharType>(itStr, itStr + digitCount));
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

                    const char* months[] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
                    auto itMonth = std::find_if(std::begin(months), std::end(months), [&](const char* name)
                    {
                        return asciiToLower(itStr[0]) == name[0] &&
                               asciiToLower(itStr[1]) == name[1] &&
                               asciiToLower(itStr[2]) == name[2];
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


template <class FormatType, class String>  inline
TimeComp parseTime(FormatType, const String& str, PredefinedFormatTag)
{
    using CharType = typename GetCharType<String>::Type;
    return parseTime(GetFormat<FormatType>().format(CharType()), str, UserDefinedFormatTag());
}
}


template <class String, class String2> inline
TimeComp parseTime(const String& format, const String2& str)
{
    using FormatTag = typename SelectIf<
                      IsSameType<String, FormatIsoDateTag    >::value ||
                      IsSameType<String, FormatIsoTimeTag    >::value ||
                      IsSameType<String, FormatIsoDateTimeTag>::value, impl::PredefinedFormatTag, impl::UserDefinedFormatTag>::Type;

    return impl::parseTime(format, str, FormatTag());
}
}

#endif //TIME_H_8457092814324342453627

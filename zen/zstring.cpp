// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "zstring.h"
#include <stdexcept>
#include "utf.h"


using namespace zen;

/*
MSDN "Handling Sorting in Your Applications": https://msdn.microsoft.com/en-us/library/windows/desktop/dd318144

Perf test: compare strings 10 mio times; 64 bit build
-----------------------------------------------------
    string a = "Fjk84$%kgfj$%T\\\\Gffg\\gsdgf\\fgsx----------d-"
    string b = "fjK84$%kgfj$%T\\\\gfFg\\gsdgf\\fgSy----------dfdf"

Windows (UTF16 wchar_t)
  4 ns | wcscmp
 67 ns | CompareStringOrdinalFunc+ + bIgnoreCase
314 ns | LCMapString + wmemcmp

OS X (UTF8 char)
   6 ns | strcmp
  98 ns | strcasecmp
 120 ns | strncasecmp + std::min(sizeLhs, sizeRhs);
 856 ns | CFStringCreateWithCString       + CFStringCompare(kCFCompareCaseInsensitive)
1110 ns | CFStringCreateWithCStringNoCopy + CFStringCompare(kCFCompareCaseInsensitive)
________________________
time per call | function
*/




namespace
{
int compareNoCaseUtf8(const char* lhs, size_t lhsLen, const char* rhs, size_t rhsLen)
{
    //- strncasecmp implements ASCII CI-comparsion only! => signature is broken for UTF8-input; toupper() similarly doesn't support Unicode
    //- wcsncasecmp: https://opensource.apple.com/source/Libc/Libc-763.12/string/wcsncasecmp-fbsd.c
    // => re-implement comparison based on towlower() to avoid memory allocations

    impl::UtfDecoder<char> decL(lhs, lhsLen);
    impl::UtfDecoder<char> decR(rhs, rhsLen);
    for (;;)
    {
        const Opt<impl::CodePoint> cpL = decL.getNext();
        const Opt<impl::CodePoint> cpR = decR.getNext();
        if (!cpL || !cpR)
            return static_cast<int>(!cpR) - static_cast<int>(!cpL);

        static_assert(sizeof(wchar_t) == sizeof(impl::CodePoint), "");
        const wchar_t charL = ::towlower(static_cast<wchar_t>(*cpL)); //ordering: towlower() converts to higher code points than towupper()
        const wchar_t charR = ::towlower(static_cast<wchar_t>(*cpR)); //uses LC_CTYPE category of current locale
        if (charL != charR)
            return static_cast<unsigned int>(charL) - static_cast<unsigned int>(charR); //unsigned char-comparison is the convention!
        //unsigned underflow is well-defined!
    }
}
}


int cmpStringNaturalLinux(const char* lhs, size_t lhsLen, const char* rhs, size_t rhsLen)
{
    const char* const lhsEnd = lhs + lhsLen;
    const char* const rhsEnd = rhs + rhsLen;
    /*
        - compare strings after conceptually creating blocks of whitespace/numbers/text
        - implement strict weak ordering!
        - don't follow broken "strnatcasecmp": https://github.com/php/php-src/blob/master/ext/standard/strnatcmp.c
                1. incorrect non-ASCII CI-comparison
                2. incorrect bounds checks
                3. incorrect trimming of *all* whitespace
                4. arbitrary handling of leading 0 only at string begin
                5. incorrect handling of whitespace following a number
                6. code is a mess
    */
    for (;;)
    {
        if (lhs == lhsEnd || rhs == rhsEnd)
            return static_cast<int>(lhs != lhsEnd) - static_cast<int>(rhs != rhsEnd); //"nothing" before "something"
        //note: "something" never would have been condensed to "nothing" further below => can finish evaluation here

        const bool wsL = isWhiteSpace(*lhs);
        const bool wsR = isWhiteSpace(*rhs);
        if (wsL != wsR)
            return static_cast<int>(!wsL) - static_cast<int>(!wsR); //whitespace before non-ws!
        if (wsL)
        {
            ++lhs, ++rhs;
            while (lhs != lhsEnd && isWhiteSpace(*lhs)) ++lhs;
            while (rhs != rhsEnd && isWhiteSpace(*rhs)) ++rhs;
            continue;
        }

        const bool digitL = isDigit(*lhs);
        const bool digitR = isDigit(*rhs);
        if (digitL != digitR)
            return static_cast<int>(!digitL) - static_cast<int>(!digitR); //number before chars!
        if (digitL)
        {
            while (lhs != lhsEnd && *lhs == '0') ++lhs;
            while (rhs != rhsEnd && *rhs == '0') ++rhs;

            int rv = 0;
            for (;; ++lhs, ++rhs)
            {
                const bool endL = lhs == lhsEnd || !isDigit(*lhs);
                const bool endR = rhs == rhsEnd || !isDigit(*rhs);
                if (endL != endR)
                    return static_cast<int>(!endL) - static_cast<int>(!endR); //more digits means bigger number
                if (endL)
                    break; //same number of digits

                if (rv == 0 && *lhs != *rhs)
                    rv = *lhs - *rhs; //found first digit difference comparing from left
            }
            if (rv != 0)
                return rv;
            continue;
        }

        //compare full junks of text: consider unicode encoding!
        const char* textBeginL = lhs++;
        const char* textBeginR = rhs++; //current char is neither white space nor digit at this point!
        while (lhs != lhsEnd && !isWhiteSpace(*lhs) && !isDigit(*lhs)) ++lhs;
        while (rhs != rhsEnd && !isWhiteSpace(*rhs) && !isDigit(*rhs)) ++rhs;

        const int rv = compareNoCaseUtf8(textBeginL, lhs - textBeginL, textBeginR, rhs - textBeginR);
        if (rv != 0)
            return rv;
    }
}


namespace
{
}


int CmpNaturalSort::operator()(const Zchar* lhs, size_t lhsLen, const Zchar* rhs, size_t rhsLen) const
{
    //auto strL = utfTo<std::string>(Zstring(lhs, lhsLen));
    //auto strR = utfTo<std::string>(Zstring(rhs, rhsLen));
    //return cmpStringNaturalLinux(strL.c_str(), strL.size(), strR.c_str(), strR.size());
    return cmpStringNaturalLinux(lhs, lhsLen, rhs, rhsLen);

}
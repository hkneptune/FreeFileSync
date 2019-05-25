// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "zstring.h"
#include <stdexcept>
#include "utf.h"

    #include <gtk/gtk.h>
    #include "sys_error.h"

using namespace zen;


Zstring makeUpperCopy(const Zstring& str)
{
    //fast pre-check:
    if (isAsciiString(str.c_str())) //perf: in the range of 3.5ns
    {
        Zstring output = str;
        for (Zchar& c : output) c = asciiToUpper(c);
        return output;
    }

    Zstring strNorm = getUnicodeNormalForm(str);
    try
    {
        static_assert(sizeof(impl::CodePoint) == sizeof(gunichar));
        Zstring output;
        output.reserve(strNorm.size());

        impl::UtfDecoder<char> decoder(strNorm.c_str(), strNorm.size());
        while (const std::optional<impl::CodePoint> cp = decoder.getNext())
            impl::codePointToUtf<char>(::g_unichar_toupper(*cp), [&](char c) { output += c; }); //don't use std::towupper: *incomplete* and locale-dependent!

        return output;

    }
    catch (SysError&)
    {
        assert(false);
        return str;
    }
}


Zstring getUnicodeNormalForm(const Zstring& str)
{
    //fast pre-check:
    if (isAsciiString(str.c_str())) //perf: in the range of 3.5ns
        return str; //god bless our ref-counting! => save output string memory consumption!

    //Example: const char* decomposed  = "\x6f\xcc\x81";
    //         const char* precomposed = "\xc3\xb3";
    try
    {
        gchar* outStr = ::g_utf8_normalize(str.c_str(), str.length(), G_NORMALIZE_DEFAULT_COMPOSE);
        if (!outStr)
            throw SysError(L"g_utf8_normalize: conversion failed. (" + utfTo<std::wstring>(str) + L")");
        ZEN_ON_SCOPE_EXIT(::g_free(outStr));
        return outStr;

    }
    catch (SysError&)
    {
        assert(false);
        return str;
    }
}


Zstring replaceCpyAsciiNoCase(const Zstring& str, const Zstring& oldTerm, const Zstring& newTerm)
{
    if (oldTerm.empty())
        return str;

    Zstring output;

    for (size_t pos = 0;;)
    {
        const size_t posFound = std::search(str.begin() + pos, str.end(), //can't use makeUpperCopy(): input/output sizes may differ!
                                            oldTerm.begin(), oldTerm.end(),
        [](Zchar charL, Zchar charR) { return asciiToUpper(charL) == asciiToUpper(charR); }) - str.begin();

        if (posFound == str.size())
        {
            if (pos == 0) //optimize "oldTerm not found": return ref-counted copy
                return str;
            output.append(str.begin() + pos, str.end());
            return output;
        }

        output.append(str.begin() + pos, str.begin() + posFound);
        output += newTerm;
        pos = posFound + oldTerm.size();
    }
}


/*
https://docs.microsoft.com/de-de/windows/desktop/Intl/handling-sorting-in-your-applications

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
int compareNativePath(const Zstring& lhs, const Zstring& rhs)
{
    assert(lhs.find(Zchar('\0')) == Zstring::npos); //don't expect embedded nulls!
    assert(rhs.find(Zchar('\0')) == Zstring::npos); //

    return compareString(lhs, rhs);

}


namespace
{
int compareNoCaseUtf8(const char* lhs, size_t lhsLen, const char* rhs, size_t rhsLen)
{
    //- strncasecmp implements ASCII CI-comparsion only! => signature is broken for UTF8-input; toupper() similarly doesn't support Unicode
    //- wcsncasecmp: https://opensource.apple.com/source/Libc/Libc-763.12/string/wcsncasecmp-fbsd.c
    // => re-implement comparison based on g_unichar_tolower() to avoid memory allocations

    impl::UtfDecoder<char> decL(lhs, lhsLen);
    impl::UtfDecoder<char> decR(rhs, rhsLen);
    for (;;)
    {
        const std::optional<impl::CodePoint> cpL = decL.getNext();
        const std::optional<impl::CodePoint> cpR = decR.getNext();
        if (!cpL || !cpR)
            return static_cast<int>(!cpR) - static_cast<int>(!cpL);

        static_assert(sizeof(gunichar) == sizeof(impl::CodePoint));

        const gunichar charL = ::g_unichar_toupper(*cpL); //note: tolower can be ambiguous, so don't use:
        const gunichar charR = ::g_unichar_toupper(*cpR); //e.g. "Σ" (upper case) can be lower-case "ς" in the end of the word or "σ" in the middle.
        if (charL != charR)
            //ordering: "to lower" converts to higher code points than "to upper"
            return static_cast<unsigned int>(charL) - static_cast<unsigned int>(charR); //unsigned char-comparison is the convention!
        //unsigned underflow is well-defined!
    }
}
}


int compareNatural(const Zstring& lhs, const Zstring& rhs)
{
    //Unicode normal forms:
    //      Windows: CompareString() already ignores NFD/NFC differences: nice...
    //      Linux:  g_unichar_toupper() can't ignore differences
    //      macOS:  CFStringCompare() considers differences

    const Zstring& lhsNorm = getUnicodeNormalForm(lhs);
    const Zstring& rhsNorm = getUnicodeNormalForm(rhs);

    const char* strL = lhsNorm.c_str();
    const char* strR = rhsNorm.c_str();

    const char* const strEndL = strL + lhsNorm.size();
    const char* const strEndR = strR + rhsNorm.size();
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
        if (strL == strEndL || strR == strEndR)
            return static_cast<int>(strL != strEndL) - static_cast<int>(strR != strEndR); //"nothing" before "something"
        //note: "something" never would have been condensed to "nothing" further below => can finish evaluation here

        const bool wsL = isWhiteSpace(*strL);
        const bool wsR = isWhiteSpace(*strR);
        if (wsL != wsR)
            return static_cast<int>(!wsL) - static_cast<int>(!wsR); //whitespace before non-ws!
        if (wsL)
        {
            ++strL, ++strR;
            while (strL != strEndL && isWhiteSpace(*strL)) ++strL;
            while (strR != strEndR && isWhiteSpace(*strR)) ++strR;
            continue;
        }

        const bool digitL = isDigit(*strL);
        const bool digitR = isDigit(*strR);
        if (digitL != digitR)
            return static_cast<int>(!digitL) - static_cast<int>(!digitR); //number before chars!
        if (digitL)
        {
            while (strL != strEndL && *strL == '0') ++strL;
            while (strR != strEndR && *strR == '0') ++strR;

            int rv = 0;
            for (;; ++strL, ++strR)
            {
                const bool endL = strL == strEndL || !isDigit(*strL);
                const bool endR = strR == strEndR || !isDigit(*strR);
                if (endL != endR)
                    return static_cast<int>(!endL) - static_cast<int>(!endR); //more digits means bigger number
                if (endL)
                    break; //same number of digits

                if (rv == 0 && *strL != *strR)
                    rv = *strL - *strR; //found first digit difference comparing from left
            }
            if (rv != 0)
                return rv;
            continue;
        }

        //compare full junks of text: consider unicode encoding!
        const char* textBeginL = strL++;
        const char* textBeginR = strR++; //current char is neither white space nor digit at this point!
        while (strL != strEndL && !isWhiteSpace(*strL) && !isDigit(*strL)) ++strL;
        while (strR != strEndR && !isWhiteSpace(*strR) && !isDigit(*strR)) ++strR;

        const int rv = compareNoCaseUtf8(textBeginL, strL - textBeginL, textBeginR, strR - textBeginR);
        if (rv != 0)
            return rv;
    }

}

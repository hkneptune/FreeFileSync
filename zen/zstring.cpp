// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "zstring.h"
    #include <glib.h>
    #include "sys_error.h"

using namespace zen;


namespace
{
Zstring getUnicodeNormalForm_NonAsciiValidUtf(const Zstring& str, UnicodeNormalForm form)
{
    //Example: const char* decomposed  = "\x6f\xcc\x81"; //ó
    //         const char* precomposed = "\xc3\xb3"; //ó
    assert(!isAsciiString(str)); //includes "not-empty" check
    assert(!contains(str, Zchar('\0'))); //don't expect embedded nulls!

    try
    {
        gchar* strNorm = ::g_utf8_normalize(str.c_str(), str.length(), form == UnicodeNormalForm::nfc ? G_NORMALIZE_NFC : G_NORMALIZE_NFD);
        if (!strNorm)
            throw SysError(formatSystemError("g_utf8_normalize", L"", L"Conversion failed."));
        ZEN_ON_SCOPE_EXIT(::g_free(strNorm));

        const std::string_view strNormView(strNorm, strLength(strNorm));

        if (equalString(str, strNormView)) //avoid extra memory allocation
            return str;

        return Zstring(strNormView);

    }
    catch (const SysError& e)
    {
        throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Error normalizing string:" + '\n' +
                                 utfTo<std::string>(str)  + "\n\n" + utfTo<std::string>(e.toString()));
    }
}


Zstring getValidUtf(const Zstring& str)
{
    /*  1. do NOT fail on broken UTF encoding, instead normalize using REPLACEMENT_CHAR!
        2. NormalizeString() haateeez them Unicode non-characters: ERROR_NO_UNICODE_TRANSLATION! http://www.unicode.org/faq/private_use.html#nonchar1
         - No such issue on Linux/macOS with g_utf8_normalize(), and CFStringGetFileSystemRepresentation()
            -> still, probably good idea to "normalize" Unicode non-characters cross-platform
         - consistency for compareNoCase(): let's *unconditionally* check before other normalization operations, not just in error case!   */
    using impl::CodePoint;
    auto isUnicodeNonCharacter = [](CodePoint cp) { assert(cp <= impl::CODE_POINT_MAX); return (0xfdd0 <= cp && cp <= 0xfdef) || cp % 0x10'000 >= 0xfffe; };

    const bool invalidUtf = [&] //pre-check: avoid memory allocation if valid UTF
    {
        UtfDecoder<Zchar> decoder(str.c_str(), str.size());
        while (const std::optional<CodePoint> cp = decoder.getNext())
            if (*cp == impl::REPLACEMENT_CHAR || //marks broken UTF encoding
                isUnicodeNonCharacter(*cp))
                return true;
        return false;
    }();

    if (invalidUtf) //band-aid broken UTF encoding with REPLACEMENT_CHAR
    {
        Zstring validStr; //don't want extra memory allocations in the standard case (valid UTF)
        UtfDecoder<Zchar> decoder(str.c_str(), str.size());
        while (std::optional<CodePoint> cp = decoder.getNext())
        {
            if (isUnicodeNonCharacter(*cp))   //
                *cp = impl::REPLACEMENT_CHAR; //"normalize" Unicode non-characters

            codePointToUtf<Zchar>(*cp, [&](Zchar ch) { validStr += ch; });
        }
        return validStr;
    }
    else
        return str;
}


Zstring getUpperCaseAscii(const Zstring& str)
{
    assert(isAsciiString(str));

    Zstring output = str;
    for (Zchar& c : output)  //identical to LCMapStringEx(), g_unichar_toupper(), CFStringUppercase() [verified!]
        c = asciiToUpper(c); //
    return output;
}


Zstring getUpperCaseNonAscii(const Zstring& str)
{
    const Zstring& strValidUtf = getValidUtf(str);
    try
    {
        const Zstring strNorm = getUnicodeNormalForm_NonAsciiValidUtf(strValidUtf, UnicodeNormalForm::native);

        Zstring output;
        output.reserve(strNorm.size());

        UtfDecoder<char> decoder(strNorm.c_str(), strNorm.size());
        while (const std::optional<impl::CodePoint> cp = decoder.getNext())
            codePointToUtf<char>(::g_unichar_toupper(*cp), [&](char c) { output += c; }); //don't use std::towupper: *incomplete* and locale-dependent!

        static_assert(sizeof(impl::CodePoint) == sizeof(gunichar));
        return output;

    }
    catch (const SysError& e)
    {
        throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Error converting string to upper case:" + '\n' +
                                 utfTo<std::string>(str)  + "\n\n" + utfTo<std::string>(e.toString()));
    }
}
}


Zstring getUnicodeNormalForm(const Zstring& str, UnicodeNormalForm form)
{
    static_assert(std::is_same_v<decltype(str), const Zbase<Zchar>&>, "god bless our ref-counting! => save needless memory allocation!");

    if (isAsciiString(str)) //fast path: in the range of 3.5ns
        return str;

    return getUnicodeNormalForm_NonAsciiValidUtf(getValidUtf(str), form); //slow path
}


Zstring getUpperCase(const Zstring& str)
{
    return isAsciiString(str) ? //fast path: in the range of 3.5ns
           getUpperCaseAscii(str) :
           getUpperCaseNonAscii(str); //slow path
}


namespace
{
std::weak_ordering compareNoCaseUtf8(const char* lhs, size_t lhsLen, const char* rhs, size_t rhsLen)
{
    //expect Unicode normalized strings!
    assert(Zstring(lhs, lhsLen) == getUnicodeNormalForm(Zstring(lhs, lhsLen), UnicodeNormalForm::nfd));
    assert(Zstring(rhs, rhsLen) == getUnicodeNormalForm(Zstring(rhs, rhsLen), UnicodeNormalForm::nfd));

    //- strncasecmp implements ASCII CI-comparsion only! => signature is broken for UTF8-input; toupper() similarly doesn't support Unicode
    //- wcsncasecmp: https://opensource.apple.com/source/Libc/Libc-763.12/string/wcsncasecmp-fbsd.c
    // => re-implement comparison based on g_unichar_tolower() to avoid memory allocations

    UtfDecoder<char> decL(lhs, lhsLen);
    UtfDecoder<char> decR(rhs, rhsLen);
    for (;;)
    {
        const std::optional<impl::CodePoint> cpL = decL.getNext();
        const std::optional<impl::CodePoint> cpR = decR.getNext();
        if (!cpL || !cpR)
            return !cpR <=> !cpL;

        static_assert(sizeof(gunichar) == sizeof(impl::CodePoint));
        static_assert(std::is_unsigned_v<gunichar>, "unsigned char-comparison is the convention!");

        //ordering: "to lower" converts to higher code points than "to upper"
        const gunichar charL = ::g_unichar_toupper(*cpL); //note: tolower can be ambiguous, so don't use:
        const gunichar charR = ::g_unichar_toupper(*cpR); //e.g. "Σ" (upper case) can be lower-case "ς" in the end of the word or "σ" in the middle.
        if (charL != charR)
            return charL <=> charR;
    }
}
}


std::weak_ordering compareNatural(const Zstring& lhs, const Zstring& rhs)
{
    try
    {
        /* Unicode normal forms:
              Windows: CompareString() ignores NFD/NFC differences and converts to NFD
              Linux:  g_unichar_toupper() can't ignore differences
              macOS:  CFStringCompare() considers differences */
        const Zstring& lhsNorm = getUnicodeNormalForm(lhs, UnicodeNormalForm::nfd); //normalize: - broken UTF encoding
        const Zstring& rhsNorm = getUnicodeNormalForm(rhs, UnicodeNormalForm::nfd); //           - Unicode non-characters

        const char* strL = lhsNorm.c_str();
        const char* strR = rhsNorm.c_str();

        const char* const strEndL = strL + lhsNorm.size();
        const char* const strEndR = strR + rhsNorm.size();
        /*  - compare strings after conceptually creating blocks of whitespace/numbers/text
            - implement strict weak ordering!
            - don't follow broken "strnatcasecmp": https://github.com/php/php-src/blob/master/ext/standard/strnatcmp.c
                    1. incorrect non-ASCII CI-comparison
                    2. incorrect bounds checks
                    3. incorrect trimming of *all* whitespace
                    4. arbitrary handling of leading 0 only at string begin
                    5. incorrect handling of whitespace following a number
                    6. code is a mess                                          */
        for (;;)
        {
            if (strL == strEndL || strR == strEndR)
                return (strL != strEndL) <=> (strR != strEndR); //"nothing" before "something"
            //note: "something" never would have been condensed to "nothing" further below => can finish evaluation here

            const bool wsL = isWhiteSpace(*strL);
            const bool wsR = isWhiteSpace(*strR);
            if (wsL != wsR)
                return !wsL <=> !wsR; //whitespace before non-ws!
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
                return !digitL <=> !digitR; //numbers before chars!
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
                        return !endL <=> !endR; //more digits means bigger number
                    if (endL)
                        break; //same number of digits

                    if (rv == 0 && *strL != *strR)
                        rv = *strL - *strR; //found first digit difference comparing from left
                }
                if (rv != 0)
                    return rv <=> 0;
                continue;
            }

            //compare full junks of text: consider unicode encoding!
            const char* textBeginL = strL++;
            const char* textBeginR = strR++; //current char is neither white space nor digit at this point!
            while (strL != strEndL && !isWhiteSpace(*strL) && !isDigit(*strL)) ++strL;
            while (strR != strEndR && !isWhiteSpace(*strR) && !isDigit(*strR)) ++strR;

            if (const std::weak_ordering cmp = compareNoCaseUtf8(textBeginL, strL - textBeginL, textBeginR, strR - textBeginR);
                cmp != std::weak_ordering::equivalent)
                return cmp;
        }

    }
    catch (const SysError& e)
    {
        throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Error comparing strings:" + '\n' +
                                 utfTo<std::string>(lhs) + '\n' + utfTo<std::string>(rhs) + "\n\n" + utfTo<std::string>(e.toString()));
    }
}


std::weak_ordering compareNoCase(const Zstring& lhs, const Zstring& rhs)
{
    const bool isAsciiL = isAsciiString(lhs);
    const bool isAsciiR = isAsciiString(rhs);

    //fast path: no memory allocations => ~ 6x speedup
    if (isAsciiL && isAsciiR)
    {
        const size_t minSize = std::min(lhs.size(), rhs.size());
        for (size_t i = 0; i < minSize; ++i)
        {
            //ordering: do NOT call compareAsciiNoCase(), which uses asciiToLower()!
            const Zchar lUp = asciiToUpper(lhs[i]); //
            const Zchar rUp = asciiToUpper(rhs[i]); //no surprises: emulate getUpperCase() [verified!]
            if (lUp != rUp)                         //
                return lUp <=> rUp;                 //
        }
        return lhs.size() <=> rhs.size();
    }
    //--------------------------------------

    //can't we instead skip isAsciiString() and compare chars as long as isAsciiChar()?
    // => NOPE! e.g. decomposed Unicode! A seemingly single isAsciiChar() might be followed by a combining character!!!

    return (isAsciiL ? getUpperCaseAscii(lhs) : getUpperCaseNonAscii(lhs)) <=>
           (isAsciiR ? getUpperCaseAscii(rhs) : getUpperCaseNonAscii(rhs));
}


bool equalNoCase(const Zstring& lhs, const Zstring& rhs)
{
    const bool isAsciiL = isAsciiString(lhs);
    const bool isAsciiR = isAsciiString(rhs);

    //fast-path: no extra memory allocations
    //caveat: ASCII-char and non-ASCII Unicode *can* compare case-insensitive equal!!! e.g. i and ı https://freefilesync.org/forum/viewtopic.php?t=9718
    if (isAsciiL && isAsciiR)
    {
        if (lhs.size() != rhs.size())
            return false;

        for (size_t i = 0; i < lhs.size(); ++i)
            if (asciiToUpper(lhs[i]) !=
                asciiToUpper(rhs[i]))
                return false;
        return true;
    }

    return (isAsciiL ? getUpperCaseAscii(lhs) : getUpperCaseNonAscii(lhs)) ==
           (isAsciiR ? getUpperCaseAscii(rhs) : getUpperCaseNonAscii(rhs));
}

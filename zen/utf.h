// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef UTF_H_01832479146991573473545
#define UTF_H_01832479146991573473545

#include <cstdint>
#include <iterator>
#include "string_tools.h" //copyStringTo


namespace zen
{
//convert all(!) char- and wchar_t-based "string-like" objects applying a UTF8 conversions (but only if necessary!)
template <class TargetString, class SourceString>
TargetString utfTo(const SourceString& str);

const char BYTE_ORDER_MARK_UTF8[] = "\xEF\xBB\xBF";

template <class UtfString>
bool isValidUtf(const UtfString& str); //check for UTF-8 encoding errors

//access unicode characters in UTF-encoded string (char- or wchar_t-based)
template <class UtfString>
size_t unicodeLength(const UtfString& str); //return number of code points for UTF-encoded string

template <class UtfString>
UtfString getUnicodeSubstring(const UtfString& str, size_t uniPosFirst, size_t uniPosLast);









//----------------------- implementation ----------------------------------
namespace impl
{
using CodePoint = uint32_t;
using Char16    = uint16_t;
using Char8     = uint8_t;

const CodePoint LEAD_SURROGATE      = 0xd800;
const CodePoint TRAIL_SURROGATE     = 0xdc00; //== LEAD_SURROGATE_MAX + 1
const CodePoint TRAIL_SURROGATE_MAX = 0xdfff;

const CodePoint REPLACEMENT_CHAR    = 0xfffd;
const CodePoint CODE_POINT_MAX      = 0x10ffff;

static_assert(LEAD_SURROGATE + TRAIL_SURROGATE + TRAIL_SURROGATE_MAX + REPLACEMENT_CHAR + CODE_POINT_MAX == 1348603);


template <class Function> inline
void codePointToUtf16(CodePoint cp, Function writeOutput) //"writeOutput" is a unary function taking a Char16
{
    //https://en.wikipedia.org/wiki/UTF-16
    if (cp < LEAD_SURROGATE)
        writeOutput(static_cast<Char16>(cp));
    else if (cp <= TRAIL_SURROGATE_MAX) //invalid code point
        codePointToUtf16(REPLACEMENT_CHAR, writeOutput); //resolves to 1-character utf16
    else if (cp < 0x10000)
        writeOutput(static_cast<Char16>(cp));
    else if (cp <= CODE_POINT_MAX)
    {
        cp -= 0x10000;
        writeOutput(static_cast<Char16>( LEAD_SURROGATE + (cp >> 10)));
        writeOutput(static_cast<Char16>(TRAIL_SURROGATE + (cp & 0x3ff)));
    }
    else //invalid code point
        codePointToUtf16(REPLACEMENT_CHAR, writeOutput); //resolves to 1-character utf16
}


inline
size_t getUtf16Len(Char16 ch) //ch must be first code unit! returns 0 on error!
{
    if (ch < LEAD_SURROGATE)
        return 1;
    else if (ch < TRAIL_SURROGATE)
        return 2;
    else if (ch <= TRAIL_SURROGATE_MAX)
        return 0; //unexpected trail surrogate!
    else
        return 1;
}


class Utf16Decoder
{
public:
    Utf16Decoder(const Char16* str, size_t len) : it_(str), last_(str + len) {}

    std::optional<CodePoint> getNext()
    {
        if (it_ == last_)
            return {};

        const Char16 ch = *it_++;
        CodePoint cp = ch;
        switch (getUtf16Len(ch))
        {
            case 0: //invalid utf16 character
                cp = REPLACEMENT_CHAR;
                break;
            case 1:
                break;
            case 2:
                decodeTrail(cp);
                break;
        }
        return cp;
    }

private:
    void decodeTrail(CodePoint& cp)
    {
        if (it_ != last_) //trail surrogate expected!
        {
            const Char16 ch = *it_;
            if (TRAIL_SURROGATE <= ch && ch <= TRAIL_SURROGATE_MAX) //trail surrogate expected!
            {
                cp = ((cp - LEAD_SURROGATE) << 10) + (ch - TRAIL_SURROGATE) + 0x10000;
                ++it_;
                return;
            }
        }
        cp = REPLACEMENT_CHAR;
    }

    const Char16* it_;
    const Char16* const last_;
};

//----------------------------------------------------------------------------------------------------------------

template <class Function> inline
void codePointToUtf8(CodePoint cp, Function writeOutput) //"writeOutput" is a unary function taking a Char8
{
    //https://en.wikipedia.org/wiki/UTF-8
    //assert(cp < LEAD_SURROGATE || TRAIL_SURROGATE_MAX < cp); //code points [0xd800, 0xdfff] are reserved for UTF-16 and *should* not be encoded in UTF-8

    if (cp < 0x80)
        writeOutput(static_cast<Char8>(cp));
    else if (cp < 0x800)
    {
        writeOutput(static_cast<Char8>((cp >> 6  ) | 0xc0));
        writeOutput(static_cast<Char8>((cp & 0x3f) | 0x80));
    }
    else if (cp < 0x10000)
    {
        writeOutput(static_cast<Char8>( (cp >> 12       ) | 0xe0));
        writeOutput(static_cast<Char8>(((cp >> 6) & 0x3f) | 0x80));
        writeOutput(static_cast<Char8>( (cp       & 0x3f) | 0x80));
    }
    else if (cp <= CODE_POINT_MAX)
    {
        writeOutput(static_cast<Char8>( (cp >> 18        ) | 0xf0));
        writeOutput(static_cast<Char8>(((cp >> 12) & 0x3f) | 0x80));
        writeOutput(static_cast<Char8>(((cp >> 6)  & 0x3f) | 0x80));
        writeOutput(static_cast<Char8>( (cp        & 0x3f) | 0x80));
    }
    else //invalid code point
        codePointToUtf8(REPLACEMENT_CHAR, writeOutput); //resolves to 3-byte utf8
}


inline
size_t getUtf8Len(Char8 ch) //ch must be first code unit! returns 0 on error!
{
    if (ch < 0x80)
        return 1;
    if (ch >> 5 == 0x6)
        return 2;
    if (ch >> 4 == 0xe)
        return 3;
    if (ch >> 3 == 0x1e)
        return 4;
    return 0; //invalid begin of UTF8 encoding
}


class Utf8Decoder
{
public:
    Utf8Decoder(const Char8* str, size_t len) : it_(str), last_(str + len) {}

    std::optional<CodePoint> getNext()
    {
        if (it_ == last_)
            return std::nullopt;

        const Char8 ch = *it_++;
        CodePoint cp = ch;
        switch (getUtf8Len(ch))
        {
            case 0: //invalid utf8 character
                cp = REPLACEMENT_CHAR;
                break;
            case 1:
                break;
            case 2:
                cp &= 0x1f;
                decodeTrail(cp);
                break;
            case 3:
                cp &= 0xf;
                if (decodeTrail(cp))
                    decodeTrail(cp);
                break;
            case 4:
                cp &= 0x7;
                if (decodeTrail(cp))
                    if (decodeTrail(cp))
                        decodeTrail(cp);
                if (cp > CODE_POINT_MAX) cp = REPLACEMENT_CHAR;
                break;
        }
        return cp;
    }

private:
    bool decodeTrail(CodePoint& cp)
    {
        if (it_ != last_) //trail surrogate expected!
        {
            const Char8 ch = *it_;
            if (ch >> 6 == 0x2) //trail surrogate expected!
            {
                cp = (cp << 6) + (ch & 0x3f);
                ++it_;
                return true;
            }
        }
        cp = REPLACEMENT_CHAR;
        return false;
    }

    const Char8* it_;
    const Char8* const last_;
};

//----------------------------------------------------------------------------------------------------------------

template <class Function> inline void codePointToUtf(CodePoint cp, Function writeOutput, std::integral_constant<int, 1>) { codePointToUtf8 (cp, writeOutput); } //UTF8-char
template <class Function> inline void codePointToUtf(CodePoint cp, Function writeOutput, std::integral_constant<int, 2>) { codePointToUtf16(cp, writeOutput); } //Windows: UTF16-wchar_t
template <class Function> inline void codePointToUtf(CodePoint cp, Function writeOutput, std::integral_constant<int, 4>) { writeOutput(cp); } //other OS: UTF32-wchar_t

template <class CharType, class Function> inline
void codePointToUtf(CodePoint cp, Function writeOutput) //"writeOutput" is a unary function taking a CharType
{
    return codePointToUtf(cp, writeOutput, std::integral_constant<int, sizeof(CharType)>());
}

//----------------------------------------------------------------------------------------------------------------

template <class CharType, int charSize>
class UtfDecoderImpl;


template <class CharType>
class UtfDecoderImpl<CharType, 1> //UTF8-char
{
public:
    UtfDecoderImpl(const CharType* str, size_t len) : decoder_(reinterpret_cast<const Char8*>(str), len) {}
    std::optional<CodePoint> getNext() { return decoder_.getNext(); }
private:
    Utf8Decoder decoder_;
};


template <class CharType>
class UtfDecoderImpl<CharType, 2> //Windows: UTF16-wchar_t
{
public:
    UtfDecoderImpl(const CharType* str, size_t len) : decoder_(reinterpret_cast<const Char16*>(str), len) {}
    std::optional<CodePoint> getNext() { return decoder_.getNext(); }
private:
    Utf16Decoder decoder_;
};


template <class CharType>
class UtfDecoderImpl<CharType, 4> //other OS: UTF32-wchar_t
{
public:
    UtfDecoderImpl(const CharType* str, size_t len) : it_(reinterpret_cast<const CodePoint*>(str)), last_(it_ + len) {}
    std::optional<CodePoint> getNext()
    {
        if (it_ == last_)
            return {};
        return *it_++;
    }
private:
    const CodePoint* it_;
    const CodePoint* last_;
};


template <class CharType>
using UtfDecoder = UtfDecoderImpl<CharType, sizeof(CharType)>;
}

//-------------------------------------------------------------------------------------------

template <class UtfString> inline
bool isValidUtf(const UtfString& str)
{
    using namespace impl;

    UtfDecoder<GetCharTypeT<UtfString>> decoder(strBegin(str), strLength(str));
    while (const std::optional<CodePoint> cp = decoder.getNext())
        if (*cp == REPLACEMENT_CHAR)
            return false;

    return true;
}


template <class UtfString> inline
size_t unicodeLength(const UtfString& str) //return number of code points (+ correctly handle broken UTF encoding)
{
    size_t uniLen = 0;
    impl::UtfDecoder<GetCharTypeT<UtfString>> decoder(strBegin(str), strLength(str));
    while (decoder.getNext())
        ++uniLen;
    return uniLen;
}


template <class UtfString> inline
UtfString getUnicodeSubstring(const UtfString& str, size_t uniPosFirst, size_t uniPosLast) //return position of unicode char in UTF-encoded string
{
    assert(uniPosFirst <= uniPosLast && uniPosLast <= unicodeLength(str));
    using namespace impl;
    using CharType = GetCharTypeT<UtfString>;
    UtfString output;
    if (uniPosFirst >= uniPosLast) //optimize for empty range
        return output;

    UtfDecoder<CharType> decoder(strBegin(str), strLength(str));
    for (size_t uniPos = 0; std::optional<CodePoint> cp = decoder.getNext(); ++uniPos) //[!] declaration in condition part of the for-loop
        if (uniPosFirst <= uniPos)
        {
            if (uniPos >= uniPosLast)
                break;
            codePointToUtf<CharType>(*cp, [&](CharType c) { output += c; });
        }
    return output;
}

//-------------------------------------------------------------------------------------------

namespace impl
{
template <class TargetString, class SourceString> inline
TargetString utfTo(const SourceString& str, std::false_type)
{
    using CharSrc = GetCharTypeT<SourceString>;
    using CharTrg = GetCharTypeT<TargetString>;
    static_assert(sizeof(CharSrc) != sizeof(CharTrg));

    TargetString output;

    UtfDecoder<CharSrc> decoder(strBegin(str), strLength(str));
    while (const std::optional<CodePoint> cp = decoder.getNext())
        codePointToUtf<CharTrg>(*cp, [&](CharTrg c) { output += c; });

    return output;
}


template <class TargetString, class SourceString> inline
TargetString utfTo(const SourceString& str, std::true_type) { return copyStringTo<TargetString>(str); }
}


template <class TargetString, class SourceString> inline
TargetString utfTo(const SourceString& str)
{
    return impl::utfTo<TargetString>(str, std::bool_constant<sizeof(GetCharTypeT<SourceString>) == sizeof(GetCharTypeT<TargetString>)>());
}
}

#endif //UTF_H_01832479146991573473545

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef UTF_H_01832479146991573473545
#define UTF_H_01832479146991573473545

#include "string_tools.h" //copyStringTo


namespace zen
{
//convert all(!) char- and wchar_t-based "string-like" objects applying UTF conversions (but only if necessary!)
template <class TargetString, class SourceString>
TargetString utfTo(const SourceString& str);

constexpr std::string_view BYTE_ORDER_MARK_UTF8 = "\xEF\xBB\xBF";

template <class UtfString>
bool isValidUtf(const UtfString& str); //check for UTF-8 encoding errors

//access unicode characters in UTF-encoded string (char- or wchar_t-based)
template <class UtfString>
size_t unicodeLength(const UtfString& str); //return number of code points for UTF-encoded string

template <class UtfStringOut, class UtfStringIn>
UtfStringOut getUnicodeSubstring(const UtfStringIn& str, size_t uniPosFirst, size_t uniPosLast);









//----------------------- implementation ----------------------------------
namespace impl
{
using CodePoint = uint32_t;
using Char16    = uint16_t;
using Char8     = uint8_t;

const CodePoint LEAD_SURROGATE      = 0xd800; //1101 1000 0000 0000    LEAD_SURROGATE_MAX = TRAIL_SURROGATE - 1
const CodePoint TRAIL_SURROGATE     = 0xdc00; //1101 1100 0000 0000
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
        writeOutput(static_cast<Char16>(REPLACEMENT_CHAR));
    else if (cp <= 0xffff)
        writeOutput(static_cast<Char16>(cp));
    else if (cp <= CODE_POINT_MAX)
    {
        cp -= 0x10000;
        writeOutput(static_cast<Char16>( LEAD_SURROGATE + (cp >> 10)));
        writeOutput(static_cast<Char16>(TRAIL_SURROGATE + (cp & 0b11'1111'1111)));
    }
    else //invalid code point
        writeOutput(static_cast<Char16>(REPLACEMENT_CHAR));
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

        if (ch < LEAD_SURROGATE || ch > TRAIL_SURROGATE_MAX) //single Char16, no surrogates
            ;
        else if (ch < TRAIL_SURROGATE) //two Char16: lead and trail surrogates
            decodeTrail(cp); //no range check needed: cp is inside [U+010000, U+10FFFF] by construction
        else //unexpected trail surrogate
            cp = REPLACEMENT_CHAR;

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
    /* https://en.wikipedia.org/wiki/UTF-8
      "high and low surrogate halves used by UTF-16 (U+D800 through U+DFFF) and
       code points not encodable by UTF-16 (those after U+10FFFF) [...] must be treated as an invalid byte sequence" */

    if (cp <= 0b111'1111)
        writeOutput(static_cast<Char8>(cp));
    else if (cp <= 0b0111'1111'1111)
    {
        writeOutput(static_cast<Char8>((cp >> 6)        | 0b1100'0000)); //110x xxxx
        writeOutput(static_cast<Char8>((cp & 0b11'1111) | 0b1000'0000)); //10xx xxxx
    }
    else if (cp <= 0b1111'1111'1111'1111)
    {
        if (LEAD_SURROGATE <= cp && cp <= TRAIL_SURROGATE_MAX) //[0xd800, 0xdfff]
            codePointToUtf8(REPLACEMENT_CHAR, writeOutput);
        else
        {
            writeOutput(static_cast<Char8>( (cp >> 12)             | 0b1110'0000)); //1110 xxxx
            writeOutput(static_cast<Char8>(((cp >> 6) & 0b11'1111) | 0b1000'0000)); //10xx xxxx
            writeOutput(static_cast<Char8>( (cp       & 0b11'1111) | 0b1000'0000)); //10xx xxxx
        }
    }
    else if (cp <= CODE_POINT_MAX)
    {
        writeOutput(static_cast<Char8>( (cp >> 18)              | 0b1111'0000)); //1111 0xxx
        writeOutput(static_cast<Char8>(((cp >> 12) & 0b11'1111) | 0b1000'0000)); //10xx xxxx
        writeOutput(static_cast<Char8>(((cp >> 6)  & 0b11'1111) | 0b1000'0000)); //10xx xxxx
        writeOutput(static_cast<Char8>( (cp        & 0b11'1111) | 0b1000'0000)); //10xx xxxx
    }
    else //invalid code point
        codePointToUtf8(REPLACEMENT_CHAR, writeOutput); //resolves to 3-byte UTF8
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

        if (ch < 0x80) //1 byte
            ;
        else if (ch >> 5 == 0b110) //2 bytes
        {
            cp &= 0b1'1111;
            if (decodeTrail(cp))
                if (cp <= 0b111'1111) //overlong encoding: "correct encoding of a code point uses only the minimum number of bytes required"
                    cp = REPLACEMENT_CHAR;
        }
        else if (ch >> 4 == 0b1110) //3 bytes
        {
            cp &= 0b1111;
            if (decodeTrail(cp) && decodeTrail(cp))
                if (cp <= 0b0111'1111'1111 ||
                    (LEAD_SURROGATE <= cp && cp <= TRAIL_SURROGATE_MAX)) //[0xd800, 0xdfff] are invalid code points
                    cp = REPLACEMENT_CHAR;
        }
        else if (ch >> 3 == 0b11110) //4 bytes
        {
            cp &= 0b111;
            if (decodeTrail(cp) && decodeTrail(cp) && decodeTrail(cp))
                if (cp <= 0b1111'1111'1111'1111 || cp > CODE_POINT_MAX)
                    cp = REPLACEMENT_CHAR;
        }
        else //invalid begin of UTF8 encoding
            cp = REPLACEMENT_CHAR;

        return cp;
    }

private:
    bool decodeTrail(CodePoint& cp)
    {
        if (it_ != last_) //trail surrogate expected!
        {
            const Char8 ch = *it_;
            if (ch >> 6 == 0b10) //trail surrogate expected!
            {
                cp = (cp << 6) + (ch & 0b11'1111);
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

template <class Function> inline void codePointToUtfImpl(CodePoint cp, Function writeOutput, std::integral_constant<int, 1>) { codePointToUtf8 (cp, writeOutput); } //UTF8-char
template <class Function> inline void codePointToUtfImpl(CodePoint cp, Function writeOutput, std::integral_constant<int, 2>) { codePointToUtf16(cp, writeOutput); } //Windows: UTF16-wchar_t
template <class Function> inline void codePointToUtfImpl(CodePoint cp, Function writeOutput, std::integral_constant<int, 4>) { writeOutput(cp); } //other OS: UTF32-wchar_t

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
}


template <class CharType>
using UtfDecoder = impl::UtfDecoderImpl<CharType, sizeof(CharType)>;


template <class CharType, class Function> inline
void codePointToUtf(impl::CodePoint cp, Function writeOutput) //"writeOutput" is a unary function taking a CharType
{
    return impl::codePointToUtfImpl(cp, writeOutput, std::integral_constant<int, sizeof(CharType)>());
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
    UtfDecoder<GetCharTypeT<UtfString>> decoder(strBegin(str), strLength(str));
    while (decoder.getNext())
        ++uniLen;
    return uniLen;
}


template <class UtfStringOut, class UtfStringIn> inline
UtfStringOut getUnicodeSubstring(const UtfStringIn& str, size_t uniPosFirst, size_t uniPosLast) //return position of unicode char in UTF-encoded string
{
    assert(uniPosFirst <= uniPosLast && uniPosLast <= unicodeLength(str));
    using namespace impl;
    using CharType = GetCharTypeT<UtfStringIn>;

    UtfStringOut output;
    assert(uniPosFirst <= uniPosLast);
    if (uniPosFirst >= uniPosLast) //optimize for empty range
        return output;

    UtfDecoder<CharType> decoder(strBegin(str), strLength(str));
    for (size_t uniPos = 0; std::optional<CodePoint> cp = decoder.getNext(); ++uniPos) //[!] declaration in condition part of the for-loop
        if (uniPos >= uniPosFirst)
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
TargetString utfTo(const SourceString& str, std::true_type) { return copyStringTo<TargetString>(str); }


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
}


template <class TargetString, class SourceString> inline
TargetString utfTo(const SourceString& str)
{
    return impl::utfTo<TargetString>(str, std::bool_constant<sizeof(GetCharTypeT<SourceString>) == sizeof(GetCharTypeT<TargetString>)>());
}
}

#endif //UTF_H_01832479146991573473545

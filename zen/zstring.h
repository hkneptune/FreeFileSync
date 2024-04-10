// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ZSTRING_H_73425873425789
#define ZSTRING_H_73425873425789

#include <stdexcept> //not used by this header, but the "rest of the world" needs it!
#include "utf.h"     //
#include "string_base.h"


    using Zchar = char;
    #define Zstr(x) x


//"The reason for all the fuss above" - Loki/SmartPtr
//a high-performance string for interfacing with native OS APIs in multithreaded contexts
using Zstring = zen::Zbase<Zchar>;

using ZstringView = std::basic_string_view<Zchar>;

//for special UI-contexts: guaranteed exponential growth + ref-counting + COW + no SSO overhead
using Zstringc = zen::Zbase<char>;
//using Zstringw = zen::Zbase<wchar_t>;


enum class UnicodeNormalForm
{
    nfc, //precomposed
    nfd, //decomposed
    native = nfc,
};
Zstring getUnicodeNormalForm(const Zstring& str, UnicodeNormalForm form = UnicodeNormalForm::native);
/* "In fact, Unicode declares that there is an equivalence relationship between decomposed and composed sequences,
    and conformant software should not treat canonically equivalent sequences, whether composed or decomposed or something in between, as different."
    https://www.win.tue.nl/~aeb/linux/uc/nfc_vs_nfd.html             */

/* Caveat: don't expect input/output string sizes to match:
    - different UTF-8 encoding length of upper-case chars
    - different number of upper case chars (e.g. ß => "SS" on macOS)
    - output is Unicode-normalized                                         */
Zstring getUpperCase(const Zstring& str);

//------------------------------------------------------------------------------------------
struct ZstringNorm //use as STL container key: better than repeated Unicode normalizations during std::map<>::find()
{
    /*explicit*/ ZstringNorm(const Zstring& str) : normStr(getUnicodeNormalForm(str)) {}
    Zstring normStr;

    std::strong_ordering operator<=>(const ZstringNorm&) const = default;
};
template<> struct std::hash<ZstringNorm> { size_t operator()(const ZstringNorm& str) const { return std::hash<Zstring>()(str.normStr); } };

//struct LessUnicodeNormal { bool operator()(const Zstring& lhs, const Zstring& rhs) const { return getUnicodeNormalForm(lhs) < getUnicodeNormalForm(rhs); } };

//------------------------------------------------------------------------------------------
struct ZstringNoCase //use as STL container key: better than repeated upper-case conversions during std::map<>::find()
{
    /*explicit*/ ZstringNoCase(const Zstring& str) : upperCase(getUpperCase(str)) {}
    Zstring upperCase;

    std::strong_ordering operator<=>(const ZstringNoCase&) const = default;
};
template<> struct std::hash<ZstringNoCase> { size_t operator()(const ZstringNoCase& str) const { return std::hash<Zstring>()(str.upperCase); } };


std::weak_ordering compareNoCase(const Zstring& lhs, const Zstring& rhs);

bool equalNoCase(const Zstring& lhs, const Zstring& rhs);

//------------------------------------------------------------------------------------------
std::weak_ordering compareNatural(const Zstring& lhs, const Zstring& rhs);

struct LessNaturalSort { bool operator()(const Zstring& lhs, const Zstring& rhs) const { return compareNatural(lhs, rhs) < 0; } };


//------------------------------------------------------------------------------------------
//common Unicode characters
const wchar_t EN_DASH = L'\u2013'; //–
const wchar_t EM_DASH = L'\u2014'; //—
    const wchar_t* const SPACED_DASH = L" \u2014 "; //using 'EM DASH'
const wchar_t* const ELLIPSIS = L"\u2026"; //…
const wchar_t MULT_SIGN = L'\u00D7'; //×
const wchar_t NOBREAK_SPACE = L'\u00A0';
const wchar_t ZERO_WIDTH_SPACE = L'\u200B';

const wchar_t EN_SPACE = L'\u2002';

const wchar_t LTR_MARK = L'\u200E'; //UTF-8: E2 80 8E
const wchar_t RTL_MARK = L'\u200F'; //UTF-8: E2 80 8F https://www.w3.org/International/questions/qa-bidi-unicode-controls
//const wchar_t BIDI_DIR_ISOLATE_RTL    = L'\u2067'; //=> not working on Win 10
//const wchar_t BIDI_POP_DIR_ISOLATE    = L'\u2069'; //=> not working on Win 10
//const wchar_t BIDI_DIR_EMBEDDING_RTL  = L'\u202B'; //=> not working on Win 10
//const wchar_t BIDI_POP_DIR_FORMATTING = L'\u202C'; //=> not working on Win 10

const wchar_t RIGHT_ARROW_CURV_DOWN = L'\u2935'; //Right Arrow Curving Down: ⤵
//Windows bug: rendered differently depending on presence of e.g. LTR_MARK!
//there is no "Left Arrow Curving Down" => WTF => better than nothing:
const wchar_t LEFT_ARROW_ANTICLOCK = L'\u2B8F'; //Anticlockwise Triangle-Headed Top U-Shaped Arrow: ⮏

const wchar_t* const TAB_SPACE = L"    "; //4: the only sensible space count for tabs

#endif //ZSTRING_H_73425873425789

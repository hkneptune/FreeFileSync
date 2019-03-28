// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ZSTRING_H_73425873425789
#define ZSTRING_H_73425873425789

#include "string_base.h"


    using Zchar = char;
    #define Zstr(x) x
    const Zchar FILE_NAME_SEPARATOR = '/';


//"The reason for all the fuss above" - Loki/SmartPtr
//a high-performance string for interfacing with native OS APIs in multithreaded contexts
using Zstring = zen::Zbase<Zchar>;

//for special UI-contexts: guaranteed exponential growth + ref-counting
using Zstringw = zen::Zbase<wchar_t>;


//Caveat: don't expect input/output string sizes to match:
// - different UTF-8 encoding length of upper-case chars
// - different number of upper case chars (e.g. "ß" => "SS" on macOS)
// - output is Unicode-normalized
Zstring makeUpperCopy(const Zstring& str);

//Windows, Linux: precomposed
//macOS: decomposed
Zstring getUnicodeNormalForm(const Zstring& str);

Zstring replaceCpyAsciiNoCase(const Zstring& str, const Zstring& oldTerm, const Zstring& newTerm);

//------------------------------------------------------------------------------------------
//inline
//int compareNoCase(const Zstring& lhs, const Zstring& rhs)
//{
//    return zen::compareString(makeUpperCopy(lhs), makeUpperCopy(rhs));
//    //avoid eager optimization bugs: e.g. "if (isAsciiString()) compareAsciiNoCase()" might model a different order!
//}

inline bool equalNoCase(const Zstring& lhs, const Zstring& rhs) { return makeUpperCopy(lhs) == makeUpperCopy(rhs); }

struct ZstringNoCase //use as STL container key: avoid needless upper-case conversions during std::map<>::find()
{
        ZstringNoCase(const Zstring& str) : upperCase(makeUpperCopy(str)) {}
        Zstring upperCase;
};
inline bool operator<(const ZstringNoCase& lhs, const ZstringNoCase& rhs) { return lhs.upperCase < rhs.upperCase; } 

//struct LessNoCase { bool operator()(const Zstring& lhs, const Zstring& rhs) const { return compareNoCase(lhs, rhs) < 0; } };

//------------------------------------------------------------------------------------------

//Compare *local* file paths:
//  Windows: igore case
//  Linux:   byte-wise comparison
//  macOS:   igore case + Unicode normalization forms
int compareLocalPath(const Zstring& lhs, const Zstring& rhs);

inline bool equalLocalPath(const Zstring& lhs, const Zstring& rhs) { return compareLocalPath(lhs, rhs) == 0;  }

struct LessLocalPath { bool operator()(const Zstring& lhs, const Zstring& rhs) const { return compareLocalPath(lhs, rhs) < 0;  } };

//------------------------------------------------------------------------------------------
int compareNatural(const Zstring& lhs, const Zstring& rhs);

struct LessNaturalSort { bool operator()(const Zstring& lhs, const Zstring rhs) const { return compareNatural(lhs, rhs) < 0; } };
//------------------------------------------------------------------------------------------

warn_static("get rid:")
inline int compareFilePath(const Zstring& lhs, const Zstring& rhs) { return compareLocalPath(lhs, rhs); }

inline bool equalFilePath(const Zstring& lhs, const Zstring& rhs) { return compareLocalPath(lhs, rhs) == 0; }

struct LessFilePath { bool operator()(const Zstring& lhs, const Zstring& rhs) const { return compareLocalPath(lhs, rhs) < 0; } };
//------------------------------------------------------------------------------------------



inline
Zstring appendSeparator(Zstring path) //support rvalue references!
{
    if (!zen::endsWith(path, FILE_NAME_SEPARATOR))
        path += FILE_NAME_SEPARATOR;
    return path; //returning a by-value parameter => RVO if possible, r-value otherwise!
}


inline
Zstring getFileExtension(const Zstring& filePath)
{
    //const Zstring fileName = afterLast(filePath, FILE_NAME_SEPARATOR, zen::IF_MISSING_RETURN_ALL);
    //return afterLast(fileName, Zstr('.'), zen::IF_MISSING_RETURN_NONE);

    auto it = zen::find_last(filePath.begin(), filePath.end(), FILE_NAME_SEPARATOR);
    if (it == filePath.end())
        it = filePath.begin();
    else
        ++it;

    auto it2 = zen::find_last(it, filePath.end(), Zstr('.'));
    if (it2 != filePath.end())
        ++it2;

    return Zstring(it2, filePath.end());
}


//common unicode characters
const wchar_t EM_DASH = L'\u2014';
const wchar_t EN_DASH = L'\u2013';
const wchar_t* const SPACED_DASH = L" \u2013 "; //using 'EN DASH'
const wchar_t LTR_MARK = L'\u200E'; //UTF-8: E2 80 8E
const wchar_t RTL_MARK = L'\u200F'; //UTF-8: E2 80 8F
const wchar_t ELLIPSIS = L'\u2026'; //"..."
const wchar_t MULT_SIGN = L'\u00D7'; //fancy "x"
//const wchar_t NOBREAK_SPACE = L'\u00A0';



//---------------------------------------------------------------------------
//ZEN macro consistency checks:


#endif //ZSTRING_H_73425873425789

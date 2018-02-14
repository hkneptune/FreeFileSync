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


//Compare filepaths: Windows/OS X does NOT distinguish between upper/lower-case, while Linux DOES
struct CmpFilePath
{
    int operator()(const Zchar* lhs, size_t lhsLen, const Zchar* rhs, size_t rhsLen) const;
};

struct CmpNaturalSort
{
    int operator()(const Zchar* lhs, size_t lhsLen, const Zchar* rhs, size_t rhsLen) const;
};


struct LessFilePath
{
    template <class S> //don't support heterogenous input! => use as container predicate only!
    bool operator()(const S& lhs, const S& rhs) const { using namespace zen; return CmpFilePath()(strBegin(lhs), strLength(lhs), strBegin(rhs), strLength(rhs)) < 0; }
};

struct LessNaturalSort
{
    template <class S> //don't support heterogenous input! => use as container predicate only!
    bool operator()(const S& lhs, const S& rhs) const { using namespace zen; return CmpNaturalSort()(strBegin(lhs), strLength(lhs), strBegin(rhs), strLength(rhs)) < 0; }
};


template <class S>
S makeUpperCopy(S str);


template <class S, class T> inline
bool equalFilePath(const S& lhs, const T& rhs) { using namespace zen; return strEqual(lhs, rhs, CmpFilePath());  }


inline
Zstring appendSeparator(Zstring path) //support rvalue references!
{
    return zen::endsWith(path, FILE_NAME_SEPARATOR) ? path : (path += FILE_NAME_SEPARATOR); //returning a by-value parameter implicitly converts to r-value!
}


inline
Zstring getFileExtension(const Zstring& filePath)
{
    const Zstring shortName = afterLast(filePath, FILE_NAME_SEPARATOR, zen::IF_MISSING_RETURN_ALL);
    return afterLast(shortName, Zchar('.'), zen::IF_MISSING_RETURN_NONE);
}


template <class S, class T, class U>
S ciReplaceCpy(const S& str, const T& oldTerm, const U& newTerm);



//common unicode sequences
const wchar_t EM_DASH = L'\u2014';
const wchar_t EN_DASH = L'\u2013';
const wchar_t* const SPACED_DASH = L" \u2013 "; //using 'EN DASH'
const wchar_t LTR_MARK = L'\u200E'; //UTF-8: E2 80 8E
const wchar_t RTL_MARK = L'\u200F'; //UTF-8: E2 80 8F
const wchar_t ELLIPSIS = L'\u2026'; //"..."






//################################# inline implementation ########################################
inline
void makeUpperInPlace(wchar_t* str, size_t strLen)
{
    std::for_each(str, str + strLen, [](wchar_t& c) { c = std::towupper(c); }); //locale-dependent!
}


inline
void makeUpperInPlace(char* str, size_t strLen)
{
    std::for_each(str, str + strLen, [](char& c) { c = std::toupper(static_cast<unsigned char>(c)); }); //locale-dependent!
    //result of toupper() is an unsigned char mapped to int range: the char representation is in the last 8 bits and we need not care about signedness!
    //this should work for UTF-8, too: all chars >= 128 are mapped upon themselves!
}


template <class S> inline
S makeUpperCopy(S str)
{
    const size_t len = str.length(); //we assert S is a string type!
    if (len > 0)
        makeUpperInPlace(&*str.begin(), len);

    return std::move(str); //"str" is an l-value parameter => no copy elision!
}


inline
int CmpFilePath::operator()(const Zchar* lhs, size_t lhsLen, const Zchar* rhs, size_t rhsLen) const
{
    assert(std::find(lhs, lhs + lhsLen, 0) == lhs + lhsLen); //don't expect embedded nulls!
    assert(std::find(rhs, rhs + rhsLen, 0) == rhs + rhsLen); //

    const int rv = std::strncmp(lhs, rhs, std::min(lhsLen, rhsLen));
    if (rv != 0)
        return rv;
    return static_cast<int>(lhsLen) - static_cast<int>(rhsLen);
}


template <class S, class T, class U> inline
S ciReplaceCpy(const S& str, const T& oldTerm, const U& newTerm)
{
    using namespace zen;
    static_assert(IsSameType<typename GetCharType<S>::Type, typename GetCharType<T>::Type>::value, "");
    static_assert(IsSameType<typename GetCharType<T>::Type, typename GetCharType<U>::Type>::value, "");
    const size_t oldLen = strLength(oldTerm);
    if (oldLen == 0)
        return str;

    const S strU = makeUpperCopy(str); //S required to be a string class
    const S oldU = makeUpperCopy<S>(oldTerm); //[!] T not required to be a string class
    assert(strLength(strU) == strLength(str    ));
    assert(strLength(oldU) == strLength(oldTerm));

    const auto* const newBegin = strBegin(newTerm);
    const auto* const newEnd   = newBegin + strLength(newTerm);

    S output;

    for (size_t pos = 0;;)
    {
        const auto itFound = std::search(strU.begin() + pos, strU.end(),
                                         oldU.begin(), oldU.end());
        if (itFound == strU.end() && pos == 0)
            return str; //optimize "oldTerm not found": return ref-counted copy

        impl::stringAppend(output, str.begin() + pos, str.begin() + (itFound - strU.begin()));
        if (itFound == strU.end())
            return output;

        impl::stringAppend(output, newBegin, newEnd);
        pos = (itFound - strU.begin()) + oldLen;
    }
}

    int cmpStringNaturalLinux(const char* lhs, size_t lhsLen, const char* rhs, size_t rhsLen);

//---------------------------------------------------------------------------
//ZEN macro consistency checks:


#endif //ZSTRING_H_73425873425789

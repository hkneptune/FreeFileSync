// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STRING_TOOLS_H_213458973046
#define STRING_TOOLS_H_213458973046

#include <cctype>  //isspace
#include <cwctype> //iswspace
#include <cstdio>  //sprintf
#include <cwchar>  //swprintf
#include <algorithm>
#include <cassert>
#include <vector>
#include <sstream>
#include "stl_tools.h"
#include "string_traits.h"


//enhance arbitray string class with useful non-member functions:
namespace zen
{
template <class Char> bool isWhiteSpace(Char c);
template <class Char> bool isDigit     (Char c); //not exactly the same as "std::isdigit" -> we consider '0'-'9' only!
template <class Char> bool isHexDigit  (Char c);
template <class Char> bool isAsciiAlpha(Char c);
template <class Char> Char asciiToLower(Char c);
template <class Char> Char asciiToUpper(Char c);

//case-sensitive comparison (compile-time correctness:  use different number of arguments as STL comparison predicates!)
struct CmpBinary { template <class Char> int operator()(const Char* lhs, size_t lhsLen, const Char* rhs, size_t rhsLen) const; };

//basic case-insensitive comparison (considering A-Z only!)
struct CmpAsciiNoCase { template <class Char> int operator()(const Char* lhs, size_t lhsLen, const Char* rhs, size_t rhsLen) const; };

struct LessAsciiNoCase
{
    template <class S> //don't support heterogenous input! => use as container predicate only!
    bool operator()(const S& lhs, const S& rhs) const { return CmpAsciiNoCase()(strBegin(lhs), strLength(lhs), strBegin(rhs), strLength(rhs)) < 0; }
};

//both S and T can be strings or char/wchar_t arrays or simple char/wchar_t
template <class S, class T> bool contains(const S& str, const T& term);

template <class S, class T>                 bool startsWith(const S& str, const T& prefix);
template <class S, class T, class Function> bool startsWith(const S& str, const T& prefix,  Function cmpStringFun);

template <class S, class T>                 bool endsWith  (const S& str, const T& postfix);
template <class S, class T, class Function> bool endsWith  (const S& str, const T& postfix, Function cmpStringFun);

template <class S, class T>                 bool strEqual(const S& lhs, const T& rhs);
template <class S, class T, class Function> bool strEqual(const S& lhs, const T& rhs, Function cmpStringFun);

enum FailureReturnVal
{
    IF_MISSING_RETURN_ALL,
    IF_MISSING_RETURN_NONE
};

template <class S, class T> S afterLast  (const S& str, const T& term, FailureReturnVal rv);
template <class S, class T> S beforeLast (const S& str, const T& term, FailureReturnVal rv);
template <class S, class T> S afterFirst (const S& str, const T& term, FailureReturnVal rv);
template <class S, class T> S beforeFirst(const S& str, const T& term, FailureReturnVal rv);

enum class SplitType
{
    ALLOW_EMPTY,
    SKIP_EMPTY
};
template <class S, class T> std::vector<S> split(const S& str, const T& delimiter, SplitType st);

template <class S>                 S    trimCpy(S  str, bool fromLeft = true, bool fromRight = true);
template <class S>                 void trim   (S& str, bool fromLeft = true, bool fromRight = true);
template <class S, class Function> void trim(S& str, bool fromLeft, bool fromRight, Function trimThisChar);

template <class S, class T, class U> void replace   (      S& str, const T& oldTerm, const U& newTerm, bool replaceAll = true);
template <class S, class T, class U> S    replaceCpy(const S& str, const T& oldTerm, const U& newTerm, bool replaceAll = true);

//high-performance conversion between numbers and strings
template <class S,   class Num> S   numberTo(const Num& number);
template <class Num, class S>   Num stringTo(const S&   str);

std::pair<char, char> hexify  (unsigned char c, bool upperCase = true);
char                  unhexify(char high, char low);

template <class S, class T, class Num> S printNumber(const T& format, const Num& number); //format a single number using std::snprintf()

//string to string conversion: converts string-like type into char-compatible target string class
template <class T, class S> T copyStringTo(S&& str);
















//---------------------- implementation ----------------------
template <> inline
bool isWhiteSpace(char c)
{
    assert(c != 0); //std C++ does not consider 0 as white space
    //caveat 1: std::isspace() takes an int, but expects an unsigned char
    //caveat 2: some parts of UTF-8 chars are erroneously seen as whitespace, e.g. the a0 from "\xec\x8b\xa0" (MSVC)
    return static_cast<unsigned char>(c) < 128 &&
           std::isspace(static_cast<unsigned char>(c)) != 0;
}

template <> inline
bool isWhiteSpace(wchar_t c)
{
    assert(c != 0); //std C++ does not consider 0 as white space
    return std::iswspace(c) != 0;
}


template <class Char> inline
bool isDigit(Char c) //similar to implmenetation of std::isdigit()!
{
    static_assert(IsSameType<Char, char>::value || IsSameType<Char, wchar_t>::value, "");
    return static_cast<Char>('0') <= c && c <= static_cast<Char>('9');
}


template <class Char> inline
bool isHexDigit(Char c)
{
    static_assert(IsSameType<Char, char>::value || IsSameType<Char, wchar_t>::value, "");
    return (static_cast<Char>('0') <= c && c <= static_cast<Char>('9')) ||
           (static_cast<Char>('A') <= c && c <= static_cast<Char>('F')) ||
           (static_cast<Char>('a') <= c && c <= static_cast<Char>('f'));
}


template <class Char> inline
bool isAsciiAlpha(Char c)
{
    static_assert(IsSameType<Char, char>::value || IsSameType<Char, wchar_t>::value, "");
    return (static_cast<Char>('A') <= c && c <= static_cast<Char>('Z')) ||
           (static_cast<Char>('a') <= c && c <= static_cast<Char>('z'));
}


template <class Char> inline
Char asciiToLower(Char c)
{
    if (static_cast<Char>('A') <= c && c <= static_cast<Char>('Z'))
        return static_cast<Char>(c - static_cast<Char>('A') + static_cast<Char>('a'));
    return c;
}


template <class Char> inline
Char asciiToUpper(Char c)
{
    if (static_cast<Char>('a') <= c && c <= static_cast<Char>('z'))
        return static_cast<Char>(c - static_cast<Char>('a') + static_cast<Char>('A'));
    return c;
}


template <class S, class T, class Function> inline
bool startsWith(const S& str, const T& prefix, Function cmpStringFun)
{
    const size_t pfLen = strLength(prefix);
    if (strLength(str) < pfLen)
        return false;

    return cmpStringFun(strBegin(str),    pfLen,
                        strBegin(prefix), pfLen) == 0;
}


template <class S, class T, class Function> inline
bool endsWith(const S& str, const T& postfix, Function cmpStringFun)
{
    const size_t strLen = strLength(str);
    const size_t pfLen  = strLength(postfix);
    if (strLen < pfLen)
        return false;

    return cmpStringFun(strBegin(str) + strLen - pfLen, pfLen,
                        strBegin(postfix), pfLen) == 0;
}


template <class S, class T, class Function> inline
bool strEqual(const S& lhs, const T& rhs, Function cmpStringFun)
{
    return cmpStringFun(strBegin(lhs), strLength(lhs), strBegin(rhs), strLength(rhs)) == 0;
}


template <class S, class T> inline bool startsWith(const S& str, const T& prefix ) { return startsWith(str, prefix,  CmpBinary()); }
template <class S, class T> inline bool endsWith  (const S& str, const T& postfix) { return endsWith  (str, postfix, CmpBinary()); }
template <class S, class T> inline bool strEqual  (const S& lhs, const T& rhs    ) { return strEqual  (lhs, rhs,     CmpBinary()); }


template <class S, class T> inline
bool contains(const S& str, const T& term)
{
    static_assert(IsSameType<typename GetCharType<S>::Type, typename GetCharType<T>::Type>::value, "");
    const size_t strLen  = strLength(str);
    const size_t termLen = strLength(term);
    if (strLen < termLen)
        return false;

    const auto* const strFirst  = strBegin(str);
    const auto* const strLast   = strFirst + strLen;
    const auto* const termFirst = strBegin(term);

    return std::search(strFirst, strLast,
                       termFirst, termFirst + termLen) != strLast;
}


template <class S, class T> inline
S afterLast(const S& str, const T& term, FailureReturnVal rv)
{
    static_assert(IsSameType<typename GetCharType<S>::Type, typename GetCharType<T>::Type>::value, "");
    const size_t termLen = strLength(term);
    assert(termLen > 0);

    const auto* const strFirst  = strBegin(str);
    const auto* const strLast   = strFirst + strLength(str);
    const auto* const termFirst = strBegin(term);

    const auto* it = search_last(strFirst, strLast,
                                 termFirst, termFirst + termLen);
    if (it == strLast)
        return rv == IF_MISSING_RETURN_ALL ? str : S();

    it += termLen;
    return S(it, strLast - it);
}


template <class S, class T> inline
S beforeLast(const S& str, const T& term, FailureReturnVal rv)
{
    static_assert(IsSameType<typename GetCharType<S>::Type, typename GetCharType<T>::Type>::value, "");
    const size_t termLen = strLength(term);
    assert(termLen > 0);

    const auto* const strFirst  = strBegin(str);
    const auto* const strLast   = strFirst + strLength(str);
    const auto* const termFirst = strBegin(term);

    const auto* it = search_last(strFirst, strLast,
                                 termFirst, termFirst + termLen);
    if (it == strLast)
        return rv == IF_MISSING_RETURN_ALL ? str : S();

    return S(strFirst, it - strFirst);
}


template <class S, class T> inline
S afterFirst(const S& str, const T& term, FailureReturnVal rv)
{
    static_assert(IsSameType<typename GetCharType<S>::Type, typename GetCharType<T>::Type>::value, "");
    const size_t termLen = strLength(term);
    assert(termLen > 0);

    const auto* const strFirst  = strBegin(str);
    const auto* const strLast   = strFirst + strLength(str);
    const auto* const termFirst = strBegin(term);

    const auto* it = std::search(strFirst, strLast,
                                 termFirst, termFirst + termLen);
    if (it == strLast)
        return rv == IF_MISSING_RETURN_ALL ? str : S();

    it += termLen;
    return S(it, strLast - it);
}


template <class S, class T> inline
S beforeFirst(const S& str, const T& term, FailureReturnVal rv)
{
    static_assert(IsSameType<typename GetCharType<S>::Type, typename GetCharType<T>::Type>::value, "");
    const size_t termLen = strLength(term);
    assert(termLen > 0);

    const auto* const strFirst  = strBegin(str);
    const auto* const strLast   = strFirst + strLength(str);
    const auto* const termFirst = strBegin(term);

    auto it = std::search(strFirst, strLast,
                          termFirst,  termFirst  + termLen);
    if (it == strLast)
        return rv == IF_MISSING_RETURN_ALL ? str : S();

    return S(strFirst, it - strFirst);
}


template <class S, class T> inline
std::vector<S> split(const S& str, const T& delimiter, SplitType st)
{
    static_assert(IsSameType<typename GetCharType<S>::Type, typename GetCharType<T>::Type>::value, "");
    const size_t delimLen = strLength(delimiter);
    assert(delimLen > 0);
    if (delimLen == 0)
    {
        if (str.empty() && st == SplitType::SKIP_EMPTY)
            return {};
        return { str };
    }

    const auto* const delimFirst = strBegin(delimiter);
    const auto* const delimLast  = delimFirst + delimLen;

    const auto* blockStart    = strBegin(str);
    const auto* const strLast = blockStart + strLength(str);

    std::vector<S> output;
    for (;;)
    {
        const auto* const blockEnd = std::search(blockStart, strLast,
                                                 delimFirst, delimLast);
        if (blockStart != blockEnd || st == SplitType::ALLOW_EMPTY)
            output.emplace_back(blockStart, blockEnd - blockStart);

        if (blockEnd == strLast)
            return output;
        blockStart = blockEnd + delimLen;
    }
}


namespace impl
{
ZEN_INIT_DETECT_MEMBER(append);

//either call operator+=(S(str, len)) or append(str, len)
template <class S, class InputIterator> inline
typename EnableIf<HasMember_append<S>::value>::Type stringAppend(S& str, InputIterator first, InputIterator last) { str.append(first, last);  }

template <class S, class InputIterator> inline
typename EnableIf<!HasMember_append<S>::value>::Type stringAppend(S& str, InputIterator first, InputIterator last) { str += S(first, last); }
}


template <class S, class T, class U> inline
S replaceCpy(const S& str, const T& oldTerm, const U& newTerm, bool replaceAll)
{
    static_assert(IsSameType<typename GetCharType<S>::Type, typename GetCharType<T>::Type>::value, "");
    static_assert(IsSameType<typename GetCharType<T>::Type, typename GetCharType<U>::Type>::value, "");
    const size_t oldLen = strLength(oldTerm);
    if (oldLen == 0)
        return str;

    const auto* const oldBegin = strBegin(oldTerm);
    const auto* const oldEnd   = oldBegin + oldLen;

    const auto* const newBegin = strBegin(newTerm);
    const auto* const newEnd   = newBegin + strLength(newTerm);

    S output;

    for (auto it = str.begin();;)
    {
        const auto itFound = std::search(it, str.end(),
                                         oldBegin, oldEnd);
        if (itFound == str.end() && it == str.begin())
            return str; //optimize "oldTerm not found": return ref-counted copy

        impl::stringAppend(output, it, itFound);
        if (itFound == str.end())
            return output;

        impl::stringAppend(output, newBegin, newEnd);
        it = itFound + oldLen;

        if (!replaceAll)
        {
            impl::stringAppend(output, it, str.end());
            return output;
        }
    }
}


template <class S, class T, class U> inline
void replace(S& str, const T& oldTerm, const U& newTerm, bool replaceAll)
{
    str = replaceCpy(str, oldTerm, newTerm, replaceAll);
}


template <class S, class Function> inline
void trim(S& str, bool fromLeft, bool fromRight, Function trimThisChar)
{
    assert(fromLeft || fromRight);

    const auto* const oldBegin = strBegin(str);
    const auto*       newBegin = oldBegin;
    const auto*       newEnd   = oldBegin + strLength(str);

    if (fromRight)
        while (newBegin != newEnd && trimThisChar(newEnd[-1]))
            --newEnd;

    if (fromLeft)
        while (newBegin != newEnd && trimThisChar(*newBegin))
            ++newBegin;

    if (newBegin != oldBegin)
        str = S(newBegin, newEnd - newBegin); //minor inefficiency: in case "str" is not shared, we could save an allocation and do a memory move only
    else
        str.resize(newEnd - newBegin);
}


template <class S> inline
void trim(S& str, bool fromLeft, bool fromRight)
{
    using CharType = typename GetCharType<S>::Type;
    trim(str, fromLeft, fromRight, [](CharType c) { return isWhiteSpace(c); });
}


template <class S> inline
S trimCpy(S str, bool fromLeft, bool fromRight)
{
    //implementing trimCpy() in terms of trim(), instead of the other way round, avoids memory allocations when trimming from right!
    trim(str, fromLeft, fromRight);
    return std::move(str); //"str" is an l-value parameter => no copy elision!
}


namespace impl
{
template <class S, class T>
struct CopyStringToString
{
    T copy(const S& src) const { return T(strBegin(src), strLength(src)); }
};

template <class T>
struct CopyStringToString<T, T> //perf: we don't need a deep copy if string types match
{
    template <class S>
    T copy(S&& str) const { return std::forward<S>(str); }
};

inline int strcmpWithNulls(const char*    ptr1, const char*    ptr2, size_t num) { return std::memcmp (ptr1, ptr2, num); }
inline int strcmpWithNulls(const wchar_t* ptr1, const wchar_t* ptr2, size_t num) { return std::wmemcmp(ptr1, ptr2, num); }
}

template <class T, class S> inline
T copyStringTo(S&& str) { return impl::CopyStringToString<std::decay_t<S>, T>().copy(std::forward<S>(str)); }


template <class Char> inline
int CmpBinary::operator()(const Char* lhs, size_t lhsLen, const Char* rhs, size_t rhsLen) const
{
    //support embedded 0, unlike strncmp/wcsncmp!
    const int rv = impl::strcmpWithNulls(lhs, rhs, std::min(lhsLen, rhsLen));
    if (rv != 0)
        return rv;
    return static_cast<int>(lhsLen) - static_cast<int>(rhsLen);
}


template <class Char> inline
int CmpAsciiNoCase::operator()(const Char* lhs, size_t lhsLen, const Char* rhs, size_t rhsLen) const
{
    const auto* const lhsLast = lhs + std::min(lhsLen, rhsLen);
    while (lhs != lhsLast)
    {
        const Char charL = asciiToLower(*lhs++); //ordering: lower-case chars have higher code points than uppper-case
        const Char charR = asciiToLower(*rhs++); //
        if (charL != charR)
            return static_cast<unsigned int>(charL) - static_cast<unsigned int>(charR); //unsigned char-comparison is the convention!
        //unsigned underflow is well-defined!
    }
    return static_cast<int>(lhsLen) - static_cast<int>(rhsLen);
}


namespace impl
{
template <class Num> inline
int saferPrintf(char* buffer, size_t bufferSize, const char* format, const Num& number) //there is no such thing as a "safe" printf ;)
{
    return std::snprintf(buffer, bufferSize, format, number); //C99: returns number of chars written if successful, < 0 or >= bufferSize on failure
}

template <class Num> inline
int saferPrintf(wchar_t* buffer, size_t bufferSize, const wchar_t* format, const Num& number)
{
    return std::swprintf(buffer, bufferSize, format, number); //C99: returns number of chars written if successful, < 0 on failure (including buffer too small)
}
}

template <class S, class T, class Num> inline
S printNumber(const T& format, const Num& number) //format a single number using ::sprintf
{
    static_assert(IsSameType<typename GetCharType<S>::Type, typename GetCharType<T>::Type>::value, "");
    using CharType = typename GetCharType<S>::Type;

    const int BUFFER_SIZE = 128;
    CharType buffer[BUFFER_SIZE]; //zero-initialize?
    const int charsWritten = impl::saferPrintf(buffer, BUFFER_SIZE, strBegin(format), number);

    return 0 < charsWritten && charsWritten < BUFFER_SIZE ? S(buffer, charsWritten) : S();
}


namespace impl
{
enum NumberType
{
    NUM_TYPE_SIGNED_INT,
    NUM_TYPE_UNSIGNED_INT,
    NUM_TYPE_FLOATING_POINT,
    NUM_TYPE_OTHER,
};


template <class S, class Num> inline
S numberTo(const Num& number, Int2Type<NUM_TYPE_OTHER>) //default number to string conversion using streams: convenient, but SLOW, SLOW, SLOW!!!! (~ factor of 20)
{
    using CharType = typename GetCharType<S>::Type;

    std::basic_ostringstream<CharType> ss;
    ss << number;
    return copyStringTo<S>(ss.str());
}


template <class S, class Num> inline S floatToString(const Num& number, char   ) { return printNumber<S>( "%g", static_cast<double>(number)); }
template <class S, class Num> inline S floatToString(const Num& number, wchar_t) { return printNumber<S>(L"%g", static_cast<double>(number)); }

template <class S, class Num> inline
S numberTo(const Num& number, Int2Type<NUM_TYPE_FLOATING_POINT>)
{
    return floatToString<S>(number, typename GetCharType<S>::Type());
}


/*
perf: integer to string: (executed 10 mio. times)
    std::stringstream - 14796 ms
    std::sprintf      -  3086 ms
    formatInteger     -   778 ms
*/

template <class OutputIterator, class Num> inline
void formatNegativeInteger(Num n, OutputIterator& it)
{
    assert(n < 0);
    using CharType = typename std::iterator_traits<OutputIterator>::value_type;
    do
    {
        const Num tmp = n / 10;
        *--it = static_cast<CharType>('0' + (tmp * 10 - n)); //8% faster than using modulus operator!
        n = tmp;
    }
    while (n != 0);

    *--it = static_cast<CharType>('-');
}

template <class OutputIterator, class Num> inline
void formatPositiveInteger(Num n, OutputIterator& it)
{
    assert(n >= 0);
    using CharType = typename std::iterator_traits<OutputIterator>::value_type;
    do
    {
        const Num tmp = n / 10;
        *--it = static_cast<CharType>('0' + (n - tmp * 10)); //8% faster than using modulus operator!
        n = tmp;
    }
    while (n != 0);
}


template <class S, class Num> inline
S numberTo(const Num& number, Int2Type<NUM_TYPE_SIGNED_INT>)
{
    using CharType = typename GetCharType<S>::Type;
    CharType buffer[2 + sizeof(Num) * 241 / 100]; //zero-initialize?
    //it's generally faster to use a buffer than to rely on String::operator+=() (in)efficiency
    //required chars (+ sign char): 1 + ceil(ln_10(256^sizeof(n) / 2 + 1))    -> divide by 2 for signed half-range; second +1 since one half starts with 1!
    // <= 1 + ceil(ln_10(256^sizeof(n))) =~ 1 + ceil(sizeof(n) * 2.4082) <= 2 + floor(sizeof(n) * 2.41)

    //caveat: consider INT_MIN: technically -INT_MIN == INT_MIN
    auto it = std::end(buffer);
    if (number < 0)
        formatNegativeInteger(number, it);
    else
        formatPositiveInteger(number, it);
    assert(it >= std::begin(buffer));

    return S(&*it, std::end(buffer) - it);
}


template <class S, class Num> inline
S numberTo(const Num& number, Int2Type<NUM_TYPE_UNSIGNED_INT>)
{
    using CharType = typename GetCharType<S>::Type;
    CharType buffer[1 + sizeof(Num) * 241 / 100]; //zero-initialize?
    //required chars: ceil(ln_10(256^sizeof(n))) =~ ceil(sizeof(n) * 2.4082) <= 1 + floor(sizeof(n) * 2.41)

    auto it = std::end(buffer);
    formatPositiveInteger(number, it);
    assert(it >= std::begin(buffer));

    return S(&*it, std::end(buffer) - it);
}

//--------------------------------------------------------------------------------

template <class Num, class S> inline
Num stringTo(const S& str, Int2Type<NUM_TYPE_OTHER>) //default string to number conversion using streams: convenient, but SLOW
{
    using CharType = typename GetCharType<S>::Type;
    Num number = 0;
    std::basic_istringstream<CharType>(copyStringTo<std::basic_string<CharType>>(str)) >> number;
    return number;
}


template <class Num> inline Num stringToFloat(const char*    str) { return std::strtod(str, nullptr); }
template <class Num> inline Num stringToFloat(const wchar_t* str) { return std::wcstod(str, nullptr); }

template <class Num, class S> inline
Num stringTo(const S& str, Int2Type<NUM_TYPE_FLOATING_POINT>)
{
    return stringToFloat<Num>(strBegin(str));
}

template <class Num, class S>
Num extractInteger(const S& str, bool& hasMinusSign) //very fast conversion to integers: slightly faster than std::atoi, but more importantly: generic
{
    using CharType = typename GetCharType<S>::Type;

    const CharType* first = strBegin(str);
    const CharType* last  = first + strLength(str);

    while (first != last && isWhiteSpace(*first)) //skip leading whitespace
        ++first;

    //handle minus sign
    hasMinusSign = false;
    if (first != last)
    {
        if (*first == static_cast<CharType>('-'))
        {
            hasMinusSign = true;
            ++first;
        }
        else if (*first == static_cast<CharType>('+'))
            ++first;
    }

    Num number = 0;
    for (const CharType* it = first; it != last; ++it)
    {
        const CharType c = *it;
        if (static_cast<CharType>('0') <= c && c <= static_cast<CharType>('9'))
        {
            number *= 10;
            number += c - static_cast<CharType>('0');
        }
        else //rest of string should contain whitespace only, it's NOT a bug if there is something else!
            break; //assert(std::all_of(iter, last, &isWhiteSpace<CharType>)); -> this is NO assert situation
    }
    return number;
}


template <class Num, class S> inline
Num stringTo(const S& str, Int2Type<NUM_TYPE_SIGNED_INT>)
{
    bool hasMinusSign = false; //handle minus sign
    const Num number = extractInteger<Num>(str, hasMinusSign);
    return hasMinusSign ? -number : number;
}


template <class Num, class S> inline
Num stringTo(const S& str, Int2Type<NUM_TYPE_UNSIGNED_INT>) //very fast conversion to integers: slightly faster than std::atoi, but more importantly: generic
{
    bool hasMinusSign = false; //handle minus sign
    const Num number = extractInteger<Num>(str, hasMinusSign);
    if (hasMinusSign)
    {
        assert(false);
        return 0U;
    }
    return number;
}
}


template <class S, class Num> inline
S numberTo(const Num& number)
{
    using TypeTag = Int2Type<
                    IsSignedInt  <Num>::value ? impl::NUM_TYPE_SIGNED_INT :
                    IsUnsignedInt<Num>::value ? impl::NUM_TYPE_UNSIGNED_INT :
                    IsFloat      <Num>::value ? impl::NUM_TYPE_FLOATING_POINT :
                    impl::NUM_TYPE_OTHER>;

    return impl::numberTo<S>(number, TypeTag());
}


template <class Num, class S> inline
Num stringTo(const S& str)
{
    using TypeTag = Int2Type<
                    IsSignedInt  <Num>::value ? impl::NUM_TYPE_SIGNED_INT :
                    IsUnsignedInt<Num>::value ? impl::NUM_TYPE_UNSIGNED_INT :
                    IsFloat      <Num>::value ? impl::NUM_TYPE_FLOATING_POINT :
                    impl::NUM_TYPE_OTHER>;

    return impl::stringTo<Num>(str, TypeTag());
}


inline //hexify beats "printNumber<std::string>("%02X", c)" by a nice factor of 3!
std::pair<char, char> hexify(unsigned char c, bool upperCase)
{
    auto hexifyDigit = [upperCase](int num) -> char //input [0, 15], output 0-9, A-F
    {
        assert(0 <= num&& num <= 15);  //guaranteed by design below!
        if (num <= 9)
            return static_cast<char>('0' + num); //no signed/unsigned char problem here!

        if (upperCase)
            return static_cast<char>('A' + (num - 10));
        else
            return static_cast<char>('a' + (num - 10));
    };
    return { hexifyDigit(c / 16), hexifyDigit(c % 16) };
}


inline //unhexify beats "::sscanf(&it[3], "%02X", &tmp)" by a factor of 3000 for ~250000 calls!!!
char unhexify(char high, char low)
{
    auto unhexifyDigit = [](char hex) -> int //input 0-9, a-f, A-F; output range: [0, 15]
    {
        if ('0' <= hex && hex <= '9') //no signed/unsigned char problem here!
            return hex - '0';
        else if ('A' <= hex && hex <= 'F')
            return (hex - 'A') + 10;
        else if ('a' <= hex && hex <= 'f')
            return (hex - 'a') + 10;
        assert(false);
        return 0;
    };
    return static_cast<unsigned char>(16 * unhexifyDigit(high) + unhexifyDigit(low)); //[!] convert to unsigned char first, then to char (which may be signed)
}
}

#endif //STRING_TOOLS_H_213458973046

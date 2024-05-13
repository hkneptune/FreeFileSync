// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STRING_TOOLS_H_213458973046
#define STRING_TOOLS_H_213458973046

//#include <cctype>  //isspace
//#include <cwctype> //iswspace
#include <cstdio>  //sprintf
#include <cwchar>  //swprintf
#include "stl_tools.h"
#include "string_traits.h"
#include "legacy_compiler.h" //<charconv> but without the compiler crashes :>


//enhance *any* string class with useful non-member functions:
namespace zen
{
template <class Char> bool isWhiteSpace(Char c);
template <class Char> bool isLineBreak (Char c);
template <class Char> bool isDigit     (Char c); //not exactly the same as "std::isdigit" -> we consider '0'-'9' only!
template <class Char> bool isHexDigit  (Char c);
template <class Char> bool isAsciiChar (Char c);
template <class Char> bool isAsciiAlpha(Char c);
template <class S   > bool isAsciiString(const S& str);
template <class Char> Char asciiToLower(Char c);
template <class Char> Char asciiToUpper(Char c);

//both S and T can be strings or char/wchar_t arrays or single char/wchar_t
template <class S, class T, typename = std::enable_if_t<isStringLike<S>/*Astyle hates tripe >*/ >> bool contains(const S& str, const T& term);

template <class S, class T> bool startsWith           (const S& str, const T& prefix);
template <class S, class T> bool startsWithAsciiNoCase(const S& str, const T& prefix);

template <class S, class T> bool endsWith           (const S& str, const T& postfix);
template <class S, class T> bool endsWithAsciiNoCase(const S& str, const T& postfix);

template <class S, class T> bool equalString     (const S& lhs, const T& rhs);
template <class S, class T> bool equalAsciiNoCase(const S& lhs, const T& rhs);

template <class S, class T> std::strong_ordering compareString(const S& lhs, const T& rhs);
template <class S, class T> std::weak_ordering compareAsciiNoCase(const S& lhs, const T& rhs); //basic case-insensitive comparison (considering A-Z only!)

//STL container predicates for std::map, std::unordered_set/map
struct StringHash;
struct StringEqual;

struct LessAsciiNoCase;
struct StringHashAsciiNoCase;
struct StringEqualAsciiNoCase;

template <class Num, class S> Num hashString(const S& str);

enum class IfNotFoundReturn
{
    all,
    none
};
template <class S, class T> S afterLast  (const S& str, const T& term, IfNotFoundReturn infr);
template <class S, class T> S beforeLast (const S& str, const T& term, IfNotFoundReturn infr);
template <class S, class T> S afterFirst (const S& str, const T& term, IfNotFoundReturn infr);
template <class S, class T> S beforeFirst(const S& str, const T& term, IfNotFoundReturn infr);

enum class SplitOnEmpty
{
    allow,
    skip
};
template <class S, class Char, class Function> void split(const S& str, Char delimiter, Function onStringPart);
template <class S, class Function1, class Function2> void split2(const S& str, Function1 isDelimiter, Function2 onStringPart);
template <class S, class Char> [[nodiscard]] std::vector<S> splitCpy(const S& str, Char delimiter, SplitOnEmpty soe);

enum class TrimSide
{
    both,
    left,
    right,
};
template <class S> [[nodiscard]] S trimCpy(const S& str, TrimSide side = TrimSide::both);
template <class S>                     void trim(S& str, TrimSide side = TrimSide::both);
template <class S, class Function>     void trim(S& str, TrimSide side, Function trimThisChar);


template <class S, class T, class U> [[nodiscard]] S replaceCpy(S  str, const T& oldTerm, const U& newTerm);
template <class S, class T, class U>            void replace   (S& str, const T& oldTerm, const U& newTerm);

template <class S, class T, class U> [[nodiscard]] S replaceCpyAsciiNoCase(S  str, const T& oldTerm, const U& newTerm);
template <class S, class T, class U>            void replaceAsciiNoCase   (S& str, const T& oldTerm, const U& newTerm);

//high-performance conversion between numbers and strings
template <class S,   class Num> S   numberTo(const Num& number);
template <class Num, class S>   Num stringTo(const S&   str);

std::pair<char, char> hexify  (unsigned char c, bool upperCase = true);
char                  unhexify(char high, char low);
std::string formatAsHexString(const std::string_view& blob); //bytes -> (human-readable) hex string

template <class S, class T, class Num> S printNumber(const T& format, const Num& number); //format a single number using std::snprintf()

//string to string conversion: converts string-like type into char-compatible target string class
template <class T, class S> T copyStringTo(S&& str);















//---------------------- implementation ----------------------
template <class Char> inline
bool isWhiteSpace(Char c)
{
    static_assert(std::is_same_v<Char, char> || std::is_same_v<Char, wchar_t>);
    assert(c != 0); //std C++ does not consider 0 as white space
    return c == static_cast<Char>(' ') || (static_cast<Char>('\t') <= c && c <= static_cast<Char>('\r'));
    //following std::isspace() for default locale but without the interface insanity:
    //  - std::isspace() takes an int, but expects an unsigned char
    //  - some parts of UTF-8 chars are erroneously seen as whitespace, e.g. the a0 from "\xec\x8b\xa0" (MSVC)
}

template <class Char> inline
bool isLineBreak(Char c)
{
    static_assert(std::is_same_v<Char, char> || std::is_same_v<Char, wchar_t>);
    return c == static_cast<Char>('\r') || c == static_cast<Char>('\n');
}


template <class Char> inline
bool isDigit(Char c) //similar to implementation of std::isdigit()!
{
    static_assert(std::is_same_v<Char, char> || std::is_same_v<Char, wchar_t>);
    return static_cast<Char>('0') <= c && c <= static_cast<Char>('9');
}


template <class Char> inline
bool isHexDigit(Char c)
{
    static_assert(std::is_same_v<Char, char> || std::is_same_v<Char, wchar_t>);
    return (static_cast<Char>('0') <= c && c <= static_cast<Char>('9')) ||
           (static_cast<Char>('A') <= c && c <= static_cast<Char>('F')) ||
           (static_cast<Char>('a') <= c && c <= static_cast<Char>('f'));
}


template <class Char> inline
bool isAsciiChar(Char c)
{
    return makeUnsigned(c) < 128;
}


template <class Char> inline
bool isAsciiAlpha(Char c)
{
    static_assert(std::is_same_v<Char, char> || std::is_same_v<Char, wchar_t>);
    return (static_cast<Char>('A') <= c && c <= static_cast<Char>('Z')) ||
           (static_cast<Char>('a') <= c && c <= static_cast<Char>('z'));
}


template <class S> inline
bool isAsciiString(const S& str)
{
    const auto* const first = strBegin(str);
    return std::all_of(first, first + strLength(str), [](auto c) { return isAsciiChar(c); });
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


namespace impl
{
template <class Char> inline
bool equalSubstring(const Char* lhs, const Char* rhs, size_t len)
{
    //support embedded 0, unlike strncmp/wcsncmp:
    return std::equal(lhs, lhs + len, rhs);
}


template <class Char1, class Char2> inline
std::weak_ordering strcmpAsciiNoCase(const Char1* lhs, const Char2* rhs, size_t len)
{
    while (len-- > 0)
    {
        const Char1 charL = asciiToLower(*lhs++); //ordering: lower-case chars have higher code points than uppper-case
        const Char2 charR = asciiToLower(*rhs++); //
        if (charL != charR)
            return makeUnsigned(charL) <=> makeUnsigned(charR); //unsigned char-comparison is the convention!
    }
    return std::weak_ordering::equivalent;
}
}


template <class S, class T> inline
bool startsWith(const S& str, const T& prefix)
{
    const size_t pfLen = strLength(prefix);
    return strLength(str) >= pfLen && impl::equalSubstring(strBegin(str), strBegin(prefix), pfLen);
}


template <class S, class T> inline
bool startsWithAsciiNoCase(const S& str, const T& prefix)
{
    assert(isAsciiString(str) || isAsciiString(prefix));
    const size_t pfLen = strLength(prefix);
    return strLength(str) >= pfLen && impl::strcmpAsciiNoCase(strBegin(str), strBegin(prefix), pfLen) == std::weak_ordering::equivalent;
}


template <class S, class T> inline
bool endsWith(const S& str, const T& postfix)
{
    const size_t strLen = strLength(str);
    const size_t pfLen  = strLength(postfix);
    return strLen >= pfLen && impl::equalSubstring(strBegin(str) + strLen - pfLen, strBegin(postfix), pfLen);
}


template <class S, class T> inline
bool endsWithAsciiNoCase(const S& str, const T& postfix)
{
    const size_t strLen = strLength(str);
    const size_t pfLen  = strLength(postfix);
    return strLen >= pfLen && impl::strcmpAsciiNoCase(strBegin(str) + strLen - pfLen, strBegin(postfix), pfLen) == std::weak_ordering::equivalent;
}


template <class S, class T> inline
bool equalString(const S& lhs, const T& rhs)
{
    const size_t lhsLen = strLength(lhs);
    return lhsLen == strLength(rhs) && impl::equalSubstring(strBegin(lhs), strBegin(rhs), lhsLen);
}


template <class S, class T> inline
bool equalAsciiNoCase(const S& lhs, const T& rhs)
{
    //assert(isAsciiString(lhs) || isAsciiString(rhs)); -> no, too strict (e.g. comparing file extensions ASCII-CI)
    const size_t lhsLen = strLength(lhs);
    return lhsLen == strLength(rhs) && impl::strcmpAsciiNoCase(strBegin(lhs), strBegin(rhs), lhsLen) == std::weak_ordering::equivalent;
}


namespace impl
{
//support embedded 0 (unlike strncmp/wcsncmp) + compare unsigned[!] char
inline std::strong_ordering strcmpWithNulls(const char*    ptr1, const char*    ptr2, size_t num) { return std:: memcmp(ptr1, ptr2, num) <=> 0; }
inline std::strong_ordering strcmpWithNulls(const wchar_t* ptr1, const wchar_t* ptr2, size_t num) { return std::wmemcmp(ptr1, ptr2, num) <=> 0; }
}

template <class S, class T> inline
std::strong_ordering compareString(const S& lhs, const T& rhs)
{
    const size_t lhsLen = strLength(lhs);
    const size_t rhsLen = strLength(rhs);

    //length check *after* strcmpWithNulls(): we DO care about natural ordering
    if (const std::strong_ordering cmp = impl::strcmpWithNulls(strBegin(lhs), strBegin(rhs), std::min(lhsLen, rhsLen));
        cmp != std::strong_ordering::equal)
        return cmp;
    return lhsLen <=> rhsLen;
}


template <class S, class T> inline
std::weak_ordering compareAsciiNoCase(const S& lhs, const T& rhs)
{
    const size_t lhsLen = strLength(lhs);
    const size_t rhsLen = strLength(rhs);

    if (const std::weak_ordering cmp = impl::strcmpAsciiNoCase(strBegin(lhs), strBegin(rhs), std::min(lhsLen, rhsLen));
        cmp != std::weak_ordering::equivalent)
        return cmp;
    return lhsLen <=> rhsLen;
}


template <class S, class T, typename> inline
bool contains(const S& str, const T& term)
{
    static_assert(std::is_same_v<GetCharTypeT<S>, GetCharTypeT<T>>);
    const size_t strLen  = strLength(str);
    const size_t termLen = strLength(term);
    if (strLen < termLen)
        return false;

    const auto* const strFirst  = strBegin(str);
    const auto* const strLast   = strFirst + strLen;
    const auto* const termFirst = strBegin(term);

    return searchFirst(strFirst, strLast,
                       termFirst, termFirst + termLen) != strLast;
}


template <class S, class T> inline
S afterLast(const S& str, const T& term, IfNotFoundReturn infr)
{
    static_assert(std::is_same_v<GetCharTypeT<S>, GetCharTypeT<T>>);
    const size_t termLen = strLength(term);
    assert(termLen > 0);

    const auto* const strFirst  = strBegin(str);
    const auto* const strLast   = strFirst + strLength(str);
    const auto* const termFirst = strBegin(term);

    const auto* it = searchLast(strFirst, strLast,
                                termFirst, termFirst + termLen);
    if (it == strLast)
        return infr == IfNotFoundReturn::all ? str : S();

    it += termLen;
    return S(it, strLast - it);
}


template <class S, class T> inline
S beforeLast(const S& str, const T& term, IfNotFoundReturn infr)
{
    static_assert(std::is_same_v<GetCharTypeT<S>, GetCharTypeT<T>>);
    const size_t termLen = strLength(term);
    assert(termLen > 0);

    const auto* const strFirst  = strBegin(str);
    const auto* const strLast   = strFirst + strLength(str);
    const auto* const termFirst = strBegin(term);

    const auto* it = searchLast(strFirst, strLast,
                                termFirst, termFirst + termLen);
    if (it == strLast)
        return infr == IfNotFoundReturn::all ? str : S();

    return S(strFirst, it - strFirst);
}


template <class S, class T> inline
S afterFirst(const S& str, const T& term, IfNotFoundReturn infr)
{
    static_assert(std::is_same_v<GetCharTypeT<S>, GetCharTypeT<T>>);
    const size_t termLen = strLength(term);
    assert(termLen > 0);

    const auto* const strFirst  = strBegin(str);
    const auto* const strLast   = strFirst + strLength(str);
    const auto* const termFirst = strBegin(term);

    const auto* it = searchFirst(strFirst, strLast,
                                 termFirst, termFirst + termLen);
    if (it == strLast)
        return infr == IfNotFoundReturn::all ? str : S();

    it += termLen;
    return S(it, strLast - it);
}


template <class S, class T> inline
S beforeFirst(const S& str, const T& term, IfNotFoundReturn infr)
{
    static_assert(std::is_same_v<GetCharTypeT<S>, GetCharTypeT<T>>);
    const size_t termLen = strLength(term);
    assert(termLen > 0);

    const auto* const strFirst  = strBegin(str);
    const auto* const strLast   = strFirst + strLength(str);
    const auto* const termFirst = strBegin(term);

    auto it = searchFirst(strFirst, strLast,
                          termFirst,  termFirst  + termLen);
    if (it == strLast)
        return infr == IfNotFoundReturn::all ? str : S();

    return S(strFirst, it - strFirst);
}


template <class S, class Function1, class Function2> inline
void split2(const S& str, Function1 isDelimiter, Function2 onStringPart)
{
    const auto* blockFirst = strBegin(str);
    const auto* const strEnd = blockFirst + strLength(str);

    for (;;)
    {
        const auto* const blockLast = std::find_if(blockFirst, strEnd, isDelimiter);
        onStringPart(makeStringView(blockFirst, blockLast));

        if (blockLast == strEnd)
            return;

        blockFirst = blockLast + 1;
    }
}


template <class S, class Char, class Function> inline
void split(const S& str, Char delimiter, Function onStringPart)
{
    static_assert(std::is_same_v<GetCharTypeT<S>, Char>);
    split2(str, [delimiter](Char c) { return c == delimiter; }, onStringPart);
}


template <class S, class Char> inline
std::vector<S> splitCpy(const S& str, Char delimiter, SplitOnEmpty soe)
{
    static_assert(std::is_same_v<GetCharTypeT<S>, Char>);
    std::vector<S> output;

    split2(str, [delimiter](Char c) { return c == delimiter; }, [&, soe](std::basic_string_view<Char> block)
    {
        if (!block.empty() || soe == SplitOnEmpty::allow)
            output.emplace_back(block.data(), block.size());
    });
    return output;
}


namespace impl
{
ZEN_INIT_DETECT_MEMBER(append)

//either call operator+=(S(str, len)) or append(str, len)
template <class S, class InputIterator, typename = std::enable_if_t<hasMember_append<S>>> inline
void stringAppend(S& str, InputIterator first, InputIterator last) { str.append(first, last);  }

//inefficient append: keep disabled until really needed
//template <class S, class InputIterator, typename = std::enable_if_t<!hasMember_append<S>>> inline
//void stringAppend(S& str, InputIterator first, InputIterator last) { str += S(first, last); }


template <class S, class T, class U, class CharEq> inline
void replace(S& str, const T& oldTerm, const U& newTerm, CharEq charEqual)
{
    static_assert(std::is_same_v<GetCharTypeT<S>, GetCharTypeT<T>>);
    static_assert(std::is_same_v<GetCharTypeT<T>, GetCharTypeT<U>>);
    const size_t oldLen = strLength(oldTerm);
    const size_t newLen = strLength(newTerm);
    //assert(oldLen != 0); -> reasonable check, but challenged by unit-test
    if (oldLen == 0)
        return;

    const auto* const oldBegin = strBegin(oldTerm);
    const auto* const oldEnd   = oldBegin + oldLen;

    const auto* const newBegin = strBegin(newTerm);
    const auto* const newEnd   = newBegin + newLen;

    using CharType = GetCharTypeT<S>;
    if (oldLen == 1 && newLen == 1) //don't use expensive std::search unless required!
        return std::replace_if(str.begin(), str.end(), [charEqual, charOld = *oldBegin](CharType c) { return charEqual(c, charOld); }, *newBegin);

    auto* it = strBegin(str); //don't use str.begin() or wxString will return this wxUni* nonsense!
    auto* const strEnd = it + strLength(str);

    auto itFound = searchFirst(it, strEnd,
                               oldBegin, oldEnd, charEqual);
    if (itFound == strEnd)
        return; //optimize "oldTerm not found"

    S output(it, itFound);
    do
    {
        impl::stringAppend(output, newBegin, newEnd);
        it = itFound + oldLen;
#if 0
        if (!replaceAll)
            itFound = strEnd;
        else
#endif
            itFound = searchFirst(it, strEnd,
                                  oldBegin, oldEnd, charEqual);

        impl::stringAppend(output, it, itFound);
    }
    while (itFound != strEnd);

    str = std::move(output);
}
}


template <class S, class T, class U> inline
void replace(S& str, const T& oldTerm, const U& newTerm)
{ impl::replace(str, oldTerm, newTerm, std::equal_to()); }


template <class S, class T, class U> inline
S replaceCpy(S str, const T& oldTerm, const U& newTerm)
{
    replace(str, oldTerm, newTerm);
    return str;
}


template <class S, class T, class U> inline
void replaceAsciiNoCase(S& str, const T& oldTerm, const U& newTerm)
{
    using CharType = GetCharTypeT<S>;
    impl::replace(str, oldTerm, newTerm,
    [](CharType charL, CharType charR) { return asciiToLower(charL) == asciiToLower(charR); });
}


template <class S, class T, class U> inline
S replaceCpyAsciiNoCase(S str, const T& oldTerm, const U& newTerm)
{
    replaceAsciiNoCase(str, oldTerm, newTerm);
    return str;
}


template <class Char, class Function>
[[nodiscard]] inline
std::pair<Char*, Char*> trimCpy2(Char* first, Char* last, TrimSide side, Function trimThisChar)
{
    if (side == TrimSide::right || side == TrimSide::both)
        while (first != last && trimThisChar(last[-1]))
            --last;

    if (side == TrimSide::left || side == TrimSide::both)
        while (first != last && trimThisChar(*first))
            ++first;

    return {first, last};
}


template <class S, class Function> inline
void trim(S& str, TrimSide side, Function trimThisChar)
{
    const auto* const oldBegin = strBegin(str);
    const auto [newBegin, newEnd] = trimCpy2(oldBegin, oldBegin + strLength(str), side, trimThisChar);

    if (newBegin != oldBegin)
        str = S(newBegin, newEnd); //minor inefficiency: in case "str" is not shared, we could save an allocation and do a memory move only
    else
        str.resize(newEnd - newBegin);
}


template <class S> inline
void trim(S& str, TrimSide side)
{
    using CharType = GetCharTypeT<S>;
    trim(str, side, [](CharType c) { return isWhiteSpace(c); });
}


template <class S> inline
S trimCpy(const S& str, TrimSide side)
{
    using CharType = GetCharTypeT<S>;
    const auto* const oldBegin = strBegin(str);
    const auto* const oldEnd = oldBegin + strLength(str);

    const auto [newBegin, newEnd] = trimCpy2(oldBegin, oldEnd, side, [](CharType c) { return isWhiteSpace(c); });

    if (newBegin == oldBegin && newEnd == oldEnd)
        return str;
    else
        return S(newBegin, newEnd - newBegin);
}


namespace impl
{
template <class S, class T>
struct CopyStringToString
{
    T copy(const S& src) const
    {
        static_assert(!std::is_same_v<std::decay_t<S>, std::decay_t<T>>);
        return {strBegin(src), strLength(src)};
    }
};

template <class T>
struct CopyStringToString<T, T> //perf: we don't need a deep copy if string types match
{
    template <class S>
    T copy(S&& str) const { return std::forward<S>(str); }
};
}

template <class T, class S> inline
T copyStringTo(S&& str) { return impl::CopyStringToString<std::decay_t<S>, T>().copy(std::forward<S>(str)); }


namespace impl
{
template <class Num> inline
int saferPrintf(char* buffer, size_t bufferSize, const char* format, const Num& number) //there is no such thing as a "safe" printf ;)
{
    return std::snprintf(buffer, bufferSize, format, number); //C99: returns number of chars written if successful, < 0 or >= bufferSize on error
}

template <class Num> inline
int saferPrintf(wchar_t* buffer, size_t bufferSize, const wchar_t* format, const Num& number)
{
    return std::swprintf(buffer, bufferSize, format, number); //C99: returns number of chars written if successful, < 0 on error (including buffer too small)
}
}

template <class S, class T, class Num> inline
S printNumber(const T& format, const Num& number) //format a single number using ::sprintf
{
    static_assert(std::is_same_v<GetCharTypeT<S>, GetCharTypeT<T>>);
    assert(strBegin(format)[strLength(format)] == 0); //format must be null-terminated!

    S buf(128, static_cast<GetCharTypeT<S>>('0'));
    const int charsWritten = impl::saferPrintf(buf.data(), buf.size(), strBegin(format), number);

    if (charsWritten < 0 || makeUnsigned(charsWritten) > buf.size())
    {
        assert(false);
        return S();
    }

    buf.resize(charsWritten);
    return buf;
}


namespace impl
{
enum class NumberType
{
    signedInt,
    unsignedInt,
    floatingPoint,
    other,
};


template <class S, class Num> S numberTo(const Num& number, std::integral_constant<NumberType, NumberType::other>) = delete;
#if 0 //default number to string conversion using streams: convenient, but SLOW, SLOW, SLOW!!!! (~ factor of 20)
template <class S, class Num> inline
S numberTo(const Num& number, std::integral_constant<NumberType, NumberType::other>)
{
    std::basic_ostringstream<GetCharTypeT<S>> ss;
    ss << number;
    return copyStringTo<S>(ss.str());
}
#endif


template <class S, class Num> inline
S numberTo(const Num& number, std::integral_constant<NumberType, NumberType::floatingPoint>)
{
    //don't use sprintf("%g"): way SLOWWWWWWER than std::to_chars()

    char buffer[128]; //zero-initialize?
    //let's give some leeway, but 24 chars should suffice: https://www.reddit.com/r/cpp/comments/dgj89g/cppcon_2019_stephan_t_lavavej_floatingpoint/f3j7d3q/
    const char* strEnd = toChars(std::begin(buffer), std::end(buffer), number);

    S output;

    for (const char c : makeStringView(static_cast<const char*>(buffer), strEnd))
        output += static_cast<GetCharTypeT<S>>(c);

    return output;
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
S numberTo(const Num& number, std::integral_constant<NumberType, NumberType::signedInt>)
{
    GetCharTypeT<S> buffer[2 + sizeof(Num) * 241 / 100]; //zero-initialize?
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
S numberTo(const Num& number, std::integral_constant<NumberType, NumberType::unsignedInt>)
{
    GetCharTypeT<S> buffer[1 + sizeof(Num) * 241 / 100]; //zero-initialize?
    //required chars: ceil(ln_10(256^sizeof(n))) =~ ceil(sizeof(n) * 2.4082) <= 1 + floor(sizeof(n) * 2.41)

    auto it = std::end(buffer);
    formatPositiveInteger(number, it);
    assert(it >= std::begin(buffer));

    return S(&*it, std::end(buffer) - it);
}

//--------------------------------------------------------------------------------

template <class Num, class S> Num stringTo(const S& str, std::integral_constant<NumberType, NumberType::other>) = delete;
#if 0 //default string to number conversion using streams: convenient, but SLOW
template <class Num, class S> inline
Num stringTo(const S& str, std::integral_constant<NumberType, NumberType::other>)
{
    using CharType = GetCharTypeT<S>;
    Num number = 0;
    std::basic_istringstream<CharType>(copyStringTo<std::basic_string<CharType>>(str)) >> number;
    return number;
}
#endif


inline
double stringToFloat(const char* first, const char* last)
{
    //don't use std::strtod(): 1. requires null-terminated string 2. SLOWER than std::from_chars()
    return fromChars(first, last);
}


inline
double stringToFloat(const wchar_t* first, const wchar_t* last)
{
    std::string buf; //let's rely on SSO

    for (const wchar_t c : makeStringView(first, last))
        buf += static_cast<char>(c);

    return fromChars(buf.c_str(), buf.c_str() + buf.size());
}


template <class Num, class S> inline
Num stringTo(const S& str, std::integral_constant<NumberType, NumberType::floatingPoint>)
{
    const auto* const first = strBegin(str);
    const auto* const last  = first + strLength(str);
    return static_cast<Num>(stringToFloat(first, last));
}


template <class Num, class S>
Num extractInteger(const S& str, bool& hasMinusSign) //very fast conversion to integers: slightly faster than std::atoi, but more importantly: generic
{
    using CharType = GetCharTypeT<S>;

    const CharType* first = strBegin(str);
    const CharType* last  = first + strLength(str);

    while (first != last && isWhiteSpace(*first)) //skip leading whitespace
        ++first;

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

    for (const CharType c : makeStringView(first, last))
        if (static_cast<CharType>('0') <= c && c <= static_cast<CharType>('9'))
        {
            number *= 10;
            number += c - static_cast<CharType>('0');
        }
        else //rest of string should contain whitespace only, it's NOT a bug if there is something else!
            break; //assert(std::all_of(it, last, isWhiteSpace<CharType>)); -> this is NO assert situation

    return number;
}


template <class Num, class S> inline
Num stringTo(const S& str, std::integral_constant<NumberType, NumberType::signedInt>)
{
    bool hasMinusSign = false; //handle minus sign
    const Num number = extractInteger<Num>(str, hasMinusSign);
    return hasMinusSign ? -number : number;
}


template <class Num, class S> inline
Num stringTo(const S& str, std::integral_constant<NumberType, NumberType::unsignedInt>) //very fast conversion to integers: slightly faster than std::atoi, but more importantly: generic
{
    bool hasMinusSign = false; //handle minus sign
    const Num number = extractInteger<Num>(str, hasMinusSign);
    if (hasMinusSign)
    {
        assert(false);
        return -makeSigned(number); //at least make some noise
    }
    return number;
}
}


template <class S, class Num> inline
S numberTo(const Num& number)
{
    using TypeTag = std::integral_constant<impl::NumberType,
                                           isSignedInt  <Num> ? impl::NumberType::signedInt :
                                           isUnsignedInt<Num> ? impl::NumberType::unsignedInt :
                                           isFloat      <Num> ? impl::NumberType::floatingPoint :
                                           impl::NumberType::other>;

    return impl::numberTo<S>(number, TypeTag());
}


template <class Num, class S> inline
Num stringTo(const S& str)
{
    using TypeTag = std::integral_constant<impl::NumberType,
                                           isSignedInt  <Num> ? impl::NumberType::signedInt :
                                           isUnsignedInt<Num> ? impl::NumberType::unsignedInt :
                                           isFloat      <Num> ? impl::NumberType::floatingPoint :
                                           impl::NumberType::other>;

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
    return {hexifyDigit(c / 16), hexifyDigit(c % 16)};
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


inline
std::string formatAsHexString(const std::string_view& blob)
{
    std::string output;
    for (const char c : blob)
    {
        const auto [high, low] = hexify(c, false /*upperCase*/);
        output += high;
        output += low;
    }
    return output;
}




template <class Num, class S> inline
Num hashString(const S& str)
{
    using CharType = GetCharTypeT<S>;
    const auto* const strFirst = strBegin(str);

    FNV1aHash<Num> hash;
    std::for_each(strFirst, strFirst + strLength(str), [&hash](CharType c) { hash.add(c); });
    return hash.get();
}


struct StringHash
{
    using is_transparent = int; //enable heterogenous lookup!

    template <class String>
    size_t operator()(const String& str) const { return hashString<size_t>(str); }
};


struct StringEqual
{
    using is_transparent = int; //enable heterogenous lookup!

    template <class String1, class String2>
    bool operator()(const String1& lhs, const String2& rhs) const { return equalString(lhs, rhs); }
};


struct LessAsciiNoCase
{
    template <class String>
    bool operator()(const String& lhs, const String& rhs) const { return compareAsciiNoCase(lhs, rhs) < 0; }
};


struct StringHashAsciiNoCase
{
    using is_transparent = int; //allow heterogenous lookup!

    template <class String>
    size_t operator()(const String& str) const
    {
        using CharType = GetCharTypeT<String>;
        const auto* const strFirst = strBegin(str);

        FNV1aHash<size_t> hash;
        std::for_each(strFirst, strFirst + strLength(str), [&hash](CharType c) { hash.add(asciiToLower(c)); });
        return hash.get();
    }
};


struct StringEqualAsciiNoCase
{
    using is_transparent = int; //allow heterogenous lookup!

    template <class String1, class String2>
    bool operator()(const String1& lhs, const String2& rhs) const
    {
        return equalAsciiNoCase(lhs, rhs);
    }
};
}

#endif //STRING_TOOLS_H_213458973046

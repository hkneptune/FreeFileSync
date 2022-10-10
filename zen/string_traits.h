// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STRING_TRAITS_H_813274321443234
#define STRING_TRAITS_H_813274321443234

#include <cstring> //strlen
#include <string_view>
#include "type_traits.h"


//uniform access to string-like types, both classes and character arrays
namespace zen
{
/*  isStringLike<>:
        isStringLike<const wchar_t*> //equals "true"
        isStringLike<const int*>     //equals "false"

    GetCharTypeT<>:
        GetCharTypeT<std::wstring> //equals wchar_t
        GetCharTypeT<wchar_t[5]>   //equals wchar_t

    strLength():
        strLength(str);   //equals str.length()
        strLength(array); //equals cStringLength(array)

    strBegin():         -> not null-terminated! -> may be nullptr if length is 0!
        std::wstring str(L"dummy");
        char array[] = "dummy";
        strBegin(str);   //returns str.c_str()
        strBegin(array); //returns array                                           */


//reference a sub-string for consumption by zen string_tools
//=> std::string_view seems decent, but of course fucks up in one regard: construction
template <class Iterator> auto makeStringView(Iterator first, Iterator last); //this constructor is not available (at least on clang)
template <class Iterator> auto makeStringView(Iterator first, size_t len);    //std::string_view(char*, int) fails to compile! expected size_t as second parameter










//---------------------- implementation ----------------------
namespace impl
{
template <class S, class Char> //test if result of S::c_str() can convert to const Char*
class HasConversion
{
    using Yes = char[1];
    using No  = char[2];

    static Yes& hasConversion(const Char*);
    static  No& hasConversion(...);

public:
    enum { value = sizeof(hasConversion(std::declval<S>().c_str())) == sizeof(Yes) };
};


template <class S, bool isStringClass> struct GetCharTypeImpl { using Type = void; };

template <class S>
struct GetCharTypeImpl<S, true>
{
    using Type = std::conditional_t<HasConversion<S, wchar_t>::value, wchar_t,
          /**/   std::conditional_t<HasConversion<S, char   >::value, char, void>>;

    //using Type = typename S::value_type;
    /*DON'T use S::value_type:
        1. support Glib::ustring: value_type is "unsigned int" but c_str() returns "const char*"
        2. wxString, wxWidgets v2.9, has some questionable string design: wxString::c_str() returns a proxy (wxCStrData) which
           is implicitly convertible to *both* "const char*" and "const wchar_t*" while wxString::value_type is a wrapper around an unsigned int
    */
};

template <> struct GetCharTypeImpl<char,    false> { using Type = char; };
template <> struct GetCharTypeImpl<wchar_t, false> { using Type = wchar_t; };

template <> struct GetCharTypeImpl<std::basic_string_view<char         >, false> { using Type = char; };
template <> struct GetCharTypeImpl<std::basic_string_view<wchar_t      >, false> { using Type = wchar_t; };
template <> struct GetCharTypeImpl<std::basic_string_view<const char   >, false> { using Type = char; };
template <> struct GetCharTypeImpl<std::basic_string_view<const wchar_t>, false> { using Type = wchar_t; };


ZEN_INIT_DETECT_MEMBER_TYPE(value_type)
ZEN_INIT_DETECT_MEMBER(c_str)  //we don't know the exact declaration of the member attribute and it may be in a base class!
ZEN_INIT_DETECT_MEMBER(length) //

template <class S>
class StringTraits
{
    using CleanType       = std::remove_cvref_t<S>;
    using NonArrayType    = std::remove_extent_t <CleanType>;
    using NonPtrType      = std::remove_pointer_t<NonArrayType>;
    using UndecoratedType = std::remove_cv_t     <NonPtrType>; //handle "const char* const"

public:
    enum
    {
        isStringClass = hasMemberType_value_type<CleanType>&&
                        hasMember_c_str         <CleanType>&&
                        hasMember_length        <CleanType>
    };

    using CharType = typename GetCharTypeImpl<UndecoratedType, isStringClass>::Type;

    enum
    {
        isStringLike = std::is_same_v<CharType, char> ||
        std::is_same_v<CharType, wchar_t>
    };
};
}


template <class T>
constexpr bool isStringLike = impl::StringTraits<T>::isStringLike;

template <class T>
using GetCharTypeT = typename impl::StringTraits<T>::CharType;


namespace impl
{
//strlen/wcslen are vectorized since VS14 CTP3
inline size_t cStringLength(const char*    str) { return std::strlen(str); }
inline size_t cStringLength(const wchar_t* str) { return std::wcslen(str); }

//no significant perf difference for "comparison" test case between cStringLength/wcslen:
#if 0
template <class C> inline
size_t cStringLength(const C* str)
{
    static_assert(std::is_same_v<C, char> || std::is_same_v<C, wchar_t>);
    size_t len = 0;
    while (*str++ != 0)
        ++len;
    return len;
}
#endif

template <class S, typename = std::enable_if_t<StringTraits<S>::isStringClass>> inline
const GetCharTypeT<S>* strBegin(const S& str) //SFINAE: T must be a "string"
{
    return str.c_str();
}

inline const char*    strBegin(const char*    str) { return str; }
inline const wchar_t* strBegin(const wchar_t* str) { return str; }
inline const char*    strBegin(const char&    ch)  { return &ch; }
inline const wchar_t* strBegin(const wchar_t& ch)  { return &ch; }

inline const char*    strBegin(const std::basic_string_view<char         >& ref) { return ref.data(); }
inline const wchar_t* strBegin(const std::basic_string_view<wchar_t      >& ref) { return ref.data(); }
inline const char*    strBegin(const std::basic_string_view<const char   >& ref) { return ref.data(); }
inline const wchar_t* strBegin(const std::basic_string_view<const wchar_t>& ref) { return ref.data(); }

template <class S, typename = std::enable_if_t<StringTraits<S>::isStringClass>> inline
size_t strLength(const S& str) //SFINAE: T must be a "string"
{
    return str.length();
}

inline size_t strLength(const char*    str) { return cStringLength(str); }
inline size_t strLength(const wchar_t* str) { return cStringLength(str); }
inline size_t strLength(char)               { return 1; }
inline size_t strLength(wchar_t)            { return 1; }

inline size_t strLength(const std::basic_string_view<char         >& ref) { return ref.length(); }
inline size_t strLength(const std::basic_string_view<wchar_t      >& ref) { return ref.length(); }
inline size_t strLength(const std::basic_string_view<const char   >& ref) { return ref.length(); }
inline size_t strLength(const std::basic_string_view<const wchar_t>& ref) { return ref.length(); }
}


template <class S> inline
auto strBegin(S&& str)
{
    static_assert(isStringLike<S>);
    return impl::strBegin(std::forward<S>(str));
}


template <class S> inline
size_t strLength(S&& str)
{
    static_assert(isStringLike<S>);
    return impl::strLength(std::forward<S>(str));
}


template <class Iterator> inline
auto makeStringView(Iterator first, Iterator last)
{
    using CharType = GetCharTypeT<decltype(&*first)>;

    return std::basic_string_view<CharType>(first != last ? &*first :
                                            reinterpret_cast<CharType*>(0x1000), /*Win32 APIs like CompareStringOrdinal() choke on nullptr!*/
                                            last - first);
}

template <class Iterator> inline
auto makeStringView(Iterator first, size_t len) { return makeStringView(first, first + len); }
}

#endif //STRING_TRAITS_H_813274321443234

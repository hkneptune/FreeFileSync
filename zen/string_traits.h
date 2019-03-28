// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STRING_TRAITS_H_813274321443234
#define STRING_TRAITS_H_813274321443234

#include <cstring> //strlen
#include "type_traits.h"


//uniform access to string-like types, both classes and character arrays
namespace zen
{
/*
IsStringLikeV<>:
    IsStringLikeV<const wchar_t*> //equals "true"
    IsStringLikeV<const int*>     //equals "false"

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
    strBegin(array); //returns array
*/


//reference a sub-string for consumption by zen string_tools
template <class Char>
class StringRef
{
public:
    template <class Iterator>
    StringRef(Iterator first, Iterator last) : len_(last - first),
        str_(first != last ? &*first : reinterpret_cast<Char*>(this) /*Win32 APIs like CompareStringOrdinal() choke on nullptr!*/)
    {
        static_assert(alignof(StringRef) % alignof(Char) == 0); //even though str_ is never dereferenced, make sure the pointer value respects alignment (why? because we can)
    }
    //StringRef(const Char* str, size_t len) : str_(str), len_(len) {} -> needless constraint! Char* not available for empty range!

    Char*  data  () const { return str_; } //no null-termination!
    size_t length() const { return len_; }

private:
    const size_t len_;
    Char* const str_;
};












//---------------------- implementation ----------------------
namespace impl
{
template<class S, class Char> //test if result of S::c_str() can convert to const Char*
class HasConversion
{
    using Yes = char[1];
    using No  = char[2];

    static Yes& hasConversion(const Char*);
    static  No& hasConversion(...);

public:
    enum { value = sizeof(hasConversion(std::declval<S>().c_str())) == sizeof(Yes) };
};


template <class S, bool isStringClass>  struct GetCharTypeImpl { using Type = void; };

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

template <> struct GetCharTypeImpl<StringRef<char         >, false> { using Type = char; };
template <> struct GetCharTypeImpl<StringRef<wchar_t      >, false> { using Type = wchar_t; };
template <> struct GetCharTypeImpl<StringRef<const char   >, false> { using Type = char; };
template <> struct GetCharTypeImpl<StringRef<const wchar_t>, false> { using Type = wchar_t; };


ZEN_INIT_DETECT_MEMBER_TYPE(value_type);
ZEN_INIT_DETECT_MEMBER(c_str);  //we don't know the exact declaration of the member attribute and it may be in a base class!
ZEN_INIT_DETECT_MEMBER(length); //

template <class S>
class StringTraits
{
    using CleanType       = std::remove_cv_t<std::remove_reference_t<S>>; //std::remove_cvref requires C++20
    using NonArrayType    = std::remove_extent_t <CleanType>;
    using NonPtrType      = std::remove_pointer_t<NonArrayType>;
    using UndecoratedType = std::remove_cv_t     <NonPtrType>; //handle "const char* const"

public:
    enum
    {
        isStringClass = HasMemberType_value_type<CleanType>::value &&
                        HasMember_c_str         <CleanType>::value &&
                        HasMember_length        <CleanType>::value
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
struct IsStringLike : std::bool_constant<impl::StringTraits<T>::isStringLike> {};

template <class T>
struct GetCharType { using Type = typename impl::StringTraits<T>::CharType; };


//template alias helpers:
template<class T>
constexpr bool IsStringLikeV = IsStringLike<T>::value;

template<class T>
using GetCharTypeT = typename GetCharType<T>::Type;


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

inline const char*    strBegin(const StringRef<char         >& ref) { return ref.data(); }
inline const wchar_t* strBegin(const StringRef<wchar_t      >& ref) { return ref.data(); }
inline const char*    strBegin(const StringRef<const char   >& ref) { return ref.data(); }
inline const wchar_t* strBegin(const StringRef<const wchar_t>& ref) { return ref.data(); }


template <class S, typename = std::enable_if_t<StringTraits<S>::isStringClass>> inline
size_t strLength(const S& str) //SFINAE: T must be a "string"
{
    return str.length();
}

inline size_t strLength(const char*    str) { return cStringLength(str); }
inline size_t strLength(const wchar_t* str) { return cStringLength(str); }
inline size_t strLength(char)               { return 1; }
inline size_t strLength(wchar_t)            { return 1; }

inline size_t strLength(const StringRef<char         >& ref) { return ref.length(); }
inline size_t strLength(const StringRef<wchar_t      >& ref) { return ref.length(); }
inline size_t strLength(const StringRef<const char   >& ref) { return ref.length(); }
inline size_t strLength(const StringRef<const wchar_t>& ref) { return ref.length(); }
}


template <class S> inline
auto strBegin(S&& str) -> const GetCharTypeT<S>*
{
    static_assert(IsStringLikeV<S>);
    return impl::strBegin(std::forward<S>(str));
}


template <class S> inline
size_t strLength(S&& str)
{
    static_assert(IsStringLikeV<S>);
    return impl::strLength(std::forward<S>(str));
}
}

#endif //STRING_TRAITS_H_813274321443234

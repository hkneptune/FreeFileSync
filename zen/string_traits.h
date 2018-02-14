// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STRING_TRAITS_H_813274321443234
#define STRING_TRAITS_H_813274321443234

#include <cstring> //strlen
#include "type_tools.h"


//uniform access to string-like types, both classes and character arrays
namespace zen
{
/*
IsStringLike<>::value:
    IsStringLike<const wchar_t*>::value; //equals "true"
    IsStringLike<const int*>    ::value; //equals "false"

GetCharType<>::Type:
    GetCharType<std::wstring>::Type  //equals wchar_t
    GetCharType<wchar_t[5]>  ::Type  //equals wchar_t

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
    StringRef(Iterator first, Iterator last) : len_(last - first), str_(first != last ? &*first : nullptr) {}
    //StringRef(const Char* str, size_t len) : str_(str), len_(len) {} -> needless constraint! Char* not available for empty range!

    Char*  data  () const { return str_; } //1. no null-termination! 2. may be nullptr!
    size_t length() const { return len_; }

private:
    const size_t len_;
    Char* str_;
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


template <class S, bool isStringClass>  struct GetCharTypeImpl : ResultType<NullType> {};

template <class S>
struct GetCharTypeImpl<S, true> :
    ResultType<
    typename SelectIf<HasConversion<S, wchar_t>::value, wchar_t,
    typename SelectIf<HasConversion<S, char   >::value, char, NullType>::Type
    >::Type>
{
    //using Type = typename S::value_type;
    /*DON'T use S::value_type:
        1. support Glib::ustring: value_type is "unsigned int" but c_str() returns "const char*"
        2. wxString, wxWidgets v2.9, has some questionable string design: wxString::c_str() returns a proxy (wxCStrData) which
           is implicitly convertible to *both* "const char*" and "const wchar_t*" while wxString::value_type is a wrapper around an unsigned int
    */
};

template <> struct GetCharTypeImpl<char,    false> : ResultType<char   > {};
template <> struct GetCharTypeImpl<wchar_t, false> : ResultType<wchar_t> {};

template <> struct GetCharTypeImpl<StringRef<char         >, false> : ResultType<char   > {};
template <> struct GetCharTypeImpl<StringRef<wchar_t      >, false> : ResultType<wchar_t> {};
template <> struct GetCharTypeImpl<StringRef<const char   >, false> : ResultType<char   > {};
template <> struct GetCharTypeImpl<StringRef<const wchar_t>, false> : ResultType<wchar_t> {};


ZEN_INIT_DETECT_MEMBER_TYPE(value_type);
ZEN_INIT_DETECT_MEMBER(c_str);  //we don't know the exact declaration of the member attribute and it may be in a base class!
ZEN_INIT_DETECT_MEMBER(length); //

template <class S>
class StringTraits
{
    using NonRefType      = typename RemoveRef    <S           >::Type;
    using NonConstType    = typename RemoveConst  <NonRefType  >::Type;
    using NonArrayType    = typename RemoveArray  <NonConstType>::Type;
    using NonPtrType      = typename RemovePointer<NonArrayType>::Type;
    using UndecoratedType = typename RemoveConst  <NonPtrType  >::Type ; //handle "const char* const"

public:
    enum
    {
        isStringClass = HasMemberType_value_type<NonConstType>::value &&
                        HasMember_c_str         <NonConstType>::value &&
                        HasMember_length        <NonConstType>::value
    };

    using CharType = typename GetCharTypeImpl<UndecoratedType, isStringClass>::Type;

    enum
    {
        isStringLike = IsSameType<CharType, char>::value ||
        IsSameType<CharType, wchar_t>::value
    };
};
}

template <class T>
struct IsStringLike : StaticBool<impl::StringTraits<T>::isStringLike> {};

template <class T>
struct GetCharType : ResultType<typename impl::StringTraits<T>::CharType> {};


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
    static_assert(IsSameType<C, char>::value || IsSameType<C, wchar_t>::value, "");
    size_t len = 0;
    while (*str++ != 0)
        ++len;
    return len;
}
#endif

template <class S, typename = typename EnableIf<StringTraits<S>::isStringClass>::Type> inline
const typename GetCharType<S>::Type* strBegin(const S& str) //SFINAE: T must be a "string"
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


template <class S, typename = typename EnableIf<StringTraits<S>::isStringClass>::Type> inline
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
auto strBegin(S&& str) -> const typename GetCharType<S>::Type*
{
    static_assert(IsStringLike<S>::value, "");
    return impl::strBegin(std::forward<S>(str));
}


template <class S> inline
size_t strLength(S&& str)
{
    static_assert(IsStringLike<S>::value, "");
    return impl::strLength(std::forward<S>(str));
}
}

#endif //STRING_TRAITS_H_813274321443234

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TYPE_TRAITS_H_3425628658765467
#define TYPE_TRAITS_H_3425628658765467

#include <type_traits>

//https://en.cppreference.com/w/cpp/header/type_traits

namespace zen
{
template<class T, class...>
struct GetFirstOf
{
    using Type = T;
};
template<class... T> using GetFirstOfT = typename GetFirstOf<T...>::Type;

template <class F>
class FunctionReturnType
{
    template <class R, class... Args> static R dummyFun(R(*)(Args...));
public:
    using Type = decltype(dummyFun(F()));
};
template<class F> using FunctionReturnTypeT = typename FunctionReturnType<F>::Type;

//=============================================================================

template<class T, size_t N>
constexpr size_t arraySize(T (&)[N]) { return N; }

template<class S, class T, size_t N>
constexpr S arrayAccumulate(T (&arr)[N])
{
    S sum = 0;
    for (size_t i = 0; i < N; ++i)
        sum += arr[i];
    return sum;
}

//Herb Sutter's signedness conversion helpers: https://herbsutter.com/2013/06/13/gotw-93-solution-auto-variables-part-2/
template<class T> inline auto makeSigned  (T t) { return static_cast<std::make_signed_t  <T>>(t); }
template<class T> inline auto makeUnsigned(T t) { return static_cast<std::make_unsigned_t<T>>(t); }

//################# Built-in Types  ########################
//Example: "IsSignedInt<int>::value" evaluates to "true"

//unfortunate standardized nonsense: std::is_integral<> includes bool, char, wchar_t! => roll our own:
template <class T> struct IsUnsignedInt;
template <class T> struct IsSignedInt;

template <class T> using IsFloat      = std::is_floating_point<T>;
template <class T> using IsInteger    = std::bool_constant<IsUnsignedInt<T>::value || IsSignedInt<T>::value>;
template <class T> using IsArithmetic = std::bool_constant<IsInteger    <T>::value || IsFloat    <T>::value>;

//remaining non-arithmetic types: bool, char, wchar_t


//optional: specialize new types like:
//template <> struct IsUnsignedInt<UInt64> : std::true_type {};

//################# Class Members ########################

/*  Detect data or function members of a class by name: ZEN_INIT_DETECT_MEMBER + HasMember_
    Example: 1. ZEN_INIT_DETECT_MEMBER(c_str);
             2. HasMemberV_c_str<T>    -> use boolean
*/

/*  Detect data or function members of a class by name *and* type: ZEN_INIT_DETECT_MEMBER2 + HasMember_

    Example: 1. ZEN_INIT_DETECT_MEMBER2(size, size_t (T::*)() const);
             2. HasMember_size<T>::value    -> use as boolean
*/

/*  Detect member type of a class: ZEN_INIT_DETECT_MEMBER_TYPE + HasMemberType_

    Example: 1. ZEN_INIT_DETECT_MEMBER_TYPE(value_type);
             2. HasMemberTypeV_value_type<T>    -> use as boolean
*/

//########## Sorting ##############################
/*
Generate a descending binary predicate at compile time!

Usage:
    static const bool ascending = ...
    makeSortDirection(old binary predicate, std::bool_constant<ascending>()) -> new binary predicate
*/

template <class Predicate>
struct LessDescending
{
    LessDescending(Predicate lessThan) : lessThan_(lessThan) {}
    template <class T> bool operator()(const T& lhs, const T& rhs) const { return lessThan_(rhs, lhs); }
private:
    Predicate lessThan_;
};

template <class Predicate> inline
/**/           Predicate  makeSortDirection(Predicate pred, std::true_type) { return pred; }

template <class Predicate> inline
LessDescending<Predicate> makeSortDirection(Predicate pred, std::false_type) { return pred; }







//################ implementation ######################
template <class T>
struct IsUnsignedInt : std::false_type {};

template <> struct IsUnsignedInt<unsigned char         > : std::true_type {};
template <> struct IsUnsignedInt<unsigned short int    > : std::true_type {};
template <> struct IsUnsignedInt<unsigned int          > : std::true_type {};
template <> struct IsUnsignedInt<unsigned long int     > : std::true_type {};
template <> struct IsUnsignedInt<unsigned long long int> : std::true_type {};

template <class T>
struct IsSignedInt : std::false_type {};

template <> struct IsSignedInt<signed char  > : std::true_type {};
template <> struct IsSignedInt<short int    > : std::true_type {};
template <> struct IsSignedInt<int          > : std::true_type {};
template <> struct IsSignedInt<long int     > : std::true_type {};
template <> struct IsSignedInt<long long int> : std::true_type {};
//####################################################################

#define ZEN_INIT_DETECT_MEMBER(NAME)        \
    \
    template<bool isClass, class T>         \
    struct HasMemberImpl_##NAME             \
    {                                       \
    private:                                \
        using Yes = char[1];                \
        using No  = char[2];                \
        \
        template <typename U, U t>          \
        class Helper {};                    \
        \
        struct Fallback { int NAME; };      \
        \
        template <class U>                  \
        struct Helper2 : public U, public Fallback {};  /*this works only for class types!!!*/  \
        \
        template <class U> static  No& hasMember(Helper<int Fallback::*, &Helper2<U>::NAME>*);  \
        template <class U> static Yes& hasMember(...);                                          \
    public:                                                                                     \
        enum { value = sizeof(hasMember<T>(nullptr)) == sizeof(Yes) };                          \
    };                                                                                          \
    \
    template<class T>                                           \
    struct HasMemberImpl_##NAME<false, T> : std::false_type {}; \
    \
    template<class T> constexpr bool HasMemberV_##NAME = HasMemberImpl_##NAME<std::is_class_v<T>, T>::value;

//####################################################################

#define ZEN_INIT_DETECT_MEMBER2(NAME, TYPE)         \
    \
    template<typename U>                            \
    class HasMember_##NAME                          \
    {                                               \
        using Yes = char[1];                        \
        using No  = char[2];                        \
        \
        template <typename T, T t> class Helper {}; \
        \
        template <class T> static Yes& hasMember(Helper<TYPE, &T::NAME>*);  \
        template <class T> static  No& hasMember(...);                      \
    public:                                                                 \
        enum { value = sizeof(hasMember<U>(nullptr)) == sizeof(Yes) };      \
    };                                                                      \
    \
    template<class T> constexpr bool HasMemberV_##NAME = HasMember_##NAME<T>::value;

//####################################################################

#define ZEN_INIT_DETECT_MEMBER_TYPE(TYPENAME)       \
    \
    template<typename T>                        \
    class HasMemberType_##TYPENAME              \
    {                                           \
        using Yes = char[1];                    \
        using No  = char[2];                    \
        \
        template <typename U> class Helper {};  \
        \
        template <class U> static Yes& hasMemberType(Helper<typename U::TYPENAME>*); \
        template <class U> static  No& hasMemberType(...);                           \
    public:                                                                          \
        enum { value = sizeof(hasMemberType<T>(nullptr)) == sizeof(Yes) };           \
    };                                                                               \
    \
    template<class T> constexpr bool HasMemberTypeV_##TYPENAME = HasMemberType_##TYPENAME<T>::value;
}

#endif //TYPE_TRAITS_H_3425628658765467

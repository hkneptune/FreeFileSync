// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TYPE_TRAITS_H_3425628658765467
#define TYPE_TRAITS_H_3425628658765467

#include <algorithm>
#include <type_traits>

//https://en.cppreference.com/w/cpp/header/type_traits

namespace zen
{
template <class T, class...>
struct GetFirstOf
{
    using Type = T;
};
template <class... T> using GetFirstOfT = typename GetFirstOf<T...>::Type;


template <class F>
class FunctionReturnType
{
    template <class R, class... Args> static R dummyFun(R(*)(Args...));
public:
    using Type = decltype(dummyFun(F()));
};
template <class F> using FunctionReturnTypeT = typename FunctionReturnType<F>::Type;

//=============================================================================

template <class T, size_t N>
constexpr uint32_t arrayHash(T (&arr)[N]) //don't bother making FNV1aHash constexpr instead
{
    uint32_t hashVal = 2166136261U; //FNV-1a base

    std::for_each(&arr[0], &arr[N], [&hashVal](T n)
    {
        //static_assert(isInteger<T> || std::is_same_v<T, char> || std::is_same_v<T, wchar_t>);
        static_assert(sizeof(T) <= sizeof(hashVal));
        hashVal ^= static_cast<uint32_t>(n);
        hashVal *= 16777619U; //prime
    });
    return hashVal;
}

//Herb Sutter's signedness conversion helpers: https://herbsutter.com/2013/06/13/gotw-93-solution-auto-variables-part-2/
template <class T> inline auto makeSigned  (T t) { return static_cast<std::make_signed_t  <T>>(t); }
template <class T> inline auto makeUnsigned(T t) { return static_cast<std::make_unsigned_t<T>>(t); }

//################# Built-in Types  ########################
//unfortunate standardized nonsense: std::is_integral<> includes bool, char, wchar_t! => roll our own:
template <class T> constexpr bool isUnsignedInt = std::is_same_v<std::remove_cv_t<T>, unsigned char>      ||
                                                  std::is_same_v<std::remove_cv_t<T>, unsigned short int> ||
                                                  std::is_same_v<std::remove_cv_t<T>, unsigned int>       ||
                                                  std::is_same_v<std::remove_cv_t<T>, unsigned long int>  ||
                                                  std::is_same_v<std::remove_cv_t<T>, unsigned long long int>;

template <class T> constexpr bool isSignedInt = std::is_same_v<std::remove_cv_t<T>, signed char> ||
                                                std::is_same_v<std::remove_cv_t<T>, short int>   ||
                                                std::is_same_v<std::remove_cv_t<T>, int>         ||
                                                std::is_same_v<std::remove_cv_t<T>, long int>    ||
                                                std::is_same_v<std::remove_cv_t<T>, long long int>;

template <class T> constexpr bool isInteger    = isUnsignedInt<T> || isSignedInt<T>;
template <class T> constexpr bool isFloat      = std::is_floating_point_v<T>;
template <class T> constexpr bool isArithmetic = isInteger<T> || isFloat<T>;

//################# Class Members ########################

/*  Detect data or function members of a class by name: ZEN_INIT_DETECT_MEMBER + hasMember_
    Example: 1. ZEN_INIT_DETECT_MEMBER(c_str);
             2. hasMember_c_str<T>    -> use boolean


    Detect data or function members of a class by name *and* type: ZEN_INIT_DETECT_MEMBER2 + HasMember_

    Example: 1. ZEN_INIT_DETECT_MEMBER2(size, size_t (T::*)() const);
             2. hasMember_size<T>::value    -> use as boolean


    Detect member type of a class: ZEN_INIT_DETECT_MEMBER_TYPE + hasMemberType_

    Example: 1. ZEN_INIT_DETECT_MEMBER_TYPE(value_type);
             2. hasMemberType_value_type<T>    -> use as boolean                       */

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
    LessDescending(Predicate lessThan) : lessThan_(std::move(lessThan)) {}
    template <class T> bool operator()(const T& lhs, const T& rhs) const { return lessThan_(rhs, lhs); }
private:
    Predicate lessThan_;
};

template <class Predicate> inline
/**/            Predicate makeSortDirection(Predicate pred, std::true_type) { return pred; }

template <class Predicate> inline
LessDescending<Predicate> makeSortDirection(Predicate pred, std::false_type) { return pred; }







//################ implementation ######################
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
    template <class T>                                          \
    struct HasMemberImpl_##NAME<false, T> : std::false_type {}; \
    \
    template <class T> constexpr bool hasMember_##NAME = HasMemberImpl_##NAME<std::is_class_v<T>, T>::value;

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
    template <class T> constexpr bool hasMember_##NAME = HasMember_##NAME<T>::value;

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
    template <class T> constexpr bool hasMemberType_##TYPENAME = HasMemberType_##TYPENAME<T>::value;
}


//---------------------------------------------------------------------------
//ZEN macro consistency checks: => place in most-used header!



#endif //TYPE_TRAITS_H_3425628658765467

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TYPE_TRAITS_H_3425628658765467
#define TYPE_TRAITS_H_3425628658765467

#include <type_traits> //all we need is std::is_class!!


namespace zen
{
//################# TMP compile time return values: "inherit to return compile-time result" ##############
template <int i>
struct StaticInt
{
    enum { value = i };
};

template <bool b>
struct StaticBool : StaticInt<b> {};

using TrueType  = StaticBool<true>;
using FalseType = StaticBool<false>;

template <class EnumType, EnumType val>
struct StaticEnum
{
    static const EnumType value = val;
};
//---------------------------------------------------------
template <class T>
struct ResultType
{
    using Type = T;
};

//Herb Sutter's signedness conversion helpers: http://herbsutter.com/2013/06/13/gotw-93-solution-auto-variables-part-2/
template<class T> inline auto makeSigned  (T t) { return static_cast<std::make_signed_t  <T>>(t); }
template<class T> inline auto makeUnsigned(T t) { return static_cast<std::make_unsigned_t<T>>(t); }

//################# Built-in Types  ########################
//Example: "IsSignedInt<int>::value" evaluates to "true"

template <class T> struct IsUnsignedInt;
template <class T> struct IsSignedInt;
template <class T> struct IsFloat;

template <class T> struct IsInteger;    //IsSignedInt or IsUnsignedInt
template <class T> struct IsArithmetic; //IsInteger or IsFloat
//remaining non-arithmetic types: bool, char, wchar_t

//optional: specialize new types like:
//template <> struct IsUnsignedInt<UInt64> : TrueType {};

//################# Class Members ########################

/*  Detect data or function members of a class by name: ZEN_INIT_DETECT_MEMBER + HasMember_
    Example: 1. ZEN_INIT_DETECT_MEMBER(c_str);
             2. HasMember_c_str<T>::value     -> use boolean
*/

/*  Detect data or function members of a class by name *and* type: ZEN_INIT_DETECT_MEMBER2 + HasMember_

    Example: 1. ZEN_INIT_DETECT_MEMBER2(size, size_t (T::*)() const);
             2. HasMember_size<T>::value     -> use as boolean
*/

/*  Detect member type of a class: ZEN_INIT_DETECT_MEMBER_TYPE + HasMemberType_

    Example: 1. ZEN_INIT_DETECT_MEMBER_TYPE(value_type);
             2. HasMemberType_value_type<T>::value     -> use as boolean
*/















//################ implementation ######################
#define ZEN_SPECIALIZE_TRAIT(X, Y) template <> struct X<Y> : TrueType {};

template <class T>
struct IsUnsignedInt : FalseType {};

ZEN_SPECIALIZE_TRAIT(IsUnsignedInt, unsigned char);
ZEN_SPECIALIZE_TRAIT(IsUnsignedInt, unsigned short int);
ZEN_SPECIALIZE_TRAIT(IsUnsignedInt, unsigned int);
ZEN_SPECIALIZE_TRAIT(IsUnsignedInt, unsigned long int);
ZEN_SPECIALIZE_TRAIT(IsUnsignedInt, unsigned long long int); //new with C++11 - same type as unsigned __int64 in VS2010
//------------------------------------------------------

template <class T>
struct IsSignedInt : FalseType {};

ZEN_SPECIALIZE_TRAIT(IsSignedInt, signed char);
ZEN_SPECIALIZE_TRAIT(IsSignedInt, short int);
ZEN_SPECIALIZE_TRAIT(IsSignedInt, int);
ZEN_SPECIALIZE_TRAIT(IsSignedInt, long int);
ZEN_SPECIALIZE_TRAIT(IsSignedInt, long long int); //new with C++11 - same type as __int64 in VS2010
//------------------------------------------------------

template <class T>
struct IsFloat : FalseType {};

ZEN_SPECIALIZE_TRAIT(IsFloat, float);
ZEN_SPECIALIZE_TRAIT(IsFloat, double);
ZEN_SPECIALIZE_TRAIT(IsFloat, long double);
//------------------------------------------------------

#undef ZEN_SPECIALIZE_TRAIT

template <class T>
struct IsInteger : StaticBool<IsUnsignedInt<T>::value || IsSignedInt<T>::value> {};

template <class T>
struct IsArithmetic : StaticBool<IsInteger<T>::value || IsFloat<T>::value> {};
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
    template<class T>                                             \
    struct HasMemberImpl_##NAME<false, T> : FalseType {}; \
    \
    template<typename T>                                          \
    struct HasMember_##NAME : StaticBool<HasMemberImpl_##NAME<std::is_class<T>::value, T>::value> {};

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
    };
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
    };
}

#endif //TYPE_TRAITS_H_3425628658765467

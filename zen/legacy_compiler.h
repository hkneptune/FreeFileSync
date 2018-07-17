// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef LEGACY_COMPILER_H_839567308565656789
#define LEGACY_COMPILER_H_839567308565656789

    #include <memory>
    #include <cstddef> //std::byte


namespace std
{
//https://gcc.gnu.org/onlinedocs/libstdc++/manual/status.html
//https://isocpp.org/std/standing-documents/sd-6-sg10-feature-test-recommendations

#ifndef __cpp_lib_type_trait_variable_templates //GCC 7.1
    template<class T, class U> constexpr bool is_same_v = is_same<T, U>::value;
    template<class T> constexpr bool is_const_v  = is_const<T>::value;
    template<class T> constexpr bool is_signed_v = is_signed<T>::value;
    template<class T> constexpr bool is_trivially_destructible_v = is_trivially_destructible<T>::value;
#endif

#if __GNUC__ < 7 || (__GNUC__ == 7 && __GNUC_MINOR__ < 1) //GCC doesn't define __cpp_lib_raw_memory_algorithms
template<class T> void destroy_at(T* p) { p->~T(); }

template< class ForwardIt >
void destroy(ForwardIt first, ForwardIt last)
{
    for (; first != last; ++first)
        std::destroy_at(std::addressof(*first));
}

template<class InputIt, class ForwardIt>
ForwardIt uninitialized_move(InputIt first, InputIt last, ForwardIt trg_first)
{
    typedef typename std::iterator_traits<ForwardIt>::value_type Value;
    ForwardIt current = trg_first;
    try
    {
        for (; first != last; ++first, ++current)
            ::new (static_cast<void*>(std::addressof(*current))) Value(std::move(*first));
        return current;
    }
    catch (...)
    {
        for (; trg_first != current; ++trg_first)
            trg_first->~Value();
        throw;
    }
}
#endif

#ifndef __cpp_lib_apply //GCC 7.1
template <class F, class T0, class T1, class T2>
constexpr decltype(auto) apply(F&& f, std::tuple<T0, T1, T2>& t) { return f(std::get<0>(t), std::get<1>(t), std::get<2>(t)); }
#endif

#if __GNUC__ < 7 || (__GNUC__ == 7 && __GNUC_MINOR__ < 1) //__cpp_lib_byte not defined before GCC 7.3 but supported earlier
    typedef unsigned char byte;
#endif

#ifndef __cpp_lib_bool_constant //GCC 6.1
    template<bool B> using bool_constant = integral_constant<bool, B>;
#endif

//================================================================================

}
namespace __cxxabiv1
{
struct __cxa_eh_globals;
extern "C" __cxa_eh_globals* __cxa_get_globals() noexcept;
}
namespace std
{
inline int uncaught_exceptions_legacy_hack() noexcept
{
    return *(reinterpret_cast<unsigned int*>(static_cast<char*>(static_cast<void*>(__cxxabiv1::__cxa_get_globals())) + sizeof(void*)));
}
#ifndef __cpp_lib_uncaught_exceptions //GCC 6.1
inline int uncaught_exceptions() noexcept { return uncaught_exceptions_legacy_hack(); }
#endif

}

#endif //LEGACY_COMPILER_H_839567308565656789

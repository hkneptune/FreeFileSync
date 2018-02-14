// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SCOPE_GUARD_H_8971632487321434
#define SCOPE_GUARD_H_8971632487321434

#include <cassert>
#include <exception>
#include "type_tools.h"


//std::uncaught_exceptions() currently unsupported on GCC and Clang => clean up ASAP
    static_assert(__GNUC__ < 7 || (__GNUC__ == 7 && (__GNUC_MINOR__ < 3 || (__GNUC_MINOR__ == 3 && __GNUC_PATCHLEVEL__ <= 0))), "check std::uncaught_exceptions support");

namespace __cxxabiv1
{
struct __cxa_eh_globals;
extern "C" __cxa_eh_globals* __cxa_get_globals() noexcept;
}

inline
int getUncaughtExceptionCount()
{
    return *(reinterpret_cast<unsigned int*>(static_cast<char*>(static_cast<void*>(__cxxabiv1::__cxa_get_globals())) + sizeof(void*)));
}

//best of Zen, Loki and C++17

namespace zen
{
//Scope Guard
/*
    auto guardAio = zen::makeGuard<ScopeGuardRunMode::ON_EXIT>([&] { ::CloseHandle(hDir); });
        ...
    guardAio.dismiss();

Scope Exit:
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hDir));
    ZEN_ON_SCOPE_FAIL(UndoPreviousWork());
    ZEN_ON_SCOPE_SUCCESS(NotifySuccess());
*/

enum class ScopeGuardRunMode
{
    ON_EXIT,
    ON_SUCCESS,
    ON_FAIL
};


//partially specialize scope guard destructor code and get rid of those pesky MSVC "4127 conditional expression is constant"
template <typename F> inline
void runScopeGuardDestructor(F& fun, int /*exeptionCountOld*/, StaticEnum<ScopeGuardRunMode, ScopeGuardRunMode::ON_EXIT>)
{
    try { fun(); }
    catch (...) { assert(false); } //consistency: don't expect exceptions for ON_EXIT even if "!failed"!
}


template <typename F> inline
void runScopeGuardDestructor(F& fun, int exeptionCountOld, StaticEnum<ScopeGuardRunMode, ScopeGuardRunMode::ON_SUCCESS>)
{
    const bool failed = getUncaughtExceptionCount() > exeptionCountOld;
    if (!failed)
        fun(); //throw X
}


template <typename F> inline
void runScopeGuardDestructor(F& fun, int exeptionCountOld, StaticEnum<ScopeGuardRunMode, ScopeGuardRunMode::ON_FAIL>)
{
    const bool failed = getUncaughtExceptionCount() > exeptionCountOld;
    if (failed)
        try { fun(); }
        catch (...) { assert(false); }
}


template <ScopeGuardRunMode runMode, typename F>
class ScopeGuard
{
public:
    explicit ScopeGuard(const F&  fun) : fun_(fun) {}
    explicit ScopeGuard(      F&& fun) : fun_(std::move(fun)) {}

    ScopeGuard(ScopeGuard&& other) : fun_(std::move(other.fun_)),
        exeptionCount_(other.exeptionCount_),
        dismissed_(other.dismissed_) { other.dismissed_ = true; }

    ~ScopeGuard() noexcept(runMode != ScopeGuardRunMode::ON_SUCCESS)
    {
        if (!dismissed_)
            runScopeGuardDestructor(fun_, exeptionCount_, StaticEnum<ScopeGuardRunMode, runMode>());
    }

    void dismiss() { dismissed_ = true; }

private:
    ScopeGuard           (const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    F fun_;
    const int exeptionCount_ = getUncaughtExceptionCount();
    bool dismissed_ = false;
};


template <ScopeGuardRunMode runMode, class F> inline
auto makeGuard(F&& fun) { return ScopeGuard<runMode, std::decay_t<F>>(std::forward<F>(fun)); }
}

#define ZEN_CONCAT_SUB(X, Y) X ## Y
#define ZEN_CONCAT(X, Y) ZEN_CONCAT_SUB(X, Y)

#define ZEN_ON_SCOPE_EXIT(X)    auto ZEN_CONCAT(dummy, __LINE__) = zen::makeGuard<zen::ScopeGuardRunMode::ON_EXIT   >([&]{ X; }); (void)ZEN_CONCAT(dummy, __LINE__);
#define ZEN_ON_SCOPE_FAIL(X)    auto ZEN_CONCAT(dummy, __LINE__) = zen::makeGuard<zen::ScopeGuardRunMode::ON_FAIL   >([&]{ X; }); (void)ZEN_CONCAT(dummy, __LINE__);
#define ZEN_ON_SCOPE_SUCCESS(X) auto ZEN_CONCAT(dummy, __LINE__) = zen::makeGuard<zen::ScopeGuardRunMode::ON_SUCCESS>([&]{ X; }); (void)ZEN_CONCAT(dummy, __LINE__);

#endif //SCOPE_GUARD_H_8971632487321434

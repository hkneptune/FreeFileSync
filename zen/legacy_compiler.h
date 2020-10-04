// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef LEGACY_COMPILER_H_839567308565656789
#define LEGACY_COMPILER_H_839567308565656789

#include <version>

/*  C++ standard conformance:
    https://en.cppreference.com/w/cpp/feature_test
    https://en.cppreference.com/w/User:D41D8CD98F/feature_testing_macros
    https://isocpp.org/std/standing-documents/sd-6-sg10-feature-test-recommendations

    MSVC https://docs.microsoft.com/en-us/cpp/overview/visual-cpp-language-conformance

    GCC       https://gcc.gnu.org/projects/cxx-status.html
    libstdc++ https://gcc.gnu.org/onlinedocs/libstdc++/manual/status.html

    Clang  https://clang.llvm.org/cxx_status.html#cxx20
    libc++ https://libcxx.llvm.org/cxx2a_status.html                                     */

namespace std
{


}
//---------------------------------------------------------------------------------

//constinit, consteval
    #define constinit2 constinit //GCC, clang have it
    #define consteval2 consteval //


namespace zen
{
double fromChars(const char* first, const char* last);
const char* toChars(char* first, char* last, double num);
}

#endif //LEGACY_COMPILER_H_839567308565656789

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef LEGACY_COMPILER_H_839567308565656789
#define LEGACY_COMPILER_H_839567308565656789



//https://en.cppreference.com/w/cpp/feature_test
//https://en.cppreference.com/w/User:D41D8CD98F/feature_testing_macros
//https://isocpp.org/std/standing-documents/sd-6-sg10-feature-test-recommendations
//https://gcc.gnu.org/onlinedocs/libstdc++/manual/status.html
namespace std
{
}
//---------------------------------------------------------------------------------

//constinit, consteval
    #define constinit2 constinit //GCC has it
    #define consteval2 consteval //


namespace zen
{
double from_chars(const char* first, const char* last);
const char* to_chars(char* first, char* last, double num);
}

#endif //LEGACY_COMPILER_H_839567308565656789

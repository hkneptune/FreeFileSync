// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef LEGACY_COMPILER_H_839567308565656789
#define LEGACY_COMPILER_H_839567308565656789

    #include <numbers> //C++20

    #include <span> //requires C++20



//https://isocpp.org/std/standing-documents/sd-6-sg10-feature-test-recommendations
//https://en.cppreference.com/w/User:D41D8CD98F/feature_testing_macros
//https://gcc.gnu.org/onlinedocs/libstdc++/manual/status.html
namespace std
{

//---------------------------------------------------------------------------------



}


namespace zen
{
double from_chars(const char* first, const char* last);
const char* to_chars(char* first, char* last, double num);
}

#endif //LEGACY_COMPILER_H_839567308565656789

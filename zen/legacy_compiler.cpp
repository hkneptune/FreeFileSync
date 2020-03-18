// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "legacy_compiler.h"
#include <charconv>
//1. including this one in string_tools.h blows up VC++:
//  "An internal error has occurred in the compiler. (compiler file 'd:\agent\_work\1\s\src\vctools\Compiler\Utc\src\p2\p2symtab.c', line 2618)"
//2. using inside PCH: "fatal error C1076: compiler limit: internal heap limit reached"


#if __cpp_lib_to_chars
    #error get rid of workarounds
#endif

double zen::from_chars(const char* first, const char* last)
{
    return std::strtod(std::string(first, last).c_str(), nullptr);
}


const char* zen::to_chars(char* first, char* last, double num)
{
    const size_t bufSize = last - first;
    const int charsWritten = std::snprintf(first, bufSize, "%g", num);
    //C99: returns number of chars written if successful, < 0 or >= bufferSize on failure

    return 0 <= charsWritten && charsWritten < static_cast<int>(bufSize) ?
           first + charsWritten : first;
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "legacy_compiler.h"
#include <charconv>
/*  1. including <charconv> in header file blows up VC++:
       - string_tools.h: "An internal error has occurred in the compiler. (compiler file 'd:\agent\_work\1\s\src\vctools\Compiler\Utc\src\p2\p2symtab.c', line 2618)"
       - PCH: "fatal error C1076: compiler limit: internal heap limit reached"
        => include in separate compilation unit
    2. Disable "C/C++ -> Code Generation -> Smaller Type Check" (and PCH usage!), at least for this compilation unit: https://github.com/microsoft/STL/pull/171   */

double zen::fromChars(const char* first, const char* last)
{
    double num = 0;
    [[maybe_unused]] const std::from_chars_result rv = std::from_chars(first, last, num);
    return num;
}


const char* zen::toChars(char* first, char* last, double num)
{
    const std::to_chars_result rv = std::to_chars(first, last, num);
    return rv.ec == std::errc{} ? rv.ptr : first;
}


// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "legacy_compiler.h"
#ifdef __cpp_lib_to_chars
    #error get rid of workarounds
#endif

double zen::fromChars(const char* first, const char* last)
{
    return std::strtod(std::string(first, last).c_str(), nullptr);
}


const char* zen::toChars(char* first, char* last, double num)
{
    const size_t bufSize = last - first;
    const int charsWritten = std::snprintf(first, bufSize, "%g", num);
    //C99: returns number of chars written if successful, < 0 or >= bufferSize on failure

    return 0 <= charsWritten && charsWritten < static_cast<int>(bufSize) ?
           first + charsWritten : first;
}

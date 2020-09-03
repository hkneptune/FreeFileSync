// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef WARN_STATIC_H_08724567834560832745
#define WARN_STATIC_H_08724567834560832745

/*  Portable Compile-Time Warning
    -----------------------------
    Usage:
        warn_static("my message")      */

#define ZEN_STRINGIZE_STRING(NUM) #NUM
#define ZEN_STRINGIZE_NUMBER(NUM) ZEN_STRINGIZE_STRING(NUM)

#if   defined __GNUC__ //Clang also defines __GNUC__!
#define warn_static(MSG) \
    _Pragma(ZEN_STRINGIZE_STRING(GCC warning MSG))
#endif

#endif //WARN_STATIC_H_08724567834560832745

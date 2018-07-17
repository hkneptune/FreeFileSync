// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef BUILD_INFO_H_5928539285603428657
#define BUILD_INFO_H_5928539285603428657

//determine build info: defines ZEN_BUILD_32BIT or ZEN_BUILD_64BIT

    #ifdef __LP64__
        #define ZEN_BUILD_64BIT
    #else
        #define ZEN_BUILD_32BIT
    #endif

#ifdef ZEN_BUILD_32BIT
    static_assert(sizeof(void*) == 4);
#endif

#ifdef ZEN_BUILD_64BIT
    static_assert(sizeof(void*) == 8);
#endif

#endif //BUILD_INFO_H_5928539285603428657

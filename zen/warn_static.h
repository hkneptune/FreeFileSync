// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef WARN_STATIC_H_08724567834560832745
#define WARN_STATIC_H_08724567834560832745

/*
Portable Compile-Time Warning
-----------------------------
Usage:
    warn_static("my message")
*/

#define STATIC_WARNING_CONCAT_SUB(X, Y) X ## Y
#define STATIC_WARNING_CONCAT(X, Y) STATIC_WARNING_CONCAT_SUB(X, Y)

#define warn_static(TXT) \
    typedef int STATIC_WARNING_87903124 __attribute__ ((deprecated)); \
    enum { STATIC_WARNING_CONCAT(warn_static_dummy_value, __LINE__) = sizeof(STATIC_WARNING_87903124) };

#endif //WARN_STATIC_H_08724567834560832745

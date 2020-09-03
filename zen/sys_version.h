// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef WIN_VER_H_238470348254325
#define WIN_VER_H_238470348254325

#include "file_error.h"


namespace zen
{
struct OsVersion //keep it a POD, so that the global version constants can be used during static initialization
{
    int major = 0;
    int minor = 0;

    std::strong_ordering operator<=>(const OsVersion&) const = default;
};


struct OsVersionDetail
{
    OsVersion version;
    std::wstring osVersionRaw;
    std::wstring osName;
};
OsVersionDetail getOsVersionDetail(); //throw SysError

OsVersion getOsVersion();


}

#endif //WIN_VER_H_238470348254325

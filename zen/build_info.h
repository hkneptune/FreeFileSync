// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef BUILD_INFO_H_5928539285603428657
#define BUILD_INFO_H_5928539285603428657



namespace zen
{
enum class BuildArch
{
    bit32,
    bit64,

#ifdef __LP64__
    program = bit64
#else
    program = bit32
#endif
};

static_assert((BuildArch::program == BuildArch::bit32 ? 32 : 64) == sizeof(void*) * 8);


//harmonize with os_arch enum in update_checks table:
constexpr const char* cpuArchName = BuildArch::program == BuildArch::bit32 ? "i686": "x86-64";

}

#endif //BUILD_INFO_H_5928539285603428657

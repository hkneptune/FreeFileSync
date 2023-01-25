// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ARGON2_H_0175896874598102356081374
#define ARGON2_H_0175896874598102356081374

#include <string>

namespace zen
{
enum class Argon2Flavor { d, i, id };

std::string zargon2(zen::Argon2Flavor flavour, uint32_t mem, uint32_t passes, uint32_t parallel, uint32_t taglen,
                    const std::string_view password, const std::string_view salt);

}

#endif //ARGON2_H_0175896874598102356081374

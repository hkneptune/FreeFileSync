// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef INIT_LIBSSH2_H_42578934275823624556
#define INIT_LIBSSH2_H_42578934275823624556

namespace zen
{
void libssh2Init(); //WITHOUT OpenSSL initialization!
void libssh2TearDown();
}

#endif //INIT_LIBSSH2_H_42578934275823624556

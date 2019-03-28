// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "init_libssh2.h"
#include <cassert>
#include <libssh2_sftp.h>
#include <openssl/opensslv.h>


#ifndef LIBSSH2_OPENSSL
    #error check code when/if-ever the OpenSSL libssh2 backend is changed
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    #error OpenSSL version too old
#endif


void zen::libssh2Init()
{
    const int rc = ::libssh2_init(0);
    //we need libssh2's crypto init:
    // - initializes a few statically allocated constants => avoid (minor) race condition if these were initialized by worker threads
    // - enable proper clean up of these variables in libssh2_exit() (otherwise: memory leaks!)
    // - there are a few other OpenSSL-related initializations which might be needed (and hopefully won't hurt...)
    assert(rc == 0); //libssh2 unconditionally returns 0 => why then have a return value in first place???
    (void)rc;
}


void zen::libssh2TearDown()
{
    ::libssh2_exit();
}

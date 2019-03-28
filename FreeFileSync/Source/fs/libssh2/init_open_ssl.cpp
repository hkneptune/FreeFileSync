// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "init_open_ssl.h"
#include <cassert>
#include <openssl/ssl.h>


#ifndef OPENSSL_THREADS
    #error FFS, we are royally screwed!
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    #error OpenSSL version too old
#endif


void zen::openSslInit()
{
    //official Wiki:           https://wiki.openssl.org/index.php/Library_Initialization
    //see apps_shutdown():     https://github.com/openssl/openssl/blob/master/apps/openssl.c
    //see Curl_ossl_cleanup(): https://github.com/curl/curl/blob/master/lib/vtls/openssl.c

    //excplicitly init OpenSSL on main thread: they seem to initialize atomically! But it still might help to avoid issues:
    if (::OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT, nullptr) != 1) //https://www.openssl.org/docs/man1.1.0/ssl/OPENSSL_init_ssl.html
        assert(false);
}


void zen::openSslTearDown() {} //OpenSSL 1.1.0+ deprecates all clean up functions


struct OpenSslThreadCleanUp
{
    ~OpenSslThreadCleanUp()
    {
        //OpenSSL 1.1.0+ deprecates all clean up functions
        //=> so much the theory, in practice it leaks, of course: https://github.com/openssl/openssl/issues/6283
        OPENSSL_thread_stop();
    }
};
thread_local OpenSslThreadCleanUp tearDownOpenSslThreadData;

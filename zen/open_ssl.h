// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef OPEN_SSL_H_801974580936508934568792347506
#define OPEN_SSL_H_801974580936508934568792347506

#include "sys_error.h"


namespace zen
{
//init OpenSSL before use!
void openSslInit();
void openSslTearDown();


enum class RsaStreamType
{
    pkix,  //base-64-encoded X.509 SubjectPublicKeyInfo structure ("BEGIN PUBLIC KEY")
    pkcs1, //base-64-encoded PKCS#1 RSAPublicKey: RSA number and exponent ("BEGIN RSA PUBLIC KEY")
    raw    //raw bytes: DER-encoded PKCS#1
};

//verify signatures produced with: "openssl dgst -sha256 -sign private.pem -out file.sig file.txt"
void verifySignature(const std::string_view message,
                     const std::string_view signature,
                     const std::string_view publicKeyStream,
                     RsaStreamType streamType); //throw SysError

std::string convertRsaKey(const std::string_view keyStream, RsaStreamType typeFrom, RsaStreamType typeTo, bool publicKey); //throw SysError


bool isPuttyKeyStream(const std::string_view keyStream);
std::string convertPuttyKeyToPkix(const std::string_view keyStream, const std::string_view passphrase); //throw SysError
}

#endif //OPEN_SSL_H_801974580936508934568792347506

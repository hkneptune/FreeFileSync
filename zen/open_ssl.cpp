// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "open_ssl.h"
#include <bit> //std::endian
#include <stdexcept>
#include "base64.h"
#include "build_info.h"
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

using namespace zen;


#ifndef OPENSSL_THREADS
    #error FFS, we are royally screwed!
#endif

static_assert(OPENSSL_VERSION_NUMBER >= 0x10100000L, "OpenSSL version too old");


void zen::openSslInit()
{
    //official Wiki:           https://wiki.openssl.org/index.php/Library_Initialization
    //see apps_shutdown():     https://github.com/openssl/openssl/blob/master/apps/openssl.c
    //see Curl_ossl_cleanup(): https://github.com/curl/curl/blob/master/lib/vtls/openssl.c

    //excplicitly init OpenSSL on main thread: seems to initialize atomically! But it still might help to avoid issues:
    [[maybe_unused]] const int rv = ::OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT | OPENSSL_INIT_NO_LOAD_CONFIG, nullptr);
    assert(rv == 1); //https://www.openssl.org/docs/man1.1.0/ssl/OPENSSL_init_ssl.html
}


void zen::openSslTearDown() {}
//OpenSSL 1.1.0+ deprecates all clean up functions
//=> so much the theory, in practice it leaks, of course: https://github.com/openssl/openssl/issues/6283
//=> OpenSslThreadCleanUp

namespace
{
struct OpenSslThreadCleanUp
{
    ~OpenSslThreadCleanUp()
    {
        ::OPENSSL_thread_stop();
    }
};
thread_local OpenSslThreadCleanUp tearDownOpenSslThreadData;


/*  Sign a file using SHA-256:
        openssl dgst -sha256 -sign private.pem -out file.sig file.txt

    verify the signature: (caveat: public key expected to be in pkix format!)
        openssl dgst -sha256 -verify public.pem -signature file.sig file.txt          */


std::wstring formatOpenSSLError(const char* functionName, unsigned long ec)
{
    char errorBuf[256] = {}; //== buffer size used by ERR_error_string(); err.c: it seems the message uses at most ~200 bytes
    ::ERR_error_string_n(ec, errorBuf, sizeof(errorBuf)); //includes null-termination

    return formatSystemError(functionName, replaceCpy(_("Error code %x"), L"%x", numberTo<std::wstring>(ec)), utfTo<std::wstring>(errorBuf));
}


std::wstring formatLastOpenSSLError(const char* functionName)
{
    const auto ec = ::ERR_peek_last_error();
    ::ERR_clear_error(); //clean up for next OpenSSL operation on this thread
    return formatOpenSSLError(functionName, ec);
}

//================================================================================

std::shared_ptr<EVP_PKEY> generateRsaKeyPair(int bits) //throw SysError
{
    EVP_PKEY_CTX* keyCtx = ::EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, //int id
                                                 nullptr);     //ENGINE* e
    if (!keyCtx)
        throw SysError(formatLastOpenSSLError("EVP_PKEY_CTX_new_id"));
    ZEN_ON_SCOPE_EXIT(::EVP_PKEY_CTX_free(keyCtx));

    if (::EVP_PKEY_keygen_init(keyCtx) != 1)
        throw SysError(formatLastOpenSSLError("EVP_PKEY_keygen_init"));

    //"RSA keys set the key length during key generation rather than parameter generation"
    if (::EVP_PKEY_CTX_set_rsa_keygen_bits(keyCtx, bits) <= 0) //"[...] return a positive value for success" => effectively returns "1"
        throw SysError(formatLastOpenSSLError("EVP_PKEY_CTX_set_rsa_keygen_bits"));

    EVP_PKEY* keyPair = nullptr;
    if (::EVP_PKEY_keygen(keyCtx, &keyPair) != 1)
        throw SysError(formatLastOpenSSLError("EVP_PKEY_keygen"));

    return std::shared_ptr<EVP_PKEY>(keyPair, ::EVP_PKEY_free);
}

//================================================================================

using BioToEvpFunc = EVP_PKEY* (*)(BIO* bp, EVP_PKEY** x, pem_password_cb* cb, void* u);

std::shared_ptr<EVP_PKEY> streamToEvpKey(const std::string& keyStream, BioToEvpFunc bioToEvp, const char* functionName) //throw SysError
{
    BIO* bio = ::BIO_new_mem_buf(keyStream.c_str(), static_cast<int>(keyStream.size()));
    if (!bio)
        throw SysError(formatLastOpenSSLError("BIO_new_mem_buf"));
    ZEN_ON_SCOPE_EXIT(::BIO_free_all(bio));

    if (EVP_PKEY* evp = bioToEvp(bio,      //BIO* bp
                                 nullptr,  //EVP_PKEY** x
                                 nullptr,  //pem_password_cb* cb
                                 nullptr)) //void* u
        return std::shared_ptr<EVP_PKEY>(evp, ::EVP_PKEY_free);
    throw SysError(formatLastOpenSSLError(functionName));
}


using BioToRsaFunc = RSA* (*)(BIO* bp, RSA** x, pem_password_cb* cb, void* u);

std::shared_ptr<EVP_PKEY> streamToEvpKey(const std::string& keyStream, BioToRsaFunc bioToRsa, const char* functionName) //throw SysError
{
    BIO* bio = ::BIO_new_mem_buf(keyStream.c_str(), static_cast<int>(keyStream.size()));
    if (!bio)
        throw SysError(formatLastOpenSSLError("BIO_new_mem_buf"));
    ZEN_ON_SCOPE_EXIT(::BIO_free_all(bio));

    RSA* rsa = bioToRsa(bio,      //BIO* bp
                        nullptr,  //RSA** x
                        nullptr,  //pem_password_cb* cb
                        nullptr); //void* u
    if (!rsa)
        throw SysError(formatLastOpenSSLError(functionName));
    ZEN_ON_SCOPE_EXIT(::RSA_free(rsa));

    EVP_PKEY* evp = ::EVP_PKEY_new();
    if (!evp)
        throw SysError(formatLastOpenSSLError("EVP_PKEY_new"));
    std::shared_ptr<EVP_PKEY> sharedKey(evp, ::EVP_PKEY_free);

    if (::EVP_PKEY_set1_RSA(evp, rsa) != 1) //no ownership transfer (internally ref-counted)
        throw SysError(formatLastOpenSSLError("EVP_PKEY_set1_RSA"));

    return sharedKey;
}

//--------------------------------------------------------------------------------

std::shared_ptr<EVP_PKEY> streamToKey(const std::string& keyStream, RsaStreamType streamType, bool publicKey) //throw SysError
{
    switch (streamType)
    {
        case RsaStreamType::pkix:
            return publicKey ?
                   streamToEvpKey(keyStream, ::PEM_read_bio_PUBKEY,     "PEM_read_bio_PUBKEY") :    //throw SysError
                   streamToEvpKey(keyStream, ::PEM_read_bio_PrivateKey, "PEM_read_bio_PrivateKey"); //

        case RsaStreamType::pkcs1:
            return publicKey ?
                   streamToEvpKey(keyStream, ::PEM_read_bio_RSAPublicKey,  "PEM_read_bio_RSAPublicKey") : //throw SysError
                   streamToEvpKey(keyStream, ::PEM_read_bio_RSAPrivateKey, "PEM_read_bio_RSAPrivateKey"); //

        case RsaStreamType::raw:
            break;
    }

    auto tmp = reinterpret_cast<const unsigned char*>(keyStream.c_str());
    EVP_PKEY* evp = (publicKey ? ::d2i_PublicKey : ::d2i_PrivateKey)(EVP_PKEY_RSA,                         //int type
                                                                     nullptr,                              //EVP_PKEY** a
                                                                     &tmp, /*changes tmp pointer itself!*/ //const unsigned char** pp
                                                                     static_cast<long>(keyStream.size())); //long length
    if (!evp)
        throw SysError(formatLastOpenSSLError(publicKey ? "d2i_PublicKey" : "d2i_PrivateKey"));
    return std::shared_ptr<EVP_PKEY>(evp, ::EVP_PKEY_free);
}

//================================================================================

using EvpToBioFunc = int (*)(BIO* bio, EVP_PKEY* evp);

std::string evpKeyToStream(EVP_PKEY* evp, EvpToBioFunc evpToBio, const char* functionName) //throw SysError
{
    BIO* bio = ::BIO_new(BIO_s_mem());
    if (!bio)
        throw SysError(formatLastOpenSSLError("BIO_new"));
    ZEN_ON_SCOPE_EXIT(::BIO_free_all(bio));

    if (evpToBio(bio, evp) != 1)
        throw SysError(formatLastOpenSSLError(functionName));
    //---------------------------------------------
    const int keyLen = BIO_pending(bio);
    if (keyLen < 0)
        throw SysError(formatLastOpenSSLError("BIO_pending"));
    if (keyLen == 0)
        throw SysError(formatSystemError("BIO_pending", L"", L"Unexpected failure.")); //no more error details

    std::string keyStream(keyLen, '\0');

    if (::BIO_read(bio, &keyStream[0], keyLen) != keyLen)
        throw SysError(formatLastOpenSSLError("BIO_read"));
    return keyStream;
}


using RsaToBioFunc = int (*)(BIO* bp, RSA* x);

std::string evpKeyToStream(EVP_PKEY* evp, RsaToBioFunc rsaToBio, const char* functionName) //throw SysError
{
    BIO* bio = ::BIO_new(BIO_s_mem());
    if (!bio)
        throw SysError(formatLastOpenSSLError("BIO_new"));
    ZEN_ON_SCOPE_EXIT(::BIO_free_all(bio));

    RSA* rsa = ::EVP_PKEY_get0_RSA(evp); //unowned reference!
    if (!rsa)
        throw SysError(formatLastOpenSSLError("EVP_PKEY_get0_RSA"));

    if (rsaToBio(bio, rsa) != 1)
        throw SysError(formatLastOpenSSLError(functionName));
    //---------------------------------------------
    const int keyLen = BIO_pending(bio);
    if (keyLen < 0)
        throw SysError(formatLastOpenSSLError("BIO_pending"));
    if (keyLen == 0)
        throw SysError(formatSystemError("BIO_pending", L"", L"Unexpected failure.")); //no more error details

    std::string keyStream(keyLen, '\0');

    if (::BIO_read(bio, &keyStream[0], keyLen) != keyLen)
        throw SysError(formatLastOpenSSLError("BIO_read"));
    return keyStream;
}


//fix OpenSSL API inconsistencies:
int PEM_write_bio_PrivateKey2(BIO* bio, EVP_PKEY* key)
{
    return ::PEM_write_bio_PrivateKey(bio,      //BIO* bp
                                      key,      //EVP_PKEY* x
                                      nullptr,  //const EVP_CIPHER* enc
                                      nullptr,  //unsigned char* kstr
                                      0,        //int klen
                                      nullptr,  //pem_password_cb* cb
                                      nullptr); //void* u
}

int PEM_write_bio_RSAPrivateKey2(BIO* bio, RSA* rsa)
{
    return ::PEM_write_bio_RSAPrivateKey(bio,      //BIO* bp
                                         rsa,      //RSA* x
                                         nullptr,  //const EVP_CIPHER* enc
                                         nullptr,  //unsigned char* kstr
                                         0,        //int klen
                                         nullptr,  //pem_password_cb* cb
                                         nullptr); //void* u
}

int PEM_write_bio_RSAPublicKey2(BIO* bio, RSA* rsa) { return ::PEM_write_bio_RSAPublicKey(bio, rsa); }

//--------------------------------------------------------------------------------

std::string keyToStream(EVP_PKEY* evp, RsaStreamType streamType, bool publicKey) //throw SysError
{
    switch (streamType)
    {
        case RsaStreamType::pkix:
            return publicKey ?
                   evpKeyToStream(evp, ::PEM_write_bio_PUBKEY,      "PEM_write_bio_PUBKEY") :    //throw SysError
                   evpKeyToStream(evp, ::PEM_write_bio_PrivateKey2, "PEM_write_bio_PrivateKey"); //

        case RsaStreamType::pkcs1:
            return publicKey ?
                   evpKeyToStream(evp, ::PEM_write_bio_RSAPublicKey2,  "PEM_write_bio_RSAPublicKey") : //throw SysError
                   evpKeyToStream(evp, ::PEM_write_bio_RSAPrivateKey2, "PEM_write_bio_RSAPrivateKey"); //

        case RsaStreamType::raw:
            break;
    }

    unsigned char* buf = nullptr;
    const int bufSize = (publicKey ? ::i2d_PublicKey : ::i2d_PrivateKey)(evp, &buf);
    if (bufSize <= 0)
        throw SysError(formatLastOpenSSLError(publicKey ? "i2d_PublicKey" : "i2d_PrivateKey"));
    ZEN_ON_SCOPE_EXIT(::OPENSSL_free(buf)); //memory is only allocated for bufSize > 0

    return {reinterpret_cast<const char*>(buf), static_cast<size_t>(bufSize)};
}

//================================================================================

std::string createSignature(const std::string& message, EVP_PKEY* privateKey) //throw SysError
{
    //https://www.openssl.org/docs/manmaster/man3/EVP_DigestSign.html
    EVP_MD_CTX* mdctx = ::EVP_MD_CTX_create();
    if (!mdctx)
        throw SysError(formatSystemError("EVP_MD_CTX_create", L"", L"Unexpected failure.")); //no more error details
    ZEN_ON_SCOPE_EXIT(::EVP_MD_CTX_destroy(mdctx));

    if (::EVP_DigestSignInit(mdctx,            //EVP_MD_CTX* ctx
                             nullptr,          //EVP_PKEY_CTX** pctx
                             EVP_sha256(),     //const EVP_MD* type
                             nullptr,          //ENGINE* e
                             privateKey) != 1) //EVP_PKEY* pkey
        throw SysError(formatLastOpenSSLError("EVP_DigestSignInit"));

    if (::EVP_DigestSignUpdate(mdctx,                //EVP_MD_CTX* ctx
                               message.c_str(),      //const void* d
                               message.size()) != 1) //size_t cnt
        throw SysError(formatLastOpenSSLError("EVP_DigestSignUpdate"));

    size_t sigLenMax = 0; //"first call to EVP_DigestSignFinal returns the maximum buffer size required"
    if (::EVP_DigestSignFinal(mdctx,            //EVP_MD_CTX* ctx
                              nullptr,          //unsigned char* sigret
                              &sigLenMax) != 1) //size_t* siglen
        throw SysError(formatLastOpenSSLError("EVP_DigestSignFinal"));

    std::string signature(sigLenMax, '\0');
    size_t sigLen = sigLenMax;

    if (::EVP_DigestSignFinal(mdctx,                                           //EVP_MD_CTX* ctx
                              reinterpret_cast<unsigned char*>(&signature[0]), //unsigned char* sigret
                              &sigLen) != 1)                                   //size_t* siglen
        throw SysError(formatLastOpenSSLError("EVP_DigestSignFinal"));

    signature.resize(sigLen);
    return signature;
}


void verifySignature(const std::string& message, const std::string& signature, EVP_PKEY* publicKey) //throw SysError
{
    //https://www.openssl.org/docs/manmaster/man3/EVP_DigestVerify.html
    EVP_MD_CTX* mdctx = ::EVP_MD_CTX_create();
    if (!mdctx)
        throw SysError(formatSystemError("EVP_MD_CTX_create", L"", L"Unexpected failure.")); //no more error details
    ZEN_ON_SCOPE_EXIT(::EVP_MD_CTX_destroy(mdctx));

    if (::EVP_DigestVerifyInit(mdctx,           //EVP_MD_CTX* ctx
                               nullptr,         //EVP_PKEY_CTX** pctx
                               EVP_sha256(),    //const EVP_MD* type
                               nullptr,         //ENGINE* e
                               publicKey) != 1) //EVP_PKEY* pkey
        throw SysError(formatLastOpenSSLError("EVP_DigestVerifyInit"));

    if (::EVP_DigestVerifyUpdate(mdctx,                //EVP_MD_CTX* ctx
                                 message.c_str(),      //const void* d
                                 message.size()) != 1) //size_t cnt
        throw SysError(formatLastOpenSSLError("EVP_DigestVerifyUpdate"));

    if (::EVP_DigestVerifyFinal(mdctx,                                                     //EVP_MD_CTX* ctx
                                reinterpret_cast<const unsigned char*>(signature.c_str()), //const unsigned char* sig
                                signature.size()) != 1)                                    //size_t siglen
        throw SysError(formatLastOpenSSLError("EVP_DigestVerifyFinal"));
}
}


std::string zen::convertRsaKey(const std::string& keyStream, RsaStreamType typeFrom, RsaStreamType typeTo, bool publicKey) //throw SysError
{
    assert(typeFrom != typeTo);
    std::shared_ptr<EVP_PKEY> evp = streamToKey(keyStream, typeFrom, publicKey); //throw SysError
    return keyToStream(evp.get(), typeTo, publicKey); //throw SysError
}


void zen::verifySignature(const std::string& message, const std::string& signature, const std::string& publicKeyStream, RsaStreamType streamType) //throw SysError
{
    std::shared_ptr<EVP_PKEY> publicKey = streamToKey(publicKeyStream, streamType, true /*publicKey*/); //throw SysError
    ::verifySignature(message, signature, publicKey.get()); //throw SysError
}


namespace
{
std::wstring getSslErrorLiteral(int ec)
{
    switch (ec)
    {
            ZEN_CHECK_CASE_FOR_CONSTANT(SSL_ERROR_NONE);
            ZEN_CHECK_CASE_FOR_CONSTANT(SSL_ERROR_SSL);
            ZEN_CHECK_CASE_FOR_CONSTANT(SSL_ERROR_WANT_READ);
            ZEN_CHECK_CASE_FOR_CONSTANT(SSL_ERROR_WANT_WRITE);
            ZEN_CHECK_CASE_FOR_CONSTANT(SSL_ERROR_WANT_X509_LOOKUP);
            ZEN_CHECK_CASE_FOR_CONSTANT(SSL_ERROR_SYSCALL);
            ZEN_CHECK_CASE_FOR_CONSTANT(SSL_ERROR_ZERO_RETURN);
            ZEN_CHECK_CASE_FOR_CONSTANT(SSL_ERROR_WANT_CONNECT);
            ZEN_CHECK_CASE_FOR_CONSTANT(SSL_ERROR_WANT_ACCEPT);
            ZEN_CHECK_CASE_FOR_CONSTANT(SSL_ERROR_WANT_ASYNC);
            ZEN_CHECK_CASE_FOR_CONSTANT(SSL_ERROR_WANT_ASYNC_JOB);
            ZEN_CHECK_CASE_FOR_CONSTANT(SSL_ERROR_WANT_CLIENT_HELLO_CB);

        default:
            return L"SSL error " + numberTo<std::wstring>(ec);
    }
}


std::wstring formatX509ErrorCode(long ec)
{
    switch (ec)
    {
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_OK);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNSPECIFIED);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNABLE_TO_GET_CRL);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CERT_SIGNATURE_FAILURE);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CRL_SIGNATURE_FAILURE);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CERT_NOT_YET_VALID);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CERT_HAS_EXPIRED);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CRL_NOT_YET_VALID);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CRL_HAS_EXPIRED);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_OUT_OF_MEM);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CERT_CHAIN_TOO_LONG);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CERT_REVOKED);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_INVALID_CA);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_PATH_LENGTH_EXCEEDED);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_INVALID_PURPOSE);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CERT_UNTRUSTED);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CERT_REJECTED);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_SUBJECT_ISSUER_MISMATCH);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_AKID_SKID_MISMATCH);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_KEYUSAGE_NO_CERTSIGN);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_KEYUSAGE_NO_CRL_SIGN);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_INVALID_NON_CA);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_PROXY_PATH_LENGTH_EXCEEDED);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_KEYUSAGE_NO_DIGITAL_SIGNATURE);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_PROXY_CERTIFICATES_NOT_ALLOWED);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_INVALID_EXTENSION);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_INVALID_POLICY_EXTENSION);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_NO_EXPLICIT_POLICY);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_DIFFERENT_CRL_SCOPE);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNSUPPORTED_EXTENSION_FEATURE);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNNESTED_RESOURCE);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_PERMITTED_VIOLATION);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_EXCLUDED_VIOLATION);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_SUBTREE_MINMAX);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_APPLICATION_VERIFICATION);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNSUPPORTED_CONSTRAINT_TYPE);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNSUPPORTED_CONSTRAINT_SYNTAX);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_UNSUPPORTED_NAME_SYNTAX);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CRL_PATH_VALIDATION_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_PATH_LOOP);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_SUITE_B_INVALID_VERSION);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_SUITE_B_INVALID_ALGORITHM);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_SUITE_B_INVALID_CURVE);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_SUITE_B_INVALID_SIGNATURE_ALGORITHM);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_SUITE_B_LOS_NOT_ALLOWED);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_SUITE_B_CANNOT_SIGN_P_384_WITH_P_256);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_HOSTNAME_MISMATCH);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_EMAIL_MISMATCH);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_IP_ADDRESS_MISMATCH);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_DANE_NO_MATCH);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_EE_KEY_TOO_SMALL);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CA_KEY_TOO_SMALL);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_CA_MD_TOO_WEAK);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_INVALID_CALL);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_STORE_LOOKUP);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_NO_VALID_SCTS);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_PROXY_SUBJECT_NAME_VIOLATION);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_OCSP_VERIFY_NEEDED);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_OCSP_VERIFY_FAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(X509_V_ERR_OCSP_CERT_UNKNOWN);

        default:
            return replaceCpy<std::wstring>(L"X509 error %x", L"%x", numberTo<std::wstring>(ec));
    }
}
}

class TlsContext::Impl
{
public:
    Impl(int socket, //throw SysError
         const std::string& server,
         const Zstring* caCertFilePath /*optional: enable certificate validation*/)
    {
        ZEN_ON_SCOPE_FAIL(cleanup(); /*destructor call would lead to member double clean-up!!!*/);

        ctx_ = ::SSL_CTX_new(::TLS_client_method());
        if (!ctx_)
            throw SysError(formatLastOpenSSLError("SSL_CTX_new"));

        ssl_ = ::SSL_new(ctx_);
        if (!ssl_)
            throw SysError(formatLastOpenSSLError("SSL_new"));

        BIO* bio = ::BIO_new_socket(socket, BIO_NOCLOSE);
        if (!bio)
            throw SysError(formatLastOpenSSLError("BIO_new_socket"));
        ::SSL_set0_rbio(ssl_, bio); //pass ownership

        if (::BIO_up_ref(bio) != 1)
            throw SysError(formatLastOpenSSLError("BIO_up_ref"));
        ::SSL_set0_wbio(ssl_, bio); //pass ownership

        assert(::SSL_get_mode(ssl_) == SSL_MODE_AUTO_RETRY); //verify OpenSSL default
        ::SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);

        if (::SSL_set_tlsext_host_name(ssl_, server.c_str()) != 1) //enable SNI (Server Name Indication)
            throw SysError(formatLastOpenSSLError("SSL_set_tlsext_host_name"));

        if (caCertFilePath)
        {
            if (!::SSL_CTX_load_verify_locations(ctx_, utfTo<std::string>(*caCertFilePath).c_str(), nullptr))
                throw SysError(formatLastOpenSSLError("SSL_CTX_load_verify_locations"));
            //alternative: SSL_CTX_set_default_verify_paths(): use OpenSSL default paths considering SSL_CERT_FILE environment variable

            //1. enable check for valid certificate: see SSL_get_verify_result()
            ::SSL_set_verify(ssl_, SSL_VERIFY_PEER, nullptr);

            //2. enable check that the certificate matches our host: see SSL_get_verify_result()
            if (::SSL_set1_host(ssl_, server.c_str()) != 1) //no ownership transfer
                throw SysError(formatSystemError("SSL_set1_host", L"", L"Unexpected failure.")); //no more error details
        }

        const int rv = ::SSL_connect(ssl_); //implicitly calls SSL_set_connect_state()
        if (rv != 1)
            throw SysError(formatLastOpenSSLError("SSL_connect") + L' ' + getSslErrorLiteral(::SSL_get_error(ssl_, rv)));

        if (caCertFilePath)
        {
            const long verifyResult = ::SSL_get_verify_result(ssl_);
            if (verifyResult != X509_V_OK)
                throw SysError(formatSystemError("SSL_get_verify_result", formatX509ErrorCode(verifyResult), L""));
        }
    }

    ~Impl()
    {
        //"SSL_shutdown() must not be called if a previous fatal error has occurred on a connection"
        const bool scopeFail = std::uncaught_exceptions() > exeptionCount_;
        if (!scopeFail)
        {
            //"It is acceptable for an application to only send its shutdown alert and then close
            //the underlying connection without waiting for the peer's response."
            [[maybe_unused]] const int rv = ::SSL_shutdown(ssl_);
            assert(rv == 0); //"the close_notify was sent but the peer did not send it back yet."
        }

        cleanup();
    }

    size_t tryRead(void* buffer, size_t bytesToRead) //throw SysError; may return short, only 0 means EOF!
    {
        if (bytesToRead == 0) //"read() with a count of 0 returns zero" => indistinguishable from end of file! => check!
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));

        size_t bytesReceived = 0;
        const int rv = ::SSL_read_ex(ssl_, buffer, bytesToRead, &bytesReceived);
        if (rv != 1)
        {
            const int sslError = ::SSL_get_error(ssl_, rv);
            if (sslError == SSL_ERROR_ZERO_RETURN)
                return 0; //EOF + close_notify alert

#if OPENSSL_VERSION_NUMBER == 0x1010105fL //OpenSSL 1.1.1e
            const auto ec = ::ERR_peek_last_error();
            if (sslError == SSL_ERROR_SSL && ERR_GET_REASON(ec) == SSL_R_UNEXPECTED_EOF_WHILE_READING) //EOF: only expected for HTTP/1.0
                return 0;
#else //obsolete handling, at least in OpenSSL 1.1.1e (but valid again with OpenSSL 1.1.1f!)
            //https://github.com/openssl/openssl/issues/10880#issuecomment-575746226
            if ((sslError == SSL_ERROR_SYSCALL && ::ERR_peek_last_error() == 0)) //EOF: only expected for HTTP/1.0
                return 0;
#endif
            throw SysError(formatLastOpenSSLError("SSL_read_ex") + L' ' + getSslErrorLiteral(sslError));
        }
        assert(bytesReceived > 0); //SSL_read_ex() considers EOF an error!
        if (bytesReceived > bytesToRead) //better safe than sorry
            throw SysError(formatSystemError("SSL_read_ex", L"", L"Buffer overflow."));

        return bytesReceived; //"zero indicates end of file"
    }

    size_t tryWrite(const void* buffer, size_t bytesToWrite) //throw SysError; may return short! CONTRACT: bytesToWrite > 0
    {
        if (bytesToWrite == 0)
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));

        size_t bytesWritten = 0;
        const int rv = ::SSL_write_ex(ssl_, buffer, bytesToWrite, &bytesWritten);
        if (rv != 1)
            throw SysError(formatLastOpenSSLError("SSL_write_ex") + L' ' + getSslErrorLiteral(::SSL_get_error(ssl_, rv)));

        if (bytesWritten > bytesToWrite)
            throw SysError(formatSystemError("SSL_write_ex", L"", L"Buffer overflow."));
        if (bytesWritten == 0)
            throw SysError(formatSystemError("SSL_write_ex", L"", L"Zero bytes processed."));

        return bytesWritten;
    }

private:
    void cleanup()
    {
        if (ssl_)
            ::SSL_free(ssl_);

        if (ctx_)
            ::SSL_CTX_free(ctx_);
    }

    Impl           (const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    SSL_CTX* ctx_ = nullptr;
    SSL*     ssl_ = nullptr;
    const int exeptionCount_ = std::uncaught_exceptions();
};


zen::TlsContext::TlsContext(int socket, const Zstring& server, const Zstring* caCertFilePath) :
    pimpl_(std::make_unique<Impl>(socket, utfTo<std::string>(server), caCertFilePath)) {} //throw SysError
zen::TlsContext::~TlsContext() {}
size_t zen::TlsContext::tryRead (      void* buffer, size_t bytesToRead ) { return pimpl_->tryRead (buffer,  bytesToRead); } //throw SysError
size_t zen::TlsContext::tryWrite(const void* buffer, size_t bytesToWrite) { return pimpl_->tryWrite(buffer, bytesToWrite); } //throw SysError


bool zen::isPuttyKeyStream(const std::string& keyStream)
{
    std::string firstLine(keyStream.begin(), std::find_if(keyStream.begin(), keyStream.end(), isLineBreak<char>));
    trim(firstLine);
    return startsWith(firstLine, "PuTTY-User-Key-File-2:");
}


std::string zen::convertPuttyKeyToPkix(const std::string& keyStream, const std::string& passphrase) //throw SysError
{
    std::vector<std::string> lines;

    for (auto it = keyStream.begin();;) //=> keep local: "warning: declaration of ‘it’ shadows a previous local"
    {
        auto itLineBegin = std::find_if_not(it, keyStream.end(), isLineBreak<char>);
        if (itLineBegin == keyStream.end())
            break;

        it = std::find_if(itLineBegin + 1, keyStream.end(), isLineBreak<char>);
        lines.emplace_back(itLineBegin, it);
    }
    //----------- parse PuTTY ppk structure ----------------------------------
    auto itLine = lines.begin();
    if (itLine == lines.end() || !startsWith(*itLine, "PuTTY-User-Key-File-2: "))
        throw SysError(L"Unknown key file format");
    const std::string algorithm = afterFirst(*itLine, ' ', IfNotFoundReturn::none);
    ++itLine;

    if (itLine == lines.end() || !startsWith(*itLine, "Encryption: "))
        throw SysError(L"Unknown key encryption");
    const std::string keyEncryption = afterFirst(*itLine, ' ', IfNotFoundReturn::none);
    ++itLine;

    if (itLine == lines.end() || !startsWith(*itLine, "Comment: "))
        throw SysError(L"Invalid key comment");
    const std::string comment = afterFirst(*itLine, ' ', IfNotFoundReturn::none);
    ++itLine;

    if (itLine == lines.end() || !startsWith(*itLine, "Public-Lines: "))
        throw SysError(L"Invalid key: invalid public lines");
    size_t pubLineCount = stringTo<size_t>(afterFirst(*itLine, ' ', IfNotFoundReturn::none));
    ++itLine;

    std::string publicBlob64;
    while (pubLineCount-- != 0)
        if (itLine != lines.end())
            publicBlob64 += *itLine++;
        else
            throw SysError(L"Invalid key: incomplete public lines");

    if (itLine == lines.end() || !startsWith(*itLine, "Private-Lines: "))
        throw SysError(L"Invalid key: invalid private lines");
    size_t privLineCount = stringTo<size_t>(afterFirst(*itLine, ' ', IfNotFoundReturn::none));
    ++itLine;

    std::string privateBlob64;
    while (privLineCount-- != 0)
        if (itLine != lines.end())
            privateBlob64 += *itLine++;
        else
            throw SysError(L"Invalid key: incomplete private lines");

    if (itLine == lines.end() || !startsWith(*itLine, "Private-MAC: "))
        throw SysError(L"Invalid key: MAC missing");
    const std::string macHex = afterFirst(*itLine, ' ', IfNotFoundReturn::none);
    ++itLine;

    //----------- unpack key file elements ---------------------
    const bool keyEncrypted = keyEncryption == "aes256-cbc";
    if (!keyEncrypted && keyEncryption != "none")
        throw SysError(L"Unknown key encryption");

    if (macHex.size() % 2 != 0 || !std::all_of(macHex.begin(), macHex.end(), isHexDigit<char>))
        throw SysError(L"Invalid key: invalid MAC");

    std::string mac;
    for (size_t i = 0; i < macHex.size(); i += 2)
        mac += unhexify(macHex[i], macHex[i + 1]);

    const std::string publicBlob     = stringDecodeBase64(publicBlob64);
    const std::string privateBlobEnc = stringDecodeBase64(privateBlob64);

    std::string privateBlob;
    if (!keyEncrypted)
        privateBlob = privateBlobEnc;
    else
    {
        if (passphrase.empty())
            throw SysError(L"Passphrase required to access private key");

        const auto block1 = std::string("\0\0\0\0", 4) + passphrase;
        const auto block2 = std::string("\0\0\0\1", 4) + passphrase;

        unsigned char key[2 * SHA_DIGEST_LENGTH] = {};
        SHA1(reinterpret_cast<const unsigned char*>(block1.c_str()), block1.size(), &key[0]);                 //no-fail
        SHA1(reinterpret_cast<const unsigned char*>(block2.c_str()), block2.size(), &key[SHA_DIGEST_LENGTH]); //

        EVP_CIPHER_CTX* cipCtx = ::EVP_CIPHER_CTX_new();
        if (!cipCtx)
            throw SysError(formatSystemError("EVP_CIPHER_CTX_new", L"", L"Unexpected failure.")); //no more error details
        ZEN_ON_SCOPE_EXIT(::EVP_CIPHER_CTX_free(cipCtx));

        if (::EVP_DecryptInit_ex(cipCtx,            //EVP_CIPHER_CTX* ctx
                                 EVP_aes_256_cbc(), //const EVP_CIPHER* type
                                 nullptr,           //ENGINE* impl
                                 key,               //const unsigned char* key => implied length of 256 bit!
                                 nullptr) != 1)     //const unsigned char* iv
            throw SysError(formatLastOpenSSLError("EVP_DecryptInit_ex"));

        if (::EVP_CIPHER_CTX_set_padding(cipCtx, 0 /*padding*/) != 1)
            throw SysError(formatSystemError("EVP_CIPHER_CTX_set_padding", L"", L"Unexpected failure.")); //no more error details

        privateBlob.resize(privateBlobEnc.size() + ::EVP_CIPHER_block_size(EVP_aes_256_cbc()));
        //"EVP_DecryptUpdate() should have room for (inl + cipher_block_size) bytes"

        int decLen1 = 0;
        if (::EVP_DecryptUpdate(cipCtx,                                                         //EVP_CIPHER_CTX* ctx
                                reinterpret_cast<unsigned char*>(&privateBlob[0]),              //unsigned char* out
                                &decLen1,                                                       //int* outl
                                reinterpret_cast<const unsigned char*>(privateBlobEnc.c_str()), //const unsigned char* in
                                static_cast<int>(privateBlobEnc.size())) != 1)                  //int inl
            throw SysError(formatLastOpenSSLError("EVP_DecryptUpdate"));

        int decLen2 = 0;
        if (::EVP_DecryptFinal_ex(cipCtx,                                                  //EVP_CIPHER_CTX* ctx
                                  reinterpret_cast<unsigned char*>(&privateBlob[decLen1]), //unsigned char* outm
                                  &decLen2) != 1)                                          //int* outl
            throw SysError(formatLastOpenSSLError("EVP_DecryptFinal_ex"));

        privateBlob.resize(decLen1 + decLen2);
    }

    //----------- verify key consistency ---------------------
    std::string macKeyBlob = "putty-private-key-file-mac-key";
    if (keyEncrypted)
        macKeyBlob += passphrase;

    unsigned char macKey[SHA_DIGEST_LENGTH] = {};
    SHA1(reinterpret_cast<const unsigned char*>(macKeyBlob.c_str()), macKeyBlob.size(), &macKey[0]); //no-fail

    auto numToBeString = [](size_t n) -> std::string
    {
        static_assert(std::endian::native == std::endian::little&& sizeof(n) >= 4);
        const char* numStr = reinterpret_cast<const char*>(&n);
        return {numStr[3], numStr[2], numStr[1], numStr[0]}; //big endian!
    };

    const std::string macData = numToBeString(algorithm    .size()) + algorithm +
                                numToBeString(keyEncryption.size()) + keyEncryption +
                                numToBeString(comment      .size()) + comment +
                                numToBeString(publicBlob   .size()) + publicBlob +
                                numToBeString(privateBlob  .size()) + privateBlob;
    char md[EVP_MAX_MD_SIZE] = {};
    unsigned int mdLen = 0;
    if (!::HMAC(EVP_sha1(),                           //const EVP_MD* evp_md
                macKey,                               //const void* key
                sizeof(macKey),                       //int key_len
                reinterpret_cast<const unsigned char*>(macData.c_str()), //const unsigned char* d
                static_cast<int>(macData.size()),     //int n
                reinterpret_cast<unsigned char*>(md), //unsigned char* md
                &mdLen))                              //unsigned int* md_len
        throw SysError(formatSystemError("HMAC", L"", L"Unexpected failure.")); //no more error details

    const bool hashValid = mac == std::string_view(md, mdLen);
    if (!hashValid)
        throw SysError(formatSystemError("HMAC", L"", keyEncrypted ? L"Validation failed: wrong passphrase or corrupted key" : L"Validation failed: corrupted key"));
    //----------------------------------------------------------

    auto extractString = [](auto& it, auto itEnd)
    {
        uint32_t byteCount = 0;
        if (itEnd - it < makeSigned(sizeof(byteCount)))
            throw SysError(L"String extraction failed: unexpected end of stream");

        static_assert(std::endian::native == std::endian::little);
        char* numStr = reinterpret_cast<char*>(&byteCount);
        numStr[3] = *it++; //
        numStr[2] = *it++; //Putty uses big endian!
        numStr[1] = *it++; //
        numStr[0] = *it++; //

        if (makeUnsigned(itEnd - it) < byteCount)
            throw SysError(L"String extraction failed: unexpected end of stream(2)");

        std::string str(it, it + byteCount);
        it += byteCount;
        return str;
    };

    struct BnFree { void operator()(BIGNUM* num) const { ::BN_free(num); } };
    auto createBigNum = []
    {
        BIGNUM* bn = ::BN_new();
        if (!bn)
            throw SysError(formatLastOpenSSLError("BN_new"));
        return std::unique_ptr<BIGNUM, BnFree>(bn);
    };

    auto extractBigNum = [&extractString](auto& it, auto itEnd)
    {
        const std::string bytes = extractString(it, itEnd);

        BIGNUM* bn = ::BN_bin2bn(reinterpret_cast<const unsigned char*>(&bytes[0]), static_cast<int>(bytes.size()), nullptr);
        if (!bn)
            throw SysError(formatLastOpenSSLError("BN_bin2bn"));
        return std::unique_ptr<BIGNUM, BnFree>(bn);
    };

    auto itPub  = publicBlob .begin();
    auto itPriv = privateBlob.begin();

    auto extractStringPub  = [&] { return extractString(itPub,  publicBlob .end()); };
    auto extractStringPriv = [&] { return extractString(itPriv, privateBlob.end()); };

    auto extractBigNumPub  = [&] { return extractBigNum(itPub,  publicBlob .end()); };
    auto extractBigNumPriv = [&] { return extractBigNum(itPriv, privateBlob.end()); };

    //----------- parse public/private key blobs ----------------
    if (extractStringPub() != algorithm)
        throw SysError(L"Invalid public key stream (header)");

    if (algorithm == "ssh-rsa")
    {
        std::unique_ptr<BIGNUM, BnFree> e    = extractBigNumPub (); //
        std::unique_ptr<BIGNUM, BnFree> n    = extractBigNumPub (); //
        std::unique_ptr<BIGNUM, BnFree> d    = extractBigNumPriv(); //throw SysError
        std::unique_ptr<BIGNUM, BnFree> p    = extractBigNumPriv(); //
        std::unique_ptr<BIGNUM, BnFree> q    = extractBigNumPriv(); //
        std::unique_ptr<BIGNUM, BnFree> iqmp = extractBigNumPriv(); //

        //------ calculate missing numbers: dmp1, dmq1 -------------
        std::unique_ptr<BIGNUM, BnFree> dmp1 = createBigNum(); //
        std::unique_ptr<BIGNUM, BnFree> dmq1 = createBigNum(); //throw SysError
        std::unique_ptr<BIGNUM, BnFree> tmp  = createBigNum(); //

        BN_CTX* bnCtx = BN_CTX_new();
        if (!bnCtx)
            throw SysError(formatLastOpenSSLError("BN_CTX_new"));
        ZEN_ON_SCOPE_EXIT(::BN_CTX_free(bnCtx));

        if (::BN_sub(tmp.get(), p.get(), BN_value_one()) != 1)
            throw SysError(formatLastOpenSSLError("BN_sub"));

        if (::BN_mod(dmp1.get(), d.get(), tmp.get(), bnCtx) != 1)
            throw SysError(formatLastOpenSSLError("BN_mod"));

        if (::BN_sub(tmp.get(), q.get(), BN_value_one()) != 1)
            throw SysError(formatLastOpenSSLError("BN_sub"));

        if (::BN_mod(dmq1.get(), d.get(), tmp.get(), bnCtx) != 1)
            throw SysError(formatLastOpenSSLError("BN_mod"));
        //----------------------------------------------------------

        RSA* rsa = ::RSA_new();
        if (!rsa)
            throw SysError(formatLastOpenSSLError("RSA_new"));
        ZEN_ON_SCOPE_EXIT(::RSA_free(rsa));

        if (::RSA_set0_key(rsa, n.release(), e.release(), d.release()) != 1) //pass BIGNUM ownership
            throw SysError(formatLastOpenSSLError("RSA_set0_key"));

        if (::RSA_set0_factors(rsa, p.release(), q.release()) != 1)
            throw SysError(formatLastOpenSSLError("RSA_set0_factors"));

        if (::RSA_set0_crt_params(rsa, dmp1.release(), dmq1.release(), iqmp.release()) != 1)
            throw SysError(formatLastOpenSSLError("RSA_set0_crt_params"));

        EVP_PKEY* evp = ::EVP_PKEY_new();
        if (!evp)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_new"));
        ZEN_ON_SCOPE_EXIT(::EVP_PKEY_free(evp));

        if (::EVP_PKEY_set1_RSA(evp, rsa) != 1) //no ownership transfer (internally ref-counted)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_set1_RSA"));

        return keyToStream(evp, RsaStreamType::pkix, false /*publicKey*/); //throw SysError
    }
    //----------------------------------------------------------
    else if (algorithm == "ssh-dss")
    {
        std::unique_ptr<BIGNUM, BnFree> p   = extractBigNumPub (); //
        std::unique_ptr<BIGNUM, BnFree> q   = extractBigNumPub (); //
        std::unique_ptr<BIGNUM, BnFree> g   = extractBigNumPub (); //throw SysError
        std::unique_ptr<BIGNUM, BnFree> pub = extractBigNumPub (); //
        std::unique_ptr<BIGNUM, BnFree> pri = extractBigNumPriv(); //
        //----------------------------------------------------------

        DSA* dsa = ::DSA_new();
        if (!dsa)
            throw SysError(formatLastOpenSSLError("DSA_new"));
        ZEN_ON_SCOPE_EXIT(::DSA_free(dsa));

        if (::DSA_set0_pqg(dsa, p.release(), q.release(), g.release()) != 1) //pass BIGNUM ownership
            throw SysError(formatLastOpenSSLError("DSA_set0_pqg"));

        if (::DSA_set0_key(dsa, pub.release(), pri.release()) != 1)
            throw SysError(formatLastOpenSSLError("DSA_set0_key"));

        EVP_PKEY* evp = ::EVP_PKEY_new();
        if (!evp)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_new"));
        ZEN_ON_SCOPE_EXIT(::EVP_PKEY_free(evp));

        if (::EVP_PKEY_set1_DSA(evp, dsa) != 1) //no ownership transfer (internally ref-counted)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_set1_DSA"));

        return keyToStream(evp, RsaStreamType::pkix, false /*publicKey*/); //throw SysError
    }
    //----------------------------------------------------------
    else if (algorithm == "ecdsa-sha2-nistp256" ||
             algorithm == "ecdsa-sha2-nistp384" ||
             algorithm == "ecdsa-sha2-nistp521")
    {
        const std::string algoShort = afterLast(algorithm, '-', IfNotFoundReturn::none);
        if (extractStringPub() != algoShort)
            throw SysError(L"Invalid public key stream (header)");

        const std::string pointStream = extractStringPub();
        std::unique_ptr<BIGNUM, BnFree> pri = extractBigNumPriv(); //throw SysError
        //----------------------------------------------------------

        const int curveNid = [&]
        {
            if (algoShort == "nistp256")
                return NID_X9_62_prime256v1; //same as SECG secp256r1
            if (algoShort == "nistp384")
                return NID_secp384r1;
            if (algoShort == "nistp521")
                return NID_secp521r1;
            throw SysError(L"Unknown elliptic curve: " + utfTo<std::wstring>(algorithm));
        }();

        EC_KEY* ecKey = ::EC_KEY_new_by_curve_name(curveNid);
        if (!ecKey)
            throw SysError(formatLastOpenSSLError("EC_KEY_new_by_curve_name"));
        ZEN_ON_SCOPE_EXIT(::EC_KEY_free(ecKey));

        const EC_GROUP* ecGroup = ::EC_KEY_get0_group(ecKey);
        if (!ecGroup)
            throw SysError(formatLastOpenSSLError("EC_KEY_get0_group"));

        EC_POINT* ecPoint = ::EC_POINT_new(ecGroup);
        if (!ecPoint)
            throw SysError(formatLastOpenSSLError("EC_POINT_new"));
        ZEN_ON_SCOPE_EXIT(::EC_POINT_free(ecPoint));

        if (::EC_POINT_oct2point(ecGroup,            //const EC_GROUP* group
                                 ecPoint,            //EC_POINT* p
                                 reinterpret_cast<const unsigned char*>(&pointStream[0]), //const unsigned char* buf
                                 pointStream.size(), //size_t len
                                 nullptr) != 1)      //BN_CTX* ctx
            throw SysError(formatLastOpenSSLError("EC_POINT_oct2point"));

        if (::EC_KEY_set_public_key(ecKey, ecPoint) != 1) //no ownership transfer (internally ref-counted)
            throw SysError(formatLastOpenSSLError("EC_KEY_set_public_key"));

        if (::EC_KEY_set_private_key(ecKey, pri.get()) != 1) //no ownership transfer (internally ref-counted)
            throw SysError(formatLastOpenSSLError("EC_KEY_set_private_key"));

        EVP_PKEY* evp = ::EVP_PKEY_new();
        if (!evp)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_new"));
        ZEN_ON_SCOPE_EXIT(::EVP_PKEY_free(evp));

        if (::EVP_PKEY_set1_EC_KEY(evp, ecKey) != 1) //no ownership transfer (internally ref-counted)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_set1_EC_KEY"));

        return keyToStream(evp, RsaStreamType::pkix, false /*publicKey*/); //throw SysError
    }
    //----------------------------------------------------------
    else if (algorithm == "ssh-ed25519")
    {
        //const std::string pubStream = extractStringPub(); -> we don't need the public key
        const std::string priStream = extractStringPriv();

        EVP_PKEY* evpPriv = ::EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519,  //int type
                                                           nullptr,           //ENGINE* e
                                                           reinterpret_cast<const unsigned char*>(&priStream[0]), //const unsigned char* priv
                                                           priStream.size()); //size_t len
        if (!evpPriv)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_new_raw_private_key"));
        ZEN_ON_SCOPE_EXIT(::EVP_PKEY_free(evpPriv));

        return keyToStream(evpPriv, RsaStreamType::pkix, false /*publicKey*/); //throw SysError
    }
    else
        throw SysError(L"Unknown key algorithm: " + utfTo<std::wstring>(algorithm));
}

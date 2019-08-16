// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "open_ssl.h"
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

using namespace zen;


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

    //excplicitly init OpenSSL on main thread: seems to initialize atomically! But it still might help to avoid issues:
    [[maybe_unused]] const int rv = ::OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT, nullptr);
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


std::wstring formatOpenSSLError(const std::wstring& functionName, unsigned long ec)
{
    char errorBuf[256] = {}; //== buffer size used by ERR_error_string(); err.c: it seems the message uses at most ~200 bytes
    ::ERR_error_string_n(ec, errorBuf, sizeof(errorBuf)); //includes null-termination

    return formatSystemError(functionName, replaceCpy(_("Error Code %x"), L"%x", numberTo<std::wstring>(ec)), utfTo<std::wstring>(errorBuf));
}


std::wstring formatLastOpenSSLError(const std::wstring& functionName)
{
    const unsigned long ec = ::ERR_peek_last_error();
    ::ERR_clear_error(); //clean up for next OpenSSL operation on this thread
    return formatOpenSSLError(functionName, ec);
}

//================================================================================

std::shared_ptr<EVP_PKEY> generateRsaKeyPair(int bits) //throw SysError
{
    EVP_PKEY_CTX* keyCtx = ::EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, //int id,
                                                 nullptr);     //ENGINE* e
    if (!keyCtx)
        throw SysError(formatLastOpenSSLError(L"EVP_PKEY_CTX_new_id"));
    ZEN_ON_SCOPE_EXIT(::EVP_PKEY_CTX_free(keyCtx));

    if (::EVP_PKEY_keygen_init(keyCtx) != 1)
        throw SysError(formatLastOpenSSLError(L"EVP_PKEY_keygen_init"));

    //"RSA keys set the key length during key generation rather than parameter generation"
    if (::EVP_PKEY_CTX_set_rsa_keygen_bits(keyCtx, bits) <= 0) //"[...] return a positive value for success" => effectively returns "1"
        throw SysError(formatLastOpenSSLError(L"EVP_PKEY_CTX_set_rsa_keygen_bits"));

    EVP_PKEY* keyPair = nullptr;
    if (::EVP_PKEY_keygen(keyCtx, &keyPair) != 1)
        throw SysError(formatLastOpenSSLError(L"EVP_PKEY_keygen"));

    return std::shared_ptr<EVP_PKEY>(keyPair, ::EVP_PKEY_free);
}

//================================================================================

using BioToEvpFunc = EVP_PKEY* (*)(BIO* bp, EVP_PKEY** x, pem_password_cb* cb, void* u);

std::shared_ptr<EVP_PKEY> streamToEvpKey(const std::string& keyStream, BioToEvpFunc bioToEvp, const wchar_t* functionName) //throw SysError
{
    BIO* bio = ::BIO_new_mem_buf(keyStream.c_str(), static_cast<int>(keyStream.size()));
    if (!bio)
        throw SysError(formatLastOpenSSLError(L"BIO_new_mem_buf"));
    ZEN_ON_SCOPE_EXIT(::BIO_free_all(bio));

    if (EVP_PKEY* evpKey = bioToEvp(bio,      //BIO* bp,
                                    nullptr,  //EVP_PKEY** x,
                                    nullptr,  //pem_password_cb* cb,
                                    nullptr)) //void* u
        return std::shared_ptr<EVP_PKEY>(evpKey, ::EVP_PKEY_free);
    throw SysError(formatLastOpenSSLError(functionName));
}


using BioToRsaFunc = RSA* (*)(BIO* bp, RSA** x, pem_password_cb* cb, void* u);

std::shared_ptr<EVP_PKEY> streamToEvpKey(const std::string& keyStream, BioToRsaFunc bioToRsa, const wchar_t* functionName) //throw SysError
{
    BIO* bio = ::BIO_new_mem_buf(keyStream.c_str(), static_cast<int>(keyStream.size()));
    if (!bio)
        throw SysError(formatLastOpenSSLError(L"BIO_new_mem_buf"));
    ZEN_ON_SCOPE_EXIT(::BIO_free_all(bio));

    RSA* rsa = bioToRsa(bio,      //BIO* bp,
                        nullptr,  //RSA** x,
                        nullptr,  //pem_password_cb* cb,
                        nullptr); //void* u
    if (!rsa)
        throw SysError(formatLastOpenSSLError(functionName));
    ZEN_ON_SCOPE_EXIT(::RSA_free(rsa));

    EVP_PKEY* evpKey = ::EVP_PKEY_new();
    if (!evpKey)
        throw SysError(formatLastOpenSSLError(L"EVP_PKEY_new"));
    std::shared_ptr<EVP_PKEY> sharedKey(evpKey, ::EVP_PKEY_free);

    if (::EVP_PKEY_set1_RSA(evpKey, rsa) != 1) //calls RSA_up_ref() + transfers ownership to evpKey
        throw SysError(formatLastOpenSSLError(L"EVP_PKEY_set1_RSA"));

    return sharedKey;
}

//--------------------------------------------------------------------------------

std::shared_ptr<EVP_PKEY> streamToKey(const std::string& keyStream, RsaStreamType streamType, bool publicKey) //throw SysError
{
    switch (streamType)
    {
        case RsaStreamType::pkix:
            return publicKey ?
                   streamToEvpKey(keyStream, ::PEM_read_bio_PUBKEY,     L"PEM_read_bio_PUBKEY") :    //throw SysError
                   streamToEvpKey(keyStream, ::PEM_read_bio_PrivateKey, L"PEM_read_bio_PrivateKey"); //

        case RsaStreamType::pkcs1:
            return publicKey ?
                   streamToEvpKey(keyStream, ::PEM_read_bio_RSAPublicKey,  L"PEM_read_bio_RSAPublicKey") : //throw SysError
                   streamToEvpKey(keyStream, ::PEM_read_bio_RSAPrivateKey, L"PEM_read_bio_RSAPrivateKey"); //

        case RsaStreamType::pkcs1_raw:
            break;
    }

    auto tmp = reinterpret_cast<const unsigned char*>(keyStream.c_str());
    EVP_PKEY* evpKey = (publicKey ? ::d2i_PublicKey : ::d2i_PrivateKey)(EVP_PKEY_RSA,                         //int type,
                                                                        nullptr,                              //EVP_PKEY** a,
                                                                        &tmp, /*changes tmp pointer itself!*/ //const unsigned char** pp,
                                                                        static_cast<long>(keyStream.size())); //long length
    if (!evpKey)
        throw SysError(formatLastOpenSSLError(publicKey ? L"d2i_PublicKey" : L"d2i_PrivateKey"));
    return std::shared_ptr<EVP_PKEY>(evpKey, ::EVP_PKEY_free);
}

//================================================================================

using EvpToBioFunc = int (*)(BIO* bio, EVP_PKEY* evpKey);

std::string evpKeyToStream(EVP_PKEY* evpKey, EvpToBioFunc evpToBio, const wchar_t* functionName) //throw SysError
{
    BIO* bio = ::BIO_new(BIO_s_mem());
    if (!bio)
        throw SysError(formatLastOpenSSLError(L"BIO_new"));
    ZEN_ON_SCOPE_EXIT(::BIO_free_all(bio));

    if (evpToBio(bio, evpKey) != 1)
        throw SysError(formatLastOpenSSLError(functionName));
    //---------------------------------------------
    const int keyLen = BIO_pending(bio);
    if (keyLen < 0)
        throw SysError(formatLastOpenSSLError(L"BIO_pending"));
    if (keyLen == 0)
        throw SysError(L"BIO_pending failed."); //no more error details

    std::string keyStream(keyLen, '\0');

    if (::BIO_read(bio, &keyStream[0], keyLen) != keyLen)
        throw SysError(formatLastOpenSSLError(L"BIO_read"));
    return keyStream;
}


using RsaToBioFunc = int (*)(BIO* bp, RSA* x);

std::string evpKeyToStream(EVP_PKEY* evpKey, RsaToBioFunc rsaToBio, const wchar_t* functionName) //throw SysError
{
    BIO* bio = ::BIO_new(BIO_s_mem());
    if (!bio)
        throw SysError(formatLastOpenSSLError(L"BIO_new"));
    ZEN_ON_SCOPE_EXIT(::BIO_free_all(bio));

    RSA* rsa = ::EVP_PKEY_get0_RSA(evpKey); //unowned reference!
    if (!rsa)
        throw SysError(formatLastOpenSSLError(L"EVP_PKEY_get0_RSA"));

    if (rsaToBio(bio, rsa) != 1)
        throw SysError(formatLastOpenSSLError(functionName));
    //---------------------------------------------
    const int keyLen = BIO_pending(bio);
    if (keyLen < 0)
        throw SysError(formatLastOpenSSLError(L"BIO_pending"));
    if (keyLen == 0)
        throw SysError(L"BIO_pending failed."); //no more error details

    std::string keyStream(keyLen, '\0');

    if (::BIO_read(bio, &keyStream[0], keyLen) != keyLen)
        throw SysError(formatLastOpenSSLError(L"BIO_read"));
    return keyStream;
}


//fix OpenSSL API inconsistencies:
int PEM_write_bio_PrivateKey2(BIO* bio, EVP_PKEY* key)
{
    return ::PEM_write_bio_PrivateKey(bio,      //BIO* bp,
                                      key,      //EVP_PKEY* x,
                                      nullptr,  //const EVP_CIPHER* enc,
                                      nullptr,  //unsigned char* kstr,
                                      0,        //int klen,
                                      nullptr,  //pem_password_cb* cb,
                                      nullptr); //void* u
}

int PEM_write_bio_RSAPrivateKey2(BIO* bio, RSA* rsa)
{
    return ::PEM_write_bio_RSAPrivateKey(bio,      //BIO* bp,
                                         rsa,      //RSA* x,
                                         nullptr,  //const EVP_CIPHER* enc,
                                         nullptr,  //unsigned char* kstr,
                                         0,        //int klen,
                                         nullptr,  //pem_password_cb* cb,
                                         nullptr); //void* u
}

int PEM_write_bio_RSAPublicKey2(BIO* bio, RSA* rsa) { return ::PEM_write_bio_RSAPublicKey(bio, rsa); }

//--------------------------------------------------------------------------------

std::string keyToStream(EVP_PKEY* evpKey, RsaStreamType streamType, bool publicKey) //throw SysError
{
    switch (streamType)
    {
        case RsaStreamType::pkix:
            return publicKey ?
                   evpKeyToStream(evpKey, ::PEM_write_bio_PUBKEY,      L"PEM_write_bio_PUBKEY") :    //throw SysError
                   evpKeyToStream(evpKey, ::PEM_write_bio_PrivateKey2, L"PEM_write_bio_PrivateKey"); //

        case RsaStreamType::pkcs1:
            return publicKey ?
                   evpKeyToStream(evpKey, ::PEM_write_bio_RSAPublicKey2,  L"PEM_write_bio_RSAPublicKey") : //throw SysError
                   evpKeyToStream(evpKey, ::PEM_write_bio_RSAPrivateKey2, L"PEM_write_bio_RSAPrivateKey"); //

        case RsaStreamType::pkcs1_raw:
            break;
    }

    unsigned char* buf = nullptr;
    const int bufSize = (publicKey ? ::i2d_PublicKey : ::i2d_PrivateKey)(evpKey, &buf);
    if (bufSize <= 0)
        throw SysError(formatLastOpenSSLError(publicKey ? L"i2d_PublicKey" : L"i2d_PrivateKey"));
    ZEN_ON_SCOPE_EXIT(::OPENSSL_free(buf)); //memory is only allocated for bufSize > 0

    return { reinterpret_cast<const char*>(buf), static_cast<size_t>(bufSize) };
}

//================================================================================

std::string createSignature(const std::string& message, EVP_PKEY* privateKey) //throw SysError
{
    //https://www.openssl.org/docs/manmaster/man3/EVP_DigestSign.html
    EVP_MD_CTX* mdctx = ::EVP_MD_CTX_create();
    if (!mdctx)
        throw SysError(L"EVP_MD_CTX_create failed."); //no more error details
    ZEN_ON_SCOPE_EXIT(::EVP_MD_CTX_destroy(mdctx));

    if (::EVP_DigestSignInit(mdctx,            //EVP_MD_CTX* ctx,
                             nullptr,          //EVP_PKEY_CTX** pctx,
                             EVP_sha256(),     //const EVP_MD* type,
                             nullptr,          //ENGINE* e,
                             privateKey) != 1) //EVP_PKEY* pkey
        throw SysError(formatLastOpenSSLError(L"EVP_DigestSignInit"));

    if (::EVP_DigestSignUpdate(mdctx,                //EVP_MD_CTX* ctx,
                               message.c_str(),      //const void* d,
                               message.size()) != 1) //size_t cnt
        throw SysError(formatLastOpenSSLError(L"EVP_DigestSignUpdate"));

    size_t sigLenMax = 0; //"first call to EVP_DigestSignFinal returns the maximum buffer size required"
    if (::EVP_DigestSignFinal(mdctx,            //EVP_MD_CTX* ctx,
                              nullptr,          //unsigned char* sigret,
                              &sigLenMax) != 1) //size_t* siglen
        throw SysError(formatLastOpenSSLError(L"EVP_DigestSignFinal"));

    std::string signature(sigLenMax, '\0');
    size_t sigLen = sigLenMax;

    if (::EVP_DigestSignFinal(mdctx,                                           //EVP_MD_CTX* ctx,
                              reinterpret_cast<unsigned char*>(&signature[0]), //unsigned char* sigret,
                              &sigLen) != 1)                                   //size_t* siglen
        throw SysError(formatLastOpenSSLError(L"EVP_DigestSignFinal"));
    signature.resize(sigLen);

    return signature;
}


void verifySignature(const std::string& message, const std::string& signature, EVP_PKEY* publicKey) //throw SysError
{
    //https://www.openssl.org/docs/manmaster/man3/EVP_DigestVerify.html
    EVP_MD_CTX* mdctx = ::EVP_MD_CTX_create();
    if (!mdctx)
        throw SysError(L"EVP_MD_CTX_create failed."); //no more error details
    ZEN_ON_SCOPE_EXIT(::EVP_MD_CTX_destroy(mdctx));

    if (::EVP_DigestVerifyInit(mdctx,           //EVP_MD_CTX* ctx,
                               nullptr,         //EVP_PKEY_CTX** pctx,
                               EVP_sha256(),    //const EVP_MD* type,
                               nullptr,         //ENGINE* e,
                               publicKey) != 1) //EVP_PKEY* pkey
        throw SysError(formatLastOpenSSLError(L"EVP_DigestVerifyInit"));

    if (::EVP_DigestVerifyUpdate(mdctx,                //EVP_MD_CTX* ctx,
                                 message.c_str(),      //const void* d,
                                 message.size()) != 1) //size_t cnt
        throw SysError(formatLastOpenSSLError(L"EVP_DigestVerifyUpdate"));

    if (::EVP_DigestVerifyFinal(mdctx,                                                     //EVP_MD_CTX* ctx,
                                reinterpret_cast<const unsigned char*>(signature.c_str()), //const unsigned char* sig,
                                signature.size()) != 1)                                    //size_t siglen
        throw SysError(formatLastOpenSSLError(L"EVP_DigestVerifyFinal"));
}
}


std::string zen::convertRsaKey(const std::string& keyStream, RsaStreamType typeFrom, RsaStreamType typeTo, bool publicKey) //throw SysError
{
    assert(typeFrom != typeTo);
    std::shared_ptr<EVP_PKEY> evpKey = streamToKey(keyStream, typeFrom, publicKey); //throw SysError
    return keyToStream(evpKey.get(), typeTo, publicKey); //throw SysError
}


void zen::verifySignature(const std::string& message, const std::string& signature, const std::string& publicKeyStream, RsaStreamType streamType) //throw SysError
{
    std::shared_ptr<EVP_PKEY> publicKey = streamToKey(publicKeyStream, streamType, true /*publicKey*/); //throw SysError
    ::verifySignature(message, signature, publicKey.get()); //throw SysError
}


namespace
{
std::wstring formatSslErrorRaw(int ec)
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
    }
    return L"Unknown SSL error: " + numberTo<std::wstring>(ec);
}


std::wstring formatX509ErrorRaw(long ec)
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
    }
    return L"Unknown X509 error: " + numberTo<std::wstring>(ec);
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

        ctx_ = ::SSL_CTX_new(TLS_client_method());
        if (!ctx_)
            throw SysError(formatLastOpenSSLError(L"SSL_CTX_new"));

        ssl_ = ::SSL_new(ctx_);
        if (!ssl_)
            throw SysError(formatLastOpenSSLError(L"SSL_new"));

        BIO* bio = ::BIO_new_socket(socket, BIO_NOCLOSE);
        if (!bio)
            throw SysError(formatLastOpenSSLError(L"BIO_new_socket"));
        ::SSL_set0_rbio(ssl_, bio); //pass ownership

        if (::BIO_up_ref(bio) != 1)
            throw SysError(formatLastOpenSSLError(L"BIO_up_ref"));
        ::SSL_set0_wbio(ssl_, bio); //pass ownership

        assert(::SSL_get_mode(ssl_) == SSL_MODE_AUTO_RETRY); //verify OpenSSL default
        ::SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);

        if (::SSL_set_tlsext_host_name(ssl_, server.c_str()) != 1) //enable SNI (Server Name Indication)
            throw SysError(formatLastOpenSSLError(L"SSL_set_tlsext_host_name"));

        if (caCertFilePath)
        {
            if (!::SSL_CTX_load_verify_locations(ctx_, utfTo<std::string>(*caCertFilePath).c_str(), nullptr))
                throw SysError(formatLastOpenSSLError(L"SSL_CTX_load_verify_locations"));
            //alternative: SSL_CTX_set_default_verify_paths(): use OpenSSL default paths considering SSL_CERT_FILE environment variable

            //1. enable check for valid certificate: see SSL_get_verify_result()
            ::SSL_set_verify(ssl_, SSL_VERIFY_PEER, nullptr);

            //2. enable check that the certificate matches our host: see SSL_get_verify_result()
            if (::SSL_set1_host(ssl_, server.c_str()) != 1)
                throw SysError(L"SSL_set1_host failed."); //no more error details
        }

        const int rv = ::SSL_connect(ssl_); //implicitly calls SSL_set_connect_state()
        if (rv != 1)
            throw SysError(formatLastOpenSSLError(L"SSL_connect") + L" " + formatSslErrorRaw(::SSL_get_error(ssl_, rv)));

        if (caCertFilePath)
        {
            const long verifyResult = ::SSL_get_verify_result(ssl_);
            if (verifyResult != X509_V_OK)
                throw SysError(formatSystemError(L"SSL_get_verify_result", formatX509ErrorRaw(verifyResult), L""));
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
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

        size_t bytesReceived = 0;
        const int rv = ::SSL_read_ex(ssl_, buffer, bytesToRead, &bytesReceived);
        if (rv != 1)
        {
            const int sslError = ::SSL_get_error(ssl_, rv);
            if (sslError == SSL_ERROR_ZERO_RETURN || //EOF + close_notify alert
                (sslError == SSL_ERROR_SYSCALL && ::ERR_peek_last_error() == 0)) //EOF: only expected for HTTP/1.0
                return 0;
            throw SysError(formatLastOpenSSLError(L"SSL_read_ex") + L" " + formatSslErrorRaw(sslError));
        }
        assert(bytesReceived > 0); //SSL_read_ex() considers EOF an error!
        if (bytesReceived > bytesToRead) //better safe than sorry
            throw SysError(L"SSL_read_ex: buffer overflow.");

        return bytesReceived; //"zero indicates end of file"
    }

    size_t tryWrite(const void* buffer, size_t bytesToWrite) //throw SysError; may return short! CONTRACT: bytesToWrite > 0
    {
        if (bytesToWrite == 0)
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

        size_t bytesWritten = 0;
        const int rv = ::SSL_write_ex(ssl_, buffer, bytesToWrite, &bytesWritten);
        if (rv != 1)
            throw SysError(formatLastOpenSSLError(L"SSL_write_ex") + L" " + formatSslErrorRaw(::SSL_get_error(ssl_, rv)));

        if (bytesWritten > bytesToWrite)
            throw SysError(L"SSL_write_ex: buffer overflow.");
        if (bytesWritten == 0)
            throw SysError(L"SSL_write_ex: zero bytes processed");

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
size_t zen::TlsContext::tryRead (      void* buffer, size_t bytesToRead ) { return pimpl_->tryRead(buffer,  bytesToRead); } //throw SysError
size_t zen::TlsContext::tryWrite(const void* buffer, size_t bytesToWrite) { return pimpl_->tryWrite(buffer, bytesToWrite); } //throw SysError

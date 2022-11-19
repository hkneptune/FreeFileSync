// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "open_ssl.h"
#include <bit> //std::endian (needed for macOS)
#include "base64.h"
#include "thread.h"
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    #include <openssl/core_names.h>
    #include <openssl/encoder.h>
    #include <openssl/decoder.h>
    #include <openssl/param_build.h>
#endif


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

    assert(runningOnMainThread());
    //excplicitly init OpenSSL on main thread: seems to initialize atomically! But it still might help to avoid issues:
    [[maybe_unused]] const int rv = ::OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT | OPENSSL_INIT_NO_LOAD_CONFIG, nullptr);
    assert(rv == 1); //https://www.openssl.org/docs/man1.1.0/ssl/OPENSSL_init_ssl.html

    warn_static("probably should log")
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

std::shared_ptr<EVP_PKEY> streamToKey(const std::string& keyStream, RsaStreamType streamType, bool publicKey) //throw SysError
{
    switch (streamType)
    {
        case RsaStreamType::pkix:
        {
            BIO* bio = ::BIO_new_mem_buf(keyStream.c_str(), static_cast<int>(keyStream.size()));
            if (!bio)
                throw SysError(formatLastOpenSSLError("BIO_new_mem_buf"));
            ZEN_ON_SCOPE_EXIT(::BIO_free_all(bio));

            if (EVP_PKEY* evp = (publicKey ?
                                 ::PEM_read_bio_PUBKEY :
                                 ::PEM_read_bio_PrivateKey)
                                (bio,      //BIO* bp
                                 nullptr,  //EVP_PKEY** x
                                 nullptr,  //pem_password_cb* cb
                                 nullptr)) //void* u
                return std::shared_ptr<EVP_PKEY>(evp, ::EVP_PKEY_free);
            throw SysError(formatLastOpenSSLError(publicKey ? "PEM_read_bio_PUBKEY" : "PEM_read_bio_PrivateKey"));
        }

        case RsaStreamType::pkcs1:
        {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            EVP_PKEY* evp = nullptr;
            auto guardEvp = makeGuard<ScopeGuardRunMode::onExit>([&] { if (evp) ::EVP_PKEY_free(evp); });

            const int selection = publicKey ? OSSL_KEYMGMT_SELECT_PUBLIC_KEY : OSSL_KEYMGMT_SELECT_PRIVATE_KEY;

            OSSL_DECODER_CTX* decCtx = ::OSSL_DECODER_CTX_new_for_pkey(&evp,      //EVP_PKEY** pkey
                                                                       "PEM",     //const char* input_type
                                                                       nullptr,   //const char* input_struct
                                                                       "RSA",     //const char* keytype
                                                                       selection, //int selection
                                                                       nullptr,   //OSSL_LIB_CTX* libctx
                                                                       nullptr);  //const char* propquery
            if (!decCtx)
                throw SysError(formatLastOpenSSLError("OSSL_DECODER_CTX_new_for_pkey"));
            ZEN_ON_SCOPE_EXIT(::OSSL_DECODER_CTX_free(decCtx));

            //key stream is password-protected? => OSSL_DECODER_CTX_set_passphrase()

            const unsigned char* keyBuf = reinterpret_cast<const unsigned char*>(keyStream.c_str());
            size_t keyLen = keyStream.size();
            if (::OSSL_DECODER_from_data(decCtx, &keyBuf, &keyLen) != 1)
                throw SysError(formatLastOpenSSLError("OSSL_DECODER_from_data"));

            guardEvp.dismiss();                                     //pass ownership
            return std::shared_ptr<EVP_PKEY>(evp, ::EVP_PKEY_free); //
#else
            BIO* bio = ::BIO_new_mem_buf(keyStream.c_str(), static_cast<int>(keyStream.size()));
            if (!bio)
                throw SysError(formatLastOpenSSLError("BIO_new_mem_buf"));
            ZEN_ON_SCOPE_EXIT(::BIO_free_all(bio));

            RSA* rsa = (publicKey ?
                        ::PEM_read_bio_RSAPublicKey :
                        ::PEM_read_bio_RSAPrivateKey)(bio,      //BIO* bp
                                                      nullptr,  //RSA** x
                                                      nullptr,  //pem_password_cb* cb
                                                      nullptr); //void* u
            if (!rsa)
                throw SysError(formatLastOpenSSLError(publicKey ? "PEM_read_bio_RSAPublicKey" : "PEM_read_bio_RSAPrivateKey"));
            ZEN_ON_SCOPE_EXIT(::RSA_free(rsa));

            EVP_PKEY* evp = ::EVP_PKEY_new();
            if (!evp)
                throw SysError(formatLastOpenSSLError("EVP_PKEY_new"));
            std::shared_ptr<EVP_PKEY> sharedKey(evp, ::EVP_PKEY_free);

            if (::EVP_PKEY_set1_RSA(evp, rsa) != 1) //no ownership transfer (internally ref-counted)
                throw SysError(formatLastOpenSSLError("EVP_PKEY_set1_RSA"));

            return sharedKey;
#endif
        }

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

std::string keyToStream(const EVP_PKEY* evp, RsaStreamType streamType, bool publicKey) //throw SysError
{
    //assert(::EVP_PKEY_get_base_id(evp) == EVP_PKEY_RSA);

    switch (streamType)
    {
        case RsaStreamType::pkix:
        {
            //fix OpenSSL API inconsistencies:
            auto PEM_write_bio_PrivateKey2 = [](BIO* bio, const EVP_PKEY* key)
            {
                return ::PEM_write_bio_PrivateKey(bio,      //BIO* bp
                                                  key,      //const EVP_PKEY* x
                                                  nullptr,  //const EVP_CIPHER* enc
                                                  nullptr,  //const unsigned char* kstr
                                                  0,        //int klen
                                                  nullptr,  //pem_password_cb* cb
                                                  nullptr); //void* u
            };

            BIO* bio = ::BIO_new(BIO_s_mem());
            if (!bio)
                throw SysError(formatLastOpenSSLError("BIO_new"));
            ZEN_ON_SCOPE_EXIT(::BIO_free_all(bio));

            if ((publicKey ?
                 ::PEM_write_bio_PUBKEY :
                 PEM_write_bio_PrivateKey2)(bio, evp) != 1)
                throw SysError(formatLastOpenSSLError(publicKey ? "PEM_write_bio_PUBKEY" : "PEM_write_bio_PrivateKey"));
            //---------------------------------------------
            const int keyLen = BIO_pending(bio);
            if (keyLen < 0)
                throw SysError(formatLastOpenSSLError("BIO_pending"));
            if (keyLen == 0)
                throw SysError(formatSystemError("BIO_pending", L"", L"Unexpected failure.")); //no more error details

            std::string keyStream(keyLen, '\0');

            if (::BIO_read(bio, keyStream.data(), keyLen) != keyLen)
                throw SysError(formatLastOpenSSLError("BIO_read"));
            return keyStream;
        }

        case RsaStreamType::pkcs1:
        {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            const int selection = publicKey ? OSSL_KEYMGMT_SELECT_PUBLIC_KEY : OSSL_KEYMGMT_SELECT_PRIVATE_KEY;

            OSSL_ENCODER_CTX* encCtx = ::OSSL_ENCODER_CTX_new_for_pkey(evp,       //const EVP_PKEY* pkey
                                                                       selection, //int selection
                                                                       "PEM",     //const char* output_type
                                                                       nullptr,   //const char* output_structure
                                                                       nullptr);  //const char* propquery
            if (!encCtx)
                throw SysError(formatLastOpenSSLError("OSSL_ENCODER_CTX_new_for_pkey"));
            ZEN_ON_SCOPE_EXIT(::OSSL_ENCODER_CTX_free(encCtx));

            //password-protect stream? => OSSL_ENCODER_CTX_set_passphrase()

            unsigned char* keyBuf = nullptr;
            size_t keyLen = 0;
            if (::OSSL_ENCODER_to_data(encCtx, &keyBuf, &keyLen) != 1)
                throw SysError(formatLastOpenSSLError("OSSL_ENCODER_to_data"));
            ZEN_ON_SCOPE_EXIT(::OPENSSL_free(keyBuf));

            return {reinterpret_cast<const char*>(keyBuf), keyLen};
#else
            //fix OpenSSL API inconsistencies:
            auto PEM_write_bio_RSAPrivateKey2 = [](BIO* bio, const RSA* rsa)
            {
                return ::PEM_write_bio_RSAPrivateKey(bio,      //BIO* bp
                                                     rsa,      //const RSA* x
                                                     nullptr,  //const EVP_CIPHER* enc
                                                     nullptr,  //const unsigned char* kstr
                                                     0,        //int klen
                                                     nullptr,  //pem_password_cb* cb
                                                     nullptr); //void* u
            };
            auto PEM_write_bio_RSAPublicKey2 = [](BIO* bio, const RSA* rsa) { return ::PEM_write_bio_RSAPublicKey(bio, rsa); };

            BIO* bio = ::BIO_new(BIO_s_mem());
            if (!bio)
                throw SysError(formatLastOpenSSLError("BIO_new"));
            ZEN_ON_SCOPE_EXIT(::BIO_free_all(bio));

            const RSA* rsa = ::EVP_PKEY_get0_RSA(evp); //unowned reference!
            if (!rsa)
                throw SysError(formatLastOpenSSLError("EVP_PKEY_get0_RSA"));

            if ((publicKey ?
                 PEM_write_bio_RSAPublicKey2 :
                 PEM_write_bio_RSAPrivateKey2)(bio, rsa) != 1)
                throw SysError(formatLastOpenSSLError(publicKey ? "PEM_write_bio_RSAPublicKey" : "PEM_write_bio_RSAPrivateKey"));
            //---------------------------------------------
            const int keyLen = BIO_pending(bio);
            if (keyLen < 0)
                throw SysError(formatLastOpenSSLError("BIO_pending"));
            if (keyLen == 0)
                throw SysError(formatSystemError("BIO_pending", L"", L"Unexpected failure.")); //no more error details

            std::string keyStream(keyLen, '\0');

            if (::BIO_read(bio, keyStream.data(), keyLen) != keyLen)
                throw SysError(formatLastOpenSSLError("BIO_read"));
            return keyStream;
#endif
        }

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

    if (::EVP_DigestSignFinal(mdctx,                                              //EVP_MD_CTX* ctx
                              reinterpret_cast<unsigned char*>(signature.data()), //unsigned char* sigret
                              &sigLen) != 1)                                      //size_t* siglen
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


bool zen::isPuttyKeyStream(const std::string& keyStream)
{
    std::string firstLine(keyStream.begin(), std::find_if(keyStream.begin(), keyStream.end(), isLineBreak<char>));
    trim(firstLine);
    return startsWith(firstLine, "PuTTY-User-Key-File-2:");
}


std::string zen::convertPuttyKeyToPkix(const std::string& keyStream, const std::string& passphrase) //throw SysError
{
    std::vector<std::string_view> lines;

    split2(keyStream, isLineBreak<char>, [&lines](const std::string_view block)
    {
        if (!block.empty()) //consider Windows' <CR><LF>
            lines.push_back(block);
    });

    //----------- parse PuTTY ppk structure ----------------------------------
    auto itLine = lines.begin();
    if (itLine == lines.end() || !startsWith(*itLine, "PuTTY-User-Key-File-2: "))
        throw SysError(L"Unknown key file format");
    const std::string_view algorithm = afterFirst(*itLine, ' ', IfNotFoundReturn::none);
    ++itLine;

    if (itLine == lines.end() || !startsWith(*itLine, "Encryption: "))
        throw SysError(L"Unknown key encryption");
    const std::string_view keyEncryption = afterFirst(*itLine, ' ', IfNotFoundReturn::none);
    ++itLine;

    if (itLine == lines.end() || !startsWith(*itLine, "Comment: "))
        throw SysError(L"Invalid key comment");
    const std::string_view comment = afterFirst(*itLine, ' ', IfNotFoundReturn::none);
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
    const std::string_view macHex = afterFirst(*itLine, ' ', IfNotFoundReturn::none);
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
        ::SHA1(reinterpret_cast<const unsigned char*>(block1.c_str()), block1.size(), key);                     //no-fail
        ::SHA1(reinterpret_cast<const unsigned char*>(block2.c_str()), block2.size(), key + SHA_DIGEST_LENGTH); //

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
                                reinterpret_cast<unsigned char*>(privateBlob.data()),              //unsigned char* out
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
    ::SHA1(reinterpret_cast<const unsigned char*>(macKeyBlob.c_str()), macKeyBlob.size(), macKey); //no-fail

    auto numToBeString = [](size_t n) -> std::string
    {
        static_assert(std::endian::native == std::endian::little && sizeof(n) >= 4);
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

        BIGNUM* bn = ::BN_bin2bn(reinterpret_cast<const unsigned char*>(bytes.c_str()), static_cast<int>(bytes.size()), nullptr);
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

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        OSSL_PARAM_BLD* paramBld = ::OSSL_PARAM_BLD_new();
        if (!paramBld)
            throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_new"));
        ZEN_ON_SCOPE_EXIT(::OSSL_PARAM_BLD_free(paramBld));

        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_N, n.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(n)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_E, e.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(e)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_D, d.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(d)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_FACTOR1, p.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(p)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_FACTOR2, q.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(q)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_EXPONENT1,    dmp1.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(dmp1)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_EXPONENT2,    dmq1.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(dmq1)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_COEFFICIENT1, iqmp.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(iqmp)"));

        OSSL_PARAM* sslParams = ::OSSL_PARAM_BLD_to_param(paramBld);
        if (!sslParams)
            throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_to_param"));
        ZEN_ON_SCOPE_EXIT(::OSSL_PARAM_free(sslParams));


        EVP_PKEY_CTX* evpCtx = ::EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr);
        if (!evpCtx)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_CTX_new_from_name(RSA)"));
        ZEN_ON_SCOPE_EXIT(::EVP_PKEY_CTX_free(evpCtx));

        if (::EVP_PKEY_fromdata_init(evpCtx) != 1)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_fromdata_init"));

        EVP_PKEY* evp = nullptr;
        if (::EVP_PKEY_fromdata(evpCtx, &evp, EVP_PKEY_KEYPAIR, sslParams) != 1)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_fromdata"));
        ZEN_ON_SCOPE_EXIT(::EVP_PKEY_free(evp));
#else
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
#endif
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
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        OSSL_PARAM_BLD* paramBld = ::OSSL_PARAM_BLD_new();
        if (!paramBld)
            throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_new"));
        ZEN_ON_SCOPE_EXIT(::OSSL_PARAM_BLD_free(paramBld));

        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_FFC_P,      p.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(p)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_FFC_Q,      q.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(q)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_FFC_G,      g.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(g)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_PUB_KEY,  pub.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(pub)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_PRIV_KEY, pri.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(pri)"));

        OSSL_PARAM* sslParams = ::OSSL_PARAM_BLD_to_param(paramBld);
        if (!sslParams)
            throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_to_param"));
        ZEN_ON_SCOPE_EXIT(::OSSL_PARAM_free(sslParams));


        EVP_PKEY_CTX* evpCtx = ::EVP_PKEY_CTX_new_from_name(nullptr, "DSA", nullptr);
        if (!evpCtx)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_CTX_new_from_name(DSA)"));
        ZEN_ON_SCOPE_EXIT(::EVP_PKEY_CTX_free(evpCtx));

        if (::EVP_PKEY_fromdata_init(evpCtx) != 1)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_fromdata_init"));

        EVP_PKEY* evp = nullptr;
        if (::EVP_PKEY_fromdata(evpCtx, &evp, EVP_PKEY_KEYPAIR, sslParams) != 1)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_fromdata"));
        ZEN_ON_SCOPE_EXIT(::EVP_PKEY_free(evp));
#else
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
#endif
        return keyToStream(evp, RsaStreamType::pkix, false /*publicKey*/); //throw SysError
    }
    //----------------------------------------------------------
    else if (algorithm == "ecdsa-sha2-nistp256" ||
             algorithm == "ecdsa-sha2-nistp384" ||
             algorithm == "ecdsa-sha2-nistp521")
    {
        const std::string_view algoShort = afterLast(algorithm, '-', IfNotFoundReturn::none);
        if (extractStringPub() != algoShort)
            throw SysError(L"Invalid public key stream (header)");

        const std::string pointStream = extractStringPub();
        std::unique_ptr<BIGNUM, BnFree> pri = extractBigNumPriv(); //throw SysError
        //----------------------------------------------------------
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        const char* groupName = [&]
        {
            if (algoShort == "nistp256")
                return SN_X9_62_prime256v1; //same as SECG secp256r1
            if (algoShort == "nistp384")
                return SN_secp384r1;
            if (algoShort == "nistp521")
                return SN_secp521r1;
            throw SysError(L"Unknown elliptic curve: " + utfTo<std::wstring>(algorithm));
        }();

        OSSL_PARAM_BLD* paramBld = ::OSSL_PARAM_BLD_new();
        if (!paramBld)
            throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_new"));
        ZEN_ON_SCOPE_EXIT(::OSSL_PARAM_BLD_free(paramBld));

        if (::OSSL_PARAM_BLD_push_utf8_string(paramBld, OSSL_PKEY_PARAM_GROUP_NAME, groupName, 0) != 1)
            throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_utf8_string(group)"));

        if (::OSSL_PARAM_BLD_push_octet_string(paramBld, OSSL_PKEY_PARAM_PUB_KEY, pointStream.data(), pointStream.size()) != 1)
            throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_octet_string(pub)"));

        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_PRIV_KEY, pri.get()) != 1)
            throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(priv)"));

        OSSL_PARAM* sslParams = ::OSSL_PARAM_BLD_to_param(paramBld);
        if (!sslParams)
            throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_to_param"));
        ZEN_ON_SCOPE_EXIT(::OSSL_PARAM_free(sslParams));


        EVP_PKEY_CTX* evpCtx = ::EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
        if (!evpCtx)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_CTX_new_from_name(EC)"));
        ZEN_ON_SCOPE_EXIT(::EVP_PKEY_CTX_free(evpCtx));

        if (::EVP_PKEY_fromdata_init(evpCtx) != 1)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_fromdata_init"));

        EVP_PKEY* evp = nullptr;
        if (::EVP_PKEY_fromdata(evpCtx, &evp, EVP_PKEY_KEYPAIR, sslParams) != 1)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_fromdata"));
        ZEN_ON_SCOPE_EXIT(::EVP_PKEY_free(evp));
#else
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
                                 reinterpret_cast<const unsigned char*>(pointStream.c_str()), //const unsigned char* buf
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
#endif
        return keyToStream(evp, RsaStreamType::pkix, false /*publicKey*/); //throw SysError
    }
    //----------------------------------------------------------
    else if (algorithm == "ssh-ed25519")
    {
        //const std::string pubStream = extractStringPub(); -> we don't need the public key
        const std::string priStream = extractStringPriv();

        EVP_PKEY* evpPriv = ::EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519,  //int type
                                                           nullptr,           //ENGINE* e
                                                           reinterpret_cast<const unsigned char*>(priStream.c_str()), //const unsigned char* priv
                                                           priStream.size()); //size_t len
        if (!evpPriv)
            throw SysError(formatLastOpenSSLError("EVP_PKEY_new_raw_private_key"));
        ZEN_ON_SCOPE_EXIT(::EVP_PKEY_free(evpPriv));

        return keyToStream(evpPriv, RsaStreamType::pkix, false /*publicKey*/); //throw SysError
    }
    else
        throw SysError(L"Unknown key algorithm: " + utfTo<std::wstring>(algorithm));
}

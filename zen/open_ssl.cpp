// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "open_ssl.h"
#include <bit> //std::endian (needed for macOS)
#include "base64.h"
#include "thread.h"
#include "argon2.h"
#include "serialize.h"
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/core_names.h>
#include <openssl/encoder.h>
#include <openssl/decoder.h>
#include <openssl/param_build.h>


using namespace zen;


namespace
{
#ifndef OPENSSL_THREADS
    #error FFS, we are royally screwed!
#endif

static_assert(OPENSSL_VERSION_NUMBER >= 0x30000000L, "OpenSSL version is too old!");


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
    const auto ec = ::ERR_peek_last_error(); //"returns latest error code from the thread's error queue without modifying it" - unlike ERR_get_error()
    //ERR_get_error: "returns the earliest error code from the thread's error queue and removes the entry.
    //                This function can be called repeatedly until there are no more error codes to return."
    ::ERR_clear_error(); //clean up for next OpenSSL operation on this thread
    return formatOpenSSLError(functionName, ec);
}
}


void zen::openSslInit()
{
    //official Wiki:           https://wiki.openssl.org/index.php/Library_Initialization
    //see apps_shutdown():     https://github.com/openssl/openssl/blob/master/apps/openssl.c
    //see Curl_ossl_cleanup(): https://github.com/curl/curl/blob/master/lib/vtls/openssl.c

    assert(runningOnMainThread());
    //explicitly init OpenSSL on main thread: seems to initialize atomically! But it still might help to avoid issues:
    //https://www.openssl.org/docs/manmaster/man3/OPENSSL_init_ssl.html
    if (::OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT | OPENSSL_INIT_NO_LOAD_CONFIG, nullptr) != 1)
        logExtraError(_("Error during process initialization.") + L"\n\n" + formatLastOpenSSLError("OPENSSL_init_ssl"));
}


void zen::openSslTearDown() {}
//OpenSSL 1.1.0+ deprecates all clean up functions
//=> so much the theory, in practice it leaks, of course: https://github.com/openssl/openssl/issues/6283
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

std::shared_ptr<EVP_PKEY> streamToKey(const std::string_view keyStream, RsaStreamType streamType, bool publicKey) //throw SysError
{
    switch (streamType)
    {
        case RsaStreamType::pkix:
        {
            BIO* bio = ::BIO_new_mem_buf(keyStream.data(), static_cast<int>(keyStream.size()));
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

#if 0 //key stream is password-protected? => OSSL_DECODER_CTX_set_passphrase()
            if (!password.empty())
                if (::OSSL_DECODER_CTX_set_passphrase(decCtx,                                                   //OSSL_DECODER_CTX *ctx
                                                      reinterpret_cast<const unsigned char*>(password.c_str()), //const unsigned char* kstr
                                                      password.size()) != 1)                                    //size_t klen
                    throw SysError(formatLastOpenSSLError("OSSL_DECODER_CTX_set_passphrase"));
#endif

            const unsigned char* keyBuf = reinterpret_cast<const unsigned char*>(keyStream.data());
            size_t keyLen = keyStream.size();
            if (::OSSL_DECODER_from_data(decCtx, &keyBuf, &keyLen) != 1)
                throw SysError(formatLastOpenSSLError("OSSL_DECODER_from_data"));

            guardEvp.dismiss();                                     //pass ownership
            return std::shared_ptr<EVP_PKEY>(evp, ::EVP_PKEY_free); //
        }

        case RsaStreamType::raw:
            break;
    }

    auto tmp = reinterpret_cast<const unsigned char*>(keyStream.data());
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
                throw SysError(formatSystemError("BIO_pending", L"", L"No more error details.")); //no more error details

            std::string keyStream(keyLen, '\0');

            if (::BIO_read(bio, keyStream.data(), keyLen) != keyLen)
                throw SysError(formatLastOpenSSLError("BIO_read"));
            return keyStream;
        }

        case RsaStreamType::pkcs1:
        {
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

std::string createHash(const std::string_view str, const EVP_MD* type) //throw SysError
{
    std::string output(EVP_MAX_MD_SIZE, '\0');
    unsigned int bytesWritten = 0;
#if 1
    //https://www.openssl.org/docs/manmaster/man3/EVP_Digest.html
    if (::EVP_Digest(str.data(),    //const void* data
                     str.size(),    //size_t count
                     reinterpret_cast<unsigned char*>(output.data()), //unsigned char* md
                     &bytesWritten, //unsigned int* size
                     type,          //const EVP_MD* type
                     nullptr) != 1) //ENGINE* impl
        throw SysError(formatLastOpenSSLError("EVP_Digest"));
#else //streaming version
    EVP_MD_CTX* mdctx = ::EVP_MD_CTX_new();
    if (!mdctx)
        throw SysError(formatSystemError("EVP_MD_CTX_new", L"", L"No more error details.")); //no more error details
    ZEN_ON_SCOPE_EXIT(::EVP_MD_CTX_free(mdctx));

    if (::EVP_DigestInit(mdctx,      //EVP_MD_CTX* ctx
                         type) != 1) //const EVP_MD* type
        throw SysError(formatLastOpenSSLError("EVP_DigestInit"));

    if (::EVP_DigestUpdate(mdctx,            //EVP_MD_CTX* ctx
                           str.data(),       //const void*
                           str.size()) != 1) //size_t cnt);
        throw SysError(formatLastOpenSSLError("EVP_DigestUpdate"));

    if (::EVP_DigestFinal_ex(mdctx,                                           //EVP_MD_CTX* ctx
                             reinterpret_cast<unsigned char*>(output.data()), //unsigned char* md
                             &bytesWritten) != 1)                             //unsigned int* s
        throw SysError(formatLastOpenSSLError("EVP_DigestFinal_ex"));
#endif
    output.resize(bytesWritten);
    return output;
}


std::string createSignature(const std::string_view message, EVP_PKEY* privateKey) //throw SysError
{
    //https://www.openssl.org/docs/manmaster/man3/EVP_DigestSign.html
    EVP_MD_CTX* mdctx = ::EVP_MD_CTX_new();
    if (!mdctx)
        throw SysError(formatSystemError("EVP_MD_CTX_new", L"", L"No more error details.")); //no more error details
    ZEN_ON_SCOPE_EXIT(::EVP_MD_CTX_free(mdctx));

    if (::EVP_DigestSignInit(mdctx,            //EVP_MD_CTX* ctx
                             nullptr,          //EVP_PKEY_CTX** pctx
                             EVP_sha256(),     //const EVP_MD* type
                             nullptr,          //ENGINE* e
                             privateKey) != 1) //EVP_PKEY* pkey
        throw SysError(formatLastOpenSSLError("EVP_DigestSignInit"));

    if (::EVP_DigestSignUpdate(mdctx,                //EVP_MD_CTX* ctx
                               message.data(),       //const void* d
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


void verifySignature(const std::string_view message, const std::string_view signature, EVP_PKEY* publicKey) //throw SysError
{
    //https://www.openssl.org/docs/manmaster/man3/EVP_DigestVerify.html
    EVP_MD_CTX* mdctx = ::EVP_MD_CTX_new();
    if (!mdctx)
        throw SysError(formatSystemError("EVP_MD_CTX_new", L"", L"No more error details.")); //no more error details
    ZEN_ON_SCOPE_EXIT(::EVP_MD_CTX_free(mdctx));

    if (::EVP_DigestVerifyInit(mdctx,           //EVP_MD_CTX* ctx
                               nullptr,         //EVP_PKEY_CTX** pctx
                               EVP_sha256(),    //const EVP_MD* type
                               nullptr,         //ENGINE* e
                               publicKey) != 1) //EVP_PKEY* pkey
        throw SysError(formatLastOpenSSLError("EVP_DigestVerifyInit"));

    if (::EVP_DigestVerifyUpdate(mdctx,                //EVP_MD_CTX* ctx
                                 message.data(),       //const void* d
                                 message.size()) != 1) //size_t cnt
        throw SysError(formatLastOpenSSLError("EVP_DigestVerifyUpdate"));

    if (::EVP_DigestVerifyFinal(mdctx,                                                    //EVP_MD_CTX* ctx
                                reinterpret_cast<const unsigned char*>(signature.data()), //const unsigned char* sig
                                signature.size()) != 1)                                   //size_t siglen
        throw SysError(formatLastOpenSSLError("EVP_DigestVerifyFinal"));
}
}


std::string zen::convertRsaKey(const std::string_view keyStream, RsaStreamType typeFrom, RsaStreamType typeTo, bool publicKey) //throw SysError
{
    assert(typeFrom != typeTo);
    std::shared_ptr<EVP_PKEY> evp = streamToKey(keyStream, typeFrom, publicKey); //throw SysError
    return keyToStream(evp.get(), typeTo, publicKey); //throw SysError
}


void zen::verifySignature(const std::string_view message, const std::string_view signature, const std::string_view publicKeyStream, RsaStreamType streamType) //throw SysError
{
    std::shared_ptr<EVP_PKEY> publicKey = streamToKey(publicKeyStream, streamType, true /*publicKey*/); //throw SysError
    ::verifySignature(message, signature, publicKey.get()); //throw SysError
}


bool zen::isPuttyKeyStream(const std::string_view keyStream)
{
    return startsWith(trimCpy(keyStream, TrimSide::left), "PuTTY-User-Key-File-");
}


std::string zen::convertPuttyKeyToPkix(const std::string_view keyStream, const std::string_view passphrase) //throw SysError
{
    std::vector<std::string_view> lines;

    split2(keyStream, isLineBreak<char>, [&lines](const std::string_view block)
    {
        if (!block.empty()) //consider Windows' <CR><LF>
            lines.push_back(block);
    });

    //----------- parse PuTTY ppk structure ----------------------------------
    auto itLine = lines.begin();

    auto lineStartsWith = [&](const char* str)
    {
        return itLine != lines.end() && startsWith(*itLine, str);
    };

    const int ppkFormat = [&]
    {
        if (lineStartsWith("PuTTY-User-Key-File-2: "))
            return 2;
        else if (lineStartsWith("PuTTY-User-Key-File-3: "))
            return 3;
        else
            throw SysError(L"Unknown key file format");
    }();

    const std::string_view algorithm = afterFirst(*itLine++, ' ', IfNotFoundReturn::none);

    if (!lineStartsWith("Encryption: "))
        throw SysError(L"Missing key encryption");
    const std::string_view keyEncryption = afterFirst(*itLine++, ' ', IfNotFoundReturn::none);

    const bool keyEncrypted = keyEncryption == "aes256-cbc";
    if (!keyEncrypted && keyEncryption != "none")
        throw SysError(L"Unknown key encryption");

    if (!lineStartsWith("Comment: "))
        throw SysError(L"Missing comment");
    const std::string_view comment = afterFirst(*itLine++, ' ', IfNotFoundReturn::none);

    if (!lineStartsWith("Public-Lines: "))
        throw SysError(L"Missing public lines");
    size_t pubLineCount = stringTo<size_t>(afterFirst(*itLine++, ' ', IfNotFoundReturn::none));

    std::string publicBlob64;
    while (pubLineCount-- != 0)
        if (itLine != lines.end())
            publicBlob64 += *itLine++;
        else
            throw SysError(L"Invalid key: incomplete public lines");

    Argon2Flavor argonFlavor = Argon2Flavor::d;
    uint32_t argonMemory = 0;
    uint32_t argonPasses = 0;
    uint32_t argonParallelism = 0;
    std::string argonSalt;
    if (ppkFormat >= 3 && keyEncrypted)
    {
        if (!lineStartsWith("Key-Derivation: "))
            throw SysError(L"Missing Argon2 parameter: Key-Derivation");
        const std::string_view keyDerivation = afterFirst(*itLine++, ' ', IfNotFoundReturn::none);

        argonFlavor = [&]
        {
            if (keyDerivation == "Argon2d")
                return Argon2Flavor::d;
            else if (keyDerivation == "Argon2i")
                return Argon2Flavor::i;
            else if (keyDerivation == "Argon2id")
                return Argon2Flavor::id;
            else
                throw SysError(L"Unexpected Argon2 parameter for Key-Derivation");
        }();

        if (!lineStartsWith("Argon2-Memory: "))
            throw SysError(L"Missing Argon2 parameter: Argon2-Memory");
        argonMemory = stringTo<uint32_t>(afterFirst(*itLine++, ' ', IfNotFoundReturn::none));

        if (!lineStartsWith("Argon2-Passes: "))
            throw SysError(L"Missing Argon2 parameter: Argon2-Passes");
        argonPasses = stringTo<uint32_t>(afterFirst(*itLine++, ' ', IfNotFoundReturn::none));

        if (!lineStartsWith("Argon2-Parallelism: "))
            throw SysError(L"Missing Argon2 parameter: Argon2-Parallelism");
        argonParallelism = stringTo<uint32_t>(afterFirst(*itLine++, ' ', IfNotFoundReturn::none));

        if (!lineStartsWith("Argon2-Salt: "))
            throw SysError(L"Missing Argon2 parameter: Argon2-Salt");
        const std::string_view argonSaltHex = afterFirst(*itLine++, ' ', IfNotFoundReturn::none);

        if (argonSaltHex.size() % 2 != 0 || !std::all_of(argonSaltHex.begin(), argonSaltHex.end(), isHexDigit<char>))
            throw SysError(L"Invalid Argon2 parameter: Argon2-Salt");

        for (size_t i = 0; i < argonSaltHex.size(); i += 2)
            argonSalt += unhexify(argonSaltHex[i], argonSaltHex[i + 1]);
    }

    if (!lineStartsWith("Private-Lines: "))
        throw SysError(L"Missing private lines");
    size_t privLineCount = stringTo<size_t>(afterFirst(*itLine++, ' ', IfNotFoundReturn::none));

    std::string privateBlob64;
    while (privLineCount-- != 0)
        if (itLine != lines.end())
            privateBlob64 += *itLine++;
        else
            throw SysError(L"Invalid key: incomplete private lines");

    if (!lineStartsWith("Private-MAC: "))
        throw SysError(L"MAC missing"); //apparently "Private-Hash" is/was possible here: maybe with ppk version 1!?
    const std::string_view macHex = afterFirst(*itLine++, ' ', IfNotFoundReturn::none);

    //----------- unpack key file elements ---------------------
    if (macHex.size() % 2 != 0 || !std::all_of(macHex.begin(), macHex.end(), isHexDigit<char>))
        throw SysError(L"Invalid key: invalid MAC");

    std::string mac;
    for (size_t i = 0; i < macHex.size(); i += 2)
        mac += unhexify(macHex[i], macHex[i + 1]);

    const std::string publicBlob     = stringDecodeBase64(publicBlob64);
    const std::string privateBlobEnc = stringDecodeBase64(privateBlob64);

    std::string privateBlob;
    std::string macKeyFmt3;

    if (!keyEncrypted)
        privateBlob = privateBlobEnc;
    else
    {
        if (passphrase.empty())
            throw SysError(L"Passphrase required to access private key");

        const EVP_CIPHER* const cipher = EVP_aes_256_cbc();
        std::string decryptKey;
        std::string iv;
        if (ppkFormat >= 3)
        {
            decryptKey.resize(::EVP_CIPHER_get_key_length(cipher));
            iv        .resize(::EVP_CIPHER_get_iv_length (cipher));
            macKeyFmt3.resize(32);

            const std::string argonBlob = zargon2(argonFlavor, argonMemory, argonPasses, argonParallelism,
                                                  static_cast<uint32_t>(decryptKey.size() + iv.size() + macKeyFmt3.size()), passphrase, argonSalt);
            MemoryStreamIn streamIn(argonBlob);
            readArray(streamIn, decryptKey.data(), decryptKey.size()); //
            readArray(streamIn, iv        .data(), iv        .size()); //throw SysErrorUnexpectedEos
            readArray(streamIn, macKeyFmt3.data(), macKeyFmt3.size()); //
        }
        else
        {
            decryptKey = createHash(std::string("\0\0\0\0", 4) + passphrase, EVP_sha1()) + //throw SysError
                         createHash(std::string("\0\0\0\1", 4) + passphrase, EVP_sha1());  //
            decryptKey.resize(::EVP_CIPHER_get_key_length(cipher)); //PuTTYgen only uses first 32 bytes as key (== key length of EVP_aes_256_cbc)

            iv.assign(::EVP_CIPHER_get_iv_length(cipher), 0); //initialization vector is 16-byte-range of zeros (== default for EVP_aes_256_cbc)
        }

        EVP_CIPHER_CTX* cipCtx = ::EVP_CIPHER_CTX_new();
        if (!cipCtx)
            throw SysError(formatSystemError("EVP_CIPHER_CTX_new", L"", L"No more error details.")); //no more error details
        ZEN_ON_SCOPE_EXIT(::EVP_CIPHER_CTX_free(cipCtx));

        if (::EVP_DecryptInit(cipCtx, //EVP_CIPHER_CTX* ctx
                              cipher, //const EVP_CIPHER* type
                              reinterpret_cast<const unsigned char*>(decryptKey.c_str()), //const unsigned char* key
                              reinterpret_cast<const unsigned char*>(iv.c_str())) != 1)   //const unsigned char* iv  => nullptr = 16-byte zeros for EVP_aes_256_cbc
            throw SysError(formatLastOpenSSLError("EVP_DecryptInit_ex"));

        if (::EVP_CIPHER_CTX_set_padding(cipCtx, 0 /*padding*/) != 1)
            throw SysError(formatSystemError("EVP_CIPHER_CTX_set_padding", L"", L"No more error details.")); //no more error details

        privateBlob.resize(privateBlobEnc.size() + ::EVP_CIPHER_block_size(EVP_aes_256_cbc()));
        //"EVP_DecryptUpdate() should have room for (inl + cipher_block_size) bytes"

        int decLen1 = 0;
        if (::EVP_DecryptUpdate(cipCtx,                                                         //EVP_CIPHER_CTX* ctx
                                reinterpret_cast<unsigned char*>(privateBlob.data()),           //unsigned char* out
                                &decLen1,                                                       //int* outl
                                reinterpret_cast<const unsigned char*>(privateBlobEnc.c_str()), //const unsigned char* in
                                static_cast<int>(privateBlobEnc.size())) != 1)                  //int inl
            throw SysError(formatLastOpenSSLError("EVP_DecryptUpdate"));

        int decLen2 = 0;
        if (::EVP_DecryptFinal(cipCtx,                                                  //EVP_CIPHER_CTX* ctx
                               reinterpret_cast<unsigned char*>(&privateBlob[decLen1]), //unsigned char* outm
                               &decLen2) != 1)                                          //int* outl
            throw SysError(formatLastOpenSSLError("EVP_DecryptFinal_ex"));

        privateBlob.resize(decLen1 + decLen2);
    }

    //----------- verify key consistency ---------------------
    std::string macKey;
    if (ppkFormat >= 3)
        macKey = macKeyFmt3;
    else
        macKey = createHash(std::string("putty-private-key-file-mac-key") + (keyEncrypted ? passphrase : ""), EVP_sha1()); //throw SysError

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
    if (!::HMAC(ppkFormat <= 2 ? EVP_sha1() : EVP_sha256(), //const EVP_MD* evp_md
                macKey.c_str(),                             //const void* key
                static_cast<int>(macKey.size()),            //int key_len
                reinterpret_cast<const unsigned char*>(macData.c_str()), //const unsigned char* d
                static_cast<int>(macData.size()),           //int n
                reinterpret_cast<unsigned char*>(md),       //unsigned char* md
                &mdLen))                                    //unsigned int* md_len
        throw SysError(formatSystemError("HMAC", L"", L"No more error details.")); //no more error details

    if (mac != std::string_view(md, mdLen))
        throw SysError(keyEncrypted ? L"Wrong passphrase (or corrupted key)" : L"Validation failed: corrupted key");
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

        OSSL_PARAM_BLD* paramBld = ::OSSL_PARAM_BLD_new();
        if (!paramBld)
            throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_new"));
        ZEN_ON_SCOPE_EXIT(::OSSL_PARAM_BLD_free(paramBld));

        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_N,               n.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(n)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_E,               e.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(e)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_D,               d.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(d)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_FACTOR1,         p.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(p)"));
        if (::OSSL_PARAM_BLD_push_BN(paramBld, OSSL_PKEY_PARAM_RSA_FACTOR2,         q.get()) != 1) throw SysError(formatLastOpenSSLError("OSSL_PARAM_BLD_push_BN(q)"));
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
        throw SysError(L"Unsupported key algorithm: " + utfTo<std::wstring>(algorithm));
    /* PuTTYgen supports many more (which are not yet supported by libssh2):
        - rsa-sha2-256
        - rsa-sha2-512
        - ssh-ed448
        - ssh-dss-cert-v01@openssh.com
        - ssh-rsa-cert-v01@openssh.com
        - rsa-sha2-256-cert-v01@openssh.com
        - rsa-sha2-512-cert-v01@openssh.com
        - ssh-ed25519-cert-v01@openssh.com
        - ecdsa-sha2-nistp256-cert-v01@openssh.com
        - ecdsa-sha2-nistp384-cert-v01@openssh.com
        - ecdsa-sha2-nistp521-cert-v01@openssh.com     */
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "curl_wrap.h"
#include <zen/http.h>
#include <zen/open_ssl.h>
#include <zen/thread.h>
    #include <fcntl.h>

using namespace zen;


namespace
{
int curlInitLevel = 0; //support interleaving initialization calls!
//zero-initialized POD => not subject to static initialization order fiasco
}

void zen::libcurlInit()
{
    assert(runningOnMainThread()); //all OpenSSL/libssh2/libcurl require init on main thread!
    assert(curlInitLevel >= 0);
    if (++curlInitLevel != 1) //non-atomic => require call from main thread
        return;

    openSslInit();

    try
    {
        ASSERT_SYSERROR(::curl_global_init(CURL_GLOBAL_NOTHING /*CURL_GLOBAL_DEFAULT = CURL_GLOBAL_SSL|CURL_GLOBAL_WIN32*/) == CURLE_OK);
    }
    catch (const SysError& e) { logExtraError(_("Error during process initialization.") + L"\n\n" + e.toString()); }
}


void zen::libcurlTearDown()
{
    assert(runningOnMainThread()); //+ avoid race condition on "curlInitLevel"
    assert(curlInitLevel >= 1);
    if (--curlInitLevel != 0)
        return;

    ::curl_global_cleanup();
    openSslTearDown();
}


HttpSession::HttpSession(const Zstring& server, bool useTls, const Zstring& caCertFilePath) : //throw SysError
    serverPrefix_((useTls ? "https://" : "http://") + utfTo<std::string>(server)),
    caCertFilePath_(utfTo<std::string>(caCertFilePath)) {}


HttpSession::~HttpSession()
{
    if (easyHandle_)
        ::curl_easy_cleanup(easyHandle_);
}


HttpSession::Result HttpSession::perform(const std::string& serverRelPath,
                                         const std::vector<std::string>& extraHeaders, const std::vector<CurlOption>& extraOptions,
                                         const std::function<void  (std::span<const char> buf)>& writeResponse /*throw X*/, //optional
                                         const std::function<size_t(std::span<      char> buf)>& readRequest   /*throw X*/, //optional; return "bytesToRead" bytes unless end of stream!
                                         const std::function<void(const std::string_view& header)>& receiveHeader /*throw X*/,
                                         int timeoutSec) //throw SysError, X
{
    if (!easyHandle_)
    {
        easyHandle_ = ::curl_easy_init();
        if (!easyHandle_)
            throw SysError(formatSystemError("curl_easy_init", formatCurlStatusCode(CURLE_OUT_OF_MEMORY), L""));
    }
    else
        ::curl_easy_reset(easyHandle_);

    auto setCurlOption = [easyHandle = easyHandle_](const CurlOption& curlOpt) //throw SysError
    {
        if (const CURLcode rc = ::curl_easy_setopt(easyHandle, curlOpt.option, curlOpt.value);
            rc != CURLE_OK)
            throw SysError(formatSystemError("curl_easy_setopt(" + numberTo<std::string>(static_cast<int>(curlOpt.option)) + ")",
                                             formatCurlStatusCode(rc), utfTo<std::wstring>(::curl_easy_strerror(rc))));
    };

    char curlErrorBuf[CURL_ERROR_SIZE] = {};
    setCurlOption({CURLOPT_ERRORBUFFER, curlErrorBuf}); //throw SysError

    setCurlOption({CURLOPT_USERAGENT, "FreeFileSync"}); //throw SysError
    //default value; may be overwritten by caller

    setCurlOption({CURLOPT_URL, (serverPrefix_ + serverRelPath).c_str()}); //throw SysError

    setCurlOption({CURLOPT_ACCEPT_ENCODING, ""}); //throw SysError
    //libcurl: generate Accept-Encoding header containing all built-in supported encodings
    //=> usually generates "Accept-Encoding: deflate, gzip" - note: "gzip" used by Google Drive

    setCurlOption({CURLOPT_NOSIGNAL, 1}); //throw SysError
    //thread-safety: https://curl.haxx.se/libcurl/c/threadsafe.html

    setCurlOption({CURLOPT_CONNECTTIMEOUT, timeoutSec}); //throw SysError

    //CURLOPT_TIMEOUT: "Since this puts a hard limit for how long time a request is allowed to take, it has limited use in dynamic use cases with varying transfer times."
    setCurlOption({CURLOPT_LOW_SPEED_TIME, timeoutSec}); //throw SysError
    setCurlOption({CURLOPT_LOW_SPEED_LIMIT, 1 /*[bytes]*/}); //throw SysError
    //can't use "0" which means "inactive", so use some low number

    //CURLOPT_SERVER_RESPONSE_TIMEOUT: does not apply to HTTP


    std::exception_ptr userCallbackException;

    //libcurl does *not* set FD_CLOEXEC for us! https://github.com/curl/curl/issues/2252
    auto onSocketCreate = [&](curl_socket_t curlfd, curlsocktype purpose)
    {
        assert(::fcntl(curlfd, F_GETFD) == 0);
        if (::fcntl(curlfd, F_SETFD, FD_CLOEXEC) == -1) //=> RACE-condition if other thread calls fork/execv before this thread sets FD_CLOEXEC!
        {
            userCallbackException = std::make_exception_ptr(SysError(formatSystemError("fcntl(FD_CLOEXEC)", errno)));
            return CURL_SOCKOPT_ERROR;
        }
        return CURL_SOCKOPT_OK;
    };

    using SocketCbType = decltype(onSocketCreate);
    using SocketCbWrapperType =            int (*)(SocketCbType* clientp, curl_socket_t curlfd, curlsocktype purpose); //needed for cdecl function pointer cast
    SocketCbWrapperType onSocketCreateWrapper = [](SocketCbType* clientp, curl_socket_t curlfd, curlsocktype purpose)
    {
        return (*clientp)(curlfd, purpose); //free this poor little C-API from its shackles and redirect to a proper lambda
    };

    setCurlOption({CURLOPT_SOCKOPTFUNCTION, onSocketCreateWrapper}); //throw SysError
    setCurlOption({CURLOPT_SOCKOPTDATA, &onSocketCreate}); //throw SysError

    //libcurl forwards this char-string to OpenSSL as is, which - thank god - accepts UTF8
    if (caCertFilePath_.empty())
    {
        setCurlOption({CURLOPT_CAINFO, 0}); //throw SysError
        setCurlOption({CURLOPT_SSL_VERIFYPEER, 0}); //throw SysError
        setCurlOption({CURLOPT_SSL_VERIFYHOST, 0}); //throw SysError
        //see remarks in ftp.cpp
    }
    else
        setCurlOption({CURLOPT_CAINFO, caCertFilePath_.c_str()}); //throw SysError
    //hopefully latest version from https://curl.haxx.se/docs/caextract.html
    //CURLOPT_SSL_VERIFYPEER => already active by default
    //CURLOPT_SSL_VERIFYHOST =>

    //---------------------------------------------------
    auto onHeaderReceived = [&](const char* buffer, size_t len)
    {
        try
        {
            receiveHeader({buffer, len}); //throw X
            return len;
        }
        catch (...)
        {
            userCallbackException = std::current_exception();
            return len + 1; //signal error condition => CURLE_WRITE_ERROR
        }
    };
    curl_write_callback onHeaderReceivedWrapper = [](/*const*/ char* buffer, size_t size, size_t nitems, void* callbackData)
    {
        return (*static_cast<decltype(onHeaderReceived)*>(callbackData))(buffer, size * nitems); //free this poor little C-API from its shackles and redirect to a proper lambda
    };
    //---------------------------------------------------
    auto onBytesReceived = [&](const char* buffer, size_t bytesToWrite)
    {
        try
        {
            writeResponse({buffer, bytesToWrite}); //throw X
            //[!] let's NOT use "incomplete write Posix semantics" for libcurl!
            //who knows if libcurl buffers properly, or if it sends incomplete packages!?
            return bytesToWrite;
        }
        catch (...)
        {
            userCallbackException = std::current_exception();
            return bytesToWrite + 1; //signal error condition => CURLE_WRITE_ERROR
        }
    };
    curl_write_callback onBytesReceivedWrapper = [](char* buffer, size_t size, size_t nitems, void* callbackData)
    {
        return (*static_cast<decltype(onBytesReceived)*>(callbackData))(buffer, size * nitems); //free this poor little C-API from its shackles and redirect to a proper lambda
    };
    //---------------------------------------------------
    auto getBytesToSend = [&](char* buffer, size_t bytesToRead) -> size_t
    {
        try
        {
            /*  libcurl calls back until 0 bytes are returned (Posix read() semantics), or,
                if CURLOPT_INFILESIZE_LARGE was set, after exactly this amount of bytes

                [!] let's NOT use "incomplete read Posix semantics" for libcurl!
                who knows if libcurl buffers properly, or if it requests incomplete packages!?     */
            const size_t bytesRead = readRequest({buffer, bytesToRead}); //throw X; return "bytesToRead" bytes unless end of stream
            assert(bytesRead == bytesToRead || bytesRead == 0 || readRequest({buffer, bytesToRead}) == 0);
            return bytesRead;
        }
        catch (...)
        {
            userCallbackException = std::current_exception();
            return CURL_READFUNC_ABORT; //signal error condition => CURLE_ABORTED_BY_CALLBACK
        }
    };
    curl_read_callback getBytesToSendWrapper = [](char* buffer, size_t size, size_t nitems, void* callbackData)
    {
        return (*static_cast<decltype(getBytesToSend)*>(callbackData))(buffer, size * nitems); //free this poor little C-API from its shackles and redirect to a proper lambda
    };
    //---------------------------------------------------
    if (receiveHeader)
    {
        setCurlOption({CURLOPT_HEADERDATA, &onHeaderReceived}); //throw SysError
        setCurlOption({CURLOPT_HEADERFUNCTION, onHeaderReceivedWrapper}); //throw SysError
    }
    if (writeResponse)
    {
        setCurlOption({CURLOPT_WRITEDATA, &onBytesReceived}); //throw SysError
        setCurlOption({CURLOPT_WRITEFUNCTION, onBytesReceivedWrapper}); //throw SysError
        //{CURLOPT_BUFFERSIZE, 256 * 1024} -> defaults is 16 kB which seems to correspond to SSL packet size
        //=> setting larget buffers size does nothing (recv still returns only 16 kB)
    }
    if (readRequest)
    {
        if (std::all_of(extraOptions.begin(), extraOptions.end(), [](const CurlOption& o) { return o.option != CURLOPT_POST; }))
        /**/setCurlOption({CURLOPT_UPLOAD, 1}); //throw SysError
        //issues HTTP PUT

        setCurlOption({CURLOPT_READDATA, &getBytesToSend}); //throw SysError
        setCurlOption({CURLOPT_READFUNCTION, getBytesToSendWrapper}); //throw SysError
        //{CURLOPT_UPLOAD_BUFFERSIZE, 256 * 1024} -> default is 64 kB. apparently no performance improvement for larger buffers like 256 kB

        //Contradicting options: CURLOPT_READFUNCTION, CURLOPT_POSTFIELDS:
        if (std::any_of(extraOptions.begin(), extraOptions.end(), [](const CurlOption& o) { return o.option == CURLOPT_POSTFIELDS; }))
        /**/ throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
    }

    if (std::any_of(extraOptions.begin(), extraOptions.end(), [](const CurlOption& o) { return o.option == CURLOPT_WRITEFUNCTION || o.option == CURLOPT_READFUNCTION; }))
    /**/ throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!"); //Option already used here!

    //---------------------------------------------------
    curl_slist* headers = nullptr; //"libcurl will not copy the entire list so you must keep it!"
    ZEN_ON_SCOPE_EXIT(::curl_slist_free_all(headers));

    for (const std::string& headerLine : extraHeaders)
        headers = ::curl_slist_append(headers, headerLine.c_str());

    //WTF!!! 1-sec delay when server doesn't support "Expect: 100-continue"!! https://stackoverflow.com/questions/49670008/how-to-disable-expect-100-continue-in-libcurl
    headers = ::curl_slist_append(headers, "Expect:"); //guess, what: www.googleapis.com doesn't support it! e.g. gdriveUploadFile()
    //CURLOPT_EXPECT_100_TIMEOUT_MS: should not be needed

    //CURLOPT_TCP_NODELAY => already set by default https://brooker.co.za/blog/2024/05/09/nagle.html

    if (headers)
        setCurlOption({CURLOPT_HTTPHEADER, headers}); //throw SysError
    //---------------------------------------------------

    for (const CurlOption& option : extraOptions)
        setCurlOption(option); //throw SysError

    //=======================================================================================================
    const CURLcode rcPerf = ::curl_easy_perform(easyHandle_);
    //WTF: curl_easy_perform() considers FTP response codes 4XX, 5XX as failure, but for HTTP response codes 4XX are considered success!! CONSISTENCY, people!!!
    //=> at least libcurl is aware: CURLOPT_FAILONERROR: "request failure on HTTP response >= 400"; default: "0, do not fail on error"
    //https://curl.haxx.se/docs/faq.html#curl_doesn_t_return_error_for_HT
    //=> BUT Google also screws up in their REST API design and returns HTTP 4XX status for domain-level errors! https://blog.slimjim.xyz/posts/stop-using-http-codes/
    //=> let caller handle HTTP status to work around this mess!

    if (userCallbackException)
        std::rethrow_exception(userCallbackException); //throw X
    //=======================================================================================================

    long httpStatus = 0; //optional
    /*const CURLcode rc = */ ::curl_easy_getinfo(easyHandle_, CURLINFO_RESPONSE_CODE, &httpStatus);

    if (rcPerf != CURLE_OK)
    {
        std::wstring errorMsg = trimCpy(utfTo<std::wstring>(curlErrorBuf)); //optional

        if (httpStatus != 0) //optional
            errorMsg += (errorMsg.empty() ? L"" : L"\n") + formatHttpError(httpStatus);
#if 0
        //utfTo<std::wstring>(::curl_easy_strerror(ec)) is uninteresting
        //use CURLINFO_OS_ERRNO ?? https://curl.haxx.se/libcurl/c/CURLINFO_OS_ERRNO.html
        long nativeErrorCode = 0;
        if (::curl_easy_getinfo(easyHandle, CURLINFO_OS_ERRNO, &nativeErrorCode) == CURLE_OK)
            if (nativeErrorCode != 0)
                errorMsg += (errorMsg.empty() ? L"" : L"\n") + std::wstring(L"Native error code: ") + numberTo<std::wstring>(nativeErrorCode);
#endif
        throw SysError(formatSystemError("curl_easy_perform", formatCurlStatusCode(rcPerf), errorMsg));
    }

    lastSuccessfulUseTime_ = std::chrono::steady_clock::now();
    return {static_cast<int>(httpStatus) /*, contentType ? contentType : ""*/};
}


std::wstring zen::formatCurlStatusCode(CURLcode sc)
{
    switch (sc)
    {
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OK);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_UNSUPPORTED_PROTOCOL);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FAILED_INIT);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_URL_MALFORMAT);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_NOT_BUILT_IN);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_COULDNT_RESOLVE_PROXY);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_COULDNT_RESOLVE_HOST);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_COULDNT_CONNECT);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_WEIRD_SERVER_REPLY);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_REMOTE_ACCESS_DENIED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FTP_ACCEPT_FAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FTP_WEIRD_PASS_REPLY);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FTP_ACCEPT_TIMEOUT);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FTP_WEIRD_PASV_REPLY);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FTP_WEIRD_227_FORMAT);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FTP_CANT_GET_HOST);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_HTTP2);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FTP_COULDNT_SET_TYPE);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_PARTIAL_FILE);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FTP_COULDNT_RETR_FILE);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE20);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_QUOTE_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_HTTP_RETURNED_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_WRITE_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE24);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_UPLOAD_FAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_READ_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OUT_OF_MEMORY);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OPERATION_TIMEDOUT);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE29);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FTP_PORT_FAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FTP_COULDNT_USE_REST);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE32);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_RANGE_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE34);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_CONNECT_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_BAD_DOWNLOAD_RESUME);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FILE_COULDNT_READ_FILE);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_LDAP_CANNOT_BIND);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_LDAP_SEARCH_FAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE40);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE41);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_ABORTED_BY_CALLBACK);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_BAD_FUNCTION_ARGUMENT);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE44);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_INTERFACE_FAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE46);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_TOO_MANY_REDIRECTS);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_UNKNOWN_OPTION);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SETOPT_OPTION_SYNTAX);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE50);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE51);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_GOT_NOTHING);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_ENGINE_NOTFOUND);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_ENGINE_SETFAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SEND_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_RECV_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE57);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_CERTPROBLEM);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_CIPHER);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_PEER_FAILED_VERIFICATION);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_BAD_CONTENT_ENCODING);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE62);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FILESIZE_EXCEEDED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_USE_SSL_FAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SEND_FAIL_REWIND);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_ENGINE_INITFAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_LOGIN_DENIED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_TFTP_NOTFOUND);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_TFTP_PERM);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_REMOTE_DISK_FULL);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_TFTP_ILLEGAL);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_TFTP_UNKNOWNID);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_REMOTE_FILE_EXISTS);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_TFTP_NOSUCHUSER);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE75);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_OBSOLETE76);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_CACERT_BADFILE);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_REMOTE_FILE_NOT_FOUND);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSH);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_SHUTDOWN_FAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_AGAIN);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_CRL_BADFILE);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_ISSUER_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FTP_PRET_FAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_RTSP_CSEQ_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_RTSP_SESSION_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_FTP_BAD_FILE_LIST);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_CHUNK_FAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_NO_CONNECTION_AVAILABLE);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_PINNEDPUBKEYNOTMATCH);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_INVALIDCERTSTATUS);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_HTTP2_STREAM);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_RECURSIVE_API_CALL);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_AUTH_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_HTTP3);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_QUIC_CONNECT_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_PROXY);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_SSL_CLIENTCERT);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_UNRECOVERABLE_POLL);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_TOO_LARGE);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURLE_ECH_REQUIRED);
            ZEN_CHECK_CASE_FOR_CONSTANT(CURL_LAST);
    }
    static_assert(CURL_LAST == CURLE_ECH_REQUIRED + 1);

    return replaceCpy<std::wstring>(L"Curl status %x", L"%x", numberTo<std::wstring>(static_cast<int>(sc)));
}

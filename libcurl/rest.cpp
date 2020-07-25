// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "rest.h"
#include <zen/system.h>
#include <zen/http.h>

using namespace zen;


HttpSession::HttpSession(const Zstring& server, const Zstring& caCertFilePath, std::chrono::seconds timeOut) : //throw SysError
    server_(utfTo<std::string>(server)),
    caCertFilePath_(utfTo<std::string>(caCertFilePath)),
    timeOutSec_(timeOut) {}


HttpSession::~HttpSession()
{
    if (easyHandle_)
        ::curl_easy_cleanup(easyHandle_);
}


HttpSession::Result HttpSession::perform(const std::string& serverRelPath,
                                         const std::vector<std::string>& extraHeaders, const std::vector<CurlOption>& extraOptions, //throw SysError
                                         const std::function<void  (const void* buffer, size_t bytesToWrite)>& writeResponse /*throw X*/, //optional
                                         const std::function<size_t(      void* buffer, size_t bytesToRead )>& readRequest   /*throw X*/) //
{
    if (!easyHandle_)
    {
        easyHandle_ = ::curl_easy_init();
        if (!easyHandle_)
            throw SysError(formatSystemError("curl_easy_init", formatCurlStatusCode(CURLE_OUT_OF_MEMORY), L""));
    }
    else
        ::curl_easy_reset(easyHandle_);


    std::vector<CurlOption> options;

    char curlErrorBuf[CURL_ERROR_SIZE] = {};
    options.emplace_back(CURLOPT_ERRORBUFFER, curlErrorBuf);

    options.emplace_back(CURLOPT_USERAGENT, "FreeFileSync"); //default value; may be overwritten by caller

    //lifetime: keep alive until after curl_easy_setopt() below
    std::string curlPath = "https://" + server_ + serverRelPath;
    options.emplace_back(CURLOPT_URL, curlPath.c_str());

    options.emplace_back(CURLOPT_ACCEPT_ENCODING, "gzip"); //won't hurt + used by Google Drive

    options.emplace_back(CURLOPT_NOSIGNAL, 1L); //thread-safety: https://curl.haxx.se/libcurl/c/threadsafe.html

    options.emplace_back(CURLOPT_CONNECTTIMEOUT, timeOutSec_.count());

    //CURLOPT_TIMEOUT: "Since this puts a hard limit for how long time a request is allowed to take, it has limited use in dynamic use cases with varying transfer times."
    options.emplace_back(CURLOPT_LOW_SPEED_TIME, timeOutSec_.count());
    options.emplace_back(CURLOPT_LOW_SPEED_LIMIT, 1L); //[bytes], can't use "0" which means "inactive", so use some low number


    //libcurl forwards this char-string to OpenSSL as is, which - thank god - accepts UTF8
    options.emplace_back(CURLOPT_CAINFO, caCertFilePath_.c_str()); //hopefully latest version from https://curl.haxx.se/docs/caextract.html
    //CURLOPT_SSL_VERIFYPEER => already active by default
    //CURLOPT_SSL_VERIFYHOST =>

    //---------------------------------------------------
    std::exception_ptr userCallbackException;

    auto onBytesReceived = [&](const void* buffer, size_t len)
    {
        try
        {
            writeResponse(buffer, len); //throw X
            return len;
        }
        catch (...)
        {
            userCallbackException = std::current_exception();
            return len + 1; //signal error condition => CURLE_WRITE_ERROR
        }
    };
    using ReadCbType = decltype(onBytesReceived);
    using ReadCbWrapperType =          size_t (*)(const void* buffer, size_t size, size_t nitems, ReadCbType* callbackData); //needed for cdecl function pointer cast
    ReadCbWrapperType onBytesReceivedWrapper = [](const void* buffer, size_t size, size_t nitems, ReadCbType* callbackData)
    {
        return (*callbackData)(buffer, size * nitems); //free this poor little C-API from its shackles and redirect to a proper lambda
    };
    //---------------------------------------------------
    auto getBytesToSend = [&](void* buffer, size_t len) -> size_t
    {
        try
        {
            //libcurl calls back until 0 bytes are returned (Posix read() semantics), or,
            //if CURLOPT_INFILESIZE_LARGE was set, after exactly this amount of bytes
            const size_t bytesRead = readRequest(buffer, len);//throw X; return "bytesToRead" bytes unless end of stream!
            return bytesRead;
        }
        catch (...)
        {
            userCallbackException = std::current_exception();
            return CURL_READFUNC_ABORT; //signal error condition => CURLE_ABORTED_BY_CALLBACK
        }
    };
    using WriteCbType = decltype(getBytesToSend);
    using WriteCbWrapperType =         size_t (*)(void* buffer, size_t size, size_t nitems, WriteCbType* callbackData);
    WriteCbWrapperType getBytesToSendWrapper = [](void* buffer, size_t size, size_t nitems, WriteCbType* callbackData)
    {
        return (*callbackData)(buffer, size * nitems); //free this poor little C-API from its shackles and redirect to a proper lambda
    };
    //---------------------------------------------------
    if (writeResponse)
    {
        options.emplace_back(CURLOPT_WRITEDATA, &onBytesReceived);
        options.emplace_back(CURLOPT_WRITEFUNCTION, onBytesReceivedWrapper);
    }
    if (readRequest)
    {
        if (std::all_of(extraOptions.begin(), extraOptions.end(), [](const CurlOption& o) { return o.option != CURLOPT_POST; }))
        /**/options.emplace_back(CURLOPT_UPLOAD, 1L); //issues HTTP PUT
        options.emplace_back(CURLOPT_READDATA, &getBytesToSend);
        options.emplace_back(CURLOPT_READFUNCTION, getBytesToSendWrapper);
    }

    if (std::any_of(extraOptions.begin(), extraOptions.end(), [](const CurlOption& o) { return o.option == CURLOPT_WRITEFUNCTION || o.option == CURLOPT_READFUNCTION; }))
    throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__)); //Option already used here!

    if (readRequest && std::any_of(extraOptions.begin(), extraOptions.end(), [](const CurlOption& o) { return o.option == CURLOPT_POSTFIELDS; }))
    throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__)); //Contradicting options: CURLOPT_READFUNCTION, CURLOPT_POSTFIELDS

    //---------------------------------------------------
    curl_slist* headers = nullptr; //"libcurl will not copy the entire list so you must keep it!"
    ZEN_ON_SCOPE_EXIT(::curl_slist_free_all(headers));

    for (const std::string& headerLine : extraHeaders)
        headers = ::curl_slist_append(headers, headerLine.c_str());

    //WTF!!! 1-sec delay when server doesn't support "Expect: 100-continue"!! https://stackoverflow.com/questions/49670008/how-to-disable-expect-100-continue-in-libcurl
    headers = ::curl_slist_append(headers, "Expect:"); //guess, what: www.googleapis.com doesn't support it! e.g. gdriveUploadFile()

    if (headers)
        options.emplace_back(CURLOPT_HTTPHEADER, headers);
    //---------------------------------------------------

    append(options, extraOptions);

    applyCurlOptions(easyHandle_, options); //throw SysError

    //=======================================================================================================
    const CURLcode rcPerf = ::curl_easy_perform(easyHandle_);
    //WTF: curl_easy_perform() considers FTP response codes 4XX, 5XX as failure, but for HTTP response codes 4XX are considered success!! CONSISTENCY, people!!!
    //=> at least libcurl is aware: CURLOPT_FAILONERROR: "request failure on HTTP response >= 400"; default: "0, do not fail on error"
    //https://curl.haxx.se/docs/faq.html#curl_doesn_t_return_error_for_HT
    //=> Curiously Google also screws up in their REST API design and returns HTTP 4XX status for domain-level errors!
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
    return { static_cast<int>(httpStatus) /*, contentType ? contentType : ""*/ };
}

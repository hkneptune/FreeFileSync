// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "gdrive.h"
#include <variant>
#include <unordered_map>
#include <unordered_set>
#include <zen/base64.h>
#include <zen/basic_math.h>
#include <zen/file_traverser.h>
#include <zen/shell_execute.h>
#include <zen/http.h>
#include <zen/zlib_wrap.h>
#include <zen/crc.h>
#include <zen/json.h>
#include <zen/time.h>
#include <zen/file_access.h>
#include <zen/guid.h>
#include <zen/socket.h>
#include <zen/file_io.h>
#include "abstract_impl.h"
#include "libcurl/curl_wrap.h" //DON'T include <curl/curl.h> directly!
#include "init_curl_libssh2.h"
#include "../base/resolve_path.h"

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


namespace fff
{
bool operator<(const GdrivePath& lhs, const GdrivePath& rhs)
{
    const int rv = compareAsciiNoCase(lhs.userEmail, rhs.userEmail);
    if (rv != 0)
        return rv < 0;

    //mirror GoogleFileState file path matching
    return compareNativePath(lhs.itemPath.value, rhs.itemPath.value) < 0;
}


Global<PathAccessLocker<GdrivePath>> globalGdrivePathAccessLocker(std::make_unique<PathAccessLocker<GdrivePath>>());
template <> std::shared_ptr<PathAccessLocker<GdrivePath>> PathAccessLocker<GdrivePath>::getGlobalInstance() { return globalGdrivePathAccessLocker.get(); }
using PathAccessLock = PathAccessLocker<GdrivePath>::Lock; //throw SysError
}


namespace
{
//Google Drive REST API Overview:  https://developers.google.com/drive/api/v3/about-sdk
//Google Drive REST API Reference: https://developers.google.com/drive/api/v3/reference

    const char*  GOOGLE_DRIVE_CLIENT_ID     = ""; // => replace with live credentials
    const char*  GOOGLE_DRIVE_CLIENT_SECRET = ""; //
const Zchar* GOOGLE_REST_API_SERVER = Zstr("www.googleapis.com");

const std::chrono::seconds HTTP_SESSION_ACCESS_TIME_OUT(15);
const std::chrono::seconds HTTP_SESSION_MAX_IDLE_TIME  (20);
const std::chrono::seconds HTTP_SESSION_CLEANUP_INTERVAL(4);
const std::chrono::seconds GOOGLE_DRIVE_SYNC_INTERVAL   (5);

const int GDRIVE_STREAM_BUFFER_SIZE = 512 * 1024; //unit: [byte]

const Zchar googleDrivePrefix[] = Zstr("gdrive:");
const char  googleFolderMimeType[] = "application/vnd.google-apps.folder";

const char DB_FORMAT_DESCR[] = "FreeFileSync: Google Drive Database";
const int  DB_FORMAT_VER = 1;


struct HttpSessionId
{
    /*explicit*/ HttpSessionId(const Zstring& serverName) :
        server(serverName) {}

    Zstring server;
};
bool operator<(const HttpSessionId& lhs, const HttpSessionId& rhs)
{
    //exactly the type of case insensitive comparison we need for server names!
    return compareAsciiNoCase(lhs.server, rhs.server) < 0; //https://msdn.microsoft.com/en-us/library/windows/desktop/ms738519#IDNs
}


//expects "clean" input data, see condenseToGoogleFolderPathPhrase()
Zstring concatenateGoogleFolderPathPhrase(const GdrivePath& gdrivePath) //noexcept
{
    Zstring pathPhrase = Zstring(googleDrivePrefix) + FILE_NAME_SEPARATOR + gdrivePath.userEmail;
    if (!gdrivePath.itemPath.value.empty())
        pathPhrase += FILE_NAME_SEPARATOR + gdrivePath.itemPath.value;
    return pathPhrase;
}


//e.g.: gdrive:/zenju@gmx.net/folder/file.txt
std::wstring getGoogleDisplayPath(const GdrivePath& gdrivePath)
{
    return utfTo<std::wstring>(concatenateGoogleFolderPathPhrase(gdrivePath)); //noexcept
}


std::wstring formatGoogleErrorRaw(const std::string& serverResponse)
{
    /* e.g.: {  "error": {  "errors": [{ "domain": "global",
                                         "reason": "invalidSharingRequest",
                                         "message": "Bad Request. User message: \"ACL change not allowed.\"" }],
                            "code":    400,
                            "message": "Bad Request" }}

    or: {  "error":             "invalid_client",
           "error_description": "Unauthorized" }

    or merely: { "error": "invalid_token" }                                    */
    try
    {
        const JsonValue jresponse = parseJson(serverResponse); //throw JsonParsingError

        if (const JsonValue* error = getChildFromJsonObject(jresponse, "error"))
        {
            if (error->type == JsonValue::Type::string)
                return utfTo<std::wstring>(error->primVal);
            //the inner message is generally more descriptive!
            else if (const JsonValue* errors = getChildFromJsonObject(*error, "errors"))
                if (errors->type == JsonValue::Type::array && !errors->arrayVal.empty())
                    if (const JsonValue* message = getChildFromJsonObject(*errors->arrayVal[0], "message"))
                        if (message->type == JsonValue::Type::string)
                            return utfTo<std::wstring>(message->primVal);
        }
    }
    catch (JsonParsingError&) {} //not JSON?

    assert(false);
    return utfTo<std::wstring>(serverResponse);
}


std::wstring tryFormatHttpErrorCode(int ec) //https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
{
    if (ec == 300) return L"Multiple Choices.";
    if (ec == 301) return L"Moved Permanently.";
    if (ec == 302) return L"Moved temporarily.";
    if (ec == 303) return L"See Other";
    if (ec == 304) return L"Not Modified.";
    if (ec == 305) return L"Use Proxy.";
    if (ec == 306) return L"Switch Proxy.";
    if (ec == 307) return L"Temporary Redirect.";
    if (ec == 308) return L"Permanent Redirect.";

    if (ec == 400) return L"Bad Request.";
    if (ec == 401) return L"Unauthorized.";
    if (ec == 402) return L"Payment Required.";
    if (ec == 403) return L"Forbidden.";
    if (ec == 404) return L"Not Found.";
    if (ec == 405) return L"Method Not Allowed.";
    if (ec == 406) return L"Not Acceptable.";
    if (ec == 407) return L"Proxy Authentication Required.";
    if (ec == 408) return L"Request Timeout.";
    if (ec == 409) return L"Conflict.";
    if (ec == 410) return L"Gone.";
    if (ec == 411) return L"Length Required.";
    if (ec == 412) return L"Precondition Failed.";
    if (ec == 413) return L"Payload Too Large.";
    if (ec == 414) return L"URI Too Long.";
    if (ec == 415) return L"Unsupported Media Type.";
    if (ec == 416) return L"Range Not Satisfiable.";
    if (ec == 417) return L"Expectation Failed.";
    if (ec == 418) return L"I'm a teapot.";
    if (ec == 421) return L"Misdirected Request.";
    if (ec == 422) return L"Unprocessable Entity.";
    if (ec == 423) return L"Locked.";
    if (ec == 424) return L"Failed Dependency.";
    if (ec == 426) return L"Upgrade Required.";
    if (ec == 428) return L"Precondition Required.";
    if (ec == 429) return L"Too Many Requests.";
    if (ec == 431) return L"Request Header Fields Too Large.";
    if (ec == 451) return L"Unavailable For Legal Reasons.";

    if (ec == 500) return L"Internal Server Error.";
    if (ec == 501) return L"Not Implemented.";
    if (ec == 502) return L"Bad Gateway.";
    if (ec == 503) return L"Service Unavailable.";
    if (ec == 504) return L"Gateway Timeout.";
    if (ec == 505) return L"HTTP Version Not Supported.";
    if (ec == 506) return L"Variant Also Negotiates.";
    if (ec == 507) return L"Insufficient Storage.";
    if (ec == 508) return L"Loop Detected.";
    if (ec == 510) return L"Not Extended.";
    if (ec == 511) return L"Network Authentication Required.";
    return L"";
}
//----------------------------------------------------------------------------------------------------------------

Global<UniSessionCounter> httpSessionCount(createUniSessionCounter());


class HttpSession
{
public:
    HttpSession(const HttpSessionId& sessionId, const Zstring& caCertFilePath) : //throw SysError
        sessionId_(sessionId),
        caCertFilePath_(utfTo<std::string>(caCertFilePath)),
        libsshCurlUnifiedInitCookie_(getLibsshCurlUnifiedInitCookie(httpSessionCount)), //throw SysError
        lastSuccessfulUseTime_(std::chrono::steady_clock::now()) {}

    ~HttpSession()
    {
        if (easyHandle_)
            ::curl_easy_cleanup(easyHandle_);
    }

    struct Option
    {
        template <class T>
        Option(CURLoption o, T val) : option(o), value(static_cast<uint64_t>(val)) { static_assert(sizeof(val) <= sizeof(value)); }

        template <class T>
        Option(CURLoption o, T* val) : option(o), value(reinterpret_cast<uint64_t>(val)) { static_assert(sizeof(val) <= sizeof(value)); }

        CURLoption option = CURLOPT_LASTENTRY;
        uint64_t value = 0;
    };

    struct HttpResult
    {
        int statusCode = 0;
        //std::string contentType;
    };
    HttpResult perform(const std::string& serverRelPath,
                       const std::vector<std::string>& extraHeaders, const std::vector<Option>& extraOptions, //throw SysError
                       const std::function<void  (const void* buffer, size_t bytesToWrite)>& writeResponse /*throw X*/, //optional
                       const std::function<size_t(      void* buffer, size_t bytesToRead )>& readRequest   /*throw X*/) //
    {
        if (!easyHandle_)
        {
            easyHandle_ = ::curl_easy_init();
            if (!easyHandle_)
                throw SysError(formatSystemError(L"curl_easy_init", formatCurlErrorRaw(CURLE_OUT_OF_MEMORY), std::wstring()));
        }
        else
            ::curl_easy_reset(easyHandle_);


        std::vector<Option> options;

        curlErrorBuf_[0] = '\0';
        options.emplace_back(CURLOPT_ERRORBUFFER, curlErrorBuf_);

        options.emplace_back(CURLOPT_USERAGENT, "FreeFileSync"); //default value; may be overwritten by caller

        //lifetime: keep alive until after curl_easy_setopt() below
        std::string curlPath = "https://" + utfTo<std::string>(sessionId_.server) + serverRelPath;
        options.emplace_back(CURLOPT_URL, curlPath.c_str());

        options.emplace_back(CURLOPT_NOSIGNAL, 1L); //thread-safety: https://curl.haxx.se/libcurl/c/threadsafe.html

        options.emplace_back(CURLOPT_CONNECTTIMEOUT, std::chrono::seconds(HTTP_SESSION_ACCESS_TIME_OUT).count());

        //CURLOPT_TIMEOUT: "Since this puts a hard limit for how long time a request is allowed to take, it has limited use in dynamic use cases with varying transfer times."
        options.emplace_back(CURLOPT_LOW_SPEED_TIME, std::chrono::seconds(HTTP_SESSION_ACCESS_TIME_OUT).count());
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
        using ReadCbWrapperType =          size_t (*)(const void* buffer, size_t size, size_t nitems, void* callbackData); //needed for cdecl function pointer cast
        ReadCbWrapperType onBytesReceivedWrapper = [](const void* buffer, size_t size, size_t nitems, void* callbackData)
        {
            auto cb = static_cast<ReadCbType*>(callbackData); //free this poor little C-API from its shackles and redirect to a proper lambda
            return (*cb)(buffer, size * nitems);
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
        using WriteCbWrapperType =         size_t (*)(void* buffer, size_t size, size_t nitems, void* callbackData);
        WriteCbWrapperType getBytesToSendWrapper = [](void* buffer, size_t size, size_t nitems, void* callbackData)
        {
            auto cb = static_cast<WriteCbType*>(callbackData); //free this poor little C-API from its shackles and redirect to a proper lambda
            return (*cb)(buffer, size * nitems);
        };
        //---------------------------------------------------
        if (writeResponse)
        {
            options.emplace_back(CURLOPT_WRITEDATA, &onBytesReceived);
            options.emplace_back(CURLOPT_WRITEFUNCTION, onBytesReceivedWrapper);
        }
        if (readRequest)
        {
            if (std::all_of(extraOptions.begin(), extraOptions.end(), [](const Option& o) { return o.option != CURLOPT_POST; }))
            options.emplace_back(CURLOPT_UPLOAD, 1L); //issues HTTP PUT
            options.emplace_back(CURLOPT_READDATA, &getBytesToSend);
            options.emplace_back(CURLOPT_READFUNCTION, getBytesToSendWrapper);
        }

        if (std::any_of(extraOptions.begin(), extraOptions.end(), [](const Option& o) { return o.option == CURLOPT_WRITEFUNCTION || o.option == CURLOPT_READFUNCTION; }))
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__)); //Option already used here!

        if (readRequest && std::any_of(extraOptions.begin(), extraOptions.end(), [](const Option& o) { return o.option == CURLOPT_POSTFIELDS; }))
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__)); //Contradicting options: CURLOPT_READFUNCTION, CURLOPT_POSTFIELDS

        //---------------------------------------------------
        struct curl_slist* headers = nullptr; //"libcurl will not copy the entire list so you must keep it!"
        ZEN_ON_SCOPE_EXIT(::curl_slist_free_all(headers));
        for (const std::string& headerLine : extraHeaders)
            headers = ::curl_slist_append(headers, headerLine.c_str());

        if (headers)
            options.emplace_back(CURLOPT_HTTPHEADER, headers);
        //---------------------------------------------------

        append(options, extraOptions);

        for (const Option& opt : options)
        {
            const CURLcode rc = ::curl_easy_setopt(easyHandle_, opt.option, opt.value);
            if (rc != CURLE_OK)
                throw SysError(formatSystemError(L"curl_easy_setopt " + numberTo<std::wstring>(opt.option),
                                                 formatCurlErrorRaw(rc), utfTo<std::wstring>(::curl_easy_strerror(rc))));
        }

        //=======================================================================================================
        const CURLcode rcPerf = ::curl_easy_perform(easyHandle_);
        //WTF: curl_easy_perform() considers FTP response codes 4XX, 5XX as failure, but for HTTP response codes 4XX are considered success!! CONSISTENCY, people!!!
        //=> at least libcurl is aware: CURLOPT_FAILONERROR: "request failure on HTTP response >= 400"; default: "0, do not fail on error"
        //=> Curiously Google also screws up in their REST API design and returns HTTP 4XX status for domain-level errors!
        //=> let caller handle HTTP status to work around this mess!

        if (userCallbackException)
            std::rethrow_exception(userCallbackException); //throw X
        //=======================================================================================================

        long httpStatusCode = 0; //optional
        /*const CURLcode rc = */ ::curl_easy_getinfo(easyHandle_, CURLINFO_RESPONSE_CODE, &httpStatusCode);

        if (rcPerf != CURLE_OK)
            throw SysError(formatLastCurlError(L"curl_easy_perform", rcPerf, httpStatusCode));

        lastSuccessfulUseTime_ = std::chrono::steady_clock::now();
        return { static_cast<int>(httpStatusCode) /*, contentType ? contentType : ""*/ };
    }

    //------------------------------------------------------------------------------------------------------------

    bool isHealthy() const
    {
        return numeric::dist(std::chrono::steady_clock::now(), lastSuccessfulUseTime_) <= HTTP_SESSION_MAX_IDLE_TIME;
    }

    const HttpSessionId& getSessionId() const { return sessionId_; }

private:
    HttpSession           (const HttpSession&) = delete;
    HttpSession& operator=(const HttpSession&) = delete;

    std::wstring formatLastCurlError(const std::wstring& functionName, CURLcode ec, int httpStatusCode) const
    {
        std::wstring errorMsg;

        if (curlErrorBuf_[0] != 0)
            errorMsg = trimCpy(utfTo<std::wstring>(curlErrorBuf_));

        const std::wstring descr = tryFormatHttpErrorCode(httpStatusCode);
        if (!descr.empty())
            errorMsg += (errorMsg.empty() ? L"" : L"\n") + numberTo<std::wstring>(httpStatusCode) + L": " + descr;
#if 0
        //utfTo<std::wstring>(::curl_easy_strerror(ec)) is uninteresting
        //use CURLINFO_OS_ERRNO ?? https://curl.haxx.se/libcurl/c/CURLINFO_OS_ERRNO.html
        long nativeErrorCode = 0;
        if (::curl_easy_getinfo(easyHandle_, CURLINFO_OS_ERRNO, &nativeErrorCode) == CURLE_OK)
            if (nativeErrorCode != 0)
                errorMsg += (errorMsg.empty() ? L"" : L"\n") + std::wstring(L"Native error code: ") + numberTo<std::wstring>(nativeErrorCode);
#endif
        return formatSystemError(functionName, formatCurlErrorRaw(ec), errorMsg);
    }

    const HttpSessionId sessionId_;
    const std::string caCertFilePath_;
    CURL* easyHandle_ = nullptr;
    char curlErrorBuf_[CURL_ERROR_SIZE] = {};

    std::shared_ptr<UniCounterCookie> libsshCurlUnifiedInitCookie_;
    std::chrono::steady_clock::time_point lastSuccessfulUseTime_;
};

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

class HttpSessionManager //reuse (healthy) HTTP sessions globally
{
public:
    HttpSessionManager(const Zstring& caCertFilePath) : caCertFilePath_(caCertFilePath),
        sessionCleaner_([this]
    {
        setCurrentThreadName("Session Cleaner[HTTP]");
        runGlobalSessionCleanUp(); //throw ThreadInterruption
    }) {}

    ~HttpSessionManager()
    {
        sessionCleaner_.interrupt();
        sessionCleaner_.join();
    }

    using IdleHttpSessions = std::vector<std::unique_ptr<HttpSession>>;

    void access(const HttpSessionId& login, const std::function<void(HttpSession& session)>& useHttpSession /*throw X*/) //throw SysError, X
    {
        Protected<HttpSessionManager::IdleHttpSessions>& sessionStore = getSessionStore(login);

        std::unique_ptr<HttpSession> httpSession;

        sessionStore.access([&](HttpSessionManager::IdleHttpSessions& sessions)
        {
            //assume "isHealthy()" to avoid hitting server connection limits: (clean up of !isHealthy() after use, idle sessions via worker thread)
            if (!sessions.empty())
            {
                httpSession = std::move(sessions.back    ());
                /**/                    sessions.pop_back();
            }
        });

        //create new HTTP session outside the lock: 1. don't block other threads 2. non-atomic regarding "sessionStore"! => one session too many is not a problem!
        if (!httpSession)
            httpSession = std::make_unique<HttpSession>(login, caCertFilePath_); //throw SysError

        ZEN_ON_SCOPE_EXIT(
            if (httpSession->isHealthy()) //thread that created the "!isHealthy()" session is responsible for clean up (avoid hitting server connection limits!)
        sessionStore.access([&](HttpSessionManager::IdleHttpSessions& sessions) { sessions.push_back(std::move(httpSession)); }); );

        useHttpSession(*httpSession); //throw X
    }

private:
    HttpSessionManager           (const HttpSessionManager&) = delete;
    HttpSessionManager& operator=(const HttpSessionManager&) = delete;

    Protected<IdleHttpSessions>& getSessionStore(const HttpSessionId& login)
    {
        //single global session store per login; life-time bound to globalInstance => never remove a sessionStore!!!
        Protected<IdleHttpSessions>* store = nullptr;

        globalSessionStore_.access([&](GlobalHttpSessions& sessionsById)
        {
            store = &sessionsById[login]; //get or create
        });
        static_assert(std::is_same_v<GlobalHttpSessions, std::map<HttpSessionId, Protected<IdleHttpSessions>>>, "require std::map so that the pointers we return remain stable");

        return *store;
    }

    //run a dedicated clean-up thread => it's unclear when the server let's a connection time out, so we do it preemptively
    //context of worker thread:
    void runGlobalSessionCleanUp() //throw ThreadInterruption
    {
        std::chrono::steady_clock::time_point lastCleanupTime;
        for (;;)
        {
            const auto now = std::chrono::steady_clock::now();

            if (now < lastCleanupTime + HTTP_SESSION_CLEANUP_INTERVAL)
                interruptibleSleep(lastCleanupTime + HTTP_SESSION_CLEANUP_INTERVAL - now); //throw ThreadInterruption

            lastCleanupTime = std::chrono::steady_clock::now();

            std::vector<Protected<IdleHttpSessions>*> sessionStores; //pointers remain stable, thanks to std::map<>

            globalSessionStore_.access([&](GlobalHttpSessions& sessionsById)
            {
                for (auto& [sessionId, idleSession] : sessionsById)
                    sessionStores.push_back(&idleSession);
            });

            for (Protected<IdleHttpSessions>* sessionStore : sessionStores)
                for (bool done = false; !done;)
                    sessionStore->access([&](IdleHttpSessions& sessions)
                {
                    for (std::unique_ptr<HttpSession>& sshSession : sessions)
                        if (!sshSession->isHealthy()) //!isHealthy() sessions are destroyed after use => in this context this means they have been idle for too long
                        {
                            sshSession.swap(sessions.back());
                            /**/            sessions.pop_back(); //run ~HttpSession *inside* the lock! => avoid hitting server limits!
                            std::this_thread::yield();
                            return; //don't hold lock for too long: delete only one session at a time, then yield...
                        }
                    done = true;
                });
        }
    }

    using GlobalHttpSessions = std::map<HttpSessionId, Protected<IdleHttpSessions>>;

    Protected<GlobalHttpSessions> globalSessionStore_;
    const Zstring caCertFilePath_;
    InterruptibleThread sessionCleaner_;
};

//--------------------------------------------------------------------------------------
UniInitializer startupInitHttp(*httpSessionCount.get()); //static ordering: place *before* HttpSessionManager instance!

Global<HttpSessionManager> httpSessionManager;
//--------------------------------------------------------------------------------------


//===========================================================================================================================

//try to get a grip on this crazy REST API: - parameters are passed via query string, header, or body, using GET, POST, PUT, PATCH, DELETE, ... it's a dice roll
HttpSession::HttpResult googleHttpsRequest(const std::string& serverRelPath, //throw SysError
                                           const std::vector<std::string>& extraHeaders,
                                           const std::vector<HttpSession::Option>& extraOptions,
                                           const std::function<void  (const void* buffer, size_t bytesToWrite)>& writeResponse /*throw X*/, //optional
                                           const std::function<size_t(      void* buffer, size_t bytesToRead )>& readRequest   /*throw X*/) //optional; returning 0 signals EOF
{
    const std::shared_ptr<HttpSessionManager> mgr = httpSessionManager.get();
    if (!mgr)
        throw SysError(L"googleHttpsRequest() function call not allowed during init/shutdown.");

    HttpSession::HttpResult httpResult;

    mgr->access(HttpSessionId(GOOGLE_REST_API_SERVER), [&](HttpSession& session) //throw SysError
    {
        std::vector<HttpSession::Option> options =
        {
            //https://developers.google.com/drive/api/v3/performance
            //"In order to receive a gzip-encoded response you must do two things: Set an Accept-Encoding header, and modify your user agent to contain the string gzip."
            { CURLOPT_ACCEPT_ENCODING, "gzip" },
            { CURLOPT_USERAGENT, "FreeFileSync (gzip)" },
        };
        append(options, extraOptions);

        httpResult = session.perform(serverRelPath, extraHeaders, options, writeResponse, readRequest); //throw SysError
    });
    return httpResult;
}

//========================================================================================================

struct GoogleUserInfo
{
    std::wstring displayName;
    Zstring email;
};
GoogleUserInfo getUserInfo(const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/about
    const std::string queryParams = xWwwFormUrlEncode(
    {
        { "fields", "user/displayName,user/emailAddress" },
    });
    std::string response;
    googleHttpsRequest("/drive/v3/about?" + queryParams, { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    if (const JsonValue* user = getChildFromJsonObject(jresponse, "user"))
    {
        const std::optional<std::string> displayName = getPrimitiveFromJsonObject(*user, "displayName");
        const std::optional<std::string> email       = getPrimitiveFromJsonObject(*user, "emailAddress");
        if (displayName && email)
            return { utfTo<std::wstring>(*displayName), utfTo<Zstring>(*email) };
    }

    throw SysError(formatGoogleErrorRaw(response));
}


const char* htmlMessageTemplate = R""(<!doctype html>
<html lang="en">
    <head>
 	    <meta charset="utf-8">
	    <title>TITLE_PLACEHOLDER</title>
	    <style type="text/css">
			* {
				font-family: "Helvetica Neue", "Segoe UI", Segoe, Helvetica, Arial, "Lucida Grande", sans-serif;
				text-align: center;
				background-color: #eee; }
			h1 {
				font-size:   45px;
				font-weight: 300;
				margin: 80px 0 20px 0; }
			.descr {
				font-size:   21px;
				font-weight: 200; }
	    </style>
    </head>
    <body>
	    <h1><img src="https://freefilesync.org/images/FreeFileSync.png" style="vertical-align:middle; height: 50px;" alt=""> TITLE_PLACEHOLDER</h1>
	    <div class="descr">MESSAGE_PLACEHOLDER</div>
    </body>
</html>
)"";

struct GoogleAuthCode
{
    std::string code;
    std::string redirectUrl;
    std::string codeChallenge;
};

struct GoogleAccessToken
{
    std::string value;
    time_t validUntil = 0; //remaining lifetime of the access token
};

struct GoogleAccessInfo
{
    GoogleAccessToken accessToken;
    std::string refreshToken;
    GoogleUserInfo userInfo;
};

GoogleAccessInfo googleDriveExchangeAuthCode(const GoogleAuthCode& authCode) //throw SysError
{
    //https://developers.google.com/identity/protocols/OAuth2InstalledApp#exchange-authorization-code
    const std::string postBuf = xWwwFormUrlEncode(
    {
        { "code",          authCode.code },
        { "client_id",     GOOGLE_DRIVE_CLIENT_ID },
        { "client_secret", GOOGLE_DRIVE_CLIENT_SECRET },
        { "redirect_uri",  authCode.redirectUrl },
        { "grant_type",    "authorization_code" },
        { "code_verifier", authCode.codeChallenge },
    });
    std::string response;
    googleHttpsRequest("/oauth2/v4/token", {} /*extraHeaders*/, { { CURLOPT_POSTFIELDS, postBuf.c_str() } }, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> accessToken  = getPrimitiveFromJsonObject(jresponse, "access_token");
    const std::optional<std::string> refreshToken = getPrimitiveFromJsonObject(jresponse, "refresh_token");
    const std::optional<std::string> expiresIn    = getPrimitiveFromJsonObject(jresponse, "expires_in"); //e.g. 3600 seconds
    if (!accessToken || !refreshToken || !expiresIn)
        throw SysError(formatGoogleErrorRaw(response));

    const GoogleUserInfo userInfo = getUserInfo(*accessToken); //throw SysError

    return { { *accessToken, std::time(nullptr) + stringTo<time_t>(*expiresIn) }, *refreshToken, userInfo };
}


GoogleAccessInfo authorizeAccessToGoogleDrive(const Zstring& googleLoginHint, const std::function<void()>& updateGui /*throw X*/) //throw SysError, X
{
    //spin up a web server to wait for the HTTP GET after Google authentication
    ::addrinfo hints = {};
    hints.ai_family   = AF_INET; //make sure our server is reached by IPv4 127.0.0.1, not IPv6 [::1]
    hints.ai_socktype = SOCK_STREAM; //we *do* care about this one!
    hints.ai_flags    = AI_PASSIVE; //the returned socket addresses will be suitable for bind(2)ing a socket that will accept(2) connections.
    hints.ai_flags |= AI_ADDRCONFIG; //no such issue on Linux: https://bugs.chromium.org/p/chromium/issues/detail?id=5234
    ::addrinfo* servinfo = nullptr;
    ZEN_ON_SCOPE_EXIT(if (servinfo) ::freeaddrinfo(servinfo));

    //ServiceName == "0" => open the next best free port
    const int rcGai = ::getaddrinfo(nullptr,    //_In_opt_ PCSTR            pNodeName,
                                    "0",        //_In_opt_ PCSTR            pServiceName,
                                    &hints,     //_In_opt_ const ADDRINFOA* pHints,
                                    &servinfo); //_Outptr_ PADDRINFOA*      ppResult
    if (rcGai != 0)
        throw SysError(formatSystemError(L"getaddrinfo", replaceCpy(_("Error Code %x"), L"%x", numberTo<std::wstring>(rcGai)), utfTo<std::wstring>(::gai_strerror(rcGai))));
    if (!servinfo)
        throw SysError(L"getaddrinfo: empty server info");

    const auto getBoundSocket = [](const auto& /*::addrinfo*/ ai)
    {
        SocketType testSocket = ::socket(ai.ai_family, ai.ai_socktype, ai.ai_protocol);
        if (testSocket == invalidSocket)
            THROW_LAST_SYS_ERROR_WSA(L"socket");
        ZEN_ON_SCOPE_FAIL(closeSocket(testSocket));

        if (::bind(testSocket, ai.ai_addr, static_cast<int>(ai.ai_addrlen)) != 0)
            THROW_LAST_SYS_ERROR_WSA(L"bind");

        return testSocket;
    };

    SocketType socket = invalidSocket;

    std::optional<SysError> firstError;
    for (const auto* /*::addrinfo*/ si = servinfo; si; si = si->ai_next)
        try
        {
            socket = getBoundSocket(*si); //throw SysError; pass ownership
            break;
        }
        catch (const SysError& e) { if (!firstError) firstError = e; }

    if (socket == invalidSocket)
        throw* firstError; //list was not empty, so there must have been an error!

    ZEN_ON_SCOPE_EXIT(closeSocket(socket));


    sockaddr_storage addr = {}; //"sufficiently large to store address information for IPv4 or IPv6" => sockaddr_in and sockaddr_in6
    socklen_t addrLen = sizeof(addr);
    if (::getsockname(socket, reinterpret_cast<sockaddr*>(&addr), &addrLen) != 0)
        THROW_LAST_SYS_ERROR_WSA(L"getsockname");

    if (addr.ss_family != AF_INET &&
        addr.ss_family != AF_INET6)
        throw SysError(L"getsockname: unknown protocol family (" + numberTo<std::wstring>(addr.ss_family) + L")");

    const int port = ntohs(reinterpret_cast<const sockaddr_in&>(addr).sin_port);
    //the socket is not bound to a specific local IP => inet_ntoa(reinterpret_cast<const sockaddr_in&>(addr).sin_addr) == "0.0.0.0"
    const std::string redirectUrl = "http://127.0.0.1:" + numberTo<std::string>(port);

    if (::listen(socket, SOMAXCONN) != 0)
        THROW_LAST_SYS_ERROR_WSA(L"listen");


    //"A code_verifier is a high-entropy cryptographic random string using the unreserved characters:"
    //[A-Z] / [a-z] / [0-9] / "-" / "." / "_" / "~", with a minimum length of 43 characters and a maximum length of 128 characters.
    std::string codeChallenge = stringEncodeBase64(generateGUID() + generateGUID());
    replace(codeChallenge, '+', '-'); //
    replace(codeChallenge, '/', '.'); //base64 is almost a perfect fit for code_verifier!
    replace(codeChallenge, '=', '_'); //
    assert(codeChallenge.size() == 44);

    //authenticate Google Drive via browser: https://developers.google.com/identity/protocols/OAuth2InstalledApp#step-2-send-a-request-to-googles-oauth-20-server
    const std::string oauthUrl = "https://accounts.google.com/o/oauth2/v2/auth?" + xWwwFormUrlEncode(
    {
        { "client_id",      GOOGLE_DRIVE_CLIENT_ID },
        { "redirect_uri",   redirectUrl },
        { "response_type",  "code" },
        { "scope",          "https://www.googleapis.com/auth/drive" },
        { "code_challenge", codeChallenge },
        { "code_challenge_method", "plain" },
        { "login_hint",     utfTo<std::string>(googleLoginHint) },
    });
    try
    {
        openWithDefaultApplication(utfTo<Zstring>(oauthUrl)); //throw FileError
    }
    catch (const FileError& e) { throw SysError(e.toString()); } //errors should be further enriched by context info => SysError

    //process incoming HTTP requests
    for (;;)
    {
        for (;;) //::accept() blocks forever if no client connects (e.g. user just closes the browser window!) => wait for incoming traffic with a time-out via ::select()
        {
            if (updateGui) updateGui(); //throw X

            fd_set rfd = {};
            FD_ZERO(&rfd);
            FD_SET(socket, &rfd);
            fd_set* readfds = &rfd;

            struct ::timeval tv = {};
            tv.tv_usec = static_cast<long>(100 /*ms*/) * 1000;

            //WSAPoll broken, even ::poll() on OS X? https://daniel.haxx.se/blog/2012/10/10/wsapoll-is-broken/
            //perf: no significant difference compared to ::WSAPoll()
            const int rc = ::select(socket + 1, readfds, nullptr /*writefds*/, nullptr /*errorfds*/, &tv);
            if (rc < 0)
                THROW_LAST_SYS_ERROR_WSA(L"select");
            if (rc != 0)
                break;
            //else: time-out!
        }
        //potential race! if the connection is gone right after ::select() and before ::accept(), latter will hang
        const SocketType clientSocket = ::accept(socket,   //SOCKET   s,
                                                 nullptr,  //sockaddr *addr,
                                                 nullptr); //int      *addrlen
        if (clientSocket == invalidSocket)
            THROW_LAST_SYS_ERROR_WSA(L"accept");

        //receive first line of HTTP request
        std::string reqLine;
        for (;;)
        {
            const size_t blockSize = 64 * 1024;
            reqLine.resize(reqLine.size() + blockSize);
            const size_t bytesReceived = tryReadSocket(clientSocket, &*(reqLine.end() - blockSize), blockSize); //throw SysError
            reqLine.resize(reqLine.size() - blockSize + bytesReceived); //caveat: unsigned arithmetics

            if (contains(reqLine, "\r\n"))
            {
                reqLine = beforeFirst(reqLine, "\r\n", IF_MISSING_RETURN_NONE);
                break;
            }
            if (bytesReceived == 0 || reqLine.size() >= 100000 /*bogus line length*/)
                break;
        }

        //get OAuth2.0 authorization result from Google, either:
        std::string code;
        std::string error;

        //parse header; e.g.: GET http://127.0.0.1:62054/?code=4/ZgBRsB9k68sFzc1Pz1q0__Kh17QK1oOmetySrGiSliXt6hZtTLUlYzm70uElNTH9vt1OqUMzJVeFfplMsYsn4uI HTTP/1.1
        const std::vector<std::string> statusItems = split(reqLine, ' ', SplitType::ALLOW_EMPTY); //Method SP Request-URI SP HTTP-Version CRLF

        if (statusItems.size() == 3 && statusItems[0] == "GET" && startsWith(statusItems[2], "HTTP/"))
        {
            for (const auto& [name, value] : xWwwFormUrlDecode(afterFirst(statusItems[1], "?", IF_MISSING_RETURN_NONE)))
                if (name == "code")
                    code = value;
                else if (name == "error")
                    error = value; //e.g. "access_denied" => no more detailed error info available :(
        } //"add explicit braces to avoid dangling else [-Wdangling-else]"

        std::optional<std::variant<GoogleAccessInfo, SysError>> authResult;

        //send HTTP response; https://www.w3.org/Protocols/HTTP/1.0/spec.html#Request-Line
        std::string httpResponse;
        if (code.empty() && error.empty()) //parsing error or unrelated HTTP request
            httpResponse = "HTTP/1.0 400 Bad Request" "\r\n" "\r\n" "400 Bad Request\n" + reqLine;
        else
        {
            std::string htmlMsg = htmlMessageTemplate;
            try
            {
                if (!error.empty())
                    throw SysError(replaceCpy(_("Error Code %x"), L"%x",  + L"\"" + utfTo<std::wstring>(error) + L"\""));

                //do as many login-related tasks as possible while we have the browser as an error output device!
                //see AFS::connectNetworkFolder() => errors will be lost after time out in dir_exist_async.h!
                authResult = googleDriveExchangeAuthCode({ code, redirectUrl, codeChallenge }); //throw SysError
                replace(htmlMsg, "TITLE_PLACEHOLDER",   utfTo<std::string>(_("Authentication completed.")));
                replace(htmlMsg, "MESSAGE_PLACEHOLDER", utfTo<std::string>(_("You may close this page now and continue with FreeFileSync.")));
            }
            catch (const SysError& e)
            {
                authResult = e;
                replace(htmlMsg, "TITLE_PLACEHOLDER",   utfTo<std::string>(_("Authentication failed.")));
                replace(htmlMsg, "MESSAGE_PLACEHOLDER", utfTo<std::string>(replaceCpy(_("Unable to connect to %x."), L"%x", L"Google Drive") + L"\n\n" + e.toString()));
            }
            httpResponse = "HTTP/1.0 200 OK"         "\r\n"
                           "Content-Type: text/html" "\r\n"
                           "Content-Length: " + numberTo<std::string>(strLength(htmlMsg)) + "\r\n"
                           "\r\n" + htmlMsg;
        }

        for (size_t bytesToSend = httpResponse.size(); bytesToSend > 0;)
            bytesToSend -= tryWriteSocket(clientSocket, &*(httpResponse.end() - bytesToSend), bytesToSend); //throw SysError

        shutdownSocketSend(clientSocket); //throw SysError
        //---------------------------------------------------------------

        if (authResult)
        {
            if (const SysError* e = std::get_if<SysError>(&*authResult))
                throw *e;
            return std::get<GoogleAccessInfo>(*authResult);
        }
    }
}


GoogleAccessToken refreshAccessToGoogleDrive(const std::string& refreshToken) //throw SysError
{
    //https://developers.google.com/identity/protocols/OAuth2InstalledApp#offline
    const std::string postBuf = xWwwFormUrlEncode(
    {
        { "refresh_token", refreshToken },
        { "client_id",     GOOGLE_DRIVE_CLIENT_ID },
        { "client_secret", GOOGLE_DRIVE_CLIENT_SECRET },
        { "grant_type",    "refresh_token" },
    });

    std::string response;
    googleHttpsRequest("/oauth2/v4/token", {} /*extraHeaders*/, { { CURLOPT_POSTFIELDS, postBuf.c_str() } }, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> accessToken = getPrimitiveFromJsonObject(jresponse, "access_token");
    const std::optional<std::string> expiresIn   = getPrimitiveFromJsonObject(jresponse, "expires_in"); //e.g. 3600 seconds
    if (!accessToken || !expiresIn)
        throw SysError(formatGoogleErrorRaw(response));

    return { *accessToken, std::time(nullptr) + stringTo<time_t>(*expiresIn) };
}


void revokeAccessToGoogleDrive(const std::string& accessToken, const Zstring& googleUserEmail) //throw SysError
{
    //https://developers.google.com/identity/protocols/OAuth2InstalledApp#tokenrevoke
    const std::shared_ptr<HttpSessionManager> mgr = httpSessionManager.get();
    if (!mgr)
        throw SysError(L"revokeAccessToGoogleDrive() Function call not allowed during process init/shutdown.");

    HttpSession::HttpResult httpResult;
    std::string response;

    mgr->access(HttpSessionId(Zstr("accounts.google.com")), [&](HttpSession& session) //throw SysError
    {
        httpResult = session.perform("/o/oauth2/revoke?token=" + accessToken, { "Content-Type: application/x-www-form-urlencoded" }, {} /*extraOptions*/,
        [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/); //throw SysError
    });

    if (httpResult.statusCode != 200)
        throw SysError(formatGoogleErrorRaw(response));
}


uint64_t gdriveGetFreeDiskSpace(const std::string& accessToken) //throw SysError; returns 0 if not available
{
    //https://developers.google.com/drive/api/v3/reference/about
    std::string response;
    googleHttpsRequest("/drive/v3/about?fields=storageQuota", { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    if (const JsonValue* storageQuota = getChildFromJsonObject(jresponse, "storageQuota"))
    {
        const std::optional<std::string> limit = getPrimitiveFromJsonObject(*storageQuota, "limit");
        const std::optional<std::string> usage = getPrimitiveFromJsonObject(*storageQuota, "usage");

        if (!limit) //"will not be present if the user has unlimited storage."
            return 0;
        if (usage)
        {
            const auto usageInt = stringTo<int64_t>(*usage);
            const auto limitInt = stringTo<int64_t>(*limit);

            if (0 <= usageInt && usageInt <= limitInt)
                return limitInt - usageInt;
        }
    }
    throw SysError(formatGoogleErrorRaw(response));
}


struct GoogleItemDetails
{
    std::string itemName;
    bool        isFolder = false;
    uint64_t    fileSize = 0;
    time_t      modTime = 0;
    std::vector<std::string> parentIds;
};
bool operator==(const GoogleItemDetails& lhs, const GoogleItemDetails& rhs)
{
    return lhs.itemName  == rhs.itemName  &&
           lhs.isFolder  == rhs.isFolder  &&
           lhs.fileSize  == rhs.fileSize  &&
           lhs.modTime   == rhs.modTime   &&
           lhs.parentIds == rhs.parentIds;
}

struct GoogleFileItem
{
    std::string itemId;
    GoogleItemDetails details;
};
std::vector<GoogleFileItem> readFolderContent(const std::string& folderId, const std::string& accessToken) //throw SysError
{
    warn_static("perf: trashed=false and ('114231411234' in parents or '123123' in parents)")

    //https://developers.google.com/drive/api/v3/reference/files/list
    std::vector<GoogleFileItem> childItems;
    {
        std::optional<std::string> nextPageToken;
        do
        {
            std::string queryParams = xWwwFormUrlEncode(
            {
                { "spaces",  "drive" }, //
                { "corpora",  "user" }, //"The 'user' corpus includes all files in "My Drive" and "Shared with me" https://developers.google.com/drive/api/v3/about-organization
                { "pageSize", "1000" }, //"[1, 1000] Default: 100"
                { "fields", "nextPageToken,incompleteSearch,files(name,id,mimeType,size,modifiedTime,parents)" }, //https://developers.google.com/drive/api/v3/reference/files
                { "q", "trashed=false and '" + folderId + "' in parents" },
                //{ "q", "sharedWithMe" },
            });
            if (nextPageToken)
                queryParams += '&' + xWwwFormUrlEncode({ { "pageToken", *nextPageToken } });

            std::string response;
            googleHttpsRequest("/drive/v3/files?" + queryParams, { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
            [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

            JsonValue jresponse;
            try { jresponse = parseJson(response); }
            catch (JsonParsingError&) {}

            /**/                             nextPageToken    = getPrimitiveFromJsonObject(jresponse, "nextPageToken");
            const std::optional<std::string> incompleteSearch = getPrimitiveFromJsonObject(jresponse, "incompleteSearch");
            const JsonValue*                 files            = getChildFromJsonObject    (jresponse, "files");
            if (!incompleteSearch || *incompleteSearch != "false" || !files || files->type != JsonValue::Type::array)
                throw SysError(formatGoogleErrorRaw(response));

            for (const auto& childVal : files->arrayVal)
            {
                const std::optional<std::string> itemId       = getPrimitiveFromJsonObject(*childVal, "id");
                const std::optional<std::string> itemName     = getPrimitiveFromJsonObject(*childVal, "name");
                const std::optional<std::string> mimeType     = getPrimitiveFromJsonObject(*childVal, "mimeType");
                const std::optional<std::string> size         = getPrimitiveFromJsonObject(*childVal, "size");
                const std::optional<std::string> modifiedTime = getPrimitiveFromJsonObject(*childVal, "modifiedTime");
                const JsonValue*                 parents      = getChildFromJsonObject    (*childVal, "parents");

                if (!itemId || !itemName || !mimeType || !modifiedTime || !parents)
                    throw SysError(formatGoogleErrorRaw(response));

                const bool isFolder = *mimeType == googleFolderMimeType;
                const uint64_t fileSize = size ? stringTo<uint64_t>(*size) : 0; //not available for folders

                //RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
                const time_t modTime = utcToTimeT(parseTime("%Y-%m-%dT%H:%M:%S", beforeLast(*modifiedTime, '.', IF_MISSING_RETURN_ALL))); //returns -1 on error
                if (modTime == -1 || !endsWith(*modifiedTime, 'Z')) //'Z' means "UTC" => it seems Google doesn't use the time-zone offset postfix
                    throw SysError(L"Modification time could not be parsed. (" + utfTo<std::wstring>(*modifiedTime) + L")");

                std::vector<std::string> parentIds;
                for (const auto& parentVal : parents->arrayVal)
                {
                    if (parentVal->type != JsonValue::Type::string)
                        throw SysError(formatGoogleErrorRaw(response));
                    parentIds.push_back(parentVal->primVal);
                }
                assert(std::find(parentIds.begin(), parentIds.end(), folderId) != parentIds.end());

                childItems.push_back({ *itemId, { *itemName, isFolder, fileSize, modTime, std::move(parentIds) } });
            }
        }
        while (nextPageToken);
    }
    return childItems;
}


struct ChangeItem
{
    std::string itemId;
    std::optional<GoogleItemDetails> details; //empty if item was deleted!
};
struct ChangesDelta
{
    std::string newStartPageToken;
    std::vector<ChangeItem> changes;
};
ChangesDelta getChangesDelta(const std::string& startPageToken, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/changes/list
    ChangesDelta delta;
    std::optional<std::string> nextPageToken = startPageToken;
    for (;;)
    {
        std::string queryParams = xWwwFormUrlEncode(
        {
            { "pageToken",  *nextPageToken },
            { "pageSize", "1000" }, //"[1, 1000] Default: 100"
            { "restrictToMyDrive", "true" }, //important! otherwise we won't get "removed: true" (because file may still be accessible from other Corpora)
            { "spaces",  "drive" },
            { "fields", "kind,nextPageToken,newStartPageToken,changes(kind,removed,fileId,file(name,mimeType,size,modifiedTime,parents,trashed))" },
        });

        std::string response;
        googleHttpsRequest("/drive/v3/changes?" + queryParams, { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
        [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

        JsonValue jresponse;
        try { jresponse = parseJson(response); }
        catch (JsonParsingError&) {}

        /**/                             nextPageToken     = getPrimitiveFromJsonObject(jresponse, "nextPageToken");
        const std::optional<std::string> newStartPageToken = getPrimitiveFromJsonObject(jresponse, "newStartPageToken");
        const std::optional<std::string> listKind          = getPrimitiveFromJsonObject(jresponse, "kind");
        const JsonValue*                 changes           = getChildFromJsonObject    (jresponse, "changes");

        if (!!nextPageToken == !!newStartPageToken || //there can be only one
            !listKind || *listKind != "drive#changeList" ||
            !changes || changes->type != JsonValue::Type::array)
            throw SysError(formatGoogleErrorRaw(response));

        for (const auto& childVal : changes->arrayVal)
        {
            const std::optional<std::string> kind    = getPrimitiveFromJsonObject(*childVal, "kind");
            const std::optional<std::string> removed = getPrimitiveFromJsonObject(*childVal, "removed");
            const std::optional<std::string> itemId  = getPrimitiveFromJsonObject(*childVal, "fileId");
            const JsonValue*                 file    = getChildFromJsonObject    (*childVal, "file");
            if (!kind || *kind != "drive#change" || !removed || !itemId)
                throw SysError(formatGoogleErrorRaw(response));

            ChangeItem changeItem;
            changeItem.itemId = *itemId;
            if (*removed != "true")
            {
                if (!file)
                    throw SysError(formatGoogleErrorRaw(response));

                const std::optional<std::string> itemName     = getPrimitiveFromJsonObject(*file, "name");
                const std::optional<std::string> mimeType     = getPrimitiveFromJsonObject(*file, "mimeType");
                const std::optional<std::string> size         = getPrimitiveFromJsonObject(*file, "size");
                const std::optional<std::string> modifiedTime = getPrimitiveFromJsonObject(*file, "modifiedTime");
                const std::optional<std::string> trashed      = getPrimitiveFromJsonObject(*file, "trashed");
                const JsonValue*                 parents      = getChildFromJsonObject    (*file, "parents");
                if (!itemName || !mimeType || !modifiedTime || !trashed || !parents)
                    throw SysError(formatGoogleErrorRaw(response));

                if (*trashed != "true")
                {
                    GoogleItemDetails itemDetails = {};
                    itemDetails.itemName = *itemName;
                    itemDetails.isFolder = *mimeType == googleFolderMimeType;
                    itemDetails.fileSize = size ? stringTo<uint64_t>(*size) : 0; //not available for folders

                    //RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
                    itemDetails.modTime = utcToTimeT(parseTime("%Y-%m-%dT%H:%M:%S", beforeLast(*modifiedTime, '.', IF_MISSING_RETURN_ALL))); //returns -1 on error
                    if (itemDetails.modTime == -1 || !endsWith(*modifiedTime, 'Z')) //'Z' means "UTC" => it seems Google doesn't use the time-zone offset postfix
                        throw SysError(L"Modification time could not be parsed. (" + utfTo<std::wstring>(*modifiedTime) + L")");

                    for (const auto& parentVal : parents->arrayVal)
                    {
                        if (parentVal->type != JsonValue::Type::string)
                            throw SysError(formatGoogleErrorRaw(response));
                        itemDetails.parentIds.push_back(parentVal->primVal);
                    }
                    changeItem.details = std::move(itemDetails);
                }
            }
            delta.changes.push_back(std::move(changeItem));
        }

        if (!nextPageToken)
        {
            delta.newStartPageToken = *newStartPageToken;
            return delta;
        }
    }
}


std::string /*startPageToken*/ getChangesCurrentToken(const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/changes/getStartPageToken
    std::string response;
    googleHttpsRequest("/drive/v3/changes/startPageToken", { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> startPageToken = getPrimitiveFromJsonObject(jresponse, "startPageToken");
    if (!startPageToken)
        throw SysError(formatGoogleErrorRaw(response));

    return *startPageToken;
}


//- if item is a folder: deletes recursively!!!
//- even deletes a hardlink with multiple parents => use gdriveUnlinkParent() first
void gdriveDeleteItem(const std::string& itemId, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/delete
    std::string response;
    const HttpSession::HttpResult httpResult = googleHttpsRequest("/drive/v3/files/" + itemId, { "Authorization: Bearer " + accessToken }, //throw SysError
    { { CURLOPT_CUSTOMREQUEST, "DELETE" } },
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    //"If successful, this method returns an empty response body"
    if (!response.empty() || httpResult.statusCode != 204)
        throw SysError(formatGoogleErrorRaw(response));
}


//item is NOT deleted when last parent is removed: it is just not accessible via the "My Drive" hierarchy but still adds to quota! => use for hard links only!
void gdriveUnlinkParent(const std::string& itemId, const std::string& parentFolderId, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/update
    const std::string queryParams = xWwwFormUrlEncode(
    {
        { "removeParents", parentFolderId },
        { "fields", "id,parents"}, //for test if operation was successful
    });
    std::string response;
    googleHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams, //throw SysError
    { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" },
    { { CURLOPT_CUSTOMREQUEST, "PATCH" }, { CURLOPT_POSTFIELDS, "{}" } },
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); /*throw JsonParsingError*/ }
    catch (const JsonParsingError&) {}

    const std::optional<std::string> id      = getPrimitiveFromJsonObject(jresponse, "id"); //id is returned on "success", unlike "parents", see below...
    const JsonValue*                 parents = getChildFromJsonObject(jresponse, "parents");
    if (!id || *id != itemId)
        throw SysError(formatGoogleErrorRaw(response));

    if (parents) //when last parent is removed (=> Google deletes item permanently), Google does NOT return the parents array (not even an empty one!)
        if (parents->type != JsonValue::Type::array ||
            std::any_of(parents->arrayVal.begin(), parents->arrayVal.end(),
        [&](const std::unique_ptr<JsonValue>& jval) { return jval->type == JsonValue::Type::string && jval->primVal == parentFolderId; }))
    throw SysError(L"gdriveUnlinkParent: Google Drive internal failure"); //user should never see this...
}


//- if item is a folder: trashes recursively!!!
//- a hardlink with multiple parents will be not be accessible anymore via any of its path aliases!
void gdriveMoveToTrash(const std::string& itemId, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/update
    const std::string postBuf = R"({ "trashed": true })";

    std::string response;
    googleHttpsRequest("/drive/v3/files/" + itemId + "?fields=trashed", //throw SysError
    { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" },
    { { CURLOPT_CUSTOMREQUEST, "PATCH" }, { CURLOPT_POSTFIELDS, postBuf.c_str() } },
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); /*throw JsonParsingError*/ }
    catch (const JsonParsingError&) {}

    const std::optional<std::string> trashed = getPrimitiveFromJsonObject(jresponse, "trashed");
    if (!trashed || *trashed != "true")
        throw SysError(formatGoogleErrorRaw(response));
}


//folder name already existing? will (happily) create duplicate folders => caller must check!
std::string /*folderId*/ gdriveCreateFolderPlain(const Zstring& folderName, const std::string& parentFolderId, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/folder#creating_a_folder
    std::string postBuf = "{\n";
    postBuf += "\"mimeType\": \"" + std::string(googleFolderMimeType) + "\",\n";
    postBuf += "\"name\":     \"" + utfTo<std::string>(folderName) + "\",\n";
    postBuf += "\"parents\": [\"" + parentFolderId + "\"]\n"; //[!] no trailing comma!
    postBuf += "}";

    std::string response;
    googleHttpsRequest("/drive/v3/files?fields=id", { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" },
    { { CURLOPT_POSTFIELDS, postBuf.c_str() } },
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/); //throw SysError

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> itemId = getPrimitiveFromJsonObject(jresponse, "id");
    if (!itemId)
        throw SysError(formatGoogleErrorRaw(response));
    return *itemId;
}


//target name already existing? will (happily) create duplicate items => caller must check!
void gdriveMoveAndRenameItem(const std::string& itemId, const std::string& parentIdFrom, const std::string& parentIdTo,
                             const Zstring& newName, time_t newModTime, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/folder#moving_files_between_folders
    std::string queryParams = "fields=name,parents"; //for test if operation was successful

    if (parentIdFrom != parentIdTo)
        queryParams += '&' + xWwwFormUrlEncode(
    {
        { "removeParents", parentIdFrom },
        { "addParents",    parentIdTo },
    });

    //more Google Drive peculiarities: changing the file name changes modifiedTime!!! => workaround:

    //RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    const std::string dateTime = formatTime<std::string>("%Y-%m-%dT%H:%M:%S.000Z", getUtcTime(newModTime)); //returns empty string on failure
    if (dateTime.empty())
        throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(newModTime) + L")");

    std::string postBuf = "{\n";
    //postBuf += "\"name\":      \"" + utfTo<std::string>(newName) + "\"\n";
    postBuf += "\"name\":         \"" + utfTo<std::string>(newName) + "\",\n";
    postBuf += "\"modifiedTime\": \"" + dateTime + "\"\n"; //[!] no trailing comma!
    postBuf += "}";

    std::string response;
    googleHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams, //throw SysError
    { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" },
    { { CURLOPT_CUSTOMREQUEST, "PATCH" }, { CURLOPT_POSTFIELDS, postBuf.c_str() } },
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); /*throw JsonParsingError*/ }
    catch (const JsonParsingError&) {}

    const std::optional<std::string> name    = getPrimitiveFromJsonObject(jresponse, "name");
    const JsonValue*                 parents = getChildFromJsonObject(jresponse, "parents");
    if (!name || *name != utfTo<std::string>(newName) ||
        !parents || parents->type != JsonValue::Type::array)
        throw SysError(formatGoogleErrorRaw(response));

    if (!std::any_of(parents->arrayVal.begin(), parents->arrayVal.end(),
    [&](const std::unique_ptr<JsonValue>& jval) { return jval->type == JsonValue::Type::string && jval->primVal == parentIdTo; }))
    throw SysError(L"gdriveMoveAndRenameItem: Google Drive internal failure"); //user should never see this...
}


#if 0
void setModTime(const std::string& itemId, time_t modTime, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/update
    //RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    const std::string dateTime = formatTime<std::string>("%Y-%m-%dT%H:%M:%S.000Z", getUtcTime(modTime)); //returns empty string on failure
    if (dateTime.empty())
        throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(modTime) + L")");

    const std::string postBuf = R"({ "modifiedTime": ")" + dateTime + "\" }";

    std::string response;
    googleHttpsRequest("/drive/v3/files/" + itemId + "?fields=modifiedTime", //throw SysError
    { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" },
    { { CURLOPT_CUSTOMREQUEST, "PATCH" }, { CURLOPT_POSTFIELDS, postBuf.c_str() } },
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); /*throw JsonParsingError*/ }
    catch (const JsonParsingError&) {}

    const std::optional<std::string> modifiedTime = getPrimitiveFromJsonObject(jresponse, "modifiedTime");
    if (!modifiedTime || *modifiedTime != dateTime)
        throw SysError(formatGoogleErrorRaw(response));
}
#endif


void gdriveDownloadFile(const std::string& itemId, const std::function<void(const void* buffer, size_t bytesToWrite)>& writeBlock /*throw X*/, //throw SysError, X
                        const std::string& accessToken)
{
    //https://developers.google.com/drive/api/v3/manage-downloads
    std::string response;
    const HttpSession::HttpResult httpResult = googleHttpsRequest("/drive/v3/files/" + itemId + "?alt=media", //throw SysError, X
    { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/,
    [&](const void* buffer, size_t bytesToWrite)
    {
        writeBlock(buffer, bytesToWrite); //throw X
        if (response.size() < 10000) //always save front part of the response in case we get an error
            response.append(static_cast<const char*>(buffer), bytesToWrite);
    }, nullptr /*readRequest*/);

    if (httpResult.statusCode / 100 != 2)
        throw SysError(formatGoogleErrorRaw(response));
}


#if 0
//file name already existing? => duplicate file created!
//note: Google Drive upload is already transactional!
//upload "small files" (5 MB or less; enforced by Google?) in a single round-trip
std::string /*itemId*/ gdriveUploadSmallFile(const Zstring& fileName, const std::string& parentFolderId, uint64_t streamSize, std::optional<time_t> modTime, //throw SysError, X
                                             const std::function<size_t(void* buffer, size_t bytesToRead)>& readBlock /*throw X*/, //returning 0 signals EOF: Posix read() semantics
                                             const std::string& accessToken)
{
    //https://developers.google.com/drive/api/v3/folder#inserting_a_file_in_a_folder
    //https://developers.google.com/drive/api/v3/multipart-upload

    std::string metaDataBuf = "{\n";
    if (modTime) //convert to RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    {
        const std::string dateTime = formatTime<std::string>("%Y-%m-%dT%H:%M:%S.000Z", getUtcTime(*modTime)); //returns empty string on failure
        if (dateTime.empty())
            throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(*modTime) + L")");

        metaDataBuf += "\"modifiedTime\": \"" + dateTime + "\",\n";
    }
    metaDataBuf += "\"name\":     \"" + utfTo<std::string>(fileName) + "\",\n";
    metaDataBuf += "\"parents\": [\"" + parentFolderId               + "\"]\n"; //[!] no trailing comma!
    metaDataBuf += "}";

    //allowed chars for border: DIGIT ALPHA ' ( ) + _ , - . / : = ?
    const std::string boundaryString = stringEncodeBase64(generateGUID() + generateGUID());

    const std::string postBufHead = "--" + boundaryString +                         "\r\n"
                                    "Content-Type: application/json; charset=UTF-8" "\r\n"
                                    /**/                                            "\r\n" +
                                    metaDataBuf +                                   "\r\n"
                                    "--" + boundaryString +                         "\r\n"
                                    "Content-Type: application/octet-stream"        "\r\n"
                                    /**/                                            "\r\n";

    const std::string postBufTail = "\r\n--" + boundaryString + "--";

    auto readMultipartBlock = [&, headPos = size_t(0), eof = false, tailPos = size_t(0)](void* buffer, size_t bytesToRead) mutable -> size_t
    {
        auto       it    = static_cast<std::byte*>(buffer);
        const auto itEnd = it + bytesToRead;

        if (headPos < postBufHead.size())
        {
            const size_t junkSize = std::min<ptrdiff_t>(postBufHead.size() - headPos, itEnd - it);
            std::memcpy(it, &postBufHead[headPos], junkSize);
            headPos += junkSize;
            it      += junkSize;
        }
        if (it != itEnd)
        {
            if (!eof) //don't assume readBlock() will return streamSize bytes as promised => exhaust and let Google Drive fail if there is a mismatch in Content-Length!
            {
                const size_t junkSize = readBlock(it, itEnd - it); //throw X
                it += junkSize;

                if (junkSize != 0)
                    return it - static_cast<std::byte*>(buffer); //perf: if input stream is at the end, should we immediately append postBufTail (and avoid extra TCP package)? => negligible!
                else
                    eof = true;
            }
            if (it != itEnd)
                if (tailPos < postBufTail.size())
                {
                    const size_t junkSize = std::min<ptrdiff_t>(postBufTail.size() - tailPos, itEnd - it);
                    std::memcpy(it, &postBufTail[tailPos], junkSize);
                    tailPos += junkSize;
                    it      += junkSize;
                }
        }
        return it - static_cast<std::byte*>(buffer);
    };

TODO:
    gzip-compress HTTP request body!

    std::string response;
    const HttpSession::HttpResult httpResult = googleHttpsRequest("/upload/drive/v3/files?uploadType=multipart", //throw SysError, X
    {
        "Authorization: Bearer " + accessToken,
        "Content-Type: multipart/related; boundary=" + boundaryString,
        "Content-Length: " + numberTo<std::string>(postBufHead.size() + streamSize + postBufTail.size())
    },
    { { CURLOPT_POST, 1 } }, //otherwise HttpSession::perform() will PUT
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, readMultipartBlock );

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> itemId = getPrimitiveFromJsonObject(jresponse, "id");
    if (!itemId)
        throw SysError(formatGoogleErrorRaw(response));

    return *itemId;
}
#endif


//file name already existing? => duplicate file created!
//note: Google Drive upload is already transactional!
std::string /*itemId*/ gdriveUploadFile(const Zstring& fileName, const std::string& parentFolderId, std::optional<time_t> modTime, //throw SysError, X
                                        const std::function<size_t(void* buffer, size_t bytesToRead)>& readBlock /*throw X*/, //returning 0 signals EOF: Posix read() semantics
                                        const std::string& accessToken)
{
    //https://developers.google.com/drive/api/v3/folder#inserting_a_file_in_a_folder
    //https://developers.google.com/drive/api/v3/resumable-upload
    try
    {
        //step 1: initiate resumable upload session
        std::string uploadUrlRelative;
        {
            std::string postBuf = "{\n";
            if (modTime) //convert to RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
            {
                const std::string dateTime = formatTime<std::string>("%Y-%m-%dT%H:%M:%S.000Z", getUtcTime(*modTime)); //returns empty string on failure
                if (dateTime.empty())
                    throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(*modTime) + L")");

                postBuf += "\"modifiedTime\": \"" + dateTime + "\",\n";
            }
            postBuf += "\"name\":     \"" + utfTo<std::string>(fileName) + "\",\n";
            postBuf += "\"parents\": [\"" + parentFolderId               + "\"]\n"; //[!] no trailing comma!
            postBuf += "}";

            std::string uploadUrl;

            auto onBytesReceived = [&](const void* buffer, size_t len)
            {
                //inside libcurl's C callstack => better not throw exceptions here!!!
                //"The callback will be called once for each header and only complete header lines are passed on to the callback" (including \r\n at the end)
                const auto strBegin = static_cast<const char*>(buffer);
                if (startsWithAsciiNoCase(StringRef<const char>(strBegin, strBegin + len), "Location:"))
                {
                    uploadUrl.assign(strBegin, len); //not null-terminated!
                    uploadUrl = afterFirst(uploadUrl, ':', IF_MISSING_RETURN_NONE);
                    trim(uploadUrl);
                }
                return len;
            };
            using ReadCbType = decltype(onBytesReceived);
            using ReadCbWrapperType =          size_t (*)(const void* buffer, size_t size, size_t nitems, void* callbackData); //needed for cdecl function pointer cast
            ReadCbWrapperType onBytesReceivedWrapper = [](const void* buffer, size_t size, size_t nitems, void* callbackData)
            {
                auto cb = static_cast<ReadCbType*>(callbackData); //free this poor little C-API from its shackles and redirect to a proper lambda
                return (*cb)(buffer, size * nitems);
            };

            std::string response;
            const HttpSession::HttpResult httpResult = googleHttpsRequest("/upload/drive/v3/files?uploadType=resumable", //throw SysError
            { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" },
            { { CURLOPT_POSTFIELDS, postBuf.c_str() }, { CURLOPT_HEADERDATA, &onBytesReceived }, { CURLOPT_HEADERFUNCTION, onBytesReceivedWrapper } },
            [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

            if (httpResult.statusCode != 200)
                throw SysError(formatGoogleErrorRaw(response));

            if (!startsWith(uploadUrl, "https://www.googleapis.com/"))
                throw SysError(L"Invalid upload URL: " + utfTo<std::wstring>(uploadUrl)); //user should never see this

            uploadUrlRelative = afterFirst(uploadUrl, "googleapis.com", IF_MISSING_RETURN_NONE);
        }
        //---------------------------------------------------
        //step 2: upload file content

        //not officially documented, but Google Drive supports compressed file upload when "Content-Encoding: gzip" is set! :)))
        InputStreamAsGzip gzipStream(readBlock); //throw ZlibInternalError

        auto readBlockAsGzip = [&](void* buffer, size_t bytesToRead) { return gzipStream.read(buffer, bytesToRead); }; //throw ZlibInternalError, X
        //returns "bytesToRead" bytes unless end of stream! => fits into "0 signals EOF: Posix read() semantics"

        std::string response;
        googleHttpsRequest(uploadUrlRelative, { "Content-Encoding: gzip" }, {} /*extraOptions*/, //throw SysError, ZlibInternalError, X
        [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, readBlockAsGzip);

        JsonValue jresponse;
        try { jresponse = parseJson(response); }
        catch (JsonParsingError&) {}

        const std::optional<std::string> itemId = getPrimitiveFromJsonObject(jresponse, "id");
        if (!itemId)
            throw SysError(formatGoogleErrorRaw(response));

        return *itemId;
    }
    catch (ZlibInternalError&)
    {
        throw SysError(L"zlib internal error");
    }
}


//instead of the "root" alias Google uses an actual ID in file metadata
std::string /*itemId*/ getRootItemId(const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/get
    std::string response;
    googleHttpsRequest("/drive/v3/files/root?fields=id", { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> itemId = getPrimitiveFromJsonObject(jresponse, "id");
    if (!itemId)
        throw SysError(formatGoogleErrorRaw(response));

    return *itemId;
}


class GoogleAccessBuffer //per-user-session! => serialize access (perf: amortized fully buffered!)
{
public:
    GoogleAccessBuffer(const GoogleAccessInfo& accessInfo) : accessInfo_(accessInfo) {}

    GoogleAccessBuffer(MemoryStreamIn<ByteArray>& stream) //throw UnexpectedEndOfStreamError
    {
        accessInfo_.accessToken.validUntil = readNumber<int64_t>(stream);                             //
        accessInfo_.accessToken.value      = readContainer<std::string>(stream);                      //
        accessInfo_.refreshToken           = readContainer<std::string>(stream);                      //UnexpectedEndOfStreamError
        accessInfo_.userInfo.displayName   = utfTo<std::wstring>(readContainer<std::string>(stream)); //
        accessInfo_.userInfo.email         = utfTo<     Zstring>(readContainer<std::string>(stream)); //
    }

    void serialize(MemoryStreamOut<ByteArray>& stream) const
    {
        writeNumber<int64_t>(stream, accessInfo_.accessToken.validUntil);
        static_assert(sizeof(accessInfo_.accessToken.validUntil) <= sizeof(int64_t)); //ensure cross-platform compatibility!
        writeContainer(stream, accessInfo_.accessToken.value);
        writeContainer(stream, accessInfo_.refreshToken);
        writeContainer(stream, utfTo<std::string>(accessInfo_.userInfo.displayName));
        writeContainer(stream, utfTo<std::string>(accessInfo_.userInfo.email));
    }

    std::string getAccessToken() //throw SysError
    {
        if (accessInfo_.accessToken.validUntil <= std::time(nullptr) + std::chrono::seconds(HTTP_SESSION_ACCESS_TIME_OUT).count() + 5 /*some leeway*/) //expired/will expire
            accessInfo_.accessToken = refreshAccessToGoogleDrive(accessInfo_.refreshToken); //throw SysError

        assert(accessInfo_.accessToken.validUntil > std::time(nullptr) + std::chrono::seconds(HTTP_SESSION_ACCESS_TIME_OUT).count());
        return accessInfo_.accessToken.value;
    }

    //const std::wstring& getUserDisplayName() const { return accessInfo_.userInfo.displayName; }
    const Zstring& getUserEmail() const { return accessInfo_.userInfo.email; }

    void update(const GoogleAccessInfo& accessInfo)
    {
        if (!equalAsciiNoCase(accessInfo.userInfo.email, accessInfo_.userInfo.email))
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
        accessInfo_ = accessInfo;
    }

private:
    GoogleAccessBuffer           (const GoogleAccessBuffer&) = delete;
    GoogleAccessBuffer& operator=(const GoogleAccessBuffer&) = delete;

    GoogleAccessInfo accessInfo_;
};


class GooglePersistentSessions;


class GoogleFileState //per-user-session! => serialize access (perf: amortized fully buffered!)
{
public:
    GoogleFileState(GoogleAccessBuffer& accessBuf) : //throw SysError
        lastSyncToken_(getChangesCurrentToken(accessBuf.getAccessToken())), //
        rootId_       (getRootItemId         (accessBuf.getAccessToken())), //throw SysError
        accessBuf_(accessBuf) {}                                            //

    GoogleFileState(MemoryStreamIn<ByteArray>& stream, GoogleAccessBuffer& accessBuf) : accessBuf_(accessBuf) //throw UnexpectedEndOfStreamError
    {
        lastSyncToken_ = readContainer<std::string>(stream); //UnexpectedEndOfStreamError
        rootId_        = readContainer<std::string>(stream); //UnexpectedEndOfStreamError

        for (;;)
        {
            const std::string folderId = readContainer<std::string>(stream); //UnexpectedEndOfStreamError
            if (folderId.empty())
                break;
            folderContents_[folderId].isKnownFolder = true;
        }

        size_t itemCount = readNumber<int32_t>(stream);
        while (itemCount-- != 0)
        {
            const std::string itemId = readContainer<std::string>(stream); //UnexpectedEndOfStreamError

            GoogleItemDetails details = {};
            details.itemName = readContainer<std::string>(stream);      //
            details.isFolder = readNumber        <int8_t>(stream) != 0; //UnexpectedEndOfStreamError
            details.fileSize = readNumber      <uint64_t>(stream);      //
            details.modTime  = readNumber       <int64_t>(stream);      //

            size_t parentsCount = readNumber<int32_t>(stream); //UnexpectedEndOfStreamError
            while (parentsCount-- != 0)
                details.parentIds.push_back(readContainer<std::string>(stream)); //UnexpectedEndOfStreamError

            updateItemState(itemId, std::move(details));
        }
    }

    void serialize(MemoryStreamOut<ByteArray>& stream) const
    {
        writeContainer(stream, lastSyncToken_);
        writeContainer(stream, rootId_);

        for (const auto& [folderId, content] : folderContents_)
            if (folderId.empty())
                throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
            else if (content.isKnownFolder)
                writeContainer(stream, folderId);
        writeContainer(stream, std::string()); //sentinel

        writeNumber(stream, static_cast<int32_t>(itemDetails_.size()));
        for (const auto& [itemId, details] : itemDetails_)
        {
            writeContainer(stream, itemId);
            writeContainer(stream, details.itemName);
            writeNumber<  int8_t>(stream, details.isFolder);
            writeNumber<uint64_t>(stream, details.fileSize);
            writeNumber< int64_t>(stream, details.modTime);
            static_assert(sizeof(details.modTime) <= sizeof(int64_t)); //ensure cross-platform compatibility!

            writeNumber(stream, static_cast<int32_t>(details.parentIds.size()));
            for (const std::string& parentId : details.parentIds)
                writeContainer(stream, parentId);
        }
    }

    struct PathStatus
    {
        std::string existingItemId;
        bool        existingIsFolder = false;
        AfsPath     existingPath;     //input path =: existingPath + relPath
        std::vector<Zstring> relPath; //
    };
    PathStatus getPathStatus(const AfsPath& afsPath) //throw SysError
    {
        const std::vector<Zstring> relPath = split(afsPath.value, FILE_NAME_SEPARATOR, SplitType::SKIP_EMPTY);
        if (relPath.empty())
            return { rootId_, true /*existingIsFolder*/, AfsPath(), {} };

        return getPathStatusSub(rootId_, AfsPath(), relPath); //throw SysError
    }

    std::string /*itemId*/ getItemId(const AfsPath& afsPath) //throw SysError
    {
        const GoogleFileState::PathStatus ps = getPathStatus(afsPath); //throw SysError
        if (ps.relPath.empty())
            return ps.existingItemId;

        const AfsPath afsPathMissingChild(nativeAppendPaths(ps.existingPath.value, ps.relPath.front()));
        throw SysError(replaceCpy(_("Cannot find %x."), L"%x", fmtPath(getGoogleDisplayPath({ accessBuf_.getUserEmail(), afsPathMissingChild }))));
    }

    std::pair<std::string /*itemId*/, GoogleItemDetails> getFileAttributes(const AfsPath& afsPath) //throw SysError
    {
        const std::string fileId = getItemId(afsPath); //throw SysError
        auto it = itemDetails_.find(fileId);
        if (it == itemDetails_.end())
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
        return *it;
    }

    std::optional<std::vector<GoogleFileItem>> tryGetBufferedFolderContent(const std::string& folderId) const
    {
        auto it = folderContents_.find(folderId);
        if (it == folderContents_.end() || !it->second.isKnownFolder)
            return std::nullopt;

        std::vector<GoogleFileItem> childItems;
        for (auto itChild : it->second.childItems)
        {
            const auto& [childId, childDetails] = *itChild;
            childItems.push_back({ childId, childDetails });
        }
        return std::move(childItems); //[!] need std::move!
    }

    //-------------- notifications --------------
    using ItemIdDelta = std::unordered_set<std::string>;

    struct FileStateDelta //as long as instance exists, GoogleFileItem will log all changed items
    {
        FileStateDelta() {}
    private:
        FileStateDelta(const std::shared_ptr<const ItemIdDelta>& cids) : changedIds(cids) {}
        friend class GoogleFileState;
        std::shared_ptr<const ItemIdDelta> changedIds; //lifetime is managed by caller; access *only* by GoogleFileState!
    };

    void notifyFolderContent(const FileStateDelta& stateDelta, const std::string& folderId, const std::vector<GoogleFileItem>& childItems)
    {
        folderContents_[folderId].isKnownFolder = true;

        for (const GoogleFileItem& item : childItems)
            notifyItemUpdate(stateDelta, item.itemId, item.details);

        //- should we remove parent links for items that are not children of folderId anymore (as of this update)?? => fringe case during first update! (still: maybe trigger sync?)
        //- what if there are multiple folder state updates incoming in wrong order!? => notifyItemUpdate() will sort it out!
    }

    void notifyItemCreated(const FileStateDelta& stateDelta, const GoogleFileItem& item)
    {
        notifyItemUpdate(stateDelta, item.itemId, item.details);
    }

    void notifyFolderCreated(const FileStateDelta& stateDelta, const std::string& folderId, const Zstring& folderName, const std::string& parentId)
    {
        GoogleItemDetails details = {};
        details.itemName = utfTo<std::string>(folderName);
        details.isFolder = true;
        details.modTime  = std::time(nullptr);
        details.parentIds.push_back(parentId);

        //avoid needless conflicts due to different Google Drive folder modTime!
        auto it = itemDetails_.find(folderId);
        if (it != itemDetails_.end())
            details.modTime = it->second.modTime;

        notifyItemUpdate(stateDelta, folderId, details);
    }

    void notifyItemDeleted(const FileStateDelta& stateDelta, const std::string& itemId)
    {
        notifyItemUpdate(stateDelta, itemId, std::nullopt);
    }

    void notifyParentRemoved(const FileStateDelta& stateDelta, const std::string& itemId, const std::string& parentIdOld)
    {
        auto it = itemDetails_.find(itemId);
        if (it != itemDetails_.end())
        {
            GoogleItemDetails detailsNew = it->second;
            eraseIf(detailsNew.parentIds, [&](const std::string& id) { return id == parentIdOld; });
            notifyItemUpdate(stateDelta, itemId, detailsNew);
        }
        else //conflict!!!
            markSyncDue();
    }

    void notifyMoveAndRename(const FileStateDelta& stateDelta, const std::string& itemId, const std::string& parentIdFrom, const std::string& parentIdTo, const Zstring& newName)
    {
        auto it = itemDetails_.find(itemId);
        if (it != itemDetails_.end())
        {
            GoogleItemDetails detailsNew = it->second;
            detailsNew.itemName = utfTo<std::string>(newName);

            eraseIf(detailsNew.parentIds, [&](const std::string& id) { return id == parentIdFrom || id == parentIdTo; }); //
            detailsNew.parentIds.push_back(parentIdTo); //not a duplicate

            notifyItemUpdate(stateDelta, itemId, detailsNew);
        }
        else //conflict!!!
            markSyncDue();
    }

private:
    GoogleFileState           (const GoogleFileState&) = delete;
    GoogleFileState& operator=(const GoogleFileState&) = delete;

    friend class GooglePersistentSessions;

    void notifyItemUpdate(const FileStateDelta& stateDelta, const std::string& itemId, const std::optional<GoogleItemDetails>& details)
    {
        if (stateDelta.changedIds->find(itemId) == stateDelta.changedIds->end()) //=> no conflicting changes in the meantime
            updateItemState(itemId, details); //accept new state data
        else //conflict?
        {
            auto it = itemDetails_.find(itemId);
            if (!details == (it == itemDetails_.end()))
                if (!details || *details == it->second)
                    return; //notified changes match our current file state
            //else: conflict!!! unclear which has the more recent data!
            markSyncDue();
        }
    }

    FileStateDelta registerFileStateDelta()
    {
        auto deltaPtr = std::make_shared<ItemIdDelta>();
        changeLog_.push_back(deltaPtr);
        return FileStateDelta(deltaPtr);
    }

    bool syncIsDue() const { return std::chrono::steady_clock::now() >= lastSyncTime_ + GOOGLE_DRIVE_SYNC_INTERVAL; }

    void markSyncDue() { lastSyncTime_ = std::chrono::steady_clock::now() - GOOGLE_DRIVE_SYNC_INTERVAL; }


    void syncWithGoogle() //throw SysError
    {
        const ChangesDelta delta = getChangesDelta(lastSyncToken_, accessBuf_.getAccessToken()); //throw SysError

        for (const ChangeItem& item : delta.changes)
            updateItemState(item.itemId, item.details);

        lastSyncToken_ = delta.newStartPageToken;
        lastSyncTime_ = std::chrono::steady_clock::now();

        //good to know: if item is created and deleted between polling for changes it is still reported as deleted by Google!
        //Same goes for any other change that is undone in between change notification syncs.
    }

    PathStatus getPathStatusSub(const std::string& folderId, const AfsPath& folderPath, const std::vector<Zstring>& relPath) //throw SysError
    {
        assert(!relPath.empty());

        std::vector<DetailsIterator>* childItems = nullptr;
        auto itKnown = folderContents_.find(folderId);
        if (itKnown != folderContents_.end() && itKnown->second.isKnownFolder)
            childItems = &(itKnown->second.childItems);
        else
        {
            notifyFolderContent(registerFileStateDelta(), folderId, readFolderContent(folderId, accessBuf_.getAccessToken())); //throw SysError

            if (!folderContents_[folderId].isKnownFolder)
                throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
            childItems = &folderContents_[folderId].childItems;
        }

        auto itFound = itemDetails_.cend();
        for (const DetailsIterator& itDetails : *childItems)
            //Since Google Drive has no concept of a file path, we have to roll our own "path to id" mapping => let's use the platform-native style
            if (equalNativePath(utfTo<Zstring>(itDetails->second.itemName), relPath.front()))
            {
                if (itFound != itemDetails_.end())
                    throw SysError(replaceCpy(_("Cannot find %x."), L"%x",
                                              fmtPath(getGoogleDisplayPath({ accessBuf_.getUserEmail(), AfsPath(nativeAppendPaths(folderPath.value, relPath.front())) }))) + L" " +
                                   replaceCpy(_("The name %x is used by more than one item in the folder."), L"%x", fmtPath(relPath.front())));

                itFound = itDetails;
            }

        if (itFound == itemDetails_.end())
            return { folderId, true /*existingIsFolder*/, folderPath, relPath }; //always a folder, see check before recursion above
        else
        {
            const auto& [childId, childDetails] = *itFound;
            const AfsPath              childItemPath(nativeAppendPaths(folderPath.value, relPath.front()));
            const std::vector<Zstring> childRelPath(relPath.begin() + 1, relPath.end());

            if (childRelPath.empty() || !childDetails.isFolder /*obscure, but possible (and not an error)*/)
                return { childId, childDetails.isFolder, childItemPath, childRelPath };

            return getPathStatusSub(childId, childItemPath, childRelPath); //throw SysError
        }
    }

    void updateItemState(const std::string& itemId, const std::optional<GoogleItemDetails>& details)
    {
        auto it = itemDetails_.find(itemId);

        if (!details == (it == itemDetails_.end()))
            if (!details || *details == it->second) //notified changes match our current file state
                return; //=> avoid misleading changeLog_ entries after Google Drive sync!!!

        //update change logs (and clean up obsolete entries)
        eraseIf(changeLog_, [&](std::weak_ptr<ItemIdDelta>& weakPtr)
        {
            if (std::shared_ptr<ItemIdDelta> iid = weakPtr.lock())
            {
                (*iid).insert(itemId);
                return false;
            }
            else
                return true;
        });

        //update file state
        if (details)
        {
            if (it != itemDetails_.end()) //update
            {
                if (it->second.isFolder != details->isFolder)
                    throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__)); //WTF!?

                std::vector<std::string> parentIdsNew     = details->parentIds;
                std::vector<std::string> parentIdsRemoved = it->second.parentIds;
                eraseIf(parentIdsNew,     [&](const std::string& id) { return std::find(it->second.parentIds.begin(), it->second.parentIds.end(), id) != it->second.parentIds.end(); });
                eraseIf(parentIdsRemoved, [&](const std::string& id) { return std::find(details->parentIds.begin(), details->parentIds.end(), id) != details->parentIds.end(); });

                for (const std::string& parentId : parentIdsNew)
                    folderContents_[parentId].childItems.push_back(it); //new insert => no need for duplicate check

                for (const std::string& parentId : parentIdsRemoved)
                {
                    auto itP = folderContents_.find(parentId);
                    if (itP != folderContents_.end())
                        eraseIf(itP->second.childItems, [&](auto itChild) { return itChild == it; });
                }
                //if all parents are removed, Google Drive will (recursively) delete the item => don't prematurely do this now: wait for change notifications!

                it->second = *details;
            }
            else //create
            {
                auto itNew = itemDetails_.emplace(itemId, *details).first;

                for (const std::string& parentId : details->parentIds)
                    folderContents_[parentId].childItems.push_back(itNew); //new insert => no need for duplicate check
            }
        }
        else //delete
        {
            if (it != itemDetails_.end())
            {
                for (const std::string& parentId : it->second.parentIds) //1. delete from parent folders
                {
                    auto itP = folderContents_.find(parentId);
                    if (itP != folderContents_.end())
                        eraseIf(itP->second.childItems, [&](auto itChild) { return itChild == it; });
                }
                itemDetails_.erase(it);
            }

            auto itP = folderContents_.find(itemId);
            if (itP != folderContents_.end())
            {
                for (auto itChild : itP->second.childItems) //2. delete as parent from child items (don't wait for change notifications of children)
                    eraseIf(itChild->second.parentIds, [&](const std::string& id) { return id == itemId; });
                folderContents_.erase(itP);
            }
        }
    }

    using DetailsIterator = std::unordered_map<std::string, GoogleItemDetails>::iterator;

    struct FolderContent
    {
        bool isKnownFolder = false; //=we've seen its full content at least once; further changes are calculated via change notifications!
        std::vector<DetailsIterator> childItems;
    };
    std::unordered_map<std::string /*folderId*/, FolderContent> folderContents_;
    std::unordered_map<std::string /*itemId*/, GoogleItemDetails> itemDetails_; //contains ALL known, existing items!

    std::string lastSyncToken_; //marker corresponding to last sync with Google's change notifications
    std::chrono::steady_clock::time_point lastSyncTime_ = std::chrono::steady_clock::now() - GOOGLE_DRIVE_SYNC_INTERVAL; //... with Google Drive (default: sync is due)

    std::vector<std::weak_ptr<ItemIdDelta>> changeLog_; //track changed items since FileStateDelta was created (includes sync with Google + our own intermediate change notifications)

    std::string rootId_;
    GoogleAccessBuffer& accessBuf_;
};

//==========================================================================================
//==========================================================================================

class GooglePersistentSessions
{
public:
    GooglePersistentSessions(const Zstring& configDirPath) : configDirPath_(configDirPath) {}

    void saveActiveSessions() //throw FileError
    {
        std::vector<Protected<SessionHolder>*> protectedSessions; //pointers remain stable, thanks to std::map<>
        globalSessions_.access([&](GlobalSessions& sessions)
        {
            for (auto& [googleUserEmail, protectedSession] : sessions)
                protectedSessions.push_back(&protectedSession);
        });

        if (!protectedSessions.empty())
        {
            createDirectoryIfMissingRecursion(configDirPath_); //throw FileError

            std::exception_ptr firstError;

            //access each session outside the globalSessions_ lock!
            for (Protected<SessionHolder>* protectedSession : protectedSessions)
                protectedSession->access([&](SessionHolder& holder)
            {
                if (holder.session)
                    try
                    {
                        const Zstring dbFilePath = getDbFilePath(holder.session->accessBuf.ref().getUserEmail());

                        //generate (hopefully) unique file name to avoid clashing with unrelated tmp file (concurrent FFS shutdown!)
                        const Zstring shortGuid = printNumber<Zstring>(Zstr("%04x"), static_cast<unsigned int>(getCrc16(generateGUID())));
                        const Zstring dbFilePathTmp = dbFilePath + Zstr('.') + shortGuid + Zstr(".tmp");

                        ZEN_ON_SCOPE_FAIL(try { removeFilePlain(dbFilePathTmp); }
                        catch (FileError&) {});

                        saveSession(dbFilePathTmp, *holder.session); //throw FileError

                        moveAndRenameItem(dbFilePathTmp, dbFilePath, true /*replaceExisting*/); //throw FileError, (ErrorMoveUnsupported), (ErrorTargetExisting)
                    }
                    catch (FileError&) { if (!firstError) firstError = std::current_exception(); }
            });

            if (firstError)
                std::rethrow_exception(firstError); //throw FileError
        }
    }

    Zstring addUserSession(const Zstring& googleLoginHint, const std::function<void()>& updateGui /*throw X*/) //throw SysError, X
    {
        const GoogleAccessInfo accessInfo = authorizeAccessToGoogleDrive(googleLoginHint, updateGui); //throw SysError, X

        accessUserSession(accessInfo.userInfo.email, [&](std::optional<UserSession>& userSession) //throw SysError
        {
            if (userSession)
                userSession->accessBuf.ref().update(accessInfo); //redundant?
            else
            {
                auto accessBuf = makeSharedRef<GoogleAccessBuffer>(accessInfo);
                auto fileState = makeSharedRef<GoogleFileState   >(accessBuf.ref()); //throw SysError
                userSession = { accessBuf, fileState };
            }
        });

        return accessInfo.userInfo.email;
    }

    void removeUserSession(const Zstring& googleUserEmail) //throw SysError
    {
        try
        {
            accessUserSession(googleUserEmail, [&](std::optional<UserSession>& userSession) //throw SysError
            {
                if (userSession)
                    revokeAccessToGoogleDrive(userSession->accessBuf.ref().getAccessToken(), googleUserEmail); //throw SysError
            });
        }
        catch (SysError&) { assert(false); }  //best effort: try to invalidate the access token
        //=> expected to fail if offline => not worse than removing FFS via "Uninstall Programs"

        try
        {
            //start with deleting the DB file (1. maybe it's corrupted? 2. skip unnecessary lazy-load)
            const Zstring dbFilePath = getDbFilePath(googleUserEmail);
            try
            {
                removeFilePlain(dbFilePath); //throw FileError
            }
            catch (FileError&)
            {
                if (itemStillExists(dbFilePath)) //throw FileError
                    throw;
            }
        }
        catch (const FileError& e) { throw SysError(e.toString()); } //file access errors should be further enriched by context info => SysError


        accessUserSession(googleUserEmail, [&](std::optional<UserSession>& userSession) //throw SysError
        {
            userSession.reset();
        });
    }

    std::vector<Zstring> /*Google user email*/ listUserSessions() //throw SysError
    {
        std::vector<Zstring> emails;

        std::vector<Protected<SessionHolder>*> protectedSessions; //pointers remain stable, thanks to std::map<>
        globalSessions_.access([&](GlobalSessions& sessions)
        {
            for (auto& [googleUserEmail, protectedSession] : sessions)
                protectedSessions.push_back(&protectedSession);
        });

        //access each session outside the globalSessions_ lock!
        for (Protected<SessionHolder>* protectedSession : protectedSessions)
            protectedSession->access([&](SessionHolder& holder)
        {
            if (holder.session)
                emails.push_back(holder.session->accessBuf.ref().getUserEmail());
        });

        //also include available, but not-yet-loaded sessions
        traverseFolder(configDirPath_,
        [&](const    FileInfo& fi) { if (endsWith(fi.itemName, Zstr(".db"))) emails.push_back(beforeLast(fi.itemName, Zstr('.'), IF_MISSING_RETURN_NONE)); },
        [&](const  FolderInfo& fi) {},
        [&](const SymlinkInfo& si) {},
        [&](const std::wstring& errorMsg)
        {
            try
            {
                if (itemStillExists(configDirPath_)) //throw FileError
                    throw FileError(errorMsg);
            }
            catch (const FileError& e) { throw SysError(e.toString()); } //file access errors should be further enriched by context info => SysError
        });

        removeDuplicates(emails, LessAsciiNoCase());
        return emails;
    }

    struct AsyncAccessInfo
    {
        std::string accessToken; //don't allow (long-running) web requests while holding the global session lock!
        GoogleFileState::FileStateDelta stateDelta;
    };
    //perf: amortized fully buffered!
    AsyncAccessInfo accessGlobalFileState(const Zstring& googleUserEmail, const std::function<void(GoogleFileState& fileState)>& useFileState /*throw X*/) //throw SysError, X
    {
        std::string accessToken;
        GoogleFileState::FileStateDelta stateDelta;

        accessUserSession(googleUserEmail, [&](std::optional<UserSession>& userSession) //throw SysError
        {
            if (!userSession)
                throw SysError(replaceCpy(_("Please authorize access to user account %x."), L"%x", fmtPath(googleUserEmail)));

            //manage last sync time here rather than in GoogleFileState, so that "lastSyncToken" remains stable while accessing GoogleFileState in the callback
            if (userSession->fileState.ref().syncIsDue())
                userSession->fileState.ref().syncWithGoogle(); //throw SysError

            accessToken = userSession->accessBuf.ref().getAccessToken(); //throw SysError
            stateDelta  = userSession->fileState.ref().registerFileStateDelta();

            useFileState(userSession->fileState.ref()); //throw X
        });
        return { accessToken, stateDelta };
    }

private:
    GooglePersistentSessions           (const GooglePersistentSessions&) = delete;
    GooglePersistentSessions& operator=(const GooglePersistentSessions&) = delete;

    struct UserSession;

    Zstring getDbFilePath(Zstring googleUserEmail) const
    {
        for (Zchar& c : googleUserEmail)
            c = asciiToLower(c);
        //return appendSeparator(configDirPath_) + utfTo<Zstring>(formatAsHexString(getMd5(utfTo<std::string>(googleUserEmail)))) + Zstr(".db");
        return appendSeparator(configDirPath_) + googleUserEmail + Zstr(".db");
    }

    void accessUserSession(const Zstring& googleUserEmail, const std::function<void(std::optional<UserSession>& userSession)>& useSession) //throw SysError
    {
        Protected<SessionHolder>* protectedSession = nullptr; //pointers remain stable, thanks to std::map<>
        globalSessions_.access([&](GlobalSessions& sessions) { protectedSession = &sessions[googleUserEmail]; });

        try
        {
            protectedSession->access([&](SessionHolder& holder)
            {
                if (!holder.dbWasLoaded) //let's NOT load the DB files under the globalSessions_ lock, but the session-specific one!
                    try
                    {
                        holder.session = loadSession(getDbFilePath(googleUserEmail)); //throw FileError
                    }
                    catch (FileError&)
                    {
                        if (itemStillExists(getDbFilePath(googleUserEmail))) //throw FileError
                            throw;
                    }
                holder.dbWasLoaded = true;
                useSession(holder.session);
            });
        }
        catch (const FileError& e) { throw SysError(e.toString()); } //GooglePersistentSessions errors should be further enriched by context info => SysError
    }

    static void saveSession(const Zstring& dbFilePath, const UserSession& userSession) //throw FileError
    {
        MemoryStreamOut<ByteArray> streamOut;

        writeArray(streamOut, DB_FORMAT_DESCR, sizeof(DB_FORMAT_DESCR));
        writeNumber<int32_t>(streamOut, DB_FORMAT_VER);

        userSession.accessBuf.ref().serialize(streamOut);
        userSession.fileState.ref().serialize(streamOut);

        ByteArray zstreamOut;
        try
        {
            zstreamOut = compress(streamOut.ref(), 3 /*compression level: see db_file.cpp*/); //throw ZlibInternalError
        }
        catch (ZlibInternalError&) { throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(dbFilePath)), L"zlib internal error"); }

        saveBinContainer(dbFilePath, zstreamOut, nullptr /*notifyUnbufferedIO*/); //throw FileError
    }

    static UserSession loadSession(const Zstring& dbFilePath) //throw FileError
    {
        ByteArray zstream = loadBinContainer<ByteArray>(dbFilePath, nullptr /*notifyUnbufferedIO*/); //throw FileError
        ByteArray rawStream;
        try
        {
            rawStream = decompress(zstream); //throw ZlibInternalError
        }
        catch (ZlibInternalError&) { throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(dbFilePath)), L"Zlib internal error"); }

        MemoryStreamIn<ByteArray> streamIn(rawStream);
        try
        {
            char tmp[sizeof(DB_FORMAT_DESCR)] = {};
            readArray(streamIn, &tmp, sizeof(tmp));                //file format header
            const int fileVersion = readNumber<int32_t>(streamIn); //

            if (!std::equal(std::begin(tmp), std::end(tmp), std::begin(DB_FORMAT_DESCR)) ||
                fileVersion != DB_FORMAT_VER)
                throw UnexpectedEndOfStreamError(); //well, not really...!?

            auto accessBuf = makeSharedRef<GoogleAccessBuffer>(streamIn);                  //throw UnexpectedEndOfStreamError
            auto fileState = makeSharedRef<GoogleFileState   >(streamIn, accessBuf.ref()); //throw UnexpectedEndOfStreamError
            return { accessBuf, fileState };
        }
        catch (UnexpectedEndOfStreamError&) { throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(dbFilePath)), L"Unexpected end of stream."); }
    }

    struct UserSession
    {
        SharedRef<GoogleAccessBuffer> accessBuf;
        SharedRef<GoogleFileState>    fileState;
    };

    struct SessionHolder
    {
        bool dbWasLoaded = false;
        std::optional<UserSession> session;
    };
    using GlobalSessions = std::map<Zstring /*Google user email*/, Protected<SessionHolder>, LessAsciiNoCase>;

    Protected<GlobalSessions> globalSessions_;
    const Zstring configDirPath_;
};
//==========================================================================================
Global<GooglePersistentSessions> globalGoogleSessions;
//==========================================================================================

GooglePersistentSessions::AsyncAccessInfo accessGlobalFileState(const Zstring& googleUserEmail, const std::function<void(GoogleFileState& fileState)>& useFileState /*throw X*/) //throw SysError, X
{
    if (const std::shared_ptr<GooglePersistentSessions> gps = globalGoogleSessions.get())
        return gps->accessGlobalFileState(googleUserEmail, useFileState); //throw SysError, X

    throw SysError(L"accessGlobalFileState() function call not allowed during init/shutdown.");
}

//==========================================================================================
//==========================================================================================

struct GetDirDetails
{
    GetDirDetails(const GdrivePath& gdriveFolderPath) : gdriveFolderPath_(gdriveFolderPath) {}

    struct Result
    {
        std::vector<GoogleFileItem> childItems;
        GdrivePath gdriveFolderPath;
    };
    Result operator()() const
    {
        try
        {
            std::string folderId;
            std::optional<std::vector<GoogleFileItem>> childItemsBuffered;
            const GooglePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdriveFolderPath_.userEmail, [&](GoogleFileState& fileState) //throw SysError
            {
                folderId           = fileState.getItemId(gdriveFolderPath_.itemPath); //throw SysError
                childItemsBuffered = fileState.tryGetBufferedFolderContent(folderId);
            });

            std::vector<GoogleFileItem> childItems;
            if (childItemsBuffered)
                childItems = std::move(*childItemsBuffered);
            else
            {
                childItems = readFolderContent(folderId, aai.accessToken); //throw SysError

                //buffer new file state ASAP => make sure accessGlobalFileState() has amortized constant access (despite the occasional internal readFolderContent() on non-leaf folders)
                accessGlobalFileState(gdriveFolderPath_.userEmail, [&](GoogleFileState& fileState) //throw SysError
                {
                    fileState.notifyFolderContent(aai.stateDelta, folderId, childItems);
                });
            }

            for (const GoogleFileItem& item : childItems)
                if (item.details.itemName.empty())
                    throw SysError(L"Folder contains child item without a name."); //mostly an issue for FFS's folder traversal, but NOT for globalGoogleSessions!

            return { std::move(childItems), gdriveFolderPath_ };
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(getGoogleDisplayPath(gdriveFolderPath_))), e.toString()); }
    }

private:
    GdrivePath gdriveFolderPath_;
};

class SingleFolderTraverser
{
public:
    SingleFolderTraverser(const Zstring& googleUserEmail, const std::vector<std::pair<AfsPath, std::shared_ptr<AFS::TraverserCallback>>>& workload /*throw X*/) :
        workload_(workload), googleUserEmail_(googleUserEmail)
    {
        while (!workload_.empty())
        {
            auto wi = std::move(workload_.    back()); //yes, no strong exception guarantee (std::bad_alloc)
            /**/                workload_.pop_back();  //
            const auto& [folderPath, cb] = wi;

            tryReportingDirError([&] //throw X
            {
                traverseWithException(folderPath, *cb); //throw FileError, X
            }, *cb);
        }
    }

private:
    SingleFolderTraverser           (const SingleFolderTraverser&) = delete;
    SingleFolderTraverser& operator=(const SingleFolderTraverser&) = delete;

    void traverseWithException(const AfsPath& folderPath, AFS::TraverserCallback& cb) //throw FileError, X
    {
        const GetDirDetails::Result r = GetDirDetails({ googleUserEmail_, folderPath })(); //throw FileError

        for (const GoogleFileItem& item : r.childItems)
        {
            const Zstring itemName = utfTo<Zstring>(item.details.itemName);
            if (item.details.isFolder)
            {
                const AfsPath afsItemPath(nativeAppendPaths(r.gdriveFolderPath.itemPath.value, itemName));

                if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb.onFolder({ itemName, nullptr /*symlinkInfo*/ })) //throw X
                    workload_.push_back({ afsItemPath, std::move(cbSub) });
            }
            else
            {
                AFS::FileId fileId = copyStringTo<AFS::FileId>(item.itemId);
                cb.onFile({ itemName, item.details.fileSize, item.details.modTime, fileId, nullptr /*symlinkInfo*/ }); //throw X
            }
        }
    }

    std::vector<std::pair<AfsPath, std::shared_ptr<AFS::TraverserCallback>>> workload_;
    const Zstring googleUserEmail_;
};


void gdriveTraverseFolderRecursive(const Zstring& googleUserEmail, const std::vector<std::pair<AfsPath, std::shared_ptr<AFS::TraverserCallback>>>& workload /*throw X*/, size_t) //throw X
{
    SingleFolderTraverser dummy(googleUserEmail, workload); //throw X
}
//==========================================================================================
//==========================================================================================

struct InputStreamGdrive : public AbstractFileSystem::InputStream
{
    InputStreamGdrive(const GdrivePath& gdrivePath, const IOCallback& notifyUnbufferedIO /*throw X*/) :
        gdrivePath_(gdrivePath),
        notifyUnbufferedIO_(notifyUnbufferedIO)
    {
        worker_ = InterruptibleThread([asyncStreamOut = this->asyncStreamIn_, gdrivePath]
        {
            setCurrentThreadName(("Istream[Gdrive] " + utfTo<std::string>(getGoogleDisplayPath(gdrivePath))). c_str());
            try
            {
                std::string accessToken;
                std::string fileId;
                try
                {
                    accessToken = accessGlobalFileState(gdrivePath.userEmail, [&](GoogleFileState& fileState) //throw SysError
                    {
                        fileId = fileState.getItemId(gdrivePath.itemPath); //throw SysError
                    }).accessToken;
                }
                catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(getGoogleDisplayPath(gdrivePath))), e.toString()); }

                try
                {
                    auto writeBlock = [&](const void* buffer, size_t bytesToWrite)
                    {
                        return asyncStreamOut->write(buffer, bytesToWrite); //throw ThreadInterruption
                    };

                    gdriveDownloadFile(fileId, writeBlock, accessToken); //throw SysError, ThreadInterruption
                }
                catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getGoogleDisplayPath(gdrivePath))), e.toString()); }

                asyncStreamOut->closeStream();
            }
            catch (FileError&) { asyncStreamOut->setWriteError(std::current_exception()); } //let ThreadInterruption pass through!
        });
    }

    ~InputStreamGdrive()
    {
        asyncStreamIn_->setReadError(std::make_exception_ptr(ThreadInterruption()));
        worker_.join();
    }

    size_t read(void* buffer, size_t bytesToRead) override //throw FileError, (ErrorFileLocked), X; return "bytesToRead" bytes unless end of stream!
    {
        const size_t bytesRead = asyncStreamIn_->read(buffer, bytesToRead); //throw FileError
        reportBytesProcessed(); //throw X
        return bytesRead;
        //no need for asyncStreamIn_->checkWriteErrors(): once end of stream is reached, asyncStreamOut->closeStream() was called => no errors occured
    }

    size_t getBlockSize() const override { return 64 * 1024; } //non-zero block size is AFS contract!

    std::optional<AFS::StreamAttributes> getAttributesBuffered() override //throw FileError
    {
        AFS::StreamAttributes attr = {};
        try
        {
            accessGlobalFileState(gdrivePath_.userEmail, [&](GoogleFileState& fileState) //throw SysError
            {
                std::pair<std::string /*itemId*/, GoogleItemDetails> gdriveAttr = fileState.getFileAttributes(gdrivePath_.itemPath); //throw SysError
                attr.modTime  = gdriveAttr.second.modTime;
                attr.fileSize = gdriveAttr.second.fileSize;
                attr.fileId   = copyStringTo<AFS::FileId>(gdriveAttr.first);
            });
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getGoogleDisplayPath(gdrivePath_))), e.toString()); }
        return std::move(attr); //[!]
    }

private:
    void reportBytesProcessed() //throw X
    {
        const int64_t totalBytesDownloaded = asyncStreamIn_->getTotalBytesWritten();
        if (notifyUnbufferedIO_) notifyUnbufferedIO_(totalBytesDownloaded - totalBytesReported_); //throw X
        totalBytesReported_ = totalBytesDownloaded;
    }

    const GdrivePath gdrivePath_;
    const IOCallback notifyUnbufferedIO_; //throw X
    int64_t totalBytesReported_ = 0;
    std::shared_ptr<AsyncStreamBuffer> asyncStreamIn_ = std::make_shared<AsyncStreamBuffer>(GDRIVE_STREAM_BUFFER_SIZE);
    InterruptibleThread worker_;
};

//==========================================================================================

//target existing: 1. fails with "already existing or 2. creates duplicate file!
struct OutputStreamGdrive : public AbstractFileSystem::OutputStreamImpl
{
    OutputStreamGdrive(const GdrivePath& gdrivePath,
                       std::optional<uint64_t> /*streamSize*/,
                       std::optional<time_t> modTime,
                       const IOCallback& notifyUnbufferedIO /*throw X*/) :
        gdrivePath_(gdrivePath),
        notifyUnbufferedIO_(notifyUnbufferedIO)
    {
        std::promise<AFS::FileId> pFileId;
        futFileId_ = pFileId.get_future();

        //PathAccessLock? Not needed, because the AFS abstraction allows for "undefined behavior"

        worker_ = InterruptibleThread([asyncStreamIn = this->asyncStreamOut_, gdrivePath, modTime, pFileId = std::move(pFileId)]() mutable
        {
            setCurrentThreadName(("Ostream[Gdrive] " + utfTo<std::string>(getGoogleDisplayPath(gdrivePath))). c_str());
            try
            {
                try
                {
                    const Zstring fileName = AFS::getItemName(gdrivePath.itemPath);

                    GoogleFileState::PathStatus ps;
                    GooglePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdrivePath.userEmail, [&](GoogleFileState& fileState) //throw SysError
                    {
                        ps = fileState.getPathStatus(gdrivePath.itemPath); //throw SysError
                    });
                    if (ps.relPath.empty())
                        throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(fileName)));

                    if (ps.relPath.size() > 1) //parent folder missing
                        throw SysError(replaceCpy(_("Cannot find %x."), L"%x",
                                                  fmtPath(getGoogleDisplayPath({ gdrivePath.userEmail, AfsPath(nativeAppendPaths(ps.existingPath.value, ps.relPath.front()))}))));

                    const std::string& parentFolderId = ps.existingItemId;

                    auto readBlock = [&](void* buffer, size_t bytesToRead)
                    {
                        //returns "bytesToRead" bytes unless end of stream! => maps nicely into Posix read() semantics expected by gdriveUploadFile()
                        return asyncStreamIn->read(buffer, bytesToRead); //throw ThreadInterruption
                    };

                    //for whatever reason, gdriveUploadFile() is equally-fast or faster than gdriveUploadSmallFile(), despite its two roundtrips, even when the file sizes are 0!!
                    //=> issue likely on Google's side
                    const std::string fileIdNew = //streamSize && *streamSize < 5 * 1024 * 1024 ?
                        //gdriveUploadSmallFile(fileName, parentFolderId, *streamSize, modTime, readBlock, aai.accessToken) :  //throw SysError, ThreadInterruption
                        gdriveUploadFile       (fileName, parentFolderId,              modTime, readBlock, aai.accessToken);   //throw SysError, ThreadInterruption
                    assert(asyncStreamIn->getTotalBytesRead() == asyncStreamIn->getTotalBytesWritten());

                    //buffer new file state ASAP (don't wait GOOGLE_DRIVE_SYNC_INTERVAL)
                    GoogleFileItem newFileItem = {};
                    newFileItem.itemId = fileIdNew;
                    newFileItem.details.itemName = utfTo<std::string>(fileName);
                    newFileItem.details.isFolder = false;
                    newFileItem.details.fileSize = asyncStreamIn->getTotalBytesRead();
                    if (modTime) //else: whatever modTime Google Drive selects will be notified after GOOGLE_DRIVE_SYNC_INTERVAL
                        newFileItem.details.modTime = *modTime;
                    newFileItem.details.parentIds.push_back(parentFolderId);

                    accessGlobalFileState(gdrivePath.userEmail, [&](GoogleFileState& fileState) //throw SysError
                    {
                        fileState.notifyItemCreated(aai.stateDelta, newFileItem);
                    });

                    pFileId.set_value(copyStringTo<AFS::FileId>(fileIdNew));
                }
                catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getGoogleDisplayPath(gdrivePath))), e.toString()); }
            }
            catch (FileError&) { asyncStreamIn->setReadError(std::current_exception()); } //let ThreadInterruption pass through!
        });
    }

    ~OutputStreamGdrive()
    {
        if (worker_.joinable())
        {
            asyncStreamOut_->setWriteError(std::make_exception_ptr(ThreadInterruption()));
            worker_.join();
        }
    }

    void write(const void* buffer, size_t bytesToWrite) override //throw FileError, X
    {
        asyncStreamOut_->write(buffer, bytesToWrite); //throw FileError
        reportBytesProcessed(); //throw X
    }

    AFS::FinalizeResult finalize() override //throw FileError, X
    {
        asyncStreamOut_->closeStream();

        while (!worker_.tryJoinFor(std::chrono::milliseconds(50)))
            reportBytesProcessed(); //throw X
        reportBytesProcessed(); //[!] once more, now that *all* bytes were written

        asyncStreamOut_->checkReadErrors(); //throw FileError
        //--------------------------------------------------------------------
        AFS::FinalizeResult result;
        assert(isReady(futFileId_)); //*must* be available since file creation completed successfully at this point
        result.fileId = futFileId_.get();
        //result.errorModTime -> already (successfully) set during file creation
        return result;
    }

private:
    void reportBytesProcessed() //throw X
    {
        const int64_t totalBytesUploaded = asyncStreamOut_->getTotalBytesRead();
        if (notifyUnbufferedIO_) notifyUnbufferedIO_(totalBytesUploaded - totalBytesReported_); //throw X
        totalBytesReported_ = totalBytesUploaded;
    }

    const GdrivePath gdrivePath_;
    const IOCallback notifyUnbufferedIO_; //throw X
    int64_t totalBytesReported_ = 0;
    std::shared_ptr<AsyncStreamBuffer> asyncStreamOut_ = std::make_shared<AsyncStreamBuffer>(GDRIVE_STREAM_BUFFER_SIZE);
    InterruptibleThread worker_;
    std::future<AFS::FileId> futFileId_; //"play it safe", however with our current access pattern, also could have used an unprotected AFS::FileId
};

//==========================================================================================

class GdriveFileSystem : public AbstractFileSystem
{
public:
    GdriveFileSystem(const Zstring& googleUserEmail) : googleUserEmail_(googleUserEmail) {}

private:
    GdrivePath getGdrivePath(const AfsPath& afsPath) const { return { googleUserEmail_, afsPath }; }

    Zstring getInitPathPhrase(const AfsPath& afsPath) const override { return concatenateGoogleFolderPathPhrase(getGdrivePath(afsPath)); }

    std::wstring getDisplayPath(const AfsPath& afsPath) const override { return getGoogleDisplayPath(getGdrivePath(afsPath)); }

    bool isNullFileSystem() const override { return googleUserEmail_.empty(); }

    int compareDeviceSameAfsType(const AbstractFileSystem& afsRhs) const override
    {
        return compareAsciiNoCase(googleUserEmail_, static_cast<const GdriveFileSystem&>(afsRhs).googleUserEmail_);
    }

    //----------------------------------------------------------------------------------------------------------------
    ItemType getItemType(const AfsPath& afsPath) const override //throw FileError
    {
        if (std::optional<ItemType> type = itemStillExists(afsPath)) //throw FileError
            return *type;
        throw FileError(replaceCpy(_("Cannot find %x."), L"%x", fmtPath(getDisplayPath(afsPath))));
    }

    std::optional<ItemType> itemStillExists(const AfsPath& afsPath) const override //throw FileError
    {
        try
        {
            GoogleFileState::PathStatus ps;
            accessGlobalFileState(googleUserEmail_, [&](GoogleFileState& fileState) //throw SysError
            {
                ps = fileState.getPathStatus(afsPath); //throw SysError
            });
            if (ps.relPath.empty())
                return ps.existingIsFolder ? ItemType::FOLDER : ItemType::FILE;
            return {};
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString()); }
    }
    //----------------------------------------------------------------------------------------------------------------

    //already existing: fail/ignore
    //=> we choose to let Google Drive fail and give a clear error message
    void createFolderPlain(const AfsPath& afsPath) const override //throw FileError
    {
        try
        {
            //avoid duplicate Google Drive item creation by multiple threads
            PathAccessLock pal(getGdrivePath(afsPath)); //throw SysError

            const Zstring folderName = getItemName(afsPath);

            GoogleFileState::PathStatus ps;
            const GooglePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(googleUserEmail_, [&](GoogleFileState& fileState) //throw SysError
            {
                ps = fileState.getPathStatus(afsPath); //throw SysError
            });

            if (ps.relPath.empty())
                throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(folderName)));

            if (ps.relPath.size() > 1) //parent folder missing
                throw SysError(replaceCpy(_("Cannot find %x."), L"%x", fmtPath(getDisplayPath(AfsPath(nativeAppendPaths(ps.existingPath.value, ps.relPath.front()))))));

            const std::string& parentFolderId = ps.existingItemId;

            const std::string folderIdNew = gdriveCreateFolderPlain(folderName, parentFolderId, aai.accessToken); //throw SysError

            //buffer new file state ASAP (don't wait GOOGLE_DRIVE_SYNC_INTERVAL)
            accessGlobalFileState(googleUserEmail_, [&](GoogleFileState& fileState) //throw SysError
            {
                fileState.notifyFolderCreated(aai.stateDelta, folderIdNew, folderName, parentFolderId);
            });
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString()); }
    }

    void removeItemPlainImpl(const AfsPath& afsPath) const //throw SysError
    {
        std::string itemId;
        std::optional<std::string> parentIdToUnlink;
        const GooglePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(googleUserEmail_, [&](GoogleFileState& fileState) //throw SysError
        {
            const std::optional<AfsPath> parentPath = getParentPath(afsPath);
            if (!parentPath) throw SysError(L"Item is device root");

            std::pair<std::string /*itemId*/, GoogleItemDetails> gdriveAttr = fileState.getFileAttributes(afsPath); //throw SysError
            itemId = gdriveAttr.first;
            assert(gdriveAttr.second.parentIds.size() > 1 ||
                   (gdriveAttr.second.parentIds.size() == 1 && gdriveAttr.second.parentIds[0] == fileState.getItemId(*parentPath)));

            if (gdriveAttr.second.parentIds.size() != 1) //hard-link handling
                parentIdToUnlink = fileState.getItemId(*parentPath); //throw SysError
        });

        if (parentIdToUnlink)
        {
            gdriveUnlinkParent(itemId, *parentIdToUnlink, aai.accessToken); //throw SysError

            //buffer new file state ASAP (don't wait GOOGLE_DRIVE_SYNC_INTERVAL)
            accessGlobalFileState(googleUserEmail_, [&](GoogleFileState& fileState) //throw SysError
            {
                fileState.notifyParentRemoved(aai.stateDelta, itemId, *parentIdToUnlink);
            });
        }
        else
        {
            gdriveDeleteItem(itemId, aai.accessToken); //throw SysError

            //buffer new file state ASAP (don't wait GOOGLE_DRIVE_SYNC_INTERVAL)
            accessGlobalFileState(googleUserEmail_, [&](GoogleFileState& fileState) //throw SysError
            {
                fileState.notifyItemDeleted(aai.stateDelta, itemId);
            });
        }
    }

    void removeFilePlain(const AfsPath& afsPath) const override //throw FileError
    {
        try
        {
            removeItemPlainImpl(afsPath); //throw SysError
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete file %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString()); }
    }

    void removeSymlinkPlain(const AfsPath& afsPath) const override //throw FileError
    {
        throw FileError(replaceCpy(_("Cannot delete symbolic link %x."), L"%x", fmtPath(getDisplayPath(afsPath))), _("Operation not supported by device."));
    }

    void removeFolderPlain(const AfsPath& afsPath) const override //throw FileError
    {
        try
        {
            removeItemPlainImpl(afsPath); //throw SysError
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString()); }
    }

    void removeFolderIfExistsRecursion(const AfsPath& afsPath, //throw FileError
                                       const std::function<void (const std::wstring& displayPath)>& onBeforeFileDeletion /*throw X*/, //optional
                                       const std::function<void (const std::wstring& displayPath)>& onBeforeFolderDeletion) const override //one call for each object!
    {
        if (onBeforeFolderDeletion) onBeforeFolderDeletion(getDisplayPath(afsPath)); //throw X
        try
        {
            //deletes recursively with a single call!
            removeFolderPlain(afsPath); //throw FileError
        }
        catch (const FileError&)
        {
            if (!itemStillExists(afsPath)) //throw FileError
                return;
            throw;
        }
    }

    //----------------------------------------------------------------------------------------------------------------
    AbstractPath getSymlinkResolvedPath(const AfsPath& afsPath) const override //throw FileError
    {
        throw FileError(replaceCpy(_("Cannot determine final path for %x."), L"%x", fmtPath(getDisplayPath(afsPath))), _("Operation not supported by device."));
    }

    std::string getSymlinkBinaryContent(const AfsPath& afsPath) const override //throw FileError
    {
        throw FileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(getDisplayPath(afsPath))), _("Operation not supported by device."));
    }
    //----------------------------------------------------------------------------------------------------------------

    //return value always bound:
    std::unique_ptr<InputStream> getInputStream(const AfsPath& afsPath, const IOCallback& notifyUnbufferedIO /*throw X*/) const override //throw FileError, (ErrorFileLocked)
    {
        return std::make_unique<InputStreamGdrive>(getGdrivePath(afsPath), notifyUnbufferedIO);
    }

    //target existing: undefined behavior! (fail/overwrite/auto-rename)
    //=> actual behavior: 1. fails with "already existing or 2. creates duplicate file!
    //=> we choose to let Google Drive create a duplicate file, because setting PathAccessLock for a potentially long-running write operation is excessive!
    std::unique_ptr<OutputStreamImpl> getOutputStream(const AfsPath& afsPath, //throw FileError
                                                      std::optional<uint64_t> streamSize,
                                                      std::optional<time_t> modTime,
                                                      const IOCallback& notifyUnbufferedIO /*throw X*/) const override
    {
        //target existing: 1. fails with "already existing or 2. creates duplicate file!
        return std::make_unique<OutputStreamGdrive>(getGdrivePath(afsPath), streamSize, modTime, notifyUnbufferedIO);
    }

    //----------------------------------------------------------------------------------------------------------------
    void traverseFolderRecursive(const TraverserWorkload& workload /*throw X*/, size_t parallelOps) const override
    {
        gdriveTraverseFolderRecursive(googleUserEmail_, workload, parallelOps); //throw X
    }
    //----------------------------------------------------------------------------------------------------------------

    //symlink handling: follow link!
    //target existing: undefined behavior! (fail/overwrite/auto-rename)
    FileCopyResult copyFileForSameAfsType(const AfsPath& afsPathSource, const StreamAttributes& attrSource, //throw FileError, (ErrorFileLocked), X
                                          const AbstractPath& apTarget, bool copyFilePermissions, const IOCallback& notifyUnbufferedIO /*throw X*/) const override
    {
        //no native Google Drive file copy => use stream-based file copy:
        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(AFS::getDisplayPath(apTarget))), _("Operation not supported by device."));

        //target existing: undefined behavior! (fail/overwrite/auto-rename)
        return copyFileAsStream(afsPathSource, attrSource, apTarget, notifyUnbufferedIO); //throw FileError, (ErrorFileLocked), X
    }

    //already existing: fail/ignore
    //symlink handling: follow link!
    void copyNewFolderForSameAfsType(const AfsPath& afsPathSource, const AbstractPath& apTarget, bool copyFilePermissions) const override //throw FileError
    {
        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(AFS::getDisplayPath(apTarget))), _("Operation not supported by device."));

        //already existing: fail/ignore
        AFS::createFolderPlain(apTarget); //throw FileError
    }

    void copySymlinkForSameAfsType(const AfsPath& afsPathSource, const AbstractPath& apTarget, bool copyFilePermissions) const override //throw FileError
    {
        throw FileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."),
                                              L"%x", L"\n" + fmtPath(getDisplayPath(afsPathSource))),
                                   L"%y", L"\n" + fmtPath(AFS::getDisplayPath(apTarget))), _("Operation not supported by device."));
    }

    //target existing: undefined behavior! (fail/overwrite/auto-rename)
    //=> actual behavior: fails with "already existing
    void moveAndRenameItemForSameAfsType(const AfsPath& pathFrom, const AbstractPath& pathTo) const override //throw FileError, ErrorMoveUnsupported
    {
        auto generateErrorMsg = [&] { return replaceCpy(replaceCpy(_("Cannot move file %x to %y."),
                                                                   L"%x", L"\n" + fmtPath(getDisplayPath(pathFrom))),
                                                        L"%y",  L"\n" + fmtPath(AFS::getDisplayPath(pathTo)));
                                    };

        if (compareDeviceSameAfsType(pathTo.afsDevice.ref()) != 0)
            throw ErrorMoveUnsupported(generateErrorMsg(), _("Operation not supported between different devices."));

        try
        {
            //avoid duplicate Google Drive item creation by multiple threads
            PathAccessLock pal(getGdrivePath(pathTo.afsPath)); //throw SysError

            const Zstring itemNameOld = getItemName(pathFrom);
            const Zstring itemNameNew = AFS::getItemName(pathTo);

            const std::optional<AfsPath> parentPathFrom = getParentPath(pathFrom);
            const std::optional<AfsPath> parentPathTo   = getParentPath(pathTo.afsPath);
            if (!parentPathFrom) throw SysError(L"Source is device root");
            if (!parentPathTo  ) throw SysError(L"Target is device root");

            std::string itemIdFrom;
            time_t modTimeFrom = 0;
            std::string parentIdFrom;
            std::string parentIdTo;
            const GooglePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(googleUserEmail_, [&](GoogleFileState& fileState) //throw SysError
            {
                std::pair<std::string /*itemId*/, GoogleItemDetails> gdriveAttr = fileState.getFileAttributes(pathFrom); //throw SysError
                itemIdFrom  = gdriveAttr.first;
                modTimeFrom = gdriveAttr.second.modTime;
                parentIdFrom                     = fileState.getItemId(*parentPathFrom); //throw SysError
                GoogleFileState::PathStatus psTo = fileState.getPathStatus(pathTo.afsPath); //throw SysError

                //e.g. changing file name case only => this is not an "already exists" situation!
                //also: hardlink referenced by two different paths, the source one will be unlinked
                if (psTo.relPath.empty() && psTo.existingItemId == itemIdFrom)
                    parentIdTo = fileState.getItemId(*parentPathTo); //throw SysError
                else
                {
                    if (psTo.relPath.empty())
                        throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(itemNameNew)));

                    if (psTo.relPath.size() > 1) //parent folder missing
                        throw SysError(replaceCpy(_("Cannot find %x."), L"%x",
                                                  fmtPath(getDisplayPath(AfsPath(nativeAppendPaths(psTo.existingPath.value, psTo.relPath.front()))))));
                    parentIdTo = psTo.existingItemId;
                }
            });

            if (parentIdFrom == parentIdTo && itemNameOld == itemNameNew)
                return; //nothing to do

            //target name already existing? will (happily) create duplicate items
            gdriveMoveAndRenameItem(itemIdFrom, parentIdFrom, parentIdTo, itemNameNew, modTimeFrom, aai.accessToken); //throw SysError

            //buffer new file state ASAP (don't wait GOOGLE_DRIVE_SYNC_INTERVAL)
            accessGlobalFileState(googleUserEmail_, [&](GoogleFileState& fileState) //throw SysError
            {
                fileState.notifyMoveAndRename(aai.stateDelta, itemIdFrom, parentIdFrom, parentIdTo, itemNameNew);
            });
        }
        catch (const SysError& e) { throw FileError(generateErrorMsg(), e.toString()); }
    }

    bool supportsPermissions(const AfsPath& afsPath) const override { return false; } //throw FileError

    //----------------------------------------------------------------------------------------------------------------
    ImageHolder getFileIcon      (const AfsPath& afsPath, int pixelSize) const override { return ImageHolder(); } //noexcept; optional return value
    ImageHolder getThumbnailImage(const AfsPath& afsPath, int pixelSize) const override { return ImageHolder(); } //noexcept; optional return value

    void authenticateAccess(bool allowUserInteraction) const override //throw FileError
    {
        if (allowUserInteraction)
            try
            {
                const std::shared_ptr<GooglePersistentSessions> gps = globalGoogleSessions.get();
                if (!gps)
                    throw SysError(L"GdriveFileSystem::authenticateAccess() function call not allowed during init/shutdown.");

                for (const Zstring& email : gps->listUserSessions()) //throw SysError
                    if (equalAsciiNoCase(email, googleUserEmail_))
                        return;
                gps->addUserSession(googleUserEmail_ /*googleLoginHint*/, nullptr /*updateGui*/); //throw SysError
                //error messages will be lost after time out in dir_exist_async.h! However:
                //The most-likely-to-fail parts (web access) are reported by authorizeAccessToGoogleDrive() via the browser!
            }
            catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to connect to %x."), L"%x", fmtPath(getDisplayPath(AfsPath()))), e.toString()); }
    }

    int getAccessTimeout() const override { return static_cast<int>(std::chrono::seconds(HTTP_SESSION_ACCESS_TIME_OUT).count()); } //returns "0" if no timeout in force

    bool hasNativeTransactionalCopy() const override { return true; }
    //----------------------------------------------------------------------------------------------------------------

    uint64_t getFreeDiskSpace(const AfsPath& afsPath) const override //throw FileError, returns 0 if not available
    {
        try
        {
            const std::string& accessToken = accessGlobalFileState(googleUserEmail_, [](GoogleFileState& fileState) {}).accessToken; //throw SysError
            return gdriveGetFreeDiskSpace(accessToken); //throw SysError; returns 0 if not available
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot determine free disk space for %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString()); }
    }

    bool supportsRecycleBin(const AfsPath& afsPath) const override { return true; } //throw FileError

    struct RecycleSessionGdrive : public RecycleSession
    {
        void recycleItemIfExists(const AbstractPath& itemPath, const Zstring& logicalRelPath) override { AFS::recycleItemIfExists(itemPath); } //throw FileError
        void tryCleanup(const std::function<void (const std::wstring& displayPath)>& notifyDeletionStatus) override {}; //throw FileError
    };
    std::unique_ptr<RecycleSession> createRecyclerSession(const AfsPath& afsPath) const override //throw FileError, return value must be bound!
    {
        return std::make_unique<RecycleSessionGdrive>();
    }

    void recycleItemIfExists(const AfsPath& afsPath) const override //throw FileError
    {
        try
        {
            GoogleFileState::PathStatus ps;
            const GooglePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(googleUserEmail_, [&](GoogleFileState& fileState) //throw SysError
            {
                ps = fileState.getPathStatus(afsPath); //throw SysError
            });
            if (ps.relPath.empty())
            {
                gdriveMoveToTrash(ps.existingItemId, aai.accessToken); //throw SysError

                //buffer new file state ASAP (don't wait GOOGLE_DRIVE_SYNC_INTERVAL)
                accessGlobalFileState(googleUserEmail_, [&](GoogleFileState& fileState) //throw SysError
                {
                    //a hardlink with multiple parents will be not be accessible anymore via any of its path aliases!
                    fileState.notifyItemDeleted(aai.stateDelta, ps.existingItemId);
                });
            }
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to move %x to the recycle bin."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString()); }
    }

    const Zstring googleUserEmail_;
};
//===========================================================================================================================
}


void fff::googleDriveInit(const Zstring& configDirPath, const Zstring& caCertFilePath)
{
    assert(!httpSessionManager.get());
    httpSessionManager.set(std::make_unique<HttpSessionManager>(caCertFilePath));

    assert(!globalGoogleSessions.get());
    globalGoogleSessions.set(std::make_unique<GooglePersistentSessions>(configDirPath));
}


void fff::googleDriveTeardown()
{
    try //don't use ~GooglePersistentSessions() to save! Might never happen, e.g. detached thread waiting for Google Drive authentication; terminated on exit!
    {
        if (const std::shared_ptr<GooglePersistentSessions> gps = globalGoogleSessions.get())
            gps->saveActiveSessions(); //throw FileError
    }
    catch (FileError&) { assert(false); }

    assert(globalGoogleSessions.get());
    globalGoogleSessions.set(nullptr);

    assert(httpSessionManager.get());
    httpSessionManager.set(nullptr);
}


Zstring fff::googleAddUser(const std::function<void()>& updateGui /*throw X*/) //throw FileError, X
{
    try
    {
        if (const std::shared_ptr<GooglePersistentSessions> gps = globalGoogleSessions.get())
            return gps->addUserSession(Zstr("") /*googleLoginHint*/, updateGui); //throw SysError, X

        throw SysError(L"googleAddUser() function call not allowed during init/shutdown.");
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to connect to %x."), L"%x", L"Google Drive"), e.toString()); }
}


void fff::googleRemoveUser(const Zstring& googleUserEmail) //throw FileError
{
    try
    {
        if (const std::shared_ptr<GooglePersistentSessions> gps = globalGoogleSessions.get())
            return gps->removeUserSession(googleUserEmail); //throw SysError

        throw SysError(L"googleRemoveUser() function call not allowed during init/shutdown.");
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to disconnect from %x."), L"%x", fmtPath(getGoogleDisplayPath({ googleUserEmail, AfsPath() }))), e.toString()); }
}


std::vector<Zstring> /*Google user email*/ fff::googleListConnectedUsers() //throw FileError
{
    try
    {
        if (const std::shared_ptr<GooglePersistentSessions> gps = globalGoogleSessions.get())
            return gps->listUserSessions(); //throw SysError

        throw SysError(L"googleListConnectedUsers() function call not allowed during init/shutdown.");
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to access %x."), L"%x", L"Google Drive"), e.toString()); }
}


Zstring fff::condenseToGoogleFolderPathPhrase(const Zstring& userEmail, const Zstring& relPath) //noexcept
{
    return concatenateGoogleFolderPathPhrase({ trimCpy(userEmail), sanitizeRootRelativePath(relPath) });
}


//e.g.: gdrive:/zenju@gmx.net/folder/file.txt
GdrivePath fff::getResolvedGooglePath(const Zstring& folderPathPhrase) //noexcept
{
    Zstring path = folderPathPhrase;
    path = expandMacros(path); //expand before trimming!
    trim(path);

    if (startsWithAsciiNoCase(path, googleDrivePrefix))
        path = path.c_str() + strLength(googleDrivePrefix);

    const AfsPath sanPath = sanitizeRootRelativePath(path); //Win/macOS compatibility: let's ignore slash/backslash differences

    const Zstring userEmail = beforeFirst(sanPath.value, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL);
    const AfsPath afsPath     (afterFirst(sanPath.value, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE));

    return { userEmail, afsPath };
}


bool fff::acceptsItemPathPhraseGdrive(const Zstring& itemPathPhrase) //noexcept
{
    Zstring path = expandMacros(itemPathPhrase); //expand before trimming!
    trim(path);
    return startsWithAsciiNoCase(path, googleDrivePrefix);
}


AbstractPath fff::createItemPathGdrive(const Zstring& itemPathPhrase) //noexcept
{
    const GdrivePath gdrivePath = getResolvedGooglePath(itemPathPhrase); //noexcept
    return AbstractPath(makeSharedRef<GdriveFileSystem>(gdrivePath.userEmail), gdrivePath.itemPath);
}

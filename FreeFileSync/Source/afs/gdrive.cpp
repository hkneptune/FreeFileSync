// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "gdrive.h"
#include <variant>
#include <unordered_set> //needed by clang
#include <unordered_map> //
#include <libcurl/rest.h>
#include <zen/basic_math.h>
#include <zen/base64.h>
#include <zen/crc.h>
#include <zen/file_access.h>
#include <zen/file_io.h>
#include <zen/file_traverser.h>
#include <zen/guid.h>
#include <zen/http.h>
#include <zen/json.h>
#include <zen/shell_execute.h>
#include <zen/socket.h>
#include <zen/time.h>
#include <zen/zlib_wrap.h>
#include "abstract_impl.h"
#include "init_curl_libssh2.h"
#include "../base/resolve_path.h"

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


namespace fff
{
struct GdrivePath
{
    GdriveLogin gdriveLogin;
    AfsPath itemPath; //path relative to drive root
};

struct GdriveRawPath
{
    std::string parentId; //Google Drive item IDs are *globally* unique!
    Zstring itemName;
};
inline
std::weak_ordering operator<=>(const GdriveRawPath& lhs, const GdriveRawPath& rhs)
{
    if (const std::strong_ordering cmp = lhs.parentId <=> rhs.parentId;
        cmp != std::strong_ordering::equal)
        return cmp;

    return compareNativePath(lhs.itemName, rhs.itemName) <=> 0;
}

constinit2 Global<PathAccessLocker<GdriveRawPath>> globalGdrivePathAccessLocker;
GLOBAL_RUN_ONCE(globalGdrivePathAccessLocker.set(std::make_unique<PathAccessLocker<GdriveRawPath>>()));

template <> std::shared_ptr<PathAccessLocker<GdriveRawPath>> PathAccessLocker<GdriveRawPath>::getGlobalInstance() { return globalGdrivePathAccessLocker.get(); }
template <> Zstring PathAccessLocker<GdriveRawPath>::getItemName(const GdriveRawPath& nativePath) { return nativePath.itemName; }

using PathAccessLock = PathAccessLocker<GdriveRawPath>::Lock; //throw SysError
using PathBlockType  = PathAccessLocker<GdriveRawPath>::BlockType;
}


namespace
{
//Google Drive REST API Overview:  https://developers.google.com/drive/api/v3/about-sdk
//Google Drive REST API Reference: https://developers.google.com/drive/api/v3/reference
const Zchar* GOOGLE_REST_API_SERVER = Zstr("www.googleapis.com");

const std::chrono::seconds HTTP_SESSION_ACCESS_TIME_OUT(15);
const std::chrono::seconds HTTP_SESSION_MAX_IDLE_TIME  (20);
const std::chrono::seconds HTTP_SESSION_CLEANUP_INTERVAL(4);
const std::chrono::seconds GDRIVE_SYNC_INTERVAL         (5);

const int GDRIVE_STREAM_BUFFER_SIZE = 512 * 1024; //unit: [byte]

const Zchar gdrivePrefix[] = Zstr("gdrive:");
const char gdriveFolderMimeType  [] = "application/vnd.google-apps.folder";
const char gdriveShortcutMimeType[] = "application/vnd.google-apps.shortcut"; //= symbolic link!

const char DB_FILE_DESCR[] = "FreeFileSync";
const int  DB_FILE_VERSION = 4; //2020-07-03

std::string getGdriveClientId    () { return ""; } // => replace with live credentials
std::string getGdriveClientSecret() { return ""; } //




struct HttpSessionId
{
    /*explicit*/ HttpSessionId(const Zstring& serverName) :
        server(serverName) {}

    Zstring server;
};
std::weak_ordering operator<=>(const HttpSessionId& lhs, const HttpSessionId& rhs)
{
    //exactly the type of case insensitive comparison we need for server names!
    return compareAsciiNoCase(lhs.server, rhs.server) <=> 0; //https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfow#IDNs
}


//expects "clean" input data
Zstring concatenateGdriveFolderPathPhrase(const GdrivePath& gdrivePath) //noexcept
{
    Zstring pathPhrase = Zstring(gdrivePrefix) + FILE_NAME_SEPARATOR + utfTo<Zstring>(gdrivePath.gdriveLogin.email);

    if (!gdrivePath.gdriveLogin.sharedDriveName.empty())
        pathPhrase += Zstr(':') + gdrivePath.gdriveLogin.sharedDriveName;

    if (!gdrivePath.itemPath.value.empty())
        pathPhrase += FILE_NAME_SEPARATOR + gdrivePath.itemPath.value;

    if (endsWith(pathPhrase, Zstr(' '))) //path phrase concept must survive trimming!
        pathPhrase += FILE_NAME_SEPARATOR;

    return pathPhrase;
}


//e.g.: gdrive:/john@gmail.com:SharedDrive/folder/file.txt
std::wstring getGdriveDisplayPath(const GdrivePath& gdrivePath)
{
    return utfTo<std::wstring>(concatenateGdriveFolderPathPhrase(gdrivePath)); //noexcept
}


std::wstring formatGdriveErrorRaw(std::string serverResponse)
{
    /* e.g.: {  "error": {  "errors": [{ "domain": "global",
                                         "reason": "invalidSharingRequest",
                                         "message": "Bad Request. User message: \"ACL change not allowed.\"" }],
                            "code":    400,
                            "message": "Bad Request" }}

    or: {  "error":             "invalid_client",
           "error_description": "Unauthorized" }

    or merely: { "error": "invalid_token" }                                    */
    trim(serverResponse);

    assert(!serverResponse.empty());
    if (serverResponse.empty())
        return L"<" + _("empty") + L">"; //at least give some indication

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
                    if (const JsonValue* message = getChildFromJsonObject(errors->arrayVal[0], "message"))
                        if (message->type == JsonValue::Type::string)
                            return utfTo<std::wstring>(message->primVal);
        }
    }
    catch (JsonParsingError&) {} //not JSON?

    return utfTo<std::wstring>(serverResponse);
}

//----------------------------------------------------------------------------------------------------------------

constinit2 Global<UniSessionCounter> httpSessionCount;
GLOBAL_RUN_ONCE(httpSessionCount.set(createUniSessionCounter()));
UniInitializer startupInitHttp(*httpSessionCount.get());

//----------------------------------------------------------------------------------------------------------------

class HttpSessionManager //reuse (healthy) HTTP sessions globally
{
public:
    explicit HttpSessionManager(const Zstring& caCertFilePath) : caCertFilePath_(caCertFilePath),
        sessionCleaner_([this]
    {
        setCurrentThreadName(Zstr("Session Cleaner[HTTP]"));
        runGlobalSessionCleanUp(); //throw ThreadStopRequest
    }) {}

    void access(const HttpSessionId& login, const std::function<void(HttpSession& session)>& useHttpSession /*throw X*/) //throw SysError, X
    {
        Protected<HttpSessionManager::IdleHttpSessions>& sessionStore = getSessionStore(login);

        std::unique_ptr<HttpInitSession> httpSession;

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
            httpSession = std::make_unique<HttpInitSession>(getLibsshCurlUnifiedInitCookie(httpSessionCount), login.server, caCertFilePath_); //throw SysError

        ZEN_ON_SCOPE_EXIT(
            if (isHealthy(httpSession->session)) //thread that created the "!isHealthy()" session is responsible for clean up (avoid hitting server connection limits!)
        sessionStore.access([&](HttpSessionManager::IdleHttpSessions& sessions) { sessions.push_back(std::move(httpSession)); }); );

        useHttpSession(httpSession->session); //throw X
    }

private:
    HttpSessionManager           (const HttpSessionManager&) = delete;
    HttpSessionManager& operator=(const HttpSessionManager&) = delete;

    //associate session counting (for initialization/teardown)
    struct HttpInitSession
    {
        HttpInitSession(std::shared_ptr<UniCounterCookie> cook, const Zstring& server, const Zstring& caCertFilePath) :
            cookie(std::move(cook)), session(server, caCertFilePath, HTTP_SESSION_ACCESS_TIME_OUT) {}

        std::shared_ptr<UniCounterCookie> cookie;
        HttpSession session; //life time must be subset of UniCounterCookie
    };
    static bool isHealthy(const HttpSession& s) { return numeric::dist(std::chrono::steady_clock::now(), s.getLastUseTime()) <= HTTP_SESSION_MAX_IDLE_TIME; }

    using IdleHttpSessions = std::vector<std::unique_ptr<HttpInitSession>>;

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
    void runGlobalSessionCleanUp() //throw ThreadStopRequest
    {
        std::chrono::steady_clock::time_point lastCleanupTime;
        for (;;)
        {
            const auto now = std::chrono::steady_clock::now();

            if (now < lastCleanupTime + HTTP_SESSION_CLEANUP_INTERVAL)
                interruptibleSleep(lastCleanupTime + HTTP_SESSION_CLEANUP_INTERVAL - now); //throw ThreadStopRequest

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
                    for (std::unique_ptr<HttpInitSession>& sshSession : sessions)
                        if (!isHealthy(sshSession->session)) //!isHealthy() sessions are destroyed after use => in this context this means they have been idle for too long
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
constinit2 Global<HttpSessionManager> globalHttpSessionManager; //caveat: life time must be subset of static UniInitializer!
//--------------------------------------------------------------------------------------


//===========================================================================================================================

//try to get a grip on this crazy REST API: - parameters are passed via query string, header, or body, using GET, POST, PUT, PATCH, DELETE, ... it's a dice roll
HttpSession::Result gdriveHttpsRequest(const std::string& serverRelPath, //throw SysError
                                       const std::vector<std::string>& extraHeaders,
                                       const std::vector<CurlOption>& extraOptions,
                                       const std::function<void  (const void* buffer, size_t bytesToWrite)>& writeResponse /*throw X*/, //optional
                                       const std::function<size_t(      void* buffer, size_t bytesToRead )>& readRequest   /*throw X*/) //optional; returning 0 signals EOF
{
    const std::shared_ptr<HttpSessionManager> mgr = globalHttpSessionManager.get();
    if (!mgr)
        throw SysError(formatSystemError("gdriveHttpsRequest", L"", L"Function call not allowed during init/shutdown."));

    HttpSession::Result httpResult;

    mgr->access(HttpSessionId(GOOGLE_REST_API_SERVER), [&](HttpSession& session) //throw SysError
    {
        std::vector<CurlOption> options =
        {
            //https://developers.google.com/drive/api/v3/performance
            //"In order to receive a gzip-encoded response you must do two things: Set an Accept-Encoding header, ["gzip" automatically set by HttpSession]
            { CURLOPT_USERAGENT, "FreeFileSync (gzip)" }, //and modify your user agent to contain the string gzip."
        };
        append(options, extraOptions);

        httpResult = session.perform(serverRelPath, extraHeaders, options, writeResponse, readRequest); //throw SysError
    });
    return httpResult;
}

//========================================================================================================

struct GdriveUser
{
    std::wstring displayName;
    std::string email;
};
GdriveUser getGdriveUser(const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/about
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        { "fields", "user/displayName,user/emailAddress" },
    });
    std::string response;
    gdriveHttpsRequest("/drive/v3/about?" + queryParams, { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    if (const JsonValue* user = getChildFromJsonObject(jresponse, "user"))
    {
        const std::optional<std::string> displayName = getPrimitiveFromJsonObject(*user, "displayName");
        const std::optional<std::string> email       = getPrimitiveFromJsonObject(*user, "emailAddress");
        if (displayName && email)
            return { utfTo<std::wstring>(*displayName), *email };
    }

    throw SysError(formatGdriveErrorRaw(response));
}


const char htmlMessageTemplate[] = R"(<!DOCTYPE html>
<html lang="en">
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>TITLE_PLACEHOLDER</title>
        <style>
            * {
                font-family: -apple-system, 'Segoe UI', arial, Tahoma, Helvetica, sans-serif;
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
        <h1><img src="https://freefilesync.org/images/FreeFileSync.png" style="vertical-align:middle; height:50px;" alt=""> TITLE_PLACEHOLDER</h1>
        <div class="descr">MESSAGE_PLACEHOLDER</div>
    </body>
</html>
)";

struct GdriveAuthCode
{
    std::string code;
    std::string redirectUrl;
    std::string codeChallenge;
};

struct GdriveAccessToken
{
    std::string value;
    time_t validUntil = 0; //remaining lifetime of the access token
};

struct GdriveAccessInfo
{
    GdriveAccessToken accessToken;
    std::string refreshToken;
    GdriveUser userInfo;
};

GdriveAccessInfo gdriveExchangeAuthCode(const GdriveAuthCode& authCode) //throw SysError
{
    //https://developers.google.com/identity/protocols/OAuth2InstalledApp#exchange-authorization-code
    const std::string postBuf = xWwwFormUrlEncode(
    {
        { "code",          authCode.code },
        { "client_id",     getGdriveClientId() },
        { "client_secret", getGdriveClientSecret() },
        { "redirect_uri",  authCode.redirectUrl },
        { "grant_type",    "authorization_code" },
        { "code_verifier", authCode.codeChallenge },
    });
    std::string response;
    gdriveHttpsRequest("/oauth2/v4/token", {} /*extraHeaders*/, { { CURLOPT_POSTFIELDS, postBuf.c_str() } }, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> accessToken  = getPrimitiveFromJsonObject(jresponse, "access_token");
    const std::optional<std::string> refreshToken = getPrimitiveFromJsonObject(jresponse, "refresh_token");
    const std::optional<std::string> expiresIn    = getPrimitiveFromJsonObject(jresponse, "expires_in"); //e.g. 3600 seconds
    if (!accessToken || !refreshToken || !expiresIn)
        throw SysError(formatGdriveErrorRaw(response));

    const GdriveUser userInfo = getGdriveUser(*accessToken); //throw SysError

    return { { *accessToken, std::time(nullptr) + stringTo<time_t>(*expiresIn) }, *refreshToken, userInfo };
}


GdriveAccessInfo gdriveAuthorizeAccess(const std::string& gdriveLoginHint, const std::function<void()>& updateGui /*throw X*/) //throw SysError, X
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
        throw SysError(formatSystemError("getaddrinfo", replaceCpy(_("Error code %x"), L"%x", numberTo<std::wstring>(rcGai)), utfTo<std::wstring>(::gai_strerror(rcGai))));
    if (!servinfo)
        throw SysError(L"getaddrinfo: empty server info");

    const auto getBoundSocket = [](const auto& /*::addrinfo*/ ai)
    {
        SocketType testSocket = ::socket(ai.ai_family, ai.ai_socktype, ai.ai_protocol);
        if (testSocket == invalidSocket)
            THROW_LAST_SYS_ERROR_WSA("socket");
        ZEN_ON_SCOPE_FAIL(closeSocket(testSocket));

        if (::bind(testSocket, ai.ai_addr, static_cast<int>(ai.ai_addrlen)) != 0)
            THROW_LAST_SYS_ERROR_WSA("bind");

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
        THROW_LAST_SYS_ERROR_WSA("getsockname");

    if (addr.ss_family != AF_INET &&
        addr.ss_family != AF_INET6)
        throw SysError(formatSystemError("getsockname", L"", L"Unknown protocol family: " + numberTo<std::wstring>(addr.ss_family))); 

const int port = ntohs(reinterpret_cast<const sockaddr_in&>(addr).sin_port);
//the socket is not bound to a specific local IP => inet_ntoa(reinterpret_cast<const sockaddr_in&>(addr).sin_addr) == "0.0.0.0"
const std::string redirectUrl = "http://127.0.0.1:" + numberTo<std::string>(port);

if (::listen(socket, SOMAXCONN) != 0)
    THROW_LAST_SYS_ERROR_WSA("listen");


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
    { "client_id",      getGdriveClientId() },
    { "redirect_uri",   redirectUrl },
    { "response_type",  "code" },
    { "scope",          "https://www.googleapis.com/auth/drive" },
    { "code_challenge", codeChallenge },
    { "code_challenge_method", "plain" },
    { "login_hint",     gdriveLoginHint },
});
try
{
    openWithDefaultApp(utfTo<Zstring>(oauthUrl)); //throw FileError
}
catch (const FileError& e) { throw SysError(replaceCpy(e.toString(), L"\n\n", L'\n')); } //errors should be further enriched by context info => SysError

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
            THROW_LAST_SYS_ERROR_WSA("select");
        if (rc != 0)
            break;
        //else: time-out!
    }
    //potential race! if the connection is gone right after ::select() and before ::accept(), latter will hang
    const SocketType clientSocket = ::accept(socket,   //SOCKET   s,
                                             nullptr,  //sockaddr *addr,
                                             nullptr); //int      *addrlen
    if (clientSocket == invalidSocket)
        THROW_LAST_SYS_ERROR_WSA("accept");

    //receive first line of HTTP request
    std::string reqLine;
    for (;;)
    {
        const size_t blockSize = 64 * 1024;
        reqLine.resize(reqLine.size() + blockSize);
        const size_t bytesReceived = tryReadSocket(clientSocket, &*(reqLine.end() - blockSize), blockSize); //throw SysError
        reqLine.resize(reqLine.size() - (blockSize - bytesReceived)); //caveat: unsigned arithmetics

        if (contains(reqLine, "\r\n"))
        {
            reqLine = beforeFirst(reqLine, "\r\n", IfNotFoundReturn::none);
            break;
        }
        if (bytesReceived == 0 || reqLine.size() >= 100'000 /*bogus line length*/)
            break;
    }

    //get OAuth2.0 authorization result from Google, either:
    std::string code;
    std::string error;

    //parse header; e.g.: GET http://127.0.0.1:62054/?code=4/ZgBRsB9k68sFzc1Pz1q0__Kh17QK1oOmetySrGiSliXt6hZtTLUlYzm70uElNTH9vt1OqUMzJVeFfplMsYsn4uI HTTP/1.1
    const std::vector<std::string> statusItems = split(reqLine, ' ', SplitOnEmpty::allow); //Method SP Request-URI SP HTTP-Version CRLF

    if (statusItems.size() == 3 && statusItems[0] == "GET" && startsWith(statusItems[2], "HTTP/"))
    {
        for (const auto& [name, value] : xWwwFormUrlDecode(afterFirst(statusItems[1], "?", IfNotFoundReturn::none)))
            if (name == "code")
                code = value;
            else if (name == "error")
                error = value; //e.g. "access_denied" => no more detailed error info available :(
    } //"add explicit braces to avoid dangling else [-Wdangling-else]"

    std::optional<std::variant<GdriveAccessInfo, SysError>> authResult;

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
                throw SysError(replaceCpy(_("Error code %x"), L"%x",  + L"\"" + utfTo<std::wstring>(error) + L"\""));

            //do as many login-related tasks as possible while we have the browser as an error output device!
            //see AFS::connectNetworkFolder() => errors will be lost after time out in dir_exist_async.h!
            authResult = gdriveExchangeAuthCode({ code, redirectUrl, codeChallenge }); //throw SysError
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
        return std::get<GdriveAccessInfo>(*authResult);
    }
}
}


GdriveAccessToken gdriveRefreshAccess(const std::string& refreshToken) //throw SysError
{
    //https://developers.google.com/identity/protocols/OAuth2InstalledApp#offline
    const std::string postBuf = xWwwFormUrlEncode(
    {
        { "refresh_token", refreshToken },
        { "client_id",     getGdriveClientId() },
        { "client_secret", getGdriveClientSecret() },
        { "grant_type",    "refresh_token" },
    });

    std::string response;
    gdriveHttpsRequest("/oauth2/v4/token", {} /*extraHeaders*/, { { CURLOPT_POSTFIELDS, postBuf.c_str() } }, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> accessToken = getPrimitiveFromJsonObject(jresponse, "access_token");
    const std::optional<std::string> expiresIn   = getPrimitiveFromJsonObject(jresponse, "expires_in"); //e.g. 3600 seconds
    if (!accessToken || !expiresIn)
        throw SysError(formatGdriveErrorRaw(response));

    return { *accessToken, std::time(nullptr) + stringTo<time_t>(*expiresIn) };
}


void gdriveRevokeAccess(const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/identity/protocols/OAuth2InstalledApp#tokenrevoke
    const std::shared_ptr<HttpSessionManager> mgr = globalHttpSessionManager.get();
    if (!mgr)
        throw SysError(formatSystemError("gdriveRevokeAccess", L"", L"Function call not allowed during init/shutdown.")); 

    HttpSession::Result httpResult;
    std::string response;

    mgr->access(HttpSessionId(Zstr("accounts.google.com")), [&](HttpSession& session) //throw SysError
    {
        httpResult = session.perform("/o/oauth2/revoke?token=" + accessToken, { "Content-Type: application/x-www-form-urlencoded" }, {} /*extraOptions*/,
        [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/); //throw SysError
    });

    if (httpResult.statusCode != 200)
        throw SysError(formatGdriveErrorRaw(response));
}


int64_t gdriveGetMyDriveFreeSpace(const std::string& accessToken) //throw SysError; returns < 0 if not available
{
    //https://developers.google.com/drive/api/v3/reference/about
    std::string response;
    gdriveHttpsRequest("/drive/v3/about?fields=storageQuota", { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    if (const JsonValue* storageQuota = getChildFromJsonObject(jresponse, "storageQuota"))
    {
        const std::optional<std::string> usage = getPrimitiveFromJsonObject(*storageQuota, "usage");
        const std::optional<std::string> limit = getPrimitiveFromJsonObject(*storageQuota, "limit");
        if (usage)
        {
            if (!limit) //"will not be present if the user has unlimited storage."
                return std::numeric_limits<int64_t>::max();

            const auto bytesUsed  = stringTo<int64_t>(*usage);
            const auto bytesLimit = stringTo<int64_t>(*limit);

            if (0 <= bytesUsed && bytesUsed <= bytesLimit)
                return bytesLimit - bytesUsed;
        }
    }
    throw SysError(formatGdriveErrorRaw(response));
}


struct DriveDetails
{
    std::string driveId;
    Zstring driveName;
};
std::vector<DriveDetails> getSharedDrives(const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/drives/list
    std::vector<DriveDetails> sharedDrives;
    {
        std::optional<std::string> nextPageToken;
        do
        {
            std::string queryParams = xWwwFormUrlEncode(
            {
                { "pageSize", "100" }, //"[1, 100] Default: 10"
                { "fields", "nextPageToken,drives(id,name)"
}
});
if (nextPageToken)
queryParams += '&' + xWwwFormUrlEncode({ { "pageToken", *nextPageToken } });

std::string response;
gdriveHttpsRequest("/drive/v3/drives?" + queryParams, { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
[&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

JsonValue jresponse;
try { jresponse = parseJson(response); }
catch (JsonParsingError&) {}

/**/             nextPageToken = getPrimitiveFromJsonObject(jresponse, "nextPageToken");
const JsonValue* drives        = getChildFromJsonObject    (jresponse, "drives");
if (!drives || drives->type != JsonValue::Type::array)
throw SysError(formatGdriveErrorRaw(response));

for (const JsonValue& driveVal : drives->arrayVal)
{
    std::optional<std::string> driveId   = getPrimitiveFromJsonObject(driveVal, "id");
        std::optional<std::string> driveName = getPrimitiveFromJsonObject(driveVal, "name");
        if (!driveId || !driveName)
            throw SysError(formatGdriveErrorRaw(serializeJson(driveVal)));

        sharedDrives.push_back({ std::move(*driveId), utfTo<Zstring>(*driveName) });
    }
}
while (nextPageToken);
}
return sharedDrives;
}


enum class GdriveItemType : unsigned char
{
file,
folder,
shortcut,
};
enum class FileOwner : unsigned char
{
none, //"ownedByMe" not populated for items in Shared Drives.
me,
other,
};
struct GdriveItemDetails
{
Zstring itemName;
uint64_t fileSize = 0;
time_t modTime  = 0;
//--- minimize padding ---
GdriveItemType type = GdriveItemType::file;
FileOwner owner = FileOwner::none;
//------------------------
std::string targetId; //for GdriveItemType::shortcut: https://developers.google.com/drive/api/v3/shortcuts
std::vector<std::string> parentIds;

bool operator==(const GdriveItemDetails&) const = default;
};


GdriveItemDetails extractItemDetails(JsonValue jvalue) //throw SysError
{
    assert(jvalue.type == JsonValue::Type::object);

    /**/  std::optional<std::string> itemName     = getPrimitiveFromJsonObject(jvalue, "name");
    const std::optional<std::string> mimeType     = getPrimitiveFromJsonObject(jvalue, "mimeType");
    const std::optional<std::string> ownedByMe    = getPrimitiveFromJsonObject(jvalue, "ownedByMe");
    const std::optional<std::string> size         = getPrimitiveFromJsonObject(jvalue, "size");
    const std::optional<std::string> modifiedTime = getPrimitiveFromJsonObject(jvalue, "modifiedTime");
    const JsonValue*                 parents      = getChildFromJsonObject    (jvalue, "parents");
    const JsonValue*                 shortcut     = getChildFromJsonObject    (jvalue, "shortcutDetails");

    if (!itemName || !mimeType || !modifiedTime)
        throw SysError(formatGdriveErrorRaw(serializeJson(jvalue)));

    const GdriveItemType type = *mimeType == gdriveFolderMimeType   ? GdriveItemType::folder :
                                *mimeType == gdriveShortcutMimeType ? GdriveItemType::shortcut :
                                GdriveItemType::file;

    const FileOwner owner = ownedByMe ? (*ownedByMe == "true" ? FileOwner::me : FileOwner::other) : FileOwner::none; //"Not populated for items in Shared Drives"
    const uint64_t fileSize = size ? stringTo<uint64_t>(*size) : 0; //not available for folders and shortcuts

    //RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    const TimeComp tc = parseTime("%Y-%m-%dT%H:%M:%S", beforeLast(*modifiedTime, '.', IfNotFoundReturn::all));
    if (tc == TimeComp() || !endsWith(*modifiedTime, 'Z')) //'Z' means "UTC" => it seems Google doesn't use the time-zone offset postfix
        throw SysError(L"Modification time could not be parsed. (" + utfTo<std::wstring>(*modifiedTime) + L')');

    time_t modTime = utcToTimeT(tc); //returns -1 on error
    if (modTime == -1)
    {
        if (tc.year == 1600 || //zero-initialized FILETIME equals "December 31, 1600" or "January 1, 1601"
            tc.year == 1601 || // => yes, possible even on Google Drive: https://freefilesync.org/forum/viewtopic.php?t=6602
            tc.year == 1) //WTF: 0001-01-01T00:00:00.000Z https://freefilesync.org/forum/viewtopic.php?t=7403
            modTime = 0;
        else
            throw SysError(L"Modification time could not be parsed. (" + utfTo<std::wstring>(*modifiedTime) + L')');
    }

    std::vector<std::string> parentIds;
    if (parents) //item without parents is possible! e.g. shared item located in "Shared with me", referenced via a Shortcut
        for (const JsonValue& parentVal : parents->arrayVal)
        {
            if (parentVal.type != JsonValue::Type::string)
                throw SysError(formatGdriveErrorRaw(serializeJson(jvalue)));
            parentIds.emplace_back(parentVal.primVal);
        }

    if (!!shortcut != (type == GdriveItemType::shortcut))
        throw SysError(formatGdriveErrorRaw(serializeJson(jvalue)));

    std::string targetId;
    if (shortcut)
    {
        std::optional<std::string> targetItemId = getPrimitiveFromJsonObject(*shortcut, "targetId");
        if (!targetItemId || targetItemId->empty())
            throw SysError(formatGdriveErrorRaw(serializeJson(jvalue)));

        targetId = std::move(*targetItemId);
        //evaluate "targetMimeType" ? don't bother: "The MIME type of a shortcut can become stale"!
    }

    return { utfTo<Zstring>(*itemName), fileSize, modTime, type, owner, std::move(targetId), std::move(parentIds) };
}


GdriveItemDetails getItemDetails(const std::string& itemId, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/get
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        { "fields", "trashed,name,mimeType,ownedByMe,size,modifiedTime,parents,shortcutDetails(targetId)" },
        { "supportsAllDrives", "true" },
    });
    std::string response;
    gdriveHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams, { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);
    try
    {
        const JsonValue jvalue = parseJson(response); //throw JsonParsingError

        //careful: do NOT return details about trashed items! they don't exist as far as FFS is concerned!!!
        const std::optional<std::string> trashed = getPrimitiveFromJsonObject(jvalue, "trashed");
        if (!trashed)
            throw SysError(formatGdriveErrorRaw(response));
        else if (*trashed == "true")
            throw SysError(L"Item has been trashed.");

        return extractItemDetails(jvalue); //throw SysError
    }
    catch (JsonParsingError&) { throw SysError(formatGdriveErrorRaw(response)); }
}


struct GdriveItem
{
    std::string itemId;
    GdriveItemDetails details;
};
std::vector<GdriveItem> readFolderContent(const std::string& folderId, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/list
    std::vector<GdriveItem> childItems;
    {
        std::optional<std::string> nextPageToken;
        do
        {
            std::string queryParams = xWwwFormUrlEncode(
            {
                { "corpora", "allDrives" }, //"The 'user' corpus includes all files in "My Drive" and "Shared with me" https://developers.google.com/drive/api/v3/reference/files/list
                { "includeItemsFromAllDrives", "true" },
                { "pageSize", "1000" }, //"[1, 1000] Default: 100"
                { "q", "trashed=false and '" + folderId + "' in parents" },
                { "spaces", "drive" },
                { "supportsAllDrives", "true" },
                { "fields", "nextPageToken,incompleteSearch,files(id,name,mimeType,ownedByMe,size,modifiedTime,parents,shortcutDetails(targetId))" }, //https://developers.google.com/drive/api/v3/reference/files
            });
            if (nextPageToken)
                queryParams += '&' + xWwwFormUrlEncode({ { "pageToken", *nextPageToken } });

            std::string response;
            gdriveHttpsRequest("/drive/v3/files?" + queryParams, { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
            [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

            JsonValue jresponse;
            try { jresponse = parseJson(response); }
            catch (JsonParsingError&) {}

            /**/                             nextPageToken    = getPrimitiveFromJsonObject(jresponse, "nextPageToken");
            const std::optional<std::string> incompleteSearch = getPrimitiveFromJsonObject(jresponse, "incompleteSearch");
            const JsonValue*                 files            = getChildFromJsonObject    (jresponse, "files");
            if (!incompleteSearch || *incompleteSearch != "false" || !files || files->type != JsonValue::Type::array)
                throw SysError(formatGdriveErrorRaw(response));

            for (const JsonValue& childVal : files->arrayVal)
            {
                std::optional<std::string> itemId = getPrimitiveFromJsonObject(childVal, "id");
                if (!itemId || itemId->empty())
                    throw SysError(formatGdriveErrorRaw(serializeJson(childVal)));

                GdriveItemDetails itemDetails(extractItemDetails(childVal)); //throw SysError
                assert(std::find(itemDetails.parentIds.begin(), itemDetails.parentIds.end(), folderId) != itemDetails.parentIds.end());

                childItems.push_back({ std::move(*itemId), std::move(itemDetails) });
            }
        }
        while (nextPageToken);
    }
    return childItems;
}


struct FileChange
{
    std::string itemId;
    std::optional<GdriveItemDetails> details; //empty if item was deleted/trashed
};
struct DriveChange
{
    std::string driveId;
    Zstring driveName; //empty if shared drive was deleted
};
struct ChangesDelta
{
    std::string newStartPageToken;
    std::vector<FileChange> fileChanges;
    std::vector<DriveChange> driveChanges;
};
ChangesDelta getChangesDelta(const std::string& startPageToken, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/changes/list
    ChangesDelta delta;
    std::optional<std::string> nextPageToken = startPageToken;
    for (;;)
    {
        const std::string& queryParams = xWwwFormUrlEncode(
        {
            { "pageToken", *nextPageToken },
            { "fields", "kind,nextPageToken,newStartPageToken,changes(kind,changeType,removed,fileId,file(trashed,name,mimeType,ownedByMe,size,modifiedTime,parents,shortcutDetails(targetId)),driveId,drive(name))" },
            { "includeItemsFromAllDrives", "true" },
            { "pageSize", "1000" }, //"[1, 1000] Default: 100"
            { "spaces", "drive" },
            { "supportsAllDrives", "true" },
            //do NOT "restrictToMyDrive": we're also interested in "Shared with me" items, which might be referenced by a shortcut in "My Drive"
        });
        std::string response;
        gdriveHttpsRequest("/drive/v3/changes?" + queryParams, { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
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
            throw SysError(formatGdriveErrorRaw(response));

        for (const JsonValue& childVal : changes->arrayVal)
        {
            const std::optional<std::string> kind       = getPrimitiveFromJsonObject(childVal, "kind");
            const std::optional<std::string> changeType = getPrimitiveFromJsonObject(childVal, "changeType");
            const std::optional<std::string> removed    = getPrimitiveFromJsonObject(childVal, "removed");
            if (!kind || *kind != "drive#change" || !changeType || !removed)
                throw SysError(formatGdriveErrorRaw(serializeJson(childVal)));

            if (*changeType == "file")
            {
                std::optional<std::string> fileId = getPrimitiveFromJsonObject(childVal, "fileId");
                if (!fileId || fileId->empty())
                    throw SysError(formatGdriveErrorRaw(serializeJson(childVal)));

                FileChange change;
                change.itemId = std::move(*fileId);
                if (*removed != "true")
                {
                    const JsonValue* file = getChildFromJsonObject(childVal, "file");
                    if (!file)
                        throw SysError(formatGdriveErrorRaw(serializeJson(childVal)));

                    const std::optional<std::string> trashed = getPrimitiveFromJsonObject(*file, "trashed");
                    if (!trashed)
                        throw SysError(formatGdriveErrorRaw(serializeJson(childVal)));

                    if (*trashed != "true")
                        change.details = extractItemDetails(*file); //throw SysError
                }
                delta.fileChanges.push_back(std::move(change));
            }
            else if (*changeType == "drive")
            {
                std::optional<std::string> driveId = getPrimitiveFromJsonObject(childVal, "driveId");
                if (!driveId || driveId->empty())
                    throw SysError(formatGdriveErrorRaw(serializeJson(childVal)));

                DriveChange change;
                change.driveId = std::move(*driveId);
                if (*removed != "true")
                {
                    const JsonValue* drive = getChildFromJsonObject(childVal, "drive");
                    if (!drive)
                        throw SysError(formatGdriveErrorRaw(serializeJson(childVal)));

                    const std::optional<std::string> name = getPrimitiveFromJsonObject(*drive, "name");
                    if (!name || name->empty())
                        throw SysError(formatGdriveErrorRaw(serializeJson(childVal)));

                    change.driveName = utfTo<Zstring>(*name);
                }
                delta.driveChanges.push_back(std::move(change));
            }
            else assert(false); //no other types (yet!)
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
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        { "supportsAllDrives", "true" },
    });
    std::string response;
    gdriveHttpsRequest("/drive/v3/changes/startPageToken?" + queryParams, { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> startPageToken = getPrimitiveFromJsonObject(jresponse, "startPageToken");
    if (!startPageToken)
        throw SysError(formatGdriveErrorRaw(response));

    return *startPageToken;
}


//- if item is a folder: deletes recursively!!!
//- even deletes a hardlink with multiple parents => use gdriveUnlinkParent() first
void gdriveDeleteItem(const std::string& itemId, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/delete
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        { "supportsAllDrives", "true" },
    });
    std::string response;
    const HttpSession::Result httpResult = gdriveHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams, { "Authorization: Bearer " + accessToken }, //throw SysError
    { { CURLOPT_CUSTOMREQUEST, "DELETE" } }, [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    if (response.empty() && httpResult.statusCode == 204)
        return; //"If successful, this method returns an empty response body"

    throw SysError(formatGdriveErrorRaw(response));
}


//item is NOT deleted when last parent is removed: it is just not accessible via the "My Drive" hierarchy but still adds to quota! => use for hard links only!
void gdriveUnlinkParent(const std::string& itemId, const std::string& parentId, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/update
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        { "removeParents", parentId },
        { "supportsAllDrives", "true" },
        { "fields", "id,parents" }, //for test if operation was successful
    });
    std::string response;
    const HttpSession::Result httpResult = gdriveHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams, //throw SysError
    { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" },
    { { CURLOPT_CUSTOMREQUEST, "PATCH" }, { CURLOPT_POSTFIELDS, "{}" } },
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    if (response.empty() && httpResult.statusCode == 204)
        return; //removing last parent of item not owned by us returns "204 No Content" (instead of 200 + file body)

    JsonValue jresponse;
    try { jresponse = parseJson(response); /*throw JsonParsingError*/ }
    catch (const JsonParsingError&) {}

    const std::optional<std::string> id      = getPrimitiveFromJsonObject(jresponse, "id"); //id is returned on "success", unlike "parents", see below...
    const JsonValue*                 parents = getChildFromJsonObject(jresponse, "parents");
    if (!id || *id != itemId)
        throw SysError(formatGdriveErrorRaw(response));

    if (parents) //when last parent is removed, Google does NOT return the parents array (not even an empty one!)
        if (parents->type != JsonValue::Type::array ||
            std::any_of(parents->arrayVal.begin(), parents->arrayVal.end(),
        [&](const JsonValue& jval) { return jval.type == JsonValue::Type::string && jval.primVal == parentId; }))
    throw SysError(L"gdriveUnlinkParent: Google Drive internal failure"); //user should never see this...
}


//- if item is a folder: trashes recursively!!!
//- a hardlink with multiple parents will NOT be accessible anymore via any of its path aliases!
void gdriveMoveToTrash(const std::string& itemId, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/update
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        { "supportsAllDrives", "true" },
        { "fields", "trashed" },
    });
    const std::string postBuf = R"({ "trashed": true })";

    std::string response;
    gdriveHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams, { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" }, //throw SysError
    { { CURLOPT_CUSTOMREQUEST, "PATCH" }, { CURLOPT_POSTFIELDS, postBuf.c_str() } },
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); /*throw JsonParsingError*/ }
    catch (const JsonParsingError&) {}

    const std::optional<std::string> trashed = getPrimitiveFromJsonObject(jresponse, "trashed");
    if (!trashed || *trashed != "true")
        throw SysError(formatGdriveErrorRaw(response));
}


//folder name already existing? will (happily) create duplicate => caller must check!
std::string /*folderId*/ gdriveCreateFolderPlain(const Zstring& folderName, const std::string& parentId, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/folder#creating_a_folder
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        { "supportsAllDrives", "true" },
        { "fields", "id" },
    });
    JsonValue postParams(JsonValue::Type::object);
    postParams.objectVal.emplace("mimeType", gdriveFolderMimeType);
    postParams.objectVal.emplace("name", utfTo<std::string>(folderName));
    postParams.objectVal.emplace("parents", std::vector<JsonValue> { JsonValue(parentId) });
    const std::string& postBuf = serializeJson(postParams, "" /*lineBreak*/, "" /*indent*/);

    std::string response;
    gdriveHttpsRequest("/drive/v3/files?" + queryParams, { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" },
    { { CURLOPT_POSTFIELDS, postBuf.c_str() } },
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/); //throw SysError

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> itemId = getPrimitiveFromJsonObject(jresponse, "id");
    if (!itemId)
        throw SysError(formatGdriveErrorRaw(response));
    return *itemId;
}


//shortcut name already existing? will (happily) create duplicate => caller must check!
std::string /*shortcutId*/ gdriveCreateShortcutPlain(const Zstring& shortcutName, const std::string& parentId, const std::string& targetId, const std::string& accessToken) //throw SysError
{
    /* https://developers.google.com/drive/api/v3/shortcuts
       - targetMimeType is determined automatically (ignored if passed)
       - creating shortcuts to shortcuts fails with "Internal Error"              */
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        { "supportsAllDrives", "true" },
        { "fields", "id" },
    });
    JsonValue shortcutDetails(JsonValue::Type::object);
    shortcutDetails.objectVal.emplace("targetId", targetId);

    JsonValue postParams(JsonValue::Type::object);
    postParams.objectVal.emplace("mimeType", gdriveShortcutMimeType);
    postParams.objectVal.emplace("name", utfTo<std::string>(shortcutName));
    postParams.objectVal.emplace("parents", std::vector<JsonValue> { JsonValue(parentId) });
    postParams.objectVal.emplace("shortcutDetails", std::move(shortcutDetails));
    const std::string& postBuf = serializeJson(postParams, "" /*lineBreak*/, "" /*indent*/);

    std::string response;
    gdriveHttpsRequest("/drive/v3/files?" + queryParams, { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" },
    { { CURLOPT_POSTFIELDS, postBuf.c_str() } },
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/); //throw SysError

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> itemId = getPrimitiveFromJsonObject(jresponse, "id");
    if (!itemId)
        throw SysError(formatGdriveErrorRaw(response));
    return *itemId;
}


//target name already existing? will (happily) create duplicate items => caller must check!
//can copy files + shortcuts (but fails for folders) + Google-specific file types (.gdoc, .gsheet, .gslides)
std::string /*fileId*/ gdriveCopyFile(const std::string& fileId, const std::string& parentIdTo, const Zstring& newName, time_t newModTime, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/copy
    const std::string queryParams = xWwwFormUrlEncode(
    {
        { "supportsAllDrives", "true" },
        { "fields", "id" },
    });

    //more Google Drive peculiarities: changing the file name changes modifiedTime!!! => workaround:

    //RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    const std::string modTimeRfc = utfTo<std::string>(formatTime(Zstr("%Y-%m-%dT%H:%M:%S.000Z"), getUtcTime(newModTime))); //returns empty string on failure
    if (modTimeRfc.empty())
        throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(newModTime) + L')');

    JsonValue postParams(JsonValue::Type::object);
    postParams.objectVal.emplace("name", utfTo<std::string>(newName));
    postParams.objectVal.emplace("parents", std::vector<JsonValue> { JsonValue(parentIdTo) });
    postParams.objectVal.emplace("modifiedTime", modTimeRfc);
    const std::string& postBuf = serializeJson(postParams, "" /*lineBreak*/, "" /*indent*/);

    std::string response;
    gdriveHttpsRequest("/drive/v3/files/" + fileId + "/copy?" + queryParams, //throw SysError
    { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" }, { { CURLOPT_POSTFIELDS, postBuf.c_str() } },
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); /*throw JsonParsingError*/ }
    catch (const JsonParsingError&) {}

    const std::optional<std::string> itemId = getPrimitiveFromJsonObject(jresponse, "id");
    if (!itemId)
        throw SysError(formatGdriveErrorRaw(response));

    return *itemId;

}


//target name already existing? will (happily) create duplicate items => caller must check!
void gdriveMoveAndRenameItem(const std::string& itemId, const std::string& parentIdFrom, const std::string& parentIdTo,
                             const Zstring& newName, time_t newModTime, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/folder#moving_files_between_folders
    std::string queryParams = xWwwFormUrlEncode(
    {
        { "supportsAllDrives", "true" },
        { "fields", "name,parents" }, //for test if operation was successful
    });

    if (parentIdFrom != parentIdTo)
        queryParams += '&' + xWwwFormUrlEncode(
    {
        { "removeParents", parentIdFrom },
        { "addParents",    parentIdTo },
    });

    //more Google Drive peculiarities: changing the file name changes modifiedTime!!! => workaround:

    //RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    const std::string modTimeRfc = utfTo<std::string>(formatTime(Zstr("%Y-%m-%dT%H:%M:%S.000Z"), getUtcTime(newModTime))); //returns empty string on failure
    if (modTimeRfc.empty())
        throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(newModTime) + L')');

    JsonValue postParams(JsonValue::Type::object);
    postParams.objectVal.emplace("name", utfTo<std::string>(newName));
    postParams.objectVal.emplace("modifiedTime", modTimeRfc);
    const std::string& postBuf = serializeJson(postParams, "" /*lineBreak*/, "" /*indent*/);

    std::string response;
    gdriveHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams, //throw SysError
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
        throw SysError(formatGdriveErrorRaw(response));

    if (!std::any_of(parents->arrayVal.begin(), parents->arrayVal.end(),
    [&](const JsonValue& jval) { return jval.type == JsonValue::Type::string && jval.primVal == parentIdTo; }))
    throw SysError(formatSystemError("gdriveMoveAndRenameItem", L"", L"Google Drive internal failure.")); //user should never see this...
}


#if 0
void setModTime(const std::string& itemId, time_t modTime, const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/update
    //RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    const std::string& modTimeRfc = formatTime<std::string>("%Y-%m-%dT%H:%M:%S.000Z", getUtcTime(modTime)); //returns empty string on failure
    if (modTimeRfc.empty())
        throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(modTime) + L')');

    const std::string& queryParams = xWwwFormUrlEncode(
    {
        { "supportsAllDrives", "true" },
        { "fields", "modifiedTime" },
    });
    const std::string postBuf = R"({ "modifiedTime": ")" + modTimeRfc + "\" }";

    std::string response;
    gdriveHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams, { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" }, //throw SysError
    { { CURLOPT_CUSTOMREQUEST, "PATCH" }, { CURLOPT_POSTFIELDS, postBuf.c_str() } },
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); /*throw JsonParsingError*/ }
    catch (const JsonParsingError&) {}

    const std::optional<std::string> modifiedTime = getPrimitiveFromJsonObject(jresponse, "modifiedTime");
    if (!modifiedTime || *modifiedTime != modTimeRfc)
        throw SysError(formatGdriveErrorRaw(response));
}
#endif


DEFINE_NEW_SYS_ERROR(SysErrorAbusiveFile)
void gdriveDownloadFileImpl(const std::string& fileId, const std::function<void(const void* buffer, size_t bytesToWrite)>& writeBlock /*throw X*/, //throw SysError, SysErrorAbusiveFile, X
                            bool acknowledgeAbuse, const std::string& accessToken)
{
    //https://developers.google.com/drive/api/v3/manage-downloads
    //doesn't work for Google-specific file types (.gdoc, .gsheet, .gslides)
    //  => interesting: Google Backup & Sync still "downloads" them but in some URL-file format:
    //      {"url": "https://docs.google.com/open?id=FILE_ID", "doc_id": "FILE_ID", "email": "ACCOUNT_EMAIL"}

    std::string queryParams = xWwwFormUrlEncode(
    {
        { "supportsAllDrives", "true" },
        { "alt", "media" },
    });
    if (acknowledgeAbuse) //apply on demand only! https://freefilesync.org/forum/viewtopic.php?t=7520")
        queryParams += '&' + xWwwFormUrlEncode({ { "acknowledgeAbuse", "true" } });

    std::string responseHead; //save front part of the response in case we get an error
    bool headFlushed = false;

    const HttpSession::Result httpResult = gdriveHttpsRequest("/drive/v3/files/" + fileId + '?' + queryParams, //throw SysError, X
    { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/,
    [&](const void* buffer, size_t bytesToWrite)
    {
        if (responseHead.size() < 10000) //don't access writeBlock() in case of error! (=> support acknowledgeAbuse retry handling)
            responseHead.append(static_cast<const char*>(buffer), bytesToWrite);
        else
        {
            if (!headFlushed)
            {
                headFlushed = true;
                writeBlock(responseHead.c_str(), responseHead.size()); //throw X
            }

            writeBlock(buffer, bytesToWrite); //throw X
        }
    }, nullptr /*readRequest*/);

    if (httpResult.statusCode / 100 != 2)
    {
        /* https://freefilesync.org/forum/viewtopic.php?t=7463 => HTTP status code 403 + body:
            { "error": { "errors": [{ "domain": "global",
                                      "reason": "cannotDownloadAbusiveFile",
                                      "message": "This file has been identified as malware or spam and cannot be downloaded." }],
                         "code": 403,
                         "message": "This file has been identified as malware or spam and cannot be downloaded." }}
        */
        if (!headFlushed && httpResult.statusCode == 403 && contains(responseHead, "\"cannotDownloadAbusiveFile\""))
            throw SysErrorAbusiveFile(formatGdriveErrorRaw(responseHead));

        throw SysError(formatGdriveErrorRaw(responseHead));
    }

    if (!headFlushed)
        writeBlock(responseHead.c_str(), responseHead.size()); //throw X
}


void gdriveDownloadFile(const std::string& fileId, const std::function<void(const void* buffer, size_t bytesToWrite)>& writeBlock /*throw X*/, //throw SysError, X
                        const std::string& accessToken)
{
    try
    {
        gdriveDownloadFileImpl(fileId, writeBlock /*throw X*/, false /*acknowledgeAbuse*/, accessToken); //throw SysError, SysErrorAbusiveFile, X
    }
    catch (SysErrorAbusiveFile&)
    {
        gdriveDownloadFileImpl(fileId, writeBlock /*throw X*/, true /*acknowledgeAbuse*/, accessToken); //throw SysError, (SysErrorAbusiveFile), X
    }
}


#if 0
//file name already existing? => duplicate file created!
//note: Google Drive upload is already transactional!
//upload "small files" (5 MB or less; enforced by Google?) in a single round-trip
std::string /*itemId*/ gdriveUploadSmallFile(const Zstring& fileName, const std::string& parentId, uint64_t streamSize, std::optional<time_t> modTime, //throw SysError, X
                                             const std::function<size_t(void* buffer, size_t bytesToRead)>& readBlock /*throw X*/, //returning 0 signals EOF: Posix read() semantics
                                             const std::string& accessToken)
{
    //https://developers.google.com/drive/api/v3/folder#inserting_a_file_in_a_folder
    //https://developers.google.com/drive/api/v3/manage-uploads#http_1

    JsonValue postParams(JsonValue::Type::object);
    postParams.objectVal.emplace("name", utfTo<std::string>(fileName));
    postParams.objectVal.emplace("parents", std::vector<JsonValue> { JsonValue(parentId) });
    if (modTime) //convert to RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    {
        const std::string& modTimeRfc = utfTo<std::string>(formatTime(Zstr("%Y-%m-%dT%H:%M:%S.000Z"), getUtcTime(*modTime))); //returns empty string on failure
        if (modTimeRfc.empty())
            throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(*modTime) + L')');

        postParams.objectVal.emplace("modifiedTime", modTimeRfc);
    }
    const std::string& metaDataBuf = serializeJson(postParams, "" /*lineBreak*/, "" /*indent*/);

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

    const std::string& queryParams = xWwwFormUrlEncode(
    {
        { "supportsAllDrives", "true" },
        { "uploadType", "multipart" },
    });
    std::string response;
    const HttpSession::Result httpResult = gdriveHttpsRequest("/upload/drive/v3/files?" + queryParams, //throw SysError, X
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
        throw SysError(formatGdriveErrorRaw(response));

    return *itemId;
}
#endif


//file name already existing? => duplicate file created!
//note: Google Drive upload is already transactional!
std::string /*itemId*/ gdriveUploadFile(const Zstring& fileName, const std::string& parentId, std::optional<time_t> modTime, //throw SysError, X
                                        const std::function<size_t(void* buffer, size_t bytesToRead)>& readBlock /*throw X*/, //returning 0 signals EOF: Posix read() semantics
                                        const std::string& accessToken)
{
    //https://developers.google.com/drive/api/v3/folder#inserting_a_file_in_a_folder
    //https://developers.google.com/drive/api/v3/manage-uploads#resumable

    //step 1: initiate resumable upload session
    std::string uploadUrlRelative;
    {
        const std::string& queryParams = xWwwFormUrlEncode(
        {
            { "supportsAllDrives", "true" },
            { "uploadType", "resumable" },
        });
        JsonValue postParams(JsonValue::Type::object);
        postParams.objectVal.emplace("name", utfTo<std::string>(fileName));
        postParams.objectVal.emplace("parents", std::vector<JsonValue> { JsonValue(parentId) });
        if (modTime) //convert to RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
        {
            const std::string& modTimeRfc = utfTo<std::string>(formatTime(Zstr("%Y-%m-%dT%H:%M:%S.000Z"), getUtcTime(*modTime))); //returns empty string on failure
            if (modTimeRfc.empty())
                throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(*modTime) + L')');

            postParams.objectVal.emplace("modifiedTime", modTimeRfc);
        }
        const std::string& postBuf = serializeJson(postParams, "" /*lineBreak*/, "" /*indent*/);
        //---------------------------------------------------

        std::string uploadUrl;

        auto onBytesReceived = [&](const char* buffer, size_t len)
        {
            //inside libcurl's C callstack => better not throw exceptions here!!!
            //"The callback will be called once for each header and only complete header lines are passed on to the callback" (including \r\n at the end)
            if (startsWithAsciiNoCase(std::string_view(buffer, len), "Location:"))
            {
                uploadUrl.assign(buffer, len); //not null-terminated!
                uploadUrl = afterFirst(uploadUrl, ':', IfNotFoundReturn::none);
                trim(uploadUrl);
            }
            return len;
        };
        using ReadCbType = decltype(onBytesReceived);
        using ReadCbWrapperType =          size_t (*)(const char* buffer, size_t size, size_t nitems, ReadCbType* callbackData); //needed for cdecl function pointer cast
        ReadCbWrapperType onBytesReceivedWrapper = [](const char* buffer, size_t size, size_t nitems, ReadCbType* callbackData)
        {
            return (*callbackData)(buffer, size * nitems); //free this poor little C-API from its shackles and redirect to a proper lambda
        };

        std::string response;
        const HttpSession::Result httpResult = gdriveHttpsRequest("/upload/drive/v3/files?" + queryParams, //throw SysError
        { "Authorization: Bearer " + accessToken, "Content-Type: application/json; charset=UTF-8" },
        { { CURLOPT_POSTFIELDS, postBuf.c_str() }, { CURLOPT_HEADERDATA, &onBytesReceived }, { CURLOPT_HEADERFUNCTION, onBytesReceivedWrapper } },
        [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

        if (httpResult.statusCode != 200)
            throw SysError(formatGdriveErrorRaw(response));

        if (!startsWith(uploadUrl, "https://www.googleapis.com/"))
            throw SysError(L"Invalid upload URL: " + utfTo<std::wstring>(uploadUrl)); //user should never see this

        uploadUrlRelative = afterFirst(uploadUrl, "googleapis.com", IfNotFoundReturn::none);
    }
    //---------------------------------------------------
    //step 2: upload file content

    //not officially documented, but Google Drive supports compressed file upload when "Content-Encoding: gzip" is set! :)))
    InputStreamAsGzip gzipStream(readBlock); //throw SysError

    auto readBlockAsGzip = [&](void* buffer, size_t bytesToRead) { return gzipStream.read(buffer, bytesToRead); }; //throw SysError, X
    //returns "bytesToRead" bytes unless end of stream! => fits into "0 signals EOF: Posix read() semantics"

    std::string response;
    gdriveHttpsRequest(uploadUrlRelative, { "Content-Encoding: gzip"  }, {} /*extraOptions*/, //throw SysError, X
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, readBlockAsGzip);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> itemId = getPrimitiveFromJsonObject(jresponse, "id");
    if (!itemId)
        throw SysError(formatGdriveErrorRaw(response));

    return *itemId;
}


//instead of the "root" alias Google uses an actual ID in file metadata
std::string /*itemId*/ getMyDriveId(const std::string& accessToken) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/get
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        { "supportsAllDrives", "true" },
        { "fields", "id" },
    });
    std::string response;
    gdriveHttpsRequest("/drive/v3/files/root?" + queryParams, { "Authorization: Bearer " + accessToken }, {} /*extraOptions*/, //throw SysError
    [&](const void* buffer, size_t bytesToWrite) { response.append(static_cast<const char*>(buffer), bytesToWrite); }, nullptr /*readRequest*/);

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> itemId = getPrimitiveFromJsonObject(jresponse, "id");
    if (!itemId)
        throw SysError(formatGdriveErrorRaw(response));

    return *itemId;
}


class GdriveAccessBuffer //per-user-session! => serialize access (perf: amortized fully buffered!)
{
public:
    GdriveAccessBuffer(const GdriveAccessInfo& accessInfo) : accessInfo_(accessInfo) {}

    GdriveAccessBuffer(MemoryStreamIn<std::string>& stream) //throw SysError
    {
        accessInfo_.accessToken.validUntil = readNumber<int64_t>(stream);                             //
        accessInfo_.accessToken.value      = readContainer<std::string>(stream);                      //
        accessInfo_.refreshToken           = readContainer<std::string>(stream);                      //SysErrorUnexpectedEos
        accessInfo_.userInfo.displayName   = utfTo<std::wstring>(readContainer<std::string>(stream)); //
        accessInfo_.userInfo.email         =                     readContainer<std::string>(stream);  //
    }

    void serialize(MemoryStreamOut<std::string>& stream) const
    {
        writeNumber<int64_t>(stream, accessInfo_.accessToken.validUntil);
        static_assert(sizeof(accessInfo_.accessToken.validUntil) <= sizeof(int64_t)); //ensure cross-platform compatibility!
        writeContainer(stream, accessInfo_.accessToken.value);
        writeContainer(stream, accessInfo_.refreshToken);
        writeContainer(stream, utfTo<std::string>(accessInfo_.userInfo.displayName));
        writeContainer(stream,                    accessInfo_.userInfo.email);
    }

    std::string getAccessToken() //throw SysError
    {
        if (accessInfo_.accessToken.validUntil <= std::time(nullptr) + std::chrono::seconds(HTTP_SESSION_ACCESS_TIME_OUT).count() + 5 /*some leeway*/) //expired/will expire
            accessInfo_.accessToken = gdriveRefreshAccess(accessInfo_.refreshToken); //throw SysError

        assert(accessInfo_.accessToken.validUntil > std::time(nullptr) + std::chrono::seconds(HTTP_SESSION_ACCESS_TIME_OUT).count());
        return accessInfo_.accessToken.value;
    }

    //const std::wstring& getUserDisplayName() const { return accessInfo_.userInfo.displayName; }
    const std::string& getUserEmail() const { return accessInfo_.userInfo.email; }

    void update(const GdriveAccessInfo& accessInfo)
    {
        if (!equalAsciiNoCase(accessInfo.userInfo.email, accessInfo_.userInfo.email))
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));
        accessInfo_ = accessInfo;
    }

private:
    GdriveAccessBuffer           (const GdriveAccessBuffer&) = delete;
    GdriveAccessBuffer& operator=(const GdriveAccessBuffer&) = delete;

    GdriveAccessInfo accessInfo_;
};


class GdrivePersistentSessions;


    class GdriveFileState //per-user-session! => serialize access (perf: amortized fully buffered!)
    {
    public:
        GdriveFileState(GdriveAccessBuffer& accessBuf) : //throw SysError
            /* issue getChangesCurrentToken() as the very first Google Drive query!*/
            lastSyncToken_(getChangesCurrentToken(accessBuf.getAccessToken())), //throw SysError
            myDriveId_    (getMyDriveId          (accessBuf.getAccessToken())), //
            accessBuf_(accessBuf)
        {
            for (const DriveDetails& drive : getSharedDrives(accessBuf.getAccessToken())) //throw SysError
                sharedDrives_.emplace(drive.driveId, drive.driveName);
        }

        GdriveFileState(MemoryStreamIn<std::string>& stream, GdriveAccessBuffer& accessBuf) : //throw SysError
            accessBuf_(accessBuf)
        {
            lastSyncToken_ = readContainer<std::string>(stream); //SysErrorUnexpectedEos
            myDriveId_     = readContainer<std::string>(stream); //

            size_t sharedDrivesCount = readNumber<uint32_t>(stream); //SysErrorUnexpectedEos
            while (sharedDrivesCount-- != 0)
            {
                std::string driveId   = readContainer<std::string>(stream); //SysErrorUnexpectedEos
                std::string driveName = readContainer<std::string>(stream); //
                sharedDrives_.emplace(driveId, utfTo<Zstring>(driveName));
            }

            for (;;)
            {
                const std::string folderId = readContainer<std::string>(stream); //SysErrorUnexpectedEos
                if (folderId.empty())
                    break;
                folderContents_[folderId].isKnownFolder = true;
            }

            for (;;)
            {
                const std::string itemId = readContainer<std::string>(stream); //SysErrorUnexpectedEos
                if (itemId.empty())
                    break;

                GdriveItemDetails details = {};
                details.itemName = utfTo<Zstring>(readContainer<std::string>(stream)); //
                details.type     = readNumber<GdriveItemType>(stream); //
                details.owner    = readNumber     <FileOwner>(stream); //
                details.fileSize = readNumber      <uint64_t>(stream); //SysErrorUnexpectedEos
                details.modTime  = readNumber       <int64_t>(stream); //
                details.targetId = readContainer<std::string>(stream); //

                size_t parentsCount = readNumber<uint32_t>(stream); //SysErrorUnexpectedEos
                while (parentsCount-- != 0)
                    details.parentIds.push_back(readContainer<std::string>(stream)); //SysErrorUnexpectedEos

                updateItemState(itemId, &details);
            }
        }

        void serialize(MemoryStreamOut<std::string>& stream) const
        {
            writeContainer(stream, lastSyncToken_);
            writeContainer(stream, myDriveId_);

            writeNumber(stream, static_cast<uint32_t>(sharedDrives_.size()));
            for (const auto& [driveId, driveName]: sharedDrives_)
            {
                writeContainer(stream, driveId);
                writeContainer(stream, utfTo<std::string>(driveName));
            }

            for (const auto& [folderId, content] : folderContents_)
                if (folderId.empty())
                    throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));
                else if (content.isKnownFolder)
                    writeContainer(stream, folderId);
            writeContainer(stream, std::string()); //sentinel

            auto serializeItem = [&](const std::string& itemId, const GdriveItemDetails& details)
            {
                writeContainer             (stream, itemId);
                writeContainer             (stream, utfTo<std::string>(details.itemName));
                writeNumber<GdriveItemType>(stream, details.type);
                writeNumber     <FileOwner>(stream, details.owner);
                writeNumber      <uint64_t>(stream, details.fileSize);
                writeNumber       <int64_t>(stream, details.modTime);
                static_assert(sizeof(details.modTime) <= sizeof(int64_t)); //ensure cross-platform compatibility!
                writeContainer(stream, details.targetId);

                writeNumber(stream, static_cast<uint32_t>(details.parentIds.size()));
                for (const std::string& parentId : details.parentIds)
                    writeContainer(stream, parentId);
            };

            //serialize + clean up: only save items in "known folders" + items referenced by shortcuts
            for (const auto& [folderId, content] : folderContents_)
                if (content.isKnownFolder)
                    for (const auto itItem : content.childItems)
                    {
                        const auto& [itemId, details] = *itItem;
                        if (itemId.empty())
                            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));
                        serializeItem(itemId, details);

                        if (details.type == GdriveItemType::shortcut)
                        {
                            if (details.targetId.empty())
                                throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));

                            if (auto it = itemDetails_.find(details.targetId);
                                it != itemDetails_.end())
                                serializeItem(details.targetId, it->second);
                        }
                    }
            writeContainer(stream, std::string()); //sentinel
        }

        std::vector<Zstring /*sharedDriveName*/> listSharedDrives() const
        {
            std::vector<Zstring> sharedDriveNames;

            for (const auto& [driveId, driveName]: sharedDrives_)
                sharedDriveNames.push_back(driveName);

            return sharedDriveNames;
        }

        struct PathStatus
        {
            std::string existingItemId;
            GdriveItemType existingType = GdriveItemType::file;
            AfsPath existingPath;         //input path =: existingPath + relPath
            std::vector<Zstring> relPath; //
        };
        PathStatus getPathStatus(const Zstring& sharedDriveName, const AfsPath& afsPath, bool followLeafShortcut) //throw SysError
        {
            const std::string driveId = [&]
            {
                if (sharedDriveName.empty())
                    return myDriveId_;

                auto itFound = sharedDrives_.cend();
                for (auto it = sharedDrives_.begin(); it != sharedDrives_.end(); ++it)
                    if (const auto& [sharedDriveId, driveName] = *it;
                        equalNativePath(driveName, sharedDriveName))
                    {
                        if (itFound != sharedDrives_.end())
                            throw SysError(replaceCpy(_("Cannot find %x."), L"%x",
                            fmtPath(getGdriveDisplayPath({{ accessBuf_.getUserEmail(), sharedDriveName}, AfsPath() }))) + L' ' +
                        replaceCpy(_("The name %x is used by more than one item in the folder."), L"%x", fmtPath(sharedDriveName)));
                        itFound = it;
                    }
                if (itFound == sharedDrives_.end())
                    throw SysError(replaceCpy(_("Cannot find %x."), L"%x",
                    fmtPath(getGdriveDisplayPath({{ accessBuf_.getUserEmail(), sharedDriveName}, AfsPath() }))));

                return itFound->first;
            }();

            const std::vector<Zstring> relPath = split(afsPath.value, FILE_NAME_SEPARATOR, SplitOnEmpty::skip);
            if (relPath.empty())
                return { driveId, GdriveItemType::folder, AfsPath(), {} };
            else
                return getPathStatusSub(driveId, sharedDriveName, AfsPath(), relPath, followLeafShortcut); //throw SysError
        }

        std::string /*itemId*/ getItemId(const Zstring& sharedDriveName, const AfsPath& afsPath, bool followLeafShortcut) //throw SysError
        {
            const GdriveFileState::PathStatus& ps = getPathStatus(sharedDriveName, afsPath, followLeafShortcut); //throw SysError
            if (ps.relPath.empty())
                return ps.existingItemId;

            const AfsPath afsPathMissingChild(nativeAppendPaths(ps.existingPath.value, ps.relPath.front()));
            throw SysError(replaceCpy(_("Cannot find %x."), L"%x", fmtPath(getGdriveDisplayPath({ { accessBuf_.getUserEmail(), sharedDriveName }, afsPathMissingChild }))));
        }

        std::pair<std::string /*itemId*/, GdriveItemDetails> getFileAttributes(const Zstring& sharedDriveName, const AfsPath& afsPath, bool followLeafShortcut) //throw SysError
        {
            const std::string itemId = getItemId(sharedDriveName, afsPath, followLeafShortcut); //throw SysError

            if (afsPath.value.empty()) //root drives obviously not covered by itemDetails_
            {
                GdriveItemDetails rootDetails = {};
                rootDetails.type = GdriveItemType::folder;
                if (itemId == myDriveId_)
                {
                    rootDetails.itemName = Zstr("My Drive");
                    rootDetails.owner = FileOwner::me;
                    return { itemId, std::move(rootDetails) };
                }

                if (auto it = sharedDrives_.find(itemId);
                    it != sharedDrives_.end())
                {
                    rootDetails.itemName = it->second;
                    rootDetails.owner = FileOwner::none;
                    return { itemId, std::move(rootDetails) };
                }
            }
            else if (auto it = itemDetails_.find(itemId);
                     it != itemDetails_.end())
                return *it;

            //itemId was found! => must either be a (shared) drive root or buffered in itemDetails_
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));
        }

        std::optional<GdriveItemDetails> tryGetBufferedItemDetails(const std::string& itemId) const
        {
            if (auto it = itemDetails_.find(itemId);
                it != itemDetails_.end())
                return it->second;
            return {};
        }

        std::optional<std::vector<GdriveItem>> tryGetBufferedFolderContent(const std::string& folderId) const
        {
            auto it = folderContents_.find(folderId);
            if (it == folderContents_.end() || !it->second.isKnownFolder)
                return std::nullopt;

            std::vector<GdriveItem> childItems;
            for (auto itChild : it->second.childItems)
            {
                const auto& [childId, childDetails] = *itChild;
                childItems.push_back({ childId, childDetails });
            }
            return std::move(childItems); //[!] need std::move!
        }

        //-------------- notifications --------------
        using ItemIdDelta = std::unordered_set<std::string>;

        struct FileStateDelta //as long as instance exists, GdriveItem will log all changed items
        {
            FileStateDelta() {}
        private:
            FileStateDelta(const std::shared_ptr<const ItemIdDelta>& cids) : changedIds(cids) {}
            friend class GdriveFileState;
            std::shared_ptr<const ItemIdDelta> changedIds; //lifetime is managed by caller; access *only* by GdriveFileState!
        };

        void notifyFolderContent(const FileStateDelta& stateDelta, const std::string& folderId, const std::vector<GdriveItem>& childItems)
        {
            folderContents_[folderId].isKnownFolder = true;

            for (const GdriveItem& item : childItems)
                notifyItemUpdated(stateDelta, item.itemId, &item.details);

            //- should we remove parent links for items that are not children of folderId anymore (as of this update)?? => fringe case during first update! (still: maybe trigger sync?)
            //- what if there are multiple folder state updates incoming in wrong order!? => notifyItemUpdated() will sort it out!
        }

        void notifyItemCreated(const FileStateDelta& stateDelta, const GdriveItem& item)
        {
            notifyItemUpdated(stateDelta, item.itemId, &item.details);
        }

        void notifyItemUpdated(const FileStateDelta& stateDelta, const GdriveItem& item)
        {
            notifyItemUpdated(stateDelta, item.itemId, &item.details);
        }

        void notifyFolderCreated(const FileStateDelta& stateDelta, const std::string& folderId, const Zstring& folderName, const std::string& parentId)
        {
            GdriveItemDetails details = {};
            details.itemName = folderName;
            details.type = GdriveItemType::folder;
            details.owner = FileOwner::me;
            details.modTime = std::time(nullptr);
            details.parentIds.push_back(parentId);

            //avoid needless conflicts due to different Google Drive folder modTime!
            if (auto it = itemDetails_.find(folderId); it != itemDetails_.end())
                details.modTime = it->second.modTime;

            notifyItemUpdated(stateDelta, folderId, &details);
        }

        void notifyShortcutCreated(const FileStateDelta& stateDelta, const std::string& shortcutId, const Zstring& shortcutName, const std::string& parentId, const std::string& targetId)
        {
            GdriveItemDetails details = {};
            details.itemName = shortcutName;
            details.type = GdriveItemType::shortcut;
            details.owner = FileOwner::me;
            details.modTime = std::time(nullptr);
            details.targetId = targetId;
            details.parentIds.push_back(parentId);

            //avoid needless conflicts due to different Google Drive folder modTime!
            if (auto it = itemDetails_.find(shortcutId); it != itemDetails_.end())
                details.modTime = it->second.modTime;

            notifyItemUpdated(stateDelta, shortcutId, &details);
        }


        void notifyItemDeleted(const FileStateDelta& stateDelta, const std::string& itemId)
        {
            notifyItemUpdated(stateDelta, itemId, nullptr);
        }

        void notifyParentRemoved(const FileStateDelta& stateDelta, const std::string& itemId, const std::string& parentIdOld)
        {
            if (auto it = itemDetails_.find(itemId); it != itemDetails_.end())
            {
                GdriveItemDetails detailsNew = it->second;
                std::erase_if(detailsNew.parentIds, [&](const std::string& id) { return id == parentIdOld; });
                notifyItemUpdated(stateDelta, itemId, &detailsNew);
            }
            else //conflict!!!
                markSyncDue();
        }

        void notifyMoveAndRename(const FileStateDelta& stateDelta, const std::string& itemId, const std::string& parentIdFrom, const std::string& parentIdTo, const Zstring& newName)
        {
            if (auto it = itemDetails_.find(itemId); it != itemDetails_.end())
            {
                GdriveItemDetails detailsNew = it->second;
                detailsNew.itemName = newName;

                std::erase_if(detailsNew.parentIds, [&](const std::string& id) { return id == parentIdFrom || id == parentIdTo; }); //
                detailsNew.parentIds.push_back(parentIdTo); //not a duplicate

                notifyItemUpdated(stateDelta, itemId, &detailsNew);
            }
            else //conflict!!!
                markSyncDue();
        }

    private:
        GdriveFileState           (const GdriveFileState&) = delete;
        GdriveFileState& operator=(const GdriveFileState&) = delete;

        friend class GdrivePersistentSessions;

        void notifyItemUpdated(const FileStateDelta& stateDelta, const std::string& itemId, const GdriveItemDetails* details)
        {
            if (!contains(*stateDelta.changedIds, itemId)) //no conflicting changes in the meantime?
                updateItemState(itemId, details);          //=> accept new state data
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

        bool syncIsDue() const { return std::chrono::steady_clock::now() >= lastSyncTime_ + GDRIVE_SYNC_INTERVAL; }

        void markSyncDue() { lastSyncTime_ = std::chrono::steady_clock::now() - GDRIVE_SYNC_INTERVAL; }

        void syncWithGoogle() //throw SysError
        {
            const ChangesDelta delta = getChangesDelta(lastSyncToken_, accessBuf_.getAccessToken()); //throw SysError

            for (const FileChange& change : delta.fileChanges)
                updateItemState(change.itemId, get(change.details));

            for (const DriveChange& change : delta.driveChanges)
                updateSharedDriveState(change.driveId, change.driveName);

            lastSyncToken_ = delta.newStartPageToken;
            lastSyncTime_ = std::chrono::steady_clock::now();

            //good to know: if item is created and deleted between polling for changes it is still reported as deleted by Google!
            //Same goes for any other change that is undone in between change notification syncs.
        }

        PathStatus getPathStatusSub(const std::string& folderId, const Zstring& sharedDriveName, const AfsPath& folderPath, const std::vector<Zstring>& relPath, bool followLeafShortcut) //throw SysError
        {
            assert(!relPath.empty());

            auto itKnown = folderContents_.find(folderId);
            if (itKnown == folderContents_.end() || !itKnown->second.isKnownFolder)
            {
                notifyFolderContent(registerFileStateDelta(), folderId, readFolderContent(folderId, accessBuf_.getAccessToken())); //throw SysError
                //perf: always buffered, except for direct, first-time folder access!
                itKnown = folderContents_.find(folderId);
                assert(itKnown != folderContents_.end());
                if (!itKnown->second.isKnownFolder)
                    throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));
            }

            auto itFound = itemDetails_.cend();
            for (const DetailsIterator& itChild : itKnown->second.childItems)
                //Since Google Drive has no concept of a file path, we have to roll our own "path to id" mapping => let's use the platform-native style
                if (equalNativePath(itChild->second.itemName, relPath.front()))
                {
                    if (itFound != itemDetails_.end())
                        throw SysError(replaceCpy(_("Cannot find %x."), L"%x",
                        fmtPath(getGdriveDisplayPath({{ accessBuf_.getUserEmail(), sharedDriveName}, AfsPath(nativeAppendPaths(folderPath.value, relPath.front())) }))) + L' ' +
                    replaceCpy(_("The name %x is used by more than one item in the folder."), L"%x", fmtPath(relPath.front())));

                    itFound = itChild;
                }

            if (itFound == itemDetails_.end())
                return { folderId, GdriveItemType::folder, folderPath, relPath }; //always a folder, see check before recursion above
            else
            {
                auto getItemDetailsBuffered = [&](const std::string& itemId) -> const GdriveItemDetails&
                {
                    auto it = itemDetails_.find(itemId);
                    if (it == itemDetails_.end())
                    {
                        notifyItemUpdated(registerFileStateDelta(), { itemId, getItemDetails(itemId, accessBuf_.getAccessToken()) }); //throw SysError
                        //perf: always buffered, except for direct, first-time folder access!
                        it = itemDetails_.find(itemId);
                        assert(it != itemDetails_.end());
                    }
                    return it->second;
                };

                const auto& [childId, childDetails] = *itFound;
                const AfsPath              childItemPath(nativeAppendPaths(folderPath.value, relPath.front()));
                const std::vector<Zstring> childRelPath(relPath.begin() + 1, relPath.end());

                if (childRelPath.empty())
                {
                    if (childDetails.type == GdriveItemType::shortcut && followLeafShortcut)
                        return { childDetails.targetId, getItemDetailsBuffered(childDetails.targetId).type, childItemPath, childRelPath };
                    else
                        return { childId, childDetails.type, childItemPath, childRelPath };
                }

                switch (childDetails.type)
                {
                    case GdriveItemType::file: //parent/file/child-rel-path... => obscure, but possible (and not an error)
                        return { childId, childDetails.type, childItemPath, childRelPath };

                    case GdriveItemType::folder:
                        return getPathStatusSub(childId, sharedDriveName, childItemPath, childRelPath, followLeafShortcut); //throw SysError

                    case GdriveItemType::shortcut:
                        switch (getItemDetailsBuffered(childDetails.targetId).type)
                        {
                            case GdriveItemType::file: //parent/file-symlink/child-rel-path... => obscure, but possible (and not an error)
                                return { childDetails.targetId, GdriveItemType::file, childItemPath, childRelPath }; //resolve symlinks if in the *middle* of a path!

                            case GdriveItemType::folder: //parent/folder-symlink/child-rel-path... => always follow
                                return getPathStatusSub(childDetails.targetId, sharedDriveName, childItemPath, childRelPath, followLeafShortcut); //throw SysError

                            case GdriveItemType::shortcut: //should never happen: creating shortcuts to shortcuts fails with "Internal Error"
                                throw SysError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x",
                                fmtPath(getGdriveDisplayPath({{ accessBuf_.getUserEmail(), sharedDriveName}, AfsPath(nativeAppendPaths(folderPath.value, relPath.front())) }))) + L' ' +
                                L"Google Drive Shortcut points to another Shortcut.");
                        }
                        break;
                }
                throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));
            }
        }

        void updateItemState(const std::string& itemId, const GdriveItemDetails* details)
        {
            auto it = itemDetails_.find(itemId);
            if (!details == (it == itemDetails_.end()))
                if (!details || *details == it->second) //notified changes match our current file state
                    return; //=> avoid misleading changeLog_ entries after Google Drive sync!!!

            //update change logs (and clean up obsolete entries)
            std::erase_if(changeLog_, [&](std::weak_ptr<ItemIdDelta>& weakPtr)
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
                    if (it->second.type != details->type)
                        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__)); //WTF!?

                    std::vector<std::string> parentIdsNew     = details->parentIds;
                    std::vector<std::string> parentIdsRemoved = it->second.parentIds;
                    std::erase_if(parentIdsNew,     [&](const std::string& id) { return std::find(it->second.parentIds.begin(), it->second.parentIds.end(), id) != it->second.parentIds.end(); });
                    std::erase_if(parentIdsRemoved, [&](const std::string& id) { return std::find(details->parentIds.begin(), details->parentIds.end(), id) != details->parentIds.end(); });

                    for (const std::string& parentId : parentIdsNew)
                        folderContents_[parentId].childItems.push_back(it); //new insert => no need for duplicate check

                    for (const std::string& parentId : parentIdsRemoved)
                        if (auto itP = folderContents_.find(parentId); itP != folderContents_.end())
                            std::erase_if(itP->second.childItems, [&](auto itChild) { return itChild == it; });
                    //if all parents are removed, Google Drive will (recursively) delete the item => don't prematurely do this now: wait for change notifications!
                    //OR: item without parents located in "Shared with me", but referenced via Shortcut => don't remove!!!

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
                        if (auto itP = folderContents_.find(parentId); itP != folderContents_.end())
                            std::erase_if(itP->second.childItems, [&](auto itChild) { return itChild == it; });

                    itemDetails_.erase(it);
                }

                if (auto itP = folderContents_.find(itemId);
                    itP != folderContents_.end())
                {
                    for (auto itChild : itP->second.childItems) //2. delete as parent from child items (don't wait for change notifications of children)
                        std::erase_if(itChild->second.parentIds, [&](const std::string& id) { return id == itemId; });
                    folderContents_.erase(itP);
                }
            }
        }

        void updateSharedDriveState(const std::string& driveId, const Zstring& driveName /*empty if shared drive was deleted*/)
        {
            if (!driveName.empty())
                sharedDrives_[driveId] = driveName;
            else //delete
            {
                sharedDrives_.erase(driveId);

                //when a shared drive is deleted, we also receive change notifications for the contained files: nice!
                if (auto itP = folderContents_.find(driveId);
                    itP != folderContents_.end())
                {
                    for (auto itChild : itP->second.childItems) //delete as parent from child items (don't wait for change notifications of children)
                        std::erase_if(itChild->second.parentIds, [&](const std::string& id) { return id == driveId; });
                    folderContents_.erase(itP);
                }
            }
        }

        using DetailsIterator = std::unordered_map<std::string, GdriveItemDetails>::iterator;

        struct FolderContent
        {
            bool isKnownFolder = false; //=we've seen its full content at least once; further changes are calculated via change notifications!
            std::vector<DetailsIterator> childItems;
        };
        std::unordered_map<std::string /*folderId*/, FolderContent> folderContents_;
        std::unordered_map<std::string /*itemId*/, GdriveItemDetails> itemDetails_; //contains ALL known, existing items!

        std::string lastSyncToken_; //marker corresponding to last sync with Google's change notifications
        std::chrono::steady_clock::time_point lastSyncTime_ = std::chrono::steady_clock::now() - GDRIVE_SYNC_INTERVAL; //... with Google Drive (default: sync is due)

        std::vector<std::weak_ptr<ItemIdDelta>> changeLog_; //track changed items since FileStateDelta was created (includes sync with Google + our own intermediate change notifications)

        std::string myDriveId_;
        std::unordered_map<std::string /*driveId*/, Zstring /*driveName*/> sharedDrives_;
        GdriveAccessBuffer& accessBuf_;
    };

//==========================================================================================
//==========================================================================================

class GdrivePersistentSessions
{
public:
    GdrivePersistentSessions(const Zstring& configDirPath) : configDirPath_(configDirPath) {}

    void saveActiveSessions() //throw FileError
    {
        std::vector<Protected<SessionHolder>*> protectedSessions; //pointers remain stable, thanks to std::map<>
        globalSessions_.access([&](GlobalSessions& sessions)
        {
            for (auto& [accountEmail, protectedSession] : sessions)
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
                        saveSession(dbFilePath, *holder.session); //throw FileError
                    }
                    catch (FileError&) { if (!firstError) firstError = std::current_exception(); }
            });

            if (firstError)
                std::rethrow_exception(firstError); //throw FileError
        }
    }

    std::string addUserSession(const std::string& gdriveLoginHint, const std::function<void()>& updateGui /*throw X*/) //throw SysError, X
    {
        const GdriveAccessInfo accessInfo = gdriveAuthorizeAccess(gdriveLoginHint, updateGui); //throw SysError, X

        accessUserSession(accessInfo.userInfo.email, [&](std::optional<UserSession>& userSession) //throw SysError
        {
            if (userSession)
                userSession->accessBuf.ref().update(accessInfo); //redundant?
            else
            {
                auto accessBuf = makeSharedRef<GdriveAccessBuffer>(accessInfo);
                auto fileState = makeSharedRef<GdriveFileState   >(accessBuf.ref()); //throw SysError
                userSession = { accessBuf, fileState };
            }
        });

        return accessInfo.userInfo.email;
    }

    void removeUserSession(const std::string& accountEmail) //throw SysError
    {
        try
        {
            accessUserSession(accountEmail, [&](std::optional<UserSession>& userSession) //throw SysError
            {
                if (userSession)
                    gdriveRevokeAccess(userSession->accessBuf.ref().getAccessToken()); //throw SysError
            });
        }
        catch (SysError&) { assert(false); } //best effort: try to invalidate the access token
        //=> expected to fail if offline => not worse than removing FFS via "Uninstall Programs"

        try
        {
            //start with deleting the DB file (1. maybe it's corrupted? 2. skip unnecessary lazy-load)
            const Zstring dbFilePath = getDbFilePath(accountEmail);
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
        catch (const FileError& e) { throw SysError(replaceCpy(e.toString(), L"\n\n", L'\n')); } //file access errors should be further enriched by context info => SysError


        accessUserSession(accountEmail, [&](std::optional<UserSession>& userSession) //throw SysError
        {
            userSession.reset();
        });
    }

    std::vector<std::string /*account email*/> listAccounts() //throw SysError
    {
        std::vector<std::string> emails;

        std::vector<Protected<SessionHolder>*> protectedSessions; //pointers remain stable, thanks to std::map<>
        globalSessions_.access([&](GlobalSessions& sessions)
        {
            for (auto& [accountEmail, protectedSession] : sessions)
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
        [&](const    FileInfo& fi) { if (endsWith(fi.itemName, Zstr(".db"))) emails.push_back(utfTo<std::string>(beforeLast(fi.itemName, Zstr('.'), IfNotFoundReturn::none))); },
        [&](const  FolderInfo& fi) {},
        [&](const SymlinkInfo& si) {},
        [&](const std::wstring& errorMsg)
        {
            try
            {
                if (itemStillExists(configDirPath_)) //throw FileError
                    throw FileError(errorMsg);
            }
            catch (const FileError& e) { throw SysError(replaceCpy(e.toString(), L"\n\n", L'\n')); } //file access errors should be further enriched by context info => SysError
        });

        removeDuplicates(emails, LessAsciiNoCase());
        return emails;
    }

    struct AsyncAccessInfo
    {
        std::string accessToken; //don't allow (long-running) web requests while holding the global session lock!
        GdriveFileState::FileStateDelta stateDelta;
    };
    //perf: amortized fully buffered!
    AsyncAccessInfo accessGlobalFileState(const std::string& accountEmail, const std::function<void(GdriveFileState& fileState)>& useFileState /*throw X*/) //throw SysError, X
    {
        std::string accessToken;
        GdriveFileState::FileStateDelta stateDelta;

        accessUserSession(accountEmail, [&](std::optional<UserSession>& userSession) //throw SysError
        {
            if (!userSession)
                throw SysError(replaceCpy(_("Please authorize access to user account %x."), L"%x", utfTo<std::wstring>(accountEmail)));

            //manage last sync time here rather than in GdriveFileState, so that "lastSyncToken" remains stable while accessing GdriveFileState in the callback
            if (userSession->fileState.ref().syncIsDue())
                userSession->fileState.ref().syncWithGoogle(); //throw SysError

            accessToken = userSession->accessBuf.ref().getAccessToken(); //throw SysError
            stateDelta  = userSession->fileState.ref().registerFileStateDelta();

            useFileState(userSession->fileState.ref()); //throw X
        });
        return { accessToken, stateDelta };
    }

private:
    GdrivePersistentSessions           (const GdrivePersistentSessions&) = delete;
    GdrivePersistentSessions& operator=(const GdrivePersistentSessions&) = delete;

    struct UserSession;

    Zstring getDbFilePath(std::string accountEmail) const
    {
        for (char& c : accountEmail)
            c = asciiToLower(c);
        //return appendSeparator(configDirPath_) + utfTo<Zstring>(formatAsHexString(getMd5(utfTo<std::string>(accountEmail)))) + Zstr(".db");
        return appendSeparator(configDirPath_) + utfTo<Zstring>(accountEmail) + Zstr(".db");
    }

    void accessUserSession(const std::string& accountEmail, const std::function<void(std::optional<UserSession>& userSession)>& useSession /*throw X*/) //throw SysError, X
    {
        Protected<SessionHolder>* protectedSession = nullptr; //pointers remain stable, thanks to std::map<>
        globalSessions_.access([&](GlobalSessions& sessions) { protectedSession = &sessions[accountEmail]; });

        protectedSession->access([&](SessionHolder& holder)
        {
            if (!holder.dbWasLoaded) //let's NOT load the DB files under the globalSessions_ lock, but the session-specific one!
                try
                {
                    holder.session = loadSession(getDbFilePath(accountEmail)); //throw SysError
                }
                catch (const FileError& e) { throw SysError(replaceCpy(e.toString(), L"\n\n", L'\n')); } //GdrivePersistentSessions errors should be further enriched with context info => SysError
            holder.dbWasLoaded = true;
            useSession(holder.session); //throw X
        });
    }

    static void saveSession(const Zstring& dbFilePath, const UserSession& userSession) //throw FileError
    {
        MemoryStreamOut<std::string> streamOut;
        writeArray(streamOut, DB_FILE_DESCR, sizeof(DB_FILE_DESCR));
        writeNumber<int32_t>(streamOut, DB_FILE_VERSION);

        MemoryStreamOut<std::string> streamOutBody;
        userSession.accessBuf.ref().serialize(streamOutBody);
        userSession.fileState.ref().serialize(streamOutBody);

        try
        {
            streamOut.ref() += compress(streamOutBody.ref(), 3 /*best compression level: see db_file.cpp*/); //throw SysError
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(dbFilePath)), e.toString()); }

        setFileContent(dbFilePath, streamOut.ref(), nullptr /*notifyUnbufferedIO*/); //throw FileError
    }

    static std::optional<UserSession> loadSession(const Zstring& dbFilePath) //throw FileError
    {
        std::string byteStream;
        try
        {
            byteStream = getFileContent(dbFilePath, nullptr /*notifyUnbufferedIO*/); //throw FileError
        }
        catch (FileError&)
        {
            if (itemStillExists(dbFilePath)) //throw FileError
                throw;

            return std::nullopt;
        }

        try
        {
            MemoryStreamIn streamIn(byteStream);
            //-------- file format header --------
            char tmp[sizeof(DB_FILE_DESCR)] = {};
            readArray(streamIn, &tmp, sizeof(tmp)); //throw SysErrorUnexpectedEos

            //TODO: remove migration code at some time! 2020-07-03
            if (!std::equal(std::begin(tmp), std::end(tmp), std::begin(DB_FILE_DESCR)))
            {
                MemoryStreamIn streamIn2(decompress(byteStream)); //throw SysError
                //-------- file format header --------
                const char DB_FILE_DESCR_OLD[] = "FreeFileSync: Google Drive Database";
                char tmp2[sizeof(DB_FILE_DESCR_OLD)] = {};
                readArray(streamIn2, &tmp2, sizeof(tmp2)); //throw SysErrorUnexpectedEos

                if (!std::equal(std::begin(tmp2), std::end(tmp2), std::begin(DB_FILE_DESCR_OLD)))
                    throw SysError(_("File content is corrupted.") + L" (invalid header)");

                const int version = readNumber<int32_t>(streamIn2);
                if (version != 1 && //TODO: remove migration code at some time! 2019-12-05
                    version != 2 && //TODO: remove migration code at some time! 2020-06-11
                    version != 3)   //TODO: remove migration code at some time! 2020-07-03
                    throw SysError(_("Unsupported data format.") + L' ' + replaceCpy(_("Version: %x"), L"%x", numberTo<std::wstring>(version)));

                auto accessBuf = makeSharedRef<GdriveAccessBuffer>(streamIn2); //throw SysError
                auto fileState =
                    //TODO: remove migration code at some time! 2020-06-11
                    version <= 2 ? //fully discard old state due to missing "ownedByMe" attribute + shortcut support
                    makeSharedRef<GdriveFileState>(           accessBuf.ref()) : //throw SysError
                    makeSharedRef<GdriveFileState>(streamIn2, accessBuf.ref());  //

                return UserSession{ accessBuf, fileState };
            }
            else
            {
                if (!std::equal(std::begin(tmp), std::end(tmp), std::begin(DB_FILE_DESCR)))
                    throw SysError(_("File content is corrupted.") + L" (invalid header)");

                const int version = readNumber<int32_t>(streamIn);
                if (version != DB_FILE_VERSION)
                    throw SysError(_("Unsupported data format.") + L' ' + replaceCpy(_("Version: %x"), L"%x", numberTo<std::wstring>(version)));

                MemoryStreamIn streamInBody(decompress(std::string(byteStream.begin() + streamIn.pos(), byteStream.end()))); //throw SysError

                auto accessBuf = makeSharedRef<GdriveAccessBuffer>(streamInBody); //throw SysError
                auto fileState = makeSharedRef<GdriveFileState   >(streamInBody, accessBuf.ref()); //throw SysError

                return UserSession{ accessBuf, fileState };
            }
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot read database file %x."), L"%x", fmtPath(dbFilePath)), e.toString());
        }
    }

    struct UserSession
    {
        SharedRef<GdriveAccessBuffer> accessBuf;
        SharedRef<GdriveFileState>    fileState;
    };

    struct SessionHolder
    {
        bool dbWasLoaded = false;
        std::optional<UserSession> session;
    };
    using GlobalSessions = std::map<std::string /*Google account email*/, Protected<SessionHolder>, LessAsciiNoCase>;

    Protected<GlobalSessions> globalSessions_;
    const Zstring configDirPath_;
};
//==========================================================================================
constinit2 Global<GdrivePersistentSessions> globalGdriveSessions;
//==========================================================================================

GdrivePersistentSessions::AsyncAccessInfo accessGlobalFileState(const std::string& accountEmail, const std::function<void(GdriveFileState& fileState)>& useFileState /*throw X*/) //throw SysError, X
{
    if (const std::shared_ptr<GdrivePersistentSessions> gps = globalGdriveSessions.get())
        return gps->accessGlobalFileState(accountEmail, useFileState); //throw SysError, X

    throw SysError(formatSystemError("accessGlobalFileState", L"", L"Function call not allowed during init/shutdown."));
}

//==========================================================================================
//==========================================================================================

struct GetDirDetails
{
    GetDirDetails(const GdrivePath& folderPath) : folderPath_(folderPath) {}

    struct Result
    {
        std::vector<GdriveItem> childItems;
        GdrivePath folderPath;
    };
    Result operator()() const
    {
        try
        {
            std::string folderId;
            std::optional<std::vector<GdriveItem>> childItemsBuf;
            const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(folderPath_.gdriveLogin.email, [&](GdriveFileState& fileState) //throw SysError
            {
                const auto& [itemId, itemDetails] = fileState.getFileAttributes(folderPath_.gdriveLogin.sharedDriveName, folderPath_.itemPath, true /*followLeafShortcut*/); //throw SysError

                if (itemDetails.type != GdriveItemType::folder) //check(!) or readFolderContent() will return empty (without failing!)
                    throw SysError(replaceCpy<std::wstring>(L"%x is not a directory.", L"%x", fmtPath(utfTo<Zstring>(itemDetails.itemName))));

                folderId      = itemId;
                childItemsBuf = fileState.tryGetBufferedFolderContent(folderId);
            });

            if (!childItemsBuf)
            {
                childItemsBuf = readFolderContent(folderId, aai.accessToken); //throw SysError

                //buffer new file state ASAP => make sure accessGlobalFileState() has amortized constant access (despite the occasional internal readFolderContent() on non-leaf folders)
                accessGlobalFileState(folderPath_.gdriveLogin.email, [&](GdriveFileState& fileState) //throw SysError
                {
                    fileState.notifyFolderContent(aai.stateDelta, folderId, *childItemsBuf);
                });
            }

            for (const GdriveItem& item : *childItemsBuf)
                if (item.details.itemName.empty())
                    throw SysError(L"Folder contains child item without a name."); //mostly an issue for FFS's folder traversal, but NOT for globalGdriveSessions!

            return { std::move(*childItemsBuf), folderPath_ };
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(getGdriveDisplayPath(folderPath_))), e.toString()); }
    }

private:
    GdrivePath folderPath_;
};


struct GetShortcutTargetDetails
{
    GetShortcutTargetDetails(const GdrivePath& shortcutPath, const GdriveItemDetails& shortcutDetails) : shortcutPath_(shortcutPath), shortcutDetails_(shortcutDetails) {}

    struct Result
    {
        GdriveItemDetails target;
        GdriveItemDetails shortcut;
        GdrivePath shortcutPath;
    };
    Result operator()() const
    {
        try
        {
            std::optional<GdriveItemDetails> targetDetailsBuf;
            const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(shortcutPath_.gdriveLogin.email, [&](GdriveFileState& fileState) //throw SysError
            {
                targetDetailsBuf = fileState.tryGetBufferedItemDetails(shortcutDetails_.targetId);
            });
            if (!targetDetailsBuf)
            {
                targetDetailsBuf = getItemDetails(shortcutDetails_.targetId, aai.accessToken); //throw SysError

                //buffer new file state ASAP
                accessGlobalFileState(shortcutPath_.gdriveLogin.email, [&](GdriveFileState& fileState) //throw SysError
                {
                    fileState.notifyItemUpdated(aai.stateDelta, { shortcutDetails_.targetId, *targetDetailsBuf });
                });
            }

            if (targetDetailsBuf->type == GdriveItemType::shortcut) //should never happen: creating shortcuts to shortcuts fails with "Internal Error"
                throw SysError(L"Google Drive Shortcut points to another Shortcut.");

            return { std::move(*targetDetailsBuf), shortcutDetails_, shortcutPath_ };
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(getGdriveDisplayPath(shortcutPath_))), e.toString()); }
    }

private:
    GdrivePath shortcutPath_;
    GdriveItemDetails shortcutDetails_;
};


class SingleFolderTraverser
{
public:
    SingleFolderTraverser(const GdriveLogin& gdriveLogin, const std::vector<std::pair<AfsPath, std::shared_ptr<AFS::TraverserCallback>>>& workload /*throw X*/) :
        gdriveLogin_(gdriveLogin), workload_(workload)
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
        const std::vector<GdriveItem>& childItems = GetDirDetails({ gdriveLogin_, folderPath })().childItems; //throw FileError

        for (const GdriveItem& item : childItems)
        {
            const Zstring itemName = utfTo<Zstring>(item.details.itemName);

            switch (item.details.type)
            {
                case GdriveItemType::file:
                    cb.onFile({ itemName, item.details.fileSize, item.details.modTime, item.itemId, false /*isFollowedSymlink*/ }); //throw X
                    break;

                case GdriveItemType::folder:
                    if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb.onFolder({ itemName, false /*isFollowedSymlink*/ })) //throw X
                    {
                        const AfsPath afsItemPath(nativeAppendPaths(folderPath.value, itemName));
                        workload_.push_back({ afsItemPath, std::move(cbSub) });
                    }
                    break;

                case GdriveItemType::shortcut:
                    switch (cb.onSymlink({ itemName, item.details.modTime })) //throw X
                    {
                        case AFS::TraverserCallback::LINK_FOLLOW:
                        {
                            const AfsPath afsItemPath(nativeAppendPaths(folderPath.value, itemName));

                            GdriveItemDetails targetDetails = {};
                            if (!tryReportingItemError([&] //throw X
                        {
                            targetDetails = GetShortcutTargetDetails({ gdriveLogin_, afsItemPath }, item.details)().target; //throw FileError
                            }, cb, itemName))
                            continue;

                            if (targetDetails.type == GdriveItemType::folder)
                            {
                                if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb.onFolder({ itemName, true /*isFollowedSymlink*/ })) //throw X
                                    workload_.push_back({ afsItemPath, std::move(cbSub) });
                            }
                            else //a file or named pipe, etc.
                                cb.onFile({ itemName, targetDetails.fileSize, targetDetails.modTime, item.details.targetId, true /*isFollowedSymlink*/ }); //throw X
                        }
                        break;

                        case AFS::TraverserCallback::LINK_SKIP:
                            break;
                    }
                    break;
            }
        }
    }

    const GdriveLogin gdriveLogin_;
    std::vector<std::pair<AfsPath, std::shared_ptr<AFS::TraverserCallback>>> workload_;
};


void gdriveTraverseFolderRecursive(const GdriveLogin& gdriveLogin, const std::vector<std::pair<AfsPath, std::shared_ptr<AFS::TraverserCallback>>>& workload /*throw X*/, size_t) //throw X
{
    SingleFolderTraverser dummy(gdriveLogin, workload); //throw X
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
            setCurrentThreadName(Zstr("Istream[Gdrive] ") + utfTo<Zstring>(getGdriveDisplayPath(gdrivePath)));
            try
            {
                std::string accessToken;
                std::string fileId;
                try
                {
                    accessToken = accessGlobalFileState(gdrivePath.gdriveLogin.email, [&](GdriveFileState& fileState) //throw SysError
                    {
                        fileId = fileState.getItemId(gdrivePath.gdriveLogin.sharedDriveName, gdrivePath.itemPath, true /*followLeafShortcut*/); //throw SysError
                    }).accessToken;
                }
                catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(getGdriveDisplayPath(gdrivePath))), e.toString()); }

                try
                {
                    auto writeBlock = [&](const void* buffer, size_t bytesToWrite)
                    {
                        return asyncStreamOut->write(buffer, bytesToWrite); //throw ThreadStopRequest
                    };

                    gdriveDownloadFile(fileId, writeBlock, accessToken); //throw SysError, ThreadStopRequest
                }
                catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getGdriveDisplayPath(gdrivePath))), e.toString()); }

                asyncStreamOut->closeStream();
            }
            catch (FileError&) { asyncStreamOut->setWriteError(std::current_exception()); } //let ThreadStopRequest pass through!
        });
    }

    ~InputStreamGdrive()
    {
        asyncStreamIn_->setReadError(std::make_exception_ptr(ThreadStopRequest()));
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
            accessGlobalFileState(gdrivePath_.gdriveLogin.email, [&](GdriveFileState& fileState) //throw SysError
            {
                const auto& [itemId, itemDetails] = fileState.getFileAttributes(gdrivePath_.gdriveLogin.sharedDriveName, gdrivePath_.itemPath, true /*followLeafShortcut*/); //throw SysError
                attr.modTime  = itemDetails.modTime;
                attr.fileSize = itemDetails.fileSize;
                attr.fileId   = itemId;
            });
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getGdriveDisplayPath(gdrivePath_))), e.toString()); }
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

//already existing: 1. fails or 2. creates duplicate
struct OutputStreamGdrive : public AbstractFileSystem::OutputStreamImpl
{
    OutputStreamGdrive(const GdrivePath& gdrivePath, //throw SysError
                       std::optional<uint64_t> /*streamSize*/,
                       std::optional<time_t> modTime,
                       const IOCallback& notifyUnbufferedIO /*throw X*/,
                       std::unique_ptr<PathAccessLock>&& pal) :
        notifyUnbufferedIO_(notifyUnbufferedIO)
    {
        std::promise<AFS::FileId> pFileId;
        futFileId_ = pFileId.get_future();

        //CAVEAT: if file is already existing, OutputStreamGdrive *constructor* must fail, not OutputStreamGdrive::write(),
        //        otherwise ~OutputStreamImpl() will delete the already existing file! => don't check asynchronously!
        const Zstring fileName = AFS::getItemName(gdrivePath.itemPath);
        std::string parentId;
        /*const*/ GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdrivePath.gdriveLogin.email, [&](GdriveFileState& fileState) //throw SysError
        {
            const GdriveFileState::PathStatus& ps = fileState.getPathStatus(gdrivePath.gdriveLogin.sharedDriveName, gdrivePath.itemPath, false /*followLeafShortcut*/); //throw SysError
            if (ps.relPath.empty())
                throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(fileName)));

            if (ps.relPath.size() > 1) //parent folder missing
                throw SysError(replaceCpy(_("Cannot find %x."), L"%x",
                                          fmtPath(getGdriveDisplayPath({ gdrivePath.gdriveLogin, AfsPath(nativeAppendPaths(ps.existingPath.value, ps.relPath.front()))}))));
            parentId = ps.existingItemId;
        });

        worker_ = InterruptibleThread([gdrivePath, modTime, fileName, asyncStreamIn = this->asyncStreamOut_,
                                                   pFileId  = std::move(pFileId),
                                                   parentId = std::move(parentId),
                                                   aai      = std::move(aai),
                                                   pal      = std::move(pal)]() mutable
        {
            assert(pal); //bind life time to worker thread!
            setCurrentThreadName(Zstr("Ostream[Gdrive] ") + utfTo<Zstring>(getGdriveDisplayPath(gdrivePath)));
            try
            {
                auto readBlock = [&](void* buffer, size_t bytesToRead)
                {
                    //returns "bytesToRead" bytes unless end of stream! => maps nicely into Posix read() semantics expected by gdriveUploadFile()
                    return asyncStreamIn->read(buffer, bytesToRead); //throw ThreadStopRequest
                };
                //for whatever reason, gdriveUploadFile() is slightly faster than gdriveUploadSmallFile()! despite its two roundtrips! even when file sizes are 0!
                //=> 1. issue likely on Google's side => 2. persists even after having fixed "Expect: 100-continue"
                const std::string fileIdNew = //streamSize && *streamSize < 5 * 1024 * 1024 ?
                    //gdriveUploadSmallFile(fileName, parentId, *streamSize, modTime, readBlock, aai.accessToken) : //throw SysError, ThreadStopRequest
                    gdriveUploadFile       (fileName, parentId,              modTime, readBlock, aai.accessToken);  //throw SysError, ThreadStopRequest
                assert(asyncStreamIn->getTotalBytesRead() == asyncStreamIn->getTotalBytesWritten());
                //already existing: creates duplicate

                //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
                GdriveItem newFileItem = {};
                newFileItem.itemId = fileIdNew;
                newFileItem.details.itemName = fileName;
                newFileItem.details.type = GdriveItemType::file;
                newFileItem.details.owner = FileOwner::me;
                newFileItem.details.fileSize = asyncStreamIn->getTotalBytesRead();
                if (modTime) //else: whatever modTime Google Drive selects will be notified after GDRIVE_SYNC_INTERVAL
                    newFileItem.details.modTime = *modTime;
                newFileItem.details.parentIds.push_back(parentId);

                accessGlobalFileState(gdrivePath.gdriveLogin.email, [&](GdriveFileState& fileState) //throw SysError
                {
                    fileState.notifyItemCreated(aai.stateDelta, newFileItem);
                });

                pFileId.set_value(fileIdNew);
            }
            catch (const SysError& e)
            {
                FileError fe(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getGdriveDisplayPath(gdrivePath))), e.toString());
                const std::exception_ptr exptr = std::make_exception_ptr(std::move(fe));
                asyncStreamIn->setReadError(exptr); //set both!
                pFileId.set_exception(exptr);       //
            }
            //let ThreadStopRequest pass through!
        });
    }

    ~OutputStreamGdrive()
    {
        asyncStreamOut_->setWriteError(std::make_exception_ptr(ThreadStopRequest()));
    }

    void write(const void* buffer, size_t bytesToWrite) override //throw FileError, X
    {
        asyncStreamOut_->write(buffer, bytesToWrite); //throw FileError
        reportBytesProcessed(); //throw X
    }

    AFS::FinalizeResult finalize() override //throw FileError, X
    {
        asyncStreamOut_->closeStream();

        while (futFileId_.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout)
            reportBytesProcessed(); //throw X
        reportBytesProcessed(); //[!] once more, now that *all* bytes were written

        asyncStreamOut_->checkReadErrors(); //throw FileError
        //--------------------------------------------------------------------

        AFS::FinalizeResult result;
        assert(isReady(futFileId_));
        result.fileId = futFileId_.get(); //throw FileError
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

    const IOCallback notifyUnbufferedIO_; //throw X
    int64_t totalBytesReported_ = 0;
    std::shared_ptr<AsyncStreamBuffer> asyncStreamOut_ = std::make_shared<AsyncStreamBuffer>(GDRIVE_STREAM_BUFFER_SIZE);
    InterruptibleThread worker_;
    std::future<AFS::FileId> futFileId_;
};

//==========================================================================================

class GdriveFileSystem : public AbstractFileSystem
{
public:
    explicit GdriveFileSystem(const GdriveLogin& gdriveLogin) : gdriveLogin_(gdriveLogin) {}

    const GdriveLogin& getGdriveLogin() const { return gdriveLogin_; }

private:
    GdrivePath getGdrivePath(const AfsPath& afsPath) const { return { gdriveLogin_, afsPath }; }

    GdriveRawPath getGdriveRawPath(const AfsPath& afsPath) const //throw SysError
    {
        const std::optional<AfsPath> parentPath = getParentPath(afsPath);
        if (!parentPath)
            throw SysError(L"Item is device root");

        std::string parentId;
        accessGlobalFileState(gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
        {
            parentId = fileState.getItemId(gdriveLogin_.sharedDriveName, *parentPath, true /*followLeafShortcut*/); //throw SysError
        });
        return { std::move(parentId), getItemName(afsPath)};
    }

    Zstring getInitPathPhrase(const AfsPath& afsPath) const override { return concatenateGdriveFolderPathPhrase(getGdrivePath(afsPath)); }

    std::wstring getDisplayPath(const AfsPath& afsPath) const override { return getGdriveDisplayPath(getGdrivePath(afsPath)); }

    bool isNullFileSystem() const override { return gdriveLogin_.email.empty(); }

    int compareDeviceSameAfsType(const AbstractFileSystem& afsRhs) const override
    {
        const GdriveLogin& lhs = gdriveLogin_;
        const GdriveLogin& rhs = static_cast<const GdriveFileSystem&>(afsRhs).gdriveLogin_;

        if (const int rv = compareAsciiNoCase(lhs.email, rhs.email);
            rv != 0)
            return rv;

        return compareNativePath(lhs.sharedDriveName, rhs.sharedDriveName);
    }

    //----------------------------------------------------------------------------------------------------------------
    ItemType getItemType(const AfsPath& afsPath) const override //throw FileError
    {
        if (const std::optional<ItemType> type = itemStillExists(afsPath)) //throw FileError
            return *type;
        throw FileError(replaceCpy(_("Cannot find %x."), L"%x", fmtPath(getDisplayPath(afsPath))));
    }

    std::optional<ItemType> itemStillExists(const AfsPath& afsPath) const override //throw FileError
    {
        try
        {
            GdriveFileState::PathStatus ps;
            accessGlobalFileState(gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
            {
                ps = fileState.getPathStatus(gdriveLogin_.sharedDriveName, afsPath, false /*followLeafShortcut*/); //throw SysError
            });
            if (ps.relPath.empty())
                switch (ps.existingType)
                {
                    //*INDENT-OFF*
                    case GdriveItemType::file:     return ItemType::file;
                    case GdriveItemType::folder:   return ItemType::folder;
                    case GdriveItemType::shortcut: return ItemType::symlink;
                    //*INDENT-ON*
                }
            return {};
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString()); }
    }
    //----------------------------------------------------------------------------------------------------------------

    //already existing: 1. fails or 2. creates duplicate (unlikely)
    void createFolderPlain(const AfsPath& afsPath) const override //throw FileError
    {
        try
        {
            //avoid duplicate Google Drive item creation by multiple threads
            PathAccessLock pal(getGdriveRawPath(afsPath), PathBlockType::otherWait); //throw SysError

            const Zstring folderName = getItemName(afsPath);
            std::string parentId;
            const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
            {
                const GdriveFileState::PathStatus& ps = fileState.getPathStatus(gdriveLogin_.sharedDriveName, afsPath, false /*followLeafShortcut*/); //throw SysError
                if (ps.relPath.empty())
                    throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(folderName)));

                if (ps.relPath.size() > 1) //parent folder missing
                    throw SysError(replaceCpy(_("Cannot find %x."), L"%x", fmtPath(getDisplayPath(AfsPath(nativeAppendPaths(ps.existingPath.value, ps.relPath.front()))))));
                parentId = ps.existingItemId;
            });

            //already existing: creates duplicate
            const std::string folderIdNew = gdriveCreateFolderPlain(folderName, parentId, aai.accessToken); //throw SysError

            //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
            accessGlobalFileState(gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
            {
                fileState.notifyFolderCreated(aai.stateDelta, folderIdNew, folderName, parentId);
            });
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString()); }
    }

    void removeItemPlainImpl(const AfsPath& afsPath, bool permanent /*...or move to trash*/) const //throw SysError
    {
        std::string itemId;
        std::optional<std::string> parentIdToUnlink;
        const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
        {
            const std::optional<AfsPath> parentPath = getParentPath(afsPath);
            if (!parentPath) throw SysError(L"Item is device root");

            GdriveItemDetails itemDetails;
            std::tie(itemId, itemDetails) = fileState.getFileAttributes(gdriveLogin_.sharedDriveName, afsPath, false /*followLeafShortcut*/); //throw SysError
            assert(std::find(itemDetails.parentIds.begin(), itemDetails.parentIds.end(), fileState.getItemId(gdriveLogin_.sharedDriveName, *parentPath, true /*followLeafShortcut*/)) != itemDetails.parentIds.end());

            //hard-link handling applies to shared files as well: 1. it's the right thing (TM) 2. if we're not the owner: deleting would fail
            if (itemDetails.parentIds.size() > 1 || itemDetails.owner == FileOwner::other) //FileOwner::other behaves like a followed symlink! i.e. vanishes if owner deletes it!
                parentIdToUnlink = fileState.getItemId(gdriveLogin_.sharedDriveName, *parentPath, true /*followLeafShortcut*/); //throw SysError
        });

        if (parentIdToUnlink)
        {
            gdriveUnlinkParent(itemId, *parentIdToUnlink, aai.accessToken); //throw SysError

            //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
            accessGlobalFileState(gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
            {
                fileState.notifyParentRemoved(aai.stateDelta, itemId, *parentIdToUnlink);
            });
        }
        else
        {
            if (permanent)
                gdriveDeleteItem(itemId, aai.accessToken); //throw SysError
            else
                gdriveMoveToTrash(itemId, aai.accessToken); //throw SysError

            //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
            accessGlobalFileState(gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
            {
                fileState.notifyItemDeleted(aai.stateDelta, itemId);
            });
        }
    }

    void removeFilePlain(const AfsPath& afsPath) const override //throw FileError
    {
        try { removeItemPlainImpl(afsPath, true /*permanent*/); /*throw SysError*/ }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete file %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString()); }
    }

    void removeSymlinkPlain(const AfsPath& afsPath) const override //throw FileError
    {
        try { removeItemPlainImpl(afsPath, true /*permanent*/); /*throw SysError*/ }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete symbolic link %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString()); }
    }

    void removeFolderPlain(const AfsPath& afsPath) const override //throw FileError
    {
        try { removeItemPlainImpl(afsPath, true /*permanent*/); /*throw SysError*/ }
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
            if (itemStillExists(afsPath)) //throw FileError
                throw;
        }
    }

    //----------------------------------------------------------------------------------------------------------------
    AbstractPath getSymlinkResolvedPath(const AfsPath& afsPath) const override //throw FileError
    {
        //this function doesn't make sense for Google Drive: Shortcuts do not refer by path, but ID!
        //even if it were possible to determine a path, doing anything with the target file (e.g. delete + recreate) would break other Shortcuts!
        throw FileError(replaceCpy(_("Cannot determine final path for %x."), L"%x", fmtPath(getDisplayPath(afsPath))), _("Operation not supported by device."));
    }

    bool equalSymlinkContentForSameAfsType(const AfsPath& afsLhs, const AbstractPath& apRhs) const override //throw FileError
    {
        auto getTargetId = [](const GdriveFileSystem& gdriveFs, const AfsPath& afsPath)
        {
            try
            {
                std::string targetId;
                const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdriveFs.gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
                {
                    const GdriveItemDetails& itemDetails = fileState.getFileAttributes(gdriveFs.gdriveLogin_.sharedDriveName, afsPath, false /*followLeafShortcut*/).second; //throw SysError
                    if (itemDetails.type != GdriveItemType::shortcut)
                        throw SysError(L"Not a Google Drive Shortcut.");

                    targetId = itemDetails.targetId;
                });
                return targetId;
            }
            catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(gdriveFs.getDisplayPath(afsPath))), e.toString()); }
        };

        return getTargetId(*this, afsLhs) == getTargetId(static_cast<const GdriveFileSystem&>(apRhs.afsDevice.ref()), apRhs.afsPath);
    }

    //----------------------------------------------------------------------------------------------------------------

    //return value always bound:
    std::unique_ptr<InputStream> getInputStream(const AfsPath& afsPath, const IOCallback& notifyUnbufferedIO /*throw X*/) const override //throw FileError, (ErrorFileLocked)
    {
        return std::make_unique<InputStreamGdrive>(getGdrivePath(afsPath), notifyUnbufferedIO);
    }

    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    //=> actual behavior: 1. fails or 2. creates duplicate (unlikely)
    std::unique_ptr<OutputStreamImpl> getOutputStream(const AfsPath& afsPath, //throw FileError
                                                      std::optional<uint64_t> streamSize,
                                                      std::optional<time_t> modTime,
                                                      const IOCallback& notifyUnbufferedIO /*throw X*/) const override
    {
        try
        {
            //avoid duplicate item creation by multiple threads
            auto pal = std::make_unique<PathAccessLock>(getGdriveRawPath(afsPath), PathBlockType::otherFail); //throw SysError
            //don't block during a potentially long-running file upload!

            //already existing: 1. fails or 2. creates duplicate
            return std::make_unique<OutputStreamGdrive>(getGdrivePath(afsPath), streamSize, modTime, notifyUnbufferedIO, std::move(pal)); //throw SysError
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString());
        }
    }

    //----------------------------------------------------------------------------------------------------------------
    void traverseFolderRecursive(const TraverserWorkload& workload /*throw X*/, size_t parallelOps) const override
    {
        gdriveTraverseFolderRecursive(gdriveLogin_, workload, parallelOps); //throw X
    }
    //----------------------------------------------------------------------------------------------------------------

    //symlink handling: follow
    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    //=> actual behavior: 1. fails or 2. creates duplicate (unlikely)
    FileCopyResult copyFileForSameAfsType(const AfsPath& afsSource, const StreamAttributes& attrSource, //throw FileError, (ErrorFileLocked), (X)
                                          const AbstractPath& apTarget, bool copyFilePermissions, const IOCallback& notifyUnbufferedIO /*throw X*/) const override
    {
        //no native Google Drive file copy => use stream-based file copy:
        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(AFS::getDisplayPath(apTarget))), _("Operation not supported by device."));

        const GdriveFileSystem& fsTarget = static_cast<const GdriveFileSystem&>(apTarget.afsDevice.ref());

        if (!equalAsciiNoCase(gdriveLogin_.email, fsTarget.gdriveLogin_.email))
            //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
            //=> actual behavior: 1. fails or 2. creates duplicate (unlikely)
            return copyFileAsStream(afsSource, attrSource, apTarget, notifyUnbufferedIO); //throw FileError, (ErrorFileLocked), X
        //else: copying files within account works, e.g. between My Drive <-> shared drives

        try
        {
            //avoid duplicate Google Drive item creation by multiple threads (blocking is okay: gdriveCopyFile() should complete instantly!)
            PathAccessLock pal(fsTarget.getGdriveRawPath(apTarget.afsPath), PathBlockType::otherWait); //throw SysError

            const Zstring itemNameNew = getItemName(apTarget);
            std::string itemIdSrc;
            time_t modTime = 0;
            uint64_t fileSize = 0;
            std::string parentIdTrg;
            const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
            {
                GdriveItemDetails itemDetails;
                std::tie(itemIdSrc, itemDetails) = fileState.getFileAttributes(gdriveLogin_.sharedDriveName, afsSource, true /*followLeafShortcut*/); //throw SysError
                modTime  = itemDetails.modTime;
                fileSize = itemDetails.fileSize;

                assert(itemDetails.type == GdriveItemType::file); //Google Drive *should* fail trying to copy folder: "This file cannot be copied by the user."
                if (itemDetails.type != GdriveItemType::file)     //=> don't trust + improve error message
                    throw SysError(replaceCpy<std::wstring>(L"%x is not a file.", L"%x", fmtPath(getItemName(afsSource))));

                const GdriveFileState::PathStatus psTo = fileState.getPathStatus(fsTarget.gdriveLogin_.sharedDriveName, apTarget.afsPath, false /*followLeafShortcut*/); //throw SysError
                if (psTo.relPath.empty())
                    throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(itemNameNew)));

                if (psTo.relPath.size() > 1) //parent folder missing
                    throw SysError(replaceCpy(_("Cannot find %x."), L"%x",
                                              fmtPath(fsTarget.getDisplayPath(AfsPath(nativeAppendPaths(psTo.existingPath.value, psTo.relPath.front()))))));
                parentIdTrg = psTo.existingItemId;
            });

            //already existing: creates duplicate
            const std::string fileIdTrg = gdriveCopyFile(itemIdSrc, parentIdTrg, itemNameNew, modTime, aai.accessToken); //throw SysError

            //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
            accessGlobalFileState(gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
            {
                GdriveItem newFileItem = {};
                newFileItem.itemId = fileIdTrg;
                newFileItem.details.itemName = itemNameNew;
                newFileItem.details.type = GdriveItemType::file;
                newFileItem.details.owner = FileOwner::me;
                newFileItem.details.fileSize = fileSize;
                newFileItem.details.modTime = modTime;
                newFileItem.details.parentIds.push_back(parentIdTrg);
                fileState.notifyItemCreated(aai.stateDelta, newFileItem);
            });

            FileCopyResult result;
            result.fileSize     = fileSize;
            result.modTime      = modTime;
            result.sourceFileId = itemIdSrc;
            result.targetFileId = fileIdTrg;
            /*result.errorModTime = */
            return result;
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(replaceCpy(_("Cannot copy file %x to %y."),
                                                  L"%x", L'\n' + fmtPath(getDisplayPath(afsSource))),
                                       L"%y",  L'\n' + fmtPath(AFS::getDisplayPath(apTarget))), e.toString());
        }
    }

    //symlink handling: follow
    //already existing: fail
    void copyNewFolderForSameAfsType(const AfsPath& afsSource, const AbstractPath& apTarget, bool copyFilePermissions) const override //throw FileError
    {
        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(AFS::getDisplayPath(apTarget))), _("Operation not supported by device."));

        //already existing: 1. fails or 2. creates duplicate (unlikely)
        AFS::createFolderPlain(apTarget); //throw FileError
    }

    //already existing: fail
    void copySymlinkForSameAfsType(const AfsPath& afsSource, const AbstractPath& apTarget, bool copyFilePermissions) const override //throw FileError
    {
        try
        {
            std::string targetId;
            accessGlobalFileState(gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
            {
                const GdriveItemDetails& itemDetails = fileState.getFileAttributes(gdriveLogin_.sharedDriveName, afsSource, false /*followLeafShortcut*/).second; //throw SysError
                if (itemDetails.type != GdriveItemType::shortcut)
                    throw SysError(L"Not a Google Drive Shortcut.");

                targetId = itemDetails.targetId;
            });

            const GdriveFileSystem& fsTarget = static_cast<const GdriveFileSystem&>(apTarget.afsDevice.ref());

            //avoid duplicate Google Drive item creation by multiple threads
            PathAccessLock pal(fsTarget.getGdriveRawPath(apTarget.afsPath), PathBlockType::otherWait); //throw SysError

            const Zstring shortcutName = getItemName(apTarget.afsPath);
            std::string parentId;
            const GdrivePersistentSessions::AsyncAccessInfo aaiTrg = accessGlobalFileState(fsTarget.gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
            {
                const GdriveFileState::PathStatus& ps = fileState.getPathStatus(fsTarget.gdriveLogin_.sharedDriveName, apTarget.afsPath, false /*followLeafShortcut*/); //throw SysError
                if (ps.relPath.empty())
                    throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(shortcutName)));

                if (ps.relPath.size() > 1) //parent folder missing
                    throw SysError(replaceCpy(_("Cannot find %x."), L"%x", fmtPath(fsTarget.getDisplayPath(AfsPath(nativeAppendPaths(ps.existingPath.value, ps.relPath.front()))))));
                parentId = ps.existingItemId;
            });

            //already existing: creates duplicate
            const std::string shortcutIdNew = gdriveCreateShortcutPlain(shortcutName, parentId, targetId, aaiTrg.accessToken); //throw SysError

            //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
            accessGlobalFileState(fsTarget.gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
            {
                fileState.notifyShortcutCreated(aaiTrg.stateDelta, shortcutIdNew, shortcutName, parentId, targetId);
            });
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."),
                                                  L"%x", L'\n' + fmtPath(getDisplayPath(afsSource))),
                                       L"%y",  L'\n' + fmtPath(AFS::getDisplayPath(apTarget))), e.toString());
        }
    }

    //already existing: undefined behavior! (e.g. fail/overwrite)
    //=> actual behavior: 1. fails or 2. creates duplicate (unlikely)
    void moveAndRenameItemForSameAfsType(const AfsPath& pathFrom, const AbstractPath& pathTo) const override //throw FileError, ErrorMoveUnsupported
    {
        auto generateErrorMsg = [&] { return replaceCpy(replaceCpy(_("Cannot move file %x to %y."),
                                                                   L"%x", L'\n' + fmtPath(getDisplayPath(pathFrom))),
                                                        L"%y",  L'\n' + fmtPath(AFS::getDisplayPath(pathTo)));
                                    };

        const GdriveFileSystem& fsTarget = static_cast<const GdriveFileSystem&>(pathTo.afsDevice.ref());

        if (!equalAsciiNoCase(gdriveLogin_.email, fsTarget.gdriveLogin_.email))
            throw ErrorMoveUnsupported(generateErrorMsg(), _("Operation not supported between different devices."));
        //else: moving files within account works, e.g. between My Drive <-> shared drives

        try
        {
            //avoid duplicate Google Drive item creation by multiple threads
            PathAccessLock pal(fsTarget.getGdriveRawPath(pathTo.afsPath), PathBlockType::otherWait); //throw SysError

            const Zstring itemNameOld = getItemName(pathFrom);
            const Zstring itemNameNew = getItemName(pathTo);
            const std::optional<AfsPath> parentPathFrom = getParentPath(pathFrom);
            const std::optional<AfsPath> parentPathTo   = getParentPath(pathTo.afsPath);
            if (!parentPathFrom) throw SysError(L"Source is device root");
            if (!parentPathTo  ) throw SysError(L"Target is device root");

            std::string itemId;
            time_t modTime = 0;
            std::string parentIdFrom;
            std::string parentIdTo;
            const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
            {
                GdriveItemDetails itemDetails;
                std::tie(itemId, itemDetails) = fileState.getFileAttributes(gdriveLogin_.sharedDriveName, pathFrom, false /*followLeafShortcut*/); //throw SysError

                modTime = itemDetails.modTime;
                parentIdFrom = fileState.getItemId(gdriveLogin_.sharedDriveName, *parentPathFrom, true /*followLeafShortcut*/); //throw SysError

                const GdriveFileState::PathStatus psTo = fileState.getPathStatus(fsTarget.gdriveLogin_.sharedDriveName, pathTo.afsPath, false /*followLeafShortcut*/); //throw SysError

                //e.g. changing file name case only => this is not an "already exists" situation!
                //also: hardlink referenced by two different paths, the source one will be unlinked
                if (psTo.relPath.empty() && psTo.existingItemId == itemId)
                    parentIdTo = fileState.getItemId(fsTarget.gdriveLogin_.sharedDriveName, *parentPathTo, true /*followLeafShortcut*/); //throw SysError
                else
                {
                    if (psTo.relPath.empty())
                        throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(itemNameNew)));

                    if (psTo.relPath.size() > 1) //parent folder missing
                        throw SysError(replaceCpy(_("Cannot find %x."), L"%x",
                                                  fmtPath(fsTarget.getDisplayPath(AfsPath(nativeAppendPaths(psTo.existingPath.value, psTo.relPath.front()))))));
                    parentIdTo = psTo.existingItemId;
                }
            });

            if (parentIdFrom == parentIdTo && itemNameOld == itemNameNew)
                return; //nothing to do

            //already existing: creates duplicate
            gdriveMoveAndRenameItem(itemId, parentIdFrom, parentIdTo, itemNameNew, modTime, aai.accessToken); //throw SysError

            //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
            accessGlobalFileState(gdriveLogin_.email, [&](GdriveFileState& fileState) //throw SysError
            {
                fileState.notifyMoveAndRename(aai.stateDelta, itemId, parentIdFrom, parentIdTo, itemNameNew);
            });
        }
        catch (const SysError& e) { throw FileError(generateErrorMsg(), e.toString()); }
    }

    bool supportsPermissions(const AfsPath& afsPath) const override { return false; } //throw FileError

    //----------------------------------------------------------------------------------------------------------------
    FileIconHolder getFileIcon      (const AfsPath& afsPath, int pixelSize) const override { return {}; } //throw SysError; optional return value
    ImageHolder    getThumbnailImage(const AfsPath& afsPath, int pixelSize) const override { return {}; } //throw SysError; optional return value

    void authenticateAccess(bool allowUserInteraction) const override //throw FileError
    {
        if (allowUserInteraction)
            try
            {
                const std::shared_ptr<GdrivePersistentSessions> gps = globalGdriveSessions.get();
                if (!gps)
                    throw SysError(formatSystemError("GdriveFileSystem::authenticateAccess", L"", L"Function call not allowed during init/shutdown."));

                for (const std::string& accountEmail : gps->listAccounts()) //throw SysError
                    if (equalAsciiNoCase(accountEmail, gdriveLogin_.email))
                        return;
                gps->addUserSession(gdriveLogin_.email /*gdriveLoginHint*/, nullptr /*updateGui*/); //throw SysError
                //error messages will be lost after time out in dir_exist_async.h! However:
                //The most-likely-to-fail parts (web access) are reported by gdriveAuthorizeAccess() via the browser!
            }
            catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to connect to %x."), L"%x", fmtPath(getDisplayPath(AfsPath()))), e.toString()); }
    }

    int getAccessTimeout() const override { return static_cast<int>(std::chrono::seconds(HTTP_SESSION_ACCESS_TIME_OUT).count()); } //returns "0" if no timeout in force

    bool hasNativeTransactionalCopy() const override { return true; }
    //----------------------------------------------------------------------------------------------------------------

    int64_t getFreeDiskSpace(const AfsPath& afsPath) const override //throw FileError, returns < 0 if not available
    {
        if (!gdriveLogin_.sharedDriveName.empty())
            return -1;

        try
        {
            const std::string& accessToken = accessGlobalFileState(gdriveLogin_.email, [](GdriveFileState& fileState) {}).accessToken; //throw SysError
            return gdriveGetMyDriveFreeSpace(accessToken); //throw SysError; returns < 0 if not available
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot determine free disk space for %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString()); }
    }

    bool supportsRecycleBin(const AfsPath& afsPath) const override { return true; } //throw FileError

    std::unique_ptr<RecycleSession> createRecyclerSession(const AfsPath& afsPath) const override //throw FileError, return value must be bound!
    {
        struct RecycleSessionGdrive : public RecycleSession
        {
            void recycleItemIfExists(const AbstractPath& itemPath, const Zstring& logicalRelPath) override { AFS::recycleItemIfExists(itemPath); } //throw FileError
            void tryCleanup(const std::function<void (const std::wstring& displayPath)>& notifyDeletionStatus) override {}; //throw FileError
        };

        return std::make_unique<RecycleSessionGdrive>();
    }

    void recycleItemIfExists(const AfsPath& afsPath) const override //throw FileError
    {
        try
        {
            removeItemPlainImpl(afsPath, false /*permanent*/); //throw SysError
        }
        catch (const SysError& e)
        {
            if (itemStillExists(afsPath)) //throw FileError
                throw FileError(replaceCpy(_("Unable to move %x to the recycle bin."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString());
        }
    }

    const GdriveLogin gdriveLogin_;
};
//===========================================================================================================================
}


void fff::gdriveInit(const Zstring& configDirPath, const Zstring& caCertFilePath)
{
    assert(!globalHttpSessionManager.get());
    globalHttpSessionManager.set(std::make_unique<HttpSessionManager>(caCertFilePath));

    assert(!globalGdriveSessions.get());
    globalGdriveSessions.set(std::make_unique<GdrivePersistentSessions>(configDirPath));
}


void fff::gdriveTeardown()
{
    try //don't use ~GdrivePersistentSessions() to save! Might never happen, e.g. detached thread waiting for Google Drive authentication; terminated on exit!
    {
        if (const std::shared_ptr<GdrivePersistentSessions> gps = globalGdriveSessions.get())
            gps->saveActiveSessions(); //throw FileError
    }
    catch (FileError&) { assert(false); }

    assert(globalGdriveSessions.get());
    globalGdriveSessions.set(nullptr);

    assert(globalHttpSessionManager.get());
    globalHttpSessionManager.set(nullptr);
}


std::string fff::gdriveAddUser(const std::function<void()>& updateGui /*throw X*/) //throw FileError, X
{
    try
    {
        if (const std::shared_ptr<GdrivePersistentSessions> gps = globalGdriveSessions.get())
            return gps->addUserSession("" /*gdriveLoginHint*/, updateGui); //throw SysError, X

        throw SysError(formatSystemError("gdriveAddUser", L"", L"Function call not allowed during init/shutdown."));
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to connect to %x."), L"%x", L"Google Drive"), e.toString()); }
}


void fff::gdriveRemoveUser(const std::string& accountEmail) //throw FileError
{
    try
    {
        if (const std::shared_ptr<GdrivePersistentSessions> gps = globalGdriveSessions.get())
            return gps->removeUserSession(accountEmail); //throw SysError

        throw SysError(formatSystemError("gdriveRemoveUser", L"", L"Function call not allowed during init/shutdown."));
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to disconnect from %x."), L"%x", fmtPath(getGdriveDisplayPath({{ accountEmail, Zstr("")}, AfsPath() }))), e.toString()); }
}


std::vector<std::string /*account email*/> fff::gdriveListAccounts() //throw FileError
{
    try
    {
        if (const std::shared_ptr<GdrivePersistentSessions> gps = globalGdriveSessions.get())
            return gps->listAccounts(); //throw SysError

        throw SysError(formatSystemError("gdriveListAccounts", L"", L"Function call not allowed during init/shutdown."));
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to access %x."), L"%x", L"Google Drive"), e.toString()); }
}


std::vector<Zstring /*sharedDriveName*/> fff::gdriveListSharedDrives(const std::string& accountEmail) //throw FileError
{
    try
    {
        std::vector<Zstring> sharedDriveNames;
        accessGlobalFileState(accountEmail, [&](GdriveFileState& fileState) //throw SysError
        {
            sharedDriveNames = fileState.listSharedDrives();
        });
        return sharedDriveNames;
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to access %x."), L"%x", fmtPath(getGdriveDisplayPath({{ accountEmail, Zstr("")}, AfsPath() }))), e.toString()); }
}


AfsDevice fff::condenseToGdriveDevice(const GdriveLogin& login) //noexcept
{
    //clean up input:
    GdriveLogin loginTmp = login;
    trim(loginTmp.email);

    return makeSharedRef<GdriveFileSystem>(loginTmp);
}


GdriveLogin fff::extractGdriveLogin(const AfsDevice& afsDevice) //noexcept
{
    if (const auto gdriveDevice = dynamic_cast<const GdriveFileSystem*>(&afsDevice.ref()))
        return gdriveDevice ->getGdriveLogin();

    assert(false);
    return {};
}


bool fff::acceptsItemPathPhraseGdrive(const Zstring& itemPathPhrase) //noexcept
{
    Zstring path = expandMacros(itemPathPhrase); //expand before trimming!
    trim(path);
    return startsWithAsciiNoCase(path, gdrivePrefix);
}


//e.g.: gdrive:/john@gmail.com:SharedDrive/folder/file.txt
AbstractPath fff::createItemPathGdrive(const Zstring& itemPathPhrase) //noexcept
{
    Zstring path = itemPathPhrase;
    path = expandMacros(path); //expand before trimming!
    trim(path);

    if (startsWithAsciiNoCase(path, gdrivePrefix))
        path = path.c_str() + strLength(gdrivePrefix);

    const AfsPath& sanPath = sanitizeDeviceRelativePath(path); //Win/macOS compatibility: let's ignore slash/backslash differences

    const Zstring& accountEmailAndDrive = beforeFirst(sanPath.value, FILE_NAME_SEPARATOR, IfNotFoundReturn::all);
    const AfsPath afsPath                 (afterFirst(sanPath.value, FILE_NAME_SEPARATOR, IfNotFoundReturn::none));

    const Zstring& accountEmail = beforeFirst(accountEmailAndDrive, Zstr(':'), IfNotFoundReturn::all);
    const Zstring& sharedDrive  = afterFirst (accountEmailAndDrive, Zstr(':'), IfNotFoundReturn::none);

    return AbstractPath(makeSharedRef<GdriveFileSystem>(GdriveLogin{utfTo<std::string>(accountEmail), sharedDrive}), afsPath);
}

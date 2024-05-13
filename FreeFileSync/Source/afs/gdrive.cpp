// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "gdrive.h"
#include <variant>
#include <unordered_set> //needed by clang
#include <unordered_map> //
#include <libcurl/curl_wrap.h> //DON'T include <curl/curl.h> directly!
//#include <zen/basic_math.h>
#include <zen/base64.h>
//#include <zen/crc.h>
#include <zen/file_access.h>
#include <zen/file_io.h>
#include <zen/file_traverser.h>
#include <zen/guid.h>
#include <zen/http.h>
#include <zen/json.h>
#include <zen/resolve_path.h>
#include <zen/process_exec.h>
#include <zen/socket.h>
#include <zen/shutdown.h>
#include <zen/time.h>
#include <zen/zlib_wrap.h>
#include "abstract_impl.h"
#include "init_curl_libssh2.h"
    #include <poll.h>

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

    return compareNativePath(lhs.itemName, rhs.itemName);
}

constinit Global<PathAccessLocker<GdriveRawPath>> globalGdrivePathAccessLocker;
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

constexpr std::chrono::seconds HTTP_SESSION_MAX_IDLE_TIME  (20);
constexpr std::chrono::seconds HTTP_SESSION_CLEANUP_INTERVAL(4);
constexpr std::chrono::seconds GDRIVE_SYNC_INTERVAL         (5);

const size_t GDRIVE_BLOCK_SIZE_DOWNLOAD =  64 * 1024; //libcurl returns blocks of only 16 kB as returned by recv() even if we request larger blocks via CURLOPT_BUFFERSIZE
const size_t GDRIVE_BLOCK_SIZE_UPLOAD   =  64 * 1024; //libcurl requests blocks of 64 kB. larger blocksizes set via CURLOPT_UPLOAD_BUFFERSIZE do not seem to make a difference
const size_t GDRIVE_STREAM_BUFFER_SIZE = 1024 * 1024; //unit: [byte]
//stream buffer should be big enough to facilitate prefetching during alternating read/write operations => e.g. see serialize.h::unbufferedStreamCopy()

constexpr ZstringView gdrivePrefix = Zstr("gdrive:");
const char gdriveFolderMimeType  [] = "application/vnd.google-apps.folder";
const char gdriveShortcutMimeType[] = "application/vnd.google-apps.shortcut"; //= symbolic link!

const char DB_FILE_DESCR[] = "FreeFileSync";
const int  DB_FILE_VERSION = 5; //2021-05-15

std::string getGdriveClientId    () { return ""; } // => replace with live credentials
std::string getGdriveClientSecret() { return ""; } //




struct HttpSessionId
{
    explicit HttpSessionId(const Zstring& serverName) :
        server(serverName) {}

    Zstring server;
};

inline
bool operator==(const HttpSessionId& lhs, const HttpSessionId& rhs) { return equalAsciiNoCase(lhs.server, rhs.server); }
}

//exactly the type of case insensitive comparison we need for server names!
//https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfow#IDNs
template<> struct std::hash<HttpSessionId> { size_t operator()(const HttpSessionId& sessionId) const { return StringHashAsciiNoCase()(sessionId.server); } };


namespace
{
Zstring concatenateGdriveFolderPathPhrase(const GdrivePath& gdrivePath); //noexcept


//e.g.: gdrive:/john@gmail.com:SharedDrive/folder/file.txt
std::wstring getGdriveDisplayPath(const GdrivePath& gdrivePath)
{
    Zstring displayPath = Zstring(gdrivePrefix) + FILE_NAME_SEPARATOR;

    displayPath += utfTo<Zstring>(gdrivePath.gdriveLogin.email);

    if (!gdrivePath.gdriveLogin.locationName.empty())
        displayPath += Zstr(':') + gdrivePath.gdriveLogin.locationName;

    if (!gdrivePath.itemPath.value.empty())
        displayPath += FILE_NAME_SEPARATOR + gdrivePath.itemPath.value;

    return utfTo<std::wstring>(displayPath);
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


AFS::FingerPrint getGdriveFilePrint(const std::string& itemId)
{
    assert(!itemId.empty());
    //Google Drive item ID is persistent and globally unique! :)
    return hashString<AFS::FingerPrint>(itemId);
}

//----------------------------------------------------------------------------------------------------------------

constinit Global<UniSessionCounter> httpSessionCount;
GLOBAL_RUN_ONCE(httpSessionCount.set(createUniSessionCounter()));
UniInitializer globalInitHttp(*httpSessionCount.get());

//----------------------------------------------------------------------------------------------------------------

class HttpSessionManager //reuse (healthy) HTTP sessions globally
{
public:
    explicit HttpSessionManager(const Zstring& caCertFilePath) :
        caCertFilePath_(caCertFilePath),
        sessionCleaner_([this]
    {
        setCurrentThreadName(Zstr("Session Cleaner[HTTP]"));
        runGlobalSessionCleanUp(); //throw ThreadStopRequest
    }) {}

    void access(const HttpSessionId& sessionId, const std::function<void(HttpSession& session)>& useHttpSession /*throw X*/) //throw SysError, X
    {
        Protected<HttpSessionManager::HttpSessionCache>& sessionCache = getSessionCache(sessionId);

        std::unique_ptr<HttpInitSession> httpSession;

        sessionCache.access([&](HttpSessionManager::HttpSessionCache& sessions)
        {
            //assume "isHealthy()" to avoid hitting server connection limits: (clean up of !isHealthy() after use, idle sessions via worker thread)
            if (!sessions.empty())
            {
                httpSession = std::move(sessions.back    ());
                /**/                    sessions.pop_back();
            }
        });

        //create new HTTP session outside the lock: 1. don't block other threads 2. non-atomic regarding "sessionCache"! => one session too many is not a problem!
        if (!httpSession)
            httpSession = std::make_unique<HttpInitSession>(sessionId.server, caCertFilePath_); //throw SysError

        ZEN_ON_SCOPE_EXIT(
            if (isHealthy(httpSession->session)) //thread that created the "!isHealthy()" session is responsible for clean up (avoid hitting server connection limits!)
        sessionCache.access([&](HttpSessionManager::HttpSessionCache& sessions) { sessions.push_back(std::move(httpSession)); }); );

        useHttpSession(httpSession->session); //throw X
    }

private:
    HttpSessionManager           (const HttpSessionManager&) = delete;
    HttpSessionManager& operator=(const HttpSessionManager&) = delete;

    //associate session counting (for initialization/teardown)
    struct HttpInitSession
    {
        HttpInitSession(const Zstring& server, const Zstring& caCertFilePath) :
            session(server, true /*useTls*/, caCertFilePath) {}

        const std::shared_ptr<UniCounterCookie> cookie{getLibsshCurlUnifiedInitCookie(httpSessionCount)}; //throw SysError
        HttpSession session; //life time must be subset of UniCounterCookie
    };
    static bool isHealthy(const HttpSession& s) { return std::chrono::steady_clock::now() - s.getLastUseTime() <= HTTP_SESSION_MAX_IDLE_TIME; }

    using HttpSessionCache = std::vector<std::unique_ptr<HttpInitSession>>;

    Protected<HttpSessionCache>& getSessionCache(const HttpSessionId& sessionId)
    {
        //single global session store per sessionId; life-time bound to globalInstance => never remove a sessionCache!!!
        Protected<HttpSessionCache>* sessionCache = nullptr;

        globalSessionCache_.access([&](GlobalHttpSessions& sessionsById)
        {
            sessionCache = &sessionsById[sessionId]; //get or create
        });
        static_assert(std::is_same_v<GlobalHttpSessions, std::unordered_map<HttpSessionId, Protected<HttpSessionCache>>>, "require std::unordered_map so that the pointers we return remain stable");

        return *sessionCache;
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

            std::vector<Protected<HttpSessionCache>*> sessionCaches; //pointers remain stable, thanks to std::unordered_map<>

            globalSessionCache_.access([&](GlobalHttpSessions& sessionsByCfg)
            {
                for (auto& [sessionCfg, idleSession] : sessionsByCfg)
                    sessionCaches.push_back(&idleSession);
            });

            for (Protected<HttpSessionCache>* sessionCache : sessionCaches)
                for (;;)
                {
                    bool done = false;
                    sessionCache->access([&](HttpSessionCache& sessions)
                    {
                        for (std::unique_ptr<HttpInitSession>& sshSession : sessions)
                            if (!isHealthy(sshSession->session)) //!isHealthy() sessions are destroyed after use => in this context this means they have been idle for too long
                            {
                                sshSession.swap(sessions.back());
                                /**/            sessions.pop_back(); //run ~HttpSession *inside* the lock! => avoid hitting server limits!
                                return; //don't hold lock for too long: delete only one session at a time, then yield...
                            }
                        done = true;
                    });
                    if (done)
                        break;
                    std::this_thread::yield();
                }
        }
    }

    using GlobalHttpSessions = std::unordered_map<HttpSessionId, Protected<HttpSessionCache>>;

    Protected<GlobalHttpSessions> globalSessionCache_;
    const Zstring caCertFilePath_;
    InterruptibleThread sessionCleaner_;
};

//--------------------------------------------------------------------------------------
constinit Global<HttpSessionManager> globalHttpSessionManager; //caveat: life time must be subset of static UniInitializer!
//--------------------------------------------------------------------------------------

struct GdriveAccess
{
    std::string token;
    int timeoutSec = 0;
};

//===========================================================================================================================

HttpSession::Result googleHttpsRequest(const Zstring& serverName, const std::string& serverRelPath, //throw SysError, X
                                       const std::vector<std::string>& extraHeaders,
                                       std::vector<CurlOption> extraOptions,
                                       const std::function<void  (std::span<const char> buf)>& writeResponse /*throw X*/, //optional
                                       const std::function<size_t(std::span<      char> buf)>& readRequest   /*throw X*/, //optional; return "bytesToRead" bytes unless end of stream!
                                       const std::function<void(const std::string_view& header)>& receiveHeader /*throw X*/, //optional
                                       int timeoutSec)
{
    //https://developers.google.com/drive/api/v3/performance
    //"In order to receive a gzip-encoded response you must do two things: Set an Accept-Encoding header, ["gzip" automatically set by HttpSession]
    extraOptions.emplace_back(CURLOPT_USERAGENT, "FreeFileSync (gzip)"); //and modify your user agent to contain the string gzip."

    const std::shared_ptr<HttpSessionManager> mgr = globalHttpSessionManager.get();
    if (!mgr)
        throw SysError(formatSystemError("googleHttpsRequest", L"", L"Function call not allowed during init/shutdown."));

    HttpSession::Result httpResult;

    mgr->access(HttpSessionId(serverName), [&](HttpSession& session) //throw SysError
    {
        httpResult = session.perform(serverRelPath, extraHeaders, extraOptions, writeResponse, readRequest, receiveHeader, timeoutSec); //throw SysError, X
    });
    return httpResult;
}


//try to get a grip on this crazy REST API: - parameters are passed via query string, header, or body, using GET, POST, PUT, PATCH, DELETE, ... it's a dice roll
HttpSession::Result gdriveHttpsRequest(const std::string& serverRelPath, //throw SysError, X
                                       std::vector<std::string> extraHeaders,
                                       const std::vector<CurlOption>& extraOptions,
                                       const std::function<void  (std::span<const char> buf)>& writeResponse /*throw X*/, //optional
                                       const std::function<size_t(std::span<      char> buf)>& readRequest   /*throw X*/, //optional; return "bytesToRead" bytes unless end of stream!
                                       const std::function<void(const std::string_view& header)>& receiveHeader /*throw X*/, //optional
                                       const GdriveAccess& access)
{
    extraHeaders.push_back("Authorization: Bearer " + access.token);

    return googleHttpsRequest(GOOGLE_REST_API_SERVER, serverRelPath,
                              extraHeaders,
                              extraOptions,
                              writeResponse /*throw X*/,
                              readRequest /*throw X*/,
                              receiveHeader /*throw X*/, access.timeoutSec); //throw SysError, X
}

//========================================================================================================

struct GdriveUser
{
    std::wstring displayName;
    std::string email;
};
GdriveUser getGdriveUser(const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/about
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        {"fields", "user/displayName,user/emailAddress"},
    });
    std::string response;
    gdriveHttpsRequest("/drive/v3/about?" + queryParams, {} /*extraHeaders*/, {} /*extraOptions*/,
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); }, nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    if (const JsonValue* user = getChildFromJsonObject(jresponse, "user"))
    {
        const std::optional<std::string> displayName = getPrimitiveFromJsonObject(*user, "displayName");
        const std::optional<std::string> email       = getPrimitiveFromJsonObject(*user, "emailAddress");
        if (displayName && email)
            return {utfTo<std::wstring>(*displayName), *email};
    }

    throw SysError(formatGdriveErrorRaw(response));
}


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

GdriveAccessInfo gdriveExchangeAuthCode(const GdriveAuthCode& authCode, int timeoutSec) //throw SysError
{
    //https://developers.google.com/identity/protocols/OAuth2InstalledApp#exchange-authorization-code
    const std::string postBuf = xWwwFormUrlEncode(
    {
        {"code",          authCode.code},
        {"client_id",     getGdriveClientId()},
        {"client_secret", getGdriveClientSecret()},
        {"redirect_uri",  authCode.redirectUrl},
        {"grant_type",    "authorization_code"},
        {"code_verifier", authCode.codeChallenge},
    });
    std::string response;
    googleHttpsRequest(Zstr("oauth2.googleapis.com"), "/token", {} /*extraHeaders*/, {{CURLOPT_POSTFIELDS, postBuf.c_str()}},
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    nullptr /*readRequest*/, nullptr /*receiveHeader*/, timeoutSec); //throw SysError

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> accessToken  = getPrimitiveFromJsonObject(jresponse, "access_token");
    const std::optional<std::string> refreshToken = getPrimitiveFromJsonObject(jresponse, "refresh_token");
    const std::optional<std::string> expiresIn    = getPrimitiveFromJsonObject(jresponse, "expires_in"); //e.g. 3600 seconds
    if (!accessToken || !refreshToken || !expiresIn)
        throw SysError(formatGdriveErrorRaw(response));

    const GdriveUser userInfo = getGdriveUser({*accessToken, timeoutSec}); //throw SysError

    return {{*accessToken, std::time(nullptr) + stringTo<time_t>(*expiresIn)}, *refreshToken, userInfo};
}


//Astyle fucks up because of the raw string literal!
//*INDENT-OFF*
GdriveAccessInfo gdriveAuthorizeAccess(const std::string& gdriveLoginHint, const std::function<void()>& updateGui /*throw X*/, int timeoutSec) //throw SysError, X
{
    //spin up a web server to wait for the HTTP GET after Google authentication
    const addrinfo hints 
    {
        .ai_flags =  
                    AI_ADDRCONFIG | //no such issue on Linux: https://bugs.chromium.org/p/chromium/issues/detail?id=5234
                    AI_PASSIVE, //the returned socket addresses will be suitable for bind(2)ing a socket that will accept(2) connections.
        .ai_family   = AF_INET, //make sure our server is reached by IPv4 127.0.0.1, not IPv6 [::1]
        .ai_socktype = SOCK_STREAM, //we *do* care about this one!
    };
    addrinfo* servinfo = nullptr;
    ZEN_ON_SCOPE_EXIT(if (servinfo) ::freeaddrinfo(servinfo));

    //ServiceName == "0": open the next best free port
    const int rcGai = ::getaddrinfo(nullptr,    //_In_opt_ PCSTR            pNodeName
                                    "0",        //_In_opt_ PCSTR            pServiceName
                                    &hints,     //_In_opt_ const ADDRINFOA* pHints
                                    &servinfo); //_Outptr_ PADDRINFOA*      ppResult
    if (rcGai != 0)
        THROW_LAST_SYS_ERROR_GAI(rcGai);
    if (!servinfo)
        throw SysError(L"getaddrinfo: empty server info");

    const auto getBoundSocket = [](const auto& /*::addrinfo*/ ai)
    {
        SocketType testSocket = ::socket(ai.ai_family,    //int socket_family
                                         SOCK_CLOEXEC |
                                         ai.ai_socktype,  //int socket_type
                                         ai.ai_protocol); //int protocol
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


    sockaddr_storage addr = {}; //"sufficiently large to store address information for IPv4 (AF_INET) or IPv6 (AF_INET6)" => sockaddr_in and sockaddr_in6
    socklen_t addrLen = sizeof(addr);
    if (::getsockname(socket, reinterpret_cast<sockaddr*>(&addr), &addrLen) != 0)
        THROW_LAST_SYS_ERROR_WSA("getsockname");

    if (addr.ss_family != AF_INET)
        throw SysError(formatSystemError("getsockname", L"", L"Unexpected protocol family: " + numberTo<std::wstring>(addr.ss_family)));

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
        {"client_id",      getGdriveClientId()},
        {"redirect_uri",   redirectUrl},
        {"response_type",  "code"},
        {"scope",          "https://www.googleapis.com/auth/drive"},
        {"code_challenge", codeChallenge},
        {"code_challenge_method", "plain"},
        {"login_hint",     gdriveLoginHint},
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

            const int waitTimeMs = 100;
            pollfd fds[] = {{socket, POLLIN}};

            const char* functionName = "poll";
            const int rv = ::poll(fds, std::size(fds), waitTimeMs); //int timeout
            if (rv < 0)
                THROW_LAST_SYS_ERROR_WSA(functionName);
            else if (rv != 0)
                break;
            //else: time-out!
        }
        //potential race! if the connection is gone right after ::select() and before ::accept(), latter will hang
        const int clientSocket = ::accept4(socket,        //int sockfd
                                           nullptr,       //sockaddr* addr
                                           nullptr,       //socklen_t* addrlen
                                           SOCK_CLOEXEC); //int flags
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
        const std::vector<std::string_view> statusItems = splitCpy<std::string_view>(reqLine, ' ', SplitOnEmpty::allow); //Method SP Request-URI SP HTTP-Version CRLF

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
            std::string htmlMsg = R"(<!DOCTYPE html>
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
            try
            {
                if (!error.empty())
                    throw SysError(replaceCpy(_("Error code %x"), L"%x",  + L"\"" + utfTo<std::wstring>(error) + L"\""));

                //do as many login-related tasks as possible while we have the browser as an error output device!
                //see AFS::connectNetworkFolder() => errors will be lost after time out in dir_exist_async.h!
                authResult = gdriveExchangeAuthCode({code, redirectUrl, codeChallenge}, timeoutSec); //throw SysError
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
//*INDENT-ON*


GdriveAccessToken gdriveRefreshAccess(const std::string& refreshToken, int timeoutSec) //throw SysError
{
    //https://developers.google.com/identity/protocols/OAuth2InstalledApp#offline
    const std::string postBuf = xWwwFormUrlEncode(
    {
        {"refresh_token", refreshToken},
        {"client_id",     getGdriveClientId()},
        {"client_secret", getGdriveClientSecret()},
        {"grant_type",    "refresh_token"},
    });
    std::string response;
    googleHttpsRequest(Zstr("oauth2.googleapis.com"), "/token", {} /*extraHeaders*/, {{CURLOPT_POSTFIELDS, postBuf.c_str()}},
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    nullptr /*readRequest*/, nullptr /*receiveHeader*/, timeoutSec); //throw SysError

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> accessToken = getPrimitiveFromJsonObject(jresponse, "access_token");
    const std::optional<std::string> expiresIn   = getPrimitiveFromJsonObject(jresponse, "expires_in"); //e.g. 3600 seconds
    if (!accessToken || !expiresIn)
        throw SysError(formatGdriveErrorRaw(response));

    return {*accessToken, std::time(nullptr) + stringTo<time_t>(*expiresIn)};
}


void gdriveRevokeAccess(const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/identity/protocols/OAuth2InstalledApp#tokenrevoke
    std::string response;
    const HttpSession::Result httpResult = googleHttpsRequest(Zstr("oauth2.googleapis.com"), "/revoke?token=" + access.token,
    {"Content-Type: application/x-www-form-urlencoded"}, {{ CURLOPT_POSTFIELDS, ""}},
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    nullptr /*readRequest*/, nullptr /*receiveHeader*/, access.timeoutSec); //throw SysError

    if (httpResult.statusCode != 200)
        throw SysError(formatGdriveErrorRaw(response));
}


int64_t gdriveGetMyDriveFreeSpace(const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/about
    std::string response;
    gdriveHttpsRequest("/drive/v3/about?fields=storageQuota", {} /*extraHeaders*/, {} /*extraOptions*/,
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

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


//instead of the "root" alias Google uses an actual ID in file metadata
std::string /*itemId*/ getMyDriveId(const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/get
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        {"supportsAllDrives", "true"},
        {"fields", "id"},
    });
    std::string response;
    gdriveHttpsRequest("/drive/v3/files/root?" + queryParams, {} /*extraHeaders*/, {} /*extraOptions*/,
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> itemId = getPrimitiveFromJsonObject(jresponse, "id");
    if (!itemId)
        throw SysError(formatGdriveErrorRaw(response));

    return *itemId;
}


struct DriveDetails
{
    std::string driveId;
    Zstring driveName;
};
std::vector<DriveDetails> getSharedDrives(const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/drives/list
    std::vector<DriveDetails> sharedDrives;
    {
        std::optional<std::string> nextPageToken;
        do
        {
            std::string queryParams = xWwwFormUrlEncode(
            {
                {"pageSize", "100"}, //"[1, 100] Default: 10"
                {"fields", "nextPageToken,drives(id,name)"},
            });
            if (nextPageToken)
                queryParams += '&' + xWwwFormUrlEncode({{"pageToken", *nextPageToken}});

            std::string response;
            gdriveHttpsRequest("/drive/v3/drives?" + queryParams, {} /*extraHeaders*/, {} /*extraOptions*/,
            [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
            nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

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
                if (!driveId || !driveName || driveName->empty())
                    throw SysError(formatGdriveErrorRaw(serializeJson(driveVal)));

                sharedDrives.push_back({std::move(*driveId), utfTo<Zstring>(*driveName)});
            }
        }
        while (nextPageToken);
    }
    return sharedDrives;
}


struct StarredFolderDetails
{
    std::string folderId;
    Zstring folderName;
    std::string sharedDriveId; //empty if on "My Drive"
};
std::vector<StarredFolderDetails> getStarredFolders(const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/list
    std::vector<StarredFolderDetails> starredFolders;
    {
        std::optional<std::string> nextPageToken;
        do
        {
            std::string queryParams = xWwwFormUrlEncode(
            {
                {"corpora", "allDrives"}, //"The 'user' corpus includes all files in "My Drive" and "Shared with me" https://developers.google.com/drive/api/v3/reference/files/list
                {"includeItemsFromAllDrives", "true"},
                {"pageSize", "1000"}, //"[1, 1000] Default: 100"
                {"q", std::string("not trashed and starred and mimeType = '") + gdriveFolderMimeType + "'"},
                {"spaces", "drive"},
                {"supportsAllDrives", "true"},
                {"fields", "nextPageToken,incompleteSearch,files(id,name,driveId)"}, //https://developers.google.com/drive/api/v3/reference/files
            });
            if (nextPageToken)
                queryParams += '&' + xWwwFormUrlEncode({{"pageToken", *nextPageToken}});

            std::string response;
            gdriveHttpsRequest("/drive/v3/files?" + queryParams, {} /*extraHeaders*/, {} /*extraOptions*/,
            [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
            nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

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
                assert(childVal.type == JsonValue::Type::object);
                const std::optional<std::string> itemId   = getPrimitiveFromJsonObject(childVal, "id");
                const std::optional<std::string> itemName = getPrimitiveFromJsonObject(childVal, "name");
                const std::optional<std::string> driveId  = getPrimitiveFromJsonObject(childVal, "driveId");

                if (!itemId || itemId->empty() || !itemName || itemName->empty())
                    throw SysError(formatGdriveErrorRaw(serializeJson(childVal)));

                starredFolders.push_back({*itemId,
                                          utfTo<Zstring>(*itemName),
                                          driveId ? *driveId : ""});
            }
        }
        while (nextPageToken);
    }
    return starredFolders;
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


GdriveItemDetails extractItemDetails(const JsonValue& jvalue) //throw SysError
{
    assert(jvalue.type == JsonValue::Type::object);

    /**/  std::optional<std::string> itemName     = getPrimitiveFromJsonObject(jvalue, "name");
    const std::optional<std::string> mimeType     = getPrimitiveFromJsonObject(jvalue, "mimeType");
    const std::optional<std::string> ownedByMe    = getPrimitiveFromJsonObject(jvalue, "ownedByMe");
    const std::optional<std::string> size         = getPrimitiveFromJsonObject(jvalue, "size");
    const std::optional<std::string> modifiedTime = getPrimitiveFromJsonObject(jvalue, "modifiedTime");
    const JsonValue*                 parents      = getChildFromJsonObject    (jvalue, "parents");
    const JsonValue*                 shortcut     = getChildFromJsonObject    (jvalue, "shortcutDetails");

    if (!itemName || itemName->empty() || !mimeType || !modifiedTime)
        throw SysError(formatGdriveErrorRaw(serializeJson(jvalue)));

    const GdriveItemType type = *mimeType == gdriveFolderMimeType   ? GdriveItemType::folder :
                                *mimeType == gdriveShortcutMimeType ? GdriveItemType::shortcut :
                                GdriveItemType::file;

    const FileOwner owner = ownedByMe ? (*ownedByMe == "true" ? FileOwner::me : FileOwner::other) : FileOwner::none; //"Not populated for items in Shared Drives"
    const uint64_t fileSize = size ? stringTo<uint64_t>(*size) : 0; //not available for folders and shortcuts

    //RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    const TimeComp tc = parseTime("%Y-%m-%dT%H:%M:%S", beforeLast(*modifiedTime, '.', IfNotFoundReturn::all));
    if (tc == TimeComp() || !endsWith(*modifiedTime, 'Z')) //'Z' means "UTC" => it seems Google doesn't use the time-zone offset postfix
        throw SysError(L"Modification time is invalid. (" + utfTo<std::wstring>(*modifiedTime) + L')');

    const auto [modTime, timeValid] = utcToTimeT(tc);
    if (!timeValid)
        throw SysError(L"Modification time is invalid. (" + utfTo<std::wstring>(*modifiedTime) + L')');

    std::vector<std::string> parentIds;
    if (parents) //item without "parents" array is possible! e.g. 1. shared item located in "Shared with me", referenced via a Shortcut 2. root folder under "Computers"
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

    return {utfTo<Zstring>(*itemName), fileSize, modTime, type, owner, std::move(targetId), std::move(parentIds)};
}


GdriveItemDetails getItemDetails(const std::string& itemId, const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/get
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        {"fields", "trashed,name,mimeType,ownedByMe,size,modifiedTime,parents,shortcutDetails(targetId)"},
        {"supportsAllDrives", "true"},
    });
    std::string response;
    gdriveHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams, {} /*extraHeaders*/, {} /*extraOptions*/,
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError
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
std::vector<GdriveItem> readFolderContent(const std::string& folderId, const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/list
    std::vector<GdriveItem> childItems;
    {
        std::optional<std::string> nextPageToken;
        do
        {
            std::string queryParams = xWwwFormUrlEncode(
            {
                {"corpora", "allDrives"}, //"The 'user' corpus includes all files in "My Drive" and "Shared with me" https://developers.google.com/drive/api/v3/reference/files/list
                {"includeItemsFromAllDrives", "true"},
                {"pageSize", "1000"}, //"[1, 1000] Default: 100"
                {"q", "not trashed and '" + folderId + "' in parents"},
                {"spaces", "drive"},
                {"supportsAllDrives", "true"},
                {"fields", "nextPageToken,incompleteSearch,files(id,name,mimeType,ownedByMe,size,modifiedTime,parents,shortcutDetails(targetId))"}, //https://developers.google.com/drive/api/v3/reference/files
            });
            if (nextPageToken)
                queryParams += '&' + xWwwFormUrlEncode({{"pageToken", *nextPageToken}});

            std::string response;
            gdriveHttpsRequest("/drive/v3/files?" + queryParams, {} /*extraHeaders*/, {} /*extraOptions*/,
            [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
            nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

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

                childItems.push_back({std::move(*itemId), std::move(itemDetails)});
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
ChangesDelta getChangesDelta(const std::string& sharedDriveId /*empty for "My Drive"*/, const std::string& startPageToken, const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/changes/list
    ChangesDelta delta;
    std::optional<std::string> nextPageToken = startPageToken;
    for (;;)
    {
        std::string queryParams = xWwwFormUrlEncode(
        {
            {"pageToken", *nextPageToken},
            {"fields", "kind,nextPageToken,newStartPageToken,changes(kind,changeType,removed,fileId,file(trashed,name,mimeType,ownedByMe,size,modifiedTime,parents,shortcutDetails(targetId)),driveId,drive(name))"},
            {"includeItemsFromAllDrives", "true"}, //semantics are a mess https://developers.google.com/drive/api/v3/enable-shareddrives https://freefilesync.org/forum/viewtopic.php?t=7827&start=30#p29712
            //in short: if driveId is set: required, but blatant lie; only drive-specific file changes returned
            //          if no driveId set: optional, but blatant lie; only changes to drive objects are returned, but not contained files (with a few exceptions)
            {"pageSize", "1000"}, //"[1, 1000] Default: 100"
            {"spaces", "drive"},
            {"supportsAllDrives", "true"},
            //do NOT "restrictToMyDrive": we're also interested in "Shared with me" items, which might be referenced by a shortcut in "My Drive"
        });
        if (!sharedDriveId.empty())
            queryParams += '&' + xWwwFormUrlEncode({{"driveId", sharedDriveId}}); //only allowed for shared drives!

        std::string response;
        gdriveHttpsRequest("/drive/v3/changes?" + queryParams, {} /*extraHeaders*/, {} /*extraOptions*/,
        [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
        nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

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


std::string /*startPageToken*/ getChangesCurrentToken(const std::string& sharedDriveId /*empty for "My Drive"*/, const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/changes/getStartPageToken
    std::string queryParams = xWwwFormUrlEncode(
    {
        {"supportsAllDrives", "true"},
    });
    if (!sharedDriveId.empty())
        queryParams += '&' + xWwwFormUrlEncode({{"driveId", sharedDriveId}}); //only allowed for shared drives!

    std::string response;
    gdriveHttpsRequest("/drive/v3/changes/startPageToken?" + queryParams, {} /*extraHeaders*/, {} /*extraOptions*/,
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

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
void gdriveDeleteItem(const std::string& itemId, const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/delete
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        {"supportsAllDrives", "true"},
    });
    std::string response;
    const HttpSession::Result httpResult = gdriveHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams,
    {} /*extraHeaders*/, {{CURLOPT_CUSTOMREQUEST, "DELETE"}}, [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

    if (response.empty() && httpResult.statusCode == 204)
        return; //"If successful, this method returns an empty response body"

    throw SysError(formatGdriveErrorRaw(response));
}


//item is NOT deleted when last parent is removed: it is just not accessible via the "My Drive" hierarchy but still adds to quota! => use for hard links only!
void gdriveUnlinkParent(const std::string& itemId, const std::string& parentId, const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/update
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        {"removeParents", parentId},
        {"supportsAllDrives", "true"},
        {"fields", "id,parents"}, //for test if operation was successful
    });
    std::string response;
    const HttpSession::Result httpResult = gdriveHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams,
    {"Content-Type: application/json; charset=UTF-8"}, {{CURLOPT_CUSTOMREQUEST, "PATCH"}, { CURLOPT_POSTFIELDS, "{}"}},
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); }, nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

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
void gdriveMoveToTrash(const std::string& itemId, const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/update
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        {"supportsAllDrives", "true"},
        {"fields", "trashed"},
    });
    const std::string postBuf = R"({ "trashed": true })";

    std::string response;
    gdriveHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams,
    {"Content-Type: application/json; charset=UTF-8"}, {{CURLOPT_CUSTOMREQUEST, "PATCH"}, {CURLOPT_POSTFIELDS, postBuf.c_str()}},
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); }, nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

    JsonValue jresponse;
    try { jresponse = parseJson(response); /*throw JsonParsingError*/ }
    catch (const JsonParsingError&) {}

    const std::optional<std::string> trashed = getPrimitiveFromJsonObject(jresponse, "trashed");
    if (!trashed || *trashed != "true")
        throw SysError(formatGdriveErrorRaw(response));
}


//folder name already existing? will (happily) create duplicate => caller must check!
std::string /*folderId*/ gdriveCreateFolderPlain(const Zstring& folderName, const std::string& parentId, const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/folder#creating_a_folder
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        {"supportsAllDrives", "true"},
        {"fields", "id"},
    });
    JsonValue postParams(JsonValue::Type::object);
    postParams.objectVal.emplace("mimeType", gdriveFolderMimeType);
    postParams.objectVal.emplace("name", utfTo<std::string>(folderName));
    postParams.objectVal.emplace("parents", std::vector<JsonValue> {JsonValue(parentId)});
    const std::string& postBuf = serializeJson(postParams, "" /*lineBreak*/, "" /*indent*/);

    std::string response;
    gdriveHttpsRequest("/drive/v3/files?" + queryParams,
    {"Content-Type: application/json; charset=UTF-8"}, {{CURLOPT_POSTFIELDS, postBuf.c_str()}},
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); }, nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> itemId = getPrimitiveFromJsonObject(jresponse, "id");
    if (!itemId)
        throw SysError(formatGdriveErrorRaw(response));
    return *itemId;
}


//shortcut name already existing? will (happily) create duplicate => caller must check!
std::string /*shortcutId*/ gdriveCreateShortcutPlain(const Zstring& shortcutName, const std::string& parentId,  const std::string& targetId, const GdriveAccess& access) //throw SysError
{
    /* https://developers.google.com/drive/api/v3/shortcuts
       - targetMimeType is determined automatically (ignored if passed)
       - creating shortcuts to shortcuts fails with "Internal Error"              */
    const std::string& queryParams = xWwwFormUrlEncode(
    {
        {"supportsAllDrives", "true"},
        {"fields", "id"},
    });
    JsonValue shortcutDetails(JsonValue::Type::object);
    shortcutDetails.objectVal.emplace("targetId", targetId);

    JsonValue postParams(JsonValue::Type::object);
    postParams.objectVal.emplace("mimeType", gdriveShortcutMimeType);
    postParams.objectVal.emplace("name", utfTo<std::string>(shortcutName));
    postParams.objectVal.emplace("parents", std::vector<JsonValue> {JsonValue(parentId)});
    postParams.objectVal.emplace("shortcutDetails", std::move(shortcutDetails));
    const std::string& postBuf = serializeJson(postParams, "" /*lineBreak*/, "" /*indent*/);

    std::string response;
    gdriveHttpsRequest("/drive/v3/files?" + queryParams, {"Content-Type: application/json; charset=UTF-8"},
    {{CURLOPT_POSTFIELDS, postBuf.c_str()}}, [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

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
std::string /*fileId*/ gdriveCopyFile(const std::string& fileId, const std::string& parentIdTo, const Zstring& newName, time_t newModTime, const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/copy
    const std::string queryParams = xWwwFormUrlEncode(
    {
        {"supportsAllDrives", "true"},
        {"fields", "id"},
    });

    //more Google Drive peculiarities: changing the file name changes modifiedTime!!! => workaround:

    //RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    const std::string modTimeRfc = utfTo<std::string>(formatTime(Zstr("%Y-%m-%dT%H:%M:%S.000Z"), getUtcTime(newModTime))); //returns empty string on error
    if (modTimeRfc.empty())
        throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(newModTime) + L')');

    JsonValue postParams(JsonValue::Type::object);
    postParams.objectVal.emplace("name", utfTo<std::string>(newName));
    postParams.objectVal.emplace("parents", std::vector<JsonValue> {JsonValue(parentIdTo)});
    postParams.objectVal.emplace("modifiedTime", modTimeRfc);
    const std::string& postBuf = serializeJson(postParams, "" /*lineBreak*/, "" /*indent*/);

    std::string response;
    gdriveHttpsRequest("/drive/v3/files/" + fileId + "/copy?" + queryParams,
    {"Content-Type: application/json; charset=UTF-8"}, {{CURLOPT_POSTFIELDS, postBuf.c_str()}},
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

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
                             const Zstring& newName, time_t newModTime, const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/folder#moving_files_between_folders
    std::string queryParams = xWwwFormUrlEncode(
    {
        {"supportsAllDrives", "true"},
        {"fields", "name,parents"}, //for test if operation was successful
    });

    if (parentIdFrom != parentIdTo)
        queryParams += '&' + xWwwFormUrlEncode(
    {
        {"removeParents", parentIdFrom},
        {"addParents",    parentIdTo},
    });

    //more Google Drive peculiarities: changing the file name changes modifiedTime!!! => workaround:

    //RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    const std::string modTimeRfc = utfTo<std::string>(formatTime(Zstr("%Y-%m-%dT%H:%M:%S.000Z"), getUtcTime(newModTime))); //returns empty string on error
    if (modTimeRfc.empty())
        throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(newModTime) + L')');

    JsonValue postParams(JsonValue::Type::object);
    postParams.objectVal.emplace("name", utfTo<std::string>(newName));
    postParams.objectVal.emplace("modifiedTime", modTimeRfc);
    const std::string& postBuf = serializeJson(postParams, "" /*lineBreak*/, "" /*indent*/);

    std::string response;
    gdriveHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams,
    {"Content-Type: application/json; charset=UTF-8"}, {{CURLOPT_CUSTOMREQUEST, "PATCH"}, {CURLOPT_POSTFIELDS, postBuf.c_str()}},
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

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
void setModTime(const std::string& itemId, time_t modTime, const GdriveAccess& access) //throw SysError
{
    //https://developers.google.com/drive/api/v3/reference/files/update
    //RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    const std::string& modTimeRfc = formatTime<std::string>("%Y-%m-%dT%H:%M:%S.000Z", getUtcTime2(modTime)); //returns empty string on error
    if (modTimeRfc.empty())
        throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(modTime) + L')');

    const std::string& queryParams = xWwwFormUrlEncode(
    {
        {"supportsAllDrives", "true"},
        {"fields", "modifiedTime"},
    });
    const std::string postBuf = R"({ "modifiedTime": ")" + modTimeRfc + "\" }";

    std::string response;
    gdriveHttpsRequest("/drive/v3/files/" + itemId + '?' + queryParams,
    {"Content-Type: application/json; charset=UTF-8"}, {{CURLOPT_CUSTOMREQUEST, "PATCH"}, {CURLOPT_POSTFIELDS, postBuf.c_str()}},
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    nullptr /*readRequest*/, nullptr /*receiveHeader*/, access); //throw SysError

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
                            bool acknowledgeAbuse, const GdriveAccess& access)
{
    /*  https://developers.google.com/drive/api/v3/manage-downloads
        doesn't work for Google-specific file types, but Google Backup & Sync still "downloads" them:
            - in some JSON-like file format:
                {"url": "https://docs.google.com/open?id=FILE_ID", "doc_id": "FILE_ID", "email": "ACCOUNT_EMAIL"}

            - adds artificial file extensions: .gdoc, .gsheet, .gslides, ...

            - 2022-10-10: In "Google Drive for Desktop" the file content now looks like:
                {"":"WARNING! DO NOT EDIT THIS FILE! ANY CHANGES MADE WILL BE LOST!","doc_id":"FILE_ID","resource_key":"","email":"ACCOUNT_EMAIL"}     */

    std::string queryParams = xWwwFormUrlEncode(
    {
        {"supportsAllDrives", "true"},
        {"alt", "media"},
    });
    if (acknowledgeAbuse) //apply on demand only! https://freefilesync.org/forum/viewtopic.php?t=7520")
        queryParams += '&' + xWwwFormUrlEncode({{"acknowledgeAbuse", "true"}});

    std::string headBytes;
    bool headBytesWritten = false;

    const HttpSession::Result httpResult = gdriveHttpsRequest("/drive/v3/files/" + fileId + '?' + queryParams, {} /*extraHeaders*/, {} /*extraOptions*/,
                                                              [&](std::span<const char> buf)
                                                              /*  libcurl feeds us a shitload of tiny kB-sized zlib-decompressed pieces of data!
                                                                  libcurl's zlib buffer is sized at ridiculous 16 kB!
                                                                  => if this ever becomes a perf issue: roll our own zlib decompression!   */
    {
        if (headBytes.size() < 16 * 1024) //don't access writeBlock() yet in case of error! (=> support acknowledgeAbuse retry handling)
            headBytes.append(buf.data(), buf.size());
        else
        {
            if (!headBytesWritten)
            {
                headBytesWritten = true;
                writeBlock(headBytes.c_str(), headBytes.size()); //throw X
            }

            writeBlock(buf.data(), buf.size()); //throw X
        }
    }, nullptr /*tryReadRequest*/, nullptr /*receiveHeader*/, access); //throw SysError, X

    if (httpResult.statusCode / 100 != 2)
    {
        /* https://freefilesync.org/forum/viewtopic.php?t=7463 => HTTP status code 403 + body:
            { "error": { "errors": [{ "domain": "global",
                                      "reason": "cannotDownloadAbusiveFile",
                                      "message": "This file has been identified as malware or spam and cannot be downloaded." }],
                         "code": 403,
                         "message": "This file has been identified as malware or spam and cannot be downloaded." }}       */
        if (!headBytesWritten && httpResult.statusCode == 403 && contains(headBytes, "\"cannotDownloadAbusiveFile\""))
            throw SysErrorAbusiveFile(formatGdriveErrorRaw(headBytes));

        throw SysError(formatGdriveErrorRaw(headBytes));
    }

    if (!headBytesWritten && !headBytes.empty())
        writeBlock(headBytes.c_str(), headBytes.size()); //throw X
}


void gdriveDownloadFile(const std::string& fileId, const std::function<void(const void* buffer, size_t bytesToWrite)>& writeBlock /*throw X*/, //throw SysError, X
                        const GdriveAccess& access)
{
    try
    {
        gdriveDownloadFileImpl(fileId, writeBlock /*throw X*/, false /*acknowledgeAbuse*/, access); //throw SysError, SysErrorAbusiveFile, X
    }
    catch (SysErrorAbusiveFile&)
    {
        gdriveDownloadFileImpl(fileId, writeBlock /*throw X*/, true /*acknowledgeAbuse*/, access); //throw SysError, (SysErrorAbusiveFile), X
    }
}


#if 0
//file name already existing? => duplicate file created!
//note: Google Drive upload is already transactional!
//upload "small files" (5 MB or less; enforced by Google?) in a single round-trip
std::string /*itemId*/ gdriveUploadSmallFile(const Zstring& fileName, const std::string& parentId, uint64_t streamSize, std::optional<time_t> modTime, //throw SysError, X
                                             const std::function<size_t(void* buffer, size_t bytesToRead)>& readBlock /*throw X; return "bytesToRead" bytes unless end of stream*/,
                                             const GdriveAccess& access)
{
    //https://developers.google.com/drive/api/v3/folder#inserting_a_file_in_a_folder
    //https://developers.google.com/drive/api/v3/manage-uploads#http_1

    JsonValue postParams(JsonValue::Type::object);
    postParams.objectVal.emplace("name", utfTo<std::string>(fileName));
    postParams.objectVal.emplace("parents", std::vector<JsonValue> {JsonValue(parentId)});
    if (modTime) //convert to RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
    {
        const std::string& modTimeRfc = utfTo<std::string>(formatTime(Zstr("%Y-%m-%dT%H:%M:%S.000Z"), getUtcTime2(*modTime))); //returns empty string on error
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
        const auto bufStart = buffer;

        if (headPos < postBufHead.size())
        {
            const size_t junkSize = std::min<ptrdiff_t>(postBufHead.size() - headPos, bytesToRead);
            std::memcpy(buffer, postBufHead.c_str() + headPos, junkSize);
            headPos += junkSize;
            buffer = static_cast<std::byte*>(buffer) + junkSize;
            bytesToRead -= junkSize;
        }
        if (bytesToRead > 0)
        {
            if (!eof) //don't assume readBlock() will return streamSize bytes as promised => exhaust and let Google Drive fail if there is a mismatch in Content-Length!
            {
                const size_t bytesRead = readBlock(buffer, bytesToRead); //throw X; return "bytesToRead" bytes unless end of stream
                buffer = static_cast<std::byte*>(buffer) + bytesRead;
                bytesToRead -= bytesRead;

                if (bytesToRead > 0)
                    eof = true;
            }
            if (bytesToRead > 0)
                if (tailPos < postBufTail.size())
                {
                    const size_t junkSize = std::min<ptrdiff_t>(postBufTail.size() - tailPos, bytesToRead);
                    std::memcpy(buffer, postBufTail.c_str() + tailPos, junkSize);
                    tailPos += junkSize;
                    buffer = static_cast<std::byte*>(buffer) + junkSize;
                    bytesToRead -= junkSize;
                }
        }
        return static_cast<std::byte*>(buffer) -
               static_cast<std::byte*>(bufStart);
    };

TODO:
    gzip-compress HTTP request body!

    const std::string& queryParams = xWwwFormUrlEncode(
    {
        {"supportsAllDrives", "true"},
        {"uploadType", "multipart"},
    });
    std::string response;
    const HttpSession::Result httpResult = gdriveHttpsRequest("/upload/drive/v3/files?" + queryParams,
    {
        "Content-Type: multipart/related; boundary=" + boundaryString,
        "Content-Length: " + numberTo<std::string>(postBufHead.size() + streamSize + postBufTail.size())
    },
    {{CURLOPT_POST, 1}}, //otherwise HttpSession::perform() will PUT
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
    readMultipartBlock, nullptr /*receiveHeader*/, access); //throw SysError, X

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
                                        const std::function<size_t(void* buffer, size_t bytesToRead)>& tryReadBlock /*throw X*/, //returning 0 signals EOF: Posix read() semantics
                                        const GdriveAccess& access)
{
    //https://developers.google.com/drive/api/v3/folder#inserting_a_file_in_a_folder
    //https://developers.google.com/drive/api/v3/manage-uploads#resumable

    //step 1: initiate resumable upload session
    std::string uploadUrlRelative;
    {
        const std::string& queryParams = xWwwFormUrlEncode(
        {
            {"supportsAllDrives", "true"},
            {"uploadType", "resumable"},
        });
        JsonValue postParams(JsonValue::Type::object);
        postParams.objectVal.emplace("name", utfTo<std::string>(fileName));
        postParams.objectVal.emplace("parents", std::vector<JsonValue> {JsonValue(parentId)});
        if (modTime) //convert to RFC 3339 date-time: e.g. "2018-09-29T08:39:12.053Z"
        {
            const std::string& modTimeRfc = utfTo<std::string>(formatTime(Zstr("%Y-%m-%dT%H:%M:%S.000Z"), getUtcTime(*modTime))); //returns empty string on error
            if (modTimeRfc.empty())
                throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(*modTime) + L')');

            postParams.objectVal.emplace("modifiedTime", modTimeRfc);
        }
        const std::string& postBuf = serializeJson(postParams, "" /*lineBreak*/, "" /*indent*/);
        //---------------------------------------------------

        std::string uploadUrl;

        auto onHeaderData = [&](const std::string_view& header)
        {
            //"The callback will be called once for each header and only complete header lines are passed on to the callback" (including \r\n at the end)
            if (startsWithAsciiNoCase(header, "Location:"))
            {
                uploadUrl = header;
                uploadUrl = afterFirst(uploadUrl, ':', IfNotFoundReturn::none);
                trim(uploadUrl);
            }
        };

        std::string response;
        const HttpSession::Result httpResult = gdriveHttpsRequest("/upload/drive/v3/files?" + queryParams,
        {"Content-Type: application/json; charset=UTF-8"}, {{CURLOPT_POSTFIELDS, postBuf.c_str()}},
        [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); },
        nullptr /*readRequest*/, onHeaderData, access); //throw SysError

        if (httpResult.statusCode != 200)
            throw SysError(formatGdriveErrorRaw(response));

        if (!startsWith(uploadUrl, "https://www.googleapis.com/"))
            throw SysError(L"Invalid upload URL: " + utfTo<std::wstring>(uploadUrl)); //user should never see this

        uploadUrlRelative = afterFirst(uploadUrl, "googleapis.com", IfNotFoundReturn::none);
    }
    //---------------------------------------------------
    //step 2: upload file content

    //not officially documented, but Google Drive supports compressed file upload when "Content-Encoding: gzip" is set! :)))
    InputStreamAsGzip gzipStream(tryReadBlock, GDRIVE_BLOCK_SIZE_UPLOAD); //throw SysError

    auto readRequest = [&](std::span<char> buf) { return gzipStream.read(buf.data(), buf.size()); }; //throw SysError, X

    std::string response; //don't need "Authorization: Bearer":
    googleHttpsRequest(GOOGLE_REST_API_SERVER, uploadUrlRelative, { "Content-Encoding: gzip" }, {} /*extraOptions*/,
    [&](std::span<const char> buf) { response.append(buf.data(), buf.size()); }, readRequest,
    nullptr /*receiveHeader*/, access.timeoutSec); //throw SysError, X

    JsonValue jresponse;
    try { jresponse = parseJson(response); }
    catch (JsonParsingError&) {}

    const std::optional<std::string> itemId = getPrimitiveFromJsonObject(jresponse, "id");
    if (!itemId)
        throw SysError(formatGdriveErrorRaw(response));

    return *itemId;
}


class GdriveAccessBuffer //per-user-session & drive! => serialize access (perf: amortized fully buffered!)
{
public:
    //GdriveDrivesBuffer constructor calls GdriveAccessBuffer::getAccessToken()
    explicit GdriveAccessBuffer(const GdriveAccessInfo& accessInfo) :
        accessInfo_(accessInfo) {}

    GdriveAccessBuffer(MemoryStreamIn& stream) //throw SysError
    {
        accessInfo_.accessToken.validUntil = readNumber<int64_t>(stream);                             //
        accessInfo_.accessToken.value      = readContainer<std::string>(stream);                      //
        accessInfo_.refreshToken           = readContainer<std::string>(stream);                      //SysErrorUnexpectedEos
        accessInfo_.userInfo.displayName   = utfTo<std::wstring>(readContainer<std::string>(stream)); //
        accessInfo_.userInfo.email         =                     readContainer<std::string>(stream);  //
    }

    void serialize(MemoryStreamOut& stream) const
    {
        writeNumber<int64_t>(stream, accessInfo_.accessToken.validUntil);
        static_assert(sizeof(accessInfo_.accessToken.validUntil) <= sizeof(int64_t)); //ensure cross-platform compatibility!
        writeContainer(stream, accessInfo_.accessToken.value);
        writeContainer(stream, accessInfo_.refreshToken);
        writeContainer(stream, utfTo<std::string>(accessInfo_.userInfo.displayName));
        writeContainer(stream,                    accessInfo_.userInfo.email);
    }

    //set *before* calling any of the subsequent functions; see GdrivePersistentSessions::accessUserSession()
    void setContextTimeout(const std::weak_ptr<int>& timeoutSec) { timeoutSec_ = timeoutSec; }

    GdriveAccess getAccessToken() //throw SysError
    {
        const int timeoutSec = getTimeoutSec();

        if (accessInfo_.accessToken.validUntil <= std::time(nullptr) + timeoutSec + 5 /*some leeway*/) //expired/will expire
        {
            GdriveAccessToken token = gdriveRefreshAccess(accessInfo_.refreshToken, timeoutSec); //throw SysError

            //"there are limits on the number of refresh tokens that will be issued"
            //Google Drive access token is usually valid for one hour => fail on pathologic user-defined time out:
            if (token.validUntil <= std::time(nullptr) + 2 * timeoutSec)
                throw SysError(_("Please set up a shorter time out for Google Drive.") + L" [" + _P("1 sec", "%x sec", timeoutSec) + L']');

            accessInfo_.accessToken = std::move(token);
        }

        return {accessInfo_.accessToken.value, timeoutSec};
    }

    const std::string& getUserEmail() const { return accessInfo_.userInfo.email; }

    void update(const GdriveAccessInfo& accessInfo)
    {
        if (!equalAsciiNoCase(accessInfo.userInfo.email, accessInfo_.userInfo.email))
            throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
        accessInfo_ = accessInfo;
    }

private:
    GdriveAccessBuffer           (const GdriveAccessBuffer&) = delete;
    GdriveAccessBuffer& operator=(const GdriveAccessBuffer&) = delete;

    int getTimeoutSec() const
    {
        const std::shared_ptr<int> timeoutSec = timeoutSec_.lock();
        assert(timeoutSec);
        if (!timeoutSec)
            throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] GdriveAccessBuffer: Timeout duration was not set.");

        return *timeoutSec;
    }

    GdriveAccessInfo accessInfo_;
    std::weak_ptr<int> timeoutSec_;
};


class GdriveDrivesBuffer;


class GdriveFileState //per-user-session! => serialize access (perf: amortized fully buffered!)
{
public:
    GdriveFileState(const std::string& driveId, //ID of shared drive or "My Drive": never empty!
                    const Zstring& sharedDriveName, //*empty* for "My Drive"
                    GdriveAccessBuffer& accessBuf) : //throw SysError
        /* issue getChangesCurrentToken() as the very first Google Drive query! */
        lastSyncToken_(getChangesCurrentToken(sharedDriveName.empty() ? std::string() : driveId, accessBuf.getAccessToken())), //throw SysError
        driveId_(driveId),
        sharedDriveName_(sharedDriveName),
        accessBuf_(accessBuf) { assert(!driveId.empty() && sharedDriveName != Zstr("My Drive")); }

    GdriveFileState(MemoryStreamIn& stream, GdriveAccessBuffer& accessBuf) : //throw SysError
        accessBuf_(accessBuf)
    {
        lastSyncToken_   = readContainer<std::string>(stream); //
        driveId_         = readContainer<std::string>(stream); //SysErrorUnexpectedEos
        sharedDriveName_ = utfTo<Zstring>(readContainer<std::string>(stream)); //

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

            GdriveItemDetails details = {}; //read in correct sequence!
            details.itemName = utfTo<Zstring>(readContainer<std::string>(stream)); //
            details.type     = readNumber<GdriveItemType>(stream); //
            details.owner    = readNumber     <FileOwner>(stream); //
            details.fileSize = readNumber      <uint64_t>(stream); //SysErrorUnexpectedEos
            details.modTime  = static_cast<time_t>(readNumber<int64_t>(stream)); //
            details.targetId = readContainer<std::string>(stream); //

            size_t parentsCount = readNumber<uint32_t>(stream); //SysErrorUnexpectedEos
            while (parentsCount-- != 0)
                details.parentIds.push_back(readContainer<std::string>(stream)); //SysErrorUnexpectedEos

            updateItemState(itemId, &details);
        }
    }

    void serialize(MemoryStreamOut& stream) const
    {
        writeContainer(stream, lastSyncToken_);
        writeContainer(stream, driveId_);
        writeContainer(stream, utfTo<std::string>(sharedDriveName_));

        for (const auto& [folderId, content] : folderContents_)
            if (folderId.empty())
                throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
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
                for (const auto& itItem : content.childItems)
                {
                    const auto& [itemId, details] = *itItem;
                    if (itemId.empty())
                        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
                    serializeItem(itemId, details);

                    if (details.type == GdriveItemType::shortcut)
                    {
                        if (details.targetId.empty())
                            throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

                        if (auto it = itemDetails_.find(details.targetId);
                            it != itemDetails_.end())
                            serializeItem(details.targetId, it->second);
                    }
                }
        writeContainer(stream, std::string()); //sentinel
    }

    std::string getDriveId() const { return driveId_; }

    Zstring getSharedDriveName() const { return sharedDriveName_; } //*empty* for "My Drive"

    void setSharedDriveName(const Zstring& sharedDriveName) { sharedDriveName_ = sharedDriveName; }

    struct PathStatus
    {
        std::string existingItemId;
        GdriveItemType existingType = GdriveItemType::file;
        AfsPath existingPath;         //input path =: existingPath + relPath
        std::vector<Zstring> relPath; //
    };
    PathStatus getPathStatus(const std::string& locationRootId, const AfsPath& itemPath, bool followLeafShortcut) //throw SysError
    {
        const std::vector<Zstring> relPath = splitCpy(itemPath.value, FILE_NAME_SEPARATOR, SplitOnEmpty::skip);
        if (relPath.empty())
            return {locationRootId, GdriveItemType::folder, AfsPath(), {}};
        else
            return getPathStatusSub(locationRootId, AfsPath(), relPath, followLeafShortcut); //throw SysError
    }

    std::string /*itemId*/ getItemId(const std::string& locationRootId, const AfsPath& itemPath, bool followLeafShortcut) //throw SysError
    {
        const GdriveFileState::PathStatus& ps = getPathStatus(locationRootId, itemPath, followLeafShortcut); //throw SysError
        if (ps.relPath.empty())
            return ps.existingItemId;

        throw SysError(replaceCpy(_("%x does not exist."), L"%x", fmtPath(ps.relPath.front())));
    }

    std::pair<std::string /*itemId*/, GdriveItemDetails> getFileAttributes(const std::string& locationRootId, const AfsPath& itemPath, bool followLeafShortcut) //throw SysError
    {
        if (itemPath.value.empty()) //location root not covered by itemDetails_
        {
            GdriveItemDetails rootDetails
            {
                .type = GdriveItemType::folder,
                //.itemName =... => better leave empty for a root item!
                .owner = sharedDriveName_.empty() ? FileOwner::me : FileOwner::none,
            };
            return {locationRootId, std::move(rootDetails)};
        }

        const std::string itemId = getItemId(locationRootId, itemPath, followLeafShortcut); //throw SysError
        if (auto it = itemDetails_.find(itemId);
            it != itemDetails_.end())
            return *it;

        //itemId was already found! => (must either be a location root) or buffered in itemDetails_
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
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
            childItems.push_back({childId, childDetails});
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
        GdriveItemDetails details
        {
            .itemName = folderName,
            .modTime = std::time(nullptr),
            .type = GdriveItemType::folder,
            .owner = FileOwner::me,
            .parentIds{parentId},
        };

        //avoid needless conflicts due to different Google Drive folder modTime!
        if (auto it = itemDetails_.find(folderId); it != itemDetails_.end())
            details.modTime = it->second.modTime;

        notifyItemUpdated(stateDelta, folderId, &details);
    }

    void notifyShortcutCreated(const FileStateDelta& stateDelta, const std::string& shortcutId, const Zstring& shortcutName, const std::string& parentId, const std::string& targetId)
    {
        GdriveItemDetails details
        {
            .itemName = shortcutName,
            .modTime = std::time(nullptr),
            .type = GdriveItemType::shortcut,
            .owner = FileOwner::me,
            .targetId = targetId,
            .parentIds{parentId},
        };

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
            std::erase(detailsNew.parentIds, parentIdOld);
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

    friend class GdriveDrivesBuffer;

    void notifyItemUpdated(const FileStateDelta& stateDelta, const std::string& itemId, const GdriveItemDetails* details)
    {
        if (!stateDelta.changedIds->contains(itemId)) //no conflicting changes in the meantime?
            updateItemState(itemId, details);         //=> accept new state data
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
        const ChangesDelta delta = getChangesDelta(sharedDriveName_.empty() ? std::string() : driveId_, lastSyncToken_, accessBuf_.getAccessToken()); //throw SysError

        for (const FileChange& change : delta.fileChanges)
            updateItemState(change.itemId, get(change.details));

        lastSyncToken_ = delta.newStartPageToken;
        lastSyncTime_ = std::chrono::steady_clock::now();

        //good to know: if item is created and deleted between polling for changes it is still reported as deleted by Google!
        //Same goes for any other change that is undone in between change notification syncs.
    }

    PathStatus getPathStatusSub(const std::string& folderId, const AfsPath& folderPath, const std::vector<Zstring>& relPath, bool followLeafShortcut) //throw SysError
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
                throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
        }

        auto itFound = itemDetails_.cend();
        for (const DetailsIterator& itChild : itKnown->second.childItems)
            //Since Google Drive has no concept of a file path, we have to roll our own "path to ID" mapping => let's use the platform-native style
            if (equalNativePath(itChild->second.itemName, relPath.front()))
            {
                if (itFound != itemDetails_.end())
                    throw SysError(replaceCpy(_("The name %x is used by more than one item in the folder."), L"%x", fmtPath(relPath.front())));

                itFound = itChild;
            }

        if (itFound == itemDetails_.end())
            return {folderId, GdriveItemType::folder, folderPath, relPath}; //always a folder, see check before recursion above
        else
        {
            auto getItemDetailsBuffered = [&](const std::string& itemId) -> const GdriveItemDetails&
            {
                auto it = itemDetails_.find(itemId);
                if (it == itemDetails_.end())
                {
                    notifyItemUpdated(registerFileStateDelta(), {itemId, getItemDetails(itemId, accessBuf_.getAccessToken())}); //throw SysError
                    //perf: always buffered, except for direct, first-time folder access!
                    it = itemDetails_.find(itemId);
                    assert(it != itemDetails_.end());
                }
                return it->second;
            };

            const auto& [childId, childDetails] = *itFound;
            const AfsPath              childItemPath(appendPath(folderPath.value, relPath.front()));
            const std::vector<Zstring> childRelPath(relPath.begin() + 1, relPath.end());

            if (childRelPath.empty())
            {
                if (childDetails.type == GdriveItemType::shortcut && followLeafShortcut)
                    return {childDetails.targetId, getItemDetailsBuffered(childDetails.targetId).type, childItemPath, childRelPath};
                else
                    return {childId, childDetails.type, childItemPath, childRelPath};
            }

            switch (childDetails.type)
            {
                case GdriveItemType::file: //parent/file/child-rel-path... => obscure, but possible
                    throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(AFS::getItemName(childItemPath))));

                case GdriveItemType::folder:
                    return getPathStatusSub(childId, childItemPath, childRelPath, followLeafShortcut); //throw SysError

                case GdriveItemType::shortcut:
                    switch (getItemDetailsBuffered(childDetails.targetId).type)
                    {
                        case GdriveItemType::file: //parent/file-symlink/child-rel-path... => obscure, but possible
                            throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(AFS::getItemName(childItemPath))));

                        case GdriveItemType::folder: //parent/folder-symlink/child-rel-path... => always follow
                            return getPathStatusSub(childDetails.targetId, childItemPath, childRelPath, followLeafShortcut); //throw SysError

                        case GdriveItemType::shortcut: //should never happen: creating shortcuts to shortcuts fails with "Internal Error"
                            throw SysError(replaceCpy<std::wstring>(L"Google Drive Shortcut %x is pointing to another Shortcut.", L"%x", fmtPath(AFS::getItemName(childItemPath))));
                    }
                    break;
            }
            throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
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
                    throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!"); //WTF!?

                std::vector<std::string> parentIdsNew     = details->parentIds;
                std::vector<std::string> parentIdsRemoved = it->second.parentIds;
                std::erase_if(parentIdsNew,     [&](const std::string& id) { return std::find(it->second.parentIds.begin(), it->second.parentIds.end(), id) != it->second.parentIds.end(); });
                std::erase_if(parentIdsRemoved, [&](const std::string& id) { return std::find(details->parentIds.begin(), details->parentIds.end(), id) != details->parentIds.end(); });

                for (const std::string& parentId : parentIdsNew)
                    folderContents_[parentId].childItems.push_back(it); //new insert => no need for duplicate check

                for (const std::string& parentId : parentIdsRemoved)
                    if (auto itP = folderContents_.find(parentId); itP != folderContents_.end())
                        std::erase(itP->second.childItems, it);
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
                        std::erase(itP->second.childItems, it);

                itemDetails_.erase(it);
            }

            if (auto itP = folderContents_.find(itemId); itP != folderContents_.end())
            {
                //2. delete as parent from child items (don't wait for change notifications of children)
                //  what if e.g. single change notification "folder removed", then folder reapears,
                //  and no notifications for child items: possible with Google drive!?
                //  => no problem: FolderContent::isKnownFolder will be false for this restored folder => only a rescan needed
                for (auto itChild : itP->second.childItems)
                    std::erase(itChild->second.parentIds, itemId);
                folderContents_.erase(itP);
            }
        }
    }

    using DetailsIterator = std::unordered_map<std::string, GdriveItemDetails>::iterator;

    struct FolderContent
    {
        bool isKnownFolder = false; //:= we've seen its full content at least once; further changes are calculated via change notifications
        std::vector<DetailsIterator> childItems;
    };
    std::unordered_map<std::string /*folderId*/, FolderContent> folderContents_;
    std::unordered_map<std::string /*itemId*/, GdriveItemDetails> itemDetails_; //contains ALL known, existing items!

    std::string lastSyncToken_; //drive-specific(!) marker corresponding to last sync with Google's change notifications
    std::chrono::steady_clock::time_point lastSyncTime_ = std::chrono::steady_clock::now() - GDRIVE_SYNC_INTERVAL; //... with Google Drive (default: sync is due)

    std::vector<std::weak_ptr<ItemIdDelta>> changeLog_; //track changed items since FileStateDelta was created (includes sync with Google + our own intermediate change notifications)

    std::string driveId_; //ID of shared drive or "My Drive": never empty!
    Zstring sharedDriveName_; //name of shared drive: empty for "My Drive"!

    GdriveAccessBuffer& accessBuf_;
};


class GdriveFileStateAtLocation
{
public:
    GdriveFileStateAtLocation(GdriveFileState& fileState, const std::string& locationRootId) : fileState_(fileState), locationRootId_(locationRootId) {}

    GdriveFileState::PathStatus getPathStatus(const AfsPath& itemPath, bool followLeafShortcut) //throw SysError
    {
        return fileState_.getPathStatus(locationRootId_, itemPath, followLeafShortcut); //throw SysError
    }

    std::string /*itemId*/ getItemId(const AfsPath& itemPath, bool followLeafShortcut) //throw SysError
    {
        return fileState_.getItemId(locationRootId_, itemPath, followLeafShortcut); //throw SysError
    }

    std::pair<std::string /*itemId*/, GdriveItemDetails> getFileAttributes(const AfsPath& itemPath, bool followLeafShortcut) //throw SysError
    {
        return fileState_.getFileAttributes(locationRootId_, itemPath, followLeafShortcut); //throw SysError
    }

    GdriveFileState& all() { return fileState_; }

private:
    GdriveFileState& fileState_;
    const std::string locationRootId_;
};


class GdriveDrivesBuffer
{
public:
    explicit GdriveDrivesBuffer(GdriveAccessBuffer& accessBuf) :
        accessBuf_(accessBuf),
        myDrive_(getMyDriveId(accessBuf.getAccessToken()), Zstring() /*sharedDriveName*/, accessBuf) {} //throw SysError

    GdriveDrivesBuffer(MemoryStreamIn& stream, GdriveAccessBuffer& accessBuf) : //throw SysError
        accessBuf_(accessBuf),
        myDrive_(stream, accessBuf) //throw SysError
    {
        size_t sharedDrivesCount = readNumber<uint32_t>(stream); //SysErrorUnexpectedEos
        while (sharedDrivesCount-- != 0)
        {
            auto fileState = makeSharedRef<GdriveFileState>(stream, accessBuf); //throw SysError
            sharedDrives_.emplace(fileState.ref().getDriveId(), fileState);
        }
    }

    void serialize(MemoryStreamOut& stream) const
    {
        myDrive_.serialize(stream);

        writeNumber(stream, static_cast<uint32_t>(sharedDrives_.size()));
        for (const auto& [driveId, fileState] : sharedDrives_)
            fileState.ref().serialize(stream);

        //starredFolders_? no, will be fully restored by syncWithGoogle()
    }

    std::vector<Zstring /*locationName*/> listLocations() //throw SysError
    {
        if (syncIsDue())
            syncWithGoogle(); //throw SysError

        std::vector<Zstring> locationNames;

        for (const auto& [driveId, fileState] : sharedDrives_)
            locationNames.push_back(fileState.ref().getSharedDriveName());

        for (const StarredFolderDetails& sfd : starredFolders_)
            locationNames.push_back(sfd.folderName);

        return locationNames;
    }

    std::pair<GdriveFileStateAtLocation, GdriveFileState::FileStateDelta> prepareAccess(const Zstring& locationName) //throw SysError
    {
        //checking for added/renamed/deleted shared drives *every* GDRIVE_SYNC_INTERVAL is needlessly excessive!
        //  => check 1. once per FFS run
        //           2. on drive access error
        if (lastSyncTime_ == std::chrono::steady_clock::time_point())
            syncWithGoogle(); //throw SysError

        GdriveFileStateAtLocation fileState = [&]
        {
            try
            {
                return getFileState(locationName); //throw SysError
            }
            catch (SysError&)
            {
                if (syncIsDue())
                    syncWithGoogle(); //throw SysError

                return getFileState(locationName); //throw SysError
            }
        }();

        //manage last sync time here so that "lastSyncToken" remains stable while accessing GdriveFileState in the callback
        if (fileState.all().syncIsDue())
            fileState.all().syncWithGoogle(); //throw SysError

        return {fileState, fileState.all().registerFileStateDelta()};
    }

private:
    bool syncIsDue() const { return std::chrono::steady_clock::now() >= lastSyncTime_ + GDRIVE_SYNC_INTERVAL; }

    void syncWithGoogle() //throw SysError
    {
        //run in parallel with getSharedDrives()
        auto ftStarredFolders = runAsync([access = accessBuf_.getAccessToken() /*throw SysError*/] { return getStarredFolders(access); /*throw SysError*/ });

        decltype(sharedDrives_) currentDrives;

        //getSharedDrives() should be fast enough to avoid the unjustified complexity of change notifications: https://freefilesync.org/forum/viewtopic.php?t=7827&start=30#p29712
        for (const auto& [driveId, driveName] : getSharedDrives(accessBuf_.getAccessToken())) //throw SysError
        {
            auto fileState = [&, &driveId /*clang bug*/= driveId, &driveName /*clang bug*/= driveName]
            {
                if (auto it = sharedDrives_.find(driveId);
                    it != sharedDrives_.end())
                {
                    it->second.ref().setSharedDriveName(driveName);
                    return it->second;
                }
                else
                    return makeSharedRef<GdriveFileState>(driveId, driveName, accessBuf_); //throw SysError
            }();
            currentDrives.emplace(driveId, fileState);
        }

        starredFolders_ = ftStarredFolders.get(); //throw SysError //
        sharedDrives_.swap(currentDrives);                         //transaction!
        lastSyncTime_ = std::chrono::steady_clock::now(); //...(uhm, mostly, except for setSharedDriveName())
    }

    GdriveFileStateAtLocation getFileState(const Zstring& locationName) //throw SysError
    {
        if (locationName.empty())
            return {myDrive_, myDrive_.getDriveId()};

        GdriveFileState* fileState = nullptr;
        std::string locationRootId;

        for (auto& [driveId, fileStateRef] : sharedDrives_)
            if (equalNativePath(fileStateRef.ref().getSharedDriveName(), locationName))
            {
                if (fileState)
                    throw SysError(replaceCpy(_("The name %x is used by more than one item in the folder."), L"%x", fmtPath(locationName)));

                fileState = &fileStateRef.ref();
                locationRootId = driveId;
            }

        for (const StarredFolderDetails& sfd : starredFolders_)
            if (equalNativePath(sfd.folderName, locationName))
            {
                if (fileState)
                    throw SysError(replaceCpy(_("The name %x is used by more than one item in the folder."), L"%x", fmtPath(locationName)));

                if (sfd.sharedDriveId.empty()) //=> My Drive
                    fileState = &myDrive_;
                else
                {
                    auto it = sharedDrives_.find(sfd.sharedDriveId);
                    if (it == sharedDrives_.end())
                        break;

                    fileState = &it->second.ref();
                }
                locationRootId = sfd.folderId;
            }

        if (!fileState)
            throw SysError(replaceCpy(_("%x does not exist."), L"%x", fmtPath(locationName)));

        return {*fileState, locationRootId};
    }

    GdriveAccessBuffer& accessBuf_;
    std::chrono::steady_clock::time_point lastSyncTime_; //... with Google Drive (default: sync is due)

    GdriveFileState myDrive_;
    std::unordered_map<std::string /*drive ID*/, SharedRef<GdriveFileState>> sharedDrives_;

    std::vector<StarredFolderDetails> starredFolders_;
};

//==========================================================================================
//==========================================================================================

class GdrivePersistentSessions
{
public:
    explicit GdrivePersistentSessions(const Zstring& configDirPath) : configDirPath_(configDirPath)
    {
        onSystemShutdownRegister(onBeforeSystemShutdownCookie_);
    }

    void saveActiveSessions() //throw FileError
    {
        std::vector<Protected<SessionHolder>*> protectedSessions; //pointers remain stable, thanks to std::unordered_map<>
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

    std::string addUserSession(const std::string& gdriveLoginHint, const std::function<void()>& updateGui /*throw X*/, int timeoutSec) //throw SysError, X
    {
        const GdriveAccessInfo accessInfo = gdriveAuthorizeAccess(gdriveLoginHint, updateGui, timeoutSec); //throw SysError, X

        accessUserSession(accessInfo.userInfo.email, timeoutSec, [&](std::optional<UserSession>& userSession) //throw SysError
        {
            if (userSession)
                userSession->accessBuf.ref().update(accessInfo); //redundant?
            else
            {
                const std::shared_ptr<int> timeoutSec2 = std::make_shared<int>(timeoutSec); //context option: valid only for duration of this call!
                auto accessBuf = makeSharedRef<GdriveAccessBuffer>(accessInfo);
                accessBuf.ref().setContextTimeout(timeoutSec2); //[!] used by GdriveDrivesBuffer()!
                auto drivesBuf = makeSharedRef<GdriveDrivesBuffer>(accessBuf.ref()); //throw SysError
                userSession = {accessBuf, drivesBuf};
            }
        });

        return accessInfo.userInfo.email;
    }

    void removeUserSession(const std::string& accountEmail, int timeoutSec) //throw SysError
    {
        try
        {
            accessUserSession(accountEmail, timeoutSec, [&](std::optional<UserSession>& userSession) //throw SysError
            {
                if (userSession)
                    gdriveRevokeAccess(userSession->accessBuf.ref().getAccessToken()); //throw SysError
            });
        }
        catch (SysError&) { assert(false); } //best effort: try to invalidate the access token
        //=> expected to fail 1. if offline => not worse than removing FFS via "Uninstall Programs" 2. already revoked 3. if DB is corrupted

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
                if (itemExists(dbFilePath)) //throw FileError
                    throw;
            }
        }
        catch (const FileError& e) { throw SysError(replaceCpy(e.toString(), L"\n\n", L'\n')); } //file access errors should be further enriched by context info => SysError


        accessUserSession(accountEmail, timeoutSec, [&](std::optional<UserSession>& userSession) //throw SysError
        {
            userSession.reset();
        });
    }

    std::vector<std::string /*account email*/> listAccounts() //throw SysError
    {
        std::vector<std::string> emails;

        std::vector<Protected<SessionHolder>*> protectedSessions; //pointers remain stable, thanks to std::unordered_map<>
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
        try
        {
            traverseFolder(configDirPath_,
            [&](const    FileInfo& fi) { if (endsWith(fi.itemName, Zstr(".db"))) emails.push_back(utfTo<std::string>(beforeLast(fi.itemName, Zstr('.'), IfNotFoundReturn::none))); },
            [&](const  FolderInfo& fi) {},
            [&](const SymlinkInfo& si) {}); //throw FileError
        }
        catch (FileError&)
        {
            try
            {
                if (itemExists(configDirPath_)) //throw FileError
                    throw;
            }
            catch (const FileError& e) { throw SysError(replaceCpy(e.toString(), L"\n\n", L'\n')); } //file access errors should be further enriched by context info => SysError
        }

        removeDuplicates(emails, LessAsciiNoCase());
        return emails;
    }

    std::vector<Zstring /*locationName*/> listLocations(const std::string& accountEmail, int timeoutSec) //throw SysError
    {
        std::vector<Zstring> locationNames;

        accessUserSession(accountEmail, timeoutSec, [&](std::optional<UserSession>& userSession) //throw SysError
        {
            if (!userSession)
                throw SysError(replaceCpy(_("Please add a connection to user account %x first."), L"%x", utfTo<std::wstring>(accountEmail)));

            locationNames = userSession->drivesBuf.ref().listLocations(); //throw SysError
        });
        return locationNames;
    }

    struct AsyncAccessInfo
    {
        GdriveAccess access; //don't allow (long-running) web requests while holding the global session lock!
        GdriveFileState::FileStateDelta stateDelta;
    };
    //perf: amortized fully buffered!
    AsyncAccessInfo accessGlobalFileState(const GdriveLogin& login, const std::function<void(GdriveFileStateAtLocation& fileState)>& useFileState /*throw X*/) //throw SysError, X
    {
        GdriveAccess access;
        GdriveFileState::FileStateDelta stateDelta;

        accessUserSession(login.email, login.timeoutSec, [&](std::optional<UserSession>& userSession) //throw SysError
        {
            if (!userSession)
                throw SysError(replaceCpy(_("Please add a connection to user account %x first."), L"%x", utfTo<std::wstring>(login.email)));

            access                        = userSession->accessBuf.ref().getAccessToken(); //throw SysError
            auto [fileState, stateDelta2] = userSession->drivesBuf.ref().prepareAccess(login.locationName); //throw SysError
            stateDelta = std::move(stateDelta2);

            useFileState(fileState); //throw X
        });
        return {access, stateDelta};
    }

private:
    GdrivePersistentSessions           (const GdrivePersistentSessions&) = delete;
    GdrivePersistentSessions& operator=(const GdrivePersistentSessions&) = delete;

    struct UserSession;

    Zstring getDbFilePath(std::string accountEmail) const
    {
        for (char& c : accountEmail)
            c = asciiToLower(c);
        //return appendPath(configDirPath_, utfTo<Zstring>(formatAsHexString(getMd5(utfTo<std::string>(accountEmail)))) + Zstr(".db"));
        return appendPath(configDirPath_, utfTo<Zstring>(accountEmail) + Zstr(".db"));
    }

    void accessUserSession(const std::string& accountEmail, int timeoutSec, const std::function<void(std::optional<UserSession>& userSession)>& useSession /*throw X*/) //throw SysError, X
    {
        Protected<SessionHolder>* protectedSession = nullptr; //pointers remain stable, thanks to std::unordered_map<>
        globalSessions_.access([&](GlobalSessions& sessions) { protectedSession = &sessions[accountEmail]; });

        protectedSession->access([&](SessionHolder& holder)
        {
            if (!holder.dbWasLoaded) //let's NOT load the DB files under the globalSessions_ lock, but the session-specific one!
                try
                {
                    holder.session = loadSession(getDbFilePath(accountEmail), timeoutSec); //throw SysError
                }
                catch (const FileError& e) { throw SysError(replaceCpy(e.toString(), L"\n\n", L'\n')); } //GdrivePersistentSessions errors should be further enriched with context info => SysError
            holder.dbWasLoaded = true;

            const std::shared_ptr<int> timeoutSec2 = std::make_shared<int>(timeoutSec); //context option: valid only for duration of this call!
            if (holder.session)
                holder.session->accessBuf.ref().setContextTimeout(timeoutSec2);

            useSession(holder.session); //throw X
        });
    }

    static void saveSession(const Zstring& dbFilePath, const UserSession& userSession) //throw FileError
    {
        MemoryStreamOut streamOut;
        writeArray(streamOut, DB_FILE_DESCR, sizeof(DB_FILE_DESCR));
        writeNumber<int32_t>(streamOut, DB_FILE_VERSION);

        MemoryStreamOut streamOutBody;
        userSession.accessBuf.ref().serialize(streamOutBody);
        userSession.drivesBuf.ref().serialize(streamOutBody);

        try
        {
            streamOut.ref() += compress(streamOutBody.ref(), 3 /*best compression level: see db_file.cpp*/); //throw SysError
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(dbFilePath)), e.toString()); }

        setFileContent(dbFilePath, streamOut.ref(), nullptr /*notifyUnbufferedIO*/); //throw FileError
    }

    static std::optional<UserSession> loadSession(const Zstring& dbFilePath, int timeoutSec) //throw FileError
    {
        std::string byteStream;
        try
        {
            byteStream = getFileContent(dbFilePath, nullptr /*notifyUnbufferedIO*/); //throw FileError
        }
        catch (FileError&)
        {
            if (itemExists(dbFilePath)) //throw FileError
                throw;

            return std::nullopt;
        }

        try
        {
            MemoryStreamIn streamIn(byteStream);
            //-------- file format header --------
            char tmp[sizeof(DB_FILE_DESCR)] = {};
            readArray(streamIn, &tmp, sizeof(tmp)); //throw SysErrorUnexpectedEos

            const std::shared_ptr<int> timeoutSec2 = std::make_shared<int>(timeoutSec); //context option: valid only for duration of this call!

            //TODO: remove migration code at some time! 2020-07-03
            if (!std::equal(std::begin(tmp), std::end(tmp), std::begin(DB_FILE_DESCR)))
            {
                const std::string& uncompressedStream = decompress(byteStream); //throw SysError
                MemoryStreamIn streamIn2(uncompressedStream);
                //-------- file format header --------
                const char DB_FILE_DESCR_OLD[] = "FreeFileSync: Google Drive Database";
                char tmp2[sizeof(DB_FILE_DESCR_OLD)] = {};
                readArray(streamIn2, &tmp2, sizeof(tmp2)); //throw SysErrorUnexpectedEos

                if (!std::equal(std::begin(tmp2), std::end(tmp2), std::begin(DB_FILE_DESCR_OLD)))
                    throw SysError(_("File content is corrupted.") + L" (invalid header)");

                const int version = readNumber<int32_t>(streamIn2); //throw SysErrorUnexpectedEos
                if (version != 1 && //TODO: remove migration code at some time! 2019-12-05
                    version != 2 && //TODO: remove migration code at some time! 2020-06-11
                    version != 3)   //TODO: remove migration code at some time! 2020-07-03
                    throw SysError(_("Unsupported data format.") + L' ' + replaceCpy(_("Version: %x"), L"%x", numberTo<std::wstring>(version)));

                //version 1 + 2: fully discard old state due to missing "ownedByMe" attribute + shortcut support
                //version 3:     fully discard old state due to revamped shared drive handling
                auto accessBuf = makeSharedRef<GdriveAccessBuffer>(streamIn2); //throw SysError
                accessBuf.ref().setContextTimeout(timeoutSec2); //not used by GdriveDrivesBuffer(), but let's be consistent
                auto drivesBuf = makeSharedRef<GdriveDrivesBuffer>(accessBuf.ref()); //throw SysError
                return UserSession{accessBuf, drivesBuf};
            }
            else
            {
                if (!std::equal(std::begin(tmp), std::end(tmp), std::begin(DB_FILE_DESCR)))
                    throw SysError(_("File content is corrupted.") + L" (invalid header)");

                const int version = readNumber<int32_t>(streamIn); //throw SysErrorUnexpectedEos
                if (version != 4 &&
                    version != DB_FILE_VERSION)
                    throw SysError(_("Unsupported data format.") + L' ' + replaceCpy(_("Version: %x"), L"%x", numberTo<std::wstring>(version)));

                const std::string& uncompressedStream = decompress(makeStringView(byteStream.begin() + streamIn.pos(), byteStream.end())); //throw SysError
                MemoryStreamIn streamInBody(uncompressedStream);

                auto accessBuf = makeSharedRef<GdriveAccessBuffer>(streamInBody); //throw SysError
                accessBuf.ref().setContextTimeout(timeoutSec2); //not used by GdriveDrivesBuffer(), but let's be consistent
                auto drivesBuf = [&]
                {
                    //TODO: remove migration code at some time! 2021-05-15
                    if (version <= 4) //fully discard old state due to revamped shared drive handling
                        return makeSharedRef<GdriveDrivesBuffer>(accessBuf.ref()); //throw SysError
                    else
                        return makeSharedRef<GdriveDrivesBuffer>(streamInBody, accessBuf.ref()); //throw SysError
                }();

                return UserSession{accessBuf, drivesBuf};
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
        SharedRef<GdriveDrivesBuffer> drivesBuf;
    };

    struct SessionHolder
    {
        bool dbWasLoaded = false;
        std::optional<UserSession> session;
    };
    using GlobalSessions = std::unordered_map<std::string /*Google account email*/, Protected<SessionHolder>, StringHashAsciiNoCase, StringEqualAsciiNoCase>;

    Protected<GlobalSessions> globalSessions_;
    const Zstring configDirPath_;

    const SharedRef<std::function<void()>> onBeforeSystemShutdownCookie_ = makeSharedRef<std::function<void()>>([this]
    {
        try //let's not lose Google Drive data due to unexpected system shutdown:
        { saveActiveSessions(); } //throw FileError
        catch (const FileError& e) { logExtraError(e.toString()); }
    });
};
//==========================================================================================
constinit Global<GdrivePersistentSessions> globalGdriveSessions;
//==========================================================================================

GdrivePersistentSessions::AsyncAccessInfo accessGlobalFileState(const GdriveLogin& login, const std::function<void(GdriveFileStateAtLocation& fileState)>& useFileState /*throw X*/) //throw SysError, X
{
    if (const std::shared_ptr<GdrivePersistentSessions> gps = globalGdriveSessions.get())
        return gps->accessGlobalFileState(login, useFileState); //throw SysError, X

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
            const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(folderPath_.gdriveLogin, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                const auto& [itemId, itemDetails] = fileState.getFileAttributes(folderPath_.itemPath, true /*followLeafShortcut*/); //throw SysError

                if (itemDetails.type != GdriveItemType::folder) //check(!) or readFolderContent() will return empty (without failing!)
                    throw SysError(replaceCpy<std::wstring>(L"%x is not a directory.", L"%x", fmtPath(utfTo<Zstring>(itemDetails.itemName))));

                folderId      = itemId;
                childItemsBuf = fileState.all().tryGetBufferedFolderContent(folderId);
            });

            if (!childItemsBuf)
            {
                childItemsBuf = readFolderContent(folderId, aai.access); //throw SysError

                //buffer new file state ASAP => make sure accessGlobalFileState() has amortized constant access (despite the occasional internal readFolderContent() on non-leaf folders)
                accessGlobalFileState(folderPath_.gdriveLogin, [&](GdriveFileStateAtLocation& fileState) //throw SysError
                {
                    fileState.all().notifyFolderContent(aai.stateDelta, folderId, *childItemsBuf);
                });
            }

            for (const GdriveItem& item : *childItemsBuf)
                if (item.details.itemName.empty())
                    throw SysError(L"Folder contains an item without name."); //mostly an issue for FFS's folder traversal, but NOT for globalGdriveSessions!

            return {std::move(*childItemsBuf), folderPath_};
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
            const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(shortcutPath_.gdriveLogin, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                targetDetailsBuf = fileState.all().tryGetBufferedItemDetails(shortcutDetails_.targetId);
            });
            if (!targetDetailsBuf)
            {
                targetDetailsBuf = getItemDetails(shortcutDetails_.targetId, aai.access); //throw SysError

                //buffer new file state ASAP
                accessGlobalFileState(shortcutPath_.gdriveLogin, [&](GdriveFileStateAtLocation& fileState) //throw SysError
                {
                    fileState.all().notifyItemUpdated(aai.stateDelta, {shortcutDetails_.targetId, *targetDetailsBuf});
                });
            }

            assert(targetDetailsBuf->targetId.empty());
            if (targetDetailsBuf->type == GdriveItemType::shortcut) //should never happen: creating shortcuts to shortcuts fails with "Internal Error"
                throw SysError(L"Google Drive Shortcut points to another Shortcut.");

            return {std::move(*targetDetailsBuf), shortcutDetails_, shortcutPath_};
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
        const std::vector<GdriveItem>& childItems = GetDirDetails({gdriveLogin_, folderPath})().childItems; //throw FileError

        for (const GdriveItem& item : childItems)
        {
            const Zstring itemName = utfTo<Zstring>(item.details.itemName);

            switch (item.details.type)
            {
                case GdriveItemType::file:
                    cb.onFile({itemName, item.details.fileSize, item.details.modTime, getGdriveFilePrint(item.itemId), false /*isFollowedSymlink*/}); //throw X
                    break;

                case GdriveItemType::folder:
                    if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb.onFolder({itemName, false /*isFollowedSymlink*/})) //throw X
                    {
                        const AfsPath afsItemPath(appendPath(folderPath.value, itemName));
                        workload_.push_back({afsItemPath, std::move(cbSub)});
                    }
                    break;

                case GdriveItemType::shortcut:
                    switch (cb.onSymlink({itemName, item.details.modTime})) //throw X
                    {
                        case AFS::TraverserCallback::HandleLink::follow:
                        {
                            const AfsPath afsItemPath(appendPath(folderPath.value, itemName));

                            GdriveItemDetails targetDetails = {};
                            if (!tryReportingItemError([&] //throw X
                        {
                            targetDetails = GetShortcutTargetDetails({gdriveLogin_, afsItemPath}, item.details)().target; //throw FileError
                            }, cb, itemName))
                            continue;

                            if (targetDetails.type == GdriveItemType::folder)
                            {
                                if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb.onFolder({itemName, true /*isFollowedSymlink*/})) //throw X
                                    workload_.push_back({afsItemPath, std::move(cbSub)});
                            }
                            else //a file or named pipe, etc.
                                cb.onFile({itemName, targetDetails.fileSize, targetDetails.modTime, getGdriveFilePrint(item.details.targetId), true /*isFollowedSymlink*/}); //throw X
                        }
                        break;

                        case AFS::TraverserCallback::HandleLink::skip:
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

struct InputStreamGdrive : public AFS::InputStream
{
    explicit InputStreamGdrive(const GdrivePath& gdrivePath) :
        gdrivePath_(gdrivePath)
    {
        worker_ = InterruptibleThread([asyncStreamOut = this->asyncStreamIn_, gdrivePath]
        {
            setCurrentThreadName(Zstr("Istream ") + utfTo<Zstring>(getGdriveDisplayPath(gdrivePath)));
            try
            {
                GdriveAccess access;
                std::string fileId;
                try
                {
                    access = accessGlobalFileState(gdrivePath.gdriveLogin, [&](GdriveFileStateAtLocation& fileState) //throw SysError
                    {
                        fileId = fileState.getItemId(gdrivePath.itemPath, true /*followLeafShortcut*/); //throw SysError
                    }).access;
                }
                catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(getGdriveDisplayPath(gdrivePath))), e.toString()); }

                try
                {
                    auto writeBlock = [&](const void* buffer, size_t bytesToWrite)
                    {
                        asyncStreamOut->write(buffer, bytesToWrite); //throw ThreadStopRequest
                    };
                    gdriveDownloadFile(fileId, writeBlock, access); //throw SysError, ThreadStopRequest
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

    size_t getBlockSize() override { return GDRIVE_BLOCK_SIZE_DOWNLOAD; } //throw (FileError)

    //may return short; only 0 means EOF! CONTRACT: bytesToRead > 0!
    size_t tryRead(void* buffer, size_t bytesToRead, const IoCallback& notifyUnbufferedIO /*throw X*/) override //throw FileError, (ErrorFileLocked), X
    {
        const size_t bytesRead = asyncStreamIn_->tryRead(buffer, bytesToRead); //throw FileError
        reportBytesProcessed(notifyUnbufferedIO); //throw X
        return bytesRead;
        //no need for asyncStreamIn_->checkWriteErrors(): once end of stream is reached, asyncStreamOut->closeStream() was called => no errors occured
    }

    std::optional<AFS::StreamAttributes> tryGetAttributesFast() override //throw FileError
    {
        AFS::StreamAttributes attr = {};
        try
        {
            accessGlobalFileState(gdrivePath_.gdriveLogin, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                const auto& [itemId, itemDetails] = fileState.getFileAttributes(gdrivePath_.itemPath, true /*followLeafShortcut*/); //throw SysError
                attr.modTime  = itemDetails.modTime;
                attr.fileSize = itemDetails.fileSize;
                attr.filePrint = getGdriveFilePrint(itemId);
            });
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getGdriveDisplayPath(gdrivePath_))), e.toString()); }
        return std::move(attr); //[!]
    }

private:
    void reportBytesProcessed(const IoCallback& notifyUnbufferedIO /*throw X*/) //throw X
    {
        const int64_t bytesDelta = makeSigned(asyncStreamIn_->getTotalBytesWritten()) - totalBytesReported_;
        totalBytesReported_ += bytesDelta;
        if (notifyUnbufferedIO) notifyUnbufferedIO(bytesDelta); //throw X
    }

    const GdrivePath gdrivePath_;
    int64_t totalBytesReported_ = 0;
    std::shared_ptr<AsyncStreamBuffer> asyncStreamIn_ = std::make_shared<AsyncStreamBuffer>(GDRIVE_STREAM_BUFFER_SIZE);
    InterruptibleThread worker_;
};

//==========================================================================================

//already existing: 1. fails or 2. creates duplicate
struct OutputStreamGdrive : public AFS::OutputStreamImpl
{
    OutputStreamGdrive(const GdrivePath& gdrivePath,
                       std::optional<uint64_t> /*streamSize*/,
                       std::optional<time_t> modTime,
                       std::unique_ptr<PathAccessLock>&& pal) //throw SysError
    {
        std::promise<AFS::FingerPrint> promFilePrint;
        futFilePrint_ = promFilePrint.get_future();

        //CAVEAT: if file is already existing, OutputStreamGdrive *constructor* must fail, not OutputStreamGdrive::write(),
        //        otherwise ~OutputStreamImpl() will delete the already existing file! => don't check asynchronously!
        const Zstring fileName = AFS::getItemName(gdrivePath.itemPath);
        std::string parentId;
        /*const*/ GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdrivePath.gdriveLogin, [&](GdriveFileStateAtLocation& fileState) //throw SysError
        {
            const GdriveFileState::PathStatus& ps = fileState.getPathStatus(gdrivePath.itemPath, false /*followLeafShortcut*/); //throw SysError
            if (ps.relPath.empty())
                throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(fileName)));

            if (ps.relPath.size() > 1) //parent folder missing
                throw SysError(replaceCpy(_("%x does not exist."), L"%x", fmtPath(ps.relPath.front())));

            parentId = ps.existingItemId;
        });

        worker_ = InterruptibleThread([gdrivePath, modTime, fileName, asyncStreamIn = this->asyncStreamOut_,
                                                   pFilePrint = std::move(promFilePrint),
                                                   parentId   = std::move(parentId),
                                                   aai        = std::move(aai),
                                                   pal        = std::move(pal)]() mutable
        {
            assert(pal); //bind life time to worker thread!
            setCurrentThreadName(Zstr("Ostream ") + utfTo<Zstring>(getGdriveDisplayPath(gdrivePath)));
            try
            {
                auto tryReadBlock = [&](void* buffer, size_t bytesToRead) //may return short, only 0 means EOF!
                {
                    return asyncStreamIn->tryRead(buffer, bytesToRead); //throw ThreadStopRequest
                };
                //for whatever reason, gdriveUploadFile() is slightly faster than gdriveUploadSmallFile()! despite its two roundtrips! even when file sizes are 0!
                //=> 1. issue likely on Google's side => 2. persists even after having fixed "Expect: 100-continue"
                const std::string fileIdNew = //streamSize && *streamSize < 5 * 1024 * 1024 ?
                    //gdriveUploadSmallFile(fileName, parentId, *streamSize, modTime,    readBlock, aai.access) : //throw SysError, ThreadStopRequest
                    gdriveUploadFile       (fileName, parentId,              modTime, tryReadBlock, aai.access);  //throw SysError, ThreadStopRequest
                assert(asyncStreamIn->getTotalBytesRead() == asyncStreamIn->getTotalBytesWritten());
                //already existing: creates duplicate

                //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
                GdriveItem newFileItem
                {
                    .itemId = fileIdNew,
                    .details{
                        .itemName = fileName,
                        .fileSize = asyncStreamIn->getTotalBytesRead(),
                        .type = GdriveItemType::file,
                        .owner = FileOwner::me,
                    }
                };
                if (modTime) //else: whatever modTime Google Drive selects will be notified after GDRIVE_SYNC_INTERVAL
                    newFileItem.details.modTime = *modTime;
                newFileItem.details.parentIds.push_back(parentId);

                accessGlobalFileState(gdrivePath.gdriveLogin, [&](GdriveFileStateAtLocation& fileState) //throw SysError
                {
                    fileState.all().notifyItemCreated(aai.stateDelta, newFileItem);
                });

                pFilePrint.set_value(getGdriveFilePrint(fileIdNew));
            }
            catch (const SysError& e)
            {
                FileError fe(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getGdriveDisplayPath(gdrivePath))), e.toString());
                const std::exception_ptr exptr = std::make_exception_ptr(std::move(fe));
                asyncStreamIn->setReadError(exptr); //set both!
                pFilePrint.set_exception(exptr);    //
            }
            //let ThreadStopRequest pass through!
        });
    }

    ~OutputStreamGdrive()
    {
        if (asyncStreamOut_) //finalize() was not called (successfully)
            asyncStreamOut_->setWriteError(std::make_exception_ptr(ThreadStopRequest()));
    }

    size_t getBlockSize() override { return GDRIVE_BLOCK_SIZE_UPLOAD; } //throw (FileError)

    size_t tryWrite(const void* buffer, size_t bytesToWrite, const IoCallback& notifyUnbufferedIO /*throw X*/) override //throw FileError, X; may return short! CONTRACT: bytesToWrite > 0
    {
        const size_t bytesWritten = asyncStreamOut_->tryWrite(buffer, bytesToWrite); //throw FileError
        reportBytesProcessed(notifyUnbufferedIO); //throw X
        return bytesWritten;
    }

    AFS::FinalizeResult finalize(const IoCallback& notifyUnbufferedIO /*throw X*/) override //throw FileError, X
    {
        if (!asyncStreamOut_)
            throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

        asyncStreamOut_->closeStream();

        while (futFilePrint_.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout)
            reportBytesProcessed(notifyUnbufferedIO); //throw X
        reportBytesProcessed(notifyUnbufferedIO); //[!] once more, now that *all* bytes were written

        AFS::FinalizeResult result;
        assert(isReady(futFilePrint_));
        result.filePrint = futFilePrint_.get(); //throw FileError

        //asyncStreamOut_->checkReadErrors(); //throw FileError -> not needed after *successful* upload
        asyncStreamOut_.reset(); //do NOT reset on error, so that ~OutputStreamGdrive() will request worker thread to stop
        //--------------------------------------------------------------------

        //result.errorModTime -> already (successfully) set during file creation
        return result;
    }

private:
    void reportBytesProcessed(const IoCallback& notifyUnbufferedIO /*throw X*/) //throw X
    {
        const int64_t bytesDelta = makeSigned(asyncStreamOut_->getTotalBytesRead()) - totalBytesReported_;
        totalBytesReported_ += bytesDelta;
        if (notifyUnbufferedIO) notifyUnbufferedIO(bytesDelta); //throw X
    }

    int64_t totalBytesReported_ = 0;
    std::shared_ptr<AsyncStreamBuffer> asyncStreamOut_ = std::make_shared<AsyncStreamBuffer>(GDRIVE_STREAM_BUFFER_SIZE);
    InterruptibleThread worker_;
    std::future<AFS::FingerPrint> futFilePrint_;
};

//==========================================================================================

class GdriveFileSystem : public AbstractFileSystem
{
public:
    explicit GdriveFileSystem(const GdriveLogin& gdriveLogin) : gdriveLogin_(gdriveLogin) {}

    const GdriveLogin& getGdriveLogin() const { return gdriveLogin_; }

    Zstring getFolderUrl(const AfsPath& folderPath) const //throw FileError
    {
        try
        {
            GdriveFileState::PathStatus ps;
            accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                ps = fileState.getPathStatus(folderPath, true /*followLeafShortcut*/); //throw SysError
            });

            if (!ps.relPath.empty())
                throw SysError(replaceCpy(_("%x does not exist."), L"%x", fmtPath(ps.relPath.front())));

            if (ps.existingType != GdriveItemType::folder)
                throw SysError(replaceCpy<std::wstring>(L"%x is not a folder.", L"%x", fmtPath(getItemName(folderPath))));

            return Zstr("https://drive.google.com/drive/folders/") + utfTo<Zstring>(ps.existingItemId);
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(getDisplayPath(folderPath))), e.toString()); }
    }

private:
    GdrivePath getGdrivePath(const AfsPath& itemPath) const { return {gdriveLogin_, itemPath}; }

    GdriveRawPath getGdriveRawPath(const AfsPath& itemPath) const //throw SysError
    {
        const std::optional<AfsPath> parentPath = getParentPath(itemPath);
        if (!parentPath)
            throw SysError(L"Item is device root");

        std::string parentId;
        accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
        {
            parentId = fileState.getItemId(*parentPath, true /*followLeafShortcut*/); //throw SysError
        });
        return { std::move(parentId), getItemName(itemPath)};
    }

    Zstring getInitPathPhrase(const AfsPath& itemPath) const override { return concatenateGdriveFolderPathPhrase(getGdrivePath(itemPath)); }

    std::vector<Zstring> getPathPhraseAliases(const AfsPath& itemPath) const override { return {getInitPathPhrase(itemPath)}; }

    std::wstring getDisplayPath(const AfsPath& itemPath) const override { return getGdriveDisplayPath(getGdrivePath(itemPath)); }

    bool isNullFileSystem() const override { return gdriveLogin_.email.empty(); }

    std::weak_ordering compareDeviceSameAfsType(const AbstractFileSystem& afsRhs) const override
    {
        const GdriveLogin& lhs = gdriveLogin_;
        const GdriveLogin& rhs = static_cast<const GdriveFileSystem&>(afsRhs).gdriveLogin_;

        if (const std::weak_ordering cmp = compareAsciiNoCase(lhs.email, rhs.email);
            cmp != std::weak_ordering::equivalent)
            return cmp;

        return compareNativePath(lhs.locationName, rhs.locationName);
    }

    //----------------------------------------------------------------------------------------------------------------
    ItemType getItemType(const AfsPath& itemPath) const override //throw FileError
    {
        try
        {
            GdriveFileState::PathStatus ps;
            accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                ps = fileState.getPathStatus(itemPath, false /*followLeafShortcut*/); //throw SysError
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

            throw SysError(replaceCpy(_("%x does not exist."), L"%x", fmtPath(Zstring(ps.relPath.front()))));
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getDisplayPath(itemPath))), e.toString()); }
    }

    std::optional<ItemType> getItemTypeIfExists(const AfsPath& itemPath) const override //throw FileError
    {
        try
        {
            GdriveFileState::PathStatus ps;
            accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                ps = fileState.getPathStatus(itemPath, false /*followLeafShortcut*/); //throw SysError
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
            return std::nullopt;
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getDisplayPath(itemPath))), e.toString()); }
    }

    //----------------------------------------------------------------------------------------------------------------
    //already existing: 1. fails or 2. creates duplicate (unlikely)
    void createFolderPlain(const AfsPath& folderPath) const override //throw FileError
    {
        try
        {
            //avoid duplicate Google Drive item creation by multiple threads
            PathAccessLock pal(getGdriveRawPath(folderPath), PathBlockType::otherWait); //throw SysError

            const Zstring folderName = getItemName(folderPath);
            std::string parentId;
            const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                const GdriveFileState::PathStatus& ps = fileState.getPathStatus(folderPath, false /*followLeafShortcut*/); //throw SysError
                if (ps.relPath.empty())
                    throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(folderName)));

                if (ps.relPath.size() > 1) //parent folder missing
                    throw SysError(replaceCpy(_("%x does not exist."), L"%x", fmtPath(ps.relPath.front())));

                parentId = ps.existingItemId;
            });

            //already existing: creates duplicate
            const std::string folderIdNew = gdriveCreateFolderPlain(folderName, parentId, aai.access); //throw SysError

            //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
            accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                fileState.all().notifyFolderCreated(aai.stateDelta, folderIdNew, folderName, parentId);
            });
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(getDisplayPath(folderPath))), e.toString()); }
    }

    void removeItemPlainImpl(const AfsPath& itemPath, std::optional<GdriveItemType> expectedType, bool permanent /*...or move to trash*/, bool failIfNotExist) const //throw SysError
    {
        const std::optional<AfsPath> parentPath = getParentPath(itemPath);
        if (!parentPath) throw SysError(L"Item is device root");

        std::string itemId;
        std::optional<std::string> parentIdToUnlink;
        const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
        {
            const GdriveFileState::PathStatus ps = fileState.getPathStatus(itemPath, false /*followLeafShortcut*/); //throw SysError
            if (!ps.relPath.empty())
            {
                if (failIfNotExist)
                    throw SysError(replaceCpy(_("%x does not exist."), L"%x", fmtPath(ps.relPath.front())));
                else
                    return;
            }

            GdriveItemDetails itemDetails;
            std::tie(itemId, itemDetails) = fileState.getFileAttributes(itemPath, false /*followLeafShortcut*/); //throw SysError
            assert(std::find(itemDetails.parentIds.begin(), itemDetails.parentIds.end(), fileState.getItemId(*parentPath, true /*followLeafShortcut*/)) != itemDetails.parentIds.end());

            if (expectedType && itemDetails.type != *expectedType)
                switch (*expectedType)
                {
                    //*INDENT-OFF*
                    case GdriveItemType::file:     throw SysError(L"Item is not a file");
                    case GdriveItemType::folder:   throw SysError(L"Item is not a folder");
                    case GdriveItemType::shortcut: throw SysError(L"Item is not a shortcut");
                    //*INDENT-ON*
                }

            //hard-link handling applies to shared files as well: 1. it's the right thing (TM) 2. if we're not the owner: deleting would fail
            if (itemDetails.parentIds.size() > 1 || itemDetails.owner == FileOwner::other) //FileOwner::other behaves like a followed symlink! i.e. vanishes if owner deletes it!
                parentIdToUnlink = fileState.getItemId(*parentPath, true /*followLeafShortcut*/); //throw SysError
        });
        if (itemId.empty())
            return;

        if (parentIdToUnlink)
        {
            gdriveUnlinkParent(itemId, *parentIdToUnlink, aai.access); //throw SysError

            //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
            accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                fileState.all().notifyParentRemoved(aai.stateDelta, itemId, *parentIdToUnlink);
            });
        }
        else
        {
            if (permanent)
                gdriveDeleteItem(itemId, aai.access); //throw SysError
            else
                gdriveMoveToTrash(itemId, aai.access); //throw SysError

            //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
            accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                fileState.all().notifyItemDeleted(aai.stateDelta, itemId);
            });
        }
    }

    void removeFilePlain(const AfsPath& filePath) const override //throw FileError
    {
        try { removeItemPlainImpl(filePath, GdriveItemType::file, true /*permanent*/, false /*failIfNotExist*/); /*throw SysError*/ }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete file %x."), L"%x", fmtPath(getDisplayPath(filePath))), e.toString()); }
    }

    void removeSymlinkPlain(const AfsPath& linkPath) const override //throw FileError
    {
        try { removeItemPlainImpl(linkPath, GdriveItemType::shortcut, true /*permanent*/, false /*failIfNotExist*/); /*throw SysError*/ }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete symbolic link %x."), L"%x", fmtPath(getDisplayPath(linkPath))), e.toString()); }
    }

    void removeFolderPlain(const AfsPath& folderPath) const override //throw FileError
    {
        try { removeItemPlainImpl(folderPath, GdriveItemType::folder, true /*permanent*/, false /*failIfNotExist*/); /*throw SysError*/ }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(getDisplayPath(folderPath))), e.toString()); }
    }

    void removeFolderIfExistsRecursion(const AfsPath& folderPath, //throw FileError
                                       const std::function<void(const std::wstring& displayPath)>& onBeforeFileDeletion   /*throw X*/,
                                       const std::function<void(const std::wstring& displayPath)>& onBeforeSymlinkDeletion/*throw X*/,
                                       const std::function<void(const std::wstring& displayPath)>& onBeforeFolderDeletion /*throw X*/) const override
    {
        if (onBeforeFolderDeletion) onBeforeFolderDeletion(getDisplayPath(folderPath)); //throw X

        try { removeItemPlainImpl(folderPath, GdriveItemType::folder, true /*permanent*/, false /*failIfNotExist*/); /*throw SysError*/ }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(getDisplayPath(folderPath))), e.toString()); }
    }

    //----------------------------------------------------------------------------------------------------------------
    AbstractPath getSymlinkResolvedPath(const AfsPath& linkPath) const override //throw FileError
    {
        //this function doesn't make sense for Google Drive: Shortcuts do not refer by path, but ID!
        //even if it were possible to determine a path, doing anything with the target file (e.g. delete + recreate) would break other Shortcuts!
        throw FileError(replaceCpy(_("Cannot determine final path for %x."), L"%x", fmtPath(getDisplayPath(linkPath))), _("Operation not supported by device."));
    }

    bool equalSymlinkContentForSameAfsType(const AfsPath& linkPathL, const AbstractPath& linkPathR) const override //throw FileError
    {
        auto getTargetId = [](const GdriveFileSystem& gdriveFs, const AfsPath& linkPath)
        {
            try
            {
                std::string targetId;
                const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdriveFs.gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
                {
                    const GdriveItemDetails& itemDetails = fileState.getFileAttributes(linkPath, false /*followLeafShortcut*/).second; //throw SysError
                    if (itemDetails.type != GdriveItemType::shortcut)
                        throw SysError(L"Not a Google Drive Shortcut.");

                    targetId = itemDetails.targetId;
                });
                return targetId;
            }
            catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(gdriveFs.getDisplayPath(linkPath))), e.toString()); }
        };

        return getTargetId(*this, linkPathL) == getTargetId(static_cast<const GdriveFileSystem&>(linkPathR.afsDevice.ref()), linkPathR.afsPath);
    }

    //----------------------------------------------------------------------------------------------------------------

    //return value always bound:
    std::unique_ptr<InputStream> getInputStream(const AfsPath& filePath) const override //throw FileError, (ErrorFileLocked)
    {
        return std::make_unique<InputStreamGdrive>(getGdrivePath(filePath));
    }

    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    //=> actual behavior: 1. fails or 2. creates duplicate (unlikely)
    std::unique_ptr<OutputStreamImpl> getOutputStream(const AfsPath& filePath, //throw FileError
                                                      std::optional<uint64_t> streamSize,
                                                      std::optional<time_t> modTime) const override
    {
        try
        {
            //avoid duplicate item creation by multiple threads
            auto pal = std::make_unique<PathAccessLock>(getGdriveRawPath(filePath), PathBlockType::otherFail); //throw SysError
            //don't block during a potentially long-running file upload!

            //already existing: 1. fails or 2. creates duplicate
            return std::make_unique<OutputStreamGdrive>(getGdrivePath(filePath), streamSize, modTime, std::move(pal)); //throw SysError
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getDisplayPath(filePath))), e.toString());
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
    FileCopyResult copyFileForSameAfsType(const AfsPath& sourcePath, const StreamAttributes& attrSource, //throw FileError, (ErrorFileLocked), (X)
                                          const AbstractPath& targetPath, bool copyFilePermissions, const IoCallback& notifyUnbufferedIO /*throw X*/) const override
    {
        //no native Google Drive file copy => use stream-based file copy:
        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(AFS::getDisplayPath(targetPath))), _("Operation not supported by device."));

        const GdriveFileSystem& fsTarget = static_cast<const GdriveFileSystem&>(targetPath.afsDevice.ref());

        if (!equalAsciiNoCase(gdriveLogin_.email, fsTarget.gdriveLogin_.email))
            //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
            //=> actual behavior: 1. fails or 2. creates duplicate (unlikely)
            return copyFileAsStream(sourcePath, attrSource, targetPath, notifyUnbufferedIO); //throw FileError, (ErrorFileLocked), X
        //else: copying files within account works, e.g. between My Drive <-> shared drives

        try
        {
            //avoid duplicate Google Drive item creation by multiple threads (blocking is okay: gdriveCopyFile() should complete instantly!)
            PathAccessLock pal(fsTarget.getGdriveRawPath(targetPath.afsPath), PathBlockType::otherWait); //throw SysError

            const Zstring itemNameNew = getItemName(targetPath);
            std::string itemIdSrc;
            GdriveItemDetails itemDetailsSrc;
            /*const GdrivePersistentSessions::AsyncAccessInfo aaiSrc =*/ accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                std::tie(itemIdSrc, itemDetailsSrc) = fileState.getFileAttributes(sourcePath, true /*followLeafShortcut*/); //throw SysError

                assert(itemDetailsSrc.type == GdriveItemType::file); //Google Drive *should* fail trying to copy folder: "This file cannot be copied by the user."
                if (itemDetailsSrc.type != GdriveItemType::file)     //=> don't trust + improve error message
                    throw SysError(replaceCpy<std::wstring>(L"%x is not a file.", L"%x", fmtPath(getItemName(sourcePath))));
            });

            std::string parentIdTrg;
            const GdrivePersistentSessions::AsyncAccessInfo aaiTrg = accessGlobalFileState(fsTarget.gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                const GdriveFileState::PathStatus psTo = fileState.getPathStatus(targetPath.afsPath, false /*followLeafShortcut*/); //throw SysError
                if (psTo.relPath.empty())
                    throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(itemNameNew)));

                if (psTo.relPath.size() > 1) //parent folder missing
                    throw SysError(replaceCpy(_("%x does not exist."), L"%x", fmtPath(psTo.relPath.front())));

                parentIdTrg = psTo.existingItemId;
            });

            //already existing: creates duplicate
            const std::string fileIdTrg = gdriveCopyFile(itemIdSrc, parentIdTrg, itemNameNew, itemDetailsSrc.modTime, aaiTrg.access); //throw SysError

            //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
            accessGlobalFileState(fsTarget.gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                const GdriveItem newFileItem
                {
                    .itemId = fileIdTrg,
                    .details{
                        .itemName = itemNameNew,
                        .fileSize = itemDetailsSrc.fileSize,
                        .modTime = itemDetailsSrc.modTime,
                        .type = GdriveItemType::file,
                        .owner = fileState.all().getSharedDriveName().empty() ? FileOwner::me : FileOwner::none,
                        .parentIds{parentIdTrg},
                    }
                };
                fileState.all().notifyItemCreated(aaiTrg.stateDelta, newFileItem);
            });

            return
            {
                .fileSize = itemDetailsSrc.fileSize,
                .modTime  = itemDetailsSrc.modTime,
                .sourceFilePrint = getGdriveFilePrint(itemIdSrc),
                .targetFilePrint = getGdriveFilePrint(fileIdTrg),
                /*.errorModTime = */
            };
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(replaceCpy(_("Cannot copy file %x to %y."),
                                                  L"%x", L'\n' + fmtPath(getDisplayPath(sourcePath))),
                                       L"%y",  L'\n' + fmtPath(AFS::getDisplayPath(targetPath))), e.toString());
        }
    }

    //symlink handling: follow
    //already existing: fail
    void copyNewFolderForSameAfsType(const AfsPath& sourcePath, const AbstractPath& targetPath, bool copyFilePermissions) const override //throw FileError
    {
        //already existing: 1. fails or 2. creates duplicate (unlikely)
        AFS::createFolderPlain(targetPath); //throw FileError

        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(AFS::getDisplayPath(targetPath))), _("Operation not supported by device."));
    }

    //already existing: fail
    void copySymlinkForSameAfsType(const AfsPath& sourcePath, const AbstractPath& targetPath, bool copyFilePermissions) const override //throw FileError
    {
        try
        {
            std::string targetId;
            accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                const GdriveItemDetails& itemDetails = fileState.getFileAttributes(sourcePath, false /*followLeafShortcut*/).second; //throw SysError
                if (itemDetails.type != GdriveItemType::shortcut)
                    throw SysError(L"Not a Google Drive Shortcut.");

                targetId = itemDetails.targetId;
            });

            const GdriveFileSystem& fsTarget = static_cast<const GdriveFileSystem&>(targetPath.afsDevice.ref());

            //avoid duplicate Google Drive item creation by multiple threads
            PathAccessLock pal(fsTarget.getGdriveRawPath(targetPath.afsPath), PathBlockType::otherWait); //throw SysError

            const Zstring shortcutName = getItemName(targetPath.afsPath);
            std::string parentId;
            const GdrivePersistentSessions::AsyncAccessInfo aaiTrg = accessGlobalFileState(fsTarget.gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                const GdriveFileState::PathStatus& ps = fileState.getPathStatus(targetPath.afsPath, false /*followLeafShortcut*/); //throw SysError
                if (ps.relPath.empty())
                    throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(shortcutName)));

                if (ps.relPath.size() > 1) //parent folder missing
                    throw SysError(replaceCpy(_("%x does not exist."), L"%x", fmtPath(ps.relPath.front())));

                parentId = ps.existingItemId;
            });

            //already existing: creates duplicate
            const std::string shortcutIdNew = gdriveCreateShortcutPlain(shortcutName, parentId, targetId, aaiTrg.access); //throw SysError

            //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
            accessGlobalFileState(fsTarget.gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                fileState.all().notifyShortcutCreated(aaiTrg.stateDelta, shortcutIdNew, shortcutName, parentId, targetId);
            });
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."),
                                                  L"%x", L'\n' + fmtPath(getDisplayPath(sourcePath))),
                                       L"%y",  L'\n' + fmtPath(AFS::getDisplayPath(targetPath))), e.toString());
        }
    }

    //already existing: undefined behavior! (e.g. fail/overwrite)
    //=> actual behavior: 1. fails or 2. creates duplicate (unlikely)
    void moveAndRenameItemForSameAfsType(const AfsPath& pathFrom, const AbstractPath& pathTo) const override //throw FileError, ErrorMoveUnsupported
    {
        if (compareDeviceSameAfsType(pathTo.afsDevice.ref()) != std::weak_ordering::equivalent)
            throw ErrorMoveUnsupported(generateMoveErrorMsg(pathFrom, pathTo), _("Operation not supported between different devices."));
        //note: moving files within account works, e.g. between My Drive <-> shared drives
        //      BUT: not supported by our model with separate GdriveFileStates; e.g. how to handle complexity of a moved folder (tree)?
        try
        {
            const GdriveFileSystem& fsTarget = static_cast<const GdriveFileSystem&>(pathTo.afsDevice.ref());

            //avoid duplicate Google Drive item creation by multiple threads
            PathAccessLock pal(fsTarget.getGdriveRawPath(pathTo.afsPath), PathBlockType::otherWait); //throw SysError

            const Zstring itemNameOld = getItemName(pathFrom);
            const Zstring itemNameNew = getItemName(pathTo);
            const std::optional<AfsPath> parentPathFrom = getParentPath(pathFrom);
            const std::optional<AfsPath> parentPathTo   = getParentPath(pathTo.afsPath);
            if (!parentPathFrom) throw SysError(L"Source is device root");
            if (!parentPathTo  ) throw SysError(L"Target is device root");

            std::string itemId;
            GdriveItemDetails itemDetails;
            std::string parentIdFrom;
            std::string parentIdTo;
            const GdrivePersistentSessions::AsyncAccessInfo aai = accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                std::tie(itemId, itemDetails) = fileState.getFileAttributes(pathFrom, false /*followLeafShortcut*/); //throw SysError

                parentIdFrom = fileState.getItemId(*parentPathFrom, true /*followLeafShortcut*/); //throw SysError

                const GdriveFileState::PathStatus psTo = fileState.getPathStatus(pathTo.afsPath, false /*followLeafShortcut*/); //throw SysError

                //e.g. changing file name case only => this is not an "already exists" situation!
                //also: hardlink referenced by two different paths, the source one will be unlinked
                if (psTo.relPath.empty() && psTo.existingItemId == itemId)
                    parentIdTo = fileState.getItemId(*parentPathTo, true /*followLeafShortcut*/); //throw SysError
                else
                {
                    if (psTo.relPath.empty())
                        throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(itemNameNew)));

                    if (psTo.relPath.size() > 1) //parent folder missing
                        throw SysError(replaceCpy(_("%x does not exist."), L"%x", fmtPath(psTo.relPath.front())));

                    parentIdTo = psTo.existingItemId;
                }
            });

            if (parentIdFrom == parentIdTo && itemNameOld == itemNameNew)
                return; //nothing to do

            //already existing: creates duplicate
            gdriveMoveAndRenameItem(itemId, parentIdFrom, parentIdTo, itemNameNew, itemDetails.modTime, aai.access); //throw SysError

            //buffer new file state ASAP (don't wait GDRIVE_SYNC_INTERVAL)
            accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState) //throw SysError
            {
                fileState.all().notifyMoveAndRename(aai.stateDelta, itemId, parentIdFrom, parentIdTo, itemNameNew);
            });
        }
        catch (const SysError& e) { throw FileError(generateMoveErrorMsg(pathFrom, pathTo), e.toString()); }
    }

    bool supportsPermissions(const AfsPath& folderPath) const override { return false; } //throw FileError

    //----------------------------------------------------------------------------------------------------------------
    FileIconHolder getFileIcon      (const AfsPath& filePath, int pixelSize) const override { return {}; } //throw FileError; optional return value
    ImageHolder    getThumbnailImage(const AfsPath& filePath, int pixelSize) const override { return {}; } //throw FileError; optional return value

    void authenticateAccess(const RequestPasswordFun& requestPassword /*throw X*/) const override //throw FileError, (X)
    {
        try
        {
            const std::shared_ptr<GdrivePersistentSessions> gps = globalGdriveSessions.get();
            if (!gps)
                throw SysError(formatSystemError("GdriveFileSystem::authenticateAccess", L"", L"Function call not allowed during init/shutdown."));

            for (const std::string& accountEmail : gps->listAccounts()) //throw SysError
                if (equalAsciiNoCase(accountEmail, gdriveLogin_.email))
                    return;

            const bool allowUserInteraction = static_cast<bool>(requestPassword);
            if (allowUserInteraction)
                gps->addUserSession(gdriveLogin_.email /*gdriveLoginHint*/, nullptr /*updateGui*/, gdriveLogin_.timeoutSec); //throw SysError
            //error messages will be lost if user cancels in dir_exist_async.h! However:
            //The most-likely-to-fail parts (web access) are reported by gdriveAuthorizeAccess() via the browser!
            else
                throw SysError(replaceCpy(_("Please add a connection to user account %x first."), L"%x", utfTo<std::wstring>(gdriveLogin_.email)));
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to connect to %x."), L"%x", fmtPath(getDisplayPath(AfsPath()))), e.toString()); }
    }

    bool hasNativeTransactionalCopy() const override { return true; }
    //----------------------------------------------------------------------------------------------------------------

    int64_t getFreeDiskSpace(const AfsPath& folderPath) const override //throw FileError, returns < 0 if not available
    {
        bool onMyDrive = false;
        try
        {
            const GdriveAccess& access = accessGlobalFileState(gdriveLogin_, [&](GdriveFileStateAtLocation& fileState)
            { onMyDrive = fileState.all().getSharedDriveName().empty(); }).access; //throw SysError

            if (onMyDrive)
                return gdriveGetMyDriveFreeSpace(access); //throw SysError
            else
                return -1;
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot determine free disk space for %x."), L"%x", fmtPath(getDisplayPath(folderPath))), e.toString()); }
    }

    std::unique_ptr<RecycleSession> createRecyclerSession(const AfsPath& folderPath) const override //throw FileError, (RecycleBinUnavailable)
    {
        struct RecycleSessionGdrive : public RecycleSession
        {
            //fails if item is not existing
            void moveToRecycleBin(const AbstractPath& itemPath, const Zstring& logicalRelPath) override { AFS::moveToRecycleBin(itemPath); } //throw FileError, (RecycleBinUnavailable)
            void tryCleanup(const std::function<void(const std::wstring& displayPath)>& notifyDeletionStatus) override {}; //throw FileError
        };

        return std::make_unique<RecycleSessionGdrive>();
    }

    //fails if item is not existing
    void moveToRecycleBin(const AfsPath& itemPath) const override //throw FileError, (RecycleBinUnavailable)
    {
        try
        {
            removeItemPlainImpl(itemPath, std::nullopt /*expectedType*/, false /*permanent*/, true /*failIfNotExist*/); //throw SysError
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to move %x to the recycle bin."), L"%x", fmtPath(getDisplayPath(itemPath))), e.toString()); }
    }

    const GdriveLogin gdriveLogin_;
};
//===========================================================================================================================

//expects "clean" input data
Zstring concatenateGdriveFolderPathPhrase(const GdrivePath& gdrivePath) //noexcept
{
    Zstring emailAndDrive = utfTo<Zstring>(gdrivePath.gdriveLogin.email);
    if (!gdrivePath.gdriveLogin.locationName.empty())
        emailAndDrive += Zstr(':') + gdrivePath.gdriveLogin.locationName;

    Zstring options;
    if (gdrivePath.gdriveLogin.timeoutSec != GdriveLogin().timeoutSec)
        options += Zstr("|timeout=") + numberTo<Zstring>(gdrivePath.gdriveLogin.timeoutSec);

    Zstring itemPath;
    if (!gdrivePath.itemPath.value.empty())
        itemPath += FILE_NAME_SEPARATOR + gdrivePath.itemPath.value;

    if (endsWith(itemPath, Zstr(' ')) && options.empty()) //path phrase concept must survive trimming!
        itemPath += FILE_NAME_SEPARATOR;

    return Zstring(gdrivePrefix) + FILE_NAME_SEPARATOR + emailAndDrive + itemPath + options;
}
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
    catch (const FileError& e) { logExtraError(e.toString()); }

    assert(globalGdriveSessions.get());
    globalGdriveSessions.set(nullptr);

    assert(globalHttpSessionManager.get());
    globalHttpSessionManager.set(nullptr);
}


std::string fff::gdriveAddUser(const std::function<void()>& updateGui /*throw X*/, int timeoutSec) //throw FileError, X
{
    try
    {
        if (const std::shared_ptr<GdrivePersistentSessions> gps = globalGdriveSessions.get())
            return gps->addUserSession("" /*gdriveLoginHint*/, updateGui, timeoutSec); //throw SysError, X

        throw SysError(formatSystemError("gdriveAddUser", L"", L"Function call not allowed during init/shutdown."));
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to connect to %x."), L"%x", L"Google Drive"), e.toString()); }
}


void fff::gdriveRemoveUser(const std::string& accountEmail, int timeoutSec) //throw FileError
{
    try
    {
        if (const std::shared_ptr<GdrivePersistentSessions> gps = globalGdriveSessions.get())
            return gps->removeUserSession(accountEmail, timeoutSec); //throw SysError

        throw SysError(formatSystemError("gdriveRemoveUser", L"", L"Function call not allowed during init/shutdown."));
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to disconnect from %x."), L"%x", fmtPath(getGdriveDisplayPath({{accountEmail, Zstr("")}, AfsPath()}))), e.toString()); }
}


std::vector<std::string /*account email*/> fff::gdriveListAccounts() //throw FileError
{
    try
    {
        if (const std::shared_ptr<GdrivePersistentSessions> gps = globalGdriveSessions.get())
            return gps->listAccounts(); //throw SysError

        throw SysError(formatSystemError("gdriveListAccounts", L"", L"Function call not allowed during init/shutdown."));
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to connect to %x."), L"%x", L"Google Drive"), e.toString()); }
}


std::vector<Zstring /*locationName*/> fff::gdriveListLocations(const std::string& accountEmail, int timeoutSec) //throw FileError
{
    try
    {
        if (const std::shared_ptr<GdrivePersistentSessions> gps = globalGdriveSessions.get())
            return gps->listLocations(accountEmail, timeoutSec); //throw SysError

        throw SysError(formatSystemError("gdriveListLocations", L"", L"Function call not allowed during init/shutdown."));
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to connect to %x."), L"%x", fmtPath(getGdriveDisplayPath({{accountEmail, Zstr("")}, AfsPath()}))), e.toString()); }
}


AfsDevice fff::condenseToGdriveDevice(const GdriveLogin& login) //noexcept
{
    //clean up input:
    GdriveLogin loginTmp = login;
    trim(loginTmp.email);

    loginTmp.timeoutSec = std::max(1, loginTmp.timeoutSec);

    return makeSharedRef<GdriveFileSystem>(loginTmp);
}


GdriveLogin fff::extractGdriveLogin(const AfsDevice& afsDevice) //noexcept
{
    if (const auto gdriveDevice = dynamic_cast<const GdriveFileSystem*>(&afsDevice.ref()))
        return gdriveDevice ->getGdriveLogin();

    assert(false);
    return {};
}


Zstring fff::getGoogleDriveFolderUrl(const AbstractPath& folderPath) //throw FileError
{
    if (const auto gdriveDevice = dynamic_cast<const GdriveFileSystem*>(&folderPath.afsDevice.ref()))
        return gdriveDevice->getFolderUrl(folderPath.afsPath); //throw FileError
    //assert(false);
    return {};
}


bool fff::acceptsItemPathPhraseGdrive(const Zstring& itemPathPhrase) //noexcept
{
    Zstring path = expandMacros(itemPathPhrase); //expand before trimming!
    trim(path);
    return startsWithAsciiNoCase(path, gdrivePrefix);
}


/* syntax: gdrive:\<email>[:<shared drive>]\<relative-path>[|option_name=value]

    e.g.: gdrive:\john@gmail.com\folder\file.txt
          gdrive:\john@gmail.com:location\folder\file.txt|option_name=value        */
AbstractPath fff::createItemPathGdrive(const Zstring& itemPathPhrase) //noexcept
{
    Zstring pathPhrase = expandMacros(itemPathPhrase); //expand before trimming!
    trim(pathPhrase);

    if (startsWithAsciiNoCase(pathPhrase, gdrivePrefix))
        pathPhrase = pathPhrase.c_str() + strLength(gdrivePrefix);
    trim(pathPhrase, TrimSide::left, [](Zchar c) { return c == Zstr('/') || c == Zstr('\\'); });

    const ZstringView fullPath = beforeFirst<ZstringView>(pathPhrase, Zstr('|'), IfNotFoundReturn::all);
    const ZstringView options  =  afterFirst<ZstringView>(pathPhrase, Zstr('|'), IfNotFoundReturn::none);

    auto it = std::find_if(fullPath.begin(), fullPath.end(), [](Zchar c) { return c == '/' || c == '\\'; });
    const ZstringView emailAndDrive = makeStringView(fullPath.begin(), it);
    const AfsPath itemPath = sanitizeDeviceRelativePath({it, fullPath.end()});

    GdriveLogin login
    {
        .email        = utfTo<std::string>(beforeFirst(emailAndDrive, Zstr(':'), IfNotFoundReturn::all)),
        .locationName =            Zstring(afterFirst (emailAndDrive, Zstr(':'), IfNotFoundReturn::none)),
    };

    split(options, Zstr('|'), [&](ZstringView optPhrase)
    {
        optPhrase = trimCpy(optPhrase);
        if (!optPhrase.empty())
        {
            if (startsWith(optPhrase, Zstr("timeout=")))
                login.timeoutSec = stringTo<int>(afterFirst(optPhrase, Zstr('='), IfNotFoundReturn::none));
            else
                assert(false);
        }
    });
    return AbstractPath(makeSharedRef<GdriveFileSystem>(login), itemPath);
}

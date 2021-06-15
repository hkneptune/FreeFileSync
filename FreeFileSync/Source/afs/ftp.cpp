// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "ftp.h"
#include <zen/basic_math.h>
#include <zen/sys_error.h>
#include <zen/globals.h>
#include <zen/resolve_path.h>
#include <zen/time.h>
#include <libcurl/curl_wrap.h> //DON'T include <curl/curl.h> directly!
#include "init_curl_libssh2.h"
#include "ftp_common.h"
#include "abstract_impl.h"
    #include <glib.h>
    #include <fcntl.h>

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


namespace
{
//Extensions to FTP: https://tools.ietf.org/html/rfc3659
//FTP commands       https://en.wikipedia.org/wiki/List_of_FTP_commands

const std::chrono::seconds FTP_SESSION_MAX_IDLE_TIME  (20);
const std::chrono::seconds FTP_SESSION_CLEANUP_INTERVAL(4);
const int FTP_STREAM_BUFFER_SIZE = 512 * 1024; //unit: [byte]
//FTP stream buffer should be at least as big as the biggest AFS block size (currently 256 KB for MTP),
//but there seems to be no reason for an upper limit

const Zchar ftpPrefix[] = Zstr("ftp:");

enum class ServerEncoding
{
    utf8,
    ansi
};


//use all configuration data that *defines* an SFTP session as key when buffering sessions! This is what user expects, e.g. when changing settings in FTP login dialog
struct FtpSessionId
{
    /*explicit*/ FtpSessionId(const FtpLogin& login) :
        server(login.server),
        port(login.port),
        username(login.username),
        password(login.password),
        useTls(login.useTls) {}

    Zstring server;
    int     port = 0;
    Zstring username;
    Zstring password;
    bool useTls = false;
    //timeoutSec => irrelevant for session equality
};

std::weak_ordering operator<=>(const FtpSessionId& lhs, const FtpSessionId& rhs)
{
    //exactly the type of case insensitive comparison we need for server names!
    if (const std::weak_ordering cmp = compareAsciiNoCase(lhs.server, rhs.server); //https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfow#IDNs
        std::is_neq(cmp))
        return cmp;

    return std::tie(lhs.port, lhs.username, lhs.password, lhs.useTls) <=> //username, password: case sensitive!
           std::tie(rhs.port, rhs.username, rhs.password, rhs.useTls);
}


Zstring concatenateFtpFolderPathPhrase(const FtpLogin& login, const AfsPath& afsPath); //noexcept


Zstring ansiToUtfEncoding(const std::string& str) //throw SysError
{
    gsize bytesWritten = 0; //not including the terminating null

    GError* error = nullptr;
    ZEN_ON_SCOPE_EXIT(if (error) ::g_error_free(error));

    //https://developer.gnome.org/glib/stable/glib-Character-Set-Conversion.html#g-convert
    gchar* utfStr = ::g_convert(str.c_str(),   //const gchar* str
                                str.size(),    //gssize len
                                "UTF-8",       //const gchar* to_codeset
                                "LATIN1",      //const gchar* from_codeset
                                nullptr,       //gsize* bytes_read
                                &bytesWritten, //gsize* bytes_written
                                &error);       //GError** error
    if (!utfStr)
        throw SysError(formatGlibError("g_convert(" + utfTo<std::string>(str) + ')', error));
    ZEN_ON_SCOPE_EXIT(::g_free(utfStr));

    return {utfStr, bytesWritten};


}


std::string utfToAnsiEncoding(const Zstring& str) //throw SysError
{
    gsize bytesWritten = 0; //not including the terminating null

    GError* error = nullptr;
    ZEN_ON_SCOPE_EXIT(if (error) ::g_error_free(error));

    gchar* ansiStr = ::g_convert(str.c_str(),   //const gchar* str
                                 str.size(),    //gssize len
                                 "LATIN1",      //const gchar* to_codeset
                                 "UTF-8",       //const gchar* from_codeset
                                 nullptr,       //gsize* bytes_read
                                 &bytesWritten, //gsize* bytes_written
                                 &error);       //GError** error
    if (!ansiStr)
        throw SysError(formatGlibError("g_convert(" + utfTo<std::string>(str) + ')', error));
    ZEN_ON_SCOPE_EXIT(::g_free(ansiStr));

    return {ansiStr, bytesWritten};

}


Zstring serverToUtfEncoding(const std::string& str, ServerEncoding enc) //throw SysError
{
    switch (enc)
    {
        case ServerEncoding::utf8:
            return utfTo<Zstring>(str);
        case ServerEncoding::ansi:
            return ansiToUtfEncoding(str); //throw SysError
    }
    assert(false);
    return {};
}


std::string utfToServerEncoding(const Zstring& str, ServerEncoding enc) //throw SysError
{
    switch (enc)
    {
        case ServerEncoding::utf8:
            return utfTo<std::string>(str);
        case ServerEncoding::ansi:
            return utfToAnsiEncoding(str); //throw SysError
    }
    assert(false);
    return {};
}


std::wstring getCurlDisplayPath(const Zstring& serverName, const AfsPath& afsPath)
{
    Zstring displayPath = Zstring(ftpPrefix) + Zstr("//") + serverName;
    const Zstring relPath = getServerRelPath(afsPath);
    if (relPath != Zstr("/"))
        displayPath += relPath;
    return utfTo<std::wstring>(displayPath);
}


std::vector<std::string> splitFtpResponse(const std::string& buf)
{
    std::vector<std::string> lines;

    const auto isLb = [](char c) { return isLineBreak(c) || c == '\0'; };
    auto it = buf.begin();
    for (;;)
    {
        auto itLineBegin = std::find_if_not(it, buf.end(), isLb);
        if (itLineBegin == buf.end())
            return lines;

        it = std::find_if(itLineBegin + 1, buf.end(), isLb);
        lines.emplace_back(itLineBegin, it);
    }
}


class FtpLineParser
{
public:
    explicit FtpLineParser(const std::string& line) : line_(line), it_(line_.begin()) {}

    template <class Function>
    std::string readRange(size_t count, Function acceptChar) //throw SysError
    {
        if (static_cast<ptrdiff_t>(count) > line_.end() - it_)
            throw SysError(L"Unexpected end of line.");

        if (!std::all_of(it_, it_ + count, acceptChar))
            throw SysError(L"Expected char type not found.");

        std::string output(it_, it_ + count);
        it_ += count;
        return output;
    }

    template <class Function> //expects non-empty range!
    std::string readRange(Function acceptChar) //throw SysError
    {
        auto itEnd = std::find_if_not(it_, line_.end(), acceptChar);
        std::string output(it_, itEnd);
        if (output.empty())
            throw SysError(L"Expected char range not found.");
        it_ = itEnd;
        return output;
    }

    char peekNextChar() const { return it_ == line_.end() ? '\0' : *it_; }

private:
    const std::string line_;
    std::string::const_iterator it_;
};

//----------------------------------------------------------------------------------------------------------------

std::wstring formatFtpStatus(int sc)
{
    const wchar_t* statusText = [&] //https://en.wikipedia.org/wiki/List_of_FTP_server_return_codes
    {
        switch (sc)
        {
            //*INDENT-OFF*
            case 400: return L"The command was not accepted but the error condition is temporary.";
            case 421: return L"Service not available, closing control connection.";
            case 425: return L"Cannot open data connection.";
            case 426: return L"Connection closed; transfer aborted.";
            case 430: return L"Invalid username or password.";
            case 431: return L"Need some unavailable resource to process security.";
            case 434: return L"Requested host unavailable.";
            case 450: return L"Requested file action not taken.";
            case 451: return L"Local error in processing.";
            case 452: return L"Insufficient storage space in system. File unavailable, e.g. file busy.";

            case 500: return L"Syntax error, command unrecognized or command line too long.";
            case 501: return L"Syntax error in parameters or arguments.";
            case 502: return L"Command not implemented.";
            case 503: return L"Bad sequence of commands.";
            case 504: return L"Command not implemented for that parameter.";
            case 521: return L"Data connection cannot be opened with this PROT setting.";
            case 522: return L"Server does not support the requested network protocol.";
            case 530: return L"User not logged in.";
            case 532: return L"Need account for storing files.";
            case 533: return L"Command protection level denied for policy reasons.";
            case 534: return L"Could not connect to server; issue regarding SSL.";
            case 535: return L"Failed security check.";
            case 536: return L"Requested PROT level not supported by mechanism.";
            case 537: return L"Command protection level not supported by security mechanism.";
            case 550: return L"File unavailable, e.g. file not found, no access.";
            case 551: return L"Requested action aborted. Page type unknown.";
            case 552: return L"Requested file action aborted. Exceeded storage allocation.";
            case 553: return L"File name not allowed.";

            default:  return L"";
            //*INDENT-ON*
        }
    }();

    if (strLength(statusText) == 0)
        return trimCpy(replaceCpy<std::wstring>(L"FTP status %x.", L"%x", numberTo<std::wstring>(sc)));
    else
        return trimCpy(replaceCpy<std::wstring>(L"FTP status %x: ", L"%x", numberTo<std::wstring>(sc)) + statusText);
}

//================================================================================================================
//================================================================================================================

constinit Global<UniSessionCounter> globalFtpSessionCount;
GLOBAL_RUN_ONCE(globalFtpSessionCount.set(createUniSessionCounter()));


class FtpSession
{
public:
    explicit FtpSession(const FtpSessionId& sessionId) : //throw SysError
        sessionId_(sessionId),
        libsshCurlUnifiedInitCookie_(getLibsshCurlUnifiedInitCookie(globalFtpSessionCount)), //throw SysError
        lastSuccessfulUseTime_(std::chrono::steady_clock::now()) {}

    ~FtpSession()
    {
        if (easyHandle_)
            ::curl_easy_cleanup(easyHandle_);
    }

    //returns server response (header data)
    std::string perform(const AfsPath& afsPath, bool isDir, curl_ftpmethod pathMethod,
                        const std::vector<CurlOption>& extraOptions, bool requiresUtf8, int timeoutSec) //throw SysError
    {
        if (requiresUtf8) //avoid endless recursion
            ensureUtf8(timeoutSec); //throw SysError

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

        std::string headerData;
        using CbType =    size_t (*)(const char* buffer, size_t size, size_t nitems, void* callbackData);
        CbType onHeaderReceived = [](const char* buffer, size_t size, size_t nitems, void* callbackData)
        {
            auto& output = *static_cast<std::string*>(callbackData);
            output.append(buffer, size * nitems);
            return size * nitems;
        };
        options.emplace_back(CURLOPT_HEADERDATA, &headerData);
        options.emplace_back(CURLOPT_HEADERFUNCTION, onHeaderReceived);

        //lifetime: keep alive until after curl_easy_setopt() below
        const std::string curlPath = getCurlUrlPath(afsPath, isDir, timeoutSec); //throw SysError
        options.emplace_back(CURLOPT_URL, curlPath.c_str());

        assert(pathMethod != CURLFTPMETHOD_MULTICWD); //too slow!
        options.emplace_back(CURLOPT_FTP_FILEMETHOD, pathMethod);

        //ANSI or UTF encoding?
        //  "modern" FTP servers (implementing RFC 2640) have UTF8 enabled by default => pray and hope for the best.
        //  What about ANSI-FTP servers and "Microsoft FTP Service" which requires "OPTS UTF8 ON"? => *psh*
        //  CURLOPT_PREQUOTE to the rescue? Nope, issued long after USER/PASS
        const auto& username = utfTo<std::string>(sessionId_.username);
        const auto& password = utfTo<std::string>(sessionId_.password);
        if (!username.empty()) //else: libcurl handles anonymous login for us (including fake email as password)
        {
            options.emplace_back(CURLOPT_USERNAME, username.c_str());
            options.emplace_back(CURLOPT_PASSWORD, password.c_str());
        }

        if (sessionId_.port > 0)
            options.emplace_back(CURLOPT_PORT, static_cast<long>(sessionId_.port));

        options.emplace_back(CURLOPT_NOSIGNAL, 1L); //thread-safety: https://curl.haxx.se/libcurl/c/threadsafe.html

        options.emplace_back(CURLOPT_CONNECTTIMEOUT, timeoutSec);

        //CURLOPT_TIMEOUT: "Since this puts a hard limit for how long time a request is allowed to take, it has limited use in dynamic use cases with varying transfer times."
        options.emplace_back(CURLOPT_LOW_SPEED_TIME, timeoutSec);
        options.emplace_back(CURLOPT_LOW_SPEED_LIMIT, 1L); //[bytes], can't use "0" which means "inactive", so use some low number

        //unlike CURLOPT_TIMEOUT, this one is NOT a limit on the total transfer time
        options.emplace_back(CURLOPT_FTP_RESPONSE_TIMEOUT, timeoutSec); //== alias of CURLOPT_SERVER_RESPONSE_TIMEOUT

        //CURLOPT_ACCEPTTIMEOUT_MS? => only relevant for "active" FTP connections

        //long-running file uploads require us to send keep-alives for the TCP control connection: https://freefilesync.org/forum/viewtopic.php?t=6928
        options.emplace_back(CURLOPT_TCP_KEEPALIVE, 1L);


        std::optional<SysError> callbackException;
        //libcurl does *not* set FD_CLOEXEC for us! https://github.com/curl/curl/issues/2252
        auto onSocketCreate = [&](curl_socket_t curlfd, curlsocktype purpose)
        {
            assert(::fcntl(curlfd, F_GETFD) == 0);
            if (::fcntl(curlfd, F_SETFD, FD_CLOEXEC) == -1) //=> RACE-condition if other thread calls fork/execv before this thread sets FD_CLOEXEC!
            {
                callbackException = SysError(formatSystemError("fcntl(FD_CLOEXEC)", errno));
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

        options.emplace_back(CURLOPT_SOCKOPTFUNCTION, onSocketCreateWrapper);
        options.emplace_back(CURLOPT_SOCKOPTDATA, &onSocketCreate);

        //Use share interface? https://curl.haxx.se/libcurl/c/libcurl-share.html
        //perf test, 4 and 8 parallel threads:
        //  CURL_LOCK_DATA_DNS         => no measurable total time difference
        //  CURL_LOCK_DATA_SSL_SESSION => freefilesync.org; not working at all: lots of CURLE_RECV_ERROR (seems nobody ever tested this with truly parallel FTP accesses!)
#if 0
        do not include this into release!
            static CURLSH* curlShare = []
        {
            struct ShareLocks
            {
                std::mutex lockIntenal;
                std::mutex lockDns;
                std::mutex lockSsl;
            };
            static ShareLocks globalLocksTestingOnly;

            using LockFunType = void (*)(CURL* handle, curl_lock_data data, curl_lock_access access, void* userptr); //needed for cdecl function pointer cast
            LockFunType lockFun =     [](CURL* handle, curl_lock_data data, curl_lock_access access, void* userptr)
            {
                auto& locks = *static_cast<ShareLocks*>(userptr);
                switch (data)
                {
                    case CURL_LOCK_DATA_SHARE:
                        return locks.lockIntenal.lock();
                    case CURL_LOCK_DATA_DNS:
                        return locks.lockDns.lock();
                    case CURL_LOCK_DATA_SSL_SESSION:
                        return locks.lockSsl.lock();
                }
                assert(false);
            };
            using UnlockFunType = void (*)(CURL *handle, curl_lock_data data, void* userptr);
            UnlockFunType unlockFun =   [](CURL *handle, curl_lock_data data, void* userptr)
            {
                auto& locks = *static_cast<ShareLocks*>(userptr);
                switch (data)
                {
                    case CURL_LOCK_DATA_SHARE:
                        return locks.lockIntenal.unlock();
                    case CURL_LOCK_DATA_DNS:
                        return locks.lockDns.unlock();
                    case CURL_LOCK_DATA_SSL_SESSION:
                        return locks.lockSsl.unlock();
                }
                assert(false);
            };

            CURLSH* cs = ::curl_share_init();
            assert(cs);
            CURLSHcode rc = CURLSHE_OK;
            rc = ::curl_share_setopt(cs, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
            assert(rc == CURLSHE_OK);
            rc = ::curl_share_setopt(cs, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION); //buggy!?
            assert(rc == CURLSHE_OK);
            rc = ::curl_share_setopt(cs, CURLSHOPT_LOCKFUNC, lockFun);
            assert(rc == CURLSHE_OK);
            rc = ::curl_share_setopt(cs, CURLSHOPT_UNLOCKFUNC, unlockFun);
            assert(rc == CURLSHE_OK);
            rc = ::curl_share_setopt(cs, CURLSHOPT_USERDATA, &globalLocksTestingOnly);
            assert(rc == CURLSHE_OK);
            return cs;
        }();
        //CURLSHcode ::curl_share_cleanup(curlShare);
        options.emplace_back(CURLOPT_SHARE, curlShare);
#endif

        //TODO: FTP option to require certificate checking?
#if 0
        options.emplace_back(CURLOPT_CAINFO, "cacert.pem"); //hopefully latest version from https://curl.haxx.se/docs/caextract.html
        //libcurl forwards this char-string to OpenSSL as is, which (thank god) accepts UTF8
#else
        options.emplace_back(CURLOPT_CAINFO, 0L); //be explicit: "even when [CURLOPT_SSL_VERIFYPEER] is disabled [...] curl may still load the certificate file specified in CURLOPT_CAINFO."

        //check if server certificate can be trusted? (Default: 1L)
        //  => may fail with: CURLE_PEER_FAILED_VERIFICATION: SSL certificate problem: certificate has expired
        options.emplace_back(CURLOPT_SSL_VERIFYPEER, 0L);
        //check that server name matches the name in the certificate? (Default: 2L)
        //  => may fail with: CURLE_PEER_FAILED_VERIFICATION: SSL: no alternative certificate subject name matches target host name 'freefilesync.org'
        options.emplace_back(CURLOPT_SSL_VERIFYHOST, 0L);
#endif
        if (sessionId_.useTls) //https://tools.ietf.org/html/rfc4217
        {
            options.emplace_back(CURLOPT_USE_SSL,    CURLUSESSL_ALL); //require SSL for both control and data
            options.emplace_back(CURLOPT_FTPSSLAUTH, CURLFTPAUTH_TLS); //try TLS first, then SSL (currently: CURLFTPAUTH_DEFAULT == CURLFTPAUTH_SSL)
        }

        //let's not hold our breath until Curl adds a reasonable PASV handling => patch libcurl accordingly!
        //https://github.com/curl/curl/issues/1455
        //https://github.com/curl/curl/pull/1470
        //support broken servers like this one: https://freefilesync.org/forum/viewtopic.php?t=4301

        append(options, extraOptions);

        applyCurlOptions(easyHandle_, options); //throw SysError

        //=======================================================================================================
        const CURLcode rcPerf = ::curl_easy_perform(easyHandle_);
        //WTF: curl_easy_perform() considers FTP response codes 4XX, 5XX as failure, but for HTTP response codes 4XX are considered success!! CONSISTENCY, people!!!
        //note: CURLOPT_FAILONERROR(default:off) is only available for HTTP => BUT at least we can prefix FTP commands with * for same effect

        if (callbackException)
            throw* callbackException; //throw SysError

        //long ftpStatusCode = 0; //optional
        ///*const CURLcode rc =*/ ::curl_easy_getinfo(easyHandle_, CURLINFO_RESPONSE_CODE, &ftpStatusCode);
        //=======================================================================================================

        if (rcPerf != CURLE_OK)
        {
            std::wstring errorMsg = trimCpy(utfTo<std::wstring>(curlErrorBuf)); //optional

            if (const std::vector<std::string>& headerLines = splitFtpResponse(headerData);
                !headerLines.empty())
                if (const std::string& response = trimCpy(headerLines.back()); //that *should* be the server's error response
                    !response.empty())
                    errorMsg += (errorMsg.empty() ? L"" : L"\n") + utfTo<std::wstring>(response);
#if 0
            //utfTo<std::wstring>(::curl_easy_strerror(ec)) is uninteresting
            //use CURLINFO_OS_ERRNO ?? https://curl.haxx.se/libcurl/c/CURLINFO_OS_ERRNO.html
            long nativeErrorCode = 0;
            if (::curl_easy_getinfo(easyHandle_, CURLINFO_OS_ERRNO, &nativeErrorCode) == CURLE_OK)
                if (nativeErrorCode != 0)
                    errorMsg += (errorMsg.empty() ? L"" : L"\n") + std::wstring(L"Native error code: ") + numberTo<std::wstring>(nativeErrorCode);
#endif
            throw SysError(formatSystemError("curl_easy_perform", formatCurlStatusCode(rcPerf), errorMsg));
        }

        lastSuccessfulUseTime_ = std::chrono::steady_clock::now();
        return headerData;
    }

    //returns server response (header data)
    std::string runSingleFtpCommand(const std::string& ftpCmd, bool requiresUtf8, int timeoutSec) //throw SysError
    {
        curl_slist* quote = nullptr;
        ZEN_ON_SCOPE_EXIT(::curl_slist_free_all(quote));
        quote = ::curl_slist_append(quote, ftpCmd.c_str());

        return perform(AfsPath(), true /*isDir*/, CURLFTPMETHOD_NOCWD /*avoid needless CWDs*/,
        {
            {CURLOPT_NOBODY, 1L},
            {CURLOPT_QUOTE, quote},
        }, requiresUtf8, timeoutSec); //throw SysError
    }

    void testConnection(int timeoutSec) //throw SysError
    {
        /*  https://en.wikipedia.org/wiki/List_of_FTP_commands
            FEAT: are there servers that don't support this command? fuck, yes: "550 FEAT: Operation not permitted" => buggy server not granting access, despite support!
            PWD? will fail if last access deleted the working dir!
            "TYPE I"? might interfere with libcurls internal handling, but that's an improvement, right? right? :>
            => but "HELP", and "NOOP" work, right??
            Fuck my life: even "HELP" is not always implemented: https://freefilesync.org/forum/viewtopic.php?t=6002
            => are there servers supporting neither FEAT nor HELP? only time will tell...
            ... and it tells! FUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU https://freefilesync.org/forum/viewtopic.php?t=8041             */

        //=> * to the rescue: as long as we get an FTP response - *any* FTP response (including 550) - the connection itself is fine!
        const std::string& featBuf = runSingleFtpCommand("*FEAT", false /*requiresUtf8*/, timeoutSec); //throw SysError

        for (const std::string& line : splitFtpResponse(featBuf))
            if (startsWith(line, "211-") ||
                startsWith(line, "211 ") ||
                startsWith(line, "500 ") ||
                startsWith(line, "550 "))
                return;

        //ever get here?
        throw SysError(L"Unexpected FTP response. (" + utfTo<std::wstring>(featBuf) + L')');
    }

    AfsPath getHomePath(int timeoutSec) //throw SysError
    {
        if (!homePathCached_)
            homePathCached_ = [&]
        {
            if (easyHandle_)
            {
                const char* homePathCurl = nullptr; //not owned
                /*CURLcode rc =*/ ::curl_easy_getinfo(easyHandle_, CURLINFO_FTP_ENTRY_PATH, &homePathCurl);

                if (homePathCurl && isAsciiString(homePathCurl))
                    return sanitizeDeviceRelativePath(utfTo<Zstring>(homePathCurl));

                //home path with non-ASCII chars: libcurl issues PWD right after login *before* server was set up for UTF8
                //=> CURLINFO_FTP_ENTRY_PATH could be in any encoding => useless!
                //   Test case: Windows 10 IIS FTP with non-Ascii entry path
                //=> start new FTP session and parse PWD *after* UTF8 is enabled:
                ::curl_easy_cleanup(easyHandle_);
                easyHandle_ = nullptr;
            }

            const std::string& pwdBuf = runSingleFtpCommand("PWD", true /*requiresUtf8*/, timeoutSec); //throw SysError

            for (const std::string& line : splitFtpResponse(pwdBuf))
                if (startsWith(line, "257 "))
                {
                    /* 257<space>[rubbish]"<directory-name>"<space><commentary>        according to libcurl

                       "The directory name can contain any character; embedded double-quotes should be escaped by
                       double-quotes (the "quote-doubling" convention)." https://tools.ietf.org/html/rfc959                    */
                    auto itBegin = std::find(line.begin(), line.end(), '"');
                    if (itBegin != line.end())
                        for (auto it = ++itBegin; it != line.end(); ++it)
                            if (*it == '"')
                            {
                                if (it + 1 != line.end() && it[1] == '"')
                                    ++it; //skip double quote
                                else
                                {
                                    const std::string homePathRaw = replaceCpy(std::string{itBegin, it}, "\"\"", '"');
                                    const ServerEncoding enc = getServerEncoding(timeoutSec); //throw SysError
                                    const Zstring homePathUtf = serverToUtfEncoding(homePathRaw, enc); //throw SysError
                                    return sanitizeDeviceRelativePath(homePathUtf);
                                }
                            }
                    break;
                }
            throw SysError(L"Unexpected FTP response. (" + utfTo<std::wstring>(pwdBuf) + L')');
        }();
        return *homePathCached_;
    }

    void ensureBinaryMode(int timeoutSec) //throw SysError
    {
        if (std::optional<curl_socket_t> currentSocket = getActiveSocket()) //throw SysError
            if (*currentSocket == binaryEnabledSocket_)
                return;

        runSingleFtpCommand("TYPE I", false /*requiresUtf8*/, timeoutSec); //throw SysError

        //make sure our binary-enabled session is still there (== libcurl behaves as we expect)
        std::optional<curl_socket_t> currentSocket = getActiveSocket(); //throw SysError
        if (!currentSocket)
            throw SysError(L"Curl failed to cache FTP session."); //why is libcurl not caching the session???

        binaryEnabledSocket_ = *currentSocket; //remember what we did
        //libcurl already buffers "conn->proto.ftpc.transfertype" but selfishly keeps it for itself!
        //=> pray libcurl doesn't internally set "TYPE A"!
        //=> this seems to be the only place where it does: https://github.com/curl/curl/issues/4342
    }

    //------------------------------------------------------------------------------------------------------------
    bool supportsMlsd(int timeoutSec) { return getFeatureSupport(&Features::mlsd, timeoutSec); } //
    bool supportsMfmt(int timeoutSec) { return getFeatureSupport(&Features::mfmt, timeoutSec); } //throw SysError
    bool supportsClnt(int timeoutSec) { return getFeatureSupport(&Features::clnt, timeoutSec); } //
    bool supportsUtf8(int timeoutSec) { return getFeatureSupport(&Features::utf8, timeoutSec); } //

    ServerEncoding getServerEncoding(int timeoutSec) { return supportsUtf8(timeoutSec) ? ServerEncoding::utf8 : ServerEncoding::ansi; } //throw SysError

    bool isHealthy() const
    {
        return std::chrono::steady_clock::now() - lastSuccessfulUseTime_ <= FTP_SESSION_MAX_IDLE_TIME;
    }

    std::string getServerPathInternal(const AfsPath& afsPath, int timeoutSec) //throw SysError
    {
        const Zstring serverPath = getServerRelPath(afsPath);

        if (afsPath.value.empty()) //endless recursion caveat!! getServerEncoding() transitively depends on getServerPathInternal()
            return utfTo<std::string>(serverPath);

        const ServerEncoding encoding = getServerEncoding(timeoutSec); //throw SysError

        return utfToServerEncoding(serverPath, encoding); //throw SysError
    }

private:
    FtpSession           (const FtpSession&) = delete;
    FtpSession& operator=(const FtpSession&) = delete;

    std::string getCurlUrlPath(const AfsPath& afsPath /*optional*/, bool isDir, int timeoutSec) //throw SysError
    {
        std::string curlRelPath; //libcurl expects encoded paths (except for '/' char!!!) => bug: https://github.com/curl/curl/pull/4423

        for (const std::string& comp : split(getServerPathInternal(afsPath, timeoutSec), '/', SplitOnEmpty::skip)) //throw SysError
        {
            char* compFmt = ::curl_easy_escape(easyHandle_, comp.c_str(), static_cast<int>(comp.size()));
            if (!compFmt)
                throw SysError(formatSystemError("curl_easy_escape(" + comp + ')', L"", L"Conversion failure"));
            ZEN_ON_SCOPE_EXIT(::curl_free(compFmt));

            if (!curlRelPath.empty())
                curlRelPath += '/';
            curlRelPath += compFmt;
        }

        static_assert(LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 67));
        /*  1. CURLFTPMETHOD_NOCWD requires absolute paths to unconditionally skip CWDs: https://github.com/curl/curl/pull/4382
            2. CURLFTPMETHOD_SINGLECWD requires absolute paths to skip one needless "CWD entry path": https://github.com/curl/curl/pull/4332
              => https://curl.haxx.se/docs/faq.html#How_do_I_list_the_root_dir_of_an
              => use // because /%2f had bugs (but they should be fixed: https://github.com/curl/curl/pull/4348)             */
        std::string path = utfTo<std::string>(Zstring(ftpPrefix) + Zstr("//") + sessionId_.server) + "//" + curlRelPath;

        if (isDir && !endsWith(path, '/')) //curl-FTP needs directory paths to end with a slash
            path += '/';
        return path;
    }

    void ensureUtf8(int timeoutSec) //throw SysError
    {
        //"OPTS UTF8 ON" needs to be activated each time libcurl internally creates a new session
        //hopyfully libcurl will offer a better solution: https://github.com/curl/curl/issues/1457

        //Some RFC-2640-non-compliant servers require UTF8 to be explicitly enabled: https://wiki.filezilla-project.org/Character_Encoding#Conflicting_specification
        //e.g. this one (Microsoft FTP Service): https://freefilesync.org/forum/viewtopic.php?t=4303
        if (supportsUtf8(timeoutSec)) //throw SysError
        {
            //[!] supportsUtf8() is buffered! => FTP session might not yet exist (or was closed by libcurl after a failure)
            if (std::optional<curl_socket_t> currentSocket = getActiveSocket()) //throw SysError
                if (*currentSocket == utf8EnabledSocket_) //caveat: a non-UTF8-enabled session might already exist, e.g. from a previous call to supportsMlsd()
                    return;

            //some servers even require "CLNT" before accepting "OPTS UTF8 ON": https://social.msdn.microsoft.com/Forums/en-US/d602574f-8a69-4d69-b337-52b6081902cf/problem-with-ftpwebrequestopts-utf8-on-501-please-clnt-first
            if (supportsClnt(timeoutSec)) //throw SysError
                runSingleFtpCommand("CLNT FreeFileSync", false /*requiresUtf8*/, timeoutSec); //throw SysError

            //"prefix the command with an asterisk to make libcurl continue even if the command fails"
            //-> ignore if server does not know this legacy command (but report all *other* issues; else getActiveSocket() below won't return value and hide real error!)
            //"If an RFC 2640 compliant client sends OPTS UTF-8 ON, it has to use UTF-8 regardless whether OPTS UTF-8 ON succeeds or not. "
            runSingleFtpCommand("*OPTS UTF8 ON", false /*requiresUtf8*/, timeoutSec); //throw SysError

            //make sure our unicode-enabled session is still there (== libcurl behaves as we expect)
            std::optional<curl_socket_t> currentSocket = getActiveSocket(); //throw SysError
            if (!currentSocket)
                throw SysError(L"Curl failed to cache FTP session."); //why is libcurl not caching the session???

            utf8EnabledSocket_ = *currentSocket; //remember what we did
        }
    }

    std::optional<curl_socket_t> getActiveSocket() //throw SysError
    {
        if (easyHandle_)
        {
            curl_socket_t currentSocket = 0;
            const CURLcode rc = ::curl_easy_getinfo(easyHandle_, CURLINFO_ACTIVESOCKET, &currentSocket);
            if (rc != CURLE_OK)
                throw SysError(formatSystemError("curl_easy_getinfo(CURLINFO_ACTIVESOCKET)", formatCurlStatusCode(rc), utfTo<std::wstring>(::curl_easy_strerror(rc))));
            if (currentSocket != CURL_SOCKET_BAD)
                return currentSocket;
        }
        return {};
    }

    struct Features
    {
        bool mlsd = false;
        bool mfmt = false;
        bool clnt = false;
        bool utf8 = false;
    };
    using FeatureList = std::map<Zstring /*server name*/, std::optional<Features>, LessAsciiNoCase>;

    bool getFeatureSupport(bool Features::* status, int timeoutSec) //throw SysError
    {
        if (!featureCache_)
        {
            static constinit FunStatGlobal<Protected<FeatureList>> globalServerFeatures;
            globalServerFeatures.initOnce([] { return std::make_unique<Protected<FeatureList>>(); });

            const auto sf = globalServerFeatures.get();
            if (!sf)
                throw SysError(formatSystemError("FtpSession::getFeatureSupport", L"", L"Function call not allowed during init/shutdown."));

            sf->access([&](FeatureList& feat) { featureCache_ = feat[sessionId_.server]; });

            if (!featureCache_)
            {
                //*: ignore error if server does not support/allow FEAT
                featureCache_ = parseFeatResponse(runSingleFtpCommand("*FEAT", false /*requiresUtf8*/, timeoutSec)); //throw SysError
                //used by ensureUtf8()! => requiresUtf8 = false!!!

                sf->access([&](FeatureList& feat) { feat[sessionId_.server] = featureCache_; });
            }
        }
        return (*featureCache_).*status;
    }

    static Features parseFeatResponse(const std::string& featResponse)
    {
        Features output; //FEAT command: https://tools.ietf.org/html/rfc2389#page-4
        std::vector<std::string> lines = splitFtpResponse(featResponse);

        auto it = std::find_if(lines.begin(), lines.end(), [](const std::string& line) { return startsWith(line, "211-") || startsWith(line, "211 "); });
        if (it != lines.end())
        {
            ++it;
            for (; it != lines.end(); ++it)
            {
                std::string& line = *it;
                if (equalAsciiNoCase     (line, "211 End") ||
                    startsWithAsciiNoCase(line, "211 End ")) //Serv-U: "211 End (for details use "HELP commmand" where command is the command of interest)"
                    break;                                   //Home Ftp Server: "211 End of extentions."

                //suppport ProFTPD with "MultilineRFC2228 = on" https://freefilesync.org/forum/viewtopic.php?t=7243
                if (startsWith(line, "211-"))
                    line = ' ' + afterFirst(line, '-', IfNotFoundReturn::none);

                //https://tools.ietf.org/html/rfc3659#section-7.8
                //"a server-FTP process that supports MLST, and MLSD [...] MUST indicate that this support exists"
                //"there is no distinct FEAT output for MLSD. The presence of the MLST feature indicates that both MLST and MLSD are supported"
                if (equalAsciiNoCase     (line, " MLST") ||
                    startsWithAsciiNoCase(line, " MLST ")) //SP "MLST" [SP factlist] CRLF
                    output.mlsd = true;
                //so much the theory. In practice FTP server implementers can't read specs, of course: https://freefilesync.org/forum/viewtopic.php?t=6752
                if (equalAsciiNoCase(line, " MLSD"))
                    output.mlsd = true;

                //https://tools.ietf.org/html/draft-somers-ftp-mfxx-04#section-3.3
                //"Where a server-FTP process supports the MFMT command [...] it MUST include the response to the FEAT command"
                else if (equalAsciiNoCase(line, " MFMT")) //SP "MFMT" CRLF
                    output.mfmt = true;

                else if (equalAsciiNoCase(line, " UTF8") ||
                         equalAsciiNoCase(line, " UTF8 ON")) //support non-compliant servers: https://freefilesync.org/forum/viewtopic.php?t=7355#p24694
                    output.utf8 = true;

                else if (equalAsciiNoCase(line, " CLNT"))
                    output.clnt = true;
            }
        }
        return output;
    }

    const FtpSessionId sessionId_;
    CURL* easyHandle_ = nullptr;

    curl_socket_t utf8EnabledSocket_ = 0;
    curl_socket_t binaryEnabledSocket_ = 0;

    std::optional<Features> featureCache_;
    std::optional<AfsPath> homePathCached_;

    const std::shared_ptr<UniCounterCookie> libsshCurlUnifiedInitCookie_;
    std::chrono::steady_clock::time_point lastSuccessfulUseTime_;
};

//================================================================================================================
//================================================================================================================

class FtpSessionManager //reuse (healthy) FTP sessions globally
{
    using IdleFtpSessions = std::vector<std::unique_ptr<FtpSession>>;

public:
    FtpSessionManager() : sessionCleaner_([this]
    {
        setCurrentThreadName(Zstr("Session Cleaner[FTP]"));
        runGlobalSessionCleanUp(); /*throw ThreadStopRequest*/
    }) {}

    void access(const FtpLogin& login, const std::function<void(FtpSession& session)>& useFtpSession /*throw X*/) //throw SysError, X
    {
        Protected<IdleFtpSessions>& sessionStore = getSessionStore(login);

        std::unique_ptr<FtpSession> ftpSession;

        sessionStore.access([&](IdleFtpSessions& sessions)
        {
            //assume "isHealthy()" to avoid hitting server connection limits: (clean up of !isHealthy() after use, idle sessions via worker thread)
            if (!sessions.empty())
            {
                ftpSession = std::move(sessions.back    ());
                /**/                   sessions.pop_back();
            }
        });

        //create new FTP session outside the lock: 1. don't block other threads 2. non-atomic regarding "sessionStore"! => one session too many is not a problem!
        if (!ftpSession)
            ftpSession = std::make_unique<FtpSession>(login); //throw SysError

        ZEN_ON_SCOPE_EXIT(
            if (ftpSession->isHealthy()) //thread that created the "!isHealthy()" session is responsible for clean up (avoid hitting server connection limits!)
        sessionStore.access([&](IdleFtpSessions& sessions) { sessions.push_back(std::move(ftpSession)); }); );

        useFtpSession(*ftpSession); //throw X
    }

private:
    FtpSessionManager           (const FtpSessionManager&) = delete;
    FtpSessionManager& operator=(const FtpSessionManager&) = delete;

    Protected<IdleFtpSessions>& getSessionStore(const FtpSessionId& sessionId)
    {
        //single global session store per login; life-time bound to globalInstance => never remove a sessionStore!!!
        Protected<IdleFtpSessions>* store = nullptr;

        globalSessionStore_.access([&](GlobalFtpSessions& sessionsById)
        {
            store = &sessionsById[sessionId]; //get or create
        });
        static_assert(std::is_same_v<GlobalFtpSessions, std::map<FtpSessionId, Protected<IdleFtpSessions>>>, "require std::map so that the pointers we return remain stable");

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

            if (now < lastCleanupTime + FTP_SESSION_CLEANUP_INTERVAL)
                interruptibleSleep(lastCleanupTime + FTP_SESSION_CLEANUP_INTERVAL - now); //throw ThreadStopRequest

            lastCleanupTime = std::chrono::steady_clock::now();

            std::vector<Protected<IdleFtpSessions>*> sessionStores; //pointers remain stable, thanks to std::map<>

            globalSessionStore_.access([&](GlobalFtpSessions& sessionsById)
            {
                for (auto& [sessionId, idleSession] : sessionsById)
                    sessionStores.push_back(&idleSession);
            });

            for (Protected<IdleFtpSessions>* sessionStore : sessionStores)
                for (bool done = false; !done;)
                    sessionStore->access([&](IdleFtpSessions& sessions)
                {
                    for (std::unique_ptr<FtpSession>& sshSession : sessions)
                        if (!sshSession->isHealthy()) //!isHealthy() sessions are destroyed after use => in this context this means they have been idle for too long
                        {
                            sshSession.swap(sessions.back());
                            /**/            sessions.pop_back(); //run ~FtpSession *inside* the lock! => avoid hitting server limits!
                            std::this_thread::yield();
                            return; //don't hold lock for too long: delete only one session at a time, then yield...
                        }
                    done = true;
                });
        }
    }

    using GlobalFtpSessions = std::map<FtpSessionId, Protected<IdleFtpSessions>>;

    Protected<GlobalFtpSessions> globalSessionStore_;
    InterruptibleThread sessionCleaner_;
};

//--------------------------------------------------------------------------------------
UniInitializer globalStartupInitFtp(*globalFtpSessionCount.get());

constinit Global<FtpSessionManager> globalFtpSessionManager; //caveat: life time must be subset of static UniInitializer!
//--------------------------------------------------------------------------------------

void accessFtpSession(const FtpLogin& login, const std::function<void(FtpSession& session)>& useFtpSession /*throw X*/) //throw SysError, X
{
    if (const std::shared_ptr<FtpSessionManager> mgr = globalFtpSessionManager.get())
        mgr->access(login, useFtpSession); //throw SysError, X
    else
        throw SysError(formatSystemError("accessFtpSession", L"", L"Function call not allowed during init/shutdown."));
}

//===========================================================================================================================

struct FtpItem
{
    AFS::ItemType type = AFS::ItemType::file;
    Zstring itemName;
    uint64_t fileSize = 0;
    time_t modTime = 0;
    AFS::FingerPrint filePrint = 0; //optional
};


//get info about *existing* symlink!
FtpItem getFtpSymlinkInfo(const FtpLogin& login, const AfsPath& linkPath) //throw FileError
{
    try
    {
        FtpItem output;
        assert(output.type == AFS::ItemType::file);
        output.itemName = AFS::getItemName(linkPath);

        std::string mdtmBuf;
        accessFtpSession(login, [&](FtpSession& session) //throw SysError
        {
            /* first test if we have a file; if it's a folder expect FTP code 550
              alternative: assume folder and try traversal? NOPE: this can *succeed* for file symlinks with MLSD! (e.g. on freefilesync.org FTP)

                 -> can't replace SIZE + MDTM with MLSD which doesn't follow symlinks! */

            session.ensureBinaryMode(login.timeoutSec); //throw SysError
            //...or some server return ASCII size or fail with '550 SIZE not allowed in ASCII mode: https://freefilesync.org/forum/viewtopic.php?t=7669&start=30#p27742
            const std::string sizeBuf = session.runSingleFtpCommand("*SIZE " + session.getServerPathInternal(linkPath, login.timeoutSec),
                                                                    true /*requiresUtf8*/, login.timeoutSec); //throw SysError
            //alternative: use libcurl + CURLINFO_CONTENT_LENGTH_DOWNLOAD_T? => nah, suprise (motherfucker)! libcurl adds needless "REST 0" command!
            for (const std::string& line : splitFtpResponse(sizeBuf))
                if (startsWith(line, "213 ")) // 213<space>[rubbish]<file size>        according to libcurl
                {
                    if (isDigit(line.back())) //https://tools.ietf.org/html/rfc3659#section-4
                    {
                        auto it = std::find_if(line.rbegin(), line.rend(), [](const char c) { return !isDigit(c); });
                        output.fileSize = stringTo<uint64_t>(makeStringView(it.base(), line.end()));

                        mdtmBuf = session.runSingleFtpCommand("MDTM " + session.getServerPathInternal(linkPath, login.timeoutSec),
                                                              true /*requiresUtf8*/, login.timeoutSec); //throw SysError
                        return;
                    }
                    break;
                }
                else if (startsWith(line, "550 ")) //e.g. "550 I can only retrieve regular files"
                {
                    output.type = AFS::ItemType::folder;
                    return;
                }
            throw SysError(L"Unexpected FTP response. (" + utfTo<std::wstring>(sizeBuf) + L')');
        });

        if (output.type == AFS::ItemType::folder)
            return output;

        output.modTime = [&] //https://tools.ietf.org/html/rfc3659#section-3
        {
            for (const std::string& line : splitFtpResponse(mdtmBuf))
                if (startsWith(line, "213 ")) // 213<space> YYYYMMDDHHMMSS[.sss]       "Time values are always represented in UTC (GMT)" ...and libcurl thinks so, too
                {
                    const auto itStart = line.begin() + 4;
                    const auto itEnd = std::find(itStart, line.end(), '.');

                    if (const TimeComp tc = parseTime("%Y%m%d%H%M%S", makeStringView(itStart, itEnd));
                        tc != TimeComp())
                        if (const time_t modTime = utcToTimeT(tc); //returns -1 on error
                            modTime != -1)
                            return modTime;
                    break;
                }
            throw SysError(L"Unexpected FTP response. (" + utfTo<std::wstring>(mdtmBuf) + L')');
        }();

        return output;
    }
    catch (const SysError& e)
    {
        throw FileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(getCurlDisplayPath(login.server, linkPath))), e.toString());
    }
}


class FtpDirectoryReader
{
public:
    static std::vector<FtpItem> execute(const FtpLogin& login, const AfsPath& afsDirPath) //throw FileError
    {
        std::string rawListing; //get raw FTP directory listing

        using CbType =   size_t (*)(const char* buffer, size_t size, size_t nitems, void* callbackData);
        CbType onBytesReceived = [](const char* buffer, size_t size, size_t nitems, void* callbackData)
        {
            auto& listing = *static_cast<std::string*>(callbackData);
            listing.append(buffer, size * nitems);
            return size * nitems;
            //folder reading might take up to a minute in extreme cases (50,000 files): https://freefilesync.org/forum/viewtopic.php?t=5312
        };

        std::vector<FtpItem> output;
        try
        {
            accessFtpSession(login, [&](FtpSession& session) //throw SysError
            {
                std::vector<CurlOption> options =
                {
                    {CURLOPT_WRITEDATA, &rawListing},
                    {CURLOPT_WRITEFUNCTION, onBytesReceived},
                };
                curl_ftpmethod pathMethod = CURLFTPMETHOD_SINGLECWD;

                if (session.supportsMlsd(login.timeoutSec)) //throw SysError
                {
                    options.emplace_back(CURLOPT_CUSTOMREQUEST, "MLSD");

                    //some FTP servers abuse https://tools.ietf.org/html/rfc3659#section-7.1
                    //and process wildcards characters inside the "dirpath"; see http://www.proftpd.org/docs/howto/Globbing.html
                    //      [] matches any character in the character set enclosed in the brackets
                    //      * (not between brackets) matches any string, including the empty string
                    //      ? (not between brackets) matches any single character
                    //
                    //of course this "helpfulness" blows up with MLSD + paths that incidentally contain wildcards: https://freefilesync.org/forum/viewtopic.php?t=5575
                    const bool pathHasWildcards = [&] //=> globbing is reproducible even with freefilesync.org's FTP!
                    {
                        const size_t pos = afsDirPath.value.find(Zstr('['));
                        if (pos != Zstring::npos)
                            if (afsDirPath.value.find(Zstr(']'), pos + 1) != Zstring::npos)
                                return true;

                        return contains(afsDirPath.value, Zstr('*')) ||
                        /**/   contains(afsDirPath.value, Zstr('?'));
                    }();

                    if (!pathHasWildcards)
                        pathMethod = CURLFTPMETHOD_NOCWD; //16% faster traversal compared to CURLFTPMETHOD_SINGLECWD (35% faster than CURLFTPMETHOD_MULTICWD)
                }
                //else: use "LIST" + CURLFTPMETHOD_SINGLECWD
                //caveat: let's better not use LIST parameters: https://cr.yp.to/ftp/list.html

                session.perform(afsDirPath, true /*isDir*/, pathMethod, options, true /*requiresUtf8*/, login.timeoutSec); //throw SysError

                const ServerEncoding encoding = session.getServerEncoding(login.timeoutSec); //throw SysError
                if (session.supportsMlsd(login.timeoutSec)) //throw SysError
                    output = parseMlsd(rawListing, encoding); //throw SysError
                else
                    output = parseUnknown(rawListing, encoding); //throw SysError
            });
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(getCurlDisplayPath(login.server, afsDirPath))), e.toString());
        }
        return output;
    }

private:
    FtpDirectoryReader           (const FtpDirectoryReader&) = delete;
    FtpDirectoryReader& operator=(const FtpDirectoryReader&) = delete;

    static std::vector<FtpItem> parseMlsd(const std::string& buf, ServerEncoding enc) //throw SysError
    {
        std::vector<FtpItem> output;
        for (const std::string& line : splitFtpResponse(buf))
        {
            const FtpItem item = parseMlstLine(line, enc); //throw SysError
            if (item.itemName != Zstr(".") &&
                item.itemName != Zstr(".."))
                output.push_back(item);
        }
        return output;
    }

    static FtpItem parseMlstLine(const std::string& rawLine, ServerEncoding enc) //throw SysError
    {
        /*  https://tools.ietf.org/html/rfc3659
            type=cdir;sizd=4096;modify=20170116230740;UNIX.mode=0755;UNIX.uid=874;UNIX.gid=869;unique=902g36e1c55; .
            type=pdir;sizd=4096;modify=20170116230740;UNIX.mode=0755;UNIX.uid=874;UNIX.gid=869;unique=902g36e1c55; ..
            type=file;size=4;modify=20170113063314;UNIX.mode=0600;UNIX.uid=874;UNIX.gid=869;unique=902g36e1c5d; readme.txt
            type=dir;sizd=4096;modify=20170117144634;UNIX.mode=0755;UNIX.uid=874;UNIX.gid=869;unique=902g36e418a; folder   */
        try
        {
            FtpItem item;

            auto itBegin = rawLine.begin();
            if (startsWith(rawLine, ' ')) //leading blank is already trimmed if MLSD was processed by curl
                ++itBegin;
            auto itBlank = std::find(itBegin, rawLine.end(), ' ');
            if (itBlank == rawLine.end())
                throw SysError(L"Item name not available.");

            const std::string facts(itBegin, itBlank);
            item.itemName = serverToUtfEncoding(std::string(itBlank + 1, rawLine.end()), enc); //throw SysError

            std::string typeFact;
            std::optional<uint64_t> fileSize;

            for (const std::string& fact : split(facts, ';', SplitOnEmpty::skip))
                if (startsWithAsciiNoCase(fact, "type=")) //must be case-insensitive!!!
                {
                    const std::string tmp = afterFirst(fact, '=', IfNotFoundReturn::none);
                    typeFact = beforeFirst(tmp, ':', IfNotFoundReturn::all);
                }
                else if (startsWithAsciiNoCase(fact, "size="))
                    fileSize = stringTo<uint64_t>(afterFirst(fact, '=', IfNotFoundReturn::none));
                else if (startsWithAsciiNoCase(fact, "modify="))
                {
                    std::string modifyFact = afterFirst(fact, '=', IfNotFoundReturn::none);
                    modifyFact = beforeLast(modifyFact, '.', IfNotFoundReturn::all); //truncate millisecond precision if available

                    const TimeComp tc = parseTime("%Y%m%d%H%M%S", modifyFact);
                    if (tc == TimeComp())
                        throw SysError(L"Modification time could not be parsed.");

                    item.modTime = utcToTimeT(tc); //returns -1 on error
                    if (item.modTime == -1)
                        throw SysError(L"Modification time could not be parsed.");
                }
                else if (startsWithAsciiNoCase(fact, "unique="))
                {
                    /*  https://tools.ietf.org/html/rfc3659#section-7.5.2
                        "The mapping between files, and unique fact tokens should be maintained, [...] for
                         *at least* the lifetime of the control connection from user-PI to server-PI."

                        => not necessarily *persistent* as far as the RFC goes!
                           BUT: practially this will be the inode ID/file index, so we can assume persistence */
                    const std::string uniqueId = afterFirst(fact, '=', IfNotFoundReturn::none);
                    assert(!uniqueId.empty());
                    item.filePrint = hashArray<AFS::FingerPrint>(uniqueId.begin(), uniqueId.end());
                    //other metadata to hash e.g. create fact? => not available on Linux-hosted FTP!
                }

            if (equalAsciiNoCase(typeFact, "cdir"))
                return {AFS::ItemType::folder, Zstr("."), 0, 0};
            if (equalAsciiNoCase(typeFact, "pdir"))
                return {AFS::ItemType::folder, Zstr(".."), 0, 0};

            if (equalAsciiNoCase(typeFact, "dir"))
                item.type = AFS::ItemType::folder;
            else if (equalAsciiNoCase(typeFact, "OS.unix=slink") || //the OS.unix=slink:/target syntax is a hack and often skips
                     equalAsciiNoCase(typeFact, "OS.unix=symlink")) //the target path after the colon: http://www.proftpd.org/docs/modules/mod_facts.html
                item.type = AFS::ItemType::symlink;
            //It may be a good idea to NOT check for type "file" explicitly: see comment in native.cpp

            //evaluate parsing errors right now (+ report raw entry in error message!)
            if (item.itemName.empty())
                throw SysError(L"Item name not available.");

            if (item.type == AFS::ItemType::file)
            {
                if (!fileSize)
                    throw SysError(L"File size not available.");
                item.fileSize = *fileSize;
            }
            return item;
        }
        catch (const SysError& e)
        {
            throw SysError(L"Unexpected FTP response. (" + utfTo<std::wstring>(rawLine) + L") " + e.toString());
        }
    }

    static std::vector<FtpItem> parseUnknown(const std::string& buf, ServerEncoding enc) //throw SysError
    {
        if (!buf.empty() && isDigit(buf[0])) //lame test to distinguish Unix/Dos formats as internally used by libcurl
            return parseWindows(buf, enc); //throw SysError
        return parseUnix(buf, enc);        //
    }

    //"ls -l"
    static std::vector<FtpItem> parseUnix(const std::string& buf, ServerEncoding enc) //throw SysError
    {
        const std::vector<std::string> lines = splitFtpResponse(buf);
        auto it = lines.begin();

        if (it != lines.end() && startsWith(*it, "total "))
            ++it;

        const time_t utcTimeNow = std::time(nullptr);
        const TimeComp tc = getUtcTime(utcTimeNow);
        if (tc == TimeComp())
            throw SysError(L"Failed to determine current time: " + numberTo<std::wstring>(utcTimeNow));

        const int utcCurrentYear = tc.year;

        std::optional<bool> unixListingHaveGroup_; //different listing format: better store at session level!?
        std::vector<FtpItem> output;

        for (; it != lines.end(); ++it)
        {
            //unix listing without group: https://freefilesync.org/forum/viewtopic.php?t=4306
            if (!unixListingHaveGroup_)
                unixListingHaveGroup_ = [&]
            {
                try
                {
                    parseUnixLine(*it, utcTimeNow, utcCurrentYear, true /*haveGroup*/, enc); //throw SysError
                    return true;
                }
                catch (SysError&)
                {
                    try
                    {
                        parseUnixLine(*it, utcTimeNow, utcCurrentYear, false /*haveGroup*/, enc); //throw SysError
                        return false;
                    }
                    catch (SysError&) {}
                    throw;
                }
            }();

            const FtpItem item = parseUnixLine(*it, utcTimeNow, utcCurrentYear, *unixListingHaveGroup_, enc); //throw SysError
            if (item.itemName != Zstr(".") &&
                item.itemName != Zstr(".."))
                output.push_back(item);
        }

        return output;
    }

    static FtpItem parseUnixLine(const std::string& rawLine, time_t utcTimeNow, int utcCurrentYear, bool haveGroup, ServerEncoding enc) //throw SysError
    {
        /*  total 4953                                                  <- optional first line
            drwxr-xr-x 1 root root    4096 Jan 10 11:58 version
            -rwxr-xr-x 1 root root    1084 Sep  2 01:17 Unit Test.vcxproj.user
            -rwxr-xr-x 1 1000  300    2217 Feb 28  2016 win32.manifest
            lrwxr-xr-x 1 root root      18 Apr 26 15:17 Projects -> /mnt/hgfs/Projects

        file type: -:file  l:symlink  d:directory  b:block device  p:named pipe  c:char device  s:socket

        permissions: (r|-)(w|-)(x|s|S|-)    user
                     (r|-)(w|-)(x|s|S|-)    group  s := S + x      S = Setgid
                     (r|-)(w|-)(x|t|T|-)    others t := T + x      T = sticky bit

        Alternative formats:
           Unix, no group ("ls -alG") https://freefilesync.org/forum/viewtopic.php?t=4306
               dr-xr-xr-x   2 root                  512 Apr  8  1994 etc

        Yet to be seen in the wild:
           Netware:
               d [R----F--] supervisor            512       Jan 16 18:53    login
               - [R----F--] rhesus             214059       Oct 20 15:27    cx.exe

           NetPresenz for the Mac:
           -------r--         326  1391972  1392298 Nov 22  1995 MegaPhone.sit
           drwxrwxr-x               folder        2 May 10  1996 network                     */
        try
        {
            FtpLineParser parser(rawLine);

            const std::string typeTag = parser.readRange(1, [](char c) //throw SysError
            {
                return c == '-' || c == 'b' || c == 'c' || c == 'd' || c == 'l' || c == 'p' || c == 's';
            });
            //------------------------------------------------------------------------------------
            //permissions
            parser.readRange(9, [](char c) //throw SysError
            {
                return c == '-' || c == 'r' || c == 'w' || c == 'x' || c == 's' || c == 'S' || c == 't' || c == 'T';
            });
            parser.readRange(&isWhiteSpace<char>); //throw SysError
            //------------------------------------------------------------------------------------
            //hard-link count (no separators)
            parser.readRange(&isDigit<char>);      //throw SysError
            parser.readRange(&isWhiteSpace<char>); //throw SysError
            //------------------------------------------------------------------------------------
            //user
            parser.readRange(std::not_fn(isWhiteSpace<char>)); //throw SysError
            parser.readRange(&isWhiteSpace<char>);             //throw SysError
            //------------------------------------------------------------------------------------
            //group
            if (haveGroup)
            {
                parser.readRange(std::not_fn(isWhiteSpace<char>)); //throw SysError
                parser.readRange(&isWhiteSpace<char>);             //throw SysError
            }
            //------------------------------------------------------------------------------------
            //file size (no separators)
            const uint64_t fileSize = stringTo<uint64_t>(parser.readRange(&isDigit<char>)); //throw SysError
            parser.readRange(&isWhiteSpace<char>);                                          //throw SysError
            //------------------------------------------------------------------------------------
            const std::string monthStr = parser.readRange(std::not_fn(isWhiteSpace<char>)); //throw SysError
            parser.readRange(&isWhiteSpace<char>);                                                  //throw SysError

            const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            auto itMonth = std::find_if(std::begin(months), std::end(months), [&](const char* name) { return equalAsciiNoCase(name, monthStr); });
            if (itMonth == std::end(months))
                throw SysError(L"Failed to parse month name.");
            //------------------------------------------------------------------------------------
            const int day = stringTo<int>(parser.readRange(&isDigit<char>)); //throw SysError
            parser.readRange(&isWhiteSpace<char>);                           //throw SysError
            if (day < 1 || day > 31)
                throw SysError(L"Failed to parse day of month.");
            //------------------------------------------------------------------------------------
            const std::string timeOrYear = parser.readRange([](char c) { return c == ':' || isDigit(c); }); //throw SysError
            parser.readRange(&isWhiteSpace<char>);                                                          //throw SysError

            TimeComp timeComp;
            timeComp.month = 1 + static_cast<int>(itMonth - std::begin(months));
            timeComp.day = day;

            if (contains(timeOrYear, ':'))
            {
                const int hour   = stringTo<int>(beforeFirst(timeOrYear, ':', IfNotFoundReturn::none));
                const int minute = stringTo<int>(afterFirst (timeOrYear, ':', IfNotFoundReturn::none));
                if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
                    throw SysError(L"Failed to parse file time.");

                timeComp.hour   = hour;
                timeComp.minute = minute;
                timeComp.year = utcCurrentYear; //tentatively
                const time_t serverLocalTime = utcToTimeT(timeComp); //returns -1 on error
                if (serverLocalTime == -1)
                    throw SysError(L"Modification time could not be parsed.");

                if (serverLocalTime - utcTimeNow > 3600 * 24) //time-zones range from UTC-12:00 to UTC+14:00, consider DST; FileZilla uses 1 day tolerance
                    --timeComp.year; //"more likely" this time is from last year
            }
            else if (timeOrYear.size() == 4)
            {
                timeComp.year = stringTo<int>(timeOrYear);

                if (timeComp.year < 1600 || timeComp.year > utcCurrentYear + 1 /*leeway*/)
                    throw SysError(L"Failed to parse file time.");
            }
            else
                throw SysError(L"Failed to parse file time.");

            //let's pretend the time listing is UTC (same behavior as FileZilla): hopefully MLSD will make this mess obsolete soon...
            //  => find exact offset with some MDTM hackery? yes, could do that, but this doesn't solve the bigger problem of imprecise LIST file times, so why bother?
            const time_t utcTime = utcToTimeT(timeComp); //returns -1 on error
            if (utcTime == -1)
                throw SysError(L"Modification time could not be parsed.");
            //------------------------------------------------------------------------------------
            const std::string trail = parser.readRange([](char) { return true; }); //throw SysError
            std::string itemName;
            if (typeTag == "l")
                itemName = beforeFirst(trail, " -> ", IfNotFoundReturn::none);
            else
                itemName = trail;
            if (itemName.empty())
                throw SysError(L"Item name not available.");

            if (itemName == "." || itemName == "..") //sometimes returned, e.g. by freefilesync.org
                return {AFS::ItemType::folder, utfTo<Zstring>(itemName), 0, 0};
            //------------------------------------------------------------------------------------
            FtpItem item;
            if (typeTag == "d")
                item.type = AFS::ItemType::folder;
            else if (typeTag == "l")
                item.type = AFS::ItemType::symlink;
            else
                item.fileSize = fileSize;

            item.itemName = serverToUtfEncoding(itemName, enc); //throw SysError
            item.modTime = utcTime;

            return item;
        }
        catch (const SysError& e)
        {
            throw SysError(L"Unexpected FTP response. (" + utfTo<std::wstring>(rawLine) + L')' + (haveGroup ? L"" : L" [no-group]") + L' ' + e.toString());
        }
    }


    //"dir"
    static std::vector<FtpItem> parseWindows(const std::string& buf, ServerEncoding enc) //throw SysError
    {
        /*  Test server: test.rebex.net username:demo pw:password  useTls = true

            listing supported by libcurl (US server)
                10-27-15  03:46AM       <DIR>          pub
                04-08-14  03:09PM               11,399 readme.txt

            Datalogic Windows CE 5.0
                01-01-98  13:00       <DIR>          Storage Card

            IIS option "four-digit years"
                06-22-2017  04:25PM       <DIR>          test
                06-20-2017  12:50PM              1875499 zstring.obj

            Alternative formats (yet to be seen in the wild)
                "dir" on Windows, US:
                    10/27/2015  03:46 AM  <DIR>          pub
                    04/08/2014  03:09 PM          11,399 readme.txt

                "dir" on Windows, German:
                    21.09.2016  18:31    <DIR>          Favorites
                    12.01.2017  19:57            11.399 gsview64.ini        */

        const TimeComp tc = getUtcTime();
        if (tc == TimeComp())
            throw SysError(L"Failed to determine current time: " + numberTo<std::wstring>(std::time(nullptr)));
        const int utcCurrentYear = tc.year;

        std::vector<FtpItem> output;
        for (const std::string& line : splitFtpResponse(buf))
        {
            try
            {
                FtpLineParser parser(line);

                const int month = stringTo<int>(parser.readRange(2, &isDigit<char>));     //throw SysError
                parser.readRange(1, [](char c) { return c == '-' || c == '/'; });         //throw SysError
                const int day = stringTo<int>(parser.readRange(2, &isDigit<char>));       //throw SysError
                parser.readRange(1, [](char c) { return c == '-' || c == '/'; });         //throw SysError
                const std::string yearString = parser.readRange(&isDigit<char>);          //throw SysError
                parser.readRange(&isWhiteSpace<char>);                                    //throw SysError

                if (month < 1 || month > 12 || day < 1 || day > 31)
                    throw SysError(L"Failed to parse file time.");

                int year = 0;
                if (yearString.size() == 2)
                {
                    year = (utcCurrentYear / 100) * 100 + stringTo<int>(yearString);
                    if (year > utcCurrentYear + 1 /*local time leeway*/)
                        year -= 100;
                }
                else if (yearString.size() == 4)
                    year = stringTo<int>(yearString);
                else
                    throw SysError(L"Failed to parse file time.");
                //------------------------------------------------------------------------------------
                int hour = stringTo<int>(parser.readRange(2, &isDigit<char>));         //throw SysError
                parser.readRange(1, [](char c) { return c == ':'; });                  //throw SysError
                const int minute = stringTo<int>(parser.readRange(2, &isDigit<char>)); //throw SysError
                if (!isWhiteSpace(parser.peekNextChar()))
                {
                    const std::string period = parser.readRange(2, [](char c) { return c == 'A' || c == 'P' || c == 'M'; }); //throw SysError
                    if (period == "PM")
                    {
                        if (0 <= hour && hour < 12)
                            hour += 12;
                    }
                    else if (hour == 12)
                        hour = 0;
                }
                parser.readRange(&isWhiteSpace<char>); //throw SysError

                if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
                    throw SysError(L"Failed to parse file time.");
                //------------------------------------------------------------------------------------
                TimeComp timeComp;
                timeComp.year   = year;
                timeComp.month  = month;
                timeComp.day    = day;
                timeComp.hour   = hour;
                timeComp.minute = minute;
                //let's pretend the time listing is UTC (same behavior as FileZilla): hopefully MLSD will make this mess obsolete soon...
                //  => find exact offset with some MDTM hackery? yes, could do that, but this doesn't solve the bigger problem of imprecise LIST file times, so why bother?
                const time_t utcTime = utcToTimeT(timeComp); //returns -1 on error
                if (utcTime == -1)
                    throw SysError(L"Modification time could not be parsed.");
                //------------------------------------------------------------------------------------
                const std::string dirTagOrSize = parser.readRange(std::not_fn(isWhiteSpace<char>)); //throw SysError
                parser.readRange(&isWhiteSpace<char>); //throw SysError

                const bool isDir = dirTagOrSize == "<DIR>";
                uint64_t fileSize = 0;
                if (!isDir)
                {
                    std::string sizeStr = dirTagOrSize;
                    replace(sizeStr, ',', "");
                    replace(sizeStr, '.', "");
                    if (!std::all_of(sizeStr.begin(), sizeStr.end(), &isDigit<char>))
                        throw SysError(L"Failed to parse file size.");
                    fileSize = stringTo<uint64_t>(sizeStr);
                }
                //------------------------------------------------------------------------------------
                const std::string itemName = parser.readRange([](char) { return true; }); //throw SysError
                if (itemName.empty())
                    throw SysError(L"Folder contains an item without name.");

                //------------------------------------------------------------------------------------
                if (itemName != "." &&
                    itemName != "..")
                {
                    FtpItem item;
                    if (isDir)
                        item.type = AFS::ItemType::folder;
                    item.itemName = serverToUtfEncoding(itemName, enc); //throw SysError
                    item.fileSize = fileSize;
                    item.modTime  = utcTime;

                    output.push_back(item);
                }
            }
            catch (const SysError& e)
            {
                throw SysError(L"Unexpected FTP response. (" + utfTo<std::wstring>(line) + L") " + e.toString());
            }
        }

        return output;
    }
};


class SingleFolderTraverser
{
public:
    SingleFolderTraverser(const FtpLogin& login, const std::vector<std::pair<AfsPath, std::shared_ptr<AFS::TraverserCallback>>>& workload /*throw X*/)
        : workload_(workload), login_(login)
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

    void traverseWithException(const AfsPath& dirPath, AFS::TraverserCallback& cb) //throw FileError, X
    {
        for (const FtpItem& item : FtpDirectoryReader::execute(login_, dirPath)) //throw FileError
        {
            const AfsPath itemPath(nativeAppendPaths(dirPath.value, item.itemName));

            switch (item.type)
            {
                case AFS::ItemType::file:
                    cb.onFile({item.itemName, item.fileSize, item.modTime, item.filePrint, false /*isFollowedSymlink*/}); //throw X
                    break;

                case AFS::ItemType::folder:
                    if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb.onFolder({item.itemName, false /*isFollowedSymlink*/})) //throw X
                        workload_.push_back({itemPath, std::move(cbSub)});
                    break;

                case AFS::ItemType::symlink:
                    switch (cb.onSymlink({item.itemName, item.modTime})) //throw X
                    {
                        case AFS::TraverserCallback::HandleLink::follow:
                        {
                            FtpItem target = {};
                            if (!tryReportingItemError([&] //throw X
                        {
                            target = getFtpSymlinkInfo(login_, itemPath); //throw FileError
                            }, cb, item.itemName))
                            continue;

                            if (target.type == AFS::ItemType::folder)
                            {
                                if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb.onFolder({item.itemName, true /*isFollowedSymlink*/})) //throw X
                                    workload_.push_back({itemPath, std::move(cbSub)});
                            }
                            else //a file or named pipe, etc.
                                cb.onFile({item.itemName, target.fileSize, target.modTime, item.filePrint, true /*isFollowedSymlink*/}); //throw X
                        }
                        break;

                        case AFS::TraverserCallback::HandleLink::skip:
                            break;
                    }
                    break;
            }
        }
    }

    std::vector<std::pair<AfsPath, std::shared_ptr<AFS::TraverserCallback>>> workload_;
    const FtpLogin login_;
};


void traverseFolderRecursiveFTP(const FtpLogin& login, const std::vector<std::pair<AfsPath, std::shared_ptr<AFS::TraverserCallback>>>& workload /*throw X*/, size_t) //throw X
{
    SingleFolderTraverser dummy(login, workload); //throw X
}
//===========================================================================================================================
//===========================================================================================================================

void ftpFileDownload(const FtpLogin& login, const AfsPath& afsFilePath, //throw FileError, X
                     const std::function<void(const void* buffer, size_t bytesToWrite)>& writeBlock /*throw X*/)
{
    std::exception_ptr exception;

    auto onBytesReceived = [&](const void* buffer, size_t len)
    {
        try
        {
            writeBlock(buffer, len); //throw X
            return len;
        }
        catch (...)
        {
            exception = std::current_exception();
            return len + 1; //signal error condition => CURLE_WRITE_ERROR
        }
    };

    using CbType = decltype(onBytesReceived);
    using CbWrapperType =          size_t (*)(const void* buffer, size_t size, size_t nitems, CbType* callbackData); //needed for cdecl function pointer cast
    CbWrapperType onBytesReceivedWrapper = [](const void* buffer, size_t size, size_t nitems, CbType* callbackData)
    {
        return (*callbackData)(buffer, size * nitems); //free this poor little C-API from its shackles and redirect to a proper lambda
    };

    try
    {
        accessFtpSession(login, [&](FtpSession& session) //throw SysError
        {
            session.perform(afsFilePath, false /*isDir*/, CURLFTPMETHOD_NOCWD, //are there any servers that require CURLFTPMETHOD_SINGLECWD? let's find out
            {
                {CURLOPT_WRITEDATA, &onBytesReceived},
                {CURLOPT_WRITEFUNCTION, onBytesReceivedWrapper},
                {CURLOPT_IGNORE_CONTENT_LENGTH, 1L}, //skip FTP "SIZE" command before download (=> download until actual EOF if file size changes)
            }, true /*requiresUtf8*/, login.timeoutSec); //throw SysError
        });
    }
    catch (const SysError& e)
    {
        if (exception)
            std::rethrow_exception(exception);

        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getCurlDisplayPath(login.server, afsFilePath))), e.toString());
    }
}


/* File already existing:
    freefilesync.org: overwrites
    FileZilla Server: overwrites
    Windows IIS:      overwrites                          */
void ftpFileUpload(const FtpLogin& login, const AfsPath& afsFilePath, //throw FileError, X
                   const std::function<size_t(void* buffer, size_t bytesToRead)>& readBlock /*throw X*/) //returning 0 signals EOF: Posix read() semantics
{
    std::exception_ptr exception;

    auto getBytesToSend = [&](void* buffer, size_t len) -> size_t
    {
        try
        {
            //libcurl calls back until 0 bytes are returned (Posix read() semantics), or,
            //if CURLOPT_INFILESIZE_LARGE was set, after exactly this amount of bytes
            const size_t bytesRead = readBlock(buffer, len);//throw X; return "bytesToRead" bytes unless end of stream!
            return bytesRead;
        }
        catch (...)
        {
            exception = std::current_exception();
            return CURL_READFUNC_ABORT; //signal error condition => CURLE_ABORTED_BY_CALLBACK
        }
    };

    using CbType = decltype(getBytesToSend);
    using CbWrapperType =         size_t (*)(void* buffer, size_t size, size_t nitems, CbType* callbackData);
    CbWrapperType getBytesToSendWrapper = [](void* buffer, size_t size, size_t nitems, CbType* callbackData)
    {
        return (*callbackData)(buffer, size * nitems); //free this poor little C-API from its shackles and redirect to a proper lambda
    };

    try
    {
        accessFtpSession(login, [&](FtpSession& session) //throw SysError
        {
            /*
                curl_slist* quote = nullptr;
                ZEN_ON_SCOPE_EXIT(::curl_slist_free_all(quote));

                //"prefix the command with an asterisk to make libcurl continue even if the command fails"
                quote = ::curl_slist_append(quote, ("*DELE " + session.getServerPathInternal(afsFilePath)).c_str()); //throw SysError

                //optimize fail-safe copy with RNFR/RNTO as CURLOPT_POSTQUOTE? -> even slightly *slower* than RNFR/RNTO as additional curl_easy_perform()
            */
            session.perform(afsFilePath, false /*isDir*/, CURLFTPMETHOD_NOCWD, //are there any servers that require CURLFTPMETHOD_SINGLECWD? let's find out
            {
                {CURLOPT_UPLOAD, 1L},
                {CURLOPT_READDATA, &getBytesToSend},
                {CURLOPT_READFUNCTION, getBytesToSendWrapper},

                //{CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(inputBuffer.size())},
                //=> CURLOPT_INFILESIZE_LARGE does not issue a specific FTP command, but is used by libcurl only!

                //{CURLOPT_PREQUOTE,  quote},
                //{CURLOPT_POSTQUOTE, quote},
            }, true /*requiresUtf8*/, login.timeoutSec); //throw SysError
        });
    }
    catch (const SysError& e)
    {
        if (exception)
            std::rethrow_exception(exception);

        throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getCurlDisplayPath(login.server, afsFilePath))), e.toString());
    }
}

//===========================================================================================================================

struct InputStreamFtp : public AFS::InputStream
{
    InputStreamFtp(const FtpLogin& login,
                   const AfsPath& afsPath,
                   const IoCallback& notifyUnbufferedIO /*throw X*/) :
        notifyUnbufferedIO_(notifyUnbufferedIO)
    {
        worker_ = InterruptibleThread([asyncStreamOut = this->asyncStreamIn_, login, afsPath]
        {
            setCurrentThreadName(Zstr("Istream[FTP] ") + utfTo<Zstring>(getCurlDisplayPath(login.server, afsPath)));
            try
            {
                auto writeBlock = [&](const void* buffer, size_t bytesToWrite)
                {
                    return asyncStreamOut->write(buffer, bytesToWrite); //throw ThreadStopRequest
                };
                ftpFileDownload(login, afsPath, writeBlock); //throw FileError, ThreadStopRequest

                asyncStreamOut->closeStream();
            }
            catch (FileError&) { asyncStreamOut->setWriteError(std::current_exception()); } //let ThreadStopRequest pass through!
        });
    }

    ~InputStreamFtp()
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
        return {}; //there is no stream handle => no buffered attribute access!
        //PERF: get attributes during file download?
        //  CURLOPT_FILETIME:                                           test case 77 files, 4MB: overall copy time increases by 12%
        //  CURLOPT_PREQUOTE/CURLOPT_PREQUOTE/CURLOPT_POSTQUOTE + MDTM: test case 77 files, 4MB: overall copy time increases by 12%
    }

private:
    void reportBytesProcessed() //throw X
    {
        const int64_t totalBytesDownloaded = asyncStreamIn_->getTotalBytesWritten();
        if (notifyUnbufferedIO_) notifyUnbufferedIO_(totalBytesDownloaded - totalBytesReported_); //throw X
        totalBytesReported_ = totalBytesDownloaded;
    }

    const IoCallback notifyUnbufferedIO_; //throw X
    int64_t totalBytesReported_ = 0;
    std::shared_ptr<AsyncStreamBuffer> asyncStreamIn_ = std::make_shared<AsyncStreamBuffer>(FTP_STREAM_BUFFER_SIZE);
    InterruptibleThread worker_;
};

//===========================================================================================================================

//CAVEAT: if upload fails due to already existing, OutputStreamFtp constructor does not fail, but OutputStreamFtp::write() does!
//  => ~OutputStreamImpl() will delete the already existing file!
struct OutputStreamFtp : public AFS::OutputStreamImpl
{
    OutputStreamFtp(const FtpLogin& login,
                    const AfsPath& afsPath,
                    std::optional<time_t> modTime,
                    const IoCallback& notifyUnbufferedIO /*throw X*/) :
        login_(login),
        afsPath_(afsPath),
        modTime_(modTime),
        notifyUnbufferedIO_(notifyUnbufferedIO)
    {
        std::promise<void> pUploadDone;
        futUploadDone_ = pUploadDone.get_future();

        worker_ = InterruptibleThread([login, afsPath,
                                              asyncStreamIn = this->asyncStreamOut_,
                                              pUploadDone   = std::move(pUploadDone)]() mutable
        {
            setCurrentThreadName(Zstr("Ostream[FTP] ") + utfTo<Zstring>(getCurlDisplayPath(login.server, afsPath)));
            try
            {
                auto readBlock = [&](void* buffer, size_t bytesToRead)
                {
                    //returns "bytesToRead" bytes unless end of stream! => maps nicely into Posix read() semantics expected by ftpFileUpload()
                    return asyncStreamIn->read(buffer, bytesToRead); //throw ThreadStopRequest
                };
                ftpFileUpload(login, afsPath, readBlock); //throw FileError, ThreadStopRequest
                assert(asyncStreamIn->getTotalBytesRead() == asyncStreamIn->getTotalBytesWritten());

                pUploadDone.set_value();
            }
            catch (FileError&)
            {
                const std::exception_ptr exptr = std::current_exception();
                asyncStreamIn->setReadError(exptr); //set both!
                pUploadDone.set_exception(exptr);   //
            }
            //let ThreadStopRequest pass through!
        });
    }

    ~OutputStreamFtp()
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

        while (futUploadDone_.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout)
            reportBytesProcessed(); //throw X
        reportBytesProcessed(); //[!] once more, now that *all* bytes were written

        asyncStreamOut_->checkReadErrors(); //throw FileError
        //--------------------------------------------------------------------

        assert(isReady(futUploadDone_));
        futUploadDone_.get(); //throw FileError

        AFS::FinalizeResult result;
        //result.filePrint = ... -> yet unknown at this point
        try
        {
            setModTimeIfAvailable(); //throw FileError, follows symlinks
            /* is setting modtime after closing the file handle a pessimization?
                FTP:    no: could set modtime via CURLOPT_POSTQUOTE (but this would internally trigger an extra round-trip anyway!) */
        }
        catch (const FileError& e) { result.errorModTime = FileError(e.toString()); /*avoid slicing*/ }

        return result;
    }

private:
    void reportBytesProcessed() //throw X
    {
        const int64_t totalBytesUploaded = asyncStreamOut_->getTotalBytesRead();
        if (notifyUnbufferedIO_) notifyUnbufferedIO_(totalBytesUploaded - totalBytesReported_); //throw X
        totalBytesReported_ = totalBytesUploaded;
    }

    void setModTimeIfAvailable() const //throw FileError, follows symlinks
    {
        //assert(isReady(futUploadDone_)); => MUST NOT CALL *after* std::future<>::get()!
        if (modTime_)
            try
            {
                const std::string isoTime = utfTo<std::string>(formatTime(Zstr("%Y%m%d%H%M%S"), getUtcTime(*modTime_))); //returns empty string on failure
                if (isoTime.empty())
                    throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(*modTime_) + L')');

                accessFtpSession(login_, [&](FtpSession& session) //throw SysError
                {
                    if (!session.supportsMfmt(login_.timeoutSec)) //throw SysError
                        throw SysError(L"Server does not support the MFMT command.");

                    session.runSingleFtpCommand("MFMT " + isoTime + ' ' + session.getServerPathInternal(afsPath_, login_.timeoutSec),
                                                true /*requiresUtf8*/, login_.timeoutSec); //throw SysError
                    //not relevant for OutputStreamFtp, but: does MFMT follow symlinks? for Linux FTP server (using utime) it does
                });
            }
            catch (const SysError& e)
            {
                throw FileError(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(getCurlDisplayPath(login_.server, afsPath_))), e.toString());
            }
    }

    const FtpLogin login_;
    const AfsPath afsPath_;
    const std::optional<time_t> modTime_;
    const IoCallback notifyUnbufferedIO_; //throw X
    int64_t totalBytesReported_ = 0;
    std::shared_ptr<AsyncStreamBuffer> asyncStreamOut_ = std::make_shared<AsyncStreamBuffer>(FTP_STREAM_BUFFER_SIZE);
    InterruptibleThread worker_;
    std::future<void> futUploadDone_;
};

//---------------------------------------------------------------------------------------------------------------------------
//===========================================================================================================================

class FtpFileSystem : public AbstractFileSystem
{
public:
    explicit FtpFileSystem(const FtpLogin& login) : login_(login) {}

    const FtpLogin& getLogin() const { return login_; }

private:
    Zstring getInitPathPhrase(const AfsPath& afsPath) const override { return concatenateFtpFolderPathPhrase(login_, afsPath); }

    std::wstring getDisplayPath(const AfsPath& afsPath) const override { return getCurlDisplayPath(login_.server, afsPath); }

    bool isNullFileSystem() const override { return login_.server.empty(); }

    std::weak_ordering compareDeviceSameAfsType(const AbstractFileSystem& afsRhs) const override
    {
        const FtpLogin& lhs = login_;
        const FtpLogin& rhs = static_cast<const FtpFileSystem&>(afsRhs).login_;

        //exactly the type of case insensitive comparison we need for server names!
        if (const std::weak_ordering cmp = compareAsciiNoCase(lhs.server, rhs.server); //https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfow#IDNs
            std::is_neq(cmp))
            return cmp;

        //port does NOT create a *different* data source!!! -> same thing for password!

        //username: usually *does* create different folder view for FTP
        return lhs.username <=> rhs.username; //case sensitive!
    }

    //----------------------------------------------------------------------------------------------------------------

    ItemType getItemType(const AfsPath& afsPath) const override //throw FileError
    {
        //don't use MLST: broken for Pure-FTPd: https://freefilesync.org/forum/viewtopic.php?t=4287

        const std::optional<AfsPath> parentAfsPath = getParentPath(afsPath);
        if (!parentAfsPath) //device root => do a quick access tests to see if the server responds at all!
            try
            {
                accessFtpSession(login_, [&](FtpSession& session) //throw SysError
                {
                    session.testConnection(login_.timeoutSec); //throw SysError
                });
                return ItemType::folder;
            }
            catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(getCurlDisplayPath(login_.server, afsPath))), e.toString()); }

        const Zstring itemName = getItemName(afsPath);
        assert(!itemName.empty());
        try
        {
            //is the underlying file system case-sensitive? we don't know => assume "case-sensitive"
            //=> all path parts (except the base folder part!) can be expected to have the right case anyway after traversal
            traverseFolderFlat(*parentAfsPath, //throw FileError
            [&](const    FileInfo& fi) { if (fi.itemName == itemName) throw ItemType::file;    },
            [&](const  FolderInfo& fi) { if (fi.itemName == itemName) throw ItemType::folder;  },
            [&](const SymlinkInfo& si) { if (si.itemName == itemName) throw ItemType::symlink; });
        }
        catch (const ItemType& type) { return type; } //yes, exceptions for control-flow are bad design... but, but...

        throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getDisplayPath(afsPath))), L"File not found.");
    }

    std::optional<ItemType> itemStillExists(const AfsPath& afsPath) const override //throw FileError
    {
        const std::optional<AfsPath> parentAfsPath = getParentPath(afsPath);
        if (!parentAfsPath) //device root
            return getItemType(afsPath); //throw FileError; do a simple access test

        const Zstring itemName = getItemName(afsPath);
        assert(!itemName.empty());
        try
        {
            traverseFolderFlat(*parentAfsPath, //throw FileError
            [&](const    FileInfo& fi) { if (fi.itemName == itemName) throw ItemType::file;    },
            [&](const  FolderInfo& fi) { if (fi.itemName == itemName) throw ItemType::folder;  },
            [&](const SymlinkInfo& si) { if (si.itemName == itemName) throw ItemType::symlink; });
        }
        catch (const ItemType& type) { return type; } //yes, exceptions for control-flow are bad design... but, but...
        catch (FileError&)
        {
            const std::optional<ItemType> parentType = itemStillExists(*parentAfsPath); //throw FileError
            if (parentType && *parentType != ItemType::file) //obscure, but possible (and not an error)
                throw; //parent path existing, so traversal should not have failed!
        }
        return {};
    }
    //----------------------------------------------------------------------------------------------------------------

    //already existing: fail
    //=> FTP will (most likely) fail and give a clear error message:
    //      freefilesync.org: "550 Can't create directory: File exists"
    //      FileZilla Server: "550 Directory already exists"
    //      Windows IIS:      "550 Cannot create a file when that file already exists"
    void createFolderPlain(const AfsPath& afsPath) const override //throw FileError
    {
        try
        {
            accessFtpSession(login_, [&](FtpSession& session) //throw SysError
            {
                session.runSingleFtpCommand("MKD " + session.getServerPathInternal(afsPath, login_.timeoutSec),
                                            true /*requiresUtf8*/, login_.timeoutSec); //throw SysError
            });
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString());
        }
    }

    void removeFilePlain(const AfsPath& afsPath) const override //throw FileError
    {
        try
        {
            accessFtpSession(login_, [&](FtpSession& session) //throw SysError
            {
                session.runSingleFtpCommand("DELE " + session.getServerPathInternal(afsPath, login_.timeoutSec),
                                            true /*requiresUtf8*/, login_.timeoutSec); //throw SysError
            });
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot delete file %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString());
        }
    }

    void removeSymlinkPlain(const AfsPath& afsPath) const override //throw FileError
    {
        this->removeFilePlain(afsPath); //throw FileError
        //works fine for Linux hosts, but what about Windows-hosted FTP??? Distinguish DELE/RMD?
        //Windows test, FileZilla Server and Windows IIS FTP: all symlinks are reported as regular folders
    }

    void removeFolderPlain(const AfsPath& afsPath) const override //throw FileError
    {
        try
        {
            std::optional<SysError> delError;

            accessFtpSession(login_, [&](FtpSession& session) //throw SysError
            {
                try
                {
                    session.runSingleFtpCommand("RMD " + session.getServerPathInternal(afsPath, login_.timeoutSec),
                                                true /*requiresUtf8*/, login_.timeoutSec); //throw SysError
                }
                catch (const SysError& e) { delError = e; }
            });

            if (delError)
            {
                //Windows test, FileZilla Server and Windows IIS FTP: all symlinks are reported as regular folders
                //tested freefilesync.org: RMD will fail for symlinks!
                bool symlinkExists = false;
                try { symlinkExists = getItemType(afsPath) == ItemType::symlink; } /*throw FileError*/ catch (FileError&) {} //previous exception is more relevant

                if (symlinkExists)
                    return removeSymlinkPlain(afsPath); //throw FileError
                else
                    throw* delError;
            }
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(getDisplayPath(afsPath))), e.toString());
        }
    }

    void removeFolderIfExistsRecursion(const AfsPath& afsPath, //throw FileError
                                       const std::function<void (const std::wstring& displayPath)>& onBeforeFileDeletion /*throw X*/, //optional
                                       const std::function<void (const std::wstring& displayPath)>& onBeforeFolderDeletion) const override //one call for each object!
    {
        //default implementation: folder traversal
        AFS::removeFolderIfExistsRecursion(afsPath, onBeforeFileDeletion, onBeforeFolderDeletion); //throw FileError, X
    }

    //----------------------------------------------------------------------------------------------------------------
    AbstractPath getSymlinkResolvedPath(const AfsPath& afsPath) const override //throw FileError
    {
        throw FileError(replaceCpy(_("Cannot determine final path for %x."), L"%x", fmtPath(getDisplayPath(afsPath))), _("Operation not supported by device."));
    }

    bool equalSymlinkContentForSameAfsType(const AfsPath& afsLhs, const AbstractPath& apRhs) const override //throw FileError
    {
        throw FileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(getDisplayPath(afsLhs))), _("Operation not supported by device."));
    }
    //----------------------------------------------------------------------------------------------------------------

    //return value always bound:
    std::unique_ptr<InputStream> getInputStream(const AfsPath& afsPath, const IoCallback& notifyUnbufferedIO /*throw X*/) const override //throw FileError, (ErrorFileLocked)
    {
        return std::make_unique<InputStreamFtp>(login_, afsPath, notifyUnbufferedIO);
    }

    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    //=> actual behavior: fail(+delete!)/overwrite/auto-rename
    std::unique_ptr<OutputStreamImpl> getOutputStream(const AfsPath& afsPath, //throw FileError
                                                      std::optional<uint64_t> streamSize,
                                                      std::optional<time_t> modTime,
                                                      const IoCallback& notifyUnbufferedIO /*throw X*/) const override
    {
        /* most FTP servers overwrite, but some (e.g. IIS) can be configured to fail, others (pureFTP) can be configured to auto-rename:
           https://download.pureftpd.org/pub/pure-ftpd/doc/README
           '-r': Never overwrite existing files. Uploading a file whose name already exists causes an automatic rename. Files are called xyz, xyz.1, xyz.2, xyz.3, etc. */

        //already existing: fail (+ delete!!!)
        return std::make_unique<OutputStreamFtp>(login_, afsPath, modTime, notifyUnbufferedIO);
    }

    //----------------------------------------------------------------------------------------------------------------
    void traverseFolderRecursive(const TraverserWorkload& workload /*throw X*/, size_t parallelOps) const override
    {
        traverseFolderRecursiveFTP(login_, workload, parallelOps); //throw X
    }
    //----------------------------------------------------------------------------------------------------------------

    //symlink handling: follow
    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    FileCopyResult copyFileForSameAfsType(const AfsPath& afsSource, const StreamAttributes& attrSource, //throw FileError, (ErrorFileLocked), X
                                          const AbstractPath& apTarget, bool copyFilePermissions, const IoCallback& notifyUnbufferedIO /*throw X*/) const override
    {
        //no native FTP file copy => use stream-based file copy:
        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(AFS::getDisplayPath(apTarget))), _("Operation not supported by device."));

        //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
        return copyFileAsStream(afsSource, attrSource, apTarget, notifyUnbufferedIO); //throw FileError, (ErrorFileLocked), X
    }

    //symlink handling: follow
    //already existing: fail
    void copyNewFolderForSameAfsType(const AfsPath& afsSource, const AbstractPath& apTarget, bool copyFilePermissions) const override //throw FileError
    {
        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(AFS::getDisplayPath(apTarget))), _("Operation not supported by device."));

        //already existing: fail
        AFS::createFolderPlain(apTarget); //throw FileError
    }

    //already existing: fail
    void copySymlinkForSameAfsType(const AfsPath& afsSource, const AbstractPath& apTarget, bool copyFilePermissions) const override
    {
        throw FileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."),
                                              L"%x", L'\n' + fmtPath(getDisplayPath(afsSource))),
                                   L"%y", L'\n' + fmtPath(AFS::getDisplayPath(apTarget))), _("Operation not supported by device."));
    }

    //already existing: undefined behavior! (e.g. fail/overwrite)
    //=> actual behavior: most linux-based FTP servers overwrite, Windows-based servers fail (but most can be configured to behave differently)
    //      freefilesync.org: silent overwrite
    //      Windows IIS:      CURLE_QUOTE_ERROR: QUOT command failed with 550 Cannot create a file when that file already exists.
    //      FileZilla Server: CURLE_QUOTE_ERROR: QUOT command failed with 553 file exists
    void moveAndRenameItemForSameAfsType(const AfsPath& pathFrom, const AbstractPath& pathTo) const override //throw FileError, ErrorMoveUnsupported
    {
        auto generateErrorMsg = [&] { return replaceCpy(replaceCpy(_("Cannot move file %x to %y."),
                                                                   L"%x", L'\n' + fmtPath(getDisplayPath(pathFrom))),
                                                        L"%y", L'\n' + fmtPath(AFS::getDisplayPath(pathTo)));
                                    };

        if (std::is_neq(compareDeviceSameAfsType(pathTo.afsDevice.ref())))
            throw ErrorMoveUnsupported(generateErrorMsg(), _("Operation not supported between different devices."));

        try
        {
            accessFtpSession(login_, [&](FtpSession& session) //throw SysError
            {
                curl_slist* quote = nullptr;
                ZEN_ON_SCOPE_EXIT(::curl_slist_free_all(quote));
                quote = ::curl_slist_append(quote, ("RNFR " + session.getServerPathInternal(pathFrom,       login_.timeoutSec)).c_str()); //throw SysError
                quote = ::curl_slist_append(quote, ("RNTO " + session.getServerPathInternal(pathTo.afsPath, login_.timeoutSec)).c_str()); //

                session.perform(AfsPath(), true /*isDir*/, CURLFTPMETHOD_NOCWD, //avoid needless CWDs
                {
                    {CURLOPT_NOBODY, 1L},
                    {CURLOPT_QUOTE, quote},
                }, true /*requiresUtf8*/, login_.timeoutSec); //throw SysError
            });
        }
        catch (const SysError& e)
        {
            throw FileError(generateErrorMsg(), e.toString());
        }
    }

    bool supportsPermissions(const AfsPath& afsPath) const override { return false; } //throw FileError
    //wait until there is real demand for copying from and to FTP with permissions => use stream-based file copy:

    //----------------------------------------------------------------------------------------------------------------
    FileIconHolder getFileIcon      (const AfsPath& afsPath, int pixelSize) const override { return {}; } //throw SysError; optional return value
    ImageHolder    getThumbnailImage(const AfsPath& afsPath, int pixelSize) const override { return {}; } //throw SysError; optional return value

    void authenticateAccess(bool allowUserInteraction) const override {} //throw FileError

    int getAccessTimeout() const override { return login_.timeoutSec; } //returns "0" if no timeout in force

    bool hasNativeTransactionalCopy() const override { return false; }
    //----------------------------------------------------------------------------------------------------------------

    int64_t getFreeDiskSpace(const AfsPath& afsPath) const override { return -1; } //throw FileError, returns < 0 if not available

    bool supportsRecycleBin(const AfsPath& afsPath) const override { return false; } //throw FileError

    std::unique_ptr<RecycleSession> createRecyclerSession(const AfsPath& afsPath) const override //throw FileError, return value must be bound!
    {
        assert(false); //see supportsRecycleBin()
        throw FileError(L"Recycle bin not supported by device.");
    }

    void recycleItemIfExists(const AfsPath& afsPath) const override //throw FileError
    {
        assert(false); //see supportsRecycleBin()
        throw FileError(replaceCpy(_("Unable to move %x to the recycle bin."), L"%x", fmtPath(getDisplayPath(afsPath))), _("Operation not supported by device."));
    }

    const FtpLogin login_;
};

//===========================================================================================================================

//expects "clean" login data
Zstring concatenateFtpFolderPathPhrase(const FtpLogin& login, const AfsPath& afsPath) //noexcept
{
    Zstring port;
    if (login.port > 0)
        port = Zstr(':') + numberTo<Zstring>(login.port);

    Zstring options;
    if (login.timeoutSec != FtpLogin().timeoutSec)
        options += Zstr("|timeout=") + numberTo<Zstring>(login.timeoutSec);

    if (login.useTls)
        options += Zstr("|ssl");

    if (!login.password.empty()) //password always last => visually truncated by folder input field
        options += Zstr("|pass64=") + encodePasswordBase64(login.password);

    Zstring username;
    if (!login.username.empty())
        username = encodeFtpUsername(login.username) + Zstr("@");

    return Zstring(ftpPrefix) + Zstr("//") + username + login.server + port + getServerRelPath(afsPath) + options;
}
}


void fff::ftpInit()
{
    assert(!globalFtpSessionManager.get());
    globalFtpSessionManager.set(std::make_unique<FtpSessionManager>());
}


void fff::ftpTeardown()
{
    assert(globalFtpSessionManager.get());
    globalFtpSessionManager.set(nullptr);
}


AfsPath fff::getFtpHomePath(const FtpLogin& login) //throw FileError
{
    try
    {
        AfsPath homePath;

        accessFtpSession(login, [&](FtpSession& session) //throw SysError
        {
            homePath = session.getHomePath(login.timeoutSec); //throw SysError
        });
        return homePath;
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot determine final path for %x."), L"%x", fmtPath(getCurlDisplayPath(login.server, AfsPath(Zstr("~"))))), e.toString()); }
}


AfsDevice fff::condenseToFtpDevice(const FtpLogin& login) //noexcept
{
    //clean up input:
    FtpLogin loginTmp = login;
    trim(loginTmp.server);
    trim(loginTmp.username);

    loginTmp.timeoutSec = std::max(1, loginTmp.timeoutSec);

    if (startsWithAsciiNoCase(loginTmp.server, "http:" ) ||
        startsWithAsciiNoCase(loginTmp.server, "https:") ||
        startsWithAsciiNoCase(loginTmp.server, "ftp:"  ) ||
        startsWithAsciiNoCase(loginTmp.server, "ftps:" ) ||
        startsWithAsciiNoCase(loginTmp.server, "sftp:" ))
        loginTmp.server = afterFirst(loginTmp.server, Zstr(':'), IfNotFoundReturn::none);
    trim(loginTmp.server, true, true, [](Zchar c) { return c == Zstr('/') || c == Zstr('\\'); });

    return makeSharedRef<FtpFileSystem>(loginTmp);
}


FtpLogin fff::extractFtpLogin(const AfsDevice& afsDevice) //noexcept
{
    if (const auto ftpDevice = dynamic_cast<const FtpFileSystem*>(&afsDevice.ref()))
        return ftpDevice->getLogin();

    assert(false);
    return {};
}


bool fff::acceptsItemPathPhraseFtp(const Zstring& itemPathPhrase) //noexcept
{
    Zstring path = expandMacros(itemPathPhrase); //expand before trimming!
    trim(path);
    return startsWithAsciiNoCase(path, ftpPrefix); //check for explicit FTP path
}


//syntax: ftp://[<user>[:<password>]@]<server>[:port]/<relative-path>[|option_name=value]
//
//   e.g. ftp://user001:secretpassword@private.example.com:222/mydirectory/
//        ftp://user001@private.example.com/mydirectory|pass64=c2VjcmV0cGFzc3dvcmQ
AbstractPath fff::createItemPathFtp(const Zstring& itemPathPhrase) //noexcept
{
    Zstring pathPhrase = expandMacros(itemPathPhrase); //expand before trimming!
    trim(pathPhrase);

    if (startsWithAsciiNoCase(pathPhrase, ftpPrefix))
        pathPhrase = pathPhrase.c_str() + strLength(ftpPrefix);
    trim(pathPhrase, true, false, [](Zchar c) { return c == Zstr('/') || c == Zstr('\\'); });

    const Zstring credentials = beforeFirst(pathPhrase, Zstr('@'), IfNotFoundReturn::none);
    const Zstring fullPathOpt =  afterFirst(pathPhrase, Zstr('@'), IfNotFoundReturn::all);

    FtpLogin login;
    login.username = decodeFtpUsername(beforeFirst(credentials, Zstr(':'), IfNotFoundReturn::all)); //support standard FTP syntax, even though ':'
    login.password =                    afterFirst(credentials, Zstr(':'), IfNotFoundReturn::none); //is not used by concatenateFtpFolderPathPhrase()!

    const Zstring fullPath = beforeFirst(fullPathOpt, Zstr('|'), IfNotFoundReturn::all);
    const Zstring options  =  afterFirst(fullPathOpt, Zstr('|'), IfNotFoundReturn::none);

    auto it = std::find_if(fullPath.begin(), fullPath.end(), [](Zchar c) { return c == '/' || c == '\\'; });
    const Zstring serverPort(fullPath.begin(), it);
    const AfsPath serverRelPath = sanitizeDeviceRelativePath({it, fullPath.end()});

    login.server       = beforeLast(serverPort, Zstr(':'), IfNotFoundReturn::all);
    const Zstring port =  afterLast(serverPort, Zstr(':'), IfNotFoundReturn::none);
    login.port = stringTo<int>(port); //0 if empty

    if (!options.empty())
    {
        for (const Zstring& optPhrase : split(options, Zstr("|"), SplitOnEmpty::skip))
            if (startsWith(optPhrase, Zstr("timeout=")))
                login.timeoutSec = stringTo<int>(afterFirst(optPhrase, Zstr("="), IfNotFoundReturn::none));
            else if (optPhrase == Zstr("ssl"))
                login.useTls = true;
            else if (startsWith(optPhrase, Zstr("pass64=")))
                login.password = decodePasswordBase64(afterFirst(optPhrase, Zstr("="), IfNotFoundReturn::none));
            else
                assert(false);
    } //fix "-Wdangling-else"

    return AbstractPath(makeSharedRef<FtpFileSystem>(login), serverRelPath);
}

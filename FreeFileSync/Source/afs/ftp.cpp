// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "ftp.h"
//#include <zen/basic_math.h>
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
//FTP commands:      https://en.wikipedia.org/wiki/List_of_FTP_commands

constexpr std::chrono::seconds FTP_SESSION_MAX_IDLE_TIME  (20);
constexpr std::chrono::seconds FTP_SESSION_CLEANUP_INTERVAL(4);

const size_t FTP_BLOCK_SIZE_DOWNLOAD = 64 * 1024; //libcurl returns blocks of only 16 kB as returned by recv() even if we request larger blocks via CURLOPT_BUFFERSIZE
const size_t FTP_BLOCK_SIZE_UPLOAD   = 64 * 1024; //libcurl requests blocks of 64 kB. larger blocksizes set via CURLOPT_UPLOAD_BUFFERSIZE do not seem to make a difference
const size_t FTP_STREAM_BUFFER_SIZE = 1024 * 1024; //unit: [byte]
//stream buffer should be big enough to facilitate prefetching during alternating read/write operations => e.g. see serialize.h::unbufferedStreamCopy()

constexpr ZstringView ftpPrefix = Zstr("ftp:");


enum class ServerEncoding
{
    unknown,
    utf8,
    ansi,
};


inline
uint16_t getEffectivePort(int portOption)
{
    if (portOption > 0)
        return static_cast<uint16_t>(portOption);
    return DEFAULT_PORT_FTP;
}


struct FtpDeviceId //= what defines a unique FTP location
{
    FtpDeviceId(const FtpLogin& login) :
        server(login.server),
        port(getEffectivePort(login.portCfg)),
        username(login.username) {}

    Zstring server;
    uint16_t port; //must be valid port!
    Zstring username;
};
std::weak_ordering operator<=>(const FtpDeviceId& lhs, const FtpDeviceId& rhs)
{
    //exactly the type of case insensitive comparison we need for server names! https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfow#IDNs
    if (const std::weak_ordering cmp = compareAsciiNoCase(lhs.server, rhs.server);
        cmp != std::weak_ordering::equivalent)
        return cmp;

    return std::tie(lhs.port, lhs.username) <=> //username: case sensitive!
           std::tie(rhs.port, rhs.username);
}
//also needed by compareDeviceSameAfsType(), so can't just replace with hash and use std::unordered_map


struct FtpSessionCfg //= config for buffered FTP session
{
    FtpDeviceId deviceId;
    Zstring password;
    bool useTls = false;
};
bool operator==(const FtpSessionCfg& lhs, const FtpSessionCfg& rhs)
{
    if (lhs.deviceId <=> rhs.deviceId != std::weak_ordering::equivalent)
        return false;

    return std::tie(lhs.password, lhs.useTls) == //password: case sensitive!
           std::tie(rhs.password, rhs.useTls);
}


Zstring concatenateFtpFolderPathPhrase(const FtpLogin& login, const AfsPath& itemPath); //noexcept


Zstring ansiToUtfEncoding(const std::string_view& str) //throw SysError
{
    if (str.empty()) return {};

    gsize bytesWritten = 0; //not including the terminating null

    GError* error = nullptr;
    ZEN_ON_SCOPE_EXIT(if (error) ::g_error_free(error));

    //https://developer.gnome.org/glib/stable/glib-Character-Set-Conversion.html#g-convert
    gchar* utfStr = ::g_convert(str.data(),    //const gchar* str
                                str.size(),    //gssize len
                                "UTF-8",       //const gchar* to_codeset
                                "LATIN1",      //const gchar* from_codeset
                                nullptr,       //gsize* bytes_read
                                &bytesWritten, //gsize* bytes_written
                                &error);       //GError** error
    if (!utfStr)
        throw SysError(formatGlibError("g_convert(" + std::string(str) + ", LATIN1 -> UTF-8)", error));
    ZEN_ON_SCOPE_EXIT(::g_free(utfStr));

    return {utfStr, bytesWritten};


}


std::string utfToAnsiEncoding(const Zstring& str) //throw SysError
{
    if (str.empty()) return {};

    const Zstring& strNorm = getUnicodeNormalForm(str); //convert to pre-composed *before* attempting conversion

    gsize bytesWritten = 0; //not including the terminating null

    GError* error = nullptr;
    ZEN_ON_SCOPE_EXIT(if (error) ::g_error_free(error));

    //fails for: 1. broken UTF-8 2. not-ANSI-encodable Unicode
    gchar* ansiStr = ::g_convert(strNorm.c_str(), //const gchar* str
                                 strNorm.size(),  //gssize len
                                 "LATIN1",        //const gchar* to_codeset
                                 "UTF-8",         //const gchar* from_codeset
                                 nullptr,         //gsize* bytes_read
                                 &bytesWritten,   //gsize* bytes_written
                                 &error);         //GError** error
    if (!ansiStr)
        throw SysError(formatGlibError("g_convert(" + utfTo<std::string>(strNorm) + ", UTF-8 -> LATIN1)", error));
    ZEN_ON_SCOPE_EXIT(::g_free(ansiStr));

    return {ansiStr, bytesWritten};

}


std::wstring getCurlDisplayPath(const FtpDeviceId& deviceId, const AfsPath& itemPath)
{
    Zstring displayPath = Zstring(ftpPrefix) + Zstr("//");

    if (!deviceId.username.empty()) //show username! consider AFS::compareDeviceSameAfsType()
        displayPath += deviceId.username + Zstr('@');

    displayPath += deviceId.server;

    if (deviceId.port != DEFAULT_PORT_FTP)
        displayPath += Zstr(':') + numberTo<Zstring>(deviceId.port);

    const Zstring& relPath = getServerRelPath(itemPath);
    if (relPath != Zstr("/"))
        displayPath += relPath;

    return utfTo<std::wstring>(displayPath);
}


std::vector<std::string_view> splitFtpResponse(std::string&&) = delete;

std::vector<std::string_view> splitFtpResponse(const std::string& buf)
{
    std::vector<std::string_view> lines;

    split2(buf, [](char c) { return isLineBreak(c) || c == '\0'; }, //is 0-char check even needed?
    [&lines](const std::string_view block)
    {
        if (!block.empty()) //consider Windows' <CR><LF>
            lines.push_back(block);
    });

    return lines;
}


class FtpLineParser
{
public:
    explicit FtpLineParser(const std::string_view& line) : it_(line.begin()), itEnd_(line.end()) {}
    /**/     FtpLineParser(std::string_view&&) = delete;

    template <class Function>
    std::string_view readRange(size_t count, Function acceptChar) //throw SysError
    {
        if (static_cast<ptrdiff_t>(count) > itEnd_ - it_)
            throw SysError(L"Unexpected end of line.");

        const auto rngEnd = it_ + count;

        if (!std::all_of(it_, rngEnd, acceptChar))
            throw SysError(L"Expected char type not found.");

        return makeStringView(std::exchange(it_, rngEnd), rngEnd);
    }

    template <class Function> //expects non-empty range!
    std::string_view readRange(Function acceptChar) //throw SysError
    {
        auto rngEnd = std::find_if_not(it_, itEnd_, acceptChar);
        if (rngEnd == it_)
            throw SysError(L"Expected char range not found.");

        return makeStringView(std::exchange(it_, rngEnd), rngEnd);
    }

    char peekNextChar() const { return it_ == itEnd_ ? '\0' : *it_; }

private:
    /**/
    std::string_view::const_iterator it_;
    const std::string_view::const_iterator itEnd_;
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

struct SysErrorFtpProtocol : public zen::SysError
{
    SysErrorFtpProtocol(const std::wstring& msg, long ftpError) : SysError(msg), ftpErrorCode(ftpError) {}

    long ftpErrorCode;
};

DEFINE_NEW_SYS_ERROR(SysErrorPassword)


constinit Global<UniSessionCounter> globalFtpSessionCount;
GLOBAL_RUN_ONCE(globalFtpSessionCount.set(createUniSessionCounter()));


class FtpSession
{
public:
    explicit FtpSession(const FtpSessionCfg& sessionCfg) : //throw SysError
        sessionCfg_(sessionCfg)
    {
        lastSuccessfulUseTime_ = std::chrono::steady_clock::now();
    }

    ~FtpSession()
    {
        if (easyHandle_)
            ::curl_easy_cleanup(easyHandle_);
    }

    const FtpSessionCfg& getSessionCfg() const { return sessionCfg_; }

    //set *before* calling any of the subsequent functions; see FtpSessionManager::access()
    void setContextTimeout(const std::weak_ptr<int>& timeoutSec) { timeoutSec_ = timeoutSec; }

    //returns server response (header data)
    std::string perform(const AfsPath& itemPath, bool isDir, curl_ftpmethod pathMethod,
                        const std::vector<CurlOption>& extraOptions, bool requestUtf8) //throw SysError, SysErrorPassword, SysErrorFtpProtocol
    {
        if (requestUtf8) //avoid endless recursion
            initUtf8(); //throw SysError, SysErrorFtpProtocol

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

        std::string headerData;
        curl_write_callback onHeaderReceived = [](/*const*/ char* buffer, size_t size, size_t nitems, void* callbackData)
        {
            auto& output = *static_cast<std::string*>(callbackData);
            output.append(buffer, size * nitems);
            return size * nitems;
        };
        setCurlOption({CURLOPT_HEADERDATA, &headerData}); //throw SysError
        setCurlOption({CURLOPT_HEADERFUNCTION, onHeaderReceived}); //throw SysError

        setCurlOption({CURLOPT_URL, getCurlUrlPath(itemPath, isDir).c_str()}); //throw SysError

        assert(pathMethod != CURLFTPMETHOD_MULTICWD); //too slow!
        setCurlOption({CURLOPT_FTP_FILEMETHOD, pathMethod}); //throw SysError

        if (!sessionCfg_.deviceId.username.empty()) //else: libcurl will default to CURL_DEFAULT_USER("anonymous") and CURL_DEFAULT_PASSWORD("ftp@example.com")
        {
            //ANSI or UTF encoding?
            //  "modern" FTP servers (implementing RFC 2640) have UTF8 enabled by default => pray and hope for the best.
            //  What about ANSI-FTP servers and "Microsoft FTP Service" which requires "OPTS UTF8 ON"? => *psh*
            //  CURLOPT_PREQUOTE to the rescue? Nope, issued long after USER/PASS
            setCurlOption({CURLOPT_USERNAME, utfTo<std::string>(sessionCfg_.deviceId.username).c_str()}); //throw SysError
            setCurlOption({CURLOPT_PASSWORD, utfTo<std::string>(sessionCfg_.password         ).c_str()}); //throw SysError
            //curious: libcurl will *not* default to CURL_DEFAULT_USER when setting password but no username
        }

        setCurlOption({CURLOPT_PORT, sessionCfg_.deviceId.port}); //throw SysError

        //thread-safety: https://curl.haxx.se/libcurl/c/threadsafe.html
        setCurlOption({CURLOPT_NOSIGNAL, 1}); //throw SysError

        //allow PASV IP: some FTP servers really use IP different from control connection
        setCurlOption({CURLOPT_FTP_SKIP_PASV_IP, 0}); //throw SysError
        //let's not hold our breath until Curl adds a reasonable PASV handling => patch libcurl accordingly!
        //https://github.com/curl/curl/issues/1455
        //https://github.com/curl/curl/pull/1470
        //support broken servers like this one: https://freefilesync.org/forum/viewtopic.php?t=4301


        const std::shared_ptr<int> timeoutSec = timeoutSec_.lock();
        assert(timeoutSec);
        if (!timeoutSec)
            throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] FtpSession: Timeout duration was not set.");

        setCurlOption({CURLOPT_CONNECTTIMEOUT, *timeoutSec}); //throw SysError

        //CURLOPT_TIMEOUT: "Since this puts a hard limit for how long time a request is allowed to take, it has limited use in dynamic use cases with varying transfer times."
        setCurlOption({CURLOPT_LOW_SPEED_TIME, *timeoutSec}); //throw SysError
        setCurlOption({CURLOPT_LOW_SPEED_LIMIT, 1 /*[bytes]*/}); //throw SysError
        //can't use "0" which means "inactive", so use some low number

        setCurlOption({CURLOPT_SERVER_RESPONSE_TIMEOUT, *timeoutSec}); //throw SysError
        //FTP only; unlike CURLOPT_TIMEOUT, this one is NOT a limit on the total transfer time

        //CURLOPT_ACCEPTTIMEOUT_MS? => only relevant for "active" FTP connections

        //long-running file uploads require keep-alives for the TCP control connection: https://freefilesync.org/forum/viewtopic.php?t=6928
        setCurlOption({CURLOPT_TCP_KEEPALIVE, 1}); //throw SysError
        //=> CURLOPT_TCP_KEEPIDLE (=delay) and CURLOPT_TCP_KEEPINTVL both default to 60 sec

        //default is 60 sec (sufficient!?):
        //setCurlOption({CURLOPT_TCP_KEEPIDLE,  30 /*[sec]*/}); //throw SysError
        //setCurlOption({CURLOPT_TCP_KEEPINTVL, 30 /*[sec]*/}); //throw SysError


        std::optional<SysError> socketException;
        //libcurl does *not* set FD_CLOEXEC for us! https://github.com/curl/curl/issues/2252
        auto onSocketCreate = [&](curl_socket_t curlfd, curlsocktype purpose)
        {
            assert(::fcntl(curlfd, F_GETFD) == 0);
            if (::fcntl(curlfd, F_SETFD, FD_CLOEXEC) == -1) //=> RACE-condition if other thread calls fork/execv before this thread sets FD_CLOEXEC!
            {
                socketException = SysError(formatSystemError("fcntl(FD_CLOEXEC)", errno));
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
        setCurlOption({CURLOPT_SHARE, curlShare}); //throw SysError
#endif

        //TODO: FTP option to require certificate checking?
#if 0
        setCurlOption({CURLOPT_CAINFO, "cacert.pem"}); //throw SysError
        //hopefully latest version from https://curl.haxx.se/docs/caextract.html
        //libcurl forwards this char-string to OpenSSL as is, which (thank god) accepts UTF8
#else
        setCurlOption({CURLOPT_CAINFO, 0}); //throw SysError
        //be explicit: "even when [CURLOPT_SSL_VERIFYPEER] is disabled [...] curl may still load the certificate file specified in CURLOPT_CAINFO."

        //check if server certificate can be trusted? (Default: 1L)
        //  => may fail with: "CURLE_PEER_FAILED_VERIFICATION: SSL certificate problem: certificate has expired"
        setCurlOption({CURLOPT_SSL_VERIFYPEER, 0}); //throw SysError
        //check that server name matches the name in the certificate? (Default: 2L)
        //  => may fail with: "CURLE_PEER_FAILED_VERIFICATION: SSL: no alternative certificate subject name matches target host name 'freefilesync.org'"
        setCurlOption({CURLOPT_SSL_VERIFYHOST, 0}); //throw SysError
#endif
        if (sessionCfg_.useTls) //https://tools.ietf.org/html/rfc4217
        {
            //require SSL for both control and data:
            setCurlOption({CURLOPT_USE_SSL,    CURLUSESSL_ALL}); //throw SysError
            //try TLS first, then SSL (currently: CURLFTPAUTH_DEFAULT == CURLFTPAUTH_SSL):
            setCurlOption({CURLOPT_FTPSSLAUTH, CURLFTPAUTH_TLS}); //throw SysError
        }

        for (const CurlOption& option : extraOptions)
            setCurlOption(option); //throw SysError

        //=======================================================================================================
        const CURLcode rcPerf = ::curl_easy_perform(easyHandle_);
        //WTF: curl_easy_perform() considers FTP response codes >= 400 as failure, but for HTTP response codes 4XX are considered success!! CONSISTENCY, people!!!
        //note: CURLOPT_FAILONERROR(default:off) is only available for HTTP => BUT at least we can prefix FTP commands with * for same effect: https://curl.se/libcurl/c/CURLOPT_QUOTE.html

        if (socketException)
            throw* socketException; //throw SysError
        //=======================================================================================================

        if (rcPerf != CURLE_OK)
        {
            std::wstring errorMsg = trimCpy(utfTo<std::wstring>(curlErrorBuf)); //optional

            if (const std::vector<std::string_view>& headerLines = splitFtpResponse(headerData);
                !headerLines.empty())
                if (const std::string_view& response = trimCpy(headerLines.back()); //that *should* be the server's error response
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
            if (rcPerf == CURLE_LOGIN_DENIED)
                throw SysErrorPassword(formatSystemError("curl_easy_perform", formatCurlStatusCode(rcPerf), errorMsg));

            long ftpStatusCode = 0; //optional
            /*const CURLcode rc =*/ ::curl_easy_getinfo(easyHandle_, CURLINFO_RESPONSE_CODE, &ftpStatusCode);
            //https://en.wikipedia.org/wiki/List_of_FTP_server_return_codes
            assert(rcPerf == CURLE_OPERATION_TIMEDOUT || rcPerf == CURLE_ABORTED_BY_CALLBACK || ftpStatusCode == 0 || 400 <= ftpStatusCode && ftpStatusCode < 600);
            if (ftpStatusCode != 0)
                throw SysErrorFtpProtocol(formatSystemError("curl_easy_perform", formatCurlStatusCode(rcPerf), errorMsg), ftpStatusCode);

            throw SysError(formatSystemError("curl_easy_perform", formatCurlStatusCode(rcPerf), errorMsg));
        }

        lastSuccessfulUseTime_ = std::chrono::steady_clock::now();
        return headerData;
    }

    //returns server response (header data)
    std::string runSingleFtpCommand(const std::string& ftpCmd, bool requestUtf8) //throw SysError, SysErrorFtpProtocol
    {
        curl_slist* quote = nullptr;
        ZEN_ON_SCOPE_EXIT(::curl_slist_free_all(quote));
        quote = ::curl_slist_append(quote, ftpCmd.c_str());

        return perform(AfsPath(), true /*isDir*/, CURLFTPMETHOD_NOCWD /*avoid needless CWDs*/,
        {
            {CURLOPT_NOBODY, 1L},
            {CURLOPT_QUOTE, quote},
        }, requestUtf8); //throw SysError, SysErrorPassword, SysErrorFtpProtocol
    }

    void testConnection() //throw SysError
    {
        /*  https://en.wikipedia.org/wiki/List_of_FTP_commands
            FEAT: are there servers that don't support this command? fuck, yes: "550 FEAT: Operation not permitted" => buggy server not granting access, despite support!
            PWD? will fail if last access deleted the working dir!
            "TYPE I"? might interfere with libcurls internal handling, but that's an improvement, right? right? :>
            => but "HELP", and "NOOP" work, right??
            Fuck my life: even "HELP" is not always implemented: https://freefilesync.org/forum/viewtopic.php?t=6002
            => are there servers supporting neither FEAT nor HELP? only time will tell...
            ... and it tells! FUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU https://freefilesync.org/forum/viewtopic.php?t=8041             */

        //=> '*' to the rescue: as long as we get an FTP response - *any* FTP response (including 550) - the connection itself is fine!
        const std::string& featBuf = runSingleFtpCommand("*FEAT", false /*requestUtf8*/); //throw SysError, SysErrorFtpProtocol

        for (const std::string_view& line : splitFtpResponse(featBuf))
            if (startsWith(line, "211 ") ||
                startsWith(line, "500 ") ||
                startsWith(line, "550 "))
                return;

        //ever get here?
        throw SysError(L"Unexpected FTP response. (" + utfTo<std::wstring>(featBuf) + L')');
    }

    AfsPath getHomePath() //throw SysError
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

            const std::string& pwdBuf = runSingleFtpCommand("PWD", true /*requestUtf8*/); //throw SysError, SysErrorFtpProtocol

            for (const std::string_view& line : splitFtpResponse(pwdBuf))
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
                                    const Zstring homePathUtf = serverToUtfEncoding(homePathRaw); //throw SysError
                                    return sanitizeDeviceRelativePath(homePathUtf);
                                }
                            }
                    break;
                }
            throw SysError(L"Unexpected FTP response. (" + utfTo<std::wstring>(pwdBuf) + L')');
        }();
        return *homePathCached_;
    }

    void ensureBinaryMode() //throw SysError
    {
        if (std::optional<curl_socket_t> currentSocket = getActiveSocket()) //throw SysError
            if (*currentSocket == binaryEnabledSocket_)
                return;

        runSingleFtpCommand("TYPE I", false /*requestUtf8*/); //throw SysError, SysErrorFtpProtocol

        //make sure our binary-enabled session is still there (== libcurl behaves as we expect)
        std::optional<curl_socket_t> currentSocket = getActiveSocket(); //throw SysError
        if (currentSocket)
            binaryEnabledSocket_ = *currentSocket; //remember what we did
        //libcurl already buffers "conn->proto.ftpc.transfertype" but selfishly keeps it for itself!
        //=> pray libcurl doesn't internally set "TYPE A"!
        //=> this seems to be the only place where it does: https://github.com/curl/curl/issues/4342
        else
            throw SysError(L"Curl failed to cache FTP session."); //why is libcurl not caching the session???
    }

    //------------------------------------------------------------------------------------------------------------
    bool supportsMlsd() { return getFeatureSupport(&Features::mlsd); } //
    bool supportsMfmt() { return getFeatureSupport(&Features::mfmt); } //throw SysError
    bool supportsClnt() { return getFeatureSupport(&Features::clnt); } //
    bool supportsUtf8()
    {
        if (getFeatureSupport(&Features::utf8))
            return true;

        initUtf8(); //vsFTPd (ftp.sunet.se): supports UTF8 via "OPTS UTF8 ON", even if "UTF8" is missing from "FEAT"
        return socketUsesUtf8_;
    }

    bool isHealthy() const
    {
        return std::chrono::steady_clock::now() - lastSuccessfulUseTime_ <= FTP_SESSION_MAX_IDLE_TIME;
    }

    std::string getServerPathInternal(const AfsPath& itemPath) //throw SysError
    {
        const Zstring serverPath = getServerRelPath(itemPath);

        if (itemPath.value.empty()) //endless recursion caveat!! utfToServerEncoding() transitively depends on getServerPathInternal()
            return utfTo<std::string>(serverPath);

        return utfToServerEncoding(serverPath); //throw SysError
    }

    Zstring serverToUtfEncoding(const std::string_view& str) //throw SysError
    {
        if (isAsciiString(str)) //fast path
            return {str.begin(), str.end()};

        switch (encoding_) //throw SysError
        {
            case ServerEncoding::unknown:
                /* "UTF-8 encodings [2] contain enough internal structure that it is always, in practice, possible to determine whether a UTF-8 or raw encoding has been used"
                    - https://www.rfc-editor.org/rfc/rfc3659#section-2.2
                  "encoding rules make it very unlikely that a character sequence from a different character set will be mistaken for a UTF-8 encoded character sequence."
                    - https://www.rfc-editor.org/rfc/rfc2640#section-2.2

                  => auto-detect encoding even if FEAT does not advertize UTF8: https://freefilesync.org/forum/viewtopic.php?t=9564       */
                encoding_ = supportsUtf8() || isValidUtf(str) ? ServerEncoding::utf8 : ServerEncoding::ansi;
                return serverToUtfEncoding(str); //throw SysError

            case ServerEncoding::utf8:
                if (!isValidUtf(str))
                    throw SysError(_("Invalid character encoding:") + L' ' + utfTo<std::wstring>(str) + L' ' + _("Expected:") + L" [UTF-8]");

                return utfTo<Zstring>(str);

            case ServerEncoding::ansi:
                return ansiToUtfEncoding(str); //throw SysError
        }
        assert(false);
        return {};
    }

    std::string utfToServerEncoding(const Zstring& str) //throw SysError
    {
        if (isAsciiString(str)) //fast path
            return {str.begin(), str.end()};
        switch (encoding_) //throw SysError
        {
            case ServerEncoding::unknown:
                if (!supportsUtf8())
                    throw SysError(_("Failed to auto-detect character encoding:") + L' ' + utfTo<std::wstring>(str)); //might be ANSI or UTF8 with non-compliant server...

                encoding_ = ServerEncoding::utf8;
                return utfToServerEncoding(str); //throw SysError

            case ServerEncoding::utf8:
                //validate! we consider REPLACEMENT_CHAR as indication for server using ANSI encoding in serverToUtfEncoding()
                if (!isValidUtf(str))
                    throw SysError(_("Invalid character encoding:") + L' ' + utfTo<std::wstring>(str) + L' ' + _("Expected:") + (sizeof(str[0]) == 1 ? L" [UTF-8]" : L" [UTF-16]"));
                static_assert(sizeof(str[0]) == 1 || sizeof(str[0]) == 2);

                return utfTo<std::string>(str);

            case ServerEncoding::ansi:
                return utfToAnsiEncoding(str); //throw SysError
        }
        assert(false);
        return {};
    }

private:
    FtpSession           (const FtpSession&) = delete;
    FtpSession& operator=(const FtpSession&) = delete;

    std::string getCurlUrlPath(const AfsPath& itemPath /*optional*/, bool isDir) //throw SysError
    {
        std::string curlRelPath; //libcurl expects encoded paths (except for '/' char!!!) => bug: https://github.com/curl/curl/pull/4423

        split(getServerPathInternal(itemPath), //throw SysError
              '/', [&](std::string_view comp)
        {
            if (!comp.empty())
            {
                char* compFmt = ::curl_easy_escape(easyHandle_, comp.data(), static_cast<int>(comp.size()));
                if (!compFmt)
                    throw SysError(formatSystemError(std::string("curl_easy_escape(") + comp + ')', L"", L"Conversion failure"));
                ZEN_ON_SCOPE_EXIT(::curl_free(compFmt));

                if (!curlRelPath.empty())
                    curlRelPath += '/';
                curlRelPath += compFmt;
            }
        });

        if (trimCpy(sessionCfg_.deviceId.server).empty())
            throw SysError(_("Server name must not be empty."));

        static_assert(LIBCURL_VERSION_MAJOR > 7 || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 67));
        /*  1. CURLFTPMETHOD_NOCWD requires absolute paths to unconditionally skip CWDs: https://github.com/curl/curl/pull/4382
            2. CURLFTPMETHOD_SINGLECWD requires absolute paths to skip one needless "CWD entry path": https://github.com/curl/curl/pull/4332
              => https://curl.se/docs/faq.html#How_do_I_list_the_root_directory
              => use // because /%2f had bugs (but they should be fixed: https://github.com/curl/curl/pull/4348)             */
        std::string path = utfTo<std::string>(Zstring(ftpPrefix) + Zstr("//") + sessionCfg_.deviceId.server) + "//" + curlRelPath;

        if (isDir && !endsWith(path, '/')) //curl-FTP needs directory paths to end with a slash
            path += '/';
        return path;
    }

    void initUtf8() //throw SysError, SysErrorFtpProtocol
    {
        /*  1. Some RFC-2640-non-compliant servers require UTF8 to be explicitly enabled: https://wiki.filezilla-project.org/Character_Encoding#Conflicting_specification
               - e.g. Microsoft FTP Service: https://freefilesync.org/forum/viewtopic.php?t=4303

            2. Others do not advertize "UTF8" in "FEAT", but *still* allow enabling it via "OPTS UTF8 ON":
               -  https://freefilesync.org/forum/viewtopic.php?t=9564
               - vsFTPd: ftp.sunet.se https://security.appspot.com/vsftpd.html#download

            "OPTS UTF8 ON" needs to be activated each time libcurl internally creates a new session
            hopyfully libcurl will offer a better solution: https://github.com/curl/curl/issues/1457       */

        if (std::optional<curl_socket_t> currentSocket = getActiveSocket()) //throw SysError
            if (*currentSocket == utf8RequestedSocket_) //caveat: a non-UTF8-enabled session might already exist, e.g. from a previous call to supportsMlsd()
                return;

        //some (broken!?) servers require "CLNT" before accepting "OPTS UTF8 ON": https://social.msdn.microsoft.com/Forums/en-US/d602574f-8a69-4d69-b337-52b6081902cf/problem-with-ftpwebrequestopts-utf8-on-501-please-clnt-first
        if (supportsClnt()) //throw SysError
            runSingleFtpCommand("CLNT FreeFileSync", false /*requestUtf8*/); //throw SysError, SysErrorFtpProtocol

        //"prefix the command with an asterisk to make libcurl continue even if the command fails"
        //-> ignore if server does not know this legacy command (but report all *other* issues; else getActiveSocket() below won't have a socket and we've hidden the real error!)
        const std::string& optsBuf = runSingleFtpCommand("*OPTS UTF8 ON", false /*requestUtf8*/); //throw SysError, (SysErrorFtpProtocol)

        //get *last* FTP status code (can there be more than one!?)
        int ftpStatusCode = 0;
        for (const std::string_view& line : splitFtpResponse(optsBuf))
            if (line.size() >= 4 &&
                isDigit(line[0]) &&
                isDigit(line[1]) &&
                isDigit(line[2]) &&
                line[3] == ' ')
                ftpStatusCode = stringTo<int>(line);

        socketUsesUtf8_ = ftpStatusCode == 200 || //"200 Always in UTF8 mode."  "200 UTF8 set to on"
                          ftpStatusCode == 202;   //"202 UTF8 mode is always enabled."

        //make sure our Unicode-enabled session is still there (== libcurl behaves as we expect)
        std::optional<curl_socket_t> currentSocket = getActiveSocket(); //throw SysError
        if (currentSocket)
            utf8RequestedSocket_ = *currentSocket; //remember what we did
        else
            throw SysError(L"Curl failed to cache FTP session."); //why is libcurl not caching the session???

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
    using FeatureList = std::unordered_map<Zstring /*server name*/, Features, StringHashAsciiNoCase, StringEqualAsciiNoCase>;

    bool getFeatureSupport(bool Features::* status) //throw SysError
    {
        if (!featureCache_)
        {
            static constinit FunStatGlobal<Protected<FeatureList>> globalServerFeatures;
            globalServerFeatures.setOnce([] { return std::make_unique<Protected<FeatureList>>(); });

            const auto sf = globalServerFeatures.get();
            if (!sf)
                throw SysError(formatSystemError("FtpSession::getFeatureSupport", L"", L"Function call not allowed during application shutdown."));

            sf->access([&](const FeatureList& featList)
            {
                auto it = featList.find(sessionCfg_.deviceId.server);
                if (it != featList.end())
                    featureCache_ = it->second;
            });

            if (!featureCache_)
            {
                //*: ignore error if server does not support/allow FEAT
                featureCache_ = parseFeatResponse(runSingleFtpCommand("*FEAT", false /*requestUtf8*/)); //throw SysError, (SysErrorFtpProtocol)
                //used by initUtf8()! => requestUtf8 = false!!!

                sf->access([&](FeatureList& feat) { feat.emplace(sessionCfg_.deviceId.server, *featureCache_); });
            }
        }
        return (*featureCache_).*status;
    }

    static Features parseFeatResponse(const std::string& featResponse)
    {
        Features output; //FEAT command: https://tools.ietf.org/html/rfc2389#page-4
        std::vector<std::string_view> lines = splitFtpResponse(featResponse);

        auto it = std::find_if(lines.begin(), lines.end(), [](const std::string_view& line) { return startsWith(line, "211-") || startsWith(line, "211 "); });
        if (it != lines.end())
        {
            ++it;
            for (; it != lines.end(); ++it)
            {
                if (equalAsciiNoCase     (*it, "211 End") || //Serv-U: "211 End (for details use "HELP commmand" where command is the command of interest)"
                    startsWithAsciiNoCase(*it, "211 End "))  //Home Ftp Server: "211 End of extentions."
                    break;

                std::string line(*it);
                //suppport ProFTPD with "MultilineRFC2228 = on" https://freefilesync.org/forum/viewtopic.php?t=7243
                if (startsWith(line, "211-"))
                    line = ' ' + afterFirst(line, '-', IfNotFoundReturn::none);

                //https://tools.ietf.org/html/rfc3659#section-7.8
                //"a server-FTP process that supports MLST, and MLSD [...] MUST indicate that this support exists"
                //"there is no distinct FEAT output for MLSD. The presence of the MLST feature indicates that both MLST and MLSD are supported"
                if (equalAsciiNoCase     (line, " MLST")  ||
                    startsWithAsciiNoCase(line, " MLST ") || //SP "MLST" [SP factlist] CRLF
                    //so much the theory. In practice FTP server implementers can't read (specs): https://freefilesync.org/forum/viewtopic.php?t=6752
                    equalAsciiNoCase(line, " MLSD"))
                    output.mlsd = true;

                //https://tools.ietf.org/html/draft-somers-ftp-mfxx-04#section-3.3
                //"Where a server-FTP process supports the MFMT command [...] it MUST include the response to the FEAT command"
                else if (equalAsciiNoCase(line, " MFMT")) //SP "MFMT" CRLF
                    output.mfmt = true;

                else if (equalAsciiNoCase(line, " UTF8") ||
                         equalAsciiNoCase(line, " UTF8 ON") || //support non-compliant servers: https://freefilesync.org/forum/viewtopic.php?t=7355#p24694
                         equalAsciiNoCase(line, " UTF-8"))     //Android 12: "File Manager" by Xiaomi
                    output.utf8 = true;

                else if (equalAsciiNoCase(line, " CLNT"))
                    output.clnt = true;
            }
        }
        return output;
    }

    const FtpSessionCfg sessionCfg_;
    CURL* easyHandle_ = nullptr;

    curl_socket_t utf8RequestedSocket_ = 0;
    curl_socket_t binaryEnabledSocket_ = 0;

    bool socketUsesUtf8_ = false;

    ServerEncoding encoding_ = ServerEncoding::unknown;

    std::optional<Features> featureCache_;
    std::optional<AfsPath> homePathCached_;

    const std::shared_ptr<UniCounterCookie> libsshCurlUnifiedInitCookie_{getLibsshCurlUnifiedInitCookie(globalFtpSessionCount)}; //throw SysError
    std::chrono::steady_clock::time_point lastSuccessfulUseTime_;
    std::weak_ptr<int> timeoutSec_;
};

//================================================================================================================
//================================================================================================================

class FtpSessionManager //reuse (healthy) FTP sessions globally
{
    struct FtpSessionCache;

public:
    FtpSessionManager() : sessionCleaner_([this]
    {
        setCurrentThreadName(Zstr("Session Cleaner[FTP]"));
        runGlobalSessionCleanUp(); /*throw ThreadStopRequest*/
    }) {}

    void access(const FtpLogin& login, const std::function<void(FtpSession& session)>& useFtpSession /*throw X*/) //throw SysError, X
    {
        Protected<FtpSessionCache>& sessionCache = getSessionCache(login);

        std::unique_ptr<FtpSession> ftpSession;  //either or
        std::optional<FtpSessionCfg> sessionCfg; //

        sessionCache.access([&](FtpSessionCache& cache)
        {
            if (!cache.activeCfg) //AFS::authenticateAccess() not called => authenticate implicitly!
                setActiveConfig(cache, login);

            //assume "isHealthy()" to avoid hitting server connection limits: (clean up of !isHealthy() after use, idle sessions via worker thread)
            if (!cache.idleFtpSessions.empty())
            {
                ftpSession = std::move(cache.idleFtpSessions.back    ());
                /**/                   cache.idleFtpSessions.pop_back();
            }
            else
                sessionCfg = *cache.activeCfg;
        });

        //create new FTP session outside the lock: 1. don't block other threads 2. non-atomic regarding "sessionCache"! => one session too many is not a problem!
        if (!ftpSession)
            ftpSession = std::make_unique<FtpSession>(*sessionCfg); //throw SysError

        const std::shared_ptr<int> timeoutSec = std::make_shared<int>(login.timeoutSec); //context option: valid only for duration of this call!
        ftpSession->setContextTimeout(timeoutSec);

        ZEN_ON_SCOPE_EXIT
        (
            //*INDENT-OFF*
            if (ftpSession->isHealthy()) //thread that created the "!isHealthy()" session is responsible for clean up (avoid hitting server connection limits!)
                sessionCache.access([&](FtpSessionCache& cache)
                {
                    if (ftpSession->getSessionCfg() == *cache.activeCfg) //created outside the lock => check *again*
                        cache.idleFtpSessions.push_back(std::move(ftpSession)); //pass ownership
                });
            //*INDENT-ON*
        );

        useFtpSession(*ftpSession); //throw X
    }

    void setActiveConfig(const FtpLogin& login)
    {
        getSessionCache(login).access([&](FtpSessionCache& cache) { setActiveConfig(cache, login); });
    }

    void setSessionPassword(const FtpLogin& login, const Zstring& password)
    {
        getSessionCache(login).access([&](FtpSessionCache& cache)
        {
            cache.sessionPassword = password;
            setActiveConfig(cache, login);
        });
    }

private:
    FtpSessionManager           (const FtpSessionManager&) = delete;
    FtpSessionManager& operator=(const FtpSessionManager&) = delete;

    Protected<FtpSessionCache>& getSessionCache(const FtpDeviceId& deviceId)
    {
        //single global session cache per login; life-time bound to globalInstance => never remove a sessionCache!!!
        Protected<FtpSessionCache>* sessionCache = nullptr;

        globalSessionCache_.access([&](GlobalFtpSessions& sessionsById)
        {
            sessionCache = &sessionsById[deviceId]; //get or create
        });
        static_assert(std::is_same_v<GlobalFtpSessions, std::map<FtpDeviceId, Protected<FtpSessionCache>>>, "require std::map so that the pointers we return remain stable");

        return *sessionCache;
    }

    void setActiveConfig(FtpSessionCache& cache, const FtpLogin& login)
    {
        if (cache.activeCfg)
            assert(std::all_of(cache.idleFtpSessions.begin(), cache.idleFtpSessions.end(),
            [&](const std::unique_ptr<FtpSession>& session) { return session->getSessionCfg() == cache.activeCfg; }));
        else
            assert(cache.idleFtpSessions.empty());

        const std::optional<FtpSessionCfg> prevCfg = cache.activeCfg;

        cache.activeCfg =
        {
            .deviceId{login},
            .password = login.password ? *login.password : cache.sessionPassword,
            .useTls = login.useTls,
        };

        /* remove incompatible sessions:
            - avoid hitting FTP connection limit if some config uses TLS, but not the other: https://freefilesync.org/forum/viewtopic.php?t=8532
            - logically consistent with AFS::compareDevice()
            - don't allow different authentication methods, when authenticateAccess() is called *once* per device in getFolderStatusParallel()
            - what user expects, e.g. when tesing changed settings in FTP login dialog      */
        if (cache.activeCfg != prevCfg)
            cache.idleFtpSessions.clear(); //run ~FtpSession *inside* the lock! => avoid hitting server limits!
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

            std::vector<Protected<FtpSessionCache>*> sessionCaches; //pointers remain stable, thanks to std::map<>

            globalSessionCache_.access([&](GlobalFtpSessions& sessionsById)
            {
                for (auto& [sessionId, idleSession] : sessionsById)
                    sessionCaches.push_back(&idleSession);
            });

            for (Protected<FtpSessionCache>* sessionCache : sessionCaches)
                for (;;)
                {
                    bool done = false;
                    sessionCache->access([&](FtpSessionCache& cache)
                    {
                        for (std::unique_ptr<FtpSession>& ftpSession : cache.idleFtpSessions)
                            if (!ftpSession->isHealthy()) //!isHealthy() sessions are destroyed after use => in this context this means they have been idle for too long
                            {
                                ftpSession.swap(cache.idleFtpSessions.back());
                                /**/            cache.idleFtpSessions.pop_back(); //run ~FtpSession *inside* the lock! => avoid hitting server limits!
                                return; //don't hold lock for too long: delete only one session at a time, then yield...
                            }
                        done = true;
                    });
                    if (done)
                        break;
                    std::this_thread::yield(); //outside the lock
                }
        }
    }

    struct FtpSessionCache
    {
        //invariant: all cached sessions correspond to activeCfg at any time!
        std::vector<std::unique_ptr<FtpSession>> idleFtpSessions; //extract *temporarily* from this list during use
        std::optional<FtpSessionCfg> activeCfg;
        Zstring sessionPassword;
    };

    using GlobalFtpSessions = std::map<FtpDeviceId, Protected<FtpSessionCache>>;
    Protected<GlobalFtpSessions> globalSessionCache_;

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

            session.ensureBinaryMode(); //throw SysError
            //...or some server return ASCII size or fail with '550 SIZE not allowed in ASCII mode: https://freefilesync.org/forum/viewtopic.php?t=7669&start=30#p27742
            const std::string sizeBuf = session.runSingleFtpCommand("*SIZE " + session.getServerPathInternal(linkPath),
                                                                    true /*requestUtf8*/); //throw SysError, SysErrorFtpProtocol
            //alternative: use libcurl + CURLINFO_CONTENT_LENGTH_DOWNLOAD_T? => nah, suprise (motherfucker)! libcurl adds needless "REST 0" command!
            for (const std::string_view& line : splitFtpResponse(sizeBuf))
                if (startsWith(line, "213 ")) // 213<space>[rubbish]<file size>        according to libcurl
                {
                    if (isDigit(line.back())) //https://tools.ietf.org/html/rfc3659#section-4
                    {
                        auto it = std::find_if(line.rbegin(), line.rend(), [](const char c) { return !isDigit(c); });
                        output.fileSize = stringTo<uint64_t>(makeStringView(it.base(), line.end()));

                        mdtmBuf = session.runSingleFtpCommand("MDTM " + session.getServerPathInternal(linkPath),
                                                              true /*requestUtf8*/); //throw SysError, SysErrorFtpProtocol
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
            for (const std::string_view& line : splitFtpResponse(mdtmBuf))
                if (startsWith(line, "213 ")) // 213<space> YYYYMMDDHHMMSS[.sss]       "Time values are always represented in UTC (GMT)" ...and libcurl thinks so, too
                {
                    const auto itStart = line.begin() + 4;
                    const auto itEnd = std::find(itStart, line.end(), '.');

                    if (const TimeComp tc = parseTime("%Y%m%d%H%M%S", makeStringView(itStart, itEnd));
                        tc != TimeComp())
                        if (const auto [modTime, timeValid] = utcToTimeT(tc);
                            timeValid)
                            return modTime;
                    break;
                }
            throw SysError(L"Unexpected FTP response. (" + utfTo<std::wstring>(mdtmBuf) + L')');
        }();

        return output;
    }
    catch (const SysError& e)
    {
        throw FileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(getCurlDisplayPath(login, linkPath))), e.toString());
    }
}


class FtpDirectoryReader
{
public:
    static std::vector<FtpItem> execute(const FtpLogin& login, const AfsPath& dirPath) //throw SysError, SysErrorFtpProtocol
    {
        std::string rawListing; //get raw FTP directory listing

        curl_write_callback onBytesReceived = [](/*const*/ char* buffer, size_t size, size_t nitems, void* callbackData)
        {
            auto& listing = *static_cast<std::string*>(callbackData);
            listing.append(buffer, size * nitems);
            return size * nitems;
            //folder reading might take up to a minute in extreme cases (50,000 files): https://freefilesync.org/forum/viewtopic.php?t=5312
        };

        std::vector<FtpItem> output;

        accessFtpSession(login, [&](FtpSession& session) //throw SysError
        {
            std::vector<CurlOption> options =
            {
                {CURLOPT_WRITEDATA, &rawListing},
                {CURLOPT_WRITEFUNCTION, onBytesReceived},
            };
            curl_ftpmethod pathMethod = CURLFTPMETHOD_SINGLECWD;

            if (session.supportsMlsd()) //throw SysError
            {
                options.emplace_back(CURLOPT_CUSTOMREQUEST, "MLSD");

                //some FTP servers abuse https://tools.ietf.org/html/rfc3659#section-7.1
                //and process wildcards characters inside the "dirpath"; see http://www.proftpd.org/docs/howto/Globbing.html
                //      [] matches any character in the character set enclosed in the brackets
                //      * (not between brackets) matches any string, including the empty string
                //      ? (not between brackets) matches any single character
                //
                //of course this "helpfulness" blows up with MLSD + paths that incidentally contain wildcards: https://freefilesync.org/forum/viewtopic.php?t=5575
                const bool pathHasWildcards = //=> globbing is reproducible even with freefilesync.org's FTP!
                    contains(afterFirst<ZstringView>(dirPath.value, Zstr('['), IfNotFoundReturn::none), Zstr(']')) ||
                    contains(dirPath.value, Zstr('*')) ||
                    contains(dirPath.value, Zstr('?'));

                if (!pathHasWildcards)
                    pathMethod = CURLFTPMETHOD_NOCWD; //16% faster traversal compared to CURLFTPMETHOD_SINGLECWD (35% faster than CURLFTPMETHOD_MULTICWD)
            }
            //else: use "LIST" + CURLFTPMETHOD_SINGLECWD
            //caveat: let's better not use LIST parameters: https://cr.yp.to/ftp/list.html

            session.perform(dirPath, true /*isDir*/, pathMethod, options, true /*requestUtf8*/); //throw SysError, SysErrorPassword, SysErrorFtpProtocol

            if (session.supportsMlsd()) //throw SysError
                output = parseMlsd(rawListing, session); //throw SysError
            else
                output = parseUnknown(rawListing, session); //throw SysError
        });

        return output;
    }

private:
    FtpDirectoryReader           (const FtpDirectoryReader&) = delete;
    FtpDirectoryReader& operator=(const FtpDirectoryReader&) = delete;

    static std::vector<FtpItem> parseMlsd(const std::string& buf, FtpSession& session) //throw SysError
    {
        std::vector<FtpItem> output;
        for (const std::string_view& line : splitFtpResponse(buf))
        {
            FtpItem item = parseMlstLine(line, session); //throw SysError
            if (item.itemName != Zstr(".") &&
                item.itemName != Zstr(".."))
                output.push_back(std::move(item));
        }
        return output;
    }

    static FtpItem parseMlstLine(const std::string_view& rawLine, FtpSession& session) //throw SysError
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

            const std::string_view facts = makeStringView(itBegin, itBlank);
            item.itemName = session.serverToUtfEncoding(makeStringView(itBlank + 1, rawLine.end())); //throw SysError

            std::string_view typeFact;
            std::string_view fileSize;

            split(facts, ';', [&](const std::string_view fact)
            {
                if (!fact.empty())
                {
                    if (startsWithAsciiNoCase(fact, "type=")) //must be case-insensitive!!!
                    {
                        const std::string_view tmp = afterFirst(fact, '=', IfNotFoundReturn::none);
                        typeFact = beforeFirst(tmp, ':', IfNotFoundReturn::all);
                    }
                    else if (startsWithAsciiNoCase(fact, "size="))
                        fileSize = afterFirst(fact, '=', IfNotFoundReturn::none);
                    else if (startsWithAsciiNoCase(fact, "modify="))
                    {
                        std::string_view modifyFact = afterFirst(fact, '=', IfNotFoundReturn::none);
                        modifyFact = beforeLast(modifyFact, '.', IfNotFoundReturn::all); //truncate millisecond precision if available

                        const TimeComp tc = parseTime("%Y%m%d%H%M%S", modifyFact);
                        if (tc == TimeComp())
                            throw SysError(L"Modification time is invalid.");

                        if (const auto [modTime, timeValid] = utcToTimeT(tc);
                            timeValid)
                            item.modTime = modTime;
                        else
                            throw SysError(L"Modification time is invalid.");
                    }
                    else if (startsWithAsciiNoCase(fact, "unique="))
                    {
                        /*  https://tools.ietf.org/html/rfc3659#section-7.5.2
                            "The mapping between files, and unique fact tokens should be maintained, [...] for
                             *at least* the lifetime of the control connection from user-PI to server-PI."

                            => not necessarily *persistent* as far as the RFC goes!
                               BUT: practially this will be the inode ID/file index, so we can assume persistence */
                        const std::string_view uniqueId = afterFirst(fact, '=', IfNotFoundReturn::none);
                        assert(!uniqueId.empty());
                        item.filePrint = hashString<AFS::FingerPrint>(uniqueId);
                        //other metadata to hash e.g. create fact? => not available on Linux-hosted FTP!
                    }
                }
            });

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
                if (fileSize.empty() || !std::all_of(fileSize.begin(), fileSize.end(), &isDigit<char>))
                    throw SysError(L"File size not available."); //crazy, but can be "-1": https://freefilesync.org/forum/viewtopic.php?t=9720#p35757
                item.fileSize = stringTo<uint64_t>(fileSize);
            }
            return item;
        }
        catch (const SysError& e)
        {
            throw SysError(L"Unexpected FTP response. (" + utfTo<std::wstring>(rawLine) + L") " + e.toString());
        }
    }

    static std::vector<FtpItem> parseUnknown(const std::string& buf, FtpSession& session) //throw SysError
    {
        if (!buf.empty() && isDigit(buf[0])) //lame test to distinguish Unix/Dos formats as internally used by libcurl
            return parseWindows(buf, session); //throw SysError
        return parseUnix(buf, session);        //
    }

    //"ls -l"
    static std::vector<FtpItem> parseUnix(const std::string& buf, FtpSession& session) //throw SysError
    {
        const std::vector<std::string_view> lines = splitFtpResponse(buf);
        auto it = lines.begin();

        if (it != lines.end() && startsWith(*it, "total "))
            ++it;

        const time_t utcTimeNow = std::time(nullptr);
        const TimeComp tc = getUtcTime(utcTimeNow);
        if (tc == TimeComp())
            throw SysError(L"Failed to determine current time: " + numberTo<std::wstring>(utcTimeNow));

        const int utcCurrentYear = tc.year;

        //different listing formats: better store at session level!?
        std::optional<int> dirOwnerGroupCount;  //
        std::optional<int> fileOwnerGroupCount; //caveat: differentiate per item type: see alternative formats!
        std::optional<int> linkOwnerGroupCount; //

        std::vector<FtpItem> output;

        std::for_each(it, lines.end(), [&](const std::string_view line)
        {
            auto& ownerGroupCount = [&]() -> std::optional<int>&
            {
                assert(!line.empty()); //see splitFtpResponse()
                switch (line[0])
                {
                    //*INDENT-OFF*
                    case 'd': return  dirOwnerGroupCount;
                    case 'l': return linkOwnerGroupCount;
                    default : return fileOwnerGroupCount;
                    //*INDENT-ON*
                }
            }();

            //unix listing without group: https://freefilesync.org/forum/viewtopic.php?t=4306
            if (!ownerGroupCount)
                ownerGroupCount = [&]
            {
                std::optional<SysError> firstError;

                for (int i = 3; i-- > 0;)
                    try
                    {
                        parseUnixLine(line, utcTimeNow, utcCurrentYear, i /*ownerGroupCount*/, session); //throw SysError
                        return i;
                    }
                    catch (const SysError& e)
                    {
                        if (!firstError)
                            firstError = e;
                    }
                throw* firstError; //most likely the relevant one: https://freefilesync.org/forum/viewtopic.php?t=10798
            }();

            const FtpItem item = parseUnixLine(line, utcTimeNow, utcCurrentYear, *ownerGroupCount, session); //throw SysError
            if (item.itemName != Zstr(".") &&
                item.itemName != Zstr(".."))
                output.push_back(item);
        });

        return output;
    }

    static FtpItem parseUnixLine(const std::string_view& rawLine, time_t utcTimeNow, int utcCurrentYear, int ownerGroupCount, FtpSession& session) //throw SysError
    {
        /* Unix standard listing: "ls -l --all"

            total 4953                                                  <- optional first line
            drwxr-xr-x 1 root root    4096 Jan 10 11:58 version
            -rwxr-xr-x 1 root root    1084 Sep  2 01:17 Unit Test.vcxproj.user
            -rwxr-xr-x 1 1000  300    2217 Feb 28  2016 win32.manifest
            lrwxr-xr-x 1 root root      18 Apr 26 15:17 Projects -> /mnt/hgfs/Projects

        file type: -:file  l:symlink  d:directory  b:block device  p:named pipe  c:char device  s:socket

        permissions: (r|-)(w|-)(x|s|S|-)    user
                     (r|-)(w|-)(x|s|S|-)    group  s := S + x      S = Setgid
                     (r|-)(w|-)(x|t|T|-)    others t := T + x      T = sticky bit

        Alternative formats
        -------------------
        No group: "ls -l --no-group" https://freefilesync.org/forum/viewtopic.php?t=4306
            dr-xr-xr-x   2 root                  512 Apr  8  1994 etc

        No owner, no group, trailing slash (but only for directories!????): "ls -g --no-group --file-type" https://freefilesync.org/forum/viewtopic.php?t=10227
            -rwxrwxrwx 1 ownername groupname      8064383 Mar 30 11:58 file.mp3
            drwxrwxrwx 1              0 Jan  1 00:00 dirname/

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

            const std::string_view typeTag = parser.readRange(1, [](char c) //throw SysError
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
            //both owner + group, owner only, or none at all
            assert(0 <= ownerGroupCount && ownerGroupCount <=2);
            for (int i = 0; i < ownerGroupCount; ++i)
            {
                parser.readRange(std::not_fn(isWhiteSpace<char>)); //throw SysError
                parser.readRange(&isWhiteSpace<char>);             //throw SysError
            }
            //------------------------------------------------------------------------------------
            //file size (no separators)
            const uint64_t fileSize = stringTo<uint64_t>(parser.readRange(&isDigit<char>)); //throw SysError
            parser.readRange(&isWhiteSpace<char>);                                          //throw SysError
            //------------------------------------------------------------------------------------
            const std::string_view monthStr = parser.readRange(std::not_fn(isWhiteSpace<char>)); //throw SysError
            parser.readRange(&isWhiteSpace<char>);                                               //throw SysError

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
            const std::string_view timeOrYear = parser.readRange([](char c) { return c == ':' || isDigit(c); }); //throw SysError
            parser.readRange(&isWhiteSpace<char>);                                                               //throw SysError

            TimeComp timeComp;
            timeComp.month = 1 + static_cast<int>(itMonth - std::begin(months));
            timeComp.day = day;

            if (contains(timeOrYear, ':'))
            {
                const int hour   = stringTo<int>(beforeFirst(timeOrYear, ':', IfNotFoundReturn::none));
                const int minute = stringTo<int>(afterFirst (timeOrYear, ':', IfNotFoundReturn::none));
                if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
                    throw SysError(L"Failed to parse modification time.");

                timeComp.hour   = hour;
                timeComp.minute = minute;
                timeComp.year = utcCurrentYear; //tentatively

                const auto [serverLocalTime, timeValid] = utcToTimeT(timeComp);
                if (!timeValid)
                    throw SysError(L"Modification time is invalid.");

                if (serverLocalTime > utcTimeNow + 24 * 3600 ) //time-zones range from UTC-12:00 to UTC+14:00, consider DST; FileZilla uses 1 day tolerance
                    --timeComp.year; //"more likely" this time is from last year
            }
            else if (timeOrYear.size() == 4)
            {
                timeComp.year = stringTo<int>(timeOrYear);

                if (timeComp.year < 1600 || timeComp.year >= 3000)
                    throw SysError(L"Failed to parse modification time.");
            }
            else
                throw SysError(L"Failed to parse modification time.");

            //let's pretend the time listing is UTC (same behavior as FileZilla): hopefully MLSD will make this mess obsolete soon...
            //  => find exact offset with some MDTM hackery? yes, could do that, but this doesn't solve the bigger problem of imprecise LIST file times, so why bother?
            const auto [modTime, timeValid] = utcToTimeT(timeComp);
            if (!timeValid)
                throw SysError(L"Modification time is invalid.");
            //------------------------------------------------------------------------------------
            const std::string_view trail = parser.readRange([](char) { return true; }); //throw SysError
            std::string_view itemName;
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

            item.itemName = session.serverToUtfEncoding(itemName); //throw SysError
            if (item.type == AFS::ItemType::folder && endsWith(item.itemName, Zstr('/')))
                item.itemName.pop_back();

            item.modTime = modTime;

            return item;
        }
        catch (const SysError& e)
        {
            throw SysError(L"Unexpected FTP response. (" + utfTo<std::wstring>(rawLine) + L") [ownerGroupCount: " + numberTo<std::wstring>(ownerGroupCount) + L"] " + e.toString());
        }
    }


    //"dir"
    static std::vector<FtpItem> parseWindows(const std::string& buf, FtpSession& session) //throw SysError
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
        for (const std::string_view& line : splitFtpResponse(buf))
        {
            try
            {
                FtpLineParser parser(line);

                const int month = stringTo<int>(parser.readRange(2, &isDigit<char>)); //throw SysError
                parser.readRange(1, [](char c) { return c == '-' || c == '/'; });     //throw SysError
                const int day = stringTo<int>(parser.readRange(2, &isDigit<char>));   //throw SysError
                parser.readRange(1, [](char c) { return c == '-' || c == '/'; });     //throw SysError
                const std::string_view yearString = parser.readRange(&isDigit<char>); //throw SysError
                parser.readRange(&isWhiteSpace<char>);                                //throw SysError

                if (month < 1 || month > 12 || day < 1 || day > 31)
                    throw SysError(L"Failed to parse modification time.");

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
                    throw SysError(L"Failed to parse modification time.");
                //------------------------------------------------------------------------------------
                int hour = stringTo<int>(parser.readRange(2, &isDigit<char>));         //throw SysError
                parser.readRange(1, [](char c) { return c == ':'; });                  //throw SysError
                const int minute = stringTo<int>(parser.readRange(2, &isDigit<char>)); //throw SysError
                if (!isWhiteSpace(parser.peekNextChar()))
                {
                    const std::string_view period = parser.readRange(2, [](char c) { return c == 'A' || c == 'P' || c == 'M'; }); //throw SysError
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
                    throw SysError(L"Failed to parse modification time.");
                //------------------------------------------------------------------------------------
                TimeComp timeComp;
                timeComp.year   = year;
                timeComp.month  = month;
                timeComp.day    = day;
                timeComp.hour   = hour;
                timeComp.minute = minute;
                //let's pretend the time listing is UTC (same behavior as FileZilla): hopefully MLSD will make this mess obsolete soon...
                //  => find exact offset with some MDTM hackery? yes, could do that, but this doesn't solve the bigger problem of imprecise LIST file times, so why bother?
                const auto [modTime, timeValid] = utcToTimeT(timeComp);
                if (!timeValid)
                    throw SysError(L"Modification time is invalid.");
                //------------------------------------------------------------------------------------
                const std::string_view dirTagOrSize = parser.readRange(std::not_fn(isWhiteSpace<char>)); //throw SysError
                parser.readRange(&isWhiteSpace<char>); //throw SysError

                const bool isDir = dirTagOrSize == "<DIR>";
                uint64_t fileSize = 0;
                if (!isDir)
                {
                    std::string sizeStr(dirTagOrSize);
                    replace(sizeStr, ',', "");
                    replace(sizeStr, '.', "");
                    if (sizeStr.empty() || !std::all_of(sizeStr.begin(), sizeStr.end(), &isDigit<char>))
                        throw SysError(L"Failed to parse file size.");
                    fileSize = stringTo<uint64_t>(sizeStr);
                }
                //------------------------------------------------------------------------------------
                const std::string_view itemName = parser.readRange([](char) { return true; }); //throw SysError
                if (itemName.empty())
                    throw SysError(L"Folder contains an item without name.");

                //------------------------------------------------------------------------------------
                if (itemName != "." &&
                    itemName != "..")
                {
                    FtpItem item;
                    if (isDir)
                        item.type = AFS::ItemType::folder;
                    item.itemName = session.serverToUtfEncoding(itemName); //throw SysError
                    item.fileSize = fileSize;
                    item.modTime  = modTime;

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
        std::vector<FtpItem> items;
        try
        {
            items = FtpDirectoryReader::execute(login_, dirPath); //throw SysError, SysErrorFtpProtocol
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(getCurlDisplayPath(login_, dirPath))), e.toString());
        }

        for (const FtpItem& item : items)
        {
            const AfsPath itemPath(appendPath(dirPath.value, item.itemName));

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

    auto onBytesReceived = [&](const void* buffer, size_t bytesToWrite)
    {
        try
        {
            writeBlock(buffer, bytesToWrite); //throw X
            //[!] let's NOT use "incomplete write Posix semantics" for libcurl!
            //who knows if libcurl buffers properly, or if it sends incomplete packages!?
            return bytesToWrite;
        }
        catch (...)
        {
            exception = std::current_exception();
            return bytesToWrite + 1; //signal error condition => CURLE_WRITE_ERROR
        }
    };
    curl_write_callback onBytesReceivedWrapper = [](char* buffer, size_t size, size_t nitems, void* callbackData)
    {
        return (*static_cast<decltype(onBytesReceived)*>(callbackData))(buffer, size * nitems); //free this poor little C-API from its shackles and redirect to a proper lambda
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

                //{CURLOPT_BUFFERSIZE, 256 * 1024} -> default is 16 kB which seems to correspond to TLS packet size
                //=> setting larger buffer size does nothing (recv still returns only 16 kB)
            }, true /*requestUtf8*/); //throw SysError, SysErrorPassword, SysErrorFtpProtocol
        });
    }
    catch (const SysError& e)
    {
        if (exception)
            std::rethrow_exception(exception);

        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getCurlDisplayPath(login, afsFilePath))), e.toString());
    }
}


/* File already existing:
    freefilesync.org: overwrites
    FileZilla Server: overwrites
    Windows IIS:      overwrites                          */
void ftpFileUpload(const FtpLogin& login, const AfsPath& afsFilePath,
                   const std::function<size_t(void* buffer, size_t bytesToRead)>& readBlock /*throw X*/) //throw FileError, X; return "bytesToRead" bytes unless end of stream
{
    std::exception_ptr exception;

    auto getBytesToSend = [&](void* buffer, size_t bytesToRead) -> size_t
    {
        try
        {
            /*  libcurl calls back until 0 bytes are returned (Posix read() semantics), or,
                if CURLOPT_INFILESIZE_LARGE was set, after exactly this amount of bytes

                [!] let's NOT use "incomplete read Posix semantics" for libcurl!
                who knows if libcurl buffers properly, or if it requests incomplete packages!?     */
            const size_t bytesRead = readBlock(buffer, bytesToRead); //throw X; return "bytesToRead" bytes unless end of stream
            assert(bytesRead == bytesToRead || bytesRead == 0 || readBlock(buffer, bytesToRead) == 0);
            return bytesRead;
        }
        catch (...)
        {
            exception = std::current_exception();
            return CURL_READFUNC_ABORT; //signal error condition => CURLE_ABORTED_BY_CALLBACK
        }
    };
    curl_read_callback getBytesToSendWrapper = [](char* buffer, size_t size, size_t nitems, void* callbackData)
    {
        return (*static_cast<decltype(getBytesToSend)*>(callbackData))(buffer, size * nitems); //free this poor little C-API from its shackles and redirect to a proper lambda
    };

    try
    {
        accessFtpSession(login, [&](FtpSession& session) //throw SysError
        {
            /*  curl_slist* quote = nullptr;
                ZEN_ON_SCOPE_EXIT(::curl_slist_free_all(quote));

                //"prefix the command with an asterisk to make libcurl continue even if the command fails"
                quote = ::curl_slist_append(quote, ("*DELE " + session.getServerPathInternal(afsFilePath)).c_str()); //throw SysError

                //optimize fail-safe copy with RNFR/RNTO as CURLOPT_POSTQUOTE? -> even slightly *slower* than RNFR/RNTO as additional curl_easy_perform()   */

            session.perform(afsFilePath, false /*isDir*/, CURLFTPMETHOD_NOCWD, //are there any servers that require CURLFTPMETHOD_SINGLECWD? let's find out
            {
                {CURLOPT_UPLOAD, 1L},
                {CURLOPT_READDATA, &getBytesToSend},
                {CURLOPT_READFUNCTION, getBytesToSendWrapper},

                //{CURLOPT_UPLOAD_BUFFERSIZE, 256 * 1024} -> defaults is 64 kB. apparently no performance improvement for larger buffers like 256 kB

                //{CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(inputBuffer.size())},
                //=> CURLOPT_INFILESIZE_LARGE does not issue a specific FTP command, but is used by libcurl only!

                //{CURLOPT_PREQUOTE,  quote},
                //{CURLOPT_POSTQUOTE, quote},
            }, true /*requestUtf8*/); //throw SysError, SysErrorPassword, SysErrorFtpProtocol
        });
    }
    catch (const SysError& e)
    {
        if (exception)
            std::rethrow_exception(exception);

        throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getCurlDisplayPath(login, afsFilePath))), e.toString());
    }
}

//===========================================================================================================================

struct InputStreamFtp : public AFS::InputStream
{
    InputStreamFtp(const FtpLogin& login, const AfsPath& filePath)
    {
        worker_ = InterruptibleThread([asyncStreamOut = this->asyncStreamIn_, login, filePath]
        {
            setCurrentThreadName(Zstr("Istream ") + utfTo<Zstring>(getCurlDisplayPath(login, filePath)));
            try
            {
                auto writeBlock = [&](const void* buffer, size_t bytesToWrite)
                {
                    asyncStreamOut->write(buffer, bytesToWrite); //throw ThreadStopRequest
                };
                ftpFileDownload(login, filePath, writeBlock); //throw FileError, ThreadStopRequest

                asyncStreamOut->closeStream();
            }
            catch (FileError&) { asyncStreamOut->setWriteError(std::current_exception()); } //let ThreadStopRequest pass through!
        });
    }

    ~InputStreamFtp()
    {
        asyncStreamIn_->setReadError(std::make_exception_ptr(ThreadStopRequest()));
    }

    size_t getBlockSize() override { return FTP_BLOCK_SIZE_DOWNLOAD; } //throw (FileError)

    //may return short; only 0 means EOF! CONTRACT: bytesToRead > 0!
    size_t tryRead(void* buffer, size_t bytesToRead, const IoCallback& notifyUnbufferedIO /*throw X*/) override //throw FileError, (ErrorFileLocked), X
    {
        const size_t bytesRead = asyncStreamIn_->tryRead(buffer, bytesToRead); //throw FileError
        reportBytesProcessed(notifyUnbufferedIO); //throw X
        return bytesRead;
        //no need for asyncStreamIn_->checkWriteErrors(): once end of stream is reached, asyncStreamOut->closeStream() was called => no errors occured
    }

    std::optional<AFS::StreamAttributes> tryGetAttributesFast() override { return {}; }//throw FileError
    //there is no stream handle => no buffered attribute access!
    //PERF: get attributes during file download?
    //  CURLOPT_FILETIME:                                           test case 77 files, 4MB: overall copy time increases by 12%
    //  CURLOPT_PREQUOTE/CURLOPT_PREQUOTE/CURLOPT_POSTQUOTE + MDTM: test case 77 files, 4MB: overall copy time increases by 12%

private:
    void reportBytesProcessed(const IoCallback& notifyUnbufferedIO /*throw X*/) //throw X
    {
        const int64_t bytesDelta = makeSigned(asyncStreamIn_->getTotalBytesWritten()) - totalBytesReported_;
        totalBytesReported_ += bytesDelta;
        if (notifyUnbufferedIO) notifyUnbufferedIO(bytesDelta); //throw X
    }

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
                    const AfsPath& filePath,
                    std::optional<time_t> modTime) :
        login_(login),
        filePath_(filePath),
        modTime_(modTime)
    {
        std::promise<void> promUploadDone;
        futUploadDone_ = promUploadDone.get_future();

        worker_ = InterruptibleThread([login, filePath,
                                              asyncStreamIn = this->asyncStreamOut_,
                                              pUploadDone   = std::move(promUploadDone)]() mutable
        {
            setCurrentThreadName(Zstr("Ostream ") + utfTo<Zstring>(getCurlDisplayPath(login, filePath)));
            try
            {
                auto readBlock = [&](void* buffer, size_t bytesToRead)
                {
                    return asyncStreamIn->read(buffer, bytesToRead); //throw ThreadStopRequest
                };
                ftpFileUpload(login, filePath, readBlock); //throw FileError, ThreadStopRequest
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
        if (asyncStreamOut_) //finalize() was not called (successfully)
            asyncStreamOut_->setWriteError(std::make_exception_ptr(ThreadStopRequest()));
    }

    size_t getBlockSize() override { return FTP_BLOCK_SIZE_UPLOAD; } //throw (FileError)

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

        while (futUploadDone_.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout)
            reportBytesProcessed(notifyUnbufferedIO); //throw X
        reportBytesProcessed(notifyUnbufferedIO); //[!] once more, now that *all* bytes were written

        assert(isReady(futUploadDone_));
        futUploadDone_.get(); //throw FileError

        //asyncStreamOut_->checkReadErrors(); //throw FileError -> not needed after *successful* upload
        asyncStreamOut_.reset(); //do NOT reset on error, so that ~OutputStreamFtp() will request worker thread to stop
        //--------------------------------------------------------------------

        AFS::FinalizeResult result;
        //result.filePrint = ... -> yet unknown at this point
        try
        {
            setModTimeIfAvailable(); //throw FileError, follows symlinks
            /* is setting modtime after closing the file handle a pessimization?
                FTP:    no: could set modtime via CURLOPT_POSTQUOTE (but this would internally trigger an extra round-trip anyway!) */
        }
        catch (const FileError& e) { result.errorModTime = e; /*might slice derived class?*/ }

        return result;
    }

private:
    void reportBytesProcessed(const IoCallback& notifyUnbufferedIO /*throw X*/) //throw X
    {
        const int64_t bytesDelta = makeSigned(asyncStreamOut_->getTotalBytesRead()) - totalBytesReported_;
        totalBytesReported_ += bytesDelta;
        if (notifyUnbufferedIO) notifyUnbufferedIO(bytesDelta); //throw X
    }

    void setModTimeIfAvailable() const //throw FileError, follows symlinks
    {
        //assert(isReady(futUploadDone_)); => MUST NOT CALL *after* std::future<>::get()!
        if (modTime_)
            try
            {
                const std::string isoTime = utfTo<std::string>(formatTime(Zstr("%Y%m%d%H%M%S"), getUtcTime(*modTime_))); //returns empty string on error
                if (isoTime.empty())
                    throw SysError(L"Invalid modification time (time_t: " + numberTo<std::wstring>(*modTime_) + L')');

                accessFtpSession(login_, [&](FtpSession& session) //throw SysError
                {
                    if (!session.supportsMfmt()) //throw SysError
                        throw SysError(L"Server does not support the MFMT command.");

                    session.runSingleFtpCommand("MFMT " + isoTime + ' ' + session.getServerPathInternal(filePath_),
                                                true /*requestUtf8*/); //throw SysError, SysErrorFtpProtocol
                    //not relevant for OutputStreamFtp, but: does MFMT follow symlinks? for Linux FTP server (using utime) it does
                });
            }
            catch (const SysError& e)
            {
                throw FileError(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(getCurlDisplayPath(login_, filePath_))), e.toString());
            }
    }

    const FtpLogin login_;
    const AfsPath filePath_;
    const std::optional<time_t> modTime_;
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
    Zstring getInitPathPhrase(const AfsPath& itemPath) const override { return concatenateFtpFolderPathPhrase(login_, itemPath); }

    std::vector<Zstring> getPathPhraseAliases(const AfsPath& itemPath) const override { return {getInitPathPhrase(itemPath)}; }

    std::wstring getDisplayPath(const AfsPath& itemPath) const override { return getCurlDisplayPath(login_, itemPath); }

    bool isNullFileSystem() const override { return login_.server.empty(); }

    std::weak_ordering compareDeviceSameAfsType(const AbstractFileSystem& afsRhs) const override
    {
        const FtpLogin& lhs = login_;
        const FtpLogin& rhs = static_cast<const FtpFileSystem&>(afsRhs).login_;

        return FtpDeviceId(lhs) <=> FtpDeviceId(rhs);
    }

    //----------------------------------------------------------------------------------------------------------------

    ItemType getItemType(const AfsPath& itemPath) const override //throw FileError
    {
        try
        {
            const std::optional<AfsPath> parentPath = getParentPath(itemPath);
            if (!parentPath) //device root => quick access test
            {
                try { accessFtpSession(login_, [](FtpSession& session) { session.testConnection(); });  /*throw SysError*/ }
                catch (const SysError& e) { throw SysError(replaceCpy(_("Unable to connect to %x."), L"%x", fmtPath(login_.server)) + L'\n' + e.toString()); }

                return ItemType::folder;
            }

            const std::vector<FtpItem> items = [&]
            {
                try
                {
                    //don't use MLST: broken for Pure-FTPd: https://freefilesync.org/forum/viewtopic.php?t=4287
                    return FtpDirectoryReader::execute(login_, *parentPath); //throw SysError, SysErrorFtpProtocol
                }
                catch (const SysError& e) //add context: error might be folder-specific
                { throw SysError(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(parentPath->value.empty() ? Zstr("/") : getItemName(*parentPath))) + L'\n' + e.toString()); }
            }();

            const Zstring itemName = getItemName(itemPath);
            assert(!itemName.empty());
            //is the underlying file system case-sensitive? we don't know => assume "case-sensitive"
            //all path components (except the base folder part!) can be expected to have the right case anyway after directory traversal
            for (const FtpItem& item : items)
                if (item.itemName == itemName)
                    return item.type;

            throw SysError(replaceCpy(_("%x does not exist."), L"%x", fmtPath(itemName)));
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getDisplayPath(itemPath))), e.toString());
        }
    }

    std::optional<ItemType> getItemTypeIfExistsImpl(const AfsPath& itemPath) const //throw SysError
    {
        const std::optional<AfsPath> parentPath = getParentPath(itemPath);
        if (!parentPath) //device root => quick access test
        {
            try { accessFtpSession(login_, [](FtpSession& session) { session.testConnection(); });  /*throw SysError*/ }
            catch (const SysError& e) { throw SysError(replaceCpy(_("Unable to connect to %x."), L"%x", fmtPath(login_.server)) + L'\n' + e.toString()); }

            return ItemType::folder;
        }

        std::optional<SysErrorFtpProtocol> lastFtpError;
        try
        {
            try
            {
                const Zstring itemName = getItemName(itemPath);
                assert(!itemName.empty());

                for (const FtpItem& item : FtpDirectoryReader::execute(login_, *parentPath)) //throw SysError, SysErrorFtpProtocol
                    if (item.itemName == itemName) //case-sensitive comparison! itemPath must be normalized!
                        return item.type;

                return std::nullopt;
            }
            catch (const SysErrorFtpProtocol& e)
            {
                //let's dig deeper, but *only* for SysErrorFtpProtocol, not for general connection issues
                //+ check if FTP error code sounds like "not existing"
                if (e.ftpErrorCode == 550) //FTP 550 No such file or directory
                    //501? "pathname that exists but is not a directory to a MLSD command generates a 501 reply": https://www.rfc-editor.org/rfc/rfc3659
                    //=> really? cannot reproduce, getting: "550 '/filename.txt' is not a directory" or "550 Can't check for file existence"
                    lastFtpError = e; //-> get out of catch clause
                else
                    throw;
            }
        }
        catch (const SysError& e) //add context: error might be folder-specific
        { throw SysError(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(parentPath->value.empty() ? Zstr("/") : getItemName(*parentPath))) + L'\n' + e.toString()); }

        //----------------------------------------------------------------
        if (const std::optional<ItemType> parentType = getItemTypeIfExistsImpl(*parentPath)) //throw SysError
        {
            if (*parentType == ItemType::file /*obscure, but possible*/)
                throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(getItemName(*parentPath))));

            throw* lastFtpError; //throw SysError; parent path existing, so traversal should not have failed!
        }
        else
            return std::nullopt;
    }

    std::optional<ItemType> getItemTypeIfExists(const AfsPath& itemPath) const override //throw FileError
    {
        try
        {
            return getItemTypeIfExistsImpl(itemPath); //throw SysError
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getDisplayPath(itemPath))), e.toString());
        }
    }

    //----------------------------------------------------------------------------------------------------------------
    //already existing: fail
    //=> FTP will (most likely) fail and give a clear error message:
    //      freefilesync.org: "550 Can't create directory: File exists"
    //      FileZilla Server: "550 Directory already exists"
    //      Windows IIS:      "550 Cannot create a file when that file already exists"
    void createFolderPlain(const AfsPath& folderPath) const override //throw FileError
    {
        try
        {
            accessFtpSession(login_, [&](FtpSession& session) //throw SysError
            {
                session.runSingleFtpCommand("MKD " + session.getServerPathInternal(folderPath), true /*requestUtf8*/); //throw SysError, SysErrorFtpProtocol
            });
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(getDisplayPath(folderPath))), e.toString());
        }
    }

    void removeFilePlain(const AfsPath& filePath) const override //throw FileError
    {
        try
        {
            accessFtpSession(login_, [&](FtpSession& session) //throw SysError
            {
                session.runSingleFtpCommand("DELE " + session.getServerPathInternal(filePath), true /*requestUtf8*/); //throw SysError, SysErrorFtpProtocol
            });
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot delete file %x."), L"%x", fmtPath(getDisplayPath(filePath))), e.toString());
        }
    }

    void removeSymlinkPlain(const AfsPath& linkPath) const override //throw FileError
    {
        try
        {
            accessFtpSession(login_, [&](FtpSession& session) //throw SysError
            {
                //works fine for Linux hosts, but what about Windows-hosted FTP??? Distinguish DELE/RMD?
                //Windows test, FileZilla Server and Windows IIS FTP: all symlinks are reported as regular folders
                session.runSingleFtpCommand("DELE " + session.getServerPathInternal(linkPath), true /*requestUtf8*/); //throw SysError, SysErrorFtpProtocol
            });
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot delete symbolic link %x."), L"%x", fmtPath(getDisplayPath(linkPath))), e.toString());
        }
    }

    void removeFolderPlain(const AfsPath& folderPath) const override //throw FileError
    {
        try
        {
            accessFtpSession(login_, [&](FtpSession& session) //throw SysError
            {
                //Windows server: FileZilla Server and Windows IIS FTP: all symlinks are reported as regular folders
                //Linux server (freefilesync.org): RMD will fail for symlinks!
                session.runSingleFtpCommand("RMD " + session.getServerPathInternal(folderPath), true /*requestUtf8*/); //throw SysError, SysErrorFtpProtocol
            });
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(getDisplayPath(folderPath))), e.toString());
        }
    }

    void removeFolderIfExistsRecursion(const AfsPath& folderPath, //throw FileError
                                       const std::function<void(const std::wstring& displayPath)>& onBeforeFileDeletion   /*throw X*/,
                                       const std::function<void(const std::wstring& displayPath)>& onBeforeSymlinkDeletion/*throw X*/,
                                       const std::function<void(const std::wstring& displayPath)>& onBeforeFolderDeletion /*throw X*/) const override
    {
        //default implementation: folder traversal
        AFS::removeFolderIfExistsRecursion(folderPath, onBeforeFileDeletion, onBeforeSymlinkDeletion, onBeforeFolderDeletion); //throw FileError, X
    }

    //----------------------------------------------------------------------------------------------------------------
    AbstractPath getSymlinkResolvedPath(const AfsPath& linkPath) const override //throw FileError
    {
        throw FileError(replaceCpy(_("Cannot determine final path for %x."), L"%x", fmtPath(getDisplayPath(linkPath))), _("Operation not supported by device."));
    }

    bool equalSymlinkContentForSameAfsType(const AfsPath& linkPathL, const AbstractPath& linkPathR) const override //throw FileError
    {
        throw FileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(getDisplayPath(linkPathL))), _("Operation not supported by device."));
    }
    //----------------------------------------------------------------------------------------------------------------

    //return value always bound:
    std::unique_ptr<InputStream> getInputStream(const AfsPath& filePath) const override //throw FileError, (ErrorFileLocked)
    {
        return std::make_unique<InputStreamFtp>(login_, filePath);
    }

    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    //=> actual behavior: fail(+delete!)/overwrite/auto-rename
    std::unique_ptr<OutputStreamImpl> getOutputStream(const AfsPath& filePath, //throw FileError
                                                      std::optional<uint64_t> streamSize,
                                                      std::optional<time_t> modTime) const override
    {
        /* most FTP servers overwrite, but some (e.g. IIS) can be configured to fail, others (pureFTP) can be configured to auto-rename:
           https://download.pureftpd.org/pub/pure-ftpd/doc/README
           '-r': Never overwrite existing files. Uploading a file whose name already exists causes an automatic rename. Files are called xyz, xyz.1, xyz.2, xyz.3, etc. */

        //already existing: fail (+ delete!!!)
        return std::make_unique<OutputStreamFtp>(login_, filePath, modTime);
    }

    //----------------------------------------------------------------------------------------------------------------
    void traverseFolderRecursive(const TraverserWorkload& workload /*throw X*/, size_t parallelOps) const override
    {
        traverseFolderRecursiveFTP(login_, workload, parallelOps); //throw X
    }
    //----------------------------------------------------------------------------------------------------------------

    //symlink handling: follow
    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    FileCopyResult copyFileForSameAfsType(const AfsPath& sourcePath, const StreamAttributes& attrSource, //throw FileError, (ErrorFileLocked), X
                                          const AbstractPath& targetPath, bool copyFilePermissions, const IoCallback& notifyUnbufferedIO /*throw X*/) const override
    {
        //no native FTP file copy => use stream-based file copy:
        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(AFS::getDisplayPath(targetPath))), _("Operation not supported by device."));

        //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
        return copyFileAsStream(sourcePath, attrSource, targetPath, notifyUnbufferedIO); //throw FileError, (ErrorFileLocked), X
    }

    //symlink handling: follow
    //already existing: fail
    void copyNewFolderForSameAfsType(const AfsPath& sourcePath, const AbstractPath& targetPath, bool copyFilePermissions) const override //throw FileError
    {
        //already existing: fail
        AFS::createFolderPlain(targetPath); //throw FileError

        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(AFS::getDisplayPath(targetPath))), _("Operation not supported by device."));
    }

    //already existing: fail
    void copySymlinkForSameAfsType(const AfsPath& sourcePath, const AbstractPath& targetPath, bool copyFilePermissions) const override
    {
        throw FileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."),
                                              L"%x", L'\n' + fmtPath(getDisplayPath(sourcePath))),
                                   L"%y", L'\n' + fmtPath(AFS::getDisplayPath(targetPath))), _("Operation not supported by device."));
    }

    //already existing: undefined behavior! (e.g. fail/overwrite)
    //=> actual behavior: most linux-based FTP servers overwrite, Windows-based servers fail (but most can be configured to behave differently)
    //      freefilesync.org: silent overwrite
    //      Windows IIS:      CURLE_QUOTE_ERROR: QUOT command failed with 550 Cannot create a file when that file already exists.
    //      FileZilla Server: CURLE_QUOTE_ERROR: QUOT command failed with 553 file exists
    void moveAndRenameItemForSameAfsType(const AfsPath& pathFrom, const AbstractPath& pathTo) const override //throw FileError, ErrorMoveUnsupported
    {
        if (compareDeviceSameAfsType(pathTo.afsDevice.ref()) != std::weak_ordering::equivalent)
            throw ErrorMoveUnsupported(generateMoveErrorMsg(pathFrom, pathTo), _("Operation not supported between different devices."));

        try
        {
            accessFtpSession(login_, [&](FtpSession& session) //throw SysError
            {
                curl_slist* quote = nullptr;
                ZEN_ON_SCOPE_EXIT(::curl_slist_free_all(quote));
                quote = ::curl_slist_append(quote, ("RNFR " + session.getServerPathInternal(pathFrom      )).c_str()); //throw SysError
                quote = ::curl_slist_append(quote, ("RNTO " + session.getServerPathInternal(pathTo.afsPath)).c_str()); //

                session.perform(AfsPath(), true /*isDir*/, CURLFTPMETHOD_NOCWD, //avoid needless CWDs
                {
                    {CURLOPT_NOBODY, 1L},
                    {CURLOPT_QUOTE, quote},
                }, true /*requestUtf8*/); //throw SysError, SysErrorPassword, SysErrorFtpProtocol
            });
        }
        catch (const SysError& e)
        {
            throw FileError(generateMoveErrorMsg(pathFrom, pathTo), e.toString());
        }
    }

    bool supportsPermissions(const AfsPath& folderPath) const override { return false; } //throw FileError
    //wait until there is real demand for copying from and to FTP with permissions => use stream-based file copy:

    //----------------------------------------------------------------------------------------------------------------
    FileIconHolder getFileIcon      (const AfsPath& filePath, int pixelSize) const override { return {}; } //throw FileError; optional return value
    ImageHolder    getThumbnailImage(const AfsPath& filePath, int pixelSize) const override { return {}; } //throw FileError; optional return value

    void authenticateAccess(const RequestPasswordFun& requestPassword /*throw X*/) const override //throw FileError, X
    {
        auto connectServer = [&] //throw SysError, SysErrorPassword
        {
            accessFtpSession(login_, [](FtpSession& session) //connect with FTP server, *unless* already connected (in which case *nothing* is sent)
            {
                session.perform(AfsPath(), true /*isDir*/, CURLFTPMETHOD_NOCWD,
                {{CURLOPT_NOBODY, 1L}, {CURLOPT_SERVER_RESPONSE_TIMEOUT, 0}}, false /*requestUtf8*/);
                //caveat: connection phase only, so disable CURLOPT_SERVER_RESPONSE_TIMEOUT, or next access may fail with CURLE_OPERATION_TIMEDOUT!
            }); //throw SysError, SysErrorPassword, SysErrorFtpProtocol
        };

        try
        {
            const std::shared_ptr<FtpSessionManager> mgr = globalFtpSessionManager.get();
            if (!mgr)
                throw SysError(formatSystemError("getSessionPassword", L"", L"Function call not allowed during init/shutdown."));

            mgr->setActiveConfig(login_);

            if (!login_.password)
            {
                try //1. test for connection error *before* bothering user to enter a password
                {
                    connectServer(); //throw SysError, SysErrorPassword
                    return; //got new FtpSession (connected in constructor) or already connected session from cache
                }
                catch (const SysErrorPassword& e)
                {
                    if (!requestPassword)
                        throw SysError(e.toString() + L'\n' + _("Password prompt not permitted by current settings."));
                }

                std::wstring lastErrorMsg;
                for (;;)
                {
                    //2. request (new) password
                    std::wstring msg = replaceCpy(_("Please enter your password to connect to %x."), L"%x", fmtPath(getDisplayPath(AfsPath())));
                    if (lastErrorMsg.empty())
                        msg += L"\n" + _("The password will only be remembered until FreeFileSync is closed.");

                    const Zstring password = requestPassword(msg, lastErrorMsg); //throw X
                    mgr->setSessionPassword(login_, password);

                    try //3. test access:
                    {
                        connectServer(); //throw SysError, SysErrorPassword
                        return;
                    }
                    catch (const SysErrorPassword& e) { lastErrorMsg = e.toString(); }
                }
            }
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Unable to connect to %x."), L"%x", fmtPath(getDisplayPath(AfsPath()))), e.toString()); }
    }

    bool hasNativeTransactionalCopy() const override { return false; }
    //----------------------------------------------------------------------------------------------------------------

    int64_t getFreeDiskSpace(const AfsPath& folderPath) const override { return -1; } //throw FileError, returns < 0 if not available

    std::unique_ptr<RecycleSession> createRecyclerSession(const AfsPath& folderPath) const override //throw FileError, RecycleBinUnavailable
    {
        throw RecycleBinUnavailable(replaceCpy(_("The recycle bin is not available for %x."), L"%x", fmtPath(getDisplayPath(folderPath))));
    }

    void moveToRecycleBin(const AfsPath& itemPath) const override //throw FileError, RecycleBinUnavailable
    {
        throw RecycleBinUnavailable(replaceCpy(_("The recycle bin is not available for %x."), L"%x", fmtPath(getDisplayPath(itemPath))));
    }

    const FtpLogin login_;
};

//===========================================================================================================================

//expects "clean" login data
Zstring concatenateFtpFolderPathPhrase(const FtpLogin& login, const AfsPath& folderPath) //noexcept
{
    Zstring username;
    if (!login.username.empty())
        username = encodeFtpUsername(login.username) + Zstr("@");

    Zstring port;
    if (login.portCfg > 0)
        port = Zstr(':') + numberTo<Zstring>(login.portCfg);

    Zstring relPath = getServerRelPath(folderPath);
    if (relPath == Zstr("/"))
        relPath.clear();

    Zstring options;
    if (login.timeoutSec != FtpLogin().timeoutSec)
        options += Zstr("|timeout=") + numberTo<Zstring>(login.timeoutSec);

    if (login.useTls)
        options += Zstr("|ssl");

    if (login.password)
    {
        if (!login.password->empty()) //password always last => visually truncated by folder input field
            options += Zstr("|pass64=") + encodePasswordBase64(*login.password);
    }
    else
        options += Zstr("|pwprompt");

    return Zstring(ftpPrefix) + Zstr("//") + username + login.server + port + relPath + options;
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
            homePath = session.getHomePath(); //throw SysError
        });
        return homePath;
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot determine final path for %x."), L"%x", fmtPath(getCurlDisplayPath(login, AfsPath(Zstr("~"))))), e.toString()); }
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
    trim(loginTmp.server, TrimSide::both, [](Zchar c) { return c == Zstr('/') || c == Zstr('\\'); });

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


/* syntax: ftp://[<user>[:<password>]@]<server>[:port]/<relative-path>[|option_name=value]

   e.g. ftp://user001:secretpassword@private.example.com:222/mydirectory/
        ftp://user001@private.example.com/mydirectory|pass64=c2VjcmV0cGFzc3dvcmQ       */
AbstractPath fff::createItemPathFtp(const Zstring& itemPathPhrase) //noexcept
{
    Zstring pathPhrase = expandMacros(itemPathPhrase); //expand before trimming!
    trim(pathPhrase);

    if (startsWithAsciiNoCase(pathPhrase, ftpPrefix))
        pathPhrase = pathPhrase.c_str() + strLength(ftpPrefix);
    trim(pathPhrase, TrimSide::left, [](Zchar c) { return c == Zstr('/') || c == Zstr('\\'); });

    const ZstringView credentials = beforeFirst<ZstringView>(pathPhrase, Zstr('@'), IfNotFoundReturn::none);
    const ZstringView fullPathOpt =  afterFirst<ZstringView>(pathPhrase, Zstr('@'), IfNotFoundReturn::all);

    FtpLogin login;
    login.username = decodeFtpUsername(Zstring(beforeFirst(credentials, Zstr(':'), IfNotFoundReturn::all))); //support standard FTP syntax, even though
    login.password =                   Zstring( afterFirst(credentials, Zstr(':'), IfNotFoundReturn::none)); //concatenateFtpFolderPathPhrase() uses "pass64" instead

    const ZstringView fullPath = beforeFirst(fullPathOpt, Zstr('|'), IfNotFoundReturn::all);
    const ZstringView options  =  afterFirst(fullPathOpt, Zstr('|'), IfNotFoundReturn::none);

    auto it = std::find_if(fullPath.begin(), fullPath.end(), [](Zchar c) { return c == '/' || c == '\\'; });
    const ZstringView serverPort = makeStringView(fullPath.begin(), it);
    const AfsPath serverRelPath = sanitizeDeviceRelativePath({it, fullPath.end()});

    login.server           = Zstring(beforeLast(serverPort, Zstr(':'), IfNotFoundReturn::all));
    const ZstringView port =          afterLast(serverPort, Zstr(':'), IfNotFoundReturn::none);
    login.portCfg = stringTo<int>(port); //0 if empty

    split(options, Zstr('|'), [&](ZstringView optPhrase)
    {
        optPhrase = trimCpy(optPhrase);
        if (!optPhrase.empty())
        {
            if (startsWith(optPhrase, Zstr("timeout=")))
                login.timeoutSec = stringTo<int>(afterFirst(optPhrase, Zstr('='), IfNotFoundReturn::none));
            else if (optPhrase == Zstr("ssl"))
                login.useTls = true;
            else if (startsWith(optPhrase, Zstr("pass64=")))
                login.password = decodePasswordBase64(afterFirst(optPhrase, Zstr('='), IfNotFoundReturn::none));
            else if (optPhrase == Zstr("pwprompt"))
                login.password = std::nullopt;
            else
                assert(false);
        }
    });
    return AbstractPath(makeSharedRef<FtpFileSystem>(login), serverRelPath);
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "sftp.h"
#include <array>
#include <zen/sys_error.h>
#include <zen/thread.h>
#include <zen/globals.h>
#include <zen/file_io.h>
#include <zen/socket.h>
#include <zen/open_ssl.h>
#include <zen/resolve_path.h>
#include <libssh2/libssh2_wrap.h> //DON'T include <libssh2_sftp.h> directly!
#include "init_curl_libssh2.h"
#include "ftp_common.h"
#include "abstract_impl.h"
    #include <poll.h>

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


namespace
{
/*
SFTP specification version 3 (implemented by libssh2): https://filezilla-project.org/specs/draft-ietf-secsh-filexfer-02.txt

libssh2: prefer OpenSSL over WinCNG backend:

WinCNG supports the following ciphers:
    rijndael-cbc@lysator.liu.se
    aes256-cbc
    aes192-cbc
    aes128-cbc
    arcfour128
    arcfour
    3des-cbc

OpenSSL supports the same ciphers like WinCNG plus the following:
    aes256-ctr
    aes192-ctr
    aes128-ctr
    cast128-cbc
    blowfish-cbc                    */

constexpr ZstringView sftpPrefix = Zstr("sftp:");

constexpr std::chrono::seconds SFTP_SESSION_MAX_IDLE_TIME           (20);
constexpr std::chrono::seconds SFTP_SESSION_CLEANUP_INTERVAL         (4); //facilitate default of 5-seconds delay for error retry
constexpr std::chrono::seconds SFTP_CHANNEL_LIMIT_DETECTION_TIME_OUT(30);

//permissions for new files: rw- rw- rw- [0666] => consider umask! (e.g. 0022 for ffs.org)
const long SFTP_DEFAULT_PERMISSION_FILE = LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR |
                                          LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IWOTH;

//permissions for new folders: rwx rwx rwx [0777] => consider umask! (e.g. 0022 for ffs.org)
const long SFTP_DEFAULT_PERMISSION_FOLDER = LIBSSH2_SFTP_S_IRWXU |
                                            LIBSSH2_SFTP_S_IRWXG |
                                            LIBSSH2_SFTP_S_IRWXO;

//attention: if operation fails due to time out, e.g. file copy, the cleanup code may hang, too => total delay = 2 x time out interval

const size_t SFTP_OPTIMAL_BLOCK_SIZE_READ  = 16 * MAX_SFTP_READ_SIZE;     //https://github.com/libssh2/libssh2/issues/90
const size_t SFTP_OPTIMAL_BLOCK_SIZE_WRITE = 16 * MAX_SFTP_OUTGOING_SIZE; //need large buffer to mitigate libssh2 stupidly waiting on "acks": https://www.libssh2.org/libssh2_sftp_write.html
static_assert(MAX_SFTP_READ_SIZE == 30000 && MAX_SFTP_OUTGOING_SIZE == 30000, "reevaluate optimal block sizes if these constants change!");

/* Perf Test, Sourceforge frs, SFTP upload, compressed 25 MB test file:

SFTP_OPTIMAL_BLOCK_SIZE_READ:              SFTP_OPTIMAL_BLOCK_SIZE_WRITE:
    multiples of                               multiples of
    MAX_SFTP_READ_SIZE  KB/s                   MAX_SFTP_OUTGOING_SIZE  KB/s
                 1       650                                1          140
                 2      1000                                2          280
                 4      1800                                4          320
                 8      1800                                8          320
                16      1800                               16          320
                32      1800                               32          320
    Filezilla download speed: 1800 KB/s        Filezilla upload speed: 560 KB/s
    DSL maximum download speed: 3060 KB/s      DSL maximum upload speed: 620 KB/s


Perf Test 2: FFS hompage (2022-09-22)

SFTP_OPTIMAL_BLOCK_SIZE_READ:              SFTP_OPTIMAL_BLOCK_SIZE_WRITE:
    multiples of                               multiples of
    MAX_SFTP_READ_SIZE  MB/s                   MAX_SFTP_OUTGOING_SIZE  MB/s
                 1      0,77                                1          0.25
                 2      1,63                                2          0.50
                 4      3,43                                4          0.97
                 8      6,93                                8          1.86
                16      9,41                               16          3.60
                32      9,58                               32          3.83
      Filezilla download speed: 12,2 MB/s        Filezilla upload speed: 4.4 MB/s  -> unfair comparison: FFS seems slower because it includes setup work, e.g. open file handle
    DSL maximum download speed: 12,9 MB/s      DSL maximum upload speed: 4,7 MB/s

=> libssh2_sftp_read/libssh2_sftp_write may take quite long for 16x and larger => use smallest multiple that fills bandwidth!            */


inline
uint16_t getEffectivePort(int portOption)
{
    if (portOption > 0)
        return static_cast<uint16_t>(portOption);
    return DEFAULT_PORT_SFTP;
}


struct SshDeviceId //= what defines a unique SFTP location
{
    /*explicit*/ SshDeviceId(const SftpLogin& login) :
        server(login.server),
        port(getEffectivePort(login.portCfg)),
        username(login.username) {}

    Zstring server;
    uint16_t port; //must be valid port!
    Zstring username;
};
std::weak_ordering operator<=>(const SshDeviceId& lhs, const SshDeviceId& rhs)
{
    //exactly the type of case insensitive comparison we need for server names! https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfow#IDNs
    if (const std::weak_ordering cmp = compareAsciiNoCase(lhs.server, rhs.server);
        cmp != std::weak_ordering::equivalent)
        return cmp;

    return std::tie(lhs.port, lhs.username) <=> //username: case sensitive!
           std::tie(rhs.port, rhs.username);
}
//also needed by compareDeviceSameAfsType(), so can't just replace with hash and use std::unordered_map


struct SshSessionCfg //= config for buffered SFTP session
{
    SshDeviceId deviceId;
    SftpAuthType authType = SftpAuthType::password;
    Zstring password;           //authType == password or keyFile
    Zstring privateKeyFilePath; //authType == keyFile: use PEM-encoded private key (protected by password) for authentication
    bool allowZlib = false;
};
bool operator==(const SshSessionCfg& lhs, const SshSessionCfg& rhs)
{
    if (lhs.deviceId <=> rhs.deviceId != std::weak_ordering::equivalent)
        return false;

    if (std::tie(lhs.authType, lhs.allowZlib) !=
        std::tie(rhs.authType, rhs.allowZlib))
        return false;

    switch (lhs.authType)
    {
        case SftpAuthType::password:
            return lhs.password == rhs.password; //case sensitive!

        case SftpAuthType::keyFile:
            return std::tie(lhs.password, lhs.privateKeyFilePath) == //case sensitive!
                   std::tie(rhs.password, rhs.privateKeyFilePath);   //

        case SftpAuthType::agent:
            return true;
    }
    assert(false);
    return true;
}


Zstring concatenateSftpFolderPathPhrase(const SftpLogin& login, const AfsPath& itemPath); //noexcept


std::string getLibssh2Path(const AfsPath& itemPath)
{
    return utfTo<std::string>(getServerRelPath(itemPath));
}


std::wstring getSftpDisplayPath(const SshDeviceId& deviceId, const AfsPath& itemPath)
{
    Zstring displayPath = Zstring(sftpPrefix) + Zstr("//");

    if (!deviceId.username.empty()) //show username! consider AFS::compareDeviceSameAfsType()
        displayPath += deviceId.username + Zstr('@');

    //if (parseIpv6Address(deviceId.server) && deviceId.port != DEFAULT_PORT_SFTP)
    //    displayPath += Zstr('[') + deviceId.server + Zstr(']');
    //else
    displayPath += deviceId.server;

    //if (deviceId.port != DEFAULT_PORT_SFTP)
    //    displayPath += Zstr(':') + numberTo<Zstring>(deviceId.port);

    const Zstring& relPath = getServerRelPath(itemPath);
    if (relPath != Zstr("/"))
        displayPath += relPath;

    return utfTo<std::wstring>(displayPath);
}

//===========================================================================================================================

//=> most likely *not* a connection issue
struct SysErrorSftpProtocol : public zen::SysError
{
    SysErrorSftpProtocol(const std::wstring& msg, unsigned long sftpError) : SysError(msg), sftpErrorCode(sftpError) {}

    const unsigned long sftpErrorCode;
};

DEFINE_NEW_SYS_ERROR(SysErrorPassword)


constinit Global<UniSessionCounter> globalSftpSessionCount;
GLOBAL_RUN_ONCE(globalSftpSessionCount.set(createUniSessionCounter()));


class SshSession
{
public:
    SshSession(const SshSessionCfg& sessionCfg, int timeoutSec) : //throw SysError, SysErrorPassword
        sessionCfg_(sessionCfg)
    {
        ZEN_ON_SCOPE_FAIL(cleanup()); //destructor call would lead to member double clean-up!!!

        const Zstring& serviceName = numberTo<Zstring>(sessionCfg_.deviceId.port);

        socket_.emplace(sessionCfg_.deviceId.server, serviceName, timeoutSec); //throw SysError

        sshSession_ = ::libssh2_session_init();
        if (!sshSession_) //does not set ssh last error; source: only memory allocation may fail
            throw SysError(formatSystemError("libssh2_session_init", formatSshStatusCode(LIBSSH2_ERROR_ALLOC), L""));

        //if zlib compression causes trouble, make it a user setting: https://freefilesync.org/forum/viewtopic.php?t=6663
        //=> surprise: it IS causing trouble: slow-down in local syncs: https://freefilesync.org/forum/viewtopic.php?t=7244#p24250
        if (sessionCfg_.allowZlib)
            if (const int rc = ::libssh2_session_flag(sshSession_, LIBSSH2_FLAG_COMPRESS, 1);
                rc != 0) //does not set SSH last error
                throw SysError(formatSystemError("libssh2_session_flag", formatSshStatusCode(rc), L""));

        ::libssh2_session_set_blocking(sshSession_, 1);

        //we don't consider the timeout part of the session when it comes to reuse! but we already require it during initialization
        ::libssh2_session_set_timeout(sshSession_, timeoutSec * 1000 /*ms*/);


        if (::libssh2_session_handshake(sshSession_, socket_->get()) != 0)
            throw SysError(formatLastSshError("libssh2_session_handshake", nullptr));

        //evaluate fingerprint = libssh2_hostkey_hash(sshSession_, LIBSSH2_HOSTKEY_HASH_SHA1) ???

        const auto usernameUtf8 = utfTo<std::string>(sessionCfg_.deviceId.username);
        const auto passwordUtf8 = utfTo<std::string>(sessionCfg_.password);

        const char* authList = ::libssh2_userauth_list(sshSession_, usernameUtf8);
        if (!authList)
        {
            if (::libssh2_userauth_authenticated(sshSession_) != 1)
                throw SysError(formatLastSshError("libssh2_userauth_list", nullptr));
            //else: SSH_USERAUTH_NONE has authenticated successfully => we're already done
        }
        else
        {
            bool supportAuthPassword    = false;
            bool supportAuthKeyfile     = false;
            bool supportAuthInteractive = false;
            split(authList, ',', [&](std::string_view authMethod)
            {
                authMethod = trimCpy(authMethod);
                if (!authMethod.empty())
                {
                    if (authMethod == "password")
                        supportAuthPassword = true;
                    else if (authMethod == "publickey")
                        supportAuthKeyfile = true;
                    else if (authMethod == "keyboard-interactive")
                        supportAuthInteractive = true;
                }
            });

            switch (sessionCfg_.authType)
            {
                case SftpAuthType::password:
                {
                    if (supportAuthPassword)
                    {
                        if (::libssh2_userauth_password(sshSession_, usernameUtf8, passwordUtf8) != 0)
                            throw SysErrorPassword(formatLastSshError("libssh2_userauth_password", nullptr));
                    }
                    else if (supportAuthInteractive) //some servers, e.g. web.sourceforge.net, support "keyboard-interactive", but not "password"
                    {
                        std::wstring unexpectedPrompts;

                        auto authCallback = [&](int num_prompts, const LIBSSH2_USERAUTH_KBDINT_PROMPT* prompts, LIBSSH2_USERAUTH_KBDINT_RESPONSE* responses)
                        {
                            //note: FileZilla assumes password requests when it finds "num_prompts == 1" and "!echo" -> prompt may be localized!
                            //test case: sourceforge.net sends a single "Password: " prompt with "!echo"
                            if (num_prompts == 1 && prompts[0].echo == 0)
                            {
                                responses[0].text = //pass ownership; will be ::free()d
                                    ::strdup(passwordUtf8.c_str());
                                responses[0].length = static_cast<unsigned int>(passwordUtf8.size());
                            }
                            else
                                for (int i = 0; i < num_prompts; ++i)
                                    unexpectedPrompts += (unexpectedPrompts.empty() ? L"" : L"|") + utfTo<std::wstring>(makeStringView(reinterpret_cast<const char*>(prompts[i].text), prompts[i].length));
                        };
                        using AuthCbType = decltype(authCallback);

                        auto authCallbackWrapper = [](const char* name, int name_len, const char* instruction, int instruction_len,
                                                      int num_prompts, const LIBSSH2_USERAUTH_KBDINT_PROMPT* prompts, LIBSSH2_USERAUTH_KBDINT_RESPONSE* responses, void** abstract)
                        {
                            try
                            {
                                AuthCbType* callback = *reinterpret_cast<AuthCbType**>(abstract); //free this poor little C-API from its shackles and redirect to a proper lambda
                                (*callback)(num_prompts, prompts, responses); //name, instruction are nullptr for sourceforge.net
                            }
                            catch (...) { assert(false); }
                        };

                        if (*::libssh2_session_abstract(sshSession_))
                            throw SysError(L"libssh2_session_abstract: non-null value");

                        *reinterpret_cast<AuthCbType**>(::libssh2_session_abstract(sshSession_)) = &authCallback;
                        ZEN_ON_SCOPE_EXIT(*::libssh2_session_abstract(sshSession_) = nullptr);

                        if (::libssh2_userauth_keyboard_interactive(sshSession_, usernameUtf8, authCallbackWrapper) != 0)
                            throw SysErrorPassword(formatLastSshError("libssh2_userauth_keyboard_interactive", nullptr) +
                                                   (unexpectedPrompts.empty() ? L"" : L"\nUnexpected prompts: " + unexpectedPrompts));
                    }
                    else
                        throw SysError(replaceCpy(_("The server does not support authentication via %x."), L"%x", L"\"username/password\"") +
                                       L'\n' +_("Required:") + L' ' + utfTo<std::wstring>(authList));
                }
                break;

                case SftpAuthType::keyFile:
                {
                    if (!supportAuthKeyfile)
                        throw SysError(replaceCpy(_("The server does not support authentication via %x."), L"%x", L"\"key file\"") +
                                       L'\n' +_("Required:") + L' ' + utfTo<std::wstring>(authList));

                    std::string passphrase = passwordUtf8;
                    std::string pkStream;
                    try
                    {
                        pkStream = getFileContent(sessionCfg_.privateKeyFilePath, nullptr /*notifyUnbufferedIO*/); //throw FileError
                        trim(pkStream);
                    }
                    catch (const FileError& e) { throw SysError(replaceCpy(e.toString(), L"\n\n", L'\n')); } //errors should be further enriched by context info => SysError

                    //libssh2 doesn't support the PuTTY key file format, but we do!
                    if (isPuttyKeyStream(pkStream))
                        try
                        {
                            pkStream = convertPuttyKeyToPkix(pkStream, passphrase); //throw SysError
                            passphrase.clear();
                        }
                        catch (const SysError& e) //add context
                        {
                            throw SysErrorPassword(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(sessionCfg_.privateKeyFilePath)) + L' ' + e.toString());
                        }

                    if (::libssh2_userauth_publickey_frommemory(sshSession_, usernameUtf8, pkStream, passphrase) != 0) //const char* passphrase
                    {
                        //libssh2_userauth_publickey_frommemory()'s "Unable to extract public key from private key" isn't exactly *helpful*
                        //=> detect invalid key files and give better error message:
                        const wchar_t* invalidKeyFormat = [&]() -> const wchar_t*
                        {
                            //"-----BEGIN PUBLIC KEY-----"      OpenSSH SSH-2 public key (X.509 SubjectPublicKeyInfo) = PKIX
                            //"-----BEGIN RSA PUBLIC KEY-----"  OpenSSH SSH-2 public key (PKCS#1 RSAPublicKey)
                            //"---- BEGIN SSH2 PUBLIC KEY ----" SSH-2 public key (RFC 4716 format)
                            const std::string_view firstLine = makeStringView(pkStream.begin(), std::find_if(pkStream.begin(), pkStream.end(), isLineBreak<char>));
                            if (contains(firstLine, "PUBLIC KEY"))
                                return L"OpenSSH public key";

                            if (startsWith(pkStream, "rsa-") || //rsa-sha2-256, rsa-sha2-512
                                startsWith(pkStream, "ssh-") || //ssh-rsa, ssh-dss, ssh-ed25519, ssh-ed448
                                startsWith(pkStream, "ecdsa-")) //ecdsa-sha2-nistp256, ecdsa-sha2-nistp384, ecdsa-sha2-nistp521
                                return L"OpenSSH public key"; //OpenSSH SSH-2 public key

                            if (std::count(pkStream.begin(), pkStream.end(), ' ') == 2 &&
                            /**/std::all_of(pkStream.begin(), pkStream.end(), [](const char c) { return isDigit(c) || c == ' '; }))
                            return L"SSH-1 public key";

                            //"-----BEGIN PRIVATE KEY-----"                => OpenSSH SSH-2 private key (PKCS#8 PrivateKeyInfo)          => should work
                            //"-----BEGIN ENCRYPTED PRIVATE KEY-----"      => OpenSSH SSH-2 private key (PKCS#8 EncryptedPrivateKeyInfo) => should work
                            //"-----BEGIN RSA PRIVATE KEY-----"            => OpenSSH SSH-2 private key (PKCS#1 RSAPrivateKey)           => should work
                            //"-----BEGIN DSA PRIVATE KEY-----"            => OpenSSH SSH-2 private key (PKCS#1 DSAPrivateKey)           => should work
                            //"-----BEGIN EC PRIVATE KEY-----"             => OpenSSH SSH-2 private key (PKCS#1 ECPrivateKey)            => should work
                            //"-----BEGIN OPENSSH PRIVATE KEY-----"        => OpenSSH SSH-2 private key (new format)                     => should work
                            //"---- BEGIN SSH2 ENCRYPTED PRIVATE KEY ----" => ssh.com SSH-2 private key                                  => unclear
                            //"SSH PRIVATE KEY FILE FORMAT 1.1"            => SSH-1 private key                                          => unclear
                            return nullptr; //other: maybe invalid, maybe not
                        }();
                        if (invalidKeyFormat)
                            throw SysError(_("Authentication failed.") + L' ' +
                                           replaceCpy<std::wstring>(L"%x is not an OpenSSH or PuTTY private key file.", L"%x",
                                                                    fmtPath(sessionCfg_.privateKeyFilePath) + L" [" + invalidKeyFormat + L']'));
                        if (isPuttyKeyStream(pkStream))
                            throw SysError(formatLastSshError("libssh2_userauth_publickey_frommemory", nullptr));
                        else
                            //can't rely on LIBSSH2_ERROR_AUTHENTICATION_FAILED: https://github.com/libssh2/libssh2/pull/789
                            throw SysErrorPassword(formatLastSshError("libssh2_userauth_publickey_frommemory", nullptr));
                    }
                }
                break;

                case SftpAuthType::agent:
                {
                    LIBSSH2_AGENT* sshAgent = ::libssh2_agent_init(sshSession_);
                    if (!sshAgent)
                        throw SysError(formatLastSshError("libssh2_agent_init", nullptr));
                    ZEN_ON_SCOPE_EXIT(::libssh2_agent_free(sshAgent));

                    if (::libssh2_agent_connect(sshAgent) != 0)
                        throw SysError(formatLastSshError("libssh2_agent_connect", nullptr));
                    ZEN_ON_SCOPE_EXIT(::libssh2_agent_disconnect(sshAgent));

                    if (::libssh2_agent_list_identities(sshAgent) != 0)
                        throw SysError(formatLastSshError("libssh2_agent_list_identities", nullptr));

                    for (libssh2_agent_publickey* prev = nullptr;;)
                    {
                        libssh2_agent_publickey* identity = nullptr;
                        const int rc = ::libssh2_agent_get_identity(sshAgent, &identity, prev);
                        if (rc == 0) //public key returned
                            ;
                        else if (rc == 1) //no more public keys
                            throw SysError(L"SSH agent contains no matching public key.");
                        else
                            throw SysError(formatLastSshError("libssh2_agent_get_identity", nullptr));

                        if (::libssh2_agent_userauth(sshAgent, usernameUtf8.c_str(), identity) == 0)
                            break; //authentication successful

                        //else: failed => try next public key
                        prev = identity;
                    }
                }
                break;
            }
        }

        lastSuccessfulUseTime_ = std::chrono::steady_clock::now();
    }

    ~SshSession() { cleanup(); }

    const SshSessionCfg& getSessionCfg() const
    {
        static_assert(std::is_const_v<decltype(sessionCfg_)>, "keep this function thread-safe!");
        return sessionCfg_;
    }

    bool isHealthy() const
    {
        for (const SftpChannelInfo& ci : sftpChannels_)
            if (ci.nbInfo.commandPending)
                return false;

        if (nbInfo_.commandPending)
            return false;

        if (possiblyCorrupted_)
            return false;

        if (std::chrono::steady_clock::now() > lastSuccessfulUseTime_ + SFTP_SESSION_MAX_IDLE_TIME)
            return false;

        return true;
    }

    void markAsCorrupted() { possiblyCorrupted_ = true; }

    struct Details
    {
        LIBSSH2_SESSION* sshSession;
        LIBSSH2_SFTP*   sftpChannel;
    };

    size_t getSftpChannelCount() const { return sftpChannels_.size(); }

    //return "false" if pending
    bool tryNonBlocking(size_t channelNo, std::chrono::steady_clock::time_point commandStartTime, const char* functionName,
                        const std::function<int(const SshSession::Details& sd)>& sftpCommand /*noexcept!*/, int timeoutSec) //throw SysError, SysErrorSftpProtocol
    {
        assert(::libssh2_session_get_blocking(sshSession_));
        ::libssh2_session_set_blocking(sshSession_, 0);
        ZEN_ON_SCOPE_EXIT(::libssh2_session_set_blocking(sshSession_, 1));

        //yes, we're non-blocking, still won't hurt to set the timeout in case libssh2 decides to use it nevertheless
        ::libssh2_session_set_timeout(sshSession_, timeoutSec * 1000 /*ms*/);

        LIBSSH2_SFTP* sftpChannel = channelNo < sftpChannels_.size() ? sftpChannels_[channelNo].sftpChannel : nullptr;
        SftpNonBlockInfo&  nbInfo = channelNo < sftpChannels_.size() ? sftpChannels_[channelNo].nbInfo : nbInfo_;

        if (!nbInfo.commandPending)
            assert(nbInfo.commandStartTime != commandStartTime);
        else if (nbInfo.commandStartTime == commandStartTime && nbInfo.functionName == functionName)
            ; //continue pending SFTP call
        else
        {
            assert(false); //pending sftp command is not completed by client: e.g. libssh2_sftp_close() cleaning up after a timed-out libssh2_sftp_read()
            possiblyCorrupted_ = true; //=> start new command (with new start time), but remember to not trust this session anymore!
        }
        nbInfo.commandPending   = true;
        nbInfo.commandStartTime = commandStartTime;
        nbInfo.functionName     = functionName;

        int rc = LIBSSH2_ERROR_NONE;
        try
        {
            rc = sftpCommand({sshSession_, sftpChannel}); //noexcept
        }
        catch (...) { assert(false); rc = LIBSSH2_ERROR_BAD_USE; }

        assert(rc >= 0 || ::libssh2_session_last_errno(sshSession_) == rc);
        if (rc < 0 && ::libssh2_session_last_errno(sshSession_) != rc) //when libssh2 fails to properly set last error; e.g. https://github.com/libssh2/libssh2/pull/123
            ::libssh2_session_set_last_error(sshSession_, rc, nullptr);

        if (rc >= LIBSSH2_ERROR_NONE ||
            (rc == LIBSSH2_ERROR_SFTP_PROTOCOL && ::libssh2_sftp_last_error(sftpChannel) != LIBSSH2_FX_OK))
            //libssh2 source: LIBSSH2_ERROR_SFTP_PROTOCOL *without* setting LIBSSH2_SFTP::last_errno indicates a corrupted connection!
        {
            nbInfo.commandPending = false;                             //
            lastSuccessfulUseTime_ = std::chrono::steady_clock::now(); //[!] LIBSSH2_ERROR_SFTP_PROTOCOL is NOT an SSH error => the SSH session is just fine!

            if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
                throw SysErrorSftpProtocol(formatLastSshError(functionName, sftpChannel), ::libssh2_sftp_last_error(sftpChannel));
            return true;
        }
        else if (rc == LIBSSH2_ERROR_EAGAIN)
        {
            if (std::chrono::steady_clock::now() > nbInfo.commandStartTime + std::chrono::seconds(timeoutSec))
                //consider SSH session corrupted! => isHealthy() will see pending command
                throw SysError(formatSystemError(functionName, formatSshStatusCode(LIBSSH2_ERROR_TIMEOUT),
                                                 _P("Operation timed out after 1 second.", "Operation timed out after %x seconds.", timeoutSec)));
            return false;
        }
        else //=> SSH session errors only (hopefully!) e.g. LIBSSH2_ERROR_SOCKET_RECV
            //consider SSH session corrupted! => isHealthy() will see pending command
            throw SysError(formatLastSshError(functionName, sftpChannel));
    }

    //returns when traffic is available or time out: both cases are handled by next tryNonBlocking() call
    static void waitForTraffic(const std::vector<SshSession*>& sshSessions, int timeoutSec) //throw SysError
    {
        //reference: session.c: _libssh2_wait_socket()
        std::vector<pollfd> fds;
        std::chrono::steady_clock::time_point startTimeMin = std::chrono::steady_clock::time_point::max();

        for (SshSession* session : sshSessions)
        {
            assert(::libssh2_session_last_errno(session->sshSession_) == LIBSSH2_ERROR_EAGAIN);
            assert(session->nbInfo_.commandPending || std::any_of(session->sftpChannels_.begin(), session->sftpChannels_.end(), [](SftpChannelInfo& ci) { return ci.nbInfo.commandPending; }));

            pollfd pfd{.fd = session->socket_->get()};

            const int dir = ::libssh2_session_block_directions(session->sshSession_);
            assert(dir != 0); //we assert a blocked direction after libssh2 returned LIBSSH2_ERROR_EAGAIN!
            if (dir & LIBSSH2_SESSION_BLOCK_INBOUND)
                pfd.events |= POLLIN;
            if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
                pfd.events |= POLLOUT;

            if (pfd.events != 0)
                fds.push_back(pfd);

            for (const SftpChannelInfo& ci : session->sftpChannels_)
                if (ci.nbInfo.commandPending)
                    startTimeMin = std::min(startTimeMin, ci.nbInfo.commandStartTime);
            if (session->nbInfo_.commandPending)
                startTimeMin = std::min(startTimeMin, session->nbInfo_.commandStartTime);
        }

        if (!fds.empty())
        {
            assert(startTimeMin != std::chrono::steady_clock::time_point::max());
            const auto now = std::chrono::steady_clock::now();
            const auto stopTime = startTimeMin + std::chrono::seconds(timeoutSec);
            if (now >= stopTime)
                return; //time-out! => let next tryNonBlocking() call fail with detailed error!
            const auto waitTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - now).count();

            //is poll() on macOS broken? https://daniel.haxx.se/blog/2016/10/11/poll-on-mac-10-12-is-broken/
            //      it seems Daniel only takes issue with "empty" input handling!? => not an issue for us
            const char* functionName = "poll";
            const int rv = ::poll(fds.data(),  //struct pollfd* fds
                                  fds.size(),  //nfds_t nfds
                                  waitTimeMs); //int timeout [ms]
            if (rv < 0) //consider SSH sessions corrupted! => isHealthy() will see pending commands
                throw SysError(formatSystemError(functionName, getLastError()));

            if (rv == 0) //time-out! => let next tryNonBlocking() call fail with detailed error!
                return;
        }
        else assert(false);
    }

    static void addSftpChannel(const std::vector<SshSession*>& sshSessions, int timeoutSec) //throw SysError
    {
        auto addChannelDetails = [](const std::wstring& msg, SshSession& sshSession) //when hitting the server's SFTP channel limit, inform user about channel number
        {
            if (sshSession.sftpChannels_.empty())
                return msg;
            return msg + L' ' + replaceCpy(_("Failed to open SFTP channel number %x."), L"%x", formatNumber(sshSession.sftpChannels_.size() + 1));
        };

        std::optional<SysError> firstSysError;

        std::vector<SshSession*> pendingSessions = sshSessions;
        const auto sftpCommandStartTime = std::chrono::steady_clock::now();

        for (;;)
        {
            //create all SFTP sessions in parallel => non-blocking
            //note: each libssh2_sftp_init() consists of multiple round-trips => poll until all sessions are finished, don't just init and then block on each!
            for (size_t pos = pendingSessions.size(); pos-- > 0 ; ) //CAREFUL WITH THESE ERASEs (invalidate positions!!!)
                try
                {
                    if (pendingSessions[pos]->tryNonBlocking(static_cast<size_t>(-1), sftpCommandStartTime, "libssh2_sftp_init",
                                                             [&](const SshSession::Details& sd) //noexcept!
                {
                    LIBSSH2_SFTP* sftpChannelNew = ::libssh2_sftp_init(sd.sshSession);
                        if (!sftpChannelNew)
                            return std::min(::libssh2_session_last_errno(sd.sshSession), LIBSSH2_ERROR_SOCKET_NONE);
                        //just in case libssh2 failed to properly set last error; e.g. https://github.com/libssh2/libssh2/pull/123

                        pendingSessions[pos]->sftpChannels_.emplace_back(sftpChannelNew);
                        return LIBSSH2_ERROR_NONE;
                    }, timeoutSec)) //throw SysError, (SysErrorSftpProtocol)
                    pendingSessions.erase(pendingSessions.begin() + pos); //= not pending
                }
                catch (const SysError& e)
                {
                    if (!firstSysError) //don't throw yet and corrupt other valid, but pending SshSessions! We also don't want to leak LIBSSH2_SFTP* waiting in libssh2 code
                        firstSysError = SysError(addChannelDetails(e.toString(), *pendingSessions[pos]));
                    //SysErrorSftpProtocol? unexpected during libssh2_sftp_init()
                    //-> still occuring for whatever reason!? => "slice" down to SysError
                    pendingSessions.erase(pendingSessions.begin() + pos);
                }

            if (pendingSessions.empty())
            {
                if (firstSysError)
                    throw* firstSysError;
                return;
            }

            waitForTraffic(pendingSessions, timeoutSec); //throw SysError
        }
    }

private:
    SshSession           (const SshSession&) = delete;
    SshSession& operator=(const SshSession&) = delete;

    void cleanup() //attention: may block heavily after error!
    {
        for (SftpChannelInfo& ci : sftpChannels_)
            //ci.nbInfo.commandPending? => may "legitimately" happen when an SFTP command times out
            if (::libssh2_sftp_shutdown(ci.sftpChannel) != LIBSSH2_ERROR_NONE)
                assert(false);

        if (sshSession_)
        {
            //*INDENT-OFF*
            if (!nbInfo_.commandPending && std::all_of(sftpChannels_.begin(), sftpChannels_.end(),
                [](const SftpChannelInfo& ci) { return !ci.nbInfo.commandPending; }))
                if (::libssh2_session_disconnect(sshSession_, "FreeFileSync says \"bye\"!") != LIBSSH2_ERROR_NONE) //= server notification only! no local cleanup apparently
                    assert(false);
            //else: avoid further stress on the broken SSH session and take French leave

            //nbInfo_.commandPending? => have to clean up, no matter what!
            if (::libssh2_session_free(sshSession_) != LIBSSH2_ERROR_NONE)
                assert(false);
            //*INDENT-ON*
        }
    }

    std::wstring formatLastSshError(const char* functionName, LIBSSH2_SFTP* sftpChannel /*optional*/) const
    {
        char* lastErrorMsg = nullptr; //owned by "sshSession"
        const int sshStatusCode = ::libssh2_session_last_error(sshSession_, &lastErrorMsg, nullptr, false /*want_buf*/);
        assert(lastErrorMsg);

        std::wstring errorMsg;
        if (lastErrorMsg)
            errorMsg = trimCpy(utfTo<std::wstring>(lastErrorMsg));

        //LIBSSH2_ERROR_SFTP_PROTOCOL does *not* mean libssh2_sftp_last_error() is also available!
        //But if it's not, we have a broken connection, and lastErrorMsg contains meaningful details!
        if (sshStatusCode == LIBSSH2_ERROR_SFTP_PROTOCOL && ::libssh2_sftp_last_error(sftpChannel) != LIBSSH2_FX_OK)
        {
            if (errorMsg == L"SFTP Protocol Error") //that's trite!
                errorMsg.clear();
            return formatSystemError(functionName, formatSftpStatusCode(::libssh2_sftp_last_error(sftpChannel)), errorMsg);
        }

        return formatSystemError(functionName, formatSshStatusCode(sshStatusCode), errorMsg);
    }

    struct SftpNonBlockInfo
    {
        bool commandPending = false;
        std::chrono::steady_clock::time_point commandStartTime; //specified by client, try to detect libssh2 usage errors
        std::string functionName;
    };

    struct SftpChannelInfo
    {
        explicit SftpChannelInfo(LIBSSH2_SFTP* sc) : sftpChannel(sc) {}

        LIBSSH2_SFTP* sftpChannel = nullptr;
        SftpNonBlockInfo nbInfo;
    };

    std::optional<Socket> socket_; //*bound* after constructor has run
    LIBSSH2_SESSION* sshSession_ = nullptr;
    std::vector<SftpChannelInfo> sftpChannels_;
    bool possiblyCorrupted_ = false;

    SftpNonBlockInfo nbInfo_; //for SSH session, e.g. libssh2_sftp_init()

    const SshSessionCfg sessionCfg_;
    const std::shared_ptr<UniCounterCookie> libsshCurlUnifiedInitCookie_{(getLibsshCurlUnifiedInitCookie(globalSftpSessionCount))}; //throw SysError
    std::chrono::steady_clock::time_point lastSuccessfulUseTime_; //...of the SSH session (but not necessarily the SFTP functionality!)
};

//===========================================================================================================================
//===========================================================================================================================

class SftpSessionManager //reuse (healthy) SFTP sessions globally
{
    struct SshSessionCache;

public:
    SftpSessionManager() : sessionCleaner_([this]
    {
        setCurrentThreadName(Zstr("Session Cleaner[SFTP]"));
        runGlobalSessionCleanUp(); /*throw ThreadStopRequest*/
    }) {}

    struct ReUseOnDelete
    {
        void operator()(SshSession* s) const;
    };

    class SshSessionShared
    {
    public:
        SshSessionShared(std::unique_ptr<SshSession, ReUseOnDelete>&& idleSession, int timeoutSec) :
            session_(std::move(idleSession)) /*bound!*/, timeoutSec_(timeoutSec) { /*assert(session_->isHealthy());*/ }

        //we need two-step initialization: 1. constructor is FAST and noexcept 2. init() is SLOW and throws
        void initSftpChannel() //throw SysError
        {
            if (session_->getSftpChannelCount() == 0) //make sure the SSH session contains at least one SFTP channel
                SshSession::addSftpChannel({session_.get()}, timeoutSec_); //throw SysError
        }

        void executeBlocking(const char* functionName, const std::function<int(const SshSession::Details& sd)>& sftpCommand /*noexcept!*/) //throw SysError, SysErrorSftpProtocol
        {
            assert(threadId_ == std::this_thread::get_id());
            assert(session_->getSftpChannelCount() > 0);
            const auto sftpCommandStartTime = std::chrono::steady_clock::now();

            for (;;)
                if (session_->tryNonBlocking(0 /*channelNo*/, sftpCommandStartTime, functionName, sftpCommand, timeoutSec_)) //throw SysError, SysErrorSftpProtocol
                    return;
                else //pending
                    SshSession::waitForTraffic({session_.get()}, timeoutSec_); //throw SysError
        }

        const SshSessionCfg& getSessionCfg() const { return session_->getSessionCfg(); } //thread-safe

    private:
        std::unique_ptr<SshSession, ReUseOnDelete> session_; //bound!
        const std::thread::id threadId_ = std::this_thread::get_id();
        const int timeoutSec_;
    };

    class SshSessionExclusive
    {
    public:
        SshSessionExclusive(std::unique_ptr<SshSession, ReUseOnDelete>&& idleSession, int timeoutSec) :
            session_(std::move(idleSession)) /*bound!*/, timeoutSec_(timeoutSec) { /*assert(session_->isHealthy());*/ }

        bool tryNonBlocking(size_t channelNo, std::chrono::steady_clock::time_point commandStartTime, const char* functionName, //throw SysError, SysErrorSftpProtocol
                            const std::function<int(const SshSession::Details& sd)>& sftpCommand /*noexcept!*/)
        {
            return session_->tryNonBlocking(channelNo, commandStartTime, functionName, sftpCommand, timeoutSec_); //throw SysError, SysErrorSftpProtocol
        }

        void waitForTraffic() //throw SysError
        {
            SshSession::waitForTraffic({session_.get()}, timeoutSec_); //throw SysError
        }

        size_t getSftpChannelCount() const { return session_->getSftpChannelCount(); }

        void markAsCorrupted() { session_->markAsCorrupted(); }

        static void addSftpChannel(const std::vector<SshSessionExclusive*>& exSessions) //throw SysError
        {
            std::vector<SshSession*> sshSessions;
            for (SshSessionExclusive* exSession : exSessions)
                sshSessions.push_back(exSession->session_.get());

            int timeoutSec = 0;
            for (SshSessionExclusive* exSession : exSessions)
                timeoutSec = std::max(timeoutSec, exSession->timeoutSec_);

            SshSession::addSftpChannel(sshSessions, timeoutSec); //throw SysError
        }

        static void waitForTraffic(const std::vector<SshSessionExclusive*>& exSessions) //throw SysError
        {
            std::vector<SshSession*> sshSessions;
            for (SshSessionExclusive* exSession : exSessions)
                sshSessions.push_back(exSession->session_.get());

            int timeoutSec = 0;
            for (SshSessionExclusive* exSession : exSessions)
                timeoutSec = std::max(timeoutSec, exSession->timeoutSec_);

            SshSession::waitForTraffic(sshSessions, timeoutSec); //throw SysError
        }

        const SshSessionCfg& getSessionCfg() const { return session_->getSessionCfg(); } //thread-safe

    private:
        std::unique_ptr<SshSession, ReUseOnDelete> session_; //bound!
        const int timeoutSec_;
    };


    std::shared_ptr<SshSessionShared> getSharedSession(const SftpLogin& login) //throw SysError, SysErrorPassword
    {
        Protected<SshSessionCache>& sessionCache = getSessionCache(login);

        const std::thread::id threadId = std::this_thread::get_id();
        std::shared_ptr<SshSessionShared> sharedSession; //either or
        std::optional<SshSessionCfg> sessionCfg;         //

        sessionCache.access([&](SshSessionCache& cache)
        {
            if (!cache.activeCfg) //AFS::authenticateAccess() not called => authenticate implicitly!
                setActiveConfig(cache, login);

            std::weak_ptr<SshSessionShared>& sharedSessionWeak = cache.sshSessionsWithThreadAffinity[threadId]; //get or create
            if (auto session = sharedSessionWeak.lock())
                //dereference session ONLY after affinity to THIS thread was confirmed!!!
                //assume "isHealthy()" to avoid hitting server connection limits: (clean up of !isHealthy() after use; idle sessions via worker thread)
                sharedSession = session;

            if (!sharedSession)
                //assume "isHealthy()" to avoid hitting server connection limits: (clean up of !isHealthy() after use; idle sessions via worker thread)
                if (!cache.idleSshSessions.empty())
                {
                    std::unique_ptr<SshSession, ReUseOnDelete> sshSession(cache.idleSshSessions.back().release());
                    /**/                                                  cache.idleSshSessions.pop_back();
                    sharedSessionWeak = sharedSession = std::make_shared<SshSessionShared>(std::move(sshSession), login.timeoutSec); //still holding lock => constructor must be *fast*!
                }
            if (!sharedSession)
                sessionCfg = *cache.activeCfg;
        });

        //create new SFTP session outside the lock: 1. don't block other threads 2. non-atomic regarding "sessionCache"! => one session too many is not a problem!
        if (!sharedSession)
        {
            sharedSession = std::make_shared<SshSessionShared>(std::unique_ptr<SshSession, ReUseOnDelete>(new SshSession(*sessionCfg, login.timeoutSec)), login.timeoutSec); //throw SysError, SysErrorPassword

            sessionCache.access([&](SshSessionCache& cache)
            {
                if (sharedSession->getSessionCfg() == *cache.activeCfg) //created outside the lock => check *again*
                    cache.sshSessionsWithThreadAffinity[threadId] = sharedSession;
            });
        }

        //finish two-step initialization outside the lock: BLOCKING!
        sharedSession->initSftpChannel(); //throw SysError

        return sharedSession;
    }


    std::unique_ptr<SshSessionExclusive> getExclusiveSession(const SftpLogin& login) //throw SysError
    {
        std::unique_ptr<SshSession, ReUseOnDelete> sshSession; //either or
        std::optional<SshSessionCfg> sessionCfg;               //

        getSessionCache(login).access([&](SshSessionCache& cache)
        {
            if (!cache.activeCfg) //AFS::authenticateAccess() not called => authenticate implicitly!
                setActiveConfig(cache, login);

            //assume "isHealthy()" to avoid hitting server connection limits: (clean up of !isHealthy() after use, idle sessions via worker thread)
            if (!cache.idleSshSessions.empty())
            {
                sshSession.reset(cache.idleSshSessions.back().release());
                /**/             cache.idleSshSessions.pop_back();
            }
            else
                sessionCfg = *cache.activeCfg;
        });

        //create new SFTP session outside the lock: 1. don't block other threads 2. non-atomic regarding "sessionCache"! => one session too many is not a problem!
        if (!sshSession)
            sshSession.reset(new SshSession(*sessionCfg, login.timeoutSec)); //throw SysError, SysErrorPassword

        return std::make_unique<SshSessionExclusive>(std::move(sshSession), login.timeoutSec);
    }

    void setActiveConfig(const SftpLogin& login)
    {
        getSessionCache(login).access([&](SshSessionCache& cache) { setActiveConfig(cache, login); });
    }

    void setSessionPassword(const SftpLogin& login, const Zstring& password, SftpAuthType authType)
    {
        getSessionCache(login).access([&](SshSessionCache& cache)
        {
            (authType == SftpAuthType::password ? cache.sessionPassword : cache.sessionPassphrase) = password;
            setActiveConfig(cache, login);
        });
    }

private:
    SftpSessionManager           (const SftpSessionManager&) = delete;
    SftpSessionManager& operator=(const SftpSessionManager&) = delete;

    Protected<SshSessionCache>& getSessionCache(const SshDeviceId& deviceId)
    {
        //single global session store per login; life-time bound to globalInstance => never remove a sessionCache!!!
        Protected<SshSessionCache>* sessionCache = nullptr;

        globalSessionCache_.access([&](GlobalSshSessions& sessionsById)
        {
            sessionCache = &sessionsById[deviceId]; //get or create
        });
        static_assert(std::is_same_v<GlobalSshSessions, std::map<SshDeviceId, Protected<SshSessionCache>>>, "require std::map so that the pointers we return remain stable");

        return *sessionCache;
    }

    void setActiveConfig(SshSessionCache& cache, const SftpLogin& login)
    {
        const Zstring password = [&]
        {
            if (login.authType == SftpAuthType::password ||
                login.authType == SftpAuthType::keyFile)
            {
                if (login.password)
                    return *login.password;

                return login.authType == SftpAuthType::password ? cache.sessionPassword : cache.sessionPassphrase;
            }
            return Zstring();
        }();

        if (cache.activeCfg)
        {
            assert(std::all_of(cache.idleSshSessions.begin(), cache.idleSshSessions.end(),
            [&](const std::unique_ptr<SshSession>& session) { return session->getSessionCfg() == cache.activeCfg; }));

            assert(std::all_of(cache.sshSessionsWithThreadAffinity.begin(), cache.sshSessionsWithThreadAffinity.end(), [&](const auto& v)
            {
                if (std::shared_ptr<SshSessionShared> sharedSession = v.second.lock())
                    return sharedSession->getSessionCfg() /*thread-safe!*/ == cache.activeCfg;
                return true;
            }));
        }
        else
            assert(cache.idleSshSessions.empty() && cache.sshSessionsWithThreadAffinity.empty());

        const std::optional<SshSessionCfg> prevCfg = cache.activeCfg;

        cache.activeCfg =
        {
            .deviceId{login},
            .authType = login.authType,
            .password = password,
            .privateKeyFilePath = login.privateKeyFilePath,
            .allowZlib = login.allowZlib,
        };

        /* remove incompatible sessions:
            - avoid hitting FTP connection limit if some config uses TLS, but not the other: https://freefilesync.org/forum/viewtopic.php?t=8532
            - logically consistent with AFS::compareDevice()
            - don't allow different authentication methods, when authenticateAccess() is called *once* per device in getFolderStatusParallel()
            - what user expects, e.g. when tesing changed settings in SFTP login dialog      */
        if (cache.activeCfg != prevCfg)
        {
            cache.idleSshSessions              .clear(); //run ~SshSession *inside* the lock! => avoid hitting server limits!
            cache.sshSessionsWithThreadAffinity.clear(); //
            //=> incompatible sessions will be deleted by ReUseOnDelete(); until then: additionally counts towards SFTP connection limit :(
        }
    }

    //run a dedicated clean-up thread => it's unclear when the server let's a connection time out, so we do it preemptively
    //context of worker thread:
    void runGlobalSessionCleanUp() //throw ThreadStopRequest
    {
        std::chrono::steady_clock::time_point lastCleanupTime;
        for (;;)
        {
            const auto now = std::chrono::steady_clock::now();

            if (now < lastCleanupTime + SFTP_SESSION_CLEANUP_INTERVAL)
                interruptibleSleep(lastCleanupTime + SFTP_SESSION_CLEANUP_INTERVAL - now); //throw ThreadStopRequest

            lastCleanupTime = std::chrono::steady_clock::now();

            std::vector<Protected<SshSessionCache>*> sessionCaches; //pointers remain stable, thanks to std::map<>

            globalSessionCache_.access([&](GlobalSshSessions& sessionsById)
            {
                for (auto& [sessionId, idleSession] : sessionsById)
                    sessionCaches.push_back(&idleSession);
            });
            for (Protected<SshSessionCache>* sessionCache : sessionCaches)
                for (;;)
                {
                    bool done = false;
                    sessionCache->access([&](SshSessionCache& cache)
                    {
                        for (std::unique_ptr<SshSession>& sshSession : cache.idleSshSessions)
                            if (!sshSession->isHealthy()) //!isHealthy() sessions are destroyed after use => in this context this means they have been idle for too long
                            {
                                sshSession.swap(cache.idleSshSessions.back());
                                /**/            cache.idleSshSessions.pop_back(); //run ~SshSession *inside* the lock! => avoid hitting server limits!
                                return; //don't hold lock for too long: delete only one session at a time, then yield...
                            }
                        std::erase_if(cache.sshSessionsWithThreadAffinity, [](const auto& v) { return v.second.expired(); }); //clean up dangling weak pointer
                        done = true;
                    });
                    if (done)
                        break;
                    std::this_thread::yield(); //outside the lock
                }
        }
    }

    struct SshSessionCache
    {
        //invariant: all cached sessions correspond to activeCfg at any time!
        std::vector<std::unique_ptr<SshSession>>                             idleSshSessions; //extract *temporarily* from this list during use
        std::unordered_map<std::thread::id, std::weak_ptr<SshSessionShared>> sshSessionsWithThreadAffinity; //Win32 thread IDs may be REUSED! still, shouldn't be a problem...

        std::optional<SshSessionCfg> activeCfg;

        Zstring sessionPassword;   //user/password
        Zstring sessionPassphrase; //keyfile/passphrase
    };

    using GlobalSshSessions = std::map<SshDeviceId, Protected<SshSessionCache>>;
    Protected<GlobalSshSessions> globalSessionCache_;

    InterruptibleThread sessionCleaner_;
};

//--------------------------------------------------------------------------------------
UniInitializer globalInitSftp(*globalSftpSessionCount.get());

constinit Global<SftpSessionManager> globalSftpSessionManager; //caveat: life time must be subset of static UniInitializer!
//--------------------------------------------------------------------------------------


void SftpSessionManager::ReUseOnDelete::operator()(SshSession* session) const
{
    //assert(session); -> custom deleter is only called on non-null pointer
    if (session->isHealthy()) //thread that created the "!isHealthy()" session is responsible for clean up (avoid hitting server connection limits!)
        if (std::shared_ptr<SftpSessionManager> mgr = globalSftpSessionManager.get())
            mgr->getSessionCache(session->getSessionCfg().deviceId).access([&](SshSessionCache& cache)
        {
            assert(cache.activeCfg);
            if (cache.activeCfg && session->getSessionCfg() == *cache.activeCfg)
                cache.idleSshSessions.emplace_back(std::exchange(session, nullptr)); //pass ownership
        });
    delete session;
}


std::shared_ptr<SftpSessionManager::SshSessionShared> getSharedSftpSession(const SftpLogin& login) //throw SysError
{
    if (const std::shared_ptr<SftpSessionManager> mgr = globalSftpSessionManager.get())
        return mgr->getSharedSession(login); //throw SysError, SysErrorPassword

    throw SysError(formatSystemError("getSharedSftpSession", L"", L"Function call not allowed during init/shutdown."));
}


std::unique_ptr<SftpSessionManager::SshSessionExclusive> getExclusiveSftpSession(const SftpLogin& login) //throw SysError
{
    if (const std::shared_ptr<SftpSessionManager> mgr = globalSftpSessionManager.get())
        return mgr->getExclusiveSession(login); //throw SysError

    throw SysError(formatSystemError("getExclusiveSftpSession", L"", L"Function call not allowed during init/shutdown."));
}


void runSftpCommand(const SftpLogin& login, const char* functionName,
                    const std::function<int(const SshSession::Details& sd)>& sftpCommand /*noexcept!*/) //throw SysError, SysErrorSftpProtocol
{
    std::shared_ptr<SftpSessionManager::SshSessionShared> asyncSession = getSharedSftpSession(login); //throw SysError
    //no need to protect against concurrency: shared session is (temporarily) bound to current thread

    asyncSession->executeBlocking(functionName, sftpCommand); //throw SysError, SysErrorSftpProtocol
}

//===========================================================================================================================
//===========================================================================================================================
struct SftpItemDetails
{
    AFS::ItemType type;
    uint64_t      fileSize;
    time_t        modTime;
};
struct SftpItem
{
    Zstring         itemName;
    SftpItemDetails details;
};
std::vector<SftpItem> getDirContentFlat(const SftpLogin& login, const AfsPath& dirPath) //throw FileError
{
    LIBSSH2_SFTP_HANDLE* dirHandle = nullptr;
    try
    {
        runSftpCommand(login, "libssh2_sftp_opendir", //throw SysError, SysErrorSftpProtocol
                       [&](const SshSession::Details& sd) //noexcept!
        {
            dirHandle = ::libssh2_sftp_opendir(sd.sftpChannel, getLibssh2Path(dirPath));
            if (!dirHandle)
                return std::min(::libssh2_session_last_errno(sd.sshSession), LIBSSH2_ERROR_SOCKET_NONE);
            return LIBSSH2_ERROR_NONE;
        });
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot open directory %x."), L"%x", fmtPath(getSftpDisplayPath(login, dirPath))), e.toString()); }

    ZEN_ON_SCOPE_EXIT(try
    {
        runSftpCommand(login, "libssh2_sftp_closedir", //throw SysError, SysErrorSftpProtocol
        [&](const SshSession::Details& sd) { return ::libssh2_sftp_closedir(dirHandle); }); //noexcept!
    }
    catch (const SysError& e) { logExtraError(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(getSftpDisplayPath(login, dirPath))) + L"\n\n" + e.toString()); });

    std::vector<SftpItem> output;
    for (;;)
    {
        std::array<char, 1024> buf; //libssh2 sample code uses 512; in practice NAME_MAX(255)+1 should suffice: https://serverfault.com/questions/9546/filename-length-limits-on-linux
        LIBSSH2_SFTP_ATTRIBUTES attribs = {};
        int rc = 0;
        try
        {
            runSftpCommand(login, "libssh2_sftp_readdir", //throw SysError, SysErrorSftpProtocol
            [&](const SshSession::Details& sd) { return rc = ::libssh2_sftp_readdir(dirHandle, buf.data(), buf.size(), &attribs); }); //noexcept!
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(getSftpDisplayPath(login, dirPath))), e.toString()); }

        if (rc == 0) //no more items
            return output;

        const std::string_view sftpItemName = makeStringView(buf.data(), rc);

        if (sftpItemName == "." || sftpItemName == "..") //check needed for SFTP, too!
            continue;

        const Zstring& itemName = utfTo<Zstring>(sftpItemName);
        const AfsPath itemPath(appendPath(dirPath.value, itemName));

        if ((attribs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) == 0) //server probably does not support these attributes => fail at folder level
            throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getSftpDisplayPath(login, itemPath))), L"File attributes not available.");

        if (LIBSSH2_SFTP_S_ISLNK(attribs.permissions))
        {
            if ((attribs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) == 0) //server probably does not support these attributes => fail at folder level
                throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getSftpDisplayPath(login, itemPath))), L"Modification time not supported.");
            output.push_back({itemName, {AFS::ItemType::symlink, 0, static_cast<time_t>(attribs.mtime)}});
        }
        else if (LIBSSH2_SFTP_S_ISDIR(attribs.permissions))
            output.push_back({itemName, {AFS::ItemType::folder, 0, static_cast<time_t>(attribs.mtime)}});
        else //a file or named pipe, ect: LIBSSH2_SFTP_S_ISREG, LIBSSH2_SFTP_S_ISCHR, LIBSSH2_SFTP_S_ISBLK, LIBSSH2_SFTP_S_ISFIFO, LIBSSH2_SFTP_S_ISSOCK
        {
            if ((attribs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) == 0) //server probably does not support these attributes => fail at folder level
                throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getSftpDisplayPath(login, itemPath))), L"Modification time not supported.");
            if ((attribs.flags & LIBSSH2_SFTP_ATTR_SIZE) == 0)
                throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getSftpDisplayPath(login, itemPath))), L"File size not supported.");
            output.push_back({itemName, {AFS::ItemType::file, attribs.filesize, static_cast<time_t>(attribs.mtime)}});
        }
    }
}


SftpItemDetails getSymlinkTargetDetails(const SftpLogin& login, const AfsPath& linkPath) //throw FileError
{
    LIBSSH2_SFTP_ATTRIBUTES attribsTrg = {};
    try
    {
        runSftpCommand(login, "libssh2_sftp_stat", //throw SysError, SysErrorSftpProtocol
        [&](const SshSession::Details& sd) { return ::libssh2_sftp_stat(sd.sftpChannel, getLibssh2Path(linkPath), &attribsTrg); }); //noexcept!
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(getSftpDisplayPath(login, linkPath))), e.toString()); }

    if ((attribsTrg.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) == 0)
        throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getSftpDisplayPath(login, linkPath))), L"File attributes not available.");

    if (LIBSSH2_SFTP_S_ISDIR(attribsTrg.permissions))
        return {AFS::ItemType::folder, 0, static_cast<time_t>(attribsTrg.mtime)};
    else
    {
        if ((attribsTrg.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) == 0) //server probably does not support these attributes => should fail at folder level!
            throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getSftpDisplayPath(login, linkPath))), L"Modification time not supported.");
        if ((attribsTrg.flags & LIBSSH2_SFTP_ATTR_SIZE) == 0)
            throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getSftpDisplayPath(login, linkPath))), L"File size not supported.");

        return {AFS::ItemType::file, attribsTrg.filesize, static_cast<time_t>(attribsTrg.mtime)};
    }
}


class SingleFolderTraverser
{
public:
    using WorkItem = std::pair<AfsPath, std::shared_ptr<AFS::TraverserCallback>>;

    SingleFolderTraverser(const SftpLogin& login, const std::vector<std::pair<AfsPath, std::shared_ptr<AFS::TraverserCallback>>>& workload /*throw X*/) :
        login_(login)
    {
        for (const auto& [folderPath, cb] : workload)
            workload_.push_back(WorkItem{folderPath, cb});

        while (!workload_.empty())
        {
            auto wi = std::move(workload_.    front()); //yes, no strong exception guarantee (std::bad_alloc)
            /**/                workload_.pop_front();  //
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
        for (const SftpItem& item : getDirContentFlat(login_, dirPath)) //throw FileError
        {
            const AfsPath itemPath(appendPath(dirPath.value, item.itemName));

            switch (item.details.type)
            {
                case AFS::ItemType::file:
                    cb.onFile({item.itemName, item.details.fileSize, item.details.modTime, AFS::FingerPrint() /*not supported by SFTP*/, false /*isFollowedSymlink*/}); //throw X
                    break;

                case AFS::ItemType::folder:
                    if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb.onFolder({item.itemName, false /*isFollowedSymlink*/})) //throw X
                        workload_.push_back(WorkItem{itemPath, std::move(cbSub)});
                    break;

                case AFS::ItemType::symlink:
                    switch (cb.onSymlink({item.itemName, item.details.modTime})) //throw X
                    {
                        case AFS::TraverserCallback::HandleLink::follow:
                        {
                            SftpItemDetails targetDetails = {};
                            if (!tryReportingItemError([&] //throw X
                        {
                            targetDetails = getSymlinkTargetDetails(login_, itemPath); //throw FileError
                            }, cb, item.itemName))
                            continue;

                            if (targetDetails.type == AFS::ItemType::folder)
                            {
                                if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb.onFolder({item.itemName, true /*isFollowedSymlink*/})) //throw X
                                    workload_.push_back(WorkItem{itemPath, std::move(cbSub)});
                            }
                            else //a file or named pipe, etc.
                                cb.onFile({item.itemName, targetDetails.fileSize, targetDetails.modTime, AFS::FingerPrint() /*not supported by SFTP*/, true /*isFollowedSymlink*/}); //throw X
                        }
                        break;

                        case AFS::TraverserCallback::HandleLink::skip:
                            break;
                    }
                    break;
            }
        }
    }

    const SftpLogin login_;
    RingBuffer<WorkItem> workload_;
};


void traverseFolderRecursiveSftp(const SftpLogin& login, const std::vector<std::pair<AfsPath, std::shared_ptr<AFS::TraverserCallback>>>& workload /*throw X*/, size_t) //throw X
{
    SingleFolderTraverser dummy(login, workload); //throw X
}

//===========================================================================================================================

struct InputStreamSftp : public AFS::InputStream
{
    InputStreamSftp(const SftpLogin& login, const AfsPath& filePath) : //throw FileError
        displayPath_(getSftpDisplayPath(login, filePath))
    {
        try
        {
            session_ = getSharedSftpSession(login); //throw SysError

            session_->executeBlocking("libssh2_sftp_open", //throw SysError, SysErrorSftpProtocol
                                      [&](const SshSession::Details& sd) //noexcept!
            {
                fileHandle_ = ::libssh2_sftp_open(sd.sftpChannel, getLibssh2Path(filePath), LIBSSH2_FXF_READ, 0);
                if (!fileHandle_)
                    return std::min(::libssh2_session_last_errno(sd.sshSession), LIBSSH2_ERROR_SOCKET_NONE);
                return LIBSSH2_ERROR_NONE;
            });
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(displayPath_)), e.toString()); }
    }

    ~InputStreamSftp()
    {
        try
        {
            session_->executeBlocking("libssh2_sftp_close", //throw SysError, SysErrorSftpProtocol
            [&](const SshSession::Details& sd) { return ::libssh2_sftp_close(fileHandle_); }); //noexcept!
        }
        catch (const SysError& e) { logExtraError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(displayPath_)) + L"\n\n" + e.toString()); }
    }

    size_t getBlockSize() override { return SFTP_OPTIMAL_BLOCK_SIZE_READ; } //throw (FileError); non-zero block size is AFS contract!

    //may return short; only 0 means EOF! CONTRACT: bytesToRead > 0!
    size_t tryRead(void* buffer, size_t bytesToRead, const IoCallback& notifyUnbufferedIO /*throw X*/) override //throw FileError, (ErrorFileLocked), X
    {
        //libssh2_sftp_read has same semantics as Posix read:
        if (bytesToRead == 0) //"read() with a count of 0 returns zero" => indistinguishable from end of file! => check!
            throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
        assert(bytesToRead % getBlockSize() == 0);

        ssize_t bytesRead = 0;
        try
        {
            session_->executeBlocking("libssh2_sftp_read", //throw SysError, SysErrorSftpProtocol
                                      [&](const SshSession::Details& sd) //noexcept!
            {
                bytesRead = ::libssh2_sftp_read(fileHandle_, static_cast<char*>(buffer), bytesToRead);
                return static_cast<int>(bytesRead);
            });

            ASSERT_SYSERROR(makeUnsigned(bytesRead) <= bytesToRead); //better safe than sorry (user should never see this)
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(displayPath_)), e.toString()); }

        if (notifyUnbufferedIO) notifyUnbufferedIO(bytesRead); //throw X
        return bytesRead; //"zero indicates end of file"
    }

    std::optional<AFS::StreamAttributes> tryGetAttributesFast() override { return {}; }//throw FileError
    //although we have an SFTP stream handle, attribute access requires an extra (expensive) round-trip!
    //PERF: test case 148 files, 1MB: overall copy time increases by 20% if libssh2_sftp_fstat() gets called per each file

private:
    const std::wstring displayPath_;
    LIBSSH2_SFTP_HANDLE* fileHandle_ = nullptr;
    std::shared_ptr<SftpSessionManager::SshSessionShared> session_;
};

//===========================================================================================================================

//libssh2_sftp_open fails with generic LIBSSH2_FX_FAILURE if already existing
struct OutputStreamSftp : public AFS::OutputStreamImpl
{
    OutputStreamSftp(const SftpLogin& login, //throw FileError
                     const AfsPath& filePath,
                     std::optional<time_t> modTime) :
        login_(login),
        filePath_(filePath),
        modTime_(modTime)
    {
        try
        {
            session_ = getSharedSftpSession(login); //throw SysError

            session_->executeBlocking("libssh2_sftp_open", //throw SysError, SysErrorSftpProtocol
                                      [&](const SshSession::Details& sd) //noexcept!
            {
                fileHandle_ = ::libssh2_sftp_open(sd.sftpChannel, getLibssh2Path(filePath),
                                                  LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_EXCL,
                                                  SFTP_DEFAULT_PERMISSION_FILE); //note: server may also apply umask! (e.g. 0022 for ffs.org)
                if (!fileHandle_)
                    return std::min(::libssh2_session_last_errno(sd.sshSession), LIBSSH2_ERROR_SOCKET_NONE);
                return LIBSSH2_ERROR_NONE;
            });
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getSftpDisplayPath(login_, filePath_))), e.toString()); }

        //NOTE: fileHandle_ still unowned until end of constructor!!!

        //pre-allocate file space? not supported
    }

    ~OutputStreamSftp()
    {
        if (fileHandle_) //=> cleanup non-finalized output file
        {
            if (!closeFailed_) //otherwise there's no much point in calling libssh2_sftp_close() a second time => let it leak!?
                try { close(); /*throw FileError*/ }
                catch (const FileError& e) { logExtraError(e.toString()); }

            session_.reset(); //reset before file deletion to potentially get new session if !SshSession::isHealthy()

            try //see removeFilePlain()
            {
                runSftpCommand(login_, "libssh2_sftp_unlink", //throw SysError, SysErrorSftpProtocol
                [&](const SshSession::Details& sd) { return ::libssh2_sftp_unlink(sd.sftpChannel, getLibssh2Path(filePath_)); }); //noexcept!
            }
            catch (const SysError& e)
            {
                logExtraError(replaceCpy(_("Cannot delete file %x."), L"%x", fmtPath(getSftpDisplayPath(login_, filePath_))) + L"\n\n" + e.toString());
            }
        }
    }

    size_t getBlockSize() override { return SFTP_OPTIMAL_BLOCK_SIZE_WRITE; } //throw (FileError)

    size_t tryWrite(const void* buffer, size_t bytesToWrite, const IoCallback& notifyUnbufferedIO /*throw X*/) override //throw FileError, X; may return short! CONTRACT: bytesToWrite > 0
    {
        if (bytesToWrite == 0)
            throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
        assert(bytesToWrite % getBlockSize() == 0 || bytesToWrite < getBlockSize());

        ssize_t bytesWritten = 0;
        try
        {
            session_->executeBlocking("libssh2_sftp_write", //throw SysError, SysErrorSftpProtocol
                                      [&](const SshSession::Details& sd) //noexcept!
            {
                bytesWritten = ::libssh2_sftp_write(fileHandle_, static_cast<const char*>(buffer), bytesToWrite);
                /*  "If this function returns zero it should not be considered an error, but simply that there was no error but yet no payload data got sent to the other end."
                     => sounds like BS, but is it really true!?
                    From the libssh2_sftp_write code it appears that the function always waits for at least one "ack", unless we give it so much data _libssh2_channel_write() can't sent it all! */
                assert(bytesWritten != 0);
                return static_cast<int>(bytesWritten);
            });

            ASSERT_SYSERROR(makeUnsigned(bytesWritten) <= bytesToWrite); //better safe than sorry
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getSftpDisplayPath(login_, filePath_))), e.toString()); }

        if (notifyUnbufferedIO) notifyUnbufferedIO(bytesWritten); //throw X!

        return bytesWritten;
    }

    AFS::FinalizeResult finalize(const IoCallback& notifyUnbufferedIO /*throw X*/) override //throw FileError, X
    {
        close(); //throw FileError
        //output finalized => no more exceptions from here on!
        //--------------------------------------------------------------------

        AFS::FinalizeResult result;
        //result.filePrint = ... -> not supported by SFTP
        try
        {
            setModTimeIfAvailable(); //throw FileError, follows symlinks
            /* is setting modtime after closing the file handle a pessimization?
                SFTP: no, needed for functional correctness (synology server), same as for Native */
        }
        catch (const FileError& e) { result.errorModTime = e; /*slicing?*/ }

        return result;
    }

private:
    void close() //throw FileError
    {
        if (!fileHandle_)
            throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

        try
        {
            session_->executeBlocking("libssh2_sftp_close", //throw SysError, SysErrorSftpProtocol
            [&](const SshSession::Details& sd) { return ::libssh2_sftp_close(fileHandle_); }); //noexcept!

            fileHandle_ = nullptr;
        }
        catch (const SysError& e)
        {
            closeFailed_ = true;
            throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getSftpDisplayPath(login_, filePath_))), e.toString());
        }
    }

    void setModTimeIfAvailable() const //throw FileError, follows symlinks
    {
        assert(!fileHandle_);
        if (modTime_)
        {
            LIBSSH2_SFTP_ATTRIBUTES attribNew = {};
            attribNew.flags = LIBSSH2_SFTP_ATTR_ACMODTIME;
            attribNew.mtime = static_cast<decltype(attribNew.mtime)>(*modTime_);        //32-bit target! loss of data!
            attribNew.atime = static_cast<decltype(attribNew.atime)>(::time(nullptr));  //

            //it seems libssh2_sftp_fsetstat() triggers bugs on synology server => set mtime by path! https://freefilesync.org/forum/viewtopic.php?t=1281
            try
            {
                session_->executeBlocking("libssh2_sftp_setstat", //throw SysError, SysErrorSftpProtocol
                [&](const SshSession::Details& sd) { return ::libssh2_sftp_setstat(sd.sftpChannel, getLibssh2Path(filePath_), &attribNew); }); //noexcept!
            }
            catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(getSftpDisplayPath(login_, filePath_))), e.toString()); }
        }
    }

    const SftpLogin login_;
    const AfsPath filePath_;
    const std::optional<time_t> modTime_;
    LIBSSH2_SFTP_HANDLE* fileHandle_ = nullptr;
    bool closeFailed_ = false;
    std::shared_ptr<SftpSessionManager::SshSessionShared> session_;
};

//===========================================================================================================================

class SftpFileSystem : public AbstractFileSystem
{
public:
    explicit SftpFileSystem(const SftpLogin& login) : login_(login) {}

    const SftpLogin& getLogin() const { return login_; }

    AfsPath getHomePath() const //throw FileError
    {
        try
        {
            //we never ever change the SFTP working directory, right? ...right?
            return getServerRealPath("."); //throw SysError
            //use "~" instead? NO: libssh2_sftp_realpath() fails with LIBSSH2_FX_NO_SUCH_FILE
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot determine final path for %x."), L"%x", fmtPath(getDisplayPath(AfsPath(Zstr("~"))))), e.toString()); }
    }

private:
    Zstring getInitPathPhrase(const AfsPath& itemPath) const override { return concatenateSftpFolderPathPhrase(login_, itemPath); }

    std::vector<Zstring> getPathPhraseAliases(const AfsPath& itemPath) const override
    {
        std::vector<Zstring> pathAliases;

        if (login_.authType != SftpAuthType::keyFile || login_.privateKeyFilePath.empty())
            pathAliases.push_back(concatenateSftpFolderPathPhrase(login_, itemPath));
        else //why going crazy with key path aliases!? because we can...
            for (const Zstring& pathPhrase : ::getPathPhraseAliases(login_.privateKeyFilePath))
            {
                auto loginTmp = login_;
                loginTmp.privateKeyFilePath = pathPhrase;

                pathAliases.push_back(concatenateSftpFolderPathPhrase(loginTmp, itemPath));
            }
        return pathAliases;
    }

    std::wstring getDisplayPath(const AfsPath& itemPath) const override { return getSftpDisplayPath(login_, itemPath); }

    bool isNullFileSystem() const override { return login_.server.empty(); }

    std::weak_ordering compareDeviceSameAfsType(const AbstractFileSystem& afsRhs) const override
    {
        const SftpLogin& lhs = login_;
        const SftpLogin& rhs = static_cast<const SftpFileSystem&>(afsRhs).login_;

        return SshDeviceId(lhs) <=> SshDeviceId(rhs);
    }

    //----------------------------------------------------------------------------------------------------------------
    ItemType getItemTypeImpl(const AfsPath& itemPath) const //throw SysError, SysErrorSftpProtocol
    {
        LIBSSH2_SFTP_ATTRIBUTES attr = {};
        runSftpCommand(login_, "libssh2_sftp_lstat", //throw SysError, SysErrorSftpProtocol
        [&](const SshSession::Details& sd) { return ::libssh2_sftp_lstat(sd.sftpChannel, getLibssh2Path(itemPath), &attr); }); //noexcept!

        if ((attr.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) == 0)
            throw SysError(formatSystemError("libssh2_sftp_lstat", L"", L"File attributes not available."));

        if (LIBSSH2_SFTP_S_ISLNK(attr.permissions))
            return ItemType::symlink;
        if (LIBSSH2_SFTP_S_ISDIR(attr.permissions))
            return ItemType::folder;
        return ItemType::file; //LIBSSH2_SFTP_S_ISREG || LIBSSH2_SFTP_S_ISCHR || LIBSSH2_SFTP_S_ISBLK || LIBSSH2_SFTP_S_ISFIFO || LIBSSH2_SFTP_S_ISSOCK
    }

    ItemType getItemType(const AfsPath& itemPath) const override //throw FileError
    {
        try
        {
            return getItemTypeImpl(itemPath); //throw SysError, SysErrorSftpProtocol
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getDisplayPath(itemPath))), e.toString());
        }
    }

    std::optional<ItemType> getItemTypeIfExists(const AfsPath& itemPath) const override //throw FileError
    {
        try
        {
            try
            {
                //fast check: 1. perf 2. expected by getFolderStatusNonBlocking() 3. traversing non-existing folder below MIGHT NOT FAIL (e.g. for SFTP on AWS)
                return getItemTypeImpl(itemPath); //throw SysError, SysErrorSftpProtocol
            }
            catch (const SysErrorSftpProtocol& e)
            {
                const std::optional<AfsPath> parentPath = getParentPath(itemPath);
                if (!parentPath) //device root => quick access test
                    throw;
                //let's dig deeper, but *only* for SysErrorSftpProtocol, not for general connection issues
                //+ check if SFTP error code sounds like "not existing"
                if (e.sftpErrorCode == LIBSSH2_FX_NO_SUCH_FILE ||
                    e.sftpErrorCode == LIBSSH2_FX_NO_SUCH_PATH) //-> not seen yet, but sounds reasonable
                {
                    if (const std::optional<ItemType> parentType = getItemTypeIfExists(*parentPath)) //throw FileError
                    {
                        if (*parentType == ItemType::file /*obscure, but possible*/)
                            throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(getItemName(*parentPath))));

                        const Zstring itemName = getItemName(itemPath);
                        assert(!itemName.empty());

                        traverseFolder(*parentPath, //throw FileError
                        [&](const    FileInfo& fi) { if (fi.itemName == itemName) throw SysError(_("Temporary access error:") + L' ' + e.toString()); },
                        [&](const  FolderInfo& fi) { if (fi.itemName == itemName) throw SysError(_("Temporary access error:") + L' ' + e.toString()); },
                        [&](const SymlinkInfo& si) { if (si.itemName == itemName) throw SysError(_("Temporary access error:") + L' ' + e.toString()); });
                        //- case-sensitive comparison! itemPath must be normalized!
                        //- finding the item after getItemType() previously failed is exceptional
                    }
                    return std::nullopt;
                }
                else
                    throw;
            }
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getDisplayPath(itemPath))), e.toString());
        }
    }

    //----------------------------------------------------------------------------------------------------------------
    //already existing: fail
    void createFolderPlain(const AfsPath& folderPath) const override //throw FileError
    {
        try
        {
            //fails with obscure LIBSSH2_FX_FAILURE if already existing
            runSftpCommand(login_, "libssh2_sftp_mkdir", //throw SysError, SysErrorSftpProtocol
                           [&](const SshSession::Details& sd) //noexcept!
            {
                return ::libssh2_sftp_mkdir(sd.sftpChannel, getLibssh2Path(folderPath), SFTP_DEFAULT_PERMISSION_FOLDER);
                //less explicit variant: return ::libssh2_sftp_mkdir(sd.sftpChannel, getLibssh2Path(folderPath), LIBSSH2_SFTP_DEFAULT_MODE);
            });
        }
        catch (const SysError& e) //libssh2_sftp_mkdir reports generic LIBSSH2_FX_FAILURE if existing
        {
            throw FileError(replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(getDisplayPath(folderPath))), e.toString());
        }
    }

    void removeFilePlain(const AfsPath& filePath) const override //throw FileError
    {
        try
        {
            runSftpCommand(login_, "libssh2_sftp_unlink", //throw SysError, SysErrorSftpProtocol
            [&](const SshSession::Details& sd) { return ::libssh2_sftp_unlink(sd.sftpChannel, getLibssh2Path(filePath)); }); //noexcept!
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
            runSftpCommand(login_, "libssh2_sftp_unlink", //throw SysError, SysErrorSftpProtocol
            [&](const SshSession::Details& sd) { return ::libssh2_sftp_unlink(sd.sftpChannel, getLibssh2Path(linkPath)); }); //noexcept!
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
            //libssh2_sftp_rmdir fails for symlinks! (LIBSSH2_ERROR_SFTP_PROTOCOL: LIBSSH2_FX_NO_SUCH_FILE)
            runSftpCommand(login_, "libssh2_sftp_rmdir", //throw SysError, SysErrorSftpProtocol
            [&](const SshSession::Details& sd) { return ::libssh2_sftp_rmdir(sd.sftpChannel, getLibssh2Path(folderPath)); }); //noexcept!
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
    AfsPath getServerRealPath(const std::string& sftpPath) const //throw SysError
    {
        const size_t bufSize = 10000;
        std::vector<char> buf(bufSize + 1); //ensure buffer is always null-terminated since we don't evaluate the byte count returned by libssh2_sftp_realpath()!

        int rc = 0;
        runSftpCommand(login_, "libssh2_sftp_realpath", //throw SysError, SysErrorSftpProtocol
        [&](const SshSession::Details& sd) { return rc = ::libssh2_sftp_realpath(sd.sftpChannel, sftpPath, buf.data(), bufSize); }); //noexcept!

        const std::string_view sftpPathTrg = makeStringView(buf.data(), rc);
        if (!startsWith(sftpPathTrg, '/'))
            throw SysError(replaceCpy<std::wstring>(L"Invalid path %x.", L"%x", fmtPath(utfTo<std::wstring>(sftpPathTrg))));

        return sanitizeDeviceRelativePath(utfTo<Zstring>(sftpPathTrg)); //code-reuse! but the sanitize part isn't really needed here...
    }

    AbstractPath getSymlinkResolvedPath(const AfsPath& linkPath) const override //throw FileError
    {
        try
        {
            const AfsPath linkPathTrg = getServerRealPath(getLibssh2Path(linkPath)); //throw SysError
            return AbstractPath(makeSharedRef<SftpFileSystem>(login_), linkPathTrg);
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot determine final path for %x."), L"%x", fmtPath(getDisplayPath(linkPath))), e.toString()); }
    }

    static std::string getSymlinkContentImpl(const SftpFileSystem& sftpFs, const AfsPath& linkPath) //throw SysError
    {
        std::string buf(10000, '\0');
        int rc = 0;

        runSftpCommand(sftpFs.login_, "libssh2_sftp_readlink", //throw SysError, SysErrorSftpProtocol
        [&](const SshSession::Details& sd) { return rc = ::libssh2_sftp_readlink(sd.sftpChannel, getLibssh2Path(linkPath), buf.data(), buf.size()); }); //noexcept!

        ASSERT_SYSERROR(makeUnsigned(rc) <= buf.size()); //better safe than sorry

        buf.resize(rc);
        return buf;
    }

    bool equalSymlinkContentForSameAfsType(const AfsPath& linkPathL, const AbstractPath& linkPathR) const override //throw FileError
    {
        auto getLinkContent = [](const SftpFileSystem& sftpFs, const AfsPath& linkPath)
        {
            try
            {
                return getSymlinkContentImpl(sftpFs, linkPath); //throw SysError
            }
            catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(sftpFs.getDisplayPath(linkPath))), e.toString()); }
        };
        return getLinkContent(*this, linkPathL) == getLinkContent(static_cast<const SftpFileSystem&>(linkPathR.afsDevice.ref()), linkPathR.afsPath); //throw FileError
    }
    //----------------------------------------------------------------------------------------------------------------

    //return value always bound:
    std::unique_ptr<InputStream> getInputStream(const AfsPath& filePath) const override //throw FileError, (ErrorFileLocked)
    {
        return std::make_unique<InputStreamSftp>(login_, filePath); //throw FileError
    }

    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    //=> actual behavior: fail with obscure LIBSSH2_FX_FAILURE error
    std::unique_ptr<OutputStreamImpl> getOutputStream(const AfsPath& filePath, //throw FileError
                                                      std::optional<uint64_t> streamSize,
                                                      std::optional<time_t> modTime) const override
    {
        return std::make_unique<OutputStreamSftp>(login_, filePath, modTime); //throw FileError
    }

    //----------------------------------------------------------------------------------------------------------------
    void traverseFolderRecursive(const TraverserWorkload& workload /*throw X*/, size_t parallelOps) const override
    {
        traverseFolderRecursiveSftp(login_, workload /*throw X*/, parallelOps); //throw X
    }
    //----------------------------------------------------------------------------------------------------------------

    //symlink handling: follow
    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    FileCopyResult copyFileForSameAfsType(const AfsPath& sourcePath, const StreamAttributes& attrSource, //throw FileError, (ErrorFileLocked), X
                                          const AbstractPath& targetPath, bool copyFilePermissions, const IoCallback& notifyUnbufferedIO /*throw X*/) const override
    {
        //no native SFTP file copy => use stream-based file copy:
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

    //already existing: fail (SSH_FX_FAILURE)
    void copySymlinkForSameAfsType(const AfsPath& sourcePath, const AbstractPath& targetPath, bool copyFilePermissions) const override //throw FileError
    {
        try
        {
            const std::string buf = getSymlinkContentImpl(*this, sourcePath); //throw SysError

            runSftpCommand(static_cast<const SftpFileSystem&>(targetPath.afsDevice.ref()).login_, "libssh2_sftp_symlink", //throw SysError, SysErrorSftpProtocol
                           [&](const SshSession::Details& sd) //noexcept!
            {
                return ::libssh2_sftp_symlink(sd.sftpChannel, getLibssh2Path(targetPath.afsPath), buf);
            });
        }
        catch (const SysError& e)
        {
            throw FileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."),
                                                  L"%x", L'\n' + fmtPath(getDisplayPath(sourcePath))),
                                       L"%y", L'\n' + fmtPath(AFS::getDisplayPath(targetPath))), e.toString());
        }
    }

    //already existing: undefined behavior! (e.g. fail/overwrite)
    //=> actual behavior: fail with obscure LIBSSH2_FX_FAILURE error
    void moveAndRenameItemForSameAfsType(const AfsPath& pathFrom, const AbstractPath& pathTo) const override //throw FileError, ErrorMoveUnsupported
    {
        if (compareDeviceSameAfsType(pathTo.afsDevice.ref()) != std::weak_ordering::equivalent)
            throw ErrorMoveUnsupported(generateMoveErrorMsg(pathFrom, pathTo), _("Operation not supported between different devices."));

        try
        {
            runSftpCommand(login_, "libssh2_sftp_rename", //throw SysError, SysErrorSftpProtocol
                           [&](const SshSession::Details& sd) //noexcept!
            {
                /* LIBSSH2_SFTP_RENAME_NATIVE:    "The server is free to do the rename operation in whatever way it chooses. Any other set flags are to be taken as hints to the server." No, thanks!
                   LIBSSH2_SFTP_RENAME_OVERWRITE: "No overwriting rename in [SFTP] v3/v4" https://www.greenend.org.uk/rjk/sftp/sftpversions.html

                   Test: LIBSSH2_SFTP_RENAME_OVERWRITE is not honored on freefilesync.org, no matter if LIBSSH2_SFTP_RENAME_NATIVE is set or not
                    => makes sense since SFTP v3 does not honor the additional flags that libssh2 sends!

                   "... the most widespread SFTP server implementation, the OpenSSH, will fail the SSH_FXP_RENAME request if the target file already exists"
                   => incidentally this is just the behavior we want!                              */
                const std::string sftpPathOld = getLibssh2Path(pathFrom);
                const std::string sftpPathNew = getLibssh2Path(pathTo.afsPath);

                return ::libssh2_sftp_rename(sd.sftpChannel, sftpPathOld, sftpPathNew, LIBSSH2_SFTP_RENAME_ATOMIC);
            });
        }
        catch (const SysError& e) //libssh2_sftp_rename_ex reports generic LIBSSH2_FX_FAILURE if target is already existing!
        {
            throw FileError(generateMoveErrorMsg(pathFrom, pathTo), e.toString());
        }
    }

    bool supportsPermissions(const AfsPath& folderPath) const override { return false; } //throw FileError
    //wait until there is real demand for copying from and to SFTP with permissions => use stream-based file copy:

    //----------------------------------------------------------------------------------------------------------------
    FileIconHolder getFileIcon      (const AfsPath& filePath, int pixelSize) const override { return {}; } //throw FileError; optional return value
    ImageHolder    getThumbnailImage(const AfsPath& filePath, int pixelSize) const override { return {}; } //throw FileError; optional return value

    void authenticateAccess(const RequestPasswordFun& requestPassword /*throw X*/) const override //throw FileError, X
    {
        try
        {
            const std::shared_ptr<SftpSessionManager> mgr = globalSftpSessionManager.get();
            if (!mgr)
                throw SysError(formatSystemError("getSessionPassword", L"", L"Function call not allowed during init/shutdown."));

            mgr->setActiveConfig(login_);

            if (login_.authType == SftpAuthType::password ||
                login_.authType == SftpAuthType::keyFile)
                if (!login_.password)
                {
                    try //1. test for connection error *before* bothering user to enter a password
                    {
                        /*auto session =*/ mgr->getSharedSession(login_); //throw SysError, SysErrorPassword
                        return; //got new SshSession (connected in constructor) or already connected session from cache
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
                        mgr->setSessionPassword(login_, password, login_.authType);

                        try //3. test access:
                        {
                            /*auto session =*/ mgr->getSharedSession(login_); //throw SysError, SysErrorPassword
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

    int64_t getFreeDiskSpace(const AfsPath& folderPath) const override //throw FileError, returns < 0 if not available
    {
        //statvfs is an SFTP v3 extension and not supported by all server implementations
        //Mikrotik SFTP server fails with LIBSSH2_FX_OP_UNSUPPORTED and corrupts session so that next SFTP call will hang
        //(Server sends a duplicate SSH_FX_OP_UNSUPPORTED response with seemingly corrupt body and fails to respond from now on)
        //https://freefilesync.org/forum/viewtopic.php?t=618
        //Just discarding the current session is not enough in all cases, e.g. 1. Open SFTP file handle 2. statvfs fails 3. must close file handle
        return -1;
#if 0
        const std::string sftpPath = "/"; //::libssh2_sftp_statvfs will fail if path is not yet existing, OTOH root path should work, too?
        //NO, for correctness we must check free space for the given folder!!

        //"It is unspecified whether all members of the returned struct have meaningful values on all file systems."
        LIBSSH2_SFTP_STATVFS fsStats = {};
        try
        {
            runSftpCommand(login_, "libssh2_sftp_statvfs", //throw SysError, SysErrorSftpProtocol
            [&](const SshSession::Details& sd) { return ::libssh2_sftp_statvfs(sd.sftpChannel, sftpPath.c_str(), sftpPath.size(), &fsStats); }); //noexcept!
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot determine free disk space for %x."), L"%x", fmtPath(getDisplayPath(L"/"))), e.toString()); }

        static_assert(sizeof(fsStats.f_bsize) >= 8);
        return fsStats.f_bsize * fsStats.f_bavail;
#endif
    }

    std::unique_ptr<RecycleSession> createRecyclerSession(const AfsPath& folderPath) const override //throw FileError, RecycleBinUnavailable
    {
        throw RecycleBinUnavailable(replaceCpy(_("The recycle bin is not available for %x."), L"%x", fmtPath(getDisplayPath(folderPath))));
    }

    void moveToRecycleBin(const AfsPath& itemPath) const override //throw FileError, RecycleBinUnavailable
    {
        throw RecycleBinUnavailable(replaceCpy(_("The recycle bin is not available for %x."), L"%x", fmtPath(getDisplayPath(itemPath))));
    }

    const SftpLogin login_;
};

//===========================================================================================================================

//expects "clean" login data
Zstring concatenateSftpFolderPathPhrase(const SftpLogin& login, const AfsPath& folderPath) //noexcept
{
    Zstring username;
    if (!login.username.empty())
        username = encodeFtpUsername(login.username) + Zstr("@");

    Zstring server = login.server;
    if (parseIpv6Address(server) && login.portCfg > 0)
        server = Zstr('[') + server + Zstr(']'); //e.g. [::1]:80

    Zstring port;
    if (login.portCfg > 0)
        port = Zstr(':') + numberTo<Zstring>(login.portCfg);

    Zstring relPath = getServerRelPath(folderPath);
    if (relPath == Zstr("/"))
        relPath.clear();

    const SftpLogin loginDefault;

    Zstring options;
    if (login.timeoutSec != loginDefault.timeoutSec)
        options += Zstr("|timeout=") + numberTo<Zstring>(login.timeoutSec);

    if (login.traverserChannelsPerConnection != loginDefault.traverserChannelsPerConnection)
        options += Zstr("|chan=") + numberTo<Zstring>(login.traverserChannelsPerConnection);

    if (login.allowZlib)
        options += Zstr("|zlib");

    switch (login.authType)
    {
        case SftpAuthType::password:
            break;

        case SftpAuthType::keyFile:
            options += Zstr("|keyfile=") + login.privateKeyFilePath;
            break;

        case SftpAuthType::agent:
            options += Zstr("|agent");
            break;
    }

    if (login.authType != SftpAuthType::agent)
    {
        if (login.password)
        {
            if (!login.password->empty()) //password always last => visually truncated by folder input field
                options += Zstr("|pass64=") + encodePasswordBase64(*login.password);
        }
        else
            options += Zstr("|pwprompt");
    }

    return Zstring(sftpPrefix) + Zstr("//") + username + server + port + relPath + options;
}
}


void fff::sftpInit()
{
    assert(!globalSftpSessionManager.get());
    globalSftpSessionManager.set(std::make_unique<SftpSessionManager>());
}


void fff::sftpTeardown()
{
    assert(globalSftpSessionManager.get());
    globalSftpSessionManager.set(nullptr);
}


AfsPath fff::getSftpHomePath(const SftpLogin& login) //throw FileError
{
    return SftpFileSystem(login).getHomePath(); //throw FileError
}


AfsDevice fff::condenseToSftpDevice(const SftpLogin& login) //noexcept
{
    //clean up input:
    SftpLogin loginTmp = login;
    trim(loginTmp.server);
    trim(loginTmp.username);
    trim(loginTmp.privateKeyFilePath);

    loginTmp.timeoutSec = std::max(1, loginTmp.timeoutSec);
    loginTmp.traverserChannelsPerConnection = std::max(1, loginTmp.traverserChannelsPerConnection);

    if (startsWithAsciiNoCase(loginTmp.server, "http:" ) ||
        startsWithAsciiNoCase(loginTmp.server, "https:") ||
        startsWithAsciiNoCase(loginTmp.server, "ftp:"  ) ||
        startsWithAsciiNoCase(loginTmp.server, "ftps:" ) ||
        startsWithAsciiNoCase(loginTmp.server, "sftp:" ))
        loginTmp.server = afterFirst(loginTmp.server, Zstr(':'), IfNotFoundReturn::none);
    trim(loginTmp.server, TrimSide::both, [](Zchar c) { return c == Zstr('/') || c == Zstr('\\'); });

    if (std::optional<std::pair<Zstring, int>> ip6AndPort = parseIpv6Address(loginTmp.server))
        loginTmp.server = ip6AndPort->first; //remove IPv6 leading/trailing brackets

    return makeSharedRef<SftpFileSystem>(loginTmp);
}


SftpLogin fff::extractSftpLogin(const AfsDevice& afsDevice) //noexcept
{
    if (const auto sftpDevice = dynamic_cast<const SftpFileSystem*>(&afsDevice.ref()))
        return sftpDevice->getLogin();

    assert(false);
    return {};
}


int fff::getServerMaxChannelsPerConnection(const SftpLogin& login) //throw FileError
{
    try
    {
        const auto timeoutTime = std::chrono::steady_clock::now() + SFTP_CHANNEL_LIMIT_DETECTION_TIME_OUT;

        std::unique_ptr<SftpSessionManager::SshSessionExclusive> exSession = getExclusiveSftpSession(login); //throw SysError

        ZEN_ON_SCOPE_EXIT(exSession->markAsCorrupted()); //after hitting the server limits, the session might have gone bananas (e.g. server fails on all requests)

        for (;;)
        {
            try
            {
                SftpSessionManager::SshSessionExclusive::addSftpChannel({exSession.get()}); //throw SysError
            }
            catch (SysError&) { if (exSession->getSftpChannelCount() == 0) throw; return static_cast<int>(exSession->getSftpChannelCount()); }

            if (std::chrono::steady_clock::now() > timeoutTime)
                throw SysError(_P("Operation timed out after 1 second.", "Operation timed out after %x seconds.",
                                  std::chrono::seconds(SFTP_CHANNEL_LIMIT_DETECTION_TIME_OUT).count()) + L' ' +
                               replaceCpy(_("Failed to open SFTP channel number %x."), L"%x", formatNumber(exSession->getSftpChannelCount() + 1)));
        }
    }
    catch (const SysError& e)
    {
        throw FileError(replaceCpy(_("Unable to connect to %x."), L"%x", fmtPath(login.server)), e.toString());
    }
}


bool fff::acceptsItemPathPhraseSftp(const Zstring& itemPathPhrase) //noexcept
{
    Zstring path = expandMacros(itemPathPhrase); //expand before trimming!
    trim(path);
    return startsWithAsciiNoCase(path, sftpPrefix); //check for explicit SFTP path
}


/* syntax: sftp://[<user>[:<password>]@]<server>[:port]/<relative-path>[|option_name=value]

   e.g. sftp://user001:secretpassword@private.example.com:222/mydirectory/
        sftp://user001:secretpassword@[::1]:80/ipv6folder/
        sftp://user001:secretpassword@::1/ipv6withoutPort/
        sftp://user001@private.example.com/mydirectory|con=2|cpc=10|keyfile=%AppData%\id_rsa|pass64=c2VjcmV0cGFzc3dvcmQ          */
AbstractPath fff::createItemPathSftp(const Zstring& itemPathPhrase) //noexcept
{
    Zstring pathPhrase = expandMacros(itemPathPhrase); //expand before trimming!
    trim(pathPhrase);

    if (startsWithAsciiNoCase(pathPhrase, sftpPrefix))
        pathPhrase = pathPhrase.c_str() + strLength(sftpPrefix);
    trim(pathPhrase, TrimSide::left, [](Zchar c) { return c == Zstr('/') || c == Zstr('\\'); });

    const ZstringView credentials = beforeFirst<ZstringView>(pathPhrase, Zstr('@'), IfNotFoundReturn::none);
    const ZstringView fullPathOpt =  afterFirst<ZstringView>(pathPhrase, Zstr('@'), IfNotFoundReturn::all);

    SftpLogin login;
    login.username = decodeFtpUsername(Zstring(beforeFirst(credentials, Zstr(':'), IfNotFoundReturn::all))); //support standard FTP syntax, even though
    login.password =                   Zstring( afterFirst(credentials, Zstr(':'), IfNotFoundReturn::none)); //concatenateSftpFolderPathPhrase() uses "pass64" instead

    const ZstringView fullPath = beforeFirst(fullPathOpt, Zstr('|'), IfNotFoundReturn::all);
    const ZstringView options  =  afterFirst(fullPathOpt, Zstr('|'), IfNotFoundReturn::none);

    auto it = std::find_if(fullPath.begin(), fullPath.end(), [](Zchar c) { return c == '/' || c == '\\'; });
    const ZstringView serverPort = makeStringView(fullPath.begin(), it);
    const AfsPath serverRelPath = sanitizeDeviceRelativePath({it, fullPath.end()});

    if (std::optional<std::pair<Zstring, int /*optional: port*/>> ip6AndPort = parseIpv6Address(serverPort)) //e.g. 2001:db8::ff00:42:8329 or [::1]:80
    {
        login.server  = ip6AndPort->first;
        login.portCfg = ip6AndPort->second; //0 if empty
    }
    else
    {
        login.server           = Zstring(beforeLast(serverPort, Zstr(':'), IfNotFoundReturn::all));
        const ZstringView port =          afterLast(serverPort, Zstr(':'), IfNotFoundReturn::none);
        login.portCfg = stringTo<int>(port); //0 if empty
    }

    assert(login.allowZlib == false);

    split(options, Zstr('|'), [&](ZstringView optPhrase)
    {
        optPhrase = trimCpy(optPhrase);
        if (!optPhrase.empty())
        {
            if (startsWith(optPhrase, Zstr("timeout=")))
                login.timeoutSec = stringTo<int>(afterFirst(optPhrase, Zstr('='), IfNotFoundReturn::none));
            else if (startsWith(optPhrase, Zstr("chan=")))
                login.traverserChannelsPerConnection = stringTo<int>(afterFirst(optPhrase, Zstr('='), IfNotFoundReturn::none));
            else if (startsWith(optPhrase, Zstr("keyfile=")))
            {
                login.authType = SftpAuthType::keyFile;
                login.privateKeyFilePath = getResolvedFilePath(Zstring(afterFirst(optPhrase, Zstr('='), IfNotFoundReturn::none)));
            }
            else if (optPhrase == Zstr("agent"))
                login.authType = SftpAuthType::agent;
            else if (startsWith(optPhrase, Zstr("pass64=")))
                login.password = decodePasswordBase64(afterFirst(optPhrase, Zstr('='), IfNotFoundReturn::none));
            else if (optPhrase == Zstr("pwprompt"))
                login.password = std::nullopt;
            else if (optPhrase == Zstr("zlib"))
                login.allowZlib = true;
            else
                assert(false);
        }
    });
    return AbstractPath(makeSharedRef<SftpFileSystem>(login), serverRelPath);
}

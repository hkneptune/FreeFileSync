// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef LIBSSH2_WRAP_H_087280957180967346572465
#define LIBSSH2_WRAP_H_087280957180967346572465

#include <zen/scope_guard.h>
#include <zen/string_tools.h>



//-------------------------------------------------
#include <libssh2_sftp.h>
//-------------------------------------------------

#ifndef LIBSSH2_SFTP_H
    #error libssh2_sftp.h header guard changed
#endif

//fix libssh2 64-bit warning mess: https://github.com/libssh2/libssh2/pull/96
#undef libssh2_userauth_password
inline int libssh2_userauth_password(LIBSSH2_SESSION* session, const std::string& username, const std::string& password)
{
    return libssh2_userauth_password_ex(session,
                                        username.c_str(), static_cast<unsigned int>(username.size()),
                                        password.c_str(), static_cast<unsigned int>(password.size()), nullptr);
}

#undef libssh2_userauth_keyboard_interactive
inline int libssh2_userauth_keyboard_interactive(LIBSSH2_SESSION* session, const std::string& username, LIBSSH2_USERAUTH_KBDINT_RESPONSE_FUNC((*response_callback)))
{
    return libssh2_userauth_keyboard_interactive_ex(session, username.c_str(), static_cast<unsigned int>(username.size()), response_callback);
}

inline char* libssh2_userauth_list(LIBSSH2_SESSION* session, const std::string& username)
{
    return libssh2_userauth_list(session, username.c_str(), static_cast<unsigned int>(username.size()));
}


inline int libssh2_userauth_publickey_frommemory(LIBSSH2_SESSION* session, const std::string& username, const std::string& privateKeyStream, const std::string& passphrase)
{
    return libssh2_userauth_publickey_frommemory(session, username.c_str(), username.size(), nullptr, 0,
                                                 privateKeyStream.c_str(), privateKeyStream.size(), passphrase.c_str());
}

#undef libssh2_sftp_opendir
inline LIBSSH2_SFTP_HANDLE* libssh2_sftp_opendir(LIBSSH2_SFTP* sftp, const std::string& path)
{
    return libssh2_sftp_open_ex(sftp, path.c_str(), static_cast<unsigned int>(path.size()), 0, 0, LIBSSH2_SFTP_OPENDIR);
}

#undef libssh2_sftp_stat
inline int libssh2_sftp_stat(LIBSSH2_SFTP* sftp, const std::string& path, LIBSSH2_SFTP_ATTRIBUTES* attrs)
{
    return libssh2_sftp_stat_ex(sftp, path.c_str(), static_cast<unsigned int>(path.size()), LIBSSH2_SFTP_STAT, attrs);
}

#undef libssh2_sftp_open
inline LIBSSH2_SFTP_HANDLE* libssh2_sftp_open(LIBSSH2_SFTP* sftp, const std::string& path, unsigned long flags, long mode)
{
    return libssh2_sftp_open_ex(sftp, path.c_str(), static_cast<unsigned int>(path.size()), flags, mode, LIBSSH2_SFTP_OPENFILE);
}

#undef libssh2_sftp_setstat
inline int libssh2_sftp_setstat(LIBSSH2_SFTP* sftp, const std::string& path, LIBSSH2_SFTP_ATTRIBUTES* attrs)
{
    return libssh2_sftp_stat_ex(sftp, path.c_str(), static_cast<unsigned int>(path.size()), LIBSSH2_SFTP_SETSTAT, attrs);
}

#undef libssh2_sftp_lstat
inline int libssh2_sftp_lstat(LIBSSH2_SFTP* sftp, const std::string& path, LIBSSH2_SFTP_ATTRIBUTES* attrs)
{
    return libssh2_sftp_stat_ex(sftp, path.c_str(), static_cast<unsigned int>(path.size()), LIBSSH2_SFTP_LSTAT, attrs);
}

#undef libssh2_sftp_mkdir
inline int libssh2_sftp_mkdir(LIBSSH2_SFTP* sftp, const std::string& path, long mode)
{
    return libssh2_sftp_mkdir_ex(sftp, path.c_str(), static_cast<unsigned int>(path.size()), mode);
}

#undef libssh2_sftp_unlink
inline int libssh2_sftp_unlink(LIBSSH2_SFTP* sftp, const std::string& path)
{
    return libssh2_sftp_unlink_ex(sftp, path.c_str(), static_cast<unsigned int>(path.size()));
}

#undef libssh2_sftp_rmdir
inline int libssh2_sftp_rmdir(LIBSSH2_SFTP* sftp, const std::string& path)
{
    return libssh2_sftp_rmdir_ex(sftp, path.c_str(), static_cast<unsigned int>(path.size()));
}

#undef libssh2_sftp_realpath
inline int libssh2_sftp_realpath(LIBSSH2_SFTP* sftp, const std::string& path, char* buf, size_t bufSize)
{
    return libssh2_sftp_symlink_ex(sftp, path.c_str(), static_cast<unsigned int>(path.size()), buf, static_cast<unsigned int>(bufSize), LIBSSH2_SFTP_REALPATH);
}

#undef libssh2_sftp_readlink
inline int libssh2_sftp_readlink(LIBSSH2_SFTP* sftp, const std::string& path, char* buf, size_t bufSize)
{
    return libssh2_sftp_symlink_ex(sftp, path.c_str(), static_cast<unsigned int>(path.size()), buf, static_cast<unsigned int>(bufSize), LIBSSH2_SFTP_READLINK);
}

#undef libssh2_sftp_rename
inline int libssh2_sftp_rename(LIBSSH2_SFTP* sftp, const std::string& pathFrom, const std::string& pathTo, long flags)
{
    return libssh2_sftp_rename_ex(sftp,
                                  pathFrom.c_str(), static_cast<unsigned int>(pathFrom.size()),
                                  pathTo.c_str(), static_cast<unsigned int>(pathTo.size()), flags);
}



namespace zen
{
namespace
{
std::wstring formatSshStatusCode(int sc)
{
    switch (sc)
    {
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_NONE);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_SOCKET_NONE);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_BANNER_RECV);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_BANNER_SEND);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_INVALID_MAC);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_KEX_FAILURE);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_ALLOC);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_SOCKET_SEND);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_KEY_EXCHANGE_FAILURE);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_TIMEOUT);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_HOSTKEY_INIT);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_HOSTKEY_SIGN);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_DECRYPT);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_SOCKET_DISCONNECT);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_PROTO);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_PASSWORD_EXPIRED);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_FILE);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_METHOD_NONE);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_AUTHENTICATION_FAILED);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_CHANNEL_OUTOFORDER);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_CHANNEL_FAILURE);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_CHANNEL_UNKNOWN);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_CHANNEL_WINDOW_EXCEEDED);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_CHANNEL_PACKET_EXCEEDED);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_CHANNEL_CLOSED);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_CHANNEL_EOF_SENT);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_SCP_PROTOCOL);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_ZLIB);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_SOCKET_TIMEOUT);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_SFTP_PROTOCOL);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_REQUEST_DENIED);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_METHOD_NOT_SUPPORTED);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_INVAL);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_INVALID_POLL_TYPE);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_PUBLICKEY_PROTOCOL);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_EAGAIN);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_BUFFER_TOO_SMALL);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_BAD_USE);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_COMPRESS);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_OUT_OF_BOUNDARY);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_AGENT_PROTOCOL);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_SOCKET_RECV);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_ENCRYPT);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_BAD_SOCKET);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_KNOWN_HOSTS);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_CHANNEL_WINDOW_FULL);
            ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_ERROR_KEYFILE_AUTH_FAILED);

        default:
            return replaceCpy<std::wstring>(L"SSH status %x", L"%x", numberTo<std::wstring>(sc));
    }
}


std::wstring formatSftpStatusCode(unsigned long sc)
{
    switch (sc)
    {
		//*INDENT-OFF*
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_OK);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_EOF);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_NO_SUCH_FILE);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_PERMISSION_DENIED);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_FAILURE);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_BAD_MESSAGE);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_NO_CONNECTION);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_CONNECTION_LOST);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_OP_UNSUPPORTED);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_INVALID_HANDLE);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_NO_SUCH_PATH);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_FILE_ALREADY_EXISTS);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_WRITE_PROTECT);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_NO_MEDIA);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_QUOTA_EXCEEDED);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_UNKNOWN_PRINCIPAL);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_LOCK_CONFLICT);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_DIR_NOT_EMPTY);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_NOT_A_DIRECTORY);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_INVALID_FILENAME);
        ZEN_CHECK_CASE_FOR_CONSTANT(LIBSSH2_FX_LINK_LOOP);

        //SFTP error codes missing from libssh2: https://tools.ietf.org/html/draft-ietf-secsh-filexfer-13#section-9.1
        case 22: return L"SSH_FX_CANNOT_DELETE";
        case 23: return L"SSH_FX_INVALID_PARAMETER";
        case 24: return L"SSH_FX_FILE_IS_A_DIRECTORY";
        case 25: return L"SSH_FX_BYTE_RANGE_LOCK_CONFLICT";
        case 26: return L"SSH_FX_BYTE_RANGE_LOCK_REFUSED";
        case 27: return L"SSH_FX_DELETE_PENDING";
        case 28: return L"SSH_FX_FILE_CORRUPT";
        case 29: return L"SSH_FX_OWNER_INVALID";
        case 30: return L"SSH_FX_GROUP_INVALID";
        case 31: return L"SSH_FX_NO_MATCHING_BYTE_RANGE_LOCK";

		default: return replaceCpy<std::wstring>(L"SFTP status %x", L"%x", numberTo<std::wstring>(sc));
		//*INDENT-ON*
    }
}
}
}

#else
#error Why is this header already defined? Do not include in other headers: encapsulate the gory details!
#endif //LIBSSH2_WRAP_H_087280957180967346572465

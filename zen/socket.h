// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SOCKET_H_23498325972583947678456437
#define SOCKET_H_23498325972583947678456437

#include <zen/zstring.h>
#include "sys_error.h"
    #include <unistd.h> //close
    #include <sys/socket.h>
    #include <netdb.h> //getaddrinfo


namespace zen
{
#define THROW_LAST_SYS_ERROR_WSA(functionName)                       \
    do { const ErrorCode ecInternal = getLastError(); throw SysError(formatSystemError(functionName, ecInternal)); } while (false)


//Winsock needs to be initialized before calling any of these functions! (WSAStartup/WSACleanup)

class Socket //throw SysError
{
public:
    Socket(const Zstring& server, const Zstring& serviceName) //throw SysError
    {
        ::addrinfo hints = {};
        hints.ai_socktype = SOCK_STREAM; //we *do* care about this one!
        hints.ai_flags = AI_ADDRCONFIG; //save a AAAA lookup on machines that can't use the returned data anyhow

        ::addrinfo* servinfo = nullptr;
        ZEN_ON_SCOPE_EXIT(if (servinfo) ::freeaddrinfo(servinfo));

        const int rcGai = ::getaddrinfo(server.c_str(), serviceName.c_str(), &hints, &servinfo);
        if (rcGai != 0)
            throw SysError(formatSystemError(L"getaddrinfo", replaceCpy(_("Error Code %x"), L"%x", numberTo<std::wstring>(rcGai)), utfTo<std::wstring>(::gai_strerror(rcGai))));
        if (!servinfo)
            throw SysError(L"getaddrinfo: empty server info");

        auto getConnectedSocket = [&](const auto& /*::addrinfo*/ ai)
        {
            SocketType testSocket = ::socket(ai.ai_family, ai.ai_socktype, ai.ai_protocol);
            if (testSocket == invalidSocket)
                THROW_LAST_SYS_ERROR_WSA(L"socket");
            ZEN_ON_SCOPE_FAIL(closeSocket(testSocket));

            if (::connect(testSocket, ai.ai_addr, static_cast<int>(ai.ai_addrlen)) != 0)
                THROW_LAST_SYS_ERROR_WSA(L"connect");

            return testSocket;
        };

        std::optional<SysError> firstError;
        for (const auto* /*::addrinfo*/ si = servinfo; si; si = si->ai_next)
            try
            {
                socket_ = getConnectedSocket(*si); //throw SysError; pass ownership
                return;
            }
            catch (const SysError& e) { if (!firstError) firstError = e; }

        throw* firstError; //list was not empty, so there must have been an error!
    }

    ~Socket() { closeSocket(socket_); }

    using SocketType = int;
    SocketType get() const { return socket_; }

private:
    Socket           (const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    static const SocketType invalidSocket = -1;
    static void closeSocket(SocketType s) { ::close(s); }

    SocketType socket_ = invalidSocket;
};
}

#endif //SOCKET_H_23498325972583947678456437

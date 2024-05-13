// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SOCKET_H_23498325972583947678456437
#define SOCKET_H_23498325972583947678456437

#include "sys_error.h"
    #include <unistd.h> //close
    #include <sys/socket.h>
    #include <netinet/tcp.h> //TCP_NODELAY
    #include <netdb.h> //getaddrinfo


namespace zen
{
#define THROW_LAST_SYS_ERROR_WSA(functionName)                       \
    do { const ErrorCode ecInternal = getLastError(); throw SysError(formatSystemError(functionName, ecInternal)); } while (false)


#define THROW_LAST_SYS_ERROR_GAI(rcGai)                        \
    do {                                                       \
        if (rcGai == EAI_SYSTEM) /*"check errno for details"*/ \
            THROW_LAST_SYS_ERROR("getaddrinfo");               \
        \
        throw SysError(formatSystemError("getaddrinfo", formatGaiErrorCode(rcGai), utfTo<std::wstring>(::gai_strerror(rcGai)))); \
    } while (false)

inline
std::wstring formatGaiErrorCode(int ec)
{
    switch (ec) //codes used on both Linux and macOS
    {
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_ADDRFAMILY);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_AGAIN);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_BADFLAGS);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_FAIL);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_FAMILY);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_MEMORY);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_NODATA);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_NONAME);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_SERVICE);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_SOCKTYPE);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_SYSTEM);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_OVERFLOW);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_INPROGRESS);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_CANCELED);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_NOTCANCELED);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_ALLDONE);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_INTR);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAI_IDN_ENCODE);
        default:
            return replaceCpy(_("Error code %x"), L"%x", numberTo<std::wstring>(ec));
    }
}

//patch up socket portability:
using SocketType = int;
const SocketType invalidSocket = -1;
inline void closeSocket(SocketType s) { ::close(s); }

void setNonBlocking(SocketType socket, bool value); //throw SysError


//Winsock needs to be initialized before calling any of these functions! (WSAStartup/WSACleanup)



class Socket //throw SysError
{
public:
    Socket(const Zstring& server, const Zstring& serviceName, int timeoutSec) //throw SysError
    {
        //GetAddrInfo(): "If the pNodeName parameter contains an empty string, all registered addresses on the local computer are returned."
        //               "If the pNodeName parameter points to a string equal to "localhost", all loopback addresses on the local computer are returned."
        if (trimCpy(server).empty())
            throw SysError(_("Server name must not be empty."));

        const addrinfo hints
        {
            .ai_flags = AI_ADDRCONFIG, //save a AAAA lookup on machines that can't use the returned data anyhow
            .ai_socktype = SOCK_STREAM, //we *do* care about this one!
        };

        addrinfo* servinfo = nullptr;
        ZEN_ON_SCOPE_EXIT(if (servinfo) ::freeaddrinfo(servinfo));

        const int rcGai = ::getaddrinfo(server.c_str(), serviceName.c_str(), &hints, &servinfo);
        if (rcGai != 0)
            THROW_LAST_SYS_ERROR_GAI(rcGai);
        if (!servinfo)
            throw SysError(formatSystemError("getaddrinfo", L"", L"Empty server info."));

        const auto getConnectedSocket = [timeoutSec](const auto& /*addrinfo*/ ai)
        {
            SocketType testSocket = ::socket(ai.ai_family,    //int socket_family
                                             SOCK_CLOEXEC | SOCK_NONBLOCK |
                                             ai.ai_socktype,  //int socket_type
                                             ai.ai_protocol); //int protocol
            if (testSocket == invalidSocket)
                THROW_LAST_SYS_ERROR_WSA("socket");
            ZEN_ON_SCOPE_FAIL(closeSocket(testSocket));

            if (::connect(testSocket, ai.ai_addr, static_cast<int>(ai.ai_addrlen)) != 0) //0 or SOCKET_ERROR(-1)
            {
                if (errno != EINPROGRESS)
                    THROW_LAST_SYS_ERROR_WSA("connect");

                fd_set writefds{};
                fd_set exceptfds{}; //mostly only relevant for connect()
                FD_SET(testSocket, &writefds);
                FD_SET(testSocket, &exceptfds);

                /*const*/ timeval tv{.tv_sec = timeoutSec};

                const int rv = ::select(
                                   testSocket + 1, //int nfds = "highest-numbered file descriptor in any of the three sets, plus 1"
                                   nullptr,       //fd_set* readfds
                                   &writefds,     //fd_set* writefds
                                   &exceptfds,    //fd_set* exceptfds
                                   &tv);          //const timeval* timeout
                if (rv < 0)
                    THROW_LAST_SYS_ERROR_WSA("select");

                if (rv == 0) //time-out!
                    throw SysError(formatSystemError("select, " + utfTo<std::string>(_P("1 sec", "%x sec", timeoutSec)), ETIMEDOUT));
                int error = 0;
                socklen_t optLen = sizeof(error);
                if (::getsockopt(testSocket, //[in]      SOCKET s
                                 SOL_SOCKET, //[in]      int    level
                                 SO_ERROR,   //[in]      int    optname
                                 reinterpret_cast<char*>(&error), //[out]     char*   optval
                                 &optLen)    //[in, out] socklen_t* optlen
                    != 0)
                    THROW_LAST_SYS_ERROR_WSA("getsockopt(SO_ERROR)");

                if (error != 0)
                    throw SysError(formatSystemError("connect, SO_ERROR", static_cast<ErrorCode>(error))/*== system error code, apparently!?*/);
            }

            setNonBlocking(testSocket, false); //throw SysError
            //-----------------------------------------------------------

            int noDelay =  1; //disable Nagle algorithm: https://brooker.co.za/blog/2024/05/09/nagle.html
            //e.g. test case "website sync": 23% shorter comparison time!
            if (::setsockopt(testSocket,                        //_In_       SOCKET s
                             IPPROTO_TCP,                       //_In_       int    level
                             TCP_NODELAY,                       //_In_       int    optname
                             reinterpret_cast<char*>(&noDelay), //_In_ const char*  optval
                             sizeof(noDelay)) != 0)             //_In_       int    optlen
                THROW_LAST_SYS_ERROR_WSA("setsockopt(TCP_NODELAY)");

            return testSocket;
        };

        /* getAddrInfo() often returns only one ai_family == AF_INET address, but more items are possible:
            facebook.com:  1 x AF_INET6, 3 x AF_INET
            microsoft.com: 5 x AF_INET            => server not allowing connection: hanging for 5x timeoutSec :(       */
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

    SocketType get() const { return socket_; }

private:
    Socket           (const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    SocketType socket_ = invalidSocket;
};


//more socket helper functions:
namespace
{
size_t tryReadSocket(SocketType socket, void* buffer, size_t bytesToRead) //throw SysError; may return short, only 0 means EOF!
{
    if (bytesToRead == 0) //"read() with a count of 0 returns zero" => indistinguishable from end of file! => check!
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

    int bytesReceived = 0;
    for (;;)
    {
        bytesReceived = ::recv(socket,                        //_In_  SOCKET s
                               static_cast<char*>(buffer),    //_Out_ char*  buf
                               static_cast<int>(bytesToRead), //_In_  int    len
                               0);                            //_In_  int    flags
        if (bytesReceived >= 0 || errno != EINTR)
            break;
    }
    if (bytesReceived < 0)
        THROW_LAST_SYS_ERROR_WSA("recv");

    ASSERT_SYSERROR(makeUnsigned(bytesReceived) <= bytesToRead); //better safe than sorry

    return bytesReceived; //"zero indicates end of file"
}


size_t tryWriteSocket(SocketType socket, const void* buffer, size_t bytesToWrite) //throw SysError; may return short! CONTRACT: bytesToWrite > 0
{
    if (bytesToWrite == 0)
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

    int bytesWritten = 0;
    for (;;)
    {
        bytesWritten = ::send(socket,                           //_In_       SOCKET s
                              static_cast<const char*>(buffer), //_In_ const char*  buf
                              static_cast<int>(bytesToWrite),   //_In_       int    len
                              0);                               //_In_       int    flags
        if (bytesWritten >= 0 || errno != EINTR)
            break;
    }
    if (bytesWritten < 0)
        THROW_LAST_SYS_ERROR_WSA("send");

    if (bytesWritten == 0)
        throw SysError(formatSystemError("send", L"", L"Zero bytes processed."));

    ASSERT_SYSERROR(makeUnsigned(bytesWritten) <= bytesToWrite); //better safe than sorry

    return bytesWritten;
}
}


//initiate termination of connection by sending TCP FIN package
inline
void shutdownSocketSend(SocketType socket) //throw SysError
{
    if (::shutdown(socket, SHUT_WR) != 0)
        THROW_LAST_SYS_ERROR_WSA("shutdown");
}


inline
void setNonBlocking(SocketType socket, bool nonBlocking) //throw SysError
{
    int flags = ::fcntl(socket, F_GETFL);
    if (flags == -1)
        THROW_LAST_SYS_ERROR("fcntl(F_GETFL)");

    if (nonBlocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;

    if (::fcntl(socket, F_SETFL, flags) != 0)
        THROW_LAST_SYS_ERROR(nonBlocking ? "fcntl(F_SETFL, O_NONBLOCK)" : "fcntl(F_SETFL, ~O_NONBLOCK)");
}
}

#endif //SOCKET_H_23498325972583947678456437

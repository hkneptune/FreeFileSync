// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef REST_H_07018456781346523454
#define REST_H_07018456781346523454

#include <chrono>
#include <functional>
#include <zen/sys_error.h>
#include <zen/zstring.h>
#include "curl_wrap.h" //DON'T include <curl/curl.h> directly!


namespace zen
{
//Initialization requirement: 1. WSAStartup 2. OpenSSL 3. curl_global_init()
// => use UniCounterCookie!

class HttpSession
{
public:
    HttpSession(const Zstring& server, const Zstring& caCertFilePath, std::chrono::seconds timeOut); //throw SysError
    ~HttpSession();

    struct Result
    {
        int statusCode = 0;
        //std::string contentType;
    };
    Result perform(const std::string& serverRelPath,
                   const std::vector<std::string>& extraHeaders, const std::vector<CurlOption>& extraOptions, //throw SysError
                   const std::function<void  (const void* buffer, size_t bytesToWrite)>& writeResponse /*throw X*/, //optional
                   const std::function<size_t(      void* buffer, size_t bytesToRead )>& readRequest   /*throw X*/); //

    std::chrono::steady_clock::time_point getLastUseTime() const { return lastSuccessfulUseTime_; }

private:
    HttpSession           (const HttpSession&) = delete;
    HttpSession& operator=(const HttpSession&) = delete;

    const std::string server_;
    const std::string caCertFilePath_;
    const std::chrono::seconds timeOutSec_;
    CURL* easyHandle_ = nullptr;
    std::chrono::steady_clock::time_point lastSuccessfulUseTime_ = std::chrono::steady_clock::now();
};
}

#endif //REST_H_07018456781346523454

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef CURL_WRAP_H_2879058325032785032789645
#define CURL_WRAP_H_2879058325032785032789645

#include <chrono>
#include <span>
#include <functional>
#include <zen/sys_error.h>


//-------------------------------------------------
#include <curl/curl.h>
//-------------------------------------------------

#ifndef CURLINC_CURL_H
    #error curl.h header guard changed
#endif

namespace zen
{
void libcurlInit();
void libcurlTearDown();


struct CurlOption
{
    template <class T>
    CurlOption(CURLoption o, T val) : option(o), value(static_cast<uint64_t>(val)) { static_assert(sizeof(val) <= sizeof(value)); }

    template <class T>
    CurlOption(CURLoption o, T* val) : option(o), value(reinterpret_cast<uint64_t>(val)) { static_assert(sizeof(val) <= sizeof(value)); }

    CURLoption option = CURLOPT_LASTENTRY;
    uint64_t value = 0;
};


class HttpSession
{
public:
    HttpSession(const Zstring& server, bool useTls, const Zstring& caCertFilePath /*optional*/); //throw SysError
    ~HttpSession();

    struct Result
    {
        int statusCode = 0;
        //std::string contentType;
    };
    Result perform(const std::string& serverRelPath,
                   const std::vector<std::string>& extraHeaders, const std::vector<CurlOption>& extraOptions,
                   const std::function<void  (std::span<const char> buf)>& writeResponse /*throw X*/, //optional
                   const std::function<size_t(std::span<      char> buf)>& readRequest   /*throw X*/, //optional; return "bytesToRead" bytes unless end of stream!
                   const std::function<void(const std::string_view& header)>& receiveHeader /*throw X*/,
                   int timeoutSec); //throw SysError, X

    std::chrono::steady_clock::time_point getLastUseTime() const { return lastSuccessfulUseTime_; }

private:
    HttpSession           (const HttpSession&) = delete;
    HttpSession& operator=(const HttpSession&) = delete;

    const std::string serverPrefix_;
    const std::string caCertFilePath_; //optional
    CURL* easyHandle_ = nullptr;
    std::chrono::steady_clock::time_point lastSuccessfulUseTime_ = std::chrono::steady_clock::now();
};


std::wstring formatCurlStatusCode(CURLcode sc);
}

#else
#error Why is this header already defined? Do not include in other headers: encapsulate the gory details!
#endif //CURL_WRAP_H_2879058325032785032789645

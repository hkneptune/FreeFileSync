// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef HTTP_H_879083425703425702
#define HTTP_H_879083425703425702

#include "sys_error.h"
#include "serialize.h"

namespace zen
{
/*  - Linux/macOS: init libcurl before use!
    - safe to use on worker thread     */
class HttpInputStream
{
public:
    //zen/serialize.h unbuffered input stream concept:
    size_t tryRead(void* buffer, size_t bytesToRead); //throw SysError; may return short; only 0 means EOF! CONTRACT: bytesToRead > 0!

    size_t getBlockSize() const;

    std::string readAll(const IoCallback& notifyUnbufferedIO /*throw X*/); //throw SysError, X

    class Impl;
    HttpInputStream(std::unique_ptr<Impl>&& pimpl);
    HttpInputStream(HttpInputStream&&) noexcept = default;
    ~HttpInputStream();

private:
    std::unique_ptr<Impl> pimpl_;
};


HttpInputStream sendHttpGet(const Zstring& url,
                            const Zstring& userAgent,
                            const Zstring& caCertFilePath /*optional: enable certificate validation*/); //throw SysError

HttpInputStream sendHttpPost(const Zstring& url,
                             const std::vector<std::pair<std::string, std::string>>& postParams, const IoCallback& notifyUnbufferedIO /*throw X*/,
                             const Zstring& userAgent,
                             const Zstring& caCertFilePath /*optional: enable certificate validation*/); //throw SysError, X

HttpInputStream sendHttpPost(const Zstring& url,
                             const std::string& postBuf, const std::string& contentType, const IoCallback& notifyUnbufferedIO /*throw X*/,
                             const Zstring& userAgent,
                             const Zstring& caCertFilePath /*optional: enable certificate validation*/); //throw SysError, X

bool internetIsAlive(); //noexcept
std::wstring formatHttpError(int httpStatus);
bool isValidEmail(const std::string_view& email);
std::string htmlSpecialChars(const std::string_view& str);

std::string xWwwFormUrlEncode(const std::vector<std::pair<std::string, std::string>>& paramPairs);
std::vector<std::pair<std::string, std::string>> xWwwFormUrlDecode(const std::string_view str);
}

#endif //HTTP_H_879083425703425702

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef HTTP_H_879083425703425702
#define HTTP_H_879083425703425702

#include <zen/zstring.h>
#include <zen/sys_error.h>
#include <zen/serialize.h>

namespace zen
{
/*
    - thread-safe! (Window/Linux/macOS)
    - Linux/macOS: init OpenSSL before use!
*/
class HttpInputStream
{
public:
    //support zen/serialize.h buffered input stream concept
    size_t read(void* buffer, size_t bytesToRead); //throw SysError, X; return "bytesToRead" bytes unless end of stream!
    std::string readAll();                         //throw SysError, X

    size_t getBlockSize() const;

    class Impl;
    HttpInputStream(std::unique_ptr<Impl>&& pimpl);
    HttpInputStream(HttpInputStream&&) noexcept = default;
    ~HttpInputStream();

private:
    std::unique_ptr<Impl> pimpl_;
};


HttpInputStream sendHttpGet(const Zstring& url,
                            const Zstring& userAgent,
                            const Zstring* caCertFilePath /*optional: enable certificate validation*/,
                            const IOCallback& notifyUnbufferedIO /*throw X*/); //throw SysError

HttpInputStream sendHttpPost(const Zstring& url,
                             const std::vector<std::pair<std::string, std::string>>& postParams,
                             const Zstring& userAgent,
                             const Zstring* caCertFilePath /*optional: enable certificate validation*/,
                             const IOCallback& notifyUnbufferedIO /*throw X*/);
bool internetIsAlive(); //noexcept

std::string xWwwFormUrlEncode(const std::vector<std::pair<std::string, std::string>>& paramPairs);
std::vector<std::pair<std::string, std::string>> xWwwFormUrlDecode(const std::string& str);
}

#endif //HTTP_H_879083425703425702

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SYS_ERROR_H_3284791347018951324534
#define SYS_ERROR_H_3284791347018951324534

#include <string>
#include "scope_guard.h"
#include "i18n.h"
#include "utf.h"

    #include <cerrno>


namespace zen
{
//evaluate GetLastError()/errno and assemble specific error message
    using ErrorCode = int;

ErrorCode getLastError();

std::wstring formatSystemError(const std::wstring& functionName, const std::wstring& errorCode, const std::wstring& errorMsg);
std::wstring formatSystemError(const std::wstring& functionName, ErrorCode ec);


//A low-level exception class giving (non-translated) detail information only - same conceptional level like "GetLastError()"!
class SysError
{
public:
    explicit SysError(const std::wstring& msg) : msg_(msg) {}
    const std::wstring& toString() const { return msg_; }

private:
    std::wstring msg_;
};

#define DEFINE_NEW_SYS_ERROR(X) struct X : public zen::SysError { X(const std::wstring& msg) : SysError(msg) {} };



#define THROW_LAST_SYS_ERROR(functionName)                           \
    do { const ErrorCode ecInternal = getLastError(); throw SysError(formatSystemError(functionName, ecInternal)); } while (false)





//######################## implementation ########################
inline
ErrorCode getLastError()
{
    return errno; //don't use "::" prefix, errno is a macro!
}


std::wstring getSystemErrorDescription(ErrorCode ec); //return empty string on error
//intentional overload ambiguity to catch usage errors with HRESULT:
std::wstring getSystemErrorDescription(long long) = delete;


}

#endif //SYS_ERROR_H_3284791347018951324534

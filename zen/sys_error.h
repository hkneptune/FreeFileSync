// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SYS_ERROR_H_3284791347018951324534
#define SYS_ERROR_H_3284791347018951324534

#include <string>
#include "utf.h"
#include "i18n.h"
#include "scope_guard.h"

    #include <cstring>
    #include <cerrno>


namespace zen
{
//evaluate GetLastError()/errno and assemble specific error message
    using ErrorCode = int;

ErrorCode getLastError();

std::wstring formatSystemError(const std::wstring& functionName, ErrorCode ec);
std::wstring formatSystemError(const std::wstring& functionName, const std::wstring& errorCode, const std::wstring& errorMsg);



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
    return errno; //don't use "::", errno is a macro!
}


std::wstring formatSystemErrorRaw(long long) = delete; //intentional overload ambiguity to catch usage errors

inline
std::wstring formatSystemErrorRaw(ErrorCode ec) //return empty string on error
{
    const ErrorCode currentError = getLastError(); //not necessarily == lastError

    std::wstring errorMsg;
    ZEN_ON_SCOPE_EXIT(errno = currentError);

    errorMsg = utfTo<std::wstring>(::strerror(ec));
    trim(errorMsg); //Windows messages seem to end with a blank...

    return errorMsg;
}


std::wstring formatSystemError(const std::wstring& functionName, long long lastError) = delete; //intentional overload ambiguity to catch usage errors with HRESULT!

inline
std::wstring formatSystemError(const std::wstring& functionName, ErrorCode ec)
{
    //const std::wstring errorCode = printNumber<std::wstring>(L"0x%08x", static_cast<int>(ec));
    const std::wstring errorCode = numberTo<std::wstring>(ec);

    return formatSystemError(functionName, replaceCpy(_("Error Code %x"), L"%x", errorCode), formatSystemErrorRaw(ec));
}


inline
std::wstring formatSystemError(const std::wstring& functionName, const std::wstring& errorCode, const std::wstring& errorMsg)
{
    std::wstring output = errorCode + L":";

    const std::wstring errorMsgFmt = trimCpy(errorMsg);
    if (!errorMsgFmt.empty())
    {
        output += L" ";
        output += errorMsgFmt;
    }

    output += L" [" + functionName + L"]";

    return output;
}

}

#endif //SYS_ERROR_H_3284791347018951324534

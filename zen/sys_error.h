// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SYS_ERROR_H_3284791347018951324534
#define SYS_ERROR_H_3284791347018951324534

#include "scope_guard.h" //
#include "i18n.h"        //not used by this header, but the "rest of the world" needs it!
#include "zstring.h"     //
#include "extra_log.h"   //

    #include <glib.h>
    #include <cerrno>


namespace zen
{
//evaluate GetLastError()/errno and assemble specific error message
    using ErrorCode = int;

ErrorCode getLastError();

std::wstring formatSystemError(const std::string& functionName, const std::wstring& errorCode, const std::wstring& errorMsg);
std::wstring formatSystemError(const std::string& functionName, ErrorCode ec);
    std::wstring formatGlibError(const std::string& functionName, GError* error);


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



//better leave it as a macro (see comment in file_error.h)
#define THROW_LAST_SYS_ERROR(functionName)                           \
    do { const ErrorCode ecInternal = getLastError(); throw zen::SysError(formatSystemError(functionName, ecInternal)); } while (false)


/* Example: ASSERT_SYSERROR(expr);

    Equivalent to:
        if (!expr)
            throw zen::SysError(L"Assertion failed: \"expr\"");            */
#define ASSERT_SYSERROR(expr) ASSERT_SYSERROR_IMPL(expr, #expr) //throw SysError



//######################## implementation ########################
inline
ErrorCode getLastError()
{
    return errno; //don't use "::" prefix, errno is a macro!
}


std::wstring getSystemErrorDescription(ErrorCode ec); //return empty string on error
//intentional overload ambiguity to catch usage errors with HRESULT:
std::wstring getSystemErrorDescription(long long) = delete;




namespace impl
{
inline bool validateBool(bool  b) { return b; }
inline bool validateBool(void* b) { return b; }
bool validateBool(int) = delete; //catch unintended bool conversions, e.g. HRESULT
}
#define ASSERT_SYSERROR_IMPL(expr, exprStr) \
    { if (!zen::impl::validateBool(expr))        \
            throw zen::SysError(L"Assertion failed: \"" L ## exprStr L"\""); }
}

#endif //SYS_ERROR_H_3284791347018951324534

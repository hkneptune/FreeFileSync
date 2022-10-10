// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_ERROR_H_839567308565656789
#define FILE_ERROR_H_839567308565656789

#include "sys_error.h" //we'll need this later anyway!


namespace zen
{
class FileError //A high-level exception class giving detailed context information for end users
{
public:
    explicit FileError(const std::wstring& msg) : msg_(msg) {}
    FileError(const std::wstring& msg, const std::wstring& details) : msg_(msg + L"\n\n" + details) {}
    virtual ~FileError() {}

    const std::wstring& toString() const { return msg_; }

private:
    std::wstring msg_;
};

#define DEFINE_NEW_FILE_ERROR(X) struct X : public zen::FileError { X(const std::wstring& msg) : FileError(msg) {} X(const std::wstring& msg, const std::wstring& descr) : FileError(msg, descr) {} };

DEFINE_NEW_FILE_ERROR(ErrorTargetExisting)
DEFINE_NEW_FILE_ERROR(ErrorFileLocked)
DEFINE_NEW_FILE_ERROR(ErrorMoveUnsupported)
DEFINE_NEW_FILE_ERROR(RecycleBinUnavailable)


//CAVEAT: thread-local Win32 error code is easily overwritten => evaluate *before* making any (indirect) system calls:
//-> MinGW + Win XP: "throw" statement allocates memory to hold the exception object => error code is cleared
//-> VC 2015, Debug: std::wstring allocator internally calls ::FlsGetValue()         => error code is cleared
//      https://connect.microsoft.com/VisualStudio/feedback/details/1775690/calling-operator-new-may-set-lasterror-to-0
#define THROW_LAST_FILE_ERROR(msg, functionName)                           \
    do { const ErrorCode ecInternal = getLastError(); throw FileError(msg, formatSystemError(functionName, ecInternal)); } while (false)

//----------- facilitate usage of std::wstring for error messages --------------------

inline std::wstring fmtPath(const std::wstring& displayPath) { return L'"' + displayPath + L'"'; }
inline std::wstring fmtPath(const Zstring& displayPath) { return fmtPath(utfTo<std::wstring>(displayPath)); }
inline std::wstring fmtPath(const wchar_t* displayPath) { return fmtPath(std::wstring(displayPath)); } //resolve overload ambiguity
}

#endif //FILE_ERROR_H_839567308565656789

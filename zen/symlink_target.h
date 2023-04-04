// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SYMLINK_TARGET_H_801783470198357483
#define SYMLINK_TARGET_H_801783470198357483

#include "file_error.h"

    #include <unistd.h>
    #include <stdlib.h> //realpath


namespace zen
{

struct SymlinkRawContent
{
    Zstring targetPath;
};
SymlinkRawContent getSymlinkRawContent(const Zstring& linkPath); //throw FileError

Zstring getSymlinkResolvedPath(const Zstring& linkPath); //throw FileError
}









//################################ implementation ################################

namespace
{
//retrieve raw target data of symlink or junction
zen::SymlinkRawContent getSymlinkRawContent_impl(const Zstring& linkPath) //throw SysError
{
    using namespace zen;
    const size_t bufSize = 10000;
    std::vector<char> buf(bufSize);

    const ssize_t bytesWritten = ::readlink(linkPath.c_str(), buf.data(), bufSize);
    if (bytesWritten < 0)
        THROW_LAST_SYS_ERROR("readlink");
    if (makeUnsigned(bytesWritten) >= bufSize) //detect truncation; not an error for readlink!
        throw SysError(formatSystemError("readlink", L"", L"Buffer truncated."));

    return {.targetPath = Zstring(buf.data(), bytesWritten)}; //readlink does not append 0-termination!
}


Zstring getResolvedSymlinkPath_impl(const Zstring& linkPath) //throw SysError
{
    using namespace zen;
    char* targetPath = ::realpath(linkPath.c_str(), nullptr);
    if (!targetPath)
        THROW_LAST_SYS_ERROR("realpath");
    ZEN_ON_SCOPE_EXIT(::free(targetPath));
    return targetPath;
}
}


namespace zen
{
inline
SymlinkRawContent getSymlinkRawContent(const Zstring& linkPath)
{
    try
    {
        return getSymlinkRawContent_impl(linkPath); //throw SysError
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(linkPath)), e.toString()); }
}


inline
Zstring getSymlinkResolvedPath(const Zstring& linkPath)
{
    try
    {
        return getResolvedSymlinkPath_impl(linkPath); //throw SysError
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot determine final path for %x."), L"%x", fmtPath(linkPath)), e.toString()); }
}

}

#endif //SYMLINK_TARGET_H_801783470198357483

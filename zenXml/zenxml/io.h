// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef IO_H_8917640501480763248343343
#define IO_H_8917640501480763248343343

#include <cstdio>
#include <cerrno>
#include <zen/scope_guard.h>
#include <zen/utf.h>
#include "error.h"


namespace zen
{
/**
\file
\brief Save and load byte streams from files
*/



///Exception thrown due to failed file I/O
struct XmlFileError : public XmlError
{
    using ErrorCode = int;

    explicit XmlFileError(ErrorCode ec) : lastError(ec) {}
    ///Native error code: errno
    ErrorCode lastError;
};




///Save byte stream to a file
/**
\tparam String Arbitrary string-like type: e.g. std::string, wchar_t*, char[], wchar_t, wxString, MyStringClass, ...
\param stream Input byte stream
\param filename Output file name
\throw XmlFileError
*/
template <class String>
void saveStream(const std::string& stream, const String& filename) //throw XmlFileError
{
    FILE* handle = ::fopen(utfTo<std::string>(filename).c_str(), "w");
    if (!handle)
        throw XmlFileError(errno);
    ZEN_ON_SCOPE_EXIT(::fclose(handle));

    const size_t bytesWritten = ::fwrite(stream.c_str(), 1, stream.size(), handle);
    if (::ferror(handle) != 0)
        throw XmlFileError(errno);

    (void)bytesWritten;
    assert(bytesWritten == stream.size());
}


///Load byte stream from a file
/**
\tparam String Arbitrary string-like type: e.g. std::string, wchar_t*, char[], wchar_t, wxString, MyStringClass, ...
\param filename Input file name
\return Output byte stream
\throw XmlFileError
*/
template <class String>
std::string loadStream(const String& filename) //throw XmlFileError
{
    FILE* handle = ::fopen(utfTo<std::string>(filename).c_str(), "r");
    if (!handle)
        throw XmlFileError(errno);
    ZEN_ON_SCOPE_EXIT(::fclose(handle));

    std::string stream;
    const size_t blockSize = 64 * 1024;
    do
    {
        stream.resize(stream.size() + blockSize); //let's pray std::string implements exponential growth!

        const size_t bytesRead = ::fread(&*(stream.end() - blockSize), 1, blockSize, handle);
        if (::ferror(handle))
            throw XmlFileError(errno);
        if (bytesRead > blockSize)
            throw XmlFileError(0);
        if (bytesRead < blockSize)
            stream.resize(stream.size() - blockSize + bytesRead); //caveat: unsigned arithmetics
    }
    while (!::feof(handle));

    return stream;
}
}

#endif //IO_H_8917640501480763248343343

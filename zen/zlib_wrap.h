// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ZLIB_WRAP_H_428597064566
#define ZLIB_WRAP_H_428597064566

#include <functional>
#include "sys_error.h"


namespace zen
{
// compression level must be between 0 and 9:
// 0: no compression
// 9: best compression
std::string compress(const std::string_view& stream, int level); //throw SysError
//caveat: output stream is physically larger than input! => strip additional reserved space if needed: "BinContainer(output.begin(), output.end())"

std::string decompress(const std::string_view& stream); //throw SysError


class InputStreamAsGzip //convert input stream into gzip on the fly
{
public:
    explicit InputStreamAsGzip(const std::function<size_t(void* buffer, size_t bytesToRead)>& tryReadBlock /*throw X; may return short, only 0 means EOF!*/,
                               size_t blockSize); //throw SysError
    ~InputStreamAsGzip();

    size_t getBlockSize() const;

    size_t read(void* buffer, size_t bytesToRead); //throw SysError, X; return "bytesToRead" bytes unless end of stream!

private:
    class Impl;
    const std::unique_ptr<Impl> pimpl_;
};

std::string compressAsGzip(const std::string_view& stream); //throw SysError
}

#endif //ZLIB_WRAP_H_428597064566

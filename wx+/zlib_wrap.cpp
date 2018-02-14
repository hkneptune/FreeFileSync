// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "zlib_wrap.h"
    #include <zlib.h> //let's pray this is the same version wxWidgets is linking against!

using namespace zen;


size_t zen::impl::zlib_compressBound(size_t len)
{
    return ::compressBound(static_cast<uLong>(len)); //upper limit for buffer size, larger than input size!!!
}


size_t zen::impl::zlib_compress(const void* src, size_t srcLen, void* trg, size_t trgLen, int level) //throw ZlibInternalError
{
    uLongf bufferSize = static_cast<uLong>(trgLen);
    const int rv = ::compress2(static_cast<Bytef*>(trg),       //Bytef* dest,
                               &bufferSize,                    //uLongf* destLen,
                               static_cast<const Bytef*>(src), //const Bytef* source,
                               static_cast<uLong>(srcLen),     //uLong sourceLen,
                               level);                         //int level
    // Z_OK: success
    // Z_MEM_ERROR: not enough memory
    // Z_BUF_ERROR: not enough room in the output buffer
    if (rv != Z_OK || bufferSize > trgLen)
        throw ZlibInternalError();
    return bufferSize;
}


size_t zen::impl::zlib_decompress(const void* src, size_t srcLen, void* trg, size_t trgLen) //throw ZlibInternalError
{
    uLongf bufferSize = static_cast<uLong>(trgLen);
    const int rv = ::uncompress(static_cast<Bytef*>(trg),       //Bytef* dest,
                                &bufferSize,                    //uLongf* destLen,
                                static_cast<const Bytef*>(src), //const Bytef* source,
                                static_cast<uLong>(srcLen));    //uLong sourceLen
    // Z_OK: success
    // Z_MEM_ERROR: not enough memory
    // Z_BUF_ERROR: not enough room in the output buffer
    // Z_DATA_ERROR: input data was corrupted or incomplete
    if (rv != Z_OK || bufferSize > trgLen)
        throw ZlibInternalError();
    return bufferSize;
}

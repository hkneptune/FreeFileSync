// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ZLIB_WRAP_H_428597064566
#define ZLIB_WRAP_H_428597064566

#include "serialize.h"


namespace zen
{
class ZlibInternalError {};

// compression level must be between 0 and 9:
// 0: no compression
// 9: best compression
template <class BinContainer> //as specified in serialize.h
BinContainer compress(const BinContainer& stream, int level); //throw ZlibInternalError
//caveat: output stream is physically larger than input! => strip additional reserved space if needed: "BinContainer(output.begin(), output.end())"

template <class BinContainer>
BinContainer decompress(const BinContainer& stream);          //throw ZlibInternalError


class InputStreamAsGzip //convert input stream into gzip on the fly
{
public:
    InputStreamAsGzip( //throw ZlibInternalError
        const std::function<size_t(void* buffer, size_t bytesToRead)>& readBlock /*throw X*/); //returning 0 signals EOF: Posix read() semantics
    ~InputStreamAsGzip();

    size_t read(void* buffer, size_t bytesToRead); //throw ZlibInternalError, X; return "bytesToRead" bytes unless end of stream!

private:
    class Impl;
    const std::unique_ptr<Impl> pimpl_;
};







//######################## implementation ##########################
namespace impl
{
size_t zlib_compressBound(size_t len);
size_t zlib_compress  (const void* src, size_t srcLen, void* trg, size_t trgLen, int level); //throw ZlibInternalError
size_t zlib_decompress(const void* src, size_t srcLen, void* trg, size_t trgLen);            //throw ZlibInternalError
}


template <class BinContainer>
BinContainer compress(const BinContainer& stream, int level) //throw ZlibInternalError
{
    BinContainer contOut;
    if (!stream.empty()) //don't dereference iterator into empty container!
    {
        //save uncompressed stream size for decompression
        const uint64_t uncompressedSize = stream.size(); //use portable number type!
        contOut.resize(sizeof(uncompressedSize));
        std::memcpy(&*contOut.begin(), &uncompressedSize, sizeof(uncompressedSize));

        const size_t bufferEstimate = impl::zlib_compressBound(stream.size()); //upper limit for buffer size, larger than input size!!!

        contOut.resize(contOut.size() + bufferEstimate);

        const size_t bytesWritten = impl::zlib_compress(&*stream.begin(),
                                                        stream.size(),
                                                        &*contOut.begin() + contOut.size() - bufferEstimate,
                                                        bufferEstimate,
                                                        level); //throw ZlibInternalError
        if (bytesWritten < bufferEstimate)
            contOut.resize(contOut.size() - (bufferEstimate - bytesWritten)); //caveat: unsigned arithmetics
        //caveat: physical memory consumption still *unchanged*!
    }
    return contOut;
}


template <class BinContainer>
BinContainer decompress(const BinContainer& stream) //throw ZlibInternalError
{
    BinContainer contOut;
    if (!stream.empty()) //don't dereference iterator into empty container!
    {
        //retrieve size of uncompressed data
        uint64_t uncompressedSize = 0; //use portable number type!
        if (stream.size() < sizeof(uncompressedSize))
            throw ZlibInternalError();

        std::memcpy(&uncompressedSize, &*stream.begin(), sizeof(uncompressedSize));

        //attention: contOut MUST NOT be empty! Else it will pass a nullptr to zlib_decompress() => Z_STREAM_ERROR although "uncompressedSize == 0"!!!
        //secondary bug: don't dereference iterator into empty container!
        if (uncompressedSize == 0) //cannot be 0: compress() directly maps empty -> empty container skipping zlib!
            throw ZlibInternalError();

        try
        {
            contOut.resize(static_cast<size_t>(uncompressedSize)); //throw std::bad_alloc
        }
        catch (std::bad_alloc&) //most likely due to data corruption!
        {
            throw ZlibInternalError();
        }

        const size_t bytesWritten = impl::zlib_decompress(&*stream.begin() + sizeof(uncompressedSize),
                                                          stream.size() - sizeof(uncompressedSize),
                                                          &*contOut.begin(),
                                                          static_cast<size_t>(uncompressedSize)); //throw ZlibInternalError
        if (bytesWritten != static_cast<size_t>(uncompressedSize))
            throw ZlibInternalError();
    }
    return contOut;
}
}

#endif //ZLIB_WRAP_H_428597064566

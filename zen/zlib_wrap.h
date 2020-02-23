// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ZLIB_WRAP_H_428597064566
#define ZLIB_WRAP_H_428597064566

#include "serialize.h"
#include "sys_error.h"


namespace zen
{
// compression level must be between 0 and 9:
// 0: no compression
// 9: best compression
template <class BinContainer> //as specified in serialize.h
BinContainer compress(const BinContainer& stream, int level); //throw SysError
//caveat: output stream is physically larger than input! => strip additional reserved space if needed: "BinContainer(output.begin(), output.end())"

template <class BinContainer>
BinContainer decompress(const BinContainer& stream); //throw SysError


class InputStreamAsGzip //convert input stream into gzip on the fly
{
public:
    explicit InputStreamAsGzip( //throw SysError
        const std::function<size_t(void* buffer, size_t bytesToRead)>& readBlock /*throw X;  returning 0 signals EOF: Posix read() semantics*/);
    ~InputStreamAsGzip();

    size_t read(void* buffer, size_t bytesToRead); //throw SysError, X; return "bytesToRead" bytes unless end of stream!

private:
    class Impl;
    const std::unique_ptr<Impl> pimpl_;
};

std::string compressAsGzip(const void* buffer, size_t bufSize); //throw SysError






//######################## implementation ##########################
namespace impl
{
size_t zlib_compressBound(size_t len);
size_t zlib_compress  (const void* src, size_t srcLen, void* trg, size_t trgLen, int level); //throw SysError
size_t zlib_decompress(const void* src, size_t srcLen, void* trg, size_t trgLen);            //throw SysError
}


template <class BinContainer>
BinContainer compress(const BinContainer& stream, int level) //throw SysError
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
                                                        level); //throw SysError
        if (bytesWritten < bufferEstimate)
            contOut.resize(contOut.size() - (bufferEstimate - bytesWritten)); //caveat: unsigned arithmetics
        //caveat: physical memory consumption still *unchanged*!
    }
    return contOut;
}


template <class BinContainer>
BinContainer decompress(const BinContainer& stream) //throw SysError
{
    BinContainer contOut;
    if (!stream.empty()) //don't dereference iterator into empty container!
    {
        //retrieve size of uncompressed data
        uint64_t uncompressedSize = 0; //use portable number type!
        if (stream.size() < sizeof(uncompressedSize))
            throw SysError(L"zlib error: stream size < 8");

        std::memcpy(&uncompressedSize, &*stream.begin(), sizeof(uncompressedSize));

        //attention: contOut MUST NOT be empty! Else it will pass a nullptr to zlib_decompress() => Z_STREAM_ERROR although "uncompressedSize == 0"!!!
        //secondary bug: don't dereference iterator into empty container!
        if (uncompressedSize == 0) //cannot be 0: compress() directly maps empty -> empty container skipping zlib!
            throw SysError(L"zlib error: uncompressed size == 0");

        try
        {
            contOut.resize(static_cast<size_t>(uncompressedSize)); //throw std::bad_alloc
        }
        //most likely this is due to data corruption:
        catch (const std::length_error& e) { throw SysError(L"zlib error: " + _("Out of memory.") + L" " + utfTo<std::wstring>(e.what())); }
        catch (const    std::bad_alloc& e) { throw SysError(L"zlib error: " + _("Out of memory.") + L" " + utfTo<std::wstring>(e.what())); }

        const size_t bytesWritten = impl::zlib_decompress(&*stream.begin() + sizeof(uncompressedSize),
                                                          stream.size() - sizeof(uncompressedSize),
                                                          &*contOut.begin(),
                                                          static_cast<size_t>(uncompressedSize)); //throw SysError
        if (bytesWritten != static_cast<size_t>(uncompressedSize))
            throw SysError(L"zlib error: bytes written != uncompressed size");
    }
    return contOut;
}
}

#endif //ZLIB_WRAP_H_428597064566

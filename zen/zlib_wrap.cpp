// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "zlib_wrap.h"
//Windows:     use the SAME zlib version that wxWidgets is linking against! //C:\Data\Projects\wxWidgets\Source\src\zlib\zlib.h
//Linux/macOS: use zlib system header for wxWidgets, libcurl (HTTP), libssh2 (SFTP)
//             => don't compile wxWidgets with: --with-zlib=builtin
#include <zlib.h>
#include "scope_guard.h"
#include "serialize.h"

using namespace zen;


namespace
{
std::wstring getZlibErrorLiteral(int sc)
{
    switch (sc)
    {
            ZEN_CHECK_CASE_FOR_CONSTANT(Z_NEED_DICT);
            ZEN_CHECK_CASE_FOR_CONSTANT(Z_STREAM_END);
            ZEN_CHECK_CASE_FOR_CONSTANT(Z_OK);
            ZEN_CHECK_CASE_FOR_CONSTANT(Z_ERRNO);
            ZEN_CHECK_CASE_FOR_CONSTANT(Z_STREAM_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(Z_DATA_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(Z_MEM_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(Z_BUF_ERROR);
            ZEN_CHECK_CASE_FOR_CONSTANT(Z_VERSION_ERROR);

        default:
            return replaceCpy<std::wstring>(L"zlib error %x", L"%x", numberTo<std::wstring>(sc));
    }
}


size_t zlib_compressBound(size_t len)
{
    return ::compressBound(static_cast<uLong>(len)); //upper limit for buffer size, larger than input size!!!
}


size_t zlib_compress(const void* src, size_t srcLen, void* trg, size_t trgLen, int level) //throw SysError
{
    uLongf bufSize = static_cast<uLong>(trgLen);
    const int rv = ::compress2(static_cast<Bytef*>(trg),       //Bytef* dest
                               &bufSize,                       //uLongf* destLen
                               static_cast<const Bytef*>(src), //const Bytef* source
                               static_cast<uLong>(srcLen),     //uLong sourceLen
                               level);                         //int level
    // Z_OK: success
    // Z_MEM_ERROR: not enough memory
    // Z_BUF_ERROR: not enough room in the output buffer
    if (rv != Z_OK || bufSize > trgLen)
        throw SysError(formatSystemError("zlib compress2", getZlibErrorLiteral(rv), L""));

    return bufSize;
}


size_t zlib_decompress(const void* src, size_t srcLen, void* trg, size_t trgLen) //throw SysError
{
    uLongf bufSize = static_cast<uLong>(trgLen);
    const int rv = ::uncompress(static_cast<Bytef*>(trg),       //Bytef* dest
                                &bufSize,                       //uLongf* destLen
                                static_cast<const Bytef*>(src), //const Bytef* source
                                static_cast<uLong>(srcLen));    //uLong sourceLen
    // Z_OK: success
    // Z_MEM_ERROR: not enough memory
    // Z_BUF_ERROR: not enough room in the output buffer
    // Z_DATA_ERROR: input data was corrupted or incomplete
    if (rv != Z_OK || bufSize > trgLen)
        throw SysError(formatSystemError("zlib uncompress", getZlibErrorLiteral(rv), L""));

    return bufSize;
}
}


#undef compress //mitigate zlib macro shit...

std::string zen::compress(const std::string_view& stream, int level) //throw SysError
{
    std::string output;
    if (!stream.empty()) //don't dereference iterator into empty container!
    {
        //save uncompressed stream size for decompression
        const uint64_t uncompressedSize = stream.size(); //use portable number type!
        output.resize(sizeof(uncompressedSize));
        std::memcpy(output.data(), &uncompressedSize, sizeof(uncompressedSize));

        const size_t bufferEstimate = zlib_compressBound(stream.size()); //upper limit for buffer size, larger than input size!!!

        output.resize(output.size() + bufferEstimate);

        const size_t bytesWritten = zlib_compress(stream.data(),
                                                  stream.size(),
                                                  output.data() + output.size() - bufferEstimate,
                                                  bufferEstimate,
                                                  level); //throw SysError
        if (bytesWritten < bufferEstimate)
            output.resize(output.size() - bufferEstimate + bytesWritten); //caveat: unsigned arithmetics
        //caveat: physical memory consumption still *unchanged*!
    }
    return output;
}


std::string zen::decompress(const std::string_view& stream) //throw SysError
{
    std::string output;
    if (!stream.empty()) //don't dereference iterator into empty container!
    {
        //retrieve size of uncompressed data
        uint64_t uncompressedSize = 0; //use portable number type!
        if (stream.size() < sizeof(uncompressedSize))
            throw SysError(L"zlib error: stream size < 8");

        std::memcpy(&uncompressedSize, stream.data(), sizeof(uncompressedSize));

        //attention: output MUST NOT be empty! Else it will pass a nullptr to zlib_decompress() => Z_STREAM_ERROR although "uncompressedSize == 0"!!!
        if (uncompressedSize == 0) //cannot be 0: compress() directly maps empty -> empty container skipping zlib!
            throw SysError(L"zlib error: uncompressed size == 0");

        try
        {
            output.resize(static_cast<size_t>(uncompressedSize)); //throw std::bad_alloc
        }
        //most likely this is due to data corruption:
        catch (const std::length_error& e) { throw SysError(L"zlib error: " + _("Out of memory.") + L' ' + utfTo<std::wstring>(e.what())); }
        catch (const    std::bad_alloc& e) { throw SysError(L"zlib error: " + _("Out of memory.") + L' ' + utfTo<std::wstring>(e.what())); }

        const size_t bytesWritten = zlib_decompress(stream.data() + sizeof(uncompressedSize),
                                                    stream.size() - sizeof(uncompressedSize),
                                                    output.data(),
                                                    static_cast<size_t>(uncompressedSize)); //throw SysError
        if (bytesWritten != static_cast<size_t>(uncompressedSize))
            throw SysError(formatSystemError("zlib_decompress", L"", L"bytes written != uncompressed size."));
    }
    return output;
}


class InputStreamAsGzip::Impl
{
public:
    Impl(const std::function<size_t(void* buffer, size_t bytesToRead)>& tryReadBlock /*throw X; may return short, only 0 means EOF!*/,
         size_t blockSize) : //throw SysError
        tryReadBlock_(tryReadBlock),
        blockSize_(blockSize)
    {
        const int windowBits = MAX_WBITS + 16; //"add 16 to windowBits to write a simple gzip header"

        //"memLevel=1 uses minimum memory but is slow and reduces compression ratio; memLevel=9 uses maximum memory for optimal speed.
        const int memLevel = 9; //test; 280 MB installer file: level 9 shrinks runtime by ~8% compared to level 8 (==DEF_MEM_LEVEL) at the cost of 128 KB extra memory
        static_assert(memLevel <= MAX_MEM_LEVEL);

        const int rv = ::deflateInit2(&gzipStream_,          //z_streamp strm
                                      3 /*see db_file.cpp*/, //int level
                                      Z_DEFLATED,            //int method
                                      windowBits,            //int windowBits
                                      memLevel,              //int memLevel
                                      Z_DEFAULT_STRATEGY);   //int strategy
        if (rv != Z_OK)
            throw SysError(formatSystemError("zlib deflateInit2", getZlibErrorLiteral(rv), L""));
    }

    ~Impl()
    {
        [[maybe_unused]] const int rv = ::deflateEnd(&gzipStream_);
        assert(rv == Z_OK);
    }

    size_t read(void* buffer, size_t bytesToRead) //throw SysError, X; return "bytesToRead" bytes unless end of stream!
    {
        if (bytesToRead == 0) //"read() with a count of 0 returns zero" => indistinguishable from end of file! => check!
            throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

        gzipStream_.next_out  = static_cast<Bytef*>(buffer);
        gzipStream_.avail_out = static_cast<uInt>(bytesToRead);

        for (;;)
        {
            //refill input buffer once avail_in == 0: https://www.zlib.net/manual.html
            if (gzipStream_.avail_in == 0 && !eof_)
            {
                const size_t bytesRead = tryReadBlock_(bufIn_.data(), blockSize_); //throw X; may return short, only 0 means EOF!
                gzipStream_.next_in  = reinterpret_cast<z_const Bytef*>(bufIn_.data());
                gzipStream_.avail_in = static_cast<uInt>(bytesRead);
                if (bytesRead == 0)
                    eof_ = true;
            }

            const int rv = ::deflate(&gzipStream_, eof_ ? Z_FINISH : Z_NO_FLUSH);
            if (eof_ && rv == Z_STREAM_END)
                return bytesToRead - gzipStream_.avail_out;
            if (rv != Z_OK)
                throw SysError(formatSystemError("zlib deflate", getZlibErrorLiteral(rv), L""));

            if (gzipStream_.avail_out == 0)
                return bytesToRead;
        }
    }

    size_t getBlockSize() const { return blockSize_; } //returning input blockSize_ makes sense for low compression ratio

private:
    const std::function<size_t(void* buffer, size_t bytesToRead)> tryReadBlock_; //throw X
    const size_t blockSize_;
    bool eof_ = false;
    std::vector<std::byte> bufIn_{blockSize_};
    z_stream gzipStream_ = {};
};


InputStreamAsGzip::InputStreamAsGzip(const std::function<size_t(void* buffer, size_t bytesToRead)>& tryReadBlock /*throw X*/, size_t blockSize) :
    pimpl_(std::make_unique<Impl>(tryReadBlock, blockSize)) {} //throw SysError

InputStreamAsGzip::~InputStreamAsGzip() {}

size_t InputStreamAsGzip::getBlockSize() const { return pimpl_->getBlockSize(); }

size_t InputStreamAsGzip::read(void* buffer, size_t bytesToRead) { return pimpl_->read(buffer, bytesToRead); } //throw SysError, X


std::string zen::compressAsGzip(const std::string_view& stream) //throw SysError
{
    MemoryStreamIn memStream(stream);

    auto tryReadBlock = [&](void* buffer, size_t bytesToRead) //may return short, only 0 means EOF!
    {
        return memStream.read(buffer, bytesToRead); //return "bytesToRead" bytes unless end of stream!
    };

    InputStreamAsGzip gzipStream(tryReadBlock, 1024 * 1024 /*blockSize*/); //throw SysError

    return unbufferedLoad<std::string>([&](void* buffer, size_t bytesToRead)
    {
        return gzipStream.read(buffer, bytesToRead); //throw SysError;  return "bytesToRead" bytes unless end of stream!
    },
    gzipStream.getBlockSize()); //throw SysError
}

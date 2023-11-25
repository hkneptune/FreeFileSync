// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SERIALIZE_H_839405783574356
#define SERIALIZE_H_839405783574356

#include <functional>
#include "sys_error.h"
//keep header clean from specific stream implementations! (e.g.file_io.h)! used by abstract.h!


namespace zen
{
/* high-performance unformatted serialization (avoiding wxMemoryOutputStream/wxMemoryInputStream inefficiencies)

    ----------------------------
    | Binary Container Concept |
    ----------------------------
        binary container for data storage: must support "basic" std::vector interface (e.g. std::vector<std::byte>, std::string, Zbase<char>)

    ---------------------------------
    | Unbuffered Input Stream Concept |
    ---------------------------------
        size_t getBlockSize(); //throw X
        size_t tryRead(void* buffer, size_t bytesToRead); //throw X; may return short; only 0 means EOF! CONTRACT: bytesToRead > 0!

    ----------------------------------
    | Unbuffered Output Stream Concept |
    ----------------------------------
        size_t getBlockSize(); //throw X
        size_t tryWrite(const void* buffer, size_t bytesToWrite); //throw X; may return short! CONTRACT: bytesToWrite > 0

    ===============================================================================================

    ---------------------------------
    | Buffered Input Stream Concept |
    ---------------------------------
        size_t read(void* buffer, size_t bytesToRead); //throw X; return "bytesToRead" bytes unless end of stream!

    ----------------------------------
    | Buffered Output Stream Concept |
    ----------------------------------
        void write(const void* buffer, size_t bytesToWrite); //throw X                          */

using IoCallback = std::function<void(int64_t bytesDelta)>; //throw X


template <class BinContainer, class Function>
BinContainer unbufferedLoad(Function tryRead/*(void* buffer, size_t bytesToRead) throw X; may return short; only 0 means EOF*/,
                            size_t blockSize); //throw X

template <class BinContainer, class Function>
void unbufferedSave(const BinContainer& cont, Function tryWrite /*(const void* buffer, size_t bytesToWrite) throw X; may return short*/,
                    size_t blockSize); //throw X

template <class Function1, class Function2>
void unbufferedStreamCopy(Function1 tryRead /*(void* buffer, size_t bytesToRead) throw X; may return short; only 0 means EOF*/,  size_t blockSizeIn,
                          Function2 tryWrite /*(const void* buffer, size_t bytesToWrite) throw X; may return short*/,            size_t blockSizeOut); //throw X


template <class N, class BufferedOutputStream> void writeNumber   (BufferedOutputStream& stream, const N& num);                   //
template <class C, class BufferedOutputStream> void writeContainer(BufferedOutputStream& stream, const C& str);                   //noexcept
template <         class BufferedOutputStream> void writeArray    (BufferedOutputStream& stream, const void* buffer, size_t len); //
//----------------------------------------------------------------------
struct SysErrorUnexpectedEos : public SysError
{
    SysErrorUnexpectedEos() : SysError(_("File content is corrupted.") + L" (unexpected end of stream)") {}
};

template <class N, class BufferedInputStream> N    readNumber   (BufferedInputStream& stream); //throw SysErrorUnexpectedEos (corrupted data)
template <class C, class BufferedInputStream> C    readContainer(BufferedInputStream& stream); //
template <         class BufferedInputStream> void readArray    (BufferedInputStream& stream, void* buffer, size_t len); //


struct IOCallbackDivider
{
    IOCallbackDivider(const IoCallback& notifyUnbufferedIO, int64_t& totalBytesNotified) :
        totalBytesNotified_(totalBytesNotified),
        notifyUnbufferedIO_(notifyUnbufferedIO) { assert(totalBytesNotified == 0); }

    void operator()(int64_t bytesDelta) //throw X!
    {
        if (notifyUnbufferedIO_) notifyUnbufferedIO_((totalBytesNotified_ + bytesDelta) / 2 - totalBytesNotified_ / 2); //throw X!
        totalBytesNotified_ += bytesDelta;
    }

private:
    int64_t& totalBytesNotified_;
    const IoCallback& notifyUnbufferedIO_;
};

//-------------------------------------------------------------------------------------

//buffered input/output stream reference implementations:
struct MemoryStreamIn
{
    explicit MemoryStreamIn(const std::string_view& stream) : memRef_(stream) {}

    MemoryStreamIn(std::string&&) = delete; //careful: do NOT store reference to a temporary!

    size_t read(void* buffer, size_t bytesToRead) //return "bytesToRead" bytes unless end of stream!
    {
        const size_t junkSize = std::min(bytesToRead, memRef_.size() - pos_);
        std::memcpy(buffer, memRef_.data() + pos_, junkSize);
        pos_ += junkSize;
        return junkSize;
    }

    size_t pos() const { return pos_; }

private:
    //MemoryStreamIn         (const MemoryStreamIn&) = delete; -> why not allow copying?
    MemoryStreamIn& operator=(const MemoryStreamIn&) = delete;

    const std::string_view memRef_;
    size_t pos_ = 0;
};


struct MemoryStreamOut
{
    MemoryStreamOut() = default;

    void write(const void* buffer, size_t bytesToWrite)
    {
        memBuf_.append(static_cast<const char*>(buffer), bytesToWrite);
    }

    const std::string& ref() const { return memBuf_; }
    /**/  std::string& ref()       { return memBuf_; }

private:
    MemoryStreamOut           (const MemoryStreamOut&) = delete;
    MemoryStreamOut& operator=(const MemoryStreamOut&) = delete;

    std::string memBuf_;
};

//-------------------------------------------------------------------------------------

template <class Function>
struct BufferedInputStream
{
    BufferedInputStream(Function tryRead /*(void* buffer, size_t bytesToRead) throw X; may return short; only 0 means EOF*/,
                        size_t blockSize) :
        tryRead_(tryRead), blockSize_(blockSize) {}

    size_t read(void* buffer, size_t bytesToRead) //throw X; return "bytesToRead" bytes unless end of stream!
    {
        assert(memBuf_.size() >= blockSize_);
        assert(bufPos_ <= bufPosEnd_ && bufPosEnd_ <= memBuf_.size());
        const auto bufStart = buffer;
        for (;;)
        {
            const size_t junkSize = std::min(bytesToRead, bufPosEnd_ - bufPos_);
            std::memcpy(buffer, memBuf_.data() + bufPos_ /*caveat: vector debug checks*/, junkSize);
            bufPos_ += junkSize;
            buffer = static_cast<std::byte*>(buffer) + junkSize;
            bytesToRead -= junkSize;

            if (bytesToRead == 0)
                break;
            //--------------------------------------------------------------------
            const size_t bytesRead = tryRead_(memBuf_.data(), blockSize_); //throw X; may return short, only 0 means EOF! => CONTRACT: bytesToRead > 0
            bufPos_ = 0;
            bufPosEnd_ = bytesRead;

            if (bytesRead == 0) //end of file
                break;
        }
        return static_cast<std::byte*>(buffer) -
               static_cast<std::byte*>(bufStart);
    }

private:
    BufferedInputStream           (const BufferedInputStream&) = delete;
    BufferedInputStream& operator=(const BufferedInputStream&) = delete;

    Function tryRead_;
    const size_t blockSize_;

    size_t bufPos_   = 0;
    size_t bufPosEnd_= 0;
    std::vector<std::byte> memBuf_{blockSize_};
};


template <class Function>
struct BufferedOutputStream
{
    BufferedOutputStream(Function tryWrite /*(const void* buffer, size_t bytesToWrite) throw X; may return short*/,
                         size_t blockSize) :
        tryWrite_(tryWrite), blockSize_(blockSize) {}

    ~BufferedOutputStream()
    {
    }

    void write(const void* buffer, size_t bytesToWrite) //throw X
    {
        assert(memBuf_.size() >= blockSize_);
        assert(bufPos_ <= bufPosEnd_ && bufPosEnd_ <= memBuf_.size());

        for (;;)
        {
            const size_t junkSize = std::min(bytesToWrite, blockSize_ - (bufPosEnd_ - bufPos_));
            std::memcpy(memBuf_.data() + bufPosEnd_, buffer, junkSize);
            bufPosEnd_ += junkSize;
            buffer = static_cast<const std::byte*>(buffer) + junkSize;
            bytesToWrite -= junkSize;

            if (bytesToWrite == 0)
                return;
            //--------------------------------------------------------------------
            bufPos_ += tryWrite_(memBuf_.data() + bufPos_, blockSize_); //throw X; may return short

            if (memBuf_.size() - bufPos_ < blockSize_ || //support memBuf_.size() > blockSize to avoid memmove()s
                bufPos_ == bufPosEnd_)
            {
                std::memmove(memBuf_.data(), memBuf_.data() + bufPos_, bufPosEnd_ - bufPos_);
                bufPosEnd_ -= bufPos_;
                bufPos_ = 0;
            }
        }
    }

    void flushBuffer() //throw X
    {
        assert(bufPosEnd_ - bufPos_ <= blockSize_);
        assert(bufPos_ <= bufPosEnd_ && bufPosEnd_ <= memBuf_.size());
        while (bufPos_ != bufPosEnd_)
            bufPos_ += tryWrite_(memBuf_.data() + bufPos_, bufPosEnd_ - bufPos_); //throw X
    }

private:
    BufferedOutputStream           (const BufferedOutputStream&) = delete;
    BufferedOutputStream& operator=(const BufferedOutputStream&) = delete;

    Function tryWrite_;
    const size_t blockSize_;

    size_t bufPos_    = 0;
    size_t bufPosEnd_ = 0;
    std::vector<std::byte> memBuf_{2 * /*=> mitigate memmove()*/ blockSize_}; //throw FileError
};

//-------------------------------------------------------------------------------------

template <class BinContainer, class Function> inline
BinContainer unbufferedLoad(Function tryRead /*(void* buffer, size_t bytesToRead) throw X; may return short; only 0 means EOF*/,
                            size_t blockSize) //throw X
{
    static_assert(sizeof(typename BinContainer::value_type) == 1); //expect: bytes
    if (blockSize == 0)
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

    BinContainer buf;
    for (;;)
    {
#ifndef ZEN_HAVE_RESIZE_AND_OVERWRITE
#error include legacy_compiler.h!
#endif
#if ZEN_HAVE_RESIZE_AND_OVERWRITE //permature(?) perf optimization; avoid needless zero-initialization:
        size_t bytesRead = 0;
        buf.resize_and_overwrite(buf.size() + blockSize, [&, bufSizeOld = buf.size()](char* rawBuf, size_t /*rawBufSize: caveat: may be larger than what's requested*/)
                                 //permature(?) perf optimization; avoid needless zero-initialization:
        {
            bytesRead = tryRead(rawBuf + bufSizeOld, blockSize); //throw X; may return short; only 0 means EOF
            return bufSizeOld + bytesRead;
        });
#else
        buf.resize(buf.size() + blockSize); //needless zero-initialization!
        const size_t bytesRead = tryRead(buf.data() + buf.size() - blockSize, blockSize); //throw X; may return short; only 0 means EOF
        buf.resize(buf.size() - blockSize + bytesRead); //caveat: unsigned arithmetics
#endif
        if (bytesRead == 0) //end of file
        {
            //caveat: memory consumption of returned string!
            if (buf.capacity() > buf.size() * 3 / 2) //reference: in worst case, std::vector with growth factor 1.5 "wastes" 50% of its size as unused capacity
                buf.shrink_to_fit();                 //=> shrink if buffer is wasting more than that!

            return buf;
        }
    }
}


template <class BinContainer, class Function> inline
void unbufferedSave(const BinContainer& cont,
                    Function tryWrite /*(const void* buffer, size_t bytesToWrite) throw X; may return short*/,
                    size_t blockSize) //throw X
{
    static_assert(sizeof(typename BinContainer::value_type) == 1); //expect: bytes
    if (blockSize == 0)
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

    const size_t bufPosEnd = cont.size();
    size_t bufPos = 0;

    while (bufPos < bufPosEnd)
        bufPos += tryWrite(cont.data() + bufPos, std::min(bufPosEnd - bufPos, blockSize)); //throw X
}


template <class Function1, class Function2> inline
void unbufferedStreamCopy(Function1 tryRead /*(void* buffer, size_t bytesToRead) throw X; may return short; only 0 means EOF*/,
                          size_t blockSizeIn,
                          Function2 tryWrite /*(const void* buffer, size_t bytesToWrite) throw X; may return short*/,
                          size_t blockSizeOut) //throw X
{
    /*  caveat: buffer block sizes might not be a power of 2:
         - f_iosize for network share on macOS
         - libssh2 uses weird packet sizes like MAX_SFTP_OUTGOING_SIZE (30000), and will send incomplete packages if block size is not an exact multiple :(
         - MTP uses file size as blocksize if under 256 kB (=> can be as small as 1 byte! https://freefilesync.org/forum/viewtopic.php?t=9823)
        => that's a problem because we want input/output sizes to be multiples of each other to help avoid the std::memmove() below */
#if 0
    blockSizeIn  = std::bit_ceil(blockSizeIn);
    blockSizeOut = std::bit_ceil(blockSizeOut);
#endif
    if (blockSizeIn == 0 || blockSizeOut == 0)
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

    const size_t bufCapacity = blockSizeOut - 1 + blockSizeIn;
    const size_t alignment = ::sysconf(_SC_PAGESIZE); //-1 on error => posix_memalign() will fail
    assert(alignment >= sizeof(void*) && std::has_single_bit(alignment)); //required by posix_memalign()
    std::byte* buf = nullptr;
    errno = ::posix_memalign(reinterpret_cast<void**>(&buf), alignment, bufCapacity);
    ZEN_ON_SCOPE_EXIT(::free(buf));

    size_t bufPosEnd = 0;
    for (;;)
    {
        const size_t bytesRead = tryRead(buf + bufPosEnd, blockSizeIn); //throw X; may return short; only 0 means EOF

        if (bytesRead == 0) //end of file
        {
            size_t bufPos = 0;
            while (bufPos < bufPosEnd)
                bufPos += tryWrite(buf + bufPos, bufPosEnd - bufPos); //throw X; may return short
            return;
        }
        else
        {
            bufPosEnd += bytesRead;

            size_t bufPos = 0;
            while (bufPosEnd - bufPos >= blockSizeOut)
                bufPos += tryWrite(buf + bufPos, blockSizeOut); //throw X; may return short

            if (bufPos > 0)
            {
                bufPosEnd -= bufPos;
                std::memmove(buf, buf + bufPos, bufPosEnd);
            }
        }
    }
}

//-------------------------------------------------------------------------------------

template <class BufferedOutputStream> inline
void writeArray(BufferedOutputStream& stream, const void* buffer, size_t len)
{
    stream.write(buffer, len);
}


template <class N, class BufferedOutputStream> inline
void writeNumber(BufferedOutputStream& stream, const N& num)
{
    static_assert(isArithmetic<N> || std::is_same_v<N, bool> || std::is_enum_v<N>);
    writeArray(stream, &num, sizeof(N));
}


template <class C, class BufferedOutputStream> inline
void writeContainer(BufferedOutputStream& stream, const C& cont) //don't even consider UTF8 conversions here, we're handling arbitrary binary data!
{
    const auto size = cont.size();

    assert(size <= INT32_MAX);
    writeNumber(stream, static_cast<int32_t>(size)); //use *signed* integer to help catch data corruption

    if (size > 0)
        writeArray(stream, &cont[0], sizeof(typename C::value_type) * size); //don't use c_str(), but access uniformly via STL interface
}


template <class BufferedInputStream> inline
void readArray(BufferedInputStream& stream, void* buffer, size_t len) //throw SysErrorUnexpectedEos
{
    const size_t bytesRead = stream.read(buffer, len);
    assert(bytesRead <= len); //buffer overflow otherwise not always detected!
    if (bytesRead < len)
        throw SysErrorUnexpectedEos();
}


template <class N, class BufferedInputStream> inline
N readNumber(BufferedInputStream& stream) //throw SysErrorUnexpectedEos
{
    static_assert(isArithmetic<N> || std::is_same_v<N, bool> || std::is_enum_v<N>);
    N num; //uninitialized
    readArray(stream, &num, sizeof(N)); //throw SysErrorUnexpectedEos
    return num;
}


template <class C, class BufferedInputStream> inline
C readContainer(BufferedInputStream& stream) //throw SysErrorUnexpectedEos
{
    const auto size = readNumber<int32_t>(stream); //throw SysErrorUnexpectedEos
    if (size < 0) //most likely due to data corruption!
        throw SysErrorUnexpectedEos();

    C cont;
    if (size > 0)
    {
        try
        {
            cont.resize(size); //throw std::length_error, std::bad_alloc
        }
        catch (std::length_error&) { throw SysErrorUnexpectedEos(); } //most likely due to data corruption!
        catch (   std::bad_alloc&) { throw SysErrorUnexpectedEos(); } //

        readArray(stream, &cont[0], sizeof(typename C::value_type) * size); //throw SysErrorUnexpectedEos
    }
    return cont;
}
}

#endif //SERIALIZE_H_839405783574356

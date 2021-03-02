// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SERIALIZE_H_839405783574356
#define SERIALIZE_H_839405783574356

#include <functional>
#include <cstdint>
#include <stdexcept>
#include "string_base.h"
#include "sys_error.h"
//keep header clean from specific stream implementations! (e.g.file_io.h)! used by abstract.h!


namespace zen
{
/* high-performance unformatted serialization (avoiding wxMemoryOutputStream/wxMemoryInputStream inefficiencies)

--------------------------
|Binary Container Concept|
--------------------------
binary container for data storage: must support "basic" std::vector interface (e.g. std::vector<std::byte>, std::string, Zbase<char>)


-------------------------------
|Buffered Input Stream Concept|
-------------------------------
struct BufferedInputStream
{
    size_t read(void* buffer, size_t bytesToRead); //throw X; return "bytesToRead" bytes unless end of stream!

Optional: support stream-copying
--------------------------------
    size_t getBlockSize() const;
    const IoCallback& notifyUnbufferedIO
};

--------------------------------
|Buffered Output Stream Concept|
--------------------------------
struct BufferedOutputStream
{
    void write(const void* buffer, size_t bytesToWrite); //throw X

Optional: support stream-copying
--------------------------------
    const IoCallback& notifyUnbufferedIO
};                                                                           */
using IoCallback = std::function<void(int64_t bytesDelta)>; //throw X


//functions based on buffered stream abstraction
template <class BufferedInputStream, class BufferedOutputStream>
void bufferedStreamCopy(BufferedInputStream& streamIn, BufferedOutputStream& streamOut); //throw X

template <class BinContainer, class BufferedInputStream> BinContainer
bufferedLoad(BufferedInputStream& streamIn); //throw X

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
    IOCallbackDivider(const IoCallback& notifyUnbufferedIO, int64_t& totalUnbufferedIO) : totalUnbufferedIO_(totalUnbufferedIO), notifyUnbufferedIO_(notifyUnbufferedIO) {}

    void operator()(int64_t bytesDelta)
    {
        if (notifyUnbufferedIO_) notifyUnbufferedIO_((totalUnbufferedIO_ - totalUnbufferedIO_ / 2 * 2 + bytesDelta) / 2); //throw X!
        totalUnbufferedIO_ += bytesDelta;
    }

private:
    int64_t& totalUnbufferedIO_;
    const IoCallback& notifyUnbufferedIO_;
};


//buffered input/output stream reference implementations:
template <class BinContainer>
struct MemoryStreamIn
{
    explicit MemoryStreamIn(const BinContainer& cont) : buffer_(cont) {} //this better be cheap!

    size_t read(void* buffer, size_t bytesToRead) //return "bytesToRead" bytes unless end of stream!
    {
        using Byte = typename BinContainer::value_type;
        static_assert(sizeof(Byte) == 1);
        const size_t bytesRead = std::min(bytesToRead, buffer_.size() - pos_);
        auto itFirst = buffer_.begin() + pos_;
        std::copy(itFirst, itFirst + bytesRead, static_cast<Byte*>(buffer));
        pos_ += bytesRead;
        return bytesRead;
    }

    size_t pos() const { return pos_; }

private:
    MemoryStreamIn           (const MemoryStreamIn&) = delete;
    MemoryStreamIn& operator=(const MemoryStreamIn&) = delete;

    const BinContainer buffer_;
    size_t pos_ = 0;
};

template <class BinContainer>
struct MemoryStreamOut
{
    MemoryStreamOut() = default;

    void write(const void* buffer, size_t bytesToWrite)
    {
        using Byte = typename BinContainer::value_type;
        static_assert(sizeof(Byte) == 1);
        buffer_.resize(buffer_.size() + bytesToWrite);
        const auto it = static_cast<const Byte*>(buffer);
        std::copy(it, it + bytesToWrite, buffer_.end() - bytesToWrite);
    }

    const BinContainer& ref() const { return buffer_; }
    /**/  BinContainer& ref()       { return buffer_; }

private:
    MemoryStreamOut           (const MemoryStreamOut&) = delete;
    MemoryStreamOut& operator=(const MemoryStreamOut&) = delete;

    BinContainer buffer_;
};








//-----------------------implementation-------------------------------
template <class BufferedInputStream, class BufferedOutputStream> inline
void bufferedStreamCopy(BufferedInputStream& streamIn,   //throw X
                        BufferedOutputStream& streamOut) //
{
    const size_t blockSize = streamIn.getBlockSize();
    if (blockSize == 0)
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));

    std::vector<std::byte> buffer(blockSize);
    for (;;)
    {
        const size_t bytesRead = streamIn.read(&buffer[0], blockSize); //throw X; return "bytesToRead" bytes unless end of stream!
        streamOut.write(&buffer[0], bytesRead); //throw X

        if (bytesRead < blockSize) //end of file
            break;
    }
}


template <class BinContainer, class BufferedInputStream> inline
BinContainer bufferedLoad(BufferedInputStream& streamIn) //throw X
{
    static_assert(sizeof(typename BinContainer::value_type) == 1); //expect: bytes

    const size_t blockSize = streamIn.getBlockSize();
    if (blockSize == 0)
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));

    BinContainer buffer;
    for (;;)
    {
        buffer.resize(buffer.size() + blockSize);
        const size_t bytesRead = streamIn.read(&*(buffer.end() - blockSize), blockSize); //throw X; return "blockSize" bytes unless end of stream!
        if (bytesRead < blockSize) //end of file
        {
            buffer.resize(buffer.size() - (blockSize - bytesRead)); //caveat: unsigned arithmetics

            //caveat: memory consumption of returned string!
            if (buffer.capacity() > buffer.size() * 3 / 2) //reference: in worst case, std::vector with growth factor 1.5 "wastes" 50% of its size as unused capacity
                buffer.shrink_to_fit();                    //=> shrink if buffer is wasting more than that!

            return buffer;
        }
    }
}


template <class BufferedOutputStream> inline
void writeArray(BufferedOutputStream& stream, const void* buffer, size_t len)
{
    stream.write(buffer, len);
}


template <class N, class BufferedOutputStream> inline
void writeNumber(BufferedOutputStream& stream, const N& num)
{
    static_assert(IsArithmeticV<N> || std::is_same_v<N, bool> || std::is_enum_v<N>);
    writeArray(stream, &num, sizeof(N));
}


template <class C, class BufferedOutputStream> inline
void writeContainer(BufferedOutputStream& stream, const C& cont) //don't even consider UTF8 conversions here, we're handling arbitrary binary data!
{
    const auto len = cont.size();
    writeNumber(stream, static_cast<uint32_t>(len));
    if (len > 0)
        writeArray(stream, &cont[0], sizeof(typename C::value_type) * len); //don't use c_str(), but access uniformly via STL interface
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
    static_assert(IsArithmeticV<N> || std::is_same_v<N, bool> || std::is_enum_v<N>);
    N num{};
    readArray(stream, &num, sizeof(N)); //throw SysErrorUnexpectedEos
    return num;
}


template <class C, class BufferedInputStream> inline
C readContainer(BufferedInputStream& stream) //throw SysErrorUnexpectedEos
{
    C cont;
    auto strLength = readNumber<uint32_t>(stream); //throw SysErrorUnexpectedEos
    if (strLength > 0)
    {
        try
        {
            cont.resize(strLength); //throw std::length_error, std::bad_alloc
        }
        catch (std::length_error&) { throw SysErrorUnexpectedEos(); } //most likely this is due to data corruption!
        catch (   std::bad_alloc&) { throw SysErrorUnexpectedEos(); } //

        readArray(stream, &cont[0], sizeof(typename C::value_type) * strLength); //throw SysErrorUnexpectedEos
    }
    return cont;
}
}

#endif //SERIALIZE_H_839405783574356

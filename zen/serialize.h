// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SERIALIZE_H_839405783574356
#define SERIALIZE_H_839405783574356

#include <functional>
#include <cstdint>
#include "string_base.h"
//keep header clean from specific stream implementations! (e.g.file_io.h)! used by abstract.h!


namespace zen
{
//high-performance unformatted serialization (avoiding wxMemoryOutputStream/wxMemoryInputStream inefficiencies)

/*
--------------------------
|Binary Container Concept|
--------------------------
binary container for data storage: must support "basic" std::vector interface (e.g. std::vector<char>, std::string, Zbase<char>)
*/

//binary container reference implementations
using Utf8String = Zbase<char>; //ref-counted + COW text stream + guaranteed performance: exponential growth
class ByteArray;                //ref-counted       byte stream + guaranteed performance: exponential growth -> no COW, but 12% faster than Utf8String (due to no null-termination?)


class ByteArray //essentially a std::vector<char> with ref-counted semantics, but no COW! => *almost* value type semantics, but not quite
{
public:
    using value_type     = std::vector<char>::value_type;
    using iterator       = std::vector<char>::iterator;
    using const_iterator = std::vector<char>::const_iterator;

    iterator begin() { return buffer_->begin(); }
    iterator end  () { return buffer_->end  (); }

    const_iterator begin() const { return buffer_->begin(); }
    const_iterator end  () const { return buffer_->end  (); }

    void resize(size_t len) { buffer_->resize(len); }
    size_t size() const { return buffer_->size(); }
    bool  empty() const { return buffer_->empty(); }

    inline friend bool operator==(const ByteArray& lhs, const ByteArray& rhs) { return *lhs.buffer_ == *rhs.buffer_; }

private:
    std::shared_ptr<std::vector<char>> buffer_ { std::make_shared<std::vector<char>>() }; //always bound!
    //perf: shared_ptr indirection irrelevant: less than 1% slower!
};

/*
-------------------------------
|Buffered Input Stream Concept|
-------------------------------
struct BufferedInputStream
{
    size_t read(void* buffer, size_t bytesToRead); //throw X; return "bytesToRead" bytes unless end of stream!

Optional: support stream-copying
--------------------------------
    size_t getBlockSize() const;
    const IOCallback& notifyUnbufferedIO
};

--------------------------------
|Buffered Output Stream Concept|
--------------------------------
struct BufferedOutputStream
{
    void write(const void* buffer, size_t bytesToWrite); //throw X

Optional: support stream-copying
--------------------------------
    const IOCallback& notifyUnbufferedIO
};
*/
using IOCallback = std::function<void(int64_t bytesDelta)>; //throw X


//functions based on buffered stream abstraction
template <class BufferedInputStream, class BufferedOutputStream>
void bufferedStreamCopy(BufferedInputStream& streamIn, BufferedOutputStream& streamOut); //throw X

template <class BinContainer, class BufferedInputStream> BinContainer
bufferedLoad(BufferedInputStream& streamIn); //throw X

template <class N, class BufferedOutputStream> void writeNumber   (BufferedOutputStream& stream, const N& num);                   //
template <class C, class BufferedOutputStream> void writeContainer(BufferedOutputStream& stream, const C& str);                   //throw ()
template <         class BufferedOutputStream> void writeArray    (BufferedOutputStream& stream, const void* buffer, size_t len); //
//----------------------------------------------------------------------
class UnexpectedEndOfStreamError {};
template <class N, class BufferedInputStream> N    readNumber   (BufferedInputStream& stream); //throw UnexpectedEndOfStreamError (corrupted data)
template <class C, class BufferedInputStream> C    readContainer(BufferedInputStream& stream); //
template <         class BufferedInputStream> void readArray    (BufferedInputStream& stream, void* buffer, size_t len); //


struct IOCallbackDivider
{
    IOCallbackDivider(const IOCallback& notifyUnbufferedIO, int64_t& totalUnbufferedIO) : totalUnbufferedIO_(totalUnbufferedIO), notifyUnbufferedIO_(notifyUnbufferedIO) {}

    void operator()(int64_t bytesDelta)
    {
        if (notifyUnbufferedIO_) notifyUnbufferedIO_((totalUnbufferedIO_ - totalUnbufferedIO_ / 2 * 2 + bytesDelta) / 2); //throw X!
        totalUnbufferedIO_ += bytesDelta;
    }

private:
    int64_t& totalUnbufferedIO_;
    const IOCallback& notifyUnbufferedIO_;
};

//buffered input/output stream reference implementations:
template <class BinContainer>
struct MemoryStreamIn
{
    MemoryStreamIn(const BinContainer& cont) : buffer_(cont) {} //this better be cheap!

    size_t read(void* buffer, size_t bytesToRead) //return "bytesToRead" bytes unless end of stream!
    {
        static_assert(sizeof(typename BinContainer::value_type) == 1, ""); //expect: bytes
        const size_t bytesRead = std::min(bytesToRead, buffer_.size() - pos_);
        auto itFirst = buffer_.begin() + pos_;
        std::copy(itFirst, itFirst + bytesRead, static_cast<char*>(buffer));
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
        static_assert(sizeof(typename BinContainer::value_type) == 1, ""); //expect: bytes
        buffer_.resize(buffer_.size() + bytesToWrite);
        std::copy(static_cast<const char*>(buffer), static_cast<const char*>(buffer) + bytesToWrite, buffer_.end() - bytesToWrite);
    }

    const BinContainer& ref() const { return buffer_; }

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
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    std::vector<char> buffer(blockSize);
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
    static_assert(sizeof(typename BinContainer::value_type) == 1, ""); //expect: bytes

    const size_t blockSize = streamIn.getBlockSize();
    if (blockSize == 0)
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    BinContainer buffer;
    for (;;)
    {
        buffer.resize(buffer.size() + blockSize);
        const size_t bytesRead = streamIn.read(&*(buffer.end() - blockSize), blockSize); //throw X; return "bytesToRead" bytes unless end of stream!
        buffer.resize(buffer.size() - blockSize + bytesRead); //caveat: unsigned arithmetics

        if (bytesRead < blockSize) //end of file
            return buffer;
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
    static_assert(IsArithmetic<N>::value || IsSameType<N, bool>::value, "not a number!");
    writeArray(stream, &num, sizeof(N));
}


template <class C, class BufferedOutputStream> inline
void writeContainer(BufferedOutputStream& stream, const C& cont) //don't even consider UTF8 conversions here, we're handling arbitrary binary data!
{
    const auto len = cont.size();
    writeNumber(stream, static_cast<uint32_t>(len));
    if (len > 0)
        writeArray(stream, &*cont.begin(), sizeof(typename C::value_type) * len); //don't use c_str(), but access uniformly via STL interface
}


template <class BufferedInputStream> inline
void readArray(BufferedInputStream& stream, void* buffer, size_t len) //throw UnexpectedEndOfStreamError
{
    const size_t bytesRead = stream.read(buffer, len);
    assert(bytesRead <= len); //buffer overflow otherwise not always detected!
    if (bytesRead < len)
        throw UnexpectedEndOfStreamError();
}


template <class N, class BufferedInputStream> inline
N readNumber(BufferedInputStream& stream) //throw UnexpectedEndOfStreamError
{
    static_assert(IsArithmetic<N>::value || IsSameType<N, bool>::value, "");
    N num = 0;
    readArray(stream, &num, sizeof(N)); //throw UnexpectedEndOfStreamError
    return num;
}


template <class C, class BufferedInputStream> inline
C readContainer(BufferedInputStream& stream) //throw UnexpectedEndOfStreamError
{
    C cont;
    auto strLength = readNumber<uint32_t>(stream); //throw UnexpectedEndOfStreamError
    if (strLength > 0)
    {
        try
        {
            cont.resize(strLength); //throw std::bad_alloc
        }
        catch (std::bad_alloc&) //most likely this is due to data corruption!
        {
            throw UnexpectedEndOfStreamError();
        }
        readArray(stream, &*cont.begin(), sizeof(typename C::value_type) * strLength); //throw UnexpectedEndOfStreamError
    }
    return cont;
}
}

#endif //SERIALIZE_H_839405783574356

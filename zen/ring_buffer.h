// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef RING_BUFFER_H_01238467085684139453534
#define RING_BUFFER_H_01238467085684139453534

#include <cassert>
#include "scope_guard.h"


namespace zen
{
//like std::deque<> but with a non-garbage implementation: circular buffer with std::vector<>-like exponential growth!
//https://stackoverflow.com/questions/39324192/why-is-an-stl-deque-not-implemented-as-just-a-circular-vector

template <class T>
class RingBuffer
{
public:
    RingBuffer() {}

    RingBuffer(RingBuffer&& tmp) noexcept : rawMem_(std::move(tmp.rawMem_)), capacity_(tmp.capacity_), bufStart_(tmp.bufStart_), size_(tmp.size_)
    {
        tmp.capacity_ = tmp.bufStart_ = tmp.size_ = 0;
    }
    RingBuffer& operator=(RingBuffer&& tmp) noexcept { swap(tmp); return *this; } //noexcept *required* to support move for reallocations in std::vector and std::swap!!!

    ~RingBuffer() { clear(); }

    using value_type      = T;
    using reference       = T&;
    using const_reference = const T&;

    size_t size    () const { return size_; }
    size_t capacity() const { return capacity_; }
    bool   empty   () const { return size_ == 0; }

    reference       front()       { checkInvariants(); assert(!empty()); return getBufPtr()[bufStart_]; }
    const_reference front() const { checkInvariants(); assert(!empty()); return getBufPtr()[bufStart_]; }

    reference       back()       { checkInvariants(); assert(!empty()); return getBufPtr()[getBufPos(size_ - 1)]; }
    const_reference back() const { checkInvariants(); assert(!empty()); return getBufPtr()[getBufPos(size_ - 1)]; }

    template <class U>
    void push_front(U&& value)
    {
        reserve(size_ + 1); //throw ?
        ::new (getBufPtr() + getBufPos(capacity_ - 1)) T(std::forward<U>(value)); //throw ?
        ++size_;
        bufStart_ = getBufPos(capacity_ - 1);
    }

    template <class U>
    void push_back(U&& value)
    {
        reserve(size_ + 1); //throw ?
        ::new (getBufPtr() + getBufPos(size_)) T(std::forward<U>(value)); //throw ?
        ++size_;
    }

    void pop_front()
    {
        front().~T();
        --size_;

        if (size_ == 0)
            bufStart_ = 0;
        else
            bufStart_ = getBufPos(1);
    }

    void pop_back()
    {
        back().~T();
        --size_;

        if (size_ == 0)
            bufStart_ = 0;
    }

    void clear()
    {
        checkInvariants();

        const size_t frontSize = std::min(size_, capacity_ - bufStart_);

        std::destroy(getBufPtr() + bufStart_, getBufPtr() + bufStart_ + frontSize);
        std::destroy(getBufPtr(), getBufPtr() + size_ - frontSize);
        bufStart_ = size_ = 0;
    }

    template <class Iterator>
    void insert_back(Iterator first, Iterator last) //throw ? (strong exception-safety!)
    {
        const size_t len = last - first;
        reserve(size_ + len); //throw ?

        const size_t endPos = getBufPos(size_);
        const size_t tailSize = std::min(len, capacity_ - endPos);

        std::uninitialized_copy(first, first + tailSize, getBufPtr() + endPos); //throw ?
        ZEN_ON_SCOPE_FAIL(std::destroy(first, first + tailSize));
        std::uninitialized_copy(first + tailSize, last, getBufPtr()); //throw ?

        size_ += len;
    }

    //contract: last - first <= size()
    template <class Iterator>
    void extract_front(Iterator first, Iterator last) //throw ? strongly exception-safe! (but only basic exception safety for [first, last) range)
    {
        checkInvariants();
        const size_t len = last - first;
        assert(size_ >= len);

        const size_t frontSize = std::min(len, capacity_ - bufStart_);

        auto itTrg = std::copy(getBufPtr() + bufStart_, getBufPtr() + bufStart_ + frontSize, first); //throw ?
        /**/         std::copy(getBufPtr(), getBufPtr() + len - frontSize, itTrg);                   //

        std::destroy(getBufPtr() + bufStart_, getBufPtr() + bufStart_ + frontSize);
        std::destroy(getBufPtr(), getBufPtr() + len - frontSize);

        size_ -= len;

        if (size_ == 0)
            bufStart_ = 0;
        else
            bufStart_ = getBufPos(len);
    }

    void swap(RingBuffer& other)
    {
        std::swap(rawMem_,   other.rawMem_);
        std::swap(capacity_, other.capacity_);
        std::swap(bufStart_, other.bufStart_);
        std::swap(size_,     other.size_);
    }

    void reserve(size_t minCapacity) //throw ? (strong exception-safety!)
    {
        checkInvariants();

        if (minCapacity > capacity_)
        {
            const size_t newCapacity = std::max(minCapacity + minCapacity / 2, minCapacity); //no lower limit for capacity: just like std::vector<>

            RingBuffer newBuf(newCapacity); //throw ?

            T* itTrg = reinterpret_cast<T*>(newBuf.rawMem_.get());

            const size_t frontSize = std::min(size_, capacity_ - bufStart_);

            itTrg = uninitializedMoveIfNoexcept(getBufPtr() + bufStart_, getBufPtr() + bufStart_ + frontSize, itTrg); //throw ?
            newBuf.size_ = frontSize; //pass ownership
            /**/    uninitializedMoveIfNoexcept(getBufPtr(), getBufPtr() + size_ - frontSize, itTrg); //throw ?
            newBuf.size_ = size_;     //

            newBuf.swap(*this);
        }
    }

    const T& operator[](size_t offset) const
    {
        assert(offset < size()); //design by contract! no runtime check!
        return getBufPtr()[getBufPos(offset)];
    }

    T& operator[](size_t offset) { return const_cast<T&>(static_cast<const RingBuffer*>(this)->operator[](offset)); }

    template <class Container, class Value>
    class Iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = Value;
        using difference_type = ptrdiff_t;
        using pointer   = Value*;
        using reference = Value&;

        Iterator(Container& container, size_t offset) : container_(&container), offset_(offset) {}
        Iterator& operator++() { ++offset_; return *this; }
        Iterator& operator--() { --offset_; return *this; }
        Iterator& operator+=(ptrdiff_t offset) { offset_ += offset; return *this; }
        Value& operator* () const { return  (*container_)[offset_]; }
        Value* operator->() const { return &(*container_)[offset_]; }
        inline friend Iterator operator+(const Iterator& lhs, ptrdiff_t offset) { Iterator tmp(lhs); return tmp += offset; }
        inline friend ptrdiff_t operator-(const Iterator& lhs, const Iterator& rhs) { return lhs.offset_ - rhs.offset_; }
        inline friend bool operator==(const Iterator& lhs, const Iterator& rhs) { assert(lhs.container_ == rhs.container_); return lhs.offset_ == rhs.offset_; }
        inline friend std::strong_ordering operator<=>(const Iterator& lhs, const Iterator& rhs) { assert(lhs.container_ == rhs.container_); return lhs.offset_ <=> rhs.offset_; }
        //GCC debug needs "operator<="
    private:
        Container* container_ = nullptr; //iterator must be assignable
        ptrdiff_t offset_ = 0;
    };

    using iterator        = Iterator<      RingBuffer,       T>;
    using const_iterator  = Iterator<const RingBuffer, const T>;

    iterator begin() { return {*this, 0    }; }
    iterator end  () { return {*this, size_}; }

    const_iterator begin() const { return {*this, 0    }; }
    const_iterator end  () const { return {*this, size_}; }

    const_iterator cbegin() const { return begin(); }
    const_iterator cend  () const { return end  (); }

private:
    RingBuffer           (const RingBuffer&) = delete; //wait until there is a reason to copy a RingBuffer
    RingBuffer& operator=(const RingBuffer&) = delete; //

    explicit RingBuffer(size_t capacity) :
        rawMem_(static_cast<std::byte*>(::operator new (capacity * sizeof(T)))), //throw std::bad_alloc
        capacity_(capacity) {}

    /**/  T* getBufPtr()       { return reinterpret_cast<T*>(rawMem_.get()); }
    const T* getBufPtr() const { return reinterpret_cast<T*>(rawMem_.get()); }

    //unlike pure std::uninitialized_move, this one allows for strong exception-safety!
    static T* uninitializedMoveIfNoexcept(T* first, T* last, T* firstTrg)
    {
        return uninitializedMoveIfNoexcept(first, last, firstTrg, std::is_nothrow_move_constructible<T>());
    }
    static T* uninitializedMoveIfNoexcept(T* first, T* last, T* firstTrg, std::true_type ) { return std::uninitialized_move(first, last, firstTrg); }
    static T* uninitializedMoveIfNoexcept(T* first, T* last, T* firstTrg, std::false_type) { return std::uninitialized_copy(first, last, firstTrg); } //throw ?

    size_t getBufPos(size_t offset) const
    {
        //assert(offset < capacity_); -> redundant in this context
        size_t bufPos = bufStart_ + offset;
        if (bufPos >= capacity_)
            bufPos -= capacity_;
        return bufPos;
    }

    void checkInvariants() const
    {
        assert(bufStart_ == 0 || bufStart_ < capacity_);
        assert(size_ <= capacity_);
    }

    struct FreeStoreDelete { void operator()(std::byte* p) const { ::operator delete (p); } };

    std::unique_ptr<std::byte, FreeStoreDelete> rawMem_;
    size_t capacity_  = 0; //as number of T
    size_t bufStart_  = 0; //<  capacity_
    size_t size_      = 0; //<= capacity_
};
}

#endif //RING_BUFFER_H_01238467085684139453534

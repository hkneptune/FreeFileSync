// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STRING_BASE_H_083217454562342526
#define STRING_BASE_H_083217454562342526

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <atomic>
#include "string_tools.h"


//Zbase - a policy based string class optimizing performance and flexibility
namespace zen
{
/*
Allocator Policy:
-----------------
    void* allocate(size_t size) //throw std::bad_alloc
    void deallocate(void* ptr)
    size_t calcCapacity(size_t length)
*/
class AllocatorOptimalSpeed //exponential growth + min size
{
protected:
    //::operator new/ ::operator delete show same performance characterisics like malloc()/free()!
    static void* allocate(size_t size) { return ::malloc(size); } //throw std::bad_alloc
    static void  deallocate(void* ptr) { ::free(ptr); }
    static size_t calcCapacity(size_t length) { return std::max<size_t>(16, std::max(length + length / 2, length)); }
    //- size_t might overflow! => better catch here than return a too small size covering up the real error: a way too large length!
    //- any growth rate should not exceed golden ratio: 1.618033989
};


class AllocatorOptimalMemory //no wasted memory, but more reallocations required when manipulating string
{
protected:
    static void* allocate(size_t size) { return ::malloc(size); } //throw std::bad_alloc
    static void  deallocate(void* ptr) { ::free(ptr); }
    static size_t calcCapacity(size_t length) { return length; }
};

/*
Storage Policy:
---------------
template <typename Char, //Character Type
         class AP>       //Allocator Policy

    Char* create(size_t size)
    Char* create(size_t size, size_t minCapacity)
    Char* clone(Char* ptr)
    void destroy(Char* ptr) //must handle "destroy(nullptr)"!
    bool canWrite(const Char* ptr, size_t minCapacity) //needs to be checked before writing to "ptr"
    size_t length(const Char* ptr)
    void setLength(Char* ptr, size_t newLength)
*/

template <class Char, //Character Type
          class AP>   //Allocator Policy
class StorageDeepCopy : public AP
{
protected:
    ~StorageDeepCopy() {}

    Char* create(size_t size) { return create(size, size); }
    Char* create(size_t size, size_t minCapacity)
    {
        assert(size <= minCapacity);
        const size_t newCapacity = AP::calcCapacity(minCapacity);
        assert(newCapacity >= minCapacity);

        Descriptor* const newDescr = static_cast<Descriptor*>(this->allocate(sizeof(Descriptor) + (newCapacity + 1) * sizeof(Char))); //throw std::bad_alloc
        new (newDescr) Descriptor(size, newCapacity);

        return reinterpret_cast<Char*>(newDescr + 1); //alignment note: "newDescr + 1" is Descriptor-aligned, which is larger than alignment for Char-array! => no problem!
    }

    Char* clone(Char* ptr)
    {
        const size_t len = length(ptr);
        Char* newData = create(len); //throw std::bad_alloc
        std::copy(ptr, ptr + len + 1, newData);
        return newData;
    }

    void destroy(Char* ptr)
    {
        if (!ptr) return; //support "destroy(nullptr)"

        Descriptor* const d = descr(ptr);
        d->~Descriptor();
        this->deallocate(d);
    }

    //this needs to be checked before writing to "ptr"
    static bool canWrite(const Char* ptr, size_t minCapacity) { return minCapacity <= descr(ptr)->capacity; }
    static size_t length(const Char* ptr) { return descr(ptr)->length; }

    static void setLength(Char* ptr, size_t newLength)
    {
        assert(canWrite(ptr, newLength));
        descr(ptr)->length = newLength;
    }

private:
    struct Descriptor
    {
        Descriptor(size_t len, size_t cap) :
            length  (static_cast<uint32_t>(len)),
            capacity(static_cast<uint32_t>(cap)) {}

        uint32_t length;
        uint32_t capacity; //allocated size without null-termination
    };

    static       Descriptor* descr(      Char* ptr) { return reinterpret_cast<      Descriptor*>(ptr) - 1; }
    static const Descriptor* descr(const Char* ptr) { return reinterpret_cast<const Descriptor*>(ptr) - 1; }
};


template <class Char, //Character Type
          class AP>   //Allocator Policy
class StorageRefCountThreadSafe : public AP
{
protected:
    ~StorageRefCountThreadSafe() {}

    Char* create(size_t size) { return create(size, size); }
    Char* create(size_t size, size_t minCapacity)
    {
        assert(size <= minCapacity);

        const size_t newCapacity = AP::calcCapacity(minCapacity);
        assert(newCapacity >= minCapacity);

        Descriptor* const newDescr = static_cast<Descriptor*>(this->allocate(sizeof(Descriptor) + (newCapacity + 1) * sizeof(Char))); //throw std::bad_alloc
        new (newDescr) Descriptor(size, newCapacity);

        return reinterpret_cast<Char*>(newDescr + 1);
    }

    static Char* clone(Char* ptr)
    {
        ++descr(ptr)->refCount;
        return ptr;
    }

    void destroy(Char* ptr)
    {
        assert(ptr != reinterpret_cast<Char*>(0x1)); //detect double-deletion

        if (!ptr) //support "destroy(nullptr)"
        {
            return;
        }

        Descriptor* const d = descr(ptr);

        if (--(d->refCount) == 0) //operator--() is overloaded to decrement and evaluate in a single atomic operation!
        {
            d->~Descriptor();
            this->deallocate(d);
        }
    }

    static bool canWrite(const Char* ptr, size_t minCapacity) //needs to be checked before writing to "ptr"
    {
        const Descriptor* const d = descr(ptr);
        assert(d->refCount > 0);
        return d->refCount == 1 && minCapacity <= d->capacity;
    }

    static size_t length(const Char* ptr) { return descr(ptr)->length; }

    static void setLength(Char* ptr, size_t newLength)
    {
        assert(canWrite(ptr, newLength));
        descr(ptr)->length = static_cast<uint32_t>(newLength);
    }

private:
    struct Descriptor
    {
        Descriptor(size_t len, size_t cap) :
            length  (static_cast<uint32_t>(len)),
            capacity(static_cast<uint32_t>(cap)) { static_assert(ATOMIC_INT_LOCK_FREE == 2, ""); } //2: "The atomic type is always lock-free"

        std::atomic<unsigned int> refCount { 1 }; //std:atomic is uninitialized by default!
        uint32_t length;
        uint32_t capacity; //allocated size without null-termination
    };

    static       Descriptor* descr(      Char* ptr) { return reinterpret_cast<      Descriptor*>(ptr) - 1; }
    static const Descriptor* descr(const Char* ptr) { return reinterpret_cast<const Descriptor*>(ptr) - 1; }
};


template <class Char>
using DefaultStoragePolicy = StorageRefCountThreadSafe<Char, AllocatorOptimalSpeed>;


//################################################################################################################################################################

//perf note: interestingly StorageDeepCopy and StorageRefCountThreadSafe show same performance in FFS comparison

template <class Char,                                       //Character Type
          template <class> class SP = DefaultStoragePolicy> //Storage Policy
class Zbase : public SP<Char>
{
public:
    Zbase();
    Zbase(const Char* str) : Zbase(str, str + strLength(str)) {} //implicit conversion from a C-string!
    Zbase(const Char* str, size_t len) : Zbase(str, str + len) {}
    Zbase(const Zbase& str);
    Zbase(Zbase&& tmp) noexcept;
    template <class InputIterator>
    Zbase(InputIterator first, InputIterator last);
    //explicit Zbase(Char ch); //dangerous if implicit: Char buffer[]; return buffer[0]; ups... forgot &, but not a compiler error! //-> non-standard extension!!!

    ~Zbase();

    //operator const Char* () const; //NO implicit conversion to a C-string!! Many problems... one of them: if we forget to provide operator overloads, it'll just work with a Char*...

    //STL accessors
    using iterator        = Char*;
    using const_iterator  = const Char*;
    using reference       = Char&;
    using const_reference = const Char&;
    using value_type      = Char;

    iterator begin();
    iterator end  ();
    const_iterator begin () const { return rawStr_; }
    const_iterator end   () const { return rawStr_ + length(); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend  () const { return end  (); }

    //std::string functions
    size_t length() const;
    size_t size  () const { return length(); }
    const Char* c_str() const { return rawStr_; } //C-string format with 0-termination
    const Char operator[](size_t pos) const;
    bool empty() const { return length() == 0; }
    void clear();
    size_t find (const Zbase& str, size_t pos = 0)    const; //
    size_t find (const Char* str,  size_t pos = 0)    const; //
    size_t find (Char  ch,         size_t pos = 0)    const; //returns "npos" if not found
    size_t rfind(Char  ch,         size_t pos = npos) const; //
    size_t rfind(const Char* str,  size_t pos = npos) const; //
    //Zbase& replace(size_t pos1, size_t n1, const Zbase& str);
    void reserve(size_t minCapacity);
    Zbase& assign(const Char* str, size_t len) { return assign(str, str + len); }
    Zbase& append(const Char* str, size_t len) { return append(str, str + len); }

    template <class InputIterator> Zbase& assign(InputIterator first, InputIterator last);
    template <class InputIterator> Zbase& append(InputIterator first, InputIterator last);

    void resize(size_t newSize, Char fillChar = 0);
    void swap(Zbase& str) { std::swap(rawStr_, str.rawStr_); }
    void push_back(Char val) { operator+=(val); } //STL access
    void pop_back();

    Zbase& operator=(Zbase&& tmp) noexcept;
    Zbase& operator=(const Zbase& str);
    Zbase& operator=(const Char* str)   { return assign(str, strLength(str)); }
    Zbase& operator=(Char ch)           { return assign(&ch, 1); }
    Zbase& operator+=(const Zbase& str) { return append(str.c_str(), str.length()); }
    Zbase& operator+=(const Char* str)  { return append(str, strLength(str)); }
    Zbase& operator+=(Char ch)          { return append(&ch, 1); }

    static const size_t npos = static_cast<size_t>(-1);

private:
    Zbase            (int) = delete; //
    Zbase& operator= (int) = delete; //detect usage errors by creating an intentional ambiguity with "Char"
    Zbase& operator+=(int) = delete; //
    void   push_back (int) = delete; //

    Char* rawStr_;
};

template <class Char, template <class> class SP>        bool operator==(const Zbase<Char, SP>& lhs, const Zbase<Char, SP>& rhs);
template <class Char, template <class> class SP>        bool operator==(const Zbase<Char, SP>& lhs, const Char*            rhs);
template <class Char, template <class> class SP> inline bool operator==(const Char*            lhs, const Zbase<Char, SP>& rhs) { return operator==(rhs, lhs); }

template <class Char, template <class> class SP> inline bool operator!=(const Zbase<Char, SP>& lhs, const Zbase<Char, SP>& rhs) { return !operator==(lhs, rhs); }
template <class Char, template <class> class SP> inline bool operator!=(const Zbase<Char, SP>& lhs, const Char*            rhs) { return !operator==(lhs, rhs); }
template <class Char, template <class> class SP> inline bool operator!=(const Char*            lhs, const Zbase<Char, SP>& rhs) { return !operator==(lhs, rhs); }

template <class Char, template <class> class SP> bool operator<(const Zbase<Char, SP>& lhs, const Zbase<Char, SP>& rhs);
template <class Char, template <class> class SP> bool operator<(const Zbase<Char, SP>& lhs, const Char*            rhs);
template <class Char, template <class> class SP> bool operator<(const Char*            lhs, const Zbase<Char, SP>& rhs);

template <class Char, template <class> class SP> inline Zbase<Char, SP> operator+(const Zbase<Char, SP>& lhs, const Zbase<Char, SP>& rhs) { return Zbase<Char, SP>(lhs) += rhs; }
template <class Char, template <class> class SP> inline Zbase<Char, SP> operator+(const Zbase<Char, SP>& lhs, const Char*            rhs) { return Zbase<Char, SP>(lhs) += rhs; }
template <class Char, template <class> class SP> inline Zbase<Char, SP> operator+(const Zbase<Char, SP>& lhs,       Char             rhs) { return Zbase<Char, SP>(lhs) += rhs; }

//don't use unified first argument but save one move-construction in the r-value case instead!
template <class Char, template <class> class SP> inline Zbase<Char, SP> operator+(Zbase<Char, SP>&& lhs, const Zbase<Char, SP>& rhs) { return std::move(lhs += rhs); } //the move *is* needed!!!
template <class Char, template <class> class SP> inline Zbase<Char, SP> operator+(Zbase<Char, SP>&& lhs, const Char*            rhs) { return std::move(lhs += rhs); } //lhs, is an l-value parameter...
template <class Char, template <class> class SP> inline Zbase<Char, SP> operator+(Zbase<Char, SP>&& lhs,       Char             rhs) { return std::move(lhs += rhs); } //and not a local variable => no copy elision

template <class Char, template <class> class SP> inline Zbase<Char, SP> operator+(      Char        lhs, const Zbase<Char, SP>& rhs) { return Zbase<Char, SP>(&lhs, 1) += rhs; }
template <class Char, template <class> class SP> inline Zbase<Char, SP> operator+(const Char*       lhs, const Zbase<Char, SP>& rhs) { return Zbase<Char, SP>(lhs    ) += rhs; }













//################################# implementation ########################################
template <class Char, template <class> class SP> inline
Zbase<Char, SP>::Zbase()
{
    //resist the temptation to avoid this allocation by referening a static global: NO performance advantage, MT issues!
    rawStr_    = this->create(0);
    rawStr_[0] = 0;
}


template <class Char, template <class> class SP>
template <class InputIterator> inline
Zbase<Char, SP>::Zbase(InputIterator first, InputIterator last)
{
    rawStr_ = this->create(std::distance(first, last));
    *std::copy(first, last, rawStr_) = 0;
}


template <class Char, template <class> class SP> inline
Zbase<Char, SP>::Zbase(const Zbase<Char, SP>& str)
{
    rawStr_ = this->clone(str.rawStr_);
}


template <class Char, template <class> class SP> inline
Zbase<Char, SP>::Zbase(Zbase<Char, SP>&& tmp) noexcept
{
    rawStr_ = tmp.rawStr_;
    tmp.rawStr_ = nullptr; //usually nullptr would violate the class invarants, but it is good enough for the destructor!
    //caveat: do not increment ref-count of an unshared string! We'd lose optimization opportunity of reusing its memory!
}


template <class Char, template <class> class SP> inline
Zbase<Char, SP>::~Zbase()
{
    static_assert(noexcept(this->~Zbase()), ""); //has exception spec of compiler-generated destructor by default

    this->destroy(rawStr_); //rawStr_ may be nullptr; see move constructor!
}


template <class Char, template <class> class SP> inline
size_t Zbase<Char, SP>::find(const Zbase& str, size_t pos) const
{
    assert(pos <= length());
    const size_t len = length();
    const Char* thisEnd = begin() + len; //respect embedded 0
    const Char* it = std::search(begin() + std::min(pos, len), thisEnd,
                                 str.begin(), str.end());
    return it == thisEnd ? npos : it - begin();
}


template <class Char, template <class> class SP> inline
size_t Zbase<Char, SP>::find(const Char* str, size_t pos) const
{
    assert(pos <= length());
    const size_t len = length();
    const Char* thisEnd = begin() + len; //respect embedded 0
    const Char* it = std::search(begin() + std::min(pos, len), thisEnd,
                                 str, str + strLength(str));
    return it == thisEnd ? npos : it - begin();
}


template <class Char, template <class> class SP> inline
size_t Zbase<Char, SP>::find(Char ch, size_t pos) const
{
    assert(pos <= length());
    const size_t len = length();
    const Char* thisEnd = begin() + len; //respect embedded 0
    const Char* it = std::find(begin() + std::min(pos, len), thisEnd, ch);
    return it == thisEnd ? npos : it - begin();
}


template <class Char, template <class> class SP> inline
size_t Zbase<Char, SP>::rfind(Char ch, size_t pos) const
{
    assert(pos == npos || pos <= length());
    const size_t len = length();
    const Char* currEnd = begin() + (pos == npos ? len : std::min(pos + 1, len));
    const Char* it = find_last(begin(), currEnd, ch);
    return it == currEnd ? npos : it - begin();
}


template <class Char, template <class> class SP> inline
size_t Zbase<Char, SP>::rfind(const Char* str, size_t pos) const
{
    assert(pos == npos || pos <= length());
    const size_t strLen = strLength(str);
    const size_t len = length();
    const Char* currEnd = begin() + (pos == npos ? len : std::min(pos + strLen, len));
    const Char* it = search_last(begin(), currEnd,
                                 str, str + strLen);
    return it == currEnd ? npos : it - begin();
}


template <class Char, template <class> class SP> inline
void Zbase<Char, SP>::resize(size_t newSize, Char fillChar)
{
    const size_t oldSize = length();
    if (this->canWrite(rawStr_, newSize))
    {
        if (oldSize < newSize)
            std::fill(rawStr_ + oldSize, rawStr_ + newSize, fillChar);
        rawStr_[newSize] = 0;
        this->setLength(rawStr_, newSize);
    }
    else
    {
        Char* newStr = this->create(newSize);
        if (oldSize < newSize)
        {
            std::copy(rawStr_, rawStr_ + oldSize, newStr);
            std::fill(newStr + oldSize, newStr + newSize, fillChar);
        }
        else
            std::copy(rawStr_, rawStr_ + newSize, newStr);
        newStr[newSize] = 0;

        this->destroy(rawStr_);
        rawStr_ = newStr;
    }
}


template <class Char, template <class> class SP> inline
bool operator==(const Zbase<Char, SP>& lhs, const Zbase<Char, SP>& rhs)
{
    return lhs.length() == rhs.length() && std::equal(lhs.begin(), lhs.end(), rhs.begin()); //respect embedded 0
}


template <class Char, template <class> class SP> inline
bool operator==(const Zbase<Char, SP>& lhs, const Char* rhs)
{
    return lhs.length() == strLength(rhs) && std::equal(lhs.begin(), lhs.end(), rhs); //respect embedded 0
}


template <class Char, template <class> class SP> inline
bool operator<(const Zbase<Char, SP>& lhs, const Zbase<Char, SP>& rhs)
{
    return std::lexicographical_compare(lhs.begin(), lhs.end(), //respect embedded 0
                                        rhs.begin(), rhs.end());
}


template <class Char, template <class> class SP> inline
bool operator<(const Zbase<Char, SP>& lhs, const Char* rhs)
{
    return std::lexicographical_compare(lhs.begin(), lhs.end(), //respect embedded 0
                                        rhs, rhs + strLength(rhs));
}


template <class Char, template <class> class SP> inline
bool operator<(const Char* lhs, const Zbase<Char, SP>& rhs)
{
    return std::lexicographical_compare(lhs, lhs + strLength(lhs), //respect embedded 0
                                        rhs.begin(), rhs.end());
}


template <class Char, template <class> class SP> inline
size_t Zbase<Char, SP>::length() const
{
    return SP<Char>::length(rawStr_);
}


template <class Char, template <class> class SP> inline
const Char Zbase<Char, SP>::operator[](size_t pos) const
{
    assert(pos < length()); //design by contract! no runtime check!
    return rawStr_[pos];
}


template <class Char, template <class> class SP> inline
auto Zbase<Char, SP>::begin() -> iterator
{
    reserve(length()); //make unshared!
    return rawStr_;
}


template <class Char, template <class> class SP> inline
auto Zbase<Char, SP>::end() -> iterator
{
    return begin() + length();
}


template <class Char, template <class> class SP> inline
void Zbase<Char, SP>::clear()
{
    if (!empty())
    {
        if (this->canWrite(rawStr_, 0))
        {
            rawStr_[0] = 0;              //keep allocated memory
            this->setLength(rawStr_, 0); //
        }
        else
            *this = Zbase();
    }
}


template <class Char, template <class> class SP> inline
void Zbase<Char, SP>::reserve(size_t minCapacity) //make unshared and check capacity
{
    if (!this->canWrite(rawStr_, minCapacity))
    {
        //allocate a new string
        const size_t len = length();
        Char* newStr = this->create(len, std::max(len, minCapacity)); //reserve() must NEVER shrink the string: logical const!
        std::copy(rawStr_, rawStr_ + len + 1, newStr); //include 0-termination

        this->destroy(rawStr_);
        rawStr_ = newStr;
    }
}


template <class Char, template <class> class SP>
template <class InputIterator> inline
Zbase<Char, SP>& Zbase<Char, SP>::assign(InputIterator first, InputIterator last)
{
    const size_t len = std::distance(first, last);
    if (this->canWrite(rawStr_, len))
    {
        *std::copy(first, last, rawStr_) = 0;
        this->setLength(rawStr_, len);
    }
    else
        *this = Zbase(first, last);

    return *this;
}


template <class Char, template <class> class SP>
template <class InputIterator> inline
Zbase<Char, SP>& Zbase<Char, SP>::append(InputIterator first, InputIterator last)
{
    const size_t len = std::distance(first, last);
    if (len > 0) //avoid making this string unshared for no reason
    {
        const size_t thisLen = length();
        reserve(thisLen + len); //make unshared and check capacity

        *std::copy(first, last, rawStr_ + thisLen) = 0;
        this->setLength(rawStr_, thisLen + len);
    }
    return *this;
}


template <class Char, template <class> class SP> inline
Zbase<Char, SP>& Zbase<Char, SP>::operator=(const Zbase<Char, SP>& str)
{
    Zbase<Char, SP>(str).swap(*this);
    return *this;
}


template <class Char, template <class> class SP> inline
Zbase<Char, SP>& Zbase<Char, SP>::operator=(Zbase<Char, SP>&& tmp) noexcept
{
    swap(tmp); //don't use unifying assignment but save one move-construction in the r-value case instead!
    return *this;
}

template <class Char, template <class> class SP> inline
void Zbase<Char, SP>::pop_back()
{
    const size_t len = length();
    assert(len > 0);
    if (len > 0)
        resize(len - 1);
}
}

#endif //STRING_BASE_H_083217454562342526

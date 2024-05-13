// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STL_TOOLS_H_84567184321434
#define STL_TOOLS_H_84567184321434

#include <set>
#include <map>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <cassert>
//#include <algorithm>
#include <optional>
#include "type_traits.h"


//enhancements for <algorithm>
namespace zen
{
//unfortunately std::erase_if is useless garbage on GCC 12 (requires non-modifying predicate)
template <class T, class Alloc, class Predicate>
void eraseIf(std::vector<T, Alloc>& v, Predicate p);

template <class T, class LessType, class Alloc, class Predicate>
void eraseIf(std::set<T, LessType, Alloc>& s, Predicate p);

template <class KeyType, class ValueType, class LessType, class Alloc, class Predicate>
void eraseIf(std::map<KeyType, ValueType, LessType, Alloc>& m, Predicate p);

//append STL containers
template <class T, class Alloc, class C>
void append(std::vector<T, Alloc>& v, const C& c);

template <class T, class LessType, class Alloc, class C>
void append(std::set<T, LessType, Alloc>& s, const C& c);

template <class KeyType, class ValueType, class LessType, class Alloc, class C>
void append(std::map<KeyType, ValueType, LessType, Alloc>& m, const C& c);

template <class T, class Alloc>
void removeDuplicates(std::vector<T, Alloc>& v);

template <class T, class Alloc, class CompLess>
void removeDuplicates(std::vector<T, Alloc>& v, CompLess less);

template <class T, class Alloc, class CompLess>
void removeDuplicatesStable(std::vector<T, Alloc>& v, CompLess less);

template <class T, class Alloc>
void removeDuplicatesStable(std::vector<T, Alloc>& v);

//searching STL containers
template <class BidirectionalIterator, class T>
BidirectionalIterator findLast(BidirectionalIterator first, BidirectionalIterator last, const T& value);

template <class RandomAccessIterator1, class RandomAccessIterator2> inline
RandomAccessIterator1 searchFirst(const RandomAccessIterator1 first,       const RandomAccessIterator1 last,
                                  const RandomAccessIterator2 needleFirst, const RandomAccessIterator2 needleLast);

template <class RandomAccessIterator1, class RandomAccessIterator2, class IsEq> inline
RandomAccessIterator1 searchFirst(const RandomAccessIterator1 first,       const RandomAccessIterator1 last,
                                  const RandomAccessIterator2 needleFirst, const RandomAccessIterator2 needleLast, IsEq isEqual);

//replacement for std::find_end taking advantage of bidirectional iterators (and giving the algorithm a reasonable name)
template <class RandomAccessIterator1, class RandomAccessIterator2>
RandomAccessIterator1 searchLast(RandomAccessIterator1 first, RandomAccessIterator1 last,
                                 RandomAccessIterator2 needleFirst, RandomAccessIterator2 needleLast);

//binary search returning an iterator
template <class RandomAccessIterator, class T, class CompLess>
RandomAccessIterator binarySearch(RandomAccessIterator first, RandomAccessIterator last, const T& value, CompLess less);

//read-only variant of std::merge; input: two sorted ranges
template <class Iterator, class FunctionLeftOnly, class FunctionBoth, class FunctionRightOnly, class Compare>
void mergeTraversal(Iterator first1, Iterator last1,
                    Iterator first2, Iterator last2,
                    FunctionLeftOnly lo, FunctionBoth bo, FunctionRightOnly ro, Compare compare);

//why, oh why is there no std::optional<T>::get()???
template <class T> inline       T* get(      std::optional<T>& opt) { return opt ? &*opt : nullptr; }
template <class T> inline const T* get(const std::optional<T>& opt) { return opt ? &*opt : nullptr; }


//===========================================================================
template <class T>
class SharedRef //why is there no std::shared_ref???
{
public:
    SharedRef() = delete; //no surprise memory allocations!

    explicit SharedRef(std::shared_ptr<T> ptr) : ref_(std::move(ptr)) { assert(ref_); }

    template <class U>
    SharedRef(const SharedRef<U>& other) : ref_(other.ref_) {}

    /**/  T& ref()       { return *ref_; };
    const T& ref() const { return *ref_; };

    std::shared_ptr<      T> ptr()       { return ref_; };
    std::shared_ptr<const T> ptr() const { return ref_; };

private:
    template <class U> friend class SharedRef;

    std::shared_ptr<T> ref_; //always bound
};

template <class T, class... Args> inline
SharedRef<T> makeSharedRef(Args&& ... args) { return SharedRef<T>(std::make_shared<T>(std::forward<Args>(args)...)); }






//######################## implementation ########################

template <class T, class Alloc, class Predicate> inline
void eraseIf(std::vector<T, Alloc>& v, Predicate p)
{
    v.erase(std::remove_if(v.begin(), v.end(), p), v.end());
}


namespace impl
{
template <class S, class Predicate> inline
void setOrMapEraseIf(S& s, Predicate p)
{
    for (auto it = s.begin(); it != s.end();)
        if (p(*it))
            s.erase(it++);
        else
            ++it;
}
}


template <class T, class LessType, class Alloc, class Predicate> inline
void eraseIf(std::set<T, LessType, Alloc>& s, Predicate p) { impl::setOrMapEraseIf(s, p); } //don't make this any more generic! e.g. must not compile for std::vector!!!


template <class KeyType, class ValueType, class LessType, class Alloc, class Predicate> inline
void eraseIf(std::map<KeyType, ValueType, LessType, Alloc>& m, Predicate p) { impl::setOrMapEraseIf(m, p); }


template <class T, class Hash, class Keyeq, class Alloc, class Predicate> inline
void eraseIf(std::unordered_set<T, Hash, Keyeq, Alloc>& s, Predicate p) { impl::setOrMapEraseIf(s, p); }


template <class KeyType, class ValueType, class Hash, class Keyeq, class Alloc, class Predicate> inline
void eraseIf(std::unordered_map<KeyType, ValueType, Hash, Keyeq, Alloc>& m, Predicate p) { impl::setOrMapEraseIf(m, p); }


template <class T, class Alloc, class C> inline
void append(std::vector<T, Alloc>& v, const C& c) { v.insert(v.end(), c.begin(), c.end()); }


template <class T, class LessType, class Alloc, class C> inline
void append(std::set<T, LessType, Alloc>& s, const C& c) { s.insert(c.begin(), c.end()); }


template <class KeyType, class ValueType, class LessType, class Alloc, class C> inline
void append(std::map<KeyType, ValueType, LessType, Alloc>& m, const C& c) { m.insert(c.begin(), c.end()); }


template <class T, class Alloc, class CompLess, class CompEqual> inline
void removeDuplicates(std::vector<T, Alloc>& v, CompLess less, CompEqual eq)
{
    std::sort(v.begin(), v.end(), less);
    v.erase(std::unique(v.begin(), v.end(), eq), v.end());
}


template <class T, class Alloc, class CompLess> inline
void removeDuplicates(std::vector<T, Alloc>& v, CompLess less)
{
    removeDuplicates(v, less, [&](const auto& lhs, const auto& rhs) { return !less(lhs, rhs) && !less(rhs, lhs); });
}


template <class T, class Alloc> inline
void removeDuplicates(std::vector<T, Alloc>& v)
{
    removeDuplicates(v, std::less{}, std::equal_to{});
}


template <class T, class Alloc, class CompLess> inline
void removeDuplicatesStable(std::vector<T, Alloc>& v, CompLess less)
{
    std::set<T, CompLess> usedItems(less);
    v.erase(std::remove_if(v.begin(), v.end(),
    /**/[&usedItems](const T& e) { return !usedItems.insert(e).second; }), v.end());
}


template <class T, class Alloc> inline
void removeDuplicatesStable(std::vector<T, Alloc>& v)
{
    removeDuplicatesStable(v, std::less{});
}


template <class RandomAccessIterator, class T, class CompLess> inline
RandomAccessIterator binarySearch(RandomAccessIterator first, RandomAccessIterator last, const T& value, CompLess less)
{
    static_assert(std::is_same_v<typename std::iterator_traits<RandomAccessIterator>::iterator_category, std::random_access_iterator_tag>);

    first = std::lower_bound(first, last, value, less); //alternative: std::partition_point
    if (first != last && !less(value, *first))
        return first;
    else
        return last;
}


template <class BidirectionalIterator, class T> inline
BidirectionalIterator findLast(const BidirectionalIterator first, const BidirectionalIterator last, const T& value)
{
    for (BidirectionalIterator it = last; it != first;) //reverse iteration: 1. check 2. decrement 3. evaluate
    {
        --it; //

        if (*it == value)
            return it;
    }
    return last;
}


template <class RandomAccessIterator1, class RandomAccessIterator2, class IsEq> inline
RandomAccessIterator1 searchFirst(const RandomAccessIterator1 first,       const RandomAccessIterator1 last,
                                  const RandomAccessIterator2 needleFirst, const RandomAccessIterator2 needleLast, IsEq isEqual)
{
    if (needleLast - needleFirst == 1) //don't use expensive std::search unless required!
        return std::find_if(first, last, [needleFirst, isEqual](const auto c) { return isEqual(*needleFirst, c); });
    //"*needleFirst" could be improved with value rather than pointer access, at least for built-in types like "char"

    return std::search(first, last,
                       needleFirst, needleLast, isEqual);
}


template <class RandomAccessIterator1, class RandomAccessIterator2> inline
RandomAccessIterator1 searchFirst(const RandomAccessIterator1 first,       const RandomAccessIterator1 last,
                                  const RandomAccessIterator2 needleFirst, const RandomAccessIterator2 needleLast)
{
    return searchFirst(first, last, needleFirst, needleLast, std::equal_to{});
}



template <class RandomAccessIterator1, class RandomAccessIterator2> inline
RandomAccessIterator1 searchLast(const RandomAccessIterator1 first,       RandomAccessIterator1 last,
                                 const RandomAccessIterator2 needleFirst, const RandomAccessIterator2 needleLast)
{
    if (needleLast - needleFirst == 1) //fast-path
        return findLast(first, last, *needleFirst);

    const RandomAccessIterator1 itNotFound = last;

    //reverse iteration: 1. check 2. decrement 3. evaluate
    for (;;)
    {
        RandomAccessIterator1 it1 = last;
        RandomAccessIterator2 it2 = needleLast;

        for (;;)
        {
            if (it2 == needleFirst) return it1;
            if (it1 == first) return itNotFound;

            --it1;
            --it2;

            if (*it1 != *it2) break;
        }
        --last;
    }
}

//---------------------------------------------------------------------------------------

//read-only variant of std::merge; input: two sorted ranges
template <class Iterator, class FunctionLeftOnly, class FunctionBoth, class FunctionRightOnly, class Compare> inline
void mergeTraversal(Iterator firstL, Iterator lastL,
                    Iterator firstR, Iterator lastR,
                    FunctionLeftOnly lo, FunctionBoth bo, FunctionRightOnly ro, Compare compare)
{
    auto itL = firstL;
    auto itR = firstR;

    auto finishLeft  = [&] { std::for_each(itL, lastL, lo); };
    auto finishRight = [&] { std::for_each(itR, lastR, ro); };

    if (itL == lastL) return finishRight();
    if (itR == lastR) return finishLeft ();

    for (;;)
        if (const std::weak_ordering cmp = compare(*itL, *itR);
            cmp < 0)
        {
            lo(*itL);
            if (++itL == lastL)
                return finishRight();
        }
        else if (cmp > 0)
        {
            ro(*itR);
            if (++itR == lastR)
                return finishLeft();
        }
        else
        {
            bo(*itL, *itR);
            ++itL; //
            ++itR; //increment BOTH before checking for end of range!
            if (itL == lastL) return finishRight();
            if (itR == lastR) return finishLeft ();
            //simplify loop by placing both EOB checks at the beginning? => slightly slower
        }
}


template <class Num>
class FNV1aHash //FNV-1a: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
{
public:
    FNV1aHash() {}
    explicit FNV1aHash(Num startVal) : hashVal_(startVal) { assert(startVal != 0); /*yes, might be a real hash, but most likely bad init value*/}

    void add(Num n)
    {
        hashVal_ ^= n;
        hashVal_ *= prime_;
    }

    Num get() const { return hashVal_; }

private:
    static_assert(isUnsignedInt<Num>);
    static_assert(sizeof(Num) == 4 || sizeof(Num) == 8);
    static constexpr Num base_  = sizeof(Num) == 4 ? 2166136261U : 14695981039346656037ULL;
    static constexpr Num prime_ = sizeof(Num) == 4 ?   16777619U :        1099511628211ULL;

    Num hashVal_ = base_;
};
}

#endif //STL_TOOLS_H_84567184321434

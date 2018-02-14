// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FIXED_LIST_H_01238467085684139453534
#define FIXED_LIST_H_01238467085684139453534

#include <cassert>
#include <iterator>
#include "stl_tools.h"


namespace zen
{
//std::list(C++11)-like class for inplace element construction supporting non-copyable/non-movable types
//-> no iterator invalidation after emplace_back()

template <class T>
class FixedList
{
    struct Node
    {
        template <class... Args>
        Node(Args&& ... args) : val(std::forward<Args>(args)...) {}

        Node* next = nullptr; //singly-linked list is sufficient
        T val;
    };

public:
    FixedList() {}

    ~FixedList() { clear(); }

    template <class NodeT, class U>
    class FixedIterator : public std::iterator<std::forward_iterator_tag, U>
    {
    public:
        FixedIterator(NodeT* it = nullptr) : it_(it) {}
        FixedIterator& operator++() { it_ = it_->next; return *this; }
        inline friend bool operator==(const FixedIterator& lhs, const FixedIterator& rhs) { return lhs.it_ == rhs.it_; }
        inline friend bool operator!=(const FixedIterator& lhs, const FixedIterator& rhs) { return !(lhs == rhs); }
        U& operator* () const { return  it_->val; }
        U* operator->() const { return &it_->val; }
    private:
        NodeT* it_;
    };

    using value_type      = T;
    using iterator        = FixedIterator<      Node,       T>;
    using const_iterator  = FixedIterator<const Node, const T>;
    using reference       = T&;
    using const_reference = const T&;

    iterator begin() { return firstInsert_; }
    iterator end  () { return iterator(); }

    const_iterator begin() const { return firstInsert_; }
    const_iterator end  () const { return const_iterator(); }

    //const_iterator cbegin() const { return firstInsert_; }
    //const_iterator cend  () const { return const_iterator(); }

    reference       front()       { return firstInsert_->val; }
    const_reference front() const { return firstInsert_->val; }

    reference&       back()       { return lastInsert_->val; }
    const_reference& back() const { return lastInsert_->val; }

    template <class... Args>
    void emplace_back(Args&& ... args)
    {
        Node* newNode = new Node(std::forward<Args>(args)...);

        if (!lastInsert_)
        {
            assert(!firstInsert_ && sz_ == 0);
            firstInsert_ = lastInsert_ = newNode;
        }
        else
        {
            assert(lastInsert_->next == nullptr);
            lastInsert_->next = newNode;
            lastInsert_ = newNode;
        }
        ++sz_;
    }

    template <class Predicate>
    void remove_if(Predicate pred)
    {
        Node* prev = nullptr;
        Node* ptr = firstInsert_;

        while (ptr)
            if (pred(ptr->val))
            {
                Node* next = ptr->next;

                delete ptr;
                assert(sz_ > 0);
                --sz_;

                ptr = next;

                if (prev)
                    prev->next = next;
                else
                    firstInsert_ = next;
                if (!next)
                    lastInsert_ = prev;
            }
            else
            {
                prev = ptr;
                ptr = ptr->next;
            }
    }

    void clear()
    {
        Node* ptr = firstInsert_;
        while (ptr)
        {
            Node* next = ptr->next;
            delete ptr;
            ptr = next;
        }

        sz_ = 0;
        firstInsert_ = lastInsert_ = nullptr;
    }

    bool empty() const { return sz_ == 0; }

    size_t size() const { return sz_; }

    void swap(FixedList& other)
    {
        std::swap(firstInsert_, other.firstInsert_);
        std::swap(lastInsert_,  other.lastInsert_);
        std::swap(sz_,          other.sz_);
    }

private:
    FixedList           (const FixedList&) = delete;
    FixedList& operator=(const FixedList&) = delete;

    Node* firstInsert_ = nullptr;
    Node* lastInsert_  = nullptr; //point to last insertion; required by efficient emplace_back()
    size_t sz_ = 0;
};


//just as fast as FixedList, but simpler, more CPU-cache-friendly => superseeds FixedList!
template <class T>
class FixedVector
{
public:
    FixedVector() {}

    /*
    class EndIterator {}; //just like FixedList: no iterator invalidation after emplace_back()

    template <class V>
    class FixedIterator : public std::iterator<std::forward_iterator_tag, V> //could make this random-access if needed
    {
    public:
        FixedIterator(std::vector<std::unique_ptr<T>>& cont, size_t pos) : cont_(cont), pos_(pos) {}
        FixedIterator& operator++() { ++pos_; return *this; }
        inline friend bool operator==(const FixedIterator& lhs, EndIterator) { return lhs.pos_ == lhs.cont_.size(); }
        inline friend bool operator!=(const FixedIterator& lhs, EndIterator) { return !(lhs == EndIterator()); }
        V& operator* () const { return  *cont_[pos_]; }
        V* operator->() const { return &*cont_[pos_]; }
    private:
        std::vector<std::unique_ptr<T>>& cont_;
        size_t pos_ = 0;
    };
    */

    template <class IterImpl, class V>
    class FixedIterator : public std::iterator<std::forward_iterator_tag, V> //could make this bidirectional if needed
    {
    public:
        FixedIterator(IterImpl it) : it_(it) {}
        FixedIterator& operator++() { ++it_; return *this; }
        inline friend bool operator==(const FixedIterator& lhs, const FixedIterator& rhs) { return lhs.it_ == rhs.it_; }
        inline friend bool operator!=(const FixedIterator& lhs, const FixedIterator& rhs) { return !(lhs == rhs); }
        V& operator* () const { return  **it_; }
        V* operator->() const { return &** it_; }
    private:
        IterImpl it_;  //TODO: avoid iterator invalidation after emplace_back(); caveat: end() must not store old length!
    };

    using value_type      = T;
    using iterator        = FixedIterator<typename std::vector<std::unique_ptr<T>>::iterator,             T>;
    using const_iterator  = FixedIterator<typename std::vector<std::unique_ptr<T>>::const_iterator, const T>;
    using reference       =       T&;
    using const_reference = const T&;

    iterator begin() { return items_.begin(); }
    iterator end  () { return items_.end  (); }

    const_iterator begin() const { return items_.begin(); }
    const_iterator end  () const { return items_.end  (); }

    reference       front()       { return *items_.front(); }
    const_reference front() const { return *items_.front(); }

    reference&       back()       { return *items_.back(); }
    const_reference& back() const { return *items_.back(); }

    template <class... Args>
    void emplace_back(Args&& ... args)
    {
        items_.push_back(std::make_unique<T>(std::forward<Args>(args)...));
    }

    template <class Predicate>
    void remove_if(Predicate pred)
    {
        erase_if(items_, [&](const std::unique_ptr<T>& p) { return pred(*p); });
    }

    void   clear() { items_.clear(); }
    bool   empty() const { return items_.empty(); }
    size_t size () const { return items_.size(); }
    void swap(FixedVector& other) { items_.swap(other.items_); }

private:
    FixedVector           (const FixedVector&) = delete;
    FixedVector& operator=(const FixedVector&) = delete;

    std::vector<std::unique_ptr<T>> items_;
};
}

#endif //FIXED_LIST_H_01238467085684139453534

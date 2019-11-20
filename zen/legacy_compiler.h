// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef LEGACY_COMPILER_H_839567308565656789
#define LEGACY_COMPILER_H_839567308565656789


#if !__cpp_lib_erase_if
#include <vector>
#include <set>
#include <map>
#endif


//https://isocpp.org/std/standing-documents/sd-6-sg10-feature-test-recommendations
//https://en.cppreference.com/w/User:D41D8CD98F/feature_testing_macros
//https://gcc.gnu.org/onlinedocs/libstdc++/manual/status.html
namespace std
{

//---------------------------------------------------------------------------------

#if __cpp_lib_span
    #error get rid of workaround:
#endif

template <class T>
class span
{
public:
    template <class Iterator>
    span(Iterator first, Iterator last) : size_(last - first), data_(first != last ? &*first : nullptr) {}

    template <class Container>
    span(Container& cont) : span(cont.begin(), cont.end()) {}

    using iterator        = T*;
    using const_iterator  = const T*;

    iterator begin() { return data_; }
    iterator end  () { return data_ + size_; }

    const_iterator begin() const { return data_; }
    const_iterator end  () const { return data_ + size_; }

    const_iterator cbegin() const { return begin(); }
    const_iterator cend  () const { return end  (); }

    T*     data() const { return data_; }
    size_t size() const { return size_; }
    bool  empty() const { return size_ == 0; }

private:
    const size_t size_;
    T* const data_;
};

//---------------------------------------------------------------------------------

}


namespace zen
{
double from_chars(const char* first, const char* last);
const char* to_chars(char* first, char* last, double num);
}

#endif //LEGACY_COMPILER_H_839567308565656789

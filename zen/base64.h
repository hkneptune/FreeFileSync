// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef BASE64_H_08473021856321840873021487213453214
#define BASE64_H_08473021856321840873021487213453214

#include <cassert>
#include <iterator>
#include "type_traits.h"


namespace zen
{
/*  https://en.wikipedia.org/wiki/Base64

    Usage:
        const std::string input = "Sample text";
        std::string output;
        zen::encodeBase64(input.begin(), input.end(), std::back_inserter(output));
        //output contains "U2FtcGxlIHRleHQ="                                       */

template <class InputIterator, class OutputIterator>
OutputIterator encodeBase64(InputIterator first, InputIterator last, OutputIterator result); //nothrow!

template <class InputIterator, class OutputIterator>
OutputIterator decodeBase64(InputIterator first, InputIterator last, OutputIterator result); //nothrow!

std::string stringEncodeBase64(const std::string_view& str);
std::string stringDecodeBase64(const std::string_view& str);










//------------------------- implementation -------------------------------
namespace impl
{
//64 chars for base64 encoding + padding char
constexpr char ENCODING_MIME[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
constexpr signed char DECODING_MIME[] =
{
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, 64, -1, -1,
        -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
        -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
    };
const unsigned char INDEX_PAD = 64; //index of "="
}


template <class InputIterator, class OutputIterator> inline
OutputIterator encodeBase64(InputIterator first, InputIterator last, OutputIterator result)
{
    using namespace impl;
    static_assert(sizeof(typename std::iterator_traits<InputIterator>::value_type) == 1);
    static_assert(std::size(ENCODING_MIME) == 64 + 1 + 1);
    static_assert(arrayHash(ENCODING_MIME) == 1616767125);

    while (first != last)
    {
        const unsigned char a = static_cast<unsigned char>(*first++);
        *result++ = ENCODING_MIME[a >> 2];

        if (first == last)
        {
            *result++ = ENCODING_MIME[((a & 0x3) << 4)];
            *result++ = ENCODING_MIME[INDEX_PAD];
            *result++ = ENCODING_MIME[INDEX_PAD];
            break;
        }
        const unsigned char b = static_cast<unsigned char>(*first++);
        *result++ = ENCODING_MIME[((a & 0x3) << 4) | (b >> 4)];

        if (first == last)
        {
            *result++ = ENCODING_MIME[((b & 0xf) << 2)];
            *result++ = ENCODING_MIME[INDEX_PAD];
            break;
        }
        const unsigned char c = static_cast<unsigned char>(*first++);
        *result++ = ENCODING_MIME[((b & 0xf) << 2) | (c >> 6)];
        *result++ = ENCODING_MIME[c & 0x3f];
    }
    return result;
}


template <class InputIterator, class OutputIterator> inline
OutputIterator decodeBase64(InputIterator first, InputIterator last, OutputIterator result)
{
    using namespace impl;
    static_assert(sizeof(typename std::iterator_traits<InputIterator>::value_type) == 1);
    static_assert(std::size(DECODING_MIME) == 128);
    static_assert(arrayHash(DECODING_MIME)== 1169145114);

    const unsigned char INDEX_END = INDEX_PAD + 1;

    auto readIndex = [&]() -> unsigned char //return index within [0, 64] or INDEX_END if end of input
    {
        for (;;)
        {
            if (first == last)
                return INDEX_END;

            const unsigned char ch = static_cast<unsigned char>(*first++);
            if (ch < std::size(DECODING_MIME)) //we're in lower ASCII table half
            {
                const int index = DECODING_MIME[ch];
                if (0 <= index && index <= static_cast<int>(INDEX_PAD)) //skip all unknown characters (including carriage return, line-break, tab)
                    return static_cast<unsigned char>(index);
            }
        }
    };

    for (;;)
    {
        const unsigned char index1 = readIndex();
        const unsigned char index2 = readIndex();
        if (index1 >= INDEX_PAD || index2 >= INDEX_PAD)
        {
            assert(index1 == INDEX_END && index2 == INDEX_END);
            break;
        }
        *result++ = static_cast<char>((index1 << 2) | (index2 >> 4));

        const unsigned char index3 = readIndex();
        if (index3 >= INDEX_PAD) //padding
        {
            assert(index3 == INDEX_PAD);
            break;
        }
        *result++ = static_cast<char>(((index2 & 0xf) << 4) | (index3 >> 2));

        const unsigned char index4 = readIndex();
        if (index4 >= INDEX_PAD) //padding
        {
            assert(index4 == INDEX_PAD);
            break;
        }
        *result++ = static_cast<char>(((index3 & 0x3) << 6) | index4);
    }
    return result;
}


inline
std::string stringEncodeBase64(const std::string_view& str)
{
    std::string out;
    encodeBase64(str.begin(), str.end(), std::back_inserter(out));
    return out;
}


inline
std::string stringDecodeBase64(const std::string_view& str)
{
    std::string out;
    decodeBase64(str.begin(), str.end(), std::back_inserter(out));
    return out;
}
}

#endif //BASE64_H_08473021856321840873021487213453214

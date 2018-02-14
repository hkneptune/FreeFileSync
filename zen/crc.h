// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef CRC_H_23489275827847235
#define CRC_H_23489275827847235

#include <boost/crc.hpp>


namespace zen
{
uint16_t getCrc16(const std::string& str);
uint32_t getCrc32(const std::string& str);
template <class ByteIterator> uint16_t getCrc16(ByteIterator first, ByteIterator last);
template <class ByteIterator> uint32_t getCrc32(ByteIterator first, ByteIterator last);




//------------------------- implementation -------------------------------
inline uint16_t getCrc16(const std::string& str) { return getCrc16(str.begin(), str.end()); }
inline uint32_t getCrc32(const std::string& str) { return getCrc32(str.begin(), str.end()); }


template <class ByteIterator> inline
uint16_t getCrc16(ByteIterator first, ByteIterator last)
{
    static_assert(sizeof(typename std::iterator_traits<ByteIterator>::value_type) == 1, "");
    boost::crc_16_type result;
    if (first != last)
        result.process_bytes(&*first, last - first);
    auto rv = result.checksum();
    static_assert(sizeof(rv) == sizeof(uint16_t), "");
    return rv;
}


template <class ByteIterator> inline
uint32_t getCrc32(ByteIterator first, ByteIterator last)
{
    static_assert(sizeof(typename std::iterator_traits<ByteIterator>::value_type) == 1, "");
    boost::crc_32_type result;
    if (first != last)
        result.process_bytes(&*first, last - first);
    auto rv = result.checksum();
    static_assert(sizeof(rv) == sizeof(uint32_t), "");
    return rv;
}
}

#endif //CRC_H_23489275827847235

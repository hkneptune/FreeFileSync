// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SOFT_FILTER_H_810457108534657
#define SOFT_FILTER_H_810457108534657

//#include <algorithm>
#include <limits>
#include "structures.h"


namespace fff
{
/*
Semantics of SoftFilter:
1. It potentially may match only one side => it MUST NOT be applied while traversing a single folder to avoid mismatches
2. => it is applied after traversing and just marks rows, (NO deletions after comparison are allowed)
3. => equivalent to a user temporarily (de-)selecting rows => not relevant for <two way>-mode!
*/

class SoftFilter
{
public:
    SoftFilter(size_t   timeSpan, UnitTime unitTimeSpan,
               uint64_t sizeMin,  UnitSize unitSizeMin,
               uint64_t sizeMax,  UnitSize unitSizeMax);

    bool matchTime(time_t writeTime) const { return timeFrom_ <= writeTime; }
    bool matchSize(uint64_t fileSize) const { return sizeMin_ <= fileSize && fileSize <= sizeMax_; }
    bool matchFolder() const { return matchesFolder_; }
    bool isNull() const; //filter is equivalent to NullFilter, but may be technically slower

    //small helper method: merge two soft filters
    friend SoftFilter combineFilters(const SoftFilter& first, const SoftFilter& second);

private:
    SoftFilter(time_t timeFrom,
               uint64_t sizeMin,
               uint64_t sizeMax,
               bool matchesFolder);

    time_t   timeFrom_ = 0; //unit: UTC, seconds
    uint64_t sizeMin_  = 0; //unit: bytes
    uint64_t sizeMax_  = 0; //unit: bytes
    const bool matchesFolder_;
};














// ----------------------- implementation -----------------------
inline
SoftFilter::SoftFilter(size_t   timeSpan, UnitTime unitTimeSpan,
                       uint64_t sizeMin,  UnitSize unitSizeMin,
                       uint64_t sizeMax,  UnitSize unitSizeMax) :
    matchesFolder_(unitTimeSpan == UnitTime::none &&
                   unitSizeMin  == UnitSize::none &&
                   unitSizeMax  == UnitSize::none) //exclude folders if size or date filter is active: avoids creating empty folders if not needed!
{
    resolveUnits(timeSpan, unitTimeSpan,
                 sizeMin, unitSizeMin,
                 sizeMax, unitSizeMax,
                 timeFrom_,
                 sizeMin_,
                 sizeMax_);
}

inline
SoftFilter::SoftFilter(time_t timeFrom,
                       uint64_t sizeMin,
                       uint64_t sizeMax,
                       bool matchesFolder) :
    timeFrom_(timeFrom),
    sizeMin_ (sizeMin),
    sizeMax_ (sizeMax),
    matchesFolder_(matchesFolder) {}


inline
SoftFilter combineFilters(const SoftFilter& lhs, const SoftFilter& rhs)
{
    return SoftFilter(std::max(lhs.timeFrom_, rhs.timeFrom_),
                      std::max(lhs.sizeMin_,  rhs.sizeMin_),
                      std::min(lhs.sizeMax_,  rhs.sizeMax_),
                      lhs.matchesFolder_ && rhs.matchesFolder_);
}


inline
bool SoftFilter::isNull() const //filter is equivalent to NullFilter, but may be technically slower
{
    return timeFrom_ == std::numeric_limits<time_t>::min() &&
           sizeMin_  == 0U &&
           sizeMax_  == std::numeric_limits<uint64_t>::max() &&
           matchesFolder_;
}
}

#endif //SOFT_FILTER_H_810457108534657

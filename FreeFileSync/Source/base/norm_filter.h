// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef NORM_FILTER_H_974896787346251
#define NORM_FILTER_H_974896787346251

#include "path_filter.h"
#include "soft_filter.h"


namespace fff
{
struct NormalizedFilter //grade-a filter: global/local filter settings combined, units resolved, ready for use
{
    NormalizedFilter(const FilterRef& hf, const SoftFilter& sf) : nameFilter(hf), timeSizeFilter(sf) {}

    //"hard" filter: relevant during comparison, physically skips files
    FilterRef nameFilter;
    //"soft" filter: relevant after comparison; equivalent to user selection
    SoftFilter timeSizeFilter;
};


//combine global and local filters via "logical and"
NormalizedFilter normalizeFilters(const FilterConfig& global, const FilterConfig& local);

inline
bool isNullFilter(const FilterConfig& filterCfg)
{
    return NameFilter::isNull(filterCfg.includeFilter, filterCfg.excludeFilter) &&
           SoftFilter(filterCfg.timeSpan, filterCfg.unitTimeSpan,
                      filterCfg.sizeMin,  filterCfg.unitSizeMin,
                      filterCfg.sizeMax,  filterCfg.unitSizeMax).isNull();
}










// ----------------------- implementation -----------------------
inline
NormalizedFilter normalizeFilters(const FilterConfig& global, const FilterConfig& local)
{
    SoftFilter globalTimeSize(global.timeSpan, global.unitTimeSpan,
                              global.sizeMin,  global.unitSizeMin,
                              global.sizeMax,  global.unitSizeMax);

    SoftFilter localTimeSize(local.timeSpan, local.unitTimeSpan,
                             local.sizeMin,  local.unitSizeMin,
                             local.sizeMax,  local.unitSizeMax);


    return NormalizedFilter(constructFilter(global.includeFilter, global.excludeFilter,
                                            local .includeFilter, local .excludeFilter),
                            combineFilters(globalTimeSize, localTimeSize));
}
}

#endif //NORM_FILTER_H_974896787346251

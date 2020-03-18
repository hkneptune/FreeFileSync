// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SEARCH_H_423905762345342526587
#define SEARCH_H_423905762345342526587

#include <wx+/grid.h>


namespace fff
{
std::pair<const zen::Grid*, ptrdiff_t> findGridMatch(const zen::Grid& grid1, const zen::Grid& grid2, const std::wstring& searchString, bool respectCase, bool searchAscending);
//returns (grid/row) where the value was found, (nullptr, -1) if not found
}

#endif //SEARCH_H_423905762345342526587

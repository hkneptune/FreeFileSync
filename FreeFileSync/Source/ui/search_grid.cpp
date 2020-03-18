// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "search_grid.h"
#include <zen/zstring.h>
#include <zen/utf.h>
#include <zen/perf.h>

using namespace zen;
using namespace fff;


namespace
{
inline std::wstring getUnicodeNormalFormWide(const std::wstring& str) { return utfTo<std::wstring>(getUnicodeNormalForm(utfTo<Zstring>(str))); }
inline std::wstring makeUpperCopyWide       (const std::wstring& str) { return utfTo<std::wstring>(makeUpperCopy       (utfTo<Zstring>(str))); }


template <bool respectCase>
class MatchFound
{
public:
    MatchFound(const std::wstring& textToFind) : textToFind_(getUnicodeNormalFormWide(textToFind)) {}
    bool operator()(const std::wstring& phrase) const
    {
        if (isAsciiString(phrase)) //perf: save Zstring conversion for getUnicodeNormalFormWide() when not needed
            return contains(phrase, textToFind_);
        else
            return contains(getUnicodeNormalFormWide(phrase), textToFind_);
    }

private:
    const std::wstring textToFind_;
};


template <>
class MatchFound<false>
{
public:
    MatchFound(const std::wstring& textToFind) : textToFind_(makeUpperCopyWide(textToFind)) {}
    bool operator()(std::wstring&& phrase) const
    {
        if (isAsciiString(phrase)) //perf: save Zstring conversion for makeUpperCopyWide() when not needed
        {
            for (wchar_t& c : phrase) c = asciiToUpper(c);
            return contains(phrase, textToFind_);
        }
        else
            return contains(makeUpperCopyWide(phrase), textToFind_); //getUnicodeNormalForm() is implied by makeUpperCopy()
    }

private:
    const std::wstring textToFind_;
};

//###########################################################################################

template <bool respectCase>
ptrdiff_t findRow(const Grid& grid, //return -1 if no matching row found
                  const std::wstring& searchString,
                  bool searchAscending,
                  size_t rowFirst, //specify area to search:
                  size_t rowLast)  // [rowFirst, rowLast)
{
    if (auto prov = grid.getDataProvider())
    {
        std::vector<Grid::ColAttributes> colAttr = grid.getColumnConfig();
        std::erase_if(colAttr, [](const Grid::ColAttributes& ca) { return !ca.visible; });
        if (!colAttr.empty())
        {
            const MatchFound<respectCase> matchFound(searchString);

            if (searchAscending)
            {
                for (size_t row = rowFirst; row < rowLast; ++row)
                    for (const Grid::ColAttributes& ca : colAttr)
                        if (matchFound(prov->getValue(row, ca.type)))
                            return row;
            }
            else
                for (size_t row = rowLast; row-- > rowFirst;)
                    for (const Grid::ColAttributes& ca : colAttr)
                        if (matchFound(prov->getValue(row, ca.type)))
                            return row;
        }
    }
    return -1;
}
}


std::pair<const Grid*, ptrdiff_t> fff::findGridMatch(const Grid& grid1, const Grid& grid2, const std::wstring& searchString, bool respectCase, bool searchAscending)
{
    //PERF_START

    const size_t rowCount1 = grid1.getRowCount();
    const size_t rowCount2 = grid2.getRowCount();

    size_t cursorRow1 = grid1.getGridCursor();
    if (cursorRow1 >= rowCount1)
        cursorRow1 = 0;

    std::pair<const Grid*, ptrdiff_t> result(nullptr, -1);

    auto finishSearch = [&](const Grid& grid, size_t rowFirst, size_t rowLast)
    {
        const ptrdiff_t targetRow = respectCase ?
                                    findRow<true >(grid, searchString, searchAscending, rowFirst, rowLast) :
                                    findRow<false>(grid, searchString, searchAscending, rowFirst, rowLast);
        if (targetRow >= 0)
        {
            result = { &grid, targetRow };
            return true;
        }
        return false;
    };

    if (searchAscending)
    {
        if (!finishSearch(grid1, cursorRow1 + 1, rowCount1))
            if (!finishSearch(grid2, 0, rowCount2))
                finishSearch(grid1, 0, cursorRow1 + 1);
    }
    else
    {
        if (!finishSearch(grid1, 0, cursorRow1))
            if (!finishSearch(grid2, 0, rowCount2))
                finishSearch(grid1, cursorRow1, rowCount1);
    }
    return result;
}

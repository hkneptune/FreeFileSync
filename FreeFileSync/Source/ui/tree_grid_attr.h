// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TREE_GRID_ATTR_H_83470918473021745
#define TREE_GRID_ATTR_H_83470918473021745

#include <vector>
#include <cassert>
#include <wx+/dc.h>


namespace fff
{
enum class ColumnTypeTree
{
    folder,
    itemCount,
    bytes,
};

struct ColAttributesTree
{
    ColumnTypeTree type    = ColumnTypeTree::folder;
    int            offset  = 0;
    int            stretch = 0;
    bool           visible = false;
};


inline
std::vector<ColAttributesTree> getTreeGridDefaultColAttribs()
{
    using namespace zen;
    return //harmonize with tree_view.cpp::onGridLabelContext() => expects stretched folder and non-stretched other columns!
    {
        { ColumnTypeTree::folder,    - 2 * fastFromDIP(60), 1, true }, //stretch to full width and substract sum of fixed size widths
        { ColumnTypeTree::itemCount, fastFromDIP(60), 0, true },
        { ColumnTypeTree::bytes,     fastFromDIP(60), 0, true }, //GTK needs a few pixels more width
    };
}

const           bool treeGridShowPercentageDefault = true;
const ColumnTypeTree treeGridLastSortColumnDefault = ColumnTypeTree::bytes;

inline
bool getDefaultSortDirection(ColumnTypeTree colType)
{
    switch (colType)
    {
        case ColumnTypeTree::folder:
            return true;
        case ColumnTypeTree::itemCount:
            return false;
        case ColumnTypeTree::bytes:
            return false;
    }
    assert(false);
    return true;
}
}

#endif //TREE_GRID_ATTR_H_83470918473021745

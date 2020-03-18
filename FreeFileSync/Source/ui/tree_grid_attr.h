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
    FOLDER_NAME,
    ITEM_COUNT,
    BYTES,
};

struct ColAttributesTree
{
    ColumnTypeTree type    = ColumnTypeTree::FOLDER_NAME;
    int            offset  = 0;
    int            stretch = 0;
    bool           visible = false;
};


inline
std::vector<ColAttributesTree> getTreeGridDefaultColAttribs()
{
    using namespace zen;
    return //harmonize with tree_view.cpp::onGridLabelContext() => expects stretched FOLDER_NAME and non-stretched other columns!
    {
        { ColumnTypeTree::FOLDER_NAME, fastFromDIP(0 - 60 - 60), 1, true }, //stretch to full width and substract sum of fixed size widths
        { ColumnTypeTree::ITEM_COUNT,  fastFromDIP(60), 0, true },
        { ColumnTypeTree::BYTES,       fastFromDIP(60), 0, true }, //GTK needs a few pixels more width
    };
}

const           bool treeGridShowPercentageDefault = true;
const ColumnTypeTree treeGridLastSortColumnDefault = ColumnTypeTree::BYTES;

inline
bool getDefaultSortDirection(ColumnTypeTree colType)
{
    switch (colType)
    {
        case ColumnTypeTree::FOLDER_NAME:
            return true;
        case ColumnTypeTree::ITEM_COUNT:
            return false;
        case ColumnTypeTree::BYTES:
            return false;
    }
    assert(false);
    return true;
}
}

#endif //TREE_GRID_ATTR_H_83470918473021745

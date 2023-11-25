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
enum class ColumnTypeOverview
{
    folder,
    itemCount,
    bytes,
};

struct ColumnAttribOverview
{
    ColumnTypeOverview type = ColumnTypeOverview::folder;
    int                offset  = 0;
    int                stretch = 0;
    bool               visible = false;
};


inline
std::vector<ColumnAttribOverview> getOverviewDefaultColAttribs()
{
    using namespace zen;
    return //harmonize with tree_view.cpp::onGridLabelContext() => expects stretched folder and non-stretched other columns!
    {
        {ColumnTypeOverview::folder, - 2 * dipToWxsize(60), 1, true}, //stretch to full width and substract sum of fixed size widths
        {ColumnTypeOverview::itemCount,    dipToWxsize(60), 0, true},
        {ColumnTypeOverview::bytes,        dipToWxsize(60), 0, true}, //GTK needs a few pixels more width
    };
}

const           bool overviewPanelShowPercentageDefault = true;
const ColumnTypeOverview overviewPanelLastSortColumnDefault = ColumnTypeOverview::bytes;

inline
bool getDefaultSortDirection(ColumnTypeOverview colType)
{
    switch (colType)
    {
        case ColumnTypeOverview::folder:
            return true;
        case ColumnTypeOverview::itemCount:
            return false;
        case ColumnTypeOverview::bytes:
            return false;
    }
    assert(false);
    return true;
}
}

#endif //TREE_GRID_ATTR_H_83470918473021745

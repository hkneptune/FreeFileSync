// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef COLUMN_ATTR_H_189467891346732143213
#define COLUMN_ATTR_H_189467891346732143213

#include <vector>
#include <cassert>
#include <wx+/dc.h>


namespace fff
{
enum class GridViewType
{
    difference,
    action,
};

enum class ColumnTypeRim
{
    path,
    size,
    date,
    extension,
};

struct ColAttributesRim
{
    ColumnTypeRim type    = ColumnTypeRim::path;
    int           offset  = 0;
    int           stretch = 0;
    bool          visible = false;
};

inline
std::vector<ColAttributesRim> getFileGridDefaultColAttribsLeft()
{
    using namespace zen;
    return //harmonize with main_dlg.cpp::onGridLabelContextRim() => expects stretched path and non-stretched other columns!
    {
        {ColumnTypeRim::path,     -dipToWxsize(100), 1, true },
        {ColumnTypeRim::extension, dipToWxsize( 60), 0, false},
        {ColumnTypeRim::date,      dipToWxsize(140), 0, false},
        {ColumnTypeRim::size,      dipToWxsize(100), 0, true },
    };
}

inline
std::vector<ColAttributesRim> getFileGridDefaultColAttribsRight()
{
    return getFileGridDefaultColAttribsLeft(); //*currently* same default
}


inline
bool getDefaultSortDirection(ColumnTypeRim type) //true: ascending; false: descending
{
    switch (type)
    {
        case ColumnTypeRim::size:
        case ColumnTypeRim::date:
            return false;

        case ColumnTypeRim::path:
        case ColumnTypeRim::extension:
            return true;
    }
    assert(false);
    return true;
}


enum class ItemPathFormat
{
    name,
    relative,
    full,
};

const ItemPathFormat defaultItemPathFormatLeftGrid  = ItemPathFormat::relative;
const ItemPathFormat defaultItemPathFormatRightGrid = ItemPathFormat::relative;

//------------------------------------------------------------------
enum class ColumnTypeCenter
{
    checkbox,
    difference,
    action,
};


inline
bool getDefaultSortDirection(ColumnTypeCenter type) //true: ascending; false: descending
{
    assert(type != ColumnTypeCenter::checkbox);
    return true;
}
//------------------------------------------------------------------
}

#endif //COLUMN_ATTR_H_189467891346732143213

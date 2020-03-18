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
enum class ColumnTypeRim
{
    ITEM_PATH,
    SIZE,
    DATE,
    EXTENSION,
};

struct ColAttributesRim
{
    ColumnTypeRim type    = ColumnTypeRim::ITEM_PATH;
    int           offset  = 0;
    int           stretch = 0;
    bool          visible = false;
};

inline
std::vector<ColAttributesRim> getFileGridDefaultColAttribsLeft()
{
    using namespace zen;
    return //harmonize with main_dlg.cpp::onGridLabelContextRim() => expects stretched ITEM_PATH and non-stretched other columns!
    {
        { ColumnTypeRim::ITEM_PATH, fastFromDIP(-100), 1, true  },
        { ColumnTypeRim::EXTENSION, fastFromDIP(  60), 0, false },
        { ColumnTypeRim::DATE,      fastFromDIP( 140), 0, false },
        { ColumnTypeRim::SIZE,      fastFromDIP( 100), 0, true  },
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
        case ColumnTypeRim::SIZE:
        case ColumnTypeRim::DATE:
            return false;

        case ColumnTypeRim::ITEM_PATH:
        case ColumnTypeRim::EXTENSION:
            return true;
    }
    assert(false);
    return true;
}


enum class ItemPathFormat
{
    FULL_PATH,
    RELATIVE_PATH,
    ITEM_NAME,
};

const ItemPathFormat defaultItemPathFormatLeftGrid  = ItemPathFormat::RELATIVE_PATH;
const ItemPathFormat defaultItemPathFormatRightGrid = ItemPathFormat::RELATIVE_PATH;

//------------------------------------------------------------------
enum class ColumnTypeCenter
{
    CHECKBOX,
    CMP_CATEGORY,
    SYNC_ACTION,
};


inline
bool getDefaultSortDirection(ColumnTypeCenter type) //true: ascending; false: descending
{
    assert(type != ColumnTypeCenter::CHECKBOX);
    return true;
}
//------------------------------------------------------------------
}

#endif //COLUMN_ATTR_H_189467891346732143213

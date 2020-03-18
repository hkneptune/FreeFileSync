// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef CHOICE_ENUM_H_132413545345687
#define CHOICE_ENUM_H_132413545345687

#include <vector>
#include <wx/choice.h>

//handle mapping of enum values to wxChoice controls
/*
Example:

Member variable:
    zen::EnumDescrList<EnumOnError> enumDescrMap;

Constructor code:
    enumDescrMap.
    add(ON_ERROR_POPUP,  "Show pop-up",    "Show pop-up on errors or warnings"). <- add localization
    add(ON_ERROR_IGNORE, "Ignore errors",  "Hide all error and warning messages").
    add(ON_ERROR_EXIT,   "Exit instantly", "Abort synchronization immediately");

Set enum value:
    setEnumVal(enumDescrMap, *m_choiceHandleError, value);

Get enum value:
    value = getEnumVal(enumDescrMap, *m_choiceHandleError)

Update enum tooltips (after user changed selection):
    updateTooltipEnumVal(enumDescrMap, *m_choiceHandleError);
*/

namespace zen
{
template <class Enum>
struct EnumDescrList
{
    EnumDescrList& add(Enum value, const wxString& text, const wxString& tooltip = {})
    {
        descrList.push_back({ value, { text, tooltip } });
        return *this;
    }
    using DescrList = std::vector<std::pair<Enum, std::pair<wxString, wxString>>>;
    DescrList descrList;
};
template <class Enum> void setEnumVal(const EnumDescrList<Enum>& mapping, wxChoice& ctrl, Enum value);
template <class Enum> Enum getEnumVal(const EnumDescrList<Enum>& mapping, const wxChoice& ctrl);
template <class Enum> void updateTooltipEnumVal(const EnumDescrList<Enum>& mapping, wxChoice& ctrl);














//--------------- impelementation -------------------------------------------
template <class Enum>
void setEnumVal(const EnumDescrList<Enum>& mapping, wxChoice& ctrl, Enum value)
{
    ctrl.Clear();

    int selectedPos = 0;
    for (auto it = mapping.descrList.begin(); it != mapping.descrList.end(); ++it)
    {
        ctrl.Append(it->second.first);
        if (it->first == value)
        {
            selectedPos = it - mapping.descrList.begin();

            if (it->second.second.empty())
                ctrl.UnsetToolTip();
            else
                ctrl.SetToolTip(it->second.second);
        }
    }

    ctrl.SetSelection(selectedPos);
}

template <class Enum>
Enum getEnumVal(const EnumDescrList<Enum>& mapping, const wxChoice& ctrl)
{
    const int selectedPos = ctrl.GetSelection();

    if (0 <= selectedPos && selectedPos < static_cast<int>(mapping.descrList.size()))
        return mapping.descrList[selectedPos].first;
    else
    {
        assert(false);
        return Enum(0);
    }
}

template <class Enum> void updateTooltipEnumVal(const EnumDescrList<Enum>& mapping, wxChoice& ctrl)
{
    const Enum currentValue = getEnumVal(mapping, ctrl);

    for (const auto& [enumValue, textAndTooltip] : mapping.descrList)
        if (currentValue == enumValue)
            ctrl.SetToolTip(textAndTooltip.second);
}
}

#endif //CHOICE_ENUM_H_132413545345687

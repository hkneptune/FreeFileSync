// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef CHOICE_ENUM_H_132413545345687
#define CHOICE_ENUM_H_132413545345687

#include <unordered_map>
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
        descrList.push_back({value, {text, tooltip}});
        return *this;
    }

    using DescrList = std::vector<std::pair<Enum, std::pair<wxString, wxString>>>;
    DescrList descrList;

    std::unordered_map<const wxChoice*, std::vector<wxString>> labelsSetLast;
};
template <class Enum> void setEnumVal(const EnumDescrList<Enum>& mapping, wxChoice& ctrl, Enum value);
template <class Enum> Enum getEnumVal(const EnumDescrList<Enum>& mapping, const wxChoice& ctrl);
template <class Enum> void updateTooltipEnumVal(const EnumDescrList<Enum>& mapping, wxChoice& ctrl);














//--------------- impelementation -------------------------------------------
template <class Enum>
void setEnumVal(EnumDescrList<Enum>& mapping, wxChoice& ctrl, Enum value)
{
    auto& labelsSetLast = mapping.labelsSetLast[&ctrl];

    std::vector<wxString> labels;
    for (const auto& [val, texts] : mapping.descrList)
        labels.push_back(texts.first);

    if (labels != labelsSetLast)
    {
        ctrl.Set(labels); //expensive as fuck! => only call when absolutely needed!
        labelsSetLast = std::move(labels);
    }
    //-----------------------------------------------------------------

    const auto it = std::find_if(mapping.descrList.begin(), mapping.descrList.end(), [&](const auto& mapItem) { return mapItem.first == value; });
    if (it != mapping.descrList.end())
    {
        if (const wxString& tooltip = it->second.second;
            !tooltip.empty())
            ctrl.SetToolTip(tooltip);
        else
            ctrl.UnsetToolTip();

        const int selectedPos = it - mapping.descrList.begin();
        ctrl.SetSelection(selectedPos);
    }
    else assert(false);
}

template <class Enum>
Enum getEnumVal(const EnumDescrList<Enum>& mapping, const wxChoice& ctrl)
{
    const int selectedPos = ctrl.GetSelection();

    if (0 <= selectedPos && selectedPos < std::ssize(mapping.descrList))
        return mapping.descrList[selectedPos].first;
    else
    {
        assert(false);
        return Enum(0);
    }
}

template <class Enum> void updateTooltipEnumVal(const EnumDescrList<Enum>& mapping, wxChoice& ctrl)
{
    const int selectedPos = ctrl.GetSelection();

    if (0 <= selectedPos && selectedPos < std::ssize(mapping.descrList))
    {
        if (const auto& [text, tooltip] = mapping.descrList[selectedPos].second;
            !tooltip.empty())
            ctrl.SetToolTip(tooltip);
        else
            ctrl.UnsetToolTip();
    }
    else assert(false);
}
}

#endif //CHOICE_ENUM_H_132413545345687

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef CHOICE_ENUM_H_132413545345687
#define CHOICE_ENUM_H_132413545345687

//#include <vector>
#include <wx/choice.h>


namespace zen
{
//handle mapping of enum values to wxChoice controls
template <class Enum>
class EnumDescrList
{
public:
    using DescrItem = std::tuple<Enum, wxString /*label*/, wxString /*tooltip*/>;

    EnumDescrList(wxChoice& ctrl, std::vector<DescrItem> list);
    ~EnumDescrList();

    void set(Enum value);
    Enum get() const ;
    void updateTooltip(); //after user changed selection

    const std::vector<DescrItem>& getConfig() const { return descrList_; }

private:
    wxChoice& ctrl_;
    const std::vector<DescrItem> descrList_;
    std::vector<wxString> labels_;
};














//--------------- impelementation -------------------------------------------
template <class Enum>
EnumDescrList<Enum>::EnumDescrList(wxChoice& ctrl, std::vector<DescrItem> list) : ctrl_(ctrl), descrList_(std::move(list))
{
    for (const auto& [val, label, tooltip] : descrList_)
        labels_.push_back(label);

    ctrl_.Set(labels_); //expensive as fuck! => only call when needed!
}


template <class Enum> inline
EnumDescrList<Enum>::~EnumDescrList()
{
}


template <class Enum>
void EnumDescrList<Enum>::set(Enum value)
{
    const auto it = std::find_if(descrList_.begin(), descrList_.end(), [&](const auto& mapItem) { return std::get<Enum>(mapItem) == value; });
    if (it != descrList_.end())
    {
        const auto& [val, label, tooltip] = *it;
        if (!tooltip.empty())
            ctrl_.SetToolTip(tooltip);
        else
            ctrl_.UnsetToolTip();

        const int selectedPos = it - descrList_.begin();
        ctrl_.SetSelection(selectedPos);
    }
    else assert(false);
}


template <class Enum>
Enum EnumDescrList<Enum>::get() const
{
    const int selectedPos = ctrl_.GetSelection();

    if (0 <= selectedPos && selectedPos < std::ssize(descrList_))
        return std::get<Enum>(descrList_[selectedPos]);

    assert(false);
    return Enum(0);
}


template <class Enum>
void EnumDescrList<Enum>::updateTooltip()
{
    const int selectedPos = ctrl_.GetSelection();

    if (0 <= selectedPos && selectedPos < std::ssize(descrList_))
    {
        const auto& [val, label, tooltip] = descrList_[selectedPos];
        if (!tooltip.empty())
            ctrl_.SetToolTip(tooltip);
        else
            ctrl_.UnsetToolTip();
    }
    else assert(false);
}
}

#endif //CHOICE_ENUM_H_132413545345687

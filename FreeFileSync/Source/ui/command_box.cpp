// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "command_box.h"
//#include <deque>
//#include <zen/i18n.h>
#include <algorithm>
//#include <zen/stl_tools.h>
#include <zen/utf.h>
#include <wx+/dc.h>

using namespace zen;
using namespace fff;


namespace
{
inline
wxString getSeparationLine() { return std::wstring(50, EM_DASH); } //no space between dashes!
}


CommandBox::CommandBox(wxWindow* parent,
                       wxWindowID id,
                       const wxString& value,
                       const wxPoint& pos,
                       const wxSize& size,
                       int n,
                       const wxString choices[],
                       long style,
                       const wxValidator& validator,
                       const wxString& name) :
    wxComboBox(parent, id, value, pos, size, n, choices, style, validator, name)
{
    //####################################
    /*#*/ SetMinSize({dipToWxsize(150), -1}); //# workaround yet another wxWidgets bug: default minimum size is much too large for a wxComboBox
    //####################################

    Bind(wxEVT_KEY_DOWN,                  [this](wxKeyEvent&     event) { onKeyEvent  (event); });
    Bind(wxEVT_LEFT_DOWN,                 [this](wxMouseEvent&   event) { onUpdateList(event); });
    Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, [this](wxCommandEvent& event) { onSelection (event); });
    Bind(wxEVT_MOUSEWHEEL,                []    (wxMouseEvent&   event) {}); //swallow! this gives confusing UI feedback anyway
}


void CommandBox::addItemHistory()
{
    const Zstring newCommand = trimCpy(getValue());

    if (newCommand == utfTo<Zstring>(getSeparationLine()) || //do not add sep. line
        newCommand.empty())
        return;

    //do not add built-in commands to history
    for (const auto& [description, cmd] : defaultCommands_)
        if (newCommand == utfTo<Zstring>(description) ||
            equalNoCase(newCommand, cmd))
            return;

    std::erase_if(history_, [&](const Zstring& item) { return equalNoCase(newCommand, item); });

    history_.insert(history_.begin(), newCommand);

    if (history_.size() > historyMax_)
        history_.resize(historyMax_);
}


Zstring CommandBox::getValue() const
{
    return utfTo<Zstring>(trimCpy(GetValue()));
}


void CommandBox::setValue(const Zstring& value)
{
    setValueAndUpdateList(trimCpy(utfTo<wxString>(value)));
}


//set value and update list are technically entangled: see potential bug description below
void CommandBox::setValueAndUpdateList(const wxString& value)
{
    //it may be a little lame to update the list on each mouse-button click, but it should be working and we dont't have to manipulate wxComboBox internals

    std::vector<wxString> items;

    //1. built in commands
    for (const auto& [description, cmd] : defaultCommands_)
        items.push_back(description);

    //2. history elements
    auto histSorted = history_;
    std::sort(histSorted.begin(), histSorted.end(), LessNaturalSort() /*even on Linux*/);

    if (!items.empty() && !histSorted.empty())
        items.push_back(getSeparationLine());

    for (const Zstring& hist : histSorted)
        items.push_back(utfTo<wxString>(hist));

    //attention: if the target value is not part of the dropdown list, SetValue() will look for a string that *starts with* this value:
    //e.g. if the dropdown list contains "222" SetValue("22") will erroneously set and select "222" instead, while "111" would be set correctly!
    // -> by design on Windows!
    if (std::find(items.begin(), items.end(), value) == items.end())
    {
        if (!items.empty() && !value.empty())
            items.insert(items.begin(), {value, getSeparationLine()});
        else
            items.insert(items.begin(), {value});
    }

    //this->Clear(); -> NO! emits yet another wxEVT_COMMAND_TEXT_UPDATED!!!
    wxItemContainer::Clear(); //suffices to clear the selection items only!
    this->Append(items); //expensive as fuck! => only call when absolutely needed!

    //this->SetSelection(wxNOT_FOUND); //don't select anything
    ChangeValue(value); //preserve main text!
}


void CommandBox::onSelection(wxCommandEvent& event)
{
    //we cannot replace built-in commands at this position in call stack, so defer to a later time!
    CallAfter([&] { onValidateSelection(); });

    event.Skip();
}


void CommandBox::onValidateSelection()
{
    const wxString value = GetValue();

    if (value == getSeparationLine())
        return setValueAndUpdateList(wxString());

    for (const auto& [description, cmd] : defaultCommands_)
        if (description == value)
            return setValueAndUpdateList(utfTo<wxString>(cmd)); //replace GUI name by actual command string
}


void CommandBox::onUpdateList(wxEvent& event)
{
    setValue(getValue());
    event.Skip();
}


void CommandBox::onKeyEvent(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();

    switch (keyCode)
    {
        case WXK_DELETE:
        case WXK_NUMPAD_DELETE:
        {
            //try to delete the currently selected config history item
            int pos = this->GetCurrentSelection();
            if (0 <= pos && pos < static_cast<int>(this->GetCount()) &&
                //what a mess...:
                (GetValue() != GetString(pos) || //avoid problems when a character shall be deleted instead of list item
                 GetValue().empty())) //exception: always allow removing empty entry
            {
                const auto selValue = utfTo<Zstring>(GetString(pos));

                if (std::find(history_.begin(), history_.end(), selValue) != history_.end()) //only history elements may be deleted
                {
                    //save old (selected) value: deletion seems to have influence on this
                    const wxString currentVal = this->GetValue();
                    //this->SetSelection(wxNOT_FOUND);

                    //delete selected row
                    std::erase(history_, selValue);

                    SetString(pos, wxString()); //in contrast to Delete(), this one does not kill the drop-down list and gives a nice visual feedback!
                    //Delete(pos);

                    //(re-)set value
                    SetValue(currentVal);
                }
                return; //eat up key event
            }
        }
        break;

        case WXK_UP:
        case WXK_NUMPAD_UP:
        case WXK_DOWN:
        case WXK_NUMPAD_DOWN:
        case WXK_PAGEUP:
        case WXK_NUMPAD_PAGEUP:
        case WXK_PAGEDOWN:
        case WXK_NUMPAD_PAGEDOWN:
            return; //swallow -> using these keys gives a weird effect due to this weird control
    }


    event.Skip();
}

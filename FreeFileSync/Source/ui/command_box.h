// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef COMMAND_BOX_H_18947773210473214
#define COMMAND_BOX_H_18947773210473214

#include <vector>
//#include <string>
#include <wx/combobox.h>
#include <zen/zstring.h>


//combobox with history function + functionality to delete items (DEL)
namespace fff
{
class CommandBox : public wxComboBox
{
public:
    CommandBox(wxWindow* parent,
               wxWindowID id,
               const wxString& value = {},
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               int n = 0,
               const wxString choices[] = nullptr,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxASCII_STR(wxComboBoxNameStr));

    void setHistory(const std::vector<Zstring>& history, size_t historyMax) { history_ = history; historyMax_ = historyMax; }
    std::vector<Zstring> getHistory() const { return history_; }
    void addItemHistory(); //adds current item to history

    // use these two accessors instead of GetValue()/SetValue():
    Zstring getValue() const;
    void setValue(const Zstring& value);
    //required for setting value correctly + Linux to ensure the dropdown is shown as being populated

private:
    void onKeyEvent(wxKeyEvent& event);
    void onSelection(wxCommandEvent& event);
    void onValidateSelection();
    void onUpdateList(wxEvent& event);

    void setValueAndUpdateList(const wxString& value);

    std::vector<Zstring> history_;
    size_t historyMax_ = 0;

    const std::vector<std::pair<wxString, Zstring>> defaultCommands_; //(description/command) pairs
};
}

#endif //COMMAND_BOX_H_18947773210473214

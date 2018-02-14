// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FOLDER_SELECTOR_H_24857842375234523463425
#define FOLDER_SELECTOR_H_24857842375234523463425

#include <zen/zstring.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx+/file_drop.h>
#include "folder_history_box.h"


namespace fff
{
//handle drag and drop, tooltip, label and manual input, coordinating a wxWindow, wxButton, and wxComboBox/wxTextCtrl
/*
Reasons NOT to use wxDirPickerCtrl, but wxButton instead:
    - Crash on GTK 2: http://favapps.wordpress.com/2012/06/11/freefilesync-crash-in-linux-when-syncing-solved/
    - still uses outdated ::SHBrowseForFolder() (even on Windows 7)
    - selection dialog remembers size, but NOT position => if user enlarges window, the next time he opens the dialog it may leap out of visible screen
    - hard-codes "Browse" button label
*/

extern const wxEventType EVENT_ON_FOLDER_SELECTED;    //directory is changed by the user (except manual type-in)
extern const wxEventType EVENT_ON_FOLDER_MANUAL_EDIT; //manual type-in
//example: wnd.Connect(EVENT_ON_FOLDER_SELECTED, wxCommandEventHandler(MyDlg::OnDirSelected), nullptr, this);

class FolderSelector: public wxEvtHandler
{
public:
    FolderSelector(wxWindow&         dropWindow,
                   wxButton&         selectFolderButton,
                   wxButton&         selectAltFolderButton,
                   FolderHistoryBox& folderComboBox,
                   wxStaticText*     staticText,   //optional
                   wxWindow*         dropWindow2); //

    ~FolderSelector();

    void setSiblingSelector(FolderSelector* selector) { siblingSelector_ = selector; }

    Zstring getPath() const;
    void setPath(const Zstring& folderPathPhrase);

    void setBackgroundText(const std::wstring& text) { folderComboBox_.SetHint(text); }

private:
    virtual bool shouldSetDroppedPaths(const std::vector<Zstring>& shellItemPaths) { return true; } //return true if drop should be processed

    void onMouseWheel     (wxMouseEvent&   event);
    void onItemPathDropped(zen::FileDropEvent&  event);
    void onEditFolderPath (wxCommandEvent& event);
    void onSelectFolder   (wxCommandEvent& event);
    void onSelectAltFolder(wxCommandEvent& event);

    wxWindow&         dropWindow_;
    wxWindow*         dropWindow2_ = nullptr;
    wxButton&         selectFolderButton_;
    wxButton&         selectAltFolderButton_;
    FolderHistoryBox& folderComboBox_;
    wxStaticText*     staticText_ = nullptr; //optional
    FolderSelector*   siblingSelector_ = nullptr;
};
}

#endif //FOLDER_SELECTOR_H_24857842375234523463425

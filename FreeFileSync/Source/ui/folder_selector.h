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
#include "../afs/abstract.h"


namespace fff
{
/* handle drag and drop, tooltip, label and manual input, coordinating a wxWindow, wxButton, and wxComboBox/wxTextCtrl

    Reasons NOT to use wxDirPickerCtrl, but wxButton instead:
    - Crash on GTK 2: https://favapps.wordpress.com/2012/06/11/freefilesync-crash-in-linux-when-syncing-solved/
    - still uses outdated ::SHBrowseForFolder() (even on Windows 7)
    - selection dialog remembers size, but NOT position => if user enlarges window, the next time he opens the dialog it may leap out of visible screen
    - hard-codes "Browse" button label                                      */

wxDECLARE_EVENT(EVENT_ON_FOLDER_SELECTED, wxCommandEvent); //directory is changed by the user, including manual type-in
//example: wnd.Bind(EVENT_ON_FOLDER_SELECTED, [this](wxCommandEvent& event) { onDirSelected(event); });

class FolderSelector: public wxEvtHandler
{
public:
    FolderSelector(wxWindow*         parent,
                   wxWindow&         dropWindow,
                   wxButton&         selectFolderButton,
                   wxButton&         selectAltFolderButton,
                   FolderHistoryBox& folderComboBox,
                   Zstring& folderLastSelected, Zstring& sftpKeyFileLastSelected,
                   wxStaticText*     staticText,  //optional
                   wxWindow*         dropWindow2, //
                   const std::function<bool  (const std::vector<Zstring>& shellItemPaths)>&          droppedPathsFilter,    //optional
                   const std::function<size_t(const Zstring& folderPathPhrase)>&                     getDeviceParallelOps,  //mandatory
                   const std::function<void  (const Zstring& folderPathPhrase, size_t parallelOps)>& setDeviceParallelOps); //optional

    ~FolderSelector();

    void setSiblingSelector(FolderSelector* selector) { siblingSelector_ = selector; }

    void setPath(const Zstring& folderPathPhrase);
    Zstring getPath() const;

private:
    void onMouseWheel     (wxMouseEvent&   event);
    void onItemPathDropped(zen::FileDropEvent&  event);
    void onEditFolderPath (wxCommandEvent& event);
    void onSelectFolder   (wxCommandEvent& event);
    void onSelectAltFolder(wxCommandEvent& event);

    const std::function<bool(const std::vector<Zstring>& shellItemPaths)> droppedPathsFilter_;

    const std::function<size_t(const Zstring& folderPathPhrase)>                     getDeviceParallelOps_;
    const std::function<void  (const Zstring& folderPathPhrase, size_t parallelOps)> setDeviceParallelOps_;

    wxWindow*         parent_;
    wxWindow&         dropWindow_;
    wxWindow*         dropWindow2_ = nullptr; //
    wxButton&         selectFolderButton_;
    wxButton&         selectAltFolderButton_;
    FolderHistoryBox& folderComboBox_;
    Zstring&          folderLastSelected_;
    Zstring& sftpKeyFileLastSelected_;
    wxStaticText*     staticText_        = nullptr; //optional
    FolderSelector*   siblingSelector_   = nullptr; //
};


//abstract version of openWithDefaultApp()
void openFolderInFileBrowser(const AbstractPath& folderPath); //throw FileError
}

#endif //FOLDER_SELECTOR_H_24857842375234523463425

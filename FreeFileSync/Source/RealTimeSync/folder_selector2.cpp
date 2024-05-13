// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "folder_selector2.h"
#include <zen/thread.h>
#include <zen/file_access.h>
#include <wx/scrolwin.h>
#include <wx/dirdlg.h>
#include <wx+/popup_dlg.h>
#include <zen/resolve_path.h>
    #include <gtk/gtk.h>

using namespace zen;
using namespace rts;


namespace
{
constexpr std::chrono::milliseconds FOLDER_SELECTED_EXISTENCE_CHECK_TIME_MAX(200);


void setFolderPath(const Zstring& dirpath, wxTextCtrl* txtCtrl, wxWindow& tooltipWnd, wxStaticText* staticText) //pointers are optional
{
    if (txtCtrl)
        txtCtrl->ChangeValue(utfTo<wxString>(dirpath));

    const Zstring folderPathFmt = getResolvedFilePath(dirpath); //may block when resolving [<volume name>]

    if (folderPathFmt.empty())
        tooltipWnd.UnsetToolTip(); //wxGTK doesn't allow wxToolTip with empty text!
    else
        tooltipWnd.SetToolTip(utfTo<wxString>(folderPathFmt));

    if (staticText) //change static box label only if there is a real difference to what is shown in wxTextCtrl anyway
        staticText->SetLabel(equalNativePath(appendSeparator(trimCpy(dirpath)), appendSeparator(folderPathFmt)) ?
                             wxString(_("Drag && drop")) : utfTo<wxString>(folderPathFmt));
}


}

//##############################################################################################################

FolderSelector2::FolderSelector2(wxWindow*     parent,
                                 wxWindow&     dropWindow,
                                 wxButton&     selectButton,
                                 wxTextCtrl&   folderPathCtrl,
                                 Zstring& folderLastSelected,
                                 wxStaticText* staticText,
                                 const std::function<bool  (const std::vector<Zstring>& shellItemPaths)>& droppedPathsFilter) :
    droppedPathsFilter_  (droppedPathsFilter),
    parent_(parent),
    dropWindow_(dropWindow),
    selectButton_(selectButton),
    folderPathCtrl_(folderPathCtrl),
    folderLastSelected_(folderLastSelected),
    staticText_(staticText)
{
    //file drag and drop directly into the text control unhelpfully inserts in format "file://..<cr><nl>"; see folder_history_box.cpp
    if (GtkWidget* widget = folderPathCtrl.GetConnectWidget())
        ::gtk_drag_dest_unset(widget);

    setupFileDrop(dropWindow_);
    dropWindow_.Bind(EVENT_DROP_FILE, &FolderSelector2::onFilesDropped, this);

    //keep folderSelector and dirpath synchronous
    folderPathCtrl_.Bind(wxEVT_MOUSEWHEEL,             &FolderSelector2::onMouseWheel,     this);
    folderPathCtrl_.Bind(wxEVT_COMMAND_TEXT_UPDATED,   &FolderSelector2::onEditFolderPath, this);
    selectButton_  .Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FolderSelector2::onSelectDir,      this);
}


FolderSelector2::~FolderSelector2()
{
    [[maybe_unused]] bool ubOk1 = dropWindow_.Unbind(EVENT_DROP_FILE, &FolderSelector2::onFilesDropped, this);

    [[maybe_unused]] bool ubOk2 = folderPathCtrl_.Unbind(wxEVT_MOUSEWHEEL,             &FolderSelector2::onMouseWheel,     this);
    [[maybe_unused]] bool ubOk3 = folderPathCtrl_.Unbind(wxEVT_COMMAND_TEXT_UPDATED,   &FolderSelector2::onEditFolderPath, this);
    [[maybe_unused]] bool ubOk4 = selectButton_  .Unbind(wxEVT_COMMAND_BUTTON_CLICKED, &FolderSelector2::onSelectDir,      this);
    assert(ubOk1 && ubOk2 && ubOk3 && ubOk4);
}


void FolderSelector2::onMouseWheel(wxMouseEvent& event)
{
    //for combobox: although switching through available items is wxWidgets default, this is NOT Windows default, e.g. Explorer
    //additionally this will delete manual entries, although all the users wanted is scroll the parent window!

    //redirect to parent scrolled window!
    for (wxWindow* wnd = folderPathCtrl_.GetParent(); wnd; wnd = wnd->GetParent())
        if (dynamic_cast<wxScrolledWindow*>(wnd) != nullptr)
            return wnd->GetEventHandler()->AddPendingEvent(event);
    assert(false);
    event.Skip();
}


void FolderSelector2::onFilesDropped(FileDropEvent& event)
{
    if (event.itemPaths_.empty())
        return;

    if (!droppedPathsFilter_ || droppedPathsFilter_(event.itemPaths_))
    {
        Zstring itemPath = event.itemPaths_[0];
        try
        {
            if (getItemType(itemPath) == ItemType::file) //throw FileError
                if (const std::optional<Zstring>& parentPath = getParentFolderPath(itemPath))
                    itemPath = *parentPath;
        }
        catch (FileError&) {} //e.g. good for inactive mapped network shares, not so nice for C:\pagefile.sys

        if (endsWith(itemPath, Zstr(' '))) //prevent getResolvedFilePath() from trimming legit trailing blank!
            itemPath += FILE_NAME_SEPARATOR;

        setPath(itemPath);
    }
    //event.Skip();
}


void FolderSelector2::onEditFolderPath(wxCommandEvent& event)
{
    setFolderPath(utfTo<Zstring>(event.GetString()), nullptr, folderPathCtrl_, staticText_);
    event.Skip();
}


void FolderSelector2::onSelectDir(wxCommandEvent& event)
{
    //IFileDialog requirements for default path: 1. accepts native paths only!!! 2. path must exist!
    Zstring defaultFolderPath;
    {
        auto folderAccessible = [stopTime = std::chrono::steady_clock::now() + FOLDER_SELECTED_EXISTENCE_CHECK_TIME_MAX](const Zstring& folderPath)
        {
            auto ft = runAsync([folderPath]
            {
                try
                {
                    return getItemType(folderPath) != ItemType::file; //throw FileError
                }
                catch (FileError&) { return false; }
            });

            return ft.wait_until(stopTime) == std::future_status::ready && ft.get(); //potentially slow network access: wait 200ms at most
        };

        auto trySetDefaultPath = [&](const Zstring& folderPathPhrase)
        {

            if (const Zstring folderPath = getResolvedFilePath(folderPathPhrase);
                !folderPath.empty())
                if (folderAccessible(folderPath))
                    defaultFolderPath = folderPath;
        };

        const Zstring& currentFolderPath = getPath();
        trySetDefaultPath(currentFolderPath);

        if (defaultFolderPath.empty() && //=> fallback: use last user-selected path
            trimCpy(folderLastSelected_) != trimCpy(currentFolderPath) /*case-sensitive comp for path phrase!*/)
            trySetDefaultPath(folderLastSelected_);
    }

    Zstring newFolderPath;
    wxDirDialog folderSelector(parent_, _("Select a folder"), utfTo<wxString>(defaultFolderPath), wxDD_DEFAULT_STYLE | wxDD_SHOW_HIDDEN);
    if (folderSelector.ShowModal() != wxID_OK)
        return;
    newFolderPath = utfTo<Zstring>(folderSelector.GetPath());
    if (endsWith(newFolderPath, Zstr(' '))) //prevent getResolvedFilePath() from trimming legit trailing blank!
        newFolderPath += FILE_NAME_SEPARATOR;

    setPath(newFolderPath);
    folderLastSelected_ = newFolderPath;
}


Zstring FolderSelector2::getPath() const
{
    return utfTo<Zstring>(folderPathCtrl_.GetValue());
}


void FolderSelector2::setPath(const Zstring& dirpath)
{
    setFolderPath(dirpath, &folderPathCtrl_, folderPathCtrl_, staticText_);
}

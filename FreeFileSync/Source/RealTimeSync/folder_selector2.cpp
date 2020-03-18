// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "folder_selector2.h"
#include <zen/thread.h>
#include <zen/file_access.h>
#include <wx/dirdlg.h>
#include <wx/scrolwin.h>
#include <wx+/popup_dlg.h>
#include "../base/resolve_path.h"
    #include <gtk/gtk.h>

using namespace zen;
using namespace rts;


namespace
{
const std::chrono::milliseconds FOLDER_SELECTED_EXISTENCE_CHECK_TIME_MAX(200);


void setFolderPath(const Zstring& dirpath, wxTextCtrl* txtCtrl, wxWindow& tooltipWnd, wxStaticText* staticText) //pointers are optional
{
    if (txtCtrl)
        txtCtrl->ChangeValue(utfTo<wxString>(dirpath));

    const Zstring folderPathFmt = fff::getResolvedFilePath(dirpath); //may block when resolving [<volume name>]

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
                                 wxStaticText* staticText) :
    parent_(parent),
    dropWindow_(dropWindow),
    selectButton_(selectButton),
    folderPathCtrl_(folderPathCtrl),
    staticText_(staticText)
{
    //file drag and drop directly into the text control unhelpfully inserts in format "file://..<cr><nl>"; see folder_history_box.cpp
    if (GtkWidget* widget = folderPathCtrl.GetConnectWidget())
        ::gtk_drag_dest_unset(widget);

    //prepare drag & drop
    setupFileDrop(dropWindow_);
    dropWindow_.Connect(EVENT_DROP_FILE, FileDropEventHandler(FolderSelector2::onFilesDropped), nullptr, this);

    //keep dirPicker and dirpath synchronous
    folderPathCtrl_.Connect(wxEVT_MOUSEWHEEL,             wxMouseEventHandler  (FolderSelector2::onMouseWheel    ), nullptr, this);
    folderPathCtrl_.Connect(wxEVT_COMMAND_TEXT_UPDATED,   wxCommandEventHandler(FolderSelector2::onEditFolderPath), nullptr, this);
    selectButton_  .Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FolderSelector2::onSelectDir     ), nullptr, this);
}


FolderSelector2::~FolderSelector2()
{
    dropWindow_.Disconnect(EVENT_DROP_FILE, FileDropEventHandler(FolderSelector2::onFilesDropped), nullptr, this);

    folderPathCtrl_.Disconnect(wxEVT_MOUSEWHEEL,             wxMouseEventHandler  (FolderSelector2::onMouseWheel    ), nullptr, this);
    folderPathCtrl_.Disconnect(wxEVT_COMMAND_TEXT_UPDATED,   wxCommandEventHandler(FolderSelector2::onEditFolderPath), nullptr, this);
    selectButton_  .Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FolderSelector2::onSelectDir     ), nullptr, this);
}


void FolderSelector2::onMouseWheel(wxMouseEvent& event)
{
    //for combobox: although switching through available items is wxWidgets default, this is NOT windows default, e.g. Explorer
    //additionally this will delete manual entries, although all the users wanted is scroll the parent window!

    //redirect to parent scrolled window!
    wxWindow* wnd = &folderPathCtrl_;
    while ((wnd = wnd->GetParent()) != nullptr) //silence MSVC warning
        if (dynamic_cast<wxScrolledWindow*>(wnd) != nullptr)
            if (wxEvtHandler* evtHandler = wnd->GetEventHandler())
            {
                evtHandler->AddPendingEvent(event);
                break;
            }
    //  event.Skip();
}


void FolderSelector2::onFilesDropped(FileDropEvent& event)
{
    const auto& itemPaths = event.getPaths();
    if (itemPaths.empty())
        return;

    Zstring itemPath = itemPaths[0];
    try
    {
        if (getItemType(itemPath) == ItemType::FILE) //throw FileError
            if (std::optional<Zstring> parentPath = getParentFolderPath(itemPath))
                itemPath = *parentPath;
    }
    catch (FileError&) {} //e.g. good for inactive mapped network shares, not so nice for C:\pagefile.sys

    setPath(itemPath);

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
        const Zstring folderPath = fff::getResolvedFilePath(getPath());
        if (!folderPath.empty())
        {
            auto ft = runAsync([folderPath] { return dirAvailable(folderPath); });

            if (ft.wait_for(FOLDER_SELECTED_EXISTENCE_CHECK_TIME_MAX) == std::future_status::ready && ft.get()) //potentially slow network access: wait 200ms at most
                defaultFolderPath = folderPath;
        }
    }

    wxDirDialog dirPicker(parent_, _("Select a folder"), utfTo<wxString>(defaultFolderPath)); //put modal wxWidgets dialogs on stack: creating on freestore leads to memleak!
    if (dirPicker.ShowModal() != wxID_OK)
        return;
    const Zstring newFolderPath = utfTo<Zstring>(dirPicker.GetPath());

    setFolderPath(newFolderPath, &folderPathCtrl_, folderPathCtrl_, staticText_);
}


Zstring FolderSelector2::getPath() const
{
    return utfTo<Zstring>(folderPathCtrl_.GetValue());
}


void FolderSelector2::setPath(const Zstring& dirpath)
{
    setFolderPath(dirpath, &folderPathCtrl_, folderPathCtrl_, staticText_);
}

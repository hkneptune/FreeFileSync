// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "folder_selector.h"
#include <zen/thread.h>
#include <zen/file_access.h>
#include <wx/dirdlg.h>
#include <wx/scrolwin.h>
#include <wx+/popup_dlg.h>
#include <wx+/context_menu.h>
#include <wx+/image_resources.h>
#include "small_dlgs.h" //includes structures.h, which defines "AFS"
#include "../afs/concrete.h"
#include "../afs/native.h"
#include "../base/icon_buffer.h"



using namespace zen;
using namespace fff;


namespace
{
const std::chrono::milliseconds FOLDER_SELECTED_EXISTENCE_CHECK_TIME_MAX(200);


void setFolderPathPhrase(const Zstring& folderPathPhrase, FolderHistoryBox* comboBox, wxWindow& tooltipWnd, wxStaticText* staticText) //pointers are optional
{
    if (comboBox)
        comboBox->setValue(utfTo<wxString>(folderPathPhrase));

    const Zstring folderPathPhraseFmt = AFS::getInitPathPhrase(createAbstractPath(folderPathPhrase)); //noexcept
    //may block when resolving [<volume name>]

    if (folderPathPhraseFmt.empty())
        tooltipWnd.UnsetToolTip(); //wxGTK doesn't allow wxToolTip with empty text!
    else
        tooltipWnd.SetToolTip(utfTo<wxString>(folderPathPhraseFmt));

    if (staticText)
    {
        //change static box label only if there is a real difference to what is shown in wxTextCtrl anyway
        staticText->SetLabel(equalNoCase(appendSeparator(trimCpy(folderPathPhrase)), appendSeparator(folderPathPhraseFmt)) ?
                             wxString(_("Drag && drop")) : utfTo<wxString>(folderPathPhraseFmt));
    }
}

}

//##############################################################################################################

const wxEventType fff::EVENT_ON_FOLDER_SELECTED    = wxNewEventType();
const wxEventType fff::EVENT_ON_FOLDER_MANUAL_EDIT = wxNewEventType();


FolderSelector::FolderSelector(wxWindow*         parent,
                               wxWindow&         dropWindow,
                               wxButton&         selectFolderButton,
                               wxButton&         selectAltFolderButton,
                               FolderHistoryBox& folderComboBox,
                               wxStaticText*     staticText,
                               wxWindow*         dropWindow2,
                               const std::function<bool  (const std::vector<Zstring>& shellItemPaths)>&          droppedPathsFilter,
                               const std::function<size_t(const Zstring& folderPathPhrase)>&                     getDeviceParallelOps,
                               const std::function<void  (const Zstring& folderPathPhrase, size_t parallelOps)>& setDeviceParallelOps) :
    droppedPathsFilter_  (droppedPathsFilter),
    getDeviceParallelOps_(getDeviceParallelOps),
    setDeviceParallelOps_(setDeviceParallelOps),
    parent_(parent),
    dropWindow_(dropWindow),
    dropWindow2_(dropWindow2),
    selectFolderButton_(selectFolderButton),
    selectAltFolderButton_(selectAltFolderButton),
    folderComboBox_(folderComboBox),
    staticText_(staticText)
{
    assert(getDeviceParallelOps_);

    auto setupDragDrop = [&](wxWindow& dropWin)
    {
        setupFileDrop(dropWin);
        dropWin.Connect(EVENT_DROP_FILE, FileDropEventHandler(FolderSelector::onItemPathDropped), nullptr, this);
    };

    setupDragDrop(dropWindow_);
    if (dropWindow2_) setupDragDrop(*dropWindow2_);

    selectAltFolderButton_.SetBitmapLabel(getResourceImage(L"cloud_small"));

    //keep dirPicker and dirpath synchronous
    folderComboBox_       .Connect(wxEVT_MOUSEWHEEL,             wxMouseEventHandler  (FolderSelector::onMouseWheel     ), nullptr, this);
    folderComboBox_       .Connect(wxEVT_COMMAND_TEXT_UPDATED,   wxCommandEventHandler(FolderSelector::onEditFolderPath ), nullptr, this);
    selectFolderButton_   .Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FolderSelector::onSelectFolder   ), nullptr, this);
    selectAltFolderButton_.Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FolderSelector::onSelectAltFolder), nullptr, this);
    //selectAltFolderButton_.Connect(wxEVT_RIGHT_DOWN,             wxCommandEventHandler(FolderSelector::onSelectAltFolder), nullptr, this);
}


FolderSelector::~FolderSelector()
{
    dropWindow_.Disconnect(EVENT_DROP_FILE, FileDropEventHandler(FolderSelector::onItemPathDropped), nullptr, this);

    if (dropWindow2_)
        dropWindow2_->Disconnect(EVENT_DROP_FILE, FileDropEventHandler(FolderSelector::onItemPathDropped), nullptr, this);

    folderComboBox_       .Disconnect(wxEVT_MOUSEWHEEL,             wxMouseEventHandler  (FolderSelector::onMouseWheel     ), nullptr, this);
    folderComboBox_       .Disconnect(wxEVT_COMMAND_TEXT_UPDATED,   wxCommandEventHandler(FolderSelector::onEditFolderPath ), nullptr, this);
    selectFolderButton_   .Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FolderSelector::onSelectFolder   ), nullptr, this);
    selectAltFolderButton_.Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FolderSelector::onSelectAltFolder), nullptr, this);
    //selectAltFolderButton_.Disconnect(wxEVT_RIGHT_DOWN,             wxCommandEventHandler(FolderSelector::onSelectAltFolder), nullptr, this);
}


void FolderSelector::onMouseWheel(wxMouseEvent& event)
{
    //for combobox: although switching through available items is wxWidgets default, this is NOT windows default, e.g. Explorer
    //additionally this will delete manual entries, although all the users wanted is scroll the parent window!

    //redirect to parent scrolled window!
    wxWindow* wnd = &folderComboBox_;
    while ((wnd = wnd->GetParent()) != nullptr) //silence MSVC warning
        if (dynamic_cast<wxScrolledWindow*>(wnd) != nullptr)
            if (wxEvtHandler* evtHandler = wnd->GetEventHandler())
            {
                evtHandler->AddPendingEvent(event);
                break;
            }
    //  event.Skip();
}


void FolderSelector::onItemPathDropped(FileDropEvent& event)
{
    const auto& itemPaths = event.getPaths();
    if (itemPaths.empty())
        return;

    if (!droppedPathsFilter_ || droppedPathsFilter_(itemPaths))
    {
        auto fmtShellPath = [](const Zstring& shellItemPath)
        {
            const AbstractPath itemPath = createAbstractPath(shellItemPath);
            try
            {
                if (AFS::getItemType(itemPath) == AFS::ItemType::FILE) //throw FileError
                    if (std::optional<AbstractPath> parentPath = AFS::getParentPath(itemPath))
                        return AFS::getInitPathPhrase(*parentPath);
            }
            catch (FileError&) {} //e.g. good for inactive mapped network shares, not so nice for C:\pagefile.sys
            //make sure FFS-specific explicit MTP-syntax is applied!
            return AFS::getInitPathPhrase(itemPath);
        };

        setPath(fmtShellPath(itemPaths[0]));
        //drop two folder paths at once:
        if (siblingSelector_ && itemPaths.size() >= 2)
            siblingSelector_->setPath(fmtShellPath(itemPaths[1]));

        //notify action invoked by user
        wxCommandEvent dummy(EVENT_ON_FOLDER_SELECTED);
        ProcessEvent(dummy);
    }

    //event.Skip(); //let other handlers try -> are there any??
}


void FolderSelector::onEditFolderPath(wxCommandEvent& event)
{
    setFolderPathPhrase(utfTo<Zstring>(event.GetString()), nullptr, folderComboBox_, staticText_);

    wxCommandEvent dummy(EVENT_ON_FOLDER_MANUAL_EDIT);
    ProcessEvent(dummy);
    event.Skip();
}


void FolderSelector::onSelectFolder(wxCommandEvent& event)
{
    //make sure default folder exists: don't let folder picker hang on non-existing network share!
    Zstring defaultFolderPath;
    {
        auto folderExistsTimed = [](const AbstractPath& folderPath)
        {
            auto ft = runAsync([folderPath]
            {
                try
                {
                    return AFS::getItemType(folderPath) != AFS::ItemType::FILE; //throw FileError
                }
                catch (FileError&) { return false; }
            });
            return ft.wait_for(FOLDER_SELECTED_EXISTENCE_CHECK_TIME_MAX) == std::future_status::ready && ft.get(); //potentially slow network access: wait 200ms at most
        };

        const Zstring folderPathPhrase = getPath();
        if (acceptsItemPathPhraseNative(folderPathPhrase)) //noexcept
        {
            const AbstractPath folderPath = createItemPathNative(folderPathPhrase);
            if (folderExistsTimed(folderPath))
                if (std::optional<Zstring> nativeFolderPath = AFS::getNativeItemPath(folderPath))
                    defaultFolderPath = *nativeFolderPath;
        }
    }

    wxDirDialog dirPicker(parent_, _("Select a folder"), utfTo<wxString>(defaultFolderPath)); //put modal wxWidgets dialogs on stack: creating on freestore leads to memleak!

    //-> following doesn't seem to do anything at all! still "Show hidden" is available as a context menu option:
    //::gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dirPicker.m_widget), true /*show_hidden*/);

    if (dirPicker.ShowModal() != wxID_OK)
        return;
    const Zstring newFolderPathPhrase = utfTo<Zstring>(dirPicker.GetPath());

    setFolderPathPhrase(newFolderPathPhrase, &folderComboBox_, folderComboBox_, staticText_);

    //notify action invoked by user
    wxCommandEvent dummy(EVENT_ON_FOLDER_SELECTED);
    ProcessEvent(dummy);
}


void FolderSelector::onSelectAltFolder(wxCommandEvent& event)
{
    Zstring folderPathPhrase = getPath();
    size_t parallelOps = getDeviceParallelOps_ ? getDeviceParallelOps_(folderPathPhrase) : 1;

    std::optional<std::wstring> parallelOpsDisabledReason;

        parallelOpsDisabledReason = _("Requires FreeFileSync Donation Edition");

    if (showCloudSetupDialog(parent_, folderPathPhrase, parallelOps, get(parallelOpsDisabledReason)) != ReturnSmallDlg::BUTTON_OKAY)
        return;

    setFolderPathPhrase(folderPathPhrase, &folderComboBox_, folderComboBox_, staticText_);

    if (setDeviceParallelOps_)
        setDeviceParallelOps_(folderPathPhrase, parallelOps);

    //notify action invoked by user
    wxCommandEvent dummy(EVENT_ON_FOLDER_SELECTED);
    ProcessEvent(dummy);
}


Zstring FolderSelector::getPath() const
{
    return utfTo<Zstring>(folderComboBox_.GetValue());
}


void FolderSelector::setPath(const Zstring& folderPathPhrase)
{
    setFolderPathPhrase(folderPathPhrase, &folderComboBox_, folderComboBox_, staticText_);
}

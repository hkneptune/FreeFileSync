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
#include "../icon_buffer.h"



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

namespace fff
{
wxDEFINE_EVENT(EVENT_ON_FOLDER_SELECTED,    wxCommandEvent);
wxDEFINE_EVENT(EVENT_ON_FOLDER_MANUAL_EDIT, wxCommandEvent);
}


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
        dropWin.Bind(EVENT_DROP_FILE, &FolderSelector::onItemPathDropped, this);
    };

    setupDragDrop(dropWindow_);
    if (dropWindow2_)
        setupDragDrop(*dropWindow2_);

    selectAltFolderButton_.SetBitmapLabel(loadImage("cloud_small"));

    //keep dirPicker and dirpath synchronous
    folderComboBox_       .Bind(wxEVT_MOUSEWHEEL,                &FolderSelector::onMouseWheel,          this);
    folderComboBox_       .Bind(wxEVT_COMMAND_TEXT_UPDATED,      &FolderSelector::onEditFolderPath,      this);
    folderComboBox_       .Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, &FolderSelector::onHistoryPathSelected, this);
    selectFolderButton_   .Bind(wxEVT_COMMAND_BUTTON_CLICKED,    &FolderSelector::onSelectFolder,        this);
    selectAltFolderButton_.Bind(wxEVT_COMMAND_BUTTON_CLICKED,    &FolderSelector::onSelectAltFolder,     this);
}


FolderSelector::~FolderSelector()
{
    [[maybe_unused]] bool ubOk1 = dropWindow_.Unbind(EVENT_DROP_FILE, &FolderSelector::onItemPathDropped, this);
    [[maybe_unused]] bool ubOk2 = true;
    if (dropWindow2_)
        ubOk2 = dropWindow2_->Unbind(EVENT_DROP_FILE, &FolderSelector::onItemPathDropped, this);

    [[maybe_unused]] bool ubOk3 = folderComboBox_       .Unbind(wxEVT_MOUSEWHEEL,                &FolderSelector::onMouseWheel,          this);
    [[maybe_unused]] bool ubOk4 = folderComboBox_       .Unbind(wxEVT_COMMAND_TEXT_UPDATED,      &FolderSelector::onEditFolderPath,      this);
    [[maybe_unused]] bool ubOk5 = folderComboBox_       .Unbind(wxEVT_COMMAND_COMBOBOX_SELECTED, &FolderSelector::onHistoryPathSelected, this);
    [[maybe_unused]] bool ubOk6 = selectFolderButton_   .Unbind(wxEVT_COMMAND_BUTTON_CLICKED,    &FolderSelector::onSelectFolder,        this);
    [[maybe_unused]] bool ubOk7 = selectAltFolderButton_.Unbind(wxEVT_COMMAND_BUTTON_CLICKED,    &FolderSelector::onSelectAltFolder,     this);
    assert(ubOk1 && ubOk2 && ubOk3 && ubOk4 && ubOk5 && ubOk6 && ubOk7);
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
    if (event.itemPaths_.empty())
        return;

    if (!droppedPathsFilter_ || droppedPathsFilter_(event.itemPaths_))
    {
        auto fmtShellPath = [](Zstring shellItemPath)
        {
            if (endsWith(shellItemPath, Zstr(' '))) //prevent createAbstractPath() from trimming legit trailing blank!
                shellItemPath += FILE_NAME_SEPARATOR;

            const AbstractPath itemPath = createAbstractPath(shellItemPath);
            try
            {
                if (AFS::getItemType(itemPath) == AFS::ItemType::file) //throw FileError
                    if (const std::optional<AbstractPath> parentPath = AFS::getParentPath(itemPath))
                        return AFS::getInitPathPhrase(*parentPath);
            }
            catch (FileError&) {} //e.g. good for inactive mapped network shares, not so nice for C:\pagefile.sys
            //make sure FFS-specific explicit MTP-syntax is applied!
            return AFS::getInitPathPhrase(itemPath);
        };

        setPath(fmtShellPath(event.itemPaths_[0]));
        //drop two folder paths at once:
        if (siblingSelector_ && event.itemPaths_.size() >= 2)
            siblingSelector_->setPath(fmtShellPath(event.itemPaths_[1]));

        //notify action invoked by user
        wxCommandEvent dummy(EVENT_ON_FOLDER_SELECTED);
        ProcessEvent(dummy);
    }

    //event.Skip(); //let other handlers try -> are there any??
}


void FolderSelector::onHistoryPathSelected(wxEvent& event)
{
    //setFolderPathPhrase() => already called by onEditFolderPath() (wxEVT_COMMAND_COMBOBOX_SELECTED implies wxEVT_COMMAND_TEXT_UPDATED)

    //notify action invoked by user
    wxCommandEvent dummy(EVENT_ON_FOLDER_SELECTED);
    ProcessEvent(dummy);
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
    Zstring defaultFolderPath;
    {
        //make sure default folder exists: don't let folder picker hang on non-existing network share!
        auto folderExistsTimed = [](const AbstractPath& folderPath)
        {
            auto ft = runAsync([folderPath]
            {
                try
                {
                    return AFS::getItemType(folderPath) != AFS::ItemType::file; //throw FileError
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

    Zstring shellItemPath;
    wxDirDialog dirPicker(parent_, _("Select a folder"), utfTo<wxString>(defaultFolderPath), wxDD_DEFAULT_STYLE | wxDD_SHOW_HIDDEN);
    //GTK2: "Show hidden" is also available as a context menu option in the folder picker!
    //It looks like wxDD_SHOW_HIDDEN only sets the default when opening for the first time!?

    if (dirPicker.ShowModal() != wxID_OK)
        return;
    shellItemPath = utfTo<Zstring>(dirPicker.GetPath());
    if (endsWith(shellItemPath, Zstr(' '))) //prevent createAbstractPath() from trimming legit trailing blank!
        shellItemPath += FILE_NAME_SEPARATOR;

    //make sure FFS-specific explicit MTP-syntax is applied!
    const Zstring newFolderPathPhrase = AFS::getInitPathPhrase(createAbstractPath(shellItemPath)); //noexcept

    setPath(newFolderPathPhrase);

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

    setPath(folderPathPhrase);

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

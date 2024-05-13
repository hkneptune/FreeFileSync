// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "folder_selector.h"
#include <zen/thread.h>
//#include <zen/file_access.h>
#include <zen/process_exec.h>
#include <wx/scrolwin.h>
#include <wx+/bitmap_button.h>
#include <wx+/popup_dlg.h>
//#include <wx+/context_menu.h>
#include <wx+/image_resources.h>
#include "small_dlgs.h" //includes structures.h, which defines "AFS"
#include "../afs/concrete.h"
#include "../afs/native.h"
//#include "../icon_buffer.h"
#include "../afs/gdrive.h"

    #include <wx/dirdlg.h>


using namespace zen;
using namespace fff;


namespace
{
constexpr std::chrono::milliseconds FOLDER_SELECTED_EXISTENCE_CHECK_TIME_MAX(200);


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

    auto trimTrailingSep = [](Zstring path)
    {
        if (endsWith(path, Zstr('/')) ||
            endsWith(path, Zstr('\\')))
            path.pop_back();
        return path;
    };

    if (staticText) //change static box label only if there is a real difference to what is shown in wxTextCtrl anyway
        staticText->SetLabel(equalNoCase(trimTrailingSep(trimCpy(folderPathPhrase)),
                                         trimTrailingSep(folderPathPhraseFmt)) ?
                             wxString(_("Drag && drop")) : utfTo<wxString>(folderPathPhraseFmt));
}


}

//##############################################################################################################

namespace fff
{
wxDEFINE_EVENT(EVENT_ON_FOLDER_SELECTED, wxCommandEvent);
}


FolderSelector::FolderSelector(wxWindow*         parent,
                               wxWindow&         dropWindow,
                               wxButton&         selectFolderButton,
                               wxButton&         selectAltFolderButton,
                               FolderHistoryBox& folderComboBox,
                               Zstring& folderLastSelected, Zstring& sftpKeyFileLastSelected,
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
    folderLastSelected_(folderLastSelected),
    sftpKeyFileLastSelected_(sftpKeyFileLastSelected),
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

    setImage(selectAltFolderButton_, loadImage("cloud_small"));

    //keep folderSelector and dirpath synchronous
    folderComboBox_       .Bind(wxEVT_MOUSEWHEEL,                &FolderSelector::onMouseWheel,          this);
    folderComboBox_       .Bind(wxEVT_COMMAND_TEXT_UPDATED,      &FolderSelector::onEditFolderPath,      this);
    //folderComboBox_.Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, &FolderSelector::onHistoryPathSelected, this);
    // => wxEVT_COMMAND_COMBOBOX_SELECTED implies wxEVT_COMMAND_TEXT_UPDATED
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
    //[[maybe_unused]] bool ubOk5 = folderComboBox_ .Unbind(wxEVT_COMMAND_COMBOBOX_SELECTED, &FolderSelector::onHistoryPathSelected, this);
    // => wxEVT_COMMAND_COMBOBOX_SELECTED implies wxEVT_COMMAND_TEXT_UPDATED
    [[maybe_unused]] bool ubOk6 = selectFolderButton_   .Unbind(wxEVT_COMMAND_BUTTON_CLICKED,    &FolderSelector::onSelectFolder,        this);
    [[maybe_unused]] bool ubOk7 = selectAltFolderButton_.Unbind(wxEVT_COMMAND_BUTTON_CLICKED,    &FolderSelector::onSelectAltFolder,     this);
    assert(ubOk1 && ubOk2 && ubOk3 && ubOk4 && /*ubOk5 &&*/ ubOk6 && ubOk7);
}


void FolderSelector::onMouseWheel(wxMouseEvent& event)
{
    //for combobox: although switching through available items is wxWidgets default, this is NOT Windows default, e.g. Explorer
    //additionally this will delete manual entries, although all the users wanted is scroll the parent window!

    //redirect to parent scrolled window!
    for (wxWindow* wnd = folderComboBox_.GetParent(); wnd; wnd = wnd->GetParent())
        if (dynamic_cast<wxScrolledWindow*>(wnd))
            if (wxEvtHandler* evtHandler = wnd->GetEventHandler())
                return evtHandler->AddPendingEvent(event);
    assert(false); //get here when attempting to scroll first folder pair (which is not inside a wxScrolledWindow)
    //event.Skip();
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


void FolderSelector::onEditFolderPath(wxCommandEvent& event)
{
    setFolderPathPhrase(utfTo<Zstring>(event.GetString()), nullptr, folderComboBox_, staticText_);

    wxCommandEvent dummy(EVENT_ON_FOLDER_SELECTED);
    ProcessEvent(dummy);
    event.Skip();
}


void FolderSelector::onSelectFolder(wxCommandEvent& event)
{
    Zstring defaultFolderNative;
    {
        //make sure default folder exists: don't let folder picker hang on non-existing network share!
        auto folderAccessible = [stopTime = std::chrono::steady_clock::now() + FOLDER_SELECTED_EXISTENCE_CHECK_TIME_MAX](const AbstractPath& folderPath)
        {
            if (AFS::isNullPath(folderPath))
                return false;

            auto ft = runAsync([folderPath]
            {
                try
                {
                    return AFS::getItemType(folderPath) != AFS::ItemType::file; //throw FileError
                }
                catch (FileError&) { return false; }
            });
            return ft.wait_until(stopTime) == std::future_status::ready && ft.get(); //potentially slow network access: wait 200ms at most
        };

        auto trySetDefaultPath = [&](const Zstring& folderPathPhrase)
        {
            if (acceptsItemPathPhraseNative(folderPathPhrase)) //noexcept
            {
                const AbstractPath folderPath = createItemPathNative(folderPathPhrase);
                if (folderAccessible(folderPath))
                    if (const Zstring& nativePath = getNativeItemPath(folderPath);
                        !nativePath.empty())
                        defaultFolderNative = nativePath;
            }
        };
        const Zstring& currentFolderPath = getPath();
        trySetDefaultPath(currentFolderPath);

        if (defaultFolderNative.empty() && //=> fallback: use last user-selected path
            trimCpy(folderLastSelected_) != trimCpy(currentFolderPath) /*case-sensitive comp for path phrase!*/)
            trySetDefaultPath(folderLastSelected_);
    }

    Zstring shellItemPath;
    //default size? Windows: not implemented, Linux(GTK2): not implemented, macOS: not implemented => wxWidgets, what is this shit!?
    wxDirDialog folderSelector(parent_, _("Select a folder"), utfTo<wxString>(defaultFolderNative), wxDD_DEFAULT_STYLE | wxDD_SHOW_HIDDEN);
    //GTK2: "Show hidden" is also available as a context menu option in the folder picker!
    //It looks like wxDD_SHOW_HIDDEN only sets the default when opening for the first time!?
    if (folderSelector.ShowModal() != wxID_OK)
        return;
    shellItemPath = utfTo<Zstring>(folderSelector.GetPath());
    if (endsWith(shellItemPath, Zstr(' '))) //prevent createAbstractPath() from trimming legit trailing blank!
        shellItemPath += FILE_NAME_SEPARATOR;

    //make sure FFS-specific explicit MTP-syntax is applied!
    const Zstring newFolderPathPhrase = AFS::getInitPathPhrase(createAbstractPath(shellItemPath)); //noexcept

    setPath(newFolderPathPhrase);
    folderLastSelected_ = newFolderPathPhrase;

    //notify action invoked by user
    wxCommandEvent dummy(EVENT_ON_FOLDER_SELECTED);
    ProcessEvent(dummy);
}


void FolderSelector::onSelectAltFolder(wxCommandEvent& event)
{
    Zstring folderPathPhrase = getPath();
    size_t parallelOps = getDeviceParallelOps_ ? getDeviceParallelOps_(folderPathPhrase) : 1;

    const AbstractPath oldPath = createAbstractPath(folderPathPhrase);

    if (showCloudSetupDialog(parent_, folderPathPhrase, sftpKeyFileLastSelected_, parallelOps, static_cast<bool>(setDeviceParallelOps_)) != ConfirmationButton::accept)
        return;

    setPath(folderPathPhrase);

    if (setDeviceParallelOps_)
        setDeviceParallelOps_(folderPathPhrase, parallelOps);

    //notify action invoked by user
    if (createAbstractPath(folderPathPhrase) != oldPath)
    {
        wxCommandEvent dummy(EVENT_ON_FOLDER_SELECTED);
        ProcessEvent(dummy);
    }
    //else: don't notify if user only changed connection settings, e.g. parallel Ops
}


Zstring FolderSelector::getPath() const
{
    return utfTo<Zstring>(folderComboBox_.GetValue());
}


void FolderSelector::setPath(const Zstring& folderPathPhrase)
{
    setFolderPathPhrase(folderPathPhrase, &folderComboBox_, folderComboBox_, staticText_);
}


void fff::openFolderInFileBrowser(const AbstractPath& folderPath) //throw FileError
{
        if (const Zstring& gdriveUrl = getGoogleDriveFolderUrl(folderPath); //throw FileError
            !gdriveUrl.empty())
            return openWithDefaultApp(gdriveUrl); //throw FileError
        else
            openWithDefaultApp(utfTo<Zstring>(AFS::getDisplayPath(folderPath))); //throw FileError
}

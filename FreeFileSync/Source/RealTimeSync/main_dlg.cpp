// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "main_dlg.h"
#include <wx/wupdlock.h>
#include <wx/filedlg.h>
#include <wx+/app_main.h>
#include <wx+/bitmap_button.h>
#include <wx+/window_layout.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include <zen/file_access.h>
#include <zen/build_info.h>
#include <zen/shutdown.h>
#include <zen/time.h>
#include "config.h"
#include "tray_menu.h"
#include "app_icon.h"
#include "../icon_buffer.h"
#include "../ffs_paths.h"
#include "../version/version.h"

    #include <gtk/gtk.h>

using namespace zen;
using namespace rts;


namespace
{
    static const size_t MAX_ADD_FOLDERS = 6;


std::wstring extractJobName(const Zstring& cfgFilePath)
{
    const Zstring fileName = getItemName(cfgFilePath);
    const Zstring jobName  = beforeLast(fileName, Zstr('.'), IfNotFoundReturn::all);
    return utfTo<std::wstring>(jobName);
}


bool acceptDialogFileDrop(const std::vector<Zstring>& shellItemPaths)
{
    if (shellItemPaths.empty())
        return false;

    const Zstring ext = getFileExtension(shellItemPaths[0]);
    return equalAsciiNoCase(ext, "ffs_real") ||
           equalAsciiNoCase(ext, "ffs_batch");
}
}


std::function<bool(const std::vector<Zstring>& shellItemPaths)> getDroppedPathsFilter(MainDialog& mainDlg)
{
    return [&mainDlg](const std::vector<Zstring>& shellItemPaths)
    {
        if (acceptDialogFileDrop(shellItemPaths))
        {
            assert(!shellItemPaths.empty());
            mainDlg.loadConfig(shellItemPaths[0]);
            return false; //don't set dropped paths
        }
        return true; //do set dropped paths
    };
}


class rts::DirectoryPanel : public FolderGenerated
{
public:
    DirectoryPanel(wxWindow* parent, MainDialog& mainDlg, Zstring& folderLastSelected) :
        FolderGenerated(parent),
        folderSelector_(parent, *this, *m_buttonSelectFolder, *m_txtCtrlDirectory, folderLastSelected, nullptr /*staticText*/, getDroppedPathsFilter(mainDlg))
    {
        setImage(*m_bpButtonRemoveFolder, loadImage("item_remove"));
    }

    void setPath(const Zstring& dirpath) { folderSelector_.setPath(dirpath); }
    Zstring getPath() const { return folderSelector_.getPath(); }

private:
    FolderSelector2 folderSelector_;
};


void MainDialog::create(const Zstring& cfgFilePath)
{
    /*MainDialog* frame = */ new MainDialog(cfgFilePath);
}


MainDialog::MainDialog(const Zstring& cfgFilePath) :
    MainDlgGenerated(nullptr),
    lastRunConfigPath_(appendPath(fff::getConfigDirPath(), Zstr("LastRun.ffs_real")))
{
    SetIcon(getRtsIcon()); //set application icon

    setRelativeFontSize(*m_buttonStart, 1.5);

    const int scrollDelta = m_buttonSelectFolderMain->GetSize().y; //more approriate than GetCharHeight() here
    m_scrolledWinFolders->SetScrollRate(scrollDelta, scrollDelta);

    m_txtCtrlDirectoryMain->SetMinSize({dipToWxsize(300), -1});
    setDefaultWidth(*m_spinCtrlDelay);

    m_bpButtonRemoveTopFolder->Hide();
    m_panelMainFolder->Layout();

    setImage(*m_bitmapBatch,   loadImage("cfg_batch", dipToScreen(20)));
    setImage(*m_bitmapFolders, fff::IconBuffer::genericDirIcon(fff::IconBuffer::IconSize::small));
    setImage(*m_bitmapConsole, loadImage("command_line", dipToScreen(20)));

    setImage(*m_bpButtonAddFolder,       loadImage("item_add"));
    setImage(*m_bpButtonRemoveTopFolder, loadImage("item_remove"));
    setBitmapTextLabel(*m_buttonStart, loadImage("start_rts"), m_buttonStart->GetLabelText(), dipToWxsize(5), dipToWxsize(8));

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); });


    //notify about (logical) application main window => program won't quit, but stay on this dialog
    setGlobalWindow(this);

    //prepare drag & drop
    firstFolderPanel_ = std::make_unique<FolderSelector2>(this, *m_panelMainFolder, *m_buttonSelectFolderMain, *m_txtCtrlDirectoryMain, folderLastSelected_,
                                                          nullptr /*staticText*/, getDroppedPathsFilter(*this));

    //--------------------------- load config values ------------------------------------
    XmlRealConfig newConfig;

    Zstring currentConfigFile = cfgFilePath;
    if (currentConfigFile.empty())
        try
        {
            if (itemExists(lastRunConfigPath_)) //throw FileError
                currentConfigFile = lastRunConfigPath_;
        }
        catch (FileError&) { currentConfigFile = lastRunConfigPath_; } //access error? => user should be informed

    bool loadCfgSuccess = false;
    if (!currentConfigFile.empty())
        try
        {
            std::wstring warningMsg;
            std::tie(newConfig, warningMsg) = readRealOrBatchConfig(currentConfigFile); //throw FileError

            if (!warningMsg.empty())
                showNotificationDialog(this, DialogInfoType::warning, PopupDialogCfg().setDetailInstructions(warningMsg));

            loadCfgSuccess = warningMsg.empty();
        }
        catch (const FileError& e)
        {
            showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString()));
        }

    const bool startWatchingImmediately = loadCfgSuccess && !cfgFilePath.empty();

    setConfiguration(newConfig);
    setLastUsedConfig(currentConfigFile);
    //-----------------------------------------------------------------------------------------

    onSystemShutdownRegister(onBeforeSystemShutdownCookie_);

    Center(); //needs to be re-applied after a dialog size change! (see addFolder() within setConfiguration())

    if (startWatchingImmediately) //start watch mode directly
    {
        wxCommandEvent dummy2(wxEVT_COMMAND_BUTTON_CLICKED);
        this->onStart(dummy2);
        //don't Show()!
    }
    else
    {
        Show();
        m_buttonStart->SetFocus(); //don't "steal" focus if program is running from sys-tray"
    }

    //drag and drop .ffs_real and .ffs_batch on main dialog
    setupFileDrop(*this);
    Bind(EVENT_DROP_FILE, [this](FileDropEvent& event) { onFilesDropped(event); });
}


MainDialog::~MainDialog()
{
    const XmlRealConfig currentCfg = getConfiguration();
    try
    {
        writeConfig(currentCfg, lastRunConfigPath_); //throw FileError
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString()));
    }
}


void MainDialog::onBeforeSystemShutdown()
{
    try { writeConfig(getConfiguration(), lastRunConfigPath_); }
    catch (const FileError& e) { logExtraError(e.toString()); }
}


void MainDialog::onMenuAbout(wxCommandEvent& event)
{
    wxString build = utfTo<wxString>(fff::ffsVersion);
#ifndef wxUSE_UNICODE
#error what is going on?
#endif

    const wchar_t* const SPACED_BULLET = L" \u2022 ";
    build += SPACED_BULLET;

    build += LTR_MARK; //fix Arabic
    build += utfTo<wxString>(cpuArchName);

    build += SPACED_BULLET;
    build += utfTo<wxString>(formatTime(formatDateTag, getCompileTime()));

    showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().
                           setTitle(_("About")).
                           setMainInstructions(L"RealTimeSync" L"\n\n" + replaceCpy(_("Version: %x"), L"%x", build)));
}


void MainDialog::onLocalKeyEvent(wxKeyEvent& event)
{
    switch (event.GetKeyCode())
    {
        case WXK_ESCAPE:
            Close();
            return;
    }
    event.Skip();
}


void MainDialog::onStart(wxCommandEvent& event)
{
    Hide();

    XmlRealConfig currentCfg = getConfiguration();
    const Zstring activeCfgFilePath = !equalNativePath(activeConfigFile_, lastRunConfigPath_) ? activeConfigFile_ : Zstring();

    switch (runFolderMonitor(currentCfg, ::extractJobName(activeCfgFilePath)))
    {
        case CancelReason::requestExit:
            Close();
            return;

        case CancelReason::requestGui:
            break;
    }

    Show(); //don't show for CancelReason::requestExit
    Raise();
    m_buttonStart->SetFocus();
}


void MainDialog::onConfigSave(wxCommandEvent& event)
{
    const Zstring activeCfgFilePath = !equalNativePath(activeConfigFile_, lastRunConfigPath_) ? activeConfigFile_ : Zstring();

    std::optional<Zstring> defaultFolderPath = getParentFolderPath(activeCfgFilePath);

    Zstring defaultFileName = !activeCfgFilePath.empty() ?
                              getItemName(activeCfgFilePath) :
                              Zstr("RealTime.ffs_real");

    //attention: activeConfigFile_ may be an imported *.ffs_batch file! We don't want to overwrite it with a RTS config!
    defaultFileName = beforeLast(defaultFileName, Zstr('.'), IfNotFoundReturn::all) + Zstr(".ffs_real");

    wxFileDialog fileSelector(this, wxString() /*message*/,  utfTo<wxString>(defaultFolderPath ? *defaultFolderPath : Zstr("")), utfTo<wxString>(defaultFileName),
                              wxString(L"RealTimeSync (*.ffs_real)|*.ffs_real") + L"|" +_("All files") + L" (*.*)|*",
                              wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (fileSelector.ShowModal() != wxID_OK)
        return;

    Zstring targetFilePath = utfTo<Zstring>(fileSelector.GetPath());
    if (!endsWithAsciiNoCase(targetFilePath, Zstr(".ffs_real"))) //no weird shit!
        targetFilePath += Zstr(".ffs_real");          //https://freefilesync.org/forum/viewtopic.php?t=9451#p34724

    const XmlRealConfig currentCfg = getConfiguration();
    try
    {
        writeConfig(currentCfg, targetFilePath); //throw FileError
        setLastUsedConfig(targetFilePath);
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString()));
    }
}


void MainDialog::loadConfig(const Zstring& filepath)
{
    XmlRealConfig newConfig;

    if (!filepath.empty())
        try
        {
            std::wstring warningMsg;
            std::tie(newConfig, warningMsg) = readRealOrBatchConfig(filepath); //throw FileError

            if (!warningMsg.empty())
                showNotificationDialog(this, DialogInfoType::warning, PopupDialogCfg().setDetailInstructions(warningMsg));
        }
        catch (const FileError& e)
        {
            showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString()));
            return;
        }

    setConfiguration(newConfig);
    setLastUsedConfig(filepath);
}


void MainDialog::setLastUsedConfig(const Zstring& filepath)
{
    activeConfigFile_ = filepath;

    const Zstring activeCfgFilePath = !equalNativePath(activeConfigFile_, lastRunConfigPath_) ? activeConfigFile_ : Zstring();

    if (!activeCfgFilePath.empty())
        SetTitle(utfTo<wxString>(activeCfgFilePath));
    else
        SetTitle(L"RealTimeSync " + utfTo<std::wstring>(fff::ffsVersion) + SPACED_DASH + _("Automated Synchronization"));

}


void MainDialog::onConfigLoad(wxCommandEvent& event)
{
    const Zstring activeCfgFilePath = !equalNativePath(activeConfigFile_, lastRunConfigPath_) ? activeConfigFile_ : Zstring();
    //better: use last user-selected config path instead!

    std::optional<Zstring> defaultFolderPath = getParentFolderPath(activeCfgFilePath);

    wxFileDialog fileSelector(this, wxString() /*message*/,  utfTo<wxString>(defaultFolderPath ? *defaultFolderPath : Zstr("")), wxString() /*default file name*/,
                              wxString(L"RealTimeSync (*.ffs_real; *.ffs_batch)|*.ffs_real;*.ffs_batch") + L"|" +_("All files") + L" (*.*)|*",
                              wxFD_OPEN);
    if (fileSelector.ShowModal() != wxID_OK)
        return;

    loadConfig(utfTo<Zstring>(fileSelector.GetPath()));
}


void MainDialog::onFilesDropped(FileDropEvent& event)
{
    if (!event.itemPaths_.empty())
        loadConfig(event.itemPaths_[0]);
}


void MainDialog::setConfiguration(const XmlRealConfig& cfg)
{
    const Zstring& firstFolderPath = cfg.directories.empty() ? Zstring() : cfg.directories[0];
    const std::vector<Zstring> addFolderPaths = cfg.directories.empty() ? std::vector<Zstring>() :
                                                std::vector<Zstring>(cfg.directories.begin() + 1, cfg.directories.end());

    firstFolderPanel_->setPath(firstFolderPath);

    bSizerFolders->Clear(true);
    additionalFolderPanels_.clear();

    insertAddFolder(addFolderPaths, 0);

    m_textCtrlCommand->SetValue(utfTo<wxString>(cfg.commandline));
    m_spinCtrlDelay  ->SetValue(static_cast<int>(cfg.delay));
}


XmlRealConfig MainDialog::getConfiguration()
{
    XmlRealConfig output;

    output.directories.push_back(firstFolderPanel_->getPath());

    for (const DirectoryPanel* dp : additionalFolderPanels_)
        output.directories.push_back(dp->getPath());

    output.commandline = utfTo<Zstring>(m_textCtrlCommand->GetValue());
    output.delay       = m_spinCtrlDelay->GetValue();

    return output;
}


void MainDialog::onAddFolder(wxCommandEvent& event)
{
    const Zstring topFolder = firstFolderPanel_->getPath();

    //clear existing top folder first
    firstFolderPanel_->setPath(Zstring());

    insertAddFolder({topFolder}, 0);
}


void MainDialog::onRemoveFolder(wxCommandEvent& event)
{
    //find folder pair originating the event
    const wxObject* const eventObj = event.GetEventObject();
    for (auto it = additionalFolderPanels_.begin(); it != additionalFolderPanels_.end(); ++it)
        if (eventObj == static_cast<wxObject*>((*it)->m_bpButtonRemoveFolder))
        {
            removeAddFolder(it - additionalFolderPanels_.begin());
            return;
        }
}


void MainDialog::onRemoveTopFolder(wxCommandEvent& event)
{
    if (!additionalFolderPanels_.empty())
    {
        firstFolderPanel_->setPath(additionalFolderPanels_[0]->getPath());
        removeAddFolder(0); //remove first of additional folders
    }
}


void MainDialog::insertAddFolder(const std::vector<Zstring>& newFolders, size_t pos)
{
    assert(pos <= additionalFolderPanels_.size() && additionalFolderPanels_.size() == bSizerFolders->GetItemCount());
    pos = std::min(pos, additionalFolderPanels_.size());

    for (size_t i = 0; i < newFolders.size(); ++i)
    {
        //add new folder pair
        DirectoryPanel* newFolder = new DirectoryPanel(m_scrolledWinFolders, *this, folderLastSelected_);

        bSizerFolders->Insert(pos + i, newFolder, 0, wxEXPAND);
        additionalFolderPanels_.insert(additionalFolderPanels_.begin() + pos + i, newFolder);

        //register events
        newFolder->m_bpButtonRemoveFolder->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](wxCommandEvent& event) { onRemoveFolder(event); });

        //make sure panel has proper default height
        newFolder->GetSizer()->SetSizeHints(newFolder); //~=Fit() + SetMinSize()

        newFolder->setPath(newFolders[i]);
    }

    //set size of scrolled window
    const int folderHeight = additionalFolderPanels_.empty() ? 0 : additionalFolderPanels_[0]->GetSize().GetHeight();
    const size_t visibleRows = std::min(additionalFolderPanels_.size(), MAX_ADD_FOLDERS); //up to MAX_ADD_FOLDERS additional folders shall be shown

    m_scrolledWinFolders->SetMinSize({-1, folderHeight * static_cast<int>(visibleRows)});

    //adapt delete top folder pair button
    m_bpButtonRemoveTopFolder->Show(!additionalFolderPanels_.empty());

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()

    m_scrolledWinFolders->Layout(); //fix GUI distortion after .ffs_batch drag & drop (Linux)

    Refresh(); //remove a little flicker near the start button
}


void MainDialog::removeAddFolder(size_t pos)
{
    if (pos < additionalFolderPanels_.size())
    {
        //remove folder pairs from window
        DirectoryPanel* pairToDelete = additionalFolderPanels_[pos];

        bSizerFolders->Detach(pairToDelete); //Remove() does not work on Window*, so do it manually
        additionalFolderPanels_.erase(additionalFolderPanels_.begin() + pos); //remove last element in vector
        //more (non-portable) wxWidgets bullshit: on OS X wxWindow::Destroy() screws up and calls "operator delete" directly rather than
        //the deferred deletion it is expected to do (and which is implemented correctly on Windows and Linux)
        //http://bb10.com/python-wxpython-devel/2012-09/msg00004.html
        //=> since we're in a mouse button callback of a sub-component of "pairToDelete" we need to delay deletion ourselves:
        guiQueue_.processAsync([] {}, [pairToDelete] { pairToDelete->Destroy(); });

        //set size of scrolled window
        const int folderHeight = additionalFolderPanels_.empty() ? 0 : additionalFolderPanels_[0]->GetSize().GetHeight();
        const size_t visibleRows = std::min(additionalFolderPanels_.size(), MAX_ADD_FOLDERS); //up to MAX_ADD_FOLDERS additional folders shall be shown

        m_scrolledWinFolders->SetMinSize({-1, folderHeight * static_cast<int>(visibleRows)});
        m_scrolledWinFolders->Layout(); //[!] needed when scrollbars are shown

        //adapt delete top folder pair button
        m_bpButtonRemoveTopFolder->Show(!additionalFolderPanels_.empty());

        GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()

        Refresh(); //remove a little flicker near the start button
    }
}

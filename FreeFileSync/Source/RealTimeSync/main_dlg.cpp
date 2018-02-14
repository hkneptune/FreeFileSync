// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "main_dlg.h"
#include <wx/wupdlock.h>
#include <wx/filedlg.h>
#include <wx+/bitmap_button.h>
#include <wx+/font_size.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include <zen/file_access.h>
#include <zen/build_info.h>
#include <zen/time.h>
#include "xml_proc.h"
#include "tray_menu.h"
#include "app_icon.h"
#include "../lib/help_provider.h"
#include "../lib/process_xml.h"
#include "../lib/ffs_paths.h"
#include "../version/version.h"

    #include <gtk/gtk.h>

using namespace zen;
using namespace rts;


namespace
{
}


class rts::DirectoryPanel : public FolderGenerated
{
public:
    DirectoryPanel(wxWindow* parent) :
        FolderGenerated(parent),
        folderSelector_(*this, *m_buttonSelectFolder, *m_txtCtrlDirectory, nullptr /*staticText*/) {}

    void setPath(const Zstring& dirpath) { folderSelector_.setPath(dirpath); }
    Zstring getPath() const { return folderSelector_.getPath(); }

private:
    FolderSelector2 folderSelector_;
};


void MainDialog::create(const Zstring& cfgFile)
{
    /*MainDialog* frame = */ new MainDialog(nullptr, cfgFile);
}


MainDialog::MainDialog(wxDialog* dlg, const Zstring& cfgFileName)
    : MainDlgGenerated(dlg),
      lastRunConfigPath_(fff::getConfigDirPathPf() + Zstr("LastRun.ffs_real"))
{

    SetIcon(getRtsIcon()); //set application icon

    setRelativeFontSize(*m_buttonStart, 1.5);

    m_bpButtonRemoveTopFolder->Hide();
    m_panelMainFolder->Layout();

    m_bpButtonAddFolder      ->SetBitmapLabel(getResourceImage(L"item_add"));
    m_bpButtonRemoveTopFolder->SetBitmapLabel(getResourceImage(L"item_remove"));
    setBitmapTextLabel(*m_buttonStart, getResourceImage(L"startRts").ConvertToImage(), m_buttonStart->GetLabel(), 5, 8);

    //register key event
    Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(MainDialog::OnKeyPressed), nullptr, this);

    //prepare drag & drop
    dirpathFirst = std::make_unique<FolderSelector2>(*m_panelMainFolder, *m_buttonSelectFolderMain, *m_txtCtrlDirectoryMain, m_staticTextFinalPath);

    //--------------------------- load config values ------------------------------------
    XmlRealConfig newConfig;

    Zstring currentConfigFile = cfgFileName;
    if (currentConfigFile.empty())
    {
        if (!itemNotExisting(lastRunConfigPath_)) //existing/access error? => user should be informed about access errors
            currentConfigFile = lastRunConfigPath_;
    }

    bool loadCfgSuccess = false;
    if (!currentConfigFile.empty())
        try
        {
            std::wstring warningMsg;
            readRealOrBatchConfig(currentConfigFile, newConfig, warningMsg); //throw FileError

            if (!warningMsg.empty())
                showNotificationDialog(this, DialogInfoType::WARNING, PopupDialogCfg().setDetailInstructions(warningMsg));

            loadCfgSuccess = warningMsg.empty();
        }
        catch (const FileError& e)
        {
            showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
        }

    const bool startWatchingImmediately = loadCfgSuccess && !cfgFileName.empty();

    setConfiguration(newConfig);
    setLastUsedConfig(currentConfigFile);
    //-----------------------------------------------------------------------------------------

    Center(); //needs to be re-applied after a dialog size change! (see addFolder() within setConfiguration())

    if (startWatchingImmediately) //start watch mode directly
    {
        wxCommandEvent dummy2(wxEVT_COMMAND_BUTTON_CLICKED);
        this->OnStart(dummy2);
        //don't Show()!
    }
    else
    {
        m_buttonStart->SetFocus(); //don't "steal" focus if program is running from sys-tray"
        Show();
    }

    //drag and drop .ffs_real and .ffs_batch on main dialog
    setupFileDrop(*this);
    Connect(EVENT_DROP_FILE, FileDropEventHandler(MainDialog::onFilesDropped), nullptr, this);
}


MainDialog::~MainDialog()
{
    //save current configuration
    const XmlRealConfig currentCfg = getConfiguration();

    try //write config to XML
    {
        writeConfig(currentCfg, lastRunConfigPath_); //throw FileError
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
    }
}


void MainDialog::onQueryEndSession()
{
    try { writeConfig(getConfiguration(), lastRunConfigPath_); } //throw FileError
    catch (const FileError&) {} //we try our best do to something useful in this extreme situation - no reason to notify or even log errors here!
}


void MainDialog::OnShowHelp(wxCommandEvent& event)
{
    fff::displayHelpEntry(L"realtimesync", this);
}


void MainDialog::OnMenuAbout(wxCommandEvent& event)
{
    std::wstring build = formatTime<std::wstring>(FORMAT_DATE, getCompileTime()) + SPACED_DASH + L"Unicode";
#ifndef wxUSE_UNICODE
#error what is going on?
#endif

    build +=
#ifdef ZEN_BUILD_32BIT
        L" x86";
#elif defined ZEN_BUILD_64BIT
        L" x64";
#endif

    showNotificationDialog(this, DialogInfoType::INFO, PopupDialogCfg().
                           setTitle(_("About")).
                           setMainInstructions(L"RealTimeSync" L"\n\n" + replaceCpy(_("Build: %x"), L"%x", build)));
}


void MainDialog::OnKeyPressed(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();
    if (keyCode == WXK_ESCAPE)
    {
        Close();
        return;
    }
    event.Skip();
}


void MainDialog::OnStart(wxCommandEvent& event)
{
    Hide();

    XmlRealConfig currentCfg = getConfiguration();
    const Zstring activeCfgFilePath = !equalFilePath(activeConfigFile_, lastRunConfigPath_) ? activeConfigFile_ : Zstring();

    switch (rts::startDirectoryMonitor(currentCfg, fff::extractJobName(activeCfgFilePath)))
    {
        case rts::EXIT_APP:
            Close();
            return;

        case rts::SHOW_GUI:
            break;
    }

    Show(); //don't show for EXIT_APP
    Raise();
}


void MainDialog::OnConfigSave(wxCommandEvent& event)
{
    Zstring defaultFilePath = !activeConfigFile_.empty() && !equalFilePath(activeConfigFile_, lastRunConfigPath_) ? activeConfigFile_ : Zstr("Realtime.ffs_real");
    //attention: currentConfigFileName may be an imported *.ffs_batch file! We don't want to overwrite it with a GUI config!
    if (endsWith(defaultFilePath, Zstr(".ffs_batch"), CmpFilePath()))
        defaultFilePath = beforeLast(defaultFilePath, Zstr("."), IF_MISSING_RETURN_NONE) + Zstr(".ffs_real");

    wxFileDialog filePicker(this,
                            wxString(),
                            //OS X really needs dir/file separated like this:
                            utfTo<wxString>(beforeLast(defaultFilePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE)), //default dir
                            utfTo<wxString>(afterLast (defaultFilePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL)), //default file
                            wxString(L"RealTimeSync (*.ffs_real)|*.ffs_real") + L"|" +_("All files") + L" (*.*)|*",
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (filePicker.ShowModal() != wxID_OK)
        return;

    const Zstring targetFilePath = utfTo<Zstring>(filePicker.GetPath());

    //write config to XML
    const XmlRealConfig currentCfg = getConfiguration();
    try
    {
        writeConfig(currentCfg, targetFilePath); //throw FileError
        setLastUsedConfig(targetFilePath);
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
    }
}


void MainDialog::loadConfig(const Zstring& filepath)
{
    XmlRealConfig newConfig;

    if (!filepath.empty())
        try
        {
            std::wstring warningMsg;
            readRealOrBatchConfig(filepath, newConfig, warningMsg); //throw FileError

            if (!warningMsg.empty())
                showNotificationDialog(this, DialogInfoType::WARNING, PopupDialogCfg().setDetailInstructions(warningMsg));
        }
        catch (const FileError& e)
        {
            showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
            return;
        }

    setConfiguration(newConfig);
    setLastUsedConfig(filepath);
}


void MainDialog::setLastUsedConfig(const Zstring& filepath)
{
    activeConfigFile_ = filepath;

    const Zstring activeCfgFilePath = !equalFilePath(activeConfigFile_, lastRunConfigPath_) ? activeConfigFile_ : Zstring();

    if (!activeCfgFilePath.empty())
        SetTitle(utfTo<wxString>(activeCfgFilePath));
    else
        SetTitle(wxString(L"RealTimeSync ") + fff::ffsVersion + SPACED_DASH + _("Automated Synchronization"));

}


void MainDialog::OnConfigNew(wxCommandEvent& event)
{
    loadConfig({});
}


void MainDialog::OnConfigLoad(wxCommandEvent& event)
{
    const Zstring activeCfgFilePath = !equalFilePath(activeConfigFile_, lastRunConfigPath_) ? activeConfigFile_ : Zstring();

    wxFileDialog filePicker(this,
                            wxString(),
                            utfTo<wxString>(beforeLast(activeCfgFilePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE)), //default dir
                            wxString(), //default file
                            wxString(L"RealTimeSync (*.ffs_real; *.ffs_batch)|*.ffs_real;*.ffs_batch") + L"|" +_("All files") + L" (*.*)|*",
                            wxFD_OPEN);
    if (filePicker.ShowModal() == wxID_OK)
        loadConfig(utfTo<Zstring>(filePicker.GetPath()));
}


void MainDialog::onFilesDropped(FileDropEvent& event)
{
    const auto& filePaths = event.getPaths();
    if (!filePaths.empty())
        loadConfig(utfTo<Zstring>(filePaths[0]));
}


void MainDialog::setConfiguration(const XmlRealConfig& cfg)
{
    //clear existing folders
    dirpathFirst->setPath(Zstring());
    clearAddFolders();

    if (!cfg.directories.empty())
    {
        //fill top folder
        dirpathFirst->setPath(*cfg.directories.begin());

        //fill additional folders
        addFolder(std::vector<Zstring>(cfg.directories.begin() + 1, cfg.directories.end()));
    }

    //fill commandline
    m_textCtrlCommand->SetValue(utfTo<wxString>(cfg.commandline));

    //set delay
    m_spinCtrlDelay->SetValue(static_cast<int>(cfg.delay));
}


XmlRealConfig MainDialog::getConfiguration()
{
    XmlRealConfig output;

    output.directories.push_back(utfTo<Zstring>(dirpathFirst->getPath()));
    for (const DirectoryPanel* dne : dirpathsExtra)
        output.directories.push_back(utfTo<Zstring>(dne->getPath()));

    output.commandline = utfTo<Zstring>(m_textCtrlCommand->GetValue());
    output.delay       = m_spinCtrlDelay->GetValue();

    return output;
}


void MainDialog::OnAddFolder(wxCommandEvent& event)
{
    const Zstring topFolder = utfTo<Zstring>(dirpathFirst->getPath());

    //clear existing top folder first
    dirpathFirst->setPath(Zstring());

    std::vector<Zstring> newFolders;
    newFolders.push_back(topFolder);

    addFolder(newFolders, true); //add pair in front of additonal pairs
}


void MainDialog::OnRemoveFolder(wxCommandEvent& event)
{
    //find folder pair originating the event
    const wxObject* const eventObj = event.GetEventObject();
    for (auto it = dirpathsExtra.begin(); it != dirpathsExtra.end(); ++it)
        if (eventObj == static_cast<wxObject*>((*it)->m_bpButtonRemoveFolder))
        {
            removeAddFolder(it - dirpathsExtra.begin());
            return;
        }
}


void MainDialog::OnRemoveTopFolder(wxCommandEvent& event)
{
    if (dirpathsExtra.size() > 0)
    {
        dirpathFirst->setPath(dirpathsExtra[0]->getPath());
        removeAddFolder(0); //remove first of additional folders
    }
}


    static const size_t MAX_ADD_FOLDERS = 6;


void MainDialog::addFolder(const std::vector<Zstring>& newFolders, bool addFront)
{
    if (newFolders.size() == 0)
        return;


    int folderHeight = 0;
    for (const Zstring& dirpath : newFolders)
    {
        //add new folder pair
        DirectoryPanel* newFolder = new DirectoryPanel(m_scrolledWinFolders);
        newFolder->m_bpButtonRemoveFolder->SetBitmapLabel(getResourceImage(L"item_remove"));

        //get size of scrolled window
        folderHeight = newFolder->GetSize().GetHeight();

        if (addFront)
        {
            bSizerFolders->Insert(0, newFolder, 0, wxEXPAND, 5);
            dirpathsExtra.insert(dirpathsExtra.begin(), newFolder);
        }
        else
        {
            bSizerFolders->Add(newFolder, 0, wxEXPAND, 5);
            dirpathsExtra.push_back(newFolder);
        }

        //register events
        newFolder->m_bpButtonRemoveFolder->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MainDialog::OnRemoveFolder), nullptr, this );

        //insert directory name
        newFolder->setPath(dirpath);
    }

    //set size of scrolled window
    const size_t additionalRows = std::min(dirpathsExtra.size(), MAX_ADD_FOLDERS); //up to MAX_ADD_FOLDERS additional folders shall be shown
    m_scrolledWinFolders->SetMinSize(wxSize( -1, folderHeight * static_cast<int>(additionalRows)));

    //adapt delete top folder pair button
    m_bpButtonRemoveTopFolder->Show();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    Layout();
    Refresh(); //remove a little flicker near the start button
}


void MainDialog::removeAddFolder(size_t pos)
{

    if (pos < dirpathsExtra.size())
    {
        //remove folder pairs from window
        DirectoryPanel* pairToDelete = dirpathsExtra[pos];
        const int folderHeight = pairToDelete->GetSize().GetHeight();

        bSizerFolders->Detach(pairToDelete); //Remove() does not work on Window*, so do it manually
        dirpathsExtra.erase(dirpathsExtra.begin() + pos); //remove last element in vector
        //more (non-portable) wxWidgets bullshit: on OS X wxWindow::Destroy() screws up and calls "operator delete" directly rather than
        //the deferred deletion it is expected to do (and which is implemented correctly on Windows and Linux)
        //http://bb10.com/python-wxpython-devel/2012-09/msg00004.html
        //=> since we're in a mouse button callback of a sub-component of "pairToDelete" we need to delay deletion ourselves:
        guiQueue_.processAsync([] {}, [pairToDelete] { pairToDelete->Destroy(); });

        //set size of scrolled window
        const size_t additionalRows = std::min(dirpathsExtra.size(), MAX_ADD_FOLDERS); //up to MAX_ADD_FOLDERS additional folders shall be shown
        m_scrolledWinFolders->SetMinSize(wxSize( -1, folderHeight * static_cast<int>(additionalRows)));

        //adapt delete top folder pair button
        if (dirpathsExtra.size() == 0)
        {
            m_bpButtonRemoveTopFolder->Hide();
            m_panelMainFolder->Layout();
        }

        GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
        Layout();
        Refresh(); //remove a little flicker near the start button
    }
}


void MainDialog::clearAddFolders()
{

    bSizerFolders->Clear(true);
    dirpathsExtra.clear();

    m_scrolledWinFolders->SetMinSize(wxSize(-1, 0));

    m_bpButtonRemoveTopFolder->Hide();
    m_panelMainFolder->Layout();

    GetSizer()->SetSizeHints(this); //~=Fit()
    Layout();
    Refresh(); //remove a little flicker near the start button
}

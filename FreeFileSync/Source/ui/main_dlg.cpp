// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "main_dlg.h"
#include <zen/format_unit.h>
#include <zen/file_access.h>
#include <zen/file_io.h>
#include <zen/thread.h>
#include <zen/shell_execute.h>
#include <zen/perf.h>
#include <wx/clipbrd.h>
#include <wx/wupdlock.h>
#include <wx/sound.h>
#include <wx/filedlg.h>
#include <wx/display.h>
#include <wx+/context_menu.h>
#include <wx+/bitmap_button.h>
#include <wx+/app_main.h>
#include <wx+/toggle_button.h>
#include <wx+/no_flicker.h>
#include <wx+/rtl.h>
#include <wx+/font_size.h>
#include <wx+/focus.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "cfg_grid.h"
#include "version_check.h"
#include "gui_status_handler.h"
#include "small_dlgs.h"
#include "progress_indicator.h"
#include "folder_pair.h"
#include "search.h"
#include "batch_config.h"
#include "triple_splitter.h"
#include "app_icon.h"
#include "../comparison.h"
#include "../synchronization.h"
#include "../algorithm.h"
#include "../fs/concrete.h"
#include "../lib/resolve_path.h"
#include "../lib/ffs_paths.h"
#include "../lib/help_provider.h"
#include "../lib/lock_holder.h"
#include "../lib/localization.h"
#include "../version/version.h"


using namespace zen;
using namespace fff;



namespace
{
const size_t EXT_APP_MASS_INVOKE_THRESHOLD = 10; //more than this is likely a user mistake (Explorer uses limit of 15)
const int TOP_BUTTON_OPTIMAL_WIDTH = 180;


IconBuffer::IconSize convert(FileIconSize isize)
{
    switch (isize)
    {
        case ICON_SIZE_SMALL:
            return IconBuffer::SIZE_SMALL;
        case ICON_SIZE_MEDIUM:
            return IconBuffer::SIZE_MEDIUM;
        case ICON_SIZE_LARGE:
            return IconBuffer::SIZE_LARGE;
    }
    return IconBuffer::SIZE_SMALL;
}


bool acceptDialogFileDrop(const std::vector<Zstring>& shellItemPaths)
{
    return std::any_of(shellItemPaths.begin(), shellItemPaths.end(), [](const Zstring& shellItemPath)
    {
        const Zstring ext = getFileExtension(shellItemPath);
        return strEqual(ext, Zstr("ffs_gui"),   CmpFilePath()) ||
               strEqual(ext, Zstr("ffs_batch"), CmpFilePath());
    });
}
}


class fff::FolderSelectorImpl : public FolderSelector
{
public:
    FolderSelectorImpl(MainDialog&       mainDlg,
                       wxPanel&          dropWindow1,
                       wxButton&         selectFolderButton,
                       wxButton&         selectSftpButton,
                       FolderHistoryBox& dirpath,
                       wxStaticText*     staticText  = nullptr,
                       wxWindow*         dropWindow2 = nullptr) :
        FolderSelector(dropWindow1, selectFolderButton, selectSftpButton, dirpath, staticText, dropWindow2),
        mainDlg_(mainDlg) {}

    bool shouldSetDroppedPaths(const std::vector<Zstring>& shellItemPaths) override
    {
        if (acceptDialogFileDrop(shellItemPaths))
        {
            assert(!shellItemPaths.empty());
            mainDlg_.loadConfiguration(shellItemPaths);
            return false;
        }
        return true; //=> return true: change directory selection via drag and drop
    }

private:
    FolderSelectorImpl           (const FolderSelectorImpl&) = delete;
    FolderSelectorImpl& operator=(const FolderSelectorImpl&) = delete;

    MainDialog& mainDlg_;
};

//------------------------------------------------------------------
/*    class hierarchy:

           template<>
           FolderPairPanelBasic
                    /|\
                     |
           template<>
           FolderPairCallback   FolderPairPanelGenerated
                    /|\                  /|\
            _________|________    ________|
           |                  |  |
    FolderPairFirst      FolderPairPanel
*/

template <class GuiPanel>
class fff::FolderPairCallback : public FolderPairPanelBasic<GuiPanel> //implements callback functionality to MainDialog as imposed by FolderPairPanelBasic
{
public:
    FolderPairCallback(GuiPanel& basicPanel, MainDialog& mainDialog) :
        FolderPairPanelBasic<GuiPanel>(basicPanel), //pass FolderPairPanelGenerated part...
        mainDlg_(mainDialog) {}

private:
    MainConfiguration getMainConfig() const override { return mainDlg_.getConfig().mainCfg; }
    wxWindow* getParentWindow() override { return &mainDlg_; }
    std::unique_ptr<FilterConfig>& getFilterCfgOnClipboardRef() override { return mainDlg_.filterCfgOnClipboard_; }

    void onAltCompCfgChange    () override { mainDlg_.applyCompareConfig(false /*setDefaultViewType*/); }
    void onAltSyncCfgChange    () override { mainDlg_.applySyncConfig(); }
    void onLocalFilterCfgChange() override { mainDlg_.applyFilterConfig(); } //re-apply filter

    MainDialog& mainDlg_;
};


class fff::FolderPairPanel :
    public FolderPairPanelGenerated, //FolderPairPanel "owns" FolderPairPanelGenerated!
    public FolderPairCallback<FolderPairPanelGenerated>
{
public:
    FolderPairPanel(wxWindow* parent, MainDialog& mainDialog) :
        FolderPairPanelGenerated(parent),
        FolderPairCallback<FolderPairPanelGenerated>(static_cast<FolderPairPanelGenerated&>(*this), mainDialog), //pass FolderPairPanelGenerated part...
        folderSelectorLeft_ (mainDialog, *m_panelLeft,  *m_buttonSelectFolderLeft,  *m_bpButtonSelectAltFolderLeft,  *m_folderPathLeft),
        folderSelectorRight_(mainDialog, *m_panelRight, *m_buttonSelectFolderRight, *m_bpButtonSelectAltFolderRight, *m_folderPathRight)
    {
        folderSelectorLeft_ .setSiblingSelector(&folderSelectorRight_);
        folderSelectorRight_.setSiblingSelector(&folderSelectorLeft_);

        folderSelectorLeft_ .Connect(EVENT_ON_FOLDER_SELECTED, wxCommandEventHandler(MainDialog::onDirSelected), nullptr, &mainDialog);
        folderSelectorRight_.Connect(EVENT_ON_FOLDER_SELECTED, wxCommandEventHandler(MainDialog::onDirSelected), nullptr, &mainDialog);

        folderSelectorLeft_ .Connect(EVENT_ON_FOLDER_MANUAL_EDIT, wxCommandEventHandler(MainDialog::onDirManualCorrection), nullptr, &mainDialog);
        folderSelectorRight_.Connect(EVENT_ON_FOLDER_MANUAL_EDIT, wxCommandEventHandler(MainDialog::onDirManualCorrection), nullptr, &mainDialog);

        m_bpButtonFolderPairOptions->SetBitmapLabel(getResourceImage(L"button_arrow_down"));
    }

    void setValues(const FolderPairEnh& fp)
    {
        setConfig(fp.altCmpConfig, fp.altSyncConfig, fp.localFilter);
        folderSelectorLeft_ .setPath(fp.folderPathPhraseLeft_);
        folderSelectorRight_.setPath(fp.folderPathPhraseRight_);
    }

    FolderPairEnh getValues() const { return FolderPairEnh(folderSelectorLeft_.getPath(), folderSelectorRight_.getPath(), getAltCompConfig(), getAltSyncConfig(), getAltFilterConfig()); }

private:
    //support for drag and drop
    FolderSelectorImpl folderSelectorLeft_;
    FolderSelectorImpl folderSelectorRight_;
};


class fff::FolderPairFirst : public FolderPairCallback<MainDialogGenerated>
{
public:
    FolderPairFirst(MainDialog& mainDialog) :
        FolderPairCallback<MainDialogGenerated>(mainDialog, mainDialog),

        //prepare drag & drop
        folderSelectorLeft_(mainDialog,
                            *mainDialog.m_panelTopLeft,
                            *mainDialog.m_buttonSelectFolderLeft,
                            *mainDialog.m_bpButtonSelectAltFolderLeft,
                            *mainDialog.m_folderPathLeft,
                            mainDialog.m_staticTextResolvedPathL,
                            &mainDialog.m_gridMainL->getMainWin()),
        folderSelectorRight_(mainDialog,
                             *mainDialog.m_panelTopRight,
                             *mainDialog.m_buttonSelectFolderRight,
                             *mainDialog.m_bpButtonSelectAltFolderRight,
                             *mainDialog.m_folderPathRight,
                             mainDialog.m_staticTextResolvedPathR,
                             &mainDialog.m_gridMainR->getMainWin())
    {
        folderSelectorLeft_ .setSiblingSelector(&folderSelectorRight_);
        folderSelectorRight_.setSiblingSelector(&folderSelectorLeft_);

        folderSelectorLeft_ .Connect(EVENT_ON_FOLDER_SELECTED, wxCommandEventHandler(MainDialog::onDirSelected), nullptr, &mainDialog);
        folderSelectorRight_.Connect(EVENT_ON_FOLDER_SELECTED, wxCommandEventHandler(MainDialog::onDirSelected), nullptr, &mainDialog);

        folderSelectorLeft_ .Connect(EVENT_ON_FOLDER_MANUAL_EDIT, wxCommandEventHandler(MainDialog::onDirManualCorrection), nullptr, &mainDialog);
        folderSelectorRight_.Connect(EVENT_ON_FOLDER_MANUAL_EDIT, wxCommandEventHandler(MainDialog::onDirManualCorrection), nullptr, &mainDialog);

        mainDialog.m_panelTopLeft  ->Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(MainDialog::onTopFolderPairKeyEvent), nullptr, &mainDialog);
        mainDialog.m_panelTopCenter->Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(MainDialog::onTopFolderPairKeyEvent), nullptr, &mainDialog);
        mainDialog.m_panelTopRight ->Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(MainDialog::onTopFolderPairKeyEvent), nullptr, &mainDialog);
    }

    void setValues(const FolderPairEnh& fp)
    {
        setConfig(fp.altCmpConfig, fp.altSyncConfig, fp.localFilter);
        folderSelectorLeft_ .setPath(fp.folderPathPhraseLeft_);
        folderSelectorRight_.setPath(fp.folderPathPhraseRight_);
    }

    FolderPairEnh getValues() const { return FolderPairEnh(folderSelectorLeft_.getPath(), folderSelectorRight_.getPath(), getAltCompConfig(), getAltSyncConfig(), getAltFilterConfig()); }

private:
    //support for drag and drop
    FolderSelectorImpl folderSelectorLeft_;
    FolderSelectorImpl folderSelectorRight_;
};




namespace
{
void updateTopButton(wxBitmapButton& btn, const wxBitmap& bmp, const wxString& variantName, bool makeGrey)
{
    wxImage labelImage   = createImageFromText(btn.GetLabel(), btn.GetFont(), wxSystemSettings::GetColour(makeGrey ? wxSYS_COLOUR_GRAYTEXT : wxSYS_COLOUR_BTNTEXT));
    wxImage variantImage = createImageFromText(variantName,
                                               wxFont(wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD),
                                               wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    wxImage descrImage = stackImages(labelImage, variantImage, ImageStackLayout::VERTICAL, ImageStackAlignment::CENTER);
    const wxImage& iconImage = makeGrey ? greyScale(bmp.ConvertToImage()) : bmp.ConvertToImage();

    wxImage dynImage = btn.GetLayoutDirection() != wxLayout_RightToLeft ?
                       stackImages(iconImage, descrImage, ImageStackLayout::HORIZONTAL, ImageStackAlignment::CENTER, 5) :
                       stackImages(descrImage, iconImage, ImageStackLayout::HORIZONTAL, ImageStackAlignment::CENTER, 5);

    //SetMinSize() instead of SetSize() is needed here for wxWindows layout determination to work correctly
    wxSize minSize = dynImage.GetSize() + wxSize(16, 16); //add border space
    minSize.x = std::max(minSize.x, TOP_BUTTON_OPTIMAL_WIDTH);

    btn.SetMinSize(minSize);

    setImage(btn, wxBitmap(dynImage));
}

//##################################################################################################################################

XmlGlobalSettings tryLoadGlobalConfig(const Zstring& globalConfigFilePath) //blocks on GUI on errors!
{
    XmlGlobalSettings globalCfg;
    try
    {
        std::wstring warningMsg;
        readConfig(globalConfigFilePath, globalCfg, warningMsg); //throw FileError

        assert(warningMsg.empty()); //ignore parsing errors: should be migration problems only *cross-fingers*
    }
    catch (const FileError& e)
    {
        if (!itemNotExisting(globalConfigFilePath)) //existing or access error
            showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString())); //no parent window: main dialog not yet created!
    }
    return globalCfg;
}
}


void MainDialog::create(const Zstring& globalConfigFilePath)
{
    const XmlGlobalSettings globalSettings = tryLoadGlobalConfig(globalConfigFilePath);

    std::vector<Zstring> cfgFilePaths = globalSettings.gui.mainDlg.lastUsedConfigFiles;

    //------------------------------------------------------------------------------------------
    //check existence of all files in parallel:
    GetFirstResult<FalseType> firstUnavailableFile;

    for (const Zstring& filePath : cfgFilePaths)
        firstUnavailableFile.addJob([filePath]() -> Opt<FalseType>
    {
        assert(!filePath.empty());
        if (!fileAvailable(filePath))
            return FalseType();
        return NoValue();
    });

    //potentially slow network access: give all checks 500ms to finish
    const bool allFilesAvailable = firstUnavailableFile.timedWait(std::chrono::milliseconds(500)) && //false: time elapsed
                                   !firstUnavailableFile.get(); //no missing
    if (!allFilesAvailable)
        cfgFilePaths.clear(); //we do NOT want to show an error due to last config file missing on application start!
    //------------------------------------------------------------------------------------------

    if (cfgFilePaths.empty())
    {
        const Zstring lastRunConfigFilePath = getLastRunConfigPath();
        if (fileAvailable(lastRunConfigFilePath)) //3. try to load auto-save config (should not block)
            cfgFilePaths.push_back(lastRunConfigFilePath);
        //else: not-existing/access error? => user may click on <Last Session> later
    }

    XmlGuiConfig guiCfg; //contains default values

    //add default exclusion filter: this is only ever relevant when creating new configurations!
    //a default XmlGuiConfig does not need these user-specific exclusions!
    Zstring& excludeFilter = guiCfg.mainCfg.globalFilter.excludeFilter;
    if (!excludeFilter.empty() && !endsWith(excludeFilter, Zstr("\n")))
        excludeFilter += Zstr("\n");
    excludeFilter += globalSettings.gui.defaultExclusionFilter;

    if (!cfgFilePaths.empty())
        try
        {
            std::wstring warningMsg;
            readAnyConfig(cfgFilePaths, guiCfg, warningMsg); //throw FileError

            if (!warningMsg.empty())
                showNotificationDialog(nullptr, DialogInfoType::WARNING, PopupDialogCfg().setDetailInstructions(warningMsg));
            //what about showing as changed config on parsing errors????
        }
        catch (const FileError& e)
        {
            showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
        }

    //------------------------------------------------------------------------------------------

    create(globalConfigFilePath, &globalSettings, guiCfg, cfgFilePaths, false);
}


void MainDialog::create(const Zstring& globalConfigFilePath,
                        const XmlGlobalSettings* globalSettings,
                        const XmlGuiConfig& guiCfg,
                        const std::vector<Zstring>& referenceFiles,
                        bool startComparison)
{
    const XmlGlobalSettings globSett = globalSettings ? *globalSettings : tryLoadGlobalConfig(globalConfigFilePath);

    try
    {
        //we need to set language *before* creating MainDialog!
        setLanguage(globSett.programLanguage); //throw FileError
    }
    catch (const FileError& e)
    {
        showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
        //continue!
    }

    MainDialog* frame = new MainDialog(globalConfigFilePath, guiCfg, referenceFiles, globSett, startComparison);
    frame->Show();
}


MainDialog::MainDialog(const Zstring& globalConfigFilePath,
                       const XmlGuiConfig& guiCfg,
                       const std::vector<Zstring>& referenceFiles,
                       const XmlGlobalSettings& globalSettings,
                       bool startComparison) :
    MainDialogGenerated(nullptr),
    globalConfigFilePath_(globalConfigFilePath),
    lastRunConfigPath_(getLastRunConfigPath())

{
    m_folderPathLeft ->init(folderHistoryLeft_);
    m_folderPathRight->init(folderHistoryRight_);

    //setup sash: detach + reparent:
    m_splitterMain->SetSizer(nullptr); //alas wxFormbuilder doesn't allow us to have child windows without a sizer, so we have to remove it here
    m_splitterMain->setupWindows(m_gridMainL, m_gridMainC, m_gridMainR);


    setRelativeFontSize(*m_buttonCompare, 1.4);
    setRelativeFontSize(*m_buttonSync,    1.4);
    setRelativeFontSize(*m_buttonCancel,  1.4);

    //set icons for this dialog
    SetIcon(getFfsIcon()); //set application icon

    m_bpButtonCmpConfig ->SetBitmapLabel(getResourceImage(L"cfg_compare"));
    m_bpButtonSyncConfig->SetBitmapLabel(getResourceImage(L"cfg_sync"));

    m_bpButtonCmpContext   ->SetBitmapLabel(mirrorIfRtl(getResourceImage(L"button_arrow_right")));
    m_bpButtonFilterContext->SetBitmapLabel(mirrorIfRtl(getResourceImage(L"button_arrow_right")));
    m_bpButtonSyncContext  ->SetBitmapLabel(mirrorIfRtl(getResourceImage(L"button_arrow_right")));

    m_bpButtonNew        ->SetBitmapLabel(getResourceImage(L"new"));
    m_bpButtonOpen       ->SetBitmapLabel(getResourceImage(L"load"));
    m_bpButtonSaveAs     ->SetBitmapLabel(getResourceImage(L"sync"));
    m_bpButtonSaveAsBatch->SetBitmapLabel(getResourceImage(L"batch"));
    m_bpButtonAddPair    ->SetBitmapLabel(getResourceImage(L"item_add"));
    m_bpButtonHideSearch ->SetBitmapLabel(getResourceImage(L"close_panel"));

    //we have to use the OS X naming convention by default, because wxMac permanently populates the display menu when the wxMenuItem is created for the first time!
    //=> other wx ports are not that badly programmed; therefore revert:
    assert(m_menuItemOptions->GetItemLabel() == _("&Preferences") + L"\tCtrl+,"); //"Ctrl" is automatically mapped to command button!
    m_menuItemOptions->SetItemLabel(_("&Options"));

    //---------------- support for dockable gui style --------------------------------
    bSizerPanelHolder->Detach(m_panelTopButtons);
    bSizerPanelHolder->Detach(m_panelDirectoryPairs);
    bSizerPanelHolder->Detach(m_gridOverview);
    bSizerPanelHolder->Detach(m_panelCenter);
    bSizerPanelHolder->Detach(m_panelConfig);
    bSizerPanelHolder->Detach(m_panelViewFilter);

    auiMgr_.SetManagedWindow(this);
    auiMgr_.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);

    compareStatus_ = std::make_unique<CompareProgressDialog>(*this); //integrate the compare status panel (in hidden state)

    //caption required for all panes that can be manipulated by the users => used by context menu
    auiMgr_.AddPane(m_panelCenter,
                    wxAuiPaneInfo().Name(L"CenterPanel").CenterPane().PaneBorder(false));
    {
        //set comparison button label tentatively for m_panelTopButtons to receive final height:
        updateTopButton(*m_buttonCompare, getResourceImage(L"compare"), L"Dummy", false /*makeGrey*/);
        m_panelTopButtons->GetSizer()->SetSizeHints(m_panelTopButtons); //~=Fit() + SetMinSize()

        setBitmapTextLabel(*m_buttonCancel, wxImage(), m_buttonCancel->GetLabel()); //we can't use a wxButton for cancel: it's rendered smaller on OS X than a wxBitmapButton!
        m_buttonCancel->SetMinSize(wxSize(std::max(m_buttonCancel->GetSize().x, TOP_BUTTON_OPTIMAL_WIDTH),
                                          std::max(m_buttonCancel->GetSize().y, m_buttonCompare->GetSize().y)));

        auiMgr_.AddPane(m_panelTopButtons,
                        wxAuiPaneInfo().Name(L"TopPanel").Layer(2).Top().Row(1).Caption(_("Main Bar")).CaptionVisible(false).PaneBorder(false).Gripper().MinSize(TOP_BUTTON_OPTIMAL_WIDTH, m_panelTopButtons->GetSize().GetHeight()));
        //note: min height is calculated incorrectly by wxAuiManager if panes with and without caption are in the same row => use smaller min-size

        auiMgr_.AddPane(compareStatus_->getAsWindow(),
                        wxAuiPaneInfo().Name(L"ProgressPanel").Layer(2).Top().Row(2).CaptionVisible(false).PaneBorder(false).Hide()
                        //wxAui does not consider the progress panel's wxRAISED_BORDER and set's too small a panel height! => use correct value from wxWindow::GetSize()
                        .MinSize(200, compareStatus_->getAsWindow()->GetSize().GetHeight())); //bonus: minimal height isn't a bad idea anyway
    }

    auiMgr_.AddPane(m_panelDirectoryPairs,
                    wxAuiPaneInfo().Name(L"FoldersPanel").Layer(2).Top().Row(3).Caption(_("Folder Pairs")).CaptionVisible(false).PaneBorder(false).Gripper());

    auiMgr_.AddPane(m_panelSearch,
                    wxAuiPaneInfo().Name(L"SearchPanel").Layer(2).Bottom().Row(2).Caption(_("Find")).CaptionVisible(false).PaneBorder(false).Gripper().MinSize(200, m_bpButtonHideSearch->GetSize().GetHeight()).Hide());

    auiMgr_.AddPane(m_panelViewFilter,
                    wxAuiPaneInfo().Name(L"ViewFilterPanel").Layer(2).Bottom().Row(1).Caption(_("View Settings")).CaptionVisible(false).PaneBorder(false).Gripper().MinSize(m_bpButtonViewTypeSyncAction->GetSize().GetWidth(), m_panelViewFilter->GetSize().GetHeight()));

    auiMgr_.AddPane(m_panelConfig,
                    wxAuiPaneInfo().Name(L"ConfigPanel").Layer(3).Left().Position(1).Caption(_("Configuration")).MinSize(bSizerCfgHistoryButtons->GetSize()));

    auiMgr_.AddPane(m_gridOverview,
                    wxAuiPaneInfo().Name(L"OverviewPanel").Layer(3).Left().Position(2).Caption(_("Overview")).MinSize(300, m_gridOverview->GetSize().GetHeight())); //MinSize(): just default size, see comment below

    auiMgr_.Update();

    if (wxAuiDockArt* artProvider = auiMgr_.GetArtProvider())
    {
        wxFont font = artProvider->GetFont(wxAUI_DOCKART_CAPTION_FONT);
        font.SetWeight(wxFONTWEIGHT_BOLD);
        font.SetPointSize(wxNORMAL_FONT->GetPointSize()); //= larger than the wxAuiDockArt default; looks better on OS X
        artProvider->SetFont(wxAUI_DOCKART_CAPTION_FONT, font);
        artProvider->SetMetric(wxAUI_DOCKART_CAPTION_SIZE, font.GetPixelSize().GetHeight() + 2 + 2);

        //- fix wxWidgets 3.1.0 insane color scheme
        artProvider->SetColor(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR,          wxColor(220, 220, 220)); //light grey
        artProvider->SetColor(wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR, wxColor(220, 220, 220)); //
        artProvider->SetColor(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR, *wxBLACK); //accessibility: always set both foreground AND background colors!
        //wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT) -> better than wxBLACK, but which background to use?
    }

    auiMgr_.GetPane(m_gridOverview).MinSize(-1, -1); //we successfully tricked wxAuiManager into setting an initial Window size :> incomplete API anyone??
    auiMgr_.Update(); //

    defaultPerspective_ = auiMgr_.SavePerspective();
    //----------------------------------------------------------------------------------
    //register view layout context menu
    m_panelTopButtons->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(MainDialog::OnContextSetLayout), nullptr, this);
    m_panelConfig    ->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(MainDialog::OnContextSetLayout), nullptr, this);
    m_panelViewFilter->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(MainDialog::OnContextSetLayout), nullptr, this);
    m_panelStatusBar ->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(MainDialog::OnContextSetLayout), nullptr, this);
    //----------------------------------------------------------------------------------

    //file grid: sorting
    m_gridMainL->Connect(EVENT_GRID_COL_LABEL_MOUSE_LEFT,  GridLabelClickEventHandler(MainDialog::onGridLabelLeftClickL), nullptr, this);
    m_gridMainC->Connect(EVENT_GRID_COL_LABEL_MOUSE_LEFT,  GridLabelClickEventHandler(MainDialog::onGridLabelLeftClickC), nullptr, this);
    m_gridMainR->Connect(EVENT_GRID_COL_LABEL_MOUSE_LEFT,  GridLabelClickEventHandler(MainDialog::onGridLabelLeftClickR), nullptr, this);

    m_gridMainL->Connect(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, GridLabelClickEventHandler(MainDialog::onGridLabelContextL), nullptr, this);
    m_gridMainC->Connect(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, GridLabelClickEventHandler(MainDialog::onGridLabelContextC), nullptr, this);
    m_gridMainR->Connect(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, GridLabelClickEventHandler(MainDialog::onGridLabelContextR), nullptr, this);

    //file grid: context menu
    m_gridMainL->Connect(EVENT_GRID_MOUSE_RIGHT_UP,   GridClickEventHandler(MainDialog::onMainGridContextL), nullptr, this);
    m_gridMainC->Connect(EVENT_GRID_MOUSE_RIGHT_DOWN, GridClickEventHandler(MainDialog::onMainGridContextC), nullptr, this);
    m_gridMainR->Connect(EVENT_GRID_MOUSE_RIGHT_UP,   GridClickEventHandler(MainDialog::onMainGridContextR), nullptr, this);

    m_gridMainL->Connect(EVENT_GRID_MOUSE_LEFT_DOUBLE, GridClickEventHandler(MainDialog::onGridDoubleClickL), nullptr, this);
    m_gridMainR->Connect(EVENT_GRID_MOUSE_LEFT_DOUBLE, GridClickEventHandler(MainDialog::onGridDoubleClickR), nullptr, this);

    //tree grid:
    m_gridOverview->Connect(EVENT_GRID_MOUSE_RIGHT_UP, GridClickEventHandler(MainDialog::onTreeGridContext),   nullptr, this);
    m_gridOverview->Connect(EVENT_GRID_SELECT_RANGE,  GridSelectEventHandler(MainDialog::onTreeGridSelection), nullptr, this);

    //cfg grid:
    m_gridCfgHistory->Connect(EVENT_GRID_SELECT_RANGE,      GridSelectEventHandler(MainDialog::onCfgGridSelection),   nullptr, this);
    m_gridCfgHistory->Connect(EVENT_GRID_MOUSE_LEFT_DOUBLE,  GridClickEventHandler(MainDialog::onCfgGridDoubleClick), nullptr, this);
    m_gridCfgHistory->getMainWin().Connect(wxEVT_KEY_DOWN,       wxKeyEventHandler(MainDialog::onCfgGridKeyEvent),    nullptr, this);
    m_gridCfgHistory->Connect(EVENT_GRID_MOUSE_RIGHT_UP,     GridClickEventHandler(MainDialog::onCfgGridContext),     nullptr, this);
    m_gridCfgHistory->Connect(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, GridLabelClickEventHandler(MainDialog::onCfgGridLabelContext  ), nullptr, this);
    m_gridCfgHistory->Connect(EVENT_GRID_COL_LABEL_MOUSE_LEFT,  GridLabelClickEventHandler(MainDialog::onCfgGridLabelLeftClick), nullptr, this);
    //----------------------------------------------------------------------------------

    m_panelSearch->Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(MainDialog::OnSearchPanelKeyPressed), nullptr, this);

    //set tool tips with (non-translated!) short cut hint
    m_bpButtonNew        ->SetToolTip(replaceCpy(_("&New"),                  L"&", L"") + L" (Ctrl+N)"); //
    m_bpButtonOpen       ->SetToolTip(replaceCpy(_("&Open..."),              L"&", L"") + L" (Ctrl+O)"); //
    m_bpButtonSave       ->SetToolTip(replaceCpy(_("&Save"),                 L"&", L"") + L" (Ctrl+S)"); //reuse texts from gui builder
    m_bpButtonSaveAs     ->SetToolTip(replaceCpy(_("Save &as..."),           L"&", L""));                //
    m_bpButtonSaveAsBatch->SetToolTip(replaceCpy(_("Save as &batch job..."), L"&", L""));                //

    m_buttonCompare     ->SetToolTip(replaceCpy(_("Start &comparison"),         L"&", L"") + L" (F5)"); //
    m_bpButtonCmpConfig ->SetToolTip(replaceCpy(_("C&omparison settings"),      L"&", L"") + L" (F6)"); //
    m_bpButtonSyncConfig->SetToolTip(replaceCpy(_("S&ynchronization settings"), L"&", L"") + L" (F8)"); //
    m_buttonSync        ->SetToolTip(replaceCpy(_("Start &synchronization"),    L"&", L"") + L" (F9)"); //

    m_bpButtonCmpContext ->SetToolTip(m_bpButtonCmpConfig ->GetToolTipText());
    m_bpButtonSyncContext->SetToolTip(m_bpButtonSyncConfig->GetToolTipText());


    {
        const wxBitmap& bmpFile = IconBuffer::genericFileIcon(IconBuffer::SIZE_SMALL);
        const wxBitmap& bmpDir  = IconBuffer::genericDirIcon (IconBuffer::SIZE_SMALL);

        m_bitmapSmallDirectoryLeft ->SetBitmap(bmpDir);
        m_bitmapSmallFileLeft      ->SetBitmap(bmpFile);
        m_bitmapSmallDirectoryRight->SetBitmap(bmpDir);
        m_bitmapSmallFileRight     ->SetBitmap(bmpFile);
    }

    m_menuItemNew        ->SetBitmap(getResourceImage(L"new_small"));
    m_menuItemLoad       ->SetBitmap(getResourceImage(L"load_small"));
    m_menuItemSave       ->SetBitmap(getResourceImage(L"save_small"));
    m_menuItemSaveAsBatch->SetBitmap(getResourceImage(L"batch_small"));

    m_menuItemCompare     ->SetBitmap(getResourceImage(L"compare_small"));
    m_menuItemCompSettings->SetBitmap(getResourceImage(L"cfg_compare_small"));
    m_menuItemFilter      ->SetBitmap(getResourceImage(L"filter_small"));
    m_menuItemSyncSettings->SetBitmap(getResourceImage(L"cfg_sync_small"));
    m_menuItemSynchronize ->SetBitmap(getResourceImage(L"sync_small"));

    m_menuItemOptions     ->SetBitmap(getResourceImage(L"settings_small"));
    m_menuItemFind        ->SetBitmap(getResourceImage(L"find_small"));

    m_menuItemHelp ->SetBitmap(getResourceImage(L"help_small"));
    m_menuItemAbout->SetBitmap(getResourceImage(L"about_small"));
    m_menuItemCheckVersionNow->SetBitmap(getResourceImage(L"update_check_small"));

    //create language selection menu
    for (const TranslationInfo& ti : getExistingTranslations())
    {
        wxMenuItem* newItem = new wxMenuItem(m_menuLanguages, wxID_ANY, ti.languageName);
        newItem->SetBitmap(getResourceImage(ti.languageFlag));

        m_menuLanguages->Bind(wxEVT_COMMAND_MENU_SELECTED, [this, langId = ti.languageID](wxCommandEvent&) { this->switchProgramLanguage(langId); }, newItem->GetId());
        m_menuLanguages->Append(newItem); //pass ownership
    }


    //set up layout items to toggle showing hidden panels
    m_menuItemShowMain      ->SetItemLabel(replaceCpy(_("Show \"%x\""), L"%x", _("Main Bar")));
    m_menuItemShowFolders   ->SetItemLabel(replaceCpy(_("Show \"%x\""), L"%x", _("Folder Pairs")));
    m_menuItemShowViewFilter->SetItemLabel(replaceCpy(_("Show \"%x\""), L"%x", _("View Settings")));
    m_menuItemShowConfig    ->SetItemLabel(replaceCpy(_("Show \"%x\""), L"%x", _("Configuration")));
    m_menuItemShowOverview  ->SetItemLabel(replaceCpy(_("Show \"%x\""), L"%x", _("Overview")));

    auto setupLayoutMenuEvent = [&](wxMenuItem* menuItem, wxWindow* panelWindow)
    {
        m_menuTools->Bind(wxEVT_COMMAND_MENU_SELECTED, [this, panelWindow](wxCommandEvent&)
        {
            wxAuiPaneInfo& paneInfo = this->auiMgr_.GetPane(panelWindow);
            paneInfo.Show();
            this->auiMgr_.Update();
        }, menuItem->GetId());

        //"hide" menu items by default
        detachedMenuItems_.insert(m_menuTools->Remove(menuItem)); //pass ownership
    };
    setupLayoutMenuEvent(m_menuItemShowMain,       m_panelTopButtons);
    setupLayoutMenuEvent(m_menuItemShowFolders,    m_panelDirectoryPairs);
    setupLayoutMenuEvent(m_menuItemShowViewFilter, m_panelViewFilter);
    setupLayoutMenuEvent(m_menuItemShowConfig,     m_panelConfig);
    setupLayoutMenuEvent(m_menuItemShowOverview,   m_gridOverview);

    m_menuTools->Connect(wxEVT_MENU_OPEN, wxMenuEventHandler(MainDialog::onOpenMenuTools), nullptr, this);


    //show FreeFileSync update reminder
    if (!globalSettings.gui.lastOnlineVersion.empty() && haveNewerVersionOnline(globalSettings.gui.lastOnlineVersion))
    {
        auto menu = new wxMenu();
        wxMenuItem* newItem = new wxMenuItem(menu, wxID_ANY, _("&Show details"));
        this->Connect(newItem->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainDialog::OnMenuUpdateAvailable));
        menu->Append(newItem); //pass ownership
        m_menubar1->Append(menu, L"\u2605 " + replaceCpy(_("FreeFileSync %x is available!"), L"%x", utfTo<std::wstring>(globalSettings.gui.lastOnlineVersion)) + L" \u2605"); //"BLACK STAR"
    }

    //notify about (logical) application main window => program won't quit, but stay on this dialog
    setMainWindow(this);

    //init handling of first folder pair
    firstFolderPair_ = std::make_unique<FolderPairFirst>(*this);

    initViewFilterButtons();

    //init grid settings
    filegrid::init(*m_gridMainL, *m_gridMainC, *m_gridMainR);
    treegrid::init(*m_gridOverview);
    cfggrid ::init(*m_gridCfgHistory);

    //initialize and load configuration
    setGlobalCfgOnInit(globalSettings);
    setConfig(guiCfg, referenceFiles);

    //support for CTRL + C and DEL on grids
    m_gridMainL->getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(MainDialog::onGridButtonEventL), nullptr, this);
    m_gridMainC->getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(MainDialog::onGridButtonEventC), nullptr, this);
    m_gridMainR->getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(MainDialog::onGridButtonEventR), nullptr, this);

    m_gridOverview->getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(MainDialog::onTreeButtonEvent), nullptr, this);

    //enable dialog-specific key local events
    Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(MainDialog::onLocalKeyEvent), nullptr, this);

    //drag and drop .ffs_gui and .ffs_batch on main dialog
    setupFileDrop(*this);
    Connect(EVENT_DROP_FILE, FileDropEventHandler(MainDialog::onDialogFilesDropped), nullptr, this);

    //Connect(wxEVT_SIZE, wxSizeEventHandler(MainDialog::OnResize), nullptr, this);
    //Connect(wxEVT_MOVE, wxSizeEventHandler(MainDialog::OnResize), nullptr, this);

    //calculate witdh of folder pair manually (if scrollbars are visible)
    m_panelTopLeft->Connect(wxEVT_SIZE, wxEventHandler(MainDialog::OnResizeLeftFolderWidth), nullptr, this);

    //dynamically change sizer direction depending on size
    m_panelTopButtons->Connect(wxEVT_SIZE, wxEventHandler(MainDialog::OnResizeTopButtonPanel), nullptr, this);
    m_panelConfig    ->Connect(wxEVT_SIZE, wxEventHandler(MainDialog::OnResizeConfigPanel),    nullptr, this);
    m_panelViewFilter->Connect(wxEVT_SIZE, wxEventHandler(MainDialog::OnResizeViewPanel),      nullptr, this);
    wxSizeEvent dummy3;
    OnResizeTopButtonPanel(dummy3); //
    OnResizeConfigPanel   (dummy3); //call once on window creation
    OnResizeViewPanel     (dummy3); //

    //event handler for manual (un-)checking of rows and setting of sync direction
    m_gridMainC->Connect(EVENT_GRID_CHECK_ROWS,     CheckRowsEventHandler    (MainDialog::onCheckRows), nullptr, this);
    m_gridMainC->Connect(EVENT_GRID_SYNC_DIRECTION, SyncDirectionEventHandler(MainDialog::onSetSyncDirection), nullptr, this);

    //mainly to update row label sizes...
    updateGui();

    //register regular check for update on next idle event
    Connect(wxEVT_IDLE, wxIdleEventHandler(MainDialog::OnRegularUpdateCheck), nullptr, this);

    //asynchronous call to wxWindow::Layout(): fix superfluous frame on right and bottom when FFS is started in fullscreen mode
    Connect(wxEVT_IDLE, wxIdleEventHandler(MainDialog::OnLayoutWindowAsync), nullptr, this);
    wxCommandEvent evtDummy;           //call once before OnLayoutWindowAsync()
    OnResizeLeftFolderWidth(evtDummy); //

    //scroll cfg history to last used position. We cannot do this earlier e.g. in setGlobalCfgOnInit()
    //1. setConfig() indirectly calls cfggrid::addAndSelect() which changes cfg history scroll position
    //2. Grid::makeRowVisible() requires final window height! => do this after window resizing is complete
    if (m_gridCfgHistory->getRowCount() > 0)
        m_gridCfgHistory->scrollTo(numeric::clampCpy<size_t>(globalSettings.gui.mainDlg.cfgGridTopRowPos, //must be set *after* wxAuiManager::LoadPerspective() to have any effect
                                                             0,  m_gridCfgHistory->getRowCount() - 1));

    //first selected item should always be visible:
    const std::vector<size_t> selectedRows = m_gridCfgHistory->getSelectedRows();
    if (!selectedRows.empty())
        m_gridCfgHistory->makeRowVisible(selectedRows.front());

    m_buttonCompare->SetFocus();

    //----------------------------------------------------------------------------------------------------------------------------------------------------------------
    //some convenience: if FFS is started with a *.ffs_gui file as commandline parameter AND all directories contained exist, comparison shall be started right away
    if (startComparison)
    {
        const MainConfiguration currMainCfg = getConfig().mainCfg;

        //------------------------------------------------------------------------------------------
        //harmonize checks with comparison.cpp:: checkForIncompleteInput()
        //we're really doing two checks: 1. check directory existence 2. check config validity -> don't mix them!
        bool havePartialPair = false;
        bool haveFullPair    = false;

        std::vector<AbstractPath> folderPathsToCheck;

        auto addFolderCheck = [&](const FolderPairEnh& fp)
        {
            const AbstractPath folderPathL = createAbstractPath(fp.folderPathPhraseLeft_);
            const AbstractPath folderPathR = createAbstractPath(fp.folderPathPhraseRight_);

            if (AFS::isNullPath(folderPathL) != AFS::isNullPath(folderPathR)) //only skip check if both sides are empty!
                havePartialPair = true;
            else if (!AFS::isNullPath(folderPathL))
                haveFullPair = true;

            if (!AFS::isNullPath(folderPathL))
                folderPathsToCheck.push_back(folderPathL); //noexcept
            if (!AFS::isNullPath(folderPathR))
                folderPathsToCheck.push_back(folderPathR); //noexcept
        };

        addFolderCheck(currMainCfg.firstPair);
        for (const FolderPairEnh& fp : currMainCfg.additionalPairs)
            addFolderCheck(fp);
        //------------------------------------------------------------------------------------------

        if (havePartialPair != haveFullPair) //either all pairs full or all half-filled -> validity check!
        {
            //check existence of all directories in parallel!
            GetFirstResult<FalseType> firstMissingDir;
            for (const AbstractPath& folderPath : folderPathsToCheck)
                firstMissingDir.addJob([folderPath]() -> Opt<FalseType>
            {
                try
                {
                    if (AFS::getItemType(folderPath) != AFS::ItemType::FILE) //throw FileError
                        return NoValue();
                }
                catch (FileError&) {}
                return FalseType();
            });

            const bool startComparisonNow = !firstMissingDir.timedWait(std::chrono::milliseconds(500)) || //= no result yet => start comparison anyway!
                                            !firstMissingDir.get(); //= all directories exist

            if (startComparisonNow)
            {
                wxCommandEvent dummy2(wxEVT_COMMAND_BUTTON_CLICKED);
                //better!? => m_buttonCompare->Command(dummy2); //simulate click
                if (wxEvtHandler* evtHandler = m_buttonCompare->GetEventHandler())
                    evtHandler->AddPendingEvent(dummy2); //simulate button click on "compare"
            }
        }
    }
}


MainDialog::~MainDialog()
{
    Opt<FileError> firstError;
    try //save "GlobalSettings.xml"
    {
        writeConfig(getGlobalCfgBeforeExit(), globalConfigFilePath_); //throw FileError
    }
    catch (const FileError& e) { firstError = e; }

    try //save "LastRun.ffs_gui"
    {
        writeConfig(getConfig(), lastRunConfigPath_); //throw FileError
    }
    catch (const FileError& e)
    {
        if (!firstError)
            firstError = e;
    }

    //don't annoy users on read-only drives: it's enough to show a single error message when saving global config
    if (firstError)
        showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(firstError->toString()));

    auiMgr_.UnInit();

    for (wxMenuItem* item : detachedMenuItems_)
        delete item; //something's got to give

    //no need for wxEventHandler::Disconnect() here; event sources are components of this window and are destroyed, too
}

//-------------------------------------------------------------------------------------------------------------------------------------

void MainDialog::onQueryEndSession()
{
    //we try our best to do something useful in this extreme situation - no reason to notify or even log errors here!
    try { writeConfig(getGlobalCfgBeforeExit(), globalConfigFilePath_); }
    catch (const FileError&) {}

    try { writeConfig(getConfig(), lastRunConfigPath_); }
    catch (const FileError&) {}
}


void MainDialog::OnClose(wxCloseEvent& event)
{
    //attention: system shutdown is handled in onQueryEndSession()!

    //regular destruction handling
    if (event.CanVeto())
    {
        //=> veto all attempts to close the main window while comparison or synchronization are running:
        if (!allowMainDialogClose_)
        {
            event.Veto();
            Raise();    //=what Windows does when vetoing a close (via middle mouse on taskbar preview) while showing a modal dialog
            SetFocus(); //
            return;
        }

        const bool cancelled = !saveOldConfig(); //notify user about changed settings
        if (cancelled)
        {
            event.Veto();
            return;
        }
    }

    Destroy();
}


void MainDialog::setGlobalCfgOnInit(const XmlGlobalSettings& globalSettings)
{
    globalCfg_ = globalSettings;

    //caveat set/get language asymmmetry! setLanguage(globalSettings.programLanguage); //throw FileError
    //we need to set langugabe before creating this class!

    //set dialog size and position:
    // - width/height are invalid if the window is minimized (eg x,y == -32000; height = 28, width = 160)
    // - multi-monitor setups: dialog may be placed on second monitor which is currently turned off
    if (globalSettings.gui.mainDlg.dlgSize.GetWidth () > 0 &&
        globalSettings.gui.mainDlg.dlgSize.GetHeight() > 0)
    {
        //calculate how much of the dialog will be visible on screen
        const int dialogAreaTotal = globalSettings.gui.mainDlg.dlgSize.GetWidth() * globalSettings.gui.mainDlg.dlgSize.GetHeight();
        int dialogAreaVisible = 0;

        const int monitorCount = wxDisplay::GetCount();
        for (int i = 0; i < monitorCount; ++i)
        {
            wxRect intersection = wxDisplay(i).GetClientArea().Intersect(wxRect(globalSettings.gui.mainDlg.dlgPos, globalSettings.gui.mainDlg.dlgSize));
            dialogAreaVisible = std::max(dialogAreaVisible, intersection.GetWidth() * intersection.GetHeight());
        }

        //wxGTK's wxWindow::SetSize seems unreliable and behaves like a wxWindow::SetClientSize
        //=> use wxWindow::SetClientSize instead (for the record: no such issue on Windows/OS X)
        SetClientSize(globalSettings.gui.mainDlg.dlgSize);

        if (dialogAreaVisible > 0.1 * dialogAreaTotal  //at least 10% of the dialog should be visible!
           )
            SetPosition(globalSettings.gui.mainDlg.dlgPos);
        else
            Center();
    }
    else
        Center();

    if (globalSettings.gui.mainDlg.isMaximized) //no real need to support both maximize and full screen functions
    {
        //#ifdef ZEN_MAC
        //        if (fullScreenApiSupported) -> starting in full screen seems to annoy users
        //            ShowFullScreen(true); //once EnableFullScreenView() is set, this internally uses the new full screen API
        //        else
        //#endif
        Maximize(true);
    }

    //set column attributes
    m_gridMainL   ->setColumnConfig(convertColAttributes(globalSettings.gui.mainDlg.columnAttribLeft,  getFileGridDefaultColAttribsLeft()));
    m_gridMainR   ->setColumnConfig(convertColAttributes(globalSettings.gui.mainDlg.columnAttribRight, getFileGridDefaultColAttribsLeft()));
    m_splitterMain->setSashOffset(globalSettings.gui.mainDlg.sashOffset);

    m_gridOverview->setColumnConfig(convertColAttributes(globalSettings.gui.mainDlg.treeGridColumnAttribs, getTreeGridDefaultColAttribs()));
    treegrid::setShowPercentage(*m_gridOverview, globalSettings.gui.mainDlg.treeGridShowPercentBar);

    treegrid::getDataView(*m_gridOverview).setSortDirection(globalSettings.gui.mainDlg.treeGridLastSortColumn, globalSettings.gui.mainDlg.treeGridLastSortAscending);

    //--------------------------------------------------------------------------------
    //load list of configuration files
    std::vector<Zstring> cfgFilePaths;
    std::vector<std::pair<Zstring, time_t>> lastSyncTimes;
    //list is stored with last used files first in XML, however m_gridCfgHistory expects them last!!!
    std::for_each(globalSettings.gui.mainDlg.cfgFileHistory.crbegin(),
                  globalSettings.gui.mainDlg.cfgFileHistory.crend(),
                  [&](const ConfigFileItem& item)
    {
        cfgFilePaths.push_back(item.filePath);
        lastSyncTimes.emplace_back(item.filePath, item.lastSyncTime);
    });
    cfgFilePaths.push_back(lastRunConfigPath_); //make sure <Last session> is always part of history list (if existing)

    cfggrid::getDataView(*m_gridCfgHistory).addCfgFiles(cfgFilePaths);
    cfggrid::getDataView(*m_gridCfgHistory).setLastSyncTime(lastSyncTimes);
    m_gridCfgHistory->Refresh();

    //globalSettings.gui.mainDlg.cfgGridTopRowPos => defer evaluation until later within MainDialog constructor
    m_gridCfgHistory->setColumnConfig(convertColAttributes(globalSettings.gui.mainDlg.cfgGridColumnAttribs, getCfgGridDefaultColAttribs()));
    cfggrid::getDataView(*m_gridCfgHistory).setSortDirection(globalSettings.gui.mainDlg.cfgGridLastSortColumn, globalSettings.gui.mainDlg.cfgGridLastSortAscending);
    cfggrid::setSyncOverdueDays(*m_gridCfgHistory, globalSettings.gui.mainDlg.cfgGridSyncOverdueDays);
    //m_gridCfgHistory->Refresh(); <- implicit in last call

    cfgHistoryRemoveObsolete(cfgFilePaths); //remove non-existent items (we need this only on startup)
    //--------------------------------------------------------------------------------

    //load list of last used folders
    *folderHistoryLeft_  = FolderHistory(globalSettings.gui.mainDlg.folderHistoryLeft,  globalSettings.gui.mainDlg.folderHistItemsMax);
    *folderHistoryRight_ = FolderHistory(globalSettings.gui.mainDlg.folderHistoryRight, globalSettings.gui.mainDlg.folderHistItemsMax);

    //show/hide file icons
    filegrid::setupIcons(*m_gridMainL, *m_gridMainC, *m_gridMainR, globalSettings.gui.mainDlg.showIcons, convert(globalSettings.gui.mainDlg.iconSize));

    filegrid::setItemPathForm(*m_gridMainL, globalSettings.gui.mainDlg.itemPathFormatLeftGrid);
    filegrid::setItemPathForm(*m_gridMainR, globalSettings.gui.mainDlg.itemPathFormatRightGrid);

    //------------------------------------------------------------------------------------------------
    m_checkBoxMatchCase->SetValue(globalCfg_.gui.mainDlg.textSearchRespectCase);

    //wxAuiManager erroneously loads panel captions, we don't want that
    std::vector<std::pair<wxString, wxString>>captionNameMap;
    const wxAuiPaneInfoArray& paneArray = auiMgr_.GetAllPanes();
    for (size_t i = 0; i < paneArray.size(); ++i)
        captionNameMap.emplace_back(paneArray[i].caption, paneArray[i].name);

    auiMgr_.LoadPerspective(globalSettings.gui.mainDlg.guiPerspectiveLast);

    //restore original captions
    for (const auto& item : captionNameMap)
        auiMgr_.GetPane(item.second).Caption(item.first);
    //------------------------------------------------------------------------------------------------

    //if MainDialog::onQueryEndSession() is called while comparison is active, this panel is saved and restored as "visible"
    auiMgr_.GetPane(compareStatus_->getAsWindow()).Hide();

    auiMgr_.GetPane(m_panelSearch).Hide(); //no need to show it on startup

    m_menuItemCheckVersionAuto->Check(updateCheckActive(globalCfg_.gui.lastUpdateCheck));

    auiMgr_.Update();
}


XmlGlobalSettings MainDialog::getGlobalCfgBeforeExit()
{
    Freeze(); //no need to Thaw() again!!

    XmlGlobalSettings globalSettings = globalCfg_;

    globalSettings.programLanguage = getLanguage();

    //retrieve column attributes
    globalSettings.gui.mainDlg.columnAttribLeft  = convertColAttributes<ColAttributesRim>(m_gridMainL->getColumnConfig());
    globalSettings.gui.mainDlg.columnAttribRight = convertColAttributes<ColAttributesRim>(m_gridMainR->getColumnConfig());
    globalSettings.gui.mainDlg.sashOffset        = m_splitterMain->getSashOffset();

    globalSettings.gui.mainDlg.treeGridColumnAttribs  = convertColAttributes<ColAttributesTree>(m_gridOverview->getColumnConfig());
    globalSettings.gui.mainDlg.treeGridShowPercentBar = treegrid::getShowPercentage(*m_gridOverview);

    std::tie(globalSettings.gui.mainDlg.treeGridLastSortColumn,
             globalSettings.gui.mainDlg.treeGridLastSortAscending) = treegrid::getDataView(*m_gridOverview).getSortDirection();

    //--------------------------------------------------------------------------------
    //write list of configuration files
    std::map<int, ConfigFileItem, std::greater<>> cfgItemsSorted; //sort by last use; put most recent items *first* (looks better in XML than reverted)
    for (size_t i = 0; i < m_gridCfgHistory->getRowCount(); ++i)
        if (const ConfigView::Details* cfg = cfggrid::getDataView(*m_gridCfgHistory).getItem(i))
            cfgItemsSorted.emplace(cfg->lastUseIndex, ConfigFileItem{ cfg->filePath, cfg->lastSyncTime });
        else
            assert(false);

    std::vector<ConfigFileItem> cfgHistory;
    for (const auto& item : cfgItemsSorted)
        cfgHistory.emplace_back(item.second);

    if (cfgHistory.size() > globalSettings.gui.mainDlg.cfgHistItemsMax) //erase oldest elements
        cfgHistory.resize(globalSettings.gui.mainDlg.cfgHistItemsMax);

    globalSettings.gui.mainDlg.cfgFileHistory = cfgHistory;

    globalSettings.gui.mainDlg.cfgGridTopRowPos       = m_gridCfgHistory->getTopRow();
    globalSettings.gui.mainDlg.cfgGridColumnAttribs   = convertColAttributes<ColAttributesCfg>(m_gridCfgHistory->getColumnConfig());
    globalSettings.gui.mainDlg.cfgGridSyncOverdueDays = cfggrid::getSyncOverdueDays(*m_gridCfgHistory);

    std::tie(globalSettings.gui.mainDlg.cfgGridLastSortColumn,
             globalSettings.gui.mainDlg.cfgGridLastSortAscending) = cfggrid::getDataView(*m_gridCfgHistory).getSortDirection();
    //--------------------------------------------------------------------------------
    globalSettings.gui.mainDlg.lastUsedConfigFiles = activeConfigFiles_;

    //write list of last used folders
    globalSettings.gui.mainDlg.folderHistoryLeft  = folderHistoryLeft_ ->getList();
    globalSettings.gui.mainDlg.folderHistoryRight = folderHistoryRight_->getList();

    globalSettings.gui.mainDlg.textSearchRespectCase = m_checkBoxMatchCase->GetValue();

    globalSettings.gui.mainDlg.guiPerspectiveLast = auiMgr_.SavePerspective();

    //we need to portably retrieve non-iconized, non-maximized size and position (non-portable: GetWindowPlacement())
    //call *after* wxAuiManager::SavePerspective()!
    if (IsIconized())
        Iconize(false);

    globalSettings.gui.mainDlg.isMaximized = false;
    if (IsMaximized()) //evaluate AFTER uniconizing!
    {
        globalSettings.gui.mainDlg.isMaximized = true;
        Maximize(false);
    }

    globalSettings.gui.mainDlg.dlgSize = GetClientSize();
    globalSettings.gui.mainDlg.dlgPos  = GetPosition();

    //wxGTK: returns full screen size and strange position (65/-4)
    //OS X 10.9 (but NO issue on 10.11!) returns full screen size and strange position (0/-22)
    if (globalSettings.gui.mainDlg.isMaximized)
        if (globalSettings.gui.mainDlg.dlgPos.y < 0)
        {
            globalSettings.gui.mainDlg.dlgSize = wxSize();
            globalSettings.gui.mainDlg.dlgPos  = wxPoint();
        }

    return globalSettings;
}


void MainDialog::setSyncDirManually(const std::vector<FileSystemObject*>& selection, SyncDirection direction)
{
    if (!selection.empty())
    {
        for (FileSystemObject* fsObj : selection)
        {
            setSyncDirectionRec(direction, *fsObj); //set new direction (recursively)
            setActiveStatus(true, *fsObj); //works recursively for directories
        }
        updateGui();
    }
}


void MainDialog::setFilterManually(const std::vector<FileSystemObject*>& selection, bool setIncluded)
{
    //if hidefiltered is active, there should be no filtered elements on screen => current element was filtered out
    assert(m_bpButtonShowExcluded->isActive() || !setIncluded);

    if (!selection.empty())
    {
        for (FileSystemObject* fsObj : selection)
            setActiveStatus(setIncluded, *fsObj); //works recursively for directories

        updateGuiDelayedIf(!m_bpButtonShowExcluded->isActive()); //show update GUI before removing rows
    }
}


namespace
{
//perf: wxString doesn't model exponential growth and is unsuitable for large data sets
using zxString = Zbase<wchar_t>; //guaranteed exponential growth
}

void MainDialog::copySelectionToClipboard(const std::vector<const Grid*>& gridRefs)
{
    try
    {
        zxString clipboardString;

        auto addSelection = [&](const Grid& grid)
        {
            if (auto prov = grid.getDataProvider())
            {
                std::vector<Grid::ColAttributes> colAttr = grid.getColumnConfig();
                erase_if(colAttr, [](const Grid::ColAttributes& ca) { return !ca.visible; });
                if (!colAttr.empty())
                    for (size_t row : grid.getSelectedRows())
                    {
                        std::for_each(colAttr.begin(), colAttr.end() - 1,
                                      [&](const Grid::ColAttributes& ca)
                        {
                            clipboardString += copyStringTo<zxString>(prov->getValue(row, ca.type));
                            clipboardString += L'\t';
                        });
                        clipboardString += copyStringTo<zxString>(prov->getValue(row, colAttr.back().type));
                        clipboardString += L'\n';
                    }
            }
        };

        for (const Grid* gr : gridRefs)
            addSelection(*gr);

        //finally write to clipboard
        if (wxClipboard::Get()->Open())
        {
            ZEN_ON_SCOPE_EXIT(wxClipboard::Get()->Close());
            wxClipboard::Get()->SetData(new wxTextDataObject(copyStringTo<wxString>(clipboardString))); //ownership passed
        }
    }
    catch (const std::bad_alloc& e)
    {
        showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setMainInstructions(_("Out of memory.") + L" " + utfTo<std::wstring>(e.what())));
    }
}


std::vector<FileSystemObject*> MainDialog::getGridSelection(bool fromLeft, bool fromRight) const
{
    std::vector<size_t> selectedRows;

    if (fromLeft)
        append(selectedRows, m_gridMainL->getSelectedRows());

    if (fromRight)
        append(selectedRows, m_gridMainR->getSelectedRows());

    removeDuplicates(selectedRows);
    assert(std::is_sorted(selectedRows.begin(), selectedRows.end()));

    return filegrid::getDataView(*m_gridMainC).getAllFileRef(selectedRows);
}


std::vector<FileSystemObject*> MainDialog::getTreeSelection() const
{
    std::vector<FileSystemObject*> output;

    for (size_t row : m_gridOverview->getSelectedRows())
        if (std::unique_ptr<TreeView::Node> node = treegrid::getDataView(*m_gridOverview).getLine(row))
        {
            if (auto root = dynamic_cast<const TreeView::RootNode*>(node.get()))
            {
                //selecting root means "select everything", *ignoring* current view filter!
                BaseFolderPair& baseDir = root->baseFolder_;

                std::vector<FileSystemObject*> dirsFilesAndLinks;

                for (FileSystemObject& fsObj : baseDir.refSubFolders()) //no need to explicitly add child elements!
                    dirsFilesAndLinks.push_back(&fsObj);
                for (FileSystemObject& fsObj : baseDir.refSubFiles())
                    dirsFilesAndLinks.push_back(&fsObj);
                for (FileSystemObject& fsObj : baseDir.refSubLinks())
                    dirsFilesAndLinks.push_back(&fsObj);

                append(output, dirsFilesAndLinks);
            }
            else if (auto dir = dynamic_cast<const TreeView::DirNode*>(node.get()))
                output.push_back(&(dir->folder_));
            else if (auto file = dynamic_cast<const TreeView::FilesNode*>(node.get()))
                append(output, file->filesAndLinks_);
            else assert(false);
        }
    return output;
}


void MainDialog::copyToAlternateFolder(const std::vector<FileSystemObject*>& selectionLeft,
                                       const std::vector<FileSystemObject*>& selectionRight)
{
    std::vector<const FileSystemObject*> rowsLeftTmp;
    std::vector<const FileSystemObject*> rowsRightTmp;
    std::copy_if(selectionLeft .begin(), selectionLeft .end(), std::back_inserter(rowsLeftTmp),  [](const FileSystemObject* fsObj) { return !fsObj->isEmpty< LEFT_SIDE>(); });
    std::copy_if(selectionRight.begin(), selectionRight.end(), std::back_inserter(rowsRightTmp), [](const FileSystemObject* fsObj) { return !fsObj->isEmpty<RIGHT_SIDE>(); });

    if (rowsLeftTmp.empty() && rowsRightTmp.empty())
        return;

    FocusPreserver fp;

    if (showCopyToDialog(this,
                         rowsLeftTmp, rowsRightTmp,
                         globalCfg_.gui.mainDlg.copyToCfg.lastUsedPath,
                         globalCfg_.gui.mainDlg.copyToCfg.folderHistory,
                         globalCfg_.gui.mainDlg.copyToCfg.historySizeMax,
                         globalCfg_.gui.mainDlg.copyToCfg.keepRelPaths,
                         globalCfg_.gui.mainDlg.copyToCfg.overwriteIfExists) != ReturnSmallDlg::BUTTON_OKAY)
        return;

    disableAllElements(true); //StatusHandlerTemporaryPanel will internally process Window messages, so avoid unexpected callbacks!
    auto app = wxTheApp; //fix lambda/wxWigets/VC fuck up
    ZEN_ON_SCOPE_EXIT(app->Yield(); enableAllElements()); //ui update before enabling buttons again: prevent strange behaviour of delayed button clicks

    try
    {
        StatusHandlerTemporaryPanel statusHandler(*this); //handle status display and error messages

        fff::copyToAlternateFolder(rowsLeftTmp, rowsRightTmp,
                                   globalCfg_.gui.mainDlg.copyToCfg.lastUsedPath,
                                   globalCfg_.gui.mainDlg.copyToCfg.keepRelPaths,
                                   globalCfg_.gui.mainDlg.copyToCfg.overwriteIfExists,
                                   globalCfg_.warnDlgs,
                                   statusHandler);
        //"clearSelection" not needed/desired
    }
    catch (AbortProcess&) {}

    //updateGui(); -> not needed
}


void MainDialog::deleteSelectedFiles(const std::vector<FileSystemObject*>& selectionLeft,
                                     const std::vector<FileSystemObject*>& selectionRight, bool moveToRecycler)
{
    std::vector<FileSystemObject*> rowsLeftTmp  = selectionLeft;
    std::vector<FileSystemObject*> rowsRightTmp = selectionRight;
    erase_if(rowsLeftTmp,  [](const FileSystemObject* fsObj) { return fsObj->isEmpty< LEFT_SIDE>(); });
    erase_if(rowsRightTmp, [](const FileSystemObject* fsObj) { return fsObj->isEmpty<RIGHT_SIDE>(); });
    if (rowsLeftTmp.empty() && rowsRightTmp.empty())
        return;

    FocusPreserver fp;

    //sigh: do senseless vector<FileSystemObject*> -> vector<const FileSystemObject*> conversion:
    if (showDeleteDialog(this, { rowsLeftTmp.begin(), rowsLeftTmp.end() }, { rowsRightTmp.begin(), rowsRightTmp.end() },
                         moveToRecycler) != ReturnSmallDlg::BUTTON_OKAY)
        return;

    disableAllElements(true); //StatusHandlerTemporaryPanel will internally process Window messages, so avoid unexpected callbacks!
    auto app = wxTheApp; //fix lambda/wxWigets/VC fuck up
    ZEN_ON_SCOPE_EXIT(app->Yield(); enableAllElements()); //ui update before enabling buttons again: prevent strange behaviour of delayed button clicks

    //wxBusyCursor dummy; -> redundant: progress already shown in status bar!
    try
    {
        StatusHandlerTemporaryPanel statusHandler(*this); //handle status display and error messages

        deleteFromGridAndHD(rowsLeftTmp, rowsRightTmp,
                            folderCmp_,
                            extractDirectionCfg(getConfig().mainCfg),
                            moveToRecycler,
                            globalCfg_.warnDlgs.warnRecyclerMissing,
                            statusHandler);

        m_gridMainL->clearSelection(ALLOW_GRID_EVENT);
        m_gridMainC->clearSelection(ALLOW_GRID_EVENT);
        m_gridMainR->clearSelection(ALLOW_GRID_EVENT);

        m_gridOverview->clearSelection(ALLOW_GRID_EVENT);
    }
    catch (AbortProcess&) {} //do not clear grids, if aborted!

    //remove rows that are empty: just a beautification, invalid rows shouldn't cause issues
    filegrid::getDataView(*m_gridMainC).removeInvalidRows();

    updateGui();
}


namespace
{
template <SelectedSide side>
AbstractPath getExistingParentFolder(const FileSystemObject& fsObj)
{
    auto folder = dynamic_cast<const FolderPair*>(&fsObj);
    if (!folder)
        folder = dynamic_cast<const FolderPair*>(&fsObj.parent());

    while (folder)
    {
        if (!folder->isEmpty<side>())
            return folder->getAbstractPath<side>();

        folder = dynamic_cast<const FolderPair*>(&folder->parent());
    }
    return fsObj.base().getAbstractPath<side>();
}


template <SelectedSide side, class Function>
void extractFileDescriptor(const FileSystemObject& fsObj, Function onDescriptor)
{
    if (!fsObj.isEmpty<side>())
        visitFSObject(fsObj, [](const FolderPair& folder) {},
    [&](const FilePair& file)
    {
        const FileDescriptor descr = { file.getAbstractPath<side>(), file.getAttributes<side>() };
        onDescriptor(descr);
    }, [](const SymlinkPair& symlink) {});
}


template <SelectedSide side>
void collectNonNativeFiles(const std::vector<FileSystemObject*>& selectedRows, const TempFileBuffer& tempFileBuf,
                           std::set<FileDescriptor>& workLoad)
{
    for (const FileSystemObject* fsObj : selectedRows)
        extractFileDescriptor<side>(*fsObj, [&](const FileDescriptor& descr)
    {
        if (!AFS::getNativeItemPath(descr.path))
            if (tempFileBuf.getTempPath(descr).empty()) //TempFileBuffer::createTempFiles() contract!
                workLoad.insert(descr);
    });
}


template <SelectedSide side>
void invokeCommandLine(const Zstring& commandLinePhrase, //throw FileError
                       const std::vector<FileSystemObject*>& selection,
                       const TempFileBuffer& tempFileBuf)
{
    static const SelectedSide side2 = OtherSide<side>::result;

    for (const FileSystemObject* fsObj : selection) //context menu calls this function only if selection is not empty!
    {
        const AbstractPath basePath  = fsObj->base().getAbstractPath<side >();
        const AbstractPath basePath2 = fsObj->base().getAbstractPath<side2>();

        //full path, even if item is not (yet) existing:
        const Zstring itemPath    = AFS::isNullPath(basePath ) ? Zstr("") : utfTo<Zstring>(AFS::getDisplayPath(fsObj->getAbstractPath<side >()));
        const Zstring itemPath2   = AFS::isNullPath(basePath2) ? Zstr("") : utfTo<Zstring>(AFS::getDisplayPath(fsObj->getAbstractPath<side2>()));
        const Zstring folderPath  = AFS::isNullPath(basePath ) ? Zstr("") : utfTo<Zstring>(AFS::getDisplayPath(fsObj->parent().getAbstractPath<side >()));
        const Zstring folderPath2 = AFS::isNullPath(basePath2) ? Zstr("") : utfTo<Zstring>(AFS::getDisplayPath(fsObj->parent().getAbstractPath<side2>()));

        Zstring localPath;
        Zstring localPath2;

        if (AFS::getNativeItemPath(basePath))
            localPath = itemPath; //no matter if item exists or not
        else //returns empty if not available (item not existing, error during copy):
            extractFileDescriptor<side>(*fsObj, [&](const FileDescriptor& descr) { localPath = tempFileBuf.getTempPath(descr); });

        if (AFS::getNativeItemPath(basePath2))
            localPath2 = itemPath2;
        else
            extractFileDescriptor<side2>(*fsObj, [&](const FileDescriptor& descr) { localPath2 = tempFileBuf.getTempPath(descr); });

        if (localPath .empty()) localPath  = replaceCpy(utfTo<Zstring>(L"<" + _("Local path not available for %x.") + L">"), Zstr("%x"), itemPath );
        if (localPath2.empty()) localPath2 = replaceCpy(utfTo<Zstring>(L"<" + _("Local path not available for %x.") + L">"), Zstr("%x"), itemPath2);

        Zstring command = commandLinePhrase;
        replace(command, Zstr("%item_path%"),    itemPath);
        replace(command, Zstr("%item_path2%"),   itemPath2);
        replace(command, Zstr("%folder_path%"),  folderPath);
        replace(command, Zstr("%folder_path2%"), folderPath2);
        replace(command, Zstr("%local_path%"),   localPath);
        replace(command, Zstr("%local_path2%"),  localPath2);

        shellExecute(command, selection.size() > EXT_APP_MASS_INVOKE_THRESHOLD ? ExecutionType::SYNC : ExecutionType::ASYNC); //throw FileError
    }
}
}


void MainDialog::openExternalApplication(const Zstring& commandLinePhrase, bool leftSide,
                                         const std::vector<FileSystemObject*>& selectionLeft,
                                         const std::vector<FileSystemObject*>& selectionRight)
{
    const XmlGlobalSettings::Gui defaultCfg;
    const bool openFileBrowserRequested = !defaultCfg.externalApps.empty() && defaultCfg.externalApps[0].cmdLine == commandLinePhrase;

    //support fallback instead of an error in this special case
    if (openFileBrowserRequested)
    {
        if (selectionLeft.size() + selectionRight.size() > 1) //do not open more than one Explorer instance!
        {
            if ((leftSide && !selectionLeft .empty()) ||
                (!leftSide && selectionRight.empty()))
                return openExternalApplication(commandLinePhrase, leftSide, { selectionLeft[0] }, {});
            else
                return openExternalApplication(commandLinePhrase, leftSide, {}, { selectionRight[0] });
        }

        auto openFolderInFileBrowser = [&](const AbstractPath& folderPath)
        {
            try
            {
                shellExecute("xdg-open \"" + utfTo<Zstring>(AFS::getDisplayPath(folderPath)) + "\"", ExecutionType::ASYNC); //
            }
            catch (const FileError& e) { showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString())); }
        };

        if (selectionLeft.empty() && selectionRight.empty())
            return openFolderInFileBrowser(leftSide ?
                                           createAbstractPath(firstFolderPair_->getValues().folderPathPhraseLeft_) :
                                           createAbstractPath(firstFolderPair_->getValues().folderPathPhraseRight_));
        //in this context either left or right selection is filled with exactly one item
        if (!selectionLeft.empty())
        {
            if (selectionLeft[0]->isEmpty<LEFT_SIDE>())
                return openFolderInFileBrowser(getExistingParentFolder<LEFT_SIDE>(*selectionLeft[0]));
        }
        else
        {
            if (selectionRight[0]->isEmpty<RIGHT_SIDE>())
                return openFolderInFileBrowser(getExistingParentFolder<RIGHT_SIDE>(*selectionRight[0]));
        }
    }

    //regular command evaluation:
    const size_t invokeCount = selectionLeft.size() + selectionRight.size();
    if (invokeCount > EXT_APP_MASS_INVOKE_THRESHOLD)
        if (globalCfg_.confirmDlgs.confirmExternalCommandMassInvoke)
        {
            bool dontAskAgain = false;
            switch (showConfirmationDialog(this, DialogInfoType::WARNING, PopupDialogCfg().
                                           setTitle(_("Confirm")).
                                           setMainInstructions(replaceCpy(_P("Do you really want to execute the command %y for one item?",
                                                                             "Do you really want to execute the command %y for %x items?", invokeCount),
                                                                          L"%y", fmtPath(commandLinePhrase))).
                                           setCheckBox(dontAskAgain, _("&Don't show this warning again")),
                                           _("&Execute")))
            {
                case ConfirmationButton::ACCEPT:
                    globalCfg_.confirmDlgs.confirmExternalCommandMassInvoke = !dontAskAgain;
                    break;
                case ConfirmationButton::CANCEL:
                    return;
            }
        }

    std::set<FileDescriptor> nonNativeFiles;
    if (contains(commandLinePhrase, Zstr("%local_path%")))
    {
        collectNonNativeFiles< LEFT_SIDE>(selectionLeft,  tempFileBuf_, nonNativeFiles);
        collectNonNativeFiles<RIGHT_SIDE>(selectionRight, tempFileBuf_, nonNativeFiles);
    }
    if (contains(commandLinePhrase, Zstr("%local_path2%")))
    {
        collectNonNativeFiles<RIGHT_SIDE>(selectionLeft,  tempFileBuf_, nonNativeFiles);
        collectNonNativeFiles< LEFT_SIDE>(selectionRight, tempFileBuf_, nonNativeFiles);
    }

    //##################### create temporary files for non-native paths ######################
    if (!nonNativeFiles.empty())
    {
        FocusPreserver fp;

        disableAllElements(true); //StatusHandlerTemporaryPanel will internally process Window messages, so avoid unexpected callbacks!
        auto app = wxTheApp; //fix lambda/wxWigets/VC fuck up
        ZEN_ON_SCOPE_EXIT(app->Yield(); enableAllElements()); //ui update before enabling buttons again: prevent strange behaviour of delayed button clicks

        try
        {
            StatusHandlerTemporaryPanel statusHandler(*this); //throw AbortProcess

            tempFileBuf_.createTempFiles(nonNativeFiles, statusHandler);
            //"clearSelection" not needed/desired
        }
        catch (AbortProcess&) { return; }

        //updateGui(); -> not needed
    }
    //########################################################################################

    const Zstring cmdExpanded = expandMacros(commandLinePhrase);

    try
    {
        invokeCommandLine< LEFT_SIDE>(cmdExpanded, selectionLeft,  tempFileBuf_); //throw FileError
        invokeCommandLine<RIGHT_SIDE>(cmdExpanded, selectionRight, tempFileBuf_); //
    }
    catch (const FileError& e) { showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString())); }
}


void MainDialog::setStatusBarFileStatistics(size_t filesOnLeftView,
                                            size_t foldersOnLeftView,
                                            size_t filesOnRightView,
                                            size_t foldersOnRightView,
                                            uint64_t filesizeLeftView,
                                            uint64_t filesizeRightView)
{

    //select state
    bSizerFileStatus->Show(true);
    m_staticTextFullStatus->Hide();

    //update status information
    bSizerStatusLeftDirectories->Show(foldersOnLeftView > 0);
    bSizerStatusLeftFiles      ->Show(filesOnLeftView   > 0);

    setText(*m_staticTextStatusLeftDirs,  _P("1 directory", "%x directories", foldersOnLeftView));
    setText(*m_staticTextStatusLeftFiles, _P("1 file", "%x files", filesOnLeftView));
    setText(*m_staticTextStatusLeftBytes, L"(" + formatFilesizeShort(filesizeLeftView) + L")");
    //------------------------------------------------------------------------------
    bSizerStatusRightDirectories->Show(foldersOnRightView > 0);
    bSizerStatusRightFiles      ->Show(filesOnRightView   > 0);

    setText(*m_staticTextStatusRightDirs,  _P("1 directory", "%x directories", foldersOnRightView));
    setText(*m_staticTextStatusRightFiles, _P("1 file", "%x files", filesOnRightView));
    setText(*m_staticTextStatusRightBytes, L"(" + formatFilesizeShort(filesizeRightView) + L")");
    //------------------------------------------------------------------------------
    wxString statusCenterNew;
    if (filegrid::getDataView(*m_gridMainC).rowsTotal() > 0)
    {
        statusCenterNew = _P("Showing %y of 1 row", "Showing %y of %x rows", filegrid::getDataView(*m_gridMainC).rowsTotal());
        replace(statusCenterNew, L"%y", formatNumber(filegrid::getDataView(*m_gridMainC).rowsOnView())); //%x is already used as plural form placeholder!
    }

    //fill middle text (considering flashStatusInformation())
    if (oldStatusMsgs_.empty())
        setText(*m_staticTextStatusCenter, statusCenterNew);
    else
        oldStatusMsgs_.front() = statusCenterNew;

    m_panelStatusBar->Layout();
}


//void MainDialog::setStatusBarFullText(const wxString& msg)
//{
//    const bool needLayoutUpdate = !m_staticTextFullStatus->IsShown();
//    //select state
//    bSizerFileStatus->Show(false);
//    m_staticTextFullStatus->Show();
//
//    //update status information
//    setText(*m_staticTextFullStatus, msg);
//    m_panelStatusBar->Layout();
//
//    if (needLayoutUpdate)
//        auiMgr.Update(); //fix status bar height (needed on OS X)
//}


void MainDialog::flashStatusInformation(const wxString& text)
{
    oldStatusMsgs_.push_back(m_staticTextStatusCenter->GetLabel());

    m_staticTextStatusCenter->SetLabel(text);
    m_staticTextStatusCenter->SetForegroundColour(wxColor(31, 57, 226)); //highlight_ color: blue
    m_staticTextStatusCenter->SetFont(m_staticTextStatusCenter->GetFont().Bold());

    m_panelStatusBar->Layout();
    //if (needLayoutUpdate) auiMgr.Update(); -> not needed here, this is called anyway in updateGui()

    guiQueue_.processAsync([] { std::this_thread::sleep_for(std::chrono::milliseconds(2500)); },
                           [this] { this->restoreStatusInformation(); });
}


void MainDialog::restoreStatusInformation()
{
    if (!oldStatusMsgs_.empty())
    {
        wxString oldMsg = oldStatusMsgs_.back();
        oldStatusMsgs_.pop_back();

        if (oldStatusMsgs_.empty()) //restore original status text
        {
            m_staticTextStatusCenter->SetLabel(oldMsg);
            m_staticTextStatusCenter->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)); //reset color

            wxFont fnt = m_staticTextStatusCenter->GetFont();
            fnt.SetWeight(wxFONTWEIGHT_NORMAL);
            m_staticTextStatusCenter->SetFont(fnt);

            m_panelStatusBar->Layout();
        }
    }
}


void MainDialog::disableAllElements(bool enableAbort)
{
    //disables all elements (except abort button) that might receive user input during long-running processes:
    //when changing consider: comparison, synchronization, manual deletion

    //OS X: wxWidgets portability promise is again a mess: http://wxwidgets.10942.n7.nabble.com/Disable-panel-and-appropriate-children-windows-linux-macos-td35357.html

    EnableCloseButton(false); //closing main dialog is not allowed during synchronization! crash!
    //EnableCloseButton(false) just does not work reliably!
    //- Windows: dialog can still be closed by clicking the task bar preview window with the middle mouse button or by pressing ALT+F4!
    //- OS X: Quit/Preferences menu items still enabled during sync,
    //       ([[m_macWindow standardWindowButton:NSWindowCloseButton] setEnabled:enable]) does not stick after calling Maximize() ([m_macWindow zoom:nil])
    //- Linux: it just works! :)
    allowMainDialogClose_ = false;

    localKeyEventsEnabled_ = false;

    for (size_t pos = 0; pos < m_menubar1->GetMenuCount(); ++pos)
        m_menubar1->EnableTop(pos, false);
    m_bpButtonCmpConfig  ->Disable();
    m_bpButtonFilter     ->Disable();
    m_bpButtonSyncConfig ->Disable();
    m_buttonSync         ->Disable();
    m_panelDirectoryPairs->Disable();
    m_splitterMain       ->Disable();
    m_gridMainL          ->Disable(); //disabled state already covered by m_splitterMain,
    m_gridMainC          ->Disable(); //however grid.cpp used IsThisEnabled() for rendering!
    m_gridMainR          ->Disable(); //
    m_panelViewFilter    ->Disable();
    m_panelConfig        ->Disable();
    m_gridOverview       ->Disable();
    m_gridCfgHistory     ->Disable();
    m_panelSearch        ->Disable();
    m_bpButtonCmpContext ->Disable();
    m_bpButtonSyncContext->Disable();
    m_bpButtonFilterContext->Disable();

    if (enableAbort)
    {
        //show abort button
        m_buttonCancel->Enable();
        m_buttonCancel->Show();
        //if (m_buttonCancel->IsShownOnScreen()) -> needed?
        m_buttonCancel->SetFocus();
        m_buttonCompare->Disable();
        m_buttonCompare->Hide();

        m_panelTopButtons->Layout();
    }
    else
        m_panelTopButtons->Disable();
}


void MainDialog::enableAllElements()
{
    //wxGTK, yet another QOI issue: some stupid bug, keeps moving main dialog to top!!

    EnableCloseButton(true);
    allowMainDialogClose_ = true;

    localKeyEventsEnabled_ = true;

    for (size_t pos = 0; pos < m_menubar1->GetMenuCount(); ++pos)
        m_menubar1->EnableTop(pos, true);
    m_bpButtonCmpConfig  ->Enable();
    m_bpButtonFilter     ->Enable();
    m_bpButtonSyncConfig ->Enable();
    m_buttonSync         ->Enable();
    m_panelDirectoryPairs->Enable();
    m_splitterMain       ->Enable();
    m_gridMainL          ->Enable();
    m_gridMainC          ->Enable();
    m_gridMainR          ->Enable();
    m_panelViewFilter    ->Enable();
    m_panelConfig        ->Enable();
    m_gridOverview       ->Enable();
    m_gridCfgHistory     ->Enable();
    m_panelSearch        ->Enable();
    m_bpButtonCmpContext ->Enable();
    m_bpButtonSyncContext->Enable();
    m_bpButtonFilterContext->Enable();

    //show compare button
    m_buttonCancel->Disable();
    m_buttonCancel->Hide();
    m_buttonCompare->Enable();
    m_buttonCompare->Show();

    m_panelTopButtons->Enable();
    m_panelTopButtons->Layout();

    //at least wxWidgets on OS X fails to do this after enabling:
    Refresh();
}


namespace
{
void updateSizerOrientation(wxBoxSizer& sizer, wxWindow& window, double horizontalWeight)
{
    const int newOrientation = window.GetSize().GetWidth() * horizontalWeight > window.GetSize().GetHeight() ? wxHORIZONTAL : wxVERTICAL; //check window NOT sizer width!
    if (sizer.GetOrientation() != newOrientation)
    {
        sizer.SetOrientation(newOrientation);
        window.Layout();
    }
}
}


void MainDialog::OnResizeTopButtonPanel(wxEvent& event)
{
    updateSizerOrientation(*bSizerTopButtons, *m_panelTopButtons, 0.5);
    event.Skip();
}


void MainDialog::OnResizeConfigPanel(wxEvent& event)
{
    updateSizerOrientation(*bSizerConfig, *m_panelConfig, 0.5);
    event.Skip();
}


void MainDialog::OnResizeViewPanel(wxEvent& event)
{
    //we need something more fancy for the statistics:
    const int parentOrient = m_panelViewFilter->GetSize().GetWidth() > m_panelViewFilter->GetSize().GetHeight() ? wxHORIZONTAL : wxVERTICAL; //check window NOT sizer width!
    if (bSizerViewFilter->GetOrientation() != parentOrient)
    {
        //apply opposite orientation for child sizers
        const int childOrient = parentOrient == wxHORIZONTAL ? wxVERTICAL : wxHORIZONTAL;
        wxSizerItemList& sl = bSizerStatistics->GetChildren();
        for (auto it = sl.begin(); it != sl.end(); ++it) //yet another wxWidgets bug keeps us from using std::for_each
        {
            wxSizerItem& szItem = **it;
            if (auto sizerChild = dynamic_cast<wxBoxSizer*>(szItem.GetSizer()))
                if (sizerChild->GetOrientation() != childOrient)
                    sizerChild->SetOrientation(childOrient);
        }

        bSizerStatistics->SetOrientation(parentOrient);
        bSizerViewFilter->SetOrientation(parentOrient);
        m_panelViewFilter->Layout();
        m_panelStatistics->Layout();
    }

    event.Skip();
}


void MainDialog::OnResizeLeftFolderWidth(wxEvent& event)
{
    //adapt left-shift display distortion caused by scrollbars for multiple folder pairs
    const int width = m_panelTopLeft->GetSize().GetWidth();
    for (FolderPairPanel* panel : additionalFolderPairs_)
        panel->m_panelLeft->SetMinSize(wxSize(width, -1));

    event.Skip();
}


void MainDialog::onTreeButtonEvent(wxKeyEvent& event)
{
    int keyCode = event.GetKeyCode();
    if (m_gridOverview->GetLayoutDirection() == wxLayout_RightToLeft)
    {
        if (keyCode == WXK_LEFT || keyCode == WXK_NUMPAD_LEFT)
            keyCode = WXK_RIGHT;
        else if (keyCode == WXK_RIGHT || keyCode == WXK_NUMPAD_RIGHT)
            keyCode = WXK_LEFT;
    }

    if (event.ControlDown())
        switch (keyCode)
        {
            case 'C':
            case WXK_INSERT: //CTRL + C || CTRL + INS
                copySelectionToClipboard({ m_gridOverview });
                return;
        }
    else if (event.AltDown())
        switch (keyCode)
        {
            case WXK_NUMPAD_LEFT:
            case WXK_LEFT: //ALT + <-
                setSyncDirManually(getTreeSelection(), SyncDirection::LEFT);
                return;

            case WXK_NUMPAD_RIGHT:
            case WXK_RIGHT: //ALT + ->
                setSyncDirManually(getTreeSelection(), SyncDirection::RIGHT);
                return;

            case WXK_NUMPAD_UP:
            case WXK_NUMPAD_DOWN:
            case WXK_UP:   /* ALT + /|\   */
            case WXK_DOWN: /* ALT + \|/   */
                setSyncDirManually(getTreeSelection(), SyncDirection::NONE);
                return;
        }

    else
        switch (keyCode)
        {
            case WXK_SPACE:
            case WXK_NUMPAD_SPACE:
            {
                const std::vector<FileSystemObject*>& selection = getTreeSelection();
                if (!selection.empty())
                    setFilterManually(selection, m_bpButtonShowExcluded->isActive() && !selection[0]->isActive());
                //always exclude items if "m_bpButtonShowExcluded is unchecked" => yes, it's possible to have already unchecked items in selection, so we need to overwrite:
                //e.g. select root node while the first item returned is not shown on grid!
            }
            return;

            case WXK_DELETE:
            case WXK_NUMPAD_DELETE:
                deleteSelectedFiles(getTreeSelection(), getTreeSelection(), !event.ShiftDown() /*moveToRecycler*/);
                return;
        }

    event.Skip(); //unknown keypress: propagate
}


void MainDialog::onGridButtonEvent(wxKeyEvent& event, Grid& grid, bool leftSide)
{
    const std::vector<FileSystemObject*> selection      = getGridSelection(); //referenced by lambdas!
    const std::vector<FileSystemObject*> selectionLeft  = getGridSelection(true, false);
    const std::vector<FileSystemObject*> selectionRight = getGridSelection(false, true);

    int keyCode = event.GetKeyCode();
    if (grid.GetLayoutDirection() == wxLayout_RightToLeft)
    {
        if (keyCode == WXK_LEFT || keyCode == WXK_NUMPAD_LEFT)
            keyCode = WXK_RIGHT;
        else if (keyCode == WXK_RIGHT || keyCode == WXK_NUMPAD_RIGHT)
            keyCode = WXK_LEFT;
    }

    if (event.ControlDown())
        switch (keyCode)
        {
            case 'C':
            case WXK_INSERT: //CTRL + C || CTRL + INS
                copySelectionToClipboard({ m_gridMainL, m_gridMainR} );
                return; // -> swallow event! don't allow default grid commands!

            case 'T': //CTRL + T
                copyToAlternateFolder(selectionLeft, selectionRight);
                return;
        }

    else if (event.AltDown())
        switch (keyCode)
        {
            case WXK_NUMPAD_LEFT:
            case WXK_LEFT: //ALT + <-
                setSyncDirManually(selection, SyncDirection::LEFT);
                return;

            case WXK_NUMPAD_RIGHT:
            case WXK_RIGHT: //ALT + ->
                setSyncDirManually(selection, SyncDirection::RIGHT);
                return;

            case WXK_NUMPAD_UP:
            case WXK_NUMPAD_DOWN:
            case WXK_UP:   /* ALT + /|\   */
            case WXK_DOWN: /* ALT + \|/   */
                setSyncDirManually(selection, SyncDirection::NONE);
                return;
        }

    else
    {
        //0 ... 9
        const size_t extAppPos = [&]() -> size_t
        {
            if ('0' <= keyCode && keyCode <= '9')
                return keyCode - '0';
            if (WXK_NUMPAD0 <= keyCode && keyCode <= WXK_NUMPAD9)
                return keyCode - WXK_NUMPAD0;
            if (keyCode == WXK_RETURN || keyCode == WXK_NUMPAD_ENTER) //open with first external application
                return 0;
            return static_cast<size_t>(-1);
        }();

        if (extAppPos < globalCfg_.gui.externalApps.size())
        {
            openExternalApplication(globalCfg_.gui.externalApps[extAppPos].cmdLine, leftSide, selectionLeft, selectionRight);
            return;
        }

        switch (keyCode)
        {
            case WXK_DELETE:
            case WXK_NUMPAD_DELETE:
                deleteSelectedFiles(selectionLeft, selectionRight, !event.ShiftDown() /*moveToRecycler*/);
                return;

            case WXK_SPACE:
            case WXK_NUMPAD_SPACE:
                if (!selection.empty())
                    setFilterManually(selection, m_bpButtonShowExcluded->isActive() && !selection[0]->isActive());
                return;
        }
    }

    event.Skip(); //unknown keypress: propagate
}


void MainDialog::onLocalKeyEvent(wxKeyEvent& event) //process key events without explicit menu entry :)
{
    if (!localKeyEventsEnabled_)
    {
        event.Skip();
        return;
    }
    localKeyEventsEnabled_ = false; //avoid recursion
    ZEN_ON_SCOPE_EXIT(localKeyEventsEnabled_ = true);


    const int keyCode = event.GetKeyCode();

    //CTRL + X
    //if (event.ControlDown())
    //    switch (keyCode)
    //    {
    //        case 'F': //CTRL + F
    //            showFindPanel();
    //            return; //-> swallow event!
    //    }

    switch (keyCode)
    {
        case WXK_F3:
        case WXK_NUMPAD_F3:
            startFindNext(!event.ShiftDown() /*searchAscending*/);
            return; //-> swallow event!

        //case WXK_F6:
        //{
        //    wxCommandEvent dummy2(wxEVT_COMMAND_BUTTON_CLICKED); //simulate button click
        //    m_bpButtonCmpConfig->Command(dummy2); //simulate click
        //}
        //return; //-> swallow event!

        //case WXK_F7:
        //{
        //    wxCommandEvent dummy2(wxEVT_COMMAND_BUTTON_CLICKED); //simulate button click
        //    m_bpButtonFilter->Command(dummy2); //simulate click
        //}
        //return; //-> swallow event!

        //case WXK_F8:
        //{
        //    wxCommandEvent dummy2(wxEVT_COMMAND_BUTTON_CLICKED); //simulate button click
        //    m_bpButtonSyncConfig->Command(dummy2); //simulate click
        //}
        //return; //-> swallow event!

        case WXK_F10:
            setViewTypeSyncAction(!m_bpButtonViewTypeSyncAction->isActive());
            return; //-> swallow event!

        //redirect certain (unhandled) keys directly to grid!
        case WXK_UP:
        case WXK_DOWN:
        case WXK_LEFT:
        case WXK_RIGHT:
        case WXK_PAGEUP:
        case WXK_PAGEDOWN:
        case WXK_HOME:
        case WXK_END:

        case WXK_NUMPAD_UP:
        case WXK_NUMPAD_DOWN:
        case WXK_NUMPAD_LEFT:
        case WXK_NUMPAD_RIGHT:
        case WXK_NUMPAD_PAGEUP:
        case WXK_NUMPAD_PAGEDOWN:
        case WXK_NUMPAD_HOME:
        case WXK_NUMPAD_END:
        {
            const wxWindow* focus = wxWindow::FindFocus();
            if (!isComponentOf(focus, m_gridMainL     ) && //
                !isComponentOf(focus, m_gridMainC     ) && //don't propagate keyboard commands if grid is already in focus
                !isComponentOf(focus, m_gridMainR     ) && //
                !isComponentOf(focus, m_gridOverview  ) &&
                !isComponentOf(focus, m_gridCfgHistory) && //don't propagate if selecting config
                !isComponentOf(focus, m_panelSearch   ) &&
                !isComponentOf(focus, m_panelTopLeft  ) && //don't propagate if changing directory fields
                !isComponentOf(focus, m_panelTopCenter) &&
                !isComponentOf(focus, m_panelTopRight ) &&
                !isComponentOf(focus, m_scrolledWindowFolderPairs) &&
                m_gridMainL->IsEnabled())
                if (wxEvtHandler* evtHandler = m_gridMainL->getMainWin().GetEventHandler())
                {
                    m_gridMainL->SetFocus();

                    event.SetEventType(wxEVT_KEY_DOWN); //the grid event handler doesn't expect wxEVT_CHAR_HOOK!
                    evtHandler->ProcessEvent(event); //propagating event to child lead to recursion with old key_event.h handling => still an issue?
                    event.Skip(false); //definitively handled now!
                    return;
                }
        }
        break;
    }

    event.Skip();
}


void MainDialog::onTreeGridSelection(GridSelectEvent& event)
{
    //scroll m_gridMain to user's new selection on m_gridOverview
    ptrdiff_t leadRow = -1;
    if (event.positive_ && event.rowFirst_ != event.rowLast_)
        if (std::unique_ptr<TreeView::Node> node = treegrid::getDataView(*m_gridOverview).getLine(event.rowFirst_))
        {
            if (const TreeView::RootNode* root = dynamic_cast<const TreeView::RootNode*>(node.get()))
                leadRow = filegrid::getDataView(*m_gridMainC).findRowFirstChild(&(root->baseFolder_));
            else if (const TreeView::DirNode* dir = dynamic_cast<const TreeView::DirNode*>(node.get()))
            {
                leadRow = filegrid::getDataView(*m_gridMainC).findRowDirect(&(dir->folder_));
                if (leadRow < 0) //directory was filtered out! still on tree view (but NOT on grid view)
                    leadRow = filegrid::getDataView(*m_gridMainC).findRowFirstChild(&(dir->folder_));
            }
            else if (const TreeView::FilesNode* files = dynamic_cast<const TreeView::FilesNode*>(node.get()))
            {
                assert(!files->filesAndLinks_.empty());
                if (!files->filesAndLinks_.empty())
                    leadRow = filegrid::getDataView(*m_gridMainC).findRowDirect(files->filesAndLinks_[0]->getId());
            }
        }

    if (leadRow >= 0)
    {
        leadRow = std::max<ptrdiff_t>(0, leadRow - 1); //scroll one more row

        m_gridMainL->scrollTo(leadRow); //scroll all of them (includes the "scroll master")
        m_gridMainC->scrollTo(leadRow); //
        m_gridMainR->scrollTo(leadRow); //

        m_gridOverview->getMainWin().Update(); //draw cursor immediately rather than on next idle event (required for slow CPUs, netbook)
    }

    //get selection on overview panel and set corresponding markers on main grid
    std::unordered_set<const FileSystemObject*> markedFilesAndLinks; //mark files/symlinks directly
    std::unordered_set<const ContainerObject*> markedContainer;      //mark full container including child-objects

    for (size_t row : m_gridOverview->getSelectedRows())
        if (std::unique_ptr<TreeView::Node> node = treegrid::getDataView(*m_gridOverview).getLine(row))
        {
            if (const TreeView::RootNode* root = dynamic_cast<const TreeView::RootNode*>(node.get()))
                markedContainer.insert(&(root->baseFolder_));
            else if (const TreeView::DirNode* dir = dynamic_cast<const TreeView::DirNode*>(node.get()))
                markedContainer.insert(&(dir->folder_));
            else if (const TreeView::FilesNode* files = dynamic_cast<const TreeView::FilesNode*>(node.get()))
                markedFilesAndLinks.insert(files->filesAndLinks_.begin(), files->filesAndLinks_.end());
        }

    filegrid::setNavigationMarker(*m_gridMainL, std::move(markedFilesAndLinks), std::move(markedContainer));

    event.Skip();
}


void MainDialog::onTreeGridContext(GridClickEvent& event)
{
    const auto& selection = getTreeSelection(); //referenced by lambdas!
    ContextMenu menu;

    //----------------------------------------------------------------------------------------------------
    if (!selection.empty())
        //std::any_of(selection.begin(), selection.end(), [](const FileSystemObject* fsObj) { return fsObj->getSyncOperation() != SO_EQUAL; })) -> doesn't consider directories
    {
        auto getImage = [&](SyncDirection dir, SyncOperation soDefault)
        {
            return mirrorIfRtl(getSyncOpImage(selection[0]->getSyncOperation() != SO_EQUAL ?
                                              selection[0]->testSyncOperation(dir) : soDefault));
        };
        const wxBitmap opRight = getImage(SyncDirection::RIGHT, SO_OVERWRITE_RIGHT);
        const wxBitmap opNone  = getImage(SyncDirection::NONE,  SO_DO_NOTHING     );
        const wxBitmap opLeft  = getImage(SyncDirection::LEFT,  SO_OVERWRITE_LEFT );

        wxString shortCutLeft  = L"\tAlt+Left";
        wxString shortCutRight = L"\tAlt+Right";
        if (wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft)
            std::swap(shortCutLeft, shortCutRight);

        menu.addItem(_("Set direction:") + L" ->" + shortCutRight, [this, &selection] { setSyncDirManually(selection, SyncDirection::RIGHT); }, &opRight);
        menu.addItem(_("Set direction:") + L" -" L"\tAlt+Down",    [this, &selection] { setSyncDirManually(selection, SyncDirection::NONE);  }, &opNone);
        menu.addItem(_("Set direction:") + L" <-" + shortCutLeft,  [this, &selection] { setSyncDirManually(selection, SyncDirection::LEFT);  }, &opLeft);
        //Gtk needs a direction, "<-", because it has no context menu icons!
        //Gtk requires "no spaces" for shortcut identifiers!
        menu.addSeparator();
    }

    //----------------------------------------------------------------------------------------------------

    auto addFilterMenu = [&](const std::wstring& label, const wxString& iconName, bool include)
    {
        if (selection.size() == 1)
        {
            ContextMenu submenu;

            const bool isFolder = dynamic_cast<const FolderPair*>(selection[0]) != nullptr;

            //by short name
            Zstring labelShort = Zstring(Zstr("*")) + FILE_NAME_SEPARATOR + selection[0]->getPairItemName();
            if (isFolder)
                labelShort += FILE_NAME_SEPARATOR;
            submenu.addItem(utfTo<wxString>(labelShort), [this, &selection, include] { filterShortname(*selection[0], include); });

            //by relative path
            Zstring labelRel = FILE_NAME_SEPARATOR + selection[0]->getPairRelativePath();
            if (isFolder)
                labelRel += FILE_NAME_SEPARATOR;
            submenu.addItem(utfTo<wxString>(labelRel), [this, &selection, include] { filterItems(selection, include); });

            menu.addSubmenu(label, submenu, &getResourceImage(iconName));
        }
        else if (selection.size() > 1)
        {
            //by relative path
            menu.addItem(label + L" <" + _("multiple selection") + L">",
                         [this, &selection, include] { filterItems(selection, include); }, &getResourceImage(iconName));
        }
    };
    addFilterMenu(_("Include via filter:"), L"filter_include_small", true);
    addFilterMenu(_("Exclude via filter:"), L"filter_exclude_small", false);

    //----------------------------------------------------------------------------------------------------
    if (!selection.empty())
    {
        if (m_bpButtonShowExcluded->isActive() && !selection[0]->isActive())
            menu.addItem(_("Include temporarily") + L"\tSpace", [this, &selection] { setFilterManually(selection, true); }, &getResourceImage(L"checkbox_true"));
        else
            menu.addItem(_("Exclude temporarily") + L"\tSpace", [this, &selection] { setFilterManually(selection, false); }, &getResourceImage(L"checkbox_false"));
    }
    else
        menu.addItem(_("Exclude temporarily") + L"\tSpace", [] {}, nullptr, false);

    //----------------------------------------------------------------------------------------------------
    const bool haveNonEmptyItems = std::any_of(selection.begin(), selection.end(), [&](const FileSystemObject* fsObj) { return !fsObj->isEmpty<LEFT_SIDE>() || !fsObj->isEmpty<RIGHT_SIDE>(); });

    //menu.addSeparator();

    //menu.addItem(_("&Copy to...") + L"\tCtrl+T", [&] { copyToAlternateFolder(selection, selection); }, nullptr, haveNonEmptyItems);

    //----------------------------------------------------------------------------------------------------

    menu.addSeparator();

    menu.addItem(_("&Delete") + L"\t(Shift+)Del", [&] { deleteSelectedFiles(selection, selection, true /*moveToRecycler*/); }, nullptr, haveNonEmptyItems);

    menu.popup(*this);
}


void MainDialog::onMainGridContextC(GridClickEvent& event)
{
    ContextMenu menu;

    menu.addItem(_("Include all"), [&]
    {
        setActiveStatus(true, folderCmp_);
        updateGui();
    }, nullptr, filegrid::getDataView(*m_gridMainC).rowsTotal() > 0);

    menu.addItem(_("Exclude all"), [&]
    {
        setActiveStatus(false, folderCmp_);
        updateGuiDelayedIf(!m_bpButtonShowExcluded->isActive()); //show update GUI before removing rows
    }, nullptr, filegrid::getDataView(*m_gridMainC).rowsTotal() > 0);

    menu.popup(*this);
}


void MainDialog::onMainGridContextL(GridClickEvent& event)
{
    onMainGridContextRim(true);
}


void MainDialog::onMainGridContextR(GridClickEvent& event)
{
    onMainGridContextRim(false);
}


void MainDialog::onMainGridContextRim(bool leftSide)
{
    const std::vector<FileSystemObject*> selection      = getGridSelection(); //referenced by lambdas!
    const std::vector<FileSystemObject*> selectionLeft  = getGridSelection(true, false);
    const std::vector<FileSystemObject*> selectionRight = getGridSelection(false, true);

    ContextMenu menu;

    if (!selection.empty())
    {
        auto getImage = [&](SyncDirection dir, SyncOperation soDefault)
        {
            return mirrorIfRtl(getSyncOpImage(selection[0]->getSyncOperation() != SO_EQUAL ?
                                              selection[0]->testSyncOperation(dir) : soDefault));
        };
        const wxBitmap opRight = getImage(SyncDirection::RIGHT, SO_OVERWRITE_RIGHT);
        const wxBitmap opNone  = getImage(SyncDirection::NONE,  SO_DO_NOTHING     );
        const wxBitmap opLeft  = getImage(SyncDirection::LEFT,  SO_OVERWRITE_LEFT );

        wxString shortCutLeft  = L"\tAlt+Left";
        wxString shortCutRight = L"\tAlt+Right";
        if (wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft)
            std::swap(shortCutLeft, shortCutRight);

        menu.addItem(_("Set direction:") + L" ->" + shortCutRight, [this, &selection] { setSyncDirManually(selection, SyncDirection::RIGHT); }, &opRight);
        menu.addItem(_("Set direction:") + L" -" L"\tAlt+Down",    [this, &selection] { setSyncDirManually(selection, SyncDirection::NONE);  }, &opNone);
        menu.addItem(_("Set direction:") + L" <-" + shortCutLeft,  [this, &selection] { setSyncDirManually(selection, SyncDirection::LEFT);  }, &opLeft);
        //Gtk needs a direction, "<-", because it has no context menu icons!
        //Gtk requires "no spaces" for shortcut identifiers!
        menu.addSeparator();
    }

    //----------------------------------------------------------------------------------------------------

    auto addFilterMenu = [&](const wxString& label, const wxString& iconName, bool include)
    {
        if (selection.size() == 1)
        {
            ContextMenu submenu;

            const bool isFolder = dynamic_cast<const FolderPair*>(selection[0]) != nullptr;

            //by extension
            if (!isFolder)
            {
                const Zstring extension = getFileExtension(selection[0]->getPairItemName());
                if (!extension.empty())
                    submenu.addItem(L"*." + utfTo<wxString>(extension),
                                    [this, extension, include] { filterExtension(extension, include); });
            }

            //by short name
            Zstring labelShort = Zstring(Zstr("*")) + FILE_NAME_SEPARATOR + selection[0]->getPairItemName();
            if (isFolder)
                labelShort += FILE_NAME_SEPARATOR;
            submenu.addItem(utfTo<wxString>(labelShort), [this, &selection, include] { filterShortname(*selection[0], include); });

            //by relative path
            Zstring labelRel = FILE_NAME_SEPARATOR + selection[0]->getPairRelativePath();
            if (isFolder)
                labelRel += FILE_NAME_SEPARATOR;
            submenu.addItem(utfTo<wxString>(labelRel), [this, &selection, include] { filterItems(selection, include); });

            menu.addSubmenu(label, submenu, &getResourceImage(iconName));
        }
        else if (selection.size() > 1)
        {
            //by relative path
            menu.addItem(label + L" <" + _("multiple selection") + L">",
                         [this, &selection, include] { filterItems(selection, include); }, &getResourceImage(iconName));
        }
    };
    addFilterMenu(_("Include via filter:"), L"filter_include_small", true);
    addFilterMenu(_("Exclude via filter:"), L"filter_exclude_small", false);

    //----------------------------------------------------------------------------------------------------

    if (!selection.empty())
    {
        if (m_bpButtonShowExcluded->isActive() && !selection[0]->isActive())
            menu.addItem(_("Include temporarily") + L"\tSpace", [this, &selection] { setFilterManually(selection, true); }, &getResourceImage(L"checkbox_true"));
        else
            menu.addItem(_("Exclude temporarily") + L"\tSpace", [this, &selection] { setFilterManually(selection, false); }, &getResourceImage(L"checkbox_false"));
    }
    else
        menu.addItem(_("Exclude temporarily") + L"\tSpace", [] {}, nullptr, false);

    //----------------------------------------------------------------------------------------------------

    if (!globalCfg_.gui.externalApps.empty())
    {
        menu.addSeparator();

        for (auto it = globalCfg_.gui.externalApps.begin();
             it != globalCfg_.gui.externalApps.end();
             ++it)
        {
            //translate default external apps on the fly: 1. "open in explorer" 2. "start directly"
            wxString description = translate(it->description);
            if (description.empty())
                description = L" "; //wxWidgets doesn't like empty items

            auto openApp = [this, command = it->cmdLine, leftSide, &selectionLeft, &selectionRight] { openExternalApplication(command, leftSide, selectionLeft, selectionRight); };

            const size_t pos = it - globalCfg_.gui.externalApps.begin();

            if (pos == 0)
                description += L"\tEnter, D-Click";
            else if (pos < 9)
                description += L"\t" + numberTo<std::wstring>(pos);

            menu.addItem(description, openApp, nullptr, !selectionLeft.empty() || !selectionRight.empty());
        }
    }

    //----------------------------------------------------------------------------------------------------

    std::vector<FileSystemObject*> nonEmptySelectionLeft  = selectionLeft;
    std::vector<FileSystemObject*> nonEmptySelectionRight = selectionRight;
    erase_if(nonEmptySelectionLeft,  [](const FileSystemObject* fsObj) { return fsObj->isEmpty< LEFT_SIDE>(); });
    erase_if(nonEmptySelectionRight, [](const FileSystemObject* fsObj) { return fsObj->isEmpty<RIGHT_SIDE>(); });

    menu.addSeparator();

    menu.addItem(_("&Copy to...") + L"\tCtrl+T", [&] { copyToAlternateFolder(nonEmptySelectionLeft, nonEmptySelectionRight); }, nullptr,
                 !nonEmptySelectionLeft.empty() || !nonEmptySelectionRight.empty());

    //----------------------------------------------------------------------------------------------------

    menu.addSeparator();
    menu.addItem(_("&Delete") + L"\t(Shift+)Del", [&] { deleteSelectedFiles(nonEmptySelectionLeft, nonEmptySelectionRight, true /*moveToRecycler*/); }, nullptr,
                 !nonEmptySelectionLeft.empty() || !nonEmptySelectionRight.empty());

    menu.popup(*this);
}



void MainDialog::addFilterPhrase(const Zstring& phrase, bool include, bool requireNewLine)
{
    Zstring& filterString = [&]() -> Zstring&
    {
        if (include)
        {
            Zstring& includeFilter = currentCfg_.mainCfg.globalFilter.includeFilter;
            if (NameFilter::isNull(includeFilter, Zstring())) //fancy way of checking for "*" include
                includeFilter.clear();
            return includeFilter;
        }
        else
            return currentCfg_.mainCfg.globalFilter.excludeFilter;
    }();

    if (requireNewLine)
    {
        trim(filterString, false, true, [](Zchar c) { return c == FILTER_ITEM_SEPARATOR || c == Zstr('\n') || c == Zstr(' '); });
        if (!filterString.empty())
            filterString += Zstr("\n");
        filterString += phrase;
    }
    else
    {
        trim(filterString, false, true, [](Zchar c) { return c == Zstr('\n') || c == Zstr(' '); });

        if (filterString.empty())
            ;
        else if (endsWith(filterString, FILTER_ITEM_SEPARATOR))
            filterString += Zstr(" ");
        else
            filterString += Zstr("\n");

        filterString += phrase + Zstr(' ') + FILTER_ITEM_SEPARATOR; //append FILTER_ITEM_SEPARATOR to 'mark' that next extension exclude should write to same line
    }

    updateGlobalFilterButton();
    if (include)
        applyFilterConfig(); //user's temporary exclusions lost!
    else //do not fully apply filter, just exclude new items: preserve user's temporary exclusions
    {
        std::for_each(begin(folderCmp_), end(folderCmp_), [&](BaseFolderPair& baseFolder) { addHardFiltering(baseFolder, phrase); });
        updateGui();
    }
}


void MainDialog::filterExtension(const Zstring& extension, bool include)
{
    assert(!extension.empty());
    addFilterPhrase(Zstr("*.") + extension, include, false);
}


void MainDialog::filterShortname(const FileSystemObject& fsObj, bool include)
{
    Zstring phrase = Zstring(Zstr("*")) + FILE_NAME_SEPARATOR + fsObj.getPairItemName();
    const bool isFolder = dynamic_cast<const FolderPair*>(&fsObj) != nullptr;
    if (isFolder)
        phrase += FILE_NAME_SEPARATOR;

    addFilterPhrase(phrase, include, true);
}


void MainDialog::filterItems(const std::vector<FileSystemObject*>& selection, bool include)
{
    if (!selection.empty())
    {
        Zstring phrase;
        for (auto it = selection.begin(); it != selection.end(); ++it)
        {
            FileSystemObject* fsObj = *it;

            if (it != selection.begin())
                phrase += Zstr("\n");

            //#pragma warning(suppress: 6011) -> fsObj bound in this context!
            phrase += FILE_NAME_SEPARATOR + fsObj->getPairRelativePath();

            const bool isFolder = dynamic_cast<const FolderPair*>(fsObj) != nullptr;
            if (isFolder)
                phrase += FILE_NAME_SEPARATOR;
        }
        addFilterPhrase(phrase, include, true);
    }
}


void MainDialog::onGridLabelContextC(GridLabelClickEvent& event)
{
    ContextMenu menu;

    const bool actionView = m_bpButtonViewTypeSyncAction->isActive();
    menu.addRadio(_("Category") + (actionView  ? L"\tF10" : L""), [&] { setViewTypeSyncAction(false); }, !actionView);
    menu.addRadio(_("Action")   + (!actionView ? L"\tF10" : L""), [&] { setViewTypeSyncAction(true ); },  actionView);

    menu.popup(*this);
}


void MainDialog::onGridLabelContextL(GridLabelClickEvent& event)
{
    onGridLabelContextRim(*m_gridMainL, static_cast<ColumnTypeRim>(event.colType_), true /*left*/);
}


void MainDialog::onGridLabelContextR(GridLabelClickEvent& event)
{
    onGridLabelContextRim(*m_gridMainR, static_cast<ColumnTypeRim>(event.colType_), false /*left*/);
}


void MainDialog::onGridLabelContextRim(Grid& grid, ColumnTypeRim type, bool left)
{
    ContextMenu menu;
    //--------------------------------------------------------------------------------------------------------
    auto toggleColumn = [&](ColumnType ct)
    {
        auto colAttr = grid.getColumnConfig();

        Grid::ColAttributes* caItemPath = nullptr;
        Grid::ColAttributes* caToggle   = nullptr;

        for (Grid::ColAttributes& ca : colAttr)
            if (ca.type == static_cast<ColumnType>(ColumnTypeRim::ITEM_PATH))
                caItemPath = &ca;
            else if (ca.type == ct)
                caToggle = &ca;

        assert(caItemPath && caItemPath->stretch > 0 && caItemPath->visible);
        assert(caToggle   && caToggle  ->stretch == 0);

        if (caItemPath && caToggle)
        {
            caToggle->visible = !caToggle->visible;

            //take width of newly visible column from stretched item path column
            caItemPath->offset -= caToggle->visible ? caToggle->offset : -caToggle->offset;

            grid.setColumnConfig(colAttr);
        }
    };

    if (const GridData* prov = grid.getDataProvider())
        for (const Grid::ColAttributes& ca : grid.getColumnConfig())
            menu.addCheckBox(prov->getColumnLabel(ca.type), [ct = ca.type, toggleColumn] { toggleColumn(ct); },
                             ca.visible, ca.type != static_cast<ColumnType>(ColumnTypeRim::ITEM_PATH)); //do not allow user to hide this column!
    //----------------------------------------------------------------------------------------------
    menu.addSeparator();

    auto& itemPathFormat = left ? globalCfg_.gui.mainDlg.itemPathFormatLeftGrid : globalCfg_.gui.mainDlg.itemPathFormatRightGrid;

    auto setItemPathFormat = [&](ItemPathFormat fmt)
    {
        itemPathFormat = fmt;
        filegrid::setItemPathForm(grid, fmt);
    };
    auto addFormatEntry = [&](const wxString& label, ItemPathFormat fmt)
    {
        menu.addRadio(label, [fmt, &setItemPathFormat] { setItemPathFormat(fmt); }, itemPathFormat == fmt);
    };
    addFormatEntry(_("Full path"    ), ItemPathFormat::FULL_PATH);
    addFormatEntry(_("Relative path"), ItemPathFormat::RELATIVE_PATH);
    addFormatEntry(_("Item name"    ), ItemPathFormat::ITEM_NAME);

    //----------------------------------------------------------------------------------------------
    menu.addSeparator();

    auto setIconSize = [&](FileIconSize sz, bool showIcons)
    {
        globalCfg_.gui.mainDlg.iconSize  = sz;
        globalCfg_.gui.mainDlg.showIcons = showIcons;
        filegrid::setupIcons(*m_gridMainL, *m_gridMainC, *m_gridMainR, globalCfg_.gui.mainDlg.showIcons, convert(globalCfg_.gui.mainDlg.iconSize));
    };

    auto setDefault = [&]
    {
        const XmlGlobalSettings defaultCfg;

        grid.setColumnConfig(convertColAttributes(left ? defaultCfg.gui.mainDlg.columnAttribLeft : defaultCfg.gui.mainDlg.columnAttribRight, defaultCfg.gui.mainDlg.columnAttribLeft));

        setItemPathFormat(left ? defaultCfg.gui.mainDlg.itemPathFormatLeftGrid : defaultCfg.gui.mainDlg.itemPathFormatRightGrid);

        setIconSize(defaultCfg.gui.mainDlg.iconSize, defaultCfg.gui.mainDlg.showIcons);
    };
    menu.addItem(_("&Default"), setDefault); //'&' -> reuse text from "default" buttons elsewhere
    //----------------------------------------------------------------------------------------------
    menu.addSeparator();
    menu.addCheckBox(_("Show icons:"), [&] { setIconSize(globalCfg_.gui.mainDlg.iconSize, !globalCfg_.gui.mainDlg.showIcons); }, globalCfg_.gui.mainDlg.showIcons);

    auto addSizeEntry = [&](const wxString& label, FileIconSize sz)
    {
        menu.addRadio(label, [sz, &setIconSize] { setIconSize(sz, true /*showIcons*/); }, globalCfg_.gui.mainDlg.iconSize == sz, globalCfg_.gui.mainDlg.showIcons);
    };
    addSizeEntry(L"    " + _("Small" ), ICON_SIZE_SMALL );
    addSizeEntry(L"    " + _("Medium"), ICON_SIZE_MEDIUM);
    addSizeEntry(L"    " + _("Large" ), ICON_SIZE_LARGE );
    //----------------------------------------------------------------------------------------------
    //    if (type == ColumnTypeRim::DATE)
    {
        menu.addSeparator();

        auto selectTimeSpan = [&]
        {
            if (showSelectTimespanDlg(this, manualTimeSpanFrom_, manualTimeSpanTo_) == ReturnSmallDlg::BUTTON_OKAY)
            {
                applyTimeSpanFilter(folderCmp_, manualTimeSpanFrom_, manualTimeSpanTo_); //overwrite current active/inactive settings
                //updateGuiDelayedIf(!m_bpButtonShowExcluded->isActive()); //show update GUI before removing rows
                updateGui();
            }
        };
        menu.addItem(_("Select time span..."), selectTimeSpan);
    }
    //--------------------------------------------------------------------------------------------------------
    menu.popup(*this);
    //event.Skip();
}


void MainDialog::resetLayout()
{
    m_splitterMain->setSashOffset(0);
    auiMgr_.LoadPerspective(defaultPerspective_);
    updateGuiForFolderPair();
}


void MainDialog::onOpenMenuTools(wxMenuEvent& event)
{
    //each layout menu item is either shown and owned by m_menuTools OR detached from m_menuTools and owned by detachedMenuItems_:
    auto filterLayoutItems = [&](wxMenuItem* menuItem, wxWindow* panelWindow)
    {
        if (detachedMenuItems_.find(menuItem) == detachedMenuItems_.end())
            detachedMenuItems_.insert(m_menuTools->Remove(menuItem)); //pass ownership

        wxAuiPaneInfo& paneInfo = this->auiMgr_.GetPane(panelWindow);
        if (!paneInfo.IsShown())
        {
            detachedMenuItems_.erase(menuItem); //pass ownership
            m_menuTools->Append(menuItem);            //
        }
    };
    filterLayoutItems(m_menuItemShowMain,       m_panelTopButtons);
    filterLayoutItems(m_menuItemShowFolders,    m_panelDirectoryPairs);
    filterLayoutItems(m_menuItemShowViewFilter, m_panelViewFilter);
    filterLayoutItems(m_menuItemShowConfig,     m_panelConfig);
    filterLayoutItems(m_menuItemShowOverview,   m_gridOverview);

    event.Skip();
}


void MainDialog::OnContextSetLayout(wxMouseEvent& event)
{
    ContextMenu menu;

    menu.addItem(replaceCpy(_("&Reset layout"), L"&", L""), [&] { resetLayout(); }); //reuse translation from gui builder
    //----------------------------------------------------------------------------------------

    bool addedSeparator = false;

    const wxAuiPaneInfoArray& paneArray = auiMgr_.GetAllPanes();
    for (size_t i = 0; i < paneArray.size(); ++i)
    {
        wxAuiPaneInfo& paneInfo = paneArray[i];
        if (!paneInfo.IsShown() &&
            paneInfo.window != compareStatus_->getAsWindow() &&
            paneInfo.window != m_panelSearch)
        {
            if (!addedSeparator)
            {
                menu.addSeparator();
                addedSeparator = true;
            }

            menu.addItem(replaceCpy(_("Show \"%x\""), L"%x", paneInfo.caption), [this, &paneInfo]
            {
                paneInfo.Show();
                this->auiMgr_.Update();
            });
        }
    }

    menu.popup(*this);
}


void MainDialog::OnCompSettingsContext(wxEvent& event)
{
    ContextMenu menu;

    auto setVariant = [&](CompareVariant var)
    {
        currentCfg_.mainCfg.cmpConfig.compareVar = var;
        applyCompareConfig(true /*setDefaultViewType*/);
    };

    const CompareVariant activeCmpVar = getConfig().mainCfg.cmpConfig.compareVar;

    auto addVariantItem = [&](CompareVariant cmpVar, const wchar_t* iconName)
    {
        const wxBitmap& iconNormal = getResourceImage(iconName);
        const wxBitmap  iconGrey   = greyScale(iconNormal);
        menu.addItem(getVariantName(cmpVar), [&setVariant, cmpVar] { setVariant(cmpVar); }, activeCmpVar == cmpVar ? &iconNormal : &iconGrey);
    };
    addVariantItem(CompareVariant::TIME_SIZE, L"file-time-small");
    addVariantItem(CompareVariant::CONTENT,   L"file-content-small");
    addVariantItem(CompareVariant::SIZE,      L"file-size-small");

    //menu.addRadio(getVariantName(CompareVariant::TIME_SIZE), [&] { setVariant(CompareVariant::TIME_SIZE); }, activeCmpVar == CompareVariant::TIME_SIZE);
    //menu.addRadio(getVariantName(CompareVariant::CONTENT  ), [&] { setVariant(CompareVariant::CONTENT);   }, activeCmpVar == CompareVariant::CONTENT);
    //menu.addRadio(getVariantName(CompareVariant::SIZE     ), [&] { setVariant(CompareVariant::SIZE);      }, activeCmpVar == CompareVariant::SIZE);

    wxPoint pos = m_bpButtonCmpContext->GetPosition();
    pos.x += m_bpButtonCmpContext->GetSize().GetWidth();
    menu.popup(*m_panelTopButtons, pos);
}


void MainDialog::OnSyncSettingsContext(wxEvent& event)
{
    ContextMenu menu;

    auto setVariant = [&](DirectionConfig::Variant var)
    {
        currentCfg_.mainCfg.syncCfg.directionCfg.var = var;
        applySyncConfig();
    };

    const auto currentVar = getConfig().mainCfg.syncCfg.directionCfg.var;

    menu.addRadio(getVariantName(DirectionConfig::TWO_WAY), [&] { setVariant(DirectionConfig::TWO_WAY); }, currentVar == DirectionConfig::TWO_WAY);
    menu.addRadio(getVariantName(DirectionConfig::MIRROR),  [&] { setVariant(DirectionConfig::MIRROR);  }, currentVar == DirectionConfig::MIRROR);
    menu.addRadio(getVariantName(DirectionConfig::UPDATE),  [&] { setVariant(DirectionConfig::UPDATE);  }, currentVar == DirectionConfig::UPDATE);
    menu.addRadio(getVariantName(DirectionConfig::CUSTOM),  [&] { setVariant(DirectionConfig::CUSTOM);  }, currentVar == DirectionConfig::CUSTOM);

    wxPoint pos = m_bpButtonSyncContext->GetPosition();
    pos.x += m_bpButtonSyncContext->GetSize().GetWidth();
    menu.popup(*m_panelTopButtons, pos);
}


void MainDialog::onDialogFilesDropped(FileDropEvent& event)
{
    assert(!event.getPaths().empty());
    loadConfiguration(event.getPaths());
    //event.Skip();
}


void MainDialog::onDirSelected(wxCommandEvent& event)
{
    //left and right directory text-control and dirpicker are synchronized by MainFolderDragDrop automatically
    clearGrid(); //disable the sync button
    event.Skip();
}


void MainDialog::onDirManualCorrection(wxCommandEvent& event)
{
    updateUnsavedCfgStatus();
    event.Skip();
}


void MainDialog::cfgHistoryRemoveObsolete(const std::vector<Zstring>& filePaths)
{
    auto getUnavailableCfgFilesAsync = [filePaths] //don't use wxString: NOT thread-safe! (e.g. non-atomic ref-count)
    {
        std::list<std::future<bool>> availableFiles; //check existence of all config files in parallel!

        for (const Zstring& filePath : filePaths)
            availableFiles.push_back(runAsync([=] { return fileAvailable(filePath); }));

        //potentially slow network access => limit maximum wait time!
        wait_for_all_timed(availableFiles.begin(), availableFiles.end(), std::chrono::milliseconds(1000));

        std::vector<Zstring> pathsToRemove;

        auto itFut = availableFiles.begin();
        for (auto it = filePaths.begin(); it != filePaths.end(); ++it, ++itFut)
            if (isReady(*itFut) && !itFut->get()) //remove only files that are confirmed to be non-existent
                pathsToRemove.push_back(*it); //file access error? probably not accessible network share or usb stick => remove cfg

        return pathsToRemove;
    };

    guiQueue_.processAsync(getUnavailableCfgFilesAsync, [this](const std::vector<Zstring>& filePaths2)
    {
        cfggrid::getDataView(*m_gridCfgHistory).removeItems(filePaths2);
        m_gridCfgHistory->Refresh();
    });
}


void MainDialog::updateUnsavedCfgStatus()
{
    const Zstring activeCfgFilePath = activeConfigFiles_.size() == 1 && !equalFilePath(activeConfigFiles_[0], lastRunConfigPath_) ? activeConfigFiles_[0] : Zstring();

    const bool haveUnsavedCfg = lastSavedCfg_ != getConfig();

    //update save config button
    const bool allowSave = haveUnsavedCfg ||
                           activeConfigFiles_.size() > 1;

    auto makeBrightGrey = [](const wxBitmap& bmp) -> wxBitmap
    {
        wxImage img = bmp.ConvertToImage().ConvertToGreyscale(1.0/3, 1.0/3, 1.0/3); //treat all channels equally!
        brighten(img, 80);
        return img;
    };

    setImage(*m_bpButtonSave, allowSave ? getResourceImage(L"save") : makeBrightGrey(getResourceImage(L"save")));
    m_bpButtonSave->Enable(allowSave);
    m_menuItemSave->Enable(allowSave); //bitmap is automatically greyscaled on Win7 (introducing a crappy looking shift), but not on XP

    //set main dialog title
    wxString title;
    if (haveUnsavedCfg)
        title += L'*';

    if (!activeCfgFilePath.empty())
        title += utfTo<wxString>(activeCfgFilePath);
    else if (activeConfigFiles_.size() > 1)
    {
        title += extractJobName(activeConfigFiles_[0]);
        std::for_each(activeConfigFiles_.begin() + 1, activeConfigFiles_.end(), [&](const Zstring& filepath) { title += SPACED_DASH + extractJobName(filepath); });
    }
    else
        title += wxString(L"FreeFileSync ") + ffsVersion + SPACED_DASH + _("Folder Comparison and Synchronization");

    SetTitle(title);
}


void MainDialog::OnConfigSave(wxCommandEvent& event)
{
    const Zstring activeCfgFilePath = activeConfigFiles_.size() == 1 && !equalFilePath(activeConfigFiles_[0], lastRunConfigPath_) ? activeConfigFiles_[0] : Zstring();

    //if we work on a single named configuration document: save directly if changed
    //else: always show file dialog
    if (activeCfgFilePath.empty())
        trySaveConfig(nullptr);
    else
        try
        {
            switch (getXmlType(activeCfgFilePath)) //throw FileError
            {
                case XML_TYPE_GUI:
                    trySaveConfig(&activeCfgFilePath);
                    break;
                case XML_TYPE_BATCH:
                    trySaveBatchConfig(&activeCfgFilePath);
                    break;
                case XML_TYPE_GLOBAL:
                case XML_TYPE_OTHER:
                    showNotificationDialog(this, DialogInfoType::ERROR2,
                                           PopupDialogCfg().setDetailInstructions(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(activeCfgFilePath))));
                    break;
            }
        }
        catch (const FileError& e)
        {
            showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
        }
}


void MainDialog::OnConfigSaveAs(wxCommandEvent& event)
{
    trySaveConfig(nullptr);
}


void MainDialog::OnSaveAsBatchJob(wxCommandEvent& event)
{
    trySaveBatchConfig(nullptr);
}


bool MainDialog::trySaveConfig(const Zstring* guiFilename) //return true if saved successfully
{
    Zstring targetFilename;

    if (guiFilename)
    {
        targetFilename = *guiFilename;
        assert(endsWith(targetFilename, Zstr(".ffs_gui")));
    }
    else
    {
        Zstring defaultFilePath = activeConfigFiles_.size() == 1 && !equalFilePath(activeConfigFiles_[0], lastRunConfigPath_) ? activeConfigFiles_[0] : Zstr("SyncSettings.ffs_gui");
        //attention: activeConfigFiles may be an imported *.ffs_batch file! We don't want to overwrite it with a GUI config!
        if (endsWith(defaultFilePath, Zstr(".ffs_batch"), CmpFilePath()))
            defaultFilePath = beforeLast(defaultFilePath, Zstr("."), IF_MISSING_RETURN_NONE) + Zstr(".ffs_gui");

        wxFileDialog filePicker(this, //put modal dialog on stack: creating this on freestore leads to memleak!
                                wxString(),
                                //OS X really needs dir/file separated like this:
                                utfTo<wxString>(beforeLast(defaultFilePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE)), //default dir
                                utfTo<wxString>(afterLast (defaultFilePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL)), //default file
                                wxString(L"FreeFileSync (*.ffs_gui)|*.ffs_gui") + L"|" +_("All files") + L" (*.*)|*",
                                wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (filePicker.ShowModal() != wxID_OK)
            return false;
        targetFilename = utfTo<Zstring>(filePicker.GetPath());
    }

    const XmlGuiConfig guiCfg = getConfig();

    try
    {
        writeConfig(guiCfg, targetFilename); //throw FileError
        setLastUsedConfig(targetFilename, guiCfg);

        flashStatusInformation(_("Configuration saved"));
        return true;
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
        return false;
    }
}


bool MainDialog::trySaveBatchConfig(const Zstring* batchFileToUpdate)
{
    //essentially behave like trySaveConfig(): the collateral damage of not saving GUI-only settings "m_bpButtonViewTypeSyncAction" is negligible

    const Zstring activeCfgFilePath = activeConfigFiles_.size() == 1 && !equalFilePath(activeConfigFiles_[0], lastRunConfigPath_) ? activeConfigFiles_[0] : Zstring();

    //prepare batch config: reuse existing batch-specific settings from file if available
    BatchExclusiveConfig batchExCfg;
    try
    {
        Zstring referenceBatchFile;
        if (batchFileToUpdate)
            referenceBatchFile = *batchFileToUpdate;
        else if (!activeCfgFilePath.empty())
            if (getXmlType(activeCfgFilePath) == XML_TYPE_BATCH) //throw FileError
                referenceBatchFile = activeCfgFilePath;

        if (!referenceBatchFile.empty())
        {
            XmlBatchConfig referenceBatchCfg;

            std::wstring warningMsg;
            readConfig(referenceBatchFile, referenceBatchCfg, warningMsg); //throw FileError
            //=> ignore warnings altogether: user has seen them already when loading the config file!
            batchExCfg = referenceBatchCfg.batchExCfg;
        }
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
        return false;
    }

    Zstring targetFilename;
    if (batchFileToUpdate)
    {
        targetFilename = *batchFileToUpdate;
        assert(endsWith(targetFilename, Zstr(".ffs_batch")));
    }
    else
    {
        //let user update batch config: this should change batch-exclusive settings only, else the "setLastUsedConfig" below would be somewhat of a lie
        if (showBatchConfigDialog(this,
                                  batchExCfg,
                                  currentCfg_.mainCfg.ignoreErrors) != ReturnBatchConfig::BUTTON_SAVE_AS)
            return false;
        updateUnsavedCfgStatus(); //nothing else to update on GUI!

        Zstring defaultFilePath = !activeCfgFilePath.empty() ? activeCfgFilePath : Zstr("BatchRun.ffs_batch");
        //attention: activeConfigFiles may be a *.ffs_gui file! We don't want to overwrite it with a BATCH config!
        if (endsWith(defaultFilePath, Zstr(".ffs_gui"), CmpFilePath()))
            defaultFilePath = beforeLast(defaultFilePath, Zstr("."), IF_MISSING_RETURN_NONE) + Zstr(".ffs_batch");

        wxFileDialog filePicker(this, //put modal dialog on stack: creating this on freestore leads to memleak!
                                wxString(),
                                //OS X really needs dir/file separated like this:
                                utfTo<wxString>(beforeLast(defaultFilePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE)), //default dir
                                utfTo<wxString>(afterLast (defaultFilePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL)), //default file
                                _("FreeFileSync batch") + L" (*.ffs_batch)|*.ffs_batch" + L"|" +_("All files") + L" (*.*)|*",
                                wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (filePicker.ShowModal() != wxID_OK)
            return false;
        targetFilename = utfTo<Zstring>(filePicker.GetPath());
    }

    const XmlGuiConfig guiCfg = getConfig();
    const XmlBatchConfig batchCfg = convertGuiToBatch(guiCfg, batchExCfg);

    try
    {
        writeConfig(batchCfg, targetFilename); //throw FileError
        setLastUsedConfig(targetFilename, guiCfg); //[!] behave as if we had saved guiCfg

        flashStatusInformation(_("Configuration saved"));
        return true;
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
        return false;
    }
}


bool MainDialog::saveOldConfig() //return false on user abort
{
    if (lastSavedCfg_ != getConfig())
    {
        const Zstring activeCfgFilePath = activeConfigFiles_.size() == 1 && !equalFilePath(activeConfigFiles_[0], lastRunConfigPath_) ? activeConfigFiles_[0] : Zstring();

        //notify user about changed settings
        if (globalCfg_.confirmDlgs.popupOnConfigChange)
            if (!activeCfgFilePath.empty())
                //only if check is active and non-default config file loaded
            {
                bool neverSaveChanges = false;
                switch (showQuestionDialog(this, DialogInfoType::INFO, PopupDialogCfg().
                                           setTitle(utfTo<wxString>(activeCfgFilePath)).
                                           setMainInstructions(replaceCpy(_("Do you want to save changes to %x?"), L"%x",
                                                                          fmtPath(afterLast(activeCfgFilePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL)))).
                                           setCheckBox(neverSaveChanges, _("Never save &changes"), QuestionButton2::YES),
                                           _("&Save"), _("Do&n't save")))
                {
                    case QuestionButton2::YES: //save
                        try
                        {
                            switch (getXmlType(activeCfgFilePath)) //throw FileError
                            {
                                case XML_TYPE_GUI:
                                    return trySaveConfig(&activeCfgFilePath);
                                case XML_TYPE_BATCH:
                                    return trySaveBatchConfig(&activeCfgFilePath);
                                case XML_TYPE_GLOBAL:
                                case XML_TYPE_OTHER:
                                    showNotificationDialog(this, DialogInfoType::ERROR2,
                                                           PopupDialogCfg().setDetailInstructions(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(activeCfgFilePath))));
                                    return false;
                            }
                        }
                        catch (const FileError& e)
                        {
                            showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
                            return false;
                        }
                        break;

                    case QuestionButton2::NO: //don't save
                        globalCfg_.confirmDlgs.popupOnConfigChange = !neverSaveChanges;
                        break;

                    case QuestionButton2::CANCEL:
                        return false;
                }
            }

        //discard current reference file(s), this ensures next app start will load <last session> instead of the original non-modified config selection
        setLastUsedConfig(std::vector<Zstring>(), lastSavedCfg_);
        //this seems to make theoretical sense also: the job of this function is to make sure current (volatile) config and reference file name are in sync
        // => if user does not save cfg, it is not attached to a physical file names anymore!
    }
    return true;
}


void MainDialog::OnConfigLoad(wxCommandEvent& event)
{
    const Zstring activeCfgFilePath = activeConfigFiles_.size() == 1 && !equalFilePath(activeConfigFiles_[0], lastRunConfigPath_) ? activeConfigFiles_[0] : Zstring();

    wxFileDialog filePicker(this,
                            wxString(),
                            utfTo<wxString>(beforeLast(activeCfgFilePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE)), //default dir
                            wxString(), //default file
                            wxString(L"FreeFileSync (*.ffs_gui; *.ffs_batch)|*.ffs_gui;*.ffs_batch") + L"|" +_("All files") + L" (*.*)|*",
                            wxFD_OPEN | wxFD_MULTIPLE);
    if (filePicker.ShowModal() == wxID_OK)
    {
        wxArrayString tmp;
        filePicker.GetPaths(tmp);

        std::vector<Zstring> filePaths;
        for (const wxString& path : tmp)
            filePaths.push_back(utfTo<Zstring>(path));

        assert(!filePaths.empty());
        loadConfiguration(filePaths);
    }
}


void MainDialog::onCfgGridSelection(GridSelectEvent& event)
{
    if (event.mouseSelect_ && !event.mouseSelect_->complete)
        return; //skip the preliminary "clear range" event for mouse-down!
    //the mouse is still captured, so we don't want to show a modal dialog (e.g. save changes?) before mouse-up!
    //what if mouse capture is lost? minor glitch: grid selection is empty, but parameter owner is "activeConfigFiles_" in any case

    std::vector<Zstring> filePaths;
    for (size_t row : m_gridCfgHistory->getSelectedRows())
        if (const ConfigView::Details* cfg = cfggrid::getDataView(*m_gridCfgHistory).getItem(row))
            filePaths.push_back(cfg->filePath);
        else
            assert(false);

    if (!loadConfiguration(filePaths))
        //user changed m_gridCfgHistory selection so it's this method's responsibility to synchronize with activeConfigFiles:
        //- if user cancelled saving old config
        //- there's an error loading new config
        cfggrid::addAndSelect(*m_gridCfgHistory, activeConfigFiles_, false /*scrollToSelection*/);
}


void MainDialog::onCfgGridDoubleClick(GridClickEvent& event)
{
    if (!activeConfigFiles_.empty())
    {
        wxCommandEvent dummy(wxEVT_COMMAND_BUTTON_CLICKED);
        m_buttonCompare->Command(dummy); //simulate click
    }
}


void MainDialog::OnConfigNew(wxCommandEvent& event)
{
    loadConfiguration({});
}


bool MainDialog::loadConfiguration(const std::vector<Zstring>& filePaths)
{
    if (!saveOldConfig())
        return false; //cancelled by user

    XmlGuiConfig newGuiCfg; //contains default values

    //add default exclusion filter: this is only ever relevant when creating new configurations!
    //a default XmlGuiConfig does not need these user-specific exclusions!
    Zstring& excludeFilter = newGuiCfg.mainCfg.globalFilter.excludeFilter;
    if (!excludeFilter.empty() && !endsWith(excludeFilter, Zstr("\n")))
        excludeFilter += Zstr("\n");
    excludeFilter += globalCfg_.gui.defaultExclusionFilter;

    if (!filePaths.empty()) //empty cfg file list means "use default"
        try
        {
            //allow reading batch configurations also
            std::wstring warningMsg;
            readAnyConfig(filePaths, newGuiCfg, warningMsg); //throw FileError

            if (!warningMsg.empty())
            {
                showNotificationDialog(this, DialogInfoType::WARNING, PopupDialogCfg().setDetailInstructions(warningMsg));
                setConfig(newGuiCfg, filePaths);
                setLastUsedConfig(filePaths, XmlGuiConfig()); //simulate changed config due to parsing errors
                return true;
            }
        }
        catch (const FileError& e)
        {
            showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
            return false;
        }

    setConfig(newGuiCfg, filePaths);
    //flashStatusInformation("Configuration loaded"); -> irrelevant!?
    return true;
}


void MainDialog::deleteSelectedCfgHistoryItems()
{
    const std::vector<size_t> selectedRows = m_gridCfgHistory->getSelectedRows();
    if (!selectedRows.empty())
    {
        std::vector<Zstring> filePaths;
        for (size_t row : selectedRows)
            if (const ConfigView::Details* cfg = cfggrid::getDataView(*m_gridCfgHistory).getItem(row))
                filePaths.push_back(cfg->filePath);
            else
                assert(false);

        cfggrid::getDataView(*m_gridCfgHistory).removeItems(filePaths);
        m_gridCfgHistory->Refresh(); //grid size changed => clears selection!

        //set active selection on next element to allow "batch-deletion" by holding down DEL key
        if (m_gridCfgHistory->getRowCount() > 0)
        {
            size_t nextRow = selectedRows.front();
            if (nextRow >= m_gridCfgHistory->getRowCount())
                nextRow = m_gridCfgHistory->getRowCount() - 1;

            m_gridCfgHistory->selectRow(nextRow, GridEventPolicy::DENY_GRID_EVENT);
        }
    }
}


void MainDialog::onCfgGridKeyEvent(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();
    if (keyCode == WXK_DELETE ||
        keyCode == WXK_NUMPAD_DELETE)
    {
        deleteSelectedCfgHistoryItems();
        return; //"swallow" event
    }
    event.Skip();
}


void MainDialog::onCfgGridContext(GridClickEvent& event)
{
    ContextMenu menu;
    //--------------------------------------------------------------------------------------------------------
    const std::vector<size_t> selectedRows = m_gridCfgHistory->getSelectedRows();

    menu.addItem(_("Hide configuration") + L"\tDel", [this] { deleteSelectedCfgHistoryItems(); }, nullptr, !selectedRows.empty());
    //--------------------------------------------------------------------------------------------------------
    menu.popup(*this);
    //event.Skip();
}


void MainDialog::onCfgGridLabelContext(GridLabelClickEvent& event)
{
    ContextMenu menu;
    //--------------------------------------------------------------------------------------------------------
    auto toggleColumn = [&](ColumnType ct)
    {
        auto colAttr = m_gridCfgHistory->getColumnConfig();

        Grid::ColAttributes* caName   = nullptr;
        Grid::ColAttributes* caToggle = nullptr;

        for (Grid::ColAttributes& ca : colAttr)
            if (ca.type == static_cast<ColumnType>(ColumnTypeCfg::NAME))
                caName = &ca;
            else if (ca.type == ct)
                caToggle = &ca;

        assert(caName && caName->stretch > 0 && caName->visible);
        assert(caToggle && caToggle->stretch == 0);

        if (caName && caToggle)
        {
            caToggle->visible = !caToggle->visible;

            //take width of newly visible column from stretched folder name column
            caName->offset -= caToggle->visible ? caToggle->offset : -caToggle->offset;

            m_gridCfgHistory->setColumnConfig(colAttr);
        }
    };

    if (auto prov = m_gridCfgHistory->getDataProvider())
        for (const Grid::ColAttributes& ca : m_gridCfgHistory->getColumnConfig())
            menu.addCheckBox(prov->getColumnLabel(ca.type), [ct = ca.type, toggleColumn] { toggleColumn(ct); },
                             ca.visible, ca.type != static_cast<ColumnType>(ColumnTypeCfg::NAME)); //do not allow user to hide name column!
    else assert(false);
    //--------------------------------------------------------------------------------------------------------
    menu.addSeparator();

    auto setDefault = [&]
    {
        const XmlGlobalSettings defaultCfg;
        m_gridCfgHistory->setColumnConfig(convertColAttributes(defaultCfg.gui.mainDlg.cfgGridColumnAttribs, getCfgGridDefaultColAttribs()));
    };
    menu.addItem(_("&Default"), setDefault); //'&' -> reuse text from "default" buttons elsewhere
    //--------------------------------------------------------------------------------------------------------
    menu.addSeparator();

    auto setCfgHighlight = [&]
    {
        int cfgGridSyncOverdueDays = cfggrid::getSyncOverdueDays(*m_gridCfgHistory);

        if (showCfgHighlightDlg(this, cfgGridSyncOverdueDays) == ReturnSmallDlg::BUTTON_OKAY)
            cfggrid::setSyncOverdueDays(*m_gridCfgHistory, cfgGridSyncOverdueDays);
    };
    menu.addItem(_("Highlight..."), setCfgHighlight);
    //--------------------------------------------------------------------------------------------------------

    menu.popup(*m_gridCfgHistory);
    //event.Skip();
}


void MainDialog::onCfgGridLabelLeftClick(GridLabelClickEvent& event)
{
    const auto colType = static_cast<ColumnTypeCfg>(event.colType_);
    bool sortAscending = getDefaultSortDirection(colType);

    const auto sortInfo = cfggrid::getDataView(*m_gridCfgHistory).getSortDirection();
    if (sortInfo.first == colType)
        sortAscending = !sortInfo.second;

    cfggrid::getDataView(*m_gridCfgHistory).setSortDirection(colType, sortAscending);
    m_gridCfgHistory->Refresh();

    //re-apply selection:
    cfggrid::addAndSelect(*m_gridCfgHistory, activeConfigFiles_, false /*scrollToSelection*/);
}


void MainDialog::onCheckRows(CheckRowsEvent& event)
{
    std::vector<size_t> selectedRows;

    const size_t rowLast = std::min(event.rowLast_, filegrid::getDataView(*m_gridMainC).rowsOnView()); //consider dummy rows
    for (size_t i = event.rowFirst_; i < rowLast; ++i)
        selectedRows.push_back(i);

    if (!selectedRows.empty())
    {
        std::vector<FileSystemObject*> objects = filegrid::getDataView(*m_gridMainC).getAllFileRef(selectedRows);
        setFilterManually(objects, event.setIncluded_);
    }
}


void MainDialog::onSetSyncDirection(SyncDirectionEvent& event)
{
    std::vector<size_t> selectedRows;

    const size_t rowLast = std::min(event.rowLast_, filegrid::getDataView(*m_gridMainC).rowsOnView()); //consider dummy rows
    for (size_t i = event.rowFirst_; i < rowLast; ++i)
        selectedRows.push_back(i);

    if (!selectedRows.empty())
    {
        std::vector<FileSystemObject*> objects = filegrid::getDataView(*m_gridMainC).getAllFileRef(selectedRows);
        setSyncDirManually(objects, event.direction_);
    }
}


void MainDialog::setLastUsedConfig(const std::vector<Zstring>& cfgFilePaths,
                                   const XmlGuiConfig& guiConfig)
{
    activeConfigFiles_ = cfgFilePaths;
    lastSavedCfg_ = guiConfig;

    cfggrid::addAndSelect(*m_gridCfgHistory, activeConfigFiles_, true /*scrollToSelection*/); //put filepath on list of last used config files

    updateUnsavedCfgStatus();
}


void MainDialog::setConfig(const XmlGuiConfig& newGuiCfg, const std::vector<Zstring>& referenceFiles)
{
    currentCfg_ = newGuiCfg;

    //evaluate new settings...

    //(re-)set view filter buttons
    setViewFilterDefault();

    updateGlobalFilterButton();

    //set first folder pair
    firstFolderPair_->setValues(currentCfg_.mainCfg.firstPair);

    //folderHistoryLeft->addItem(currentCfg.mainCfg.firstPair.leftDirectory);
    //folderHistoryRight->addItem(currentCfg.mainCfg.firstPair.rightDirectory);

    setAddFolderPairs(currentCfg_.mainCfg.additionalPairs);

    setViewTypeSyncAction(currentCfg_.highlightSyncAction);

    clearGrid(); //+ update GUI!

    setLastUsedConfig(referenceFiles, newGuiCfg);
}


XmlGuiConfig MainDialog::getConfig() const
{
    XmlGuiConfig guiCfg = currentCfg_;

    //load settings whose ownership lies not in currentCfg:

    //first folder pair
    guiCfg.mainCfg.firstPair = firstFolderPair_->getValues();

    //add additional pairs
    guiCfg.mainCfg.additionalPairs.clear();

    for (const FolderPairPanel* panel : additionalFolderPairs_)
        guiCfg.mainCfg.additionalPairs.push_back(panel->getValues());

    //sync preview
    guiCfg.highlightSyncAction = m_bpButtonViewTypeSyncAction->isActive();

    return guiCfg;
}


void MainDialog::updateGuiDelayedIf(bool condition)
{
    const int delay = 400;

    if (condition)
    {
        filegrid::refresh(*m_gridMainL, *m_gridMainC, *m_gridMainR);
        m_gridMainL->Update();
        m_gridMainC->Update();
        m_gridMainR->Update();

        wxMilliSleep(delay); //some delay to show the changed GUI before removing rows from sight
    }

    updateGui();
}


void MainDialog::showConfigDialog(SyncConfigPanel panelToShow, int localPairIndexToShow)
{
    std::vector<LocalPairConfig> folderPairConfig;
    auto addPairCfg = [&](const FolderPairEnh& fp)
    {
        LocalPairConfig fpCfg;
        fpCfg.folderPairName = getShortDisplayNameForFolderPair(createAbstractPath(fp.folderPathPhraseLeft_ ),
                                                                createAbstractPath(fp.folderPathPhraseRight_));
        fpCfg.altCmpConfig  = fp.altCmpConfig;
        fpCfg.altSyncConfig = fp.altSyncConfig;
        fpCfg.localFilter   = fp.localFilter;
        folderPairConfig.push_back(fpCfg);
    };

    //don't recalculate value but consider current screen status!!!
    //e.g. it's possible that the first folder pair local config is shown with all config initial if user just removed local config via mouse context menu!
    const bool showLocalCfgFirstPair = m_bpButtonAltCompCfg->IsShown();
    //harmonize with MainDialog::updateGuiForFolderPair()!

    assert(m_bpButtonAltCompCfg->IsShown() == m_bpButtonAltSyncCfg->IsShown() &&
           m_bpButtonAltCompCfg->IsShown() == m_bpButtonLocalFilter->IsShown());

    if (showLocalCfgFirstPair)
    {
        addPairCfg(firstFolderPair_->getValues());
        for (const FolderPairPanel* panel : additionalFolderPairs_)
            addPairCfg(panel->getValues());
    }

    //------------------------------------------------

    const std::vector<LocalPairConfig> folderPairConfigOld = folderPairConfig;

    const CompConfig    cmpCfgOld    = currentCfg_.mainCfg.cmpConfig;
    const SyncConfig    syncCfgOld   = currentCfg_.mainCfg.syncCfg;
    const FilterConfig  filterCfgOld = currentCfg_.mainCfg.globalFilter;

    const bool                 ignoreErrorsOld      = currentCfg_.mainCfg.ignoreErrors;
    const Zstring              postSyncCommandOld   = currentCfg_.mainCfg.postSyncCommand;
    const PostSyncCondition    postSyncConditionOld = currentCfg_.mainCfg.postSyncCondition;
    const std::vector<Zstring> commandHistoryOld    = globalCfg_.gui.commandHistory;

    if (showSyncConfigDlg(this,
                          panelToShow,
                          localPairIndexToShow,
                          folderPairConfig,

                          currentCfg_.mainCfg.cmpConfig,
                          currentCfg_.mainCfg.syncCfg,
                          currentCfg_.mainCfg.globalFilter,

                          currentCfg_.mainCfg.ignoreErrors,
                          currentCfg_.mainCfg.postSyncCommand,
                          currentCfg_.mainCfg.postSyncCondition,
                          globalCfg_.gui.commandHistory,
                          globalCfg_.gui.commandHistItemsMax) == ReturnSyncConfig::BUTTON_OKAY)
    {
        assert(folderPairConfig.size() == folderPairConfigOld.size());

        if (showLocalCfgFirstPair)
        {
            {
                auto fp = firstFolderPair_->getValues();
                fp.altCmpConfig  = folderPairConfig[0].altCmpConfig;
                fp.altSyncConfig = folderPairConfig[0].altSyncConfig;
                fp.localFilter   = folderPairConfig[0].localFilter;
                firstFolderPair_->setValues(fp);
            }

            for (size_t i = 1; i < folderPairConfig.size(); ++i)
            {
                auto fp = additionalFolderPairs_[i - 1]->getValues();
                fp.altCmpConfig  = folderPairConfig[i].altCmpConfig;
                fp.altSyncConfig = folderPairConfig[i].altSyncConfig;
                fp.localFilter   = folderPairConfig[i].localFilter;
                additionalFolderPairs_[i - 1]->setValues(fp);
            }
        }

        //------------------------------------------------

        const bool cmpConfigChanged = currentCfg_.mainCfg.cmpConfig != cmpCfgOld || [&]
        {
            for (size_t i = 0; i < folderPairConfig.size(); ++i)
            {
                if ((folderPairConfig[i].altCmpConfig.get() == nullptr) != (folderPairConfigOld[i].altCmpConfig.get() == nullptr))
                    return true;
                if (folderPairConfig[i].altCmpConfig.get())
                    if (*folderPairConfig[i].altCmpConfig != *folderPairConfigOld[i].altCmpConfig)
                        return true;
            }
            return false;
        }();

        const bool syncConfigChanged = currentCfg_.mainCfg.syncCfg != syncCfgOld || [&]
        {
            for (size_t i = 0; i < folderPairConfig.size(); ++i)
            {
                if ((folderPairConfig[i].altSyncConfig.get() == nullptr) != (folderPairConfigOld[i].altSyncConfig.get() == nullptr))
                    return true;
                if (folderPairConfig[i].altSyncConfig.get())
                    if (*folderPairConfig[i].altSyncConfig != *folderPairConfigOld[i].altSyncConfig)
                        return true;
            }
            return false;
        }();

        const bool filterConfigChanged = currentCfg_.mainCfg.globalFilter != filterCfgOld || [&]
        {
            for (size_t i = 0; i < folderPairConfig.size(); ++i)
                if (folderPairConfig[i].localFilter != folderPairConfigOld[i].localFilter)
                    return true;
            return false;
        }();

        const bool miscConfigChanged = currentCfg_.mainCfg.ignoreErrors      != ignoreErrorsOld ||
                                       currentCfg_.mainCfg.postSyncCommand   != postSyncCommandOld ||
                                       currentCfg_.mainCfg.postSyncCondition != postSyncConditionOld;
        //globalCfg.gui.commandHistory != commandHistoryOld;

        //------------------------------------------------

        if (cmpConfigChanged)
        {
            const bool setDefaultViewType = currentCfg_.mainCfg.cmpConfig.compareVar != cmpCfgOld.compareVar;
            applyCompareConfig(setDefaultViewType);
        }

        if (syncConfigChanged)
            applySyncConfig();

        if (filterConfigChanged)
        {
            updateGlobalFilterButton(); //refresh global filter icon
            applyFilterConfig(); //re-apply filter
        }

        if (miscConfigChanged)
            updateUnsavedCfgStatus(); //usually included by: updateGui();
    }
}


void MainDialog::OnGlobalFilterContext(wxEvent& event)
{
    auto clearFilter = [&]
    {
        currentCfg_.mainCfg.globalFilter = FilterConfig();
        updateGlobalFilterButton(); //refresh global filter icon
        applyFilterConfig(); //re-apply filter
    };
    auto copyFilter  = [&] { filterCfgOnClipboard_ = std::make_unique<FilterConfig>(currentCfg_.mainCfg.globalFilter); };
    auto pasteFilter = [&]
    {
        if (filterCfgOnClipboard_)
        {
            currentCfg_.mainCfg.globalFilter = *filterCfgOnClipboard_;
            updateGlobalFilterButton(); //refresh global filter icon
            applyFilterConfig(); //re-apply filter
        }
    };

    ContextMenu menu;
    menu.addItem( _("Clear filter"), clearFilter, nullptr, !isNullFilter(currentCfg_.mainCfg.globalFilter));
    menu.addSeparator();
    menu.addItem( _("Copy"),  copyFilter,  nullptr, !isNullFilter(currentCfg_.mainCfg.globalFilter));
    menu.addItem( _("Paste"), pasteFilter, nullptr, filterCfgOnClipboard_.get() != nullptr);

    wxPoint pos = m_bpButtonFilterContext->GetPosition();
    pos.x += m_bpButtonFilterContext->GetSize().GetWidth();
    menu.popup(*m_panelTopButtons, pos);
}


void MainDialog::OnToggleViewType(wxCommandEvent& event)
{
    setViewTypeSyncAction(!m_bpButtonViewTypeSyncAction->isActive()); //toggle view
}


void MainDialog::OnToggleViewButton(wxCommandEvent& event)
{
    if (auto button = dynamic_cast<ToggleButton*>(event.GetEventObject()))
    {
        button->toggle();
        updateGui();
    }
    else
        assert(false);
}


inline
wxBitmap buttonPressed(const std::string& name)
{
    wxBitmap background = getResourceImage(L"buttonPressed");
    return mirrorIfRtl(layOver(background, getResourceImage(utfTo<wxString>(name))));
}


inline
wxBitmap buttonReleased(const std::string& name)
{
    wxImage output = getResourceImage(utfTo<wxString>(name)).ConvertToImage().ConvertToGreyscale(1.0/3, 1.0/3, 1.0/3); //treat all channels equally!
    //moveImage(output, 1, 0); //move image right one pixel

    brighten(output, 80);
    return mirrorIfRtl(output);
}


void MainDialog::initViewFilterButtons()
{
    m_bpButtonViewTypeSyncAction->init(getResourceImage(L"viewtype_sync_action"), getResourceImage(L"viewtype_cmp_result"));
    //tooltip is updated dynamically in setViewTypeSyncAction()

    auto initButton = [](ToggleButton& btn, const char* imgName, const wxString& tooltip) { btn.init(buttonPressed(imgName), buttonReleased(imgName)); btn.SetToolTip(tooltip); };

    //compare result buttons
    initButton(*m_bpButtonShowLeftOnly,   "cat_left_only",   _("Show files that exist on left side only"));
    initButton(*m_bpButtonShowRightOnly,  "cat_right_only",  _("Show files that exist on right side only"));
    initButton(*m_bpButtonShowLeftNewer,  "cat_left_newer",  _("Show files that are newer on left"));
    initButton(*m_bpButtonShowRightNewer, "cat_right_newer", _("Show files that are newer on right"));
    initButton(*m_bpButtonShowEqual,      "cat_equal",       _("Show files that are equal"));
    initButton(*m_bpButtonShowDifferent,  "cat_different",   _("Show files that are different"));
    initButton(*m_bpButtonShowConflict,   "cat_conflict",    _("Show conflicts"));

    //sync preview buttons
    initButton(*m_bpButtonShowCreateLeft,  "so_create_left",  _("Show files that will be created on the left side"));
    initButton(*m_bpButtonShowCreateRight, "so_create_right", _("Show files that will be created on the right side"));
    initButton(*m_bpButtonShowDeleteLeft,  "so_delete_left",  _("Show files that will be deleted on the left side"));
    initButton(*m_bpButtonShowDeleteRight, "so_delete_right", _("Show files that will be deleted on the right side"));
    initButton(*m_bpButtonShowUpdateLeft,  "so_update_left",  _("Show files that will be updated on the left side"));
    initButton(*m_bpButtonShowUpdateRight, "so_update_right", _("Show files that will be updated on the right side"));
    initButton(*m_bpButtonShowDoNothing,   "so_none",         _("Show files that won't be copied"));

    initButton(*m_bpButtonShowExcluded, "checkbox_false", _("Show filtered or temporarily excluded files"));
}


void MainDialog::setViewFilterDefault()
{
    auto setButton = [](ToggleButton* tb, bool value) { tb->setActive(value); };

    const auto& def = globalCfg_.gui.mainDlg.viewFilterDefault;
    setButton(m_bpButtonShowExcluded,   def.excluded);
    setButton(m_bpButtonShowEqual,      def.equal);
    setButton(m_bpButtonShowConflict,   def.conflict);

    setButton(m_bpButtonShowLeftOnly,   def.leftOnly);
    setButton(m_bpButtonShowRightOnly,  def.rightOnly);
    setButton(m_bpButtonShowLeftNewer,  def.leftNewer);
    setButton(m_bpButtonShowRightNewer, def.rightNewer);
    setButton(m_bpButtonShowDifferent,  def.different);

    setButton(m_bpButtonShowCreateLeft,  def.createLeft);
    setButton(m_bpButtonShowCreateRight, def.createRight);
    setButton(m_bpButtonShowUpdateLeft,  def.updateLeft);
    setButton(m_bpButtonShowUpdateRight, def.updateRight);
    setButton(m_bpButtonShowDeleteLeft,  def.deleteLeft);
    setButton(m_bpButtonShowDeleteRight, def.deleteRight);
    setButton(m_bpButtonShowDoNothing,   def.doNothing);
}


void MainDialog::OnViewButtonRightClick(wxMouseEvent& event)
{
    auto setButtonDefault = [](const ToggleButton* tb, bool& defaultValue)
    {
        if (tb->IsShown())
            defaultValue = tb->isActive();
    };

    auto saveDefault = [&]
    {
        auto& def = globalCfg_.gui.mainDlg.viewFilterDefault;
        setButtonDefault(m_bpButtonShowExcluded,   def.excluded);
        setButtonDefault(m_bpButtonShowEqual,      def.equal);
        setButtonDefault(m_bpButtonShowConflict,   def.conflict);

        setButtonDefault(m_bpButtonShowLeftOnly,   def.leftOnly);
        setButtonDefault(m_bpButtonShowRightOnly,  def.rightOnly);
        setButtonDefault(m_bpButtonShowLeftNewer,  def.leftNewer);
        setButtonDefault(m_bpButtonShowRightNewer, def.rightNewer);
        setButtonDefault(m_bpButtonShowDifferent,  def.different);

        setButtonDefault(m_bpButtonShowCreateLeft,  def.createLeft);
        setButtonDefault(m_bpButtonShowCreateRight, def.createRight);
        setButtonDefault(m_bpButtonShowUpdateLeft,  def.updateLeft);
        setButtonDefault(m_bpButtonShowUpdateRight, def.updateRight);
        setButtonDefault(m_bpButtonShowDeleteLeft,  def.deleteLeft);
        setButtonDefault(m_bpButtonShowDeleteRight, def.deleteRight);
        setButtonDefault(m_bpButtonShowDoNothing,   def.doNothing);
    };

    ContextMenu menu;
    menu.addItem( _("Save as default"), saveDefault);
    menu.popup(*this);
}


void MainDialog::updateGlobalFilterButton()
{
    //global filter: test for Null-filter
    std::wstring status;
    if (!isNullFilter(currentCfg_.mainCfg.globalFilter))
    {
        setImage(*m_bpButtonFilter, getResourceImage(L"filter"));
        status = _("Active");
    }
    else
    {
        setImage(*m_bpButtonFilter, greyScale(getResourceImage(L"filter")));
        status = _("None");
    }

    m_bpButtonFilter->SetToolTip(_("Filter") + L" (F7) (" + status + L")");
    m_bpButtonFilterContext->SetToolTip(m_bpButtonFilter->GetToolTipText());
}


void MainDialog::OnCompare(wxCommandEvent& event)
{
    //wxBusyCursor dummy; -> redundant: progress already shown in progress dialog!

    FocusPreserver fp; //e.g. keep focus on config panel after pressing F5

    int scrollPosX = 0;
    int scrollPosY = 0;
    m_gridMainL->GetViewStart(&scrollPosX, &scrollPosY); //preserve current scroll position
    ZEN_ON_SCOPE_EXIT(
        m_gridMainL->Scroll(scrollPosX, scrollPosY); //
        m_gridMainR->Scroll(scrollPosX, scrollPosY); //restore
        m_gridMainC->Scroll(-1, scrollPosY); );      //

    clearGrid(); //avoid memory peak by clearing old data first

    disableAllElements(true); //StatusHandlerTemporaryPanel will internally process Window messages, so avoid unexpected callbacks!
    auto app = wxTheApp; //fix lambda/wxWigets/VC fuck up
    ZEN_ON_SCOPE_EXIT(app->Yield(); enableAllElements()); //ui update before enabling buttons again: prevent strange behaviour of delayed button clicks

    try
    {
        //handle status display and error messages
        StatusHandlerTemporaryPanel statusHandler(*this);

        const std::vector<FolderPairCfg> cmpConfig = extractCompareCfg(getConfig().mainCfg);

        //GUI mode: place directory locks on directories isolated(!) during both comparison and synchronization
        std::unique_ptr<LockHolder> dirLocks;

        //COMPARE DIRECTORIES
        folderCmp_ = compare(globalCfg_.warnDlgs,
                             globalCfg_.fileTimeTolerance,
                             true, //allowUserInteraction
                             globalCfg_.runWithBackgroundPriority,
                             globalCfg_.folderAccessTimeout,
                             globalCfg_.createLockFile,
                             dirLocks,
                             cmpConfig,
                             statusHandler); //throw AbortProcess
    }
    catch (AbortProcess&)
    {
        updateGui(); //refresh grid in ANY case! (also on abort)
        return;
    }

    filegrid::getDataView(*m_gridMainC).setData(folderCmp_); //update view on data
    treegrid::getDataView(*m_gridOverview).setData(folderCmp_); //
    updateGui();

    m_gridMainL->clearSelection(ALLOW_GRID_EVENT);
    m_gridMainC->clearSelection(ALLOW_GRID_EVENT);
    m_gridMainR->clearSelection(ALLOW_GRID_EVENT);

    m_gridOverview->clearSelection(ALLOW_GRID_EVENT);

    //play (optional) sound notification
    if (!globalCfg_.soundFileCompareFinished.empty())
    {
        const Zstring soundFilePath = getResourceDirPf() + globalCfg_.soundFileCompareFinished;
        if (fileAvailable(soundFilePath))
            wxSound::Play(utfTo<wxString>(soundFilePath), wxSOUND_ASYNC);
        //warning: this may fail and show a wxWidgets error message! => must not play when running FFS without user interaction!
    }

    if (!IsActive())
        RequestUserAttention();

    //add to folder history after successful comparison only
    folderHistoryLeft_ ->addItem(utfTo<Zstring>(m_folderPathLeft ->GetValue()));
    folderHistoryRight_->addItem(utfTo<Zstring>(m_folderPathRight->GetValue()));

    if (fp.getFocus() == m_buttonCompare)
        fp.setFocus(m_buttonSync);

    //prepare status information
    if (allElementsEqual(folderCmp_))
    {
        flashStatusInformation(_("All files are in sync"));

        //update last sync date for selected cfg files https://www.freefilesync.org/forum/viewtopic.php?t=4991
        updateLastSyncTimesToNow();
    }
}


void MainDialog::updateGui()
{
    updateGridViewData(); //update gridDataView and write status information

    updateStatistics();

    updateUnsavedCfgStatus();

    updateTopButton(*m_buttonCompare, getResourceImage(L"compare"), getConfig().mainCfg.getCompVariantName(), false /*makeGrey*/);
    updateTopButton(*m_buttonSync,    getResourceImage(L"sync"),    getConfig().mainCfg.getSyncVariantName(), folderCmp_.empty());
    m_panelTopButtons->Layout();

    m_menuItemExportList->Enable(!folderCmp_.empty()); //a CSV without even folder names confuses users: https://www.freefilesync.org/forum/viewtopic.php?t=4787

    auiMgr_.Update(); //fix small display distortion, if view filter panel is empty
}


void MainDialog::clearGrid(ptrdiff_t pos)
{
    if (!folderCmp_.empty())
    {
        assert(pos < makeSigned(folderCmp_.size()));
        if (pos < 0)
            folderCmp_.clear();
        else
            folderCmp_.erase(folderCmp_.begin() + pos);
    }

    filegrid::getDataView(*m_gridMainC).setData(folderCmp_);
    treegrid::getDataView(*m_gridOverview).setData(folderCmp_);
    updateGui();
}


void MainDialog::updateStatistics()
{
    auto setValue = [](wxStaticText& txtControl, bool isZeroValue, const wxString& valueAsString, wxStaticBitmap& bmpControl, const wchar_t* bmpName)
    {
        wxFont fnt = txtControl.GetFont();
        fnt.SetWeight(isZeroValue ? wxFONTWEIGHT_NORMAL : wxFONTWEIGHT_BOLD);
        txtControl.SetFont(fnt);

        setText(txtControl, valueAsString);

        if (isZeroValue)
            bmpControl.SetBitmap(greyScale(mirrorIfRtl(getResourceImage(bmpName))));
        else
            bmpControl.SetBitmap(mirrorIfRtl(getResourceImage(bmpName)));
    };

    auto setIntValue = [&setValue](wxStaticText& txtControl, int value, wxStaticBitmap& bmpControl, const wchar_t* bmpName)
    {
        setValue(txtControl, value == 0, formatNumber(value), bmpControl, bmpName);
    };

    //update preview of item count and bytes to be transferred:
    const SyncStatistics st(folderCmp_);

    setValue(*m_staticTextData, st.getBytesToProcess() == 0, formatFilesizeShort(st.getBytesToProcess()), *m_bitmapData,  L"data");
    setIntValue(*m_staticTextCreateLeft,  st.createCount< LEFT_SIDE>(), *m_bitmapCreateLeft,  L"so_create_left_small");
    setIntValue(*m_staticTextUpdateLeft,  st.updateCount< LEFT_SIDE>(), *m_bitmapUpdateLeft,  L"so_update_left_small");
    setIntValue(*m_staticTextDeleteLeft,  st.deleteCount< LEFT_SIDE>(), *m_bitmapDeleteLeft,  L"so_delete_left_small");
    setIntValue(*m_staticTextCreateRight, st.createCount<RIGHT_SIDE>(), *m_bitmapCreateRight, L"so_create_right_small");
    setIntValue(*m_staticTextUpdateRight, st.updateCount<RIGHT_SIDE>(), *m_bitmapUpdateRight, L"so_update_right_small");
    setIntValue(*m_staticTextDeleteRight, st.deleteCount<RIGHT_SIDE>(), *m_bitmapDeleteRight, L"so_delete_right_small");

    m_panelStatistics->Layout();
    m_panelStatistics->Refresh(); //fix small mess up on RTL layout
}


void MainDialog::applyCompareConfig(bool setDefaultViewType)
{
    clearGrid(); //+ GUI update

    //convenience: change sync view
    if (setDefaultViewType)
        switch (currentCfg_.mainCfg.cmpConfig.compareVar)
        {
            case CompareVariant::TIME_SIZE:
            case CompareVariant::SIZE:
                setViewTypeSyncAction(true);
                break;

            case CompareVariant::CONTENT:
                setViewTypeSyncAction(false);
                break;
        }
}


void MainDialog::OnStartSync(wxCommandEvent& event)
{
    if (folderCmp_.empty())
    {
        //quick sync: simulate button click on "compare"
        wxCommandEvent dummy(wxEVT_COMMAND_BUTTON_CLICKED);
        m_buttonCompare->Command(dummy); //simulate click

        if (folderCmp_.empty()) //check if user aborted or error occurred, ect...
            return;
    }

    //show sync preview/confirmation dialog
    if (globalCfg_.confirmDlgs.confirmSyncStart)
    {
        bool dontShowAgain = false;

        if (showSyncConfirmationDlg(this,
                                    getConfig().mainCfg.getSyncVariantName(),
                                    SyncStatistics(folderCmp_),
                                    dontShowAgain) != ReturnSmallDlg::BUTTON_OKAY)
            return;

        globalCfg_.confirmDlgs.confirmSyncStart = !dontShowAgain;
    }

    bool exitAfterSync = false;
    try
    {
        const std::chrono::system_clock::time_point syncStartTime = std::chrono::system_clock::now();

        //PERF_START;
        const Zstring activeCfgFilePath = activeConfigFiles_.size() == 1 && !equalFilePath(activeConfigFiles_[0], lastRunConfigPath_) ? activeConfigFiles_[0] : Zstring();

        const auto& guiCfg = getConfig();

        disableAllElements(false); //StatusHandlerFloatingDialog will internally process Window messages, so avoid unexpected callbacks!
        ZEN_ON_SCOPE_EXIT(enableAllElements());

        //class handling status updates and error messages
        StatusHandlerFloatingDialog statusHandler(this, //throw AbortProcess
                                                  syncStartTime,
                                                  globalCfg_.lastSyncsLogFileSizeMax,
                                                  currentCfg_.mainCfg.ignoreErrors,
                                                  globalCfg_.automaticRetryCount,
                                                  globalCfg_.automaticRetryDelay,
                                                  extractJobName(activeCfgFilePath),
                                                  globalCfg_.soundFileSyncFinished,
                                                  guiCfg.mainCfg.postSyncCommand,
                                                  guiCfg.mainCfg.postSyncCondition,
                                                  exitAfterSync,
                                                  globalCfg_.autoCloseProgressDialog);

        //inform about (important) non-default global settings
        logNonDefaultSettings(globalCfg_, statusHandler); //let's report here rather than before comparison (user might have changed global settings in the meantime!)

        //wxBusyCursor dummy; -> redundant: progress already shown in progress dialog!

        //GUI mode: place directory locks on directories isolated(!) during both comparison and synchronization
        std::unique_ptr<LockHolder> dirLocks;
        if (globalCfg_.createLockFile)
        {
            std::set<Zstring, LessFilePath> availableDirPaths;
            for (auto it = begin(folderCmp_); it != end(folderCmp_); ++it)
            {
                if (it->isAvailable<LEFT_SIDE>()) //do NOT check directory existence again!
                    if (Opt<Zstring> nativeFolderPath = AFS::getNativeItemPath(it->getAbstractPath<LEFT_SIDE>())) //restrict directory locking to native paths until further
                        availableDirPaths.insert(*nativeFolderPath);

                if (it->isAvailable<RIGHT_SIDE>())
                    if (Opt<Zstring> nativeFolderPath = AFS::getNativeItemPath(it->getAbstractPath<RIGHT_SIDE>()))
                        availableDirPaths.insert(*nativeFolderPath);
            }
            dirLocks = std::make_unique<LockHolder>(availableDirPaths, globalCfg_.warnDlgs.warnDirectoryLockFailed, statusHandler);
        }

        //START SYNCHRONIZATION
        const std::vector<FolderPairSyncCfg> syncProcessCfg = extractSyncCfg(guiCfg.mainCfg);
        if (syncProcessCfg.size() != folderCmp_.size())
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
        //should never happen: sync button is deactivated if they are not in sync

        synchronize(syncStartTime,
                    globalCfg_.verifyFileCopy,
                    globalCfg_.copyLockedFiles,
                    globalCfg_.copyFilePermissions,
                    globalCfg_.failSafeFileCopy,
                    globalCfg_.runWithBackgroundPriority,
                    globalCfg_.folderAccessTimeout,
                    syncProcessCfg,
                    folderCmp_,
                    globalCfg_.warnDlgs,
                    statusHandler);

        //not cancelled? => update last sync date for selected cfg files
        updateLastSyncTimesToNow();
    }
    catch (AbortProcess&) {}

    //remove empty rows: just a beautification, invalid rows shouldn't cause issues
    filegrid::getDataView(*m_gridMainC).removeInvalidRows();

    updateGui();

    if (exitAfterSync)
        Destroy(); //don't use Close(): we don't want to show the prompt to save current config in OnClose()
}


void MainDialog::updateLastSyncTimesToNow()
{
    const time_t now = std::time(nullptr);

    std::vector<std::pair<Zstring, time_t>> lastSyncTimes;
    for (const Zstring& cfgPath : activeConfigFiles_)
        lastSyncTimes.emplace_back(cfgPath, now);

    cfggrid::getDataView(*m_gridCfgHistory).setLastSyncTime(lastSyncTimes);

    //re-apply selection: sort order changed if sorted by last sync time
    cfggrid::addAndSelect(*m_gridCfgHistory, activeConfigFiles_, false /*scrollToSelection*/);
    //m_gridCfgHistory->Refresh(); <- implicit in last call
}


void MainDialog::onGridDoubleClickL(GridClickEvent& event)
{
    onGridDoubleClickRim(event.row_, true);
}


void MainDialog::onGridDoubleClickR(GridClickEvent& event)
{
    onGridDoubleClickRim(event.row_, false);
}


void MainDialog::onGridDoubleClickRim(size_t row, bool leftSide)
{
    if (!globalCfg_.gui.externalApps.empty())
    {
        std::vector<FileSystemObject*> selectionLeft;
        std::vector<FileSystemObject*> selectionRight;
        if (FileSystemObject* fsObj = filegrid::getDataView(*m_gridMainC).getObject(row)) //selection must be a list of BOUND pointers!
            (leftSide ? selectionLeft : selectionRight) = { fsObj };

        openExternalApplication(globalCfg_.gui.externalApps[0].cmdLine, leftSide, selectionLeft, selectionRight);
    }
}


void MainDialog::onGridLabelLeftClick(bool onLeft, ColumnTypeRim type)
{
    auto sortInfo = filegrid::getDataView(*m_gridMainC).getSortInfo();

    bool sortAscending = getDefaultSortDirection(type);
    if (sortInfo && sortInfo->onLeft == onLeft && sortInfo->type == type)
        sortAscending = !sortInfo->ascending;

    const ItemPathFormat itemPathFormat = onLeft ? globalCfg_.gui.mainDlg.itemPathFormatLeftGrid : globalCfg_.gui.mainDlg.itemPathFormatRightGrid;

    filegrid::getDataView(*m_gridMainC).sortView(type, itemPathFormat, onLeft, sortAscending);

    m_gridMainL->clearSelection(ALLOW_GRID_EVENT);
    m_gridMainC->clearSelection(ALLOW_GRID_EVENT);
    m_gridMainR->clearSelection(ALLOW_GRID_EVENT);

    updateGui(); //refresh gridDataView
}

void MainDialog::onGridLabelLeftClickL(GridLabelClickEvent& event)
{
    onGridLabelLeftClick(true, static_cast<ColumnTypeRim>(event.colType_));
}


void MainDialog::onGridLabelLeftClickR(GridLabelClickEvent& event)
{
    onGridLabelLeftClick(false, static_cast<ColumnTypeRim>(event.colType_));
}


void MainDialog::onGridLabelLeftClickC(GridLabelClickEvent& event)
{
    //sorting middle grid is more or less useless: therefore let's toggle view instead!
    setViewTypeSyncAction(!m_bpButtonViewTypeSyncAction->isActive()); //toggle view
}


void MainDialog::OnSwapSides(wxCommandEvent& event)
{
    //swap directory names:
    FolderPairEnh fp1st = firstFolderPair_->getValues();
    std::swap(fp1st.folderPathPhraseLeft_, fp1st.folderPathPhraseRight_);
    firstFolderPair_->setValues(fp1st);

    for (FolderPairPanel* panel : additionalFolderPairs_)
    {
        FolderPairEnh fp = panel->getValues();
        std::swap(fp.folderPathPhraseLeft_, fp.folderPathPhraseRight_);
        panel->setValues(fp);
    }

    //swap view filter
    bool tmp = m_bpButtonShowLeftOnly->isActive();
    m_bpButtonShowLeftOnly->setActive(m_bpButtonShowRightOnly->isActive());
    m_bpButtonShowRightOnly->setActive(tmp);

    tmp = m_bpButtonShowLeftNewer->isActive();
    m_bpButtonShowLeftNewer->setActive(m_bpButtonShowRightNewer->isActive());
    m_bpButtonShowRightNewer->setActive(tmp);

    /* for sync preview and "mirror" variant swapping may create strange effect:
    tmp = m_bpButtonShowCreateLeft->isActive();
    m_bpButtonShowCreateLeft->setActive(m_bpButtonShowCreateRight->isActive());
    m_bpButtonShowCreateRight->setActive(tmp);

    tmp = m_bpButtonShowDeleteLeft->isActive();
    m_bpButtonShowDeleteLeft->setActive(m_bpButtonShowDeleteRight->isActive());
    m_bpButtonShowDeleteRight->setActive(tmp);

    tmp = m_bpButtonShowUpdateLeft->isActive();
    m_bpButtonShowUpdateLeft->setActive(m_bpButtonShowUpdateRight->isActive());
    m_bpButtonShowUpdateRight->setActive(tmp);
    */

    try
    {
        swapGrids(getConfig().mainCfg, folderCmp_); //throw FileError
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
    }

    updateGui();
}


void MainDialog::updateGridViewData()
{
    size_t filesOnLeftView    = 0;
    size_t foldersOnLeftView  = 0;
    size_t filesOnRightView   = 0;
    size_t foldersOnRightView = 0;
    uint64_t filesizeLeftView  = 0;
    uint64_t filesizeRightView = 0;

    auto updateVisibility = [](ToggleButton* btn, bool shown)
    {
        if (btn->IsShown() != shown)
            btn->Show(shown);
    };

    if (m_bpButtonViewTypeSyncAction->isActive())
    {
        const FileView::StatusSyncPreview result = filegrid::getDataView(*m_gridMainC).updateSyncPreview(m_bpButtonShowExcluded   ->isActive(),
                                                   m_bpButtonShowCreateLeft ->isActive(),
                                                   m_bpButtonShowCreateRight->isActive(),
                                                   m_bpButtonShowDeleteLeft ->isActive(),
                                                   m_bpButtonShowDeleteRight->isActive(),
                                                   m_bpButtonShowUpdateLeft ->isActive(),
                                                   m_bpButtonShowUpdateRight->isActive(),
                                                   m_bpButtonShowDoNothing  ->isActive(),
                                                   m_bpButtonShowEqual      ->isActive(),
                                                   m_bpButtonShowConflict   ->isActive());
        filesOnLeftView    = result.filesOnLeftView;
        foldersOnLeftView  = result.foldersOnLeftView;
        filesOnRightView   = result.filesOnRightView;
        foldersOnRightView = result.foldersOnRightView;
        filesizeLeftView   = result.filesizeLeftView;
        filesizeRightView  = result.filesizeRightView;

        //sync preview buttons
        updateVisibility(m_bpButtonShowExcluded, result.existsExcluded);
        updateVisibility(m_bpButtonShowEqual,    result.existsEqual);
        updateVisibility(m_bpButtonShowConflict, result.existsConflict);

        updateVisibility(m_bpButtonShowCreateLeft,  result.existsSyncCreateLeft);
        updateVisibility(m_bpButtonShowCreateRight, result.existsSyncCreateRight);
        updateVisibility(m_bpButtonShowDeleteLeft,  result.existsSyncDeleteLeft);
        updateVisibility(m_bpButtonShowDeleteRight, result.existsSyncDeleteRight);
        updateVisibility(m_bpButtonShowUpdateLeft,  result.existsSyncDirLeft);
        updateVisibility(m_bpButtonShowUpdateRight, result.existsSyncDirRight);
        updateVisibility(m_bpButtonShowDoNothing,   result.existsSyncDirNone);

        updateVisibility(m_bpButtonShowLeftOnly,   false);
        updateVisibility(m_bpButtonShowRightOnly,  false);
        updateVisibility(m_bpButtonShowLeftNewer,  false);
        updateVisibility(m_bpButtonShowRightNewer, false);
        updateVisibility(m_bpButtonShowDifferent,  false);
    }
    else
    {
        const FileView::StatusCmpResult result = filegrid::getDataView(*m_gridMainC).updateCmpResult(m_bpButtonShowExcluded  ->isActive(),
                                                 m_bpButtonShowLeftOnly  ->isActive(),
                                                 m_bpButtonShowRightOnly ->isActive(),
                                                 m_bpButtonShowLeftNewer ->isActive(),
                                                 m_bpButtonShowRightNewer->isActive(),
                                                 m_bpButtonShowDifferent ->isActive(),
                                                 m_bpButtonShowEqual     ->isActive(),
                                                 m_bpButtonShowConflict  ->isActive());
        filesOnLeftView    = result.filesOnLeftView;
        foldersOnLeftView  = result.foldersOnLeftView;
        filesOnRightView   = result.filesOnRightView;
        foldersOnRightView = result.foldersOnRightView;
        filesizeLeftView   = result.filesizeLeftView;
        filesizeRightView  = result.filesizeRightView;

        //comparison result view buttons
        updateVisibility(m_bpButtonShowExcluded, result.existsExcluded);
        updateVisibility(m_bpButtonShowEqual,    result.existsEqual);
        updateVisibility(m_bpButtonShowConflict, result.existsConflict);

        updateVisibility(m_bpButtonShowCreateLeft,  false);
        updateVisibility(m_bpButtonShowCreateRight, false);
        updateVisibility(m_bpButtonShowDeleteLeft,  false);
        updateVisibility(m_bpButtonShowDeleteRight, false);
        updateVisibility(m_bpButtonShowUpdateLeft,  false);
        updateVisibility(m_bpButtonShowUpdateRight, false);
        updateVisibility(m_bpButtonShowDoNothing,   false);

        updateVisibility(m_bpButtonShowLeftOnly,   result.existsLeftOnly);
        updateVisibility(m_bpButtonShowRightOnly,  result.existsRightOnly);
        updateVisibility(m_bpButtonShowLeftNewer,  result.existsLeftNewer);
        updateVisibility(m_bpButtonShowRightNewer, result.existsRightNewer);
        updateVisibility(m_bpButtonShowDifferent,  result.existsDifferent);
    }

    const bool anySelectViewButtonShown = m_bpButtonShowEqual      ->IsShown() ||
                                          m_bpButtonShowConflict   ->IsShown() ||

                                          m_bpButtonShowCreateLeft ->IsShown() ||
                                          m_bpButtonShowCreateRight->IsShown() ||
                                          m_bpButtonShowDeleteLeft ->IsShown() ||
                                          m_bpButtonShowDeleteRight->IsShown() ||
                                          m_bpButtonShowUpdateLeft ->IsShown() ||
                                          m_bpButtonShowUpdateRight->IsShown() ||
                                          m_bpButtonShowDoNothing  ->IsShown() ||

                                          m_bpButtonShowLeftOnly  ->IsShown() ||
                                          m_bpButtonShowRightOnly ->IsShown() ||
                                          m_bpButtonShowLeftNewer ->IsShown() ||
                                          m_bpButtonShowRightNewer->IsShown() ||
                                          m_bpButtonShowDifferent ->IsShown();

    const bool anyViewButtonShown = anySelectViewButtonShown || m_bpButtonShowExcluded->IsShown();

    m_staticTextViewType        ->Show(anyViewButtonShown);
    m_bpButtonViewTypeSyncAction->Show(anyViewButtonShown);
    m_staticTextSelectView      ->Show(anySelectViewButtonShown);

    m_panelViewFilter->Layout();

    //all three grids retrieve their data directly via gridDataView
    filegrid::refresh(*m_gridMainL, *m_gridMainC, *m_gridMainR);

    //overview panel
    if (m_bpButtonViewTypeSyncAction->isActive())
        treegrid::getDataView(*m_gridOverview).updateSyncPreview(m_bpButtonShowExcluded   ->isActive(),
                                                                 m_bpButtonShowCreateLeft ->isActive(),
                                                                 m_bpButtonShowCreateRight->isActive(),
                                                                 m_bpButtonShowDeleteLeft ->isActive(),
                                                                 m_bpButtonShowDeleteRight->isActive(),
                                                                 m_bpButtonShowUpdateLeft ->isActive(),
                                                                 m_bpButtonShowUpdateRight->isActive(),
                                                                 m_bpButtonShowDoNothing  ->isActive(),
                                                                 m_bpButtonShowEqual      ->isActive(),
                                                                 m_bpButtonShowConflict   ->isActive());
    else
        treegrid::getDataView(*m_gridOverview).updateCmpResult(m_bpButtonShowExcluded  ->isActive(),
                                                               m_bpButtonShowLeftOnly  ->isActive(),
                                                               m_bpButtonShowRightOnly ->isActive(),
                                                               m_bpButtonShowLeftNewer ->isActive(),
                                                               m_bpButtonShowRightNewer->isActive(),
                                                               m_bpButtonShowDifferent ->isActive(),
                                                               m_bpButtonShowEqual     ->isActive(),
                                                               m_bpButtonShowConflict  ->isActive());
    m_gridOverview->Refresh();

    //update status bar information
    setStatusBarFileStatistics(filesOnLeftView,
                               foldersOnLeftView,
                               filesOnRightView,
                               foldersOnRightView,
                               filesizeLeftView,
                               filesizeRightView);
}


void MainDialog::applyFilterConfig()
{
    applyFiltering(folderCmp_, getConfig().mainCfg);
    updateGui();
    //updateGuiDelayedIf(currentCfg.hideExcludedItems); //show update GUI before removing rows
}


void MainDialog::applySyncConfig()
{
    try
    {
        redetermineSyncDirection(getConfig().mainCfg, folderCmp_, nullptr /*notifyStatus*/); //throw FileError
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
    }

    updateGui();
}


void MainDialog::OnMenuFindItem(wxCommandEvent& event) //CTRL + F
{
    showFindPanel();
}


void MainDialog::OnSearchGridEnter(wxCommandEvent& event)
{
    startFindNext(true /*searchAscending*/);
}


void MainDialog::OnHideSearchPanel(wxCommandEvent& event)
{
    hideFindPanel();
}


void MainDialog::OnSearchPanelKeyPressed(wxKeyEvent& event)
{
    switch (event.GetKeyCode())
    {
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER: //catches ENTER keys while focus is on *any* part of m_panelSearch! Seems to obsolete OnSearchGridEnter()!
            startFindNext(true /*searchAscending*/);
            return;
        case WXK_ESCAPE:
            hideFindPanel();
            return;
    }
    event.Skip();
}


void MainDialog::showFindPanel() //CTRL + F or F3 with empty search phrase
{
    auiMgr_.GetPane(m_panelSearch).Show();
    auiMgr_.Update();

    m_textCtrlSearchTxt->SelectAll();

    wxWindow* focus = wxWindow::FindFocus(); //restore when closing panel!
    if (!isComponentOf(focus, m_panelSearch))
        focusWindowAfterSearch_ = focus == &m_gridMainR->getMainWin() ? focus : &m_gridMainL->getMainWin();
    //don't save pointer to arbitrary window: it might not exist anymore when hideFindPanel() uses it!!! (e.g. some folder pair panel)
    m_textCtrlSearchTxt->SetFocus();
}


void MainDialog::hideFindPanel()
{
    auiMgr_.GetPane(m_panelSearch).Hide();
    auiMgr_.Update();

    if (focusWindowAfterSearch_)
    {
        focusWindowAfterSearch_->SetFocus();
        focusWindowAfterSearch_ = nullptr;
    }
}


void MainDialog::startFindNext(bool searchAscending) //F3 or ENTER in m_textCtrlSearchTxt
{
    Zstring searchString = utfTo<Zstring>(trimCpy(m_textCtrlSearchTxt->GetValue()));


    if (searchString.empty())
        showFindPanel();
    else
    {
        Grid* grid1 = m_gridMainL;
        Grid* grid2 = m_gridMainR;

        wxWindow* focus = wxWindow::FindFocus();
        if ((isComponentOf(focus, m_panelSearch) ? focusWindowAfterSearch_ : focus) == &m_gridMainR->getMainWin())
            std::swap(grid1, grid2); //select side to start search at grid cursor position

        wxBeginBusyCursor(wxHOURGLASS_CURSOR);
        const std::pair<const Grid*, ptrdiff_t> result = findGridMatch(*grid1, *grid2, utfTo<std::wstring>(searchString),
                                                                       m_checkBoxMatchCase->GetValue(), searchAscending); //parameter owned by GUI, *not* globalCfg structure! => we should better implement a getGlocalCfg()!
        wxEndBusyCursor();

        if (Grid* grid = const_cast<Grid*>(result.first)) //grid wasn't const when passing to findAndSelectNext(), so this is safe
        {
            assert(result.second >= 0);

            filegrid::setScrollMaster(*grid);
            grid->setGridCursor(result.second);

            focusWindowAfterSearch_ = &grid->getMainWin();

            if (!isComponentOf(wxWindow::FindFocus(), m_panelSearch))
                grid->getMainWin().SetFocus();
        }
        else
        {
            showFindPanel();
            showNotificationDialog(this, DialogInfoType::INFO, PopupDialogCfg().
                                   setTitle(_("Find")).
                                   setMainInstructions(replaceCpy(_("Cannot find %x"), L"%x", fmtPath(searchString))));
        }
    }
}


void MainDialog::OnTopFolderPairAdd(wxCommandEvent& event)
{

    insertAddFolderPair({ FolderPairEnh() }, 0);
    moveAddFolderPairUp(0);
}


void MainDialog::OnTopFolderPairRemove(wxCommandEvent& event)
{

    assert(!additionalFolderPairs_.empty());
    if (!additionalFolderPairs_.empty())
    {
        moveAddFolderPairUp(0);
        removeAddFolderPair(0);
    }
}


void MainDialog::OnLocalCompCfg(wxCommandEvent& event)
{
    const wxObject* const eventObj = event.GetEventObject(); //find folder pair originating the event
    for (auto it = additionalFolderPairs_.begin(); it != additionalFolderPairs_.end(); ++it)
        if (eventObj == (*it)->m_bpButtonAltCompCfg)
        {
            showConfigDialog(SyncConfigPanel::COMPARISON, (it - additionalFolderPairs_.begin()) + 1);
            break;
        }
}


void MainDialog::OnLocalSyncCfg(wxCommandEvent& event)
{
    const wxObject* const eventObj = event.GetEventObject(); //find folder pair originating the event
    for (auto it = additionalFolderPairs_.begin(); it != additionalFolderPairs_.end(); ++it)
        if (eventObj == (*it)->m_bpButtonAltSyncCfg)
        {
            showConfigDialog(SyncConfigPanel::SYNC, (it - additionalFolderPairs_.begin()) + 1);
            break;
        }
}


void MainDialog::OnLocalFilterCfg(wxCommandEvent& event)
{
    const wxObject* const eventObj = event.GetEventObject(); //find folder pair originating the event
    for (auto it = additionalFolderPairs_.begin(); it != additionalFolderPairs_.end(); ++it)
        if (eventObj == (*it)->m_bpButtonLocalFilter)
        {
            showConfigDialog(SyncConfigPanel::FILTER, (it - additionalFolderPairs_.begin()) + 1);
            break;
        }
}


void MainDialog::OnRemoveFolderPair(wxCommandEvent& event)
{

    const wxObject* const eventObj = event.GetEventObject(); //find folder pair originating the event
    for (auto it = additionalFolderPairs_.begin(); it != additionalFolderPairs_.end(); ++it)
        if (eventObj == (*it)->m_bpButtonRemovePair)
        {
            removeAddFolderPair(it - additionalFolderPairs_.begin());
            break;
        }
}


void MainDialog::OnShowFolderPairOptions(wxEvent& event)
{

    const wxObject* const eventObj = event.GetEventObject(); //find folder pair originating the event
    for (auto it = additionalFolderPairs_.begin(); it != additionalFolderPairs_.end(); ++it)
        if (eventObj == (*it)->m_bpButtonFolderPairOptions)
        {
            const ptrdiff_t pos = it - additionalFolderPairs_.begin();

            ContextMenu menu;
            menu.addItem(_("Add folder pair"), [this, pos] { insertAddFolderPair({ FolderPairEnh() },  pos); }, &getResourceImage(L"item_add_small"));
            menu.addSeparator();
            menu.addItem(_("Move up"  ) + L"\tAlt+Page Up",   [this, pos] { moveAddFolderPairUp(pos);     }, &getResourceImage(L"move_up_small"));
            menu.addItem(_("Move down") + L"\tAlt+Page Down", [this, pos] { moveAddFolderPairUp(pos + 1); }, &getResourceImage(L"move_down_small"), pos + 1 < makeSigned(additionalFolderPairs_.size()));

            wxPoint ctxPos = (*it)->m_bpButtonFolderPairOptions->GetPosition();
            ctxPos.x += (*it)->m_bpButtonFolderPairOptions->GetSize().GetWidth();
            menu.popup(*(*it)->m_panelLeft, ctxPos);
            break;
        }
}


void MainDialog::onTopFolderPairKeyEvent(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();

    if (event.AltDown())
        switch (keyCode)
        {
            case WXK_PAGEDOWN: //Alt + Page Down
            case WXK_NUMPAD_PAGEDOWN:
                if (!additionalFolderPairs_.empty())
                {
                    moveAddFolderPairUp(0);
                    additionalFolderPairs_[0]->m_folderPathLeft->SetFocus();
                }
                return;
        }

    event.Skip();
}


void MainDialog::onAddFolderPairKeyEvent(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();

    auto getAddFolderPairPos = [&]() -> ptrdiff_t //find folder pair originating the event
    {
        if (auto eventObj = dynamic_cast<const wxWindow*>(event.GetEventObject()))
            for (auto it = additionalFolderPairs_.begin(); it != additionalFolderPairs_.end(); ++it)
                if (isComponentOf(eventObj, *it))
                    return it - additionalFolderPairs_.begin();
        return -1;
    };

    if (event.AltDown())
        switch (keyCode)
        {
            case WXK_PAGEUP: //Alt + Page Up
            case WXK_NUMPAD_PAGEUP:
            {
                const ptrdiff_t pos = getAddFolderPairPos();
                if (pos >= 0)
                {
                    moveAddFolderPairUp(pos);
                    (pos == 0 ? m_folderPathLeft : additionalFolderPairs_[pos - 1]->m_folderPathLeft)->SetFocus();
                }
            }
            return;
            case WXK_PAGEDOWN: //Alt + Page Down
            case WXK_NUMPAD_PAGEDOWN:
            {
                const ptrdiff_t pos = getAddFolderPairPos();
                if (0 <= pos && pos + 1 < makeSigned(additionalFolderPairs_.size()))
                {
                    moveAddFolderPairUp(pos + 1);
                    additionalFolderPairs_[pos + 1]->m_folderPathLeft->SetFocus();
                }
            }
            return;
        }

    event.Skip();
}


void MainDialog::updateGuiForFolderPair()
{

    //adapt delete top folder pair button
    m_bpButtonRemovePair->Show(!additionalFolderPairs_.empty());
    m_panelTopLeft->Layout();

    //adapt local filter and sync cfg for first folder pair
    const bool showLocalCfgFirstPair = !additionalFolderPairs_.empty()                       ||
                                       firstFolderPair_->getAltCompConfig().get() != nullptr ||
                                       firstFolderPair_->getAltSyncConfig().get() != nullptr ||
                                       !isNullFilter(firstFolderPair_->getAltFilterConfig());
    //harmonize with MainDialog::showConfigDialog()!

    m_bpButtonAltCompCfg ->Show(showLocalCfgFirstPair);
    m_bpButtonAltSyncCfg ->Show(showLocalCfgFirstPair);
    m_bpButtonLocalFilter->Show(showLocalCfgFirstPair);
    setImage(*m_bpButtonSwapSides, getResourceImage(showLocalCfgFirstPair ? L"swap_slim" : L"swap"));

    //update sub-panel sizes for calculations below!!!
    m_panelTopCenter->GetSizer()->SetSizeHints(m_panelTopCenter); //~=Fit() + SetMinSize()

    int addPairMinimalHeight = 0;
    int addPairOptimalHeight = 0;
    if (!additionalFolderPairs_.empty())
    {
        const int pairHeight = additionalFolderPairs_[0]->GetSize().GetHeight();
        addPairMinimalHeight = std::min<double>(1.5, additionalFolderPairs_.size()) * pairHeight; //have 1.5 * height indicate that more folders are there
        addPairOptimalHeight = std::min<double>(globalCfg_.gui.mainDlg.maxFolderPairsVisible - 1 + 0.5, //subtract first/main folder pair and add 0.5 to indicate additional folders
                                                additionalFolderPairs_.size()) * pairHeight;

        addPairOptimalHeight = std::max(addPairOptimalHeight, addPairMinimalHeight); //implicitly handle corrupted values for "maxFolderPairsVisible"
    }

    const int firstPairHeight = std::max(m_panelDirectoryPairs->ClientToWindowSize(m_panelTopLeft  ->GetSize()).GetHeight(),  //include m_panelDirectoryPairs window borders!
                                         m_panelDirectoryPairs->ClientToWindowSize(m_panelTopCenter->GetSize()).GetHeight()); //

    //########################################################################################################################
    //wxAUI hack: set minimum height to desired value, then call wxAuiPaneInfo::Fixed() to apply it
    auiMgr_.GetPane(m_panelDirectoryPairs).MinSize(-1, firstPairHeight + addPairOptimalHeight);
    auiMgr_.GetPane(m_panelDirectoryPairs).Fixed();
    auiMgr_.Update();

    //now make resizable again
    auiMgr_.GetPane(m_panelDirectoryPairs).Resizable();
    auiMgr_.Update();
    //########################################################################################################################

    //make sure user cannot fully shrink additional folder pairs
    auiMgr_.GetPane(m_panelDirectoryPairs).MinSize(-1, firstPairHeight + addPairMinimalHeight);
    auiMgr_.Update();

    //it seems there is no GetSizer()->SetSizeHints(this)/Fit() required due to wxAui "magic"
    //=> *massive* perf improvement on OS X!
}


void MainDialog::insertAddFolderPair(const std::vector<FolderPairEnh>& newPairs, size_t pos)
{
    assert(pos <= additionalFolderPairs_.size() && additionalFolderPairs_.size() == bSizerAddFolderPairs->GetItemCount());
    pos = std::min(pos, additionalFolderPairs_.size());

    for (size_t i = 0; i < newPairs.size(); ++i)
    {
        FolderPairPanel* newPair = new FolderPairPanel(m_scrolledWindowFolderPairs, *this);

        //init dropdown history
        newPair->m_folderPathLeft ->init(folderHistoryLeft_);
        newPair->m_folderPathRight->init(folderHistoryRight_);

        //set width of left folder panel
        const int width = m_panelTopLeft->GetSize().GetWidth();
        newPair->m_panelLeft->SetMinSize(wxSize(width, -1));

        bSizerAddFolderPairs->Insert(pos, newPair, 0, wxEXPAND);
        additionalFolderPairs_.insert(additionalFolderPairs_.begin() + pos, newPair);

        //register events
        newPair->m_bpButtonFolderPairOptions->Connect(wxEVT_COMMAND_BUTTON_CLICKED,        wxEventHandler(MainDialog::OnShowFolderPairOptions), nullptr, this);
        newPair->m_bpButtonFolderPairOptions->Connect(wxEVT_RIGHT_DOWN,                    wxEventHandler(MainDialog::OnShowFolderPairOptions), nullptr, this);
        newPair->m_bpButtonRemovePair       ->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MainDialog::OnRemoveFolderPair     ), nullptr, this);
        static_cast<FolderPairPanelGenerated*>(newPair)->Connect(wxEVT_CHAR_HOOK,       wxKeyEventHandler(MainDialog::onAddFolderPairKeyEvent), nullptr, this);

        newPair->m_bpButtonAltCompCfg ->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MainDialog::OnLocalCompCfg  ), nullptr, this);
        newPair->m_bpButtonAltSyncCfg ->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MainDialog::OnLocalSyncCfg  ), nullptr, this);
        newPair->m_bpButtonLocalFilter->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MainDialog::OnLocalFilterCfg), nullptr, this);
    }

    updateGuiForFolderPair();

    //wxComboBox screws up miserably if width/height is smaller than the magic number 4! Problem occurs when trying to set tooltip
    //so we have to update window sizes before setting configuration:
    for (auto it = newPairs.begin(); it != newPairs.end(); ++it)//set alternate configuration
        additionalFolderPairs_[pos + (it - newPairs.begin())]->setValues(*it);
    clearGrid(); //+ GUI update
}


void MainDialog::moveAddFolderPairUp(size_t pos)
{
    assert(pos < additionalFolderPairs_.size());
    if (pos < additionalFolderPairs_.size())
    {
        const FolderPairEnh cfgTmp = additionalFolderPairs_[pos]->getValues();
        if (pos == 0)
        {
            additionalFolderPairs_[pos]->setValues(firstFolderPair_->getValues());
            firstFolderPair_->setValues(cfgTmp);
        }
        else
        {
            additionalFolderPairs_[pos]->setValues(additionalFolderPairs_[pos - 1]->getValues());
            additionalFolderPairs_[pos - 1]->setValues(cfgTmp);
        }

        //move comparison results, too!
        if (!folderCmp_.empty())
            std::swap(folderCmp_[pos], folderCmp_[pos + 1]); //invariant: folderCmp is empty or matches number of all folder pairs

        filegrid::getDataView(*m_gridMainC   ).setData(folderCmp_);
        treegrid::getDataView(*m_gridOverview).setData(folderCmp_);
        updateGui();
    }
}


void MainDialog::removeAddFolderPair(size_t pos)
{
    assert(pos < additionalFolderPairs_.size());
    if (pos < additionalFolderPairs_.size())
    {
        FolderPairPanel* panel = additionalFolderPairs_[pos];

        bSizerAddFolderPairs->Detach(panel); //Remove() does not work on wxWindow*, so do it manually
        additionalFolderPairs_.erase(additionalFolderPairs_.begin() + pos);
        //more (non-portable) wxWidgets bullshit: on OS X wxWindow::Destroy() screws up and calls "operator delete" directly rather than
        //the deferred deletion it is expected to do (and which is implemented correctly on Windows and Linux)
        //http://bb10.com/python-wxpython-devel/2012-09/msg00004.html
        //=> since we're in a mouse button callback of a sub-component of "panel" we need to delay deletion ourselves:
        guiQueue_.processAsync([] {}, [panel] { panel->Destroy(); });

        updateGuiForFolderPair();
        clearGrid(pos + 1); //+ GUI update
    }
}


void MainDialog::setAddFolderPairs(const std::vector<FolderPairEnh>& newPairs)
{

    additionalFolderPairs_.clear();
    bSizerAddFolderPairs->Clear(true);

    //updateGuiForFolderPair(); -> already called in insertAddFolderPair()
    insertAddFolderPair(newPairs, 0);
}


//########################################################################################################


//menu events
void MainDialog::OnMenuOptions(wxCommandEvent& event)
{
    showOptionsDlg(this, globalCfg_);
}


void MainDialog::OnMenuExportFileList(wxCommandEvent& event)
{
    //get a filepath
    wxFileDialog filePicker(this, //creating this on freestore leads to memleak!
                            wxString(),
                            wxString(),
                            L"FileList.csv", //default file name
                            _("Comma-separated values") + L" (*.csv)|*.csv" + L"|" +_("All files") + L" (*.*)|*",
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (filePicker.ShowModal() != wxID_OK)
        return;

    wxBusyCursor dummy;

    const Zstring filePath = utfTo<Zstring>(filePicker.GetPath());

    //http://en.wikipedia.org/wiki/Comma-separated_values
    const lconv* localInfo = ::localeconv(); //always bound according to doc
    const bool haveCommaAsDecimalSep = std::string(localInfo->decimal_point) == ",";

    const char CSV_SEP = haveCommaAsDecimalSep ? ';' : ',';

    auto fmtValue = [&](const std::wstring& val) -> std::string
    {
        std::string&& tmp = utfTo<std::string>(val);

        if (contains(tmp, CSV_SEP))
            return '\"' + tmp + '\"';
        else
            return std::move(tmp);
    };

    std::string header; //perf: wxString doesn't model exponential growth and so is out, std::string doesn't give performance guarantee!
    header += BYTE_ORDER_MARK_UTF8;

    //base folders
    header += fmtValue(_("Folder Pairs")) + LINE_BREAK;
    std::for_each(begin(folderCmp_), end(folderCmp_), [&](BaseFolderPair& baseFolder)
    {
        header += fmtValue(AFS::getDisplayPath(baseFolder.getAbstractPath< LEFT_SIDE>())) + CSV_SEP;
        header += fmtValue(AFS::getDisplayPath(baseFolder.getAbstractPath<RIGHT_SIDE>())) + LINE_BREAK;
    });
    header += LINE_BREAK;

    //write header
    auto provLeft   = m_gridMainL->getDataProvider();
    auto provCenter = m_gridMainC->getDataProvider();
    auto provRight  = m_gridMainR->getDataProvider();

    auto colAttrLeft   = m_gridMainL->getColumnConfig();
    auto colAttrCenter = m_gridMainC->getColumnConfig();
    auto colAttrRight  = m_gridMainR->getColumnConfig();

    erase_if(colAttrLeft,   [](const Grid::ColAttributes& ca) { return !ca.visible; });
    erase_if(colAttrCenter, [](const Grid::ColAttributes& ca) { return !ca.visible || static_cast<ColumnTypeCenter>(ca.type) == ColumnTypeCenter::CHECKBOX; });
    erase_if(colAttrRight,  [](const Grid::ColAttributes& ca) { return !ca.visible; });

    if (provLeft && provCenter && provRight)
    {
        for (const Grid::ColAttributes& ca : colAttrLeft)
        {
            header += fmtValue(provLeft->getColumnLabel(ca.type));
            header += CSV_SEP;
        }
        for (const Grid::ColAttributes& ca : colAttrCenter)
        {
            header += fmtValue(provCenter->getColumnLabel(ca.type));
            header += CSV_SEP;
        }
        if (!colAttrRight.empty())
        {
            std::for_each(colAttrRight.begin(), colAttrRight.end() - 1,
                          [&](const Grid::ColAttributes& ca)
            {
                header += fmtValue(provRight->getColumnLabel(ca.type));
                header += CSV_SEP;
            });
            header += fmtValue(provRight->getColumnLabel(colAttrRight.back().type));
        }
        header += LINE_BREAK;

        try
        {
            //write file
            FileOutput fileOut(filePath, FileOutput::ACC_OVERWRITE, nullptr /*notifyUnbufferedIO*/); //throw FileError

            fileOut.write(&*header.begin(), header.size()); //throw FileError, (X)
            //main grid: write rows one after the other instead of creating one big string: memory allocation might fail; think 1 million rows!
            /*
            performance test case "export 600.000 rows" to CSV:
            aproach 1. assemble single temporary string, then write file:   4.6s
            aproach 2. write to buffered file output directly for each row: 6.4s
            */
            std::string buffer;
            const size_t rowCount = m_gridMainL->getRowCount();
            for (size_t row = 0; row < rowCount; ++row)
            {
                for (const Grid::ColAttributes& ca : colAttrLeft)
                {
                    buffer += fmtValue(provLeft->getValue(row, ca.type));
                    buffer += CSV_SEP;
                }

                for (const Grid::ColAttributes& ca : colAttrCenter)
                {
                    buffer += fmtValue(provCenter->getValue(row, ca.type));
                    buffer += CSV_SEP;
                }

                for (const Grid::ColAttributes& ca : colAttrRight)
                {
                    buffer += fmtValue(provRight->getValue(row, ca.type));
                    buffer += CSV_SEP;
                }
                buffer += LINE_BREAK;

                fileOut.write(&buffer[0], buffer.size()); //throw FileError, (X)
                buffer.clear();
            }
            fileOut.finalize(); //throw FileError, (X)

            flashStatusInformation(_("File list exported"));
        }
        catch (const FileError& e)
        {
            showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
        }
    }
}


void MainDialog::OnMenuCheckVersion(wxCommandEvent& event)
{
    checkForUpdateNow(this, globalCfg_.gui.lastOnlineVersion);
}


void MainDialog::OnMenuUpdateAvailable(wxCommandEvent& event)
{
    checkForUpdateNow(this, globalCfg_.gui.lastOnlineVersion); //show changelog + handle Donation Edition auto-updater (including expiration)
}


void MainDialog::OnMenuCheckVersionAutomatically(wxCommandEvent& event)
{
    if (updateCheckActive(globalCfg_.gui.lastUpdateCheck))
        disableUpdateCheck(globalCfg_.gui.lastUpdateCheck);
    else
        globalCfg_.gui.lastUpdateCheck = 0; //reset to GlobalSettings.xml default value!

    m_menuItemCheckVersionAuto->Check(updateCheckActive(globalCfg_.gui.lastUpdateCheck));

    if (shouldRunAutomaticUpdateCheck(globalCfg_.gui.lastUpdateCheck))
    {
        flashStatusInformation(_("Searching for program updates..."));
        //synchronous update check is sufficient here:
        automaticUpdateCheckEval(this, globalCfg_.gui.lastUpdateCheck, globalCfg_.gui.lastOnlineVersion,
                                 automaticUpdateCheckRunAsync(automaticUpdateCheckPrepare().get()).get());
    }
}


void MainDialog::OnRegularUpdateCheck(wxIdleEvent& event)
{
    //execute just once per startup!
    Disconnect(wxEVT_IDLE, wxIdleEventHandler(MainDialog::OnRegularUpdateCheck), nullptr, this);

    if (shouldRunAutomaticUpdateCheck(globalCfg_.gui.lastUpdateCheck))
    {
        flashStatusInformation(_("Searching for program updates..."));

        std::shared_ptr<UpdateCheckResultPrep> resultPrep = automaticUpdateCheckPrepare(); //run on main thread:

        guiQueue_.processAsync([resultPrep] { return automaticUpdateCheckRunAsync(resultPrep.get()); }, //run on worker thread: (long-running part of the check)
                               [this] (std::shared_ptr<UpdateCheckResult>&& resultAsync)
        {
            automaticUpdateCheckEval(this, globalCfg_.gui.lastUpdateCheck, globalCfg_.gui.lastOnlineVersion,
                                     resultAsync.get()); //run on main thread:
        });
    }
}


void MainDialog::OnLayoutWindowAsync(wxIdleEvent& event)
{
    //execute just once per startup!
    Disconnect(wxEVT_IDLE, wxIdleEventHandler(MainDialog::OnLayoutWindowAsync), nullptr, this);


    //adjust folder pair distortion on startup
    for (FolderPairPanel* panel : additionalFolderPairs_)
        panel->Layout();

    m_panelTopButtons->Layout();
    Layout(); //strangely this layout call works if called in next idle event only
    auiMgr_.Update(); //fix view filter distortion
}


void MainDialog::OnMenuAbout(wxCommandEvent& event)
{
    showAboutDialog(this);
}


void MainDialog::OnShowHelp(wxCommandEvent& event)
{
    displayHelpEntry(L"freefilesync", this);
}

//#########################################################################################################

//language selection
void MainDialog::switchProgramLanguage(wxLanguage langId)
{
    //create new dialog with respect to new language
    XmlGlobalSettings newGlobalCfg = getGlobalCfgBeforeExit();
    newGlobalCfg.programLanguage = langId;

    //show new dialog, then delete old one
    MainDialog::create(globalConfigFilePath_, &newGlobalCfg, getConfig(), activeConfigFiles_, false);

    //we don't use Close():
    //1. we don't want to show the prompt to save current config in OnClose()
    //2. after getGlobalCfgBeforeExit() the old main dialog is invalid so we want to force deletion
    Destroy();
}


//#########################################################################################################

void MainDialog::setViewTypeSyncAction(bool value)
{
    //if (m_bpButtonViewTypeSyncAction->isActive() == value) return; support polling -> what about initialization?

    m_bpButtonViewTypeSyncAction->setActive(value);
    m_bpButtonViewTypeSyncAction->SetToolTip((value ? _("Action") : _("Category")) + L" (F10)");

    //toggle display of sync preview in middle grid
    filegrid::highlightSyncAction(*m_gridMainC, value);

    updateGui();
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef MAIN_DLG_H_8910481324545644545
#define MAIN_DLG_H_8910481324545644545

//#include <map>
#include <memory>
#include <wx+/async_task.h>
#include <wx+/file_drop.h>
#include <wx/aui/aui.h>
#include "gui_generated.h"
#include "folder_selector.h"
#include "file_grid.h"
#include "tree_grid.h"
#include "sync_cfg.h"
#include "log_panel.h"
#include "folder_history_box.h"
#include "../config.h"
#include "../status_handler.h"
#include "../base/algorithm.h"
//#include "../return_codes.h"
#include "../base/synchronization.h"


namespace fff
{
class FolderPairFirst;
class FolderPairPanel;
class CompareProgressPanel;
template <class GuiPanel> class FolderPairCallback;


class MainDialog : public MainDialogGenerated
{
public:
    //default behavior, application start, restores last used config
    static void create(const Zstring& globalConfigFilePath);

    //when loading dynamically assembled config,
    //when switching language,
    //or switching from batch run to GUI on warnings
    static void create(const Zstring& globalConfigFilePath,
                       const XmlGlobalSettings* globalSettings, //optional: take over ownership => save on exit
                       const XmlGuiConfig& guiCfg,
                       const std::vector<Zstring>& referenceFiles,
                       bool startComparison);

private:
    MainDialog(const Zstring& globalConfigFilePath,
               const XmlGuiConfig& guiCfg,
               const std::vector<Zstring>& referenceFiles,
               const XmlGlobalSettings& globalSettings); //take over ownership => save on exit
    ~MainDialog();

    void onBeforeSystemShutdown(); //last chance to do something useful before killing the application!

    friend class StatusHandlerTemporaryPanel;
    friend class StatusHandlerFloatingDialog;
    friend class FolderPairFirst;
    friend class FolderPairPanel;
    template <class GuiPanel>
    friend class FolderPairCallback;
    friend class PanelMoveWindow;

    class SingleOperationBlocker; //mitigate unwanted reentrancy caused by wxApp::Yield()

    //configuration load/save
    void setLastUsedConfig(const XmlGuiConfig& guiConfig, const std::vector<Zstring>& cfgFilePaths);

    XmlGuiConfig getConfig() const;
    void setConfig(const XmlGuiConfig& newGuiCfg, const std::vector<Zstring>& referenceFiles);

    void setGlobalCfgOnInit(const XmlGlobalSettings& globalSettings); //messes with Maximize(), window sizes, so call just once!
    XmlGlobalSettings getGlobalCfgBeforeExit(); //destructive "get" thanks to "Iconize(false), Maximize(false)"

    bool loadConfiguration(const std::vector<Zstring>& filepaths, bool ignoreBrokenConfig = false); //"false": error/cancel

    bool trySaveConfig     (const Zstring* guiCfgPath);   //
    bool trySaveBatchConfig(const Zstring* batchCfgPath); //"false": error/cancel
    bool saveOldConfig();                                 //

    void updateGlobalFilterButton();

    void setViewFilterDefault();

    void cfgHistoryRemoveObsolete(const std::vector<Zstring>& filepaths);
    void cfgHistoryUpdateNotes   (const std::vector<Zstring>& filepaths);

    void insertAddFolderPair(const std::vector<LocalPairConfig>& newPairs, size_t pos);
    void moveAddFolderPairUp(size_t pos);
    void removeAddFolderPair(size_t pos);
    void setAddFolderPairs(const std::vector<LocalPairConfig>& newPairs);

    void updateGuiForFolderPair(); //helper method: add usability by showing/hiding buttons related to folder pairs
    void recalcMaxFolderPairsVisible();

    //main method for putting gridDataView on UI: updates data respecting current view settings
    void updateGui(); //kitchen-sink update
    void updateGuiDelayedIf(bool condition); //400 ms delay

    void updateGridViewData();     //
    void updateStatistics(const SyncStatistics& st); // more fine-grained updaters
    void updateUnsavedCfgStatus(); //

    std::vector<std::wstring> getJobNames() const;

    //context menu functions
    std::vector<FileSystemObject*> getGridSelection(bool fromLeft = true, bool fromRight = true) const;
    std::vector<FileSystemObject*> getTreeSelection() const;

    void setSyncDirManually (const std::vector<FileSystemObject*>& selection, SyncDirection direction);
    void setIncludedManually(const std::vector<FileSystemObject*>& selection, bool setIncluded);
    void copyGridSelectionToClipboard(const zen::Grid& grid);
    void copyPathsToClipboard(const std::vector<FileSystemObject*>& selectionL,
                              const std::vector<FileSystemObject*>& selectionR);

    void copyToAlternateFolder(const std::vector<FileSystemObject*>& selectionL,
                               const std::vector<FileSystemObject*>& selectionR);

    void deleteSelectedFiles(const std::vector<FileSystemObject*>& selectionL,
                             const std::vector<FileSystemObject*>& selectionR, bool moveToRecycler);

    void renameSelectedFiles(const std::vector<FileSystemObject*>& selectionL,
                             const std::vector<FileSystemObject*>& selectionR);

    void openExternalApplication(const Zstring& commandLinePhrase, bool leftSide,
                                 const std::vector<FileSystemObject*>& selectionL,
                                 const std::vector<FileSystemObject*>& selectionR); //selection may be empty

    void setStatusBarFileStats(FileView::FileStats statsLeft, FileView::FileStats statsRight);

    void setStatusInfo(const wxString& text, bool highlight); //(permanently) set status bar center text
    void flashStatusInfo(const wxString& text); //temporarily show different status
    void popStatusInfo();

    //events
    void onGridKeyEvent(wxKeyEvent& event, zen::Grid& grid, bool leftSide);

    void onTreeKeyEvent    (wxKeyEvent& event);
    void onSetLayoutContext(wxMouseEvent& event);
    void onLocalKeyEvent   (wxKeyEvent& event);

    void applyCompareConfig(bool setDefaultViewType);

    //context menu handler methods
    void onGridContextRim(zen::GridContextMenuEvent& event, bool leftSide);

    void onGridGroupContextRim(zen::GridClickEvent& event, bool leftSide);

    void onGridContextRim(const std::vector<FileSystemObject*>& selection,
                          const std::vector<FileSystemObject*>& selectionL,
                          const std::vector<FileSystemObject*>& selectionR, bool leftSide, wxPoint mousePos);

    void onTreeGridContext(zen::GridContextMenuEvent& event);

    void onTreeGridSelection(zen::GridSelectEvent& event);

    void onDialogFilesDropped(zen::FileDropEvent& event);

    void onFolderSelected(wxCommandEvent& event);

    void onCheckRows       (CheckRowsEvent&     event);
    void onSetSyncDirection(SyncDirectionEvent& event);

    void swapSides();

    void onGridDoubleClickRim(zen::GridClickEvent& event, bool leftSide);

    void onGridLabelLeftClickRim(zen::GridLabelClickEvent& event, bool onLeft);
    void onGridLabelLeftClickC  (zen::GridLabelClickEvent& event);

    void onGridLabelContextRim(zen::GridLabelClickEvent& event, bool leftSide);
    void onGridLabelContextC  (zen::GridLabelClickEvent& event);

    void onToggleViewType  (wxCommandEvent& event) override;
    void onToggleViewButton(wxCommandEvent& event) override;

    void onViewTypeContextMouse  (wxMouseEvent&   event) override;
    void onViewFilterContext     (wxCommandEvent& event) override { onViewFilterContext(static_cast<wxEvent&>(event)); }
    void onViewFilterContextMouse(wxMouseEvent&   event) override { onViewFilterContext(static_cast<wxEvent&>(event)); }
    void onViewFilterContext(wxEvent& event);

    void onConfigNew     (wxCommandEvent& event) override;
    void onConfigSave    (wxCommandEvent& event) override;
    void onConfigSaveAs  (wxCommandEvent& event) override { trySaveConfig(nullptr); }
    void onSaveAsBatchJob(wxCommandEvent& event) override { trySaveBatchConfig(nullptr); }
    void onConfigLoad    (wxCommandEvent& event) override;

    void onCfgGridSelection  (zen::GridSelectEvent& event);
    void onCfgGridDoubleClick(zen::GridClickEvent& event);
    void onCfgGridKeyEvent            (wxKeyEvent& event);
    void onCfgGridContext       (zen::GridContextMenuEvent& event);
    void onCfgGridLabelContext  (zen::GridLabelClickEvent& event);
    void onCfgGridLabelLeftClick(zen::GridLabelClickEvent& event);

    void removeSelectedCfgHistoryItems(bool removeFromDisk);
    void renameSelectedCfgHistoryItem();

    void onStartupUpdateCheck(wxIdleEvent& event);
    void onLayoutWindowAsync (wxIdleEvent& event);

    void onResizeLeftFolderWidth(wxEvent& event);
    void onResizeTopButtonPanel (wxEvent& event);
    void onResizeConfigPanel    (wxEvent& event);
    void onResizeViewPanel      (wxEvent& event);
    void onToggleLog            (wxCommandEvent& event) override;
    void onCompare              (wxCommandEvent& event) override;
    void onStartSync            (wxCommandEvent& event) override;
    void onClose                (wxCloseEvent&   event) override;
    void onSwapSides            (wxCommandEvent& event) override { swapSides(); }

    void startSyncForSelecction(const std::vector<FileSystemObject*>& selection);

    void onCmpSettings    (wxCommandEvent& event) override { showConfigDialog(SyncConfigPanel::compare, -1); }
    void onSyncSettings   (wxCommandEvent& event) override { showConfigDialog(SyncConfigPanel::sync,    -1); }
    void onConfigureFilter(wxCommandEvent& event) override { showConfigDialog(SyncConfigPanel::filter,  -1); }

    void onCompSettingsContext     (wxCommandEvent& event) override { onCompSettingsContext(static_cast<wxEvent&>(event)); }
    void onCompSettingsContextMouse(wxMouseEvent&   event) override { onCompSettingsContext(static_cast<wxEvent&>(event)); }
    void onSyncSettingsContext     (wxCommandEvent& event) override { onSyncSettingsContext(static_cast<wxEvent&>(event)); }
    void onSyncSettingsContextMouse(wxMouseEvent&   event) override { onSyncSettingsContext(static_cast<wxEvent&>(event)); }
    void onGlobalFilterContext     (wxCommandEvent& event) override { onGlobalFilterContext(static_cast<wxEvent&>(event)); }
    void onGlobalFilterContextMouse(wxMouseEvent&   event) override { onGlobalFilterContext(static_cast<wxEvent&>(event)); }

    void onCompSettingsContext(wxEvent& event);
    void onSyncSettingsContext(wxEvent& event);
    void onGlobalFilterContext(wxEvent& event);

    void showConfigDialog(SyncConfigPanel panelToShow, int localPairIndexToShow);

    void setLastOperationLog(const ProcessSummary& summary, const std::shared_ptr<const zen::ErrorLog>& errorLog);
    void showLogPanel(bool show);

    void addFilterPhrase(const Zstring& phrase, bool include, bool requireNewLine);

    void onTopFolderPairAdd   (wxCommandEvent& event) override;
    void onTopFolderPairRemove(wxCommandEvent& event) override;
    void onRemoveFolderPair   (wxCommandEvent& event);
    void onShowFolderPairOptions(wxEvent& event);

    void onTopLocalCompCfg  (wxCommandEvent& event) override { showConfigDialog(SyncConfigPanel::compare, 0); }
    void onTopLocalSyncCfg  (wxCommandEvent& event) override { showConfigDialog(SyncConfigPanel::sync,       0); }
    void onTopLocalFilterCfg(wxCommandEvent& event) override { showConfigDialog(SyncConfigPanel::filter,     0); }

    void onLocalCompCfg  (wxCommandEvent& event);
    void onLocalSyncCfg  (wxCommandEvent& event);
    void onLocalFilterCfg(wxCommandEvent& event);

    void onTopFolderPairKeyEvent(wxKeyEvent& event);
    void onAddFolderPairKeyEvent(wxKeyEvent& event);

    void applyFilterConfig();
    void applySyncDirections();

    void showFindPanel(bool show); //CTRL + F
    void startFindNext(bool searchAscending); //F3

    void resetLayout();

    void onSearchGridEnter(wxCommandEvent& event) override;
    void onHideSearchPanel(wxCommandEvent& event) override;
    void onSearchPanelKeyPressed(wxKeyEvent& event);

    //menu events
    void onOpenMenuTools(wxMenuEvent& event);
    void onMenuOptions        (wxCommandEvent& event) override;
    void onMenuExportFileList (wxCommandEvent& event) override;
    void onMenuResetLayout    (wxCommandEvent& event) override { resetLayout(); }
    void onMenuFindItem       (wxCommandEvent& event) override { showFindPanel(true /*show*/); } //CTRL + F
    void onMenuCheckVersion   (wxCommandEvent& event) override;
    void onMenuAbout          (wxCommandEvent& event) override;
    void onShowHelp           (wxCommandEvent& event) override { wxLaunchDefaultBrowser(L"https://freefilesync.org/manual.php?topic=freefilesync"); }
    void onMenuQuit           (wxCommandEvent& event) override { Close(); }

    void switchProgramLanguage(wxLanguage langId);

    std::set<wxMenuItem*> detachedMenuItems_; //owning pointers!!!
    //alternatives: 1. std::set<unique_ptr<>>? key is const => no support for moving items out! 2. std::map<wxMenuItem*, unique_ptr<>>: redundant info, inconvenient use

    void clearGrid(ptrdiff_t pos = -1);

    //***********************************************
    //application variables are stored here:

    //global settings shared by GUI and batch mode
    XmlGlobalSettings globalCfg_;

    const Zstring globalConfigFilePath_;

    //-------------------------------------
    //program configuration
    XmlGuiConfig currentCfg_; //caveat: some parts are owned by GUI controls! see setConfig()

    //used when saving configuration
    std::vector<Zstring> activeConfigFiles_; //name of currently loaded config files: NOT owned by m_gridCfgHistory, see onCfgGridSelection()

    XmlGuiConfig lastSavedCfg_; //support for: "Save changed configuration?" dialog

    const Zstring lastRunConfigPath_ = getLastRunConfigPath(); //let's not use another global...
    //-------------------------------------

    //the prime data structure of this tool *bling*:
    FolderComparison folderCmp_; //optional!: sync button not available if empty

    //merge logs of individual steps (comparison, manual operations, sync) into a combined result (just as for ffs_batch jobs)
    struct FullSyncLog
    {
        zen::ErrorLog log;
        std::chrono::system_clock::time_point startTime;
        std::chrono::milliseconds totalTime{};
    };
    std::optional<FullSyncLog> fullSyncLog_;

    //folder pairs:
    std::unique_ptr<FolderPairFirst> firstFolderPair_; //always bound!!!
    std::vector<FolderPairPanel*> additionalFolderPairs_; //additional pairs to the first pair

    std::optional<double> addPairCountLast_;

    //-------------------------------------
    //fight sluggish GUI: FolderPairPanel are too expensive to casually throw away and recreate!
    struct DeleteWxWindow { void operator()(wxWindow* win) const { win->Destroy(); } };

    std::vector<std::unique_ptr<FolderPairPanel, DeleteWxWindow>> folderPairScrapyard_;
    //-------------------------------------

    //***********************************************
    //status bar center text
    std::vector<wxString> statusTxts_; //the first one is the original/non-flash status message
    bool statusTxtHighlightFirst_ = false;

    //compare status panel (hidden on start, shown during comparison)
    std::unique_ptr<CompareProgressPanel> compareStatus_; //always bound

    LogPanel* logPanel_ = nullptr;

    //toggle to display configuration preview instead of comparison result:
    //for read access use: m_bpButtonViewType->isActive()
    //when changing value use:
    void setGridViewType(GridViewType vt);

    wxAuiManager auiMgr_; //implement dockable GUI design

    wxString defaultPerspective_;

    time_t manualTimeSpanFrom_ = 0;
    time_t manualTimeSpanTo_   = 0; //buffer manual time span selection at session level

    //recreate view filter button labels only when necessary:
    std::unordered_map<const zen::ToggleButton*, int /*itemCount*/> buttonLabelItemCount_;

    const std::shared_ptr<HistoryList> folderHistoryLeft_;  //shared by all wxComboBox dropdown controls
    const std::shared_ptr<HistoryList> folderHistoryRight_; //

    zen::AsyncGuiQueue guiQueue_; //schedule and run long-running tasks asynchronously, but process results on GUI queue

    wxWindowID focusAfterCloseLog_    = wxID_ANY; //
    wxWindowID focusAfterCloseSearch_ = wxID_ANY; //restore focus after panel is closed
    //don't save wxWindow* to arbitrary window: might not exist anymore when hideFindPanel() uses it!!! (e.g. some folder pair panel)

    //mitigate reentrancy:
    bool localKeyEventsEnabled_ = true;
    bool operationInProgress_  = false; //see SingleOperationBlocker; e.g. do NOT allow dialog exit while sync is running => crash!!!

    TempFileBuffer tempFileBuf_; //buffer temporary copies of non-native files for %local_path%

    const wxImage imgTrashSmall_;
    const wxImage imgFileManagerSmall_;

    const zen::SharedRef<std::function<void()>> onBeforeSystemShutdownCookie_ = zen::makeSharedRef<std::function<void()>>([this] { onBeforeSystemShutdown(); });
};
}

#endif //MAIN_DLG_H_8910481324545644545

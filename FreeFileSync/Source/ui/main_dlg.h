// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef MAIN_DLG_H_8910481324545644545
#define MAIN_DLG_H_8910481324545644545

#include <map>
#include <memory>
#include <wx+/async_task.h>
#include <wx+/file_drop.h>
#include <wx/aui/aui.h>
#include "gui_generated.h"
#include "file_grid.h"
#include "tree_grid.h"
#include "sync_cfg.h"
#include "log_panel.h"
#include "folder_history_box.h"
#include "../base/status_handler.h"
#include "../base/algorithm.h"
#include "../base/return_codes.h"


namespace fff
{
class FolderPairFirst;
class FolderPairPanel;
class CompareProgressDialog;
template <class GuiPanel>
class FolderPairCallback;
class PanelMoveWindow;


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

    void disableAllElements(bool enableAbort); //dis-/enables all elements (except abort button) that might receive user input
    void enableAllElements();                  //during long-running processes: comparison, deletion

    void onQueryEndSession(); //last chance to do something useful before killing the application!

private:
    MainDialog(const Zstring& globalConfigFilePath,
               const XmlGuiConfig& guiCfg,
               const std::vector<Zstring>& referenceFiles,
               const XmlGlobalSettings& globalSettings, //take over ownership => save on exit
               bool startComparison);
    ~MainDialog();

    friend class StatusHandlerTemporaryPanel;
    friend class StatusHandlerFloatingDialog;
    friend class FolderPairFirst;
    friend class FolderPairPanel;
    template <class GuiPanel>
    friend class FolderPairCallback;
    friend class PanelMoveWindow;

    //configuration load/save
    void setLastUsedConfig(const XmlGuiConfig& guiConfig, const std::vector<Zstring>& cfgFilePaths);

    XmlGuiConfig getConfig() const;
    void setConfig(const XmlGuiConfig& newGuiCfg, const std::vector<Zstring>& referenceFiles);

    void setGlobalCfgOnInit(const XmlGlobalSettings& globalSettings); //messes with Maximize(), window sizes, so call just once!
    XmlGlobalSettings getGlobalCfgBeforeExit(); //destructive "get" thanks to "Iconize(false), Maximize(false)"

    bool loadConfiguration(const std::vector<Zstring>& filepaths); //return "true" if loaded successfully; "false" if cancelled or error

    bool trySaveConfig     (const Zstring* guiCfgPath); //return true if saved successfully
    bool trySaveBatchConfig(const Zstring* batchCfgPath); //
    bool saveOldConfig(); //return false on user abort

    void updateGlobalFilterButton();

    void initViewFilterButtons();
    void setViewFilterDefault();

    void cfgHistoryRemoveObsolete(const std::vector<Zstring>& filepaths);

    void insertAddFolderPair(const std::vector<LocalPairConfig>& newPairs, size_t pos);
    void moveAddFolderPairUp(size_t pos);
    void removeAddFolderPair(size_t pos);
    void setAddFolderPairs(const std::vector<LocalPairConfig>& newPairs);

    void updateGuiForFolderPair(); //helper method: add usability by showing/hiding buttons related to folder pairs
    void recalcMaxFolderPairsVisible();

    //main method for putting gridDataView on UI: updates data respecting current view settings
    void updateGui(); //kitchen-sink update
    void updateGuiDelayedIf(bool condition); // 400 ms delay

    void updateGridViewData();     //
    void updateStatistics();       // more fine-grained updaters
    void updateUnsavedCfgStatus(); //

    //context menu functions
    std::vector<FileSystemObject*> getGridSelection(bool fromLeft = true, bool fromRight = true) const;
    std::vector<FileSystemObject*> getTreeSelection() const;

    void setSyncDirManually(const std::vector<FileSystemObject*>& selection, SyncDirection direction);
    void setFilterManually (const std::vector<FileSystemObject*>& selection, bool setIncluded);
    void copySelectionToClipboard(const std::vector<const zen::Grid*>& gridRefs);

    void copyToAlternateFolder(const std::vector<FileSystemObject*>& selectionLeft,
                               const std::vector<FileSystemObject*>& selectionRight);

    void deleteSelectedFiles(const std::vector<FileSystemObject*>& selectionLeft,
                             const std::vector<FileSystemObject*>& selectionRight, bool moveToRecycler);

    void openExternalApplication(const Zstring& commandLinePhrase, bool leftSide,
                                 const std::vector<FileSystemObject*>& selectionLeft,
                                 const std::vector<FileSystemObject*>& selectionRight); //selection may be empty

    //status bar supports one of the following two states at a time:
    void setStatusBarFileStats(size_t fileCountLeft,
                               size_t folderCountLeft,
                               uint64_t bytesLeft,
                               size_t fileCountRight,
                               size_t folderCountRight,
                               uint64_t bytesRight);
    //void setStatusBarFullText(const wxString& msg);

    void flashStatusInformation(const wxString& msg); //temporarily show different status (only valid for setStatusBarFileStats)

    //events
    void onGridButtonEventL(wxKeyEvent& event) { onGridButtonEvent(event, *m_gridMainL,  true); }
    void onGridButtonEventC(wxKeyEvent& event) { onGridButtonEvent(event, *m_gridMainC,  true); }
    void onGridButtonEventR(wxKeyEvent& event) { onGridButtonEvent(event, *m_gridMainR, false); }
    void onGridButtonEvent (wxKeyEvent& event, zen::Grid& grid, bool leftSide);

    void onTreeButtonEvent (wxKeyEvent& event);
    void OnContextSetLayout(wxMouseEvent& event);
    void onLocalKeyEvent   (wxKeyEvent& event);

    void OnCompSettingsContext(wxMouseEvent&   event) override { OnCompSettingsContext(static_cast<wxEvent&>(event)); }
    void OnCompSettingsContext(wxCommandEvent& event) override { OnCompSettingsContext(static_cast<wxEvent&>(event)); }
    void OnSyncSettingsContext(wxMouseEvent&   event) override { OnSyncSettingsContext(static_cast<wxEvent&>(event)); }
    void OnSyncSettingsContext(wxCommandEvent& event) override { OnSyncSettingsContext(static_cast<wxEvent&>(event)); }
    void OnGlobalFilterContext(wxMouseEvent&   event) override { OnGlobalFilterContext(static_cast<wxEvent&>(event)); }
    void OnGlobalFilterContext(wxCommandEvent& event) override { OnGlobalFilterContext(static_cast<wxEvent&>(event)); }

    void OnCompSettingsContext(wxEvent& event);
    void OnSyncSettingsContext(wxEvent& event);
    void OnGlobalFilterContext(wxEvent& event);

    void OnViewFilterSave(wxCommandEvent& event) override;

    void applyCompareConfig(bool setDefaultViewType);

    //context menu handler methods
    void onMainGridContextL(zen::GridClickEvent& event);
    void onMainGridContextR(zen::GridClickEvent& event);
    void onMainGridContextRim(bool leftSide, zen::GridClickEvent& event);

    void onTreeGridContext(zen::GridClickEvent& event);

    void onTreeGridSelection(zen::GridSelectEvent& event);

    void onDialogFilesDropped(zen::FileDropEvent& event);

    void onDirSelected(wxCommandEvent& event);
    void onDirManualCorrection(wxCommandEvent& event);

    void onCheckRows       (CheckRowsEvent&     event);
    void onSetSyncDirection(SyncDirectionEvent& event);

    void onGridDoubleClickL(zen::GridClickEvent& event);
    void onGridDoubleClickR(zen::GridClickEvent& event);
    void onGridDoubleClickRim(size_t row, bool leftSide);

    void onGridLabelLeftClickL(zen::GridLabelClickEvent& event);
    void onGridLabelLeftClickC(zen::GridLabelClickEvent& event);
    void onGridLabelLeftClickR(zen::GridLabelClickEvent& event);
    void onGridLabelLeftClick(bool onLeft, ColumnTypeRim type);

    void onGridLabelContextL(zen::GridLabelClickEvent& event);
    void onGridLabelContextC(zen::GridLabelClickEvent& event);
    void onGridLabelContextR(zen::GridLabelClickEvent& event);
    void onGridLabelContextRim(zen::Grid& grid, ColumnTypeRim type, bool left);

    void OnToggleViewType  (wxCommandEvent& event) override;
    void OnToggleViewButton(wxCommandEvent& event) override;

    void OnConfigNew      (wxCommandEvent& event) override;
    void OnConfigSave     (wxCommandEvent& event) override;
    void OnConfigSaveAs   (wxCommandEvent& event) override;
    void OnSaveAsBatchJob (wxCommandEvent& event) override;
    void OnConfigLoad     (wxCommandEvent& event) override;

    void onCfgGridSelection  (zen::GridSelectEvent& event);
    void onCfgGridDoubleClick(zen::GridClickEvent& event);
    void onCfgGridKeyEvent            (wxKeyEvent& event);
    void onCfgGridContext       (zen::GridClickEvent& event);
    void onCfgGridLabelContext  (zen::GridLabelClickEvent& event);
    void onCfgGridLabelLeftClick(zen::GridLabelClickEvent& event);

    void deleteSelectedCfgHistoryItems();
    void renameSelectedCfgHistoryItem();

    void OnRegularUpdateCheck  (wxIdleEvent&  event);
    void OnLayoutWindowAsync   (wxIdleEvent&  event);

    void OnResizeLeftFolderWidth(wxEvent& event);
    void OnResizeTopButtonPanel (wxEvent& event);
    void OnResizeConfigPanel    (wxEvent& event);
    void OnResizeViewPanel      (wxEvent& event);
    void OnShowLog              (wxCommandEvent& event) override;
    void OnCompare              (wxCommandEvent& event) override;
    void OnStartSync            (wxCommandEvent& event) override;
    void OnSwapSides            (wxCommandEvent& event) override;
    void OnClose                (wxCloseEvent&   event) override;

    void startSyncForSelecction(const std::vector<FileSystemObject*>& selection);

    void OnCmpSettings    (wxCommandEvent& event) override { showConfigDialog(SyncConfigPanel::COMPARISON, -1); }
    void OnConfigureFilter(wxCommandEvent& event) override { showConfigDialog(SyncConfigPanel::FILTER,     -1); }
    void OnSyncSettings   (wxCommandEvent& event) override { showConfigDialog(SyncConfigPanel::SYNC,       -1); }

    void showConfigDialog(SyncConfigPanel panelToShow, int localPairIndexToShow);

    void updateConfigLastRunStats(time_t lastRunTime, SyncResult result, const AbstractPath& logFilePath);

    void setLastOperationLog(const ProcessSummary& summary, const std::shared_ptr<const zen::ErrorLog>& errorLog);
    void showLogPanel(bool show);

    void filterExtension(const Zstring& extension, bool include);
    void filterShortname(const FileSystemObject& fsObj, bool include);
    void filterItems(const std::vector<FileSystemObject*>& selection, bool include);
    void addFilterPhrase(const Zstring& phrase, bool include, bool requireNewLine);

    void OnTopFolderPairAdd   (wxCommandEvent& event) override;
    void OnTopFolderPairRemove(wxCommandEvent& event) override;
    void OnRemoveFolderPair   (wxCommandEvent& event);
    void OnShowFolderPairOptions(wxEvent& event);

    void OnTopLocalCompCfg  (wxCommandEvent& event) override { showConfigDialog(SyncConfigPanel::COMPARISON, 0); }
    void OnTopLocalSyncCfg  (wxCommandEvent& event) override { showConfigDialog(SyncConfigPanel::SYNC,       0); }
    void OnTopLocalFilterCfg(wxCommandEvent& event) override { showConfigDialog(SyncConfigPanel::FILTER,     0); }

    void OnLocalCompCfg  (wxCommandEvent& event);
    void OnLocalSyncCfg  (wxCommandEvent& event);
    void OnLocalFilterCfg(wxCommandEvent& event);

    void onTopFolderPairKeyEvent(wxKeyEvent& event);
    void onAddFolderPairKeyEvent(wxKeyEvent& event);

    void applyFilterConfig();
    void applySyncDirections();

    void showFindPanel(); //CTRL + F
    void hideFindPanel();
    void startFindNext(bool searchAscending); //F3

    void resetLayout();

    void OnSearchGridEnter(wxCommandEvent& event) override;
    void OnHideSearchPanel(wxCommandEvent& event) override;
    void OnSearchPanelKeyPressed(wxKeyEvent& event);

    //menu events
    void onOpenMenuTools(wxMenuEvent& event);
    void OnMenuOptions        (wxCommandEvent& event) override;
    void OnMenuExportFileList (wxCommandEvent& event) override;
    void OnMenuResetLayout    (wxCommandEvent& event) override { resetLayout(); }
    void OnMenuFindItem       (wxCommandEvent& event) override;
    void OnMenuCheckVersion   (wxCommandEvent& event) override;
    void OnMenuCheckVersionAutomatically(wxCommandEvent& event) override;
    void OnMenuUpdateAvailable(wxCommandEvent& event);
    void OnMenuAbout          (wxCommandEvent& event) override;
    void OnShowHelp           (wxCommandEvent& event) override;
    void OnMenuQuit           (wxCommandEvent& event) override { Close(); }

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

    const Zstring lastRunConfigPath_ = getLastRunConfigPath(); //let's not use another static...
    //-------------------------------------

    //the prime data structure of this tool *bling*:
    FolderComparison folderCmp_; //optional!: sync button not available if empty

    //folder pairs:
    std::unique_ptr<FolderPairFirst> firstFolderPair_; //always bound!!!
    std::vector<FolderPairPanel*> additionalFolderPairs_; //additional pairs to the first pair

    std::optional<double> addPairCountLast_;
    //-------------------------------------

    //***********************************************
    //status information
    std::vector<wxString> oldStatusMsgs_; //the first one is the original/non-flash status message

    //compare status panel (hidden on start, shown when comparing)
    std::unique_ptr<CompareProgressDialog> compareStatus_; //always bound

    LogPanel* logPanel_ = nullptr;

    //toggle to display configuration preview instead of comparison result:
    //for read access use: m_bpButtonViewTypeSyncAction->isActive()
    //when changing value use:
    void setViewTypeSyncAction(bool value);

    wxAuiManager auiMgr_; //implement dockable GUI design

    wxString defaultPerspective_;

    time_t manualTimeSpanFrom_ = 0;
    time_t manualTimeSpanTo_   = 0; //buffer manual time span selection at session level

    zen::SharedRef<FolderHistory> folderHistoryLeft_  = zen::makeSharedRef<FolderHistory>(); //shared by all wxComboBox dropdown controls
    zen::SharedRef<FolderHistory> folderHistoryRight_ = zen::makeSharedRef<FolderHistory>(); //always bound!

    zen::AsyncGuiQueue guiQueue_; //schedule and run long-running tasks asynchronously, but process results on GUI queue

    std::unique_ptr<FilterConfig> filterCfgOnClipboard_; //copy/paste of filter config

    wxWindowID focusIdAfterSearch_ = wxID_ANY; //used to restore focus after search panel is closed

    bool localKeyEventsEnabled_ = true;
    bool allowMainDialogClose_ = true; //e.g. do NOT allow close while sync is running => crash!!!

    TempFileBuffer tempFileBuf_; //buffer temporary copies of non-native files for %local_path%
};
}

#endif //MAIN_DLG_H_8910481324545644545

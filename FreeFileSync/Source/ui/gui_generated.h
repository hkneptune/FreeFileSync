///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-0-g8feb16b3)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
namespace zen{ class BitmapTextButton; }
namespace zen{ class ToggleButton; }

#include "wx+/bitmap_button.h"
#include "folder_history_box.h"
#include "wx+/grid.h"
#include "triple_splitter.h"
#include "wx+/toggle_button.h"
#include "command_box.h"
#include "wx+/graph.h"
#include <wx/string.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/menu.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/bmpbuttn.h>
#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/scrolwin.h>
#include <wx/statbmp.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/frame.h>
#include <wx/listbox.h>
#include <wx/radiobut.h>
#include <wx/hyperlink.h>
#include <wx/spinctrl.h>
#include <wx/choice.h>
#include <wx/notebook.h>
#include <wx/dialog.h>
#include <wx/tglbtn.h>
#include <wx/treectrl.h>
#include <wx/checklst.h>
#include <wx/grid.h>
#include <wx/calctrl.h>
#include <wx/gauge.h>
#include <wx/richtext/richtextctrl.h>

#include "zen/i18n.h"

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class MainDialogGenerated
///////////////////////////////////////////////////////////////////////////////
class MainDialogGenerated : public wxFrame
{
	private:

	protected:
		wxMenuBar* m_menubar;
		wxMenu* m_menuFile;
		wxMenuItem* m_menuItemNew;
		wxMenuItem* m_menuItemLoad;
		wxMenuItem* m_menuItemSave;
		wxMenuItem* m_menuItemSaveAs;
		wxMenuItem* m_menuItemSaveAsBatch;
		wxMenuItem* m_menuItemQuit;
		wxMenu* m_menuActions;
		wxMenuItem* m_menuItemShowLog;
		wxMenuItem* m_menuItemCompare;
		wxMenuItem* m_menuItemCompSettings;
		wxMenuItem* m_menuItemFilter;
		wxMenuItem* m_menuItemSyncSettings;
		wxMenuItem* m_menuItemSynchronize;
		wxMenu* m_menuTools;
		wxMenuItem* m_menuItemOptions;
		wxMenu* m_menuLanguages;
		wxMenuItem* m_menuItemFind;
		wxMenuItem* m_menuItemExportList;
		wxMenuItem* m_menuItemResetLayout;
		wxMenuItem* m_menuItemShowMain;
		wxMenuItem* m_menuItemShowFolders;
		wxMenuItem* m_menuItemShowViewFilter;
		wxMenuItem* m_menuItemShowConfig;
		wxMenuItem* m_menuItemShowOverview;
		wxMenu* m_menuHelp;
		wxMenuItem* m_menuItemHelp;
		wxMenuItem* m_menuItemCheckVersionNow;
		wxMenuItem* m_menuItemAbout;
		wxBoxSizer* bSizerPanelHolder;
		wxPanel* m_panelTopButtons;
		wxBoxSizer* bSizerTopButtons;
		wxButton* m_buttonCancel;
		zen::BitmapTextButton* m_buttonCompare;
		wxBitmapButton* m_bpButtonCmpConfig;
		wxBitmapButton* m_bpButtonCmpContext;
		wxBitmapButton* m_bpButtonFilter;
		wxBitmapButton* m_bpButtonFilterContext;
		wxBitmapButton* m_bpButtonSyncConfig;
		wxBitmapButton* m_bpButtonSyncContext;
		zen::BitmapTextButton* m_buttonSync;
		wxPanel* m_panelDirectoryPairs;
		wxPanel* m_panelTopLeft;
		wxStaticText* m_staticTextResolvedPathL;
		wxBitmapButton* m_bpButtonAddPair;
		fff::FolderHistoryBox* m_folderPathLeft;
		wxButton* m_buttonSelectFolderLeft;
		wxBitmapButton* m_bpButtonSelectAltFolderLeft;
		wxPanel* m_panelTopCenter;
		wxBitmapButton* m_bpButtonSwapSides;
		wxPanel* m_panelTopRight;
		wxStaticText* m_staticTextResolvedPathR;
		fff::FolderHistoryBox* m_folderPathRight;
		wxButton* m_buttonSelectFolderRight;
		wxBitmapButton* m_bpButtonSelectAltFolderRight;
		wxScrolledWindow* m_scrolledWindowFolderPairs;
		wxBoxSizer* bSizerAddFolderPairs;
		zen::Grid* m_gridOverview;
		wxPanel* m_panelCenter;
		fff::TripleSplitter* m_splitterMain;
		zen::Grid* m_gridMainL;
		zen::Grid* m_gridMainC;
		zen::Grid* m_gridMainR;
		wxPanel* m_panelStatusBar;
		wxBoxSizer* bSizerStatusLeftDirectories;
		wxStaticBitmap* m_bitmapSmallDirectoryLeft;
		wxStaticText* m_staticTextStatusLeftDirs;
		wxBoxSizer* bSizerStatusLeftFiles;
		wxStaticBitmap* m_bitmapSmallFileLeft;
		wxStaticText* m_staticTextStatusLeftFiles;
		wxStaticText* m_staticTextStatusLeftBytes;
		wxStaticText* m_staticTextStatusCenter;
		wxBoxSizer* bSizerStatusRightDirectories;
		wxStaticBitmap* m_bitmapSmallDirectoryRight;
		wxStaticText* m_staticTextStatusRightDirs;
		wxBoxSizer* bSizerStatusRightFiles;
		wxStaticBitmap* m_bitmapSmallFileRight;
		wxStaticText* m_staticTextStatusRightFiles;
		wxStaticText* m_staticTextStatusRightBytes;
		wxPanel* m_panelSearch;
		wxBitmapButton* m_bpButtonHideSearch;
		wxTextCtrl* m_textCtrlSearchTxt;
		wxCheckBox* m_checkBoxMatchCase;
		wxPanel* m_panelLog;
		wxBoxSizer* bSizerLog;
		wxStaticBitmap* m_bitmapSyncResult;
		wxStaticText* m_staticTextSyncResult;
		wxStaticText* m_staticTextProcessed;
		wxStaticText* m_staticTextRemaining;
		wxPanel* m_panelItemStats;
		wxStaticBitmap* m_bitmapItemStat;
		wxStaticText* m_staticTextItemsProcessed;
		wxStaticText* m_staticTextBytesProcessed;
		wxStaticText* m_staticTextItemsRemaining;
		wxStaticText* m_staticTextBytesRemaining;
		wxPanel* m_panelTimeStats;
		wxStaticBitmap* m_bitmapTimeStat;
		wxStaticText* m_staticTextTimeElapsed;
		wxPanel* m_panelConfig;
		wxBoxSizer* bSizerConfig;
		wxBoxSizer* bSizerCfgHistoryButtons;
		wxBitmapButton* m_bpButtonNew;
		wxBitmapButton* m_bpButtonOpen;
		wxBitmapButton* m_bpButtonSave;
		wxBoxSizer* bSizerSaveAs;
		wxBitmapButton* m_bpButtonSaveAs;
		wxBitmapButton* m_bpButtonSaveAsBatch;
		zen::Grid* m_gridCfgHistory;
		wxPanel* m_panelViewFilter;
		wxBoxSizer* bSizerViewFilter;
		zen::ToggleButton* m_bpButtonToggleLog;
		wxBoxSizer* bSizerViewButtons;
		zen::ToggleButton* m_bpButtonViewType;
		zen::ToggleButton* m_bpButtonShowExcluded;
		zen::ToggleButton* m_bpButtonShowDeleteLeft;
		zen::ToggleButton* m_bpButtonShowUpdateLeft;
		zen::ToggleButton* m_bpButtonShowCreateLeft;
		zen::ToggleButton* m_bpButtonShowLeftOnly;
		zen::ToggleButton* m_bpButtonShowLeftNewer;
		zen::ToggleButton* m_bpButtonShowEqual;
		zen::ToggleButton* m_bpButtonShowDoNothing;
		zen::ToggleButton* m_bpButtonShowDifferent;
		zen::ToggleButton* m_bpButtonShowRightNewer;
		zen::ToggleButton* m_bpButtonShowRightOnly;
		zen::ToggleButton* m_bpButtonShowCreateRight;
		zen::ToggleButton* m_bpButtonShowUpdateRight;
		zen::ToggleButton* m_bpButtonShowDeleteRight;
		zen::ToggleButton* m_bpButtonShowConflict;
		wxBitmapButton* m_bpButtonViewFilterContext;
		wxPanel* m_panelStatistics;
		wxBoxSizer* bSizerStatistics;
		wxStaticBitmap* m_bitmapDeleteLeft;
		wxStaticText* m_staticTextDeleteLeft;
		wxStaticBitmap* m_bitmapUpdateLeft;
		wxStaticText* m_staticTextUpdateLeft;
		wxStaticBitmap* m_bitmapCreateLeft;
		wxStaticText* m_staticTextCreateLeft;
		wxStaticBitmap* m_bitmapData;
		wxStaticText* m_staticTextData;
		wxStaticBitmap* m_bitmapCreateRight;
		wxStaticText* m_staticTextCreateRight;
		wxStaticBitmap* m_bitmapUpdateRight;
		wxStaticText* m_staticTextUpdateRight;
		wxStaticBitmap* m_bitmapDeleteRight;
		wxStaticText* m_staticTextDeleteRight;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onConfigNew( wxCommandEvent& event ) { event.Skip(); }
		virtual void onConfigLoad( wxCommandEvent& event ) { event.Skip(); }
		virtual void onConfigSave( wxCommandEvent& event ) { event.Skip(); }
		virtual void onConfigSaveAs( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSaveAsBatchJob( wxCommandEvent& event ) { event.Skip(); }
		virtual void onMenuQuit( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleLog( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCompare( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCmpSettings( wxCommandEvent& event ) { event.Skip(); }
		virtual void onConfigureFilter( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSyncSettings( wxCommandEvent& event ) { event.Skip(); }
		virtual void onStartSync( wxCommandEvent& event ) { event.Skip(); }
		virtual void onMenuOptions( wxCommandEvent& event ) { event.Skip(); }
		virtual void onMenuFindItem( wxCommandEvent& event ) { event.Skip(); }
		virtual void onMenuExportFileList( wxCommandEvent& event ) { event.Skip(); }
		virtual void onMenuResetLayout( wxCommandEvent& event ) { event.Skip(); }
		virtual void onShowHelp( wxCommandEvent& event ) { event.Skip(); }
		virtual void onMenuCheckVersion( wxCommandEvent& event ) { event.Skip(); }
		virtual void onMenuAbout( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCompSettingsContextMouse( wxMouseEvent& event ) { event.Skip(); }
		virtual void onCompSettingsContext( wxCommandEvent& event ) { event.Skip(); }
		virtual void onGlobalFilterContextMouse( wxMouseEvent& event ) { event.Skip(); }
		virtual void onGlobalFilterContext( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSyncSettingsContextMouse( wxMouseEvent& event ) { event.Skip(); }
		virtual void onSyncSettingsContext( wxCommandEvent& event ) { event.Skip(); }
		virtual void onTopFolderPairAdd( wxCommandEvent& event ) { event.Skip(); }
		virtual void onTopFolderPairRemove( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSwapSides( wxCommandEvent& event ) { event.Skip(); }
		virtual void onTopLocalCompCfg( wxCommandEvent& event ) { event.Skip(); }
		virtual void onTopLocalFilterCfg( wxCommandEvent& event ) { event.Skip(); }
		virtual void onTopLocalSyncCfg( wxCommandEvent& event ) { event.Skip(); }
		virtual void onHideSearchPanel( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSearchGridEnter( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleViewType( wxCommandEvent& event ) { event.Skip(); }
		virtual void onViewTypeContextMouse( wxMouseEvent& event ) { event.Skip(); }
		virtual void onToggleViewButton( wxCommandEvent& event ) { event.Skip(); }
		virtual void onViewFilterContextMouse( wxMouseEvent& event ) { event.Skip(); }
		virtual void onViewFilterContext( wxCommandEvent& event ) { event.Skip(); }


	public:
		wxBitmapButton* m_bpButtonRemovePair;
		wxBitmapButton* m_bpButtonLocalCompCfg;
		wxBitmapButton* m_bpButtonLocalFilter;
		wxBitmapButton* m_bpButtonLocalSyncCfg;

		MainDialogGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("dummy"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL );

		~MainDialogGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class FolderPairPanelGenerated
///////////////////////////////////////////////////////////////////////////////
class FolderPairPanelGenerated : public wxPanel
{
	private:

	protected:
		wxButton* m_buttonSelectFolderLeft;
		wxBitmapButton* m_bpButtonSelectAltFolderLeft;
		wxPanel* m_panelRight;
		wxButton* m_buttonSelectFolderRight;
		wxBitmapButton* m_bpButtonSelectAltFolderRight;

	public:
		wxPanel* m_panelLeft;
		wxBitmapButton* m_bpButtonFolderPairOptions;
		wxBitmapButton* m_bpButtonRemovePair;
		fff::FolderHistoryBox* m_folderPathLeft;
		wxBitmapButton* m_bpButtonLocalCompCfg;
		wxBitmapButton* m_bpButtonLocalFilter;
		wxBitmapButton* m_bpButtonLocalSyncCfg;
		fff::FolderHistoryBox* m_folderPathRight;

		FolderPairPanelGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 698,67 ), long style = 0, const wxString& name = wxEmptyString );

		~FolderPairPanelGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class ConfigDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class ConfigDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticText* m_staticTextFolderPairLabel;
		wxListBox* m_listBoxFolderPair;
		wxNotebook* m_notebook;
		wxPanel* m_panelCompSettingsTab;
		wxBoxSizer* bSizerHeaderCompSettings;
		wxStaticText* m_staticTextMainCompSettings;
		wxCheckBox* m_checkBoxUseLocalCmpOptions;
		wxStaticLine* m_staticlineCompHeader;
		wxPanel* m_panelComparisonSettings;
		zen::ToggleButton* m_buttonByTimeSize;
		zen::ToggleButton* m_buttonByContent;
		zen::ToggleButton* m_buttonBySize;
		wxStaticBitmap* m_bitmapCompVariant;
		wxStaticText* m_staticTextCompVarDescription;
		wxCheckBox* m_checkBoxSymlinksInclude;
		wxRadioButton* m_radioBtnSymlinksFollow;
		wxRadioButton* m_radioBtnSymlinksDirect;
		wxTextCtrl* m_textCtrlTimeShift;
		wxBoxSizer* bSizerCompMisc;
		wxStaticBitmap* m_bitmapIgnoreErrors;
		wxCheckBox* m_checkBoxIgnoreErrors;
		wxStaticBitmap* m_bitmapRetryErrors;
		wxCheckBox* m_checkBoxAutoRetry;
		wxFlexGridSizer* fgSizerAutoRetry;
		wxStaticText* m_staticTextAutoRetryDelay;
		wxSpinCtrl* m_spinCtrlAutoRetryCount;
		wxSpinCtrl* m_spinCtrlAutoRetryDelay;
		wxBoxSizer* bSizerPerformance;
		wxPanel* m_panel57;
		wxStaticBitmap* m_bitmapPerf;
		wxHyperlinkCtrl* m_hyperlinkPerfDeRequired;
		wxBoxSizer* bSizer260;
		wxStaticText* m_staticTextPerfParallelOps;
		wxScrolledWindow* m_scrolledWindowPerf;
		wxFlexGridSizer* fgSizerPerf;
		wxPanel* m_panelFilterSettingsTab;
		wxBoxSizer* bSizerHeaderFilterSettings;
		wxStaticText* m_staticTextMainFilterSettings;
		wxStaticText* m_staticTextLocalFilterSettings;
		wxStaticLine* m_staticlineFilterHeader;
		wxStaticBitmap* m_bitmapInclude;
		wxTextCtrl* m_textCtrlInclude;
		wxStaticBitmap* m_bitmapExclude;
		wxTextCtrl* m_textCtrlExclude;
		wxStaticBitmap* m_bitmapFilterSize;
		wxSpinCtrl* m_spinCtrlMinSize;
		wxChoice* m_choiceUnitMinSize;
		wxSpinCtrl* m_spinCtrlMaxSize;
		wxChoice* m_choiceUnitMaxSize;
		wxStaticBitmap* m_bitmapFilterDate;
		wxChoice* m_choiceUnitTimespan;
		wxSpinCtrl* m_spinCtrlTimespan;
		wxStaticText* m_staticTextFilterDescr;
		wxButton* m_buttonDefault;
		wxBitmapButton* m_bpButtonDefaultContext;
		wxButton* m_buttonClear;
		wxPanel* m_panelSyncSettingsTab;
		wxBoxSizer* bSizerHeaderSyncSettings;
		wxStaticText* m_staticTextMainSyncSettings;
		wxCheckBox* m_checkBoxUseLocalSyncOptions;
		wxStaticLine* m_staticlineSyncHeader;
		wxPanel* m_panelSyncSettings;
		zen::ToggleButton* m_buttonTwoWay;
		zen::ToggleButton* m_buttonMirror;
		zen::ToggleButton* m_buttonUpdate;
		zen::ToggleButton* m_buttonCustom;
		wxStaticBitmap* m_bitmapDatabase;
		wxCheckBox* m_checkBoxUseDatabase;
		wxStaticText* m_staticTextSyncVarDescription;
		wxStaticBitmap* m_bitmapMoveLeft;
		wxStaticBitmap* m_bitmapMoveRight;
		wxStaticText* m_staticTextDetectMove;
		wxBoxSizer* bSizerSyncDirHolder;
		wxBoxSizer* bSizerSyncDirsDiff;
		wxStaticBitmap* m_bitmapLeftOnly;
		wxStaticBitmap* m_bitmapLeftNewer;
		wxStaticBitmap* m_bitmapDifferent;
		wxStaticBitmap* m_bitmapRightNewer;
		wxStaticBitmap* m_bitmapRightOnly;
		wxBitmapButton* m_bpButtonLeftOnly;
		wxBitmapButton* m_bpButtonLeftNewer;
		wxBitmapButton* m_bpButtonDifferent;
		wxBitmapButton* m_bpButtonRightNewer;
		wxBitmapButton* m_bpButtonRightOnly;
		wxBoxSizer* bSizerSyncDirsChanges;
		wxBitmapButton* m_bpButtonLeftCreate;
		wxBitmapButton* m_bpButtonRightCreate;
		wxBitmapButton* m_bpButtonLeftUpdate;
		wxBitmapButton* m_bpButtonRightUpdate;
		wxBitmapButton* m_bpButtonLeftDelete;
		wxBitmapButton* m_bpButtonRightDelete;
		zen::ToggleButton* m_buttonRecycler;
		zen::ToggleButton* m_buttonPermanent;
		zen::ToggleButton* m_buttonVersioning;
		wxBoxSizer* bSizerVersioningHolder;
		wxStaticBitmap* m_bitmapDeletionType;
		wxStaticText* m_staticTextDeletionTypeDescription;
		wxPanel* m_panelVersioning;
		wxStaticBitmap* m_bitmapVersioning;
		fff::FolderHistoryBox* m_versioningFolderPath;
		wxButton* m_buttonSelectVersioningFolder;
		wxBitmapButton* m_bpButtonSelectVersioningAltFolder;
		wxChoice* m_choiceVersioningStyle;
		wxStaticText* m_staticTextNamingCvtPart1;
		wxStaticText* m_staticTextNamingCvtPart2Bold;
		wxStaticText* m_staticTextNamingCvtPart3;
		wxStaticText* m_staticTextLimitVersions;
		wxCheckBox* m_checkBoxVersionMaxDays;
		wxCheckBox* m_checkBoxVersionCountMin;
		wxCheckBox* m_checkBoxVersionCountMax;
		wxSpinCtrl* m_spinCtrlVersionMaxDays;
		wxSpinCtrl* m_spinCtrlVersionCountMin;
		wxSpinCtrl* m_spinCtrlVersionCountMax;
		wxBoxSizer* bSizerSyncMisc;
		wxStaticBitmap* m_bitmapEmail;
		wxCheckBox* m_checkBoxSendEmail;
		fff::CommandBox* m_comboBoxEmail;
		wxBitmapButton* m_bpButtonEmailAlways;
		wxBitmapButton* m_bpButtonEmailErrorWarning;
		wxBitmapButton* m_bpButtonEmailErrorOnly;
		wxHyperlinkCtrl* m_hyperlinkPerfDeRequired2;
		wxPanel* m_panelLogfile;
		wxStaticBitmap* m_bitmapLogFile;
		wxCheckBox* m_checkBoxOverrideLogPath;
		wxBitmapButton* m_bpButtonShowLogFolder;
		fff::FolderHistoryBox* m_logFolderPath;
		wxButton* m_buttonSelectLogFolder;
		wxBitmapButton* m_bpButtonSelectAltLogFolder;
		wxStaticText* m_staticTextPostSync;
		wxChoice* m_choicePostSyncCondition;
		fff::CommandBox* m_comboBoxPostSyncCommand;
		wxPanel* m_panelNotes;
		wxStaticBitmap* m_bitmapNotes;
		wxTextCtrl* m_textCtrNotes;
		wxBoxSizer* bSizerStdButtons;
		zen::BitmapTextButton* m_buttonAddNotes;
		wxButton* m_buttonOK;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onListBoxKeyEvent( wxKeyEvent& event ) { event.Skip(); }
		virtual void onSelectFolderPair( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleLocalCompSettings( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCompByTimeSize( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCompByTimeSizeDouble( wxMouseEvent& event ) { event.Skip(); }
		virtual void onCompByContent( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCompByContentDouble( wxMouseEvent& event ) { event.Skip(); }
		virtual void onCompBySize( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCompBySizeDouble( wxMouseEvent& event ) { event.Skip(); }
		virtual void onChangeCompOption( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleIgnoreErrors( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleAutoRetry( wxCommandEvent& event ) { event.Skip(); }
		virtual void onChangeFilterOption( wxCommandEvent& event ) { event.Skip(); }
		virtual void onFilterDefault( wxCommandEvent& event ) { event.Skip(); }
		virtual void onFilterDefaultContextMouse( wxMouseEvent& event ) { event.Skip(); }
		virtual void onFilterDefaultContext( wxCommandEvent& event ) { event.Skip(); }
		virtual void onFilterClear( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleLocalSyncSettings( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSyncTwoWay( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSyncTwoWayDouble( wxMouseEvent& event ) { event.Skip(); }
		virtual void onSyncMirror( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSyncMirrorDouble( wxMouseEvent& event ) { event.Skip(); }
		virtual void onSyncUpdate( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSyncUpdateDouble( wxMouseEvent& event ) { event.Skip(); }
		virtual void onSyncCustom( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSyncCustomDouble( wxMouseEvent& event ) { event.Skip(); }
		virtual void onToggleUseDatabase( wxCommandEvent& event ) { event.Skip(); }
		virtual void onLeftOnly( wxCommandEvent& event ) { event.Skip(); }
		virtual void onLeftNewer( wxCommandEvent& event ) { event.Skip(); }
		virtual void onDifferent( wxCommandEvent& event ) { event.Skip(); }
		virtual void onRightNewer( wxCommandEvent& event ) { event.Skip(); }
		virtual void onRightOnly( wxCommandEvent& event ) { event.Skip(); }
		virtual void onLeftCreate( wxCommandEvent& event ) { event.Skip(); }
		virtual void onRightCreate( wxCommandEvent& event ) { event.Skip(); }
		virtual void onLeftUpdate( wxCommandEvent& event ) { event.Skip(); }
		virtual void onRightUpdate( wxCommandEvent& event ) { event.Skip(); }
		virtual void onLeftDelete( wxCommandEvent& event ) { event.Skip(); }
		virtual void onRightDelete( wxCommandEvent& event ) { event.Skip(); }
		virtual void onDeletionRecycler( wxCommandEvent& event ) { event.Skip(); }
		virtual void onDeletionPermanent( wxCommandEvent& event ) { event.Skip(); }
		virtual void onDeletionVersioning( wxCommandEvent& event ) { event.Skip(); }
		virtual void onChangeVersioningStyle( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleVersioningLimit( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleMiscEmail( wxCommandEvent& event ) { event.Skip(); }
		virtual void onEmailAlways( wxCommandEvent& event ) { event.Skip(); }
		virtual void onEmailErrorWarning( wxCommandEvent& event ) { event.Skip(); }
		virtual void onEmailErrorOnly( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleMiscOption( wxCommandEvent& event ) { event.Skip(); }
		virtual void onShowLogFolder( wxCommandEvent& event ) { event.Skip(); }
		virtual void onAddNotes( wxCommandEvent& event ) { event.Skip(); }
		virtual void onOkay( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		ConfigDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Synchronization Settings"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxMAXIMIZE_BOX|wxRESIZE_BORDER );

		~ConfigDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class CloudSetupDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class CloudSetupDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticBitmap* m_bitmapCloud;
		wxToggleButton* m_toggleBtnGdrive;
		wxToggleButton* m_toggleBtnSftp;
		wxToggleButton* m_toggleBtnFtp;
		wxBoxSizer* bSizerGdrive;
		wxStaticBitmap* m_bitmapGdriveUser;
		wxListBox* m_listBoxGdriveUsers;
		zen::BitmapTextButton* m_buttonGdriveAddUser;
		zen::BitmapTextButton* m_buttonGdriveRemoveUser;
		wxStaticBitmap* m_bitmapGdriveDrive;
		wxListBox* m_listBoxGdriveDrives;
		wxBoxSizer* bSizerServer;
		wxStaticBitmap* m_bitmapServer;
		wxTextCtrl* m_textCtrlServer;
		wxTextCtrl* m_textCtrlPort;
		wxBoxSizer* bSizerAuth;
		wxBoxSizer* bSizerAuthInner;
		wxBoxSizer* bSizerFtpEncrypt;
		wxRadioButton* m_radioBtnEncryptNone;
		wxRadioButton* m_radioBtnEncryptSsl;
		wxBoxSizer* bSizerSftpAuth;
		wxRadioButton* m_radioBtnPassword;
		wxRadioButton* m_radioBtnKeyfile;
		wxRadioButton* m_radioBtnAgent;
		wxPanel* m_panelAuth;
		wxTextCtrl* m_textCtrlUserName;
		wxStaticText* m_staticTextKeyfile;
		wxBoxSizer* bSizerKeyFile;
		wxTextCtrl* m_textCtrlKeyfilePath;
		wxButton* m_buttonSelectKeyfile;
		wxStaticText* m_staticTextPassword;
		wxBoxSizer* bSizerPassword;
		wxTextCtrl* m_textCtrlPasswordVisible;
		wxTextCtrl* m_textCtrlPasswordHidden;
		wxCheckBox* m_checkBoxShowPassword;
		wxCheckBox* m_checkBoxPasswordPrompt;
		wxStaticBitmap* m_bitmapServerDir;
		wxStaticText* m_staticTextTimeout;
		wxSpinCtrl* m_spinCtrlTimeout;
		wxTextCtrl* m_textCtrlServerPath;
		wxButton* m_buttonSelectFolder;
		wxStaticBitmap* m_bitmapPerf;
		wxBoxSizer* bSizerConnectionsLabel;
		wxStaticText* m_staticTextConnectionsLabel;
		wxStaticText* m_staticTextConnectionsLabelSub;
		wxSpinCtrl* m_spinCtrlConnectionCount;
		wxStaticText* m_staticTextConnectionCountDescr;
		wxHyperlinkCtrl* m_hyperlinkDeRequired;
		wxStaticText* m_staticTextChannelCountSftp;
		wxSpinCtrl* m_spinCtrlChannelCountSftp;
		wxButton* m_buttonChannelCountSftp;
		wxCheckBox* m_checkBoxAllowZlib;
		wxStaticText* m_staticTextZlibDescr;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonOK;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onConnectionGdrive( wxCommandEvent& event ) { event.Skip(); }
		virtual void onConnectionSftp( wxCommandEvent& event ) { event.Skip(); }
		virtual void onConnectionFtp( wxCommandEvent& event ) { event.Skip(); }
		virtual void onGdriveUserSelect( wxCommandEvent& event ) { event.Skip(); }
		virtual void onGdriveUserAdd( wxCommandEvent& event ) { event.Skip(); }
		virtual void onGdriveUserRemove( wxCommandEvent& event ) { event.Skip(); }
		virtual void onAuthPassword( wxCommandEvent& event ) { event.Skip(); }
		virtual void onAuthKeyfile( wxCommandEvent& event ) { event.Skip(); }
		virtual void onAuthAgent( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSelectKeyfile( wxCommandEvent& event ) { event.Skip(); }
		virtual void onTypingPassword( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleShowPassword( wxCommandEvent& event ) { event.Skip(); }
		virtual void onTogglePasswordPrompt( wxCommandEvent& event ) { event.Skip(); }
		virtual void onBrowseCloudFolder( wxCommandEvent& event ) { event.Skip(); }
		virtual void onDetectServerChannelLimit( wxCommandEvent& event ) { event.Skip(); }
		virtual void onOkay( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		CloudSetupDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Access Online Storage"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

		~CloudSetupDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class AbstractFolderPickerGenerated
///////////////////////////////////////////////////////////////////////////////
class AbstractFolderPickerGenerated : public wxDialog
{
	private:

	protected:
		wxStaticText* m_staticTextStatus;
		wxTreeCtrl* m_treeCtrlFileSystem;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonOK;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onExpandNode( wxTreeEvent& event ) { event.Skip(); }
		virtual void onOkay( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		AbstractFolderPickerGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Select a folder"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxMAXIMIZE_BOX|wxRESIZE_BORDER );

		~AbstractFolderPickerGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class SyncConfirmationDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class SyncConfirmationDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticBitmap* m_bitmapSync;
		wxStaticText* m_staticTextCaption;
		wxPanel* m_panelStatistics;
		wxStaticText* m_staticTextSyncVar;
		wxStaticBitmap* m_bitmapSyncVar;
		wxStaticBitmap* m_bitmapDeleteLeft;
		wxStaticBitmap* m_bitmapUpdateLeft;
		wxStaticBitmap* m_bitmapCreateLeft;
		wxStaticBitmap* m_bitmapData;
		wxStaticBitmap* m_bitmapCreateRight;
		wxStaticBitmap* m_bitmapUpdateRight;
		wxStaticBitmap* m_bitmapDeleteRight;
		wxStaticText* m_staticTextDeleteLeft;
		wxStaticText* m_staticTextUpdateLeft;
		wxStaticText* m_staticTextCreateLeft;
		wxStaticText* m_staticTextData;
		wxStaticText* m_staticTextCreateRight;
		wxStaticText* m_staticTextUpdateRight;
		wxStaticText* m_staticTextDeleteRight;
		wxCheckBox* m_checkBoxDontShowAgain;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonOK;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onStartSync( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		SyncConfirmationDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE );

		~SyncConfirmationDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class CompareProgressDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class CompareProgressDlgGenerated : public wxPanel
{
	private:

	protected:
		wxStaticText* m_staticTextStatus;
		wxStaticText* m_staticTextProcessed;
		wxStaticText* m_staticTextRemaining;
		wxPanel* m_panelItemStats;
		wxStaticBitmap* m_bitmapItemStat;
		wxStaticText* m_staticTextItemsProcessed;
		wxStaticText* m_staticTextBytesProcessed;
		wxStaticText* m_staticTextItemsRemaining;
		wxStaticText* m_staticTextBytesRemaining;
		wxPanel* m_panelTimeStats;
		wxStaticBitmap* m_bitmapTimeStat;
		wxStaticText* m_staticTextTimeElapsed;
		wxStaticText* m_staticTextTimeRemaining;
		wxStaticText* m_staticTextErrors;
		wxStaticText* m_staticTextWarnings;
		wxPanel* m_panelErrorStats;
		wxStaticBitmap* m_bitmapErrors;
		wxStaticText* m_staticTextErrorCount;
		wxStaticBitmap* m_bitmapWarnings;
		wxStaticText* m_staticTextWarningCount;
		wxBoxSizer* bSizerErrorsRetry;
		wxStaticBitmap* m_bitmapRetryErrors;
		wxStaticText* m_staticTextRetryCount;
		wxBoxSizer* bSizerErrorsIgnore;
		wxStaticBitmap* m_bitmapIgnoreErrors;
		wxBoxSizer* bSizerProgressGraph;
		zen::Graph2D* m_panelProgressGraph;

	public:

		CompareProgressDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxBORDER_RAISED, const wxString& name = wxEmptyString );

		~CompareProgressDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class SyncProgressPanelGenerated
///////////////////////////////////////////////////////////////////////////////
class SyncProgressPanelGenerated : public wxPanel
{
	private:

	protected:

	public:
		wxBoxSizer* bSizerRoot;
		wxStaticBitmap* m_bitmapStatus;
		wxStaticText* m_staticTextPhase;
		wxStaticText* m_staticTextPercentTotal;
		wxBitmapButton* m_bpButtonMinimizeToTray;
		wxBoxSizer* bSizerStatusText;
		wxStaticText* m_staticTextStatus;
		wxPanel* m_panelProgress;
		zen::Graph2D* m_panelGraphBytes;
		wxStaticBitmap* m_bitmapGraphKeyBytes;
		wxStaticBitmap* m_bitmapGraphKeyItems;
		wxStaticText* m_staticTextProcessed;
		wxStaticText* m_staticTextRemaining;
		wxPanel* m_panelItemStats;
		wxStaticBitmap* m_bitmapItemStat;
		wxStaticText* m_staticTextItemsProcessed;
		wxStaticText* m_staticTextBytesProcessed;
		wxStaticText* m_staticTextItemsRemaining;
		wxStaticText* m_staticTextBytesRemaining;
		wxPanel* m_panelTimeStats;
		wxStaticBitmap* m_bitmapTimeStat;
		wxStaticText* m_staticTextTimeElapsed;
		wxStaticText* m_staticTextTimeRemaining;
		wxStaticText* m_staticTextErrors;
		wxStaticText* m_staticTextWarnings;
		wxPanel* m_panelErrorStats;
		wxStaticBitmap* m_bitmapErrors;
		wxStaticText* m_staticTextErrorCount;
		wxStaticBitmap* m_bitmapWarnings;
		wxStaticText* m_staticTextWarningCount;
		wxBoxSizer* bSizerDynSpace;
		zen::Graph2D* m_panelGraphItems;
		wxBoxSizer* bSizerProgressFooter;
		wxBoxSizer* bSizerErrorsRetry;
		wxStaticBitmap* m_bitmapRetryErrors;
		wxStaticText* m_staticTextRetryCount;
		wxBoxSizer* bSizerErrorsIgnore;
		wxStaticBitmap* m_bitmapIgnoreErrors;
		wxChoice* m_choicePostSyncAction;
		wxNotebook* m_notebookResult;
		wxStaticLine* m_staticlineFooter;
		wxBoxSizer* bSizerStdButtons;
		wxCheckBox* m_checkBoxAutoClose;
		wxButton* m_buttonClose;
		wxButton* m_buttonPause;
		wxButton* m_buttonStop;

		SyncProgressPanelGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxTAB_TRAVERSAL, const wxString& name = wxEmptyString );

		~SyncProgressPanelGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class LogPanelGenerated
///////////////////////////////////////////////////////////////////////////////
class LogPanelGenerated : public wxPanel
{
	private:

	protected:
		zen::ToggleButton* m_bpButtonErrors;
		zen::ToggleButton* m_bpButtonWarnings;
		zen::ToggleButton* m_bpButtonInfo;
		zen::Grid* m_gridMessages;

		// Virtual event handlers, override them in your derived class
		virtual void onErrors( wxCommandEvent& event ) { event.Skip(); }
		virtual void onWarnings( wxCommandEvent& event ) { event.Skip(); }
		virtual void onInfo( wxCommandEvent& event ) { event.Skip(); }


	public:

		LogPanelGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL, const wxString& name = wxEmptyString );

		~LogPanelGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class BatchDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class BatchDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticBitmap* m_bitmapBatchJob;
		wxStaticText* m_staticTextHeader;
		wxStaticBitmap* m_bitmapMinimizeToTray;
		wxCheckBox* m_checkBoxRunMinimized;
		wxCheckBox* m_checkBoxAutoClose;
		wxStaticBitmap* m_bitmapIgnoreErrors;
		wxCheckBox* m_checkBoxIgnoreErrors;
		wxRadioButton* m_radioBtnErrorDialogShow;
		wxRadioButton* m_radioBtnErrorDialogCancel;
		wxChoice* m_choicePostSyncAction;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonSaveAs;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onToggleRunMinimized( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleIgnoreErrors( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSaveBatchJob( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		BatchDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Save as a Batch Job"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

		~BatchDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class DeleteDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class DeleteDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticBitmap* m_bitmapDeleteType;
		wxStaticText* m_staticTextHeader;
		wxTextCtrl* m_textCtrlFileList;
		wxBoxSizer* bSizerStdButtons;
		wxCheckBox* m_checkBoxUseRecycler;
		wxButton* m_buttonOK;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onUseRecycler( wxCommandEvent& event ) { event.Skip(); }
		virtual void onOkay( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		DeleteDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Delete Items"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxMAXIMIZE_BOX|wxRESIZE_BORDER );

		~DeleteDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class CopyToDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class CopyToDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticBitmap* m_bitmapCopyTo;
		wxStaticText* m_staticTextHeader;
		wxTextCtrl* m_textCtrlFileList;
		fff::FolderHistoryBox* m_targetFolderPath;
		wxButton* m_buttonSelectTargetFolder;
		wxBitmapButton* m_bpButtonSelectAltTargetFolder;
		wxBoxSizer* bSizerStdButtons;
		wxCheckBox* m_checkBoxKeepRelPath;
		wxCheckBox* m_checkBoxOverwriteIfExists;
		wxButton* m_buttonOK;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onOkay( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		CopyToDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Copy Items"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxMAXIMIZE_BOX|wxRESIZE_BORDER );

		~CopyToDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class RenameDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class RenameDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticBitmap* m_bitmapRename;
		wxStaticText* m_staticTextHeader;
		zen::Grid* m_gridRenamePreview;
		wxStaticLine* m_staticlinePreview;
		wxStaticText* m_staticTextPlaceholderDescription;
		wxTextCtrl* m_textCtrlNewName;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonOK;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onOkay( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		RenameDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Rename Items"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxMAXIMIZE_BOX|wxRESIZE_BORDER );

		~RenameDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class OptionsDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class OptionsDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticBitmap* m_bitmapSettings;
		wxCheckBox* m_checkBoxFailSafe;
		wxBoxSizer* bSizerLockedFiles;
		wxCheckBox* m_checkBoxCopyLocked;
		wxCheckBox* m_checkBoxCopyPermissions;
		wxBoxSizer* bSizerColorTheme;
		wxStaticBitmap* m_bitmapColorTheme;
		wxChoice* m_choiceColorTheme;
		wxStaticBitmap* m_bitmapWarnings;
		wxStaticText* m_staticTextHiddenDialogsCount;
		wxButton* m_buttonShowHiddenDialogs;
		wxCheckListBox* m_checkListHiddenDialogs;
		wxStaticBitmap* m_bitmapLogFile;
		wxBitmapButton* m_bpButtonShowLogFolder;
		wxPanel* m_panelLogfile;
		fff::FolderHistoryBox* m_logFolderPath;
		wxButton* m_buttonSelectLogFolder;
		wxBitmapButton* m_bpButtonSelectAltLogFolder;
		wxCheckBox* m_checkBoxLogFilesMaxAge;
		wxSpinCtrl* m_spinCtrlLogFilesMaxAge;
		wxRadioButton* m_radioBtnLogHtml;
		wxRadioButton* m_radioBtnLogText;
		wxStaticBitmap* m_bitmapNotificationSounds;
		wxStaticBitmap* m_bitmapCompareDone;
		wxBitmapButton* m_bpButtonPlayCompareDone;
		wxTextCtrl* m_textCtrlSoundPathCompareDone;
		wxButton* m_buttonSelectSoundCompareDone;
		wxStaticBitmap* m_bitmapSyncDone;
		wxBitmapButton* m_bpButtonPlaySyncDone;
		wxTextCtrl* m_textCtrlSoundPathSyncDone;
		wxButton* m_buttonSelectSoundSyncDone;
		wxStaticBitmap* m_bitmapAlertPending;
		wxBitmapButton* m_bpButtonPlayAlertPending;
		wxTextCtrl* m_textCtrlSoundPathAlertPending;
		wxButton* m_buttonSelectSoundAlertPending;
		wxStaticBitmap* m_bitmapConsole;
		wxButton* m_buttonShowCtxCustomize;
		wxBoxSizer* bSizerContextCustomize;
		wxBitmapButton* m_bpButtonAddRow;
		wxBitmapButton* m_bpButtonRemoveRow;
		wxGrid* m_gridCustomCommand;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonDefault;
		wxButton* m_buttonOK;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onChangeColorTheme( wxCommandEvent& event ) { event.Skip(); }
		virtual void onShowHiddenDialogs( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleHiddenDialog( wxCommandEvent& event ) { event.Skip(); }
		virtual void onShowLogFolder( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleLogfilesLimit( wxCommandEvent& event ) { event.Skip(); }
		virtual void onPlayCompareDone( wxCommandEvent& event ) { event.Skip(); }
		virtual void onChangeSoundFilePath( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSelectSoundCompareDone( wxCommandEvent& event ) { event.Skip(); }
		virtual void onPlaySyncDone( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSelectSoundSyncDone( wxCommandEvent& event ) { event.Skip(); }
		virtual void onPlayAlertPending( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSelectSoundAlertPending( wxCommandEvent& event ) { event.Skip(); }
		virtual void onShowContextCustomize( wxCommandEvent& event ) { event.Skip(); }
		virtual void onAddRow( wxCommandEvent& event ) { event.Skip(); }
		virtual void onRemoveRow( wxCommandEvent& event ) { event.Skip(); }
		virtual void onDefault( wxCommandEvent& event ) { event.Skip(); }
		virtual void onOkay( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		OptionsDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Options"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

		~OptionsDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class SelectTimespanDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class SelectTimespanDlgGenerated : public wxDialog
{
	private:

	protected:
		wxCalendarCtrl* m_calendarFrom;
		wxCalendarCtrl* m_calendarTo;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonOK;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onChangeSelectionFrom( wxCalendarEvent& event ) { event.Skip(); }
		virtual void onChangeSelectionTo( wxCalendarEvent& event ) { event.Skip(); }
		virtual void onOkay( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		SelectTimespanDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Select Time Span"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE );

		~SelectTimespanDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class AboutDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class AboutDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticBitmap* m_bitmapLogoLeft;
		wxBoxSizer* bSizerMainSection;
		wxStaticBitmap* m_bitmapLogo;
		wxStaticText* m_staticFfsTextVersion;
		wxStaticText* m_staticTextFfsVariant;
		wxBoxSizer* bSizerDonate;
		wxPanel* m_panelDonate;
		wxStaticBitmap* m_bitmapAnimalSmall;
		wxStaticText* m_staticTextDonate;
		zen::BitmapTextButton* m_buttonDonate1;
		wxStaticBitmap* m_bitmapAnimalBig;
		wxBitmapButton* m_bpButtonForum;
		wxBitmapButton* m_bpButtonEmail;
		wxStaticText* m_staticTextThanksForLoc;
		wxScrolledWindow* m_scrolledWindowTranslators;
		wxFlexGridSizer* fgSizerTranslators;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonShowSupporterDetails;
		zen::BitmapTextButton* m_buttonDonate2;
		wxButton* m_buttonClose;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onDonate( wxCommandEvent& event ) { event.Skip(); }
		virtual void onOpenForum( wxCommandEvent& event ) { event.Skip(); }
		virtual void onSendEmail( wxCommandEvent& event ) { event.Skip(); }
		virtual void onShowSupporterDetails( wxCommandEvent& event ) { event.Skip(); }
		virtual void onOkay( wxCommandEvent& event ) { event.Skip(); }


	public:

		AboutDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("About"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE );

		~AboutDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class DownloadProgressDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class DownloadProgressDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticBitmap* m_bitmapDownloading;
		wxStaticText* m_staticTextHeader;
		wxGauge* m_gaugeProgress;
		wxStaticText* m_staticTextDetails;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		DownloadProgressDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0 );

		~DownloadProgressDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class CfgHighlightDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class CfgHighlightDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticText* m_staticTextHighlight;
		wxSpinCtrl* m_spinCtrlOverdueDays;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonOK;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onOkay( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		CfgHighlightDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Highlight Configurations"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE );

		~CfgHighlightDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class PasswordPromptDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class PasswordPromptDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticText* m_staticTextMain;
		wxStaticText* m_staticTextPassword;
		wxTextCtrl* m_textCtrlPasswordVisible;
		wxTextCtrl* m_textCtrlPasswordHidden;
		wxCheckBox* m_checkBoxShowPassword;
		wxBoxSizer* bSizerError;
		wxStaticBitmap* m_bitmapError;
		wxStaticText* m_staticTextError;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonOK;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onTypingPassword( wxCommandEvent& event ) { event.Skip(); }
		virtual void onToggleShowPassword( wxCommandEvent& event ) { event.Skip(); }
		virtual void onOkay( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		PasswordPromptDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("dummy"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

		~PasswordPromptDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class ActivationDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class ActivationDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticBitmap* m_bitmapActivation;
		wxRichTextCtrl* m_richTextLastError;
		wxStaticText* m_staticTextMain;
		wxStaticText* m_staticTextMain1;
		wxButton* m_buttonActivateOnline;
		wxButton* m_buttonCopyUrl;
		wxRichTextCtrl* m_richTextManualActivationUrl;
		wxTextCtrl* m_textCtrlOfflineActivationKey;
		wxButton* m_buttonActivateOffline;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonCancel;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onActivateOnline( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCopyUrl( wxCommandEvent& event ) { event.Skip(); }
		virtual void onOfflineActivationEnter( wxCommandEvent& event ) { event.Skip(); }
		virtual void onActivateOffline( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		ActivationDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("dummy"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

		~ActivationDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class WarnAccessRightsMissingDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class WarnAccessRightsMissingDlgGenerated : public wxDialog
{
	private:

	protected:
		wxStaticBitmap* m_bitmapGrantAccess;
		wxStaticText* m_staticTextDescr;
		wxStaticText* m_staticTextStep1;
		wxButton* m_buttonLocateBundle;
		wxStaticText* m_staticTextStep2;
		wxButton* m_buttonOpenSecurity;
		wxStaticText* m_staticTextStep3;
		wxStaticText* m_staticTextGrantAccess;
		wxCheckBox* m_checkBoxDontShowAgain;
		wxBoxSizer* bSizerStdButtons;
		wxButton* m_buttonClose;

		// Virtual event handlers, override them in your derived class
		virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void onShowAppBundle( wxCommandEvent& event ) { event.Skip(); }
		virtual void onOpenSecuritySettings( wxCommandEvent& event ) { event.Skip(); }
		virtual void onCheckBoxClick( wxCommandEvent& event ) { event.Skip(); }
		virtual void onOkay( wxCommandEvent& event ) { event.Skip(); }


	public:

		WarnAccessRightsMissingDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Grant Full Disk Access"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE );

		~WarnAccessRightsMissingDlgGenerated();

};


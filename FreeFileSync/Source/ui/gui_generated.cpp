///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version May 29 2018)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "gui_generated.h"

///////////////////////////////////////////////////////////////////////////

MainDialogGenerated::MainDialogGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxFrame( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 640,400 ), wxDefaultSize );
	
	m_menubar = new wxMenuBar( 0 );
	m_menuFile = new wxMenu();
	m_menuItemNew = new wxMenuItem( m_menuFile, wxID_NEW, wxString( _("&New") ) + wxT('\t') + wxT("Ctrl+N"), wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuItemNew );
	
	m_menuItemLoad = new wxMenuItem( m_menuFile, wxID_OPEN, wxString( _("&Open...") ) + wxT('\t') + wxT("Ctrl+O"), wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuItemLoad );
	
	m_menuFile->AppendSeparator();
	
	m_menuItemSave = new wxMenuItem( m_menuFile, wxID_SAVE, wxString( _("&Save") ) + wxT('\t') + wxT("Ctrl+S"), wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuItemSave );
	
	m_menuItemSaveAs = new wxMenuItem( m_menuFile, wxID_SAVEAS, wxString( _("Save &as...") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuItemSaveAs );
	
	m_menuItemSaveAsBatch = new wxMenuItem( m_menuFile, wxID_ANY, wxString( _("Save as &batch job...") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuItemSaveAsBatch );
	
	m_menuFile->AppendSeparator();
	
	wxMenuItem* m_menuItem4;
	m_menuItem4 = new wxMenuItem( m_menuFile, wxID_EXIT, wxString( _("E&xit") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuItem4 );
	
	m_menubar->Append( m_menuFile, _("&File") ); 
	
	m_menu4 = new wxMenu();
	m_menuItemShowLog = new wxMenuItem( m_menu4, wxID_ANY, wxString( _("Show &log") ) + wxT('\t') + wxT("F4"), wxEmptyString, wxITEM_NORMAL );
	m_menu4->Append( m_menuItemShowLog );
	
	m_menu4->AppendSeparator();
	
	m_menuItemCompare = new wxMenuItem( m_menu4, wxID_ANY, wxString( _("Start &comparison") ) + wxT('\t') + wxT("F5"), wxEmptyString, wxITEM_NORMAL );
	m_menu4->Append( m_menuItemCompare );
	
	m_menu4->AppendSeparator();
	
	m_menuItemCompSettings = new wxMenuItem( m_menu4, wxID_ANY, wxString( _("C&omparison settings") ) + wxT('\t') + wxT("F6"), wxEmptyString, wxITEM_NORMAL );
	m_menu4->Append( m_menuItemCompSettings );
	
	m_menuItemFilter = new wxMenuItem( m_menu4, wxID_ANY, wxString( _("&Filter settings") ) + wxT('\t') + wxT("F7"), wxEmptyString, wxITEM_NORMAL );
	m_menu4->Append( m_menuItemFilter );
	
	m_menuItemSyncSettings = new wxMenuItem( m_menu4, wxID_ANY, wxString( _("S&ynchronization settings") ) + wxT('\t') + wxT("F8"), wxEmptyString, wxITEM_NORMAL );
	m_menu4->Append( m_menuItemSyncSettings );
	
	m_menu4->AppendSeparator();
	
	m_menuItemSynchronize = new wxMenuItem( m_menu4, wxID_ANY, wxString( _("Start &synchronization") ) + wxT('\t') + wxT("F9"), wxEmptyString, wxITEM_NORMAL );
	m_menu4->Append( m_menuItemSynchronize );
	
	m_menubar->Append( m_menu4, _("&Actions") ); 
	
	m_menuTools = new wxMenu();
	m_menuItemOptions = new wxMenuItem( m_menuTools, wxID_PREFERENCES, wxString( _("&Preferences") ) + wxT('\t') + wxT("Ctrl+,"), wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItemOptions );
	
	m_menuLanguages = new wxMenu();
	wxMenuItem* m_menuLanguagesItem = new wxMenuItem( m_menuTools, wxID_ANY, _("&Language"), wxEmptyString, wxITEM_NORMAL, m_menuLanguages );
	m_menuTools->Append( m_menuLanguagesItem );
	
	m_menuTools->AppendSeparator();
	
	m_menuItemFind = new wxMenuItem( m_menuTools, wxID_FIND, wxString( _("&Find...") ) + wxT('\t') + wxT("Ctrl+F"), wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItemFind );
	
	m_menuItemExportList = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("&Export file list...") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItemExportList );
	
	m_menuTools->AppendSeparator();
	
	wxMenuItem* m_menuItem51;
	m_menuItem51 = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("&Reset layout") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItem51 );
	
	m_menuItemShowMain = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("dummy") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItemShowMain );
	
	m_menuItemShowFolders = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("dummy") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItemShowFolders );
	
	m_menuItemShowViewFilter = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("dummy") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItemShowViewFilter );
	
	m_menuItemShowConfig = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("dummy") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItemShowConfig );
	
	m_menuItemShowOverview = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("dummy") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItemShowOverview );
	
	m_menubar->Append( m_menuTools, _("&Tools") ); 
	
	m_menuHelp = new wxMenu();
	m_menuItemHelp = new wxMenuItem( m_menuHelp, wxID_HELP, wxString( _("&View help") ) + wxT('\t') + wxT("F1"), wxEmptyString, wxITEM_NORMAL );
	m_menuHelp->Append( m_menuItemHelp );
	
	m_menuHelp->AppendSeparator();
	
	m_menuItemCheckVersionNow = new wxMenuItem( m_menuHelp, wxID_ANY, wxString( _("&Check for updates now") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuHelp->Append( m_menuItemCheckVersionNow );
	
	m_menuItemCheckVersionAuto = new wxMenuItem( m_menuHelp, wxID_ANY, wxString( _("Check &automatically once a week") ) , wxEmptyString, wxITEM_CHECK );
	m_menuHelp->Append( m_menuItemCheckVersionAuto );
	m_menuItemCheckVersionAuto->Check( true );
	
	m_menuHelp->AppendSeparator();
	
	m_menuItemAbout = new wxMenuItem( m_menuHelp, wxID_ABOUT, wxString( _("&About") ) + wxT('\t') + wxT("Shift+F1"), wxEmptyString, wxITEM_NORMAL );
	m_menuHelp->Append( m_menuItemAbout );
	
	m_menubar->Append( m_menuHelp, _("&Help") ); 
	
	this->SetMenuBar( m_menubar );
	
	bSizerPanelHolder = new wxBoxSizer( wxVERTICAL );
	
	m_panelTopButtons = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer1791;
	bSizer1791 = new wxBoxSizer( wxVERTICAL );
	
	bSizerTopButtons = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer261;
	bSizer261 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer261->Add( 0, 0, 1, 0, 5 );
	
	m_buttonCancel = new wxButton( m_panelTopButtons, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonCancel->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	m_buttonCancel->Enable( false );
	m_buttonCancel->Hide();
	
	bSizer261->Add( m_buttonCancel, 0, wxEXPAND, 5 );
	
	m_buttonCompare = new zen::BitmapTextButton( m_panelTopButtons, wxID_ANY, _("Compare"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonCompare->SetDefault(); 
	m_buttonCompare->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	m_buttonCompare->SetToolTip( _("dummy") );
	
	bSizer261->Add( m_buttonCompare, 0, wxEXPAND, 5 );
	
	
	bSizer261->Add( 4, 0, 0, 0, 5 );
	
	m_bpButtonCmpConfig = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonCmpConfig->SetToolTip( _("dummy") );
	
	bSizer261->Add( m_bpButtonCmpConfig, 0, wxEXPAND, 5 );
	
	m_bpButtonCmpContext = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonCmpContext->SetToolTip( _("dummy") );
	
	bSizer261->Add( m_bpButtonCmpContext, 0, wxEXPAND, 5 );
	
	
	bSizer261->Add( 0, 0, 1, 0, 5 );
	
	
	bSizerTopButtons->Add( bSizer261, 1, wxEXPAND, 5 );
	
	
	bSizerTopButtons->Add( 5, 2, 0, 0, 5 );
	
	wxBoxSizer* bSizer199;
	bSizer199 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer199->Add( 0, 0, 1, 0, 5 );
	
	m_bpButtonFilter = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW|wxFULL_REPAINT_ON_RESIZE );
	m_bpButtonFilter->SetToolTip( _("dummy") );
	
	bSizer199->Add( m_bpButtonFilter, 0, wxEXPAND, 5 );
	
	m_bpButtonFilterContext = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonFilterContext->SetToolTip( _("dummy") );
	
	bSizer199->Add( m_bpButtonFilterContext, 0, wxEXPAND, 5 );
	
	
	bSizer199->Add( 0, 0, 1, 0, 5 );
	
	
	bSizerTopButtons->Add( bSizer199, 0, wxEXPAND, 5 );
	
	
	bSizerTopButtons->Add( 5, 2, 0, 0, 5 );
	
	wxBoxSizer* bSizer262;
	bSizer262 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer262->Add( 0, 0, 1, 0, 5 );
	
	m_bpButtonSyncConfig = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSyncConfig->SetToolTip( _("dummy") );
	
	bSizer262->Add( m_bpButtonSyncConfig, 0, wxEXPAND, 5 );
	
	m_bpButtonSyncContext = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSyncContext->SetToolTip( _("dummy") );
	
	bSizer262->Add( m_bpButtonSyncContext, 0, wxEXPAND, 5 );
	
	
	bSizer262->Add( 4, 0, 0, 0, 5 );
	
	m_buttonSync = new zen::BitmapTextButton( m_panelTopButtons, wxID_ANY, _("Synchronize"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonSync->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	m_buttonSync->SetToolTip( _("dummy") );
	
	bSizer262->Add( m_buttonSync, 0, wxEXPAND, 5 );
	
	
	bSizer262->Add( 0, 0, 1, 0, 5 );
	
	
	bSizerTopButtons->Add( bSizer262, 1, wxEXPAND, 5 );
	
	
	bSizer1791->Add( bSizerTopButtons, 1, wxEXPAND, 5 );
	
	
	m_panelTopButtons->SetSizer( bSizer1791 );
	m_panelTopButtons->Layout();
	bSizer1791->Fit( m_panelTopButtons );
	bSizerPanelHolder->Add( m_panelTopButtons, 0, wxEXPAND, 5 );
	
	m_panelDirectoryPairs = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSTATIC_BORDER|wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer1601;
	bSizer1601 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer91;
	bSizer91 = new wxBoxSizer( wxHORIZONTAL );
	
	m_panelTopLeft = new wxPanel( m_panelDirectoryPairs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelTopLeft->SetMinSize( wxSize( 1,-1 ) );
	
	wxFlexGridSizer* fgSizer8;
	fgSizer8 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer8->AddGrowableCol( 1 );
	fgSizer8->SetFlexibleDirection( wxBOTH );
	fgSizer8->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_ALL );
	
	
	fgSizer8->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextResolvedPathL = new wxStaticText( m_panelTopLeft, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextResolvedPathL->Wrap( -1 );
	fgSizer8->Add( m_staticTextResolvedPathL, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
	
	wxBoxSizer* bSizer159;
	bSizer159 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonAddPair = new wxBitmapButton( m_panelTopLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonAddPair->SetToolTip( _("Add folder pair") );
	
	bSizer159->Add( m_bpButtonAddPair, 0, wxEXPAND, 5 );
	
	m_bpButtonRemovePair = new wxBitmapButton( m_panelTopLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonRemovePair->SetToolTip( _("Remove folder pair") );
	
	bSizer159->Add( m_bpButtonRemovePair, 0, wxEXPAND, 5 );
	
	
	fgSizer8->Add( bSizer159, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer182;
	bSizer182 = new wxBoxSizer( wxHORIZONTAL );
	
	m_folderPathLeft = new fff::FolderHistoryBox( m_panelTopLeft, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer182->Add( m_folderPathLeft, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectFolderLeft = new wxButton( m_panelTopLeft, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectFolderLeft->SetToolTip( _("Select a folder") );
	
	bSizer182->Add( m_buttonSelectFolderLeft, 0, wxEXPAND, 5 );
	
	m_bpButtonSelectAltFolderLeft = new wxBitmapButton( m_panelTopLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSelectAltFolderLeft->SetToolTip( _("Access online storage") );
	
	bSizer182->Add( m_bpButtonSelectAltFolderLeft, 0, wxEXPAND, 5 );
	
	
	fgSizer8->Add( bSizer182, 0, wxEXPAND, 5 );
	
	
	m_panelTopLeft->SetSizer( fgSizer8 );
	m_panelTopLeft->Layout();
	fgSizer8->Fit( m_panelTopLeft );
	bSizer91->Add( m_panelTopLeft, 1, wxLEFT|wxALIGN_BOTTOM, 5 );
	
	m_panelTopCenter = new wxPanel( m_panelDirectoryPairs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer1771;
	bSizer1771 = new wxBoxSizer( wxVERTICAL );
	
	m_bpButtonSwapSides = new wxBitmapButton( m_panelTopCenter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSwapSides->SetToolTip( _("dummy") );
	
	bSizer1771->Add( m_bpButtonSwapSides, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer160;
	bSizer160 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonLocalCompCfg = new wxBitmapButton( m_panelTopCenter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonLocalCompCfg->SetToolTip( _("dummy") );
	
	bSizer160->Add( m_bpButtonLocalCompCfg, 0, wxEXPAND, 5 );
	
	m_bpButtonLocalFilter = new wxBitmapButton( m_panelTopCenter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonLocalFilter->SetToolTip( _("dummy") );
	
	bSizer160->Add( m_bpButtonLocalFilter, 0, wxEXPAND, 5 );
	
	m_bpButtonLocalSyncCfg = new wxBitmapButton( m_panelTopCenter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonLocalSyncCfg->SetToolTip( _("dummy") );
	
	bSizer160->Add( m_bpButtonLocalSyncCfg, 0, wxEXPAND, 5 );
	
	
	bSizer1771->Add( bSizer160, 1, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	m_panelTopCenter->SetSizer( bSizer1771 );
	m_panelTopCenter->Layout();
	bSizer1771->Fit( m_panelTopCenter );
	bSizer91->Add( m_panelTopCenter, 0, wxRIGHT|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_panelTopRight = new wxPanel( m_panelDirectoryPairs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelTopRight->SetMinSize( wxSize( 1,-1 ) );
	
	wxBoxSizer* bSizer183;
	bSizer183 = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextResolvedPathR = new wxStaticText( m_panelTopRight, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextResolvedPathR->Wrap( -1 );
	bSizer183->Add( m_staticTextResolvedPathR, 0, wxALL, 2 );
	
	wxBoxSizer* bSizer179;
	bSizer179 = new wxBoxSizer( wxHORIZONTAL );
	
	m_folderPathRight = new fff::FolderHistoryBox( m_panelTopRight, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer179->Add( m_folderPathRight, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectFolderRight = new wxButton( m_panelTopRight, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectFolderRight->SetToolTip( _("Select a folder") );
	
	bSizer179->Add( m_buttonSelectFolderRight, 0, wxEXPAND, 5 );
	
	m_bpButtonSelectAltFolderRight = new wxBitmapButton( m_panelTopRight, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSelectAltFolderRight->SetToolTip( _("Access online storage") );
	
	bSizer179->Add( m_bpButtonSelectAltFolderRight, 0, wxEXPAND, 5 );
	
	
	bSizer183->Add( bSizer179, 0, wxEXPAND, 5 );
	
	
	m_panelTopRight->SetSizer( bSizer183 );
	m_panelTopRight->Layout();
	bSizer183->Fit( m_panelTopRight );
	bSizer91->Add( m_panelTopRight, 1, wxRIGHT|wxALIGN_BOTTOM, 5 );
	
	
	bSizer1601->Add( bSizer91, 0, wxEXPAND, 5 );
	
	m_scrolledWindowFolderPairs = new wxScrolledWindow( m_panelDirectoryPairs, wxID_ANY, wxDefaultPosition, wxSize( -1,-1 ), wxHSCROLL|wxVSCROLL );
	m_scrolledWindowFolderPairs->SetScrollRate( 10, 10 );
	m_scrolledWindowFolderPairs->SetMinSize( wxSize( -1,0 ) );
	
	bSizerAddFolderPairs = new wxBoxSizer( wxVERTICAL );
	
	
	m_scrolledWindowFolderPairs->SetSizer( bSizerAddFolderPairs );
	m_scrolledWindowFolderPairs->Layout();
	bSizerAddFolderPairs->Fit( m_scrolledWindowFolderPairs );
	bSizer1601->Add( m_scrolledWindowFolderPairs, 1, wxEXPAND, 5 );
	
	
	m_panelDirectoryPairs->SetSizer( bSizer1601 );
	m_panelDirectoryPairs->Layout();
	bSizer1601->Fit( m_panelDirectoryPairs );
	bSizerPanelHolder->Add( m_panelDirectoryPairs, 0, wxEXPAND, 5 );
	
	m_gridOverview = new zen::Grid( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_gridOverview->SetScrollRate( 5, 5 );
	bSizerPanelHolder->Add( m_gridOverview, 0, 0, 5 );
	
	m_panelCenter = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer1711;
	bSizer1711 = new wxBoxSizer( wxVERTICAL );
	
	m_splitterMain = new fff::TripleSplitter( m_panelCenter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer1781;
	bSizer1781 = new wxBoxSizer( wxHORIZONTAL );
	
	m_gridMainL = new zen::Grid( m_splitterMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_gridMainL->SetScrollRate( 5, 5 );
	bSizer1781->Add( m_gridMainL, 1, wxEXPAND, 5 );
	
	m_gridMainC = new zen::Grid( m_splitterMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_gridMainC->SetScrollRate( 5, 5 );
	bSizer1781->Add( m_gridMainC, 0, wxEXPAND, 5 );
	
	m_gridMainR = new zen::Grid( m_splitterMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_gridMainR->SetScrollRate( 5, 5 );
	bSizer1781->Add( m_gridMainR, 1, wxEXPAND, 5 );
	
	
	m_splitterMain->SetSizer( bSizer1781 );
	m_splitterMain->Layout();
	bSizer1781->Fit( m_splitterMain );
	bSizer1711->Add( m_splitterMain, 1, wxEXPAND, 5 );
	
	m_panelStatusBar = new wxPanel( m_panelCenter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSTATIC_BORDER|wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer451;
	bSizer451 = new wxBoxSizer( wxHORIZONTAL );
	
	bSizer451->SetMinSize( wxSize( -1,22 ) ); 
	bSizerFileStatus = new wxBoxSizer( wxHORIZONTAL );
	
	bSizerStatusLeft = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer53;
	bSizer53 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer53->Add( 0, 0, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	bSizerStatusLeftDirectories = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapSmallDirectoryLeft = new wxStaticBitmap( m_panelStatusBar, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerStatusLeftDirectories->Add( m_bitmapSmallDirectoryLeft, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusLeftDirectories->Add( 2, 0, 0, 0, 5 );
	
	m_staticTextStatusLeftDirs = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusLeftDirs->Wrap( -1 );
	bSizerStatusLeftDirectories->Add( m_staticTextStatusLeftDirs, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer53->Add( bSizerStatusLeftDirectories, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	bSizerStatusLeftFiles = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizerStatusLeftFiles->Add( 10, 0, 0, 0, 5 );
	
	m_bitmapSmallFileLeft = new wxStaticBitmap( m_panelStatusBar, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerStatusLeftFiles->Add( m_bitmapSmallFileLeft, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusLeftFiles->Add( 2, 0, 0, 0, 5 );
	
	m_staticTextStatusLeftFiles = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusLeftFiles->Wrap( -1 );
	bSizerStatusLeftFiles->Add( m_staticTextStatusLeftFiles, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusLeftFiles->Add( 4, 0, 0, 0, 5 );
	
	m_staticTextStatusLeftBytes = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusLeftBytes->Wrap( -1 );
	bSizerStatusLeftFiles->Add( m_staticTextStatusLeftBytes, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer53->Add( bSizerStatusLeftFiles, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer53->Add( 0, 0, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusLeft->Add( bSizer53, 1, wxEXPAND, 5 );
	
	m_staticline9 = new wxStaticLine( m_panelStatusBar, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizerStatusLeft->Add( m_staticline9, 0, wxEXPAND|wxTOP, 2 );
	
	
	bSizerFileStatus->Add( bSizerStatusLeft, 1, wxEXPAND, 5 );
	
	
	bSizerFileStatus->Add( 26, 0, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextStatusCenter = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusCenter->Wrap( -1 );
	bSizerFileStatus->Add( m_staticTextStatusCenter, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerFileStatus->Add( 26, 0, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	bSizerStatusRight = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticline10 = new wxStaticLine( m_panelStatusBar, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizerStatusRight->Add( m_staticline10, 0, wxEXPAND|wxTOP, 2 );
	
	wxBoxSizer* bSizer52;
	bSizer52 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer52->Add( 0, 0, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	bSizerStatusRightDirectories = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapSmallDirectoryRight = new wxStaticBitmap( m_panelStatusBar, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerStatusRightDirectories->Add( m_bitmapSmallDirectoryRight, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusRightDirectories->Add( 2, 0, 0, 0, 5 );
	
	m_staticTextStatusRightDirs = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusRightDirs->Wrap( -1 );
	bSizerStatusRightDirectories->Add( m_staticTextStatusRightDirs, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer52->Add( bSizerStatusRightDirectories, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	bSizerStatusRightFiles = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizerStatusRightFiles->Add( 10, 0, 0, 0, 5 );
	
	m_bitmapSmallFileRight = new wxStaticBitmap( m_panelStatusBar, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerStatusRightFiles->Add( m_bitmapSmallFileRight, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusRightFiles->Add( 2, 0, 0, 0, 5 );
	
	m_staticTextStatusRightFiles = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusRightFiles->Wrap( -1 );
	bSizerStatusRightFiles->Add( m_staticTextStatusRightFiles, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusRightFiles->Add( 4, 0, 0, 0, 5 );
	
	m_staticTextStatusRightBytes = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusRightBytes->Wrap( -1 );
	bSizerStatusRightFiles->Add( m_staticTextStatusRightBytes, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer52->Add( bSizerStatusRightFiles, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer52->Add( 0, 0, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusRight->Add( bSizer52, 1, wxEXPAND, 5 );
	
	
	bSizerFileStatus->Add( bSizerStatusRight, 1, wxEXPAND, 5 );
	
	
	bSizer451->Add( bSizerFileStatus, 1, wxEXPAND, 5 );
	
	m_staticTextFullStatus = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextFullStatus->Wrap( -1 );
	m_staticTextFullStatus->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer451->Add( m_staticTextFullStatus, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	m_panelStatusBar->SetSizer( bSizer451 );
	m_panelStatusBar->Layout();
	bSizer451->Fit( m_panelStatusBar );
	bSizer1711->Add( m_panelStatusBar, 0, wxEXPAND, 5 );
	
	
	m_panelCenter->SetSizer( bSizer1711 );
	m_panelCenter->Layout();
	bSizer1711->Fit( m_panelCenter );
	bSizerPanelHolder->Add( m_panelCenter, 1, wxEXPAND, 5 );
	
	m_panelSearch = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer1713;
	bSizer1713 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonHideSearch = new wxBitmapButton( m_panelSearch, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonHideSearch->SetToolTip( _("Close search bar") );
	
	bSizer1713->Add( m_bpButtonHideSearch, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText101 = new wxStaticText( m_panelSearch, wxID_ANY, _("Find:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText101->Wrap( -1 );
	bSizer1713->Add( m_staticText101, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_textCtrlSearchTxt = new wxTextCtrl( m_panelSearch, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_PROCESS_ENTER|wxWANTS_CHARS );
	bSizer1713->Add( m_textCtrlSearchTxt, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	m_checkBoxMatchCase = new wxCheckBox( m_panelSearch, wxID_ANY, _("Match case"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer1713->Add( m_checkBoxMatchCase, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	m_panelSearch->SetSizer( bSizer1713 );
	m_panelSearch->Layout();
	bSizer1713->Fit( m_panelSearch );
	bSizerPanelHolder->Add( m_panelSearch, 0, 0, 5 );
	
	m_panelLog = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelLog->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizerLog = new wxBoxSizer( wxVERTICAL );
	
	bSizer42 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapLogStatus = new wxStaticBitmap( m_panelLog, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer42->Add( m_bitmapLogStatus, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_staticTextLogStatus = new wxStaticText( m_panelLog, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextLogStatus->Wrap( -1 );
	m_staticTextLogStatus->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer42->Add( m_staticTextLogStatus, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_panelItemsProcessed = new wxPanel( m_panelLog, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelItemsProcessed->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer165;
	bSizer165 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer165->Add( 0, 5, 0, 0, 5 );
	
	wxStaticText* m_staticText962;
	m_staticText962 = new wxStaticText( m_panelItemsProcessed, wxID_ANY, _("Items processed:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText962->Wrap( -1 );
	bSizer165->Add( m_staticText962, 0, wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer169;
	bSizer169 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticTextItemsProcessed = new wxStaticText( m_panelItemsProcessed, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticTextItemsProcessed->Wrap( -1 );
	m_staticTextItemsProcessed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer169->Add( m_staticTextItemsProcessed, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextBytesProcessed = new wxStaticText( m_panelItemsProcessed, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextBytesProcessed->Wrap( -1 );
	bSizer169->Add( m_staticTextBytesProcessed, 0, wxLEFT|wxALIGN_BOTTOM, 5 );
	
	
	bSizer165->Add( bSizer169, 0, wxRIGHT|wxLEFT, 5 );
	
	
	bSizer165->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelItemsProcessed->SetSizer( bSizer165 );
	m_panelItemsProcessed->Layout();
	bSizer165->Fit( m_panelItemsProcessed );
	bSizer42->Add( m_panelItemsProcessed, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 10 );
	
	m_panelItemsRemaining = new wxPanel( m_panelLog, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelItemsRemaining->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer166;
	bSizer166 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer166->Add( 0, 5, 0, 0, 5 );
	
	wxStaticText* m_staticText971;
	m_staticText971 = new wxStaticText( m_panelItemsRemaining, wxID_ANY, _("Items remaining:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText971->Wrap( -1 );
	bSizer166->Add( m_staticText971, 0, wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer170;
	bSizer170 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticTextItemsRemaining = new wxStaticText( m_panelItemsRemaining, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticTextItemsRemaining->Wrap( -1 );
	m_staticTextItemsRemaining->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer170->Add( m_staticTextItemsRemaining, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextBytesRemaining = new wxStaticText( m_panelItemsRemaining, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextBytesRemaining->Wrap( -1 );
	bSizer170->Add( m_staticTextBytesRemaining, 0, wxLEFT|wxALIGN_BOTTOM, 5 );
	
	
	bSizer166->Add( bSizer170, 0, wxRIGHT|wxLEFT, 5 );
	
	
	bSizer166->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelItemsRemaining->SetSizer( bSizer166 );
	m_panelItemsRemaining->Layout();
	bSizer166->Fit( m_panelItemsRemaining );
	bSizer42->Add( m_panelItemsRemaining, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 10 );
	
	wxPanel* m_panelTimeElapsed;
	m_panelTimeElapsed = new wxPanel( m_panelLog, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelTimeElapsed->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer168;
	bSizer168 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer168->Add( 0, 5, 0, 0, 5 );
	
	wxStaticText* m_staticText9611;
	m_staticText9611 = new wxStaticText( m_panelTimeElapsed, wxID_ANY, _("Total time:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText9611->Wrap( -1 );
	bSizer168->Add( m_staticText9611, 0, wxRIGHT|wxLEFT, 5 );
	
	m_staticTextTotalTime = new wxStaticText( m_panelTimeElapsed, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextTotalTime->Wrap( -1 );
	m_staticTextTotalTime->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer168->Add( m_staticTextTotalTime, 0, wxRIGHT|wxLEFT, 5 );
	
	
	bSizer168->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelTimeElapsed->SetSizer( bSizer168 );
	m_panelTimeElapsed->Layout();
	bSizer168->Fit( m_panelTimeElapsed );
	bSizer42->Add( m_panelTimeElapsed, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 10 );
	
	
	bSizerLog->Add( bSizer42, 0, wxALL, 5 );
	
	m_staticline70 = new wxStaticLine( m_panelLog, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerLog->Add( m_staticline70, 0, wxEXPAND, 5 );
	
	
	m_panelLog->SetSizer( bSizerLog );
	m_panelLog->Layout();
	bSizerLog->Fit( m_panelLog );
	bSizerPanelHolder->Add( m_panelLog, 0, 0, 5 );
	
	m_panelConfig = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	bSizerConfig = new wxBoxSizer( wxHORIZONTAL );
	
	bSizerCfgHistoryButtons = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer17611;
	bSizer17611 = new wxBoxSizer( wxVERTICAL );
	
	m_bpButtonNew = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonNew->SetToolTip( _("dummy") );
	
	bSizer17611->Add( m_bpButtonNew, 0, wxEXPAND, 5 );
	
	m_staticText951 = new wxStaticText( m_panelConfig, wxID_ANY, _("New"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText951->Wrap( -1 );
	bSizer17611->Add( m_staticText951, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 2 );
	
	
	bSizerCfgHistoryButtons->Add( bSizer17611, 0, 0, 5 );
	
	wxBoxSizer* bSizer1761;
	bSizer1761 = new wxBoxSizer( wxVERTICAL );
	
	m_bpButtonOpen = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonOpen->SetToolTip( _("dummy") );
	
	bSizer1761->Add( m_bpButtonOpen, 0, wxEXPAND, 5 );
	
	m_staticText95 = new wxStaticText( m_panelConfig, wxID_ANY, _("Open..."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText95->Wrap( -1 );
	bSizer1761->Add( m_staticText95, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 2 );
	
	
	bSizerCfgHistoryButtons->Add( bSizer1761, 0, 0, 5 );
	
	wxBoxSizer* bSizer175;
	bSizer175 = new wxBoxSizer( wxVERTICAL );
	
	m_bpButtonSave = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSave->SetToolTip( _("dummy") );
	
	bSizer175->Add( m_bpButtonSave, 0, wxEXPAND, 5 );
	
	m_staticText961 = new wxStaticText( m_panelConfig, wxID_ANY, _("Save"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText961->Wrap( -1 );
	bSizer175->Add( m_staticText961, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 2 );
	
	
	bSizerCfgHistoryButtons->Add( bSizer175, 0, 0, 5 );
	
	wxBoxSizer* bSizer174;
	bSizer174 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer1772;
	bSizer1772 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonSaveAs = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSaveAs->SetToolTip( _("dummy") );
	
	bSizer1772->Add( m_bpButtonSaveAs, 1, 0, 5 );
	
	m_bpButtonSaveAsBatch = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSaveAsBatch->SetToolTip( _("dummy") );
	
	bSizer1772->Add( m_bpButtonSaveAsBatch, 1, 0, 5 );
	
	
	bSizer174->Add( bSizer1772, 0, wxEXPAND, 5 );
	
	m_staticText97 = new wxStaticText( m_panelConfig, wxID_ANY, _("Save as..."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText97->Wrap( -1 );
	bSizer174->Add( m_staticText97, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 2 );
	
	
	bSizerCfgHistoryButtons->Add( bSizer174, 0, 0, 5 );
	
	
	bSizerConfig->Add( bSizerCfgHistoryButtons, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_gridCfgHistory = new zen::Grid( m_panelConfig, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_gridCfgHistory->SetScrollRate( 5, 5 );
	bSizerConfig->Add( m_gridCfgHistory, 1, wxEXPAND, 5 );
	
	
	m_panelConfig->SetSizer( bSizerConfig );
	m_panelConfig->Layout();
	bSizerConfig->Fit( m_panelConfig );
	bSizerPanelHolder->Add( m_panelConfig, 0, 0, 5 );
	
	m_panelViewFilter = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	bSizerViewFilter = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonShowLog = new wxBitmapButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowLog, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerViewFilter->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextViewType = new wxStaticText( m_panelViewFilter, wxID_ANY, _("View type:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextViewType->Wrap( -1 );
	bSizerViewFilter->Add( m_staticTextViewType, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonViewTypeSyncAction = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonViewTypeSyncAction, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxRIGHT, 5 );
	
	
	bSizerViewFilter->Add( 0, 0, 3, wxEXPAND, 5 );
	
	m_staticTextSelectView = new wxStaticText( m_panelViewFilter, wxID_ANY, _("Select view:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextSelectView->Wrap( -1 );
	bSizerViewFilter->Add( m_staticTextSelectView, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowExcluded = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowExcluded, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxRIGHT, 5 );
	
	m_bpButtonShowDeleteLeft = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowDeleteLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowUpdateLeft = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowUpdateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowCreateLeft = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowCreateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowLeftOnly = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowLeftOnly, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowLeftNewer = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowLeftNewer, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowEqual = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowEqual, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowDoNothing = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowDoNothing, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowDifferent = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowDifferent, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowRightNewer = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowRightNewer, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowRightOnly = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowRightOnly, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowCreateRight = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowCreateRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowUpdateRight = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowUpdateRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowDeleteRight = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowDeleteRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowConflict = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowConflict, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonViewFilterSave = new wxBitmapButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW );
	m_bpButtonViewFilterSave->SetToolTip( _("Save as default") );
	
	bSizerViewFilter->Add( m_bpButtonViewFilterSave, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	bSizerViewFilter->Add( 0, 0, 3, wxEXPAND, 5 );
	
	m_staticText96 = new wxStaticText( m_panelViewFilter, wxID_ANY, _("Statistics:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText96->Wrap( -1 );
	bSizerViewFilter->Add( m_staticText96, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_panelStatistics = new wxPanel( m_panelViewFilter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
	m_panelStatistics->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizer1801 = new wxBoxSizer( wxVERTICAL );
	
	bSizerStatistics = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer173;
	bSizer173 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapDeleteLeft = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapDeleteLeft->SetToolTip( _("Number of files and folders that will be deleted") );
	
	bSizer173->Add( m_bitmapDeleteLeft, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer173->Add( 5, 2, 0, 0, 5 );
	
	
	bSizer173->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextDeleteLeft = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDeleteLeft->Wrap( -1 );
	m_staticTextDeleteLeft->SetToolTip( _("Number of files and folders that will be deleted") );
	
	bSizer173->Add( m_staticTextDeleteLeft, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatistics->Add( bSizer173, 0, wxEXPAND, 5 );
	
	
	bSizerStatistics->Add( 5, 5, 0, 0, 5 );
	
	wxBoxSizer* bSizer172;
	bSizer172 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapUpdateLeft = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapUpdateLeft->SetToolTip( _("Number of files that will be updated") );
	
	bSizer172->Add( m_bitmapUpdateLeft, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer172->Add( 5, 2, 0, 0, 5 );
	
	
	bSizer172->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextUpdateLeft = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextUpdateLeft->Wrap( -1 );
	m_staticTextUpdateLeft->SetToolTip( _("Number of files that will be updated") );
	
	bSizer172->Add( m_staticTextUpdateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerStatistics->Add( bSizer172, 0, wxEXPAND, 5 );
	
	
	bSizerStatistics->Add( 5, 5, 0, 0, 5 );
	
	wxBoxSizer* bSizer1712;
	bSizer1712 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapCreateLeft = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapCreateLeft->SetToolTip( _("Number of files and folders that will be created") );
	
	bSizer1712->Add( m_bitmapCreateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer1712->Add( 5, 2, 0, 0, 5 );
	
	
	bSizer1712->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextCreateLeft = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextCreateLeft->Wrap( -1 );
	m_staticTextCreateLeft->SetToolTip( _("Number of files and folders that will be created") );
	
	bSizer1712->Add( m_staticTextCreateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerStatistics->Add( bSizer1712, 0, wxEXPAND, 5 );
	
	
	bSizerStatistics->Add( 5, 5, 0, 0, 5 );
	
	bSizerData = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapData = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapData->SetToolTip( _("Total bytes to copy") );
	
	bSizerData->Add( m_bitmapData, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerData->Add( 5, 2, 0, 0, 5 );
	
	
	bSizerData->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextData = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextData->Wrap( -1 );
	m_staticTextData->SetToolTip( _("Total bytes to copy") );
	
	bSizerData->Add( m_staticTextData, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatistics->Add( bSizerData, 0, wxEXPAND, 5 );
	
	
	bSizerStatistics->Add( 5, 5, 0, 0, 5 );
	
	wxBoxSizer* bSizer178;
	bSizer178 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapCreateRight = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapCreateRight->SetToolTip( _("Number of files and folders that will be created") );
	
	bSizer178->Add( m_bitmapCreateRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer178->Add( 5, 2, 0, 0, 5 );
	
	
	bSizer178->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextCreateRight = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextCreateRight->Wrap( -1 );
	m_staticTextCreateRight->SetToolTip( _("Number of files and folders that will be created") );
	
	bSizer178->Add( m_staticTextCreateRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatistics->Add( bSizer178, 0, wxEXPAND, 5 );
	
	
	bSizerStatistics->Add( 5, 5, 0, 0, 5 );
	
	wxBoxSizer* bSizer177;
	bSizer177 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapUpdateRight = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapUpdateRight->SetToolTip( _("Number of files that will be updated") );
	
	bSizer177->Add( m_bitmapUpdateRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer177->Add( 5, 2, 0, 0, 5 );
	
	
	bSizer177->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextUpdateRight = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextUpdateRight->Wrap( -1 );
	m_staticTextUpdateRight->SetToolTip( _("Number of files that will be updated") );
	
	bSizer177->Add( m_staticTextUpdateRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatistics->Add( bSizer177, 0, wxEXPAND, 5 );
	
	
	bSizerStatistics->Add( 5, 5, 0, 0, 5 );
	
	wxBoxSizer* bSizer176;
	bSizer176 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapDeleteRight = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapDeleteRight->SetToolTip( _("Number of files and folders that will be deleted") );
	
	bSizer176->Add( m_bitmapDeleteRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer176->Add( 5, 2, 0, 0, 5 );
	
	
	bSizer176->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextDeleteRight = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDeleteRight->Wrap( -1 );
	m_staticTextDeleteRight->SetToolTip( _("Number of files and folders that will be deleted") );
	
	bSizer176->Add( m_staticTextDeleteRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatistics->Add( bSizer176, 0, wxEXPAND, 5 );
	
	
	bSizer1801->Add( bSizerStatistics, 0, wxALL, 4 );
	
	
	m_panelStatistics->SetSizer( bSizer1801 );
	m_panelStatistics->Layout();
	bSizer1801->Fit( m_panelStatistics );
	bSizerViewFilter->Add( m_panelStatistics, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	m_panelViewFilter->SetSizer( bSizerViewFilter );
	m_panelViewFilter->Layout();
	bSizerViewFilter->Fit( m_panelViewFilter );
	bSizerPanelHolder->Add( m_panelViewFilter, 0, 0, 5 );
	
	
	this->SetSizer( bSizerPanelHolder );
	this->Layout();
	bSizerPanelHolder->Fit( this );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( MainDialogGenerated::OnClose ) );
	this->Connect( m_menuItemNew->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnConfigNew ) );
	this->Connect( m_menuItemLoad->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnConfigLoad ) );
	this->Connect( m_menuItemSave->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnConfigSave ) );
	this->Connect( m_menuItemSaveAs->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnConfigSaveAs ) );
	this->Connect( m_menuItemSaveAsBatch->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnSaveAsBatchJob ) );
	this->Connect( m_menuItem4->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuQuit ) );
	this->Connect( m_menuItemShowLog->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnShowLog ) );
	this->Connect( m_menuItemCompare->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnCompare ) );
	this->Connect( m_menuItemCompSettings->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnCmpSettings ) );
	this->Connect( m_menuItemFilter->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnConfigureFilter ) );
	this->Connect( m_menuItemSyncSettings->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnSyncSettings ) );
	this->Connect( m_menuItemSynchronize->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnStartSync ) );
	this->Connect( m_menuItemOptions->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuOptions ) );
	this->Connect( m_menuItemFind->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuFindItem ) );
	this->Connect( m_menuItemExportList->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuExportFileList ) );
	this->Connect( m_menuItem51->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuResetLayout ) );
	this->Connect( m_menuItemHelp->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnShowHelp ) );
	this->Connect( m_menuItemCheckVersionNow->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuCheckVersion ) );
	this->Connect( m_menuItemCheckVersionAuto->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuCheckVersionAutomatically ) );
	this->Connect( m_menuItemAbout->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuAbout ) );
	m_buttonCompare->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnCompare ), NULL, this );
	m_bpButtonCmpConfig->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnCmpSettings ), NULL, this );
	m_bpButtonCmpConfig->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnCompSettingsContext ), NULL, this );
	m_bpButtonCmpContext->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnCompSettingsContext ), NULL, this );
	m_bpButtonCmpContext->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnCompSettingsContext ), NULL, this );
	m_bpButtonFilter->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnConfigureFilter ), NULL, this );
	m_bpButtonFilter->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnGlobalFilterContext ), NULL, this );
	m_bpButtonFilterContext->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnGlobalFilterContext ), NULL, this );
	m_bpButtonFilterContext->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnGlobalFilterContext ), NULL, this );
	m_bpButtonSyncConfig->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnSyncSettings ), NULL, this );
	m_bpButtonSyncConfig->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnSyncSettingsContext ), NULL, this );
	m_bpButtonSyncContext->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnSyncSettingsContext ), NULL, this );
	m_bpButtonSyncContext->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnSyncSettingsContext ), NULL, this );
	m_buttonSync->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnStartSync ), NULL, this );
	m_bpButtonAddPair->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnTopFolderPairAdd ), NULL, this );
	m_bpButtonRemovePair->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnTopFolderPairRemove ), NULL, this );
	m_bpButtonSwapSides->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnSwapSides ), NULL, this );
	m_bpButtonLocalCompCfg->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnTopLocalCompCfg ), NULL, this );
	m_bpButtonLocalFilter->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnTopLocalFilterCfg ), NULL, this );
	m_bpButtonLocalSyncCfg->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnTopLocalSyncCfg ), NULL, this );
	m_bpButtonHideSearch->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnHideSearchPanel ), NULL, this );
	m_textCtrlSearchTxt->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainDialogGenerated::OnSearchGridEnter ), NULL, this );
	m_bpButtonNew->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnConfigNew ), NULL, this );
	m_bpButtonOpen->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnConfigLoad ), NULL, this );
	m_bpButtonSave->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnConfigSave ), NULL, this );
	m_bpButtonSaveAs->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnConfigSaveAs ), NULL, this );
	m_bpButtonSaveAsBatch->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnSaveAsBatchJob ), NULL, this );
	m_bpButtonShowLog->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnShowLog ), NULL, this );
	m_bpButtonViewTypeSyncAction->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewType ), NULL, this );
	m_bpButtonShowExcluded->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowDeleteLeft->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowUpdateLeft->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowCreateLeft->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowLeftOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowLeftNewer->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowEqual->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowDoNothing->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowDifferent->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowRightNewer->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowRightOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowCreateRight->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowUpdateRight->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowDeleteRight->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowConflict->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonViewFilterSave->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnViewFilterSave ), NULL, this );
}

MainDialogGenerated::~MainDialogGenerated()
{
}

FolderPairPanelGenerated::FolderPairPanelGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) : wxPanel( parent, id, pos, size, style )
{
	wxBoxSizer* bSizer74;
	bSizer74 = new wxBoxSizer( wxHORIZONTAL );
	
	m_panelLeft = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelLeft->SetMinSize( wxSize( 1,-1 ) );
	
	wxBoxSizer* bSizer134;
	bSizer134 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonFolderPairOptions = new wxBitmapButton( m_panelLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonFolderPairOptions->SetToolTip( _("Arrange folder pair") );
	
	bSizer134->Add( m_bpButtonFolderPairOptions, 0, wxEXPAND, 5 );
	
	m_bpButtonRemovePair = new wxBitmapButton( m_panelLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonRemovePair->SetToolTip( _("Remove folder pair") );
	
	bSizer134->Add( m_bpButtonRemovePair, 0, wxEXPAND, 5 );
	
	m_folderPathLeft = new fff::FolderHistoryBox( m_panelLeft, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer134->Add( m_folderPathLeft, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectFolderLeft = new wxButton( m_panelLeft, wxID_ANY, _("Browse"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonSelectFolderLeft->SetToolTip( _("Select a folder") );
	
	bSizer134->Add( m_buttonSelectFolderLeft, 0, wxEXPAND, 5 );
	
	m_bpButtonSelectAltFolderLeft = new wxBitmapButton( m_panelLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSelectAltFolderLeft->SetToolTip( _("Access online storage") );
	
	bSizer134->Add( m_bpButtonSelectAltFolderLeft, 0, wxEXPAND, 5 );
	
	
	m_panelLeft->SetSizer( bSizer134 );
	m_panelLeft->Layout();
	bSizer134->Fit( m_panelLeft );
	bSizer74->Add( m_panelLeft, 0, wxLEFT|wxEXPAND, 5 );
	
	m_panel20 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer95;
	bSizer95 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonLocalCompCfg = new wxBitmapButton( m_panel20, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonLocalCompCfg->SetToolTip( _("dummy") );
	
	bSizer95->Add( m_bpButtonLocalCompCfg, 0, wxEXPAND, 5 );
	
	m_bpButtonLocalFilter = new wxBitmapButton( m_panel20, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonLocalFilter->SetToolTip( _("dummy") );
	
	bSizer95->Add( m_bpButtonLocalFilter, 0, wxEXPAND, 5 );
	
	m_bpButtonLocalSyncCfg = new wxBitmapButton( m_panel20, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonLocalSyncCfg->SetToolTip( _("dummy") );
	
	bSizer95->Add( m_bpButtonLocalSyncCfg, 0, wxEXPAND, 5 );
	
	
	m_panel20->SetSizer( bSizer95 );
	m_panel20->Layout();
	bSizer95->Fit( m_panel20 );
	bSizer74->Add( m_panel20, 0, wxRIGHT|wxLEFT|wxEXPAND, 5 );
	
	m_panelRight = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelRight->SetMinSize( wxSize( 1,-1 ) );
	
	wxBoxSizer* bSizer135;
	bSizer135 = new wxBoxSizer( wxHORIZONTAL );
	
	m_folderPathRight = new fff::FolderHistoryBox( m_panelRight, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer135->Add( m_folderPathRight, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectFolderRight = new wxButton( m_panelRight, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectFolderRight->SetToolTip( _("Select a folder") );
	
	bSizer135->Add( m_buttonSelectFolderRight, 0, wxEXPAND, 5 );
	
	m_bpButtonSelectAltFolderRight = new wxBitmapButton( m_panelRight, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSelectAltFolderRight->SetToolTip( _("Access online storage") );
	
	bSizer135->Add( m_bpButtonSelectAltFolderRight, 0, wxEXPAND, 5 );
	
	
	m_panelRight->SetSizer( bSizer135 );
	m_panelRight->Layout();
	bSizer135->Fit( m_panelRight );
	bSizer74->Add( m_panelRight, 1, wxRIGHT|wxEXPAND, 5 );
	
	
	this->SetSizer( bSizer74 );
	this->Layout();
	bSizer74->Fit( this );
}

FolderPairPanelGenerated::~FolderPairPanelGenerated()
{
}

ConfigDlgGenerated::ConfigDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer7;
	bSizer7 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer190;
	bSizer190 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer1911;
	bSizer1911 = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextFolderPairLabel = new wxStaticText( this, wxID_ANY, _("Folder pair:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextFolderPairLabel->Wrap( -1 );
	bSizer1911->Add( m_staticTextFolderPairLabel, 0, wxALL, 5 );
	
	m_listBoxFolderPair = new wxListBox( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_NEEDED_SB ); 
	bSizer1911->Add( m_listBoxFolderPair, 1, 0, 5 );
	
	
	bSizer190->Add( bSizer1911, 0, wxEXPAND|wxLEFT, 5 );
	
	m_notebook = new wxNotebook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelCompSettingsTab = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelCompSettingsTab->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer275;
	bSizer275 = new wxBoxSizer( wxVERTICAL );
	
	bSizerHeaderCompSettings = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextMainCompSettings = new wxStaticText( m_panelCompSettingsTab, wxID_ANY, _("Main settings:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextMainCompSettings->Wrap( -1 );
	bSizerHeaderCompSettings->Add( m_staticTextMainCompSettings, 0, wxALL, 10 );
	
	m_checkBoxUseLocalCmpOptions = new wxCheckBox( m_panelCompSettingsTab, wxID_ANY, _("Use local settings:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxUseLocalCmpOptions->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizerHeaderCompSettings->Add( m_checkBoxUseLocalCmpOptions, 0, wxALL|wxEXPAND, 10 );
	
	m_staticlineCompHeader = new wxStaticLine( m_panelCompSettingsTab, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerHeaderCompSettings->Add( m_staticlineCompHeader, 0, wxEXPAND, 5 );
	
	
	bSizer275->Add( bSizerHeaderCompSettings, 0, wxEXPAND, 5 );
	
	m_panelComparisonSettings = new wxPanel( m_panelCompSettingsTab, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelComparisonSettings->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer2561;
	bSizer2561 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer159;
	bSizer159 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer178;
	bSizer178 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer182;
	bSizer182 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText91 = new wxStaticText( m_panelComparisonSettings, wxID_ANY, _("Select a variant:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText91->Wrap( -1 );
	bSizer182->Add( m_staticText91, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer2381;
	bSizer2381 = new wxBoxSizer( wxVERTICAL );
	
	m_toggleBtnByTimeSize = new wxToggleButton( m_panelComparisonSettings, wxID_ANY, _("File time and size"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_toggleBtnByTimeSize->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer2381->Add( m_toggleBtnByTimeSize, 0, wxEXPAND|wxBOTTOM, 5 );
	
	m_toggleBtnByContent = new wxToggleButton( m_panelComparisonSettings, wxID_ANY, _("File content"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_toggleBtnByContent->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer2381->Add( m_toggleBtnByContent, 0, wxEXPAND|wxBOTTOM, 5 );
	
	m_toggleBtnBySize = new wxToggleButton( m_panelComparisonSettings, wxID_ANY, _("File size"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_toggleBtnBySize->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer2381->Add( m_toggleBtnBySize, 0, wxEXPAND, 5 );
	
	
	bSizer182->Add( bSizer2381, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer178->Add( bSizer182, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer2371;
	bSizer2371 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapCompVariant = new wxStaticBitmap( m_panelComparisonSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer2371->Add( m_bitmapCompVariant, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextCompVarDescription = new wxStaticText( m_panelComparisonSettings, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextCompVarDescription->Wrap( -1 );
	m_staticTextCompVarDescription->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer2371->Add( m_staticTextCompVarDescription, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer178->Add( bSizer2371, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	bSizer159->Add( bSizer178, 0, wxEXPAND, 5 );
	
	m_staticline33 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer159->Add( m_staticline33, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer1734;
	bSizer1734 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer1721;
	bSizer1721 = new wxBoxSizer( wxVERTICAL );
	
	m_checkBoxSymlinksInclude = new wxCheckBox( m_panelComparisonSettings, wxID_ANY, _("Include &symbolic links:"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer1721->Add( m_checkBoxSymlinksInclude, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer176;
	bSizer176 = new wxBoxSizer( wxVERTICAL );
	
	m_radioBtnSymlinksFollow = new wxRadioButton( m_panelComparisonSettings, wxID_ANY, _("&Follow"), wxDefaultPosition, wxDefaultSize, 0 );
	m_radioBtnSymlinksFollow->SetValue( true ); 
	bSizer176->Add( m_radioBtnSymlinksFollow, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	m_radioBtnSymlinksDirect = new wxRadioButton( m_panelComparisonSettings, wxID_ANY, _("&Direct"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer176->Add( m_radioBtnSymlinksDirect, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer1721->Add( bSizer176, 0, wxLEFT|wxEXPAND, 18 );
	
	
	bSizer1721->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_hyperlink24 = new wxHyperlinkCtrl( m_panelComparisonSettings, wxID_ANY, _("More information"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer1721->Add( m_hyperlink24, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer1734->Add( bSizer1721, 0, wxALL|wxEXPAND, 5 );
	
	m_staticline44 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer1734->Add( m_staticline44, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer1733;
	bSizer1733 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText112 = new wxStaticText( m_panelComparisonSettings, wxID_ANY, _("&Ignore time shift [hh:mm]"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText112->Wrap( -1 );
	bSizer1733->Add( m_staticText112, 0, wxALL, 5 );
	
	m_textCtrlTimeShift = new wxTextCtrl( m_panelComparisonSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_textCtrlTimeShift->SetToolTip( _("List of file time offsets to ignore") );
	
	bSizer1733->Add( m_textCtrlTimeShift, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer197;
	bSizer197 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText1381 = new wxStaticText( m_panelComparisonSettings, wxID_ANY, _("Example:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1381->Wrap( -1 );
	m_staticText1381->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer197->Add( m_staticText1381, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	m_staticText13811 = new wxStaticText( m_panelComparisonSettings, wxID_ANY, _("1, 2, 4:30"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText13811->Wrap( -1 );
	m_staticText13811->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer197->Add( m_staticText13811, 0, wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer1733->Add( bSizer197, 0, 0, 5 );
	
	
	bSizer1733->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_hyperlink241 = new wxHyperlinkCtrl( m_panelComparisonSettings, wxID_ANY, _("Handle daylight saving time"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer1733->Add( m_hyperlink241, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer1734->Add( bSizer1733, 0, wxALL|wxEXPAND, 5 );
	
	m_staticline441 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer1734->Add( m_staticline441, 0, wxEXPAND, 5 );
	
	
	bSizer159->Add( bSizer1734, 0, 0, 5 );
	
	m_staticline331 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer159->Add( m_staticline331, 0, wxEXPAND, 5 );
	
	bSizerCompMisc = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer2781;
	bSizer2781 = new wxBoxSizer( wxHORIZONTAL );
	
	wxFlexGridSizer* fgSizer61;
	fgSizer61 = new wxFlexGridSizer( 0, 2, 5, 5 );
	fgSizer61->SetFlexibleDirection( wxBOTH );
	fgSizer61->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_bitmapIgnoreErrors = new wxStaticBitmap( m_panelComparisonSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer61->Add( m_bitmapIgnoreErrors, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_checkBoxIgnoreErrors = new wxCheckBox( m_panelComparisonSettings, wxID_ANY, _("Ignore errors"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	fgSizer61->Add( m_checkBoxIgnoreErrors, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	m_bitmapRetryErrors = new wxStaticBitmap( m_panelComparisonSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer61->Add( m_bitmapRetryErrors, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_checkBoxAutoRetry = new wxCheckBox( m_panelComparisonSettings, wxID_ANY, _("Automatic retry"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer61->Add( m_checkBoxAutoRetry, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer2781->Add( fgSizer61, 0, wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	fgSizerAutoRetry = new wxFlexGridSizer( 0, 2, 5, 10 );
	fgSizerAutoRetry->SetFlexibleDirection( wxBOTH );
	fgSizerAutoRetry->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_staticText96 = new wxStaticText( m_panelComparisonSettings, wxID_ANY, _("Retry count:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText96->Wrap( -1 );
	fgSizerAutoRetry->Add( m_staticText96, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextAutoRetryDelay = new wxStaticText( m_panelComparisonSettings, wxID_ANY, _("Delay (in seconds):"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextAutoRetryDelay->Wrap( -1 );
	fgSizerAutoRetry->Add( m_staticTextAutoRetryDelay, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_spinCtrlAutoRetryCount = new wxSpinCtrl( m_panelComparisonSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
	fgSizerAutoRetry->Add( m_spinCtrlAutoRetryCount, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_spinCtrlAutoRetryDelay = new wxSpinCtrl( m_panelComparisonSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxSP_ARROW_KEYS, 0, 2000000000, 0 );
	fgSizerAutoRetry->Add( m_spinCtrlAutoRetryDelay, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer2781->Add( fgSizerAutoRetry, 0, wxTOP|wxBOTTOM|wxRIGHT|wxALIGN_CENTER_VERTICAL, 10 );
	
	
	bSizerCompMisc->Add( bSizer2781, 0, wxEXPAND, 5 );
	
	m_staticline3311 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerCompMisc->Add( m_staticline3311, 0, wxEXPAND, 5 );
	
	
	bSizer159->Add( bSizerCompMisc, 0, wxEXPAND, 5 );
	
	
	bSizer2561->Add( bSizer159, 0, wxEXPAND, 5 );
	
	m_staticline751 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer2561->Add( m_staticline751, 0, wxEXPAND, 5 );
	
	bSizerPerformance = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextPerfDeRequired = new wxStaticText( m_panelComparisonSettings, wxID_ANY, _("Requires FreeFileSync Donation Edition"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextPerfDeRequired->Wrap( -1 );
	bSizerPerformance->Add( m_staticTextPerfDeRequired, 0, wxALL, 5 );
	
	m_staticlinePerfDeRequired = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerPerformance->Add( m_staticlinePerfDeRequired, 0, wxEXPAND, 5 );
	
	m_panelPerfHeader = new wxPanel( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelPerfHeader->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer2191;
	bSizer2191 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapPerf = new wxStaticBitmap( m_panelPerfHeader, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer2191->Add( m_bitmapPerf, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_staticText13611 = new wxStaticText( m_panelPerfHeader, wxID_ANY, _("Performance improvements:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText13611->Wrap( -1 );
	bSizer2191->Add( m_staticText13611, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 10 );
	
	
	m_panelPerfHeader->SetSizer( bSizer2191 );
	m_panelPerfHeader->Layout();
	bSizer2191->Fit( m_panelPerfHeader );
	bSizerPerformance->Add( m_panelPerfHeader, 0, wxEXPAND, 5 );
	
	wxStaticLine* m_staticline75;
	m_staticline75 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerPerformance->Add( m_staticline75, 0, wxEXPAND, 5 );
	
	bSizer260 = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextPerfParallelOps = new wxStaticText( m_panelComparisonSettings, wxID_ANY, _("Parallel file operations:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextPerfParallelOps->Wrap( -1 );
	bSizer260->Add( m_staticTextPerfParallelOps, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	
	m_scrolledWindowPerf = new wxScrolledWindow( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_scrolledWindowPerf->SetScrollRate( 5, 5 );
	m_scrolledWindowPerf->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	fgSizerPerf = new wxFlexGridSizer( 0, 2, 5, 5 );
	fgSizerPerf->SetFlexibleDirection( wxBOTH );
	fgSizerPerf->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	
	m_scrolledWindowPerf->SetSizer( fgSizerPerf );
	m_scrolledWindowPerf->Layout();
	fgSizerPerf->Fit( m_scrolledWindowPerf );
	bSizer260->Add( m_scrolledWindowPerf, 1, wxALL|wxEXPAND, 5 );
	
	m_hyperlink1711 = new wxHyperlinkCtrl( m_panelComparisonSettings, wxID_ANY, _("How to get best performance?"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer260->Add( m_hyperlink1711, 0, wxALL, 5 );
	
	
	bSizerPerformance->Add( bSizer260, 1, wxALL|wxEXPAND, 5 );
	
	
	bSizer2561->Add( bSizerPerformance, 1, wxEXPAND, 5 );
	
	
	m_panelComparisonSettings->SetSizer( bSizer2561 );
	m_panelComparisonSettings->Layout();
	bSizer2561->Fit( m_panelComparisonSettings );
	bSizer275->Add( m_panelComparisonSettings, 1, wxEXPAND, 5 );
	
	
	m_panelCompSettingsTab->SetSizer( bSizer275 );
	m_panelCompSettingsTab->Layout();
	bSizer275->Fit( m_panelCompSettingsTab );
	m_notebook->AddPage( m_panelCompSettingsTab, _("dummy"), false );
	m_panelFilterSettingsTab = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelFilterSettingsTab->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer278;
	bSizer278 = new wxBoxSizer( wxVERTICAL );
	
	bSizerHeaderFilterSettings = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextMainFilterSettings = new wxStaticText( m_panelFilterSettingsTab, wxID_ANY, _("Main settings:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextMainFilterSettings->Wrap( -1 );
	bSizerHeaderFilterSettings->Add( m_staticTextMainFilterSettings, 0, wxALL, 10 );
	
	m_staticTextLocalFilterSettings = new wxStaticText( m_panelFilterSettingsTab, wxID_ANY, _("Local settings:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextLocalFilterSettings->Wrap( -1 );
	bSizerHeaderFilterSettings->Add( m_staticTextLocalFilterSettings, 0, wxALL, 10 );
	
	m_staticlineFilterHeader = new wxStaticLine( m_panelFilterSettingsTab, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerHeaderFilterSettings->Add( m_staticlineFilterHeader, 0, wxEXPAND, 5 );
	
	
	bSizer278->Add( bSizerHeaderFilterSettings, 0, wxEXPAND, 5 );
	
	m_panelFilterSettings = new wxPanel( m_panelFilterSettingsTab, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelFilterSettings->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer1591;
	bSizer1591 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer166;
	bSizer166 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer166->Add( 0, 10, 0, 0, 5 );
	
	wxBoxSizer* bSizer1661;
	bSizer1661 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapInclude = new wxStaticBitmap( m_panelFilterSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer1661->Add( m_bitmapInclude, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );
	
	wxBoxSizer* bSizer1731;
	bSizer1731 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText78 = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("Include:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText78->Wrap( -1 );
	bSizer1731->Add( m_staticText78, 0, 0, 5 );
	
	m_textCtrlInclude = new wxTextCtrl( m_panelFilterSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_MULTILINE );
	bSizer1731->Add( m_textCtrlInclude, 1, wxEXPAND|wxTOP, 5 );
	
	
	bSizer1661->Add( bSizer1731, 1, wxEXPAND, 5 );
	
	
	bSizer166->Add( bSizer1661, 3, wxEXPAND|wxLEFT, 5 );
	
	
	bSizer166->Add( 0, 10, 0, 0, 5 );
	
	wxBoxSizer* bSizer1651;
	bSizer1651 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapExclude = new wxStaticBitmap( m_panelFilterSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer1651->Add( m_bitmapExclude, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	wxBoxSizer* bSizer1742;
	bSizer1742 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer189;
	bSizer189 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText77 = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("Exclude:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText77->Wrap( -1 );
	bSizer189->Add( m_staticText77, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer189->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_hyperlink171 = new wxHyperlinkCtrl( m_panelFilterSettings, wxID_ANY, _("Show examples"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer189->Add( m_hyperlink171, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer1742->Add( bSizer189, 0, wxEXPAND, 5 );
	
	m_textCtrlExclude = new wxTextCtrl( m_panelFilterSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_MULTILINE );
	bSizer1742->Add( m_textCtrlExclude, 1, wxEXPAND|wxTOP, 5 );
	
	
	bSizer1651->Add( bSizer1742, 1, wxEXPAND, 5 );
	
	
	bSizer166->Add( bSizer1651, 5, wxEXPAND|wxLEFT, 5 );
	
	m_staticTextFilterDescr = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("Select filter rules to exclude certain files from synchronization. Enter file paths relative to their corresponding folder pair."), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticTextFilterDescr->Wrap( -1 );
	m_staticTextFilterDescr->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer166->Add( m_staticTextFilterDescr, 0, wxALL, 10 );
	
	
	bSizer1591->Add( bSizer166, 1, wxEXPAND, 5 );
	
	m_staticline24 = new wxStaticLine( m_panelFilterSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer1591->Add( m_staticline24, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer160;
	bSizer160 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer168;
	bSizer168 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapFilterSize = new wxStaticBitmap( m_panelFilterSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer168->Add( m_bitmapFilterSize, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );
	
	wxBoxSizer* bSizer158;
	bSizer158 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText80 = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("File size:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText80->Wrap( -1 );
	bSizer158->Add( m_staticText80, 0, wxBOTTOM, 5 );
	
	wxBoxSizer* bSizer162;
	bSizer162 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText101 = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("Minimum:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText101->Wrap( -1 );
	bSizer162->Add( m_staticText101, 0, wxBOTTOM, 2 );
	
	m_spinCtrlMinSize = new wxSpinCtrl( m_panelFilterSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 2000000000, 0 );
	bSizer162->Add( m_spinCtrlMinSize, 0, wxEXPAND, 5 );
	
	wxArrayString m_choiceUnitMinSizeChoices;
	m_choiceUnitMinSize = new wxChoice( m_panelFilterSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceUnitMinSizeChoices, 0 );
	m_choiceUnitMinSize->SetSelection( 0 );
	bSizer162->Add( m_choiceUnitMinSize, 0, wxEXPAND, 5 );
	
	
	bSizer158->Add( bSizer162, 0, wxBOTTOM|wxEXPAND, 5 );
	
	wxBoxSizer* bSizer163;
	bSizer163 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText102 = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("Maximum:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText102->Wrap( -1 );
	bSizer163->Add( m_staticText102, 0, wxBOTTOM, 2 );
	
	m_spinCtrlMaxSize = new wxSpinCtrl( m_panelFilterSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 2000000000, 0 );
	bSizer163->Add( m_spinCtrlMaxSize, 0, wxEXPAND, 5 );
	
	wxArrayString m_choiceUnitMaxSizeChoices;
	m_choiceUnitMaxSize = new wxChoice( m_panelFilterSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceUnitMaxSizeChoices, 0 );
	m_choiceUnitMaxSize->SetSelection( 0 );
	bSizer163->Add( m_choiceUnitMaxSize, 0, wxEXPAND, 5 );
	
	
	bSizer158->Add( bSizer163, 0, wxEXPAND, 5 );
	
	
	bSizer168->Add( bSizer158, 1, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer160->Add( bSizer168, 2, wxEXPAND|wxALL, 5 );
	
	m_staticline23 = new wxStaticLine( m_panelFilterSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer160->Add( m_staticline23, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer167;
	bSizer167 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapFilterDate = new wxStaticBitmap( m_panelFilterSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer167->Add( m_bitmapFilterDate, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	wxBoxSizer* bSizer165;
	bSizer165 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText79 = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("Time span:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText79->Wrap( -1 );
	bSizer165->Add( m_staticText79, 0, wxBOTTOM, 5 );
	
	m_spinCtrlTimespan = new wxSpinCtrl( m_panelFilterSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 2000000000, 0 );
	bSizer165->Add( m_spinCtrlTimespan, 0, wxEXPAND, 5 );
	
	wxArrayString m_choiceUnitTimespanChoices;
	m_choiceUnitTimespan = new wxChoice( m_panelFilterSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceUnitTimespanChoices, 0 );
	m_choiceUnitTimespan->SetSelection( 0 );
	bSizer165->Add( m_choiceUnitTimespan, 0, wxEXPAND, 5 );
	
	
	bSizer167->Add( bSizer165, 1, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer160->Add( bSizer167, 1, wxEXPAND|wxALL, 5 );
	
	m_staticline231 = new wxStaticLine( m_panelFilterSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer160->Add( m_staticline231, 0, wxEXPAND, 5 );
	
	m_buttonClear = new wxButton( m_panelFilterSettings, wxID_ANY, _("C&lear"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer160->Add( m_buttonClear, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 10 );
	
	
	bSizer1591->Add( bSizer160, 0, wxEXPAND, 5 );
	
	
	m_panelFilterSettings->SetSizer( bSizer1591 );
	m_panelFilterSettings->Layout();
	bSizer1591->Fit( m_panelFilterSettings );
	bSizer278->Add( m_panelFilterSettings, 1, wxEXPAND, 5 );
	
	
	m_panelFilterSettingsTab->SetSizer( bSizer278 );
	m_panelFilterSettingsTab->Layout();
	bSizer278->Fit( m_panelFilterSettingsTab );
	m_notebook->AddPage( m_panelFilterSettingsTab, _("dummy"), false );
	m_panelSyncSettingsTab = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelSyncSettingsTab->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer276;
	bSizer276 = new wxBoxSizer( wxVERTICAL );
	
	bSizerHeaderSyncSettings = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextMainSyncSettings = new wxStaticText( m_panelSyncSettingsTab, wxID_ANY, _("Main settings:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextMainSyncSettings->Wrap( -1 );
	bSizerHeaderSyncSettings->Add( m_staticTextMainSyncSettings, 0, wxALL, 10 );
	
	m_checkBoxUseLocalSyncOptions = new wxCheckBox( m_panelSyncSettingsTab, wxID_ANY, _("Use local settings:"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizerHeaderSyncSettings->Add( m_checkBoxUseLocalSyncOptions, 0, wxALL|wxEXPAND, 10 );
	
	m_staticlineSyncHeader = new wxStaticLine( m_panelSyncSettingsTab, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerHeaderSyncSettings->Add( m_staticlineSyncHeader, 0, wxEXPAND, 5 );
	
	
	bSizer276->Add( bSizerHeaderSyncSettings, 0, wxEXPAND, 5 );
	
	m_panelSyncSettings = new wxPanel( m_panelSyncSettingsTab, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelSyncSettings->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer232;
	bSizer232 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer237;
	bSizer237 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer235;
	bSizer235 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText86 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Select a variant:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText86->Wrap( -1 );
	bSizer235->Add( m_staticText86, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer236;
	bSizer236 = new wxBoxSizer( wxVERTICAL );
	
	m_toggleBtnTwoWay = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_toggleBtnTwoWay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer236->Add( m_toggleBtnTwoWay, 0, wxBOTTOM|wxEXPAND, 5 );
	
	m_toggleBtnMirror = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_toggleBtnMirror->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer236->Add( m_toggleBtnMirror, 0, wxEXPAND|wxBOTTOM, 5 );
	
	m_toggleBtnUpdate = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_toggleBtnUpdate->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer236->Add( m_toggleBtnUpdate, 0, wxEXPAND|wxBOTTOM, 5 );
	
	m_toggleBtnCustom = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_toggleBtnCustom->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer236->Add( m_toggleBtnCustom, 0, wxEXPAND, 5 );
	
	
	bSizer235->Add( bSizer236, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer237->Add( bSizer235, 0, wxALL, 5 );
	
	
	bSizer237->Add( 10, 0, 0, 0, 5 );
	
	wxBoxSizer* bSizer238;
	bSizer238 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer238->Add( 0, 0, 1, wxEXPAND, 5 );
	
	bSizerSyncDirHolder = new wxBoxSizer( wxHORIZONTAL );
	
	bSizerSyncDirections = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextCategory = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Category"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextCategory->Wrap( -1 );
	bSizerSyncDirections->Add( m_staticTextCategory, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	ffgSizer11 = new wxFlexGridSizer( 2, 0, 5, 5 );
	ffgSizer11->SetFlexibleDirection( wxBOTH );
	ffgSizer11->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_bitmapLeftOnly = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_bitmapLeftOnly->SetToolTip( _("Item exists on left side only") );
	
	ffgSizer11->Add( m_bitmapLeftOnly, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapLeftNewer = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_bitmapLeftNewer->SetToolTip( _("Left side is newer") );
	
	ffgSizer11->Add( m_bitmapLeftNewer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapDifferent = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_bitmapDifferent->SetToolTip( _("Items have different content") );
	
	ffgSizer11->Add( m_bitmapDifferent, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapConflict = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_bitmapConflict->SetToolTip( _("Conflict/item cannot be categorized") );
	
	ffgSizer11->Add( m_bitmapConflict, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapRightNewer = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_bitmapRightNewer->SetToolTip( _("Right side is newer") );
	
	ffgSizer11->Add( m_bitmapRightNewer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapRightOnly = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_bitmapRightOnly->SetToolTip( _("Item exists on right side only") );
	
	ffgSizer11->Add( m_bitmapRightOnly, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonLeftOnly = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	ffgSizer11->Add( m_bpButtonLeftOnly, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonLeftNewer = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	ffgSizer11->Add( m_bpButtonLeftNewer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonDifferent = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	ffgSizer11->Add( m_bpButtonDifferent, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonConflict = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	ffgSizer11->Add( m_bpButtonConflict, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonRightNewer = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	ffgSizer11->Add( m_bpButtonRightNewer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonRightOnly = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	ffgSizer11->Add( m_bpButtonRightOnly, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerSyncDirections->Add( ffgSizer11, 0, 0, 5 );
	
	m_staticText120 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Action"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText120->Wrap( -1 );
	bSizerSyncDirections->Add( m_staticText120, 0, wxALIGN_CENTER_HORIZONTAL|wxTOP, 5 );
	
	
	bSizerSyncDirHolder->Add( bSizerSyncDirections, 0, 0, 5 );
	
	bSizerDatabase = new wxWrapSizer( wxVERTICAL );
	
	m_bitmapDatabase = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerDatabase->Add( m_bitmapDatabase, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerDatabase->Add( 0, 3, 0, 0, 5 );
	
	m_staticText145 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("sync.ffs_db"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText145->Wrap( -1 );
	m_staticText145->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_SWISS, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL, false, wxT("Arial") ) );
	m_staticText145->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizerDatabase->Add( m_staticText145, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerSyncDirHolder->Add( bSizerDatabase, 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextSyncVarDescription = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticTextSyncVarDescription->Wrap( -1 );
	m_staticTextSyncVarDescription->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizerSyncDirHolder->Add( m_staticTextSyncVarDescription, 0, wxALL|wxALIGN_CENTER_VERTICAL, 10 );
	
	
	bSizer238->Add( bSizerSyncDirHolder, 0, wxTOP|wxBOTTOM|wxRIGHT, 10 );
	
	
	bSizer238->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticline431 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer238->Add( m_staticline431, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer201;
	bSizer201 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticline72 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer201->Add( m_staticline72, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer249;
	bSizer249 = new wxBoxSizer( wxHORIZONTAL );
	
	m_checkBoxDetectMove = new wxCheckBox( m_panelSyncSettings, wxID_ANY, _("Detect moved files"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxDetectMove->SetToolTip( _("- Not supported by all file systems\n- Requires and creates database files\n- Detection not available for first sync") );
	
	bSizer249->Add( m_checkBoxDetectMove, 0, wxALL|wxEXPAND, 5 );
	
	m_hyperlink242 = new wxHyperlinkCtrl( m_panelSyncSettings, wxID_ANY, _("More information"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer249->Add( m_hyperlink242, 0, wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer201->Add( bSizer249, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer238->Add( bSizer201, 0, 0, 5 );
	
	
	bSizer237->Add( bSizer238, 1, wxEXPAND, 5 );
	
	
	bSizer232->Add( bSizer237, 0, wxEXPAND, 5 );
	
	m_staticline54 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer232->Add( m_staticline54, 0, wxEXPAND, 5 );
	
	bSizer2361 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer202;
	bSizer202 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText87 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Delete files:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText87->Wrap( -1 );
	bSizer202->Add( m_staticText87, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer234;
	bSizer234 = new wxBoxSizer( wxVERTICAL );
	
	m_toggleBtnRecycler = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("&Recycle bin"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer234->Add( m_toggleBtnRecycler, 0, wxEXPAND, 5 );
	
	m_toggleBtnPermanent = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("&Permanent"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer234->Add( m_toggleBtnPermanent, 0, wxEXPAND, 5 );
	
	m_toggleBtnVersioning = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("&Versioning"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer234->Add( m_toggleBtnVersioning, 0, wxEXPAND, 5 );
	
	
	bSizer202->Add( bSizer234, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer2361->Add( bSizer202, 0, wxALL, 5 );
	
	bSizerVersioningHolder = new wxBoxSizer( wxVERTICAL );
	
	
	bSizerVersioningHolder->Add( 0, 0, 1, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer2331;
	bSizer2331 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapDeletionType = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer2331->Add( m_bitmapDeletionType, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_staticTextDeletionTypeDescription = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDeletionTypeDescription->Wrap( -1 );
	m_staticTextDeletionTypeDescription->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer2331->Add( m_staticTextDeletionTypeDescription, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	bSizerVersioningHolder->Add( bSizer2331, 0, wxALL|wxEXPAND, 5 );
	
	m_panelVersioning = new wxPanel( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelVersioning->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer191;
	bSizer191 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer191->Add( 0, 5, 0, 0, 5 );
	
	wxBoxSizer* bSizer252;
	bSizer252 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapVersioning = new wxStaticBitmap( m_panelVersioning, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer252->Add( m_bitmapVersioning, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	wxBoxSizer* bSizer253;
	bSizer253 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer254;
	bSizer254 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText155 = new wxStaticText( m_panelVersioning, wxID_ANY, _("Move files to a user-defined folder"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText155->Wrap( -1 );
	m_staticText155->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer254->Add( m_staticText155, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	
	bSizer254->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_hyperlink243 = new wxHyperlinkCtrl( m_panelVersioning, wxID_ANY, _("Show examples"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer254->Add( m_hyperlink243, 0, wxLEFT|wxALIGN_BOTTOM, 5 );
	
	
	bSizer253->Add( bSizer254, 0, wxEXPAND|wxBOTTOM, 5 );
	
	wxBoxSizer* bSizer156;
	bSizer156 = new wxBoxSizer( wxHORIZONTAL );
	
	m_versioningFolderPath = new fff::FolderHistoryBox( m_panelVersioning, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer156->Add( m_versioningFolderPath, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectVersioningFolder = new wxButton( m_panelVersioning, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectVersioningFolder->SetToolTip( _("Select a folder") );
	
	bSizer156->Add( m_buttonSelectVersioningFolder, 0, wxEXPAND, 5 );
	
	m_bpButtonSelectVersioningAltFolder = new wxBitmapButton( m_panelVersioning, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSelectVersioningAltFolder->SetToolTip( _("Access online storage") );
	
	bSizer156->Add( m_bpButtonSelectVersioningAltFolder, 0, wxEXPAND, 5 );
	
	
	bSizer253->Add( bSizer156, 0, wxEXPAND, 5 );
	
	
	bSizer252->Add( bSizer253, 1, wxRIGHT, 5 );
	
	
	bSizer191->Add( bSizer252, 0, wxEXPAND|wxTOP|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer198;
	bSizer198 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer255;
	bSizer255 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer256;
	bSizer256 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText93 = new wxStaticText( m_panelVersioning, wxID_ANY, _("Naming convention:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText93->Wrap( -1 );
	bSizer256->Add( m_staticText93, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	wxArrayString m_choiceVersioningStyleChoices;
	m_choiceVersioningStyle = new wxChoice( m_panelVersioning, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceVersioningStyleChoices, 0 );
	m_choiceVersioningStyle->SetSelection( 0 );
	bSizer256->Add( m_choiceVersioningStyle, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer255->Add( bSizer256, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer257;
	bSizer257 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticTextNamingCvtPart1 = new wxStaticText( m_panelVersioning, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextNamingCvtPart1->Wrap( -1 );
	m_staticTextNamingCvtPart1->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer257->Add( m_staticTextNamingCvtPart1, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextNamingCvtPart2Bold = new wxStaticText( m_panelVersioning, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextNamingCvtPart2Bold->Wrap( -1 );
	m_staticTextNamingCvtPart2Bold->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	m_staticTextNamingCvtPart2Bold->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer257->Add( m_staticTextNamingCvtPart2Bold, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextNamingCvtPart3 = new wxStaticText( m_panelVersioning, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextNamingCvtPart3->Wrap( -1 );
	m_staticTextNamingCvtPart3->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer257->Add( m_staticTextNamingCvtPart3, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer255->Add( bSizer257, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer198->Add( bSizer255, 0, wxALL, 5 );
	
	m_staticline69 = new wxStaticLine( m_panelVersioning, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer198->Add( m_staticline69, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer258;
	bSizer258 = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextLimitVersions = new wxStaticText( m_panelVersioning, wxID_ANY, _("Limit file versions:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextLimitVersions->Wrap( -1 );
	bSizer258->Add( m_staticTextLimitVersions, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	
	fgSizer15 = new wxFlexGridSizer( 0, 3, 5, 10 );
	fgSizer15->SetFlexibleDirection( wxBOTH );
	fgSizer15->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_checkBoxVersionMaxDays = new wxCheckBox( m_panelVersioning, wxID_ANY, _("Last x days:"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer15->Add( m_checkBoxVersionMaxDays, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_checkBoxVersionCountMin = new wxCheckBox( m_panelVersioning, wxID_ANY, _("Minimum:"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer15->Add( m_checkBoxVersionCountMin, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_checkBoxVersionCountMax = new wxCheckBox( m_panelVersioning, wxID_ANY, _("Maximum:"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer15->Add( m_checkBoxVersionCountMax, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_spinCtrlVersionMaxDays = new wxSpinCtrl( m_panelVersioning, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
	fgSizer15->Add( m_spinCtrlVersionMaxDays, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_spinCtrlVersionCountMin = new wxSpinCtrl( m_panelVersioning, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
	fgSizer15->Add( m_spinCtrlVersionCountMin, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_spinCtrlVersionCountMax = new wxSpinCtrl( m_panelVersioning, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 2000000000, 1 );
	fgSizer15->Add( m_spinCtrlVersionCountMax, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer258->Add( fgSizer15, 0, wxALL, 5 );
	
	
	bSizer198->Add( bSizer258, 0, wxALL, 5 );
	
	
	bSizer191->Add( bSizer198, 0, wxEXPAND, 5 );
	
	
	m_panelVersioning->SetSizer( bSizer191 );
	m_panelVersioning->Layout();
	bSizer191->Fit( m_panelVersioning );
	bSizerVersioningHolder->Add( m_panelVersioning, 0, wxEXPAND, 5 );
	
	
	bSizerVersioningHolder->Add( 0, 0, 1, wxEXPAND, 5 );
	
	
	bSizer2361->Add( bSizerVersioningHolder, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer232->Add( bSizer2361, 0, wxEXPAND, 5 );
	
	m_staticline582 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer232->Add( m_staticline582, 0, wxEXPAND, 5 );
	
	bSizerSyncMisc = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer2372;
	bSizer2372 = new wxBoxSizer( wxHORIZONTAL );
	
	m_panelLogfile = new wxPanel( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelLogfile->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer1912;
	bSizer1912 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer279;
	bSizer279 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapLogFile = new wxStaticBitmap( m_panelLogfile, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer279->Add( m_bitmapLogFile, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_checkBoxSaveLog = new wxCheckBox( m_panelLogfile, wxID_ANY, _("&Override default log path:"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer279->Add( m_checkBoxSaveLog, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_buttonSelectLogFolder = new wxButton( m_panelLogfile, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectLogFolder->SetToolTip( _("Select a folder") );
	
	bSizer279->Add( m_buttonSelectLogFolder, 0, wxEXPAND, 5 );
	
	m_bpButtonSelectAltLogFolder = new wxBitmapButton( m_panelLogfile, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSelectAltLogFolder->SetToolTip( _("Access online storage") );
	
	bSizer279->Add( m_bpButtonSelectAltLogFolder, 0, wxEXPAND, 5 );
	
	
	bSizer1912->Add( bSizer279, 0, wxEXPAND, 5 );
	
	m_logFolderPath = new fff::FolderHistoryBox( m_panelLogfile, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer1912->Add( m_logFolderPath, 0, wxEXPAND|wxTOP, 5 );
	
	
	m_panelLogfile->SetSizer( bSizer1912 );
	m_panelLogfile->Layout();
	bSizer1912->Fit( m_panelLogfile );
	bSizer2372->Add( m_panelLogfile, 1, 0, 5 );
	
	
	bSizerSyncMisc->Add( bSizer2372, 1, wxALL, 10 );
	
	m_staticline57 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizerSyncMisc->Add( m_staticline57, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer247;
	bSizer247 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer251;
	bSizer251 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticTextPostSync = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Run a command:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextPostSync->Wrap( -1 );
	bSizer251->Add( m_staticTextPostSync, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	
	bSizer251->Add( 0, 0, 1, 0, 5 );
	
	wxArrayString m_choicePostSyncConditionChoices;
	m_choicePostSyncCondition = new wxChoice( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choicePostSyncConditionChoices, 0 );
	m_choicePostSyncCondition->SetSelection( 0 );
	bSizer251->Add( m_choicePostSyncCondition, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer247->Add( bSizer251, 0, wxEXPAND, 5 );
	
	m_comboBoxPostSyncCommand = new fff::CommandBox( m_panelSyncSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer247->Add( m_comboBoxPostSyncCommand, 0, wxTOP|wxEXPAND, 5 );
	
	
	bSizerSyncMisc->Add( bSizer247, 0, wxALL, 10 );
	
	
	bSizer232->Add( bSizerSyncMisc, 1, wxEXPAND, 5 );
	
	
	m_panelSyncSettings->SetSizer( bSizer232 );
	m_panelSyncSettings->Layout();
	bSizer232->Fit( m_panelSyncSettings );
	bSizer276->Add( m_panelSyncSettings, 1, wxEXPAND, 5 );
	
	
	m_panelSyncSettingsTab->SetSizer( bSizer276 );
	m_panelSyncSettingsTab->Layout();
	bSizer276->Fit( m_panelSyncSettingsTab );
	m_notebook->AddPage( m_panelSyncSettingsTab, _("dummy"), true );
	
	bSizer190->Add( m_notebook, 1, wxEXPAND|wxTOP|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer7->Add( bSizer190, 1, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonOkay->SetDefault(); 
	m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonOkay, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer7->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer7 );
	this->Layout();
	bSizer7->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( ConfigDlgGenerated::OnClose ) );
	m_listBoxFolderPair->Connect( wxEVT_KEY_DOWN, wxKeyEventHandler( ConfigDlgGenerated::onListBoxKeyEvent ), NULL, this );
	m_listBoxFolderPair->Connect( wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::OnSelectFolderPair ), NULL, this );
	m_checkBoxUseLocalCmpOptions->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnToggleLocalCompSettings ), NULL, this );
	m_toggleBtnByTimeSize->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnCompByTimeSizeDouble ), NULL, this );
	m_toggleBtnByTimeSize->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnCompByTimeSize ), NULL, this );
	m_toggleBtnByContent->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnCompByContentDouble ), NULL, this );
	m_toggleBtnByContent->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnCompByContent ), NULL, this );
	m_toggleBtnBySize->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnCompBySizeDouble ), NULL, this );
	m_toggleBtnBySize->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnCompBySize ), NULL, this );
	m_checkBoxSymlinksInclude->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeCompOption ), NULL, this );
	m_hyperlink24->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( ConfigDlgGenerated::OnHelpComparisonSettings ), NULL, this );
	m_hyperlink241->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( ConfigDlgGenerated::OnHelpTimeShift ), NULL, this );
	m_checkBoxIgnoreErrors->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnToggleIgnoreErrors ), NULL, this );
	m_checkBoxAutoRetry->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnToggleAutoRetry ), NULL, this );
	m_hyperlink1711->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( ConfigDlgGenerated::OnHelpPerformance ), NULL, this );
	m_textCtrlInclude->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeFilterOption ), NULL, this );
	m_hyperlink171->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( ConfigDlgGenerated::OnHelpShowExamples ), NULL, this );
	m_textCtrlExclude->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeFilterOption ), NULL, this );
	m_choiceUnitMinSize->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeFilterOption ), NULL, this );
	m_choiceUnitMaxSize->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeFilterOption ), NULL, this );
	m_choiceUnitTimespan->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeFilterOption ), NULL, this );
	m_buttonClear->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnFilterReset ), NULL, this );
	m_checkBoxUseLocalSyncOptions->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnToggleLocalSyncSettings ), NULL, this );
	m_toggleBtnTwoWay->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnSyncTwoWayDouble ), NULL, this );
	m_toggleBtnTwoWay->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnSyncTwoWay ), NULL, this );
	m_toggleBtnMirror->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnSyncMirrorDouble ), NULL, this );
	m_toggleBtnMirror->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnSyncMirror ), NULL, this );
	m_toggleBtnUpdate->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnSyncUpdateDouble ), NULL, this );
	m_toggleBtnUpdate->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnSyncUpdate ), NULL, this );
	m_toggleBtnCustom->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnSyncCustomDouble ), NULL, this );
	m_toggleBtnCustom->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnSyncCustom ), NULL, this );
	m_bpButtonLeftOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnExLeftSideOnly ), NULL, this );
	m_bpButtonLeftNewer->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnLeftNewer ), NULL, this );
	m_bpButtonDifferent->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnDifferent ), NULL, this );
	m_bpButtonConflict->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnConflict ), NULL, this );
	m_bpButtonRightNewer->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnRightNewer ), NULL, this );
	m_bpButtonRightOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnExRightSideOnly ), NULL, this );
	m_checkBoxDetectMove->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnToggleDetectMovedFiles ), NULL, this );
	m_hyperlink242->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( ConfigDlgGenerated::OnHelpDetectMovedFiles ), NULL, this );
	m_toggleBtnRecycler->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnDeletionRecycler ), NULL, this );
	m_toggleBtnPermanent->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnDeletionPermanent ), NULL, this );
	m_toggleBtnVersioning->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnDeletionVersioning ), NULL, this );
	m_hyperlink243->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( ConfigDlgGenerated::OnHelpVersioning ), NULL, this );
	m_choiceVersioningStyle->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::OnChanegVersioningStyle ), NULL, this );
	m_checkBoxVersionMaxDays->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnToggleVersioningLimit ), NULL, this );
	m_checkBoxVersionCountMin->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnToggleVersioningLimit ), NULL, this );
	m_checkBoxVersionCountMax->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnToggleVersioningLimit ), NULL, this );
	m_checkBoxSaveLog->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnToggleSaveLogfile ), NULL, this );
	m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnOkay ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnCancel ), NULL, this );
}

ConfigDlgGenerated::~ConfigDlgGenerated()
{
}

CloudSetupDlgGenerated::CloudSetupDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer134;
	bSizer134 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer72;
	bSizer72 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapCloud = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer72->Add( m_bitmapCloud, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	wxBoxSizer* bSizer272;
	bSizer272 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText136 = new wxStaticText( this, wxID_ANY, _("Connection type:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText136->Wrap( -1 );
	bSizer272->Add( m_staticText136, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer231;
	bSizer231 = new wxBoxSizer( wxHORIZONTAL );
	
	m_toggleBtnGdrive = new wxToggleButton( this, wxID_ANY, _("Google Drive"), wxDefaultPosition, wxDefaultSize, 0 );
	m_toggleBtnGdrive->SetValue( true ); 
	m_toggleBtnGdrive->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer231->Add( m_toggleBtnGdrive, 0, wxTOP|wxBOTTOM|wxLEFT|wxEXPAND, 5 );
	
	m_toggleBtnSftp = new wxToggleButton( this, wxID_ANY, _("SFTP"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_toggleBtnSftp->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer231->Add( m_toggleBtnSftp, 0, wxTOP|wxBOTTOM|wxLEFT|wxEXPAND, 5 );
	
	m_toggleBtnFtp = new wxToggleButton( this, wxID_ANY, _("FTP"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_toggleBtnFtp->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer231->Add( m_toggleBtnFtp, 0, wxALL|wxEXPAND, 5 );
	
	
	bSizer272->Add( bSizer231, 0, 0, 5 );
	
	
	bSizer72->Add( bSizer272, 0, wxALL, 5 );
	
	
	bSizer134->Add( bSizer72, 0, wxEXPAND, 5 );
	
	m_staticline371 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxSize( -1,-1 ), wxLI_HORIZONTAL );
	bSizer134->Add( m_staticline371, 0, wxEXPAND, 5 );
	
	m_panel41 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel41->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer185;
	bSizer185 = new wxBoxSizer( wxVERTICAL );
	
	bSizerGdrive = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer284;
	bSizer284 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer285;
	bSizer285 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText166 = new wxStaticText( m_panel41, wxID_ANY, _("Connected user accounts:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText166->Wrap( -1 );
	bSizer285->Add( m_staticText166, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	
	m_listBoxGdriveUsers = new wxListBox( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_NEEDED_SB|wxLB_SINGLE|wxLB_SORT ); 
	bSizer285->Add( m_listBoxGdriveUsers, 1, wxTOP|wxBOTTOM|wxLEFT|wxEXPAND, 5 );
	
	
	bSizer284->Add( bSizer285, 0, wxEXPAND|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	wxBoxSizer* bSizer286;
	bSizer286 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer289;
	bSizer289 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer288;
	bSizer288 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText167 = new wxStaticText( m_panel41, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText167->Wrap( -1 );
	bSizer288->Add( m_staticText167, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	
	m_buttonGdriveAddUser = new zen::BitmapTextButton( m_panel41, wxID_ANY, _("&Add connection"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer288->Add( m_buttonGdriveAddUser, 0, wxEXPAND|wxTOP|wxRIGHT|wxLEFT, 5 );
	
	m_buttonGdriveRemoveUser = new zen::BitmapTextButton( m_panel41, wxID_ANY, _("&Disconnect"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer288->Add( m_buttonGdriveRemoveUser, 0, wxEXPAND|wxALL, 5 );
	
	
	bSizer289->Add( bSizer288, 0, wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	m_staticline76 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer289->Add( m_staticline76, 0, wxEXPAND, 5 );
	
	
	bSizer286->Add( bSizer289, 0, 0, 5 );
	
	m_staticline74 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer286->Add( m_staticline74, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer287;
	bSizer287 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText165 = new wxStaticText( m_panel41, wxID_ANY, _("Selected user account:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText165->Wrap( -1 );
	bSizer287->Add( m_staticText165, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer279;
	bSizer279 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapGdriveSelectedUser = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer279->Add( m_bitmapGdriveSelectedUser, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	m_staticTextGdriveUser = new wxStaticText( m_panel41, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextGdriveUser->Wrap( -1 );
	bSizer279->Add( m_staticTextGdriveUser, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer287->Add( bSizer279, 0, 0, 5 );
	
	
	bSizer286->Add( bSizer287, 0, wxALL, 5 );
	
	
	bSizer284->Add( bSizer286, 1, 0, 5 );
	
	
	bSizerGdrive->Add( bSizer284, 0, wxEXPAND, 5 );
	
	m_staticline73 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerGdrive->Add( m_staticline73, 0, wxEXPAND, 5 );
	
	
	bSizer185->Add( bSizerGdrive, 0, wxEXPAND, 5 );
	
	bSizerServer = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer276;
	bSizer276 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapServer = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer276->Add( m_bitmapServer, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	m_staticText12311 = new wxStaticText( m_panel41, wxID_ANY, _("Server name or IP address:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText12311->Wrap( -1 );
	bSizer276->Add( m_staticText12311, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	m_textCtrlServer = new wxTextCtrl( m_panel41, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer276->Add( m_textCtrlServer, 1, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText1233 = new wxStaticText( m_panel41, wxID_ANY, _("Port:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1233->Wrap( -1 );
	bSizer276->Add( m_staticText1233, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	m_textCtrlPort = new wxTextCtrl( m_panel41, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer276->Add( m_textCtrlPort, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerServer->Add( bSizer276, 0, wxALL|wxEXPAND, 5 );
	
	m_staticline58 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerServer->Add( m_staticline58, 0, wxEXPAND, 5 );
	
	
	bSizer185->Add( bSizerServer, 0, wxEXPAND, 5 );
	
	bSizerAuth = new wxBoxSizer( wxVERTICAL );
	
	bSizerAuthInner = new wxBoxSizer( wxHORIZONTAL );
	
	bSizerFtpEncrypt = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer2181;
	bSizer2181 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText1251 = new wxStaticText( m_panel41, wxID_ANY, _("Encryption:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1251->Wrap( -1 );
	bSizer2181->Add( m_staticText1251, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	
	m_radioBtnEncryptNone = new wxRadioButton( m_panel41, wxID_ANY, _("&Disabled"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
	m_radioBtnEncryptNone->SetValue( true ); 
	bSizer2181->Add( m_radioBtnEncryptNone, 0, wxEXPAND|wxALL, 5 );
	
	m_radioBtnEncryptSsl = new wxRadioButton( m_panel41, wxID_ANY, _("&Explicit SSL/TLS"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer2181->Add( m_radioBtnEncryptSsl, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxEXPAND, 5 );
	
	
	bSizerFtpEncrypt->Add( bSizer2181, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticline5721 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizerFtpEncrypt->Add( m_staticline5721, 0, wxEXPAND, 5 );
	
	
	bSizerAuthInner->Add( bSizerFtpEncrypt, 0, wxEXPAND, 5 );
	
	bSizerSftpAuth = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer218;
	bSizer218 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText125 = new wxStaticText( m_panel41, wxID_ANY, _("Authentication:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText125->Wrap( -1 );
	bSizer218->Add( m_staticText125, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	
	m_radioBtnPassword = new wxRadioButton( m_panel41, wxID_ANY, _("&Password"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
	m_radioBtnPassword->SetValue( true ); 
	bSizer218->Add( m_radioBtnPassword, 0, wxEXPAND|wxALL, 5 );
	
	m_radioBtnKeyfile = new wxRadioButton( m_panel41, wxID_ANY, _("&Key file"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer218->Add( m_radioBtnKeyfile, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxEXPAND, 5 );
	
	m_radioBtnAgent = new wxRadioButton( m_panel41, wxID_ANY, _("&SSH agent"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer218->Add( m_radioBtnAgent, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizerSftpAuth->Add( bSizer218, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticline572 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizerSftpAuth->Add( m_staticline572, 0, wxEXPAND, 5 );
	
	
	bSizerAuthInner->Add( bSizerSftpAuth, 0, wxEXPAND, 5 );
	
	m_panelAuth = new wxPanel( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelAuth->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer221;
	bSizer221 = new wxBoxSizer( wxVERTICAL );
	
	wxFlexGridSizer* fgSizer161;
	fgSizer161 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer161->AddGrowableCol( 1 );
	fgSizer161->SetFlexibleDirection( wxBOTH );
	fgSizer161->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_staticText123 = new wxStaticText( m_panelAuth, wxID_ANY, _("User name:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText123->Wrap( -1 );
	fgSizer161->Add( m_staticText123, 0, wxALIGN_RIGHT|wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_textCtrlUserName = new wxTextCtrl( m_panelAuth, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer161->Add( m_textCtrlUserName, 0, wxALL|wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextKeyfile = new wxStaticText( m_panelAuth, wxID_ANY, _("Private key file:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextKeyfile->Wrap( -1 );
	fgSizer161->Add( m_staticTextKeyfile, 0, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT, 5 );
	
	bSizerKeyFile = new wxBoxSizer( wxHORIZONTAL );
	
	m_textCtrlKeyfilePath = new wxTextCtrl( m_panelAuth, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerKeyFile->Add( m_textCtrlKeyfilePath, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectKeyfile = new wxButton( m_panelAuth, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectKeyfile->SetToolTip( _("Select a folder") );
	
	bSizerKeyFile->Add( m_buttonSelectKeyfile, 0, wxEXPAND, 5 );
	
	
	fgSizer161->Add( bSizerKeyFile, 0, wxALL|wxEXPAND, 5 );
	
	m_staticTextPassword = new wxStaticText( m_panelAuth, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextPassword->Wrap( -1 );
	fgSizer161->Add( m_staticTextPassword, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	bSizerPassword = new wxBoxSizer( wxHORIZONTAL );
	
	m_textCtrlPasswordVisible = new wxTextCtrl( m_panelAuth, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerPassword->Add( m_textCtrlPasswordVisible, 1, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	m_textCtrlPasswordHidden = new wxTextCtrl( m_panelAuth, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD );
	bSizerPassword->Add( m_textCtrlPasswordHidden, 1, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	m_checkBoxShowPassword = new wxCheckBox( m_panelAuth, wxID_ANY, _("&Show password"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizerPassword->Add( m_checkBoxShowPassword, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	fgSizer161->Add( bSizerPassword, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer221->Add( fgSizer161, 0, wxALL|wxEXPAND, 5 );
	
	
	m_panelAuth->SetSizer( bSizer221 );
	m_panelAuth->Layout();
	bSizer221->Fit( m_panelAuth );
	bSizerAuthInner->Add( m_panelAuth, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerAuth->Add( bSizerAuthInner, 0, wxEXPAND, 5 );
	
	m_staticline581 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerAuth->Add( m_staticline581, 0, wxEXPAND, 5 );
	
	
	bSizer185->Add( bSizerAuth, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer269;
	bSizer269 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer270;
	bSizer270 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer275;
	bSizer275 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapServerDir = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer275->Add( m_bitmapServerDir, 0, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText1232 = new wxStaticText( m_panel41, wxID_ANY, _("Directory on server:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1232->Wrap( -1 );
	bSizer275->Add( m_staticText1232, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer270->Add( bSizer275, 1, wxALIGN_BOTTOM|wxTOP|wxRIGHT, 5 );
	
	bSizerAccessTimeout = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticline72 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizerAccessTimeout->Add( m_staticline72, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer273;
	bSizer273 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticTextTimeout = new wxStaticText( m_panel41, wxID_ANY, _("Access timeout (in seconds):"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextTimeout->Wrap( -1 );
	bSizer273->Add( m_staticTextTimeout, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	m_spinCtrlTimeout = new wxSpinCtrl( m_panel41, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
	bSizer273->Add( m_spinCtrlTimeout, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerAccessTimeout->Add( bSizer273, 0, wxALL, 5 );
	
	
	bSizer270->Add( bSizerAccessTimeout, 0, 0, 5 );
	
	
	bSizer269->Add( bSizer270, 0, wxEXPAND|wxLEFT, 5 );
	
	wxBoxSizer* bSizer217;
	bSizer217 = new wxBoxSizer( wxHORIZONTAL );
	
	m_textCtrlServerPath = new wxTextCtrl( m_panel41, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer217->Add( m_textCtrlServerPath, 1, wxALIGN_CENTER_VERTICAL|wxBOTTOM|wxLEFT, 5 );
	
	m_buttonSelectFolder = new wxButton( m_panel41, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectFolder->SetToolTip( _("Select a folder") );
	
	bSizer217->Add( m_buttonSelectFolder, 0, wxBOTTOM|wxRIGHT|wxEXPAND, 5 );
	
	
	bSizer269->Add( bSizer217, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer185->Add( bSizer269, 0, wxEXPAND, 5 );
	
	
	m_panel41->SetSizer( bSizer185 );
	m_panel41->Layout();
	bSizer185->Fit( m_panel41 );
	bSizer134->Add( m_panel41, 0, wxEXPAND, 5 );
	
	bSizer255 = new wxBoxSizer( wxVERTICAL );
	
	m_staticline571 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer255->Add( m_staticline571, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer219;
	bSizer219 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer219->Add( 5, 0, 0, 0, 5 );
	
	m_bitmapPerf = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer219->Add( m_bitmapPerf, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_staticText1361 = new wxStaticText( this, wxID_ANY, _("Performance improvements:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1361->Wrap( -1 );
	bSizer219->Add( m_staticText1361, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 10 );
	
	
	bSizer219->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_hyperlink171 = new wxHyperlinkCtrl( this, wxID_ANY, _("How to get best performance?"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer219->Add( m_hyperlink171, 0, wxALL|wxALIGN_CENTER_VERTICAL, 10 );
	
	
	bSizer255->Add( bSizer219, 0, wxEXPAND, 5 );
	
	m_staticline57 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer255->Add( m_staticline57, 0, wxEXPAND, 5 );
	
	m_panel411 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel411->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer1851;
	bSizer1851 = new wxBoxSizer( wxVERTICAL );
	
	wxFlexGridSizer* fgSizer1611;
	fgSizer1611 = new wxFlexGridSizer( 0, 3, 0, 0 );
	fgSizer1611->AddGrowableCol( 1 );
	fgSizer1611->SetFlexibleDirection( wxBOTH );
	fgSizer1611->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	bSizerConnectionsLabel = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextConnectionsLabel = new wxStaticText( m_panel411, wxID_ANY, _("Parallel file operations:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextConnectionsLabel->Wrap( -1 );
	bSizerConnectionsLabel->Add( m_staticTextConnectionsLabel, 0, 0, 5 );
	
	m_staticTextConnectionsLabelSub = new wxStaticText( m_panel411, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextConnectionsLabelSub->Wrap( -1 );
	bSizerConnectionsLabel->Add( m_staticTextConnectionsLabelSub, 0, wxALIGN_RIGHT, 5 );
	
	
	fgSizer1611->Add( bSizerConnectionsLabel, 0, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_spinCtrlConnectionCount = new wxSpinCtrl( m_panel411, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
	fgSizer1611->Add( m_spinCtrlConnectionCount, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextConnectionCountDescr = new wxStaticText( m_panel411, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextConnectionCountDescr->Wrap( -1 );
	m_staticTextConnectionCountDescr->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	fgSizer1611->Add( m_staticTextConnectionCountDescr, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_staticTextChannelCountSftp = new wxStaticText( m_panel411, wxID_ANY, _("SFTP channels per connection:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextChannelCountSftp->Wrap( -1 );
	fgSizer1611->Add( m_staticTextChannelCountSftp, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	m_spinCtrlChannelCountSftp = new wxSpinCtrl( m_panel411, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
	fgSizer1611->Add( m_spinCtrlChannelCountSftp, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonChannelCountSftp = new wxButton( m_panel411, wxID_ANY, _("Detect server limit"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer1611->Add( m_buttonChannelCountSftp, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	bSizer1851->Add( fgSizer1611, 0, wxALL, 5 );
	
	
	m_panel411->SetSizer( bSizer1851 );
	m_panel411->Layout();
	bSizer1851->Fit( m_panel411 );
	bSizer255->Add( m_panel411, 1, wxEXPAND, 5 );
	
	
	bSizer134->Add( bSizer255, 1, wxEXPAND, 5 );
	
	m_staticline12 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer134->Add( m_staticline12, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonOkay->SetDefault(); 
	m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonOkay, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer134->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer134 );
	this->Layout();
	bSizer134->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CloudSetupDlgGenerated::OnClose ) );
	m_toggleBtnGdrive->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::OnConnectionGdrive ), NULL, this );
	m_toggleBtnSftp->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::OnConnectionSftp ), NULL, this );
	m_toggleBtnFtp->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::OnConnectionFtp ), NULL, this );
	m_listBoxGdriveUsers->Connect( wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler( CloudSetupDlgGenerated::OnGdriveUserSelect ), NULL, this );
	m_buttonGdriveAddUser->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::OnGdriveUserAdd ), NULL, this );
	m_buttonGdriveRemoveUser->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::OnGdriveUserRemove ), NULL, this );
	m_radioBtnPassword->Connect( wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler( CloudSetupDlgGenerated::OnAuthPassword ), NULL, this );
	m_radioBtnKeyfile->Connect( wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler( CloudSetupDlgGenerated::OnAuthKeyfile ), NULL, this );
	m_radioBtnAgent->Connect( wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler( CloudSetupDlgGenerated::OnAuthAgent ), NULL, this );
	m_buttonSelectKeyfile->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::OnSelectKeyfile ), NULL, this );
	m_checkBoxShowPassword->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::OnToggleShowPassword ), NULL, this );
	m_buttonSelectFolder->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::OnBrowseCloudFolder ), NULL, this );
	m_hyperlink171->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( CloudSetupDlgGenerated::OnHelpFtpPerformance ), NULL, this );
	m_buttonChannelCountSftp->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::OnDetectServerChannelLimit ), NULL, this );
	m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::OnOkay ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::OnCancel ), NULL, this );
}

CloudSetupDlgGenerated::~CloudSetupDlgGenerated()
{
}

AbstractFolderPickerGenerated::AbstractFolderPickerGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer134;
	bSizer134 = new wxBoxSizer( wxVERTICAL );
	
	m_panel41 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel41->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer185;
	bSizer185 = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextStatus = new wxStaticText( m_panel41, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatus->Wrap( -1 );
	bSizer185->Add( m_staticTextStatus, 0, wxALL, 5 );
	
	m_treeCtrlFileSystem = new wxTreeCtrl( m_panel41, wxID_ANY, wxDefaultPosition, wxSize( -1,-1 ), wxTR_FULL_ROW_HIGHLIGHT|wxTR_HAS_BUTTONS|wxTR_LINES_AT_ROOT|wxTR_NO_LINES|wxNO_BORDER );
	bSizer185->Add( m_treeCtrlFileSystem, 1, wxEXPAND, 5 );
	
	
	m_panel41->SetSizer( bSizer185 );
	m_panel41->Layout();
	bSizer185->Fit( m_panel41 );
	bSizer134->Add( m_panel41, 1, wxEXPAND, 5 );
	
	m_staticline12 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer134->Add( m_staticline12, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonOkay = new wxButton( this, wxID_OK, _("Select Folder"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonOkay->SetDefault(); 
	m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonOkay, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer134->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer134 );
	this->Layout();
	bSizer134->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( AbstractFolderPickerGenerated::OnClose ) );
	m_treeCtrlFileSystem->Connect( wxEVT_COMMAND_TREE_ITEM_EXPANDING, wxTreeEventHandler( AbstractFolderPickerGenerated::OnExpandNode ), NULL, this );
	m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AbstractFolderPickerGenerated::OnOkay ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AbstractFolderPickerGenerated::OnCancel ), NULL, this );
}

AbstractFolderPickerGenerated::~AbstractFolderPickerGenerated()
{
}

SyncConfirmationDlgGenerated::SyncConfirmationDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer134;
	bSizer134 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer72;
	bSizer72 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapSync = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer72->Add( m_bitmapSync, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_staticTextCaption = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextCaption->Wrap( -1 );
	bSizer72->Add( m_staticTextCaption, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	
	bSizer134->Add( bSizer72, 0, 0, 5 );
	
	m_staticline371 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer134->Add( m_staticline371, 0, wxEXPAND, 5 );
	
	m_panelStatistics = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelStatistics->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer185;
	bSizer185 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer185->Add( 40, 0, 0, 0, 5 );
	
	
	bSizer185->Add( 0, 0, 1, 0, 5 );
	
	m_staticline38 = new wxStaticLine( m_panelStatistics, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer185->Add( m_staticline38, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer162;
	bSizer162 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer182;
	bSizer182 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText84 = new wxStaticText( m_panelStatistics, wxID_ANY, _("Variant:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText84->Wrap( -1 );
	bSizer182->Add( m_staticText84, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	
	bSizer182->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextVariant = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextVariant->Wrap( -1 );
	m_staticTextVariant->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer182->Add( m_staticTextVariant, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	bSizer182->Add( 0, 0, 1, wxEXPAND, 5 );
	
	
	bSizer162->Add( bSizer182, 0, wxALL|wxEXPAND, 5 );
	
	m_staticline14 = new wxStaticLine( m_panelStatistics, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer162->Add( m_staticline14, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer181;
	bSizer181 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText83 = new wxStaticText( m_panelStatistics, wxID_ANY, _("Statistics:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText83->Wrap( -1 );
	bSizer181->Add( m_staticText83, 0, wxALL, 5 );
	
	wxFlexGridSizer* fgSizer11;
	fgSizer11 = new wxFlexGridSizer( 2, 7, 2, 5 );
	fgSizer11->SetFlexibleDirection( wxBOTH );
	fgSizer11->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_bitmapDeleteLeft = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapDeleteLeft->SetToolTip( _("Number of files and folders that will be deleted") );
	
	fgSizer11->Add( m_bitmapDeleteLeft, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapUpdateLeft = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapUpdateLeft->SetToolTip( _("Number of files that will be updated") );
	
	fgSizer11->Add( m_bitmapUpdateLeft, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapCreateLeft = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapCreateLeft->SetToolTip( _("Number of files and folders that will be created") );
	
	fgSizer11->Add( m_bitmapCreateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bitmapData = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapData->SetToolTip( _("Total bytes to copy") );
	
	fgSizer11->Add( m_bitmapData, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bitmapCreateRight = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapCreateRight->SetToolTip( _("Number of files and folders that will be created") );
	
	fgSizer11->Add( m_bitmapCreateRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bitmapUpdateRight = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapUpdateRight->SetToolTip( _("Number of files that will be updated") );
	
	fgSizer11->Add( m_bitmapUpdateRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bitmapDeleteRight = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapDeleteRight->SetToolTip( _("Number of files and folders that will be deleted") );
	
	fgSizer11->Add( m_bitmapDeleteRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextDeleteLeft = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDeleteLeft->Wrap( -1 );
	m_staticTextDeleteLeft->SetToolTip( _("Number of files and folders that will be deleted") );
	
	fgSizer11->Add( m_staticTextDeleteLeft, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextUpdateLeft = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextUpdateLeft->Wrap( -1 );
	m_staticTextUpdateLeft->SetToolTip( _("Number of files that will be updated") );
	
	fgSizer11->Add( m_staticTextUpdateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_staticTextCreateLeft = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextCreateLeft->Wrap( -1 );
	m_staticTextCreateLeft->SetToolTip( _("Number of files and folders that will be created") );
	
	fgSizer11->Add( m_staticTextCreateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_staticTextData = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextData->Wrap( -1 );
	m_staticTextData->SetToolTip( _("Total bytes to copy") );
	
	fgSizer11->Add( m_staticTextData, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextCreateRight = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextCreateRight->Wrap( -1 );
	m_staticTextCreateRight->SetToolTip( _("Number of files and folders that will be created") );
	
	fgSizer11->Add( m_staticTextCreateRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextUpdateRight = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextUpdateRight->Wrap( -1 );
	m_staticTextUpdateRight->SetToolTip( _("Number of files that will be updated") );
	
	fgSizer11->Add( m_staticTextUpdateRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextDeleteRight = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDeleteRight->Wrap( -1 );
	m_staticTextDeleteRight->SetToolTip( _("Number of files and folders that will be deleted") );
	
	fgSizer11->Add( m_staticTextDeleteRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer181->Add( fgSizer11, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxEXPAND, 5 );
	
	
	bSizer162->Add( bSizer181, 0, wxEXPAND|wxALL, 5 );
	
	
	bSizer185->Add( bSizer162, 0, 0, 5 );
	
	m_staticline381 = new wxStaticLine( m_panelStatistics, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer185->Add( m_staticline381, 0, wxEXPAND, 5 );
	
	
	bSizer185->Add( 0, 0, 1, 0, 5 );
	
	
	bSizer185->Add( 40, 0, 0, 0, 5 );
	
	
	m_panelStatistics->SetSizer( bSizer185 );
	m_panelStatistics->Layout();
	bSizer185->Fit( m_panelStatistics );
	bSizer134->Add( m_panelStatistics, 0, wxEXPAND, 5 );
	
	m_staticline12 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer134->Add( m_staticline12, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer164;
	bSizer164 = new wxBoxSizer( wxVERTICAL );
	
	m_checkBoxDontShowAgain = new wxCheckBox( this, wxID_ANY, _("&Don't show this dialog again"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer164->Add( m_checkBoxDontShowAgain, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonStartSync = new wxButton( this, wxID_OK, _("Start"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonStartSync->SetDefault(); 
	m_buttonStartSync->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonStartSync, 0, wxALIGN_CENTER_VERTICAL|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer164->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	bSizer134->Add( bSizer164, 1, wxEXPAND, 5 );
	
	
	this->SetSizer( bSizer134 );
	this->Layout();
	bSizer134->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( SyncConfirmationDlgGenerated::OnClose ) );
	m_buttonStartSync->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( SyncConfirmationDlgGenerated::OnStartSync ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( SyncConfirmationDlgGenerated::OnCancel ), NULL, this );
}

SyncConfirmationDlgGenerated::~SyncConfirmationDlgGenerated()
{
}

CompareProgressDlgGenerated::CompareProgressDlgGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) : wxPanel( parent, id, pos, size, style )
{
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer40;
	bSizer40 = new wxBoxSizer( wxHORIZONTAL );
	
	m_panelStatistics = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelStatistics->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer187;
	bSizer187 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer187->Add( 0, 5, 0, 0, 5 );
	
	wxFlexGridSizer* fgSizer7;
	fgSizer7 = new wxFlexGridSizer( 0, 2, 5, 5 );
	fgSizer7->SetFlexibleDirection( wxHORIZONTAL );
	fgSizer7->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_staticTextItemsFoundLabel = new wxStaticText( m_panelStatistics, wxID_ANY, _("Items found:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextItemsFoundLabel->Wrap( -1 );
	fgSizer7->Add( m_staticTextItemsFoundLabel, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextItemsFound = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextItemsFound->Wrap( -1 );
	m_staticTextItemsFound->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	fgSizer7->Add( m_staticTextItemsFound, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextItemsRemainingLabel = new wxStaticText( m_panelStatistics, wxID_ANY, _("Items remaining:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextItemsRemainingLabel->Wrap( -1 );
	fgSizer7->Add( m_staticTextItemsRemainingLabel, 0, wxALIGN_BOTTOM, 5 );
	
	bSizerItemsRemaining = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticTextItemsRemaining = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextItemsRemaining->Wrap( -1 );
	m_staticTextItemsRemaining->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizerItemsRemaining->Add( m_staticTextItemsRemaining, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextBytesRemaining = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextBytesRemaining->Wrap( -1 );
	bSizerItemsRemaining->Add( m_staticTextBytesRemaining, 0, wxLEFT|wxALIGN_BOTTOM, 5 );
	
	
	fgSizer7->Add( bSizerItemsRemaining, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextTimeRemainingLabel = new wxStaticText( m_panelStatistics, wxID_ANY, _("Time remaining:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextTimeRemainingLabel->Wrap( -1 );
	fgSizer7->Add( m_staticTextTimeRemainingLabel, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextTimeRemaining = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextTimeRemaining->Wrap( -1 );
	m_staticTextTimeRemaining->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	fgSizer7->Add( m_staticTextTimeRemaining, 0, wxALIGN_BOTTOM, 5 );
	
	wxStaticText* m_staticText37;
	m_staticText37 = new wxStaticText( m_panelStatistics, wxID_ANY, _("Time elapsed:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText37->Wrap( -1 );
	fgSizer7->Add( m_staticText37, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextTimeElapsed = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextTimeElapsed->Wrap( -1 );
	m_staticTextTimeElapsed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	fgSizer7->Add( m_staticTextTimeElapsed, 0, wxALIGN_BOTTOM, 5 );
	
	
	bSizer187->Add( fgSizer7, 0, wxRIGHT|wxLEFT, 5 );
	
	
	bSizer187->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelStatistics->SetSizer( bSizer187 );
	m_panelStatistics->Layout();
	bSizer187->Fit( m_panelStatistics );
	bSizer40->Add( m_panelStatistics, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	wxBoxSizer* bSizer181;
	bSizer181 = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextStatus = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatus->Wrap( -1 );
	bSizer181->Add( m_staticTextStatus, 0, 0, 5 );
	
	wxBoxSizer* bSizer199;
	bSizer199 = new wxBoxSizer( wxHORIZONTAL );
	
	bSizerErrorsRetry = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapRetryErrors = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerErrorsRetry->Add( m_bitmapRetryErrors, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText1461 = new wxStaticText( this, wxID_ANY, _("Automatic retry"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1461->Wrap( -1 );
	bSizerErrorsRetry->Add( m_staticText1461, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	m_staticTextRetryCount = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextRetryCount->Wrap( -1 );
	bSizerErrorsRetry->Add( m_staticTextRetryCount, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	
	bSizer199->Add( bSizerErrorsRetry, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	bSizerErrorsIgnore = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapIgnoreErrors = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerErrorsIgnore->Add( m_bitmapIgnoreErrors, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText146 = new wxStaticText( this, wxID_ANY, _("Ignore errors"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText146->Wrap( -1 );
	bSizerErrorsIgnore->Add( m_staticText146, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer199->Add( bSizerErrorsIgnore, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	bSizerProgressGraph = new wxBoxSizer( wxHORIZONTAL );
	
	m_panelProgressGraph = new zen::Graph2D( this, wxID_ANY, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_panelProgressGraph->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	bSizerProgressGraph->Add( m_panelProgressGraph, 1, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer247;
	bSizer247 = new wxBoxSizer( wxVERTICAL );
	
	wxStaticText* m_staticText99;
	m_staticText99 = new wxStaticText( this, wxID_ANY, _("Bytes"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText99->Wrap( -1 );
	bSizer247->Add( m_staticText99, 0, wxALL, 5 );
	
	wxStaticText* m_staticText100;
	m_staticText100 = new wxStaticText( this, wxID_ANY, _("Items"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText100->Wrap( -1 );
	bSizer247->Add( m_staticText100, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizerProgressGraph->Add( bSizer247, 0, 0, 5 );
	
	
	bSizer199->Add( bSizerProgressGraph, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer181->Add( bSizer199, 0, wxTOP|wxEXPAND, 5 );
	
	
	bSizer40->Add( bSizer181, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	this->SetSizer( bSizer40 );
	this->Layout();
	bSizer40->Fit( this );
}

CompareProgressDlgGenerated::~CompareProgressDlgGenerated()
{
}

SyncProgressPanelGenerated::SyncProgressPanelGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) : wxPanel( parent, id, pos, size, style )
{
	bSizerRoot = new wxBoxSizer( wxVERTICAL );
	
	m_panel53 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel53->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizer42 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer42->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_bitmapStatus = new wxStaticBitmap( m_panel53, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer42->Add( m_bitmapStatus, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
	
	m_staticTextPhase = new wxStaticText( m_panel53, wxID_ANY, _("Synchronizing..."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextPhase->Wrap( -1 );
	m_staticTextPhase->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer42->Add( m_staticTextPhase, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	wxBoxSizer* bSizer247;
	bSizer247 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer247->Add( 0, 0, 1, 0, 5 );
	
	m_bpButtonMinimizeToTray = new wxBitmapButton( m_panel53, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonMinimizeToTray->SetToolTip( _("Minimize to notification area") );
	
	bSizer247->Add( m_bpButtonMinimizeToTray, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5 );
	
	
	bSizer42->Add( bSizer247, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	m_panel53->SetSizer( bSizer42 );
	m_panel53->Layout();
	bSizer42->Fit( m_panel53 );
	bSizerRoot->Add( m_panel53, 0, wxEXPAND, 5 );
	
	bSizerStatusText = new wxBoxSizer( wxVERTICAL );
	
	
	bSizerStatusText->Add( 0, 5, 0, 0, 5 );
	
	m_staticTextStatus = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatus->Wrap( -1 );
	bSizerStatusText->Add( m_staticTextStatus, 0, wxEXPAND|wxLEFT, 10 );
	
	
	bSizerStatusText->Add( 0, 5, 0, 0, 5 );
	
	
	bSizerRoot->Add( bSizerStatusText, 0, wxEXPAND, 5 );
	
	m_panelProgress = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelProgress->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer173;
	bSizer173 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer161;
	bSizer161 = new wxBoxSizer( wxVERTICAL );
	
	m_panelGraphBytes = new zen::Graph2D( m_panelProgress, wxID_ANY, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_panelGraphBytes->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizer161->Add( m_panelGraphBytes, 1, wxEXPAND|wxLEFT, 10 );
	
	wxBoxSizer* bSizer232;
	bSizer232 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer233;
	bSizer233 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer175;
	bSizer175 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapGraphKeyBytes = new wxStaticBitmap( m_panelProgress, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer175->Add( m_bitmapGraphKeyBytes, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	wxStaticText* m_staticText99;
	m_staticText99 = new wxStaticText( m_panelProgress, wxID_ANY, _("Bytes"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText99->Wrap( -1 );
	bSizer175->Add( m_staticText99, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer233->Add( bSizer175, 0, 0, 5 );
	
	wxBoxSizer* bSizer174;
	bSizer174 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapGraphKeyItems = new wxStaticBitmap( m_panelProgress, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer174->Add( m_bitmapGraphKeyItems, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	wxStaticText* m_staticText100;
	m_staticText100 = new wxStaticText( m_panelProgress, wxID_ANY, _("Items"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText100->Wrap( -1 );
	bSizer174->Add( m_staticText100, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer233->Add( bSizer174, 0, wxTOP, 5 );
	
	
	bSizer232->Add( bSizer233, 1, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 10 );
	
	m_panelItemsProcessed = new wxPanel( m_panelProgress, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelItemsProcessed->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer165;
	bSizer165 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer165->Add( 0, 5, 0, 0, 5 );
	
	wxStaticText* m_staticText96;
	m_staticText96 = new wxStaticText( m_panelItemsProcessed, wxID_ANY, _("Items processed:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText96->Wrap( -1 );
	bSizer165->Add( m_staticText96, 0, wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer169;
	bSizer169 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticTextItemsProcessed = new wxStaticText( m_panelItemsProcessed, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticTextItemsProcessed->Wrap( -1 );
	m_staticTextItemsProcessed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer169->Add( m_staticTextItemsProcessed, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextBytesProcessed = new wxStaticText( m_panelItemsProcessed, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextBytesProcessed->Wrap( -1 );
	bSizer169->Add( m_staticTextBytesProcessed, 0, wxLEFT|wxALIGN_BOTTOM, 5 );
	
	
	bSizer165->Add( bSizer169, 0, wxRIGHT|wxLEFT, 5 );
	
	
	bSizer165->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelItemsProcessed->SetSizer( bSizer165 );
	m_panelItemsProcessed->Layout();
	bSizer165->Fit( m_panelItemsProcessed );
	bSizer232->Add( m_panelItemsProcessed, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 10 );
	
	m_panelItemsRemaining = new wxPanel( m_panelProgress, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelItemsRemaining->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer166;
	bSizer166 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer166->Add( 0, 5, 0, 0, 5 );
	
	wxStaticText* m_staticText97;
	m_staticText97 = new wxStaticText( m_panelItemsRemaining, wxID_ANY, _("Items remaining:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText97->Wrap( -1 );
	bSizer166->Add( m_staticText97, 0, wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer170;
	bSizer170 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticTextItemsRemaining = new wxStaticText( m_panelItemsRemaining, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticTextItemsRemaining->Wrap( -1 );
	m_staticTextItemsRemaining->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer170->Add( m_staticTextItemsRemaining, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextBytesRemaining = new wxStaticText( m_panelItemsRemaining, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextBytesRemaining->Wrap( -1 );
	bSizer170->Add( m_staticTextBytesRemaining, 0, wxLEFT|wxALIGN_BOTTOM, 5 );
	
	
	bSizer166->Add( bSizer170, 0, wxRIGHT|wxLEFT, 5 );
	
	
	bSizer166->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelItemsRemaining->SetSizer( bSizer166 );
	m_panelItemsRemaining->Layout();
	bSizer166->Fit( m_panelItemsRemaining );
	bSizer232->Add( m_panelItemsRemaining, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 10 );
	
	m_panelTimeRemaining = new wxPanel( m_panelProgress, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelTimeRemaining->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer167;
	bSizer167 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer167->Add( 0, 5, 0, 0, 5 );
	
	wxStaticText* m_staticText98;
	m_staticText98 = new wxStaticText( m_panelTimeRemaining, wxID_ANY, _("Time remaining:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText98->Wrap( -1 );
	bSizer167->Add( m_staticText98, 0, wxRIGHT|wxLEFT, 5 );
	
	m_staticTextTimeRemaining = new wxStaticText( m_panelTimeRemaining, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextTimeRemaining->Wrap( -1 );
	m_staticTextTimeRemaining->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer167->Add( m_staticTextTimeRemaining, 0, wxRIGHT|wxLEFT, 5 );
	
	
	bSizer167->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelTimeRemaining->SetSizer( bSizer167 );
	m_panelTimeRemaining->Layout();
	bSizer167->Fit( m_panelTimeRemaining );
	bSizer232->Add( m_panelTimeRemaining, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 10 );
	
	wxPanel* m_panelTimeElapsed;
	m_panelTimeElapsed = new wxPanel( m_panelProgress, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelTimeElapsed->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer168;
	bSizer168 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer168->Add( 0, 5, 0, 0, 5 );
	
	wxStaticText* m_staticText961;
	m_staticText961 = new wxStaticText( m_panelTimeElapsed, wxID_ANY, _("Time elapsed:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText961->Wrap( -1 );
	bSizer168->Add( m_staticText961, 0, wxRIGHT|wxLEFT, 5 );
	
	m_staticTextTimeElapsed = new wxStaticText( m_panelTimeElapsed, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextTimeElapsed->Wrap( -1 );
	m_staticTextTimeElapsed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer168->Add( m_staticTextTimeElapsed, 0, wxRIGHT|wxLEFT, 5 );
	
	
	bSizer168->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelTimeElapsed->SetSizer( bSizer168 );
	m_panelTimeElapsed->Layout();
	bSizer168->Fit( m_panelTimeElapsed );
	bSizer232->Add( m_panelTimeElapsed, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 10 );
	
	
	bSizer232->Add( 10, 0, 0, 0, 5 );
	
	
	bSizer232->Add( 0, 0, 1, 0, 5 );
	
	
	bSizer161->Add( bSizer232, 0, wxTOP|wxBOTTOM|wxEXPAND, 10 );
	
	m_panelGraphItems = new zen::Graph2D( m_panelProgress, wxID_ANY, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_panelGraphItems->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizer161->Add( m_panelGraphItems, 1, wxEXPAND|wxLEFT, 10 );
	
	bSizerProgressFooter = new wxBoxSizer( wxHORIZONTAL );
	
	bSizerErrorsRetry = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapRetryErrors = new wxStaticBitmap( m_panelProgress, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerErrorsRetry->Add( m_bitmapRetryErrors, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText1461 = new wxStaticText( m_panelProgress, wxID_ANY, _("Automatic retry"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1461->Wrap( -1 );
	bSizerErrorsRetry->Add( m_staticText1461, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	m_staticTextRetryCount = new wxStaticText( m_panelProgress, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextRetryCount->Wrap( -1 );
	bSizerErrorsRetry->Add( m_staticTextRetryCount, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	
	bSizerProgressFooter->Add( bSizerErrorsRetry, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	bSizerErrorsIgnore = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapIgnoreErrors = new wxStaticBitmap( m_panelProgress, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerErrorsIgnore->Add( m_bitmapIgnoreErrors, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText146 = new wxStaticText( m_panelProgress, wxID_ANY, _("Ignore errors"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText146->Wrap( -1 );
	bSizerErrorsIgnore->Add( m_staticText146, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	
	bSizerProgressFooter->Add( bSizerErrorsIgnore, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	
	bSizerProgressFooter->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticText137 = new wxStaticText( m_panelProgress, wxID_ANY, _("When finished:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText137->Wrap( -1 );
	bSizerProgressFooter->Add( m_staticText137, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	wxArrayString m_choicePostSyncActionChoices;
	m_choicePostSyncAction = new wxChoice( m_panelProgress, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choicePostSyncActionChoices, 0 );
	m_choicePostSyncAction->SetSelection( 0 );
	bSizerProgressFooter->Add( m_choicePostSyncAction, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer161->Add( bSizerProgressFooter, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 10 );
	
	
	bSizer173->Add( bSizer161, 1, wxEXPAND|wxLEFT, 10 );
	
	
	m_panelProgress->SetSizer( bSizer173 );
	m_panelProgress->Layout();
	bSizer173->Fit( m_panelProgress );
	bSizerRoot->Add( m_panelProgress, 1, wxEXPAND, 5 );
	
	m_notebookResult = new wxNotebook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_FIXEDWIDTH );
	m_notebookResult->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	
	bSizerRoot->Add( m_notebookResult, 1, wxEXPAND, 5 );
	
	m_staticlineFooter = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerRoot->Add( m_staticlineFooter, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_checkBoxAutoClose = new wxCheckBox( this, wxID_ANY, _("Auto-close"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizerStdButtons->Add( m_checkBoxAutoClose, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	bSizerStdButtons->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_buttonClose = new wxButton( this, wxID_OK, _("Close"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonClose->SetDefault(); 
	m_buttonClose->Enable( false );
	
	bSizerStdButtons->Add( m_buttonClose, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	m_buttonPause = new wxButton( this, wxID_ANY, _("&Pause"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonPause, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	m_buttonStop = new wxButton( this, wxID_CANCEL, _("Stop"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonStop, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizerRoot->Add( bSizerStdButtons, 0, wxEXPAND, 5 );
	
	
	this->SetSizer( bSizerRoot );
	this->Layout();
	bSizerRoot->Fit( this );
}

SyncProgressPanelGenerated::~SyncProgressPanelGenerated()
{
}

LogPanelGenerated::LogPanelGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) : wxPanel( parent, id, pos, size, style )
{
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer153;
	bSizer153 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer154;
	bSizer154 = new wxBoxSizer( wxVERTICAL );
	
	m_bpButtonErrors = new zen::ToggleButton( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizer154->Add( m_bpButtonErrors, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonWarnings = new zen::ToggleButton( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizer154->Add( m_bpButtonWarnings, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonInfo = new zen::ToggleButton( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizer154->Add( m_bpButtonInfo, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer153->Add( bSizer154, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	m_staticline13 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer153->Add( m_staticline13, 0, wxEXPAND, 5 );
	
	m_gridMessages = new zen::Grid( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_gridMessages->SetScrollRate( 5, 5 );
	bSizer153->Add( m_gridMessages, 1, wxEXPAND, 5 );
	
	
	this->SetSizer( bSizer153 );
	this->Layout();
	bSizer153->Fit( this );
	
	// Connect Events
	m_bpButtonErrors->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( LogPanelGenerated::OnErrors ), NULL, this );
	m_bpButtonWarnings->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( LogPanelGenerated::OnWarnings ), NULL, this );
	m_bpButtonInfo->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( LogPanelGenerated::OnInfo ), NULL, this );
}

LogPanelGenerated::~LogPanelGenerated()
{
}

BatchDlgGenerated::BatchDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( -1,-1 ), wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer54;
	bSizer54 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer72;
	bSizer72 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapBatchJob = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer72->Add( m_bitmapBatchJob, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_staticTextHeader = new wxStaticText( this, wxID_ANY, _("Create a batch file for unattended synchronization. To start, double-click this file or schedule in a task planner: %x"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextHeader->Wrap( -1 );
	bSizer72->Add( m_staticTextHeader, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	
	bSizer54->Add( bSizer72, 0, 0, 5 );
	
	m_staticline18 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer54->Add( m_staticline18, 0, wxEXPAND, 5 );
	
	m_panel35 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel35->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer172;
	bSizer172 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer180;
	bSizer180 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer2361;
	bSizer2361 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText146 = new wxStaticText( m_panel35, wxID_ANY, _("Progress dialog:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText146->Wrap( -1 );
	bSizer2361->Add( m_staticText146, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	
	ffgSizer11 = new wxFlexGridSizer( 0, 2, 5, 5 );
	ffgSizer11->SetFlexibleDirection( wxBOTH );
	ffgSizer11->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_bitmapMinimizeToTray = new wxStaticBitmap( m_panel35, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	ffgSizer11->Add( m_bitmapMinimizeToTray, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_checkBoxRunMinimized = new wxCheckBox( m_panel35, wxID_ANY, _("Run minimized"), wxDefaultPosition, wxDefaultSize, 0 );
	ffgSizer11->Add( m_checkBoxRunMinimized, 0, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	ffgSizer11->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_checkBoxAutoClose = new wxCheckBox( m_panel35, wxID_ANY, _("Auto-close"), wxDefaultPosition, wxDefaultSize, 0 );
	ffgSizer11->Add( m_checkBoxAutoClose, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer2361->Add( ffgSizer11, 0, wxEXPAND|wxALL, 5 );
	
	
	bSizer180->Add( bSizer2361, 0, wxALL, 5 );
	
	m_staticline26 = new wxStaticLine( m_panel35, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer180->Add( m_staticline26, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer242;
	bSizer242 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer243;
	bSizer243 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapIgnoreErrors = new wxStaticBitmap( m_panel35, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer243->Add( m_bitmapIgnoreErrors, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_checkBoxIgnoreErrors = new wxCheckBox( m_panel35, wxID_ANY, _("Ignore errors"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer243->Add( m_checkBoxIgnoreErrors, 1, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );
	
	
	bSizer242->Add( bSizer243, 0, wxTOP|wxRIGHT|wxLEFT|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	wxBoxSizer* bSizer246;
	bSizer246 = new wxBoxSizer( wxVERTICAL );
	
	m_radioBtnErrorDialogShow = new wxRadioButton( m_panel35, wxID_ANY, _("&Show error message"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
	m_radioBtnErrorDialogShow->SetValue( true ); 
	m_radioBtnErrorDialogShow->SetToolTip( _("Show pop-up on errors or warnings") );
	
	bSizer246->Add( m_radioBtnErrorDialogShow, 0, wxALL|wxEXPAND, 5 );
	
	m_radioBtnErrorDialogCancel = new wxRadioButton( m_panel35, wxID_ANY, _("&Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
	m_radioBtnErrorDialogCancel->SetToolTip( _("Stop synchronization at first error") );
	
	bSizer246->Add( m_radioBtnErrorDialogCancel, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxEXPAND, 5 );
	
	
	bSizer242->Add( bSizer246, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer180->Add( bSizer242, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticline261 = new wxStaticLine( m_panel35, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer180->Add( m_staticline261, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer247;
	bSizer247 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText137 = new wxStaticText( m_panel35, wxID_ANY, _("When finished:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText137->Wrap( -1 );
	bSizer247->Add( m_staticText137, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	
	wxArrayString m_choicePostSyncActionChoices;
	m_choicePostSyncAction = new wxChoice( m_panel35, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choicePostSyncActionChoices, 0 );
	m_choicePostSyncAction->SetSelection( 0 );
	bSizer247->Add( m_choicePostSyncAction, 0, wxALL, 5 );
	
	
	bSizer180->Add( bSizer247, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticline262 = new wxStaticLine( m_panel35, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer180->Add( m_staticline262, 0, wxEXPAND, 5 );
	
	
	bSizer172->Add( bSizer180, 0, 0, 5 );
	
	m_staticline25 = new wxStaticLine( m_panel35, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer172->Add( m_staticline25, 0, wxEXPAND, 5 );
	
	m_hyperlink17 = new wxHyperlinkCtrl( m_panel35, wxID_ANY, _("How can I schedule a batch job?"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer172->Add( m_hyperlink17, 0, wxALL, 10 );
	
	
	m_panel35->SetSizer( bSizer172 );
	m_panel35->Layout();
	bSizer172->Fit( m_panel35 );
	bSizer54->Add( m_panel35, 1, wxEXPAND, 5 );
	
	m_staticline13 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer54->Add( m_staticline13, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonSaveAs = new wxButton( this, wxID_SAVE, _("Save &as..."), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonSaveAs->SetDefault(); 
	m_buttonSaveAs->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonSaveAs, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer54->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer54 );
	this->Layout();
	bSizer54->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( BatchDlgGenerated::OnClose ) );
	m_checkBoxRunMinimized->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( BatchDlgGenerated::OnToggleRunMinimized ), NULL, this );
	m_checkBoxIgnoreErrors->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( BatchDlgGenerated::OnToggleIgnoreErrors ), NULL, this );
	m_radioBtnErrorDialogShow->Connect( wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler( BatchDlgGenerated::OnErrorDialogShow ), NULL, this );
	m_radioBtnErrorDialogCancel->Connect( wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler( BatchDlgGenerated::OnErrorDialogCancel ), NULL, this );
	m_hyperlink17->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( BatchDlgGenerated::OnHelpScheduleBatch ), NULL, this );
	m_buttonSaveAs->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( BatchDlgGenerated::OnSaveBatchJob ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( BatchDlgGenerated::OnCancel ), NULL, this );
}

BatchDlgGenerated::~BatchDlgGenerated()
{
}

DeleteDlgGenerated::DeleteDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( -1,-1 ), wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer24;
	bSizer24 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer72;
	bSizer72 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapDeleteType = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer72->Add( m_bitmapDeleteType, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_staticTextHeader = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextHeader->Wrap( -1 );
	bSizer72->Add( m_staticTextHeader, 0, wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	
	bSizer24->Add( bSizer72, 0, 0, 5 );
	
	m_staticline91 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer24->Add( m_staticline91, 0, wxEXPAND, 5 );
	
	m_panel31 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel31->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer185;
	bSizer185 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer185->Add( 60, 0, 0, 0, 5 );
	
	m_staticline42 = new wxStaticLine( m_panel31, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer185->Add( m_staticline42, 0, wxEXPAND, 5 );
	
	m_textCtrlFileList = new wxTextCtrl( m_panel31, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_DONTWRAP|wxTE_MULTILINE|wxTE_READONLY|wxNO_BORDER );
	bSizer185->Add( m_textCtrlFileList, 1, wxEXPAND, 5 );
	
	
	m_panel31->SetSizer( bSizer185 );
	m_panel31->Layout();
	bSizer185->Fit( m_panel31 );
	bSizer24->Add( m_panel31, 1, wxEXPAND, 5 );
	
	m_staticline9 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer24->Add( m_staticline9, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_checkBoxUseRecycler = new wxCheckBox( this, wxID_ANY, _("&Recycle bin"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizerStdButtons->Add( m_checkBoxUseRecycler, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	bSizerStdButtons->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_buttonOK = new wxButton( this, wxID_OK, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonOK->SetDefault(); 
	m_buttonOK->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonOK, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer24->Add( bSizerStdButtons, 0, wxEXPAND, 5 );
	
	
	this->SetSizer( bSizer24 );
	this->Layout();
	bSizer24->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( DeleteDlgGenerated::OnClose ) );
	m_checkBoxUseRecycler->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( DeleteDlgGenerated::OnUseRecycler ), NULL, this );
	m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DeleteDlgGenerated::OnOK ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DeleteDlgGenerated::OnCancel ), NULL, this );
}

DeleteDlgGenerated::~DeleteDlgGenerated()
{
}

CopyToDlgGenerated::CopyToDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( -1,-1 ), wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer24;
	bSizer24 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer72;
	bSizer72 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapCopyTo = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer72->Add( m_bitmapCopyTo, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_staticTextHeader = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextHeader->Wrap( -1 );
	bSizer72->Add( m_staticTextHeader, 0, wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	
	bSizer24->Add( bSizer72, 0, 0, 5 );
	
	m_staticline91 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer24->Add( m_staticline91, 0, wxEXPAND, 5 );
	
	m_panel31 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel31->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer242;
	bSizer242 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer185;
	bSizer185 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer185->Add( 60, 0, 0, 0, 5 );
	
	m_staticline42 = new wxStaticLine( m_panel31, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer185->Add( m_staticline42, 0, wxEXPAND, 5 );
	
	m_textCtrlFileList = new wxTextCtrl( m_panel31, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_DONTWRAP|wxTE_MULTILINE|wxTE_READONLY|wxNO_BORDER );
	bSizer185->Add( m_textCtrlFileList, 1, wxEXPAND, 5 );
	
	
	bSizer242->Add( bSizer185, 1, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer182;
	bSizer182 = new wxBoxSizer( wxHORIZONTAL );
	
	m_targetFolderPath = new fff::FolderHistoryBox( m_panel31, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer182->Add( m_targetFolderPath, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectTargetFolder = new wxButton( m_panel31, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectTargetFolder->SetToolTip( _("Select a folder") );
	
	bSizer182->Add( m_buttonSelectTargetFolder, 0, wxEXPAND, 5 );
	
	m_bpButtonSelectAltTargetFolder = new wxBitmapButton( m_panel31, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSelectAltTargetFolder->SetToolTip( _("Access online storage") );
	
	bSizer182->Add( m_bpButtonSelectAltTargetFolder, 0, wxEXPAND, 5 );
	
	
	bSizer242->Add( bSizer182, 0, wxALL|wxEXPAND, 10 );
	
	
	m_panel31->SetSizer( bSizer242 );
	m_panel31->Layout();
	bSizer242->Fit( m_panel31 );
	bSizer24->Add( m_panel31, 1, wxEXPAND, 5 );
	
	m_staticline9 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer24->Add( m_staticline9, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer189;
	bSizer189 = new wxBoxSizer( wxVERTICAL );
	
	m_checkBoxKeepRelPath = new wxCheckBox( this, wxID_ANY, _("&Keep relative paths"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxKeepRelPath->SetValue(true); 
	bSizer189->Add( m_checkBoxKeepRelPath, 0, wxALL|wxEXPAND, 5 );
	
	m_checkBoxOverwriteIfExists = new wxCheckBox( this, wxID_ANY, _("&Overwrite existing files"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxOverwriteIfExists->SetValue(true); 
	bSizer189->Add( m_checkBoxOverwriteIfExists, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxEXPAND, 5 );
	
	
	bSizerStdButtons->Add( bSizer189, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStdButtons->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_buttonOK = new wxButton( this, wxID_OK, _("Copy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonOK->SetDefault(); 
	m_buttonOK->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonOK, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer24->Add( bSizerStdButtons, 0, wxEXPAND, 5 );
	
	
	this->SetSizer( bSizer24 );
	this->Layout();
	bSizer24->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CopyToDlgGenerated::OnClose ) );
	m_checkBoxKeepRelPath->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( CopyToDlgGenerated::OnUseRecycler ), NULL, this );
	m_checkBoxOverwriteIfExists->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( CopyToDlgGenerated::OnUseRecycler ), NULL, this );
	m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CopyToDlgGenerated::OnOK ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CopyToDlgGenerated::OnCancel ), NULL, this );
}

CopyToDlgGenerated::~CopyToDlgGenerated()
{
}

OptionsDlgGenerated::OptionsDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( -1,-1 ), wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer95;
	bSizer95 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer72;
	bSizer72 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapSettings = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer72->Add( m_bitmapSettings, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_staticText44 = new wxStaticText( this, wxID_ANY, _("The following settings are used for all synchronization jobs."), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticText44->Wrap( -1 );
	bSizer72->Add( m_staticText44, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	
	bSizer95->Add( bSizer72, 0, 0, 5 );
	
	m_staticline20 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer95->Add( m_staticline20, 0, wxEXPAND, 5 );
	
	m_panel39 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel39->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer166;
	bSizer166 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer186;
	bSizer186 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer160;
	bSizer160 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer176;
	bSizer176 = new wxBoxSizer( wxHORIZONTAL );
	
	m_checkBoxFailSafe = new wxCheckBox( m_panel39, wxID_ANY, _("Fail-safe file copy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxFailSafe->SetValue(true); 
	m_checkBoxFailSafe->SetToolTip( _("Copy to a temporary file (*.ffs_tmp) before overwriting target.\nThis guarantees a consistent state even in case of a serious error.") );
	
	bSizer176->Add( m_checkBoxFailSafe, 1, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText911 = new wxStaticText( m_panel39, wxID_ANY, _("("), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText911->Wrap( -1 );
	m_staticText911->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer176->Add( m_staticText911, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5 );
	
	m_staticText91 = new wxStaticText( m_panel39, wxID_ANY, _("recommended"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText91->Wrap( -1 );
	m_staticText91->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer176->Add( m_staticText91, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5 );
	
	m_staticText9111 = new wxStaticText( m_panel39, wxID_ANY, _(")"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText9111->Wrap( -1 );
	m_staticText9111->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer176->Add( m_staticText9111, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer160->Add( bSizer176, 0, wxEXPAND, 5 );
	
	bSizerLockedFiles = new wxBoxSizer( wxHORIZONTAL );
	
	m_checkBoxCopyLocked = new wxCheckBox( m_panel39, wxID_ANY, _("Copy locked files"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxCopyLocked->SetValue(true); 
	m_checkBoxCopyLocked->SetToolTip( _("Copy shared or locked files using the Volume Shadow Copy Service.") );
	
	bSizerLockedFiles->Add( m_checkBoxCopyLocked, 1, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText921 = new wxStaticText( m_panel39, wxID_ANY, _("("), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText921->Wrap( -1 );
	m_staticText921->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizerLockedFiles->Add( m_staticText921, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5 );
	
	m_staticText92 = new wxStaticText( m_panel39, wxID_ANY, _("requires administrator rights"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText92->Wrap( -1 );
	m_staticText92->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizerLockedFiles->Add( m_staticText92, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5 );
	
	m_staticText922 = new wxStaticText( m_panel39, wxID_ANY, _(")"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText922->Wrap( -1 );
	m_staticText922->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizerLockedFiles->Add( m_staticText922, 0, wxTOP|wxBOTTOM|wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer160->Add( bSizerLockedFiles, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer178;
	bSizer178 = new wxBoxSizer( wxHORIZONTAL );
	
	m_checkBoxCopyPermissions = new wxCheckBox( m_panel39, wxID_ANY, _("Copy file access permissions"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxCopyPermissions->SetValue(true); 
	m_checkBoxCopyPermissions->SetToolTip( _("Transfer file and folder permissions.") );
	
	bSizer178->Add( m_checkBoxCopyPermissions, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_staticText931 = new wxStaticText( m_panel39, wxID_ANY, _("("), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText931->Wrap( -1 );
	m_staticText931->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer178->Add( m_staticText931, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5 );
	
	m_staticText93 = new wxStaticText( m_panel39, wxID_ANY, _("requires administrator rights"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText93->Wrap( -1 );
	m_staticText93->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer178->Add( m_staticText93, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5 );
	
	m_staticText932 = new wxStaticText( m_panel39, wxID_ANY, _(")"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText932->Wrap( -1 );
	m_staticText932->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer178->Add( m_staticText932, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer160->Add( bSizer178, 0, wxEXPAND, 5 );
	
	
	bSizer186->Add( bSizer160, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticline39 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer186->Add( m_staticline39, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer1881;
	bSizer1881 = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextResetDialogs = new wxStaticText( m_panel39, wxID_ANY, _("Show all permanently hidden dialogs and warning messages again"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextResetDialogs->Wrap( -1 );
	m_staticTextResetDialogs->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer1881->Add( m_staticTextResetDialogs, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	
	m_buttonResetDialogs = new zen::BitmapTextButton( m_panel39, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer1881->Add( m_buttonResetDialogs, 0, wxALL, 5 );
	
	
	bSizer186->Add( bSizer1881, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer166->Add( bSizer186, 0, wxEXPAND, 5 );
	
	m_staticline191 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer166->Add( m_staticline191, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer259;
	bSizer259 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer258;
	bSizer258 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapLogFile = new wxStaticBitmap( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer258->Add( m_bitmapLogFile, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText163 = new wxStaticText( m_panel39, wxID_ANY, _("Default log path:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText163->Wrap( -1 );
	bSizer258->Add( m_staticText163, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	m_hyperlinkLogFolder = new wxHyperlinkCtrl( m_panel39, wxID_ANY, _("dummy"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer258->Add( m_hyperlinkLogFolder, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );
	
	
	bSizer259->Add( bSizer258, 0, wxALL|wxEXPAND, 5 );
	
	wxBoxSizer* bSizer282;
	bSizer282 = new wxBoxSizer( wxHORIZONTAL );
	
	m_checkBoxLogFilesMaxAge = new wxCheckBox( m_panel39, wxID_ANY, _("&Delete logs after x days:"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer282->Add( m_checkBoxLogFilesMaxAge, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_spinCtrlLogFilesMaxAge = new wxSpinCtrl( m_panel39, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
	bSizer282->Add( m_spinCtrlLogFilesMaxAge, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer259->Add( bSizer282, 0, wxALL, 5 );
	
	
	bSizer166->Add( bSizer259, 0, wxALL|wxEXPAND, 5 );
	
	m_staticline361 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer166->Add( m_staticline361, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer288;
	bSizer288 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer291;
	bSizer291 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapNotificationSounds = new wxStaticBitmap( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer291->Add( m_bitmapNotificationSounds, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText851 = new wxStaticText( m_panel39, wxID_ANY, _("Notification sounds:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText851->Wrap( -1 );
	bSizer291->Add( m_staticText851, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );
	
	
	bSizer288->Add( bSizer291, 0, wxALL, 5 );
	
	ffgSizer11 = new wxFlexGridSizer( 0, 4, 0, 0 );
	ffgSizer11->AddGrowableCol( 3 );
	ffgSizer11->SetFlexibleDirection( wxBOTH );
	ffgSizer11->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	
	ffgSizer11->Add( 10, 0, 0, 0, 5 );
	
	m_staticText171 = new wxStaticText( m_panel39, wxID_ANY, _("Comparison finished:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText171->Wrap( -1 );
	ffgSizer11->Add( m_staticText171, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT, 5 );
	
	m_bitmapCompareDone = new wxStaticBitmap( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	ffgSizer11->Add( m_bitmapCompareDone, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer290;
	bSizer290 = new wxBoxSizer( wxHORIZONTAL );
	
	m_textCtrlSoundPathCompareDone = new wxTextCtrl( m_panel39, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer290->Add( m_textCtrlSoundPathCompareDone, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectSoundCompareDone = new wxButton( m_panel39, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectSoundCompareDone->SetToolTip( _("Select a folder") );
	
	bSizer290->Add( m_buttonSelectSoundCompareDone, 0, wxEXPAND, 5 );
	
	m_bpButtonPlayCompareDone = new wxBitmapButton( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW );
	bSizer290->Add( m_bpButtonPlayCompareDone, 0, wxEXPAND, 5 );
	
	
	ffgSizer11->Add( bSizer290, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	ffgSizer11->Add( 0, 0, 0, 0, 5 );
	
	m_staticText1711 = new wxStaticText( m_panel39, wxID_ANY, _("Synchronization finished:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1711->Wrap( -1 );
	ffgSizer11->Add( m_staticText1711, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT, 5 );
	
	m_bitmapSyncDone = new wxStaticBitmap( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	ffgSizer11->Add( m_bitmapSyncDone, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer2901;
	bSizer2901 = new wxBoxSizer( wxHORIZONTAL );
	
	m_textCtrlSoundPathSyncDone = new wxTextCtrl( m_panel39, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer2901->Add( m_textCtrlSoundPathSyncDone, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectSoundSyncDone = new wxButton( m_panel39, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectSoundSyncDone->SetToolTip( _("Select a folder") );
	
	bSizer2901->Add( m_buttonSelectSoundSyncDone, 0, wxEXPAND, 5 );
	
	m_bpButtonPlaySyncDone = new wxBitmapButton( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW );
	bSizer2901->Add( m_bpButtonPlaySyncDone, 0, wxEXPAND, 5 );
	
	
	ffgSizer11->Add( bSizer2901, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer288->Add( ffgSizer11, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer166->Add( bSizer288, 0, wxALL|wxEXPAND, 5 );
	
	m_staticline3611 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer166->Add( m_staticline3611, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer181;
	bSizer181 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText85 = new wxStaticText( m_panel39, wxID_ANY, _("Customize context menu:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText85->Wrap( -1 );
	bSizer181->Add( m_staticText85, 0, wxBOTTOM, 5 );
	
	m_gridCustomCommand = new wxGrid( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	
	// Grid
	m_gridCustomCommand->CreateGrid( 3, 2 );
	m_gridCustomCommand->EnableEditing( true );
	m_gridCustomCommand->EnableGridLines( true );
	m_gridCustomCommand->EnableDragGridSize( false );
	m_gridCustomCommand->SetMargins( 0, 0 );
	
	// Columns
	m_gridCustomCommand->EnableDragColMove( false );
	m_gridCustomCommand->EnableDragColSize( true );
	m_gridCustomCommand->SetColLabelSize( -1 );
	m_gridCustomCommand->SetColLabelValue( 0, _("Description") );
	m_gridCustomCommand->SetColLabelValue( 1, _("Command line") );
	m_gridCustomCommand->SetColLabelAlignment( wxALIGN_CENTRE, wxALIGN_CENTRE );
	
	// Rows
	m_gridCustomCommand->EnableDragRowSize( false );
	m_gridCustomCommand->SetRowLabelSize( 1 );
	m_gridCustomCommand->SetRowLabelAlignment( wxALIGN_CENTRE, wxALIGN_CENTRE );
	
	// Label Appearance
	
	// Cell Defaults
	m_gridCustomCommand->SetDefaultCellAlignment( wxALIGN_LEFT, wxALIGN_TOP );
	bSizer181->Add( m_gridCustomCommand, 1, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer193;
	bSizer193 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonAddRow = new wxBitmapButton( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizer193->Add( m_bpButtonAddRow, 0, wxEXPAND, 5 );
	
	m_bpButtonRemoveRow = new wxBitmapButton( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	bSizer193->Add( m_bpButtonRemoveRow, 0, wxEXPAND, 5 );
	
	
	bSizer193->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_hyperlink17 = new wxHyperlinkCtrl( m_panel39, wxID_ANY, _("Show examples"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer193->Add( m_hyperlink17, 0, wxLEFT, 5 );
	
	
	bSizer181->Add( bSizer193, 0, wxTOP|wxEXPAND, 5 );
	
	
	bSizer166->Add( bSizer181, 1, wxEXPAND|wxALL, 10 );
	
	
	m_panel39->SetSizer( bSizer166 );
	m_panel39->Layout();
	bSizer166->Fit( m_panel39 );
	bSizer95->Add( m_panel39, 1, wxEXPAND, 5 );
	
	m_staticline36 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer95->Add( m_staticline36, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonDefault = new wxButton( this, wxID_DEFAULT, _("&Default"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonDefault, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStdButtons->Add( 0, 0, 1, 0, 5 );
	
	m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonOkay->SetDefault(); 
	m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonOkay, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer95->Add( bSizerStdButtons, 0, wxEXPAND, 5 );
	
	
	this->SetSizer( bSizer95 );
	this->Layout();
	bSizer95->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( OptionsDlgGenerated::OnClose ) );
	m_buttonResetDialogs->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnResetDialogs ), NULL, this );
	m_hyperlinkLogFolder->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( OptionsDlgGenerated::OnShowLogFolder ), NULL, this );
	m_checkBoxLogFilesMaxAge->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnToggleLogfilesLimit ), NULL, this );
	m_textCtrlSoundPathCompareDone->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( OptionsDlgGenerated::OnChangeSoundFilePath ), NULL, this );
	m_buttonSelectSoundCompareDone->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnSelectSoundCompareDone ), NULL, this );
	m_bpButtonPlayCompareDone->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnPlayCompareDone ), NULL, this );
	m_textCtrlSoundPathSyncDone->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( OptionsDlgGenerated::OnChangeSoundFilePath ), NULL, this );
	m_buttonSelectSoundSyncDone->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnSelectSoundSyncDone ), NULL, this );
	m_bpButtonPlaySyncDone->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnPlaySyncDone ), NULL, this );
	m_bpButtonAddRow->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnAddRow ), NULL, this );
	m_bpButtonRemoveRow->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnRemoveRow ), NULL, this );
	m_hyperlink17->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( OptionsDlgGenerated::OnHelpShowExamples ), NULL, this );
	m_buttonDefault->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnDefault ), NULL, this );
	m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnOkay ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnCancel ), NULL, this );
}

OptionsDlgGenerated::~OptionsDlgGenerated()
{
}

TooltipDlgGenerated::TooltipDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxBoxSizer* bSizer158;
	bSizer158 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapLeft = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer158->Add( m_bitmapLeft, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextMain = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextMain->Wrap( 600 );
	bSizer158->Add( m_staticTextMain, 0, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	this->SetSizer( bSizer158 );
	this->Layout();
	bSizer158->Fit( this );
}

TooltipDlgGenerated::~TooltipDlgGenerated()
{
}

SelectTimespanDlgGenerated::SelectTimespanDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer96;
	bSizer96 = new wxBoxSizer( wxVERTICAL );
	
	m_panel35 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel35->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer98;
	bSizer98 = new wxBoxSizer( wxHORIZONTAL );
	
	m_calendarFrom = new wxCalendarCtrl( m_panel35, wxID_ANY, wxDefaultDateTime, wxDefaultPosition, wxDefaultSize, wxCAL_SHOW_HOLIDAYS|wxNO_BORDER );
	bSizer98->Add( m_calendarFrom, 0, wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_calendarTo = new wxCalendarCtrl( m_panel35, wxID_ANY, wxDefaultDateTime, wxDefaultPosition, wxDefaultSize, wxCAL_SHOW_HOLIDAYS|wxNO_BORDER );
	bSizer98->Add( m_calendarTo, 0, wxALL, 10 );
	
	
	m_panel35->SetSizer( bSizer98 );
	m_panel35->Layout();
	bSizer98->Fit( m_panel35 );
	bSizer96->Add( m_panel35, 0, wxEXPAND, 5 );
	
	m_staticline21 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer96->Add( m_staticline21, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonOkay->SetDefault(); 
	m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonOkay, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer96->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer96 );
	this->Layout();
	bSizer96->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( SelectTimespanDlgGenerated::OnClose ) );
	m_calendarFrom->Connect( wxEVT_CALENDAR_SEL_CHANGED, wxCalendarEventHandler( SelectTimespanDlgGenerated::OnChangeSelectionFrom ), NULL, this );
	m_calendarTo->Connect( wxEVT_CALENDAR_SEL_CHANGED, wxCalendarEventHandler( SelectTimespanDlgGenerated::OnChangeSelectionTo ), NULL, this );
	m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( SelectTimespanDlgGenerated::OnOkay ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( SelectTimespanDlgGenerated::OnCancel ), NULL, this );
}

SelectTimespanDlgGenerated::~SelectTimespanDlgGenerated()
{
}

AboutDlgGenerated::AboutDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer31;
	bSizer31 = new wxBoxSizer( wxVERTICAL );
	
	m_panel41 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel41->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer174;
	bSizer174 = new wxBoxSizer( wxHORIZONTAL );
	
	bSizerMainSection = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapLogo = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerMainSection->Add( m_bitmapLogo, 0, 0, 5 );
	
	m_staticline341 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerMainSection->Add( m_staticline341, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer186;
	bSizer186 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText94 = new wxStaticText( m_panel41, wxID_ANY, _("Feedback and suggestions are welcome:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText94->Wrap( -1 );
	bSizer186->Add( m_staticText94, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer166;
	bSizer166 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer251;
	bSizer251 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapHomepage = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_bitmapHomepage->SetToolTip( _("Home page") );
	
	bSizer251->Add( m_bitmapHomepage, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_hyperlink1 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("FreeFileSync.org"), wxT("https://freefilesync.org/"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink1->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, true, wxEmptyString ) );
	m_hyperlink1->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink1->SetToolTip( _("https://freefilesync.org") );
	
	bSizer251->Add( m_hyperlink1, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer166->Add( bSizer251, 0, wxALIGN_BOTTOM|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer250;
	bSizer250 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapForum = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_bitmapForum->SetToolTip( _("FreeFileSync Forum") );
	
	bSizer250->Add( m_bitmapForum, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_hyperlink21 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("FreeFileSync Forum"), wxT("https://freefilesync.org/forum/"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink21->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, true, wxEmptyString ) );
	m_hyperlink21->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink21->SetToolTip( _("https://freefilesync.org/forum/") );
	
	bSizer250->Add( m_hyperlink21, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer166->Add( bSizer250, 0, wxALIGN_BOTTOM|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer249;
	bSizer249 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapEmail = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_bitmapEmail->SetToolTip( _("Email") );
	
	bSizer249->Add( m_bitmapEmail, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_hyperlink2 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("zenju@freefilesync.org"), wxT("mailto:zenju@freefilesync.org"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink2->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, true, wxEmptyString ) );
	m_hyperlink2->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink2->SetToolTip( _("mailto:zenju@freefilesync.org") );
	
	bSizer249->Add( m_hyperlink2, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer166->Add( bSizer249, 0, wxALIGN_BOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer186->Add( bSizer166, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerMainSection->Add( bSizer186, 0, wxALL|wxEXPAND, 5 );
	
	m_staticline3412 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerMainSection->Add( m_staticline3412, 0, wxEXPAND, 5 );
	
	m_panelDonate = new wxPanel( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelDonate->SetBackgroundColour( wxColour( 153, 170, 187 ) );
	
	wxBoxSizer* bSizer183;
	bSizer183 = new wxBoxSizer( wxVERTICAL );
	
	m_panel39 = new wxPanel( m_panelDonate, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel39->SetBackgroundColour( wxColour( 248, 248, 248 ) );
	
	wxBoxSizer* bSizer184;
	bSizer184 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer184->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_bitmapDonate = new wxStaticBitmap( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer184->Add( m_bitmapDonate, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	wxBoxSizer* bSizer178;
	bSizer178 = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextDonate = new wxStaticText( m_panel39, wxID_ANY, _("If you like FreeFileSync:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDonate->Wrap( -1 );
	m_staticTextDonate->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxEmptyString ) );
	m_staticTextDonate->SetForegroundColour( wxColour( 0, 0, 0 ) );
	
	bSizer178->Add( m_staticTextDonate, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_buttonDonate = new wxButton( m_panel39, wxID_ANY, _("Support with a donation"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonDonate->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	m_buttonDonate->SetToolTip( _("https://freefilesync.org/donate.php") );
	
	bSizer178->Add( m_buttonDonate, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer184->Add( bSizer178, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	
	bSizer184->Add( 0, 0, 1, wxEXPAND, 5 );
	
	
	m_panel39->SetSizer( bSizer184 );
	m_panel39->Layout();
	bSizer184->Fit( m_panel39 );
	bSizer183->Add( m_panel39, 0, wxALL|wxEXPAND, 5 );
	
	
	m_panelDonate->SetSizer( bSizer183 );
	m_panelDonate->Layout();
	bSizer183->Fit( m_panelDonate );
	bSizerMainSection->Add( m_panelDonate, 0, wxEXPAND|wxTOP|wxRIGHT|wxLEFT, 10 );
	
	m_panelThankYou = new wxPanel( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelThankYou->SetBackgroundColour( wxColour( 153, 170, 187 ) );
	
	wxBoxSizer* bSizer1831;
	bSizer1831 = new wxBoxSizer( wxVERTICAL );
	
	m_panel391 = new wxPanel( m_panelThankYou, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel391->SetBackgroundColour( wxColour( 248, 248, 248 ) );
	
	wxBoxSizer* bSizer243;
	bSizer243 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer1841;
	bSizer1841 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapThanks = new wxStaticBitmap( m_panel391, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer1841->Add( m_bitmapThanks, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	m_staticTextThanks = new wxStaticText( m_panel391, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextThanks->Wrap( -1 );
	m_staticTextThanks->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxEmptyString ) );
	m_staticTextThanks->SetForegroundColour( wxColour( 0, 0, 0 ) );
	
	bSizer1841->Add( m_staticTextThanks, 0, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer243->Add( bSizer1841, 1, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 5 );
	
	m_staticTextNoAutoUpdate = new wxStaticText( m_panel391, wxID_ANY, _("The auto updater was disabled by the administrator."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextNoAutoUpdate->Wrap( -1 );
	m_staticTextNoAutoUpdate->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL, false, wxEmptyString ) );
	
	bSizer243->Add( m_staticTextNoAutoUpdate, 0, wxALIGN_CENTER_HORIZONTAL|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	m_buttonShowDonationDetails = new wxButton( m_panel391, wxID_ANY, _("Donation details"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonShowDonationDetails->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxEmptyString ) );
	
	bSizer243->Add( m_buttonShowDonationDetails, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxEXPAND, 5 );
	
	
	m_panel391->SetSizer( bSizer243 );
	m_panel391->Layout();
	bSizer243->Fit( m_panel391 );
	bSizer1831->Add( m_panel391, 0, wxEXPAND|wxALL, 5 );
	
	
	m_panelThankYou->SetSizer( bSizer1831 );
	m_panelThankYou->Layout();
	bSizer1831->Fit( m_panelThankYou );
	bSizerMainSection->Add( m_panelThankYou, 0, wxEXPAND|wxTOP|wxRIGHT|wxLEFT, 10 );
	
	wxBoxSizer* bSizer187;
	bSizer187 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText96 = new wxStaticText( m_panel41, wxID_ANY, _("Source code written in C++ using:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText96->Wrap( -1 );
	bSizer187->Add( m_staticText96, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer171;
	bSizer171 = new wxBoxSizer( wxHORIZONTAL );
	
	m_hyperlink11 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("MS Visual Studio"), wxT("https://www.visualstudio.com"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink11->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink11->SetToolTip( _("https://www.visualstudio.com") );
	
	bSizer171->Add( m_hyperlink11, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink7 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("wxWidgets"), wxT("http://www.wxwidgets.org"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink7->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink7->SetToolTip( _("http://www.wxwidgets.org") );
	
	bSizer171->Add( m_hyperlink7, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink14 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("wxFormBuilder"), wxT("https://github.com/wxFormBuilder/wxFormBuilder"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink14->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink14->SetToolTip( _("https://github.com/wxFormBuilder/wxFormBuilder") );
	
	bSizer171->Add( m_hyperlink14, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink16 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("Artistic Style"), wxT("http://astyle.sourceforge.net"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink16->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink16->SetToolTip( _("http://astyle.sourceforge.net") );
	
	bSizer171->Add( m_hyperlink16, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer187->Add( bSizer171, 0, wxALIGN_CENTER_HORIZONTAL|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer172;
	bSizer172 = new wxBoxSizer( wxHORIZONTAL );
	
	m_hyperlink15 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("zen::Xml"), wxT("http://zenxml.sourceforge.net"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink15->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink15->SetToolTip( _("http://zenxml.sourceforge.net") );
	
	bSizer172->Add( m_hyperlink15, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink12 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("Google Test"), wxT("https://github.com/google/googletest"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink12->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink12->SetToolTip( _("https://github.com/google/googletest") );
	
	bSizer172->Add( m_hyperlink12, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink10 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("libssh2"), wxT("https://www.libssh2.org"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink10->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink10->SetToolTip( _("https://www.libssh2.org") );
	
	bSizer172->Add( m_hyperlink10, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink101 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("libcurl"), wxT("https://curl.haxx.se/libcurl"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink101->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink101->SetToolTip( _("https://curl.haxx.se/libcurl") );
	
	bSizer172->Add( m_hyperlink101, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_hyperlink18 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("NSIS"), wxT("http://nsis.sourceforge.net"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink18->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink18->SetToolTip( _("http://nsis.sourceforge.net") );
	
	bSizer172->Add( m_hyperlink18, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink9 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("Inno Setup"), wxT("http://www.jrsoftware.org"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink9->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink9->SetToolTip( _("http://www.jrsoftware.org") );
	
	bSizer172->Add( m_hyperlink9, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer187->Add( bSizer172, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerMainSection->Add( bSizer187, 0, wxALL|wxEXPAND, 5 );
	
	m_staticline34 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerMainSection->Add( m_staticline34, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer185;
	bSizer185 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText93 = new wxStaticText( m_panel41, wxID_ANY, _("Published under the GNU General Public License:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText93->Wrap( -1 );
	bSizer185->Add( m_staticText93, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer1671;
	bSizer1671 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapGpl = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer1671->Add( m_bitmapGpl, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink5 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("https://www.gnu.org/licenses/gpl-3.0"), wxT("https://www.gnu.org/licenses/gpl-3.0"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink5->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizer1671->Add( m_hyperlink5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer185->Add( bSizer1671, 0, wxALIGN_CENTER_HORIZONTAL|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizerMainSection->Add( bSizer185, 0, wxALL|wxEXPAND, 5 );
	
	
	bSizer174->Add( bSizerMainSection, 0, 0, 5 );
	
	m_staticline37 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer174->Add( m_staticline37, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer177;
	bSizer177 = new wxBoxSizer( wxVERTICAL );
	
	m_staticline74 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer177->Add( m_staticline74, 0, wxEXPAND, 5 );
	
	m_staticTextThanksForLoc = new wxStaticText( m_panel41, wxID_ANY, _("Many thanks for localization:"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticTextThanksForLoc->Wrap( -1 );
	m_staticTextThanksForLoc->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer177->Add( m_staticTextThanksForLoc, 0, wxALIGN_CENTER_HORIZONTAL|wxTOP|wxRIGHT|wxLEFT, 10 );
	
	
	bSizer177->Add( 0, 5, 0, 0, 5 );
	
	m_scrolledWindowTranslators = new wxScrolledWindow( m_panel41, wxID_ANY, wxDefaultPosition, wxSize( -1,-1 ), wxVSCROLL );
	m_scrolledWindowTranslators->SetScrollRate( 10, 10 );
	m_scrolledWindowTranslators->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	fgSizerTranslators = new wxFlexGridSizer( 0, 2, 2, 10 );
	fgSizerTranslators->SetFlexibleDirection( wxBOTH );
	fgSizerTranslators->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	
	m_scrolledWindowTranslators->SetSizer( fgSizerTranslators );
	m_scrolledWindowTranslators->Layout();
	fgSizerTranslators->Fit( m_scrolledWindowTranslators );
	bSizer177->Add( m_scrolledWindowTranslators, 1, wxLEFT|wxEXPAND, 10 );
	
	
	bSizer174->Add( bSizer177, 0, wxEXPAND, 5 );
	
	
	m_panel41->SetSizer( bSizer174 );
	m_panel41->Layout();
	bSizer174->Fit( m_panel41 );
	bSizer31->Add( m_panel41, 0, wxEXPAND, 5 );
	
	m_staticline36 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer31->Add( m_staticline36, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonClose = new wxButton( this, wxID_OK, _("Close"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonClose->SetDefault(); 
	bSizerStdButtons->Add( m_buttonClose, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer31->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer31 );
	this->Layout();
	bSizer31->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( AboutDlgGenerated::OnClose ) );
	m_buttonDonate->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AboutDlgGenerated::OnDonate ), NULL, this );
	m_buttonShowDonationDetails->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AboutDlgGenerated::OnShowDonationDetails ), NULL, this );
	m_buttonClose->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AboutDlgGenerated::OnOK ), NULL, this );
}

AboutDlgGenerated::~AboutDlgGenerated()
{
}

DownloadProgressDlgGenerated::DownloadProgressDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxBoxSizer* bSizer24;
	bSizer24 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer72;
	bSizer72 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapDownloading = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer72->Add( m_bitmapDownloading, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_staticTextHeader = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextHeader->Wrap( -1 );
	bSizer72->Add( m_staticTextHeader, 0, wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	
	bSizer72->Add( 20, 0, 0, 0, 5 );
	
	
	bSizer24->Add( bSizer72, 0, 0, 5 );
	
	wxBoxSizer* bSizer212;
	bSizer212 = new wxBoxSizer( wxVERTICAL );
	
	m_gaugeProgress = new wxGauge( this, wxID_ANY, 100, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL );
	m_gaugeProgress->SetValue( 0 ); 
	bSizer212->Add( m_gaugeProgress, 0, wxEXPAND|wxRIGHT|wxLEFT, 5 );
	
	m_staticTextDetails = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDetails->Wrap( -1 );
	bSizer212->Add( m_staticTextDetails, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );
	
	
	bSizer24->Add( bSizer212, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	m_staticline9 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer24->Add( m_staticline9, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonCancel->SetDefault(); 
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	bSizer24->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer24 );
	this->Layout();
	bSizer24->Fit( this );
	
	// Connect Events
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DownloadProgressDlgGenerated::OnCancel ), NULL, this );
}

DownloadProgressDlgGenerated::~DownloadProgressDlgGenerated()
{
}

ActivationDlgGenerated::ActivationDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( -1,-1 ), wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer54;
	bSizer54 = new wxBoxSizer( wxVERTICAL );
	
	m_panel35 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel35->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer172;
	bSizer172 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer165;
	bSizer165 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapActivation = new wxStaticBitmap( m_panel35, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer165->Add( m_bitmapActivation, 0, wxALL, 10 );
	
	
	bSizer165->Add( 0, 120, 0, 0, 5 );
	
	wxBoxSizer* bSizer16;
	bSizer16 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer16->Add( 0, 10, 0, 0, 5 );
	
	m_textCtrlLastError = new wxTextCtrl( m_panel35, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY|wxNO_BORDER );
	bSizer16->Add( m_textCtrlLastError, 1, wxEXPAND, 5 );
	
	
	bSizer16->Add( 0, 5, 0, 0, 5 );
	
	
	bSizer165->Add( bSizer16, 1, wxEXPAND, 5 );
	
	
	bSizer172->Add( bSizer165, 1, wxEXPAND, 5 );
	
	m_staticTextMain = new wxStaticText( m_panel35, wxID_ANY, _("Activate the FreeFileSync Donation Edition by one of the following methods:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextMain->Wrap( -1 );
	bSizer172->Add( m_staticTextMain, 0, wxBOTTOM|wxRIGHT|wxLEFT, 10 );
	
	
	m_panel35->SetSizer( bSizer172 );
	m_panel35->Layout();
	bSizer172->Fit( m_panel35 );
	bSizer54->Add( m_panel35, 1, wxEXPAND, 5 );
	
	m_staticline181 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer54->Add( m_staticline181, 0, wxEXPAND|wxBOTTOM, 5 );
	
	m_staticline18111 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer54->Add( m_staticline18111, 0, wxEXPAND|wxTOP, 5 );
	
	m_panel3511 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel3511->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer263;
	bSizer263 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer234;
	bSizer234 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText136 = new wxStaticText( m_panel3511, wxID_ANY, _("1. Activate via internet now:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText136->Wrap( -1 );
	bSizer234->Add( m_staticText136, 1, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	m_buttonActivateOnline = new wxButton( m_panel3511, wxID_ANY, _("Activate online"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonActivateOnline->SetDefault(); 
	m_buttonActivateOnline->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer234->Add( m_buttonActivateOnline, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	bSizer263->Add( bSizer234, 0, wxEXPAND|wxALL, 5 );
	
	
	m_panel3511->SetSizer( bSizer263 );
	m_panel3511->Layout();
	bSizer263->Fit( m_panel3511 );
	bSizer54->Add( m_panel3511, 0, wxEXPAND, 5 );
	
	m_staticline181111 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer54->Add( m_staticline181111, 0, wxEXPAND|wxBOTTOM, 5 );
	
	m_staticline181112 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer54->Add( m_staticline181112, 0, wxEXPAND|wxTOP, 5 );
	
	m_panel351 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel351->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer266;
	bSizer266 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer237;
	bSizer237 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer236;
	bSizer236 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText1361 = new wxStaticText( m_panel351, wxID_ANY, _("2. Retrieve an offline activation key from the following URL:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText1361->Wrap( -1 );
	bSizer236->Add( m_staticText1361, 1, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	m_buttonCopyUrl = new wxButton( m_panel351, wxID_ANY, _("&Copy to clipboard"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonCopyUrl->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxEmptyString ) );
	
	bSizer236->Add( m_buttonCopyUrl, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	bSizer237->Add( bSizer236, 0, wxEXPAND, 5 );
	
	m_textCtrlManualActivationUrl = new wxTextCtrl( m_panel351, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,55 ), wxTE_MULTILINE|wxTE_READONLY );
	bSizer237->Add( m_textCtrlManualActivationUrl, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxEXPAND, 5 );
	
	wxBoxSizer* bSizer235;
	bSizer235 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText13611 = new wxStaticText( m_panel351, wxID_ANY, _("Enter activation key:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText13611->Wrap( -1 );
	bSizer235->Add( m_staticText13611, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	m_textCtrlOfflineActivationKey = new wxTextCtrl( m_panel351, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 250,-1 ), wxTE_PROCESS_ENTER );
	bSizer235->Add( m_textCtrlOfflineActivationKey, 1, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonActivateOffline = new wxButton( m_panel351, wxID_ANY, _("Activate offline"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonActivateOffline->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizer235->Add( m_buttonActivateOffline, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer237->Add( bSizer235, 0, wxEXPAND, 5 );
	
	
	bSizer266->Add( bSizer237, 0, wxALL|wxEXPAND, 5 );
	
	
	m_panel351->SetSizer( bSizer266 );
	m_panel351->Layout();
	bSizer266->Fit( m_panel351 );
	bSizer54->Add( m_panel351, 0, wxEXPAND, 5 );
	
	m_staticline13 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer54->Add( m_staticline13, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer54->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer54 );
	this->Layout();
	bSizer54->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( ActivationDlgGenerated::OnClose ) );
	m_buttonActivateOnline->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ActivationDlgGenerated::OnActivateOnline ), NULL, this );
	m_buttonCopyUrl->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ActivationDlgGenerated::OnCopyUrl ), NULL, this );
	m_textCtrlOfflineActivationKey->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( ActivationDlgGenerated::OnOfflineActivationEnter ), NULL, this );
	m_buttonActivateOffline->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ActivationDlgGenerated::OnActivateOffline ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ActivationDlgGenerated::OnCancel ), NULL, this );
}

ActivationDlgGenerated::~ActivationDlgGenerated()
{
}

CfgHighlightDlgGenerated::CfgHighlightDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer96;
	bSizer96 = new wxBoxSizer( wxVERTICAL );
	
	m_panel35 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel35->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer98;
	bSizer98 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer238;
	bSizer238 = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextHighlight = new wxStaticText( m_panel35, wxID_ANY, _("Highlight configurations that have not been run for more than the following number of days:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextHighlight->Wrap( -1 );
	bSizer238->Add( m_staticTextHighlight, 0, wxTOP|wxRIGHT|wxLEFT, 5 );
	
	m_spinCtrlOverdueDays = new wxSpinCtrl( m_panel35, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxSP_ARROW_KEYS, 0, 2000000000, 0 );
	bSizer238->Add( m_spinCtrlOverdueDays, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer98->Add( bSizer238, 1, wxALL|wxEXPAND, 5 );
	
	
	m_panel35->SetSizer( bSizer98 );
	m_panel35->Layout();
	bSizer98->Fit( m_panel35 );
	bSizer96->Add( m_panel35, 0, 0, 5 );
	
	m_staticline21 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer96->Add( m_staticline21, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonOkay->SetDefault(); 
	m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonOkay, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer96->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer96 );
	this->Layout();
	bSizer96->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CfgHighlightDlgGenerated::OnClose ) );
	m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CfgHighlightDlgGenerated::OnOkay ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CfgHighlightDlgGenerated::OnCancel ), NULL, this );
}

CfgHighlightDlgGenerated::~CfgHighlightDlgGenerated()
{
}

///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-0-g8feb16b3)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "gui_generated.h"

///////////////////////////////////////////////////////////////////////////

MainDialogGenerated::MainDialogGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxFrame( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxDefaultSize, wxDefaultSize );

    m_menubar = new wxMenuBar( 0 );
    m_menuFile = new wxMenu();
    m_menuItemNew = new wxMenuItem( m_menuFile, wxID_NEW, wxString( _("&New") ) + wxT('\t') + wxT("Ctrl+N"), wxEmptyString, wxITEM_NORMAL );
    m_menuFile->Append( m_menuItemNew );

    m_menuItemLoad = new wxMenuItem( m_menuFile, wxID_OPEN, wxString( _("&Open...") ) + wxT('\t') + wxT("Ctrl+O"), wxEmptyString, wxITEM_NORMAL );
    m_menuFile->Append( m_menuItemLoad );

    m_menuFile->AppendSeparator();

    m_menuItemSave = new wxMenuItem( m_menuFile, wxID_SAVE, wxString( _("&Save") ) + wxT('\t') + wxT("Ctrl+S"), wxEmptyString, wxITEM_NORMAL );
    m_menuFile->Append( m_menuItemSave );

    m_menuItemSaveAs = new wxMenuItem( m_menuFile, wxID_SAVEAS, wxString( _("Save &as...") ), wxEmptyString, wxITEM_NORMAL );
    m_menuFile->Append( m_menuItemSaveAs );

    m_menuItemSaveAsBatch = new wxMenuItem( m_menuFile, wxID_ANY, wxString( _("Save as &batch job...") ), wxEmptyString, wxITEM_NORMAL );
    m_menuFile->Append( m_menuItemSaveAsBatch );

    m_menuFile->AppendSeparator();

    m_menuItemQuit = new wxMenuItem( m_menuFile, wxID_EXIT, wxString( _("E&xit") ), wxEmptyString, wxITEM_NORMAL );
    m_menuFile->Append( m_menuItemQuit );

    m_menubar->Append( m_menuFile, _("&File") );

    m_menuActions = new wxMenu();
    m_menuItemShowLog = new wxMenuItem( m_menuActions, wxID_ANY, wxString( _("Show &log") ) + wxT('\t') + wxT("F4"), wxEmptyString, wxITEM_NORMAL );
    m_menuActions->Append( m_menuItemShowLog );

    m_menuActions->AppendSeparator();

    m_menuItemCompare = new wxMenuItem( m_menuActions, wxID_ANY, wxString( _("Start &comparison") ) + wxT('\t') + wxT("F5"), wxEmptyString, wxITEM_NORMAL );
    m_menuActions->Append( m_menuItemCompare );

    m_menuActions->AppendSeparator();

    m_menuItemCompSettings = new wxMenuItem( m_menuActions, wxID_ANY, wxString( _("C&omparison settings") ) + wxT('\t') + wxT("F6"), wxEmptyString, wxITEM_NORMAL );
    m_menuActions->Append( m_menuItemCompSettings );

    m_menuItemFilter = new wxMenuItem( m_menuActions, wxID_ANY, wxString( _("&Filter settings") ) + wxT('\t') + wxT("F7"), wxEmptyString, wxITEM_NORMAL );
    m_menuActions->Append( m_menuItemFilter );

    m_menuItemSyncSettings = new wxMenuItem( m_menuActions, wxID_ANY, wxString( _("S&ynchronization settings") ) + wxT('\t') + wxT("F8"), wxEmptyString, wxITEM_NORMAL );
    m_menuActions->Append( m_menuItemSyncSettings );

    m_menuActions->AppendSeparator();

    m_menuItemSynchronize = new wxMenuItem( m_menuActions, wxID_ANY, wxString( _("Start &synchronization") ) + wxT('\t') + wxT("F9"), wxEmptyString, wxITEM_NORMAL );
    m_menuActions->Append( m_menuItemSynchronize );

    m_menubar->Append( m_menuActions, _("&Actions") );

    m_menuTools = new wxMenu();
    m_menuItemOptions = new wxMenuItem( m_menuTools, wxID_PREFERENCES, wxString( _("&Preferences") ) + wxT('\t') + wxT("Ctrl+,"), wxEmptyString, wxITEM_NORMAL );
    m_menuTools->Append( m_menuItemOptions );

    m_menuLanguages = new wxMenu();
    wxMenuItem* m_menuLanguagesItem = new wxMenuItem( m_menuTools, wxID_ANY, _("&Language"), wxEmptyString, wxITEM_NORMAL, m_menuLanguages );
    m_menuTools->Append( m_menuLanguagesItem );

    m_menuTools->AppendSeparator();

    m_menuItemFind = new wxMenuItem( m_menuTools, wxID_FIND, wxString( _("&Find...") ) + wxT('\t') + wxT("Ctrl+F"), wxEmptyString, wxITEM_NORMAL );
    m_menuTools->Append( m_menuItemFind );

    m_menuItemExportList = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("&Export file list") ), wxEmptyString, wxITEM_NORMAL );
    m_menuTools->Append( m_menuItemExportList );

    m_menuTools->AppendSeparator();

    m_menuItemResetLayout = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("&Reset layout") ), wxEmptyString, wxITEM_NORMAL );
    m_menuTools->Append( m_menuItemResetLayout );

    m_menuItemShowMain = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("dummy") ), wxEmptyString, wxITEM_NORMAL );
    m_menuTools->Append( m_menuItemShowMain );

    m_menuItemShowFolders = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("dummy") ), wxEmptyString, wxITEM_NORMAL );
    m_menuTools->Append( m_menuItemShowFolders );

    m_menuItemShowViewFilter = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("dummy") ), wxEmptyString, wxITEM_NORMAL );
    m_menuTools->Append( m_menuItemShowViewFilter );

    m_menuItemShowConfig = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("dummy") ), wxEmptyString, wxITEM_NORMAL );
    m_menuTools->Append( m_menuItemShowConfig );

    m_menuItemShowOverview = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("dummy") ), wxEmptyString, wxITEM_NORMAL );
    m_menuTools->Append( m_menuItemShowOverview );

    m_menubar->Append( m_menuTools, _("&Tools") );

    m_menuHelp = new wxMenu();
    m_menuItemHelp = new wxMenuItem( m_menuHelp, wxID_HELP, wxString( _("&View help") ) + wxT('\t') + wxT("F1"), wxEmptyString, wxITEM_NORMAL );
    m_menuHelp->Append( m_menuItemHelp );

    m_menuHelp->AppendSeparator();

    m_menuItemCheckVersionNow = new wxMenuItem( m_menuHelp, wxID_ANY, wxString( _("&Check for updates now") ), wxEmptyString, wxITEM_NORMAL );
    m_menuHelp->Append( m_menuItemCheckVersionNow );

    m_menuHelp->AppendSeparator();

    m_menuItemAbout = new wxMenuItem( m_menuHelp, wxID_ABOUT, wxString( _("&About") ) + wxT('\t') + wxT("Shift+F1"), wxEmptyString, wxITEM_NORMAL );
    m_menuHelp->Append( m_menuItemAbout );

    m_menubar->Append( m_menuHelp, _("&Help") );

    this->SetMenuBar( m_menubar );

    bSizerPanelHolder = new wxBoxSizer( wxVERTICAL );

    m_panelTopButtons = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelTopButtons->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer1791;
    bSizer1791 = new wxBoxSizer( wxHORIZONTAL );

    bSizerTopButtons = new wxBoxSizer( wxHORIZONTAL );

    wxBoxSizer* bSizer261;
    bSizer261 = new wxBoxSizer( wxHORIZONTAL );


    bSizer261->Add( 0, 0, 1, 0, 5 );

    m_buttonCancel = new wxButton( m_panelTopButtons, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonCancel->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
    m_buttonCancel->Enable( false );
    m_buttonCancel->Hide();

    bSizer261->Add( m_buttonCancel, 0, wxEXPAND, 5 );

    m_buttonCompare = new zen::BitmapTextButton( m_panelTopButtons, wxID_ANY, _("Compare"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonCompare->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
    m_buttonCompare->SetToolTip( _("dummy") );

    bSizer261->Add( m_buttonCompare, 0, wxEXPAND, 5 );


    bSizerTopButtons->Add( bSizer261, 1, wxEXPAND, 5 );


    bSizerTopButtons->Add( 8, 8, 0, 0, 5 );

    wxBoxSizer* bSizer2942;
    bSizer2942 = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonCmpConfig = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonCmpConfig->SetToolTip( _("dummy") );

    bSizer2942->Add( m_bpButtonCmpConfig, 0, wxEXPAND, 5 );

    m_bpButtonCmpContext = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizer2942->Add( m_bpButtonCmpContext, 0, wxEXPAND, 5 );


    bSizer2942->Add( 0, 0, 1, 0, 5 );


    bSizer2942->Add( 8, 0, 0, 0, 5 );

    m_bpButtonFilter = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0|wxFULL_REPAINT_ON_RESIZE );
    m_bpButtonFilter->SetToolTip( _("dummy") );

    bSizer2942->Add( m_bpButtonFilter, 0, wxEXPAND, 5 );

    m_bpButtonFilterContext = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizer2942->Add( m_bpButtonFilterContext, 0, wxEXPAND, 5 );


    bSizer2942->Add( 8, 0, 0, 0, 5 );


    bSizer2942->Add( 0, 0, 1, 0, 5 );

    m_bpButtonSyncConfig = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonSyncConfig->SetToolTip( _("dummy") );

    bSizer2942->Add( m_bpButtonSyncConfig, 0, wxEXPAND, 5 );

    m_bpButtonSyncContext = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizer2942->Add( m_bpButtonSyncContext, 0, wxEXPAND, 5 );


    bSizerTopButtons->Add( bSizer2942, 1, wxEXPAND, 5 );


    bSizerTopButtons->Add( 8, 8, 0, 0, 5 );

    wxBoxSizer* bSizer262;
    bSizer262 = new wxBoxSizer( wxHORIZONTAL );

    m_buttonSync = new zen::BitmapTextButton( m_panelTopButtons, wxID_ANY, _("Synchronize"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonSync->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
    m_buttonSync->SetToolTip( _("dummy") );

    bSizer262->Add( m_buttonSync, 0, wxEXPAND, 5 );


    bSizerTopButtons->Add( bSizer262, 1, wxEXPAND, 5 );


    bSizer1791->Add( bSizerTopButtons, 1, wxALIGN_CENTER_VERTICAL, 5 );


    m_panelTopButtons->SetSizer( bSizer1791 );
    m_panelTopButtons->Layout();
    bSizer1791->Fit( m_panelTopButtons );
    bSizerPanelHolder->Add( m_panelTopButtons, 0, wxEXPAND, 5 );

    m_panelDirectoryPairs = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL|wxBORDER_STATIC );
    wxBoxSizer* bSizer1601;
    bSizer1601 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer91;
    bSizer91 = new wxBoxSizer( wxHORIZONTAL );

    m_panelTopLeft = new wxPanel( m_panelDirectoryPairs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelTopLeft->SetMinSize( wxSize( 1, -1 ) );

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

    m_bpButtonAddPair = new wxBitmapButton( m_panelTopLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonAddPair->SetToolTip( _("Add folder pair") );

    bSizer159->Add( m_bpButtonAddPair, 0, wxEXPAND, 5 );

    m_bpButtonRemovePair = new wxBitmapButton( m_panelTopLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
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

    m_bpButtonSelectAltFolderLeft = new wxBitmapButton( m_panelTopLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
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

    m_bpButtonSwapSides = new wxBitmapButton( m_panelTopCenter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonSwapSides->SetToolTip( _("dummy") );

    bSizer1771->Add( m_bpButtonSwapSides, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer160;
    bSizer160 = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonLocalCompCfg = new wxBitmapButton( m_panelTopCenter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonLocalCompCfg->SetToolTip( _("dummy") );

    bSizer160->Add( m_bpButtonLocalCompCfg, 0, wxEXPAND, 5 );

    m_bpButtonLocalFilter = new wxBitmapButton( m_panelTopCenter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonLocalFilter->SetToolTip( _("dummy") );

    bSizer160->Add( m_bpButtonLocalFilter, 0, wxEXPAND, 5 );

    m_bpButtonLocalSyncCfg = new wxBitmapButton( m_panelTopCenter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonLocalSyncCfg->SetToolTip( _("dummy") );

    bSizer160->Add( m_bpButtonLocalSyncCfg, 0, wxEXPAND, 5 );


    bSizer1771->Add( bSizer160, 1, wxALIGN_CENTER_HORIZONTAL, 5 );


    m_panelTopCenter->SetSizer( bSizer1771 );
    m_panelTopCenter->Layout();
    bSizer1771->Fit( m_panelTopCenter );
    bSizer91->Add( m_panelTopCenter, 0, wxRIGHT|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );

    m_panelTopRight = new wxPanel( m_panelDirectoryPairs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelTopRight->SetMinSize( wxSize( 1, -1 ) );

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

    m_bpButtonSelectAltFolderRight = new wxBitmapButton( m_panelTopRight, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonSelectAltFolderRight->SetToolTip( _("Access online storage") );

    bSizer179->Add( m_bpButtonSelectAltFolderRight, 0, wxEXPAND, 5 );


    bSizer183->Add( bSizer179, 0, wxEXPAND, 5 );


    m_panelTopRight->SetSizer( bSizer183 );
    m_panelTopRight->Layout();
    bSizer183->Fit( m_panelTopRight );
    bSizer91->Add( m_panelTopRight, 1, wxRIGHT|wxALIGN_BOTTOM, 5 );


    bSizer1601->Add( bSizer91, 0, wxEXPAND, 5 );

    m_scrolledWindowFolderPairs = new wxScrolledWindow( m_panelDirectoryPairs, wxID_ANY, wxDefaultPosition, wxSize( -1, -1 ), wxHSCROLL|wxVSCROLL );
    m_scrolledWindowFolderPairs->SetScrollRate( 5, 5 );
    m_scrolledWindowFolderPairs->SetMinSize( wxSize( -1, 0 ) );

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

    m_panelStatusBar = new wxPanel( m_panelCenter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL|wxBORDER_STATIC );
    wxBoxSizer* bSizer451;
    bSizer451 = new wxBoxSizer( wxHORIZONTAL );

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

    m_bpButtonHideSearch = new wxBitmapButton( m_panelSearch, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonHideSearch->SetToolTip( _("Close search bar") );

    bSizer1713->Add( m_bpButtonHideSearch, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText101 = new wxStaticText( m_panelSearch, wxID_ANY, _("Find:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText101->Wrap( -1 );
    bSizer1713->Add( m_staticText101, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_textCtrlSearchTxt = new wxTextCtrl( m_panelSearch, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxTE_PROCESS_ENTER|wxWANTS_CHARS );
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

    m_bitmapSyncResult = new wxStaticBitmap( m_panelLog, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer42->Add( m_bitmapSyncResult, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_staticTextSyncResult = new wxStaticText( m_panelLog, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextSyncResult->Wrap( -1 );
    m_staticTextSyncResult->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer42->Add( m_staticTextSyncResult, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 10 );


    bSizer42->Add( 10, 0, 0, 0, 5 );

    ffgSizer11 = new wxFlexGridSizer( 2, 0, 5, 5 );
    ffgSizer11->SetFlexibleDirection( wxBOTH );
    ffgSizer11->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_staticTextProcessed = new wxStaticText( m_panelLog, wxID_ANY, _("Processed:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextProcessed->Wrap( -1 );
    ffgSizer11->Add( m_staticTextProcessed, 0, wxALIGN_RIGHT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticTextRemaining = new wxStaticText( m_panelLog, wxID_ANY, _("Remaining:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextRemaining->Wrap( -1 );
    ffgSizer11->Add( m_staticTextRemaining, 0, wxALIGN_RIGHT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );


    bSizer42->Add( ffgSizer11, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 10 );

    m_panelItemStats = new wxPanel( m_panelLog, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
    m_panelItemStats->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer291;
    bSizer291 = new wxBoxSizer( wxVERTICAL );

    ffgSizer111 = new wxFlexGridSizer( 0, 2, 5, 5 );
    ffgSizer111->SetFlexibleDirection( wxBOTH );
    ffgSizer111->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    wxBoxSizer* bSizer293;
    bSizer293 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapItemStat = new wxStaticBitmap( m_panelItemStats, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer293->Add( m_bitmapItemStat, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticTextItemsProcessed = new wxStaticText( m_panelItemStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_staticTextItemsProcessed->Wrap( -1 );
    m_staticTextItemsProcessed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer293->Add( m_staticTextItemsProcessed, 0, wxALIGN_BOTTOM, 5 );


    ffgSizer111->Add( bSizer293, 0, wxEXPAND|wxALIGN_RIGHT, 5 );

    m_staticTextBytesProcessed = new wxStaticText( m_panelItemStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextBytesProcessed->Wrap( -1 );
    ffgSizer111->Add( m_staticTextBytesProcessed, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );

    m_staticTextItemsRemaining = new wxStaticText( m_panelItemStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_staticTextItemsRemaining->Wrap( -1 );
    m_staticTextItemsRemaining->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    ffgSizer111->Add( m_staticTextItemsRemaining, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );

    m_staticTextBytesRemaining = new wxStaticText( m_panelItemStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextBytesRemaining->Wrap( -1 );
    ffgSizer111->Add( m_staticTextBytesRemaining, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );


    bSizer291->Add( ffgSizer111, 0, wxALL, 5 );


    m_panelItemStats->SetSizer( bSizer291 );
    m_panelItemStats->Layout();
    bSizer291->Fit( m_panelItemStats );
    bSizer42->Add( m_panelItemStats, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 10 );

    m_panelTimeStats = new wxPanel( m_panelLog, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
    m_panelTimeStats->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer292;
    bSizer292 = new wxBoxSizer( wxVERTICAL );

    ffgSizer112 = new wxFlexGridSizer( 0, 1, 5, 5 );
    ffgSizer112->SetFlexibleDirection( wxBOTH );
    ffgSizer112->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    wxBoxSizer* bSizer294;
    bSizer294 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapTimeStat = new wxStaticBitmap( m_panelTimeStats, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer294->Add( m_bitmapTimeStat, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    m_staticTextTimeElapsed = new wxStaticText( m_panelTimeStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextTimeElapsed->Wrap( -1 );
    m_staticTextTimeElapsed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer294->Add( m_staticTextTimeElapsed, 0, wxALIGN_BOTTOM, 5 );


    ffgSizer112->Add( bSizer294, 0, wxEXPAND|wxALIGN_RIGHT, 5 );


    bSizer292->Add( ffgSizer112, 0, wxALL, 5 );


    m_panelTimeStats->SetSizer( bSizer292 );
    m_panelTimeStats->Layout();
    bSizer292->Fit( m_panelTimeStats );
    bSizer42->Add( m_panelTimeStats, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 10 );


    bSizerLog->Add( bSizer42, 0, wxLEFT, 5 );

    m_staticline70 = new wxStaticLine( m_panelLog, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizerLog->Add( m_staticline70, 0, wxEXPAND, 5 );


    m_panelLog->SetSizer( bSizerLog );
    m_panelLog->Layout();
    bSizerLog->Fit( m_panelLog );
    bSizerPanelHolder->Add( m_panelLog, 0, 0, 5 );

    m_panelConfig = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelConfig->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    bSizerConfig = new wxBoxSizer( wxVERTICAL );

    bSizerCfgHistoryButtons = new wxBoxSizer( wxHORIZONTAL );

    wxBoxSizer* bSizer17611;
    bSizer17611 = new wxBoxSizer( wxVERTICAL );

    m_bpButtonNew = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonNew->SetToolTip( _("dummy") );

    bSizer17611->Add( m_bpButtonNew, 0, wxEXPAND, 5 );

    m_staticText951 = new wxStaticText( m_panelConfig, wxID_ANY, _("New"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText951->Wrap( -1 );
    bSizer17611->Add( m_staticText951, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 2 );


    bSizerCfgHistoryButtons->Add( bSizer17611, 0, 0, 5 );

    wxBoxSizer* bSizer1761;
    bSizer1761 = new wxBoxSizer( wxVERTICAL );

    m_bpButtonOpen = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonOpen->SetToolTip( _("dummy") );

    bSizer1761->Add( m_bpButtonOpen, 0, wxEXPAND, 5 );

    m_staticText95 = new wxStaticText( m_panelConfig, wxID_ANY, _("Open..."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText95->Wrap( -1 );
    bSizer1761->Add( m_staticText95, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 2 );


    bSizerCfgHistoryButtons->Add( bSizer1761, 0, 0, 5 );

    wxBoxSizer* bSizer175;
    bSizer175 = new wxBoxSizer( wxVERTICAL );

    m_bpButtonSave = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonSave->SetToolTip( _("dummy") );

    bSizer175->Add( m_bpButtonSave, 0, wxEXPAND, 5 );

    m_staticText961 = new wxStaticText( m_panelConfig, wxID_ANY, _("Save"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText961->Wrap( -1 );
    bSizer175->Add( m_staticText961, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 2 );


    bSizerCfgHistoryButtons->Add( bSizer175, 0, 0, 5 );

    wxBoxSizer* bSizer174;
    bSizer174 = new wxBoxSizer( wxVERTICAL );

    bSizerSaveAs = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonSaveAs = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonSaveAs->SetToolTip( _("dummy") );

    bSizerSaveAs->Add( m_bpButtonSaveAs, 1, 0, 5 );

    m_bpButtonSaveAsBatch = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonSaveAsBatch->SetToolTip( _("dummy") );

    bSizerSaveAs->Add( m_bpButtonSaveAsBatch, 1, 0, 5 );


    bSizer174->Add( bSizerSaveAs, 0, wxEXPAND, 5 );

    m_staticText97 = new wxStaticText( m_panelConfig, wxID_ANY, _("Save as..."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText97->Wrap( -1 );
    bSizer174->Add( m_staticText97, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 2 );


    bSizerCfgHistoryButtons->Add( bSizer174, 0, 0, 5 );


    bSizerConfig->Add( bSizerCfgHistoryButtons, 0, wxALIGN_CENTER_HORIZONTAL, 5 );

    m_staticline81 = new wxStaticLine( m_panelConfig, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizerConfig->Add( m_staticline81, 0, wxEXPAND|wxTOP, 5 );


    bSizerConfig->Add( 10, 0, 0, 0, 5 );

    m_gridCfgHistory = new zen::Grid( m_panelConfig, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
    m_gridCfgHistory->SetScrollRate( 5, 5 );
    bSizerConfig->Add( m_gridCfgHistory, 1, wxEXPAND, 5 );


    m_panelConfig->SetSizer( bSizerConfig );
    m_panelConfig->Layout();
    bSizerConfig->Fit( m_panelConfig );
    bSizerPanelHolder->Add( m_panelConfig, 0, 0, 5 );

    m_panelViewFilter = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelViewFilter->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    bSizerViewFilter = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonToggleLog = new wxBitmapButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW|0 );
    bSizerViewFilter->Add( m_bpButtonToggleLog, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );


    bSizerViewFilter->Add( 0, 0, 1, wxEXPAND, 5 );

    bSizerViewButtons = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonViewType = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonViewType, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );


    bSizerViewButtons->Add( 10, 10, 0, 0, 5 );

    m_bpButtonShowExcluded = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowExcluded, 0, wxEXPAND, 5 );


    bSizerViewButtons->Add( 10, 10, 0, 0, 5 );

    m_bpButtonShowDeleteLeft = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowDeleteLeft, 0, wxEXPAND, 5 );

    m_bpButtonShowUpdateLeft = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowUpdateLeft, 0, wxEXPAND, 5 );

    m_bpButtonShowCreateLeft = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowCreateLeft, 0, wxEXPAND, 5 );

    m_bpButtonShowLeftOnly = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowLeftOnly, 0, wxEXPAND, 5 );

    m_bpButtonShowLeftNewer = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowLeftNewer, 0, wxEXPAND, 5 );

    m_bpButtonShowEqual = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowEqual, 0, wxEXPAND, 5 );

    m_bpButtonShowDoNothing = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowDoNothing, 0, wxEXPAND, 5 );

    m_bpButtonShowDifferent = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowDifferent, 0, wxEXPAND, 5 );

    m_bpButtonShowRightNewer = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowRightNewer, 0, wxEXPAND, 5 );

    m_bpButtonShowRightOnly = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowRightOnly, 0, wxEXPAND, 5 );

    m_bpButtonShowCreateRight = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowCreateRight, 0, wxEXPAND, 5 );

    m_bpButtonShowUpdateRight = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowUpdateRight, 0, wxEXPAND, 5 );

    m_bpButtonShowDeleteRight = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowDeleteRight, 0, wxEXPAND, 5 );

    m_bpButtonShowConflict = new zen::ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonShowConflict, 0, wxEXPAND, 5 );

    m_bpButtonViewFilterContext = new wxBitmapButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizerViewButtons->Add( m_bpButtonViewFilterContext, 0, wxEXPAND, 5 );


    bSizerViewFilter->Add( bSizerViewButtons, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );


    bSizerViewFilter->Add( 0, 0, 1, wxEXPAND, 5 );

    m_staticText96 = new wxStaticText( m_panelViewFilter, wxID_ANY, _("Statistics:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText96->Wrap( -1 );
    bSizerViewFilter->Add( m_staticText96, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );

    m_panelStatistics = new wxPanel( m_panelViewFilter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL|wxBORDER_SUNKEN );
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
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( MainDialogGenerated::onClose ) );
    m_menuFile->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onConfigNew ), this, m_menuItemNew->GetId());
    m_menuFile->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onConfigLoad ), this, m_menuItemLoad->GetId());
    m_menuFile->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onConfigSave ), this, m_menuItemSave->GetId());
    m_menuFile->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onConfigSaveAs ), this, m_menuItemSaveAs->GetId());
    m_menuFile->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onSaveAsBatchJob ), this, m_menuItemSaveAsBatch->GetId());
    m_menuFile->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onMenuQuit ), this, m_menuItemQuit->GetId());
    m_menuActions->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onToggleLog ), this, m_menuItemShowLog->GetId());
    m_menuActions->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onCompare ), this, m_menuItemCompare->GetId());
    m_menuActions->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onCmpSettings ), this, m_menuItemCompSettings->GetId());
    m_menuActions->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onConfigureFilter ), this, m_menuItemFilter->GetId());
    m_menuActions->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onSyncSettings ), this, m_menuItemSyncSettings->GetId());
    m_menuActions->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onStartSync ), this, m_menuItemSynchronize->GetId());
    m_menuTools->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onMenuOptions ), this, m_menuItemOptions->GetId());
    m_menuTools->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onMenuFindItem ), this, m_menuItemFind->GetId());
    m_menuTools->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onMenuExportFileList ), this, m_menuItemExportList->GetId());
    m_menuTools->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onMenuResetLayout ), this, m_menuItemResetLayout->GetId());
    m_menuHelp->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onShowHelp ), this, m_menuItemHelp->GetId());
    m_menuHelp->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onMenuCheckVersion ), this, m_menuItemCheckVersionNow->GetId());
    m_menuHelp->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::onMenuAbout ), this, m_menuItemAbout->GetId());
    m_buttonCompare->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onCompare ), NULL, this );
    m_bpButtonCmpConfig->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onCmpSettings ), NULL, this );
    m_bpButtonCmpConfig->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onCompSettingsContextMouse ), NULL, this );
    m_bpButtonCmpContext->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onCompSettingsContext ), NULL, this );
    m_bpButtonCmpContext->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onCompSettingsContextMouse ), NULL, this );
    m_bpButtonFilter->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onConfigureFilter ), NULL, this );
    m_bpButtonFilter->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onGlobalFilterContextMouse ), NULL, this );
    m_bpButtonFilterContext->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onGlobalFilterContext ), NULL, this );
    m_bpButtonFilterContext->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onGlobalFilterContextMouse ), NULL, this );
    m_bpButtonSyncConfig->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onSyncSettings ), NULL, this );
    m_bpButtonSyncConfig->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onSyncSettingsContextMouse ), NULL, this );
    m_bpButtonSyncContext->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onSyncSettingsContext ), NULL, this );
    m_bpButtonSyncContext->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onSyncSettingsContextMouse ), NULL, this );
    m_buttonSync->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onStartSync ), NULL, this );
    m_bpButtonAddPair->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onTopFolderPairAdd ), NULL, this );
    m_bpButtonRemovePair->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onTopFolderPairRemove ), NULL, this );
    m_bpButtonSwapSides->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onSwapSides ), NULL, this );
    m_bpButtonLocalCompCfg->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onTopLocalCompCfg ), NULL, this );
    m_bpButtonLocalFilter->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onTopLocalFilterCfg ), NULL, this );
    m_bpButtonLocalSyncCfg->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onTopLocalSyncCfg ), NULL, this );
    m_bpButtonHideSearch->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onHideSearchPanel ), NULL, this );
    m_textCtrlSearchTxt->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainDialogGenerated::onSearchGridEnter ), NULL, this );
    m_bpButtonNew->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onConfigNew ), NULL, this );
    m_bpButtonOpen->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onConfigLoad ), NULL, this );
    m_bpButtonSave->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onConfigSave ), NULL, this );
    m_bpButtonSaveAs->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onConfigSaveAs ), NULL, this );
    m_bpButtonSaveAsBatch->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onSaveAsBatchJob ), NULL, this );
    m_bpButtonToggleLog->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleLog ), NULL, this );
    m_bpButtonViewType->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewType ), NULL, this );
    m_bpButtonViewType->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewTypeContextMouse ), NULL, this );
    m_bpButtonShowExcluded->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowExcluded->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowDeleteLeft->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowDeleteLeft->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowUpdateLeft->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowUpdateLeft->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowCreateLeft->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowCreateLeft->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowLeftOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowLeftOnly->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowLeftNewer->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowLeftNewer->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowEqual->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowEqual->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowDoNothing->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowDoNothing->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowDifferent->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowDifferent->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowRightNewer->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowRightNewer->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowRightOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowRightOnly->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowCreateRight->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowCreateRight->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowUpdateRight->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowUpdateRight->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowDeleteRight->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowDeleteRight->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonShowConflict->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onToggleViewButton ), NULL, this );
    m_bpButtonShowConflict->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
    m_bpButtonViewFilterContext->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::onViewFilterContext ), NULL, this );
    m_bpButtonViewFilterContext->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::onViewFilterContextMouse ), NULL, this );
}

MainDialogGenerated::~MainDialogGenerated()
{
}

FolderPairPanelGenerated::FolderPairPanelGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style, const wxString& name ) : wxPanel( parent, id, pos, size, style, name )
{
    wxBoxSizer* bSizer74;
    bSizer74 = new wxBoxSizer( wxHORIZONTAL );

    m_panelLeft = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelLeft->SetMinSize( wxSize( 1, -1 ) );

    wxBoxSizer* bSizer134;
    bSizer134 = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonFolderPairOptions = new wxBitmapButton( m_panelLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonFolderPairOptions->SetToolTip( _("Arrange folder pair") );

    bSizer134->Add( m_bpButtonFolderPairOptions, 0, wxEXPAND, 5 );

    m_bpButtonRemovePair = new wxBitmapButton( m_panelLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonRemovePair->SetToolTip( _("Remove folder pair") );

    bSizer134->Add( m_bpButtonRemovePair, 0, wxEXPAND, 5 );

    m_folderPathLeft = new fff::FolderHistoryBox( m_panelLeft, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 );
    bSizer134->Add( m_folderPathLeft, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonSelectFolderLeft = new wxButton( m_panelLeft, wxID_ANY, _("Browse"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonSelectFolderLeft->SetToolTip( _("Select a folder") );

    bSizer134->Add( m_buttonSelectFolderLeft, 0, wxEXPAND, 5 );

    m_bpButtonSelectAltFolderLeft = new wxBitmapButton( m_panelLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonSelectAltFolderLeft->SetToolTip( _("Access online storage") );

    bSizer134->Add( m_bpButtonSelectAltFolderLeft, 0, wxEXPAND, 5 );


    m_panelLeft->SetSizer( bSizer134 );
    m_panelLeft->Layout();
    bSizer134->Fit( m_panelLeft );
    bSizer74->Add( m_panelLeft, 0, wxLEFT|wxEXPAND, 5 );

    m_panel20 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    wxBoxSizer* bSizer95;
    bSizer95 = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonLocalCompCfg = new wxBitmapButton( m_panel20, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonLocalCompCfg->SetToolTip( _("dummy") );

    bSizer95->Add( m_bpButtonLocalCompCfg, 0, wxEXPAND, 5 );

    m_bpButtonLocalFilter = new wxBitmapButton( m_panel20, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonLocalFilter->SetToolTip( _("dummy") );

    bSizer95->Add( m_bpButtonLocalFilter, 0, wxEXPAND, 5 );

    m_bpButtonLocalSyncCfg = new wxBitmapButton( m_panel20, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonLocalSyncCfg->SetToolTip( _("dummy") );

    bSizer95->Add( m_bpButtonLocalSyncCfg, 0, wxEXPAND, 5 );


    m_panel20->SetSizer( bSizer95 );
    m_panel20->Layout();
    bSizer95->Fit( m_panel20 );
    bSizer74->Add( m_panel20, 0, wxRIGHT|wxLEFT|wxEXPAND, 5 );

    m_panelRight = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelRight->SetMinSize( wxSize( 1, -1 ) );

    wxBoxSizer* bSizer135;
    bSizer135 = new wxBoxSizer( wxHORIZONTAL );

    m_folderPathRight = new fff::FolderHistoryBox( m_panelRight, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 );
    bSizer135->Add( m_folderPathRight, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonSelectFolderRight = new wxButton( m_panelRight, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
    m_buttonSelectFolderRight->SetToolTip( _("Select a folder") );

    bSizer135->Add( m_buttonSelectFolderRight, 0, wxEXPAND, 5 );

    m_bpButtonSelectAltFolderRight = new wxBitmapButton( m_panelRight, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonSelectAltFolderRight->SetToolTip( _("Access online storage") );

    bSizer135->Add( m_bpButtonSelectAltFolderRight, 0, wxEXPAND, 5 );


    m_panelRight->SetSizer( bSizer135 );
    m_panelRight->Layout();
    bSizer135->Fit( m_panelRight );
    bSizer74->Add( m_panelRight, 1, wxRIGHT|wxEXPAND, 5 );


    this->SetSizer( bSizer74 );
    this->Layout();
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


    bSizer190->Add( bSizer1911, 0, wxEXPAND, 5 );

    m_notebook = new wxNotebook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_NOPAGETHEME );
    m_panelCompSettingsTab = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelCompSettingsTab->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer275;
    bSizer275 = new wxBoxSizer( wxVERTICAL );

    bSizerHeaderCompSettings = new wxBoxSizer( wxVERTICAL );

    m_staticTextMainCompSettings = new wxStaticText( m_panelCompSettingsTab, wxID_ANY, _("Common settings:"), wxDefaultPosition, wxDefaultSize, 0 );
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

    wxGridSizer* gSizer2;
    gSizer2 = new wxGridSizer( 0, 1, 0, 0 );

    m_buttonByTimeSize = new zen::ToggleButton( m_panelComparisonSettings, wxID_ANY, _("File time and size"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonByTimeSize->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
    m_buttonByTimeSize->SetToolTip( _("dummy") );

    gSizer2->Add( m_buttonByTimeSize, 0, wxEXPAND, 5 );

    m_buttonByContent = new zen::ToggleButton( m_panelComparisonSettings, wxID_ANY, _("File content"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonByContent->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
    m_buttonByContent->SetToolTip( _("dummy") );

    gSizer2->Add( m_buttonByContent, 0, wxEXPAND, 5 );

    m_buttonBySize = new zen::ToggleButton( m_panelComparisonSettings, wxID_ANY, _("File size"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonBySize->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
    m_buttonBySize->SetToolTip( _("dummy") );

    gSizer2->Add( m_buttonBySize, 0, wxEXPAND, 5 );


    bSizer182->Add( gSizer2, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );


    bSizer178->Add( bSizer182, 0, wxALL, 5 );

    wxBoxSizer* bSizer2371;
    bSizer2371 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapCompVariant = new wxStaticBitmap( m_panelComparisonSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
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


    bSizer1734->Add( 0, 0, 1, 0, 5 );

    wxBoxSizer* bSizer1721;
    bSizer1721 = new wxBoxSizer( wxVERTICAL );

    m_checkBoxSymlinksInclude = new wxCheckBox( m_panelComparisonSettings, wxID_ANY, _("Include &symbolic links:"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer1721->Add( m_checkBoxSymlinksInclude, 0, wxALL, 5 );

    wxBoxSizer* bSizer176;
    bSizer176 = new wxBoxSizer( wxVERTICAL );

    m_radioBtnSymlinksFollow = new wxRadioButton( m_panelComparisonSettings, wxID_ANY, _("&Follow"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
    m_radioBtnSymlinksFollow->SetValue( true );
    bSizer176->Add( m_radioBtnSymlinksFollow, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );

    m_radioBtnSymlinksDirect = new wxRadioButton( m_panelComparisonSettings, wxID_ANY, _("As &link"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer176->Add( m_radioBtnSymlinksDirect, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );


    bSizer1721->Add( bSizer176, 0, wxLEFT|wxEXPAND, 15 );


    bSizer1721->Add( 0, 0, 1, wxEXPAND, 5 );

    m_hyperlink24 = new wxHyperlinkCtrl( m_panelComparisonSettings, wxID_ANY, _("More information"), wxT("https://freefilesync.org/manual.php?topic=comparison-settings"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlink24->SetToolTip( _("https://freefilesync.org/manual.php?topic=comparison-settings") );

    bSizer1721->Add( m_hyperlink24, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );


    bSizer1734->Add( bSizer1721, 0, wxALL|wxEXPAND, 5 );


    bSizer1734->Add( 0, 0, 1, 0, 5 );

    m_staticline44 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
    bSizer1734->Add( m_staticline44, 0, wxEXPAND, 5 );


    bSizer1734->Add( 0, 0, 1, 0, 5 );

    wxBoxSizer* bSizer1733;
    bSizer1733 = new wxBoxSizer( wxVERTICAL );

    m_staticText112 = new wxStaticText( m_panelComparisonSettings, wxID_ANY, _("&Ignore exact time shift [hh:mm]"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText112->Wrap( -1 );
    bSizer1733->Add( m_staticText112, 0, wxALL, 5 );

    m_textCtrlTimeShift = new wxTextCtrl( m_panelComparisonSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    m_textCtrlTimeShift->SetToolTip( _("List of file time offsets to ignore") );

    bSizer1733->Add( m_textCtrlTimeShift, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxEXPAND, 5 );

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

    m_hyperlink241 = new wxHyperlinkCtrl( m_panelComparisonSettings, wxID_ANY, _("Handle daylight saving time"), wxT("https://freefilesync.org/manual.php?topic=daylight-saving-time"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlink241->SetToolTip( _("https://freefilesync.org/manual.php?topic=daylight-saving-time") );

    bSizer1733->Add( m_hyperlink241, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );


    bSizer1734->Add( bSizer1733, 0, wxALL|wxEXPAND, 5 );


    bSizer1734->Add( 0, 0, 1, 0, 5 );


    bSizer159->Add( bSizer1734, 0, wxEXPAND, 5 );

    m_staticline331 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer159->Add( m_staticline331, 0, wxEXPAND, 5 );


    bSizer159->Add( 0, 0, 1, 0, 5 );

    bSizerCompMisc = new wxBoxSizer( wxVERTICAL );

    m_staticline3311 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizerCompMisc->Add( m_staticline3311, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer2781;
    bSizer2781 = new wxBoxSizer( wxHORIZONTAL );

    wxFlexGridSizer* fgSizer61;
    fgSizer61 = new wxFlexGridSizer( 0, 2, 5, 5 );
    fgSizer61->SetFlexibleDirection( wxBOTH );
    fgSizer61->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_bitmapIgnoreErrors = new wxStaticBitmap( m_panelComparisonSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    fgSizer61->Add( m_bitmapIgnoreErrors, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_checkBoxIgnoreErrors = new wxCheckBox( m_panelComparisonSettings, wxID_ANY, _("Ignore errors"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
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

    m_spinCtrlAutoRetryCount = new wxSpinCtrl( m_panelComparisonSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
    fgSizerAutoRetry->Add( m_spinCtrlAutoRetryCount, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_spinCtrlAutoRetryDelay = new wxSpinCtrl( m_panelComparisonSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxSP_ARROW_KEYS, 0, 2000000000, 0 );
    fgSizerAutoRetry->Add( m_spinCtrlAutoRetryDelay, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer2781->Add( fgSizerAutoRetry, 0, wxTOP|wxBOTTOM|wxRIGHT|wxALIGN_CENTER_VERTICAL, 10 );


    bSizerCompMisc->Add( bSizer2781, 0, wxEXPAND, 5 );


    bSizer159->Add( bSizerCompMisc, 0, wxEXPAND, 5 );


    bSizer2561->Add( bSizer159, 0, wxEXPAND, 5 );

    m_staticline751 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
    bSizer2561->Add( m_staticline751, 0, wxEXPAND, 5 );

    bSizerPerformance = new wxBoxSizer( wxVERTICAL );

    m_panel57 = new wxPanel( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panel57->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer2191;
    bSizer2191 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapPerf = new wxStaticBitmap( m_panel57, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer2191->Add( m_bitmapPerf, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_staticText13611 = new wxStaticText( m_panel57, wxID_ANY, _("Performance improvements:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText13611->Wrap( -1 );
    bSizer2191->Add( m_staticText13611, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 10 );


    m_panel57->SetSizer( bSizer2191 );
    m_panel57->Layout();
    bSizer2191->Fit( m_panel57 );
    bSizerPerformance->Add( m_panel57, 0, wxEXPAND, 5 );

    wxStaticLine* m_staticline75;
    m_staticline75 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizerPerformance->Add( m_staticline75, 0, wxEXPAND, 5 );

    m_hyperlinkPerfDeRequired = new wxHyperlinkCtrl( m_panelComparisonSettings, wxID_ANY, _("Requires FreeFileSync Donation Edition"), wxT("https://freefilesync.org/faq.php#donation-edition"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlinkPerfDeRequired->SetToolTip( _("https://freefilesync.org/faq.php#donation-edition") );

    bSizerPerformance->Add( m_hyperlinkPerfDeRequired, 0, wxALL, 10 );

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


    bSizerPerformance->Add( bSizer260, 1, wxALL|wxEXPAND, 5 );

    m_hyperlink1711 = new wxHyperlinkCtrl( m_panelComparisonSettings, wxID_ANY, _("How to get the best performance?"), wxT("https://freefilesync.org/manual.php?topic=performance"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlink1711->SetToolTip( _("https://freefilesync.org/manual.php?topic=performance") );

    bSizerPerformance->Add( m_hyperlink1711, 0, wxBOTTOM|wxRIGHT|wxLEFT, 10 );


    bSizer2561->Add( bSizerPerformance, 1, wxEXPAND, 5 );


    m_panelComparisonSettings->SetSizer( bSizer2561 );
    m_panelComparisonSettings->Layout();
    bSizer2561->Fit( m_panelComparisonSettings );
    bSizer275->Add( m_panelComparisonSettings, 1, wxEXPAND, 5 );


    m_panelCompSettingsTab->SetSizer( bSizer275 );
    m_panelCompSettingsTab->Layout();
    bSizer275->Fit( m_panelCompSettingsTab );
    m_notebook->AddPage( m_panelCompSettingsTab, _("dummy"), true );
    m_panelFilterSettingsTab = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelFilterSettingsTab->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer278;
    bSizer278 = new wxBoxSizer( wxVERTICAL );

    bSizerHeaderFilterSettings = new wxBoxSizer( wxVERTICAL );

    m_staticTextMainFilterSettings = new wxStaticText( m_panelFilterSettingsTab, wxID_ANY, _("Common settings:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextMainFilterSettings->Wrap( -1 );
    bSizerHeaderFilterSettings->Add( m_staticTextMainFilterSettings, 0, wxALL, 10 );

    m_staticTextLocalFilterSettings = new wxStaticText( m_panelFilterSettingsTab, wxID_ANY, _("Local settings:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextLocalFilterSettings->Wrap( -1 );
    bSizerHeaderFilterSettings->Add( m_staticTextLocalFilterSettings, 0, wxALL, 10 );

    m_staticlineFilterHeader = new wxStaticLine( m_panelFilterSettingsTab, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizerHeaderFilterSettings->Add( m_staticlineFilterHeader, 0, wxEXPAND, 5 );


    bSizer278->Add( bSizerHeaderFilterSettings, 0, wxEXPAND, 5 );

    m_panel571 = new wxPanel( m_panelFilterSettingsTab, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panel571->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer307;
    bSizer307 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer301;
    bSizer301 = new wxBoxSizer( wxHORIZONTAL );

    wxBoxSizer* bSizer166;
    bSizer166 = new wxBoxSizer( wxVERTICAL );


    bSizer166->Add( 0, 10, 0, 0, 5 );

    wxBoxSizer* bSizer1661;
    bSizer1661 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapInclude = new wxStaticBitmap( m_panel571, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer1661->Add( m_bitmapInclude, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    wxBoxSizer* bSizer1731;
    bSizer1731 = new wxBoxSizer( wxVERTICAL );

    m_staticText78 = new wxStaticText( m_panel571, wxID_ANY, _("Include:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText78->Wrap( -1 );
    bSizer1731->Add( m_staticText78, 0, 0, 5 );

    m_textCtrlInclude = new wxTextCtrl( m_panel571, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxTE_MULTILINE );
    bSizer1731->Add( m_textCtrlInclude, 1, wxEXPAND|wxTOP, 5 );


    bSizer1661->Add( bSizer1731, 1, wxEXPAND, 5 );


    bSizer166->Add( bSizer1661, 3, wxEXPAND|wxLEFT, 5 );


    bSizer166->Add( 0, 10, 0, 0, 5 );

    wxBoxSizer* bSizer1651;
    bSizer1651 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapExclude = new wxStaticBitmap( m_panel571, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer1651->Add( m_bitmapExclude, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxBoxSizer* bSizer1742;
    bSizer1742 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer189;
    bSizer189 = new wxBoxSizer( wxHORIZONTAL );

    m_staticText77 = new wxStaticText( m_panel571, wxID_ANY, _("Exclude:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText77->Wrap( -1 );
    bSizer189->Add( m_staticText77, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer189->Add( 0, 0, 1, wxEXPAND, 5 );

    m_hyperlink171 = new wxHyperlinkCtrl( m_panel571, wxID_ANY, _("Show examples"), wxT("https://freefilesync.org/manual.php?topic=exclude-files"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlink171->SetToolTip( _("https://freefilesync.org/manual.php?topic=exclude-files") );

    bSizer189->Add( m_hyperlink171, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );


    bSizer1742->Add( bSizer189, 0, wxEXPAND, 5 );

    m_textCtrlExclude = new wxTextCtrl( m_panel571, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxTE_MULTILINE );
    bSizer1742->Add( m_textCtrlExclude, 1, wxEXPAND|wxTOP, 5 );


    bSizer1651->Add( bSizer1742, 1, wxEXPAND, 5 );


    bSizer166->Add( bSizer1651, 5, wxEXPAND|wxLEFT, 5 );


    bSizer301->Add( bSizer166, 1, wxEXPAND, 5 );

    m_staticline24 = new wxStaticLine( m_panel571, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
    bSizer301->Add( m_staticline24, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer160;
    bSizer160 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer168;
    bSizer168 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapFilterSize = new wxStaticBitmap( m_panel571, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer168->Add( m_bitmapFilterSize, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    wxBoxSizer* bSizer158;
    bSizer158 = new wxBoxSizer( wxVERTICAL );

    m_staticText80 = new wxStaticText( m_panel571, wxID_ANY, _("File size:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText80->Wrap( -1 );
    bSizer158->Add( m_staticText80, 0, wxBOTTOM, 5 );

    wxBoxSizer* bSizer162;
    bSizer162 = new wxBoxSizer( wxVERTICAL );

    m_staticText101 = new wxStaticText( m_panel571, wxID_ANY, _("Minimum:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText101->Wrap( -1 );
    bSizer162->Add( m_staticText101, 0, wxBOTTOM, 2 );

    m_spinCtrlMinSize = new wxSpinCtrl( m_panel571, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 2000000000, 0 );
    bSizer162->Add( m_spinCtrlMinSize, 0, wxEXPAND, 5 );

    wxArrayString m_choiceUnitMinSizeChoices;
    m_choiceUnitMinSize = new wxChoice( m_panel571, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceUnitMinSizeChoices, 0 );
    m_choiceUnitMinSize->SetSelection( 0 );
    bSizer162->Add( m_choiceUnitMinSize, 0, wxEXPAND, 5 );


    bSizer158->Add( bSizer162, 0, wxEXPAND, 5 );


    bSizer158->Add( 0, 10, 0, 0, 5 );

    wxBoxSizer* bSizer163;
    bSizer163 = new wxBoxSizer( wxVERTICAL );

    m_staticText102 = new wxStaticText( m_panel571, wxID_ANY, _("Maximum:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText102->Wrap( -1 );
    bSizer163->Add( m_staticText102, 0, wxBOTTOM, 2 );

    m_spinCtrlMaxSize = new wxSpinCtrl( m_panel571, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 2000000000, 0 );
    bSizer163->Add( m_spinCtrlMaxSize, 0, wxEXPAND, 5 );

    wxArrayString m_choiceUnitMaxSizeChoices;
    m_choiceUnitMaxSize = new wxChoice( m_panel571, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceUnitMaxSizeChoices, 0 );
    m_choiceUnitMaxSize->SetSelection( 0 );
    bSizer163->Add( m_choiceUnitMaxSize, 0, wxEXPAND, 5 );


    bSizer158->Add( bSizer163, 0, wxEXPAND, 5 );


    bSizer168->Add( bSizer158, 1, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer160->Add( bSizer168, 2, wxEXPAND|wxALL, 5 );

    m_staticline23 = new wxStaticLine( m_panel571, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer160->Add( m_staticline23, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer167;
    bSizer167 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapFilterDate = new wxStaticBitmap( m_panel571, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer167->Add( m_bitmapFilterDate, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxBoxSizer* bSizer165;
    bSizer165 = new wxBoxSizer( wxVERTICAL );

    m_staticText79 = new wxStaticText( m_panel571, wxID_ANY, _("Time span:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText79->Wrap( -1 );
    bSizer165->Add( m_staticText79, 0, wxBOTTOM, 5 );

    wxArrayString m_choiceUnitTimespanChoices;
    m_choiceUnitTimespan = new wxChoice( m_panel571, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceUnitTimespanChoices, 0 );
    m_choiceUnitTimespan->SetSelection( 0 );
    bSizer165->Add( m_choiceUnitTimespan, 0, wxEXPAND, 5 );

    m_spinCtrlTimespan = new wxSpinCtrl( m_panel571, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 2000000000, 0 );
    bSizer165->Add( m_spinCtrlTimespan, 0, wxEXPAND, 5 );


    bSizer167->Add( bSizer165, 1, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer160->Add( bSizer167, 1, wxEXPAND|wxALL, 5 );

    m_staticline231 = new wxStaticLine( m_panel571, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer160->Add( m_staticline231, 0, wxEXPAND, 5 );


    bSizer301->Add( bSizer160, 0, wxEXPAND, 5 );


    bSizer307->Add( bSizer301, 1, wxEXPAND, 5 );

    wxBoxSizer* bSizer302;
    bSizer302 = new wxBoxSizer( wxHORIZONTAL );

    m_staticTextFilterDescr = new wxStaticText( m_panel571, wxID_ANY, _("Select filter rules to exclude certain files from synchronization. Enter file paths relative to their corresponding folder pair."), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_staticTextFilterDescr->Wrap( -1 );
    m_staticTextFilterDescr->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    bSizer302->Add( m_staticTextFilterDescr, 1, wxALL|wxALIGN_CENTER_VERTICAL, 10 );

    wxBoxSizer* bSizer303;
    bSizer303 = new wxBoxSizer( wxHORIZONTAL );

    m_buttonDefault = new wxButton( m_panel571, wxID_ANY, _("&Default"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer303->Add( m_buttonDefault, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_bpButtonDefaultContext = new wxBitmapButton( m_panel571, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizer303->Add( m_bpButtonDefaultContext, 0, wxEXPAND, 5 );


    bSizer302->Add( bSizer303, 0, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 10 );

    m_buttonClear = new wxButton( m_panel571, wxID_ANY, _("C&lear"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer302->Add( m_buttonClear, 0, wxALL|wxALIGN_CENTER_VERTICAL, 10 );


    bSizer307->Add( bSizer302, 0, wxEXPAND, 5 );


    m_panel571->SetSizer( bSizer307 );
    m_panel571->Layout();
    bSizer307->Fit( m_panel571 );
    bSizer278->Add( m_panel571, 1, wxEXPAND, 5 );


    m_panelFilterSettingsTab->SetSizer( bSizer278 );
    m_panelFilterSettingsTab->Layout();
    bSizer278->Fit( m_panelFilterSettingsTab );
    m_notebook->AddPage( m_panelFilterSettingsTab, _("dummy"), false );
    m_panelSyncSettingsTab = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelSyncSettingsTab->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer276;
    bSizer276 = new wxBoxSizer( wxVERTICAL );

    bSizerHeaderSyncSettings = new wxBoxSizer( wxVERTICAL );

    m_staticTextMainSyncSettings = new wxStaticText( m_panelSyncSettingsTab, wxID_ANY, _("Common settings:"), wxDefaultPosition, wxDefaultSize, 0 );
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

    wxGridSizer* gSizer1;
    gSizer1 = new wxGridSizer( 0, 1, 0, 0 );

    m_buttonTwoWay = new zen::ToggleButton( m_panelSyncSettings, wxID_ANY, _("Two way"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonTwoWay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
    m_buttonTwoWay->SetToolTip( _("dummy") );

    gSizer1->Add( m_buttonTwoWay, 0, wxEXPAND, 5 );

    m_buttonMirror = new zen::ToggleButton( m_panelSyncSettings, wxID_ANY, _("Mirror"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonMirror->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
    m_buttonMirror->SetToolTip( _("dummy") );

    gSizer1->Add( m_buttonMirror, 0, wxEXPAND, 5 );

    m_buttonUpdate = new zen::ToggleButton( m_panelSyncSettings, wxID_ANY, _("Update"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonUpdate->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
    m_buttonUpdate->SetToolTip( _("dummy") );

    gSizer1->Add( m_buttonUpdate, 0, wxEXPAND, 5 );

    m_buttonCustom = new zen::ToggleButton( m_panelSyncSettings, wxID_ANY, _("Custom"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonCustom->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
    m_buttonCustom->SetToolTip( _("dummy") );

    gSizer1->Add( m_buttonCustom, 0, wxEXPAND, 5 );


    bSizer235->Add( gSizer1, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );


    bSizer237->Add( bSizer235, 0, wxALL, 5 );


    bSizer237->Add( 10, 0, 0, 0, 5 );

    wxBoxSizer* bSizer312;
    bSizer312 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer311;
    bSizer311 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapDatabase = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_bitmapDatabase->SetToolTip( _("sync.ffs_db") );

    bSizer311->Add( m_bitmapDatabase, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

    m_checkBoxUseDatabase = new wxCheckBox( m_panelSyncSettings, wxID_ANY, _("Use database file to detect changes"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer311->Add( m_checkBoxUseDatabase, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer312->Add( bSizer311, 0, wxTOP|wxRIGHT|wxLEFT, 10 );


    bSizer312->Add( 0, 0, 1, wxEXPAND, 5 );

    m_staticTextSyncVarDescription = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_staticTextSyncVarDescription->Wrap( -1 );
    m_staticTextSyncVarDescription->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    bSizer312->Add( m_staticTextSyncVarDescription, 0, wxALL, 10 );


    bSizer312->Add( 0, 0, 1, wxEXPAND, 5 );

    wxBoxSizer* bSizer310;
    bSizer310 = new wxBoxSizer( wxVERTICAL );

    m_staticline431 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer310->Add( m_staticline431, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer201;
    bSizer201 = new wxBoxSizer( wxHORIZONTAL );

    m_staticline72 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
    bSizer201->Add( m_staticline72, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer3121;
    bSizer3121 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapMoveLeft = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_bitmapMoveLeft->SetToolTip( _("- Not supported by all file systems\n- Requires and creates database files\n- Detection not available for first sync") );

    bSizer3121->Add( m_bitmapMoveLeft, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_bitmapMoveRight = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_bitmapMoveRight->SetToolTip( _("- Not supported by all file systems\n- Requires and creates database files\n- Detection not available for first sync") );

    bSizer3121->Add( m_bitmapMoveRight, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    m_staticTextDetectMove = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Detect moved files"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextDetectMove->Wrap( -1 );
    m_staticTextDetectMove->SetToolTip( _("- Not supported by all file systems\n- Requires and creates database files\n- Detection not available for first sync") );

    bSizer3121->Add( m_staticTextDetectMove, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer201->Add( bSizer3121, 0, wxALL, 10 );

    m_hyperlink242 = new wxHyperlinkCtrl( m_panelSyncSettings, wxID_ANY, _("More information"), wxT("https://freefilesync.org/manual.php?topic=synchronization-settings"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlink242->SetToolTip( _("https://freefilesync.org/manual.php?topic=synchronization-settings") );

    bSizer201->Add( m_hyperlink242, 0, wxTOP|wxBOTTOM|wxRIGHT|wxALIGN_CENTER_VERTICAL, 10 );

    m_staticline721 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
    bSizer201->Add( m_staticline721, 0, wxEXPAND, 5 );


    bSizer310->Add( bSizer201, 0, 0, 5 );


    bSizer312->Add( bSizer310, 0, 0, 5 );


    bSizer237->Add( bSizer312, 0, wxEXPAND, 5 );

    bSizerSyncDirHolder = new wxBoxSizer( wxHORIZONTAL );

    bSizerSyncDirsDiff = new wxBoxSizer( wxVERTICAL );

    m_staticText184 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Difference"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText184->Wrap( -1 );
    bSizerSyncDirsDiff->Add( m_staticText184, 0, wxALIGN_CENTER_HORIZONTAL, 5 );

    ffgSizer11 = new wxFlexGridSizer( 2, 0, 5, 5 );
    ffgSizer11->SetFlexibleDirection( wxBOTH );
    ffgSizer11->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_bitmapLeftOnly = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_bitmapLeftOnly->SetToolTip( _("Item exists on left side only") );

    ffgSizer11->Add( m_bitmapLeftOnly, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_bitmapLeftNewer = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_bitmapLeftNewer->SetToolTip( _("Left side is newer") );

    ffgSizer11->Add( m_bitmapLeftNewer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_bitmapDifferent = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_bitmapDifferent->SetToolTip( _("Items have different content") );

    ffgSizer11->Add( m_bitmapDifferent, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_bitmapRightNewer = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_bitmapRightNewer->SetToolTip( _("Right side is newer") );

    ffgSizer11->Add( m_bitmapRightNewer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_bitmapRightOnly = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_bitmapRightOnly->SetToolTip( _("Item exists on right side only") );

    ffgSizer11->Add( m_bitmapRightOnly, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_bpButtonLeftOnly = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    ffgSizer11->Add( m_bpButtonLeftOnly, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_bpButtonLeftNewer = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    ffgSizer11->Add( m_bpButtonLeftNewer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_bpButtonDifferent = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    ffgSizer11->Add( m_bpButtonDifferent, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_bpButtonRightNewer = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    ffgSizer11->Add( m_bpButtonRightNewer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_bpButtonRightOnly = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    ffgSizer11->Add( m_bpButtonRightOnly, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );


    bSizerSyncDirsDiff->Add( ffgSizer11, 0, 0, 5 );

    m_staticText120 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Action"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText120->Wrap( -1 );
    bSizerSyncDirsDiff->Add( m_staticText120, 0, wxALIGN_CENTER_HORIZONTAL|wxTOP, 5 );


    bSizerSyncDirHolder->Add( bSizerSyncDirsDiff, 0, wxALIGN_CENTER_VERTICAL, 5 );

    bSizerSyncDirsChanges = new wxBoxSizer( wxVERTICAL );

    ffgSizer111 = new wxFlexGridSizer( 0, 3, 5, 5 );
    ffgSizer111->SetFlexibleDirection( wxBOTH );
    ffgSizer111->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_staticText12011 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Create:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText12011->Wrap( -1 );
    ffgSizer111->Add( m_staticText12011, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT, 5 );

    m_bpButtonLeftCreate = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    ffgSizer111->Add( m_bpButtonLeftCreate, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_bpButtonRightCreate = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    ffgSizer111->Add( m_bpButtonRightCreate, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText12012 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Update:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText12012->Wrap( -1 );
    ffgSizer111->Add( m_staticText12012, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5 );

    m_bpButtonLeftUpdate = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    ffgSizer111->Add( m_bpButtonLeftUpdate, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_bpButtonRightUpdate = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    ffgSizer111->Add( m_bpButtonRightUpdate, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText12013 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Delete:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText12013->Wrap( -1 );
    ffgSizer111->Add( m_staticText12013, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT, 5 );

    m_bpButtonLeftDelete = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    ffgSizer111->Add( m_bpButtonLeftDelete, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );

    m_bpButtonRightDelete = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    ffgSizer111->Add( m_bpButtonRightDelete, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );


    ffgSizer111->Add( 0, 0, 0, 0, 5 );

    m_staticText1201 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Left"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText1201->Wrap( -1 );
    ffgSizer111->Add( m_staticText1201, 0, wxALIGN_CENTER_HORIZONTAL, 5 );

    m_staticText1202 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Right"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText1202->Wrap( -1 );
    ffgSizer111->Add( m_staticText1202, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );


    bSizerSyncDirsChanges->Add( ffgSizer111, 0, 0, 5 );


    bSizerSyncDirHolder->Add( bSizerSyncDirsChanges, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer237->Add( bSizerSyncDirHolder, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5 );


    bSizer237->Add( 0, 0, 1, 0, 5 );


    bSizer232->Add( bSizer237, 0, wxEXPAND, 5 );

    m_staticline54 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer232->Add( m_staticline54, 0, wxEXPAND, 5 );

    bSizer2361 = new wxBoxSizer( wxHORIZONTAL );

    wxBoxSizer* bSizer202;
    bSizer202 = new wxBoxSizer( wxVERTICAL );

    m_staticText87 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Delete and overwrite:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText87->Wrap( -1 );
    bSizer202->Add( m_staticText87, 0, wxALL, 5 );

    wxBoxSizer* bSizer234;
    bSizer234 = new wxBoxSizer( wxVERTICAL );

    m_buttonRecycler = new zen::ToggleButton( m_panelSyncSettings, wxID_ANY, _("&Recycle bin"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonRecycler->SetToolTip( _("dummy") );

    bSizer234->Add( m_buttonRecycler, 0, wxEXPAND, 5 );

    m_buttonPermanent = new zen::ToggleButton( m_panelSyncSettings, wxID_ANY, _("&Permanent"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonPermanent->SetToolTip( _("dummy") );

    bSizer234->Add( m_buttonPermanent, 0, wxEXPAND, 5 );

    m_buttonVersioning = new zen::ToggleButton( m_panelSyncSettings, wxID_ANY, _("&Versioning"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonVersioning->SetToolTip( _("dummy") );

    bSizer234->Add( m_buttonVersioning, 0, wxEXPAND, 5 );


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

    m_hyperlink243 = new wxHyperlinkCtrl( m_panelVersioning, wxID_ANY, _("Show examples"), wxT("https://freefilesync.org/manual.php?topic=versioning"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlink243->SetToolTip( _("https://freefilesync.org/manual.php?topic=versioning") );

    bSizer254->Add( m_hyperlink243, 0, wxLEFT|wxALIGN_BOTTOM, 5 );


    bSizer253->Add( bSizer254, 0, wxEXPAND|wxBOTTOM, 5 );

    wxBoxSizer* bSizer156;
    bSizer156 = new wxBoxSizer( wxHORIZONTAL );

    m_versioningFolderPath = new fff::FolderHistoryBox( m_panelVersioning, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 );
    bSizer156->Add( m_versioningFolderPath, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonSelectVersioningFolder = new wxButton( m_panelVersioning, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
    m_buttonSelectVersioningFolder->SetToolTip( _("Select a folder") );

    bSizer156->Add( m_buttonSelectVersioningFolder, 0, wxEXPAND, 5 );

    m_bpButtonSelectVersioningAltFolder = new wxBitmapButton( m_panelVersioning, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
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

    m_spinCtrlVersionMaxDays = new wxSpinCtrl( m_panelVersioning, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
    fgSizer15->Add( m_spinCtrlVersionMaxDays, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_spinCtrlVersionCountMin = new wxSpinCtrl( m_panelVersioning, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
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

    wxBoxSizer* bSizer292;
    bSizer292 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer287;
    bSizer287 = new wxBoxSizer( wxHORIZONTAL );

    wxBoxSizer* bSizer290;
    bSizer290 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer291;
    bSizer291 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapEmail = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer291->Add( m_bitmapEmail, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_checkBoxSendEmail = new wxCheckBox( m_panelSyncSettings, wxID_ANY, _("Send email notification:"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer291->Add( m_checkBoxSendEmail, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );


    bSizer290->Add( bSizer291, 0, 0, 5 );

    m_comboBoxEmail = new fff::CommandBox( m_panelSyncSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 );
    bSizer290->Add( m_comboBoxEmail, 0, wxEXPAND|wxTOP, 5 );


    bSizer287->Add( bSizer290, 1, wxRIGHT, 5 );

    wxBoxSizer* bSizer289;
    bSizer289 = new wxBoxSizer( wxVERTICAL );

    m_bpButtonEmailAlways = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW|0 );
    bSizer289->Add( m_bpButtonEmailAlways, 0, 0, 5 );

    m_bpButtonEmailErrorWarning = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW|0 );
    bSizer289->Add( m_bpButtonEmailErrorWarning, 0, 0, 5 );

    m_bpButtonEmailErrorOnly = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW|0 );
    bSizer289->Add( m_bpButtonEmailErrorOnly, 0, 0, 5 );


    bSizer287->Add( bSizer289, 0, wxLEFT, 5 );


    bSizer292->Add( bSizer287, 0, wxEXPAND, 5 );

    m_hyperlinkPerfDeRequired2 = new wxHyperlinkCtrl( m_panelSyncSettings, wxID_ANY, _("Requires FreeFileSync Donation Edition"), wxT("https://freefilesync.org/faq.php#donation-edition"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlinkPerfDeRequired2->SetToolTip( _("https://freefilesync.org/faq.php#donation-edition") );

    bSizer292->Add( m_hyperlinkPerfDeRequired2, 0, wxALL, 5 );


    bSizerSyncMisc->Add( bSizer292, 0, wxEXPAND|wxALL, 10 );

    m_staticline57 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
    bSizerSyncMisc->Add( m_staticline57, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer293;
    bSizer293 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer2372;
    bSizer2372 = new wxBoxSizer( wxHORIZONTAL );

    m_panelLogfile = new wxPanel( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelLogfile->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer1912;
    bSizer1912 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer279;
    bSizer279 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapLogFile = new wxStaticBitmap( m_panelLogfile, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer279->Add( m_bitmapLogFile, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_checkBoxOverrideLogPath = new wxCheckBox( m_panelLogfile, wxID_ANY, _("&Change log folder:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_checkBoxOverrideLogPath->SetValue(true);
    bSizer279->Add( m_checkBoxOverrideLogPath, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );


    bSizer279->Add( 0, 0, 1, 0, 5 );

    m_bpButtonShowLogFolder = new wxBitmapButton( m_panelLogfile, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonShowLogFolder->SetToolTip( _("dummy") );

    bSizer279->Add( m_bpButtonShowLogFolder, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer1912->Add( bSizer279, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer297;
    bSizer297 = new wxBoxSizer( wxHORIZONTAL );

    m_logFolderPath = new fff::FolderHistoryBox( m_panelLogfile, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 );
    bSizer297->Add( m_logFolderPath, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonSelectLogFolder = new wxButton( m_panelLogfile, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
    m_buttonSelectLogFolder->SetToolTip( _("Select a folder") );

    bSizer297->Add( m_buttonSelectLogFolder, 0, wxEXPAND, 5 );

    m_bpButtonSelectAltLogFolder = new wxBitmapButton( m_panelLogfile, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonSelectAltLogFolder->SetToolTip( _("Access online storage") );

    bSizer297->Add( m_bpButtonSelectAltLogFolder, 0, wxEXPAND, 5 );


    bSizer1912->Add( bSizer297, 0, wxEXPAND|wxTOP, 5 );


    m_panelLogfile->SetSizer( bSizer1912 );
    m_panelLogfile->Layout();
    bSizer1912->Fit( m_panelLogfile );
    bSizer2372->Add( m_panelLogfile, 1, 0, 5 );


    bSizer293->Add( bSizer2372, 0, wxALL|wxEXPAND, 10 );

    m_staticline80 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer293->Add( m_staticline80, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer247;
    bSizer247 = new wxBoxSizer( wxHORIZONTAL );

    m_staticTextPostSync = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Run a command:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextPostSync->Wrap( -1 );
    bSizer247->Add( m_staticTextPostSync, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

    wxArrayString m_choicePostSyncConditionChoices;
    m_choicePostSyncCondition = new wxChoice( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choicePostSyncConditionChoices, 0 );
    m_choicePostSyncCondition->SetSelection( 0 );
    bSizer247->Add( m_choicePostSyncCondition, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_comboBoxPostSyncCommand = new fff::CommandBox( m_panelSyncSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 );
    bSizer247->Add( m_comboBoxPostSyncCommand, 1, wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );


    bSizer293->Add( bSizer247, 0, wxALL|wxEXPAND, 10 );


    bSizerSyncMisc->Add( bSizer293, 1, 0, 5 );


    bSizer232->Add( bSizerSyncMisc, 1, wxEXPAND, 5 );


    m_panelSyncSettings->SetSizer( bSizer232 );
    m_panelSyncSettings->Layout();
    bSizer232->Fit( m_panelSyncSettings );
    bSizer276->Add( m_panelSyncSettings, 1, wxEXPAND, 5 );


    m_panelSyncSettingsTab->SetSizer( bSizer276 );
    m_panelSyncSettingsTab->Layout();
    bSizer276->Fit( m_panelSyncSettingsTab );
    m_notebook->AddPage( m_panelSyncSettingsTab, _("dummy"), false );

    bSizer190->Add( m_notebook, 1, wxEXPAND, 5 );


    bSizer7->Add( bSizer190, 1, wxEXPAND, 5 );

    m_panelNotes = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelNotes->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer3021;
    bSizer3021 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer17311;
    bSizer17311 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapNotes = new wxStaticBitmap( m_panelNotes, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer17311->Add( m_bitmapNotes, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );

    m_staticText781 = new wxStaticText( m_panelNotes, wxID_ANY, _("Notes:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText781->Wrap( -1 );
    bSizer17311->Add( m_staticText781, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_textCtrNotes = new wxTextCtrl( m_panelNotes, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxTE_MULTILINE );
    bSizer17311->Add( m_textCtrNotes, 1, wxEXPAND, 5 );


    bSizer3021->Add( bSizer17311, 1, wxEXPAND, 5 );

    m_staticline83 = new wxStaticLine( m_panelNotes, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer3021->Add( m_staticline83, 0, wxEXPAND, 5 );


    m_panelNotes->SetSizer( bSizer3021 );
    m_panelNotes->Layout();
    bSizer3021->Fit( m_panelNotes );
    bSizer7->Add( m_panelNotes, 0, wxEXPAND, 5 );

    bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );

    m_buttonAddNotes = new zen::BitmapTextButton( this, wxID_ANY, _("Add &notes"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonAddNotes, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    bSizerStdButtons->Add( 0, 0, 1, wxEXPAND, 5 );

    m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonOkay->SetDefault();
    m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizerStdButtons->Add( m_buttonOkay, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer7->Add( bSizerStdButtons, 0, wxEXPAND, 5 );


    this->SetSizer( bSizer7 );
    this->Layout();
    bSizer7->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( ConfigDlgGenerated::onClose ) );
    m_listBoxFolderPair->Connect( wxEVT_KEY_DOWN, wxKeyEventHandler( ConfigDlgGenerated::onListBoxKeyEvent ), NULL, this );
    m_listBoxFolderPair->Connect( wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::onSelectFolderPair ), NULL, this );
    m_checkBoxUseLocalCmpOptions->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onToggleLocalCompSettings ), NULL, this );
    m_buttonByTimeSize->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onCompByTimeSize ), NULL, this );
    m_buttonByTimeSize->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::onCompByTimeSizeDouble ), NULL, this );
    m_buttonByContent->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onCompByContent ), NULL, this );
    m_buttonByContent->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::onCompByContentDouble ), NULL, this );
    m_buttonBySize->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onCompBySize ), NULL, this );
    m_buttonBySize->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::onCompBySizeDouble ), NULL, this );
    m_checkBoxSymlinksInclude->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onChangeCompOption ), NULL, this );
    m_checkBoxIgnoreErrors->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onToggleIgnoreErrors ), NULL, this );
    m_checkBoxAutoRetry->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onToggleAutoRetry ), NULL, this );
    m_textCtrlInclude->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( ConfigDlgGenerated::onChangeFilterOption ), NULL, this );
    m_textCtrlExclude->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( ConfigDlgGenerated::onChangeFilterOption ), NULL, this );
    m_choiceUnitMinSize->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::onChangeFilterOption ), NULL, this );
    m_choiceUnitMaxSize->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::onChangeFilterOption ), NULL, this );
    m_choiceUnitTimespan->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::onChangeFilterOption ), NULL, this );
    m_buttonDefault->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onFilterDefault ), NULL, this );
    m_buttonDefault->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( ConfigDlgGenerated::onFilterDefaultContextMouse ), NULL, this );
    m_bpButtonDefaultContext->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onFilterDefaultContext ), NULL, this );
    m_bpButtonDefaultContext->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( ConfigDlgGenerated::onFilterDefaultContextMouse ), NULL, this );
    m_buttonClear->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onFilterClear ), NULL, this );
    m_checkBoxUseLocalSyncOptions->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onToggleLocalSyncSettings ), NULL, this );
    m_buttonTwoWay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onSyncTwoWay ), NULL, this );
    m_buttonTwoWay->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::onSyncTwoWayDouble ), NULL, this );
    m_buttonMirror->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onSyncMirror ), NULL, this );
    m_buttonMirror->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::onSyncMirrorDouble ), NULL, this );
    m_buttonUpdate->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onSyncUpdate ), NULL, this );
    m_buttonUpdate->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::onSyncUpdateDouble ), NULL, this );
    m_buttonCustom->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onSyncCustom ), NULL, this );
    m_buttonCustom->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::onSyncCustomDouble ), NULL, this );
    m_checkBoxUseDatabase->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onToggleUseDatabase ), NULL, this );
    m_bpButtonLeftOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onLeftOnly ), NULL, this );
    m_bpButtonLeftNewer->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onLeftNewer ), NULL, this );
    m_bpButtonDifferent->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onDifferent ), NULL, this );
    m_bpButtonRightNewer->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onRightNewer ), NULL, this );
    m_bpButtonRightOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onRightOnly ), NULL, this );
    m_bpButtonLeftCreate->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onLeftCreate ), NULL, this );
    m_bpButtonRightCreate->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onRightCreate ), NULL, this );
    m_bpButtonLeftUpdate->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onLeftUpdate ), NULL, this );
    m_bpButtonRightUpdate->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onRightUpdate ), NULL, this );
    m_bpButtonLeftDelete->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onLeftDelete ), NULL, this );
    m_bpButtonRightDelete->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onRightDelete ), NULL, this );
    m_buttonRecycler->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onDeletionRecycler ), NULL, this );
    m_buttonPermanent->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onDeletionPermanent ), NULL, this );
    m_buttonVersioning->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onDeletionVersioning ), NULL, this );
    m_choiceVersioningStyle->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::onChanegVersioningStyle ), NULL, this );
    m_checkBoxVersionMaxDays->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onToggleVersioningLimit ), NULL, this );
    m_checkBoxVersionCountMin->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onToggleVersioningLimit ), NULL, this );
    m_checkBoxVersionCountMax->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onToggleVersioningLimit ), NULL, this );
    m_checkBoxSendEmail->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onToggleMiscEmail ), NULL, this );
    m_bpButtonEmailAlways->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onEmailAlways ), NULL, this );
    m_bpButtonEmailErrorWarning->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onEmailErrorWarning ), NULL, this );
    m_bpButtonEmailErrorOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onEmailErrorOnly ), NULL, this );
    m_checkBoxOverrideLogPath->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onToggleMiscOption ), NULL, this );
    m_bpButtonShowLogFolder->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onShowLogFolder ), NULL, this );
    m_buttonAddNotes->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onAddNotes ), NULL, this );
    m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onOkay ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::onCancel ), NULL, this );
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

    m_toggleBtnSftp = new wxToggleButton( this, wxID_ANY, _("SFTP"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_toggleBtnSftp->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer231->Add( m_toggleBtnSftp, 0, wxTOP|wxBOTTOM|wxLEFT|wxEXPAND, 5 );

    m_toggleBtnFtp = new wxToggleButton( this, wxID_ANY, _("FTP"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_toggleBtnFtp->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer231->Add( m_toggleBtnFtp, 0, wxALL|wxEXPAND, 5 );


    bSizer272->Add( bSizer231, 0, 0, 5 );


    bSizer72->Add( bSizer272, 0, wxALL, 5 );


    bSizer134->Add( bSizer72, 0, wxEXPAND, 5 );

    m_staticline371 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxSize( -1, -1 ), wxLI_HORIZONTAL );
    bSizer134->Add( m_staticline371, 0, wxEXPAND, 5 );

    m_panel41 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panel41->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer185;
    bSizer185 = new wxBoxSizer( wxVERTICAL );

    bSizerGdrive = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer284;
    bSizer284 = new wxBoxSizer( wxHORIZONTAL );

    wxBoxSizer* bSizer307;
    bSizer307 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer306;
    bSizer306 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapGdriveUser = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer306->Add( m_bitmapGdriveUser, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    m_staticText166 = new wxStaticText( m_panel41, wxID_ANY, _("Connected user accounts:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText166->Wrap( -1 );
    bSizer306->Add( m_staticText166, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer307->Add( bSizer306, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5 );

    m_listBoxGdriveUsers = new wxListBox( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_NEEDED_SB|wxLB_SINGLE|wxLB_SORT );
    bSizer307->Add( m_listBoxGdriveUsers, 1, wxBOTTOM|wxRIGHT|wxLEFT|wxEXPAND, 5 );

    wxBoxSizer* bSizer3002;
    bSizer3002 = new wxBoxSizer( wxHORIZONTAL );

    m_buttonGdriveAddUser = new zen::BitmapTextButton( m_panel41, wxID_ANY, _("&Add connection"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer3002->Add( m_buttonGdriveAddUser, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonGdriveRemoveUser = new zen::BitmapTextButton( m_panel41, wxID_ANY, _("&Disconnect"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer3002->Add( m_buttonGdriveRemoveUser, 1, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );


    bSizer307->Add( bSizer3002, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxALIGN_CENTER_HORIZONTAL, 5 );


    bSizer284->Add( bSizer307, 0, wxALL|wxEXPAND, 5 );

    m_staticline841 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
    bSizer284->Add( m_staticline841, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer3041;
    bSizer3041 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer305;
    bSizer305 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapGdriveDrive = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer305->Add( m_bitmapGdriveDrive, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText186 = new wxStaticText( m_panel41, wxID_ANY, _("Select drive:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText186->Wrap( -1 );
    bSizer305->Add( m_staticText186, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer3041->Add( bSizer305, 0, wxALL, 5 );

    m_listBoxGdriveDrives = new wxListBox( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_NEEDED_SB|wxLB_SINGLE );
    bSizer3041->Add( m_listBoxGdriveDrives, 1, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );


    bSizer284->Add( bSizer3041, 1, wxALL|wxEXPAND, 5 );


    bSizerGdrive->Add( bSizer284, 1, wxEXPAND, 5 );

    m_staticline73 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizerGdrive->Add( m_staticline73, 0, wxEXPAND, 5 );


    bSizer185->Add( bSizerGdrive, 1, wxEXPAND, 5 );

    bSizerServer = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer276;
    bSizer276 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapServer = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer276->Add( m_bitmapServer, 0, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText12311 = new wxStaticText( m_panel41, wxID_ANY, _("Server name or IP address:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText12311->Wrap( -1 );
    bSizer276->Add( m_staticText12311, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );

    m_textCtrlServer = new wxTextCtrl( m_panel41, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer276->Add( m_textCtrlServer, 1, wxALL|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText1233 = new wxStaticText( m_panel41, wxID_ANY, _("Port:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText1233->Wrap( -1 );
    bSizer276->Add( m_staticText1233, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );

    m_textCtrlPort = new wxTextCtrl( m_panel41, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), 0 );
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

    m_staticText123 = new wxStaticText( m_panelAuth, wxID_ANY, _("Username:"), wxDefaultPosition, wxDefaultSize, 0 );
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

    m_checkBoxPasswordPrompt = new wxCheckBox( m_panelAuth, wxID_ANY, _("Prompt during login"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizerPassword->Add( m_checkBoxPasswordPrompt, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


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

    wxBoxSizer* bSizer3051;
    bSizer3051 = new wxBoxSizer( wxHORIZONTAL );

    wxBoxSizer* bSizer270;
    bSizer270 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapServerDir = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer270->Add( m_bitmapServerDir, 0, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText1232 = new wxStaticText( m_panel41, wxID_ANY, _("Directory on server:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText1232->Wrap( -1 );
    bSizer270->Add( m_staticText1232, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    bSizer3051->Add( bSizer270, 0, wxTOP|wxRIGHT|wxLEFT|wxALIGN_BOTTOM, 5 );


    bSizer3051->Add( 0, 0, 1, 0, 5 );

    wxBoxSizer* bSizer3031;
    bSizer3031 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer303;
    bSizer303 = new wxBoxSizer( wxHORIZONTAL );

    m_staticline83 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
    bSizer303->Add( m_staticline83, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer3042;
    bSizer3042 = new wxBoxSizer( wxHORIZONTAL );

    m_staticTextTimeout = new wxStaticText( m_panel41, wxID_ANY, _("Access timeout (in seconds):"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextTimeout->Wrap( -1 );
    bSizer3042->Add( m_staticTextTimeout, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );

    m_spinCtrlTimeout = new wxSpinCtrl( m_panel41, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
    bSizer3042->Add( m_spinCtrlTimeout, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    bSizer303->Add( bSizer3042, 0, wxALL, 5 );


    bSizer3031->Add( bSizer303, 0, wxALIGN_RIGHT, 5 );

    m_staticline82 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer3031->Add( m_staticline82, 0, wxEXPAND, 5 );


    bSizer3051->Add( bSizer3031, 0, wxBOTTOM, 10 );


    bSizer269->Add( bSizer3051, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer217;
    bSizer217 = new wxBoxSizer( wxHORIZONTAL );

    m_textCtrlServerPath = new wxTextCtrl( m_panel41, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer217->Add( m_textCtrlServerPath, 1, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );

    m_buttonSelectFolder = new wxButton( m_panel41, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
    m_buttonSelectFolder->SetToolTip( _("Select a folder") );

    bSizer217->Add( m_buttonSelectFolder, 0, wxRIGHT|wxEXPAND, 5 );


    bSizer269->Add( bSizer217, 0, wxRIGHT|wxLEFT|wxEXPAND, 5 );


    bSizer269->Add( 0, 10, 0, 0, 5 );


    bSizer185->Add( bSizer269, 0, wxEXPAND, 5 );


    m_panel41->SetSizer( bSizer185 );
    m_panel41->Layout();
    bSizer185->Fit( m_panel41 );
    bSizer134->Add( m_panel41, 1, wxEXPAND, 5 );

    m_staticline571 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer134->Add( m_staticline571, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer219;
    bSizer219 = new wxBoxSizer( wxHORIZONTAL );


    bSizer219->Add( 5, 0, 0, 0, 5 );

    m_bitmapPerf = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer219->Add( m_bitmapPerf, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_staticText1361 = new wxStaticText( this, wxID_ANY, _("Performance improvements:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText1361->Wrap( -1 );
    bSizer219->Add( m_staticText1361, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 10 );


    bSizer219->Add( 0, 0, 1, wxEXPAND, 5 );

    m_hyperlink171 = new wxHyperlinkCtrl( this, wxID_ANY, _("How to get the best performance?"), wxT("https://freefilesync.org/manual.php?topic=ftp-setup"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlink171->SetToolTip( _("https://freefilesync.org/manual.php?topic=ftp-setup") );

    bSizer219->Add( m_hyperlink171, 0, wxALL|wxALIGN_CENTER_VERTICAL, 10 );


    bSizer134->Add( bSizer219, 0, wxEXPAND, 5 );

    m_staticline57 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer134->Add( m_staticline57, 0, wxEXPAND, 5 );

    m_panel411 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panel411->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer1851;
    bSizer1851 = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer* fgSizer1611;
    fgSizer1611 = new wxFlexGridSizer( 0, 2, 0, 0 );
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

    wxBoxSizer* bSizer300;
    bSizer300 = new wxBoxSizer( wxHORIZONTAL );

    m_spinCtrlConnectionCount = new wxSpinCtrl( m_panel411, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
    bSizer300->Add( m_spinCtrlConnectionCount, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticTextConnectionCountDescr = new wxStaticText( m_panel411, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextConnectionCountDescr->Wrap( -1 );
    m_staticTextConnectionCountDescr->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    bSizer300->Add( m_staticTextConnectionCountDescr, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_hyperlinkDeRequired = new wxHyperlinkCtrl( m_panel411, wxID_ANY, _("Requires FreeFileSync Donation Edition"), wxT("https://freefilesync.org/faq.php#donation-edition"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlinkDeRequired->SetToolTip( _("https://freefilesync.org/faq.php#donation-edition") );

    bSizer300->Add( m_hyperlinkDeRequired, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    fgSizer1611->Add( bSizer300, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticTextChannelCountSftp = new wxStaticText( m_panel411, wxID_ANY, _("SFTP channels per connection:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextChannelCountSftp->Wrap( -1 );
    fgSizer1611->Add( m_staticTextChannelCountSftp, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxTOP|wxBOTTOM|wxLEFT, 5 );

    wxBoxSizer* bSizer3001;
    bSizer3001 = new wxBoxSizer( wxHORIZONTAL );

    m_spinCtrlChannelCountSftp = new wxSpinCtrl( m_panel411, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
    bSizer3001->Add( m_spinCtrlChannelCountSftp, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonChannelCountSftp = new wxButton( m_panel411, wxID_ANY, _("Detect server limit"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer3001->Add( m_buttonChannelCountSftp, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );


    fgSizer1611->Add( bSizer3001, 0, wxALIGN_CENTER_VERTICAL, 5 );


    fgSizer1611->Add( 0, 0, 0, 0, 5 );

    wxBoxSizer* bSizer304;
    bSizer304 = new wxBoxSizer( wxHORIZONTAL );

    m_checkBoxAllowZlib = new wxCheckBox( m_panel411, wxID_ANY, _("Enable &compression"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer304->Add( m_checkBoxAllowZlib, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_staticTextZlibDescr = new wxStaticText( m_panel411, wxID_ANY, _("(zlib)"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextZlibDescr->Wrap( -1 );
    m_staticTextZlibDescr->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    bSizer304->Add( m_staticTextZlibDescr, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    fgSizer1611->Add( bSizer304, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer1851->Add( fgSizer1611, 0, wxALL, 5 );


    m_panel411->SetSizer( bSizer1851 );
    m_panel411->Layout();
    bSizer1851->Fit( m_panel411 );
    bSizer134->Add( m_panel411, 0, wxEXPAND, 5 );

    m_staticline12 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer134->Add( m_staticline12, 0, wxEXPAND, 5 );

    bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );

    m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonOkay->SetDefault();
    m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizerStdButtons->Add( m_buttonOkay, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer134->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );


    this->SetSizer( bSizer134 );
    this->Layout();
    bSizer134->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CloudSetupDlgGenerated::onClose ) );
    m_toggleBtnGdrive->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::onConnectionGdrive ), NULL, this );
    m_toggleBtnSftp->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::onConnectionSftp ), NULL, this );
    m_toggleBtnFtp->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::onConnectionFtp ), NULL, this );
    m_listBoxGdriveUsers->Connect( wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler( CloudSetupDlgGenerated::onGdriveUserSelect ), NULL, this );
    m_buttonGdriveAddUser->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::onGdriveUserAdd ), NULL, this );
    m_buttonGdriveRemoveUser->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::onGdriveUserRemove ), NULL, this );
    m_radioBtnPassword->Connect( wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler( CloudSetupDlgGenerated::onAuthPassword ), NULL, this );
    m_radioBtnKeyfile->Connect( wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler( CloudSetupDlgGenerated::onAuthKeyfile ), NULL, this );
    m_radioBtnAgent->Connect( wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler( CloudSetupDlgGenerated::onAuthAgent ), NULL, this );
    m_buttonSelectKeyfile->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::onSelectKeyfile ), NULL, this );
    m_textCtrlPasswordVisible->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( CloudSetupDlgGenerated::onTypingPassword ), NULL, this );
    m_textCtrlPasswordHidden->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( CloudSetupDlgGenerated::onTypingPassword ), NULL, this );
    m_checkBoxShowPassword->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::onToggleShowPassword ), NULL, this );
    m_checkBoxPasswordPrompt->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::onTogglePasswordPrompt ), NULL, this );
    m_buttonSelectFolder->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::onBrowseCloudFolder ), NULL, this );
    m_buttonChannelCountSftp->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::onDetectServerChannelLimit ), NULL, this );
    m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::onOkay ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CloudSetupDlgGenerated::onCancel ), NULL, this );
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

    m_treeCtrlFileSystem = new wxTreeCtrl( m_panel41, wxID_ANY, wxDefaultPosition, wxSize( -1, -1 ), wxTR_FULL_ROW_HIGHLIGHT|wxTR_HAS_BUTTONS|wxTR_LINES_AT_ROOT|wxTR_NO_LINES|wxBORDER_NONE );
    bSizer185->Add( m_treeCtrlFileSystem, 1, wxEXPAND, 5 );


    m_panel41->SetSizer( bSizer185 );
    m_panel41->Layout();
    bSizer185->Fit( m_panel41 );
    bSizer134->Add( m_panel41, 1, wxEXPAND, 5 );

    m_staticline12 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer134->Add( m_staticline12, 0, wxEXPAND, 5 );

    bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );

    m_buttonOkay = new wxButton( this, wxID_OK, _("Select Folder"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonOkay->SetDefault();
    m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizerStdButtons->Add( m_buttonOkay, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer134->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );


    this->SetSizer( bSizer134 );
    this->Layout();
    bSizer134->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( AbstractFolderPickerGenerated::onClose ) );
    m_treeCtrlFileSystem->Connect( wxEVT_COMMAND_TREE_ITEM_EXPANDING, wxTreeEventHandler( AbstractFolderPickerGenerated::onExpandNode ), NULL, this );
    m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AbstractFolderPickerGenerated::onOkay ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AbstractFolderPickerGenerated::onCancel ), NULL, this );
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

    m_staticTextSyncVar = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextSyncVar->Wrap( -1 );
    m_staticTextSyncVar->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer182->Add( m_staticTextSyncVar, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_bitmapSyncVar = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer182->Add( m_bitmapSyncVar, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


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

    m_buttonStartSync = new wxButton( this, wxID_OK, _("Start"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonStartSync->SetDefault();
    m_buttonStartSync->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizerStdButtons->Add( m_buttonStartSync, 0, wxALIGN_CENTER_VERTICAL|wxBOTTOM|wxRIGHT|wxLEFT, 5 );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxBOTTOM|wxRIGHT, 5 );


    bSizer164->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );


    bSizer134->Add( bSizer164, 1, wxEXPAND, 5 );


    this->SetSizer( bSizer134 );
    this->Layout();
    bSizer134->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( SyncConfirmationDlgGenerated::onClose ) );
    m_buttonStartSync->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( SyncConfirmationDlgGenerated::onStartSync ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( SyncConfirmationDlgGenerated::onCancel ), NULL, this );
}

SyncConfirmationDlgGenerated::~SyncConfirmationDlgGenerated()
{
}

CompareProgressDlgGenerated::CompareProgressDlgGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style, const wxString& name ) : wxPanel( parent, id, pos, size, style, name )
{
    this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer1811;
    bSizer1811 = new wxBoxSizer( wxVERTICAL );


    bSizer1811->Add( 0, 0, 1, 0, 5 );

    m_staticTextStatus = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextStatus->Wrap( -1 );
    bSizer1811->Add( m_staticTextStatus, 0, wxTOP|wxRIGHT|wxLEFT, 10 );

    wxBoxSizer* bSizer199;
    bSizer199 = new wxBoxSizer( wxHORIZONTAL );


    bSizer199->Add( 10, 0, 0, 0, 5 );

    ffgSizer11 = new wxFlexGridSizer( 2, 0, 5, 5 );
    ffgSizer11->SetFlexibleDirection( wxBOTH );
    ffgSizer11->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_staticTextProcessed = new wxStaticText( this, wxID_ANY, _("Processed:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextProcessed->Wrap( -1 );
    ffgSizer11->Add( m_staticTextProcessed, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxRIGHT, 5 );

    m_staticTextRemaining = new wxStaticText( this, wxID_ANY, _("Remaining:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextRemaining->Wrap( -1 );
    ffgSizer11->Add( m_staticTextRemaining, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );


    bSizer199->Add( ffgSizer11, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 10 );

    m_panelItemStats = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
    m_panelItemStats->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer291;
    bSizer291 = new wxBoxSizer( wxHORIZONTAL );

    ffgSizer111 = new wxFlexGridSizer( 0, 2, 5, 5 );
    ffgSizer111->SetFlexibleDirection( wxBOTH );
    ffgSizer111->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    wxBoxSizer* bSizer293;
    bSizer293 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapItemStat = new wxStaticBitmap( m_panelItemStats, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer293->Add( m_bitmapItemStat, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    m_staticTextItemsProcessed = new wxStaticText( m_panelItemStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_staticTextItemsProcessed->Wrap( -1 );
    m_staticTextItemsProcessed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer293->Add( m_staticTextItemsProcessed, 0, wxALIGN_BOTTOM, 5 );


    ffgSizer111->Add( bSizer293, 0, wxEXPAND|wxALIGN_RIGHT, 5 );

    m_staticTextBytesProcessed = new wxStaticText( m_panelItemStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextBytesProcessed->Wrap( -1 );
    ffgSizer111->Add( m_staticTextBytesProcessed, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );

    m_staticTextItemsRemaining = new wxStaticText( m_panelItemStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_staticTextItemsRemaining->Wrap( -1 );
    m_staticTextItemsRemaining->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    ffgSizer111->Add( m_staticTextItemsRemaining, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );

    m_staticTextBytesRemaining = new wxStaticText( m_panelItemStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextBytesRemaining->Wrap( -1 );
    ffgSizer111->Add( m_staticTextBytesRemaining, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );


    bSizer291->Add( ffgSizer111, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    m_panelItemStats->SetSizer( bSizer291 );
    m_panelItemStats->Layout();
    bSizer291->Fit( m_panelItemStats );
    bSizer199->Add( m_panelItemStats, 0, wxTOP|wxBOTTOM|wxRIGHT|wxEXPAND, 10 );

    m_panelTimeStats = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
    m_panelTimeStats->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer292;
    bSizer292 = new wxBoxSizer( wxHORIZONTAL );

    ffgSizer112 = new wxFlexGridSizer( 0, 1, 5, 5 );
    ffgSizer112->SetFlexibleDirection( wxBOTH );
    ffgSizer112->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    wxBoxSizer* bSizer294;
    bSizer294 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapTimeStat = new wxStaticBitmap( m_panelTimeStats, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer294->Add( m_bitmapTimeStat, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    m_staticTextTimeElapsed = new wxStaticText( m_panelTimeStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextTimeElapsed->Wrap( -1 );
    m_staticTextTimeElapsed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer294->Add( m_staticTextTimeElapsed, 0, wxALIGN_BOTTOM, 5 );


    ffgSizer112->Add( bSizer294, 0, wxEXPAND|wxALIGN_RIGHT, 5 );

    m_staticTextTimeRemaining = new wxStaticText( m_panelTimeStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextTimeRemaining->Wrap( -1 );
    m_staticTextTimeRemaining->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    ffgSizer112->Add( m_staticTextTimeRemaining, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );


    bSizer292->Add( ffgSizer112, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    m_panelTimeStats->SetSizer( bSizer292 );
    m_panelTimeStats->Layout();
    bSizer292->Fit( m_panelTimeStats );
    bSizer199->Add( m_panelTimeStats, 0, wxTOP|wxBOTTOM|wxRIGHT|wxEXPAND, 10 );

    ffgSizer114 = new wxFlexGridSizer( 2, 0, 5, 5 );
    ffgSizer114->SetFlexibleDirection( wxBOTH );
    ffgSizer114->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_staticTextErrors = new wxStaticText( this, wxID_ANY, _("Errors:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextErrors->Wrap( -1 );
    ffgSizer114->Add( m_staticTextErrors, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxRIGHT, 5 );

    m_staticTextWarnings = new wxStaticText( this, wxID_ANY, _("Warnings:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextWarnings->Wrap( -1 );
    ffgSizer114->Add( m_staticTextWarnings, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );


    bSizer199->Add( ffgSizer114, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 10 );

    m_panelErrorStats = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
    m_panelErrorStats->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer2921;
    bSizer2921 = new wxBoxSizer( wxHORIZONTAL );

    ffgSizer1121 = new wxFlexGridSizer( 0, 2, 5, 5 );
    ffgSizer1121->SetFlexibleDirection( wxBOTH );
    ffgSizer1121->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_bitmapErrors = new wxStaticBitmap( m_panelErrorStats, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    ffgSizer1121->Add( m_bitmapErrors, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticTextErrorCount = new wxStaticText( m_panelErrorStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextErrorCount->Wrap( -1 );
    m_staticTextErrorCount->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    ffgSizer1121->Add( m_staticTextErrorCount, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );

    m_bitmapWarnings = new wxStaticBitmap( m_panelErrorStats, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    ffgSizer1121->Add( m_bitmapWarnings, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticTextWarningCount = new wxStaticText( m_panelErrorStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextWarningCount->Wrap( -1 );
    m_staticTextWarningCount->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    ffgSizer1121->Add( m_staticTextWarningCount, 0, wxALIGN_BOTTOM|wxALIGN_RIGHT, 5 );


    bSizer2921->Add( ffgSizer1121, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    m_panelErrorStats->SetSizer( bSizer2921 );
    m_panelErrorStats->Layout();
    bSizer2921->Fit( m_panelErrorStats );
    bSizer199->Add( m_panelErrorStats, 0, wxEXPAND|wxTOP|wxBOTTOM|wxRIGHT, 10 );

    ffgSizer1141 = new wxFlexGridSizer( 2, 0, 5, 5 );
    ffgSizer1141->SetFlexibleDirection( wxBOTH );
    ffgSizer1141->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    bSizerErrorsRetry = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapRetryErrors = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizerErrorsRetry->Add( m_bitmapRetryErrors, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText1461 = new wxStaticText( this, wxID_ANY, _("Automatic retry"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText1461->Wrap( -1 );
    bSizerErrorsRetry->Add( m_staticText1461, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );

    m_staticTextRetryCount = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextRetryCount->Wrap( -1 );
    bSizerErrorsRetry->Add( m_staticTextRetryCount, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );


    ffgSizer1141->Add( bSizerErrorsRetry, 0, wxALIGN_CENTER_VERTICAL, 10 );

    bSizerErrorsIgnore = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapIgnoreErrors = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizerErrorsIgnore->Add( m_bitmapIgnoreErrors, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText146 = new wxStaticText( this, wxID_ANY, _("Ignore errors"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText146->Wrap( -1 );
    bSizerErrorsIgnore->Add( m_staticText146, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );


    ffgSizer1141->Add( bSizerErrorsIgnore, 0, wxALIGN_CENTER_VERTICAL, 10 );


    bSizer199->Add( ffgSizer1141, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 10 );

    bSizerProgressGraph = new wxBoxSizer( wxHORIZONTAL );

    ffgSizer113 = new wxFlexGridSizer( 2, 0, 5, 5 );
    ffgSizer113->SetFlexibleDirection( wxBOTH );
    ffgSizer113->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    wxStaticText* m_staticText99;
    m_staticText99 = new wxStaticText( this, wxID_ANY, _("Bytes:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText99->Wrap( -1 );
    ffgSizer113->Add( m_staticText99, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    wxStaticText* m_staticText100;
    m_staticText100 = new wxStaticText( this, wxID_ANY, _("Items:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText100->Wrap( -1 );
    ffgSizer113->Add( m_staticText100, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );


    bSizerProgressGraph->Add( ffgSizer113, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5 );

    m_panelProgressGraph = new zen::Graph2D( this, wxID_ANY, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_panelProgressGraph->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    bSizerProgressGraph->Add( m_panelProgressGraph, 1, wxEXPAND, 5 );


    bSizer199->Add( bSizerProgressGraph, 1, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 10 );


    bSizer1811->Add( bSizer199, 0, wxEXPAND, 5 );


    bSizer1811->Add( 0, 0, 1, 0, 5 );


    this->SetSizer( bSizer1811 );
    this->Layout();
    bSizer1811->Fit( this );
}

CompareProgressDlgGenerated::~CompareProgressDlgGenerated()
{
}

SyncProgressPanelGenerated::SyncProgressPanelGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style, const wxString& name ) : wxPanel( parent, id, pos, size, style, name )
{
    bSizerRoot = new wxBoxSizer( wxVERTICAL );

    m_panel53 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panel53->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer301;
    bSizer301 = new wxBoxSizer( wxVERTICAL );

    bSizer42 = new wxBoxSizer( wxHORIZONTAL );


    bSizer42->Add( 0, 0, 1, 0, 5 );

    m_bitmapStatus = new wxStaticBitmap( m_panel53, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer42->Add( m_bitmapStatus, 0, wxALIGN_CENTER_VERTICAL, 5 );

    wxBoxSizer* bSizer305;
    bSizer305 = new wxBoxSizer( wxHORIZONTAL );

    m_staticTextPhase = new wxStaticText( m_panel53, wxID_ANY, _("Synchronizing..."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextPhase->Wrap( -1 );
    m_staticTextPhase->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer305->Add( m_staticTextPhase, 0, wxALIGN_BOTTOM, 5 );

    m_staticTextPercentTotal = new wxStaticText( m_panel53, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextPercentTotal->Wrap( -1 );
    bSizer305->Add( m_staticTextPercentTotal, 0, wxALIGN_BOTTOM, 5 );


    bSizer42->Add( bSizer305, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );

    wxBoxSizer* bSizer247;
    bSizer247 = new wxBoxSizer( wxHORIZONTAL );


    bSizer247->Add( 0, 0, 1, 0, 5 );

    m_bpButtonMinimizeToTray = new wxBitmapButton( m_panel53, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonMinimizeToTray->SetToolTip( _("Minimize to notification area") );

    bSizer247->Add( m_bpButtonMinimizeToTray, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer42->Add( bSizer247, 1, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer301->Add( bSizer42, 0, wxEXPAND|wxTOP|wxBOTTOM, 5 );

    bSizerStatusText = new wxBoxSizer( wxVERTICAL );

    m_staticTextStatus = new wxStaticText( m_panel53, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextStatus->Wrap( -1 );
    bSizerStatusText->Add( m_staticTextStatus, 0, wxEXPAND|wxLEFT, 15 );


    bSizerStatusText->Add( 0, 10, 0, 0, 5 );


    bSizer301->Add( bSizerStatusText, 0, wxEXPAND, 5 );


    m_panel53->SetSizer( bSizer301 );
    m_panel53->Layout();
    bSizer301->Fit( m_panel53 );
    bSizerRoot->Add( m_panel53, 0, wxEXPAND, 5 );

    m_panelProgress = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelProgress->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer173;
    bSizer173 = new wxBoxSizer( wxHORIZONTAL );

    wxBoxSizer* bSizer161;
    bSizer161 = new wxBoxSizer( wxVERTICAL );

    m_panelGraphBytes = new zen::Graph2D( m_panelProgress, wxID_ANY, wxDefaultPosition, wxSize( -1, -1 ), 0 );
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


    bSizer233->Add( bSizer175, 0, wxALL, 5 );


    bSizer233->Add( 0, 0, 1, 0, 5 );

    wxBoxSizer* bSizer174;
    bSizer174 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapGraphKeyItems = new wxStaticBitmap( m_panelProgress, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer174->Add( m_bitmapGraphKeyItems, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    wxStaticText* m_staticText100;
    m_staticText100 = new wxStaticText( m_panelProgress, wxID_ANY, _("Items"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText100->Wrap( -1 );
    bSizer174->Add( m_staticText100, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer233->Add( bSizer174, 0, wxALL, 5 );


    bSizer232->Add( bSizer233, 1, wxEXPAND|wxRIGHT|wxLEFT, 5 );

    wxBoxSizer* bSizer304;
    bSizer304 = new wxBoxSizer( wxHORIZONTAL );

    ffgSizer11 = new wxFlexGridSizer( 2, 0, 5, 5 );
    ffgSizer11->SetFlexibleDirection( wxBOTH );
    ffgSizer11->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_staticTextProcessed = new wxStaticText( m_panelProgress, wxID_ANY, _("Processed:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextProcessed->Wrap( -1 );
    ffgSizer11->Add( m_staticTextProcessed, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxRIGHT, 5 );

    m_staticTextRemaining = new wxStaticText( m_panelProgress, wxID_ANY, _("Remaining:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextRemaining->Wrap( -1 );
    ffgSizer11->Add( m_staticTextRemaining, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );


    bSizer304->Add( ffgSizer11, 0, wxTOP|wxBOTTOM|wxALIGN_CENTER_VERTICAL, 10 );

    m_panelItemStats = new wxPanel( m_panelProgress, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
    m_panelItemStats->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer291;
    bSizer291 = new wxBoxSizer( wxHORIZONTAL );

    ffgSizer111 = new wxFlexGridSizer( 2, 0, 5, 5 );
    ffgSizer111->SetFlexibleDirection( wxBOTH );
    ffgSizer111->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    wxBoxSizer* bSizer293;
    bSizer293 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapItemStat = new wxStaticBitmap( m_panelItemStats, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer293->Add( m_bitmapItemStat, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    m_staticTextItemsProcessed = new wxStaticText( m_panelItemStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_staticTextItemsProcessed->Wrap( -1 );
    m_staticTextItemsProcessed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer293->Add( m_staticTextItemsProcessed, 0, wxALIGN_BOTTOM, 5 );


    ffgSizer111->Add( bSizer293, 0, wxEXPAND|wxALIGN_RIGHT, 5 );

    m_staticTextBytesProcessed = new wxStaticText( m_panelItemStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextBytesProcessed->Wrap( -1 );
    ffgSizer111->Add( m_staticTextBytesProcessed, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );

    m_staticTextItemsRemaining = new wxStaticText( m_panelItemStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_staticTextItemsRemaining->Wrap( -1 );
    m_staticTextItemsRemaining->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    ffgSizer111->Add( m_staticTextItemsRemaining, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );

    m_staticTextBytesRemaining = new wxStaticText( m_panelItemStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextBytesRemaining->Wrap( -1 );
    ffgSizer111->Add( m_staticTextBytesRemaining, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );


    bSizer291->Add( ffgSizer111, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    m_panelItemStats->SetSizer( bSizer291 );
    m_panelItemStats->Layout();
    bSizer291->Fit( m_panelItemStats );
    bSizer304->Add( m_panelItemStats, 0, wxTOP|wxBOTTOM|wxRIGHT|wxEXPAND, 10 );

    m_panelTimeStats = new wxPanel( m_panelProgress, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
    m_panelTimeStats->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer292;
    bSizer292 = new wxBoxSizer( wxHORIZONTAL );

    ffgSizer112 = new wxFlexGridSizer( 2, 0, 5, 5 );
    ffgSizer112->SetFlexibleDirection( wxBOTH );
    ffgSizer112->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    wxBoxSizer* bSizer294;
    bSizer294 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapTimeStat = new wxStaticBitmap( m_panelTimeStats, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer294->Add( m_bitmapTimeStat, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    m_staticTextTimeElapsed = new wxStaticText( m_panelTimeStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextTimeElapsed->Wrap( -1 );
    m_staticTextTimeElapsed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer294->Add( m_staticTextTimeElapsed, 0, wxALIGN_BOTTOM, 5 );


    ffgSizer112->Add( bSizer294, 0, wxEXPAND|wxALIGN_RIGHT, 5 );

    m_staticTextTimeRemaining = new wxStaticText( m_panelTimeStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextTimeRemaining->Wrap( -1 );
    m_staticTextTimeRemaining->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    ffgSizer112->Add( m_staticTextTimeRemaining, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );


    bSizer292->Add( ffgSizer112, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    m_panelTimeStats->SetSizer( bSizer292 );
    m_panelTimeStats->Layout();
    bSizer292->Fit( m_panelTimeStats );
    bSizer304->Add( m_panelTimeStats, 0, wxTOP|wxBOTTOM|wxRIGHT|wxEXPAND, 10 );

    ffgSizer114 = new wxFlexGridSizer( 2, 0, 5, 5 );
    ffgSizer114->SetFlexibleDirection( wxBOTH );
    ffgSizer114->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_staticTextErrors = new wxStaticText( m_panelProgress, wxID_ANY, _("Errors:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextErrors->Wrap( -1 );
    ffgSizer114->Add( m_staticTextErrors, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxRIGHT, 5 );

    m_staticTextWarnings = new wxStaticText( m_panelProgress, wxID_ANY, _("Warnings:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextWarnings->Wrap( -1 );
    ffgSizer114->Add( m_staticTextWarnings, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );


    bSizer304->Add( ffgSizer114, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 10 );

    m_panelErrorStats = new wxPanel( m_panelProgress, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
    m_panelErrorStats->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer2921;
    bSizer2921 = new wxBoxSizer( wxHORIZONTAL );

    ffgSizer1121 = new wxFlexGridSizer( 0, 2, 5, 5 );
    ffgSizer1121->SetFlexibleDirection( wxBOTH );
    ffgSizer1121->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_bitmapErrors = new wxStaticBitmap( m_panelErrorStats, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    ffgSizer1121->Add( m_bitmapErrors, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticTextErrorCount = new wxStaticText( m_panelErrorStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextErrorCount->Wrap( -1 );
    m_staticTextErrorCount->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    ffgSizer1121->Add( m_staticTextErrorCount, 0, wxALIGN_BOTTOM|wxALIGN_RIGHT, 5 );

    m_bitmapWarnings = new wxStaticBitmap( m_panelErrorStats, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    ffgSizer1121->Add( m_bitmapWarnings, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticTextWarningCount = new wxStaticText( m_panelErrorStats, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextWarningCount->Wrap( -1 );
    m_staticTextWarningCount->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    ffgSizer1121->Add( m_staticTextWarningCount, 0, wxALIGN_RIGHT|wxALIGN_BOTTOM, 5 );


    bSizer2921->Add( ffgSizer1121, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    m_panelErrorStats->SetSizer( bSizer2921 );
    m_panelErrorStats->Layout();
    bSizer2921->Fit( m_panelErrorStats );
    bSizer304->Add( m_panelErrorStats, 0, wxTOP|wxBOTTOM|wxRIGHT|wxEXPAND, 10 );


    bSizer232->Add( bSizer304, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer232->Add( 0, 0, 1, 0, 5 );

    bSizerDynSpace = new wxBoxSizer( wxVERTICAL );


    bSizerDynSpace->Add( 0, 0, 0, 0, 5 );


    bSizer232->Add( bSizerDynSpace, 0, 0, 5 );


    bSizer161->Add( bSizer232, 0, wxEXPAND, 5 );

    m_panelGraphItems = new zen::Graph2D( m_panelProgress, wxID_ANY, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_panelGraphItems->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    bSizer161->Add( m_panelGraphItems, 1, wxEXPAND|wxLEFT, 10 );

    bSizerProgressFooter = new wxBoxSizer( wxHORIZONTAL );

    bSizerErrorsRetry = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapRetryErrors = new wxStaticBitmap( m_panelProgress, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizerErrorsRetry->Add( m_bitmapRetryErrors, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText1461 = new wxStaticText( m_panelProgress, wxID_ANY, _("Automatic retry"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText1461->Wrap( -1 );
    bSizerErrorsRetry->Add( m_staticText1461, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );

    m_staticTextRetryCount = new wxStaticText( m_panelProgress, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextRetryCount->Wrap( -1 );
    bSizerErrorsRetry->Add( m_staticTextRetryCount, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );


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


    bSizer173->Add( bSizer161, 1, wxEXPAND|wxLEFT, 5 );


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


    bSizerStdButtons->Add( 0, 0, 1, wxEXPAND, 5 );

    m_checkBoxAutoClose = new wxCheckBox( this, wxID_ANY, _("Auto-close"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizerStdButtons->Add( m_checkBoxAutoClose, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );

    m_buttonClose = new wxButton( this, wxID_OK, _("Close"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonClose->Enable( false );

    bSizerStdButtons->Add( m_buttonClose, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );

    m_buttonPause = new wxButton( this, wxID_ANY, _("&Pause"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonPause, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );

    m_buttonStop = new wxButton( this, wxID_CANCEL, _("Stop"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonStop, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizerRoot->Add( bSizerStdButtons, 0, wxEXPAND, 5 );


    this->SetSizer( bSizerRoot );
    this->Layout();
    bSizerRoot->Fit( this );
}

SyncProgressPanelGenerated::~SyncProgressPanelGenerated()
{
}

LogPanelGenerated::LogPanelGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style, const wxString& name ) : wxPanel( parent, id, pos, size, style, name )
{
    this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer153;
    bSizer153 = new wxBoxSizer( wxHORIZONTAL );

    wxBoxSizer* bSizer154;
    bSizer154 = new wxBoxSizer( wxVERTICAL );

    m_bpButtonErrors = new zen::ToggleButton( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizer154->Add( m_bpButtonErrors, 0, wxALIGN_CENTER_HORIZONTAL, 5 );

    m_bpButtonWarnings = new zen::ToggleButton( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizer154->Add( m_bpButtonWarnings, 0, wxALIGN_CENTER_HORIZONTAL, 5 );

    m_bpButtonInfo = new zen::ToggleButton( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
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
    m_bpButtonErrors->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( LogPanelGenerated::onErrors ), NULL, this );
    m_bpButtonWarnings->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( LogPanelGenerated::onWarnings ), NULL, this );
    m_bpButtonInfo->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( LogPanelGenerated::onInfo ), NULL, this );
}

LogPanelGenerated::~LogPanelGenerated()
{
}

BatchDlgGenerated::BatchDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxSize( -1, -1 ), wxDefaultSize );
    this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer54;
    bSizer54 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer72;
    bSizer72 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapBatchJob = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
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


    bSizer242->Add( bSizer243, 0, wxTOP|wxRIGHT|wxLEFT, 5 );

    wxBoxSizer* bSizer246;
    bSizer246 = new wxBoxSizer( wxVERTICAL );

    m_radioBtnErrorDialogShow = new wxRadioButton( m_panel35, wxID_ANY, _("&Show error message"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
    m_radioBtnErrorDialogShow->SetValue( true );
    m_radioBtnErrorDialogShow->SetToolTip( _("Show pop-up on errors or warnings") );

    bSizer246->Add( m_radioBtnErrorDialogShow, 0, wxALL|wxEXPAND, 5 );

    m_radioBtnErrorDialogCancel = new wxRadioButton( m_panel35, wxID_ANY, _("&Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    m_radioBtnErrorDialogCancel->SetToolTip( _("Stop synchronization at first error") );

    bSizer246->Add( m_radioBtnErrorDialogCancel, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxEXPAND, 5 );


    bSizer242->Add( bSizer246, 0, wxALIGN_CENTER_HORIZONTAL|wxLEFT, 15 );


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

    m_hyperlink17 = new wxHyperlinkCtrl( m_panel35, wxID_ANY, _("How can I schedule a batch job?"), wxT("https://freefilesync.org/manual.php?topic=schedule-a-batch-job"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlink17->SetToolTip( _("https://freefilesync.org/manual.php?topic=schedule-a-batch-job") );

    bSizer172->Add( m_hyperlink17, 0, wxALL, 10 );


    m_panel35->SetSizer( bSizer172 );
    m_panel35->Layout();
    bSizer172->Fit( m_panel35 );
    bSizer54->Add( m_panel35, 1, wxEXPAND, 5 );

    m_staticline13 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer54->Add( m_staticline13, 0, wxEXPAND, 5 );

    bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );

    m_buttonSaveAs = new wxButton( this, wxID_SAVE, _("Save &as..."), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonSaveAs->SetDefault();
    m_buttonSaveAs->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizerStdButtons->Add( m_buttonSaveAs, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer54->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );


    this->SetSizer( bSizer54 );
    this->Layout();
    bSizer54->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( BatchDlgGenerated::onClose ) );
    m_checkBoxRunMinimized->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( BatchDlgGenerated::onToggleRunMinimized ), NULL, this );
    m_checkBoxIgnoreErrors->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( BatchDlgGenerated::onToggleIgnoreErrors ), NULL, this );
    m_buttonSaveAs->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( BatchDlgGenerated::onSaveBatchJob ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( BatchDlgGenerated::onCancel ), NULL, this );
}

BatchDlgGenerated::~BatchDlgGenerated()
{
}

DeleteDlgGenerated::DeleteDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxSize( -1, -1 ), wxDefaultSize );
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

    m_textCtrlFileList = new wxTextCtrl( m_panel31, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxTE_DONTWRAP|wxTE_MULTILINE|wxTE_READONLY|wxBORDER_NONE );
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

    m_buttonOK = new wxButton( this, wxID_OK, _("dummy"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonOK->SetDefault();
    m_buttonOK->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizerStdButtons->Add( m_buttonOK, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer24->Add( bSizerStdButtons, 0, wxEXPAND, 5 );


    this->SetSizer( bSizer24 );
    this->Layout();
    bSizer24->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( DeleteDlgGenerated::onClose ) );
    m_checkBoxUseRecycler->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( DeleteDlgGenerated::onUseRecycler ), NULL, this );
    m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DeleteDlgGenerated::onOkay ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DeleteDlgGenerated::onCancel ), NULL, this );
}

DeleteDlgGenerated::~DeleteDlgGenerated()
{
}

CopyToDlgGenerated::CopyToDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxSize( -1, -1 ), wxDefaultSize );
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

    m_textCtrlFileList = new wxTextCtrl( m_panel31, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxTE_DONTWRAP|wxTE_MULTILINE|wxTE_READONLY|wxBORDER_NONE );
    bSizer185->Add( m_textCtrlFileList, 1, wxEXPAND, 5 );


    bSizer242->Add( bSizer185, 1, wxEXPAND, 5 );

    wxBoxSizer* bSizer182;
    bSizer182 = new wxBoxSizer( wxHORIZONTAL );

    m_targetFolderPath = new fff::FolderHistoryBox( m_panel31, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 );
    bSizer182->Add( m_targetFolderPath, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonSelectTargetFolder = new wxButton( m_panel31, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
    m_buttonSelectTargetFolder->SetToolTip( _("Select a folder") );

    bSizer182->Add( m_buttonSelectTargetFolder, 0, wxEXPAND, 5 );

    m_bpButtonSelectAltTargetFolder = new wxBitmapButton( m_panel31, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
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

    m_buttonOK = new wxButton( this, wxID_OK, _("Copy"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonOK->SetDefault();
    m_buttonOK->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizerStdButtons->Add( m_buttonOK, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer24->Add( bSizerStdButtons, 0, wxEXPAND, 5 );


    this->SetSizer( bSizer24 );
    this->Layout();
    bSizer24->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CopyToDlgGenerated::onClose ) );
    m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CopyToDlgGenerated::onOkay ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CopyToDlgGenerated::onCancel ), NULL, this );
}

CopyToDlgGenerated::~CopyToDlgGenerated()
{
}

RenameDlgGenerated::RenameDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxSize( -1, -1 ), wxDefaultSize );
    this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer24;
    bSizer24 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer72;
    bSizer72 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapRename = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer72->Add( m_bitmapRename, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );

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

    m_gridRenamePreview = new zen::Grid( m_panel31, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
    m_gridRenamePreview->SetScrollRate( 5, 5 );
    bSizer242->Add( m_gridRenamePreview, 1, wxEXPAND, 5 );

    m_staticlinePreview = new wxStaticLine( m_panel31, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer242->Add( m_staticlinePreview, 0, wxEXPAND, 5 );

    m_staticTextPlaceholderDescription = new wxStaticText( m_panel31, wxID_ANY, _("Placeholders represent differences between the names."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextPlaceholderDescription->Wrap( -1 );
    m_staticTextPlaceholderDescription->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    bSizer242->Add( m_staticTextPlaceholderDescription, 0, wxTOP|wxRIGHT|wxLEFT|wxALIGN_CENTER_HORIZONTAL, 10 );

    m_textCtrlNewName = new wxTextCtrl( m_panel31, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer242->Add( m_textCtrlNewName, 0, wxEXPAND|wxALL, 10 );


    m_panel31->SetSizer( bSizer242 );
    m_panel31->Layout();
    bSizer242->Fit( m_panel31 );
    bSizer24->Add( m_panel31, 1, wxEXPAND, 5 );

    m_staticline9 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer24->Add( m_staticline9, 0, wxEXPAND, 5 );

    bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );

    m_buttonOK = new wxButton( this, wxID_OK, _("&Rename"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonOK->SetDefault();
    m_buttonOK->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizerStdButtons->Add( m_buttonOK, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer24->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );


    this->SetSizer( bSizer24 );
    this->Layout();
    bSizer24->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( RenameDlgGenerated::onClose ) );
    m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( RenameDlgGenerated::onOkay ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( RenameDlgGenerated::onCancel ), NULL, this );
}

RenameDlgGenerated::~RenameDlgGenerated()
{
}

OptionsDlgGenerated::OptionsDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxSize( -1, -1 ), wxDefaultSize );
    this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer95;
    bSizer95 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer72;
    bSizer72 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapSettings = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer72->Add( m_bitmapSettings, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );

    m_staticText44 = new wxStaticText( this, wxID_ANY, _("The following settings are used for all synchronization jobs."), wxDefaultPosition, wxSize( -1, -1 ), 0 );
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


    bSizer186->Add( bSizer160, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_staticline39 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
    bSizer186->Add( m_staticline39, 0, wxEXPAND, 5 );


    bSizer166->Add( bSizer186, 0, wxEXPAND, 5 );

    m_staticline191 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer166->Add( m_staticline191, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer292;
    bSizer292 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapWarnings = new wxStaticBitmap( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer292->Add( m_bitmapWarnings, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    m_staticText182 = new wxStaticText( m_panel39, wxID_ANY, _("Show hidden dialogs and warning messages again:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText182->Wrap( -1 );
    bSizer292->Add( m_staticText182, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticTextHiddenDialogsCount = new wxStaticText( m_panel39, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextHiddenDialogsCount->Wrap( -1 );
    m_staticTextHiddenDialogsCount->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    bSizer292->Add( m_staticTextHiddenDialogsCount, 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonShowHiddenDialogs = new wxButton( m_panel39, wxID_ANY, _("&Show details"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer292->Add( m_buttonShowHiddenDialogs, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );


    bSizer166->Add( bSizer292, 0, wxALL, 10 );

    wxArrayString m_checkListHiddenDialogsChoices;
    m_checkListHiddenDialogs = new wxCheckListBox( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_checkListHiddenDialogsChoices, wxLB_EXTENDED );
    bSizer166->Add( m_checkListHiddenDialogs, 1, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 10 );

    m_staticline1911 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer166->Add( m_staticline1911, 0, wxEXPAND, 5 );

    wxFlexGridSizer* fgSizer25111;
    fgSizer25111 = new wxFlexGridSizer( 0, 2, 0, 0 );
    fgSizer25111->AddGrowableCol( 1 );
    fgSizer25111->AddGrowableRow( 0 );
    fgSizer25111->SetFlexibleDirection( wxBOTH );
    fgSizer25111->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_bitmapLogFile = new wxStaticBitmap( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    fgSizer25111->Add( m_bitmapLogFile, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

    wxBoxSizer* bSizer296;
    bSizer296 = new wxBoxSizer( wxHORIZONTAL );

    m_staticText163 = new wxStaticText( m_panel39, wxID_ANY, _("Default log folder:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText163->Wrap( -1 );
    bSizer296->Add( m_staticText163, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_bpButtonShowLogFolder = new wxBitmapButton( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonShowLogFolder->SetToolTip( _("dummy") );

    bSizer296->Add( m_bpButtonShowLogFolder, 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );


    fgSizer25111->Add( bSizer296, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );


    fgSizer25111->Add( 0, 0, 0, 0, 5 );

    m_panelLogfile = new wxPanel( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelLogfile->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer279;
    bSizer279 = new wxBoxSizer( wxHORIZONTAL );

    m_logFolderPath = new fff::FolderHistoryBox( m_panelLogfile, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 );
    bSizer279->Add( m_logFolderPath, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonSelectLogFolder = new wxButton( m_panelLogfile, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
    m_buttonSelectLogFolder->SetToolTip( _("Select a folder") );

    bSizer279->Add( m_buttonSelectLogFolder, 0, wxEXPAND, 5 );

    m_bpButtonSelectAltLogFolder = new wxBitmapButton( m_panelLogfile, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonSelectAltLogFolder->SetToolTip( _("Access online storage") );

    bSizer279->Add( m_bpButtonSelectAltLogFolder, 0, wxEXPAND, 5 );


    m_panelLogfile->SetSizer( bSizer279 );
    m_panelLogfile->Layout();
    bSizer279->Fit( m_panelLogfile );
    fgSizer25111->Add( m_panelLogfile, 0, wxEXPAND, 5 );


    fgSizer25111->Add( 0, 0, 0, 0, 5 );

    wxBoxSizer* bSizer297;
    bSizer297 = new wxBoxSizer( wxHORIZONTAL );

    m_checkBoxLogFilesMaxAge = new wxCheckBox( m_panel39, wxID_ANY, _("&Delete logs after x days:"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer297->Add( m_checkBoxLogFilesMaxAge, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_spinCtrlLogFilesMaxAge = new wxSpinCtrl( m_panel39, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
    bSizer297->Add( m_spinCtrlLogFilesMaxAge, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

    m_staticline81 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
    bSizer297->Add( m_staticline81, 0, wxEXPAND|wxRIGHT|wxLEFT, 5 );

    m_staticText184 = new wxStaticText( m_panel39, wxID_ANY, _("Log file format:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText184->Wrap( -1 );
    bSizer297->Add( m_staticText184, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );

    wxFlexGridSizer* fgSizer251;
    fgSizer251 = new wxFlexGridSizer( 0, 1, 5, 0 );
    fgSizer251->SetFlexibleDirection( wxBOTH );
    fgSizer251->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_radioBtnLogHtml = new wxRadioButton( m_panel39, wxID_ANY, _("&HTML"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
    m_radioBtnLogHtml->SetValue( true );
    fgSizer251->Add( m_radioBtnLogHtml, 0, wxEXPAND, 5 );

    m_radioBtnLogText = new wxRadioButton( m_panel39, wxID_ANY, _("&Plain text"), wxDefaultPosition, wxDefaultSize, 0 );
    fgSizer251->Add( m_radioBtnLogText, 0, wxEXPAND, 5 );


    bSizer297->Add( fgSizer251, 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );


    fgSizer25111->Add( bSizer297, 0, wxTOP, 5 );


    bSizer166->Add( fgSizer25111, 0, wxALL|wxEXPAND, 10 );

    m_staticline361 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer166->Add( m_staticline361, 0, wxEXPAND, 5 );

    wxFlexGridSizer* fgSizer251111;
    fgSizer251111 = new wxFlexGridSizer( 0, 2, 0, 0 );
    fgSizer251111->AddGrowableCol( 1 );
    fgSizer251111->AddGrowableRow( 0 );
    fgSizer251111->SetFlexibleDirection( wxBOTH );
    fgSizer251111->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_bitmapNotificationSounds = new wxStaticBitmap( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    fgSizer251111->Add( m_bitmapNotificationSounds, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText851 = new wxStaticText( m_panel39, wxID_ANY, _("Notification sounds:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText851->Wrap( -1 );
    fgSizer251111->Add( m_staticText851, 0, wxALIGN_CENTER_VERTICAL, 5 );


    fgSizer251111->Add( 0, 0, 0, 0, 5 );

    ffgSizer11 = new wxFlexGridSizer( 0, 3, 0, 10 );
    ffgSizer11->AddGrowableCol( 2 );
    ffgSizer11->SetFlexibleDirection( wxBOTH );
    ffgSizer11->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_staticText171 = new wxStaticText( m_panel39, wxID_ANY, _("Comparison finished:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText171->Wrap( -1 );
    m_staticText171->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    ffgSizer11->Add( m_staticText171, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_bitmapCompareDone = new wxStaticBitmap( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    ffgSizer11->Add( m_bitmapCompareDone, 0, wxALIGN_CENTER_VERTICAL, 5 );

    wxBoxSizer* bSizer290;
    bSizer290 = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonPlayCompareDone = new wxBitmapButton( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW|0 );
    bSizer290->Add( m_bpButtonPlayCompareDone, 0, wxEXPAND, 5 );

    m_textCtrlSoundPathCompareDone = new wxTextCtrl( m_panel39, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer290->Add( m_textCtrlSoundPathCompareDone, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonSelectSoundCompareDone = new wxButton( m_panel39, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
    m_buttonSelectSoundCompareDone->SetToolTip( _("Select a folder") );

    bSizer290->Add( m_buttonSelectSoundCompareDone, 0, wxEXPAND, 5 );


    ffgSizer11->Add( bSizer290, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );

    m_staticText1711 = new wxStaticText( m_panel39, wxID_ANY, _("Synchronization finished:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText1711->Wrap( -1 );
    m_staticText1711->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    ffgSizer11->Add( m_staticText1711, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_bitmapSyncDone = new wxStaticBitmap( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    ffgSizer11->Add( m_bitmapSyncDone, 0, wxALIGN_CENTER_VERTICAL, 5 );

    wxBoxSizer* bSizer2901;
    bSizer2901 = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonPlaySyncDone = new wxBitmapButton( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW|0 );
    bSizer2901->Add( m_bpButtonPlaySyncDone, 0, wxEXPAND, 5 );

    m_textCtrlSoundPathSyncDone = new wxTextCtrl( m_panel39, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer2901->Add( m_textCtrlSoundPathSyncDone, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonSelectSoundSyncDone = new wxButton( m_panel39, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
    m_buttonSelectSoundSyncDone->SetToolTip( _("Select a folder") );

    bSizer2901->Add( m_buttonSelectSoundSyncDone, 0, wxEXPAND, 5 );


    ffgSizer11->Add( bSizer2901, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );

    m_staticText17111 = new wxStaticText( m_panel39, wxID_ANY, _("Unattended error message:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText17111->Wrap( -1 );
    m_staticText17111->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    ffgSizer11->Add( m_staticText17111, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_bitmapAlertPending = new wxStaticBitmap( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    ffgSizer11->Add( m_bitmapAlertPending, 0, wxALIGN_CENTER_VERTICAL, 5 );

    wxBoxSizer* bSizer29011;
    bSizer29011 = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonPlayAlertPending = new wxBitmapButton( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW|0 );
    bSizer29011->Add( m_bpButtonPlayAlertPending, 0, wxEXPAND, 5 );

    m_textCtrlSoundPathAlertPending = new wxTextCtrl( m_panel39, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer29011->Add( m_textCtrlSoundPathAlertPending, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonSelectSoundAlertPending = new wxButton( m_panel39, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
    m_buttonSelectSoundAlertPending->SetToolTip( _("Select a folder") );

    bSizer29011->Add( m_buttonSelectSoundAlertPending, 0, wxEXPAND, 5 );


    ffgSizer11->Add( bSizer29011, 1, wxEXPAND, 5 );


    fgSizer251111->Add( ffgSizer11, 0, wxEXPAND|wxTOP, 5 );


    bSizer166->Add( fgSizer251111, 0, wxALL|wxEXPAND, 10 );

    m_staticline3611 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer166->Add( m_staticline3611, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer2971;
    bSizer2971 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapConsole = new wxStaticBitmap( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer2971->Add( m_bitmapConsole, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText85 = new wxStaticText( m_panel39, wxID_ANY, _("Customize context menu:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText85->Wrap( -1 );
    bSizer2971->Add( m_staticText85, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );

    m_buttonShowCtxCustomize = new wxButton( m_panel39, wxID_ANY, _("&Show details"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer2971->Add( m_buttonShowCtxCustomize, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );


    bSizer166->Add( bSizer2971, 0, wxALL, 10 );

    bSizerContextCustomize = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer181;
    bSizer181 = new wxBoxSizer( wxHORIZONTAL );

    wxBoxSizer* bSizer2991;
    bSizer2991 = new wxBoxSizer( wxVERTICAL );


    bSizer2991->Add( 0, 0, 1, 0, 5 );

    wxBoxSizer* bSizer193;
    bSizer193 = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonAddRow = new wxBitmapButton( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizer193->Add( m_bpButtonAddRow, 0, wxALIGN_BOTTOM, 5 );

    m_bpButtonRemoveRow = new wxBitmapButton( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    bSizer193->Add( m_bpButtonRemoveRow, 0, wxALIGN_BOTTOM, 5 );


    bSizer2991->Add( bSizer193, 0, 0, 5 );


    bSizer181->Add( bSizer2991, 1, wxEXPAND, 5 );

    wxFlexGridSizer* fgSizer37;
    fgSizer37 = new wxFlexGridSizer( 0, 2, 0, 10 );
    fgSizer37->SetFlexibleDirection( wxBOTH );
    fgSizer37->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_staticText174 = new wxStaticText( m_panel39, wxID_ANY, _("%item_path%"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText174->Wrap( -1 );
    m_staticText174->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxEmptyString ) );
    m_staticText174->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    fgSizer37->Add( m_staticText174, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText175 = new wxStaticText( m_panel39, wxID_ANY, _("Full file or folder path"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText175->Wrap( -1 );
    m_staticText175->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    fgSizer37->Add( m_staticText175, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText178 = new wxStaticText( m_panel39, wxID_ANY, _("%local_path%"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText178->Wrap( -1 );
    m_staticText178->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxEmptyString ) );
    m_staticText178->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    fgSizer37->Add( m_staticText178, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText179 = new wxStaticText( m_panel39, wxID_ANY, _("Temporary local copy for SFTP and MTP storage"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText179->Wrap( -1 );
    m_staticText179->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    fgSizer37->Add( m_staticText179, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText189 = new wxStaticText( m_panel39, wxID_ANY, _("%item_name%"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText189->Wrap( -1 );
    m_staticText189->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxEmptyString ) );
    m_staticText189->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    fgSizer37->Add( m_staticText189, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText190 = new wxStaticText( m_panel39, wxID_ANY, _("File or folder name"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText190->Wrap( -1 );
    m_staticText190->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    fgSizer37->Add( m_staticText190, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText176 = new wxStaticText( m_panel39, wxID_ANY, _("%parent_path%"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText176->Wrap( -1 );
    m_staticText176->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxEmptyString ) );
    m_staticText176->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    fgSizer37->Add( m_staticText176, 0, wxALIGN_CENTER_VERTICAL, 5 );

    wxBoxSizer* bSizer298;
    bSizer298 = new wxBoxSizer( wxHORIZONTAL );

    m_staticText177 = new wxStaticText( m_panel39, wxID_ANY, _("Parent folder path"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText177->Wrap( -1 );
    m_staticText177->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    bSizer298->Add( m_staticText177, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );


    bSizer298->Add( 0, 0, 1, 0, 5 );

    m_hyperlink17 = new wxHyperlinkCtrl( m_panel39, wxID_ANY, _("Show examples"), wxT("https://freefilesync.org/manual.php?topic=external-applications"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlink17->SetToolTip( _("https://freefilesync.org/manual.php?topic=external-applications") );

    bSizer298->Add( m_hyperlink17, 0, wxLEFT|wxALIGN_BOTTOM, 5 );


    fgSizer37->Add( bSizer298, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );


    bSizer181->Add( fgSizer37, 0, wxBOTTOM|wxLEFT, 10 );


    bSizerContextCustomize->Add( bSizer181, 0, wxEXPAND, 5 );

    m_gridCustomCommand = new wxGrid( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );

    // Grid
    m_gridCustomCommand->CreateGrid( 3, 2 );
    m_gridCustomCommand->EnableEditing( true );
    m_gridCustomCommand->EnableGridLines( true );
    m_gridCustomCommand->EnableDragGridSize( false );
    m_gridCustomCommand->SetMargins( 0, 0 );

    // Columns
    m_gridCustomCommand->EnableDragColMove( false );
    m_gridCustomCommand->EnableDragColSize( false );
    m_gridCustomCommand->SetColLabelValue( 0, _("Description") );
    m_gridCustomCommand->SetColLabelValue( 1, _("Command line") );
    m_gridCustomCommand->SetColLabelSize( -1 );
    m_gridCustomCommand->SetColLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

    // Rows
    m_gridCustomCommand->EnableDragRowSize( false );
    m_gridCustomCommand->SetRowLabelSize( 1 );
    m_gridCustomCommand->SetRowLabelAlignment( wxALIGN_CENTER, wxALIGN_CENTER );

    // Label Appearance

    // Cell Defaults
    m_gridCustomCommand->SetDefaultCellAlignment( wxALIGN_LEFT, wxALIGN_TOP );
    bSizerContextCustomize->Add( m_gridCustomCommand, 1, wxEXPAND, 5 );


    bSizer166->Add( bSizerContextCustomize, 1, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 10 );


    m_panel39->SetSizer( bSizer166 );
    m_panel39->Layout();
    bSizer166->Fit( m_panel39 );
    bSizer95->Add( m_panel39, 1, wxEXPAND, 5 );

    m_staticline36 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer95->Add( m_staticline36, 0, wxEXPAND, 5 );

    bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );

    m_buttonDefault = new wxButton( this, wxID_DEFAULT, _("&Default"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonDefault, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    bSizerStdButtons->Add( 0, 0, 1, 0, 5 );

    m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonOkay->SetDefault();
    m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizerStdButtons->Add( m_buttonOkay, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer95->Add( bSizerStdButtons, 0, wxEXPAND, 5 );


    this->SetSizer( bSizer95 );
    this->Layout();
    bSizer95->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( OptionsDlgGenerated::onClose ) );
    m_buttonShowHiddenDialogs->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onShowHiddenDialogs ), NULL, this );
    m_checkListHiddenDialogs->Connect( wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, wxCommandEventHandler( OptionsDlgGenerated::onToggleHiddenDialog ), NULL, this );
    m_bpButtonShowLogFolder->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onShowLogFolder ), NULL, this );
    m_checkBoxLogFilesMaxAge->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onToggleLogfilesLimit ), NULL, this );
    m_bpButtonPlayCompareDone->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onPlayCompareDone ), NULL, this );
    m_textCtrlSoundPathCompareDone->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( OptionsDlgGenerated::onChangeSoundFilePath ), NULL, this );
    m_buttonSelectSoundCompareDone->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onSelectSoundCompareDone ), NULL, this );
    m_bpButtonPlaySyncDone->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onPlaySyncDone ), NULL, this );
    m_textCtrlSoundPathSyncDone->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( OptionsDlgGenerated::onChangeSoundFilePath ), NULL, this );
    m_buttonSelectSoundSyncDone->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onSelectSoundSyncDone ), NULL, this );
    m_bpButtonPlayAlertPending->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onPlayAlertPending ), NULL, this );
    m_textCtrlSoundPathAlertPending->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( OptionsDlgGenerated::onChangeSoundFilePath ), NULL, this );
    m_buttonSelectSoundAlertPending->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onSelectSoundAlertPending ), NULL, this );
    m_buttonShowCtxCustomize->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onShowContextCustomize ), NULL, this );
    m_bpButtonAddRow->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onAddRow ), NULL, this );
    m_bpButtonRemoveRow->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onRemoveRow ), NULL, this );
    m_buttonDefault->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onDefault ), NULL, this );
    m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onOkay ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::onCancel ), NULL, this );
}

OptionsDlgGenerated::~OptionsDlgGenerated()
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

    m_calendarFrom = new wxCalendarCtrl( m_panel35, wxID_ANY, wxDefaultDateTime, wxDefaultPosition, wxDefaultSize, wxCAL_SHOW_HOLIDAYS|wxCAL_SHOW_SURROUNDING_WEEKS|wxBORDER_NONE );
    bSizer98->Add( m_calendarFrom, 0, wxTOP|wxBOTTOM|wxLEFT, 10 );

    m_calendarTo = new wxCalendarCtrl( m_panel35, wxID_ANY, wxDefaultDateTime, wxDefaultPosition, wxDefaultSize, wxCAL_SHOW_HOLIDAYS|wxCAL_SHOW_SURROUNDING_WEEKS|wxBORDER_NONE );
    bSizer98->Add( m_calendarTo, 0, wxALL, 10 );


    m_panel35->SetSizer( bSizer98 );
    m_panel35->Layout();
    bSizer98->Fit( m_panel35 );
    bSizer96->Add( m_panel35, 0, wxEXPAND, 5 );

    m_staticline21 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer96->Add( m_staticline21, 0, wxEXPAND, 5 );

    bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );

    m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonOkay->SetDefault();
    m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizerStdButtons->Add( m_buttonOkay, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer96->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );


    this->SetSizer( bSizer96 );
    this->Layout();
    bSizer96->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( SelectTimespanDlgGenerated::onClose ) );
    m_calendarFrom->Connect( wxEVT_CALENDAR_SEL_CHANGED, wxCalendarEventHandler( SelectTimespanDlgGenerated::onChangeSelectionFrom ), NULL, this );
    m_calendarTo->Connect( wxEVT_CALENDAR_SEL_CHANGED, wxCalendarEventHandler( SelectTimespanDlgGenerated::onChangeSelectionTo ), NULL, this );
    m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( SelectTimespanDlgGenerated::onOkay ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( SelectTimespanDlgGenerated::onCancel ), NULL, this );
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

    m_bitmapLogoLeft = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer174->Add( m_bitmapLogoLeft, 0, wxBOTTOM, 5 );

    m_staticline81 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
    bSizer174->Add( m_staticline81, 0, wxEXPAND, 5 );

    bSizerMainSection = new wxBoxSizer( wxVERTICAL );

    m_staticline82 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizerMainSection->Add( m_staticline82, 0, wxEXPAND, 5 );

    m_bitmapLogo = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerMainSection->Add( m_bitmapLogo, 0, 0, 5 );

    m_staticline341 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizerMainSection->Add( m_staticline341, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer298;
    bSizer298 = new wxBoxSizer( wxHORIZONTAL );

    m_staticFfsTextVersion = new wxStaticText( m_panel41, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticFfsTextVersion->Wrap( -1 );
    bSizer298->Add( m_staticFfsTextVersion, 0, wxALIGN_BOTTOM, 5 );

    m_staticTextFfsVariant = new wxStaticText( m_panel41, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextFfsVariant->Wrap( -1 );
    m_staticTextFfsVariant->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer298->Add( m_staticTextFfsVariant, 0, wxLEFT|wxALIGN_BOTTOM, 10 );


    bSizerMainSection->Add( bSizer298, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    m_staticline3411 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizerMainSection->Add( m_staticline3411, 0, wxEXPAND, 5 );

    bSizerDonate = new wxBoxSizer( wxVERTICAL );


    bSizerDonate->Add( 0, 0, 1, 0, 5 );

    m_panelDonate = new wxPanel( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelDonate->SetBackgroundColour( wxColour( 153, 170, 187 ) );

    wxBoxSizer* bSizer183;
    bSizer183 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapAnimalSmall = new wxStaticBitmap( m_panelDonate, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer183->Add( m_bitmapAnimalSmall, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_panel39 = new wxPanel( m_panelDonate, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panel39->SetBackgroundColour( wxColour( 248, 248, 248 ) );

    wxBoxSizer* bSizer184;
    bSizer184 = new wxBoxSizer( wxHORIZONTAL );

    m_staticTextDonate = new wxStaticText( m_panel39, wxID_ANY, _("Get the Donation Edition with bonus features and help keep FreeFileSync ad-free."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextDonate->Wrap( -1 );
    m_staticTextDonate->SetForegroundColour( wxColour( 0, 0, 0 ) );

    bSizer184->Add( m_staticTextDonate, 0, wxALIGN_CENTER_HORIZONTAL|wxLEFT|wxALIGN_CENTER_VERTICAL, 10 );


    m_panel39->SetSizer( bSizer184 );
    m_panel39->Layout();
    bSizer184->Fit( m_panel39 );
    bSizer183->Add( m_panel39, 1, wxTOP|wxBOTTOM|wxRIGHT|wxEXPAND, 5 );


    m_panelDonate->SetSizer( bSizer183 );
    m_panelDonate->Layout();
    bSizer183->Fit( m_panelDonate );
    bSizerDonate->Add( m_panelDonate, 0, wxEXPAND, 5 );

    m_buttonDonate1 = new zen::BitmapTextButton( m_panel41, wxID_ANY, _("Support with a donation"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonDonate1->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
    m_buttonDonate1->SetToolTip( _("https://freefilesync.org/donate") );

    bSizerDonate->Add( m_buttonDonate1, 0, wxEXPAND|wxALL, 10 );


    bSizerDonate->Add( 0, 0, 1, 0, 5 );


    bSizerMainSection->Add( bSizerDonate, 1, wxEXPAND, 5 );

    m_bitmapAnimalBig = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizerMainSection->Add( m_bitmapAnimalBig, 0, wxALIGN_CENTER_HORIZONTAL, 5 );

    m_staticline3412 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizerMainSection->Add( m_staticline3412, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer186;
    bSizer186 = new wxBoxSizer( wxVERTICAL );

    m_staticText94 = new wxStaticText( m_panel41, wxID_ANY, _("Share your feedback and ideas:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText94->Wrap( -1 );
    bSizer186->Add( m_staticText94, 0, wxALIGN_CENTER_HORIZONTAL|wxTOP|wxRIGHT|wxLEFT, 5 );

    wxBoxSizer* bSizer289;
    bSizer289 = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonForum = new wxBitmapButton( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW|0 );
    m_bpButtonForum->SetToolTip( _("https://freefilesync.org/forum") );

    bSizer289->Add( m_bpButtonForum, 0, wxALL|wxEXPAND, 5 );

    m_bpButtonEmail = new wxBitmapButton( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW|0 );
    bSizer289->Add( m_bpButtonEmail, 0, wxTOP|wxBOTTOM|wxRIGHT|wxEXPAND, 5 );


    bSizer186->Add( bSizer289, 0, wxALIGN_CENTER_HORIZONTAL, 5 );


    bSizerMainSection->Add( bSizer186, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );


    bSizer174->Add( bSizerMainSection, 0, wxEXPAND, 5 );

    m_staticline37 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
    bSizer174->Add( m_staticline37, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer177;
    bSizer177 = new wxBoxSizer( wxVERTICAL );

    m_staticTextThanksForLoc = new wxStaticText( m_panel41, wxID_ANY, _("Many thanks for translation:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextThanksForLoc->Wrap( -1 );
    bSizer177->Add( m_staticTextThanksForLoc, 0, wxALL, 5 );

    m_scrolledWindowTranslators = new wxScrolledWindow( m_panel41, wxID_ANY, wxDefaultPosition, wxSize( -1, -1 ), wxVSCROLL );
    m_scrolledWindowTranslators->SetScrollRate( 10, 10 );
    m_scrolledWindowTranslators->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    fgSizerTranslators = new wxFlexGridSizer( 0, 2, 2, 10 );
    fgSizerTranslators->SetFlexibleDirection( wxBOTH );
    fgSizerTranslators->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );


    m_scrolledWindowTranslators->SetSizer( fgSizerTranslators );
    m_scrolledWindowTranslators->Layout();
    fgSizerTranslators->Fit( m_scrolledWindowTranslators );
    bSizer177->Add( m_scrolledWindowTranslators, 1, wxEXPAND|wxLEFT, 5 );


    bSizer174->Add( bSizer177, 0, wxEXPAND|wxLEFT, 5 );


    m_panel41->SetSizer( bSizer174 );
    m_panel41->Layout();
    bSizer174->Fit( m_panel41 );
    bSizer31->Add( m_panel41, 0, wxEXPAND, 5 );

    m_staticline36 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer31->Add( m_staticline36, 0, wxEXPAND, 5 );

    bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );

    m_buttonShowSupporterDetails = new wxButton( this, wxID_ANY, _("Thank you, %x, for your support!"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizerStdButtons->Add( m_buttonShowSupporterDetails, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );


    bSizerStdButtons->Add( 0, 0, 1, 0, 5 );

    m_buttonDonate2 = new zen::BitmapTextButton( this, wxID_ANY, _("&Donate"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonDonate2->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
    m_buttonDonate2->SetToolTip( _("https://freefilesync.org/donate") );

    bSizerStdButtons->Add( m_buttonDonate2, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );

    m_buttonClose = new wxButton( this, wxID_OK, _("Close"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonClose->SetDefault();
    bSizerStdButtons->Add( m_buttonClose, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    bSizer31->Add( bSizerStdButtons, 0, wxEXPAND, 5 );


    this->SetSizer( bSizer31 );
    this->Layout();
    bSizer31->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( AboutDlgGenerated::onClose ) );
    m_buttonDonate1->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AboutDlgGenerated::onDonate ), NULL, this );
    m_bpButtonForum->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AboutDlgGenerated::onOpenForum ), NULL, this );
    m_bpButtonEmail->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AboutDlgGenerated::onSendEmail ), NULL, this );
    m_buttonShowSupporterDetails->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AboutDlgGenerated::onShowSupporterDetails ), NULL, this );
    m_buttonDonate2->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AboutDlgGenerated::onDonate ), NULL, this );
    m_buttonClose->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AboutDlgGenerated::onOkay ), NULL, this );
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

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonCancel->SetDefault();
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );


    bSizer24->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );


    this->SetSizer( bSizer24 );
    this->Layout();
    bSizer24->Fit( this );

    // Connect Events
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DownloadProgressDlgGenerated::onCancel ), NULL, this );
}

DownloadProgressDlgGenerated::~DownloadProgressDlgGenerated()
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

    m_spinCtrlOverdueDays = new wxSpinCtrl( m_panel35, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxSP_ARROW_KEYS, 0, 2000000000, 0 );
    bSizer238->Add( m_spinCtrlOverdueDays, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5 );


    bSizer98->Add( bSizer238, 1, wxALL|wxEXPAND, 5 );


    m_panel35->SetSizer( bSizer98 );
    m_panel35->Layout();
    bSizer98->Fit( m_panel35 );
    bSizer96->Add( m_panel35, 0, 0, 5 );

    m_staticline21 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer96->Add( m_staticline21, 0, wxEXPAND, 5 );

    bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );

    m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonOkay->SetDefault();
    m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizerStdButtons->Add( m_buttonOkay, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer96->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );


    this->SetSizer( bSizer96 );
    this->Layout();
    bSizer96->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CfgHighlightDlgGenerated::onClose ) );
    m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CfgHighlightDlgGenerated::onOkay ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CfgHighlightDlgGenerated::onCancel ), NULL, this );
}

CfgHighlightDlgGenerated::~CfgHighlightDlgGenerated()
{
}

PasswordPromptDlgGenerated::PasswordPromptDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
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

    m_staticTextMain = new wxStaticText( m_panel35, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextMain->Wrap( -1 );
    bSizer238->Add( m_staticTextMain, 1, wxALL, 5 );

    wxBoxSizer* bSizer305;
    bSizer305 = new wxBoxSizer( wxHORIZONTAL );

    m_staticTextPassword = new wxStaticText( m_panel35, wxID_ANY, _("Password:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextPassword->Wrap( -1 );
    bSizer305->Add( m_staticTextPassword, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );

    m_textCtrlPasswordVisible = new wxTextCtrl( m_panel35, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer305->Add( m_textCtrlPasswordVisible, 1, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );

    m_textCtrlPasswordHidden = new wxTextCtrl( m_panel35, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD );
    bSizer305->Add( m_textCtrlPasswordHidden, 1, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );

    m_checkBoxShowPassword = new wxCheckBox( m_panel35, wxID_ANY, _("&Show password"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer305->Add( m_checkBoxShowPassword, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );


    bSizer238->Add( bSizer305, 0, wxEXPAND|wxTOP|wxBOTTOM, 5 );

    bSizerError = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapError = new wxStaticBitmap( m_panel35, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizerError->Add( m_bitmapError, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticTextError = new wxStaticText( m_panel35, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextError->Wrap( -1 );
    bSizerError->Add( m_staticTextError, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );


    bSizer238->Add( bSizerError, 0, 0, 5 );


    bSizer98->Add( bSizer238, 1, wxALL, 5 );


    m_panel35->SetSizer( bSizer98 );
    m_panel35->Layout();
    bSizer98->Fit( m_panel35 );
    bSizer96->Add( m_panel35, 1, wxEXPAND, 5 );

    m_staticline21 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer96->Add( m_staticline21, 0, wxEXPAND, 5 );

    bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );

    m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonOkay->SetDefault();
    m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizerStdButtons->Add( m_buttonOkay, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer96->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );


    this->SetSizer( bSizer96 );
    this->Layout();
    bSizer96->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( PasswordPromptDlgGenerated::onClose ) );
    m_textCtrlPasswordVisible->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( PasswordPromptDlgGenerated::onTypingPassword ), NULL, this );
    m_textCtrlPasswordHidden->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( PasswordPromptDlgGenerated::onTypingPassword ), NULL, this );
    m_checkBoxShowPassword->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( PasswordPromptDlgGenerated::onToggleShowPassword ), NULL, this );
    m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( PasswordPromptDlgGenerated::onOkay ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( PasswordPromptDlgGenerated::onCancel ), NULL, this );
}

PasswordPromptDlgGenerated::~PasswordPromptDlgGenerated()
{
}

ActivationDlgGenerated::ActivationDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxSize( -1, -1 ), wxDefaultSize );
    this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer54;
    bSizer54 = new wxBoxSizer( wxVERTICAL );

    m_panel35 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panel35->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer172;
    bSizer172 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer165;
    bSizer165 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapActivation = new wxStaticBitmap( m_panel35, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer165->Add( m_bitmapActivation, 0, wxALL, 10 );

    wxBoxSizer* bSizer16;
    bSizer16 = new wxBoxSizer( wxVERTICAL );


    bSizer16->Add( 0, 10, 0, 0, 5 );

    m_richTextLastError = new wxRichTextCtrl( m_panel35, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY|wxBORDER_NONE|wxVSCROLL|wxWANTS_CHARS );
    bSizer16->Add( m_richTextLastError, 1, wxEXPAND, 5 );


    bSizer165->Add( bSizer16, 1, wxEXPAND, 5 );


    bSizer172->Add( bSizer165, 1, wxEXPAND, 5 );

    m_staticline82 = new wxStaticLine( m_panel35, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer172->Add( m_staticline82, 0, wxEXPAND, 5 );

    m_staticTextMain = new wxStaticText( m_panel35, wxID_ANY, _("Activate FreeFileSync by one of the following methods:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextMain->Wrap( -1 );
    bSizer172->Add( m_staticTextMain, 0, wxALL, 10 );


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

    m_staticTextMain1 = new wxStaticText( m_panel3511, wxID_ANY, _("1."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextMain1->Wrap( -1 );
    bSizer234->Add( m_staticTextMain1, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText136 = new wxStaticText( m_panel3511, wxID_ANY, _("Activate via internet now:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText136->Wrap( -1 );
    bSizer234->Add( m_staticText136, 1, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    m_buttonActivateOnline = new wxButton( m_panel3511, wxID_ANY, _("Activate online"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonActivateOnline->SetDefault();
    m_buttonActivateOnline->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer234->Add( m_buttonActivateOnline, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer263->Add( bSizer234, 0, wxEXPAND|wxALL, 10 );


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

    m_staticText175 = new wxStaticText( m_panel351, wxID_ANY, _("2."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText175->Wrap( -1 );
    bSizer236->Add( m_staticText175, 0, wxRIGHT|wxALIGN_BOTTOM, 5 );

    m_staticText1361 = new wxStaticText( m_panel351, wxID_ANY, _("Retrieve an offline activation key from the following URL:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText1361->Wrap( -1 );
    bSizer236->Add( m_staticText1361, 1, wxRIGHT|wxALIGN_BOTTOM, 5 );

    m_buttonCopyUrl = new wxButton( m_panel351, wxID_ANY, _("&Copy to clipboard"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer236->Add( m_buttonCopyUrl, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer237->Add( bSizer236, 0, wxEXPAND|wxBOTTOM, 5 );

    m_richTextManualActivationUrl = new wxRichTextCtrl( m_panel351, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY|wxBORDER_NONE|wxVSCROLL|wxWANTS_CHARS );
    bSizer237->Add( m_richTextManualActivationUrl, 0, wxEXPAND|wxBOTTOM, 5 );

    wxBoxSizer* bSizer235;
    bSizer235 = new wxBoxSizer( wxHORIZONTAL );

    m_staticText13611 = new wxStaticText( m_panel351, wxID_ANY, _("Enter activation key:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText13611->Wrap( -1 );
    bSizer235->Add( m_staticText13611, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    m_textCtrlOfflineActivationKey = new wxTextCtrl( m_panel351, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), wxTE_PROCESS_ENTER );
    bSizer235->Add( m_textCtrlOfflineActivationKey, 1, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    m_buttonActivateOffline = new wxButton( m_panel351, wxID_ANY, _("Activate offline"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_buttonActivateOffline->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );

    bSizer235->Add( m_buttonActivateOffline, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer237->Add( bSizer235, 0, wxEXPAND|wxTOP, 5 );


    bSizer266->Add( bSizer237, 0, wxALL|wxEXPAND, 10 );


    m_panel351->SetSizer( bSizer266 );
    m_panel351->Layout();
    bSizer266->Fit( m_panel351 );
    bSizer54->Add( m_panel351, 0, wxEXPAND, 5 );

    m_staticline13 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer54->Add( m_staticline13, 0, wxEXPAND, 5 );

    bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );

    m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );


    bSizer54->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );


    this->SetSizer( bSizer54 );
    this->Layout();
    bSizer54->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( ActivationDlgGenerated::onClose ) );
    m_buttonActivateOnline->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ActivationDlgGenerated::onActivateOnline ), NULL, this );
    m_buttonCopyUrl->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ActivationDlgGenerated::onCopyUrl ), NULL, this );
    m_textCtrlOfflineActivationKey->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( ActivationDlgGenerated::onOfflineActivationEnter ), NULL, this );
    m_buttonActivateOffline->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ActivationDlgGenerated::onActivateOffline ), NULL, this );
    m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ActivationDlgGenerated::onCancel ), NULL, this );
}

ActivationDlgGenerated::~ActivationDlgGenerated()
{
}

WarnAccessRightsMissingDlgGenerated::WarnAccessRightsMissingDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxSize( -1, -1 ), wxDefaultSize );
    this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    wxBoxSizer* bSizer330;
    bSizer330 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapGrantAccess = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer330->Add( m_bitmapGrantAccess, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );

    wxBoxSizer* bSizer95;
    bSizer95 = new wxBoxSizer( wxVERTICAL );

    m_staticTextDescr = new wxStaticText( this, wxID_ANY, _("FreeFileSync requires access rights to avoid \"Operation not permitted\" errors when synchronizing your data (e.g. Mail, Messages, Calendars)."), wxDefaultPosition, wxSize( -1, -1 ), 0 );
    m_staticTextDescr->Wrap( -1 );
    bSizer95->Add( m_staticTextDescr, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 10 );

    m_staticline20 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer95->Add( m_staticline20, 0, wxEXPAND, 5 );

    m_panel39 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panel39->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer166;
    bSizer166 = new wxBoxSizer( wxVERTICAL );

    ffgSizer11 = new wxFlexGridSizer( 0, 2, 5, 5 );
    ffgSizer11->SetFlexibleDirection( wxBOTH );
    ffgSizer11->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

    m_staticTextStep1 = new wxStaticText( m_panel39, wxID_ANY, _("1."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextStep1->Wrap( -1 );
    ffgSizer11->Add( m_staticTextStep1, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );

    m_buttonLocateBundle = new wxButton( m_panel39, wxID_ANY, _("Locate the FreeFileSync app"), wxDefaultPosition, wxDefaultSize, 0 );
    ffgSizer11->Add( m_buttonLocateBundle, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );

    m_staticTextStep2 = new wxStaticText( m_panel39, wxID_ANY, _("2."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextStep2->Wrap( -1 );
    ffgSizer11->Add( m_staticTextStep2, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );

    m_buttonOpenSecurity = new wxButton( m_panel39, wxID_ANY, _("Open Security && Privacy"), wxDefaultPosition, wxDefaultSize, 0 );
    ffgSizer11->Add( m_buttonOpenSecurity, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );

    m_staticTextStep3 = new wxStaticText( m_panel39, wxID_ANY, _("3."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextStep3->Wrap( -1 );
    ffgSizer11->Add( m_staticTextStep3, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );

    m_staticTextAllowChanges = new wxStaticText( m_panel39, wxID_ANY, _("Click the lock to allow changes."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextAllowChanges->Wrap( -1 );
    ffgSizer11->Add( m_staticTextAllowChanges, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticTextStep4 = new wxStaticText( m_panel39, wxID_ANY, _("4."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextStep4->Wrap( -1 );
    ffgSizer11->Add( m_staticTextStep4, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );

    m_staticTextGrantAccess = new wxStaticText( m_panel39, wxID_ANY, _("Drag FreeFileSync into the panel."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticTextGrantAccess->Wrap( -1 );
    ffgSizer11->Add( m_staticTextGrantAccess, 0, wxALIGN_CENTER_VERTICAL, 5 );


    bSizer166->Add( ffgSizer11, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 10 );


    m_panel39->SetSizer( bSizer166 );
    m_panel39->Layout();
    bSizer166->Fit( m_panel39 );
    bSizer95->Add( m_panel39, 1, wxEXPAND, 5 );

    m_staticline36 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer95->Add( m_staticline36, 0, wxEXPAND, 5 );

    m_checkBoxDontShowAgain = new wxCheckBox( this, wxID_ANY, _("&Don't show this dialog again"), wxDefaultPosition, wxDefaultSize, 0 );
    bSizer95->Add( m_checkBoxDontShowAgain, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );

    m_buttonClose = new wxButton( this, wxID_OK, _("Close"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonClose->SetDefault();
    bSizerStdButtons->Add( m_buttonClose, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );


    bSizer95->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );


    bSizer330->Add( bSizer95, 1, wxEXPAND, 5 );


    this->SetSizer( bSizer330 );
    this->Layout();
    bSizer330->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( WarnAccessRightsMissingDlgGenerated::onClose ) );
    m_buttonLocateBundle->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( WarnAccessRightsMissingDlgGenerated::onShowAppBundle ), NULL, this );
    m_buttonOpenSecurity->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( WarnAccessRightsMissingDlgGenerated::onOpenSecuritySettings ), NULL, this );
    m_checkBoxDontShowAgain->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( WarnAccessRightsMissingDlgGenerated::onCheckBoxClick ), NULL, this );
    m_buttonClose->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( WarnAccessRightsMissingDlgGenerated::onOkay ), NULL, this );
}

WarnAccessRightsMissingDlgGenerated::~WarnAccessRightsMissingDlgGenerated()
{
}

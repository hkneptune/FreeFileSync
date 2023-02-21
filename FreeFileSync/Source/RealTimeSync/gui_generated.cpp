///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-0-g8feb16b3)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "wx+/bitmap_button.h"

#include "gui_generated.h"

///////////////////////////////////////////////////////////////////////////

MainDlgGenerated::MainDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxFrame( parent, id, title, pos, size, style )
{
    this->SetSizeHints( wxSize( -1, -1 ), wxDefaultSize );
    this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

    m_menubar1 = new wxMenuBar( 0 );
    m_menuFile = new wxMenu();
    wxMenuItem* m_menuItem6;
    m_menuItem6 = new wxMenuItem( m_menuFile, wxID_NEW, wxString( _("&New") ) + wxT('\t') + wxT("Ctrl+N"), wxEmptyString, wxITEM_NORMAL );
    m_menuFile->Append( m_menuItem6 );

    wxMenuItem* m_menuItem13;
    m_menuItem13 = new wxMenuItem( m_menuFile, wxID_OPEN, wxString( _("&Open...") ) + wxT('\t') + wxT("CTRL+O"), wxEmptyString, wxITEM_NORMAL );
    m_menuFile->Append( m_menuItem13 );

    wxMenuItem* m_menuItem14;
    m_menuItem14 = new wxMenuItem( m_menuFile, wxID_SAVEAS, wxString( _("Save &as...") ), wxEmptyString, wxITEM_NORMAL );
    m_menuFile->Append( m_menuItem14 );

    m_menuFile->AppendSeparator();

    m_menuItemQuit = new wxMenuItem( m_menuFile, wxID_EXIT, wxString( _("E&xit") ), wxEmptyString, wxITEM_NORMAL );
    m_menuFile->Append( m_menuItemQuit );

    m_menubar1->Append( m_menuFile, _("&File") );

    m_menuHelp = new wxMenu();
    wxMenuItem* m_menuItemContent;
    m_menuItemContent = new wxMenuItem( m_menuHelp, wxID_HELP, wxString( _("&View help") ) + wxT('\t') + wxT("F1"), wxEmptyString, wxITEM_NORMAL );
    m_menuHelp->Append( m_menuItemContent );

    m_menuHelp->AppendSeparator();

    m_menuItemAbout = new wxMenuItem( m_menuHelp, wxID_ABOUT, wxString( _("&About") ) + wxT('\t') + wxT("SHIFT+F1"), wxEmptyString, wxITEM_NORMAL );
    m_menuHelp->Append( m_menuItemAbout );

    m_menubar1->Append( m_menuHelp, _("&Help") );

    this->SetMenuBar( m_menubar1 );

    bSizerMain = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer161;
    bSizer161 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer152;
    bSizer152 = new wxBoxSizer( wxHORIZONTAL );

    m_staticText811 = new wxStaticText( this, wxID_ANY, _("To get started, just import a \"ffs_batch\" file."), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText811->Wrap( -1 );
    m_staticText811->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    bSizer152->Add( m_staticText811, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );

    m_staticText10 = new wxStaticText( this, wxID_ANY, _("("), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText10->Wrap( -1 );
    m_staticText10->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    bSizer152->Add( m_staticText10, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 2 );

    m_bitmapBatch = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer152->Add( m_bitmapBatch, 0, wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText11 = new wxStaticText( this, wxID_ANY, _(")"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText11->Wrap( -1 );
    m_staticText11->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );

    bSizer152->Add( m_staticText11, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 2 );

    m_hyperlink243 = new wxHyperlinkCtrl( this, wxID_ANY, _("Show examples"), wxT("https://freefilesync.org/manual.php?topic=realtimesync"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
    m_hyperlink243->SetToolTip( _("https://freefilesync.org/manual.php?topic=realtimesync") );

    bSizer152->Add( m_hyperlink243, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );


    bSizer161->Add( bSizer152, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5 );


    bSizerMain->Add( bSizer161, 0, wxALL|wxEXPAND, 5 );

    m_staticline2 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizerMain->Add( m_staticline2, 0, wxEXPAND, 5 );

    m_panelMain = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelMain->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer1;
    bSizer1 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer151;
    bSizer151 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer142;
    bSizer142 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapFolders = new wxStaticBitmap( m_panelMain, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer142->Add( m_bitmapFolders, 0, wxTOP|wxBOTTOM|wxLEFT, 5 );

    m_staticText7 = new wxStaticText( m_panelMain, wxID_ANY, _("Folders to watch for changes:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText7->Wrap( -1 );
    bSizer142->Add( m_staticText7, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    bSizer151->Add( bSizer142, 0, 0, 5 );

    m_panelMainFolder = new wxPanel( m_panelMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
    m_panelMainFolder->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer143;
    bSizer143 = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonAddFolder = new wxBitmapButton( m_panelMainFolder, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonAddFolder->SetToolTip( _("Add folder") );

    bSizer143->Add( m_bpButtonAddFolder, 0, wxEXPAND, 5 );

    m_bpButtonRemoveTopFolder = new wxBitmapButton( m_panelMainFolder, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonRemoveTopFolder->SetToolTip( _("Remove folder") );

    bSizer143->Add( m_bpButtonRemoveTopFolder, 0, wxEXPAND, 5 );

    m_txtCtrlDirectoryMain = new wxTextCtrl( m_panelMainFolder, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1, -1 ), 0 );
    bSizer143->Add( m_txtCtrlDirectoryMain, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonSelectFolderMain = new wxButton( m_panelMainFolder, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
    m_buttonSelectFolderMain->SetToolTip( _("Select a folder") );

    bSizer143->Add( m_buttonSelectFolderMain, 0, wxEXPAND, 5 );


    m_panelMainFolder->SetSizer( bSizer143 );
    m_panelMainFolder->Layout();
    bSizer143->Fit( m_panelMainFolder );
    bSizer151->Add( m_panelMainFolder, 0, wxRIGHT|wxLEFT|wxEXPAND, 5 );

    m_scrolledWinFolders = new wxScrolledWindow( m_panelMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
    m_scrolledWinFolders->SetScrollRate( 5, 5 );
    m_scrolledWinFolders->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    bSizerFolders = new wxBoxSizer( wxVERTICAL );


    m_scrolledWinFolders->SetSizer( bSizerFolders );
    m_scrolledWinFolders->Layout();
    bSizerFolders->Fit( m_scrolledWinFolders );
    bSizer151->Add( m_scrolledWinFolders, 1, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );


    bSizer1->Add( bSizer151, 1, wxALL|wxEXPAND, 10 );

    m_staticline212 = new wxStaticLine( m_panelMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer1->Add( m_staticline212, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer141;
    bSizer141 = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer* bSizer13;
    bSizer13 = new wxBoxSizer( wxHORIZONTAL );

    m_bitmapConsole = new wxStaticBitmap( m_panelMain, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer13->Add( m_bitmapConsole, 0, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );

    m_staticText6 = new wxStaticText( m_panelMain, wxID_ANY, _("Command line to run when changes are detected:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText6->Wrap( -1 );
    bSizer13->Add( m_staticText6, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );


    bSizer141->Add( bSizer13, 0, 0, 5 );

    m_textCtrlCommand = new wxTextCtrl( m_panelMain, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    m_textCtrlCommand->SetToolTip( _("The command is triggered if:\n- files or subfolders change\n- new folders arrive (e.g. USB stick insert)") );

    bSizer141->Add( m_textCtrlCommand, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );


    bSizer1->Add( bSizer141, 0, wxALL|wxEXPAND, 10 );

    m_staticline211 = new wxStaticLine( m_panelMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizer1->Add( m_staticline211, 0, wxEXPAND, 5 );

    wxBoxSizer* bSizer14;
    bSizer14 = new wxBoxSizer( wxHORIZONTAL );

    m_staticText8 = new wxStaticText( m_panelMain, wxID_ANY, _("Minimum idle time (in seconds) before running command:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_staticText8->Wrap( -1 );
    bSizer14->Add( m_staticText8, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );

    m_spinCtrlDelay = new wxSpinCtrl( m_panelMain, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 2000000000, 0 );
    m_spinCtrlDelay->SetToolTip( _("Idle time between last detected change and execution of command") );

    bSizer14->Add( m_spinCtrlDelay, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );


    bSizer1->Add( bSizer14, 0, wxALL|wxEXPAND, 10 );


    m_panelMain->SetSizer( bSizer1 );
    m_panelMain->Layout();
    bSizer1->Fit( m_panelMain );
    bSizerMain->Add( m_panelMain, 1, wxEXPAND, 5 );

    m_staticline5 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    bSizerMain->Add( m_staticline5, 0, wxEXPAND, 5 );

    m_buttonStart = new zen::BitmapTextButton( this, wxID_OK, _("Start"), wxDefaultPosition, wxSize( -1, -1 ), 0 );

    m_buttonStart->SetDefault();
    m_buttonStart->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxEmptyString ) );

    bSizerMain->Add( m_buttonStart, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );


    this->SetSizer( bSizerMain );
    this->Layout();
    bSizerMain->Fit( this );

    this->Centre( wxBOTH );

    // Connect Events
    this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( MainDlgGenerated::onClose ) );
    m_menuFile->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDlgGenerated::onConfigNew ), this, m_menuItem6->GetId());
    m_menuFile->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDlgGenerated::onConfigLoad ), this, m_menuItem13->GetId());
    m_menuFile->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDlgGenerated::onConfigSave ), this, m_menuItem14->GetId());
    m_menuFile->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDlgGenerated::onMenuQuit ), this, m_menuItemQuit->GetId());
    m_menuHelp->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDlgGenerated::onShowHelp ), this, m_menuItemContent->GetId());
    m_menuHelp->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDlgGenerated::onMenuAbout ), this, m_menuItemAbout->GetId());
    m_bpButtonAddFolder->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDlgGenerated::onAddFolder ), NULL, this );
    m_bpButtonRemoveTopFolder->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDlgGenerated::onRemoveTopFolder ), NULL, this );
    m_buttonStart->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDlgGenerated::onStart ), NULL, this );
}

MainDlgGenerated::~MainDlgGenerated()
{
}

FolderGenerated::FolderGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style, const wxString& name ) : wxPanel( parent, id, pos, size, style, name )
{
    this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

    wxBoxSizer* bSizer114;
    bSizer114 = new wxBoxSizer( wxHORIZONTAL );

    m_bpButtonRemoveFolder = new wxBitmapButton( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1, -1 ), wxBU_AUTODRAW|0 );
    m_bpButtonRemoveFolder->SetToolTip( _("Remove folder") );

    bSizer114->Add( m_bpButtonRemoveFolder, 0, wxEXPAND, 5 );

    m_txtCtrlDirectory = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
    bSizer114->Add( m_txtCtrlDirectory, 1, wxALIGN_CENTER_VERTICAL, 5 );

    m_buttonSelectFolder = new wxButton( this, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
    m_buttonSelectFolder->SetToolTip( _("Select a folder") );

    bSizer114->Add( m_buttonSelectFolder, 0, wxEXPAND, 5 );


    this->SetSizer( bSizer114 );
    this->Layout();
    bSizer114->Fit( this );
}

FolderGenerated::~FolderGenerated()
{
}

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
namespace zen { class BitmapTextButton; }

#include <wx/string.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/menu.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/stattext.h>
#include <wx/statbmp.h>
#include <wx/hyperlink.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/spinctrl.h>
#include <wx/frame.h>

#include "zen/i18n.h"

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class MainDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class MainDlgGenerated : public wxFrame
{
private:

protected:
    wxMenuBar* m_menubar1;
    wxMenu* m_menuFile;
    wxMenuItem* m_menuItemQuit;
    wxMenu* m_menuHelp;
    wxMenuItem* m_menuItemAbout;
    wxBoxSizer* bSizerMain;
    wxStaticText* m_staticText811;
    wxStaticText* m_staticText10;
    wxStaticBitmap* m_bitmapBatch;
    wxStaticText* m_staticText11;
    wxHyperlinkCtrl* m_hyperlink243;
    wxStaticLine* m_staticline2;
    wxPanel* m_panelMain;
    wxStaticBitmap* m_bitmapFolders;
    wxStaticText* m_staticText7;
    wxPanel* m_panelMainFolder;
    wxBitmapButton* m_bpButtonAddFolder;
    wxBitmapButton* m_bpButtonRemoveTopFolder;
    wxTextCtrl* m_txtCtrlDirectoryMain;
    wxButton* m_buttonSelectFolderMain;
    wxScrolledWindow* m_scrolledWinFolders;
    wxBoxSizer* bSizerFolders;
    wxStaticLine* m_staticline212;
    wxStaticBitmap* m_bitmapConsole;
    wxStaticText* m_staticText6;
    wxTextCtrl* m_textCtrlCommand;
    wxStaticLine* m_staticline211;
    wxStaticText* m_staticText8;
    wxSpinCtrl* m_spinCtrlDelay;
    wxStaticLine* m_staticline5;
    zen::BitmapTextButton* m_buttonStart;

    // Virtual event handlers, override them in your derived class
    virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
    virtual void onConfigNew( wxCommandEvent& event ) { event.Skip(); }
    virtual void onConfigLoad( wxCommandEvent& event ) { event.Skip(); }
    virtual void onConfigSave( wxCommandEvent& event ) { event.Skip(); }
    virtual void onMenuQuit( wxCommandEvent& event ) { event.Skip(); }
    virtual void onShowHelp( wxCommandEvent& event ) { event.Skip(); }
    virtual void onMenuAbout( wxCommandEvent& event ) { event.Skip(); }
    virtual void onAddFolder( wxCommandEvent& event ) { event.Skip(); }
    virtual void onRemoveTopFolder( wxCommandEvent& event ) { event.Skip(); }
    virtual void onStart( wxCommandEvent& event ) { event.Skip(); }


public:

    MainDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("dummy"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1, -1 ), long style = wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL );

    ~MainDlgGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class FolderGenerated
///////////////////////////////////////////////////////////////////////////////
class FolderGenerated : public wxPanel
{
private:

protected:
    wxButton* m_buttonSelectFolder;

public:
    wxBitmapButton* m_bpButtonRemoveFolder;
    wxTextCtrl* m_txtCtrlDirectory;

    FolderGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1, -1 ), long style = 0, const wxString& name = wxEmptyString );

    ~FolderGenerated();

};


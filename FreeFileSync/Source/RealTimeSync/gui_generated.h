///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Nov  6 2017)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#ifndef __GUI_GENERATED_H__
#define __GUI_GENERATED_H__

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
    wxMenu* m_menuHelp;
    wxMenuItem* m_menuItemAbout;
    wxBoxSizer* bSizerMain;
    wxStaticText* m_staticText9;
    wxStaticText* m_staticText3;
    wxStaticText* m_staticText4;
    wxStaticText* m_staticText5;
    wxStaticText* m_staticText811;
    wxStaticLine* m_staticline2;
    wxPanel* m_panelMain;
    wxStaticText* m_staticText7;
    wxPanel* m_panelMainFolder;
    wxStaticText* m_staticTextFinalPath;
    wxBitmapButton* m_bpButtonAddFolder;
    wxBitmapButton* m_bpButtonRemoveTopFolder;
    wxTextCtrl* m_txtCtrlDirectoryMain;
    wxButton* m_buttonSelectFolderMain;
    wxScrolledWindow* m_scrolledWinFolders;
    wxBoxSizer* bSizerFolders;
    wxStaticLine* m_staticline212;
    wxStaticText* m_staticText8;
    wxSpinCtrl* m_spinCtrlDelay;
    wxStaticLine* m_staticline211;
    wxStaticText* m_staticText6;
    wxTextCtrl* m_textCtrlCommand;
    wxStaticLine* m_staticline5;
    zen::BitmapTextButton* m_buttonStart;

    // Virtual event handlers, overide them in your derived class
    virtual void OnClose( wxCloseEvent& event ) { event.Skip(); }
    virtual void OnConfigNew( wxCommandEvent& event ) { event.Skip(); }
    virtual void OnConfigLoad( wxCommandEvent& event ) { event.Skip(); }
    virtual void OnConfigSave( wxCommandEvent& event ) { event.Skip(); }
    virtual void OnMenuQuit( wxCommandEvent& event ) { event.Skip(); }
    virtual void OnShowHelp( wxCommandEvent& event ) { event.Skip(); }
    virtual void OnMenuAbout( wxCommandEvent& event ) { event.Skip(); }
    virtual void OnAddFolder( wxCommandEvent& event ) { event.Skip(); }
    virtual void OnRemoveTopFolder( wxCommandEvent& event ) { event.Skip(); }
    virtual void OnStart( wxCommandEvent& event ) { event.Skip(); }


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

    FolderGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1, -1 ), long style = 0 );
    ~FolderGenerated();

};

#endif //__GUI_GENERATED_H__

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
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/statbmp.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/statline.h>
#include <wx/checkbox.h>
#include <wx/button.h>
#include <wx/dialog.h>

#include "zen/i18n.h"

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class PopupDialogGenerated
///////////////////////////////////////////////////////////////////////////////
class PopupDialogGenerated : public wxDialog
{
private:

protected:
    wxPanel* m_panel33;
    wxStaticBitmap* m_bitmapMsgType;
    wxStaticText* m_staticTextMain;
    wxRichTextCtrl* m_richTextDetail;
    wxStaticLine* m_staticline6;
    wxCheckBox* m_checkBoxCustom;
    wxBoxSizer* bSizerStdButtons;
    wxButton* m_buttonAccept;
    wxButton* m_buttonAccept2;
    wxButton* m_buttonDecline;
    wxButton* m_buttonCancel;

    // Virtual event handlers, override them in your derived class
    virtual void onClose( wxCloseEvent& event ) { event.Skip(); }
    virtual void onCheckBoxClick( wxCommandEvent& event ) { event.Skip(); }
    virtual void onButtonAccept( wxCommandEvent& event ) { event.Skip(); }
    virtual void onButtonAccept2( wxCommandEvent& event ) { event.Skip(); }
    virtual void onButtonDecline( wxCommandEvent& event ) { event.Skip(); }
    virtual void onCancel( wxCommandEvent& event ) { event.Skip(); }


public:

    PopupDialogGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("dummy"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

    ~PopupDialogGenerated();

};

///////////////////////////////////////////////////////////////////////////////
/// Class TooltipDlgGenerated
///////////////////////////////////////////////////////////////////////////////
class TooltipDlgGenerated : public wxDialog
{
private:

protected:

public:
    wxStaticBitmap* m_bitmapLeft;
    wxStaticText* m_staticTextMain;

    TooltipDlgGenerated( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE );

    ~TooltipDlgGenerated();

};


// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************
#include "tooltip.h"
#include <zen/zstring.h>
#include <wx/dialog.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
//#include <wx/statbmp.h>
#include <wx/settings.h>
#include <wx/app.h>
//#include "image_tools.h"
#include "bitmap_button.h"
#include "dc.h"
    #include <gtk/gtk.h>

using namespace zen;


namespace
{
const int TIP_WINDOW_OFFSET_DIP = 20;
}


class Tooltip::TooltipDlgGenerated : public wxDialog
{
public:
    TooltipDlgGenerated(wxWindow* parent) : //Suse Linux/X11: needs parent window, else there are z-order issues
        wxDialog(parent, wxID_ANY, L"" /*title*/, wxDefaultPosition, wxDefaultSize, wxSIMPLE_BORDER /*style*/)
        //wxSIMPLE_BORDER side effect: removes title bar on KDE
    {
        SetSizeHints(wxDefaultSize, wxDefaultSize);
        SetExtraStyle(this->GetExtraStyle() | wxWS_EX_TRANSIENT);
        SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));   //both required: on Ubuntu background is black, foreground white!
        SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT)); //

        wxBoxSizer* bSizer158 = new wxBoxSizer(wxHORIZONTAL);
        bitmapLeft_ = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);
        bSizer158->Add(bitmapLeft_, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

        staticTextMain_ = new wxStaticText(this, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, 0);
        bSizer158->Add(staticTextMain_, 0, wxALL | wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL, 5);

        SetSizer(bSizer158);

    }

    bool AcceptsFocus() const override { return false; } //any benefit?

    wxStaticText* staticTextMain_ = nullptr;
    wxStaticBitmap* bitmapLeft_   = nullptr;
};


void Tooltip::show(const wxString& text, wxPoint mousePos, const wxImage* img)
{
    if (!tipWindow_)
        tipWindow_ = new TooltipDlgGenerated(&parent_); //ownership passed to parent

    const wxImage& newImg = img ? *img : wxNullImage;

    const bool imgChanged = !newImg.IsSameAs(lastUsedImg_);
    const bool txtChanged = text != lastUsedText_;

    if (imgChanged)
    {
        lastUsedImg_ = newImg;
        setImage(*tipWindow_->bitmapLeft_, newImg);
        // tipWindow_->Refresh(); //needed if bitmap size changed!    ->???
    }

    if (txtChanged)
    {
        lastUsedText_ = text;
            tipWindow_->staticTextMain_->SetLabelText(text);

        tipWindow_->staticTextMain_->Wrap(dipToWxsize(600));
    }

    if (imgChanged || txtChanged)
        //tipWindow_->Dimensions(); -> apparently not needed!?
        tipWindow_->GetSizer()->SetSizeHints(tipWindow_); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //=> call wxWindow::Show() to "execute"
#endif

    const wxPoint newPos = mousePos + wxPoint(wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft ?
                                              - dipToWxsize(TIP_WINDOW_OFFSET_DIP) - tipWindow_->GetSize().GetWidth() :
                                              dipToWxsize(TIP_WINDOW_OFFSET_DIP),
                                              dipToWxsize(TIP_WINDOW_OFFSET_DIP));

    if (newPos != tipWindow_->GetScreenPosition())
        tipWindow_->Move(newPos);
    //caveat: possible endless loop! mouse pointer must NOT be within tipWindow!
    //else it will trigger a wxEVT_LEAVE_WINDOW on middle grid which will hide the window, causing the window to be shown again via this method, etc.

    if (!tipWindow_->IsShown())
        tipWindow_->Show();
}


void Tooltip::hide()
{
    if (tipWindow_)
    {
#if GTK_MAJOR_VERSION == 2 //the tooltip sometimes turns blank or is not shown again after it was hidden: e.g. drag-selection on middle grid
        //=> no such issues on GTK3!
        tipWindow_->Destroy(); //apply brute force:
        tipWindow_ = nullptr;  //
        lastUsedImg_ = wxNullImage;

#elif GTK_MAJOR_VERSION == 3
        tipWindow_->Hide();
#else
#error unknown GTK version!
#endif
    }
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "tooltip.h"
#include <wx/dialog.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/settings.h>
#include <wx/app.h>
#include <wx+/image_tools.h>

using namespace zen;


class Tooltip::TooltipDlgGenerated : public wxDialog
{
public:
    TooltipDlgGenerated(wxWindow* parent,
                        wxWindowID id = wxID_ANY,
                        const wxString& title = {},
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        long style = 0) : wxDialog(parent, id, title, pos, size, style)
    {
        //Suse Linux/X11: needs parent window, else there are z-order issues

        this->SetSizeHints(wxDefaultSize, wxDefaultSize);
        this->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));   //both required: on Ubuntu background is black, foreground white!
        this->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT)); //

        wxBoxSizer* bSizer158 = new wxBoxSizer(wxHORIZONTAL);
        bitmapLeft_ = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);
        bSizer158->Add(bitmapLeft_, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

        staticTextMain_ = new wxStaticText(this, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, 0);
        bSizer158->Add(staticTextMain_, 0, wxALL | wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL, 5);

        this->SetSizer(bSizer158);
        this->Layout();
        bSizer158->Fit(this);

    }

    wxStaticText* staticTextMain_;
    wxStaticBitmap* bitmapLeft_;
};


void Tooltip::show(const wxString& text, wxPoint mousePos, const wxBitmap* bmp)
{
    if (!tipWindow_)
        tipWindow_ = new TooltipDlgGenerated(&parent_); //ownership passed to parent

    const wxBitmap& newBmp = bmp ? *bmp : wxNullBitmap;

    if (!isEqual(tipWindow_->bitmapLeft_->GetBitmap(), newBmp))
    {
        tipWindow_->bitmapLeft_->SetBitmap(newBmp);
        tipWindow_->Refresh(); //needed if bitmap size changed!
    }

    if (text != tipWindow_->staticTextMain_->GetLabel())
    {
        tipWindow_->staticTextMain_->SetLabel(text);
        tipWindow_->staticTextMain_->Wrap(600);
    }

    tipWindow_->GetSizer()->SetSizeHints(tipWindow_); //~=Fit() + SetMinSize()
    //Linux: Fit() seems to be broken => this needs to be called EVERY time inside show, not only if text or bmp change

    const wxPoint newPos = wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft ?
                           mousePos - wxPoint(30 + tipWindow_->GetSize().GetWidth(), 0) :
                           mousePos + wxPoint(30, 0);

    if (newPos != tipWindow_->GetScreenPosition())
        tipWindow_->SetSize(newPos.x, newPos.y, wxDefaultCoord, wxDefaultCoord);
    //attention!!! possible endless loop: mouse pointer must NOT be within tipWindow!
    //else it will trigger a wxEVT_LEAVE_WINDOW on middle grid which will hide the window, causing the window to be shown again via this method, etc.

    if (!tipWindow_->IsShown())
        tipWindow_->Show();
}


void Tooltip::hide()
{
    if (tipWindow_)
    {
        //on wxGTK the tooltip is sometimes not shown again after it was hidden: e.g. drag-selection on middle grid
        tipWindow_->Destroy(); //apply brute force:
        tipWindow_ = nullptr;  //
    }
}

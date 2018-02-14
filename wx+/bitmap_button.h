// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef BITMAP_BUTTON_H_83415718945878341563415
#define BITMAP_BUTTON_H_83415718945878341563415

#include <wx/bmpbuttn.h>
#include "image_tools.h"


namespace zen
{
//zen::BitmapTextButton is identical to wxBitmapButton, but preserves the label via SetLabel(), which wxFormbuilder would ditch!
class BitmapTextButton : public wxBitmapButton
{
public:
    BitmapTextButton(wxWindow* parent,
                     wxWindowID id,
                     const wxString& label,
                     const wxPoint& pos           = wxDefaultPosition,
                     const wxSize& size           = wxDefaultSize,
                     long style                   = 0,
                     const wxValidator& validator = wxDefaultValidator,
                     const wxString& name         = wxButtonNameStr) :
        wxBitmapButton(parent, id, wxNullBitmap, pos, size, style | wxBU_AUTODRAW, validator, name) { SetLabel(label); }
};

void setBitmapTextLabel(wxBitmapButton& btn, const wxImage& img, const wxString& text, int gap = 5, int border = 5);

//set bitmap label flicker free:
void setImage(wxBitmapButton& button, const wxBitmap& bmp);











//################################### implementation ###################################
inline
void setBitmapTextLabel(wxBitmapButton& btn, const wxImage& img, const wxString& text, int gap, int border)
{
    assert(gap >= 0 && border >= 0);
    gap    = std::max(0, gap);
    border = std::max(0, border);

    wxImage dynImage = createImageFromText(text, btn.GetFont(), btn.GetForegroundColour());
    if (img.IsOk())
    {
        if (btn.GetLayoutDirection() != wxLayout_RightToLeft)
            dynImage = stackImages(img, dynImage, ImageStackLayout::HORIZONTAL, ImageStackAlignment::CENTER, gap);
        else
            dynImage = stackImages(dynImage, img, ImageStackLayout::HORIZONTAL, ImageStackAlignment::CENTER, gap);
    }

    //SetMinSize() instead of SetSize() is needed here for wxWindows layout determination to work corretly
    const int defaultHeight = wxButton::GetDefaultSize().GetHeight();
    btn.SetMinSize(wxSize(dynImage.GetWidth () + 2 * border,
                          std::max(dynImage.GetHeight() + 2 * border, defaultHeight)));

    btn.SetBitmapLabel(wxBitmap(dynImage));
    //SetLabel() calls confuse wxBitmapButton in the disabled state and it won't show the image! workaround:
    btn.SetBitmapDisabled(wxBitmap(dynImage.ConvertToDisabled()));
}


inline
void setImage(wxBitmapButton& button, const wxBitmap& bmp)
{
    if (!isEqual(button.GetBitmapLabel(), bmp))
    {
        button.SetBitmapLabel(bmp);

        //wxWidgets excels at screwing up consistently once again:
        //the first call to SetBitmapLabel() *implicitly* sets the disabled bitmap, too, subsequent calls, DON'T!
        button.SetBitmapDisabled(bmp.ConvertToDisabled()); //inefficiency: wxBitmap::ConvertToDisabled() implicitly converts to wxImage!
    }
}
}

#endif //BITMAP_BUTTON_H_83415718945878341563415

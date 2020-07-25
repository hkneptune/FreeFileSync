// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef BITMAP_BUTTON_H_83415718945878341563415
#define BITMAP_BUTTON_H_83415718945878341563415

#include <wx/bmpbuttn.h>
#include <wx/settings.h>
#include "image_tools.h"
#include "dc.h"


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
        wxBitmapButton(parent, id, wxNullBitmap, pos, size, style, validator, name)
    {
        SetLabel(label);
    }
};

//wxButton::SetBitmap() also supports "image + text", but screws up proper gap and border handling
void setBitmapTextLabel(wxBitmapButton& btn, const wxImage& img, const wxString& text, int gap = fastFromDIP(5), int border = fastFromDIP(5));

//set bitmap label flicker free:
void setImage(wxBitmapButton& button, const wxImage& bmp);

wxBitmap renderSelectedButton(const wxSize& sz);
wxBitmap renderPressedButton(const wxSize& sz);








//################################### implementation ###################################
inline
void setBitmapTextLabel(wxBitmapButton& btn, const wxImage& img, const wxString& text, int gap, int border)
{
    assert(gap >= 0 && border >= 0);
    gap    = std::max(0, gap);
    border = std::max(0, border);

    wxImage imgTxt = createImageFromText(text, btn.GetFont(), btn.GetForegroundColour());
    if (img.IsOk())
        imgTxt = btn.GetLayoutDirection() != wxLayout_RightToLeft ?
                 stackImages(img, imgTxt, ImageStackLayout::horizontal, ImageStackAlignment::center, gap) :
                 stackImages(imgTxt, img, ImageStackLayout::horizontal, ImageStackAlignment::center, gap);

    //SetMinSize() instead of SetSize() is needed here for wxWindows layout determination to work correctly
    const int defaultHeight = wxButton::GetDefaultSize().GetHeight();
    btn.SetMinSize({imgTxt.GetWidth () + 2 * border,
                    std::max(imgTxt.GetHeight() + 2 * border, defaultHeight)});

    btn.SetBitmapLabel(imgTxt);
    //SetLabel() calls confuse wxBitmapButton in the disabled state and it won't show the image! workaround:
    btn.SetBitmapDisabled(imgTxt.ConvertToDisabled());
}


inline
void setImage(wxBitmapButton& button, const wxImage& img)
{
    if (!img.IsOk())
    {
        button.SetBitmapLabel   (wxNullBitmap);
        button.SetBitmapDisabled(wxNullBitmap);
        return;
    }

    button.SetBitmapLabel(img);

    //wxWidgets excels at screwing up consistently once again:
    //the first call to SetBitmapLabel() *implicitly* sets the disabled bitmap, too, subsequent calls, DON'T!
    button.SetBitmapDisabled(img.ConvertToDisabled()); //inefficiency: wxBitmap::ConvertToDisabled() implicitly converts to wxImage!
}


inline
wxBitmap renderSelectedButton(const wxSize& sz)
{
    wxBitmap bmp(sz); //seems we don't need to pass 24-bit depth here even for high-contrast color schemes
    {
        wxMemoryDC dc(bmp);
        dc.SetBrush(wxColor(0xcc, 0xe4, 0xf8)); //light blue
        dc.SetPen  (wxColor(0x79, 0xbc, 0xed)); //medium blue
        dc.DrawRectangle(wxRect(bmp.GetSize()));
    }
    return bmp;
}


inline
wxBitmap renderPressedButton(const wxSize& sz)
{
    wxBitmap bmp(sz); //seems we don't need to pass 24-bit depth here even for high-contrast color schemes
    {
        //draw rectangle border with gradient
        const wxColor colFrom = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
        const wxColor colTo(0x11, 0x79, 0xfe); //light blue

        wxMemoryDC dc(bmp);
        dc.SetBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        wxRect rect(bmp.GetSize());

        const int borderSize = fastFromDIP(3);
        for (int i = 1 ; i <= borderSize; ++i)
        {
            const wxColor colGradient((colFrom.Red  () * (borderSize - i) + colTo.Red  () * i) / borderSize,
                                      (colFrom.Green() * (borderSize - i) + colTo.Green() * i) / borderSize,
                                      (colFrom.Blue () * (borderSize - i) + colTo.Blue () * i) / borderSize);
            dc.SetPen(colGradient);
            dc.DrawRectangle(rect);
            rect.Deflate(1);
        }
    }
    return bmp;
}
}

#endif //BITMAP_BUTTON_H_83415718945878341563415

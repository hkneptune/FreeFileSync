// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TOGGLE_BUTTON_H_8173024810574556
#define TOGGLE_BUTTON_H_8173024810574556

#include <wx/bmpbuttn.h>
#include <wx+/bitmap_button.h>


namespace zen
{
class ToggleButton : public wxBitmapButton
{
public:
    ToggleButton(wxWindow*          parent,
                 wxWindowID         id,
                 const wxBitmap&    bitmap,
                 const wxPoint&     pos = wxDefaultPosition,
                 const wxSize&      size = wxDefaultSize,
                 long               style = 0,
                 const wxValidator& validator = wxDefaultValidator,
                 const wxString&    name = wxButtonNameStr) : wxBitmapButton(parent, id, bitmap, pos, size, style, validator, name)
    {
        SetLayoutDirection(wxLayout_LeftToRight); //avoid mirroring RTL languages like Hebrew or Arabic
    }

    void init(const wxBitmap& bmpActive,
              const wxBitmap& bmpInactive);

    void setActive(bool value);
    bool isActive() const { return active_; }
    void toggle()         { setActive(!active_); }

private:
    bool active_ = false;
    wxBitmap bmpActive_;
    wxBitmap bmpInactive_;
};







//######################## implementation ########################
inline
void ToggleButton::init(const wxBitmap& bmpActive,
                        const wxBitmap& bmpInactive)
{
    bmpActive_   = bmpActive;
    bmpInactive_ = bmpInactive;

    setImage(*this, active_ ? bmpActive_ : bmpInactive_);
}


inline
void ToggleButton::setActive(bool value)
{
    if (active_ != value)
    {
        active_ = value;
        setImage(*this, active_ ? bmpActive_ : bmpInactive_);
    }
}
}

#endif //TOGGLE_BUTTON_H_8173024810574556

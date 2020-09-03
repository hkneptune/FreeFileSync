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
    //wxBitmapButton constructor
    ToggleButton(wxWindow*          parent,
                 wxWindowID         id,
                 const wxBitmap&    bitmap,
                 const wxPoint&     pos = wxDefaultPosition,
                 const wxSize&      size = wxDefaultSize,
                 long               style = 0,
                 const wxValidator& validator = wxDefaultValidator,
                 const wxString&    name = wxASCII_STR(wxButtonNameStr)) :
        wxBitmapButton(parent, id, bitmap, pos, size, style, validator, name) {}

    //wxButton constructor
    ToggleButton(wxWindow*          parent,
                 wxWindowID         id,
                 const wxString&    label,
                 const wxPoint&     pos = wxDefaultPosition,
                 const wxSize&      size = wxDefaultSize,
                 long               style = 0,
                 const wxValidator& validator = wxDefaultValidator,
                 const wxString&    name = wxASCII_STR(wxButtonNameStr)) :
        wxBitmapButton(parent, id, wxNullBitmap, pos, size, style, validator, name)
    {
        SetLabel(label);
    }

    void init(const wxImage& imgActive,
              const wxImage& imgInactive);

    void setActive(bool value);
    bool isActive() const { return active_; }
    void toggle()         { setActive(!active_); }

private:
    bool active_ = false;
    wxImage imgActive_;
    wxImage imgInactive_;
};







//######################## implementation ########################
inline
void ToggleButton::init(const wxImage& imgActive,
                        const wxImage& imgInactive)
{
    imgActive_   = imgActive;
    imgInactive_ = imgInactive;

    setImage(*this, active_ ? imgActive_ : imgInactive_);
}


inline
void ToggleButton::setActive(bool value)
{
    if (active_ != value)
    {
        active_ = value;
        setImage(*this, active_ ? imgActive_ : imgInactive_);
    }
}
}

#endif //TOGGLE_BUTTON_H_8173024810574556

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FONT_SIZE_H_23849632846734343234532
#define FONT_SIZE_H_23849632846734343234532

#include <zen/basic_math.h>
#include <wx/window.h>
#include <zen/scope_guard.h>
#include "dc.h"


namespace zen
{
//set portable font size in multiples of the operating system's default font size
void setRelativeFontSize(wxWindow& control, double factor);
void setMainInstructionFont(wxWindow& control); //following Windows/Gnome/OS X guidelines










//###################### implementation #####################
inline
void setRelativeFontSize(wxWindow& control, double factor)
{
    wxFont font = control.GetFont();
    font.SetPointSize(numeric::round(wxNORMAL_FONT->GetPointSize() * factor));
    control.SetFont(font);
}


inline
void setMainInstructionFont(wxWindow& control)
{
    wxFont font = control.GetFont();
    font.SetPointSize(numeric::round(wxNORMAL_FONT->GetPointSize() * 12.0 / 11));
    font.SetWeight(wxFONTWEIGHT_BOLD);

    control.SetFont(font);
}
}

#endif //FONT_SIZE_H_23849632846734343234532

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef NO_FLICKER_H_893421590321532
#define NO_FLICKER_H_893421590321532

#include <wx/textctrl.h>
#include <wx/stattext.h>


namespace zen
{
inline
void setText(wxTextCtrl& control, const wxString& newText, bool* additionalLayoutChange = nullptr)
{
    const wxString& label = control.GetValue(); //perf: don't call twice!
    if (additionalLayoutChange && !*additionalLayoutChange) //never revert from true to false!
        *additionalLayoutChange = label.length() != newText.length(); //avoid screen flicker: update layout only when necessary

    if (label != newText)
        control.ChangeValue(newText);
}

inline
void setText(wxStaticText& control, wxString newText, bool* additionalLayoutChange = nullptr)
{

    const wxString& label = control.GetLabel(); //perf: don't call twice!
    if (additionalLayoutChange && !*additionalLayoutChange)
        *additionalLayoutChange = label.length() != newText.length(); //avoid screen flicker: update layout only when necessary

    if (label != newText)
        control.SetLabel(newText);
}
}

#endif //NO_FLICKER_H_893421590321532

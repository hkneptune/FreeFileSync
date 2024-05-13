// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef WINDOW_LAYOUT_H_23849632846734343234532
#define WINDOW_LAYOUT_H_23849632846734343234532

//#include <zen/basic_math.h>
//#include <wx/window.h>
#include <wx/spinctrl.h>
#include <zen/scope_guard.h>
    #include <gtk/gtk.h>
#include "dc.h"


namespace zen
{
//set portable font size in multiples of the operating system's default font size
void setRelativeFontSize(wxWindow& control, double factor);
void setMainInstructionFont(wxWindow& control); //following Windows/Gnome/OS X guidelines

void setDefaultWidth(wxSpinCtrl& m_spinCtrl);








//###################### implementation #####################
inline
void setRelativeFontSize(wxWindow& control, double factor)
{
    wxFont font = control.GetFont();
    font.SetPointSize(std::round(wxNORMAL_FONT->GetPointSize() * factor));
    control.SetFont(font);
}


inline
void setMainInstructionFont(wxWindow& control)
{
    wxFont font = control.GetFont();
    font.SetPointSize(std::round(wxNORMAL_FONT->GetPointSize() * 12.0 / 11));
    font.SetWeight(wxFONTWEIGHT_BOLD);

    control.SetFont(font);
}


inline
void setDefaultWidth(wxSpinCtrl& m_spinCtrl)
{
#ifdef __WXGTK3__
    //there's no way to set width using GTK's CSS! =>
    m_spinCtrl.InvalidateBestSize();
    ::gtk_entry_set_width_chars(GTK_ENTRY(m_spinCtrl.m_widget), 3);

#if 0 //apparently not needed!?
    if (::gtk_check_version(3, 12, 0) == NULL)
        ::gtk_entry_set_max_width_chars(GTK_ENTRY(m_spinCtrl.m_widget), 3);
#endif

    //get rid of excessive default width on old GTK3 3.14 (Debian);
    //gtk_entry_set_width_chars() not working => mitigate
    m_spinCtrl.SetMinSize({dipToWxsize(100), -1}); //must be wider than gtk_entry_set_width_chars(), or it breaks newer GTK e.g. 3.22!

#if 0 //generic property syntax:
    GValue bval = G_VALUE_INIT;
    ::g_value_init(&bval, G_TYPE_BOOLEAN);
    ::g_value_set_boolean(&bval, false);
    ZEN_ON_SCOPE_EXIT(::g_value_unset(&bval));
    ::g_object_set_property(G_OBJECT(m_spinCtrl.m_widget), "visibility", &bval);
#endif
#else
    m_spinCtrl.SetMinSize({dipToWxsize(70), -1});
#endif

}
}

#endif //WINDOW_LAYOUT_H_23849632846734343234532

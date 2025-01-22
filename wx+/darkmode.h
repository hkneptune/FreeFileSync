// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef DARKMODE_H_754298057018
#define DARKMODE_H_754298057018

#include <zen/file_error.h>
#include <wx/app.h>


namespace zen
{
bool darkModeAvailable();

//support not only "dark mode" but dark themes in general
using ColorTheme = wxApp::Appearance; //why reinvent the wheel?

void colorThemeInit(wxApp& app, ColorTheme colTheme); //throw FileError
void colorThemeCleanup();

bool equalAppearance(ColorTheme colTheme1, ColorTheme colTheme2);
void changeColorTheme(ColorTheme colTheme); //throw FileError
}

#endif //DARKMODE_H_754298057018

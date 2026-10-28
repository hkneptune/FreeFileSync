// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************
#pragma once

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

DEFINE_NEW_FILE_ERROR(ErrorRestartRequired)
void changeColorTheme(ColorTheme colTheme); //throw ErrorRestartRequired, FileError
}

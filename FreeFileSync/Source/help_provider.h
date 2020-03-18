// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef HELP_PROVIDER_H_85930427583421563126
#define HELP_PROVIDER_H_85930427583421563126

#include <wx/utils.h>


namespace fff
{
inline
void displayHelpEntry(const wxString& topic, wxWindow* parent)
{
    wxLaunchDefaultBrowser(L"https://freefilesync.org/manual.php?topic=" + topic);
}
}

#endif //HELP_PROVIDER_H_85930427583421563126

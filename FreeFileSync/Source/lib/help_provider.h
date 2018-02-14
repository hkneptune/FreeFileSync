// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef HELP_PROVIDER_H_85930427583421563126
#define HELP_PROVIDER_H_85930427583421563126

#if 1
namespace fff
{
inline void displayHelpEntry(const wxString& topic, wxWindow* parent) { wxLaunchDefaultBrowser(L"https://www.freefilesync.org/manual.php?topic=" + topic); }
inline void uninitializeHelp() {}
}

#else
#include <zen/globals.h>
#include <wx+/http.h>
#include "ffs_paths.h"

    #include <wx/html/helpctrl.h>


namespace fff
{
void displayHelpEntry(const wxString& topic, wxWindow* parent);
void uninitializeHelp(); //clean up gracefully during app shutdown: leaving this up to static destruction crashes on Win 8.1!






//######################## implementation ########################
namespace impl
{
}


inline
void displayHelpEntry(const wxString& topic, wxWindow* parent)
{
    if (internetIsAlive()) //noexcept
        wxLaunchDefaultBrowser(L"https://www.freefilesync.org/manual.php?topic=" + topic);
    else
        -> what if FFS is blocked, but the web browser would have internet access??
    {
        const wxString section = L"html/" + topic + L".html";
        wxHtmlModalHelp dlg(parent, utfTo<wxString>(zen::getResourceDirPf()) + L"Help/FreeFileSync.hhp", section,
                            wxHF_DEFAULT_STYLE | wxHF_DIALOG | wxHF_MODAL | wxHF_MERGE_BOOKS);
            (void)dlg;
            //-> solves modal help craziness on OSX!
            //-> Suse Linux: avoids program hang on exit if user closed help parent dialog before the help dialog itself was closed (why is this even possible???)
            //               avoids ESC key not being recognized by help dialog (but by parent dialog instead)

        }
}


inline
void uninitializeHelp()
{
}
}
#endif

#endif //HELP_PROVIDER_H_85930427583421563126

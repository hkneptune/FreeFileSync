// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef APP_MAIN_H_08215601837818347575856
#define APP_MAIN_H_08215601837818347575856

#include <wx/window.h>
#include <wx/app.h>


namespace zen
{
//just some wrapper around a global variable representing the (logical) main application window
void setMainWindow(wxWindow* window); //set main window and enable "exit on frame delete"
bool mainWindowWasSet();






//######################## implementation ########################
namespace impl
{
inline
bool& refMainWndStatus()
{
    static bool status = false; //external linkage!
    return status;
}
}


inline
void setMainWindow(wxWindow* window)
{
    wxTheApp->SetTopWindow(window);
    wxTheApp->SetExitOnFrameDelete(true);

    impl::refMainWndStatus() = true;
}

inline bool mainWindowWasSet() { return impl::refMainWndStatus(); }
}

#endif //APP_MAIN_H_08215601837818347575856

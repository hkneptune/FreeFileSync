// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef APPLICATION_H_081568741942010985702395
#define APPLICATION_H_081568741942010985702395

#include <vector>
#include <zen/zstring.h>
#include <wx/app.h>
#include "return_codes.h"


namespace fff //avoid name clash with "int ffs()" for fuck's sake! (maxOS, Linux issue only: <string> internally includes <strings.h>, WTF!)
{
class Application : public wxApp
{
private:
    bool OnInit() override;
    int  OnRun () override;
    int  OnExit() override;
    bool OnExceptionInMainLoop() override { throw; } //just re-throw and avoid display of additional messagebox: it will be caught in OnRun()
    void OnUnhandledException () override { throw; } //just re-throw and avoid display of additional messagebox
    wxLayoutDirection GetLayoutDirection() const override;
    void onEnterEventLoop(wxEvent& event);
    void onQueryEndSession(wxEvent& event);
    void launch(const std::vector<Zstring>& commandArgs);

    FfsReturnCode returnCode_ = FFS_RC_SUCCESS;
};
}

#endif //APPLICATION_H_081568741942010985702395

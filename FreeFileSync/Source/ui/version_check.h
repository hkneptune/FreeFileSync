// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef VERSION_CHECK_H_324872374893274983275
#define VERSION_CHECK_H_324872374893274983275

#include <wx/window.h>
#include <zen/stl_tools.h>


namespace fff
{
bool haveNewerVersionOnline(const std::string& onlineVersion);
//----------------------------------------------------------------------------
bool automaticUpdateCheckDue(time_t lastUpdateCheck);

struct UpdateCheckResultPrep;
struct UpdateCheckResult;

//run on main thread:
zen::SharedRef<const UpdateCheckResultPrep> automaticUpdateCheckPrepare(wxWindow& parent);
//run on worker thread: (long-running part of the check)
zen::SharedRef<const UpdateCheckResult> automaticUpdateCheckRunAsync(const UpdateCheckResultPrep& resultPrep);
//run on main thread:
void automaticUpdateCheckEval(wxWindow& parent, time_t& lastUpdateCheck, std::string& lastOnlineVersion, const UpdateCheckResult& result);
//----------------------------------------------------------------------------
//call from main thread:
void checkForUpdateNow(wxWindow& parent, std::string& lastOnlineVersion);
//----------------------------------------------------------------------------
}

#endif //VERSION_CHECK_H_324872374893274983275

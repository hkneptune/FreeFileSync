// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef VERSION_CHECK_H_324872374893274983275
#define VERSION_CHECK_H_324872374893274983275

#include <functional>
#include <memory>
#include <wx/window.h>


namespace fff
{
bool updateCheckActive (time_t  lastUpdateCheck);
void disableUpdateCheck(time_t& lastUpdateCheck);
bool haveNewerVersionOnline(const std::string& onlineVersion);
//----------------------------------------------------------------------------
//periodic update check:
bool shouldRunAutomaticUpdateCheck(time_t lastUpdateCheck);

struct UpdateCheckResultPrep;
struct UpdateCheckResult;

//run on main thread:
std::shared_ptr<UpdateCheckResultPrep> automaticUpdateCheckPrepare();
//run on worker thread: (long-running part of the check)
std::shared_ptr<UpdateCheckResult> automaticUpdateCheckRunAsync(const UpdateCheckResultPrep* resultPrep);
//run on main thread:
void automaticUpdateCheckEval(wxWindow* parent, time_t& lastUpdateCheck, std::string& lastOnlineVersion,
                              const UpdateCheckResult* asyncResult);
//----------------------------------------------------------------------------
//call from main thread:
void checkForUpdateNow(wxWindow* parent, std::string& lastOnlineVersion);
//----------------------------------------------------------------------------
}

#endif //VERSION_CHECK_H_324872374893274983275

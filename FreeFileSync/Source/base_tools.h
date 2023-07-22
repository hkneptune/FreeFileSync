// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STRUCTURE_TOOLS_H_7823097420397434
#define STRUCTURE_TOOLS_H_7823097420397434

#include "base/structures.h"
#include "base/process_callback.h"
#include "config.h"


namespace fff
{
//convert "ignoreTimeShiftMinutes" into compact format:
std::vector<unsigned int> fromTimeShiftPhrase(const std::wstring_view timeShiftPhrase);
std::wstring              toTimeShiftPhrase  (const std::vector<unsigned int>& ignoreTimeShiftMinutes);

//inform about (important) non-default global settings related to comparison and synchronization
void logNonDefaultSettings(const XmlGlobalSettings& currentSettings, PhaseCallback& callback);

//facilitate drag & drop config merge:
MainConfiguration merge(const std::vector<MainConfiguration>& mainCfgs);
}

#endif //STRUCTURE_TOOLS_H_7823097420397434

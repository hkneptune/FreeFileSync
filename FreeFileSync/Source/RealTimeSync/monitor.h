// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef MONITOR_H_345087425834253425
#define MONITOR_H_345087425834253425

#include <chrono>
#include <functional>
#include <zen/zstring.h>


namespace rts
{
void monitorDirectories(const std::vector<Zstring>& folderPathPhrases,
                        //non-formatted paths that yet require call to getFormattedDirectoryName(); empty directories must be checked by caller!
                        std::chrono::seconds delay,
                        const std::function<void(const Zstring& changedItemPath, const std::wstring& actionName)>& executeExternalCommand,
                        const std::function<void(const Zstring* missingFolderPath)>& requestUiUpdate, //either waiting for change notifications or at least one folder is missing
                        const std::function<void(const std::wstring& msg         )>& reportError, //automatically retries after return!
                        std::chrono::milliseconds cbInterval);
}

#endif //MONITOR_H_345087425834253425

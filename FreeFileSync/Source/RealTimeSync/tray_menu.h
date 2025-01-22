// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TRAY_MENU_H_3967857420987534253245
#define TRAY_MENU_H_3967857420987534253245

#include <wx/string.h>
#include "config.h"


namespace rts
{
enum class CancelReason
{
    requestGui,
    requestExit
};
CancelReason runFolderMonitor(const FfsRealConfig& config, const wxString& jobname); //jobname may be empty
}

#endif //TRAY_MENU_H_3967857420987534253245

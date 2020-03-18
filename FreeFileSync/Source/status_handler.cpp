// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "status_handler.h"
#include <chrono>
#include <zen/basic_math.h>


namespace
{
std::chrono::steady_clock::time_point lastExec;
}


bool fff::uiUpdateDue(bool force)
{
    const auto now = std::chrono::steady_clock::now();

    if (now >= lastExec + UI_UPDATE_INTERVAL || force)
    {
        lastExec = now;
        return true;
    }
    return false;
}

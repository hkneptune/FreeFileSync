// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef RETURN_CODES_H_81307482137054156
#define RETURN_CODES_H_81307482137054156

#include <zen/i18n.h>


namespace fff
{
enum class FfsExitCode //as returned on process exit
{
    success = 0,
    warning,
    error,
    cancelled,
    exception,
};


inline
void raiseExitCode(FfsExitCode& rc, FfsExitCode rcProposed)
{
    if (rc < rcProposed)
        rc = rcProposed;
}


enum class TaskResult
{
    success,
    warning,
    error,
    cancelled,
};


inline
std::wstring getSyncResultLabel(TaskResult syncResult)
{
    switch (syncResult)
    {
        //*INDENT-OFF*
        case TaskResult::success: return _("Completed successfully");
        case TaskResult::warning: return _("Completed with warnings");
        case TaskResult::error:   return _("Completed with errors");
        case TaskResult::cancelled: return _("Stopped");
        //*INDENT-ON*
    }
    assert(false);
    return std::wstring();
}
}

#endif //RETURN_CODES_H_81307482137054156

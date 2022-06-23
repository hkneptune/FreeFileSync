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
    aborted,
    exception,
};


inline
void raiseExitCode(FfsExitCode& rc, FfsExitCode rcProposed)
{
    if (rc < rcProposed)
        rc = rcProposed;
}


enum class SyncResult
{
    finishedSuccess,
    finishedWarning,
    finishedError,
    aborted,
};


inline
std::wstring getSyncResultLabel(SyncResult syncResult)
{
    switch (syncResult)
    {
        //*INDENT-OFF*
        case SyncResult::finishedSuccess: return _("Completed successfully");
        case SyncResult::finishedWarning: return _("Completed with warnings");
        case SyncResult::finishedError:   return _("Completed with errors");
        case SyncResult::aborted:         return _("Stopped");
        //*INDENT-ON*
    }
    assert(false);
    return std::wstring();
}
}

#endif //RETURN_CODES_H_81307482137054156

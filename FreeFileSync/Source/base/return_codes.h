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
enum FfsReturnCode //as returned after process exit
{
    FFS_RC_SUCCESS = 0,
    FFS_RC_FINISHED_WITH_WARNINGS,
    FFS_RC_FINISHED_WITH_ERRORS,
    FFS_RC_ABORTED,
    FFS_RC_EXCEPTION,
};


inline
void raiseReturnCode(FfsReturnCode& rc, FfsReturnCode rcProposed)
{
    if (rc < rcProposed)
        rc = rcProposed;
}


enum class SyncResult
{
    FINISHED_WITH_SUCCESS,
    FINISHED_WITH_WARNINGS,
    FINISHED_WITH_ERROR,
    ABORTED,
};


inline
FfsReturnCode mapToReturnCode(SyncResult syncStatus)
{
    switch (syncStatus)
    {
        case SyncResult::FINISHED_WITH_SUCCESS:
            return FFS_RC_SUCCESS;
        case SyncResult::FINISHED_WITH_WARNINGS:
            return FFS_RC_FINISHED_WITH_WARNINGS;
        case SyncResult::FINISHED_WITH_ERROR:
            return FFS_RC_FINISHED_WITH_ERRORS;
        case SyncResult::ABORTED:
            return FFS_RC_ABORTED;
    }
    assert(false);
    return FFS_RC_ABORTED;
}


inline
std::wstring getFinalStatusLabel(SyncResult finalStatus)
{
    switch (finalStatus)
    {
        case SyncResult::FINISHED_WITH_SUCCESS:
            return  _("Completed successfully");
        case SyncResult::FINISHED_WITH_WARNINGS:
            return _("Completed with warnings");
        case SyncResult::FINISHED_WITH_ERROR:
            return _("Completed with errors");
        case SyncResult::ABORTED:
            return _("Stopped");
    }
    assert(false);
    return std::wstring();
}
}

#endif //RETURN_CODES_H_81307482137054156

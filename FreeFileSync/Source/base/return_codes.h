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
    FFS_RC_WARNING,
    FFS_RC_ERROR,
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
    finishedSuccess,
    finishedWarning,
    finishedError,
    aborted,
};


inline
FfsReturnCode mapToReturnCode(SyncResult syncStatus)
{
    switch (syncStatus)
    {
        case SyncResult::finishedSuccess:
            return FFS_RC_SUCCESS;
        case SyncResult::finishedWarning:
            return FFS_RC_WARNING;
        case SyncResult::finishedError:
            return FFS_RC_ERROR;
        case SyncResult::aborted:
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
        case SyncResult::finishedSuccess:
            return  _("Completed successfully");
        case SyncResult::finishedWarning:
            return _("Completed with warnings");
        case SyncResult::finishedError:
            return _("Completed with errors");
        case SyncResult::aborted:
            return _("Stopped");
    }
    assert(false);
    return std::wstring();
}
}

#endif //RETURN_CODES_H_81307482137054156

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef RETURN_CODES_H_81307482137054156
#define RETURN_CODES_H_81307482137054156

namespace fff
{
enum FfsReturnCode
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
}

#endif //RETURN_CODES_H_81307482137054156

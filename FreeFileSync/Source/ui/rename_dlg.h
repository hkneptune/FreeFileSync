// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef RENAME_DLG_H_23487982347324
#define RENAME_DLG_H_23487982347324

#include <wx+/popup_dlg.h>


namespace fff
{
zen::ConfirmationButton showRenameDialog(wxWindow* parent,
                                         const std::vector<Zstring>& fileNamesOld,
                                         std::vector<Zstring>& fileNamesNew);
}

#endif //RENAME_DLG_H_23487982347324

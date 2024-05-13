// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ABSTRACT_FOLDER_PICKER_HEADER_324872346895690
#define ABSTRACT_FOLDER_PICKER_HEADER_324872346895690

//#include <wx/window.h>
#include <wx+/popup_dlg.h>
#include "../afs/abstract.h"


namespace fff
{
zen::ConfirmationButton showAbstractFolderPicker(wxWindow* parent, AbstractPath& folderPath);
}

#endif //ABSTRACT_FOLDER_PICKER_HEADER_324872346895690

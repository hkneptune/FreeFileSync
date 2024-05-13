// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef BATCH_CONFIG_H_3921674832168945
#define BATCH_CONFIG_H_3921674832168945

//#include <wx/window.h>
#include <wx+/popup_dlg.h>
#include "../config.h"


namespace fff
{
//show and let user customize batch settings (without saving)
zen::ConfirmationButton showBatchConfigDialog(wxWindow* parent,
                                              BatchExclusiveConfig& batchExCfg,
                                              bool& ignoreErrors);
}

#endif //BATCH_CONFIG_H_3921674832168945

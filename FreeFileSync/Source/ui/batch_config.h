// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef BATCH_CONFIG_H_3921674832168945
#define BATCH_CONFIG_H_3921674832168945

#include <wx/window.h>
#include "../lib/process_xml.h"


namespace fff
{
struct ReturnBatchConfig
{
    enum ButtonPressed
    {
        BUTTON_CANCEL,
        BUTTON_SAVE_AS
    };
};


//show and let user customize batch settings (without saving)
ReturnBatchConfig::ButtonPressed showBatchConfigDialog(wxWindow* parent,
                                                       BatchExclusiveConfig& batchExCfg,
                                                       bool& ignoreErrors);
}

#endif //BATCH_CONFIG_H_3921674832168945

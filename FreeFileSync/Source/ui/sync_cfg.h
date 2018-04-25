// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SYNC_CFG_H_31289470134253425
#define SYNC_CFG_H_31289470134253425

#include <wx/window.h>
#include "../structures.h"


namespace fff
{
struct ReturnSyncConfig
{
    enum ButtonPressed
    {
        BUTTON_CANCEL,
        BUTTON_OKAY
    };
};

enum class SyncConfigPanel
{
    COMPARISON = 0, //
    FILTER     = 1, //used as zero-based notebook page index!
    SYNC       = 2, //
};

struct MiscSyncConfig
{
    bool ignoreErrors = false;
    size_t automaticRetryCount = 0;
    size_t automaticRetryDelay = 0;
    Zstring postSyncCommand;
    PostSyncCondition postSyncCondition = PostSyncCondition::COMPLETION;
    std::vector<Zstring> commandHistory;
};

struct GlobalPairConfig
{
    CompConfig   cmpConfig;
    SyncConfig   syncCfg;
    FilterConfig filter;
    MiscSyncConfig miscCfg;
};


ReturnSyncConfig::ButtonPressed showSyncConfigDlg(wxWindow* parent,
                                                  SyncConfigPanel panelToShow,
                                                  int localPairIndexToShow, //< 0 to show global config

                                                  GlobalPairConfig&             globalPairCfg,
                                                  std::vector<LocalPairConfig>& localPairConfig,

                                                  size_t commandHistoryMax);
}

#endif //SYNC_CFG_H_31289470134253425

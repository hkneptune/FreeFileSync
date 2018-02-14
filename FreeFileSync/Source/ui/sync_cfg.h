// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SYNC_CFG_H_31289470134253425
#define SYNC_CFG_H_31289470134253425

#include <wx/window.h>
#include "../lib/process_xml.h"


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

struct LocalPairConfig
{
    Zstring folderPairName; //read-only!
    std::shared_ptr<const CompConfig> altCmpConfig;  //optional
    std::shared_ptr<const SyncConfig> altSyncConfig; //
    FilterConfig localFilter;
};


ReturnSyncConfig::ButtonPressed showSyncConfigDlg(wxWindow* parent,
                                                  SyncConfigPanel panelToShow,
                                                  int localPairIndexToShow, //< 0 to show global config

                                                  std::vector<LocalPairConfig>& folderPairConfig,

                                                  CompConfig&   globalCmpConfig,
                                                  SyncConfig&   globalSyncCfg,
                                                  FilterConfig& globalFilter,

                                                  bool& ignoreErrors,
                                                  Zstring& postSyncCommand,
                                                  PostSyncCondition& postSyncCondition,
                                                  std::vector<Zstring>& commandHistory,

                                                  size_t commandHistoryMax);
}

#endif //SYNC_CFG_H_31289470134253425

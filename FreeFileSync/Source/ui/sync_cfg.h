// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SYNC_CFG_H_31289470134253425
#define SYNC_CFG_H_31289470134253425

//#include <wx/window.h>
#include <wx+/popup_dlg.h>
#include "../base/structures.h"


namespace fff
{
enum class SyncConfigPanel
{
    compare = 0, //used as zero-based notebook page index!
    filter,
    sync,
};

struct MiscSyncConfig
{
    std::map<AfsDevice, size_t> deviceParallelOps;
    bool ignoreErrors = false;
    size_t autoRetryCount = 0;
    std::chrono::seconds autoRetryDelay{0};

    Zstring postSyncCommand;
    PostSyncCondition postSyncCondition = PostSyncCondition::completion;

    Zstring altLogFolderPathPhrase;

    std::string emailNotifyAddress;
    ResultsNotification emailNotifyCondition = ResultsNotification::always;

    std::wstring notes;
};

struct GlobalPairConfig
{
    CompConfig     cmpCfg;
    SyncConfig     syncCfg;
    FilterConfig   filter;
    MiscSyncConfig miscCfg;
};


zen::ConfirmationButton showSyncConfigDlg(wxWindow* parent,
                                          SyncConfigPanel panelToShow,
                                          int localPairIndexToShow, //< 0 to show global config
                                          bool showMultipleCfgs,

                                          GlobalPairConfig&             globalPairCfg,
                                          std::vector<LocalPairConfig>& localPairCfg,

                                          FilterConfig& defaultFilter,
                                          std::vector<Zstring>& versioningFolderHistory, Zstring& versioningFolderLastSelected,
                                          std::vector<Zstring>& logFolderHistory, Zstring& logFolderLastSelected, const Zstring& globalLogFolderPhrase,
                                          size_t folderHistoryMax, Zstring& sftpKeyFileLastSelected,
                                          std::vector<Zstring>& emailHistory,   size_t emailHistoryMax,
                                          std::vector<Zstring>& commandHistory, size_t commandHistoryMax);
}

#endif //SYNC_CFG_H_31289470134253425

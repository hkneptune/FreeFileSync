// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SMALL_DLGS_H_8321790875018750245
#define SMALL_DLGS_H_8321790875018750245

#include <span>
#include <wx/window.h>
#include <wx+/popup_dlg.h>
#include "../base/synchronization.h"
#include "../config.h"


namespace fff
{
//parent window, optional: support correct dialog placement above parent on multiple monitor systems

void showAboutDialog(wxWindow* parent);

zen::ConfirmationButton showCopyToDialog(wxWindow* parent,
                                         std::span<const FileSystemObject* const> rowsOnLeft,
                                         std::span<const FileSystemObject* const> rowsOnRight,
                                         Zstring& targetFolderPath, Zstring& targetFolderLastSelected,
                                         std::vector<Zstring>& folderPathHistory, size_t folderPathHistoryMax,
                                         Zstring& sftpKeyFileLastSelected,
                                         bool& keepRelPaths,
                                         bool& overwriteIfExists);

zen::ConfirmationButton showDeleteDialog(wxWindow* parent,
                                         std::span<const FileSystemObject* const> rowsOnLeft,
                                         std::span<const FileSystemObject* const> rowsOnRight,
                                         bool& useRecycleBin);

zen::ConfirmationButton showSyncConfirmationDlg(wxWindow* parent,
                                                bool syncSelection,
                                                std::optional<SyncVariant> syncVar,
                                                const SyncStatistics& statistics,
                                                bool& dontShowAgain);

zen::ConfirmationButton showOptionsDlg(wxWindow* parent, XmlGlobalSettings& globalCfg);

zen::ConfirmationButton showSelectTimespanDlg(wxWindow* parent, time_t& timeFrom, time_t& timeTo);

zen::ConfirmationButton showCfgHighlightDlg(wxWindow* parent, int& cfgHistSyncOverdueDays);

zen::ConfirmationButton showCloudSetupDialog(wxWindow* parent, Zstring& folderPathPhrase, Zstring& sftpKeyFileLastSelected,
                                             size_t& parallelOps, const std::wstring* parallelOpsDisabledReason /*optional: disable control + show text*/);

enum class ActivationDlgButton
{
    cancel,
    activateOnline,
    activateOffline,
};
ActivationDlgButton showActivationDialog(wxWindow* parent, const std::wstring& lastErrorMsg, const std::wstring& manualActivationUrl, std::wstring& manualActivationKey);


class DownloadProgressWindow //temporary progress info => life-time: stack
{
public:
    DownloadProgressWindow(wxWindow* parent, int64_t fileSizeTotal);
    ~DownloadProgressWindow();

    struct CancelPressed {};
    void notifyNewFile(const Zstring& filePath);
    void notifyProgress(int64_t delta);
    void requestUiUpdate(); //throw CancelPressed

private:
    class Impl;
    Impl* const pimpl_;
};


}

#endif //SMALL_DLGS_H_8321790875018750245

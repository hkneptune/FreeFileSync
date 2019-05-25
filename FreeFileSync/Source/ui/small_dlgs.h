// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SMALL_DLGS_H_8321790875018750245
#define SMALL_DLGS_H_8321790875018750245

#include <wx/window.h>
#include "../base/config.h"
#include "../base/synchronization.h"


namespace fff
{
//parent window, optional: support correct dialog placement above parent on multiple monitor systems

struct ReturnSmallDlg
{
    enum ButtonPressed
    {
        BUTTON_CANCEL,
        BUTTON_OKAY = 1
    };
};

void showAboutDialog(wxWindow* parent);

ReturnSmallDlg::ButtonPressed showCopyToDialog(wxWindow* parent,
                                               std::span<const FileSystemObject* const> rowsOnLeft,
                                               std::span<const FileSystemObject* const> rowsOnRight,
                                               Zstring& lastUsedPath,
                                               std::vector<Zstring>& folderPathHistory,
                                               size_t historySizeMax,
                                               bool& keepRelPaths,
                                               bool& overwriteIfExists);

ReturnSmallDlg::ButtonPressed showDeleteDialog(wxWindow* parent,
                                               std::span<const FileSystemObject* const> rowsOnLeft,
                                               std::span<const FileSystemObject* const> rowsOnRight,
                                               bool& useRecycleBin);

ReturnSmallDlg::ButtonPressed showSyncConfirmationDlg(wxWindow* parent,
                                                      bool syncSelection,
                                                      const wxString& variantName,
                                                      const SyncStatistics& statistics,
                                                      bool& dontShowAgain);

ReturnSmallDlg::ButtonPressed showOptionsDlg(wxWindow* parent, XmlGlobalSettings& globalCfg);

ReturnSmallDlg::ButtonPressed showSelectTimespanDlg(wxWindow* parent, time_t& timeFrom, time_t& timeTo);

ReturnSmallDlg::ButtonPressed showCfgHighlightDlg(wxWindow* parent, int& cfgHistSyncOverdueDays);

ReturnSmallDlg::ButtonPressed showCloudSetupDialog(wxWindow* parent, Zstring& folderPathPhrase,
                                                   size_t& parallelOps, const std::wstring* parallelOpsDisabledReason /*optional: disable control + show text*/);

enum class ReturnActivationDlg
{
    CANCEL,
    ACTIVATE_ONLINE,
    ACTIVATE_OFFLINE,
};
ReturnActivationDlg showActivationDialog(wxWindow* parent, const std::wstring& lastErrorMsg, const std::wstring& manualActivationUrl, std::wstring& manualActivationKey);


class DownloadProgressWindow //temporary progress info => life-time: stack
{
public:
    DownloadProgressWindow(wxWindow* parent, int64_t fileSizeTotal);
    ~DownloadProgressWindow();

    struct CancelPressed {};
    void notifyNewFile(const Zstring& filePath);
    void notifyProgress(int64_t delta);
    void requestUiRefresh(); //throw CancelPressed

private:
    class Impl;
    Impl* const pimpl_;
};
}

#endif //SMALL_DLGS_H_8321790875018750245

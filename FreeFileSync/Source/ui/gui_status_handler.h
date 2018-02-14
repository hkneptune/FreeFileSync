// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef GUI_STATUS_HANDLER_H_0183247018545
#define GUI_STATUS_HANDLER_H_0183247018545

#include <zen/error_log.h>
#include <wx/event.h>
#include "progress_indicator.h"
#include "main_dlg.h"
#include "../lib/status_handler.h"


namespace fff
{
//classes handling sync and compare errors as well as status feedback

//StatusHandlerTemporaryPanel(CompareProgressDialog) will internally process Window messages! disable GUI controls to avoid unexpected callbacks!
class StatusHandlerTemporaryPanel : private wxEvtHandler, public StatusHandler //throw AbortProcess
{
public:
    StatusHandlerTemporaryPanel(MainDialog& dlg);
    ~StatusHandlerTemporaryPanel();

    void initNewPhase(int itemsTotal, int64_t bytesTotal, Phase phaseID) override;

    void     reportInfo      (const std::wstring& text)                                override;
    Response reportError     (const std::wstring& text, size_t retryNumber)            override;
    void     reportFatalError(const std::wstring& errorMessage)                        override;
    void     reportWarning   (const std::wstring& warningMessage, bool& warningActive) override;

    void forceUiRefreshNoThrow() override;

    zen::ErrorLog getErrorLog() const { return errorLog_; }

private:
    void OnKeyPressed(wxKeyEvent& event);
    void OnAbortCompare(wxCommandEvent& event); //handle abort button click

    MainDialog& mainDlg_;
    zen::ErrorLog errorLog_;
};


//StatusHandlerFloatingDialog(SyncProgressDialog) will internally process Window messages! disable GUI controls to avoid unexpected callbacks!
class StatusHandlerFloatingDialog : public StatusHandler //throw AbortProcess
{
public:
    StatusHandlerFloatingDialog(wxFrame* parentDlg,
                                const std::chrono::system_clock::time_point& startTime,
                                size_t lastSyncsLogFileSizeMax,
                                bool ignoreErrors,
                                size_t automaticRetryCount,
                                size_t automaticRetryDelay,
                                const std::wstring& jobName,
                                const Zstring& soundFileSyncComplete,
                                const Zstring& postSyncCommand,
                                PostSyncCondition postSyncCondition,
                                bool& exitAfterSync,
                                bool& autoCloseDialog);
    ~StatusHandlerFloatingDialog();

    void initNewPhase       (int itemsTotal, int64_t bytesTotal, Phase phaseID) override;
    void updateProcessedData(int itemsDelta, int64_t bytesDelta               ) override;

    void     reportInfo      (const std::wstring& text                               ) override;
    Response reportError     (const std::wstring& text, size_t retryNumber           ) override;
    void     reportFatalError(const std::wstring& errorMessage                       ) override;
    void     reportWarning   (const std::wstring& warningMessage, bool& warningActive) override;

    void forceUiRefreshNoThrow() override;

private:
    void onProgressDialogTerminate();

    SyncProgressDialog* progressDlg_; //managed to have shorter lifetime than this handler!
    const size_t lastSyncsLogFileSizeMax_;
    zen::ErrorLog errorLog_;
    const size_t automaticRetryCount_;
    const size_t automaticRetryDelay_;
    const std::wstring jobName_;
    const std::chrono::system_clock::time_point startTime_; //don't use wxStopWatch: may overflow after a few days due to ::QueryPerformanceCounter()
    const Zstring postSyncCommand_;
    const PostSyncCondition postSyncCondition_;
    bool& exitAfterSync_;
    bool& autoCloseDialogOut_; //owned by SyncProgressDialog
};
}

#endif //GUI_STATUS_HANDLER_H_0183247018545

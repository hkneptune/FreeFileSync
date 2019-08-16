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
#include "../base/status_handler.h"


namespace fff
{
//classes handling sync and compare errors as well as status feedback

//StatusHandlerTemporaryPanel(CompareProgressDialog) will internally process Window messages! disable GUI controls to avoid unexpected callbacks!
class StatusHandlerTemporaryPanel : private wxEvtHandler, public StatusHandler
{
public:
    StatusHandlerTemporaryPanel(MainDialog& dlg, const std::chrono::system_clock::time_point& startTime, bool ignoreErrors, size_t automaticRetryCount, std::chrono::seconds automaticRetryDelay);
    ~StatusHandlerTemporaryPanel();

    void     initNewPhase    (int itemsTotal, int64_t bytesTotal, Phase phaseID) override; //
    void     logInfo         (const std::wstring& msg)                           override; //
    void     reportWarning   (const std::wstring& msg, bool& warningActive)      override; //throw AbortProcess
    Response reportError     (const std::wstring& msg, size_t retryNumber)       override; //
    void     reportFatalError(const std::wstring& msg)                           override; //

    void forceUiRefreshNoThrow() override;

    struct Result
    {
        ProcessSummary summary;
        std::shared_ptr<const zen::ErrorLog> errorLog;
    };
    Result reportFinalStatus(); //noexcept!!

private:
    void OnKeyPressed(wxKeyEvent& event);
    void OnAbortCompare(wxCommandEvent& event); //handle abort button click

    MainDialog& mainDlg_;
    zen::ErrorLog errorLog_;
    const size_t automaticRetryCount_;
    const std::chrono::seconds automaticRetryDelay_;
    const std::chrono::system_clock::time_point startTime_;
};


//StatusHandlerFloatingDialog(SyncProgressDialog) will internally process Window messages! disable GUI controls to avoid unexpected callbacks!
class StatusHandlerFloatingDialog : public StatusHandler
{
public:
    StatusHandlerFloatingDialog(wxFrame* parentDlg,
                                const std::chrono::system_clock::time_point& startTime,
                                bool ignoreErrors,
                                size_t automaticRetryCount,
                                std::chrono::seconds automaticRetryDelay,
                                const std::wstring& jobName,
                                const Zstring& soundFileSyncComplete,
                                const Zstring& postSyncCommand,
                                PostSyncCondition postSyncCondition,
                                bool& autoCloseDialog); //noexcept!
    ~StatusHandlerFloatingDialog();

    void     initNewPhase    (int itemsTotal, int64_t bytesTotal, Phase phaseID) override; //
    void     logInfo         (const std::wstring& msg)                           override; //
    void     reportWarning   (const std::wstring& msg, bool& warningActive)      override; //throw AbortProcess
    Response reportError     (const std::wstring& msg, size_t retryNumber)       override; //
    void     reportFatalError(const std::wstring& msg)                           override; //

    void updateDataProcessed(int itemsDelta, int64_t bytesDelta) override; //noexcept!!
    void forceUiRefreshNoThrow()                                 override; //

    enum class FinalRequest
    {
        none,
        exit,
        shutdown
    };
    struct Result
    {
        ProcessSummary summary;
        std::shared_ptr<const zen::ErrorLog> errorLog;
        FinalRequest finalRequest;
        AbstractPath logFilePath;
    };
    Result reportFinalStatus(const Zstring& altLogFolderPathPhrase, int logfilesMaxAgeDays, const std::set<AbstractPath>& logFilePathsToKeep); //noexcept!!

private:
    SyncProgressDialog* progressDlg_; //managed to have the same lifetime as this handler!
    zen::ErrorLog errorLog_;
    const size_t automaticRetryCount_;
    const std::chrono::seconds automaticRetryDelay_;
    const std::wstring jobName_;
    const std::chrono::system_clock::time_point startTime_;
    const Zstring postSyncCommand_;
    const PostSyncCondition postSyncCondition_;
    bool& autoCloseDialogOut_; //owned by SyncProgressDialog
};
}

#endif //GUI_STATUS_HANDLER_H_0183247018545

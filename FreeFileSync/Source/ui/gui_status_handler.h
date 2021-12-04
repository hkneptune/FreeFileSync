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
#include "../status_handler.h"


namespace fff
{
//classes handling sync and compare errors as well as status feedback

//internally pumps window messages => disable GUI controls to avoid unexpected callbacks!
class StatusHandlerTemporaryPanel : private wxEvtHandler, public StatusHandler
{
public:
    StatusHandlerTemporaryPanel(MainDialog& dlg,
                                const std::chrono::system_clock::time_point& startTime,
                                bool ignoreErrors,
                                size_t autoRetryCount,
                                std::chrono::seconds autoRetryDelay,
                                const Zstring& soundFileAlertPending);
    ~StatusHandlerTemporaryPanel();

    void     initNewPhase    (int itemsTotal, int64_t bytesTotal, ProcessPhase phaseID) override; //
    void     logInfo         (const std::wstring& msg)                                  override; //
    void     reportWarning   (const std::wstring& msg, bool& warningActive)             override; //throw AbortProcess
    Response reportError     (const ErrorInfo& errorInfo)                               override; //
    void     reportFatalError(const std::wstring& msg)                                  override; //

    void forceUiUpdateNoThrow() override;

    struct Result
    {
        ProcessSummary summary;
        std::shared_ptr<const zen::ErrorLog> errorLog;
    };
    Result reportResults(); //noexcept!!

private:
    void onLocalKeyEvent(wxKeyEvent& event);
    void onAbortCompare(wxCommandEvent& event); //handle abort button click
    void showStatsPanel();

    MainDialog& mainDlg_;
    zen::ErrorLog errorLog_;
    const bool ignoreErrors_;
    const size_t autoRetryCount_;
    const std::chrono::seconds autoRetryDelay_;
    const Zstring soundFileAlertPending_;

    const std::chrono::system_clock::time_point startTime_;
    const std::chrono::steady_clock::time_point startTimeSteady_ = std::chrono::steady_clock::now();
};


//StatusHandlerFloatingDialog(SyncProgressDialog) will internally process Window messages! disable GUI controls to avoid unexpected callbacks!
class StatusHandlerFloatingDialog : public StatusHandler
{
public:
    StatusHandlerFloatingDialog(wxFrame* parentDlg,
                                const std::vector<std::wstring>& jobNames,
                                const std::chrono::system_clock::time_point& startTime,
                                bool ignoreErrors,
                                size_t autoRetryCount,
                                std::chrono::seconds autoRetryDelay,
                                const Zstring& soundFileSyncComplete,
                                const Zstring& soundFileAlertPending,
                                const wxSize& progressDlgSize, bool dlgMaximize,
                                bool autoCloseDialog); //noexcept!
    ~StatusHandlerFloatingDialog();

    void     initNewPhase    (int itemsTotal, int64_t bytesTotal, ProcessPhase phaseID) override; //
    void     logInfo         (const std::wstring& msg)                                  override; //
    void     reportWarning   (const std::wstring& msg, bool& warningActive)             override; //throw AbortProcess
    Response reportError     (const ErrorInfo& errorInfo)                               override; //
    void     reportFatalError(const std::wstring& msg)                                  override; //

    void updateDataProcessed(int itemsDelta, int64_t bytesDelta) override; //noexcept!!
    void forceUiUpdateNoThrow()                                  override; //

    enum class FinalRequest
    {
        none,
        exit,
        shutdown
    };
    struct Result
    {
        ProcessSummary summary;
        zen::SharedRef<const zen::ErrorLog> errorLog;
        FinalRequest finalRequest;
        AbstractPath logFilePath;
        wxSize dlgSize;
        bool dlgIsMaximized;
        bool autoCloseDialog;
    };
    Result reportResults(const Zstring& postSyncCommand, PostSyncCondition postSyncCondition,
                         const Zstring& altLogFolderPathPhrase, int logfilesMaxAgeDays, LogFileFormat logFormat, const std::set<AbstractPath>& logFilePathsToKeep,
                         const std::string& emailNotifyAddress, ResultsNotification emailNotifyCondition); //noexcept!!

private:
    const std::vector<std::wstring> jobNames_;
    const std::chrono::system_clock::time_point startTime_;
    const size_t autoRetryCount_;
    const std::chrono::seconds autoRetryDelay_;
    const Zstring soundFileSyncComplete_;
    const Zstring soundFileAlertPending_;

    SyncProgressDialog* progressDlg_; //managed to have the same lifetime as this handler!
    zen::ErrorLog errorLog_;
};
}

#endif //GUI_STATUS_HANDLER_H_0183247018545

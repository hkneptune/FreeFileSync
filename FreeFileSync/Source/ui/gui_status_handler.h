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
    void     logMessage      (const std::wstring& msg, MsgType type)                    override; //
    void     reportWarning   (const std::wstring& msg, bool& warningActive)             override; //throw CancelProcess
    Response reportError     (const ErrorInfo& errorInfo)                               override; //
    void     reportFatalError(const std::wstring& msg)                                  override; //
    ErrorStats getErrorStats() const override;

    void forceUiUpdateNoThrow() override;

    struct Result
    {
        ProcessSummary summary;
        zen::SharedRef<const zen::ErrorLog> errorLog;
    };
    Result prepareResult(); //noexcept!!

private:
    void onLocalKeyEvent(wxKeyEvent& event);
    void onAbortCompare(wxCommandEvent& event); //handle abort button click
    void showStatsPanel();

    MainDialog& mainDlg_;
    zen::ErrorLog errorLog_;
    mutable Statistics::ErrorStats errorStatsBuf_{};
    mutable size_t errorStatsRowsChecked_ = 0;
    const bool ignoreErrors_;
    const size_t autoRetryCount_;
    const std::chrono::seconds autoRetryDelay_;
    const Zstring soundFileAlertPending_;
    const std::chrono::system_clock::time_point startTime_;
    const std::chrono::steady_clock::time_point panelInitTime_ = std::chrono::steady_clock::now();
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
                                const zen::WindowLayout::Dimensions& dim,
                                bool autoCloseDialog); //noexcept!
    ~StatusHandlerFloatingDialog();

    void     initNewPhase    (int itemsTotal, int64_t bytesTotal, ProcessPhase phaseID) override; //
    void     logMessage      (const std::wstring& msg, MsgType type)                    override; //
    void     reportWarning   (const std::wstring& msg, bool& warningActive)             override; //throw CancelProcess
    Response reportError     (const ErrorInfo& errorInfo)                               override; //
    void     reportFatalError(const std::wstring& msg)                                  override; //
    ErrorStats getErrorStats() const override;

    void updateDataProcessed(int itemsDelta, int64_t bytesDelta) override; //noexcept!!
    void forceUiUpdateNoThrow()                                  override; //

    struct Result
    {
        ProcessSummary summary;
        zen::SharedRef<zen::ErrorLog> errorLog;
    };
    Result prepareResult();

    enum class FinalRequest
    {
        none,
        exit,
        shutdown
    };
    struct DlgOptions
    {
        bool autoCloseSelected;
        zen::WindowLayout::Dimensions dim;
        FinalRequest finalRequest;
    };
    DlgOptions showResult();

private:
    const std::vector<std::wstring> jobNames_;
    const std::chrono::system_clock::time_point startTime_;
    const size_t autoRetryCount_;
    const std::chrono::seconds autoRetryDelay_;
    const Zstring soundFileSyncComplete_;
    const Zstring soundFileAlertPending_;
    SyncProgressDialog* progressDlg_; //managed to have the same lifetime as this handler!
    zen::SharedRef<zen::ErrorLog> errorLog_ = zen::makeSharedRef<zen::ErrorLog>();
    mutable Statistics::ErrorStats errorStatsBuf_{};
    mutable size_t errorStatsRowsChecked_ = 0;
    std::optional<TaskResult> syncResult_;
};
}

#endif //GUI_STATUS_HANDLER_H_0183247018545

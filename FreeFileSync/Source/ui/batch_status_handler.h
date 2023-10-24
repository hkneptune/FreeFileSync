// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef BATCH_STATUS_HANDLER_H_857390451451234566
#define BATCH_STATUS_HANDLER_H_857390451451234566

#include <zen/error_log.h>
#include "progress_indicator.h"
#include "../config.h"
#include "../status_handler.h"


namespace fff
{
//BatchStatusHandler(SyncProgressDialog) will internally process Window messages! disable GUI controls to avoid unexpected callbacks!
class BatchStatusHandler : public StatusHandler
{
public:
    BatchStatusHandler(bool showProgress,
                       const std::wstring& jobName, //should not be empty for a batch job!
                       const std::chrono::system_clock::time_point& startTime,
                       bool ignoreErrors,
                       size_t autoRetryCount,
                       std::chrono::seconds autoRetryDelay,
                       const Zstring& soundFileSyncComplete,
                       const Zstring& soundFileAlertPending,
                       const zen::WindowLayout::Dimensions& dim,
                       bool autoCloseDialog,
                       PostBatchAction postBatchAction,
                       BatchErrorHandling batchErrorHandling); //noexcept!!
    ~BatchStatusHandler();

    void     initNewPhase    (int itemsTotal, int64_t bytesTotal, ProcessPhase phaseID) override; //
    void     logMessage      (const std::wstring& msg, MsgType type)                    override; //
    void     reportWarning   (const std::wstring& msg, bool& warningActive)             override; //throw CancelProcess
    Response reportError     (const ErrorInfo& errorInfo)                               override; //
    void     reportFatalError(const std::wstring& msg)                                  override; //
    ErrorStats getErrorStats() const override;

    void updateDataProcessed(int itemsDelta, int64_t bytesDelta) override; //noexcept
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
        switchGui,
        shutdown
    };
    struct DlgOptions
    {
        zen::WindowLayout::Dimensions dim;
        FinalRequest finalRequest;
    };
    DlgOptions showResult();

    wxWindow* getWindowIfVisible();

private:
    const std::wstring jobName_;
    const std::chrono::system_clock::time_point startTime_;
    const size_t autoRetryCount_;
    const std::chrono::seconds autoRetryDelay_;
    const Zstring soundFileSyncComplete_;
    const Zstring soundFileAlertPending_;

    SyncProgressDialog* progressDlg_; //managed to have the same lifetime as this handler!
    zen::SharedRef<zen::ErrorLog> errorLog_ = zen::makeSharedRef<zen::ErrorLog>();
    mutable Statistics::ErrorStats errorStatsBuf_{};
    mutable size_t errorStatsRowsChecked_ = 0;
    const BatchErrorHandling batchErrorHandling_;
    bool switchToGuiRequested_ = false;
    std::optional<TaskResult> syncResult_;
};
}

#endif //BATCH_STATUS_HANDLER_H_857390451451234566

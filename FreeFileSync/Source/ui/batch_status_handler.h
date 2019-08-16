// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef BATCH_STATUS_HANDLER_H_857390451451234566
#define BATCH_STATUS_HANDLER_H_857390451451234566

#include <chrono>
#include <zen/error_log.h>
#include "progress_indicator.h"
#include "../base/config.h"
#include "../base/status_handler.h"


namespace fff
{
//BatchStatusHandler(SyncProgressDialog) will internally process Window messages! disable GUI controls to avoid unexpected callbacks!
class BatchStatusHandler : public StatusHandler
{
public:
    BatchStatusHandler(bool showProgress,
                       bool autoCloseDialog,
                       const std::wstring& jobName, //should not be empty for a batch job!
                       const Zstring& soundFileSyncComplete,
                       const std::chrono::system_clock::time_point& startTime,
                       bool ignoreErrors,
                       BatchErrorHandling batchErrorHandling,
                       size_t automaticRetryCount,
                       std::chrono::seconds automaticRetryDelay,
                       const Zstring& postSyncCommand,
                       PostSyncCondition postSyncCondition,
                       PostSyncAction postSyncAction); //noexcept!!
    ~BatchStatusHandler();

    void     initNewPhase    (int itemsTotal, int64_t bytesTotal, Phase phaseID) override; //
    void     logInfo         (const std::wstring& msg)                           override; //
    void     reportWarning   (const std::wstring& msg, bool& warningActive)      override; //throw AbortProcess
    Response reportError     (const std::wstring& msg, size_t retryNumber)       override; //
    void     reportFatalError(const std::wstring& msg)                           override; //

    void updateDataProcessed(int itemsDelta, int64_t bytesDelta) override; //noexcept
    void forceUiRefreshNoThrow()                                 override; //

    enum class FinalRequest
    {
        none,
        switchGui,
        shutdown
    };
    struct Result
    {
        SyncResult finalStatus;
        FinalRequest finalRequest;
        AbstractPath logFilePath;
    };
    Result reportFinalStatus(const Zstring& altLogFolderPathPhrase, int logfilesMaxAgeDays, const std::set<AbstractPath>& logFilePathsToKeep); //noexcept!!

private:
    bool switchToGuiRequested_ = false;
    const BatchErrorHandling batchErrorHandling_;
    zen::ErrorLog errorLog_; //list of non-resolved errors and warnings
    const size_t automaticRetryCount_;
    const std::chrono::seconds automaticRetryDelay_;
    SyncProgressDialog* progressDlg_; //managed to have the same lifetime as this handler!
    const std::wstring jobName_;
    const std::chrono::system_clock::time_point startTime_;
    const Zstring postSyncCommand_;
    const PostSyncCondition postSyncCondition_;
};
}

#endif //BATCH_STATUS_HANDLER_H_857390451451234566

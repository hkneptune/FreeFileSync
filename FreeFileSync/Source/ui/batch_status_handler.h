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
#include "../lib/status_handler.h"
#include "../lib/process_xml.h"
#include "../lib/return_codes.h"


namespace fff
{
class BatchRequestSwitchToMainDialog {};


//BatchStatusHandler(SyncProgressDialog) will internally process Window messages! disable GUI controls to avoid unexpected callbacks!
class BatchStatusHandler : public StatusHandler //throw AbortProcess, BatchRequestSwitchToMainDialog
{
public:
    BatchStatusHandler(bool showProgress, //defines: -start minimized and -quit immediately when finished
                       bool autoCloseDialog,
                       const std::wstring& jobName, //should not be empty for a batch job!
                       const Zstring& soundFileSyncComplete,
                       const std::chrono::system_clock::time_point& startTime,
                       const Zstring& logFolderPathPhrase,
                       int logfilesCountLimit, //0: logging inactive; < 0: no limit
                       size_t lastSyncsLogFileSizeMax,
                       bool ignoreErrors,
                       BatchErrorDialog batchErrorDialog,
                       size_t automaticRetryCount,
                       size_t automaticRetryDelay,
                       FfsReturnCode& returnCode,
                       const Zstring& postSyncCommand,
                       PostSyncCondition postSyncCondition,
                       PostSyncAction postSyncAction);
    ~BatchStatusHandler();

    void initNewPhase       (int itemsTotal, int64_t bytesTotal, Phase phaseID) override;
    void updateProcessedData(int itemsDelta, int64_t bytesDelta)                override;
    void reportInfo         (const std::wstring& text)                          override;
    void forceUiRefreshNoThrow()                                                override;

    void     reportWarning   (const std::wstring& warningMessage, bool& warningActive) override;
    Response reportError     (const std::wstring& errorMessage, size_t retryNumber   ) override;
    void     reportFatalError(const std::wstring& errorMessage                       ) override;

private:
    void onProgressDialogTerminate();

    bool switchToGuiRequested_ = false;
    const int logfilesCountLimit_;
    const size_t lastSyncsLogFileSizeMax_;
    const BatchErrorDialog batchErrorDialog_;
    zen::ErrorLog errorLog_; //list of non-resolved errors and warnings
    FfsReturnCode& returnCode_;

    const size_t automaticRetryCount_;
    const size_t automaticRetryDelay_;

    SyncProgressDialog* progressDlg_; //managed to have shorter lifetime than this handler!

    const std::wstring jobName_;
    const std::chrono::system_clock::time_point startTime_;

    const Zstring logFolderPathPhrase_;
    const Zstring postSyncCommand_;
    const PostSyncCondition postSyncCondition_;
};
}

#endif //BATCH_STATUS_HANDLER_H_857390451451234566

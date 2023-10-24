// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "batch_status_handler.h"
#include <zen/shutdown.h>
#include <wx+/popup_dlg.h>
#include <wx/app.h>
#include <wx/sound.h>

using namespace zen;
using namespace fff;


BatchStatusHandler::BatchStatusHandler(bool showProgress,
                                       const std::wstring& jobName,
                                       const std::chrono::system_clock::time_point& startTime,
                                       bool ignoreErrors,
                                       size_t autoRetryCount,
                                       std::chrono::seconds autoRetryDelay,
                                       const Zstring& soundFileSyncComplete,
                                       const Zstring& soundFileAlertPending,
                                       const WindowLayout::Dimensions& dims,
                                       bool autoCloseDialog,
                                       PostBatchAction postBatchAction,
                                       BatchErrorHandling batchErrorHandling) :
    jobName_(jobName),
    startTime_(startTime),
    autoRetryCount_(autoRetryCount),
    autoRetryDelay_(autoRetryDelay),
    soundFileSyncComplete_(soundFileSyncComplete),
    soundFileAlertPending_(soundFileAlertPending),
    batchErrorHandling_(batchErrorHandling)
{
    //set *after* initializer list => callbacks during construction to getErrorStats()!
    progressDlg_ = SyncProgressDialog::create(dims, [this] { userRequestCancel(); }, *this, nullptr /*parentWindow*/, showProgress, autoCloseDialog,
    {jobName}, std::chrono::system_clock::to_time_t(startTime), ignoreErrors, autoRetryCount, [&]
    {
        switch (postBatchAction)
        {
            case PostBatchAction::none:
                return PostSyncAction::none;
            case PostBatchAction::sleep:
                return PostSyncAction::sleep;
            case PostBatchAction::shutdown:
                return PostSyncAction::shutdown;
        }
        assert(false);
        return PostSyncAction::none;
    }());
    //ATTENTION: "progressDlg_" is an unmanaged resource!!! However, at this point we already consider construction complete! =>
    //ZEN_ON_SCOPE_FAIL( cleanup(); ); //destructor call would lead to member double clean-up!!!
}


BatchStatusHandler::~BatchStatusHandler()
{
    if (progressDlg_) //prepareResult() was not called!
        std::abort();
}


BatchStatusHandler::Result BatchStatusHandler::prepareResult()
{
    //keep correct summary window stats considering count down timer, system sleep
    const std::chrono::milliseconds totalTime = progressDlg_->pauseAndGetTotalTime();

    //append "extra" log for sync errors that could not otherwise be reported:
    if (const ErrorLog extraLog = fetchExtraLog();
        !extraLog.empty())
    {
        append(errorLog_.ref(), extraLog);
        std::stable_sort(errorLog_.ref().begin(), errorLog_.ref().end(), [](const LogEntry& lhs, const LogEntry& rhs) { return lhs.time < rhs.time; });
    }

    //determine post-sync status irrespective of further errors during tear-down
    assert(!syncResult_);
    syncResult_ = [&]
    {
        if (taskCancelled())
        {
            logMsg(errorLog_.ref(), _("Stopped"), MSG_TYPE_ERROR); //= user cancel or "stop on first error"
            return TaskResult::cancelled;
        }
        const ErrorLogStats logCount = getStats(errorLog_.ref());
        if (logCount.error > 0)
            return TaskResult::error;
        else if (logCount.warning > 0)
            return TaskResult::warning;

        if (getTotalStats() == ProgressStats())
            logMsg(errorLog_.ref(), _("Nothing to synchronize"), MSG_TYPE_INFO);
        return TaskResult::success;
    }();

    assert(*syncResult_ == TaskResult::cancelled || currentPhase() == ProcessPhase::sync);

    const ProcessSummary summary
    {
        startTime_, *syncResult_, {jobName_},
        getCurrentStats(),
        getTotalStats  (),
        totalTime
    };

    return {summary, errorLog_};
}


BatchStatusHandler::DlgOptions BatchStatusHandler::showResult()
{
    bool autoClose = false;
    bool suspend = false;
    FinalRequest finalRequest = FinalRequest::none;

    if (taskCancelled() && *taskCancelled() == CancelReason::user)
    {
        /* user cancelled => don't run post sync command
                           => don't send email notification
                           => don't play sound notification
                           => don't run post sync action     */
        if (switchToGuiRequested_) //-> avoid recursive yield() calls, thous switch not before ending batch mode
        {
            autoClose = true;
            finalRequest = FinalRequest::switchGui;
        }
    }
    else
    {
        //--------------------- post sync actions ----------------------
        auto proceedWithShutdown = [&](const std::wstring& operationName)
        {
            if (progressDlg_->getWindowIfVisible())
                try
                {
                    assert(!endsWith(operationName, L"."));
                    auto notifyStatusThrowOnCancel = [&](const std::wstring& timeRemMsg)
                    {
                        try { updateStatus(operationName + L"... " + timeRemMsg); /*throw CancelProcess*/ }
                        catch (CancelProcess&)
                        {
                            if (taskCancelled() && *taskCancelled() == CancelReason::user)
                                throw;
                        }
                    };
                    delayAndCountDown(std::chrono::steady_clock::now() + std::chrono::seconds(10), notifyStatusThrowOnCancel); //throw CancelProcess
                }
                catch (CancelProcess&) { return false; }

            return true;
        };

        switch (progressDlg_->getOptionPostSyncAction())
        {
            case PostSyncAction::none:
                autoClose = progressDlg_->getOptionAutoCloseDialog();
                break;
            case PostSyncAction::exit:
                assert(false);
                break;
            case PostSyncAction::sleep:
                if (proceedWithShutdown(_("System: Sleep")))
                {
                    autoClose = progressDlg_->getOptionAutoCloseDialog();
                    suspend = true;
                }
                break;
            case PostSyncAction::shutdown:
                if (proceedWithShutdown(_("System: Shut down")))
                {
                    autoClose = true;
                    finalRequest = FinalRequest::shutdown; //system shutdown must be handled by calling context!
                }
                break;
        }
    }

    if (suspend) //...*before* results dialog is shown
        try
        {
            suspendSystem(); //throw FileError
        }
        catch (const FileError& e) { logMsg(errorLog_.ref(), e.toString(), MSG_TYPE_ERROR); }

    //--------------------- sound notification ----------------------
    if (taskCancelled() && *taskCancelled() == CancelReason::user)
        ;
    else if (!suspend && !autoClose && //only play when actually showing results dialog
             !soundFileSyncComplete_.empty())
    {
        //wxWidgets shows modal error dialog by default => "no, wxWidgets, NO!"
        wxLog* oldLogTarget = wxLog::SetActiveTarget(new wxLogStderr); //transfer and receive ownership!
        ZEN_ON_SCOPE_EXIT(delete wxLog::SetActiveTarget(oldLogTarget));

        wxSound::Play(utfTo<wxString>(soundFileSyncComplete_), wxSOUND_ASYNC);
    }
    //if (::GetForegroundWindow() != GetHWND())
    //    RequestUserAttention(); -> probably too much since task bar is already colorized with Taskbar::Status::error or Status::normal

    const auto [autoCloseDialog, dim] = progressDlg_->destroy(autoClose,
                                                              true /*restoreParentFrame: n/a here*/,
                                                              *syncResult_, errorLog_);
    //caveat: calls back to getErrorStats() => share errorLog_
    progressDlg_ = nullptr;

    return {dim, finalRequest};
}


wxWindow* BatchStatusHandler::getWindowIfVisible()
{
    return progressDlg_ ? progressDlg_->getWindowIfVisible() : nullptr;
}


void BatchStatusHandler::initNewPhase(int itemsTotal, int64_t bytesTotal, ProcessPhase phaseID)
{
    StatusHandler::initNewPhase(itemsTotal, bytesTotal, phaseID);
    progressDlg_->initNewPhase(); //call after "StatusHandler::initNewPhase"

    //macOS needs a full yield to update GUI and get rid of "dummy" texts
    requestUiUpdate(true /*force*/); //throw CancelProcess
}


void BatchStatusHandler::updateDataProcessed(int itemsDelta, int64_t bytesDelta) //noexcept!
{
    StatusHandler::updateDataProcessed(itemsDelta, bytesDelta);

    //note: this method should NOT throw in order to properly allow undoing setting of statistics!
    progressDlg_->notifyProgressChange(); //noexcept
    //for "curveDataBytes_->addRecord()"
}


void BatchStatusHandler::logMessage(const std::wstring& msg, MsgType type)
{
    logMsg(errorLog_.ref(), msg, [&]
    {
        switch (type)
        {
            //*INDENT-OFF*
            case MsgType::info:    return MSG_TYPE_INFO;
            case MsgType::warning: return MSG_TYPE_WARNING;
            case MsgType::error:   return MSG_TYPE_ERROR;
            //*INDENT-ON*
        }
        assert(false);
        return MSG_TYPE_ERROR;
    }());
    requestUiUpdate(false /*force*/); //throw CancelProcess
}


void BatchStatusHandler::reportWarning(const std::wstring& msg, bool& warningActive)
{
    PauseTimers dummy(*progressDlg_);

    logMsg(errorLog_.ref(), msg, MSG_TYPE_WARNING);

    if (!warningActive)
        return;

    if (!progressDlg_->getOptionIgnoreErrors())
        switch (batchErrorHandling_)
        {
            case BatchErrorHandling::showPopup:
            {
                forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

                bool dontWarnAgain = false;
                switch (showQuestionDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::warning,
                                           PopupDialogCfg().setDetailInstructions(msg + L"\n\n" + _("You can switch to FreeFileSync's main window to resolve this issue.")).
                                           alertWhenPending(soundFileAlertPending_).
                                           setCheckBox(dontWarnAgain, _("&Don't show this warning again"), static_cast<ConfirmationButton3>(QuestionButton2::no)),
                                           _("&Ignore"), _("&Switch")))
                {
                    case QuestionButton2::yes: //ignore
                        warningActive = !dontWarnAgain;
                        break;

                    case QuestionButton2::no: //switch
                        logMsg(errorLog_.ref(), _("Switching to FreeFileSync's main window"), MSG_TYPE_INFO);
                        switchToGuiRequested_ = true; //treat as a special kind of cancel
                        cancelProcessNow(CancelReason::user); //throw CancelProcess

                    case QuestionButton2::cancel:
                        cancelProcessNow(CancelReason::user); //throw CancelProcess
                        break;
                }
            }
            break; //keep it! last switch might not find match

            case BatchErrorHandling::cancel:
                cancelProcessNow(CancelReason::firstError); //throw CancelProcess
                break;
        }
}


ProcessCallback::Response BatchStatusHandler::reportError(const ErrorInfo& errorInfo)
{
    PauseTimers dummy(*progressDlg_);

    //log actual fail time (not "now"!)
    const time_t failTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() -
                                                                 std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::steady_clock::now() - errorInfo.failTime));
    //auto-retry
    if (errorInfo.retryNumber < autoRetryCount_)
    {
        logMsg(errorLog_.ref(), errorInfo.msg + L"\n-> " + _("Automatic retry"), MSG_TYPE_INFO, failTime);
        delayAndCountDown(errorInfo.failTime + autoRetryDelay_,
                          [&, statusPrefix  = _("Automatic retry") +
                                              (errorInfo.retryNumber == 0 ? L"" : L' ' + formatNumber(errorInfo.retryNumber + 1)) + SPACED_DASH,
                              statusPostfix = SPACED_DASH + _("Error") + L": " + replaceCpy(errorInfo.msg, L'\n', L' ')](const std::wstring& timeRemMsg)
        { this->updateStatus(statusPrefix + timeRemMsg + statusPostfix); }); //throw CancelProcess
        return ProcessCallback::retry;
    }

    //always, except for "retry":
    auto guardWriteLog = makeGuard<ScopeGuardRunMode::onExit>([&] { logMsg(errorLog_.ref(), errorInfo.msg, MSG_TYPE_ERROR, failTime); });

    if (!progressDlg_->getOptionIgnoreErrors())
    {
        switch (batchErrorHandling_)
        {
            case BatchErrorHandling::showPopup:
            {
                forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

                switch (showConfirmationDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::error,
                                               PopupDialogCfg().setDetailInstructions(errorInfo.msg).
                                               alertWhenPending(soundFileAlertPending_),
                                               _("&Ignore"), _("Ignore &all"), _("&Retry")))
                {
                    case ConfirmationButton3::accept: //ignore
                        return ProcessCallback::ignore;

                    case ConfirmationButton3::accept2: //ignore all
                        progressDlg_->setOptionIgnoreErrors(true);
                        return ProcessCallback::ignore;

                    case ConfirmationButton3::decline: //retry
                        guardWriteLog.dismiss();
                        logMsg(errorLog_.ref(), errorInfo.msg + L"\n-> " + _("Retrying operation..."), MSG_TYPE_INFO, failTime);
                        return ProcessCallback::retry;

                    case ConfirmationButton3::cancel:
                        cancelProcessNow(CancelReason::user); //throw CancelProcess
                        break;
                }
            }
            break; //used if last switch didn't find a match

            case BatchErrorHandling::cancel:
                cancelProcessNow(CancelReason::firstError); //throw CancelProcess
                break;
        }
    }
    else
        return ProcessCallback::ignore;

    assert(false);
    return ProcessCallback::ignore; //dummy value
}


void BatchStatusHandler::reportFatalError(const std::wstring& msg)
{
    PauseTimers dummy(*progressDlg_);

    logMsg(errorLog_.ref(), msg, MSG_TYPE_ERROR);

    if (!progressDlg_->getOptionIgnoreErrors())
        switch (batchErrorHandling_)
        {
            case BatchErrorHandling::showPopup:
            {
                forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

                switch (showConfirmationDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::error,
                                               PopupDialogCfg().setDetailInstructions(msg).
                                               alertWhenPending(soundFileAlertPending_),
                                               _("&Ignore"), _("Ignore &all")))
                {
                    case ConfirmationButton2::accept: //ignore
                        break;

                    case ConfirmationButton2::accept2: //ignore all
                        progressDlg_->setOptionIgnoreErrors(true);
                        break;

                    case ConfirmationButton2::cancel:
                        cancelProcessNow(CancelReason::user); //throw CancelProcess
                        break;
                }
            }
            break;

            case BatchErrorHandling::cancel:
                cancelProcessNow(CancelReason::firstError); //throw CancelProcess
                break;
        }
}


Statistics::ErrorStats BatchStatusHandler::getErrorStats() const
{
    //errorLog_ is an "append only" structure, so we can make getErrorStats() complexity "constant time":
    std::for_each(errorLog_.ref().begin() + errorStatsRowsChecked_, errorLog_.ref().end(), [&](const LogEntry& entry)
    {
        switch (entry.type)
        {
            case MSG_TYPE_INFO:
                break;
            case MSG_TYPE_WARNING:
                ++errorStatsBuf_.warningCount;
                break;
            case MSG_TYPE_ERROR:
                ++errorStatsBuf_.errorCount;
                break;
        }
    });
    errorStatsRowsChecked_ = errorLog_.ref().size();

    return errorStatsBuf_;
}


void BatchStatusHandler::forceUiUpdateNoThrow()
{
    progressDlg_->updateGui();
}

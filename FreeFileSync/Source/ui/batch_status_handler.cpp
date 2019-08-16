// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "batch_status_handler.h"
#include <zen/shell_execute.h>
#include <zen/shutdown.h>
#include <wx+/popup_dlg.h>
#include <wx/app.h>
#include "../base/resolve_path.h"
#include "../base/log_file.h"
#include "../afs/concrete.h"

using namespace zen;
using namespace fff;


BatchStatusHandler::BatchStatusHandler(bool showProgress,
                                       bool autoCloseDialog,
                                       const std::wstring& jobName,
                                       const Zstring& soundFileSyncComplete,
                                       const std::chrono::system_clock::time_point& startTime,
                                       bool ignoreErrors,
                                       BatchErrorHandling batchErrorHandling,
                                       size_t automaticRetryCount,
                                       std::chrono::seconds automaticRetryDelay,
                                       const Zstring& postSyncCommand,
                                       PostSyncCondition postSyncCondition,
                                       PostSyncAction postSyncAction) :
    batchErrorHandling_(batchErrorHandling),
    automaticRetryCount_(automaticRetryCount),
    automaticRetryDelay_(automaticRetryDelay),
    progressDlg_(SyncProgressDialog::create([this] { userRequestAbort(); }, *this, nullptr /*parentWindow*/, showProgress, autoCloseDialog,
startTime, jobName, soundFileSyncComplete, ignoreErrors, automaticRetryCount, [&]
{
    switch (postSyncAction)
    {
        case PostSyncAction::none:
            return PostSyncAction2::none;
        case PostSyncAction::sleep:
            return PostSyncAction2::sleep;
        case PostSyncAction::shutdown:
            return PostSyncAction2::shutdown;
    }
    assert(false);
    return PostSyncAction2::none;
}())),
jobName_(jobName),
         startTime_(startTime),
         postSyncCommand_(postSyncCommand),
         postSyncCondition_(postSyncCondition)
{
    //ATTENTION: "progressDlg_" is an unmanaged resource!!! However, at this point we already consider construction complete! =>
    //ZEN_ON_SCOPE_FAIL( cleanup(); ); //destructor call would lead to member double clean-up!!!
}


BatchStatusHandler::~BatchStatusHandler()
{
    if (progressDlg_) //reportFinalStatus() was not called!
        std::abort();
}


BatchStatusHandler::Result BatchStatusHandler::reportFinalStatus(const Zstring& altLogFolderPathPhrase, int logfilesMaxAgeDays, const std::set<AbstractPath>& logFilePathsToKeep) //noexcept!!
{
    const auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime_);

    progressDlg_->timerSetStatus(false /*active*/); //keep correct summary window stats considering count down timer, system sleep

    //determine post-sync status irrespective of further errors during tear-down
    const SyncResult finalStatus = [&]
    {
        if (getAbortStatus())
        {
            errorLog_.logMsg(_("Stopped"), MSG_TYPE_ERROR); //= user cancel; *not* a MSG_TYPE_FATAL_ERROR!
            return SyncResult::aborted;
        }
        else if (errorLog_.getItemCount(MSG_TYPE_ERROR | MSG_TYPE_FATAL_ERROR) > 0)
            return SyncResult::finishedError;
        else if (errorLog_.getItemCount(MSG_TYPE_WARNING) > 0)
            return SyncResult::finishedWarning;

        if (getStatsTotal(currentPhase()) == ProgressStats())
            errorLog_.logMsg(_("Nothing to synchronize"), MSG_TYPE_INFO);
        return SyncResult::finishedSuccess;
    }();

    assert(finalStatus == SyncResult::aborted || currentPhase() == PHASE_SYNCHRONIZING);

    const ProcessSummary summary
    {
        startTime_, finalStatus, jobName_,
        getStatsCurrent(currentPhase()),
        getStatsTotal  (currentPhase()),
        totalTime
    };

    //post sync command
    Zstring commandLine = [&]
    {
        if (getAbortStatus() && *getAbortStatus() == AbortTrigger::USER)
            ; //user cancelled => don't run post sync command!
        else
            switch (postSyncCondition_)
            {
                case PostSyncCondition::COMPLETION:
                    return postSyncCommand_;
                case PostSyncCondition::ERRORS:
                    if (finalStatus == SyncResult::aborted ||
                        finalStatus == SyncResult::finishedError)
                        return postSyncCommand_;
                    break;
                case PostSyncCondition::SUCCESS:
                    if (finalStatus == SyncResult::finishedWarning ||
                        finalStatus == SyncResult::finishedSuccess)
                        return postSyncCommand_;
                    break;
            }
        return Zstring();
    }();
    trim(commandLine);

    if (!commandLine.empty())
        errorLog_.logMsg(_("Executing command:") + L" " + utfTo<std::wstring>(commandLine), MSG_TYPE_INFO);

    //----------------- always save log under %appdata%\FreeFileSync\Logs ------------------------
    //create not before destruction: 1. avoid issues with FFS trying to sync open log file 2. simplify transactional retry on failure 3. include status in log file name without rename
    // 4. failure to write to particular stream must not be retried!
    AbstractPath logFilePath = getNullPath();
    try
    {
        //do NOT use tryReportingError()! saving log files should not be cancellable!
        auto notifyStatusNoThrow = [&](const std::wstring& msg) { try { reportStatus(msg); /*throw AbortProcess*/ } catch (...) {} };
        logFilePath = saveLogFile(summary, errorLog_, altLogFolderPathPhrase, logfilesMaxAgeDays, logFilePathsToKeep, notifyStatusNoThrow); //throw FileError
    }
    catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }

    //execute post sync command *after* writing log files, so that user can refer to the log via the command!
    if (!commandLine.empty())
        try
        {
            //----------------------------------------------------------------------
            ::wxSetEnv(L"logfile_path", AFS::getDisplayPath(logFilePath));
            //----------------------------------------------------------------------
            //use ExecutionType::ASYNC until there is a reason not to: https://freefilesync.org/forum/viewtopic.php?t=31
            shellExecute(expandMacros(commandLine), ExecutionType::ASYNC, false/*hideConsole*/); //throw FileError
        }
        catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }

    //post sync action
    bool autoClose = false;
    FinalRequest finalRequest = FinalRequest::none;

    if (getAbortStatus() && *getAbortStatus() == AbortTrigger::USER)
        ; //user cancelled => don't run post sync command!
    else
    {
        auto mayRunAfterCountDown = [&](const std::wstring& operationName)
        {
            auto notifyStatusThrowOnCancel = [&](const std::wstring& msg)
            {
                try { reportStatus(msg); /*throw AbortProcess*/ }
                catch (...)
                {
                    if (getAbortStatus() && *getAbortStatus() == AbortTrigger::USER)
                        throw;
                }
            };

            if (progressDlg_->getWindowIfVisible())
                try
                {
                    delayAndCountDown(operationName, std::chrono::seconds(5), notifyStatusThrowOnCancel); //throw AbortProcess
                }
                catch (...) { return false; }

            return true;
        };

        switch (progressDlg_->getOptionPostSyncAction())
        {
            case PostSyncAction2::none:
                autoClose = progressDlg_->getOptionAutoCloseDialog();
                break;
            case PostSyncAction2::exit:
                assert(false);
                break;
            case PostSyncAction2::sleep:
                if (mayRunAfterCountDown(_("System: Sleep")))
                    try
                    {
                        suspendSystem(); //throw FileError
                        autoClose = progressDlg_->getOptionAutoCloseDialog();
                    }
                    catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }
                break;
            case PostSyncAction2::shutdown:
                if (mayRunAfterCountDown(_("System: Shut down")))
                {
                    autoClose = true;
                    finalRequest = FinalRequest::shutdown; //system shutdown must be handled by calling context!
                }
                break;
        }
    }
    if (switchToGuiRequested_) //-> avoid recursive yield() calls, thous switch not before ending batch mode
    {
        autoClose = true;
        finalRequest = FinalRequest::switchGui;
    }

    auto errorLogFinal = std::make_shared<const ErrorLog>(std::move(errorLog_));

    progressDlg_->destroy(autoClose,
                          true /*restoreParentFrame: n/a here*/,
                          finalStatus, errorLogFinal);
    progressDlg_ = nullptr;

    return { finalStatus, finalRequest, logFilePath };
}


void BatchStatusHandler::initNewPhase(int itemsTotal, int64_t bytesTotal, ProcessCallback::Phase phaseID)
{
    StatusHandler::initNewPhase(itemsTotal, bytesTotal, phaseID);
    progressDlg_->initNewPhase(); //call after "StatusHandler::initNewPhase"

    forceUiRefresh(); //throw AbortProcess; OS X needs a full yield to update GUI and get rid of "dummy" texts
}


void BatchStatusHandler::updateDataProcessed(int itemsDelta, int64_t bytesDelta)
{
    StatusHandler::updateDataProcessed(itemsDelta, bytesDelta);

    //note: this method should NOT throw in order to properly allow undoing setting of statistics!
    progressDlg_->notifyProgressChange(); //noexcept
    //for "curveDataBytes_->addRecord()"
}


void BatchStatusHandler::logInfo(const std::wstring& msg)
{
    errorLog_.logMsg(msg, MSG_TYPE_INFO);
}


void BatchStatusHandler::reportWarning(const std::wstring& msg, bool& warningActive)
{
    PauseTimers dummy(*progressDlg_);

    errorLog_.logMsg(msg, MSG_TYPE_WARNING);

    if (!warningActive)
        return;

    if (!progressDlg_->getOptionIgnoreErrors())
        switch (batchErrorHandling_)
        {
            case BatchErrorHandling::showPopup:
            {
                forceUiRefreshNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

                bool dontWarnAgain = false;
                switch (showQuestionDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::warning,
                                           PopupDialogCfg().setDetailInstructions(msg + L"\n\n" + _("You can switch to FreeFileSync's main window to resolve this issue.")).
                                           setCheckBox(dontWarnAgain, _("&Don't show this warning again"), QuestionButton2::no),
                                           _("&Ignore"), _("&Switch")))
                {
                    case QuestionButton2::yes: //ignore
                        warningActive = !dontWarnAgain;
                        break;

                    case QuestionButton2::no: //switch
                        errorLog_.logMsg(_("Switching to FreeFileSync's main window"), MSG_TYPE_INFO);
                        switchToGuiRequested_ = true; //treat as a special kind of cancel
                        userAbortProcessNow(); //throw AbortProcess

                    case QuestionButton2::cancel:
                        userAbortProcessNow(); //throw AbortProcess
                        break;
                }
            }
            break; //keep it! last switch might not find match

            case BatchErrorHandling::cancel:
                abortProcessNow(); //throw AbortProcess (not user-initiated!)
                break;
        }
}


ProcessCallback::Response BatchStatusHandler::reportError(const std::wstring& msg, size_t retryNumber)
{
    PauseTimers dummy(*progressDlg_);

    //auto-retry
    if (retryNumber < automaticRetryCount_)
    {
        errorLog_.logMsg(msg + L"\n-> " + _("Automatic retry"), MSG_TYPE_INFO);
        delayAndCountDown(_("Automatic retry") + (automaticRetryCount_ <= 1 ? L"" :  L" " + numberTo<std::wstring>(retryNumber + 1) + L"/" + numberTo<std::wstring>(automaticRetryCount_)),
        automaticRetryDelay_, [&](const std::wstring& statusMsg) { this->reportStatus(_("Error") + L": " + statusMsg); }); //throw AbortProcess
        return ProcessCallback::retry;
    }

    //always, except for "retry":
    auto guardWriteLog = makeGuard<ScopeGuardRunMode::ON_EXIT>([&] { errorLog_.logMsg(msg, MSG_TYPE_ERROR); });

    if (!progressDlg_->getOptionIgnoreErrors())
    {
        switch (batchErrorHandling_)
        {
            case BatchErrorHandling::showPopup:
            {
                forceUiRefreshNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

                switch (showConfirmationDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::error,
                                               PopupDialogCfg().setDetailInstructions(msg),
                                               _("&Ignore"), _("Ignore &all"), _("&Retry")))
                {
                    case ConfirmationButton3::accept: //ignore
                        return ProcessCallback::ignoreError;

                    case ConfirmationButton3::acceptAll: //ignore all
                        progressDlg_->setOptionIgnoreErrors(true);
                        return ProcessCallback::ignoreError;

                    case ConfirmationButton3::decline: //retry
                        guardWriteLog.dismiss();
                        errorLog_.logMsg(msg + L"\n-> " + _("Retrying operation..."), MSG_TYPE_INFO);
                        return ProcessCallback::retry;

                    case ConfirmationButton3::cancel:
                        userAbortProcessNow(); //throw AbortProcess
                        break;
                }
            }
            break; //used if last switch didn't find a match

            case BatchErrorHandling::cancel:
                abortProcessNow(); //throw AbortProcess (not user-initiated!)
                break;
        }
    }
    else
        return ProcessCallback::ignoreError;

    assert(false);
    return ProcessCallback::ignoreError; //dummy value
}


void BatchStatusHandler::reportFatalError(const std::wstring& msg)
{
    PauseTimers dummy(*progressDlg_);

    errorLog_.logMsg(msg, MSG_TYPE_FATAL_ERROR);

    if (!progressDlg_->getOptionIgnoreErrors())
        switch (batchErrorHandling_)
        {
            case BatchErrorHandling::showPopup:
            {
                forceUiRefreshNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

                switch (showConfirmationDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::error,
                                               PopupDialogCfg().setTitle(_("Serious Error")).
                                               setDetailInstructions(msg),
                                               _("&Ignore"), _("Ignore &all")))
                {
                    case ConfirmationButton2::accept:
                        break;

                    case ConfirmationButton2::acceptAll:
                        progressDlg_->setOptionIgnoreErrors(true);
                        break;

                    case ConfirmationButton2::cancel:
                        userAbortProcessNow(); //throw AbortProcess
                        break;
                }
            }
            break;

            case BatchErrorHandling::cancel:
                abortProcessNow(); //throw AbortProcess (not user-initiated!)
                break;
        }
}


void BatchStatusHandler::forceUiRefreshNoThrow()
{
    progressDlg_->updateGui();
}

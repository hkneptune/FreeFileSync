// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "batch_status_handler.h"
#include <zen/shell_execute.h>
#include <zen/thread.h>
#include <zen/shutdown.h>
#include <wx+/popup_dlg.h>
#include <wx/app.h>
#include "../lib/ffs_paths.h"
#include "../lib/resolve_path.h"
#include "../lib/status_handler_impl.h"
#include "../lib/generate_logfile.h"
#include "../fs/concrete.h"

using namespace zen;
using namespace fff;


namespace
{
//"Backup FreeFileSync 2013-09-15 015052.123.log" ->
//"Backup FreeFileSync 2013-09-15 015052.123 [Error].log"


//return value always bound!
std::unique_ptr<AFS::OutputStream> prepareNewLogfile(const AbstractPath& logFolderPath, //throw FileError
                                                     const std::wstring& jobName,
                                                     const std::chrono::system_clock::time_point& syncStartTime,
                                                     const std::wstring& failStatus,
                                                     const std::function<void(const std::wstring& msg)>& notifyStatus)
{
    assert(!jobName.empty());

    //create logfile folder if required
    AFS::createFolderIfMissingRecursion(logFolderPath); //throw FileError

    //const std::string colon = "\xcb\xb8"; //="modifier letter raised colon" => regular colon is forbidden in file names on Windows and OS X
    //=> too many issues, most notably cmd.exe is not Unicode-aware: https://www.freefilesync.org/forum/viewtopic.php?t=1679

    //assemble logfile name
    const TimeComp tc = getLocalTime(std::chrono::system_clock::to_time_t(syncStartTime));
    if (tc == TimeComp())
        throw FileError(L"Failed to determine current time: " + numberTo<std::wstring>(syncStartTime.time_since_epoch().count()));

    const auto timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(syncStartTime.time_since_epoch()).count() % 1000;

    Zstring logFileName = utfTo<Zstring>(jobName) +
                          Zstr(" ") + formatTime<Zstring>(Zstr("%Y-%m-%d %H%M%S"), tc) +
                          Zstr(".") + printNumber<Zstring>(Zstr("%03d"), static_cast<int>(timeMs)); //[ms] should yield a fairly unique name
    if (!failStatus.empty())
        logFileName += utfTo<Zstring>(L" [" + failStatus + L"]");
    logFileName += Zstr(".log");

    const AbstractPath logFilePath = AFS::appendRelPath(logFolderPath, logFileName);

    auto notifyUnbufferedIO = [notifyStatus,
                               bytesWritten_ = int64_t(0),
                               msg_ = replaceCpy(_("Saving file %x..."), L"%x", fmtPath(AFS::getDisplayPath(logFilePath)))]
         (int64_t bytesDelta) mutable
    {
        if (notifyStatus)
            notifyStatus(msg_ + L" (" + formatFilesizeShort(bytesWritten_ += bytesDelta) + L")"); /*throw X*/
    };

    return AFS::getOutputStream(logFilePath, nullptr, /*streamSize*/ notifyUnbufferedIO); //throw FileError
}


struct LogTraverserCallback: public AFS::TraverserCallback
{
    LogTraverserCallback(const Zstring& prefix, const std::function<void()>& onUpdateStatus) :
        prefix_(prefix),
        onUpdateStatus_(onUpdateStatus) {}

    void onFile(const FileInfo& fi) override
    {
        if (startsWith(fi.itemName, prefix_, CmpFilePath() /*even on Linux!*/) && endsWith(fi.itemName, Zstr(".log"), CmpFilePath()))
            logFileNames_.push_back(fi.itemName);

        if (onUpdateStatus_) onUpdateStatus_();
    }
    std::unique_ptr<TraverserCallback> onFolder (const FolderInfo&  fi) override { return nullptr; }
    HandleLink                         onSymlink(const SymlinkInfo& si) override { return TraverserCallback::LINK_SKIP; }

    HandleError reportDirError (const std::wstring& msg, size_t retryNumber                         ) override { setError(msg); return ON_ERROR_CONTINUE; }
    HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName) override { setError(msg); return ON_ERROR_CONTINUE; }

    const std::vector<Zstring>& refFileNames() const { return logFileNames_; }
    const Opt<FileError>& getLastError() const { return lastError_; }

private:
    void setError(const std::wstring& msg) //implement late failure
    {
        if (!lastError_)
            lastError_ = FileError(msg);
    }

    const Zstring prefix_;
    const std::function<void()> onUpdateStatus_;
    std::vector<Zstring> logFileNames_; //out
    Opt<FileError> lastError_;
};


void limitLogfileCount(const AbstractPath& logFolderPath, const std::wstring& jobname, size_t maxCount, //throw FileError
                       const std::function<void(const std::wstring& msg)>& notifyStatus)
{
    const Zstring prefix = utfTo<Zstring>(jobname);
    const std::wstring cleaningMsg = _("Cleaning up old log files...");;

    LogTraverserCallback lt(prefix, [&] { if (notifyStatus) notifyStatus(cleaningMsg); }); //traverse source directory one level deep
    AFS::traverseFolder(logFolderPath, lt);

    std::vector<Zstring> logFileNames = lt.refFileNames();
    Opt<FileError> lastError = lt.getLastError();

    if (logFileNames.size() > maxCount)
    {
        //delete oldest logfiles: take advantage of logfile naming convention to find them
        std::nth_element(logFileNames.begin(), logFileNames.end() - maxCount, logFileNames.end(), LessFilePath());

        std::for_each(logFileNames.begin(), logFileNames.end() - maxCount, [&](const Zstring& logFileName)
        {
            const AbstractPath filePath = AFS::appendRelPath(logFolderPath, logFileName);
            if (notifyStatus) notifyStatus(cleaningMsg + L" " + fmtPath(AFS::getDisplayPath(filePath)));

            try
            {
                AFS::removeFilePlain(filePath); //throw FileError
            }
            catch (const FileError& e) { if (!lastError) lastError = e; };
        });
    }

    if (lastError) //late failure!
        throw* lastError;
}
}

//##############################################################################################################################

BatchStatusHandler::BatchStatusHandler(bool showProgress,
                                       bool autoCloseDialog,
                                       const std::wstring& jobName,
                                       const Zstring& soundFileSyncComplete,
                                       const std::chrono::system_clock::time_point& startTime,
                                       const Zstring& logFolderPathPhrase, //may be empty
                                       int logfilesCountLimit,
                                       size_t lastSyncsLogFileSizeMax,
                                       bool ignoreErrors,
                                       BatchErrorDialog batchErrorDialog,
                                       size_t automaticRetryCount,
                                       size_t automaticRetryDelay,
                                       FfsReturnCode& returnCode,
                                       const Zstring& postSyncCommand,
                                       PostSyncCondition postSyncCondition,
                                       PostSyncAction postSyncAction) :
    logfilesCountLimit_(logfilesCountLimit),
    lastSyncsLogFileSizeMax_(lastSyncsLogFileSizeMax),
    batchErrorDialog_(batchErrorDialog),
    returnCode_(returnCode),
    automaticRetryCount_(automaticRetryCount),
    automaticRetryDelay_(automaticRetryDelay),
    progressDlg_(createProgressDialog(*this, [this] { this->onProgressDialogTerminate(); }, *this, nullptr /*parentWindow*/, showProgress, autoCloseDialog,
jobName, soundFileSyncComplete, ignoreErrors, [&]
{
    switch (postSyncAction)
    {
        case PostSyncAction::NONE:
            return PostSyncAction2::NONE;
        case PostSyncAction::SLEEP:
            return PostSyncAction2::SLEEP;
        case PostSyncAction::SHUTDOWN:
            return PostSyncAction2::SHUTDOWN;
    }
    assert(false);
    return PostSyncAction2::NONE;
}())),
jobName_(jobName),
         startTime_(startTime),
         logFolderPathPhrase_(logFolderPathPhrase),
         postSyncCommand_(postSyncCommand),
         postSyncCondition_(postSyncCondition)
{
    //ATTENTION: "progressDlg_" is an unmanaged resource!!! However, at this point we already consider construction complete! =>
    //ZEN_ON_SCOPE_FAIL( cleanup(); ); //destructor call would lead to member double clean-up!!!

    //...

    //if (logFile)
    //  ::wxSetEnv(L"logfile", utfTo<wxString>(logFile->getFilename()));
}


BatchStatusHandler::~BatchStatusHandler()
{
    const int totalErrors   = errorLog_.getItemCount(MSG_TYPE_ERROR | MSG_TYPE_FATAL_ERROR); //evaluate before finalizing log
    const int totalWarnings = errorLog_.getItemCount(MSG_TYPE_WARNING);

    //finalize error log
    SyncProgressDialog::SyncResult finalStatus = SyncProgressDialog::RESULT_FINISHED_WITH_SUCCESS;
    std::wstring finalStatusMsg;
    std::wstring failStatus; //additionally indicate errors in log file name
    if (getAbortStatus())
    {
        finalStatus = SyncProgressDialog::RESULT_ABORTED;
        raiseReturnCode(returnCode_, FFS_RC_ABORTED);
        finalStatusMsg = _("Stopped");
        errorLog_.logMsg(finalStatusMsg, MSG_TYPE_ERROR);
        failStatus = _("Stopped");
    }
    else if (totalErrors > 0)
    {
        finalStatus = SyncProgressDialog::RESULT_FINISHED_WITH_ERROR;
        raiseReturnCode(returnCode_, FFS_RC_FINISHED_WITH_ERRORS);
        finalStatusMsg = _("Completed with errors");
        errorLog_.logMsg(finalStatusMsg, MSG_TYPE_ERROR);
        failStatus = _("Error");
    }
    else if (totalWarnings > 0)
    {
        finalStatus = SyncProgressDialog::RESULT_FINISHED_WITH_WARNINGS;
        raiseReturnCode(returnCode_, FFS_RC_FINISHED_WITH_WARNINGS);
        finalStatusMsg = _("Completed with warnings");
        errorLog_.logMsg(finalStatusMsg, MSG_TYPE_WARNING);
        failStatus = _("Warning");
    }
    else
    {
        if (getItemsTotal(PHASE_SYNCHRONIZING) == 0 && //we're past "initNewPhase(PHASE_SYNCHRONIZING)" at this point!
            getBytesTotal(PHASE_SYNCHRONIZING) == 0)
            finalStatusMsg = _("Nothing to synchronize"); //even if "ignored conflicts" occurred!
        else
            finalStatusMsg = _("Completed");
        errorLog_.logMsg(finalStatusMsg, MSG_TYPE_INFO);
    }

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
                    if (finalStatus == SyncProgressDialog::RESULT_ABORTED ||
                        finalStatus == SyncProgressDialog::RESULT_FINISHED_WITH_ERROR)
                        return postSyncCommand_;
                    break;
                case PostSyncCondition::SUCCESS:
                    if (finalStatus == SyncProgressDialog::RESULT_FINISHED_WITH_WARNINGS ||
                        finalStatus == SyncProgressDialog::RESULT_FINISHED_WITH_SUCCESS)
                        return postSyncCommand_;
                    break;
            }
        return Zstring();
    }();
    trim(commandLine);

    if (!commandLine.empty())
        errorLog_.logMsg(replaceCpy(_("Executing command %x"), L"%x", fmtPath(commandLine)), MSG_TYPE_INFO);

    //----------------- write results into user-specified logfile ------------------------
    const SummaryInfo summary =
    {
        jobName_,
        finalStatusMsg,
        getItemsCurrent(PHASE_SYNCHRONIZING), getBytesCurrent(PHASE_SYNCHRONIZING),
        getItemsTotal  (PHASE_SYNCHRONIZING), getBytesTotal  (PHASE_SYNCHRONIZING),
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - startTime_).count()
    };
    if (progressDlg_) progressDlg_->timerSetStatus(false /*active*/); //keep correct summary window stats considering count down timer, system sleep

    //do NOT use tryReportingError()! saving log files should not be cancellable!
    auto notifyStatusNoThrow = [&](const std::wstring& msg)
    {
        try { reportStatus(msg); /*throw X*/ }
        catch (...) {}
    };

    //create not before destruction: 1. avoid issues with FFS trying to sync open log file 2. simplify transactional retry on failure 3. no need to rename log file to include status
    // 4. failure to write to particular stream must not be retried!
    if (logfilesCountLimit_ != 0)
    {
        const AbstractPath logFolderPath = createAbstractPath(trimCpy(logFolderPathPhrase_).empty() ? getDefaultLogFolderPath() : logFolderPathPhrase_); //noexcept

        try
        {
            std::unique_ptr<AFS::OutputStream> logFileStream = prepareNewLogfile(logFolderPath, jobName_, startTime_, failStatus, notifyStatusNoThrow); //throw FileError; return value always bound!

            streamToLogFile(summary, errorLog_, *logFileStream); //throw FileError, (X)
            logFileStream->finalize();                           //throw FileError, (X)
        }
        catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }

        if (logfilesCountLimit_ > 0)
            try
            {
                limitLogfileCount(logFolderPath, jobName_, logfilesCountLimit_, notifyStatusNoThrow); //throw FileError, (X)
            }
            catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }
    }

    try
    {
        saveToLastSyncsLog(summary, errorLog_, lastSyncsLogFileSizeMax_, notifyStatusNoThrow); //throw FileError, (X)
    }
    catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }

    //execute post sync command *after* writing log files, so that user can refer to the log via the command!
    if (!commandLine.empty())
        try
        {
            //use ExecutionType::ASYNC until there is reason not to: https://www.freefilesync.org/forum/viewtopic.php?t=31
            shellExecute(expandMacros(commandLine), ExecutionType::ASYNC); //throw FileError
        }
        catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }

    if (progressDlg_)
    {
        auto mayRunAfterCountDown = [&](const std::wstring& operationName)
        {
            auto notifyStatusThrowOnCancel = [&](const std::wstring& msg)
            {
                try { reportStatus(msg); /*throw X*/ }
                catch (...)
                {
                    if (getAbortStatus() && *getAbortStatus() == AbortTrigger::USER)
                        throw;
                }
            };

            if (progressDlg_->getWindowIfVisible())
                try
                {
                    delayAndCountDown(operationName, 5 /*delayInSec*/, notifyStatusThrowOnCancel); //throw X
                }
                catch (...) { return false; }

            return true;
        };

        //post sync action
        bool autoClose = false;
        if (getAbortStatus() && *getAbortStatus() == AbortTrigger::USER)
            ; //user cancelled => don't run post sync command!
        else
            switch (progressDlg_->getOptionPostSyncAction())
            {
                case PostSyncAction2::NONE:
                    autoClose = progressDlg_->getOptionAutoCloseDialog();
                    break;
                case PostSyncAction2::EXIT:
                    assert(false);
                    break;
                case PostSyncAction2::SLEEP:
                    if (mayRunAfterCountDown(_("System: Sleep")))
                        try
                        {
                            suspendSystem(); //throw FileError
                            autoClose = progressDlg_->getOptionAutoCloseDialog();
                        }
                        catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }
                    break;
                case PostSyncAction2::SHUTDOWN:
                    if (mayRunAfterCountDown(_("System: Shut down")))
                        try
                        {
                            shutdownSystem(); //throw FileError
                            autoClose = true;
                        }
                        catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }
                    break;
            }
        if (switchToGuiRequested_) //-> avoid recursive yield() calls, thous switch not before ending batch mode
            autoClose = true;

        //close progress dialog
        if (autoClose) //warning: wxWindow::Show() is called within showSummary()!
            progressDlg_->closeDirectly(true /*restoreParentFrame: n/a here*/); //progressDlg_ is main window => program will quit shortly after
        else
            //notify about (logical) application main window => program won't quit, but stay on this dialog
            //setMainWindow(progressDlg_->getAsWindow()); -> not required anymore since we block waiting until dialog is closed below
            progressDlg_->showSummary(finalStatus, errorLog_);

        //wait until progress dialog notified shutdown via onProgressDialogTerminate()
        //-> required since it has our "this" pointer captured in lambda "notifyWindowTerminate"!
        //-> nicely manages dialog lifetime
        for (;;)
        {
            wxTheApp->Yield(); //*first* refresh GUI (removing flicker) before sleeping!
            if (!progressDlg_) break;
            std::this_thread::sleep_for(UI_UPDATE_INTERVAL);
        }
    }
}


void BatchStatusHandler::initNewPhase(int itemsTotal, int64_t bytesTotal, ProcessCallback::Phase phaseID)
{
    StatusHandler::initNewPhase(itemsTotal, bytesTotal, phaseID);
    if (progressDlg_)
        progressDlg_->initNewPhase(); //call after "StatusHandler::initNewPhase"

    forceUiRefresh(); //throw X; OS X needs a full yield to update GUI and get rid of "dummy" texts
}


void BatchStatusHandler::updateProcessedData(int itemsDelta, int64_t bytesDelta)
{
    StatusHandler::updateProcessedData(itemsDelta, bytesDelta);

    if (progressDlg_)
        progressDlg_->notifyProgressChange(); //noexcept
    //note: this method should NOT throw in order to properly allow undoing setting of statistics!
}


void BatchStatusHandler::reportInfo(const std::wstring& text)
{
    errorLog_.logMsg(text, MSG_TYPE_INFO); //log first!
    StatusHandler::reportInfo(text); //throw X
}


void BatchStatusHandler::reportWarning(const std::wstring& warningMessage, bool& warningActive)
{
    if (!progressDlg_) abortProcessNow();

    errorLog_.logMsg(warningMessage, MSG_TYPE_WARNING);

    if (!warningActive)
        return;

    if (!progressDlg_->getOptionIgnoreErrors())
        switch (batchErrorDialog_)
        {
            case BatchErrorDialog::SHOW:
            {
                PauseTimers dummy(*progressDlg_);
                forceUiRefreshNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

                bool dontWarnAgain = false;
                switch (showQuestionDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::WARNING, PopupDialogCfg().
                                           setDetailInstructions(warningMessage + L"\n\n" + _("You can switch to FreeFileSync's main window to resolve this issue.")).
                                           setCheckBox(dontWarnAgain, _("&Don't show this warning again"), QuestionButton2::NO),
                                           _("&Ignore"), _("&Switch")))
                {
                    case QuestionButton2::YES: //ignore
                        warningActive = !dontWarnAgain;
                        break;

                    case QuestionButton2::NO: //switch
                        errorLog_.logMsg(_("Switching to FreeFileSync's main window"), MSG_TYPE_INFO);
                        switchToGuiRequested_ = true; //treat as a special kind of cancel
                        userRequestAbort();           //
                        throw BatchRequestSwitchToMainDialog();

                    case QuestionButton2::CANCEL:
                        userAbortProcessNow(); //throw AbortProcess
                        break;
                }
            }
            break; //keep it! last switch might not find match

            case BatchErrorDialog::CANCEL:
                abortProcessNow(); //not user-initiated! throw AbortProcess
                break;
        }
}


ProcessCallback::Response BatchStatusHandler::reportError(const std::wstring& errorMessage, size_t retryNumber)
{
    if (!progressDlg_) abortProcessNow();

    //auto-retry
    if (retryNumber < automaticRetryCount_)
    {
        errorLog_.logMsg(errorMessage + L"\n-> " + _("Automatic retry"), MSG_TYPE_INFO);
        delayAndCountDown(_("Automatic retry"), automaticRetryDelay_, [&](const std::wstring& msg) { this->reportStatus(_("Error") + L": " + msg); });
        return ProcessCallback::RETRY;
    }

    //always, except for "retry":
    auto guardWriteLog = makeGuard<ScopeGuardRunMode::ON_EXIT>([&] { errorLog_.logMsg(errorMessage, MSG_TYPE_ERROR); });

    if (!progressDlg_->getOptionIgnoreErrors())
    {
        switch (batchErrorDialog_)
        {
            case BatchErrorDialog::SHOW:
            {
                PauseTimers dummy(*progressDlg_);
                forceUiRefreshNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

                switch (showConfirmationDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::ERROR2, PopupDialogCfg().
                                               setDetailInstructions(errorMessage),
                                               _("&Ignore"), _("Ignore &all"), _("&Retry")))
                {
                    case ConfirmationButton3::ACCEPT: //ignore
                        return ProcessCallback::IGNORE_ERROR;

                    case ConfirmationButton3::ACCEPT_ALL: //ignore all
                        progressDlg_->setOptionIgnoreErrors(true);
                        return ProcessCallback::IGNORE_ERROR;

                    case ConfirmationButton3::DECLINE: //retry
                        guardWriteLog.dismiss();
                        errorLog_.logMsg(errorMessage + L"\n-> " + _("Retrying operation..."), MSG_TYPE_INFO);
                        return ProcessCallback::RETRY;

                    case ConfirmationButton3::CANCEL:
                        userAbortProcessNow(); //throw AbortProcess
                        break;
                }
            }
            break; //used if last switch didn't find a match

            case BatchErrorDialog::CANCEL:
                abortProcessNow(); //not user-initiated! throw AbortProcess
                break;
        }
    }
    else
        return ProcessCallback::IGNORE_ERROR;

    assert(false);
    return ProcessCallback::IGNORE_ERROR; //dummy value
}


void BatchStatusHandler::reportFatalError(const std::wstring& errorMessage)
{
    if (!progressDlg_) abortProcessNow();

    errorLog_.logMsg(errorMessage, MSG_TYPE_FATAL_ERROR);

    if (!progressDlg_->getOptionIgnoreErrors())
        switch (batchErrorDialog_)
        {
            case BatchErrorDialog::SHOW:
            {
                PauseTimers dummy(*progressDlg_);
                forceUiRefreshNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

                switch (showConfirmationDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::ERROR2,
                                               PopupDialogCfg().setTitle(_("Serious Error")).
                                               setDetailInstructions(errorMessage),
                                               _("&Ignore"), _("Ignore &all")))
                {
                    case ConfirmationButton2::ACCEPT:
                        break;

                    case ConfirmationButton2::ACCEPT_ALL:
                        progressDlg_->setOptionIgnoreErrors(true);
                        break;

                    case ConfirmationButton2::CANCEL:
                        userAbortProcessNow(); //throw AbortProcess
                        break;
                }
            }
            break;

            case BatchErrorDialog::CANCEL:
                abortProcessNow(); //not user-initiated! throw AbortProcess
                break;
        }
}


void BatchStatusHandler::forceUiRefreshNoThrow()
{
    if (progressDlg_)
        progressDlg_->updateGui();
}


void BatchStatusHandler::onProgressDialogTerminate()
{
    progressDlg_ = nullptr;
}

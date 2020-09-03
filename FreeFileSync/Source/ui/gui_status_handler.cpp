// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "gui_status_handler.h"
#include <zen/shell_execute.h>
#include <zen/shutdown.h>
#include <wx/app.h>
#include <wx/sound.h>
#include <wx/wupdlock.h>
#include <wx+/popup_dlg.h>
#include "main_dlg.h"
#include "../afs/concrete.h"
#include "../base/resolve_path.h"
#include "../log_file.h"
#include "status_handler_impl.h"

using namespace zen;
using namespace fff;

namespace
{
const std::chrono::seconds TEMP_PANEL_DISPLAY_DELAY(1);
}

StatusHandlerTemporaryPanel::StatusHandlerTemporaryPanel(MainDialog& dlg,
                                                         const std::chrono::system_clock::time_point& startTime,
                                                         bool ignoreErrors,
                                                         size_t automaticRetryCount,
                                                         std::chrono::seconds automaticRetryDelay) :
    mainDlg_(dlg),
    ignoreErrors_(ignoreErrors),
    automaticRetryCount_(automaticRetryCount),
    automaticRetryDelay_(automaticRetryDelay),
    startTime_(startTime)
{
    mainDlg_.compareStatus_->init(*this, ignoreErrors_, automaticRetryCount_); //clear old values before showing panel

    //showStatsPanel(); => delay and avoid GUI distraction for short-lived tasks

    mainDlg_.Update(); //don't wait until idle event!

    //register keys
    mainDlg_.                Bind(wxEVT_CHAR_HOOK,              &StatusHandlerTemporaryPanel::onLocalKeyEvent, this);
    mainDlg_.m_buttonCancel->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &StatusHandlerTemporaryPanel::onAbortCompare,  this);
}


void StatusHandlerTemporaryPanel::showStatsPanel()
{
    assert(!mainDlg_.auiMgr_.GetPane(mainDlg_.compareStatus_->getAsWindow()).IsShown());
    {
        //------------------------------------------------------------------
        const wxAuiPaneInfo& topPanel = mainDlg_.auiMgr_.GetPane(mainDlg_.m_panelTopButtons);
        wxAuiPaneInfo& statusPanel    = mainDlg_.auiMgr_.GetPane(mainDlg_.compareStatus_->getAsWindow());

        //determine best status panel row near top panel
        switch (topPanel.dock_direction)
        {
            case wxAUI_DOCK_TOP:
            case wxAUI_DOCK_BOTTOM:
                statusPanel.Layer    (topPanel.dock_layer);
                statusPanel.Direction(topPanel.dock_direction);
                statusPanel.Row      (topPanel.dock_row + 1);
                break;

            case wxAUI_DOCK_LEFT:
            case wxAUI_DOCK_RIGHT:
                statusPanel.Layer    (std::max(0, topPanel.dock_layer - 1));
                statusPanel.Direction(wxAUI_DOCK_TOP);
                statusPanel.Row      (0);
                break;
                //case wxAUI_DOCK_CENTRE:
        }

        wxAuiPaneInfoArray& paneArray = mainDlg_.auiMgr_.GetAllPanes();

        const bool statusRowTaken = [&]
        {
            for (size_t i = 0; i < paneArray.size(); ++i)
            {
                const wxAuiPaneInfo& paneInfo = paneArray[i];
                //doesn't matter if paneInfo.IsShown() or not! => move down in either case!
                if (&paneInfo != &statusPanel &&
                    paneInfo.dock_layer     == statusPanel.dock_layer &&
                    paneInfo.dock_direction == statusPanel.dock_direction &&
                    paneInfo.dock_row       == statusPanel.dock_row)
                    return true;
            }
            return false;
        }();

        //move all rows that are in the way one step further
        if (statusRowTaken)
            for (size_t i = 0; i < paneArray.size(); ++i)
            {
                wxAuiPaneInfo& paneInfo = paneArray[i];

                if (&paneInfo != &statusPanel &&
                    paneInfo.dock_layer     == statusPanel.dock_layer &&
                    paneInfo.dock_direction == statusPanel.dock_direction &&
                    paneInfo.dock_row       >= statusPanel.dock_row)
                    ++paneInfo.dock_row;
            }
        //------------------------------------------------------------------

        statusPanel.Show();
        mainDlg_.auiMgr_.Update();
        mainDlg_.compareStatus_->getAsWindow()->Refresh(); //macOS: fix background corruption for the statistics boxes (call *after* wxAuiManager::Update()
    }
}


StatusHandlerTemporaryPanel::~StatusHandlerTemporaryPanel()
{
    //Workaround wxAuiManager crash when starting panel resizing during comparison and holding button until after comparison has finished:
    //- unlike regular window resizing, wxAuiManager does not run a dedicated event loop while the mouse button is held
    //- wxAuiManager internally stores the panel index that is currently resized
    //- our previous hiding of the compare status panel invalidates this index
    // => the next mouse move will have wxAuiManager crash => another fine piece of "wxQuality" code
    // => mitigate:
    wxMouseCaptureLostEvent dummy;
    mainDlg_.auiMgr_.ProcessEvent(dummy); //should be no-op if no mouse buttons are pressed

    mainDlg_.auiMgr_.GetPane(mainDlg_.compareStatus_->getAsWindow()).Hide();
    mainDlg_.auiMgr_.Update();

    //unregister keys
    [[maybe_unused]] bool ubOk1 = mainDlg_.                Unbind(wxEVT_CHAR_HOOK,              &StatusHandlerTemporaryPanel::onLocalKeyEvent, this);
    [[maybe_unused]] bool ubOk2 = mainDlg_.m_buttonCancel->Unbind(wxEVT_COMMAND_BUTTON_CLICKED, &StatusHandlerTemporaryPanel::onAbortCompare,  this);
    assert(ubOk1 && ubOk2);

    mainDlg_.compareStatus_->teardown();

    if (!errorLog_.empty()) //reportResults() was not called!
        std::abort();
}


StatusHandlerTemporaryPanel::Result StatusHandlerTemporaryPanel::reportResults() //noexcept!!
{
    const auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime_);

    //determine post-sync status irrespective of further errors during tear-down
    const SyncResult syncResult = [&]
    {
        if (getAbortStatus())
        {
            errorLog_.logMsg(_("Stopped"), MSG_TYPE_ERROR); //= user cancel
            return SyncResult::aborted;
        }

        const ErrorLog::Stats logCount = errorLog_.getStats();
        if (logCount.error > 0)
            return SyncResult::finishedError;
        else if (logCount.warning > 0)
            return SyncResult::finishedWarning;
        else
            return SyncResult::finishedSuccess;
    }();

    const ProcessSummary summary
    {
        startTime_, syncResult, {} /*jobName*/,
        getStatsCurrent(),
        getStatsTotal  (),
        totalTime
    };

    auto errorLogFinal = std::make_shared<const ErrorLog>(std::move(errorLog_));
    errorLog_ = ErrorLog(); //see check in ~StatusHandlerTemporaryPanel()

    return { summary, errorLogFinal };
}


void StatusHandlerTemporaryPanel::initNewPhase(int itemsTotal, int64_t bytesTotal, ProcessPhase phaseID)
{
    StatusHandler::initNewPhase(itemsTotal, bytesTotal, phaseID);

    mainDlg_.compareStatus_->initNewPhase(); //call after "StatusHandler::initNewPhase"

    //macOS needs a full yield to update GUI and get rid of "dummy" texts
    requestUiUpdate(true /*force*/); //throw AbortProcess
}


void StatusHandlerTemporaryPanel::reportInfo(const std::wstring& msg)
{
    errorLog_.logMsg(msg, MSG_TYPE_INFO);
    updateStatus(msg); //throw AbortProcess
}


void StatusHandlerTemporaryPanel::reportWarning(const std::wstring& msg, bool& warningActive)
{
    PauseTimers dummy(*mainDlg_.compareStatus_);

    errorLog_.logMsg(msg, MSG_TYPE_WARNING);

    if (!warningActive) //if errors are ignored, then warnings should also
        return;

    if (!mainDlg_.compareStatus_->getOptionIgnoreErrors())
    {
        forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        bool dontWarnAgain = false;
        switch (showConfirmationDialog(&mainDlg_, DialogInfoType::warning,
                                       PopupDialogCfg().setDetailInstructions(msg).
                                       setCheckBox(dontWarnAgain, _("&Don't show this warning again")),
                                       _("&Ignore")))
        {
            case ConfirmationButton::accept:
                warningActive = !dontWarnAgain;
                break;
            case ConfirmationButton::cancel:
                abortProcessNow(AbortTrigger::user); //throw AbortProcess
                break;
        }
    }
    //else: if errors are ignored, then warnings should also
}


ProcessCallback::Response StatusHandlerTemporaryPanel::reportError(const std::wstring& msg, size_t retryNumber)
{
    PauseTimers dummy(*mainDlg_.compareStatus_);

    //auto-retry
    if (retryNumber < automaticRetryCount_)
    {
        errorLog_.logMsg(msg + L"\n-> " + _("Automatic retry"), MSG_TYPE_INFO);
        delayAndCountDown(_("Automatic retry") + (automaticRetryCount_ <= 1 ? L"" : L' ' + numberTo<std::wstring>(retryNumber + 1) + L"/" + numberTo<std::wstring>(automaticRetryCount_)),
        automaticRetryDelay_, [&](const std::wstring& statusMsg) { this->updateStatus(_("Error") + L": " + statusMsg); }); //throw AbortProcess
        return ProcessCallback::retry;
    }

    //always, except for "retry":
    auto guardWriteLog = zen::makeGuard<ScopeGuardRunMode::onExit>([&] { errorLog_.logMsg(msg, MSG_TYPE_ERROR); });

    if (!mainDlg_.compareStatus_->getOptionIgnoreErrors())
    {
        forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        switch (showConfirmationDialog(&mainDlg_, DialogInfoType::error,
                                       PopupDialogCfg().setDetailInstructions(msg),
                                       _("&Ignore"), _("Ignore &all"), _("&Retry")))
        {
            case ConfirmationButton3::accept: //ignore
                return ProcessCallback::ignore;

            case ConfirmationButton3::acceptAll: //ignore all
                mainDlg_.compareStatus_->setOptionIgnoreErrors(true);
                return ProcessCallback::ignore;

            case ConfirmationButton3::decline: //retry
                guardWriteLog.dismiss();
                errorLog_.logMsg(msg + L"\n-> " + _("Retrying operation..."), MSG_TYPE_INFO); //explain why there are duplicate "doing operation X" info messages in the log!
                return ProcessCallback::retry;

            case ConfirmationButton3::cancel:
                abortProcessNow(AbortTrigger::user); //throw AbortProcess
                break;
        }
    }
    else
        return ProcessCallback::ignore;

    assert(false);
    return ProcessCallback::ignore; //dummy return value
}


void StatusHandlerTemporaryPanel::reportFatalError(const std::wstring& msg)
{
    PauseTimers dummy(*mainDlg_.compareStatus_);

    errorLog_.logMsg(msg, MSG_TYPE_ERROR);

    if (!mainDlg_.compareStatus_->getOptionIgnoreErrors())
    {
        forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        switch (showConfirmationDialog(&mainDlg_, DialogInfoType::error,
                                       PopupDialogCfg().setTitle(_("Error")).
                                       setDetailInstructions(msg),
                                       _("&Ignore"), _("Ignore &all")))
        {
            case ConfirmationButton2::accept: //ignore
                break;

            case ConfirmationButton2::acceptAll: //ignore all
                mainDlg_.compareStatus_->setOptionIgnoreErrors(true);
                break;

            case ConfirmationButton2::cancel:
                abortProcessNow(AbortTrigger::user); //throw AbortProcess
                break;
        }
    }
}


void StatusHandlerTemporaryPanel::forceUiUpdateNoThrow()
{
    if (!mainDlg_.auiMgr_.GetPane(mainDlg_.compareStatus_->getAsWindow()).IsShown() &&
        std::chrono::steady_clock::now() > startTimeSteady_ + TEMP_PANEL_DISPLAY_DELAY)
        showStatsPanel();

    mainDlg_.compareStatus_->updateGui();
}


void StatusHandlerTemporaryPanel::onLocalKeyEvent(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();
    if (keyCode == WXK_ESCAPE)
    {
        wxCommandEvent dummy;
        onAbortCompare(dummy);
    }

    event.Skip();
}


void StatusHandlerTemporaryPanel::onAbortCompare(wxCommandEvent& event)
{
    userRequestAbort();
}

//########################################################################################################

StatusHandlerFloatingDialog::StatusHandlerFloatingDialog(wxFrame* parentDlg,
                                                         const std::vector<std::wstring>& jobNames,
                                                         const std::chrono::system_clock::time_point& startTime,
                                                         bool ignoreErrors,
                                                         size_t automaticRetryCount,
                                                         std::chrono::seconds automaticRetryDelay,
                                                         const Zstring& soundFileSyncComplete,
                                                         bool& autoCloseDialog) :
    jobNames_(jobNames),
    startTime_(startTime),
    automaticRetryCount_(automaticRetryCount),
    automaticRetryDelay_(automaticRetryDelay),
    soundFileSyncComplete_(soundFileSyncComplete),
    progressDlg_(SyncProgressDialog::create([this] { userRequestAbort(); }, *this, parentDlg, true /*showProgress*/, autoCloseDialog,
jobNames, startTime, ignoreErrors, automaticRetryCount, PostSyncAction2::none)),
autoCloseDialogOut_(autoCloseDialog) {}


StatusHandlerFloatingDialog::~StatusHandlerFloatingDialog()
{
    if (progressDlg_) //reportResults() was not called!
        std::abort();
}


StatusHandlerFloatingDialog::Result StatusHandlerFloatingDialog::reportResults(const Zstring& postSyncCommand, PostSyncCondition postSyncCondition,
                                                                               const Zstring& altLogFolderPathPhrase, int logfilesMaxAgeDays, LogFileFormat logFormat,
                                                                               const std::set<AbstractPath>& logFilePathsToKeep,
                                                                               const std::string& emailNotifyAddress, ResultsNotification emailNotifyCondition)
{
    const auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime_);

    progressDlg_->timerSetStatus(false /*active*/); //keep correct summary window stats considering count down timer, system sleep

    //determine post-sync status irrespective of further errors during tear-down
    const SyncResult syncResult = [&]
    {
        if (getAbortStatus())
        {
            errorLog_.logMsg(_("Stopped"), MSG_TYPE_ERROR); //= user cancel
            return SyncResult::aborted;
        }

        const ErrorLog::Stats logCount = errorLog_.getStats();
        if (logCount.error > 0)
            return SyncResult::finishedError;
        else if (logCount.warning > 0)
            return SyncResult::finishedWarning;

        if (getStatsTotal() == ProgressStats())
            errorLog_.logMsg(_("Nothing to synchronize"), MSG_TYPE_INFO);
        return SyncResult::finishedSuccess;
    }();

    assert(syncResult == SyncResult::aborted || currentPhase() == ProcessPhase::synchronizing);

    const ProcessSummary summary
    {
        startTime_, syncResult, jobNames_,
        getStatsCurrent(),
        getStatsTotal  (),
        totalTime
    };

    const AbstractPath logFilePath = generateLogFilePath(logFormat, summary, altLogFolderPathPhrase);
    //e.g. %AppData%\FreeFileSync\Logs\Backup FreeFileSync 2013-09-15 015052.123 [Error].log

    auto notifyStatusNoThrow = [&](const std::wstring& msg) { try { updateStatus(msg); /*throw AbortProcess*/ } catch (AbortProcess&) {} };

    bool autoClose = false;
    FinalRequest finalRequest = FinalRequest::none;
    bool suspend = false;

    if (getAbortStatus() && *getAbortStatus() == AbortTrigger::user)
        ; /* user cancelled => don't run post sync command
                            => don't send email notification
                            => don't run post sync action
                            => don't play sound notification   */
    else
    {
        //--------------------- post sync command ----------------------
        if (const Zstring cmdLine = trimCpy(postSyncCommand);
            !cmdLine.empty())
            if (postSyncCondition == PostSyncCondition::completion ||
                (postSyncCondition == PostSyncCondition::errors) == (syncResult == SyncResult::aborted ||
                                                                     syncResult == SyncResult::finishedError))
                ////----------------------------------------------------------------------
                //::wxSetEnv(L"logfile_path", AFS::getDisplayPath(logFilePath));
                ////----------------------------------------------------------------------
                runCommandAndLogErrors(expandMacros(cmdLine), errorLog_);

        //--------------------- email notification ----------------------
        if (const std::string notifyEmail = trimCpy(emailNotifyAddress);
            !notifyEmail.empty())
            if (emailNotifyCondition == ResultsNotification::always ||
                (emailNotifyCondition == ResultsNotification::errorWarning && (syncResult == SyncResult::aborted       ||
                                                                               syncResult == SyncResult::finishedError ||
                                                                               syncResult == SyncResult::finishedWarning)) ||
                (emailNotifyCondition == ResultsNotification::errorOnly && (syncResult == SyncResult::aborted ||
                                                                            syncResult == SyncResult::finishedError)))
                try
                {
                    sendLogAsEmail(notifyEmail, summary, errorLog_, logFilePath, notifyStatusNoThrow); //throw FileError
                }
                catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }

        //--------------------- post sync actions ----------------------
        auto mayRunAfterCountDown = [&](const std::wstring& operationName)
        {
            if (progressDlg_->getWindowIfVisible())
                try
                {
                    auto notifyStatusThrowOnCancel = [&](const std::wstring& msg)
                    {
                        try { updateStatus(msg); /*throw AbortProcess*/ }
                        catch (AbortProcess&)
                        {
                            if (getAbortStatus() && *getAbortStatus() == AbortTrigger::user)
                                throw;
                        }
                    };
                    delayAndCountDown(operationName, std::chrono::seconds(5), notifyStatusThrowOnCancel); //throw AbortProcess
                }
                catch (AbortProcess&) { return false; }

            return true;
        };

        switch (progressDlg_->getOptionPostSyncAction())
        {
            case PostSyncAction2::none:
                autoClose = progressDlg_->getOptionAutoCloseDialog();
                break;
            case PostSyncAction2::exit:
                autoClose = true;
                finalRequest = FinalRequest::exit; //program exit must be handled by calling context!
                break;
            case PostSyncAction2::sleep:
                if (mayRunAfterCountDown(_("System: Sleep")))
                {
                    autoClose = progressDlg_->getOptionAutoCloseDialog();
                    suspend = true;
                }
                break;
            case PostSyncAction2::shutdown:
                if (mayRunAfterCountDown(_("System: Shut down")))
                {
                    autoClose = true;
                    finalRequest = FinalRequest::shutdown; //system shutdown must be handled by calling context!
                }
                break;
        }

        //--------------------- sound notification ----------------------
        if (!autoClose) //only play when showing results dialog
            if (!soundFileSyncComplete_.empty())
            {
                //wxWidgets shows modal error dialog by default => NO!
                wxLog* oldLogTarget = wxLog::SetActiveTarget(new wxLogStderr); //transfer and receive ownership!
                ZEN_ON_SCOPE_EXIT(delete wxLog::SetActiveTarget(oldLogTarget));

                wxSound::Play(utfTo<wxString>(soundFileSyncComplete_), wxSOUND_ASYNC);
            }
        //if (::GetForegroundWindow() != GetHWND())
        //  RequestUserAttention(); -> probably too much since task bar is already colorized with Taskbar::STATUS_ERROR or STATUS_NORMAL
    }

    //--------------------- save log file ----------------------
    try //create not before destruction: 1. avoid issues with FFS trying to sync open log file 2. include status in log file name without extra rename
    {
        //do NOT use tryReportingError()! saving log files should not be cancellable!
        saveLogFile(logFilePath, summary, errorLog_, logfilesMaxAgeDays, logFormat, logFilePathsToKeep, notifyStatusNoThrow); //throw FileError
    }
    catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }
    //----------------------------------------------------------


    if (suspend) //...*before* results dialog is shown
        try
        {
            suspendSystem(); //throw FileError
        }
        catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }


    auto errorLogFinal = makeSharedRef<const ErrorLog>(std::move(errorLog_));

    autoCloseDialogOut_ = //output parameter owned by SyncProgressDialog (evaluate *after* user closed the results dialog)
        progressDlg_->destroy(autoClose,
                              finalRequest == FinalRequest::none /*restoreParentFrame*/,
                              syncResult, errorLogFinal).autoCloseDialog;
    progressDlg_ = nullptr;

    return { summary, errorLogFinal, finalRequest, logFilePath };
}


void StatusHandlerFloatingDialog::initNewPhase(int itemsTotal, int64_t bytesTotal, ProcessPhase phaseID)
{
    assert(phaseID == ProcessPhase::synchronizing);
    StatusHandler::initNewPhase(itemsTotal, bytesTotal, phaseID);
    progressDlg_->initNewPhase(); //call after "StatusHandler::initNewPhase"

    //macOS needs a full yield to update GUI and get rid of "dummy" texts
    requestUiUpdate(true /*force*/); //throw AbortProcess
}


void StatusHandlerFloatingDialog::reportInfo(const std::wstring& msg)
{
    errorLog_.logMsg(msg, MSG_TYPE_INFO);
    updateStatus(msg); //throw AbortProcess
}


void StatusHandlerFloatingDialog::reportWarning(const std::wstring& msg, bool& warningActive)
{
    PauseTimers dummy(*progressDlg_);

    errorLog_.logMsg(msg, MSG_TYPE_WARNING);

    if (!warningActive)
        return;

    if (!progressDlg_->getOptionIgnoreErrors())
    {
        forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        bool dontWarnAgain = false;
        switch (showConfirmationDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::warning,
                                       PopupDialogCfg().setDetailInstructions(msg).
                                       setCheckBox(dontWarnAgain, _("&Don't show this warning again")),
                                       _("&Ignore")))
        {
            case ConfirmationButton::accept:
                warningActive = !dontWarnAgain;
                break;
            case ConfirmationButton::cancel:
                abortProcessNow(AbortTrigger::user); //throw AbortProcess
                break;
        }
    }
    //else: if errors are ignored, then warnings should be, too
}


ProcessCallback::Response StatusHandlerFloatingDialog::reportError(const std::wstring& msg, size_t retryNumber)
{
    PauseTimers dummy(*progressDlg_);

    //auto-retry
    if (retryNumber < automaticRetryCount_)
    {
        errorLog_.logMsg(msg + L"\n-> " + _("Automatic retry"), MSG_TYPE_INFO);
        delayAndCountDown(_("Automatic retry") + (automaticRetryCount_ <= 1 ? L"" : L' ' + numberTo<std::wstring>(retryNumber + 1) + L"/" + numberTo<std::wstring>(automaticRetryCount_)),
        automaticRetryDelay_, [&](const std::wstring& statusMsg) { this->updateStatus(_("Error") + L": " + statusMsg); }); //throw AbortProcess
        return ProcessCallback::retry;
    }

    //always, except for "retry":
    auto guardWriteLog = zen::makeGuard<ScopeGuardRunMode::onExit>([&] { errorLog_.logMsg(msg, MSG_TYPE_ERROR); });

    if (!progressDlg_->getOptionIgnoreErrors())
    {
        forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        switch (showConfirmationDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::error,
                                       PopupDialogCfg().setDetailInstructions(msg),
                                       _("&Ignore"), _("Ignore &all"), _("&Retry")))
        {
            case ConfirmationButton3::accept: //ignore
                return ProcessCallback::ignore;

            case ConfirmationButton3::acceptAll: //ignore all
                progressDlg_->setOptionIgnoreErrors(true);
                return ProcessCallback::ignore;

            case ConfirmationButton3::decline: //retry
                guardWriteLog.dismiss();
                errorLog_.logMsg(msg + L"\n-> " + _("Retrying operation..."), MSG_TYPE_INFO); //explain why there are duplicate "doing operation X" info messages in the log!
                return ProcessCallback::retry;

            case ConfirmationButton3::cancel:
                abortProcessNow(AbortTrigger::user); //throw AbortProcess
                break;
        }
    }
    else
        return ProcessCallback::ignore;

    assert(false);
    return ProcessCallback::ignore; //dummy value
}


void StatusHandlerFloatingDialog::reportFatalError(const std::wstring& msg)
{
    PauseTimers dummy(*progressDlg_);

    errorLog_.logMsg(msg, MSG_TYPE_ERROR);

    if (!progressDlg_->getOptionIgnoreErrors())
    {
        forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        switch (showConfirmationDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::error,
                                       PopupDialogCfg().setTitle(_("Error")).
                                       setDetailInstructions(msg),
                                       _("&Ignore"), _("Ignore &all")))
        {
            case ConfirmationButton2::accept: //ignore
                break;

            case ConfirmationButton2::acceptAll: //ignore all
                progressDlg_->setOptionIgnoreErrors(true);
                break;

            case ConfirmationButton2::cancel:
                abortProcessNow(AbortTrigger::user); //throw AbortProcess
                break;
        }
    }
}


void StatusHandlerFloatingDialog::updateDataProcessed(int itemsDelta, int64_t bytesDelta) //noexcept!
{
    StatusHandler::updateDataProcessed(itemsDelta, bytesDelta);

    //note: this method should NOT throw in order to properly allow undoing setting of statistics!
    progressDlg_->notifyProgressChange(); //noexcept
    //for "curveDataBytes_->addRecord()"
}


void StatusHandlerFloatingDialog::forceUiUpdateNoThrow()
{
    progressDlg_->updateGui();
}

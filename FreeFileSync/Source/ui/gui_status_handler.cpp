// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "gui_status_handler.h"
#include <zen/shell_execute.h>
#include <zen/shutdown.h>
#include <wx/app.h>
#include <wx/wupdlock.h>
#include <wx+/bitmap_button.h>
#include <wx+/popup_dlg.h>
#include "main_dlg.h"
#include "../lib/generate_logfile.h"
#include "../lib/resolve_path.h"
#include "../lib/status_handler_impl.h"

using namespace zen;
using namespace fff;


StatusHandlerTemporaryPanel::StatusHandlerTemporaryPanel(MainDialog& dlg) : mainDlg_(dlg)
{
    {
        mainDlg_.compareStatus_->init(*this, false /*ignoreErrors*/); //clear old values before showing panel

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
                wxAuiPaneInfo& paneInfo = paneArray[i];

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
    }

    mainDlg_.Update(); //don't wait until idle event!

    //register keys
    mainDlg_.Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(StatusHandlerTemporaryPanel::OnKeyPressed), nullptr, this);
    mainDlg_.m_buttonCancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(StatusHandlerTemporaryPanel::OnAbortCompare), nullptr, this);
}


StatusHandlerTemporaryPanel::~StatusHandlerTemporaryPanel()
{
    //unregister keys
    mainDlg_.Disconnect(wxEVT_CHAR_HOOK, wxKeyEventHandler(StatusHandlerTemporaryPanel::OnKeyPressed), nullptr, this);
    mainDlg_.m_buttonCancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(StatusHandlerTemporaryPanel::OnAbortCompare), nullptr, this);

    mainDlg_.auiMgr_.GetPane(mainDlg_.compareStatus_->getAsWindow()).Hide();
    mainDlg_.auiMgr_.Update();
    mainDlg_.compareStatus_->teardown();
}


void StatusHandlerTemporaryPanel::OnKeyPressed(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();
    if (keyCode == WXK_ESCAPE)
    {
        wxCommandEvent dummy;
        OnAbortCompare(dummy);
    }

    event.Skip();
}


void StatusHandlerTemporaryPanel::initNewPhase(int itemsTotal, int64_t bytesTotal, Phase phaseID)
{
    StatusHandler::initNewPhase(itemsTotal, bytesTotal, phaseID);

    mainDlg_.compareStatus_->initNewPhase(); //call after "StatusHandler::initNewPhase"

    forceUiRefresh(); //throw X; OS X needs a full yield to update GUI and get rid of "dummy" texts
}


void StatusHandlerTemporaryPanel::reportInfo(const std::wstring& text)
{
    errorLog_.logMsg(text, MSG_TYPE_INFO); //log first!
    StatusHandler::reportInfo(text); //throw X
}


ProcessCallback::Response StatusHandlerTemporaryPanel::reportError(const std::wstring& errorMessage, size_t retryNumber)
{
    //no need to implement auto-retry here: 1. user is watching 2. comparison is fast
    //=> similar behavior like "ignoreErrors" which is also not used for the comparison phase in GUI mode

    //always, except for "retry":
    auto guardWriteLog = zen::makeGuard<ScopeGuardRunMode::ON_EXIT>([&] { errorLog_.logMsg(errorMessage, MSG_TYPE_ERROR); });

    if (!mainDlg_.compareStatus_->getOptionIgnoreErrors())
    {
        forceUiRefreshNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        switch (showConfirmationDialog(&mainDlg_, DialogInfoType::ERROR2, PopupDialogCfg().
                                       setDetailInstructions(errorMessage),
                                       _("&Ignore"), _("Ignore &all"), _("&Retry")))
        {
            case ConfirmationButton3::ACCEPT: //ignore
                return ProcessCallback::IGNORE_ERROR;

            case ConfirmationButton3::ACCEPT_ALL: //ignore all
                mainDlg_.compareStatus_->setOptionIgnoreErrors(true);
                return ProcessCallback::IGNORE_ERROR;

            case ConfirmationButton3::DECLINE: //retry
                guardWriteLog.dismiss();
                errorLog_.logMsg(errorMessage + L"\n-> " + _("Retrying operation..."), MSG_TYPE_INFO); //explain why there are duplicate "doing operation X" info messages in the log!
                return ProcessCallback::RETRY;

            case ConfirmationButton3::CANCEL:
                userAbortProcessNow(); //throw AbortProcess
                break;
        }
    }
    else
        return ProcessCallback::IGNORE_ERROR;

    assert(false);
    return ProcessCallback::IGNORE_ERROR; //dummy return value
}


void StatusHandlerTemporaryPanel::reportFatalError(const std::wstring& errorMessage)
{
    errorLog_.logMsg(errorMessage, MSG_TYPE_FATAL_ERROR);

    forceUiRefreshNoThrow(); //noexcept! => don't throw here when error occurs during clean up!
    showNotificationDialog(&mainDlg_, DialogInfoType::ERROR2, PopupDialogCfg().setTitle(_("Serious Error")).setDetailInstructions(errorMessage));
}


void StatusHandlerTemporaryPanel::reportWarning(const std::wstring& warningMessage, bool& warningActive)
{
    errorLog_.logMsg(warningMessage, MSG_TYPE_WARNING);

    if (!warningActive) //if errors are ignored, then warnings should also
        return;

    if (!mainDlg_.compareStatus_->getOptionIgnoreErrors())
    {
        forceUiRefreshNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        bool dontWarnAgain = false;
        switch (showConfirmationDialog(&mainDlg_, DialogInfoType::WARNING,
                                       PopupDialogCfg().setDetailInstructions(warningMessage).
                                       setCheckBox(dontWarnAgain, _("&Don't show this warning again")),
                                       _("&Ignore")))
        {
            case ConfirmationButton::ACCEPT:
                warningActive = !dontWarnAgain;
                break;
            case ConfirmationButton::CANCEL:
                userAbortProcessNow(); //throw AbortProcess
                break;
        }
    }
    //else: if errors are ignored, then warnings should also
}


void StatusHandlerTemporaryPanel::forceUiRefreshNoThrow()
{
    mainDlg_.compareStatus_->updateGui();
}


void StatusHandlerTemporaryPanel::OnAbortCompare(wxCommandEvent& event)
{
    userRequestAbort();
}

//########################################################################################################

StatusHandlerFloatingDialog::StatusHandlerFloatingDialog(wxFrame* parentDlg,
                                                         const std::chrono::system_clock::time_point& startTime,
                                                         size_t lastSyncsLogFileSizeMax,
                                                         bool ignoreErrors,
                                                         size_t automaticRetryCount,
                                                         size_t automaticRetryDelay,
                                                         const std::wstring& jobName,
                                                         const Zstring& soundFileSyncComplete,
                                                         const Zstring& postSyncCommand,
                                                         PostSyncCondition postSyncCondition,
                                                         bool& exitAfterSync,
                                                         bool& autoCloseDialog) :
    progressDlg_(createProgressDialog(*this, [this] { this->onProgressDialogTerminate(); }, *this, parentDlg, true /*showProgress*/, autoCloseDialog,
jobName, soundFileSyncComplete, ignoreErrors, PostSyncAction2::NONE)),
         lastSyncsLogFileSizeMax_(lastSyncsLogFileSizeMax),
         automaticRetryCount_(automaticRetryCount),
         automaticRetryDelay_(automaticRetryDelay),
         jobName_(jobName),
         startTime_(startTime),
         postSyncCommand_(postSyncCommand),
         postSyncCondition_(postSyncCondition),
         exitAfterSync_(exitAfterSync),
         autoCloseDialogOut_(autoCloseDialog)
{
    assert(!exitAfterSync);
}


StatusHandlerFloatingDialog::~StatusHandlerFloatingDialog()
{
    const int totalErrors   = errorLog_.getItemCount(MSG_TYPE_ERROR | MSG_TYPE_FATAL_ERROR); //evaluate before finalizing log
    const int totalWarnings = errorLog_.getItemCount(MSG_TYPE_WARNING);

    //finalize error log
    SyncProgressDialog::SyncResult finalStatus = SyncProgressDialog::RESULT_FINISHED_WITH_SUCCESS;
    std::wstring finalStatusMsg;
    if (getAbortStatus())
    {
        finalStatus = SyncProgressDialog::RESULT_ABORTED;
        finalStatusMsg = _("Stopped");
        errorLog_.logMsg(finalStatusMsg, MSG_TYPE_ERROR);
    }
    else if (totalErrors > 0)
    {
        finalStatus = SyncProgressDialog::RESULT_FINISHED_WITH_ERROR;
        finalStatusMsg = _("Completed with errors");
        errorLog_.logMsg(finalStatusMsg, MSG_TYPE_ERROR);
    }
    else if (totalWarnings > 0)
    {
        finalStatus = SyncProgressDialog::RESULT_FINISHED_WITH_WARNINGS;
        finalStatusMsg = _("Completed with warnings");
        errorLog_.logMsg(finalStatusMsg, MSG_TYPE_WARNING); //give status code same warning priority as display category!
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

    //----------------- write results into LastSyncs.log------------------------
    const SummaryInfo summary =
    {
        jobName_, finalStatusMsg,
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
                    autoClose = exitAfterSync_ = true; //program exit must be handled by calling context!
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
                            autoClose = exitAfterSync_ = true;
                        }
                        catch (const FileError& e) { errorLog_.logMsg(e.toString(), MSG_TYPE_ERROR); }
                    break;
            }

        //close progress dialog
        if (autoClose)
            progressDlg_->closeDirectly(!exitAfterSync_ /*restoreParentFrame*/);
        else
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


void StatusHandlerFloatingDialog::initNewPhase(int itemsTotal, int64_t bytesTotal, Phase phaseID)
{
    assert(phaseID == PHASE_SYNCHRONIZING);
    StatusHandler::initNewPhase(itemsTotal, bytesTotal, phaseID);
    if (progressDlg_)
        progressDlg_->initNewPhase(); //call after "StatusHandler::initNewPhase"

    forceUiRefresh(); //throw X; OS X needs a full yield to update GUI and get rid of "dummy" texts
}


void StatusHandlerFloatingDialog::updateProcessedData(int itemsDelta, int64_t bytesDelta)
{
    StatusHandler::updateProcessedData(itemsDelta, bytesDelta);
    if (progressDlg_)
        progressDlg_->notifyProgressChange(); //noexcept
    //note: this method should NOT throw in order to properly allow undoing setting of statistics!
}


void StatusHandlerFloatingDialog::reportInfo(const std::wstring& text)
{
    errorLog_.logMsg(text, MSG_TYPE_INFO); //log first!
    StatusHandler::reportInfo(text); //throw X
}


ProcessCallback::Response StatusHandlerFloatingDialog::reportError(const std::wstring& errorMessage, size_t retryNumber)
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
    auto guardWriteLog = zen::makeGuard<ScopeGuardRunMode::ON_EXIT>([&] { errorLog_.logMsg(errorMessage, MSG_TYPE_ERROR); });

    if (!progressDlg_->getOptionIgnoreErrors())
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
                errorLog_.logMsg(errorMessage + L"\n-> " + _("Retrying operation..."), MSG_TYPE_INFO); //explain why there are duplicate "doing operation X" info messages in the log!
                return ProcessCallback::RETRY;

            case ConfirmationButton3::CANCEL:
                userAbortProcessNow(); //throw AbortProcess
                break;
        }
    }
    else
        return ProcessCallback::IGNORE_ERROR;

    assert(false);
    return ProcessCallback::IGNORE_ERROR; //dummy value
}


void StatusHandlerFloatingDialog::reportFatalError(const std::wstring& errorMessage)
{
    if (!progressDlg_) abortProcessNow();

    errorLog_.logMsg(errorMessage, MSG_TYPE_FATAL_ERROR);

    if (!progressDlg_->getOptionIgnoreErrors())
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
}


void StatusHandlerFloatingDialog::reportWarning(const std::wstring& warningMessage, bool& warningActive)
{
    if (!progressDlg_) abortProcessNow();

    errorLog_.logMsg(warningMessage, MSG_TYPE_WARNING);

    if (!warningActive)
        return;

    if (!progressDlg_->getOptionIgnoreErrors())
    {
        PauseTimers dummy(*progressDlg_);
        forceUiRefreshNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        bool dontWarnAgain = false;
        switch (showConfirmationDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::WARNING,
                                       PopupDialogCfg().setDetailInstructions(warningMessage).
                                       setCheckBox(dontWarnAgain, _("&Don't show this warning again")),
                                       _("&Ignore")))
        {
            case ConfirmationButton::ACCEPT:
                warningActive = !dontWarnAgain;
                break;
            case ConfirmationButton::CANCEL:
                userAbortProcessNow(); //throw AbortProcess
                break;
        }
    }
    //else: if errors are ignored, then warnings should be, too
}


void StatusHandlerFloatingDialog::forceUiRefreshNoThrow()
{
    if (progressDlg_)
        progressDlg_->updateGui();
}


void StatusHandlerFloatingDialog::onProgressDialogTerminate()
{
    //output parameters owned by SyncProgressDialog
    if (progressDlg_)
        autoCloseDialogOut_ = progressDlg_->getOptionAutoCloseDialog();

    progressDlg_ = nullptr;
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "gui_status_handler.h"
#include <zen/shutdown.h>
#include <wx/app.h>
#include <wx/sound.h>
#include <wx/wupdlock.h>

using namespace zen;
using namespace fff;

namespace
{
constexpr std::chrono::seconds TEMP_PANEL_DISPLAY_DELAY(1);
}

StatusHandlerTemporaryPanel::StatusHandlerTemporaryPanel(MainDialog& dlg,
                                                         const std::chrono::system_clock::time_point& startTime,
                                                         bool ignoreErrors,
                                                         size_t autoRetryCount,
                                                         std::chrono::seconds autoRetryDelay,
                                                         const Zstring& soundFileAlertPending) :
    mainDlg_(dlg),
    ignoreErrors_(ignoreErrors),
    autoRetryCount_(autoRetryCount),
    autoRetryDelay_(autoRetryDelay),
    soundFileAlertPending_(soundFileAlertPending),
    startTime_(startTime)
{
    mainDlg_.compareStatus_->init(*this, ignoreErrors_, autoRetryCount_); //clear old values before showing panel

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
    if (!errorLog_.empty()) //prepareResult() was not called!
        std::abort();

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
}


StatusHandlerTemporaryPanel::Result StatusHandlerTemporaryPanel::prepareResult() //noexcept!!
{
    const std::chrono::milliseconds totalTime = mainDlg_.compareStatus_->pauseAndGetTotalTime();

    //append "extra" log for sync errors that could not otherwise be reported:
    if (const ErrorLog extraLog = fetchExtraLog();
        !extraLog.empty())
    {
        append(errorLog_, extraLog);
        std::stable_sort(errorLog_.begin(), errorLog_.end(), [](const LogEntry& lhs, const LogEntry& rhs) { return lhs.time < rhs.time; });
    }

    //determine post-sync status irrespective of further errors during tear-down
    const TaskResult syncResult = [&]
    {
        if (taskCancelled())
        {
            logMsg(errorLog_, _("Stopped"), MSG_TYPE_ERROR); //= user cancel
            return TaskResult::cancelled;
        }

        const ErrorLogStats logCount = getStats(errorLog_);
        if (logCount.error > 0)
            return TaskResult::error;
        else if (logCount.warning > 0)
            return TaskResult::warning;
        else
            return TaskResult::success;
    }();

    const ProcessSummary summary
    {
        startTime_, syncResult, {} /*jobNames*/,
        getCurrentStats(),
        getTotalStats  (),
        totalTime
    };

    return {summary, makeSharedRef<const ErrorLog>(std::exchange(errorLog_, {}))}; //see check in ~StatusHandlerTemporaryPanel()
}


void StatusHandlerTemporaryPanel::initNewPhase(int itemsTotal, int64_t bytesTotal, ProcessPhase phaseID)
{
    StatusHandler::initNewPhase(itemsTotal, bytesTotal, phaseID);

    mainDlg_.compareStatus_->initNewPhase(); //call after "StatusHandler::initNewPhase"

    //macOS needs a full yield to update GUI and get rid of "dummy" texts
    requestUiUpdate(true /*force*/); //throw CancelProcess
}


void StatusHandlerTemporaryPanel::logMessage(const std::wstring& msg, MsgType type)
{
    logMsg(errorLog_, msg, [&]
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


void StatusHandlerTemporaryPanel::reportWarning(const std::wstring& msg, bool& warningActive)
{
    PauseTimers dummy(*mainDlg_.compareStatus_);

    logMsg(errorLog_, msg, MSG_TYPE_WARNING);

    if (!warningActive) //if errors are ignored, then warnings should also
        return;

    if (!mainDlg_.compareStatus_->getOptionIgnoreErrors())
    {
        forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        bool dontWarnAgain = false;
        switch (showConfirmationDialog(&mainDlg_, DialogInfoType::warning,
                                       PopupDialogCfg().setDetailInstructions(msg).
                                       alertWhenPending(soundFileAlertPending_).
                                       setCheckBox(dontWarnAgain, _("&Don't show this warning again")),
                                       _("&Ignore")))
        {
            case ConfirmationButton::accept:
                warningActive = !dontWarnAgain;
                break;
            case ConfirmationButton::cancel:
                cancelProcessNow(CancelReason::user); //throw CancelProcess
                break;
        }
    }
    //else: if errors are ignored, then warnings should also
}


ProcessCallback::Response StatusHandlerTemporaryPanel::reportError(const ErrorInfo& errorInfo)
{
    PauseTimers dummy(*mainDlg_.compareStatus_);

    //log actual fail time (not "now"!)
    const time_t failTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() -
                                                                 std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::steady_clock::now() - errorInfo.failTime));
    //auto-retry
    if (errorInfo.retryNumber < autoRetryCount_)
    {
        logMsg(errorLog_, errorInfo.msg + L"\n-> " + _("Automatic retry"), MSG_TYPE_INFO, failTime);
        delayAndCountDown(errorInfo.failTime + autoRetryDelay_,
                          [&, statusPrefix  = _("Automatic retry") +
                                              (errorInfo.retryNumber == 0 ? L"" : L' ' + formatNumber(errorInfo.retryNumber + 1)) + SPACED_DASH,
                              statusPostfix = SPACED_DASH + _("Error") + L": " + replaceCpy(errorInfo.msg, L'\n', L' ')](const std::wstring& timeRemMsg)
        { this->updateStatus(statusPrefix + timeRemMsg + statusPostfix); }); //throw CancelProcess
        return ProcessCallback::retry;
    }

    //always, except for "retry":
    auto guardWriteLog = makeGuard<ScopeGuardRunMode::onExit>([&] { logMsg(errorLog_, errorInfo.msg, MSG_TYPE_ERROR, failTime); });

    if (!mainDlg_.compareStatus_->getOptionIgnoreErrors())
    {
        forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        switch (showConfirmationDialog(&mainDlg_, DialogInfoType::error,
                                       PopupDialogCfg().setDetailInstructions(errorInfo.msg).
                                       alertWhenPending(soundFileAlertPending_),
                                       _("&Ignore"), _("Ignore &all"), _("&Retry")))
        {
            case ConfirmationButton3::accept: //ignore
                return ProcessCallback::ignore;

            case ConfirmationButton3::accept2: //ignore all
                mainDlg_.compareStatus_->setOptionIgnoreErrors(true);
                return ProcessCallback::ignore;

            case ConfirmationButton3::decline: //retry
                guardWriteLog.dismiss();
                logMsg(errorLog_, errorInfo.msg + L"\n-> " + _("Retrying operation..."), //explain why there are duplicate "doing operation X" info messages in the log!
                       MSG_TYPE_INFO, failTime);
                return ProcessCallback::retry;

            case ConfirmationButton3::cancel:
                cancelProcessNow(CancelReason::user); //throw CancelProcess
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

    logMsg(errorLog_, msg, MSG_TYPE_ERROR);

    if (!mainDlg_.compareStatus_->getOptionIgnoreErrors())
    {
        forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        switch (showConfirmationDialog(&mainDlg_, DialogInfoType::error,
                                       PopupDialogCfg().setDetailInstructions(msg).
                                       alertWhenPending(soundFileAlertPending_),
                                       _("&Ignore"), _("Ignore &all")))
        {
            case ConfirmationButton2::accept: //ignore
                break;

            case ConfirmationButton2::accept2: //ignore all
                mainDlg_.compareStatus_->setOptionIgnoreErrors(true);
                break;

            case ConfirmationButton2::cancel:
                cancelProcessNow(CancelReason::user); //throw CancelProcess
                break;
        }
    }
}


Statistics::ErrorStats StatusHandlerTemporaryPanel::getErrorStats() const
{
    //errorLog_ is an "append only" structure, so we can make getErrorStats() complexity "constant time":
    std::for_each(errorLog_.begin() + errorStatsRowsChecked_, errorLog_.end(), [&](const LogEntry& entry)
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
    errorStatsRowsChecked_ = errorLog_.size();

    return errorStatsBuf_;
}


void StatusHandlerTemporaryPanel::forceUiUpdateNoThrow()
{
    if (!mainDlg_.auiMgr_.GetPane(mainDlg_.compareStatus_->getAsWindow()).IsShown() &&
        std::chrono::steady_clock::now() > panelInitTime_ + TEMP_PANEL_DISPLAY_DELAY)
        showStatsPanel();

    mainDlg_.compareStatus_->updateGui();
}


void StatusHandlerTemporaryPanel::onLocalKeyEvent(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();
    if (keyCode == WXK_ESCAPE)
        return userRequestCancel();

    event.Skip();
}


void StatusHandlerTemporaryPanel::onAbortCompare(wxCommandEvent& event)
{
    userRequestCancel();
}

//########################################################################################################

StatusHandlerFloatingDialog::StatusHandlerFloatingDialog(wxFrame* parentDlg,
                                                         const std::vector<std::wstring>& jobNames,
                                                         const std::chrono::system_clock::time_point& startTime,
                                                         bool ignoreErrors,
                                                         size_t autoRetryCount,
                                                         std::chrono::seconds autoRetryDelay,
                                                         const Zstring& soundFileSyncComplete,
                                                         const Zstring& soundFileAlertPending,
                                                         const WindowLayout::Dimensions& dim,
                                                         bool autoCloseDialog) :
    jobNames_(jobNames),
    startTime_(startTime),
    autoRetryCount_(autoRetryCount),
    autoRetryDelay_(autoRetryDelay),
    soundFileSyncComplete_(soundFileSyncComplete),
    soundFileAlertPending_(soundFileAlertPending)
{
    //set *after* initializer list => callbacks during construction to getErrorStats()!
    progressDlg_ = SyncProgressDialog::create(dim, [this] { userRequestCancel(); }, *this, parentDlg, true /*showProgress*/, autoCloseDialog,
                                              jobNames, std::chrono::system_clock::to_time_t(startTime), ignoreErrors, autoRetryCount, PostSyncAction::none);
}


StatusHandlerFloatingDialog::~StatusHandlerFloatingDialog()
{
    if (progressDlg_) //showResults() was not called!
        std::abort();
}


StatusHandlerFloatingDialog::Result StatusHandlerFloatingDialog::prepareResult()
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
        if (taskCancelled()) //= user cancel
        {
            assert(*taskCancelled() == CancelReason::user); //"stop on first error" is ffs_batch-only
            logMsg(errorLog_.ref(), _("Stopped"), MSG_TYPE_ERROR);
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
        startTime_, *syncResult_, jobNames_,
        getCurrentStats(),
        getTotalStats  (),
        totalTime
    };

    return {summary, errorLog_};
}


StatusHandlerFloatingDialog::DlgOptions StatusHandlerFloatingDialog::showResult()
{
    bool autoClose = false;
    bool suspend = false;
    FinalRequest finalRequest = FinalRequest::none;

    if (taskCancelled())
        assert(*taskCancelled() == CancelReason::user); //"stop on first error" is only for ffs_batch
    else
    {
        //--------------------- post sync actions ----------------------
        //give user chance to cancel shutdown; do *not* consider the sync itself cancelled
        auto proceedWithShutdown = [&](const std::wstring& operationName)
        {
            if (progressDlg_->getWindowIfVisible())
                try
                {
                    assert(!endsWith(operationName, L"."));
                    auto notifyStatus = [&](const std::wstring& timeRemMsg) { updateStatus(operationName + L"... " + timeRemMsg); /*throw CancelProcess*/ };

                    delayAndCountDown(std::chrono::steady_clock::now() + std::chrono::seconds(10), notifyStatus); //throw CancelProcess
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
                autoClose = true;
                finalRequest = FinalRequest::exit; //program exit must be handled by calling context!
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

    if (suspend) //*before* showing results dialog
        try
        {
            suspendSystem(); //throw FileError
        }
        catch (const FileError& e) { logMsg(errorLog_.ref(), e.toString(), MSG_TYPE_ERROR); }

    //--------------------- sound notification ----------------------
    if (!taskCancelled() && !suspend && !autoClose && //only play when actually showing results dialog
        !soundFileSyncComplete_.empty())
    {
        //wxWidgets shows modal error dialog by default => "no, wxWidgets, NO!"
        wxLog* oldLogTarget = wxLog::SetActiveTarget(new wxLogStderr); //transfer and receive ownership!
        ZEN_ON_SCOPE_EXIT(delete wxLog::SetActiveTarget(oldLogTarget));

        wxSound::Play(utfTo<wxString>(soundFileSyncComplete_), wxSOUND_ASYNC);
    }
    //if (::GetForegroundWindow() != GetHWND())
    //    RequestUserAttention(); -> probably too much since task bar is already colorized with Taskbar::Status::error or Status::normal

    const auto [autoCloseSelected, dim] = progressDlg_->destroy(autoClose,
                                                                finalRequest == FinalRequest::none /*restoreParentFrame*/,
                                                                *syncResult_, errorLog_);
    //caveat: calls back to getErrorStats() => *share* (and not move) errorLog_
    progressDlg_ = nullptr;

    return {autoCloseSelected, dim, finalRequest};
}


void StatusHandlerFloatingDialog::initNewPhase(int itemsTotal, int64_t bytesTotal, ProcessPhase phaseID)
{
    assert(phaseID == ProcessPhase::sync);
    StatusHandler::initNewPhase(itemsTotal, bytesTotal, phaseID);
    progressDlg_->initNewPhase(); //call after "StatusHandler::initNewPhase"

    //macOS needs a full yield to update GUI and get rid of "dummy" texts
    requestUiUpdate(true /*force*/); //throw CancelProcess
}



void StatusHandlerFloatingDialog::logMessage(const std::wstring& msg, MsgType type)
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


void StatusHandlerFloatingDialog::reportWarning(const std::wstring& msg, bool& warningActive)
{
    PauseTimers dummy(*progressDlg_);

    logMsg(errorLog_.ref(), msg, MSG_TYPE_WARNING);

    if (!warningActive)
        return;

    if (!progressDlg_->getOptionIgnoreErrors())
    {
        forceUiUpdateNoThrow(); //noexcept! => don't throw here when error occurs during clean up!

        bool dontWarnAgain = false;
        switch (showConfirmationDialog(progressDlg_->getWindowIfVisible(), DialogInfoType::warning,
                                       PopupDialogCfg().setDetailInstructions(msg).
                                       alertWhenPending(soundFileAlertPending_).
                                       setCheckBox(dontWarnAgain, _("&Don't show this warning again")),
                                       _("&Ignore")))
        {
            case ConfirmationButton::accept:
                warningActive = !dontWarnAgain;
                break;
            case ConfirmationButton::cancel:
                cancelProcessNow(CancelReason::user); //throw CancelProcess
                break;
        }
    }
    //else: if errors are ignored, then warnings should be, too
}


ProcessCallback::Response StatusHandlerFloatingDialog::reportError(const ErrorInfo& errorInfo)
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
                logMsg(errorLog_.ref(), errorInfo.msg + L"\n-> " + _("Retrying operation..."), //explain why there are duplicate "doing operation X" info messages in the log!
                       MSG_TYPE_INFO, failTime);
                return ProcessCallback::retry;

            case ConfirmationButton3::cancel:
                cancelProcessNow(CancelReason::user); //throw CancelProcess
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

    logMsg(errorLog_.ref(), msg, MSG_TYPE_ERROR);

    if (!progressDlg_->getOptionIgnoreErrors())
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
}


Statistics::ErrorStats StatusHandlerFloatingDialog::getErrorStats() const
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

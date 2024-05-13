// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PROGRESS_INDICATOR_H_8037493452348
#define PROGRESS_INDICATOR_H_8037493452348

#include <functional>
#include <zen/error_log.h>
//#include <wx/frame.h>
#include "wx+/window_tools.h"
#include "../status_handler.h"


namespace fff
{
class CompareProgressPanel
{
public:
    explicit CompareProgressPanel(wxFrame& parentWindow); //CompareProgressPanel will be owned by parentWindow!

    wxWindow* getAsWindow(); //convenience! don't abuse!

    void init(const Statistics& syncStat, bool ignoreErrors, size_t autoRetryCount); //begin of sync: make visible, set pointer to "syncStat", initialize all status values
    void teardown(); //end of sync: hide again, clear pointer to "syncStat"

    void initNewPhase(); //call after "StatusHandler::initNewPhase"

    void updateGui();

    //allow changing a few options dynamically during sync
    bool getOptionIgnoreErrors() const;
    void setOptionIgnoreErrors(bool ignoreError);

    void timerSetStatus(bool active); //start/stop all internal timers!
    bool timerIsRunning() const;
    std::chrono::milliseconds pauseAndGetTotalTime();

private:
    class Impl;
    Impl* const pimpl_;
};


//StatusHandlerFloatingDialog will internally process Window messages => disable GUI controls to avoid unexpected callbacks!

enum class PostSyncAction
{
    none,
    exit,
    sleep,
    shutdown
};

struct SyncProgressDialog
{
    static SyncProgressDialog* create(const zen::WindowLayout::Dimensions& dim,
                                      const std::function<void()>& userRequestCancel,
                                      const Statistics& syncStat,
                                      wxFrame* parentWindow, //may be nullptr
                                      bool showProgress,
                                      bool autoCloseDialog,
                                      const std::vector<std::wstring>& jobNames,
                                      time_t syncStartTime,
                                      bool ignoreErrors,
                                      size_t autoRetryCount,
                                      PostSyncAction postSyncAction);
    struct Result
    {
        bool autoCloseDialog;
        zen::WindowLayout::Dimensions dim;
    };
    virtual Result destroy(bool autoClose, bool restoreParentFrame, TaskResult syncResult, const zen::SharedRef<const zen::ErrorLog>& log) = 0;
    //---------------------------------------------------------------------------

    virtual wxWindow* getWindowIfVisible() = 0; //may be nullptr; don't abuse, use as parent for modal dialogs only!

    virtual void initNewPhase        () = 0; //call after "StatusHandler::initNewPhase"
    virtual void notifyProgressChange() = 0; //noexcept, required by graph!
    virtual void updateGui           () = 0; //update GUI and process Window messages

    //allow changing a few options dynamically during sync
    virtual bool getOptionIgnoreErrors()           const = 0;
    virtual void setOptionIgnoreErrors(bool ignoreError) = 0;
    virtual PostSyncAction getOptionPostSyncAction() const = 0;
    virtual bool getOptionAutoCloseDialog() const = 0;

    virtual void timerSetStatus(bool active) = 0; //start/stop all internal timers!
    virtual bool timerIsRunning() const = 0;
    virtual std::chrono::milliseconds pauseAndGetTotalTime() = 0;

protected:
    ~SyncProgressDialog() {}
};


template <class ProgressDlg>
class PauseTimers
{
public:
    explicit PauseTimers(ProgressDlg& ss) : ss_(ss), timerWasRunning_(ss.timerIsRunning()) { ss_.timerSetStatus(false); }
    ~PauseTimers() { ss_.timerSetStatus(timerWasRunning_); } //restore previous state: support recursive calls
private:
    ProgressDlg& ss_;
    const bool timerWasRunning_;
};
}

#endif //PROGRESS_INDICATOR_H_8037493452348

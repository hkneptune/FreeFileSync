// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PROGRESS_INDICATOR_H_8037493452348
#define PROGRESS_INDICATOR_H_8037493452348

#include <functional>
#include <zen/error_log.h>
#include <zen/zstring.h>
#include <wx/frame.h>
#include "../base/config.h"
#include "../base/status_handler.h"
#include "../base/return_codes.h"


namespace fff
{
class CompareProgressDialog
{
public:
    CompareProgressDialog(wxFrame& parentWindow); //CompareProgressDialog will be owned by parentWindow!

    wxWindow* getAsWindow(); //convenience! don't abuse!

    void init(const Statistics& syncStat, bool ignoreErrors, size_t automaticRetryCount); //begin of sync: make visible, set pointer to "syncStat", initialize all status values
    void teardown(); //end of sync: hide again, clear pointer to "syncStat"

    void initNewPhase(); //call after "StatusHandler::initNewPhase"

    void updateGui();

    //allow changing a few options dynamically during sync
    bool getOptionIgnoreErrors() const;
    void setOptionIgnoreErrors(bool ignoreError);

    void timerSetStatus(bool active); //start/stop all internal timers!
    bool timerIsRunning() const;

private:
    class Impl;
    Impl* const pimpl_;
};


//StatusHandlerFloatingDialog will internally process Window messages => disable GUI controls to avoid unexpected callbacks!

enum class PostSyncAction2
{
    NONE,
    EXIT,
    SLEEP,
    SHUTDOWN
};

struct SyncProgressDialog
{
    //essential to call one of these two methods in StatusUpdater derived class' destructor at the LATEST(!)
    //to prevent access to callback to updater (e.g. request abort)
    virtual void showSummary(SyncResult finalStatus, const std::shared_ptr<const zen::ErrorLog>& log /*bound!*/) = 0; //sync finished, still dialog may live on
    virtual void closeDirectly(bool restoreParentFrame) = 0; //don't wait for user

    //---------------------------------------------------------------------------

    virtual wxWindow* getWindowIfVisible() = 0; //may be nullptr; don't abuse, use as parent for modal dialogs only!

    virtual void initNewPhase        () = 0; //call after "StatusHandler::initNewPhase"
    virtual void notifyProgressChange() = 0; //noexcept, required by graph!
    virtual void updateGui           () = 0; //update GUI and process Window messages

    //allow changing a few options dynamically during sync
    virtual bool getOptionIgnoreErrors()           const = 0;
    virtual void setOptionIgnoreErrors(bool ignoreError) = 0;
    virtual PostSyncAction2 getOptionPostSyncAction() const = 0;
    virtual bool getOptionAutoCloseDialog() const = 0;

    virtual void timerSetStatus(bool active) = 0; //start/stop all internal timers!
    virtual bool timerIsRunning() const = 0;

protected:
    ~SyncProgressDialog() {}
};


SyncProgressDialog* createProgressDialog(AbortCallback& abortCb,
                                         const std::function<void()>& notifyWindowTerminate, //note: user closing window cannot be prevented on OS X! (And neither on Windows during system shutdown!)
                                         const Statistics& syncStat,
                                         wxFrame* parentWindow, //may be nullptr
                                         bool showProgress,
                                         bool autoCloseDialog,
                                         const std::chrono::system_clock::time_point& syncStartTime,
                                         const wxString& jobName,
                                         const Zstring& soundFileSyncComplete,
                                         bool ignoreErrors,
                                         size_t automaticRetryCount,
                                         PostSyncAction2 postSyncAction);
//DON'T delete the pointer! it will be deleted by the user clicking "OK/Cancel"/wxWindow::Destroy() after showSummary() or closeDirectly()


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

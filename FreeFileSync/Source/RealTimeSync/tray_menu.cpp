// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "tray_menu.h"
#include <chrono>
#include <zen/thread.h>
#include <wx/taskbar.h>
#include <wx/icon.h> //Linux needs this
#include <wx/app.h>
#include <wx/menu.h>
#include <wx/timer.h>
#include <wx+/image_tools.h>
#include <zen/shell_execute.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "monitor.h"
#include "../lib/resolve_path.h"

using namespace zen;
using namespace rts;


namespace
{
const std::chrono::seconds RETRY_AFTER_ERROR_INTERVAL(15);
const std::chrono::milliseconds UI_UPDATE_INTERVAL(100); //perform ui updates not more often than necessary, 100 seems to be a good value with only a minimal performance loss


std::chrono::steady_clock::time_point lastExec;


bool updateUiIsAllowed()
{
    const auto now = std::chrono::steady_clock::now();

    if (numeric::dist(now, lastExec) > UI_UPDATE_INTERVAL) //handle potential chrono wrap-around!
    {
        lastExec = now;
        return true;
    }
    return false;
}


enum TrayMode
{
    TRAY_MODE_ACTIVE,
    TRAY_MODE_WAITING,
    TRAY_MODE_ERROR,
};


class TrayIconObject : public wxTaskBarIcon
{
public:
    TrayIconObject(const wxString& jobname) :
        resumeRequested(false),
        abortRequested(false),
        showErrorMsgRequested(false),
        mode(TRAY_MODE_ACTIVE),
        iconFlashStatusLast(false),
        jobName_(jobname),
        trayBmp(getResourceImage(L"RTS_tray_24x24")) //use a 24x24 bitmap for perfect fit
    {
        Connect(wxEVT_TASKBAR_LEFT_DCLICK, wxEventHandler(TrayIconObject::OnDoubleClick), nullptr, this);
        setMode(mode);
    }

    //require polling:
    bool resumeIsRequested() const { return resumeRequested; }
    bool abortIsRequested () const { return abortRequested;  }

    //during TRAY_MODE_ERROR those two functions are available:
    void clearShowErrorRequested() { assert(mode == TRAY_MODE_ERROR); showErrorMsgRequested = false; }
    bool getShowErrorRequested() const { assert(mode == TRAY_MODE_ERROR); return showErrorMsgRequested; }

    void setMode(TrayMode m)
    {
        mode = m;
        timer.Stop();
        timer.Disconnect(wxEVT_TIMER, wxEventHandler(TrayIconObject::OnErrorFlashIcon), nullptr, this);
        switch (m)
        {
            case TRAY_MODE_ACTIVE:
                setTrayIcon(trayBmp, _("Directory monitoring active"));
                break;

            case TRAY_MODE_WAITING:
                setTrayIcon(greyScale(trayBmp), _("Waiting until all directories are available..."));
                break;

            case TRAY_MODE_ERROR:
                timer.Connect(wxEVT_TIMER, wxEventHandler(TrayIconObject::OnErrorFlashIcon), nullptr, this);
                timer.Start(500); //timer interval in [ms]
                break;
        }
    }

private:
    void OnErrorFlashIcon(wxEvent& event)
    {
        iconFlashStatusLast = !iconFlashStatusLast;
        setTrayIcon(iconFlashStatusLast ? trayBmp : greyScale(trayBmp), _("Error"));
    }

    void setTrayIcon(const wxBitmap& bmp, const wxString& statusTxt)
    {
        wxIcon realtimeIcon;
        realtimeIcon.CopyFromBitmap(bmp);
        wxString tooltip = L"RealTimeSync\n" + statusTxt;
        if (!jobName_.empty())
            tooltip += L"\n\"" + jobName_ + L"\"";
        SetIcon(realtimeIcon, tooltip);
    }

    enum Selection
    {
        CONTEXT_RESTORE = 1, //wxWidgets: "A MenuItem ID of zero does not work under Mac"
        CONTEXT_SHOW_ERROR,
        CONTEXT_ABORT = wxID_EXIT
    };

    wxMenu* CreatePopupMenu() override
    {
        wxMenu* contextMenu = new wxMenu;

        wxMenuItem* defaultItem = nullptr;
        switch (mode)
        {
            case TRAY_MODE_ACTIVE:
            case TRAY_MODE_WAITING:
                defaultItem = new wxMenuItem(contextMenu, CONTEXT_RESTORE, _("&Restore"));
                break;
            case TRAY_MODE_ERROR:
                defaultItem = new wxMenuItem(contextMenu, CONTEXT_SHOW_ERROR, _("&Show error"));
                break;
        }
        contextMenu->Append(defaultItem);

        contextMenu->AppendSeparator();
        contextMenu->Append(CONTEXT_ABORT, _("&Quit"));
        //event handling
        contextMenu->Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TrayIconObject::OnContextMenuSelection), nullptr, this);

        return contextMenu; //ownership transferred to caller
    }

    void OnContextMenuSelection(wxCommandEvent& event)
    {
        switch (static_cast<Selection>(event.GetId()))
        {
            case CONTEXT_ABORT:
                abortRequested  = true;
                break;

            case CONTEXT_RESTORE:
                resumeRequested = true;
                break;

            case CONTEXT_SHOW_ERROR:
                showErrorMsgRequested = true;
                break;
        }
    }

    void OnDoubleClick(wxEvent& event)
    {
        switch (mode)
        {
            case TRAY_MODE_ACTIVE:
            case TRAY_MODE_WAITING:
                resumeRequested = true; //never throw exceptions through a C-Layer call stack (GUI)!
                break;
            case TRAY_MODE_ERROR:
                showErrorMsgRequested = true;
                break;
        }
    }

    bool resumeRequested;
    bool abortRequested;
    bool showErrorMsgRequested;

    TrayMode mode;

    bool iconFlashStatusLast; //flash try icon for TRAY_MODE_ERROR
    wxTimer timer;            //

    const wxString jobName_; //RTS job name, may be empty
    const wxBitmap trayBmp;
};


struct AbortMonitoring //exception class
{
    AbortMonitoring(AbortReason reasonCode) : reasonCode_(reasonCode) {}
    AbortReason reasonCode_;
};


//=> don't derive from wxEvtHandler or any other wxWidgets object unless instance is safely deleted (deferred) during idle event!!tray_icon.h
class TrayIconHolder
{
public:
    TrayIconHolder(const wxString& jobname) :
        trayObj(new TrayIconObject(jobname)) {}

    ~TrayIconHolder()
    {
        //harmonize with tray_icon.cpp!!!
        trayObj->RemoveIcon();
        //use wxWidgets delayed destruction: delete during next idle loop iteration (handle late window messages, e.g. when double-clicking)
        wxPendingDelete.Append(trayObj);
    }

    void doUiRefreshNow() //throw AbortMonitoring
    {
        wxTheApp->Yield(); //yield is UI-layer which is represented by this tray icon

        //advantage of polling vs callbacks: we can throw exceptions!
        if (trayObj->resumeIsRequested())
            throw AbortMonitoring(SHOW_GUI);

        if (trayObj->abortIsRequested())
            throw AbortMonitoring(EXIT_APP);
    }

    void setMode(TrayMode m) { trayObj->setMode(m); }

    bool getShowErrorRequested() const { return trayObj->getShowErrorRequested(); }
    void clearShowErrorRequested() { trayObj->clearShowErrorRequested(); }

private:
    TrayIconObject* trayObj;
};

//##############################################################################################################
}


rts::AbortReason rts::startDirectoryMonitor(const XmlRealConfig& config, const wxString& jobname)
{
    std::vector<Zstring> dirNamesNonFmt = config.directories;
    erase_if(dirNamesNonFmt, [](const Zstring& str) { return trimCpy(str).empty(); }); //remove empty entries WITHOUT formatting paths yet!

    if (dirNamesNonFmt.empty())
    {
        showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setMainInstructions(_("A folder input field is empty.")));
        return SHOW_GUI;
    }

    const Zstring cmdLine = trimCpy(config.commandline);

    if (cmdLine.empty())
    {
        showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setMainInstructions(_("Incorrect command line:") + L" \"\""));
        return SHOW_GUI;
    }

    struct MonitorCallbackImpl : public MonitorCallback
    {
        MonitorCallbackImpl(const wxString& jobname,
                            const Zstring& cmdLine) : trayIcon(jobname), cmdLine_(cmdLine) {}

        void setPhase(WatchPhase mode) override
        {
            switch (mode)
            {
                case MONITOR_PHASE_ACTIVE:
                    trayIcon.setMode(TRAY_MODE_ACTIVE);
                    break;
                case MONITOR_PHASE_WAITING:
                    trayIcon.setMode(TRAY_MODE_WAITING);
                    break;
            }
        }

        void executeExternalCommand() override
        {
            auto cmdLineExp = fff::expandMacros(cmdLine_);
            try
            {
                shellExecute(cmdLineExp, ExecutionType::SYNC); //throw FileError
            }
            catch (const FileError& e)
            {
                showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
            }
        }

        void requestUiRefresh() override
        {
            if (updateUiIsAllowed())
                trayIcon.doUiRefreshNow(); //throw AbortMonitoring
        }

        void reportError(const std::wstring& msg) override
        {
            trayIcon.setMode(TRAY_MODE_ERROR);
            trayIcon.clearShowErrorRequested();

            //wait for some time, then return to retry
            const auto delayUntil = std::chrono::steady_clock::now() + RETRY_AFTER_ERROR_INTERVAL;
            for (auto now = std::chrono::steady_clock::now(); now < delayUntil; now = std::chrono::steady_clock::now())
            {
                trayIcon.doUiRefreshNow(); //throw AbortMonitoring

                if (trayIcon.getShowErrorRequested())
                    switch (showConfirmationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().
                                                   setDetailInstructions(msg), _("&Retry")))
                    {
                        case ConfirmationButton::ACCEPT: //retry
                            return;

                        case ConfirmationButton::CANCEL:
                            throw AbortMonitoring(SHOW_GUI);
                    }
                std::this_thread::sleep_for(UI_UPDATE_INTERVAL);
            }
        }

        TrayIconHolder trayIcon;
        const Zstring cmdLine_;
    } cb(jobname, cmdLine);

    try
    {
        monitorDirectories(dirNamesNonFmt, config.delay, cb, UI_UPDATE_INTERVAL / 2); //cb: throw AbortMonitoring
        assert(false);
        return SHOW_GUI;
    }
    catch (const AbortMonitoring& ab)
    {
        return ab.reasonCode_;
    }
}

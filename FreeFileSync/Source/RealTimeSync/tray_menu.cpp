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
#include "../base/resolve_path.h"

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

    if (now > lastExec + UI_UPDATE_INTERVAL)
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
        jobName_(jobname)
    {
        Connect(wxEVT_TASKBAR_LEFT_DCLICK, wxEventHandler(TrayIconObject::OnDoubleClick), nullptr, this);

        assert(mode_ != TRAY_MODE_ACTIVE); //setMode() supports polling!
        setMode(TRAY_MODE_ACTIVE, Zstring());
    }

    //require polling:
    bool resumeIsRequested() const { return resumeRequested_; }
    bool abortIsRequested () const { return abortRequested_;  }

    //during TRAY_MODE_ERROR those two functions are available:
    void clearShowErrorRequested()     { assert(mode_ == TRAY_MODE_ERROR); showErrorMsgRequested_ = false; }
    bool getShowErrorRequested() const { assert(mode_ == TRAY_MODE_ERROR); return showErrorMsgRequested_; }

    void setMode(TrayMode m, const Zstring& missingFolderPath)
    {
        if (mode_ == m && missingFolderPath_ == missingFolderPath)
            return; //support polling

        mode_ = m;
        missingFolderPath_ = missingFolderPath;

        timer_.Stop();
        timer_.Disconnect(wxEVT_TIMER, wxEventHandler(TrayIconObject::OnErrorFlashIcon), nullptr, this);
        switch (m)
        {
            case TRAY_MODE_ACTIVE:
                setTrayIcon(trayBmp_, _("Directory monitoring active"));
                break;

            case TRAY_MODE_WAITING:
                assert(!missingFolderPath.empty());
                setTrayIcon(greyScale(trayBmp_), _("Waiting until directory is available:") + L" " + fmtPath(missingFolderPath));
                break;

            case TRAY_MODE_ERROR:
                timer_.Connect(wxEVT_TIMER, wxEventHandler(TrayIconObject::OnErrorFlashIcon), nullptr, this);
                timer_.Start(500); //timer interval in [ms]
                break;
        }
    }

private:
    void OnErrorFlashIcon(wxEvent& event)
    {
        iconFlashStatusLast_ = !iconFlashStatusLast_;
        setTrayIcon(iconFlashStatusLast_ ? trayBmp_ : greyScale(trayBmp_), _("Error"));
    }

    void setTrayIcon(const wxBitmap& bmp, const wxString& statusTxt)
    {
        wxIcon realtimeIcon;
        realtimeIcon.CopyFromBitmap(bmp);
        wxString tooltip = L"RealTimeSync\n" + statusTxt;
        if (!jobName_.empty())
            tooltip += L"\n\"" + jobName_ + L'"';
        SetIcon(realtimeIcon, tooltip);
    }

    enum Selection
    {
        CONTEXT_CONFIGURE = 1, //wxWidgets: "A MenuItem ID of zero does not work under Mac"
        CONTEXT_SHOW_ERROR,
        CONTEXT_ABORT = wxID_EXIT
    };

    wxMenu* CreatePopupMenu() override
    {
        wxMenu* contextMenu = new wxMenu;

        wxMenuItem* defaultItem = nullptr;
        switch (mode_)
        {
            case TRAY_MODE_ACTIVE:
            case TRAY_MODE_WAITING:
                defaultItem = new wxMenuItem(contextMenu, CONTEXT_CONFIGURE, _("&Configure")); //better than "Restore"? https://freefilesync.org/forum/viewtopic.php?t=2044&p=20391#p20391
                break;
            case TRAY_MODE_ERROR:
                defaultItem = new wxMenuItem(contextMenu, CONTEXT_SHOW_ERROR, _("&Show error message"));
                break;
        }
        contextMenu->Append(defaultItem);

        contextMenu->AppendSeparator();
        contextMenu->Append(CONTEXT_ABORT, _("&Quit"));

        contextMenu->Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TrayIconObject::OnContextMenuSelection), nullptr, this);
        return contextMenu; //ownership transferred to caller
    }

    void OnContextMenuSelection(wxCommandEvent& event)
    {
        switch (static_cast<Selection>(event.GetId()))
        {
            case CONTEXT_ABORT:
                abortRequested_ = true;
                break;

            case CONTEXT_CONFIGURE:
                resumeRequested_ = true;
                break;

            case CONTEXT_SHOW_ERROR:
                showErrorMsgRequested_ = true;
                break;
        }
    }

    void OnDoubleClick(wxEvent& event)
    {
        switch (mode_)
        {
            case TRAY_MODE_ACTIVE:
            case TRAY_MODE_WAITING:
                resumeRequested_ = true; //never throw exceptions through a C-Layer call stack (GUI)!
                break;
            case TRAY_MODE_ERROR:
                showErrorMsgRequested_ = true;
                break;
        }
    }

    bool resumeRequested_       = false;
    bool abortRequested_        = false;
    bool showErrorMsgRequested_ = false;

    TrayMode mode_ = TRAY_MODE_WAITING;
    Zstring missingFolderPath_;

    bool iconFlashStatusLast_ = false; //flash try icon for TRAY_MODE_ERROR
    wxTimer timer_;            //

    const wxString jobName_; //RTS job name, may be empty

    const wxBitmap trayBmp_ = getResourceImage(L"RTS_tray_24x24"); //use a 24x24 bitmap for perfect fit
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
        trayObj_(new TrayIconObject(jobname)) {}

    ~TrayIconHolder()
    {
        //harmonize with tray_icon.cpp!!!
        trayObj_->RemoveIcon();
        //use wxWidgets delayed destruction: delete during next idle loop iteration (handle late window messages, e.g. when double-clicking)
        wxPendingDelete.Append(trayObj_);
    }

    void doUiRefreshNow() //throw AbortMonitoring
    {
        wxTheApp->Yield(); //yield is UI-layer which is represented by this tray icon

        //advantage of polling vs callbacks: we can throw exceptions!
        if (trayObj_->resumeIsRequested())
            throw AbortMonitoring(AbortReason::REQUEST_GUI);

        if (trayObj_->abortIsRequested())
            throw AbortMonitoring(AbortReason::REQUEST_EXIT);
    }

    void setMode(TrayMode m, const Zstring& missingFolderPath) { trayObj_->setMode(m, missingFolderPath); }

    bool getShowErrorRequested() const { return trayObj_->getShowErrorRequested(); }
    void clearShowErrorRequested() { trayObj_->clearShowErrorRequested(); }

private:
    TrayIconObject* const trayObj_;
};

//##############################################################################################################
}


rts::AbortReason rts::runFolderMonitor(const XmlRealConfig& config, const wxString& jobname)
{
    std::vector<Zstring> dirNamesNonFmt = config.directories;
    eraseIf(dirNamesNonFmt, [](const Zstring& str) { return trimCpy(str).empty(); }); //remove empty entries WITHOUT formatting paths yet!

    if (dirNamesNonFmt.empty())
    {
        showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setMainInstructions(_("A folder input field is empty.")));
        return AbortReason::REQUEST_GUI;
    }

    const Zstring cmdLine = trimCpy(config.commandline);

    if (cmdLine.empty())
    {
        showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setMainInstructions(_("Incorrect command line:") + L" \"\""));
        return AbortReason::REQUEST_GUI;
    }


    TrayIconHolder trayIcon(jobname);

    auto executeExternalCommand = [&](const Zstring& changedItemPath, const std::wstring& actionName)
    {
        ::wxSetEnv(L"change_path", utfTo<wxString>(changedItemPath)); //some way to output what file changed to the user
        ::wxSetEnv(L"change_action", actionName);                     //

        auto cmdLineExp = fff::expandMacros(cmdLine);
        try
        {
            shellExecute(cmdLineExp, ExecutionType::SYNC); //throw FileError
        }
        catch (const FileError& e)
        {
            //blocks! however, we *expect* this to be a persistent error condition...
            showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
        }
    };

    auto requestUiRefresh = [&](const Zstring* missingFolderPath)
    {
        if (missingFolderPath)
            trayIcon.setMode(TRAY_MODE_WAITING, *missingFolderPath);
        else
            trayIcon.setMode(TRAY_MODE_ACTIVE, Zstring());

        if (updateUiIsAllowed())
            trayIcon.doUiRefreshNow(); //throw AbortMonitoring
    };

    auto reportError = [&](const std::wstring& msg)
    {
        trayIcon.setMode(TRAY_MODE_ERROR, Zstring());
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
                        throw AbortMonitoring(AbortReason::REQUEST_GUI);
                }
            std::this_thread::sleep_for(UI_UPDATE_INTERVAL);
        }
    };

    try
    {
        monitorDirectories(dirNamesNonFmt, std::chrono::seconds(config.delay),
                           executeExternalCommand,
                           requestUiRefresh, //throw AbortMonitoring
                           reportError,      //
                           UI_UPDATE_INTERVAL / 2);
        assert(false);
        return AbortReason::REQUEST_GUI;
    }
    catch (const AbortMonitoring& ab)
    {
        return ab.reasonCode_;
    }
}

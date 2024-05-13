// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "tray_menu.h"
#include <chrono>
//#include <zen/thread.h>
#include <zen/resolve_path.h>
#include <wx/taskbar.h>
#include <wx/icon.h> //Linux needs this
#include <wx/app.h>
#include <wx/menu.h>
#include <wx/timer.h>
#include <wx+/dc.h>
#include <wx+/image_tools.h>
#include <zen/process_exec.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "monitor.h"

using namespace zen;
using namespace rts;


namespace
{
constexpr std::chrono::seconds RETRY_AFTER_ERROR_INTERVAL(15);
constexpr std::chrono::milliseconds UI_UPDATE_INTERVAL(100); //perform ui updates not more often than necessary, 100 seems to be a good value with only a minimal performance loss


std::chrono::steady_clock::time_point lastExec;


bool uiUpdateDue()
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
    active,
    waiting,
    error,
};


class TrayIconObject : public wxTaskBarIcon
{
public:
    TrayIconObject(const wxString& jobname) :
        jobName_(jobname)
    {
        Bind(wxEVT_TASKBAR_LEFT_DCLICK, [this](wxTaskBarIconEvent& event) { onDoubleClick(event); });

        assert(mode_ != TrayMode::active); //setMode() supports polling!
        setMode(TrayMode::active, Zstring());

        timer_.Bind(wxEVT_TIMER, [this](wxTimerEvent& event) { onErrorFlashIcon(event); });
    }

    //require polling:
    bool resumeIsRequested() const { return resumeRequested_; }
    bool abortIsRequested () const { return cancelRequested_;  }

    //during TrayMode::error those two functions are available:
    void clearShowErrorRequested()     { assert(mode_ == TrayMode::error); showErrorMsgRequested_ = false; }
    bool getShowErrorRequested() const { assert(mode_ == TrayMode::error); return showErrorMsgRequested_; }

    void setMode(TrayMode m, const Zstring& missingFolderPath)
    {
        if (mode_ == m && missingFolderPath_ == missingFolderPath)
            return; //support polling

        mode_ = m;
        missingFolderPath_ = missingFolderPath;

        timer_.Stop();
        switch (m)
        {
            case TrayMode::active:
                setTrayIcon(trayImg_, _("Directory monitoring active"));
                break;

            case TrayMode::waiting:
                assert(!missingFolderPath.empty());
                setTrayIcon(greyScale(trayImg_), _("Waiting until directory is available:") + L' ' + fmtPath(missingFolderPath));
                break;

            case TrayMode::error:
                timer_.Start(500); //timer interval in [ms]
                break;
        }
    }

private:
    void onErrorFlashIcon(wxEvent& event)
    {
        iconFlashStatusLast_ = !iconFlashStatusLast_;
        setTrayIcon(greyScaleIfDisabled(trayImg_, iconFlashStatusLast_), _("Error"));
    }

    void setTrayIcon(const wxImage& img, const wxString& statusTxt)
    {
        wxIcon realtimeIcon;
        realtimeIcon.CopyFromBitmap(img);

        wxString tooltip = L"RealTimeSync";
        if (!jobName_.empty())
            tooltip += SPACED_DASH + jobName_;

        tooltip += L"\n" + statusTxt;

        SetIcon(realtimeIcon, tooltip);
    }

    wxMenu* CreatePopupMenu() override
    {
        wxMenu* contextMenu = new wxMenu;

        wxMenuItem* defaultItem = nullptr;
        switch (mode_)
        {
            case TrayMode::active:
            case TrayMode::waiting:
                defaultItem = new wxMenuItem(contextMenu, wxID_ANY, _("&Configure")); //better than "Restore"? https://freefilesync.org/forum/viewtopic.php?t=2044&p=20391#p20391
                contextMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent& event) { resumeRequested_ = true; }, defaultItem->GetId());
                break;

            case TrayMode::error:
                defaultItem = new wxMenuItem(contextMenu, wxID_ANY, _("&Show error message"));
                contextMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent& event) { showErrorMsgRequested_ = true; }, defaultItem->GetId());
                break;
        }
        contextMenu->Append(defaultItem);

        contextMenu->AppendSeparator();

        wxMenuItem* itemAbort = contextMenu->Append(wxID_ANY, _("&Quit"));
        contextMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent& event) { cancelRequested_ = true; }, itemAbort->GetId());

        return contextMenu; //ownership transferred to caller
    }

    void onDoubleClick(wxEvent& event)
    {
        switch (mode_)
        {
            case TrayMode::active:
            case TrayMode::waiting:
                resumeRequested_ = true; //never throw exceptions through a C-Layer call stack (GUI)!
                break;
            case TrayMode::error:
                showErrorMsgRequested_ = true;
                break;
        }
    }

    bool resumeRequested_       = false;
    bool cancelRequested_        = false;
    bool showErrorMsgRequested_ = false;

    TrayMode mode_ = TrayMode::waiting;
    Zstring missingFolderPath_;

    bool iconFlashStatusLast_ = false; //flash try icon for TrayMode::error
    wxTimer timer_;                    //

    const wxString jobName_; //RTS job name, may be empty

    const wxImage trayImg_ = loadImage("start_rts", dipToScreen(24)); //use 24x24 bitmap for perfect fit
};


struct AbortMonitoring //exception class
{
    AbortMonitoring(CancelReason reasonCode) : reasonCode_(reasonCode) {}
    CancelReason reasonCode_;
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
            throw AbortMonitoring(CancelReason::requestGui);

        if (trayObj_->abortIsRequested())
            throw AbortMonitoring(CancelReason::requestExit);
    }

    void setMode(TrayMode m, const Zstring& missingFolderPath) { trayObj_->setMode(m, missingFolderPath); }

    bool getShowErrorRequested() const { return trayObj_->getShowErrorRequested(); }
    void clearShowErrorRequested() { trayObj_->clearShowErrorRequested(); }

private:
    TrayIconObject* const trayObj_;
};

//##############################################################################################################
}


rts::CancelReason rts::runFolderMonitor(const XmlRealConfig& config, const wxString& jobname)
{
    std::vector<Zstring> dirNamesNonFmt = config.directories;
    std::erase_if(dirNamesNonFmt, [](const Zstring& str) { return trimCpy(str).empty(); }); //remove empty entries WITHOUT formatting paths yet!

    if (dirNamesNonFmt.empty())
    {
        showNotificationDialog(nullptr, DialogInfoType::error, PopupDialogCfg().setMainInstructions(_("A folder input field is empty.")));
        return CancelReason::requestGui;
    }

    const Zstring cmdLine = trimCpy(config.commandline);

    if (cmdLine.empty())
    {
        showNotificationDialog(nullptr, DialogInfoType::error, PopupDialogCfg().setMainInstructions(replaceCpy(_("Command %x failed."), L"%x", fmtPath(cmdLine))));
        return CancelReason::requestGui;
    }


    TrayIconHolder trayIcon(jobname);

    auto executeExternalCommand = [&](const Zstring& changedItemPath, const std::wstring& actionName) //throw FileError
    {
        ::wxSetEnv(L"change_path", utfTo<wxString>(changedItemPath)); //crude way to report changed file
        ::wxSetEnv(L"change_action", actionName);                     //
        auto cmdLineExp = expandMacros(cmdLine);

        try
        {
            if (const auto& [exitCode, output] = consoleExecute(cmdLineExp, std::nullopt /*timeoutMs*/); //throw SysError, (SysErrorTimeOut)
                exitCode != 0)
                throw SysError(formatSystemError("", replaceCpy(_("Exit code %x"), L"%x", numberTo<std::wstring>(exitCode)), utfTo<std::wstring>(output)));
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Command %x failed."), L"%x", fmtPath(cmdLineExp)), e.toString()); }
    };

    auto requestUiUpdate = [&](const Zstring* missingFolderPath)
    {
        if (missingFolderPath)
            trayIcon.setMode(TrayMode::waiting, *missingFolderPath);
        else
            trayIcon.setMode(TrayMode::active, Zstring());

        if (uiUpdateDue())
            trayIcon.doUiRefreshNow(); //throw AbortMonitoring
    };

    auto reportError = [&](const std::wstring& msg)
    {
        trayIcon.setMode(TrayMode::error, Zstring());
        trayIcon.clearShowErrorRequested();

        //wait for some time, then return to retry
        const auto delayUntil = std::chrono::steady_clock::now() + RETRY_AFTER_ERROR_INTERVAL;
        for (auto now = std::chrono::steady_clock::now(); now < delayUntil; now = std::chrono::steady_clock::now())
        {
            trayIcon.doUiRefreshNow(); //throw AbortMonitoring

            if (trayIcon.getShowErrorRequested())
                switch (showConfirmationDialog(nullptr, DialogInfoType::error, PopupDialogCfg().
                                               setDetailInstructions(msg), _("&Retry")))
                {
                    case ConfirmationButton::accept: //retry
                        return;

                    case ConfirmationButton::cancel:
                        throw AbortMonitoring(CancelReason::requestGui);
                }
            std::this_thread::sleep_for(UI_UPDATE_INTERVAL);
        }
    };

    try
    {
        monitorDirectories(dirNamesNonFmt, std::chrono::seconds(config.delay),
                           executeExternalCommand /*throw FileError*/,
                           requestUiUpdate, //throw AbortMonitoring
                           reportError,     //
                           UI_UPDATE_INTERVAL / 2);
        assert(false);
        return CancelReason::requestGui;
    }
    catch (const AbortMonitoring& ab)
    {
        return ab.reasonCode_;
    }
}

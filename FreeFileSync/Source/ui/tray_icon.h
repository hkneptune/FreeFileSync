// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TRAY_ICON_H_84217830427534285
#define TRAY_ICON_H_84217830427534285

#include <functional>
#include <memory>
#include <wx/string.h>
//#include <wx/image.h>


/*  show tray icon with progress during lifetime of this instance

    ATTENTION: wxWidgets never assumes that an object indirectly destroys itself while processing an event!
               this includes wxEvtHandler-derived objects!!!
               it seems wxTaskBarIcon::ProcessEvent() works (on Windows), but AddPendingEvent() will crash since it uses "this" after the event processing!

    => don't derive from wxEvtHandler or any other wxWidgets object here!!!!!!
    => use simple std::function as callback instead => FfsTrayIcon instance may now be safely deleted in callback
        while ~wxTaskBarIcon is delayed via wxPendingDelete                      */
namespace fff
{
class FfsTrayIcon
{
public:
    explicit FfsTrayIcon(const std::function<void()>& requestResume); //callback only held during lifetime of this instance
    ~FfsTrayIcon();

    void setToolTip(const wxString& toolTip);
    void setProgress(double fraction); //number between [0, 1], for small progress indicator

private:
    FfsTrayIcon           (const FfsTrayIcon&) = delete;
    FfsTrayIcon& operator=(const FfsTrayIcon&) = delete;

    class TaskBarImpl;
    TaskBarImpl* trayIcon_;

    class ProgressIconGenerator;
    std::unique_ptr<ProgressIconGenerator> iconGenerator_;

    wxString activeToolTip_ = L"FreeFileSync";
    double activeFraction_ = 1;
};
}

#endif //TRAY_ICON_H_84217830427534285

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "application.h"
#include "main_dlg.h"
#include <zen/file_access.h>
#include <zen/thread.h>
#include <zen/shutdown.h>
#include <wx/event.h>
#include <wx/log.h>
#include <wx/tooltip.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "xml_proc.h"
#include "../base/localization.h"
#include "../base/ffs_paths.h"
#include "../base/return_codes.h"
#include "../base/fatal_error.h"
#include "../base/help_provider.h"
#include "../base/resolve_path.h"

    #include <gtk/gtk.h>

using namespace zen;
using namespace rts;


IMPLEMENT_APP(Application);


namespace
{
const wxEventType EVENT_ENTER_EVENT_LOOP = wxNewEventType();
}


bool Application::OnInit()
{
    //do not call wxApp::OnInit() to avoid using wxWidgets command line parser

    ::gtk_rc_parse((fff::getResourceDirPf() + Zstr("Misc") + FILE_NAME_SEPARATOR + "styles.gtk_rc").c_str()); //remove inner border from bitmap buttons

    //Windows User Experience Interaction Guidelines: tool tips should have 5s timeout, info tips no timeout => compromise:
    wxToolTip::Enable(true); //yawn, a wxWidgets screw-up: wxToolTip::SetAutoPop is no-op if global tooltip window is not yet constructed: wxToolTip::Enable creates it
    wxToolTip::SetAutoPop(10000); //https://msdn.microsoft.com/en-us/library/windows/desktop/aa511495

    SetAppName(L"RealTimeSync");

    initResourceImages(fff::getResourceDirPf() + Zstr("Resources.zip"));

    try
    {
        fff::setLanguage(getProgramLanguage()); //throw FileError
    }
    catch (const FileError& e)
    {
        //following dialog does NOT trigger "exit on frame delete" while we are in OnInit(): http://docs.wxwidgets.org/trunk/overview_app.html#overview_app_shutdown
        showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
        //continue!
    }


    Connect(wxEVT_QUERY_END_SESSION, wxEventHandler(Application::onQueryEndSession), nullptr, this);
    Connect(wxEVT_END_SESSION,       wxEventHandler(Application::onQueryEndSession), nullptr, this);

    //Note: app start is deferred:  -> see FreeFileSync
    Connect(EVENT_ENTER_EVENT_LOOP, wxEventHandler(Application::onEnterEventLoop), nullptr, this);
    wxCommandEvent scrollEvent(EVENT_ENTER_EVENT_LOOP);
    AddPendingEvent(scrollEvent);
    return true; //true: continue processing; false: exit immediately.
}


int Application::OnExit()
{
    fff::releaseWxLocale();
    cleanupResourceImages();
    return wxApp::OnExit();
}


void Application::onEnterEventLoop(wxEvent& event)
{
    Disconnect(EVENT_ENTER_EVENT_LOOP, wxEventHandler(Application::onEnterEventLoop), nullptr, this);

    //try to set config/batch- filepath set by %1 parameter
    std::vector<Zstring> commandArgs;
    for (int i = 1; i < argc; ++i)
    {
        Zstring filePath = fff::getResolvedFilePath(utfTo<Zstring>(argv[i]));

        if (!fileAvailable(filePath)) //be a little tolerant
        {
            if (fileAvailable(filePath + Zstr(".ffs_real")))
                filePath += Zstr(".ffs_real");
            else if (fileAvailable(filePath + Zstr(".ffs_batch")))
                filePath += Zstr(".ffs_batch");
            else
            {
                showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setMainInstructions(replaceCpy(_("Cannot find file %x."), L"%x", fmtPath(filePath))));
                return;
            }
        }
        commandArgs.push_back(filePath);
    }

    Zstring cfgFilename;
    if (!commandArgs.empty())
        cfgFilename = commandArgs[0];

    MainDialog::create(cfgFilename);
}


int Application::OnRun()
{
    try
    {
        wxApp::OnRun();
    }
    catch (const std::bad_alloc& e) //the only kind of exception we don't want crash dumps for
    {
        fff::logFatalError(e.what()); //it's not always possible to display a message box, e.g. corrupted stack, however low-level file output works!

        const auto titleFmt = copyStringTo<std::wstring>(wxTheApp->GetAppDisplayName()) + SPACED_DASH + _("An exception occurred");
        std::cerr << utfTo<std::string>(titleFmt + SPACED_DASH) << e.what() << "\n";
        return fff::FFS_RC_EXCEPTION;
    }
    //catch (...) -> let it crash and create mini dump!!!

    return fff::FFS_RC_SUCCESS; //program's return code
}



void Application::onQueryEndSession(wxEvent& event)
{
    if (auto mainWin = dynamic_cast<MainDialog*>(GetTopWindow()))
        mainWin->onQueryEndSession();
    //it's futile to try and clean up while the process is in full swing (CRASH!) => just terminate!
    zen::terminateProcess(fff::FFS_RC_ABORTED);
}

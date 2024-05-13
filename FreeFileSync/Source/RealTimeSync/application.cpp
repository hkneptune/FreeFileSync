// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "application.h"
#include "main_dlg.h"
#include <zen/file_access.h>
//#include <zen/thread.h>
#include <zen/shutdown.h>
#include <zen/resolve_path.h>
#include <wx/clipbrd.h>
#include <wx/event.h>
//#include <wx/log.h>
#include <wx/tooltip.h>
#include <wx+/app_main.h>
//#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "config.h"
#include "../localization.h"
#include "../ffs_paths.h"
#include "../return_codes.h"

    #include <gtk/gtk.h>

using namespace zen;
using namespace rts;

using fff::FfsExitCode;

#ifdef __WXGTK3__ //deprioritize Wayland: see FFS' application.cpp
    GLOBAL_RUN_ONCE(::gdk_set_allowed_backends("x11,*")); //call *before* gtk_init()
#endif

IMPLEMENT_APP(Application)


namespace
{
void notifyAppError(const std::wstring& msg)
{
    //error handling strategy unknown and no sync log output available at this point!
        std::cerr << utfTo<std::string>(_("Error") + L": " + msg) + '\n';
    //alternative0: std::wcerr: cannot display non-ASCII at all, so why does it exist???
    //alternative1: wxSafeShowMessage => NO console output on Debian x86, WTF!
    //alternative2: wxMessageBox() => works, but we probably shouldn't block during command line usage
}
}


bool Application::OnInit()
{
    //do not call wxApp::OnInit() to avoid using wxWidgets command line parser

    initExtraLog([](const ErrorLog& log) //don't call functions depending on global state (which might be destroyed already!)
    {
        std::wstring msg;
        for (const LogEntry& e : log)
            msg += utfTo<std::wstring>(formatMessage(e));
        trim(msg);
        notifyAppError(msg);
    });

    try { imageResourcesInit(appendPath(fff::getResourceDirPath(), Zstr("Icons.zip"))); }
    catch (const FileError& e) { logExtraError(e.toString()); } //not critical in this context

    //GTK should already have been initialized by wxWidgets (see \src\gtk\app.cpp:wxApp::Initialize)
#if GTK_MAJOR_VERSION == 2
    ::gtk_rc_parse(appendPath(fff::getResourceDirPath(), "Gtk2Styles.rc").c_str());

    //fix hang on Ubuntu 19.10 (see FFS's application.cpp)
    [[maybe_unused]] GVfs* defaultFs = ::g_vfs_get_default(); //not owned by us!

#elif GTK_MAJOR_VERSION == 3
    auto loadCSS = [&](const char* fileName)
    {
        GtkCssProvider* provider = ::gtk_css_provider_new();
        ZEN_ON_SCOPE_EXIT(::g_object_unref(provider));

        GError* error = nullptr;
        ZEN_ON_SCOPE_EXIT(if (error) ::g_error_free(error));

        ::gtk_css_provider_load_from_path(provider, //GtkCssProvider* css_provider
                                          appendPath(fff::getResourceDirPath(), fileName).c_str(), //const gchar* path
                                          &error); //GError** error
        if (error)
            throw SysError(formatGlibError("gtk_css_provider_load_from_path", error));

        ::gtk_style_context_add_provider_for_screen(::gdk_screen_get_default(),               //GdkScreen* screen
                                                    GTK_STYLE_PROVIDER(provider),             //GtkStyleProvider* provider
                                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION); //guint priority
    };
    try
    {
        loadCSS("Gtk3Styles.css"); //throw SysError
    }
    catch (const SysError& e)
    {
        std::cerr << "[RealTimeSync] " + utfTo<std::string>(e.toString()) + "\n" "Loading GTK3\'s old CSS format instead..." "\n";
        try
        {
            loadCSS("Gtk3Styles.old.css"); //throw SysError
        }
        catch (const SysError& e3) { logExtraError(_("Error during process initialization.") + L"\n\n" + e3.toString()); }
    }
#else
#error unknown GTK version!
#endif

    /* we're a GUI app: ignore SIGHUP when the parent terminal quits! (or process is killed!)
            => the FFS launcher will still be killed => fine
            => macOS: apparently not needed! interestingly the FFS launcher does receive SIGHUP and *is* killed  */
    if (sighandler_t oldHandler = ::signal(SIGHUP, SIG_IGN);
        oldHandler == SIG_ERR)
        logExtraError(_("Error during process initialization.") + L"\n\n" + formatSystemError("signal(SIGHUP)", getLastError()));
    else assert(!oldHandler);


    //Windows User Experience Interaction Guidelines: tool tips should have 5s timeout, info tips no timeout => compromise:
    wxToolTip::Enable(true); //wxWidgets screw-up: wxToolTip::SetAutoPop is no-op if global tooltip window is not yet constructed: wxToolTip::Enable creates it
    wxToolTip::SetAutoPop(15'000); //https://docs.microsoft.com/en-us/windows/win32/uxguide/ctrl-tooltips-and-infotips

    SetAppName(L"RealTimeSync"); //if not set, defaults to executable name


    try
    {
        fff::localizationInit(appendPath(fff::getResourceDirPath(), Zstr("Languages.zip"))); //throw FileError
        fff::setLanguage(getProgramLanguage()); //throw FileError
    }
    catch (const FileError& e) { logExtraError(e.toString()); }

    auto onSystemShutdown = [](int /*unused*/ = 0)
    {
        onSystemShutdownRunTasks();

        //it's futile to try and clean up while the process is in full swing (CRASH!) => just terminate!
        terminateProcess(static_cast<int>(FfsExitCode::cancelled));
    };
    Bind(wxEVT_QUERY_END_SESSION, [onSystemShutdown](wxCloseEvent& event) { onSystemShutdown(); }); //can veto
    Bind(wxEVT_END_SESSION,       [onSystemShutdown](wxCloseEvent& event) { onSystemShutdown(); }); //can *not* veto
    if (auto /*sighandler_t n.a. on macOS*/ oldHandler = ::signal(SIGTERM, onSystemShutdown);//"graceful" exit requested, unlike SIGKILL
        oldHandler == SIG_ERR)
        logExtraError(_("Error during process initialization.") + L"\n\n" + formatSystemError("signal(SIGTERM)", getLastError()));
    else assert(!oldHandler);

    //Note: app start is deferred:  -> see FreeFileSync
    CallAfter([&] { onEnterEventLoop(); });

    return true; //true: continue processing; false: exit immediately.
}


void Application::onEnterEventLoop()
{
    //wxWidgets app exit handling is weird... we want to exit only if the logical main window is closed, not just *any* window!
    wxTheApp->SetExitOnFrameDelete(false); //prevent popup-windows from becoming temporary top windows leading to program exit after closure
    ZEN_ON_SCOPE_EXIT(if (!globalWindowWasSet()) wxTheApp->ExitMainLoop()); //quit application, if no main window was set (batch silent mode)

    //try to set config/batch- filepath set by %1 parameter
    std::vector<Zstring> commandArgs;

    try
    {
        for (int i = 1; i < argc; ++i)
        {
            const Zstring& filePath = getResolvedFilePath(utfTo<Zstring>(argv[i]));
#if 0
            if (!fileAvailable(filePath)) //...be a little tolerant
                for (const Zchar* ext : {Zstr(".ffs_real"), Zstr(".ffs_batch")})
                    if (fileAvailable(filePath + ext))
                        filePath += ext;
#endif
            if (endsWithAsciiNoCase(filePath, Zstr(".ffs_real")) ||
                endsWithAsciiNoCase(filePath, Zstr(".ffs_batch")))
                commandArgs.push_back(filePath);
            else
                throw FileError(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(filePath)),
                                _("Unexpected file extension:") + L' ' + fmtPath(getFileExtension(filePath)) + L'\n' +
                                _("Expected:") + L" ffs_real, ffs_batch");
        }

        Zstring cfgFilePath;
        if (!commandArgs.empty())
            cfgFilePath = commandArgs[0];

        MainDialog::create(cfgFilePath);
    }
    catch (const FileError& e)
    {
        notifyAppError(e.toString());
    }
}


int Application::OnExit()
{
    [[maybe_unused]] const bool rv = wxClipboard::Get()->Flush(); //see wx+/context_menu.h
    //assert(rv); -> fails if clipboard wasn't used
    fff::localizationCleanup();
    imageResourcesCleanup();
    return wxApp::OnExit();
}


wxLayoutDirection Application::GetLayoutDirection() const { return languageLayoutIsRtl() ? wxLayout_RightToLeft : wxLayout_LeftToRight; }


int Application::OnRun()
{
#if wxUSE_EXCEPTIONS
#error why is wxWidgets uncaught exception handling enabled!?
#endif

    //exception => Windows: let it crash and create mini dump!!! Linux/macOS: std::exception::what() logged to console
        [[maybe_unused]] const int rc = wxApp::OnRun();
    return static_cast<int>(FfsExitCode::success); //process exit code
}

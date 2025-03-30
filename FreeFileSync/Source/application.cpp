// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "application.h"
#include <memory>
#include <zen/file_access.h>
#include <zen/shutdown.h>
#include <zen/process_exec.h>
#include <zen/resolve_path.h>
#include <zen/sys_info.h>
#include <wx/clipbrd.h>
#include <wx/tooltip.h>
#include <wx+/darkmode.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "afs/concrete.h"
#include "base/comparison.h"
#include "base/synchronization.h"
#include "ui/batch_status_handler.h"
#include "ui/main_dlg.h"
#include "ui/small_dlgs.h"
#include "base_tools.h"
#include "ffs_paths.h"
#include "return_codes.h"

    #include <gtk/gtk.h>

using namespace zen;
using namespace fff;


#ifdef __WXGTK3__
    /* Wayland backend used by GTK3 does not allow to move windows! (no such issue on GTK2)

    "I'd really like to know if there is some deep technical reason for it or
    if this is really as bloody stupid as it seems?" - vadz  https://github.com/wxWidgets/wxWidgets/issues/18733#issuecomment-1011235902

    Show all available GTK backends: run FreeFileSync with env variable:    GDK_BACKEND=help

    => workaround: https://docs.gtk.org/gdk3/func.set_allowed_backends.html           */
    GLOBAL_RUN_ONCE(::gdk_set_allowed_backends("x11,*")); //call *before* gtk_init()
#endif

IMPLEMENT_APP(Application)


namespace
{
std::vector<Zstring> getCommandlineArgs(const wxApp& app)
{
    std::vector<Zstring> args;
    for (const wxString& arg : app.argv.GetArguments())
        args.push_back(utfTo<Zstring>(arg));
    //remove first argument which is exe path by convention: https://devblogs.microsoft.com/oldnewthing/20060515-07/?p=31203
    if (!args.empty())
        args.erase(args.begin());

    return args;
}


void showSyntaxHelp()
{
    showNotificationDialog(nullptr, DialogInfoType::info, PopupDialogCfg().
                           setTitle(_("Command line")).
                           setDetailInstructions(_("Syntax:") + L"\n\n" +
                                                 L"FreeFileSync" + L'\n' +
                                                 TAB_SPACE + L"[" + _("config files:") + L" *.ffs_gui/*.ffs_batch]" + L'\n' +
                                                 TAB_SPACE + L"[-DirPair " + _("directory") + L' ' + _("directory") + L"]" L"\n" +
                                                 TAB_SPACE + L"[-Edit]" + L'\n' +
                                                 TAB_SPACE + L"[" + _("global config file:") + L" GlobalSettings.xml]" + L"\n\n" +

                                                 _("config files:") + L'\n' +
                                                 _("Any number of FreeFileSync \"ffs_gui\" and/or \"ffs_batch\" configuration files.") + L"\n\n" +

                                                 L"-DirPair " + _("directory") + L' ' + _("directory") + L'\n' +
                                                 _("Any number of alternative directory pairs for at most one config file.") + L"\n\n" +

                                                 L"-Edit" + L'\n' +
                                                 _("Open the selected configuration for editing only, without executing it.") + L"\n\n" +

                                                 _("global config file:") + L'\n' +
                                                 _("Path to an alternate GlobalSettings.xml file.")));
}


void notifyAppError(const std::wstring& msg)
{
        std::cerr << utfTo<std::string>(_("Error") + L": " + msg) + '\n';
    //alternative0: std::wcerr: cannot display non-ASCII at all, so why does it exist???
    //alternative1: wxSafeShowMessage => NO console output on Debian x86, WTF!
    //alternative2: wxMessageBox() => works, but we probably shouldn't block during command line usage
}
}

//##################################################################################################################

bool Application::OnInit()
{
    //do not call wxApp::OnInit() to avoid using wxWidgets command line parser

    const auto now = std::chrono::system_clock::now(); //e.g. "ErrorLog 2023-07-05 105207.073.xml"
    initExtraLog([logFilePath = appendPath(getConfigDirPath(), Zstr("ErrorLog ") +
                                           formatTime(Zstr("%Y-%m-%d %H%M%S"), getLocalTime(std::chrono::system_clock::to_time_t(now))) + Zstr('.') +
                                           printNumber<Zstring>(Zstr("%03d"), //[ms] should yield a fairly unique name
                                                                static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000)) +
                                           Zstr(".xml"))](const ErrorLog& log)
    {
        try //don't call functions depending on global state (which might be destroyed already!)
        {
            saveErrorLog(log, logFilePath); //throw FileError
        }
        catch (const FileError& e) { assert(false); notifyAppError(e.toString()); }
    });

    //tentatively set program language to OS default until GlobalSettings.xml is read later
    try { localizationInit(appendPath(getResourceDirPath(), Zstr("Languages.zip"))); } //throw FileError
    catch (const FileError& e) { logExtraError(e.toString()); }

    //parallel xBRZ-scaling! => run as early as possible
    try { imageResourcesInit(appendPath(getResourceDirPath(), Zstr("Icons.zip"))); }
    catch (const FileError& e) { logExtraError(e.toString()); } //not critical in this context

    //GTK should already have been initialized by wxWidgets (see \src\gtk\app.cpp:wxApp::Initialize)
#if GTK_MAJOR_VERSION == 2
    ::gtk_rc_parse(appendPath(getResourceDirPath(), "Gtk2Styles.rc").c_str());

    //hang on Ubuntu 19.10 (GLib 2.62) caused by ibus initialization: https://freefilesync.org/forum/viewtopic.php?t=6704
    //=> work around 1: bonus: avoid needless DBus calls: https://developer.gnome.org/gio/stable/running-gio-apps.html
    //                  drawback: missing MTP and network links in folder picker: https://freefilesync.org/forum/viewtopic.php?t=6871
    //if (::setenv("GIO_USE_VFS", "local", true /*overwrite*/) != 0)
    //    std::cerr << utfTo<std::string>(formatSystemError("setenv(GIO_USE_VFS)", errno)) + '\n';
    //    //BUGZ!?: "Modifications of environment variables are not allowed in multi-threaded programs" - https://rachelbythebay.com/w/2017/01/30/env/

    //=> work around 2:
    [[maybe_unused]] GVfs* defaultFs = ::g_vfs_get_default(); //not owned by us!
    //no such issue on GTK3!

#elif GTK_MAJOR_VERSION == 3
    auto loadCSS = [&](const char* fileName)
    {
        GtkCssProvider* provider = ::gtk_css_provider_new();
        ZEN_ON_SCOPE_EXIT(::g_object_unref(provider));

        GError* error = nullptr;
        ZEN_ON_SCOPE_EXIT(if (error) ::g_error_free(error));

        ::gtk_css_provider_load_from_path(provider, //GtkCssProvider* css_provider
                                          appendPath(getResourceDirPath(), fileName).c_str(), //const gchar* path
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
        std::cerr << "[FreeFileSync] " + utfTo<std::string>(e.toString()) + "\n" "Loading GTK3\'s old CSS format instead..." "\n";
        try
        {
            loadCSS("Gtk3Styles.old.css"); //throw SysError
        }
        catch (const SysError& e2) { logExtraError(_("Failed to update the color theme.") + L"\n\n" + e2.toString()); }
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

    SetAppName(L"FreeFileSync"); //if not set, defaults to executable name


    initAfs({getResourceDirPath(), getConfigDirPath()}); //bonus: using FTP Gdrive implicitly inits OpenSSL (used in runSanityChecks() on Linux) already during globals init


    auto onSystemShutdown = [](int /*unused*/ = 0)
    {
        onSystemShutdownRunTasks();

        //- it's futile to try and clean up while the process is in full swing (CRASH!) => just terminate!
        //- system sends close events to all open dialogs: If one of these calls wxCloseEvent::Veto(),
        //  e.g. user clicking cancel on save prompt, this would cancel the shutdown
        terminateProcess(static_cast<int>(FfsExitCode::cancelled));
    };
    Bind(wxEVT_QUERY_END_SESSION, [onSystemShutdown](wxCloseEvent& event) { onSystemShutdown(); }); //can veto
    Bind(wxEVT_END_SESSION,       [onSystemShutdown](wxCloseEvent& event) { onSystemShutdown(); }); //can *not* veto
    //- log off: Windows/macOS generates wxEVT_QUERY_END_SESSION/wxEVT_END_SESSION
    //           Linux/macOS generates SIGTERM, which we handle below
    //- Windows sends WM_QUERYENDSESSION, WM_ENDSESSION during log off, *not* WM_CLOSE https://devblogs.microsoft.com/oldnewthing/20080421-00/?p=22663
    //   => taskkill sending WM_CLOSE (without /f) is a misguided app simulating a button-click on X
    //      -> should send WM_QUERYENDSESSION instead!
    if (auto /*sighandler_t n.a. on macOS*/ oldHandler = ::signal(SIGTERM, onSystemShutdown);//"graceful" exit requested, unlike SIGKILL
        oldHandler == SIG_ERR)
        logExtraError(_("Error during process initialization.") + L"\n\n" + formatSystemError("signal(SIGTERM)", getLastError()));
    else assert(!oldHandler);

    //Note: app start is deferred: batch mode requires the wxApp eventhandler to be established for UI update events. This is not the case at the time of OnInit()!
    CallAfter([&] { onEnterEventLoop(); });

    return true; //true: continue processing; false: exit immediately
}


int Application::OnExit()
{
    [[maybe_unused]] const bool rv = wxClipboard::Get()->Flush(); //see wx+/context_menu.h
    //assert(rv); -> fails if clipboard wasn't used
    localizationCleanup();
    imageResourcesCleanup();
    teardownAfs();
    colorThemeCleanup();
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
    return static_cast<int>(exitCode_);
}




void Application::onEnterEventLoop()
{
    const std::vector<Zstring>& commandArgs = getCommandlineArgs(*this);

    //wxWidgets app exit handling is weird... we want to exit only if the logical main window is closed, not just *any* window!
    wxTheApp->SetExitOnFrameDelete(false); //prevent popup-windows from becoming temporary top windows leading to program exit after closure
    ZEN_ON_SCOPE_EXIT(if (!wxTheApp->GetExitOnFrameDelete()) wxTheApp->ExitMainLoop()); //quit application, if no main window was set (batch silent mode)

    try
    {
        //parse command line arguments
        std::vector<std::pair<Zstring, Zstring>> dirPathPhrasePairs;
        std::vector<Zstring> cfgFilePaths;
        Zstring globalCfgPathAlt;
        bool openForEdit = false;
        {
            const char* optionEdit    = "-edit";
            const char* optionDirPair = "-dirpair";
            const char* optionSendTo  = "-sendto"; //remaining arguments are unspecified number of folder paths; wonky syntax; let's keep it undocumented

            auto isHelpRequest = [](const Zstring& arg)
            {
                auto it = std::find_if(arg.begin(), arg.end(), [](Zchar c) { return c != Zstr('/') && c != Zstr('-'); });
                if (it == arg.begin()) return false; //require at least one prefix character

                const Zstring argTmp(it, arg.end());
                return equalAsciiNoCase(argTmp, "help") ||
                       equalAsciiNoCase(argTmp, "h")    ||
                       argTmp == Zstr("?");
            };

            auto isCommandLineOption = [&](const Zstring& arg)
            {
                return equalAsciiNoCase(arg, optionEdit   ) ||
                       equalAsciiNoCase(arg, optionDirPair) ||
                       equalAsciiNoCase(arg, optionSendTo ) ||
                       isHelpRequest(arg);
            };

            for (auto it = commandArgs.begin(); it != commandArgs.end(); ++it)
                if (isHelpRequest(*it))
                    return showSyntaxHelp();
                else if (equalAsciiNoCase(*it, optionEdit))
                    openForEdit = true;
                else if (equalAsciiNoCase(*it, optionDirPair))
                {
                    if (++it == commandArgs.end() || isCommandLineOption(*it))
                        throw FileError(replaceCpy(_("A left and a right directory path are expected after %x."), L"%x", utfTo<std::wstring>(optionDirPair)));
                    dirPathPhrasePairs.emplace_back(*it, Zstring());

                    if (++it == commandArgs.end() || isCommandLineOption(*it))
                        throw FileError(replaceCpy(_("A left and a right directory path are expected after %x."), L"%x", utfTo<std::wstring>(optionDirPair)));
                    dirPathPhrasePairs.back().second = *it;
                }
                else if (equalAsciiNoCase(*it, optionSendTo))
                {
                    for (size_t i = 0; ; ++i)
                    {
                        if (++it == commandArgs.end() || isCommandLineOption(*it))
                        {
                            --it;
                            break;
                        }

                        if (i < 2) //else: -SendTo with more than 2 paths? Doesn't make any sense, does it!?
                        {
                            //for -SendTo we expect a list of full native paths, not "phrases" that need to be resolved!
                            auto getFolderPath = [](Zstring itemPath)
                            {
                                try
                                {
                                    if (getItemType(itemPath) == ItemType::file) //throw FileError
                                        if (const std::optional<Zstring>& parentPath = getParentFolderPath(itemPath))
                                            return *parentPath;
                                }
                                catch (FileError&) {}

                                return itemPath;
                            };

                            if (i % 2 == 0)
                                dirPathPhrasePairs.emplace_back(getFolderPath(*it), Zstring());
                            else
                            {
                                const Zstring folderPath = getFolderPath(*it);
                                if (dirPathPhrasePairs.back().first != folderPath) //else: user accidentally sending to two files, which each time yield the same parent folder
                                    dirPathPhrasePairs.back().second = folderPath;
                            }
                        }
                    }
                }
                else
                {
                    const Zstring& filePath = getResolvedFilePath(*it);
#if 0
                    if (!fileAvailable(filePath)) //...be a little tolerant
                        for (const Zchar* ext : {Zstr(".ffs_gui"), Zstr(".ffs_batch"), Zstr(".xml")})
                            if (fileAvailable(filePath + ext))
                                filePath += ext;
#endif
                    if (endsWithAsciiNoCase(filePath, Zstr(".ffs_gui")) ||
                        endsWithAsciiNoCase(filePath, Zstr(".ffs_batch")))
                        cfgFilePaths.push_back(filePath);
                    else if (endsWithAsciiNoCase(filePath, Zstr(".xml")))
                        globalCfgPathAlt = filePath;
                    else
                        throw FileError(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(filePath)),
                                        _("Unexpected file extension:") + L' ' + fmtPath(getFileExtension(filePath)) + L'\n' +
                                        _("Expected:") + L" ffs_gui, ffs_batch, xml");
                }
        }
        //----------------------------------------------------------------------------------------------------

        auto hasNonDefaultConfig = [](const LocalPairConfig& lpc)
        {
            return lpc != LocalPairConfig{lpc.folderPathPhraseLeft,
                                          lpc.folderPathPhraseRight,
                                          std::nullopt, std::nullopt, FilterConfig()};
        };

        auto replaceDirectories = [&](MainConfiguration& mainCfg) //throw FileError
        {
            if (!dirPathPhrasePairs.empty())
            {
                if (cfgFilePaths.size() > 1)
                    throw FileError(_("Directories cannot be set for more than one configuration file."));

                //check if config at folder-pair level is present: this probably doesn't make sense when replacing/adding the user-specified directories
                if (hasNonDefaultConfig(mainCfg.firstPair) || std::any_of(mainCfg.additionalPairs.begin(), mainCfg.additionalPairs.end(), hasNonDefaultConfig))
                    throw FileError(_("The config file must not contain settings at directory pair level when directories are set via command line."));

                mainCfg.additionalPairs.clear();
                for (size_t i = 0; i < dirPathPhrasePairs.size(); ++i)
                    if (i == 0)
                    {
                        mainCfg.firstPair.folderPathPhraseLeft  = dirPathPhrasePairs[0].first;
                        mainCfg.firstPair.folderPathPhraseRight = dirPathPhrasePairs[0].second;
                    }
                    else
                        mainCfg.additionalPairs.push_back({dirPathPhrasePairs[i].first, dirPathPhrasePairs[i].second,
                                                           std::nullopt, std::nullopt, FilterConfig()});
            }
        };

        const Zstring globalCfgFilePath = !globalCfgPathAlt.empty() ? globalCfgPathAlt : getGlobalConfigDefaultPath();

        GlobalConfig globalCfg;
        try
        {
            std::wstring warningMsg;
            std::tie(globalCfg, warningMsg) = readGlobalConfig(globalCfgFilePath); //throw FileError
            assert(warningMsg.empty()); //ignore parsing errors: should be migration problems only *cross-fingers*
        }
        catch (const FileError& e)
        {
            try
            {
                bool cfgFileExists = true;
                try { cfgFileExists  = itemExists(globalCfgFilePath); /*throw FileError*/ } //=> unclear which exception is more relevant/useless:
                catch (const FileError& e2) { throw FileError(replaceCpy(e.toString(), L"\n\n", L'\n'), replaceCpy(e2.toString(), L"\n\n", L'\n')); }

                if (cfgFileExists)
                    throw;
            }
            catch (const FileError& e3) { logExtraError(e3.toString()); }
        }

        //late GlobalSettings.xml-dependent app initialization:
        try { setLanguage(globalCfg.programLanguage); } //throw FileError
        catch (const FileError& e) { logExtraError(e.toString()); }

        try { colorThemeInit(*this, globalCfg.appColorTheme); } //throw FileError
        catch (const FileError& e) { logExtraError(e.toString()); } //not critical in this context


        //-----------------------------------------------------------
        //distinguish sync scenarios:
        //-----------------------------------------------------------
        if (cfgFilePaths.empty())
        {
            //gui mode: default startup
            if (dirPathPhrasePairs.empty())
                MainDialog::create(globalCfg, globalCfgFilePath);
            //gui mode: default config with given directories
            else
            {
                FfsGuiConfig guiCfg;
                guiCfg.mainCfg.syncCfg.directionCfg = getDefaultSyncCfg(SyncVariant::mirror);

                replaceDirectories(guiCfg.mainCfg); //throw FileError

                MainDialog::create(guiCfg, {} /*cfgFilePaths*/, globalCfg, globalCfgFilePath, !openForEdit /*startComparison*/);
            }
        }
        else if (const Zstring filePath0 = cfgFilePaths[0];
                 //batch mode (single config)
                 cfgFilePaths.size() == 1 && endsWithAsciiNoCase(filePath0, Zstr(".ffs_batch")) && !openForEdit)
        {
            auto [batchCfg, warningMsg] = readBatchConfig(filePath0); //throw FileError
            if (!warningMsg.empty())
                throw FileError(warningMsg); //batch mode: break on errors AND even warnings!

            replaceDirectories(batchCfg.guiCfg.mainCfg); //throw FileError

            runBatchMode(batchCfg, filePath0, globalCfg, globalCfgFilePath);
        }
        else //GUI mode: (ffs_gui *or* ffs_batch)
        {
            auto [guiCfg, warningMsg] = readAnyConfig(cfgFilePaths); //throw FileError
            if (!warningMsg.empty())
                showNotificationDialog(nullptr, DialogInfoType::warning, PopupDialogCfg().setDetailInstructions(warningMsg));
            //what about simulating changed config on parsing errors?

            replaceDirectories(guiCfg.mainCfg); //throw FileError
            //what about simulating changed config due to directory replacement?
            //-> propably fine to not show as changed on GUI and not ask user to save on exit!

            MainDialog::create(guiCfg, cfgFilePaths, globalCfg, globalCfgFilePath, !openForEdit /*startComparison*/);
        }
    }
    catch (const FileError& e)
    {
        raiseExitCode(exitCode_, FfsExitCode::exception);
        notifyAppError(e.toString());
    }
}


void Application::runBatchMode(const FfsBatchConfig& batchCfg, const Zstring& cfgFilePath, GlobalConfig globalCfg, const Zstring& globalCfgFilePath)
{
    const bool allowUserInteraction = !batchCfg.batchExCfg.autoCloseSummary ||
                                      (!batchCfg.guiCfg.mainCfg.ignoreErrors && batchCfg.batchExCfg.batchErrorHandling == BatchErrorHandling::showPopup);


    /* regular check for software updates -> disabled for batch
        if (batchCfg.showProgress && manualProgramUpdateRequired())
            checkForUpdatePeriodically(globalCfg.lastUpdateCheck);
        -> WinInet not working when FFS is running as a service!!! https://support.microsoft.com/en-us/help/238425/info-wininet-not-supported-for-use-in-services   */


    const std::chrono::system_clock::time_point syncStartTime = std::chrono::system_clock::now();

    const WindowLayout::Dimensions progressDim
    {
        globalCfg.dpiLayouts[getDpiScalePercent()].progressDlg.size,
        std::nullopt /*pos*/,
        globalCfg.dpiLayouts[getDpiScalePercent()].progressDlg.isMaximized
    };

    //class handling status updates and error messages
    BatchStatusHandler statusHandler(!batchCfg.batchExCfg.runMinimized,
                                     extractJobName(cfgFilePath),
                                     syncStartTime,
                                     batchCfg.guiCfg.mainCfg.ignoreErrors,
                                     batchCfg.guiCfg.mainCfg.autoRetryCount,
                                     batchCfg.guiCfg.mainCfg.autoRetryDelay,
                                     globalCfg.soundFileSyncFinished,
                                     globalCfg.soundFileAlertPending,
                                     progressDim,
                                     batchCfg.batchExCfg.autoCloseSummary,
                                     batchCfg.batchExCfg.postBatchAction,
                                     batchCfg.batchExCfg.batchErrorHandling);

    AFS::RequestPasswordFun requestPassword; //throw CancelProcess
    if (allowUserInteraction)
        requestPassword = [&, password = Zstring()](const std::wstring& msg, const std::wstring& lastErrorMsg) mutable
    {
        assert(runningOnMainThread());
        if (showPasswordPrompt(statusHandler.getWindowIfVisible(), msg, lastErrorMsg, password) != ConfirmationButton::accept)
            statusHandler.cancelProcessNow(CancelReason::user); //throw CancelProcess

        return password;
    };

    try
    {
        //inform about (important) non-default global settings
        logNonDefaultSettings(globalCfg, statusHandler); //throw CancelProcess

        //batch mode: place directory locks on directories during both comparison AND synchronization
        std::unique_ptr<LockHolder> dirLocks;

        FolderComparison cmpResult = compare(globalCfg.warnDlgs,
                                             globalCfg.fileTimeTolerance,
                                             requestPassword,
                                             globalCfg.runWithBackgroundPriority,
                                             globalCfg.createLockFile,
                                             dirLocks,
                                             extractCompareCfg(batchCfg.guiCfg.mainCfg),
                                             statusHandler); //throw CancelProcess
        if (!cmpResult.empty())
            synchronize(syncStartTime,
                        globalCfg.verifyFileCopy,
                        globalCfg.copyLockedFiles,
                        globalCfg.copyFilePermissions,
                        globalCfg.failSafeFileCopy,
                        globalCfg.runWithBackgroundPriority,
                        extractSyncCfg(batchCfg.guiCfg.mainCfg),
                        cmpResult,
                        globalCfg.warnDlgs,
                        statusHandler); //throw CancelProcess
    }
    catch (CancelProcess&) {}

    //-------------------------------------------------------------------
    BatchStatusHandler::Result r = statusHandler.prepareResult();


    AbstractPath logFolderPath = createAbstractPath(batchCfg.guiCfg.mainCfg.altLogFolderPathPhrase); //optional
    if (AFS::isNullPath(logFolderPath))
        logFolderPath = createAbstractPath(globalCfg.logFolderPhrase);
    assert(!AFS::isNullPath(logFolderPath)); //mandatory! but still: let's include fall back
    if (AFS::isNullPath(logFolderPath))
        logFolderPath = createAbstractPath(getLogFolderDefaultPath());

    AbstractPath logFilePath = AFS::appendRelPath(logFolderPath, generateLogFileName(globalCfg.logFormat, r.summary));
    //e.g. %AppData%\FreeFileSync\Logs\Backup FreeFileSync 2013-09-15 015052.123 [Error].log

    auto notifyStatusNoThrow = [&](std::wstring&& msg) { try { statusHandler.updateStatus(std::move(msg)); /*throw CancelProcess*/ } catch (CancelProcess&) {} };


    if (statusHandler.taskCancelled() && *statusHandler.taskCancelled() == CancelReason::user)
        ; /* user cancelled => don't run post sync command
                            => don't run post sync action
                            => don't send email notification
                            => don't play sound notification  */
    else
    {
        //--------------------- post sync command ----------------------
        if (const Zstring cmdLine = trimCpy(expandMacros(batchCfg.guiCfg.mainCfg.postSyncCommand));
            !cmdLine.empty())
            if (batchCfg.guiCfg.mainCfg.postSyncCondition == PostSyncCondition::completion ||
                (batchCfg.guiCfg.mainCfg.postSyncCondition == PostSyncCondition::errors) == (r.summary.result == TaskResult::cancelled ||
                    r.summary.result == TaskResult::error))
                try
                {
                    //give consoleExecute() some "time to fail", but not too long to hang our process
                    const int DEFAULT_APP_TIMEOUT_MS = 100;

                    if (const auto& [exitCode, output] = consoleExecute(cmdLine, DEFAULT_APP_TIMEOUT_MS); //throw SysError, SysErrorTimeOut
                        exitCode != 0)
                        throw SysError(formatSystemError("", replaceCpy(_("Exit code %x"), L"%x", numberTo<std::wstring>(exitCode)), utfTo<std::wstring>(output)));

                    logMsg(r.errorLog.ref(), _("Executing command:") + L' ' + utfTo<std::wstring>(cmdLine) + L" [" + replaceCpy(_("Exit code %x"), L"%x", L"0") + L']', MSG_TYPE_INFO);
                }
                catch (SysErrorTimeOut&) //child process not failed yet => probably fine :>
                {
                    logMsg(r.errorLog.ref(), _("Executing command:") + L' ' + utfTo<std::wstring>(cmdLine), MSG_TYPE_INFO);
                }
                catch (const SysError& e)
                {
                    logMsg(r.errorLog.ref(), replaceCpy(_("Command %x failed."), L"%x", fmtPath(cmdLine)) + L"\n\n" + e.toString(), MSG_TYPE_ERROR);
                }

        //--------------------- email notification ----------------------
        if (const std::string notifyEmail = trimCpy(batchCfg.guiCfg.mainCfg.emailNotifyAddress);
            !notifyEmail.empty())
            if (batchCfg.guiCfg.mainCfg.emailNotifyCondition == ResultsNotification::always ||
                (batchCfg.guiCfg.mainCfg.emailNotifyCondition == ResultsNotification::errorWarning && (r.summary.result == TaskResult::cancelled ||
                    r.summary.result == TaskResult::error ||
                    r.summary.result == TaskResult::warning)) ||
                (batchCfg.guiCfg.mainCfg.emailNotifyCondition == ResultsNotification::errorOnly && (r.summary.result == TaskResult::cancelled ||
                    r.summary.result == TaskResult::error)))
                try
                {
                    logMsg(r.errorLog.ref(), replaceCpy(_("Sending email notification to %x"), L"%x", utfTo<std::wstring>(notifyEmail)), MSG_TYPE_INFO);
                    sendLogAsEmail(notifyEmail, r.summary, r.errorLog.ref(), logFilePath, notifyStatusNoThrow); //throw FileError
                }
                catch (const FileError& e) { logMsg(r.errorLog.ref(), e.toString(), MSG_TYPE_ERROR); }
    }

    //--------------------- save log file ----------------------
    std::set<AbstractPath> logsToKeepPaths;
    for (const ConfigFileItem& cfi : globalCfg.mainDlg.config.fileHistory)
        if (!equalNativePath(cfi.cfgFilePath, cfgFilePath)) //exception: don't keep old log for the selected cfg file!
            logsToKeepPaths.insert(cfi.lastRunStats.logFilePath);

    try //create not before destruction: 1. avoid issues with FFS trying to sync open log file 2. include status in log file name without extra rename
    {
        //do NOT use tryReportingError()! saving log files should not be cancellable!
        saveLogFile(logFilePath, r.summary, r.errorLog.ref(), globalCfg.logfilesMaxAgeDays, globalCfg.logFormat, logsToKeepPaths, notifyStatusNoThrow); //throw FileError
    }
    catch (const FileError& e)
    {
        try //fallback: log file *must* be saved no matter what!
        {
            const AbstractPath logFileDefaultPath = AFS::appendRelPath(createAbstractPath(getLogFolderDefaultPath()), generateLogFileName(globalCfg.logFormat, r.summary));
            if (logFilePath == logFileDefaultPath)
                throw;

            logMsg(r.errorLog.ref(), e.toString(), MSG_TYPE_ERROR);

            logFilePath = logFileDefaultPath;
            saveLogFile(logFileDefaultPath, r.summary, r.errorLog.ref(), globalCfg.logfilesMaxAgeDays, globalCfg.logFormat, logsToKeepPaths, notifyStatusNoThrow); //throw FileError
        }
        catch (const FileError& e2) { logMsg(r.errorLog.ref(), e2.toString(), MSG_TYPE_ERROR); logExtraError(e2.toString()); } //should never happen!!!
    }

    //--------- update last sync stats for the selected cfg file ---------
    const ErrorLogStats& logStats = getStats(r.errorLog.ref());

    for (ConfigFileItem& cfi : globalCfg.mainDlg.config.fileHistory)
        if (equalNativePath(cfi.cfgFilePath, cfgFilePath))
        {
            assert(r.summary.startTime == syncStartTime);
            assert(!AFS::isNullPath(logFilePath));

            cfi.lastRunStats =
            {
                std::chrono::system_clock::to_time_t(r.summary.startTime),
                logFilePath,
                r.summary.result,
                r.summary.statsProcessed.items,
                r.summary.statsProcessed.bytes,
                r.summary.totalTime,
                logStats.errors,
                logStats.warnings,
            };
            break;
        }

    //---------------------------------------------------------------------------
    const BatchStatusHandler::DlgOptions dlgOpt = statusHandler.showResult();

    globalCfg.dpiLayouts[getDpiScalePercent()].progressDlg.size        = dlgOpt.dim.size; //=> ignore dim.pos
    globalCfg.dpiLayouts[getDpiScalePercent()].progressDlg.isMaximized = dlgOpt.dim.isMaximized;

    //----------------------------------------------------------------------
    switch (r.summary.result)
    {
        case TaskResult::success:   raiseExitCode(exitCode_, FfsExitCode::success); break;
        case TaskResult::warning:   raiseExitCode(exitCode_, FfsExitCode::warning); break;
        case TaskResult::error:     raiseExitCode(exitCode_, FfsExitCode::error  ); break;
        case TaskResult::cancelled: raiseExitCode(exitCode_, FfsExitCode::cancelled); break;
    }

    //email sending, or saving log file failed? at least this should affect the exit code:
    if (logStats.errors > 0)
        raiseExitCode(exitCode_, FfsExitCode::error);
    else if (logStats.warnings > 0)
        raiseExitCode(exitCode_, FfsExitCode::warning);

    //---------------------------------------------------------------------------
    try //save global settings to XML: e.g. ignored warnings, last sync stats
    {
        writeConfig(globalCfg, globalCfgFilePath); //FileError
    }
    catch (const FileError& e)
    {
        //raiseExitCode(exitCode_, FfsExitCode::error); -> sync successful
        if (allowUserInteraction)
            showNotificationDialog(nullptr, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString()));
        else
            logExtraError(e.toString());
    }

    //---------------------------------------------------------------------------
    //run shutdown *after* saving global config! https://freefilesync.org/forum/viewtopic.php?t=5761
    using FinalRequest = BatchStatusHandler::FinalRequest;
    switch (dlgOpt.finalRequest)
    {
        case FinalRequest::none:
            break;

        case FinalRequest::switchGui: //open new top-level window *after* progress dialog is gone => run on main event loop
            MainDialog::create(batchCfg.guiCfg, {cfgFilePath}, globalCfg, globalCfgFilePath, true /*startComparison*/);
            break;

        case FinalRequest::shutdown:
            try
            {
                shutdownSystem(); //throw FileError
                terminateProcess(static_cast<int>(exitCode_)); //better exit in a controlled manner rather than letting the OS kill us any time!
            }
            catch (const FileError& e)
            {
                //raiseExitCode(exitCode_, FfsExitCode::error); -> no! sync was successful
                if (allowUserInteraction)
                    showNotificationDialog(nullptr, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString()));
                else
                    logExtraError(e.toString());
            }
            break;
    }
}

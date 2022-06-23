// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "application.h"
#include <memory>
#include <zen/file_access.h>
#include <zen/perf.h>
#include <zen/shutdown.h>
#include <zen/process_exec.h>
#include <zen/resolve_path.h>
#include <wx/tooltip.h>
#include <wx/log.h>
#include <wx+/app_main.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include <wx/msgdlg.h>
#include "afs/concrete.h"
#include "base/algorithm.h"
#include "base/comparison.h"
#include "base/synchronization.h"
#include "ui/batch_status_handler.h"
#include "ui/main_dlg.h"
#include "base_tools.h"
//#include "log_file.h"
#include "ffs_paths.h"

    #include <gtk/gtk.h>

using namespace zen;
using namespace fff;


IMPLEMENT_APP(Application)


namespace
{
std::vector<Zstring> getCommandlineArgs(const wxApp& app)
{
    std::vector<Zstring> args;
    for (int i = 1; i < app.argc; ++i) //wxWidgets screws up once again making "argv implicitly convertible to a wxChar**" in 2.9.3,
        args.push_back(utfTo<Zstring>(wxString(app.argv[i]))); //so we are forced to use this pitiful excuse for a range construction!!
    return args;
}

wxDEFINE_EVENT(EVENT_ENTER_EVENT_LOOP, wxCommandEvent);


void showSyntaxHelp()
{
    showNotificationDialog(nullptr, DialogInfoType::info, PopupDialogCfg().
                           setTitle(_("Command line")).
                           setDetailInstructions(_("Syntax:") + L"\n\n" +
                                                 L"FreeFileSync" + L'\n' +
                                                 L"    [" + _("config files:") + L" *.ffs_gui/*.ffs_batch]" + L'\n' +
                                                 L"    [-DirPair " + _("directory") + L' ' + _("directory") + L"]" L"\n" +
                                                 L"    [-Edit]" + L'\n' +
                                                 L"    [" + _("global config file:") + L" GlobalSettings.xml]" + L"\n\n" +

                                                 _("config files:") + L'\n' +
                                                 _("Any number of FreeFileSync \"ffs_gui\" and/or \"ffs_batch\" configuration files.") + L"\n\n" +

                                                 L"-DirPair " + _("directory") + L' ' + _("directory") + L'\n' +
                                                 _("Any number of alternative directory pairs for at most one config file.") + L"\n\n" +

                                                 L"-Edit" + '\n' +
                                                 _("Open the selected configuration for editing only, without executing it.") + L"\n\n" +

                                                 _("global config file:") + L'\n' +
                                                 _("Path to an alternate GlobalSettings.xml file.")));
}
}


void Application::notifyAppError(const std::wstring& msg, FfsExitCode rc)
{
    raiseExitCode(exitCode_, rc);

    const std::wstring msgTypeName = [&]
    {
        switch (rc)
        {
            //*INDENT-OFF*
            case FfsExitCode::success: break;
            case FfsExitCode::warning:   return _("Warning");
            case FfsExitCode::error:     return _("Error");
            case FfsExitCode::aborted:   return _("Error");
            case FfsExitCode::exception: return _("An exception occurred");
            //*INDENT-ON*
        }
        assert(false);
        return std::wstring{};
    }();
    const std::wstring title = copyStringTo<std::wstring>(wxTheApp->GetAppDisplayName()) +
                               (msgTypeName.empty() ? L"" : SPACED_DASH + msgTypeName);

    //error handling strategy unknown and no sync log output available at this point!
    std::cerr << '[' + utfTo<std::string>(title) + "] " + utfTo<std::string>(msg) << '\n';
    //alternative0: std::wcerr: cannot display non-ASCII at all, so why does it exist???
    //alternative1: wxSafeShowMessage => NO console output on Debian x86, WTF!
    //alternative2: wxMessageBox() => works, but we probably shouldn't block during command line usage
}

//##################################################################################################################

bool Application::OnInit()
{
    //do not call wxApp::OnInit() to avoid using wxWidgets command line parser

    //parallel xBRZ-scaling! => run as early as possible
    try { imageResourcesInit(appendPath(getResourceDirPath(), Zstr("Icons.zip"))); }
    catch (const FileError& e) { notifyAppError(e.toString(), FfsExitCode::warning); }
    //errors are not really critical in this context

    //GTK should already have been initialized by wxWidgets (see \src\gtk\app.cpp:wxApp::Initialize)
#if GTK_MAJOR_VERSION == 2
    ::gtk_rc_parse(appendPath(getResourceDirPath(), "Gtk2Styles.rc").c_str());

    //hang on Ubuntu 19.10 (GLib 2.62) caused by ibus initialization: https://freefilesync.org/forum/viewtopic.php?t=6704
    //=> work around 1: bonus: avoid needless DBus calls: https://developer.gnome.org/gio/stable/running-gio-apps.html
    //                  drawback: missing MTP and network links in folder picker: https://freefilesync.org/forum/viewtopic.php?t=6871
    //if (::setenv("GIO_USE_VFS", "local", true /*overwrite*/) != 0)
    //    std::cerr << utfTo<std::string>(formatSystemError("setenv(GIO_USE_VFS)", errno)) << "\n";
    //
    //=> work around 2:
    g_vfs_get_default(); //returns unowned GVfs*
    //no such issue on GTK3!

#elif GTK_MAJOR_VERSION == 3
    auto loadCSS = [&](const char* fileName)
    {
        GtkCssProvider* provider = ::gtk_css_provider_new();
        ZEN_ON_SCOPE_EXIT(::g_object_unref(provider));

        GError* error = nullptr;
        ZEN_ON_SCOPE_EXIT(if (error) ::g_error_free(error));

        ::gtk_css_provider_load_from_path(provider, //GtkCssProvider* css_provider,
                                          (getResourceDirPf() + fileName).c_str(), //const gchar* path,
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
        catch (const SysError& e2) { notifyAppError(e2.toString(), FfsExitCode::warning); }
    }
#else
#error unknown GTK version!
#endif

    try
    {
        /* we're a GUI app: ignore SIGHUP when the parent terminal quits! (or process is killed!)
            => the FFS launcher will still be killed => fine
            => macOS: apparently not needed! interestingly the FFS launcher does receive SIGHUP and *is* killed  */
        if (sighandler_t oldHandler = ::signal(SIGHUP, SIG_IGN);
            oldHandler == SIG_ERR)
            THROW_LAST_SYS_ERROR("signal(SIGHUP)");
        else assert(!oldHandler);
    }
    catch (const SysError& e) { notifyAppError(e.toString(), FfsExitCode::warning); }


    //Windows User Experience Interaction Guidelines: tool tips should have 5s timeout, info tips no timeout => compromise:
    wxToolTip::Enable(true); //wxWidgets screw-up: wxToolTip::SetAutoPop is no-op if global tooltip window is not yet constructed: wxToolTip::Enable creates it
    wxToolTip::SetAutoPop(10'000); //https://docs.microsoft.com/en-us/windows/win32/uxguide/ctrl-tooltips-and-infotips

    SetAppName(L"FreeFileSync"); //if not set, the default is the executable's name!

    //tentatively set program language to OS default until GlobalSettings.xml is read later
    try { localizationInit(appendPath(getResourceDirPath(), Zstr("Languages.zip"))); } //throw FileError
    catch (const FileError& e) { notifyAppError(e.toString(), FfsExitCode::warning); }

    initAfs({getResourceDirPath(), getConfigDirPath()}); //bonus: using FTP Gdrive implicitly inits OpenSSL (used in runSanityChecks() on Linux) already during globals init



    auto onSystemShutdown = []
    {
        onSystemShutdownRunTasks();

        //- it's futile to try and clean up while the process is in full swing (CRASH!) => just terminate!
        //- system sends close events to all open dialogs: If one of these calls wxCloseEvent::Veto(),
        //  e.g. user clicking cancel on save prompt, this would cancel the shutdown
        terminateProcess(static_cast<int>(FfsExitCode::aborted));
    };
    Bind(wxEVT_QUERY_END_SESSION, [onSystemShutdown](wxCloseEvent& event) { onSystemShutdown(); }); //can veto
    Bind(wxEVT_END_SESSION,       [onSystemShutdown](wxCloseEvent& event) { onSystemShutdown(); }); //can *not* veto

    //Note: app start is deferred: batch mode requires the wxApp eventhandler to be established for UI update events. This is not the case at the time of OnInit()!
    Bind(EVENT_ENTER_EVENT_LOOP, &Application::onEnterEventLoop, this);
    AddPendingEvent(wxCommandEvent(EVENT_ENTER_EVENT_LOOP));
    return true; //true: continue processing; false: exit immediately.
}


void Application::onEnterEventLoop(wxEvent& event)
{
    [[maybe_unused]] const bool ubOk = Unbind(EVENT_ENTER_EVENT_LOOP, &Application::onEnterEventLoop, this);
    assert(ubOk);

    launch(getCommandlineArgs(*this)); //determine FFS mode of operation
}


int Application::OnExit()
{
    localizationCleanup();
    imageResourcesCleanup();

    const std::wstring& warningMsg = teardownAfs();
    if (!warningMsg.empty())
        notifyAppError(warningMsg, FfsExitCode::warning);

    return wxApp::OnExit();
}


wxLayoutDirection Application::GetLayoutDirection() const { return getLayoutDirection(); }


int Application::OnRun()
{
    [[maybe_unused]] const int rc = wxApp::OnRun();
    return static_cast<int>(exitCode_);
}


void Application::OnUnhandledException() //handles both wxApp::OnInit() + wxApp::OnRun()
{
    try
    {
        throw; //just re-throw
    }
    catch (const std::bad_alloc& e) //the only kind of exception we don't want crash dumps for
    {
        notifyAppError(utfTo<std::wstring>(e.what()), FfsExitCode::exception);
        terminateProcess(static_cast<int>(FfsExitCode::exception));
    }
    //catch (...) -> Windows: let it crash and create mini dump!!! Linux/macOS: std::exception::what() logged to console
}




void Application::launch(const std::vector<Zstring>& commandArgs)
{
    //wxWidgets app exit handling is weird... we want to exit only if the logical main window is closed, not just *any* window!
    wxTheApp->SetExitOnFrameDelete(false); //prevent popup-windows from becoming temporary top windows leading to program exit after closure
    ZEN_ON_SCOPE_EXIT(if (!globalWindowWasSet()) wxTheApp->ExitMainLoop()); //quit application, if no main window was set (batch silent mode)

    try
    {
        //parse command line arguments
        std::vector<std::pair<Zstring, Zstring>> dirPathPhrasePairs;
        std::vector<std::pair<Zstring, XmlType>> configFiles; //XmlType: batch or GUI files only
        Zstring globalConfigFile;
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
                                        if (std::optional<Zstring> parentPath = getParentFolderPath(itemPath))
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
                    Zstring filePath = getResolvedFilePath(*it);

                    if (!fileAvailable(filePath)) //...be a little tolerant
                    {
                        if (fileAvailable(filePath + Zstr(".ffs_batch")))
                            filePath += Zstr(".ffs_batch");
                        else if (fileAvailable(filePath + Zstr(".ffs_gui")))
                            filePath += Zstr(".ffs_gui");
                        else if (fileAvailable(filePath + Zstr(".xml")))
                            filePath += Zstr(".xml");
                        else
                            throw FileError(replaceCpy(_("Cannot find file %x."), L"%x", fmtPath(filePath)));
                    }

                    switch (getXmlType(filePath)) //throw FileError
                    {
                        case XmlType::gui:
                            configFiles.emplace_back(filePath, XmlType::gui);
                            break;
                        case XmlType::batch:
                            configFiles.emplace_back(filePath, XmlType::batch);
                            break;
                        case XmlType::global:
                            globalConfigFile = filePath;
                            break;
                        case XmlType::other:
                            throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));
                    }
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

        //distinguish sync scenarios:
        //---------------------------
        const Zstring globalConfigFilePath = !globalConfigFile.empty() ? globalConfigFile : getGlobalConfigDefaultPath();

        if (configFiles.empty())
        {
            //gui mode: default startup
            if (dirPathPhrasePairs.empty())
                runGuiMode(globalConfigFilePath);
            //gui mode: default config with given directories
            else
            {
                XmlGuiConfig guiCfg;
                guiCfg.mainCfg.syncCfg.directionCfg.var = SyncVariant::mirror;

                replaceDirectories(guiCfg.mainCfg); //throw FileError

                runGuiMode(globalConfigFilePath, guiCfg, std::vector<Zstring>(), !openForEdit /*startComparison*/);
            }
        }
        else if (configFiles.size() == 1)
        {
            const Zstring filepath = configFiles[0].first;

            //batch mode
            if (configFiles[0].second == XmlType::batch && !openForEdit)
            {
                auto [batchCfg, warningMsg] = readBatchConfig(filepath); //throw FileError
                if (!warningMsg.empty())
                    throw FileError(warningMsg); //batch mode: break on errors AND even warnings!

                replaceDirectories(batchCfg.mainCfg); //throw FileError

                runBatchMode(globalConfigFilePath, batchCfg, filepath);
            }
            //GUI mode: single config (ffs_gui *or* ffs_batch)
            else
            {
                auto [guiCfg, warningMsg] = readAnyConfig({filepath}); //throw FileError
                if (!warningMsg.empty())
                    showNotificationDialog(nullptr, DialogInfoType::warning, PopupDialogCfg().setDetailInstructions(warningMsg));
                //what about simulating changed config on parsing errors?

                replaceDirectories(guiCfg.mainCfg); //throw FileError
                //what about simulating changed config due to directory replacement?
                //-> propably fine to not show as changed on GUI and not ask user to save on exit!

                runGuiMode(globalConfigFilePath, guiCfg, {filepath}, !openForEdit); //caveat: guiCfg and filepath do not match if directories were set/replaced via command line!
            }
        }
        //gui mode: merged configs
        else
        {
            if (!dirPathPhrasePairs.empty())
                throw FileError(_("Directories cannot be set for more than one configuration file."));

            std::vector<Zstring> filePaths;
            for (const auto& [filePath, xmlType] : configFiles)
                filePaths.push_back(filePath);

            const auto& [guiCfg, warningMsg] = readAnyConfig(filePaths); //throw FileError
            if (!warningMsg.empty())
                showNotificationDialog(nullptr, DialogInfoType::warning, PopupDialogCfg().setDetailInstructions(warningMsg));
            //what about simulating changed config on parsing errors?

            runGuiMode(globalConfigFilePath, guiCfg, filePaths, !openForEdit /*startComparison*/);
        }
    }
    catch (const FileError& e)
    {
        notifyAppError(e.toString(), FfsExitCode::aborted);
    }
}


void Application::runGuiMode(const Zstring& globalConfigFilePath) { MainDialog::create(globalConfigFilePath); }


void Application::runGuiMode(const Zstring& globalConfigFilePath,
                             const XmlGuiConfig& guiCfg,
                             const std::vector<Zstring>& cfgFilePaths,
                             bool startComparison)
{
    MainDialog::create(globalConfigFilePath, nullptr, guiCfg, cfgFilePaths, startComparison);
}


void Application::runBatchMode(const Zstring& globalConfigFilePath, const XmlBatchConfig& batchCfg, const Zstring& cfgFilePath)
{
    XmlGlobalSettings globalCfg;
    try
    {
        std::wstring warningMsg;
        std::tie(globalCfg, warningMsg) = readGlobalConfig(globalConfigFilePath); //throw FileError
        assert(warningMsg.empty()); //ignore parsing errors: should be migration problems only *cross-fingers*
    }
    catch (const FileError& e)
    {
        try
        {
            bool cfgFileExists = true;
            try { cfgFileExists  = !!itemStillExists(globalConfigFilePath); /*throw FileError*/ } //=> unclear which exception is more relevant/useless:
            catch (const FileError& e2) { throw FileError(replaceCpy(e.toString(), L"\n\n", L'\n'), replaceCpy(e2.toString(), L"\n\n", L'\n')); }

            if (cfgFileExists)
                throw;
        }
        catch (const FileError& e3)
        {
            return notifyAppError(e3.toString(), FfsExitCode::aborted); //abort sync!
        }
    }

    try
    {
        setLanguage(globalCfg.programLanguage); //throw FileError
    }
    catch (const FileError& e)
    {
        notifyAppError(e.toString(), FfsExitCode::warning);
        //continue!
    }

    //all settings have been read successfully...

    /* regular check for program updates -> disabled for batch
        if (batchCfg.showProgress && manualProgramUpdateRequired())
            checkForUpdatePeriodically(globalCfg.lastUpdateCheck);
        -> WinInet not working when FFS is running as a service!!! https://support.microsoft.com/en-us/help/238425/info-wininet-not-supported-for-use-in-services   */


    std::set<AbstractPath> logFilePathsToKeep;
    for (const ConfigFileItem& item : globalCfg.mainDlg.config.fileHistory)
        logFilePathsToKeep.insert(item.logFilePath);

    const std::chrono::system_clock::time_point syncStartTime = std::chrono::system_clock::now();

    //class handling status updates and error messages
    BatchStatusHandler statusHandler(!batchCfg.batchExCfg.runMinimized,
                                     extractJobName(cfgFilePath),
                                     syncStartTime,
                                     batchCfg.mainCfg.ignoreErrors,
                                     batchCfg.mainCfg.autoRetryCount,
                                     batchCfg.mainCfg.autoRetryDelay,
                                     globalCfg.soundFileSyncFinished,
                                     globalCfg.soundFileAlertPending,
                                     globalCfg.dpiLayouts[getDpiScalePercent()].progressDlg.dlgSize,
                                     globalCfg.dpiLayouts[getDpiScalePercent()].progressDlg.isMaximized,
                                     batchCfg.batchExCfg.autoCloseSummary,
                                     batchCfg.batchExCfg.postSyncAction,
                                     batchCfg.batchExCfg.batchErrorHandling);
    try
    {
        //inform about (important) non-default global settings
        logNonDefaultSettings(globalCfg, statusHandler); //throw AbortProcess

        //batch mode: place directory locks on directories during both comparison AND synchronization
        std::unique_ptr<LockHolder> dirLocks;

        const bool allowUserInteraction = !batchCfg.mainCfg.ignoreErrors && batchCfg.batchExCfg.batchErrorHandling == BatchErrorHandling::showPopup;

        //COMPARE DIRECTORIES
        FolderComparison cmpResult = compare(globalCfg.warnDlgs,
                                             globalCfg.fileTimeTolerance,
                                             allowUserInteraction,
                                             globalCfg.runWithBackgroundPriority,
                                             globalCfg.createLockFile,
                                             dirLocks,
                                             extractCompareCfg(batchCfg.mainCfg),
                                             statusHandler); //throw AbortProcess
        //START SYNCHRONIZATION
        if (!cmpResult.empty())
            synchronize(syncStartTime,
                        globalCfg.verifyFileCopy,
                        globalCfg.copyLockedFiles,
                        globalCfg.copyFilePermissions,
                        globalCfg.failSafeFileCopy,
                        globalCfg.runWithBackgroundPriority,
                        extractSyncCfg(batchCfg.mainCfg),
                        cmpResult,
                        globalCfg.warnDlgs,
                        statusHandler); //throw AbortProcess
    }
    catch (AbortProcess&) {} //exit used by statusHandler

    AbstractPath logFolderPath = createAbstractPath(batchCfg.mainCfg.altLogFolderPathPhrase); //optional
    if (AFS::isNullPath(logFolderPath))
        logFolderPath = createAbstractPath(globalCfg.logFolderPhrase);
    assert(!AFS::isNullPath(logFolderPath)); //mandatory! but still: let's include fall back
    if (AFS::isNullPath(logFolderPath))
        logFolderPath = createAbstractPath(getLogFolderDefaultPath());

    BatchStatusHandler::Result r = statusHandler.reportResults(batchCfg.mainCfg.postSyncCommand, batchCfg.mainCfg.postSyncCondition,
                                                               logFolderPath, globalCfg.logfilesMaxAgeDays, globalCfg.logFormat, logFilePathsToKeep,
                                                               batchCfg.mainCfg.emailNotifyAddress, batchCfg.mainCfg.emailNotifyCondition); //noexcept
    //----------------------------------------------------------------------
    switch (r.syncResult)
    {
        //*INDENT-OFF*
        case SyncResult::finishedSuccess: raiseExitCode(exitCode_, FfsExitCode::success); break;
        case SyncResult::finishedWarning: raiseExitCode(exitCode_, FfsExitCode::warning); break;
        case SyncResult::finishedError:   raiseExitCode(exitCode_, FfsExitCode::error  ); break;
        case SyncResult::aborted:         raiseExitCode(exitCode_, FfsExitCode::aborted); break;
        //*INDENT-ON*
    }

    globalCfg.dpiLayouts[getDpiScalePercent()].progressDlg.dlgSize     = r.dlgSize;
    globalCfg.dpiLayouts[getDpiScalePercent()].progressDlg.isMaximized = r.dlgIsMaximized;

    //email sending, or saving log file failed? at the very least this should affect the exit code:
    if (r.logStats.error > 0)
        raiseExitCode(exitCode_, FfsExitCode::error);
    else if (r.logStats.warning > 0)
        raiseExitCode(exitCode_, FfsExitCode::warning);


    //update last sync stats for the selected cfg file
    for (ConfigFileItem& cfi : globalCfg.mainDlg.config.fileHistory)
        if (equalNativePath(cfi.cfgFilePath, cfgFilePath))
        {
            if (r.syncResult != SyncResult::aborted)
                cfi.lastSyncTime = std::chrono::system_clock::to_time_t(syncStartTime);
            assert(!AFS::isNullPath(r.logFilePath));
            if (!AFS::isNullPath(r.logFilePath))
            {
                cfi.logFilePath = r.logFilePath;
                cfi.logResult   = r.syncResult;
            }
            break;
        }

    //---------------------------------------------------------------------------
    try //save global settings to XML: e.g. ignored warnings, last sync stats
    {
        writeConfig(globalCfg, globalConfigFilePath); //FileError
    }
    catch (const FileError& e)
    {
        notifyAppError(e.toString(), FfsExitCode::warning);
    }

    using FinalRequest = BatchStatusHandler::FinalRequest;
    switch (r.finalRequest)
    {
        case FinalRequest::none:
            break;
        case FinalRequest::switchGui: //open new top-level window *after* progress dialog is gone => run on main event loop
            MainDialog::create(globalConfigFilePath, &globalCfg, convertBatchToGui(batchCfg), {cfgFilePath}, true /*startComparison*/);
            break;
        case FinalRequest::shutdown: //run *after* last sync stats were updated and saved! https://freefilesync.org/forum/viewtopic.php?t=5761
            try
            {
                shutdownSystem(); //throw FileError
                terminateProcess(static_cast<int>(exitCode_)); //no point in continuing and saving cfg again in onSystemShutdown() while the OS will kill us anytime!
            }
            catch (const FileError& e) { notifyAppError(e.toString(), FfsExitCode::error); }
            break;
    }
}

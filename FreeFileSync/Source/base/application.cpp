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
#include <wx/tooltip.h>
#include <wx/log.h>
#include <wx+/app_main.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include <wx/msgdlg.h>
#include "comparison.h"
#include "config.h"
#include "algorithm.h"
#include "synchronization.h"
#include "help_provider.h"
#include "fatal_error.h"
#include "resolve_path.h"
#include "generate_logfile.h"
#include "../ui/batch_status_handler.h"
#include "../ui/main_dlg.h"
#include "../afs/concrete.h"

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

const wxEventType EVENT_ENTER_EVENT_LOOP = wxNewEventType();
}

//##################################################################################################################

bool Application::OnInit()
{
    //do not call wxApp::OnInit() to avoid using wxWidgets command line parser

    ::gtk_init(nullptr, nullptr);
    ::gtk_rc_parse((getResourceDirPf() + "styles.gtk_rc").c_str()); //remove excessive inner border from bitmap buttons


    //Windows User Experience Interaction Guidelines: tool tips should have 5s timeout, info tips no timeout => compromise:
    wxToolTip::Enable(true); //yawn, a wxWidgets screw-up: wxToolTip::SetAutoPop is no-op if global tooltip window is not yet constructed: wxToolTip::Enable creates it
    wxToolTip::SetAutoPop(10000); //https://msdn.microsoft.com/en-us/library/windows/desktop/aa511495

    SetAppName(L"FreeFileSync"); //if not set, the default is the executable's name!

    initResourceImages(getResourceDirPf() + Zstr("Icons.zip")); //parallel xBRZ-scaling! => run as early as possible

    try
    {
        //tentatively set program language to OS default until GlobalSettings.xml is read later
        setLanguage(XmlGlobalSettings().programLanguage); //throw FileError
    }
    catch (FileError&) { assert(false); }

    initAfs({ getResourceDirPf(), getConfigDirPathPf() }); //bonus: using FTP Gdrive implicitly inits OpenSSL (used in runSanityChecks() on Linux) alredy during globals init


    Connect(wxEVT_QUERY_END_SESSION, wxEventHandler(Application::onQueryEndSession), nullptr, this); //can veto
    Connect(wxEVT_END_SESSION,       wxEventHandler(Application::onQueryEndSession), nullptr, this); //can *not* veto

    //Note: app start is deferred: batch mode requires the wxApp eventhandler to be established for UI update events. This is not the case at the time of OnInit()!
    Connect(EVENT_ENTER_EVENT_LOOP, wxEventHandler(Application::onEnterEventLoop), nullptr, this);
    AddPendingEvent(wxCommandEvent(EVENT_ENTER_EVENT_LOOP));
    return true; //true: continue processing; false: exit immediately.
}


int Application::OnExit()
{
    releaseWxLocale();
    cleanupResourceImages();
    teardownAfs();
    return wxApp::OnExit();
}


void Application::onEnterEventLoop(wxEvent& event)
{
    Disconnect(EVENT_ENTER_EVENT_LOOP, wxEventHandler(Application::onEnterEventLoop), nullptr, this);

    //determine FFS mode of operation
    std::vector<Zstring> commandArgs = getCommandlineArgs(*this);
    launch(commandArgs);
}




int Application::OnRun()
{
    try
    {
        wxApp::OnRun();
    }
    catch (const std::bad_alloc& e) //the only kind of exception we don't want crash dumps for
    {
        logFatalError(e.what()); //it's not always possible to display a message box, e.g. corrupted stack, however low-level file output works!

        const auto titleFmt = copyStringTo<std::wstring>(wxTheApp->GetAppDisplayName()) + SPACED_DASH + _("An exception occurred");
        std::cerr << utfTo<std::string>(titleFmt + SPACED_DASH) << e.what() << "\n";
        return FFS_RC_EXCEPTION;
    }
    //catch (...) -> let it crash and create mini dump!!!

    return returnCode_;
}


void Application::onQueryEndSession(wxEvent& event)
{
    if (auto mainWin = dynamic_cast<MainDialog*>(GetTopWindow()))
        mainWin->onQueryEndSession();
    //it's futile to try and clean up while the process is in full swing (CRASH!) => just terminate!
    //also: avoid wxCloseEvent::Veto() cancelling shutdown when some dialogs receive a close event from the system
    terminateProcess(FFS_RC_ABORTED);
}


void runGuiMode  (const Zstring& globalConfigFile);
void runGuiMode  (const Zstring& globalConfigFile, const XmlGuiConfig& guiCfg, const std::vector<Zstring>& cfgFilePaths, bool startComparison);
void runBatchMode(const Zstring& globalConfigFile, const XmlBatchConfig& batchCfg, const Zstring& cfgFilePath, FfsReturnCode& returnCode);
void showSyntaxHelp();


void Application::launch(const std::vector<Zstring>& commandArgs)
{
    //wxWidgets app exit handling is weird... we want to exit only if the logical main window is closed, not just *any* window!
    wxTheApp->SetExitOnFrameDelete(false); //prevent popup-windows from becoming temporary top windows leading to program exit after closure
    ZEN_ON_SCOPE_EXIT(if (!mainWindowWasSet()) wxTheApp->ExitMainLoop();); //quit application, if no main window was set (batch silent mode)

    auto notifyFatalError = [&](const std::wstring& msg, const std::wstring& title)
    {
        logFatalError(utfTo<std::string>(msg));

        //error handling strategy unknown and no sync log output available at this point!
        auto titleFmt = copyStringTo<std::wstring>(wxTheApp->GetAppDisplayName()) + SPACED_DASH + title;
        std::cerr << utfTo<std::string>(titleFmt + SPACED_DASH + msg) << "\n";
        //alternative0: std::wcerr: cannot display non-ASCII at all, so why does it exist???
        //alternative1: wxSafeShowMessage => NO console output on Debian x86, WTF!
        //alternative2: wxMessageBox() => works, but we probably shouldn't block during command line usage
        raiseReturnCode(returnCode_, FFS_RC_ABORTED);
    };

    //parse command line arguments
    std::vector<std::pair<Zstring, Zstring>> dirPathPhrasePairs;
    std::vector<std::pair<Zstring, XmlType>> configFiles; //XmlType: batch or GUI files only
    Zstring globalConfigFile;
    bool openForEdit = false;
    {
        const Zchar* optionEdit    = Zstr("-edit");
        const Zchar* optionDirPair = Zstr("-dirpair");
        const Zchar* optionSendTo  = Zstr("-sendto"); //remaining arguments are unspecified number of folder paths; wonky syntax; let's keep it undocumented

        auto isHelpRequest = [](const Zstring& arg)
        {
            auto it = std::find_if(arg.begin(), arg.end(), [](Zchar c) { return c != Zstr('/') && c != Zstr('-'); });
            if (it == arg.begin()) return false; //require at least one prefix character

            const Zstring argTmp(it, arg.end());
            return equalAsciiNoCase(argTmp, Zstr("help")) ||
                   equalAsciiNoCase(argTmp, Zstr("h"))    ||
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
                    return notifyFatalError(replaceCpy(_("A left and a right directory path are expected after %x."), L"%x", utfTo<std::wstring>(optionDirPair)), _("Syntax error"));
                dirPathPhrasePairs.emplace_back(*it, Zstring());

                if (++it == commandArgs.end() || isCommandLineOption(*it))
                    return notifyFatalError(replaceCpy(_("A left and a right directory path are expected after %x."), L"%x", utfTo<std::wstring>(optionDirPair)), _("Syntax error"));
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
                                if (getItemType(itemPath) == ItemType::FILE) //throw FileError
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
                        return notifyFatalError(replaceCpy(_("Cannot find file %x."), L"%x", fmtPath(filePath)), _("Error"));
                }

                try
                {
                    switch (getXmlType(filePath)) //throw FileError
                    {
                        case XmlType::GUI:
                            configFiles.emplace_back(filePath, XmlType::GUI);
                            break;
                        case XmlType::BATCH:
                            configFiles.emplace_back(filePath, XmlType::BATCH);
                            break;
                        case XmlType::GLOBAL:
                            globalConfigFile = filePath;
                            break;
                        case XmlType::OTHER:
                            return notifyFatalError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)), _("Error"));
                    }
                }
                catch (const FileError& e)
                {
                    return notifyFatalError(e.toString(), _("Error"));
                }
            }
    }
    //----------------------------------------------------------------------------------------------------

    auto hasNonDefaultConfig = [](const LocalPairConfig& lpc)
    {
        return lpc != LocalPairConfig{ lpc.folderPathPhraseLeft,
                                       lpc.folderPathPhraseRight,
                                       std::nullopt, std::nullopt, FilterConfig() };
    };

    auto replaceDirectories = [&](MainConfiguration& mainCfg)
    {
        if (!dirPathPhrasePairs.empty())
        {
            //check if config at folder-pair level is present: this probably doesn't make sense when replacing/adding the user-specified directories
            if (hasNonDefaultConfig(mainCfg.firstPair) || std::any_of(mainCfg.additionalPairs.begin(), mainCfg.additionalPairs.end(), hasNonDefaultConfig))
            {
                notifyFatalError(_("The config file must not contain settings at directory pair level when directories are set via command line."), _("Syntax error"));
                return false;
            }

            mainCfg.additionalPairs.clear();
            for (size_t i = 0; i < dirPathPhrasePairs.size(); ++i)
                if (i == 0)
                {
                    mainCfg.firstPair.folderPathPhraseLeft  = dirPathPhrasePairs[0].first;
                    mainCfg.firstPair.folderPathPhraseRight = dirPathPhrasePairs[0].second;
                }
                else
                    mainCfg.additionalPairs.push_back({ dirPathPhrasePairs[i].first, dirPathPhrasePairs[i].second,
                                                        std::nullopt, std::nullopt, FilterConfig() });
        }
        return true;
    };

    //distinguish sync scenarios:
    //---------------------------
    const Zstring globalConfigFilePath = !globalConfigFile.empty() ? globalConfigFile : getGlobalConfigFile();

    if (configFiles.empty())
    {
        //gui mode: default startup
        if (dirPathPhrasePairs.empty())
            runGuiMode(globalConfigFilePath);
        //gui mode: default config with given directories
        else
        {
            XmlGuiConfig guiCfg;
            guiCfg.mainCfg.syncCfg.directionCfg.var = DirectionConfig::MIRROR;

            if (!replaceDirectories(guiCfg.mainCfg))
                return;
            runGuiMode(globalConfigFilePath, guiCfg, std::vector<Zstring>(), !openForEdit /*startComparison*/);
        }
    }
    else if (configFiles.size() == 1)
    {
        const Zstring filepath = configFiles[0].first;

        //batch mode
        if (configFiles[0].second == XmlType::BATCH && !openForEdit)
        {
            XmlBatchConfig batchCfg;
            try
            {
                std::wstring warningMsg;
                readConfig(filepath, batchCfg, warningMsg); //throw FileError

                if (!warningMsg.empty())
                    throw FileError(warningMsg); //batch mode: break on errors AND even warnings!
            }
            catch (const FileError& e)
            {
                return notifyFatalError(e.toString(), _("Error"));
            }
            if (!replaceDirectories(batchCfg.mainCfg))
                return;
            runBatchMode(globalConfigFilePath, batchCfg, filepath, returnCode_);
        }
        //GUI mode: single config (ffs_gui *or* ffs_batch)
        else
        {
            XmlGuiConfig guiCfg;
            try
            {
                std::wstring warningMsg;
                readAnyConfig({ filepath }, guiCfg, warningMsg); //throw FileError

                if (!warningMsg.empty())
                    showNotificationDialog(nullptr, DialogInfoType::WARNING, PopupDialogCfg().setDetailInstructions(warningMsg));
                //what about simulating changed config on parsing errors?
            }
            catch (const FileError& e)
            {
                return notifyFatalError(e.toString(), _("Error"));
            }
            if (!replaceDirectories(guiCfg.mainCfg))
                return;
            //what about simulating changed config due to directory replacement?
            //-> propably fine to not show as changed on GUI and not ask user to save on exit!

            runGuiMode(globalConfigFilePath, guiCfg, { filepath }, !openForEdit); //caveat: guiCfg and filepath do not match if directories were set/replaced via command line!
        }
    }
    //gui mode: merged configs
    else
    {
        if (!dirPathPhrasePairs.empty())
            return notifyFatalError(_("Directories cannot be set for more than one configuration file."), _("Syntax error"));

        std::vector<Zstring> filePaths;
        for (const auto& [filePath, xmlType] : configFiles)
            filePaths.push_back(filePath);

        XmlGuiConfig guiCfg; //structure to receive gui settings with default values
        try
        {
            std::wstring warningMsg;
            readAnyConfig(filePaths, guiCfg, warningMsg); //throw FileError

            if (!warningMsg.empty())
                showNotificationDialog(nullptr, DialogInfoType::WARNING, PopupDialogCfg().setDetailInstructions(warningMsg));
            //what about simulating changed config on parsing errors?
        }
        catch (const FileError& e)
        {
            return notifyFatalError(e.toString(), _("Error"));
        }
        runGuiMode(globalConfigFilePath, guiCfg, filePaths, !openForEdit /*startComparison*/);
    }
}


void runGuiMode(const Zstring& globalConfigFilePath) { MainDialog::create(globalConfigFilePath); }


void runGuiMode(const Zstring& globalConfigFilePath,
                const XmlGuiConfig& guiCfg,
                const std::vector<Zstring>& cfgFilePaths,
                bool startComparison)
{
    MainDialog::create(globalConfigFilePath, nullptr, guiCfg, cfgFilePaths, startComparison);
}


void showSyntaxHelp()
{
    showNotificationDialog(nullptr, DialogInfoType::INFO, PopupDialogCfg().
                           setTitle(_("Command line")).
                           setDetailInstructions(_("Syntax:") + L"\n\n" +
                                                 L"./FreeFileSync" + L"\n" +
                                                 L"    [" + _("config files:") + L" *.ffs_gui/*.ffs_batch]" + L"\n" +
                                                 L"    [-DirPair " + _("directory") + L" " + _("directory") + L"]" + L"\n" +
                                                 L"    [-Edit]" + L"\n" +
                                                 L"    [" + _("global config file:") + L" GlobalSettings.xml]" + L"\n" +
                                                 L"\n" +

                                                 _("config files:") + L"\n" +
                                                 _("Any number of FreeFileSync \"ffs_gui\" and/or \"ffs_batch\" configuration files.") + L"\n\n" +

                                                 L"-DirPair " + _("directory") + L" " + _("directory") + L"\n" +
                                                 _("Any number of alternative directory pairs for at most one config file.") + L"\n\n" +

                                                 L"-Edit" + L"\n" +
                                                 _("Open the selected configuration for editing only, without executing it.") + L"\n\n" +

                                                 _("global config file:") + L"\n" +
                                                 _("Path to an alternate GlobalSettings.xml file.")));
}


void runBatchMode(const Zstring& globalConfigFilePath, const XmlBatchConfig& batchCfg, const Zstring& cfgFilePath, FfsReturnCode& returnCode)
{
    const bool showPopupAllowed = !batchCfg.mainCfg.ignoreErrors && batchCfg.batchExCfg.batchErrorHandling == BatchErrorHandling::SHOW_POPUP;

    auto notifyError = [&](const std::wstring& msg, FfsReturnCode rc)
    {
        if (showPopupAllowed)
            showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(msg));
        else //"exit" or "ignore"
            logFatalError(utfTo<std::string>(msg));

        raiseReturnCode(returnCode, rc);
    };

    XmlGlobalSettings globalCfg;
    try
    {
        std::wstring warningMsg;
        readConfig(globalConfigFilePath, globalCfg, warningMsg); //throw FileError
        assert(warningMsg.empty()); //ignore parsing errors: should be migration problems only *cross-fingers*
    }
    catch (FileError&)
    {
        try
        {
            if (itemStillExists(globalConfigFilePath)) //throw FileError
                throw;
        }
        catch (const FileError& e)
        {
            return notifyError(e.toString(), FFS_RC_ABORTED); //abort sync!
        }
    }

    try
    {
        setLanguage(globalCfg.programLanguage); //throw FileError
    }
    catch (const FileError& e)
    {
        notifyError(e.toString(), FFS_RC_FINISHED_WITH_WARNINGS);
        //continue!
    }

    //all settings have been read successfully...

    //regular check for program updates -> disabled for batch
    //if (batchCfg.showProgress && manualProgramUpdateRequired())
    //    checkForUpdatePeriodically(globalCfg.lastUpdateCheck);
    //WinInet not working when FFS is running as a service!!! https://support.microsoft.com/en-us/kb/238425


    std::set<AbstractPath> logFilePathsToKeep;
    for (const ConfigFileItem& item : globalCfg.gui.mainDlg.cfgFileHistory)
        logFilePathsToKeep.insert(item.logFilePath);

    const std::chrono::system_clock::time_point syncStartTime = std::chrono::system_clock::now();

    //class handling status updates and error messages
    BatchStatusHandler statusHandler(!batchCfg.batchExCfg.runMinimized,
                                     batchCfg.batchExCfg.autoCloseSummary,
                                     extractJobName(cfgFilePath),
                                     globalCfg.soundFileSyncFinished,
                                     syncStartTime,
                                     batchCfg.mainCfg.ignoreErrors,
                                     batchCfg.batchExCfg.batchErrorHandling,
                                     batchCfg.mainCfg.automaticRetryCount,
                                     batchCfg.mainCfg.automaticRetryDelay,
                                     batchCfg.mainCfg.postSyncCommand,
                                     batchCfg.mainCfg.postSyncCondition,
                                     batchCfg.batchExCfg.postSyncAction);
    try
    {
        //inform about (important) non-default global settings
        logNonDefaultSettings(globalCfg, statusHandler); //throw AbortProcess

        //batch mode: place directory locks on directories during both comparison AND synchronization
        std::unique_ptr<LockHolder> dirLocks;

        //COMPARE DIRECTORIES
        FolderComparison cmpResult = compare(globalCfg.warnDlgs,
                                             globalCfg.fileTimeTolerance,
                                             showPopupAllowed, //allowUserInteraction
                                             globalCfg.runWithBackgroundPriority,
                                             globalCfg.createLockFile,
                                             dirLocks,
                                             extractCompareCfg(batchCfg.mainCfg),
                                             statusHandler); //throw AbortProcess
        //START SYNCHRONIZATION
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

    BatchStatusHandler::Result r = statusHandler.reportFinalStatus(batchCfg.mainCfg.altLogFolderPathPhrase, globalCfg.logfilesMaxAgeDays, logFilePathsToKeep); //noexcept
    //----------------------------------------------------------------------

    raiseReturnCode(returnCode, mapToReturnCode(r.finalStatus));

    //update last sync stats for the selected cfg file
    for (ConfigFileItem& cfi : globalCfg.gui.mainDlg.cfgFileHistory)
        if (equalNativePath(cfi.cfgFilePath, cfgFilePath))
        {
            if (r.finalStatus != SyncResult::ABORTED)
                cfi.lastSyncTime = std::chrono::system_clock::to_time_t(syncStartTime);
            assert(!AFS::isNullPath(r.logFilePath));
            if (!AFS::isNullPath(r.logFilePath))
            {
                cfi.logFilePath = r.logFilePath;
                cfi.logResult   = r.finalStatus;
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
        notifyError(e.toString(), FFS_RC_FINISHED_WITH_WARNINGS);
    }

    using FinalRequest = BatchStatusHandler::FinalRequest;
    switch (r.finalRequest)
    {
        case FinalRequest::none:
            break;
        case FinalRequest::switchGui: //open new top-level window *after* progress dialog is gone => run on main event loop
            MainDialog::create(globalConfigFilePath, &globalCfg, convertBatchToGui(batchCfg), { cfgFilePath }, true /*startComparison*/);
            break;
        case FinalRequest::shutdown: //run *after* last sync stats were updated and saved! https://freefilesync.org/forum/viewtopic.php?t=5761
            try
            {
                shutdownSystem(); //throw FileError
                terminateProcess(0 /*exitCode*/); //no point in continuing and saving cfg again in onQueryEndSession() while the OS will kill us anytime!
            }
            catch (const FileError& e) { notifyError(e.toString(), FFS_RC_FINISHED_WITH_WARNINGS); }
            break;
    }
}

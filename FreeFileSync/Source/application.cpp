// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "application.h"
#include <memory>
#include <zen/file_access.h>
#include <zen/perf.h>
#include <wx/tooltip.h>
#include <wx/log.h>
#include <wx+/app_main.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "comparison.h"
#include "algorithm.h"
#include "synchronization.h"
#include "ui/batch_status_handler.h"
#include "ui/main_dlg.h"
#include "lib/help_provider.h"
#include "lib/process_xml.h"
#include "lib/error_log.h"
#include "lib/resolve_path.h"

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
    ::gtk_rc_parse((getResourceDirPf() + "styles.gtk_rc").c_str()); //remove inner border from bitmap buttons

    //Windows User Experience Interaction Guidelines: tool tips should have 5s timeout, info tips no timeout => compromise:
    wxToolTip::Enable(true); //yawn, a wxWidgets screw-up: wxToolTip::SetAutoPop is no-op if global tooltip window is not yet constructed: wxToolTip::Enable creates it
    wxToolTip::SetAutoPop(10000); //https://msdn.microsoft.com/en-us/library/windows/desktop/aa511495

    SetAppName(L"FreeFileSync"); //if not set, the default is the executable's name!

    initResourceImages(getResourceDirPf() + Zstr("Resources.zip"));

    try
    {
        //tentatively set program language to OS default until GlobalSettings.xml is read later
        setLanguage(XmlGlobalSettings().programLanguage); //throw FileError
    }
    catch (const FileError&) { assert(false); }


    Connect(wxEVT_QUERY_END_SESSION, wxEventHandler(Application::onQueryEndSession), nullptr, this);
    Connect(wxEVT_END_SESSION,       wxEventHandler(Application::onQueryEndSession), nullptr, this);

    //Note: app start is deferred: batch mode requires the wxApp eventhandler to be established for UI update events. This is not the case at the time of OnInit()!
    Connect(EVENT_ENTER_EVENT_LOOP, wxEventHandler(Application::onEnterEventLoop), nullptr, this);
    AddPendingEvent(wxCommandEvent(EVENT_ENTER_EVENT_LOOP));
    return true; //true: continue processing; false: exit immediately.
}


int Application::OnExit()
{
    uninitializeHelp();
    releaseWxLocale();
    cleanupResourceImages();
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

        const auto title = copyStringTo<std::wstring>(wxTheApp->GetAppDisplayName()) + SPACED_DASH + _("An exception occurred");
        wxSafeShowMessage(title, e.what());
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
    std::abort(); //on Windows calls ::ExitProcess() which can still internally process Window messages and crash!
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

        //error handling strategy unknown and no sync log output available at this point! => show message box
        auto titleFmt = copyStringTo<std::wstring>(wxTheApp->GetAppDisplayName()) + SPACED_DASH + title;
        wxSafeShowMessage(titleFmt, msg);

        raiseReturnCode(returnCode_, FFS_RC_ABORTED);
    };

    //parse command line arguments
    std::vector<std::pair<Zstring, Zstring>> dirPathPhrasePairs;
    std::vector<std::pair<Zstring, XmlType>> configFiles; //XmlType: batch or GUI files only
    Zstring globalConfigFile;
    bool openForEdit = false;
    {
        std::vector<Zstring> dirPathPhrasesLeft;  //TODO: remove migration code at some time! 2017-12-14
        std::vector<Zstring> dirPathPhrasesRight; //

        const Zchar optionEdit    [] = Zstr("-edit");
        const Zchar optionLeftDir [] = Zstr("-leftdir");  //TODO: remove migration code at some time! 2017-12-14
        const Zchar optionRightDir[] = Zstr("-rightdir"); //
        const Zchar optionDirPair [] = Zstr("-dirpair");
        const Zchar optionSendTo  [] = Zstr("-sendto"); //remaining arguments are unspecified number of folder paths; wonky syntax; let's keep it undocumented

        auto syntaxHelpRequested = [&](const Zstring& arg)
        {
            auto it = std::find_if(arg.begin(), arg.end(), [](Zchar c) { return c != Zchar('/') && c != Zchar('-'); });
            if (it == arg.begin()) return false; //require at least one prefix character

            const Zstring argTmp(it, arg.end());
            return strEqual(argTmp, Zstr("help"), CmpAsciiNoCase()) ||
                   strEqual(argTmp, Zstr("h"),    CmpAsciiNoCase()) ||
                   argTmp == Zstr("?");
        };

        auto isCommandLineOption = [&](const Zstring& arg)
        {
            return strEqual(arg, optionEdit,     CmpAsciiNoCase()) ||
                   strEqual(arg, optionLeftDir,  CmpAsciiNoCase()) ||
                   strEqual(arg, optionRightDir, CmpAsciiNoCase()) ||
                   strEqual(arg, optionDirPair,  CmpAsciiNoCase()) ||
                   strEqual(arg, optionSendTo,   CmpAsciiNoCase()) ||
                   syntaxHelpRequested(arg);
        };

        for (auto it = commandArgs.begin(); it != commandArgs.end(); ++it)
            if (syntaxHelpRequested(*it))
                return showSyntaxHelp();
            else if (strEqual(*it, optionEdit, CmpAsciiNoCase()))
                openForEdit = true;
            else if (strEqual(*it, optionLeftDir, CmpAsciiNoCase()))
            {
                if (++it == commandArgs.end() || isCommandLineOption(*it))
                {
                    notifyFatalError(replaceCpy(_("A directory path is expected after %x."), L"%x", utfTo<std::wstring>(optionLeftDir)), _("Syntax error"));
                    return;
                }
                dirPathPhrasesLeft.push_back(*it);
            }
            else if (strEqual(*it, optionRightDir, CmpAsciiNoCase()))
            {
                if (++it == commandArgs.end() || isCommandLineOption(*it))
                {
                    notifyFatalError(replaceCpy(_("A directory path is expected after %x."), L"%x", utfTo<std::wstring>(optionRightDir)), _("Syntax error"));
                    return;
                }
                dirPathPhrasesRight.push_back(*it);
            }
            else if (strEqual(*it, optionDirPair, CmpAsciiNoCase()))
            {
                if (++it == commandArgs.end() || isCommandLineOption(*it))
                {
                    notifyFatalError(replaceCpy(_("A left and a right directory path are expected after %x."), L"%x", utfTo<std::wstring>(optionDirPair)), _("Syntax error"));
                    return;
                }
                dirPathPhrasePairs.emplace_back(*it, Zstring());

                if (++it == commandArgs.end() || isCommandLineOption(*it))
                {
                    notifyFatalError(replaceCpy(_("A left and a right directory path are expected after %x."), L"%x", utfTo<std::wstring>(optionDirPair)), _("Syntax error"));
                    return;
                }
                dirPathPhrasePairs.back().second = *it;
            }
            else if (strEqual(*it, optionSendTo, CmpAsciiNoCase()))
            {
                for (size_t i = 0; ; ++i)
                {
                    if (++it == commandArgs.end() || isCommandLineOption(*it))
                    {
                        --it;
                        break;
                    }

                    if (i < 2) //-SendTo with more than 2 paths? Doesn't make any sense, does it!?
                    {
                        //for -SendTo we expect a list of full native paths, not "phrases" that need to be resolved!
                        auto getFolderPath = [](Zstring itemPath)
                        {
                            try
                            {
                                if (getItemType(itemPath) == ItemType::FILE) //throw FileError
                                    if (Opt<Zstring> parentPath = getParentFolderPath(itemPath))
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
                            if (!equalFilePath(dirPathPhrasePairs.back().first, folderPath)) //user accidentally sending to two files, which each time yield the same parent folder
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
                    {
                        notifyFatalError(replaceCpy(_("Cannot find file %x."), L"%x", fmtPath(filePath)), _("Error"));
                        return;
                    }
                }

                try
                {
                    switch (getXmlType(filePath)) //throw FileError
                    {
                        case XML_TYPE_GUI:
                            configFiles.emplace_back(filePath, XML_TYPE_GUI);
                            break;
                        case XML_TYPE_BATCH:
                            configFiles.emplace_back(filePath, XML_TYPE_BATCH);
                            break;
                        case XML_TYPE_GLOBAL:
                            globalConfigFile = filePath;
                            break;
                        case XML_TYPE_OTHER:
                            notifyFatalError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)), _("Error"));
                            return;
                    }
                }
                catch (const FileError& e)
                {
                    notifyFatalError(e.toString(), _("Error"));
                    return;
                }
            }

        if (dirPathPhrasesLeft.size() != dirPathPhrasesRight.size())
        {
            notifyFatalError(_("Unequal number of left and right directories specified."), _("Syntax error"));
            return;
        }

        for (size_t i = 0; i < dirPathPhrasesLeft.size(); ++i)
            dirPathPhrasePairs.emplace_back(dirPathPhrasesLeft[i], dirPathPhrasesRight[i]);
    }
    //----------------------------------------------------------------------------------------------------

    auto hasNonDefaultConfig = [](const FolderPairEnh& fp)
    {
        return !(fp == FolderPairEnh(fp.folderPathPhraseLeft_,
                                     fp.folderPathPhraseRight_,
                                     nullptr, nullptr, FilterConfig()));
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
                    mainCfg.firstPair.folderPathPhraseLeft_  = dirPathPhrasePairs[0].first;
                    mainCfg.firstPair.folderPathPhraseRight_ = dirPathPhrasePairs[0].second;
                }
                else
                    mainCfg.additionalPairs.emplace_back(dirPathPhrasePairs[i].first, dirPathPhrasePairs[i].second,
                                                         nullptr, nullptr, FilterConfig());
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
        if (configFiles[0].second == XML_TYPE_BATCH && !openForEdit)
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
                notifyFatalError(e.toString(), _("Error"));
                return;
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
                notifyFatalError(e.toString(), _("Error"));
                return;
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
        {
            notifyFatalError(_("Directories cannot be set for more than one configuration file."), _("Syntax error"));
            return;
        }

        std::vector<Zstring> filepaths;
        for (const auto& item : configFiles)
            filepaths.push_back(item.first);

        XmlGuiConfig guiCfg; //structure to receive gui settings with default values
        try
        {
            std::wstring warningMsg;
            readAnyConfig(filepaths, guiCfg, warningMsg); //throw FileError

            if (!warningMsg.empty())
                showNotificationDialog(nullptr, DialogInfoType::WARNING, PopupDialogCfg().setDetailInstructions(warningMsg));
            //what about simulating changed config on parsing errors?
        }
        catch (const FileError& e)
        {
            notifyFatalError(e.toString(), _("Error"));
            return;
        }
        runGuiMode(globalConfigFilePath, guiCfg, filepaths, !openForEdit /*startComparison*/);
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
                                                 L"./FreeFileSync " + L"\n" +
                                                 L"    [" + _("config files:") + L" *.ffs_gui/*.ffs_batch]" + L"\n" +
                                                 L"    [-DirPair " + _("directory") + L" " + _("directory") + L"]" + L"\n" +
                                                 L"    [-Edit]" + L"\n" +
                                                 L"    [" + _("global config file:") + L" GlobalSettings.xml]" + L"\n" +
                                                 L"\n" +

                                                 _("config files:") + L"\n" +
                                                 _("Any number of FreeFileSync .ffs_gui and/or .ffs_batch configuration files.") + L"\n\n" +

                                                 L"-DirPair " + _("directory") + L" " + _("directory") + L"\n" +
                                                 _("Any number of alternative directory pairs for at most one config file.") + L"\n\n" +

                                                 L"-Edit" + L"\n" +
                                                 _("Open the selected configuration for editing only without executing it.") + L"\n\n" +

                                                 _("global config file:") + L"\n" +
                                                 _("Path to an alternate GlobalSettings.xml file.")));
}


void runBatchMode(const Zstring& globalConfigFilePath, const XmlBatchConfig& batchCfg, const Zstring& cfgFilePath, FfsReturnCode& returnCode)
{
    const bool showPopupAllowed = !batchCfg.mainCfg.ignoreErrors && batchCfg.batchExCfg.batchErrorDialog == BatchErrorDialog::SHOW;

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
    catch (const FileError& e)
    {
        if (!itemNotExisting(globalConfigFilePath)) //existing or access error
            return notifyError(e.toString(), FFS_RC_ABORTED); //abort sync!
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

    try //begin of synchronization process (all in one try-catch block)
    {
        const std::chrono::system_clock::time_point syncStartTime = std::chrono::system_clock::now();

        //class handling status updates and error messages
        BatchStatusHandler statusHandler(!batchCfg.batchExCfg.runMinimized, //throw AbortProcess, BatchRequestSwitchToMainDialog
                                         batchCfg.batchExCfg.autoCloseSummary,
                                         extractJobName(cfgFilePath),
                                         globalCfg.soundFileSyncFinished,
                                         syncStartTime,
                                         batchCfg.batchExCfg.logFolderPathPhrase,
                                         batchCfg.batchExCfg.logfilesCountLimit,
                                         globalCfg.lastSyncsLogFileSizeMax,
                                         batchCfg.mainCfg.ignoreErrors,
                                         batchCfg.batchExCfg.batchErrorDialog,
                                         globalCfg.automaticRetryCount,
                                         globalCfg.automaticRetryDelay,
                                         returnCode,
                                         batchCfg.mainCfg.postSyncCommand,
                                         batchCfg.mainCfg.postSyncCondition,
                                         batchCfg.batchExCfg.postSyncAction);

        logNonDefaultSettings(globalCfg, statusHandler); //inform about (important) non-default global settings

        const std::vector<FolderPairCfg> cmpConfig = extractCompareCfg(batchCfg.mainCfg);

        //batch mode: place directory locks on directories during both comparison AND synchronization
        std::unique_ptr<LockHolder> dirLocks;

        //COMPARE DIRECTORIES
        FolderComparison cmpResult = compare(globalCfg.warnDlgs,
                                             globalCfg.fileTimeTolerance,
                                             showPopupAllowed, //allowUserInteraction
                                             globalCfg.runWithBackgroundPriority,
                                             globalCfg.folderAccessTimeout,
                                             globalCfg.createLockFile,
                                             dirLocks,
                                             cmpConfig,
                                             statusHandler); //throw ?

        //START SYNCHRONIZATION
        const std::vector<FolderPairSyncCfg> syncProcessCfg = extractSyncCfg(batchCfg.mainCfg);
        if (syncProcessCfg.size() != cmpResult.size())
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

        synchronize(syncStartTime,
                    globalCfg.verifyFileCopy,
                    globalCfg.copyLockedFiles,
                    globalCfg.copyFilePermissions,
                    globalCfg.failSafeFileCopy,
                    globalCfg.runWithBackgroundPriority,
                    globalCfg.folderAccessTimeout,
                    syncProcessCfg,
                    cmpResult,
                    globalCfg.warnDlgs,
                    statusHandler); //throw ?

        //not cancelled? => update last sync date for the selected cfg file
        for (ConfigFileItem& cfi : globalCfg.gui.mainDlg.cfgFileHistory)
            if (equalFilePath(cfi.filePath, cfgFilePath))
            {
                cfi.lastSyncTime = std::time(nullptr);
                break;
            }
    }
    catch (AbortProcess&) {} //exit used by statusHandler
    catch (BatchRequestSwitchToMainDialog&)
    {
        //open new toplevel window *after* progress dialog is gone => run on main event loop
        return MainDialog::create(globalConfigFilePath, &globalCfg, convertBatchToGui(batchCfg), { cfgFilePath }, true /*startComparison*/);
    }

    try //save global settings to XML: e.g. ignored warnings
    {
        writeConfig(globalCfg, globalConfigFilePath); //FileError
    }
    catch (const FileError& e)
    {
        notifyError(e.toString(), FFS_RC_FINISHED_WITH_WARNINGS);
    }
}

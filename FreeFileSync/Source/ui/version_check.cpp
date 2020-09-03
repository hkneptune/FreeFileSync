// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "version_check.h"
#include <ctime>
#include <zen/crc.h>
#include <zen/string_tools.h>
#include <zen/i18n.h>
#include <zen/utf.h>
#include <zen/file_access.h>
#include <zen/scope_guard.h>
#include <zen/build_info.h>
#include <zen/basic_math.h>
#include <zen/file_error.h>
#include <zen/http.h>
#include <zen/sys_version.h>
#include <zen/thread.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "../ffs_paths.h"
#include "../version/version.h"
#include "small_dlgs.h"



using namespace zen;
using namespace fff;


namespace
{
const Zchar ffsUpdateCheckUserAgent[] = Zstr("FFS-Update-Check");


time_t getVersionCheckInactiveId()
{
    //use current version to calculate a changing number for the inactive state near UTC begin, in order to always check for updates after installing a new version
    //=> interpret version as 11-based *unique* number (this breaks lexicographical version ordering, but that's irrelevant!)
    int id = 0;
    const char* first = ffsVersion;
    const char* last = first + zen::strLength(ffsVersion);
    std::for_each(first, last, [&](char c)
    {
        id *= 11;
        if ('0' <= c && c <= '9')
            id += c - '0';
        else
        {
            assert(c == FFS_VERSION_SEPARATOR);
            id += 10;
        }
    });
    assert(0 < id && id < 3600 * 24 * 365); //as long as value is within a year after UTC begin (1970) there's no risk to clash with *current* time
    return id;
}




time_t getVersionCheckCurrentTime()
{
    time_t now = std::time(nullptr);
    return now;
}
}


bool fff::shouldRunAutomaticUpdateCheck(time_t lastUpdateCheck)
{
    if (lastUpdateCheck == getVersionCheckInactiveId())
        return false;

    const time_t now = std::time(nullptr);
    return numeric::dist(now, lastUpdateCheck) >= 7 * 24 * 3600; //check weekly
}


std::wstring getIso639Language()
{
    assert(runningOnMainThread()); //this function is not thread-safe, consider wxWidgets usage

    std::wstring localeName(wxLocale::GetLanguageCanonicalName(wxLocale::GetSystemLanguage()));
    localeName = beforeFirst(localeName, L"@", IfNotFoundReturn::all); //the locale may contain an @, e.g. "sr_RS@latin"; see wxLocale::InitLanguagesDB()

    if (!localeName.empty())
    {
        assert(beforeFirst(localeName, L"_", IfNotFoundReturn::all).size() == 2);
        return beforeFirst(localeName, L"_", IfNotFoundReturn::all);
    }
    assert(false);
    return L"zz";
}


namespace
{
std::wstring getIso3166Country()
{
    assert(runningOnMainThread()); //this function is not thread-safe, consider wxWidgets usage

    std::wstring localeName(wxLocale::GetLanguageCanonicalName(wxLocale::GetSystemLanguage()));
    localeName = beforeFirst(localeName, L"@", IfNotFoundReturn::all); //the locale may contain an @, e.g. "sr_RS@latin"; see wxLocale::InitLanguagesDB()

    if (contains(localeName, L"_"))
        return afterFirst(localeName, L"_", IfNotFoundReturn::none);
    assert(false);
    return L"ZZ";
}


//coordinate with get_latest_version_number.php
std::vector<std::pair<std::string, std::string>> geHttpPostParameters(wxWindow& parent)
{
    assert(runningOnMainThread()); //this function is not thread-safe, e.g. consider wxWidgets usage in isPortableVersion()
    std::vector<std::pair<std::string, std::string>> params;

    params.emplace_back("ffs_version", ffsVersion);
    params.emplace_back("installation_type", isPortableVersion() ? "Portable" : "Local");


    params.emplace_back("os_name", "Linux");

    const OsVersion osv = getOsVersion();
    params.emplace_back("os_version", numberTo<std::string>(osv.major) + "." + numberTo<std::string>(osv.minor));

#ifndef ZEN_BUILD_ARCH
#error include <zen/build_info.h>
#endif
    const char* osArch = ZEN_STRINGIZE_NUMBER(ZEN_BUILD_ARCH);
    params.emplace_back("os_arch", osArch);

#if GTK_MAJOR_VERSION == 2
    //wxWindow::GetContentScaleFactor() requires GTK3 or later
#elif GTK_MAJOR_VERSION == 3
    params.emplace_back("dip_scale", numberTo<std::string>(parent.GetContentScaleFactor()));
#else
#error unknown GTK version!
#endif

    params.emplace_back("language", utfTo<std::string>(getIso639Language()));
    params.emplace_back("country",  utfTo<std::string>(getIso3166Country()));
    return params;
}




void showUpdateAvailableDialog(wxWindow* parent, const std::string& onlineVersion)
{
    std::wstring updateDetailsMsg;
    try
    {
        try
        {
            const std::string buf = sendHttpGet(utfTo<Zstring>("https://api.freefilesync.org/latest_changes?" + xWwwFormUrlEncode({ { "since", ffsVersion } })),
            ffsUpdateCheckUserAgent, nullptr /*caCertFilePath*/, nullptr /*notifyUnbufferedIO*/).readAll(); //throw SysError
            updateDetailsMsg = utfTo<std::wstring>(buf);
        }
        catch (const SysError& e) { throw FileError(_("Failed to retrieve update information."), e.toString()); }

    }
    catch (const FileError& e) //fall back to regular update info dialog:
    {
        updateDetailsMsg = e.toString() + L"\n\n\n" + updateDetailsMsg;
    }

    switch (showConfirmationDialog(parent, DialogInfoType::info, PopupDialogCfg().
                                   setIcon(loadImage("update_available")).
                                   setTitle(_("Check for Program Updates")).
                                   setMainInstructions(replaceCpy(_("FreeFileSync %x is available!"), L"%x", utfTo<std::wstring>(onlineVersion)) + L' ' + _("Download now?")).
                                   setDetailInstructions(updateDetailsMsg),
                                   _("&Download")))
    {
        case ConfirmationButton::accept:
                wxLaunchDefaultBrowser(L"https://freefilesync.org/get_latest.php");
            break;
        case ConfirmationButton::cancel:
            break;
    }
}


std::string getOnlineVersion(const std::vector<std::pair<std::string, std::string>>& postParams) //throw SysError
{
    const std::string response = sendHttpPost(Zstr("https://api.freefilesync.org/latest_version"), postParams,
                                              ffsUpdateCheckUserAgent, nullptr /*caCertFilePath*/, nullptr /*notifyUnbufferedIO*/).readAll(); //throw SysError
    return trimCpy(response);
}


std::vector<size_t> parseVersion(const std::string& version)
{
    std::vector<size_t> output;
    for (const std::string& digit : split(version, FFS_VERSION_SEPARATOR, SplitOnEmpty::allow))
        output.push_back(stringTo<size_t>(digit));
    return output;
}
}


bool fff::haveNewerVersionOnline(const std::string& onlineVersion)
{
    const std::vector<size_t> current = parseVersion(ffsVersion);
    const std::vector<size_t> online  = parseVersion(onlineVersion);

    if (online.empty() || online[0] == 0) //online version string may be "This website has been moved..." In this case better check for an update
        return true;

    return std::lexicographical_compare(current.begin(), current.end(),
                                        online .begin(), online .end());
}


bool fff::updateCheckActive(time_t lastUpdateCheck)
{
    return lastUpdateCheck != getVersionCheckInactiveId();
}


void fff::disableUpdateCheck(time_t& lastUpdateCheck)
{
    lastUpdateCheck = getVersionCheckInactiveId();
}


void fff::checkForUpdateNow(wxWindow& parent, std::string& lastOnlineVersion)
{
    try
    {
        const std::string onlineVersion = getOnlineVersion(geHttpPostParameters(parent)); //throw SysError
        lastOnlineVersion = onlineVersion;

        if (haveNewerVersionOnline(onlineVersion))
            showUpdateAvailableDialog(&parent, onlineVersion);
        else
            showNotificationDialog(&parent, DialogInfoType::info, PopupDialogCfg().
                                   setIcon(loadImage("update_check")).
                                   setTitle(_("Check for Program Updates")).
                                   setMainInstructions(_("FreeFileSync is up to date.")));
    }
    catch (const SysError& e)
    {
        if (internetIsAlive())
        {
            lastOnlineVersion = "Unknown";

            switch (showQuestionDialog(&parent, DialogInfoType::error, PopupDialogCfg().
                                       setTitle(_("Check for Program Updates")).
                                       setMainInstructions(_("Cannot find current FreeFileSync version number online. A newer version is likely available. Check manually now?")).
                                       setDetailInstructions(e.toString()), _("&Check"), _("&Retry")))
            {
                case QuestionButton2::yes:
                    wxLaunchDefaultBrowser(L"https://freefilesync.org/get_latest.php");
                    break;
                case QuestionButton2::no: //retry
                    checkForUpdateNow(parent, lastOnlineVersion); //note: retry via recursion!!!
                    break;
                case QuestionButton2::cancel:
                    break;
            }
        }
        else
            switch (showConfirmationDialog(&parent, DialogInfoType::error, PopupDialogCfg().
                                           setTitle(_("Check for Program Updates")).
                                           setMainInstructions(replaceCpy(_("Unable to connect to %x."), L"%x", L"freefilesync.org")).
                                           setDetailInstructions(e.toString()), _("&Retry")))
            {
                case ConfirmationButton::accept: //retry
                    checkForUpdateNow(parent, lastOnlineVersion); //note: retry via recursion!!!
                    break;
                case ConfirmationButton::cancel:
                    break;
            }
    }
}


struct fff::UpdateCheckResultPrep
{
    std::vector<std::pair<std::string, std::string>> postParameters;
};

std::shared_ptr<const UpdateCheckResultPrep> fff::automaticUpdateCheckPrepare(wxWindow& parent)
{
    assert(runningOnMainThread());
    auto prep = std::make_shared<UpdateCheckResultPrep>();
    prep->postParameters = geHttpPostParameters(parent);
    return prep;
}


struct fff::UpdateCheckResult
{
    UpdateCheckResult(const std::string& ver, const std::optional<SysError>& err, bool alive)  : onlineVersion(ver), error(err), internetIsAlive(alive) {}

    std::string onlineVersion;
    std::optional<SysError> error;
    bool internetIsAlive = false;
};

std::shared_ptr<const UpdateCheckResult> fff::automaticUpdateCheckRunAsync(const UpdateCheckResultPrep* resultPrep)
{
    //assert(!runningOnMainThread()); -> allow synchronous call, too
    try
    {
        const std::string onlineVersion = getOnlineVersion(resultPrep->postParameters); //throw SysError
        return std::make_shared<UpdateCheckResult>(onlineVersion, std::nullopt, true);
    }
    catch (const SysError& e)
    {
        return std::make_shared<UpdateCheckResult>("", e, internetIsAlive());
    }
}


void fff::automaticUpdateCheckEval(wxWindow* parent, time_t& lastUpdateCheck, std::string& lastOnlineVersion, const UpdateCheckResult* asyncResult)
{
    assert(runningOnMainThread());


    const UpdateCheckResult& result = *asyncResult;

    if (!result.error)
    {
        lastUpdateCheck   = getVersionCheckCurrentTime();
        lastOnlineVersion = result.onlineVersion;

            if (haveNewerVersionOnline(result.onlineVersion))
                showUpdateAvailableDialog(parent, result.onlineVersion);
    }
    else
    {
        if (result.internetIsAlive)
        {
            lastOnlineVersion = "Unknown";

                switch (showQuestionDialog(parent, DialogInfoType::error, PopupDialogCfg().
                                           setTitle(_("Check for Program Updates")).
                                           setMainInstructions(_("Cannot find current FreeFileSync version number online. A newer version is likely available. Check manually now?")).
                                           setDetailInstructions(result.error->toString()),
                                           _("&Check"), _("&Retry")))
                {
                    case QuestionButton2::yes:
                        wxLaunchDefaultBrowser(L"https://freefilesync.org/get_latest.php");
                        break;
                    case QuestionButton2::no: //retry
                        automaticUpdateCheckEval(parent, lastUpdateCheck, lastOnlineVersion, asyncResult); //note: retry via recursion!!!
                        break;
                    case QuestionButton2::cancel:
                        break;
                }
        }
        //else: ignore this error
    }
}



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
#include <zen/thread.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "../base/ffs_paths.h"
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
    assert(runningMainThread()); //this function is not thread-safe, consider wxWidgets usage

    std::wstring localeName(wxLocale::GetLanguageCanonicalName(wxLocale::GetSystemLanguage()));
    localeName = beforeFirst(localeName, L"@", IF_MISSING_RETURN_ALL); //the locale may contain an @, e.g. "sr_RS@latin"; see wxLocale::InitLanguagesDB()

    if (!localeName.empty())
    {
        assert(beforeFirst(localeName, L"_", IF_MISSING_RETURN_ALL).size() == 2);
        return beforeFirst(localeName, L"_", IF_MISSING_RETURN_ALL);
    }
    assert(false);
    return L"zz";
}


namespace
{
std::wstring getIso3166Country()
{
    assert(runningMainThread()); //this function is not thread-safe, consider wxWidgets usage

    std::wstring localeName(wxLocale::GetLanguageCanonicalName(wxLocale::GetSystemLanguage()));
    localeName = beforeFirst(localeName, L"@", IF_MISSING_RETURN_ALL); //the locale may contain an @, e.g. "sr_RS@latin"; see wxLocale::InitLanguagesDB()

    if (contains(localeName, L"_"))
        return afterFirst(localeName, L"_", IF_MISSING_RETURN_NONE);
    assert(false);
    return L"ZZ";
}


//coordinate with get_latest_version_number.php
std::vector<std::pair<std::string, std::string>> geHttpPostParameters()
{
    assert(runningMainThread()); //this function is not thread-safe, e.g. consider wxWidgets usage in isPortableVersion()
    std::vector<std::pair<std::string, std::string>> params;

    params.emplace_back("ffs_version", ffsVersion);
    params.emplace_back("installation_type", isPortableVersion() ? "Portable" : "Local");


    params.emplace_back("os_name", "Linux");

    const wxLinuxDistributionInfo distribInfo = wxGetLinuxDistributionInfo();
    assert(contains(distribInfo.Release, L'.'));
    std::vector<wxString> digits = split<wxString>(distribInfo.Release, L'.', SplitType::ALLOW_EMPTY); //e.g. "15.04"
    digits.resize(2);
    //distribInfo.Id //e.g. "Ubuntu"

    const int osvMajor = stringTo<int>(digits[0]);
    const int osvMinor = stringTo<int>(digits[1]);

    params.emplace_back("os_version", numberTo<std::string>(osvMajor) + "." + numberTo<std::string>(osvMinor));

#ifdef ZEN_BUILD_32BIT
    const char* osArch = "32";
#elif defined ZEN_BUILD_64BIT
    const char* osArch = "64";
#endif
    params.emplace_back("os_arch", osArch);

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
            //consider wxHTTP limitation: URL must be accessible without https!!!
            const std::string buf = sendHttpPost(Zstr("http://freefilesync.org/get_latest_changes.php"), ffsUpdateCheckUserAgent,
            nullptr /*notifyUnbufferedIO*/, { { "since", ffsVersion } }).readAll(); //throw SysError
            updateDetailsMsg = utfTo<std::wstring>(buf);
        }
        catch (const zen::SysError& e) { throw FileError(_("Failed to retrieve update information."), e.toString()); }

    }
    catch (const FileError& e) //fall back to regular update info dialog:
    {
        updateDetailsMsg = e.toString() + L"\n\n\n" + updateDetailsMsg;
    }

    switch (showConfirmationDialog(parent, DialogInfoType::INFO, PopupDialogCfg().
                                   setIcon(getResourceImage(L"update_available")).
                                   setTitle(_("Check for Program Updates")).
                                   setMainInstructions(replaceCpy(_("FreeFileSync %x is available!"), L"%x", utfTo<std::wstring>(onlineVersion)) + L" " + _("Download now?")).
                                   setDetailInstructions(updateDetailsMsg),
                                   _("&Download")))
    {
        case ConfirmationButton::ACCEPT:
                wxLaunchDefaultBrowser(L"https://freefilesync.org/get_latest.php");
            break;
        case ConfirmationButton::CANCEL:
            break;
    }
}


//access is thread-safe on Windows (WinInet), but not on Linux/OS X (wxWidgets)
std::string getOnlineVersion(const std::vector<std::pair<std::string, std::string>>& postParams) //throw SysError
{
    //consider wxHTTP limitation: URL must be accessible without https!!!
    const std::string buffer = sendHttpPost(Zstr("http://freefilesync.org/get_latest_version_number.php"), ffsUpdateCheckUserAgent,
                                            nullptr /*notifyUnbufferedIO*/, postParams).readAll(); //throw SysError
    return trimCpy(buffer);
}


std::vector<size_t> parseVersion(const std::string& version)
{
    std::vector<size_t> output;
    for (const std::string& digit : split(version, FFS_VERSION_SEPARATOR, SplitType::ALLOW_EMPTY))
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


void fff::checkForUpdateNow(wxWindow* parent, std::string& lastOnlineVersion)
{
    try
    {
        const std::string onlineVersion = getOnlineVersion(geHttpPostParameters()); //throw SysError
        lastOnlineVersion = onlineVersion;

        if (haveNewerVersionOnline(onlineVersion))
            showUpdateAvailableDialog(parent, onlineVersion);
        else
            showNotificationDialog(parent, DialogInfoType::INFO, PopupDialogCfg().
                                   setIcon(getResourceImage(L"update_check")).
                                   setTitle(_("Check for Program Updates")).
                                   setMainInstructions(_("FreeFileSync is up to date.")));
    }
    catch (const zen::SysError& e)
    {
        if (internetIsAlive())
        {
            lastOnlineVersion = "Unknown";

            switch (showQuestionDialog(parent, DialogInfoType::ERROR2, PopupDialogCfg().
                                       setTitle(_("Check for Program Updates")).
                                       setMainInstructions(_("Cannot find current FreeFileSync version number online. A newer version is likely available. Check manually now?")).
                                       setDetailInstructions(e.toString()), _("&Check"), _("&Retry")))
            {
                case QuestionButton2::YES:
                    wxLaunchDefaultBrowser(L"https://freefilesync.org/get_latest.php");
                    break;
                case QuestionButton2::NO: //retry
                    checkForUpdateNow(parent, lastOnlineVersion); //note: retry via recursion!!!
                    break;
                case QuestionButton2::CANCEL:
                    break;
            }
        }
        else
            switch (showConfirmationDialog(parent, DialogInfoType::ERROR2, PopupDialogCfg().
                                           setTitle(_("Check for Program Updates")).
                                           setMainInstructions(replaceCpy(_("Unable to connect to %x."), L"%x", L"freefilesync.org")).
                                           setDetailInstructions(e.toString()), _("&Retry")))
            {
                case ConfirmationButton::ACCEPT: //retry
                    checkForUpdateNow(parent, lastOnlineVersion); //note: retry via recursion!!!
                    break;
                case ConfirmationButton::CANCEL:
                    break;
            }
    }
}


struct fff::UpdateCheckResultPrep
{
    const std::vector<std::pair<std::string, std::string>> postParameters = geHttpPostParameters();
};

std::shared_ptr<UpdateCheckResultPrep> fff::automaticUpdateCheckPrepare()
{
    assert(runningMainThread());
    return std::make_shared<UpdateCheckResultPrep>();
}


struct fff::UpdateCheckResult
{
    UpdateCheckResult(const std::string& ver, const std::optional<zen::SysError>& err, bool alive)  : onlineVersion(ver), error(err), internetIsAlive(alive) {}

    std::string onlineVersion;
    std::optional<zen::SysError> error;
    bool internetIsAlive = false;
};

std::shared_ptr<UpdateCheckResult> fff::automaticUpdateCheckRunAsync(const UpdateCheckResultPrep* resultPrep)
{
    //assert(!runningMainThread()); -> allow synchronous call, too
    try
    {
        const std::string onlineVersion = getOnlineVersion(resultPrep->postParameters); //throw SysError
        return std::make_shared<UpdateCheckResult>(onlineVersion, std::nullopt, true);
    }
    catch (const zen::SysError& e)
    {
        return std::make_shared<UpdateCheckResult>("", e, internetIsAlive());
    }
}


void fff::automaticUpdateCheckEval(wxWindow* parent, time_t& lastUpdateCheck, std::string& lastOnlineVersion, const UpdateCheckResult* asyncResult)
{
    assert(runningMainThread());


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

                switch (showQuestionDialog(parent, DialogInfoType::ERROR2, PopupDialogCfg().
                                           setTitle(_("Check for Program Updates")).
                                           setMainInstructions(_("Cannot find current FreeFileSync version number online. A newer version is likely available. Check manually now?")).
                                           setDetailInstructions(result.error->toString()),
                                           _("&Check"), _("&Retry")))
                {
                    case QuestionButton2::YES:
                        wxLaunchDefaultBrowser(L"https://freefilesync.org/get_latest.php");
                        break;
                    case QuestionButton2::NO: //retry
                        automaticUpdateCheckEval(parent, lastUpdateCheck, lastOnlineVersion, asyncResult); //note: retry via recursion!!!
                        break;
                    case QuestionButton2::CANCEL:
                        break;
                }
        }
        //else: ignore this error
    }
}



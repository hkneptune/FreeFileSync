// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "version_check.h"
#include <numeric>
#include <zen/build_info.h>
#include <zen/crc.h>
#include <zen/file_io.h>
#include <zen/http.h>
#include <zen/process_exec.h>
#include <zen/sys_version.h>
#include <zen/sys_info.h>
#include <wx+/image_resources.h>
#include "../ffs_paths.h"
#include "../version/version.h"
#include "small_dlgs.h"

    #include <gtk/gtk.h>
    #include <wx/uilocale.h>


using namespace zen;
using namespace fff;


namespace
{
const Zchar ffsUpdateCheckUserAgent[] = Zstr("FFS-Update-Check");


time_t getVersionCheckCurrentTime()
{
    time_t now = std::time(nullptr);
    return now;
}


void openBrowserForDownload(wxWindow* parent)
{
        wxLaunchDefaultBrowser(L"https://freefilesync.org/get_latest.php");
}
}


bool fff::automaticUpdateCheckDue(time_t lastUpdateCheck)
{
    const time_t now = std::time(nullptr);
    return numeric::dist(now, lastUpdateCheck) >= 7 * 24 * 3600; //check weekly
}


namespace
{
std::wstring getIso639Language()
{
    assert(runningOnMainThread()); //this function is not thread-safe: consider wxWidgets usage

    std::wstring localeName(wxUILocale::GetLanguageCanonicalName(wxUILocale::GetSystemLanguage()));
    localeName = beforeFirst(localeName, L'@', IfNotFoundReturn::all); //the locale may contain an @, e.g. "sr_RS@latin"; see wxUILocale::InitLanguagesDB()

    if (!localeName.empty())
    {
        const std::wstring langCode = beforeFirst(localeName, L'_', IfNotFoundReturn::all);
        assert(langCode.size() == 2 || langCode.size() == 3); //ISO 639: 3-letter possible!
        return langCode;
    }
    assert(false);
    return L"zz";
}


std::wstring getIso3166Country()
{
    assert(runningOnMainThread()); //this function is not thread-safe, consider wxWidgets usage

    std::wstring localeName(wxUILocale::GetLanguageCanonicalName(wxUILocale::GetSystemLanguage()));
    localeName = beforeFirst(localeName, L'@', IfNotFoundReturn::all); //the locale may contain an @, e.g. "sr_RS@latin"; see wxUILocale::InitLanguagesDB()

    if (contains(localeName, L'_'))
    {
        const std::wstring cc = afterFirst(localeName, L'_', IfNotFoundReturn::none);
        assert(cc.size() == 2 || cc.size() == 3); //ISO 3166: 3-letter possible!
        return cc;
    }
    assert(false);
    return L"ZZ";
}


//coordinate with get_latest_version_number.php
std::vector<std::pair<std::string, std::string>> geHttpPostParameters() //throw SysError
{
    assert(runningOnMainThread()); //this function is not thread-safe, e.g. consider wxWidgets usage in getIso639Language()
    std::vector<std::pair<std::string, std::string>> params;

    params.emplace_back("ffs_version", ffsVersion);


    params.emplace_back("os_name", "Linux");

    const OsVersion osv = getOsVersion();
    params.emplace_back("os_version", numberTo<std::string>(osv.major) + "." + numberTo<std::string>(osv.minor));

    const char* osArch = cpuArchName;
    params.emplace_back("os_arch", osArch);

#if GTK_MAJOR_VERSION == 2
    //GetContentScaleFactor() requires GTK3 or later
#elif GTK_MAJOR_VERSION == 3
    params.emplace_back("dip_scale", numberTo<std::string>(wxScreenDC().GetContentScaleFactor()));
#else
#error unknown GTK version!
#endif

    const std::string ffsLang = []
    {
        const wxLanguage lang = getLanguage();

        for (const TranslationInfo& ti : getAvailableTranslations())
            if (ti.languageID == lang)
                return ti.locale;
        return std::string("zz");
    }();
    params.emplace_back("ffs_lang",  ffsLang);

    params.emplace_back("language", utfTo<std::string>(getIso639Language()));
    params.emplace_back("country",  utfTo<std::string>(getIso3166Country()));

    return params;
}




void showUpdateAvailableDialog(wxWindow* parent, const std::string& onlineVersion)
{
    std::wstring updateDetailsMsg;
    try
    {
        updateDetailsMsg = utfTo<std::wstring>(sendHttpGet(utfTo<Zstring>("https://api.freefilesync.org/latest_changes?" + xWwwFormUrlEncode({{"since", ffsVersion}})),
        ffsUpdateCheckUserAgent, Zstring() /*caCertFilePath*/).readAll(nullptr /*notifyUnbufferedIO*/)); //throw SysError
    }
    catch (const SysError& e) { updateDetailsMsg = _("Failed to retrieve update information.") + + L"\n\n" + e.toString(); }


    switch (showConfirmationDialog(parent, DialogInfoType::info, PopupDialogCfg().
                                   setIcon(loadImage("FreeFileSync", dipToScreen(48))).
                                   setTitle(_("Check for Program Updates")).
                                   setMainInstructions(replaceCpy(_("FreeFileSync %x is available!"), L"%x", utfTo<std::wstring>(onlineVersion)) + L"\n\n" + _("Download now?")).
                                   setDetailInstructions(updateDetailsMsg), _("&Download")))
    {
        case ConfirmationButton::accept: //download
            openBrowserForDownload(parent);
            break;
        case ConfirmationButton::cancel:
            break;
    }
}


std::string getOnlineVersion(const std::vector<std::pair<std::string, std::string>>& postParams) //throw SysError
{
    const std::string response = sendHttpPost(Zstr("https://api.freefilesync.org/latest_version"), postParams, nullptr /*notifyUnbufferedIO*/,
                                              ffsUpdateCheckUserAgent, Zstring() /*caCertFilePath*/).readAll(nullptr /*notifyUnbufferedIO*/); //throw SysError

    if (response.empty() ||
    !std::all_of(response.begin(), response.end(), [](char c) { return isDigit(c) || c == FFS_VERSION_SEPARATOR; }) ||
    startsWith(response, FFS_VERSION_SEPARATOR) ||
    endsWith(response, FFS_VERSION_SEPARATOR) ||
    contains(response, std::string() + FFS_VERSION_SEPARATOR + FFS_VERSION_SEPARATOR))
    throw SysError(L"Unexpected server response: \"" +  utfTo<std::wstring>(response) + L'"');
    //response may be "This website has been moved...", or a Javascript challenge: https://freefilesync.org/forum/viewtopic.php?t=8400

    return response;
}


std::string getUnknownVersionTag()
{
    return '<' + utfTo<std::string>(_("version unknown")) + '>';
}
}


bool fff::haveNewerVersionOnline(const std::string& onlineVersion)
{
    auto parseVersion = [](const std::string_view& version)
    {
        std::vector<size_t> output;
        split(version, FFS_VERSION_SEPARATOR,
        [&](const std::string_view digit) { output.push_back(stringTo<size_t>(digit)); });
        assert(!output.empty());
        return output;
    };
    const std::vector<size_t> current = parseVersion(ffsVersion);
    const std::vector<size_t> online  = parseVersion(onlineVersion);

    if (online.empty() || online[0] == 0) //online version string may be "Unknown", see automaticUpdateCheckEval() below!
        return true;

    return online > current; //std::vector compares lexicographically
}


void fff::checkForUpdateNow(wxWindow& parent, std::string& lastOnlineVersion)
{
    try
    {
        const std::string onlineVersion = getOnlineVersion(geHttpPostParameters()); //throw SysError
        lastOnlineVersion = onlineVersion;

        if (haveNewerVersionOnline(onlineVersion))
            showUpdateAvailableDialog(&parent, onlineVersion);
        else
            showNotificationDialog(&parent, DialogInfoType::info, PopupDialogCfg().
                                   setIcon(loadImage("update_check")).
                                   setTitle(_("Check for Program Updates")).
                                   setMainInstructions(_("FreeFileSync is up-to-date.")));
    }
    catch (const SysError& e)
    {
        if (internetIsAlive())
        {
            lastOnlineVersion = getUnknownVersionTag();

            switch (showConfirmationDialog(&parent, DialogInfoType::error, PopupDialogCfg().
                                           setTitle(_("Check for Program Updates")).
                                           setMainInstructions(_("Cannot find current FreeFileSync version number online. A newer version is likely available. Check manually now?")).
                                           setDetailInstructions(e.toString()), _("&Check"), _("&Retry")))
            {
                case ConfirmationButton2::accept:
                    openBrowserForDownload(&parent);
                    break;
                case ConfirmationButton2::accept2: //retry
                    checkForUpdateNow(parent, lastOnlineVersion); //note: retry via recursion!!!
                    break;
                case ConfirmationButton2::cancel:
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
    std::optional<SysError> error;
};
SharedRef<const UpdateCheckResultPrep> fff::automaticUpdateCheckPrepare(wxWindow& parent)
{
    assert(runningOnMainThread());
    auto prep = makeSharedRef<UpdateCheckResultPrep>();
    try
    {
        prep.ref().postParameters = geHttpPostParameters(); //throw SysError
    }
    catch (const SysError& e)
    {
        prep.ref().error = e;
    }
    return prep;
}


struct fff::UpdateCheckResult
{
    std::string onlineVersion;
    std::optional<SysError> error;
    bool internetIsAlive = false;
};
SharedRef<const UpdateCheckResult> fff::automaticUpdateCheckRunAsync(const UpdateCheckResultPrep& resultPrep)
{
    //assert(!runningOnMainThread()); -> allow synchronous call, too
    auto result = makeSharedRef<UpdateCheckResult>();
    try
    {
        if (resultPrep.error)
            throw* resultPrep.error; //throw SysError

        result.ref().onlineVersion = getOnlineVersion(resultPrep.postParameters); //throw SysError
        result.ref().internetIsAlive = true;
    }
    catch (const SysError& e)
    {
        result.ref().error = e;
        result.ref().internetIsAlive = internetIsAlive();
    }
    return result;
}


void fff::automaticUpdateCheckEval(wxWindow& parent, time_t& lastUpdateCheck, std::string& lastOnlineVersion, const UpdateCheckResult& result)
{
    assert(runningOnMainThread());

    if (!result.error)
    {
        lastUpdateCheck = getVersionCheckCurrentTime();

        if (lastOnlineVersion != result.onlineVersion) //show new version popup only *once* per new release
        {
            lastOnlineVersion = result.onlineVersion;

            if (haveNewerVersionOnline(result.onlineVersion)) //beta or development version is newer than online
                showUpdateAvailableDialog(&parent, result.onlineVersion);
        }
    }
    else
    {
        if (result.internetIsAlive)
        {
            if (lastOnlineVersion != getUnknownVersionTag())
                switch (showConfirmationDialog(&parent, DialogInfoType::error, PopupDialogCfg().
                                               setTitle(_("Check for Program Updates")).
                                               setMainInstructions(_("Cannot find current FreeFileSync version number online. A newer version is likely available. Check manually now?")).
                                               setDetailInstructions(result.error->toString()),
                                               _("&Check"), _("&Retry")))
                {
                    case ConfirmationButton2::accept:
                        lastOnlineVersion = getUnknownVersionTag();
                        openBrowserForDownload(&parent);
                        break;
                    case ConfirmationButton2::accept2: //retry
                        automaticUpdateCheckEval(parent, lastUpdateCheck, lastOnlineVersion,
                                                 automaticUpdateCheckRunAsync(automaticUpdateCheckPrepare(parent).ref()).ref()); //retry via recursion!!!
                        break;
                    case ConfirmationButton2::cancel:
                        lastOnlineVersion = getUnknownVersionTag();
                        break;
                }
        }
        else //no internet connection
        {
            if (lastOnlineVersion.empty())
                lastOnlineVersion = ffsVersion;
        }
    }
}

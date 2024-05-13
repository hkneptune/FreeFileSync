// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "sys_info.h"
#include "crc.h"
#include "file_access.h"
#include "sys_version.h"
//#include "time.h"

    #include "symlink_target.h"
    #include "file_io.h"
    #include <ifaddrs.h>
    #include <net/if.h> //IFF_LOOPBACK
    #include <netpacket/packet.h> //sockaddr_ll


    #include "process_exec.h"
    #include <unistd.h> //getuid()
    #include <pwd.h>    //getpwuid_r()

using namespace zen;


Zstring zen::getLoginUser() //throw FileError
{
    auto tryGetNonRootUser = [](const char* varName) -> std::optional<Zstring>
    {
        if (const std::optional<Zstring> username = getEnvironmentVar(varName))
            if (!username->empty() && *username != "root")
                return *username;
        return {};
    };

    if (const uid_t userIdNo = ::getuid(); //never fails
        userIdNo != 0) //nofail; non-root
    {
        //ugh, the world's stupidest API:
        std::vector<char> buf(std::max<long>(10000, ::sysconf(_SC_GETPW_R_SIZE_MAX))); //::sysconf may return long(-1) or even a too small size!! WTF!
        passwd buf2 = {};
        passwd* pwEntry = nullptr;
        if (const int rv = ::getpwuid_r(userIdNo,   //uid_t uid
                                        &buf2,      //struct passwd* pwd
                                        buf.data(), //char* buf
                                        buf.size(), //size_t buflen
                                        &pwEntry);  //struct passwd** result
            rv != 0 || !pwEntry)
        {
            //"If an error occurs, errno is set appropriately" => why the fuck, then, also return errno as return value!?
            errno = rv != 0 ? rv : ENOENT;
            THROW_LAST_FILE_ERROR(_("Cannot get process information."), "getpwuid_r(" + numberTo<std::string>(userIdNo) + ')');
        }

        return pwEntry->pw_name;
    }
    //else: root(0) => consider as request for elevation, NOT impersonation!

    //getlogin() is smarter than simply evaluating $LOGNAME! even in contexts without
    //$LOGNAME, e.g. "sudo su" on Ubuntu, it returns the correct non-root user!
    if (const char* loginUser = ::getlogin()) //https://linux.die.net/man/3/getlogin
        if (strLength(loginUser) > 0 && !equalString(loginUser, "root"))
            return loginUser;
    //BUT: getlogin() can fail with ENOENT on Linux Mint: https://freefilesync.org/forum/viewtopic.php?t=8181

    //getting a little desperate: variables used by installer.sh
    if (const std::optional<Zstring> username = tryGetNonRootUser("USER"))      return *username;
    if (const std::optional<Zstring> username = tryGetNonRootUser("SUDO_USER")) return *username;
    if (const std::optional<Zstring> username = tryGetNonRootUser("LOGNAME"))   return *username;


    //apparently the current user really IS root: https://freefilesync.org/forum/viewtopic.php?t=8405
    assert(getuid() == 0);
    return "root";
}


Zstring zen::getUserDescription() //throw FileError
{
    const Zstring username     = getLoginUser(); //throw FileError
    const Zstring computerName = []() -> Zstring //throw FileError
    {
        std::vector<char> buf(10000);
        if (::gethostname(buf.data(), buf.size()) != 0)
            THROW_LAST_FILE_ERROR(_("Cannot get process information."), "gethostname");

        Zstring hostName = buf.data();
        if (endsWithAsciiNoCase(hostName, ".local")) //strip fluff (macOS) => apparently not added on Linux?
            hostName = beforeLast(hostName, '.', IfNotFoundReturn::none);

        return hostName;
    }();

    if (contains(getUpperCase(computerName), getUpperCase(username)))
        return username; //no need for text duplication! e.g. "Zenju (Zenju-PC)"

    return username + Zstr(" (") + computerName + Zstr(')'); //e.g. "Admin (Zenju-PC)"
}


namespace
{
}


ComputerModel zen::getComputerModel() //throw FileError
{
    ComputerModel cm;
    try
    {
        auto tryGetInfo = [](const Zstring& filePath)
        {
            try
            {
                const std::string stream = getFileContent(filePath, nullptr /*notifyUnbufferedIO*/); //throw FileError
                return utfTo<std::wstring>(trimCpy(stream));
            }
            catch (FileError&)
            {
                if (!itemExists(filePath)) //throw FileError
                    return std::wstring();

                throw;
            }
        };
        cm.model  = tryGetInfo("/sys/devices/virtual/dmi/id/product_name"); //throw FileError
        cm.vendor = tryGetInfo("/sys/devices/virtual/dmi/id/sys_vendor");   //

        //clean up:
        cm.model  = beforeFirst(cm.model,  L'\u00ff', IfNotFoundReturn::all); //fix broken BIOS entries:
        cm.vendor = beforeFirst(cm.vendor, L'\u00ff', IfNotFoundReturn::all); //0xff can be considered 0

        trim(cm.model,  TrimSide::right, [](wchar_t c) { return c == L'_'; }); //e.g. "CBX3___" or just "_"
        trim(cm.vendor, TrimSide::right, [](wchar_t c) { return c == L'_'; }); //e.g. "DELL__"  or just "_"

        for (const char* dummyModel :
             {
                 "Please change product name",
                 "SYSTEM_PRODUCT_NAME",
                 "System Product Name",
                 "To Be Filled By O.E.M.",
                 "Default string",
                 "$(DEFAULT_STRING)",
                 "<null string>",
                 "Product Name",
                 "Undefined",
                 "INVALID",
                 "Unknow",
                 "empty",
                 "O.E.M.",
                 "O.E.M",
                 "OEM",
                 "NA",
                 ".",
             })
            if (equalAsciiNoCase(cm.model, dummyModel))
            {
                cm.model.clear();
                break;
            }

        for (const char* dummyVendor :
             {
                 "OEM Manufacturer",
                 "SYSTEM_MANUFACTURER",
                 "System manufacturer",
                 "System Manufacter",
                 "To Be Filled By O.E.M.",
                 "Default string",
                 "$(DEFAULT_STRING)",
                 "Undefined",
                 "Unknow",
                 "empty",
                 "O.E.M.",
                 "O.E.M",
                 "OEM",
                 "NA",
                 ".",
             })
            if (equalAsciiNoCase(cm.vendor, dummyVendor))
            {
                cm.vendor.clear();
                break;
            }

        return cm;
    }
    catch (const SysError& e) { throw FileError(_("Cannot get process information."), e.toString()); }
}





std::wstring zen::getOsDescription() //throw FileError
{
    try
    {
        const OsVersionDetail verDetail = getOsVersionDetail(); //throw SysError
        return trimCpy(verDetail.osName + L' ' + verDetail.osVersionRaw); //e.g. "CentOS 7.8.2003"

    }
    catch (const SysError& e) { throw FileError(_("Cannot get process information."), e.toString()); }
}




Zstring zen::getProcessPath() //throw FileError
{
    try
    {
        return getSymlinkRawContent_impl("/proc/self/exe").targetPath; //throw SysError
        //path does not contain symlinks => no need for ::realpath()

    }
    catch (const SysError& e) { throw FileError(_("Cannot get process information."), e.toString()); }
}


Zstring zen::getUserHome() //throw FileError
{
    if (::getuid() != 0) //nofail; non-root
        /*   https://linux.die.net/man/3/getpwuid: An application that wants to determine its user's home directory
           should inspect the value of HOME (rather than the value getpwuid(getuid())->pw_dir) since this allows
           the user to modify their notion of "the home directory" during a login session.                       */
        if (const std::optional<Zstring> homeDirPath = getEnvironmentVar("HOME"))
            return *homeDirPath;

    //root(0) => consider as request for elevation, NOT impersonation!
    //=> "HOME=/root" :(

    const Zstring loginUser = getLoginUser(); //throw FileError

    //ugh, the world's stupidest API:
    std::vector<char> buf(std::max<long>(10000, ::sysconf(_SC_GETPW_R_SIZE_MAX))); //::sysconf may return long(-1) or even a too small size!! WTF!
    passwd buf2 = {};
    passwd* pwEntry = nullptr;
    if (const int rv = ::getpwnam_r(loginUser.c_str(), //const char *name
                                    &buf2,             //struct passwd* pwd
                                    buf.data(),        //char* buf
                                    buf.size(),        //size_t buflen
                                    &pwEntry);         //struct passwd** result
        rv != 0 || !pwEntry)
    {
        //"If an error occurs, errno is set appropriately" => why the fuck, then also return errno as return value!?
        errno = rv != 0 ? rv : ENOENT;
        THROW_LAST_FILE_ERROR(_("Cannot get process information."), "getpwnam_r(" + utfTo<std::string>(loginUser) + ')');
    }

    return pwEntry->pw_dir; //home directory
}


Zstring zen::getUserDataPath() //throw FileError
{
    if (::getuid() != 0) //nofail; non-root
        if (const std::optional<Zstring> xdgCfgPath = getEnvironmentVar("XDG_CONFIG_HOME");
            xdgCfgPath&& !xdgCfgPath->empty())
            return *xdgCfgPath;
    //root(0) => consider as request for elevation, NOT impersonation

    return appendPath(getUserHome(), ".config"); //throw FileError
}


Zstring zen::getUserDownloadsPath() //throw FileError
{
    try
    {
        if (::getuid() != 0) //nofail; non-root
            if (const auto& [exitCode, output] = consoleExecute("xdg-user-dir DOWNLOAD", std::nullopt /*timeoutMs*/); //throw SysError
                exitCode == 0)
            {
                const Zstring& downloadsPath = trimCpy(output);
                ASSERT_SYSERROR(!downloadsPath.empty());
                return downloadsPath;
            }
        //root(0) => consider as request for elevation, NOT impersonation

        //fallback: probably correct 99.9% of the time anyway...
        return appendPath(getUserHome(), "Downloads"); //throw FileError
    }
    catch (const SysError& e) { throw FileError(_("Cannot get process information."), e.toString()); }
}


bool zen::runningElevated() //throw FileError
{
    if (::geteuid() != 0) //nofail; non-root
        return false;

    return getLoginUser() != "root"; //throw FileError
    //consider "root login" like "UAC disabled" on Windows
}

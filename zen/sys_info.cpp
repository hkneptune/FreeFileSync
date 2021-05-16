// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "sys_info.h"
#include "crc.h"
#include "file_access.h"
#include "sys_version.h"
#include "symlink_target.h"
#include "time.h"

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
    const uid_t userIdNo = ::getuid(); //never fails

    if (userIdNo != 0) //nofail; root(0) => consider as request for elevation, NOT impersonation
    {
        std::vector<char> buf(std::max<long>(10000, ::sysconf(_SC_GETPW_R_SIZE_MAX))); //::sysconf may return long(-1)
        passwd buf2 = {};
        passwd* pwsEntry = nullptr;
        if (::getpwuid_r(userIdNo,        //uid_t uid
                         &buf2,           //struct passwd* pwd
                         &buf[0],         //char* buf
                         buf.size(),      //size_t buflen
                         &pwsEntry) != 0) //struct passwd** result
            THROW_LAST_FILE_ERROR(_("Cannot get process information."), "getpwuid_r");

        if (!pwsEntry)
            throw FileError(_("Cannot get process information."), L"no login found"); //should not happen?

        return pwsEntry->pw_name;
    }
    //else root(0): what now!?

    //getlogin() is smarter than simply evaluating $LOGNAME! even in contexts without
    //$LOGNAME, e.g. "sudo su" on Ubuntu, it returns the correct non-root user!
    if (const char* loginUser = ::getlogin()) //https://linux.die.net/man/3/getlogin
        if (strLength(loginUser) > 0 && !equalString(loginUser, "root"))
            return loginUser;
    //BUT: getlogin() can fail with ENOENT on Linux Mint: https://freefilesync.org/forum/viewtopic.php?t=8181

    auto tryGetNonRootUser = [](const char* varName) -> const char*
    {
        if (const char* buf = ::getenv(varName)) //no extended error reporting
            if (strLength(buf) > 0 && !equalString(buf, "root"))
                return buf;
        return nullptr;
    };
    //getting a little desperate: variables used by installer.sh
    if (const char* userName = tryGetNonRootUser("USER"))      return userName;
    if (const char* userName = tryGetNonRootUser("SUDO_USER")) return userName;
    if (const char* userName = tryGetNonRootUser("LOGNAME"))   return userName;

    //apparently the current user really IS root: https://freefilesync.org/forum/viewtopic.php?t=8405
    return "root";

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
            if (!fileAvailable(filePath))
                return std::wstring();
            try
            {
                const std::string stream = getFileContent(filePath, nullptr /*notifyUnbufferedIO*/); //throw FileError
                return utfTo<std::wstring>(trimCpy(stream));
            }
            catch (const FileError& e) { throw SysError(replaceCpy(e.toString(), L"\n\n", L'\n')); } //errors should be further enriched by context info => SysError
        };
        cm.model  = tryGetInfo("/sys/devices/virtual/dmi/id/product_name"); //throw SysError
        cm.vendor = tryGetInfo("/sys/devices/virtual/dmi/id/sys_vendor");   //

        //clean up:
        cm.model  = beforeFirst(cm.model,  L'\u00ff', IfNotFoundReturn::all); //fix broken BIOS entries:
        cm.vendor = beforeFirst(cm.vendor, L'\u00ff', IfNotFoundReturn::all); //0xff can be considered 0

        for (const char* dummyModel :
             {
                 "To Be Filled By O.E.M.", "Default string", "$(DEFAULT_STRING)", "Undefined", "empty", "O.E.M", "OEM", "NA",
                 "System Product Name", "Please change product name", "INVALID",
             })
            if (equalAsciiNoCase(cm.model, dummyModel))
            {
                cm.model.clear();
                break;
            }

        for (const char* dummyVendor :
             {
                 "To Be Filled By O.E.M.", "Default string", "$(DEFAULT_STRING)", "Undefined",  "empty", "O.E.M", "OEM", "NA",
                 "System manufacturer", "OEM Manufacturer",
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




Zstring zen::getRealProcessPath() //throw FileError
{
    return getSymlinkRawContent("/proc/self/exe").targetPath; //throw FileError
    //path does not contain symlinks => no need for ::realpath()

}


namespace
{
Zstring getUserDir() //throw FileError
{
    const Zstring loginUser = getLoginUser(); //throw FileError
    if (loginUser == "root")
        return "/root";
    else
        return "/home/" + loginUser;
}
}


Zstring zen::getUserDataPath() //throw FileError
{
    if (::getuid() != 0) //nofail; root(0) => consider as request for elevation, NOT impersonation
        if (const char* xdgCfgPath = ::getenv("XDG_CONFIG_HOME"); //no extended error reporting
            xdgCfgPath && xdgCfgPath[0] != 0)
            return xdgCfgPath;

    return getUserDir() + "/.config"; //throw FileError
}


Zstring zen::getUserDownloadsPath() //throw FileError
{
    try
    {
        if (::getuid() != 0) //nofail; root(0) => consider as request for elevation, NOT impersonation
            if (const auto& [exitCode, output] = consoleExecute("xdg-user-dir DOWNLOAD", std::nullopt /*timeoutMs*/); //throw SysError
                exitCode == 0)
            {
                const Zstring& downloadsPath = trimCpy(output);
                ASSERT_SYSERROR(!downloadsPath.empty());
                return downloadsPath;
            }

        //fallback: probably correct 99.9% of the time anyway...
        return getUserDir() + "/Downloads"; //throw FileError
    }
    catch (const SysError& e) { throw FileError(_("Cannot get process information."), e.toString()); }
}

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

    #include "file_io.h"
    #include <ifaddrs.h>
    #include <net/if.h> //IFF_LOOPBACK
    #include <netpacket/packet.h> //sockaddr_ll


    #include "process_exec.h"
    #include <unistd.h> //getuid()
    #include <pwd.h>    //getpwuid_r()

using namespace zen;


std::wstring zen::getUserName() //throw FileError
{
    //https://linux.die.net/man/3/getlogin
    //https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/getlogin.2.html
    const char* loginUser = ::getlogin();
    if (!loginUser)
        THROW_LAST_FILE_ERROR(_("Cannot get process information."), "getlogin");
    //getlogin() is smarter than simply evaluating $LOGNAME! even in contexts without
    //$LOGNAME, e.g. "sudo su" on Ubuntu, it returns the correct non-root user!
    return utfTo<std::wstring>(loginUser);
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


Zstring zen::getUserDataPath() //throw FileError
{
    if (::getuid() != 0) //nofail; root(0) => consider as request for elevation, NOT impersonation
        if (const char* xdgCfgPath = ::getenv("XDG_CONFIG_HOME"); //no extended error reporting
            xdgCfgPath && xdgCfgPath[0] != 0)
            return xdgCfgPath;

    return Zstring("/home/") + utfTo<Zstring>(getUserName()) + "/.config"; //throw FileError
}


Zstring zen::getUserDownloadsPath() //throw FileError
{
    try
    {
        const Zstring cmdLine = ::getuid() == 0 ? //nofail; root(0) => consider as request for elevation, NOT impersonation
                                //sudo better be installed :>
                                "sudo -u " + utfTo<Zstring>(getUserName()) + " xdg-user-dir DOWNLOAD" : //throw FileError
                                "xdg-user-dir DOWNLOAD";

        const auto& [exitCode, output] = consoleExecute(cmdLine, std::nullopt /*timeoutMs*/); //throw SysError
        if (exitCode != 0)
            throw SysError(formatSystemError(cmdLine.c_str(),
                                             replaceCpy(_("Exit code %x"), L"%x", numberTo<std::wstring>(exitCode)), utfTo<std::wstring>(output)));
        const Zstring& downloadsPath = trimCpy(output);
        ASSERT_SYSERROR(!downloadsPath.empty());
        return downloadsPath;
    }
    catch (const SysError& e) { throw FileError(_("Cannot get process information."), e.toString()); }
}

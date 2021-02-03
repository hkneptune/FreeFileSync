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
    const uid_t userIdNo = ::getuid(); //"real user ID"; never fails

    std::vector<char> buffer(std::max<long>(10000, ::sysconf(_SC_GETPW_R_SIZE_MAX))); //::sysconf may return long(-1)
    struct passwd buffer2 = {};
    struct passwd* pwsEntry = nullptr;
    if (::getpwuid_r(userIdNo, &buffer2, &buffer[0], buffer.size(), &pwsEntry) != 0) //getlogin() is deprecated and not working on Ubuntu at all!!!
        THROW_LAST_FILE_ERROR(_("Cannot get process information."), "getpwuid_r");
    if (!pwsEntry)
        throw FileError(_("Cannot get process information."), L"no login found"); //should not happen?

    return utfTo<std::wstring>(pwsEntry->pw_name);
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
        cm.model  = beforeFirst(cm.model, L'\u00ff', IfNotFoundReturn::all);  //fix broken BIOS entries:
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


Zstring zen::getUserDownloadsPath() //throw FileError
{
    try
    {
        Zstring cmdLine;
        if (getuid() == 0) //nofail; root(0) => consider as request for elevation, NOT impersonation
        {
            const char* loginUser = getlogin(); //https://linux.die.net/man/3/getlogin
            if (!loginUser)
                THROW_LAST_SYS_ERROR("getlogin");

            cmdLine = Zstring("sudo -u ") + loginUser + " xdg-user-dir DOWNLOAD"; //sudo better be installed :>
        }
        else
            cmdLine = "xdg-user-dir DOWNLOAD";

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


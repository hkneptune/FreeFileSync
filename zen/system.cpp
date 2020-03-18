// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "system.h"
#include "file_access.h"
#include "crc.h"

    #include "symlink_target.h"
    #include "file_io.h"
    #include <ifaddrs.h>
    #include <net/if.h> //IFF_LOOPBACK
    #include <netpacket/packet.h> //sockaddr_ll

    #include <unistd.h> //getuid()
    #include <pwd.h>    //getpwuid_r()
    #include "shell_execute.h"

using namespace zen;


std::wstring zen::getUserName() //throw FileError
{
    const uid_t userIdNo = ::getuid(); //never fails

    std::vector<char> buffer(std::max<long>(10000, ::sysconf(_SC_GETPW_R_SIZE_MAX))); //::sysconf may return long(-1)
    struct passwd buffer2 = {};
    struct passwd* pwsEntry = nullptr;
    if (::getpwuid_r(userIdNo, &buffer2, &buffer[0], buffer.size(), &pwsEntry) != 0) //getlogin() is deprecated and not working on Ubuntu at all!!!
        THROW_LAST_FILE_ERROR(_("Cannot get process information."), L"getpwuid_r");
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
                const std::string stream = loadBinContainer<std::string>(filePath, nullptr /*notifyUnbufferedIO*/); //throw FileError
                return utfTo<std::wstring>(trimCpy(stream));
            }
            catch (const FileError& e) { throw SysError(e.toString()); } //errors should be further enriched by context info => SysError
        };
        cm.model  = tryGetInfo("/sys/devices/virtual/dmi/id/product_name"); //throw SysError
        cm.vendor = tryGetInfo("/sys/devices/virtual/dmi/id/sys_vendor");   //

        //clean up:
        for (const char* dummyModel :
             {
                 "To Be Filled By O.E.M.", "Default string", "empty", "O.E.M", "OEM", "NA",
                 "System Product Name", "Please change product name", "INVALID",
             })
            if (equalAsciiNoCase(cm.model, dummyModel))
            {
                cm.model.clear();
                break;
            }

        for (const char* dummyVendor :
             {
                 "To Be Filled By O.E.M.", "Default string", "empty", "O.E.M", "OEM", "NA",
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
        const std::string osName    = trimCpy(getCommandOutput("lsb_release --id -s"     )); //throw SysError
        const std::string osVersion = trimCpy(getCommandOutput("lsb_release --release -s")); //
        return utfTo<std::wstring>(osName + ' ' + osVersion); //e.g. "CentOS 7.7.1908"

    }
    catch (const SysError& e) { throw FileError(_("Cannot get process information."), e.toString()); }
}





// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "sys_version.h"
    #include <iostream>
    #include "file_io.h"
    #include "shell_execute.h"

using namespace zen;


OsVersionDetail zen::getOsVersionDetail() //throw SysError
{
    /* prefer lsb_release:             lsb_release      Distributor ID: Debian
         1. terser OS name                              Release:        8.11
         2. detailed version number
                                       /etc/os-release  NAME="Debian GNU/Linux"
                                                        VERSION_ID="8"                         */
    std::wstring osName;
    std::wstring osVersion;
    try
    {
        if (const auto [exitCode, output] = consoleExecute("lsb_release --id -s", std::nullopt); //throw SysError
            exitCode != 0)
            throw SysError(formatSystemError("lsb_release --id", replaceCpy(_("Exit code %x"), L"%x", numberTo<std::wstring>(exitCode)), output));
        else
            osName = trimCpy(output);

        if (const auto [exitCode, output] = consoleExecute("lsb_release --release -s", std::nullopt); //throw SysError
            exitCode != 0)
            throw SysError(formatSystemError("lsb_release --release", replaceCpy(_("Exit code %x"), L"%x", numberTo<std::wstring>(exitCode)), output));
        else
            osVersion = trimCpy(output);
    }
    //lsb_release not available on some systems: https://freefilesync.org/forum/viewtopic.php?t=7191
    catch (SysError&) // => fall back to /etc/os-release: https://www.freedesktop.org/software/systemd/man/os-release.html
    {
        std::string releaseInfo;
        try
        {
            releaseInfo = getFileContent("/etc/os-release", nullptr /*notifyUnbufferedIO*/); //throw FileError
        }
        catch (const FileError& e) { throw SysError(replaceCpy(e.toString(), L"\n\n", L'\n')); } //errors should be further enriched by context info => SysError

        for (const std::string& line : split(releaseInfo, '\n', SplitOnEmpty::skip))
            if (startsWith(line, "NAME="))
                osName = utfTo<std::wstring>(afterFirst(line, '=', IfNotFoundReturn::none));
            else if (startsWith(line, "VERSION_ID="))
                osVersion = utfTo<std::wstring>(afterFirst(line, '=', IfNotFoundReturn::none));
        //PRETTY_NAME? too wordy! e.g. "Fedora 17 (Beefy Miracle)"

        trim(osName,    true, true, [](char c) { return c == L'"' || c == L'\''; });
        trim(osVersion, true, true, [](char c) { return c == L'"' || c == L'\''; });
    }

    if (osName.empty())
        throw SysError(L"Operating system release could not be determined."); //should never happen!
    //osVersion is usually available, except for Arch Linux: https://freefilesync.org/forum/viewtopic.php?t=7276
    //  lsb_release Release is "rolling"
    //  etc/os-release: VERSION_ID is missing

    std::vector<std::wstring> verDigits = split<std::wstring>(osVersion, L'.', SplitOnEmpty::allow); //e.g. "7.7.1908"
    verDigits.resize(2);

    return OsVersionDetail
    {
        {
            stringTo<int>(verDigits[0]),
            stringTo<int>(verDigits[1])
        },
        osVersion, osName
    };
}


OsVersion zen::getOsVersion()
{
    try
    {
        static const OsVersionDetail verDetail = getOsVersionDetail(); //throw SysError
        return verDetail.version;
    }
    catch (const SysError& e)
    {
        std::cerr << utfTo<std::string>(e.toString()) << '\n';
        return {}; //sigh, it's a jungle out there: https://freefilesync.org/forum/viewtopic.php?t=7276
    }
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STATUS_HANDLER_IMPL_H_145234543248059083415565
#define STATUS_HANDLER_IMPL_H_145234543248059083415565

#include <chrono>
#include <thread>
#include <zen/zstring.h>
#include <zen/i18n.h>


namespace fff
{
namespace
{
void delayAndCountDown(std::chrono::steady_clock::time_point delayUntil, const std::function<void(const std::wstring& timeRemMsg)>& notifyStatus)
{
    for (auto now = std::chrono::steady_clock::now(); now < delayUntil; now = std::chrono::steady_clock::now())
    {
        if (notifyStatus)
        {
            const auto timeRemMs = std::chrono::duration_cast<std::chrono::milliseconds>(delayUntil - now).count();
            notifyStatus(_P("1 sec", "%x sec", numeric::integerDivideRoundUp(timeRemMs, 1000)));
        }

        std::this_thread::sleep_for(UI_UPDATE_INTERVAL / 2);
    }
}


void runCommandAndLogErrors(const Zstring& cmdLine, zen::ErrorLog& errorLog)
{
    using namespace zen;

    try
    {
        //give consoleExecute() some "time to fail", but not too long to hang our process
        const int DEFAULT_APP_TIMEOUT_MS = 100;

        if (const auto& [exitCode, output] = consoleExecute(cmdLine, DEFAULT_APP_TIMEOUT_MS); //throw SysError, SysErrorTimeOut
            exitCode != 0)
            throw SysError(formatSystemError("", replaceCpy(_("Exit code %x"), L"%x", numberTo<std::wstring>(exitCode)), output));

        errorLog.logMsg(_("Executing command:") + L' ' + utfTo<std::wstring>(cmdLine) + L" [" + replaceCpy(_("Exit code %x"), L"%x", L"0") + L']', MSG_TYPE_INFO);
    }
    catch (SysErrorTimeOut&) //child process not failed yet => probably fine :>
    {
        errorLog.logMsg(_("Executing command:") + L' ' + utfTo<std::wstring>(cmdLine), MSG_TYPE_INFO);
    }
    catch (const SysError& e)
    {
        errorLog.logMsg(replaceCpy(_("Command %x failed."), L"%x", fmtPath(cmdLine)) + L"\n\n" + e.toString(), MSG_TYPE_ERROR);
    }
}
}
}

#endif //STATUS_HANDLER_IMPL_H_145234543248059083415565

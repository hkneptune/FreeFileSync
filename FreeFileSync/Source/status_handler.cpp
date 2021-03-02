// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "status_handler.h"
#include <zen/basic_math.h>
#include <zen/process_exec.h>

using namespace zen;


namespace
{
std::chrono::steady_clock::time_point lastExec;
}


bool fff::uiUpdateDue(bool force)
{
    const auto now = std::chrono::steady_clock::now();

    if (now >= lastExec + UI_UPDATE_INTERVAL || force)
    {
        lastExec = now;
        return true;
    }
    return false;
}


void fff::runCommandAndLogErrors(const Zstring& cmdLine, ErrorLog& errorLog)
{
    try
    {
        //give consoleExecute() some "time to fail", but not too long to hang our process
        const int DEFAULT_APP_TIMEOUT_MS = 100;

        if (const auto& [exitCode, output] = consoleExecute(cmdLine, DEFAULT_APP_TIMEOUT_MS); //throw SysError, SysErrorTimeOut
            exitCode != 0)
            throw SysError(formatSystemError("", replaceCpy(_("Exit code %x"), L"%x", numberTo<std::wstring>(exitCode)), utfTo<std::wstring>(output)));

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


void fff::delayAndCountDown(std::chrono::steady_clock::time_point delayUntil, const std::function<void(const std::wstring& timeRemMsg)>& notifyStatus)
{
    for (auto now = std::chrono::steady_clock::now(); now < delayUntil; now = std::chrono::steady_clock::now())
    {
        if (notifyStatus)
        {
            const auto timeRemMs = std::chrono::duration_cast<std::chrono::milliseconds>(delayUntil - now).count();
            notifyStatus(_P("1 sec", "%x sec", numeric::intDivCeil(timeRemMs, 1000)));
        }

        std::this_thread::sleep_for(UI_UPDATE_INTERVAL / 2);
    }
}

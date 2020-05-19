// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "shutdown.h"
    #include <zen/shell_execute.h>


using namespace zen;




void zen::shutdownSystem() //throw FileError
{
    try
    {
        //https://linux.die.net/man/2/reboot => needs admin rights!
        //"systemctl" should work without admin rights:
        const auto& [exitCode, output] = consoleExecute("systemctl poweroff", std::nullopt /*timeoutMs*/); //throw SysError, (SysErrorTimeOut)
        if (!trimCpy(output).empty()) //see comment in suspendSystem()
            throw SysError(output);

    }
    catch (const SysError& e) { throw FileError(_("Unable to shut down the system."), e.toString()); }
}


void zen::suspendSystem() //throw FileError
{
    try
    {
        //"systemctl" should work without admin rights:
        const auto& [exitCode, output] = consoleExecute("systemctl suspend", std::nullopt /*timeoutMs*/); //throw SysError, (SysErrorTimeOut)
        //why does "systemctl suspend" return exit code 1 despite apparent success!??
        if (!trimCpy(output).empty()) //at least we can assume "no output" on success
            throw SysError(output);

    }
    catch (const SysError& e) { throw FileError(_("Unable to shut down the system."), e.toString()); }
}


void zen::terminateProcess(int exitCode)
{
    std::quick_exit(exitCode); //[[noreturn]]; "Causes normal program termination to occur without completely cleaning the resources." => perfect


    for (;;) //why still here?? => crash deliberately!
        *reinterpret_cast<volatile int*>(0) = 0; //crude but at least we'll get crash dumps if it ever happens
}


//Command line alternatives:
    //Shut down:  systemctl poweroff      //alternative requiring admin: sudo shutdown -h 1
    //Sleep:      systemctl suspend       //alternative requiring admin: sudo pm-suspend
    //Log off:    gnome-session-quit --no-prompt
    //      alternative requiring admin: sudo killall Xorg
    //      alternative without admin: dbus-send --session --print-reply --dest=org.gnome.SessionManager /org/gnome/SessionManager org.gnome.SessionManager.Logout uint32:1


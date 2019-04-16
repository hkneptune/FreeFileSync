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
    //https://linux.die.net/man/2/reboot => needs admin rights!

    //"systemctl" should work without admin rights:
    shellExecute("systemctl poweroff", ExecutionType::SYNC); //throw FileError

}


void zen::suspendSystem() //throw FileError
{
    //"systemctl" should work without admin rights:
    shellExecute("systemctl suspend", ExecutionType::SYNC); //throw FileError

}


void zen::terminateProcess(int exitCode)
{
    std::exit(exitCode); //[[noreturn]]; "Stack is not unwound: destructors of variables with automatic storage duration are not called." => perfect
    //don't use std::abort() => crashes process with "EXC_CRASH (SIGABRT)" on macOS
    for (;;) //why still here?? => crash deliberately!
        *reinterpret_cast<volatile int*>(0) = 0; //crude but at least we'll get crash dumps if it ever happens
}


//Command line alternatives:
    //Shut down:  systemctl poweroff      //alternative requiring admin: sudo shutdown -h 1
    //Sleep:      systemctl suspend       //alternative requiring admin: sudo pm-suspend
    //Log off:    gnome-session-quit --no-prompt
    //      alternative requiring admin: sudo killall Xorg
    //      alternative without admin: dbus-send --session --print-reply --dest=org.gnome.SessionManager /org/gnome/SessionManager org.gnome.SessionManager.Logout uint32:1


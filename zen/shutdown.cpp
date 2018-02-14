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
    shellExecute("sleep 1; systemctl poweroff", ExecutionType::ASYNC); //throw FileError
    //sleep 1: give FFS some time to properly shut down!

}


void zen::suspendSystem() //throw FileError
{
    //"systemctl" should work without admin rights:
    shellExecute("systemctl suspend", ExecutionType::ASYNC); //throw FileError

}

/*
Command line alternatives:

#ifdef ZEN_WIN
#ifdef ZEN_WIN_VISTA_AND_LATER
    Shut down:  shutdown /s /t 60
    Sleep:      rundll32.exe powrprof.dll,SetSuspendState Sleep
    Log off:    shutdown /l
#else //XP
    Shut down:  shutdown -s -t 60
    Standby:    rundll32.exe powrprof.dll,SetSuspendState   //this triggers standby OR hibernate, depending on whether hibernate setting is active! no suspend on XP?
    Log off:    shutdown -l
#endif

#elif defined ZEN_LINUX
    Shut down:  systemctl poweroff      //alternative requiring admin: sudo shutdown -h 1
    Sleep:      systemctl suspend       //alternative requiring admin: sudo pm-suspend
    Log off:    gnome-session-quit --no-prompt
        //alternative requiring admin: sudo killall Xorg
        //alternative without admin: dbus-send --session --print-reply --dest=org.gnome.SessionManager /org/gnome/SessionManager org.gnome.SessionManager.Logout uint32:1

#elif defined ZEN_MAC
    Shut down:  osascript -e 'tell application "System Events" to shut down'
    Sleep:      osascript -e 'tell application "System Events" to sleep'
    Log off:    osascript -e 'tell application "System Events" to log out'
#endif
*/

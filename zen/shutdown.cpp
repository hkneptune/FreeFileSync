// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "shutdown.h"
#include "thread.h"
    #include <zen/process_exec.h>


using namespace zen;




void zen::shutdownSystem() //throw FileError
{
    assert(runningOnMainThread());
    if (runningOnMainThread())
        onSystemShutdownRunTasks();
    try
    {
        //https://linux.die.net/man/2/reboot => needs admin rights!
        //"systemctl" should work without admin rights:
        auto [exitCode, output] = consoleExecute("systemctl poweroff", std::nullopt /*timeoutMs*/); //throw SysError, (SysErrorTimeOut)
        trim(output);
        if (!output.empty()) //see comment in suspendSystem()
            throw SysError(utfTo<std::wstring>(output));

    }
    catch (const SysError& e) { throw FileError(_("Unable to shut down the system."), e.toString()); }
}


void zen::suspendSystem() //throw FileError
{
    try
    {
        //"systemctl" should work without admin rights:
        auto [exitCode, output] = consoleExecute("systemctl suspend", std::nullopt /*timeoutMs*/); //throw SysError, (SysErrorTimeOut)
        trim(output);
        //why does "systemctl suspend" return exit code 1 despite apparent success!??
        if (!output.empty()) //at least we can assume "no output" on success
            throw SysError(utfTo<std::wstring>(output));

    }
    catch (const SysError& e) { throw FileError(_("Unable to shut down the system."), e.toString()); }
}


void zen::terminateProcess(int exitCode)
{
    std::quick_exit(exitCode); //[[noreturn]]; "Causes normal program termination to occur without completely cleaning the resources." => perfect


    for (;;) //why still here?? => crash deliberately!
        *reinterpret_cast<volatile int*>(0) = 0; //crude but at least we'll get crash dumps *if* it ever happens
}


//Command line alternatives:
    //Shut down:  systemctl poweroff      //alternative requiring admin: sudo shutdown -h 1
    //Sleep:      systemctl suspend       //alternative requiring admin: sudo pm-suspend
    //Log off:    gnome-session-quit --no-prompt
    //      alternative requiring admin: sudo killall Xorg
    //      alternative without admin: dbus-send --session --print-reply --dest=org.gnome.SessionManager /org/gnome/SessionManager org.gnome.SessionManager.Logout uint32:1



namespace
{
using ShutdownTaskList = std::vector<std::weak_ptr<const std::function<void()>>>;
constinit Global<ShutdownTaskList> globalShutdownTasks;
GLOBAL_RUN_ONCE(globalShutdownTasks.set(std::make_unique<ShutdownTaskList>()));
}


void zen::onSystemShutdownRegister(const SharedRef<std::function<void()>>& task)
{
    assert(runningOnMainThread());

    const auto& tasks = globalShutdownTasks.get();
    assert(tasks);
    if (tasks)
        tasks->push_back(task.ptr());
}


void zen::onSystemShutdownRunTasks()
{
    assert(runningOnMainThread()); //no multithreading! else: after taskWeak.lock() task() references may go out of scope! (e.g. "this")

    const auto& tasks = globalShutdownTasks.get();
    assert(tasks);
    if (tasks)
        for (const std::weak_ptr<const std::function<void()>>& taskWeak : *tasks)
            if (const std::shared_ptr<const std::function<void()>>& task = taskWeak.lock();
                task)
                try
                { (*task)(); }
                catch (...) { assert(false); }

    globalShutdownTasks.set(nullptr); //trigger assert in onSystemShutdownRegister(), just in case...
}

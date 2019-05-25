// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "thread.h"
    #include <sys/prctl.h>
    #include <unistd.h>
    #include <sys/syscall.h>

using namespace zen;




void zen::setCurrentThreadName(const char* threadName)
{
    ::prctl(PR_SET_NAME, threadName, 0, 0, 0);

}


namespace
{
uint64_t getThreadIdNative()
{
    const pid_t tid = ::syscall(SYS_gettid); //no-fail
    //"Invalid thread and process IDs": https://devblogs.microsoft.com/oldnewthing/20040223-00/?p=40503
    //if (tid == 0) -> not sure this holds on Linux, too!
    //    throw std::runtime_error(std::string(__FILE__) + "[" + numberTo<std::string>(__LINE__) + "] Failed to get thread ID.");
    static_assert(sizeof(uint64_t) >= sizeof(tid));
    return tid;
}


const uint64_t globalMainThreadId = getThreadId(); //avoid code-gen for "magic static"!
}


uint64_t zen::getThreadId()
{
    thread_local const uint64_t tid = getThreadIdNative(); //buffer to get predictable perf characteristics
    return tid;
}


uint64_t zen::getMainThreadId()
{
    //don't make this a function-scope static (avoid code-gen for "magic static")
    if (globalMainThreadId == 0) //might be called during static initialization
        return getThreadId();

    return globalMainThreadId;
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "thread.h"
    #include <sys/prctl.h>

using namespace zen;




void zen::setCurrentThreadName(const Zstring& threadName)
{
    ::prctl(PR_SET_NAME, threadName.c_str(), 0, 0, 0);

}


namespace
{
//don't make this a function-scope static (avoid code-gen for "magic static")
const std::thread::id globalMainThreadId = std::this_thread::get_id();
}


bool zen::runningOnMainThread()
{
    if (globalMainThreadId == std::thread::id()) //if called during static initialization!
        return true;

    return std::this_thread::get_id() == globalMainThreadId;
}



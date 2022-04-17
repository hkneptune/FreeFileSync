// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SHUTDOWN_H_3423847870238407783265
#define SHUTDOWN_H_3423847870238407783265

#include <functional>
#include "file_error.h"


namespace zen
{
void shutdownSystem(); //throw FileError
void suspendSystem();  //
[[noreturn]] void terminateProcess(int exitCode);

void onSystemShutdownRegister(const SharedRef<std::function<void()>>& task /*noexcept*/); //save important/user data!
void onSystemShutdownRegister(      SharedRef<std::function<void()>>&& task) = delete; //no temporaries! shared_ptr should manage life time!
void onSystemShutdownRunTasks(); //call at appropriate time, e.g. when receiving wxEVT_QUERY_END_SESSION/wxEVT_END_SESSION
//+ also called by shutdownSystem()
}

#endif //SHUTDOWN_H_3423847870238407783265

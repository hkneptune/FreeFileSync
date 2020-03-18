// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SHUTDOWN_H_3423847870238407783265
#define SHUTDOWN_H_3423847870238407783265

#include "file_error.h"


namespace zen
{
void shutdownSystem(); //throw FileError
void suspendSystem();  //
[[noreturn]] void terminateProcess(int exitCode);
}

#endif //SHUTDOWN_H_3423847870238407783265

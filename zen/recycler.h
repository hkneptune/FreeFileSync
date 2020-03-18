// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef RECYCLER_H_18345067341545
#define RECYCLER_H_18345067341545

#include <vector>
#include <functional>
#include "file_error.h"


namespace zen
{
/*
--------------------
|Recycle Bin Access|
--------------------

Windows
-------
-> Recycler API (IFileOperation) always available
-> COM needs to be initialized before calling any of these functions! CoInitializeEx/CoUninitialize

Linux
-----
Compiler flags: `pkg-config --cflags gio-2.0`
Linker   flags: `pkg-config --libs gio-2.0`

Already included in package "gtk+-2.0"!
*/



//move a file or folder to Recycle Bin (deletes permanently if recycler is not available) -> crappy semantics, but we have no choice thanks to Windows' design
bool recycleOrDeleteIfExists(const Zstring& itemPath); //throw FileError, return "true" if file/dir was actually deleted


}

#endif //RECYCLER_H_18345067341545

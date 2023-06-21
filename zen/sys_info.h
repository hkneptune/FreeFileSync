// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SYSTEM_H_4189731847832147508915
#define SYSTEM_H_4189731847832147508915

#include "file_error.h"


namespace zen
{
//COM needs to be initialized before calling any of these functions! CoInitializeEx/CoUninitialize

Zstring getLoginUser(); //throw FileError
Zstring getUserDescription();//throw FileError


struct ComputerModel
{
    std::wstring model;  //best-effort: empty if not available
    std::wstring vendor; //
};
ComputerModel getComputerModel(); //throw FileError



std::wstring getOsDescription(); //throw FileError


Zstring getProcessPath(); //throw FileError

Zstring getUserDownloadsPath(); //throw FileError
Zstring getUserDataPath(); //throw FileError

Zstring getUserHome(); //throw FileError

bool runningElevated(); //throw FileError
}

#endif //SYSTEM_H_4189731847832147508915

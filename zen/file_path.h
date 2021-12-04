// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_PATH_H_3984678473567247567
#define FILE_PATH_H_3984678473567247567

#include "zstring.h"


namespace zen
{
struct PathComponents
{
    Zstring rootPath; //itemPath = rootPath + (FILE_NAME_SEPARATOR?) + relPath
    Zstring relPath;  //
};
std::optional<PathComponents> parsePathComponents(const Zstring& itemPath); //no value on failure

std::optional<Zstring> getParentFolderPath(const Zstring& itemPath);


}

#endif //FILE_PATH_H_3984678473567247567

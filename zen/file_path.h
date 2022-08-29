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
    const Zchar FILE_NAME_SEPARATOR = '/';

struct PathComponents
{
    Zstring rootPath; //itemPath = rootPath + (FILE_NAME_SEPARATOR?) + relPath
    Zstring relPath;  //
};
std::optional<PathComponents> parsePathComponents(const Zstring& itemPath); //no value on failure

std::optional<Zstring> getParentFolderPath(const Zstring& itemPath);

Zstring appendSeparator(Zstring path); //support rvalue references!

bool isValidRelPath(const Zstring& relPath);

Zstring appendPath(const Zstring& basePath, const Zstring& relPath);

Zstring getFileExtension(const Zstring& filePath);

//------------------------------------------------------------------------------------------
/* Compare *local* file paths:
     Windows: igore case (but distinguish Unicode normalization forms!)
     Linux:   byte-wise comparison
     macOS:   ignore case + Unicode normalization forms                    */
std::weak_ordering compareNativePath(const Zstring& lhs, const Zstring& rhs);

inline bool equalNativePath(const Zstring& lhs, const Zstring& rhs) { return compareNativePath(lhs, rhs) == std::weak_ordering::equivalent; }

struct LessNativePath { bool operator()(const Zstring& lhs, const Zstring& rhs) const { return compareNativePath(lhs, rhs) < 0; } };
//------------------------------------------------------------------------------------------


}

#endif //FILE_PATH_H_3984678473567247567

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILER_TRAVERSER_H_127463214871234
#define FILER_TRAVERSER_H_127463214871234

#include <cstdint>
#include <functional>
#include "zstring.h"


namespace zen
{
struct FileInfo
{
    Zstring itemName;
    Zstring fullPath;
    uint64_t fileSize = 0; //[bytes]
    time_t modTime = 0; //number of seconds since Jan. 1st 1970 UTC
};

struct FolderInfo
{
    Zstring itemName;
    Zstring fullPath;
};

struct SymlinkInfo
{
    Zstring itemName;
    Zstring fullPath;
    time_t modTime = 0; //number of seconds since Jan. 1st 1970 UTC
};

//- non-recursive
//- directory path may end with PATH_SEPARATOR
void traverseFolder(const Zstring& dirPath, //noexcept
                    const std::function<void (const FileInfo&    fi)>& onFile,          //
                    const std::function<void (const FolderInfo&  fi)>& onFolder,        //optional
                    const std::function<void (const SymlinkInfo& si)>& onSymlink,       //
                    const std::function<void (const std::wstring& errorMsg)>& onError); //
}

#endif //FILER_TRAVERSER_H_127463214871234

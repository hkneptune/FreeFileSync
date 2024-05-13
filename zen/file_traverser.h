// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILER_TRAVERSER_H_127463214871234
#define FILER_TRAVERSER_H_127463214871234

#include <functional>
#include "file_error.h"
//#include "file_path.h"

namespace zen
{
struct FileInfo
{
    Zstring itemName;
    Zstring fullPath;
    uint64_t fileSize = 0; //[bytes]
    time_t modTime = 0; //number of seconds since Jan. 1st 1970 GMT
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
    time_t modTime = 0; //number of seconds since Jan. 1st 1970 GMT
};

//- non-recursive
void traverseFolder(const Zstring& dirPath,
                    const std::function<void(const FileInfo&    fi)>& onFile,  /*optional*/
                    const std::function<void(const FolderInfo&  fi)>& onFolder,/*optional*/
                    const std::function<void(const SymlinkInfo& si)>& onSymlink/*optional*/); //throw FileError
}

#endif //FILER_TRAVERSER_H_127463214871234

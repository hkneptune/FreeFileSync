// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PARALLEL_SCAN_H_924588904275284572857
#define PARALLEL_SCAN_H_924588904275284572857

#include <map>
#include <set>
#include <chrono>
#include "path_filter.h"
#include "structures.h"
#include "file_hierarchy.h"


namespace fff
{
struct DirectoryKey
{
    AbstractPath folderPath;
    FilterRef filter;
    SymLinkHandling handleSymlinks = SymLinkHandling::EXCLUDE;
};


inline
bool operator<(const DirectoryKey& lhs, const DirectoryKey& rhs)
{
    if (lhs.handleSymlinks != rhs.handleSymlinks)
        return lhs.handleSymlinks < rhs.handleSymlinks;

    const int cmp = AbstractFileSystem::comparePath(lhs.folderPath, rhs.folderPath);
    if (cmp != 0)
        return cmp < 0;

    return lhs.filter.ref() < rhs.filter.ref();
}


struct DirectoryValue
{
    FolderContainer folderCont;

    //relative paths (or empty string for root) for directories that could not be read (completely), e.g. access denied, or temporary network drop
    std::map<Zstring, std::wstring> failedFolderReads; //with corresponding error message

    //relative paths (never empty) for failure to read single file/dir/symlink with corresponding error message
    std::map<Zstring, std::wstring> failedItemReads;
};


//Attention: 1. ensure directory filtering is applied later to exclude filtered folders which have been kept as parent folders
//           2. remove folder aliases (e.g. case differences) *before* calling this function!!!

using TravErrorCb  = std::function<AFS::TraverserCallback::HandleError(const std::wstring& msg,        size_t retryNumber)>;
using TravStatusCb = std::function<                              void (const std::wstring& statusLine, int     itemsTotal)>;

void parallelDeviceTraversal(const std::set<DirectoryKey>& foldersToRead,
                             std::map<DirectoryKey, DirectoryValue>& output,
                             const TravErrorCb& onError, const TravStatusCb& onStatusUpdate, //NOT optional
                             std::chrono::milliseconds cbInterval);
}

#endif //PARALLEL_SCAN_H_924588904275284572857

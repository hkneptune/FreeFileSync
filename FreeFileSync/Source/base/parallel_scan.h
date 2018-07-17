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
#include "hard_filter.h"
#include "structures.h"
#include "file_hierarchy.h"


namespace fff
{
struct DirectoryKey
{
    AbstractPath folderPath;
    HardFilter::FilterRef filter; //always bound by design!
    SymLinkHandling handleSymlinks = SymLinkHandling::EXCLUDE;
};


inline
bool operator<(const DirectoryKey& lhs, const DirectoryKey& rhs)
{
    if (lhs.handleSymlinks != rhs.handleSymlinks)
        return lhs.handleSymlinks < rhs.handleSymlinks;

    const int cmp = AbstractFileSystem::compareAbstractPath(lhs.folderPath, rhs.folderPath);
    if (cmp != 0)
        return cmp < 0;

    return *lhs.filter < *rhs.filter;
}


struct DirectoryValue
{
    FolderContainer folderCont;

    //relative names (or empty string for root) for directories that could not be read (completely), e.g. access denied, or temporal network drop
    std::map<Zstring, std::wstring, LessFilePath> failedFolderReads; //with corresponding error message

    //relative names (never empty) for failure to read single file/dir/symlink with corresponding error message
    std::map<Zstring, std::wstring, LessFilePath> failedItemReads;
};


struct FillBufferCallback
{
    virtual ~FillBufferCallback() {}

    enum HandleError
    {
        ON_ERROR_RETRY,
        ON_ERROR_CONTINUE
    };
    virtual HandleError reportError (const std::wstring& msg, size_t retryNumber) = 0; //throw X
    virtual void        reportStatus(const std::wstring& msg, int    itemsTotal ) = 0; //
};

//attention: ensure directory filtering is applied later to exclude filtered folders which have been kept as parent folders

void fillBuffer(const std::set<DirectoryKey>& foldersToRead, //in
                std::map<DirectoryKey, DirectoryValue>& buf, //out
                const std::map<AbstractPath, size_t>& deviceParallelOps,
                FillBufferCallback& callback,
                std::chrono::milliseconds cbInterval);
}

#endif //PARALLEL_SCAN_H_924588904275284572857

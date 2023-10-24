// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef DB_FILE_H_834275398588021574
#define DB_FILE_H_834275398588021574

#include <unordered_map>
#include <zen/file_error.h>
#include "file_hierarchy.h"
#include "process_callback.h"


namespace fff
{
constexpr ZstringView SYNC_DB_FILE_ENDING = Zstr(".ffs_db"); //don't use Zstring as global constant: avoid static initialization order problem in global namespace!

struct InSyncDescrFile //subset of FileAttributes
{
    time_t modTime = 0;
    AFS::FingerPrint filePrint = 0; //optional!
};

struct InSyncDescrLink
{
    time_t modTime = 0;
};


//artificial hierarchy of last synchronous state:
struct InSyncFile
{
    InSyncDescrFile left;  //support flip()!
    InSyncDescrFile right; //
    CompareVariant cmpVar = CompareVariant::timeSize; //the one active while finding "file in sync"
    uint64_t fileSize = 0; //file size must be identical on both sides!
};

struct InSyncSymlink
{
    InSyncDescrLink left;
    InSyncDescrLink right;
    CompareVariant cmpVar = CompareVariant::timeSize;
};

struct InSyncFolder
{
    //------------------------------------------------------------------
    using FolderList  = std::unordered_map<ZstringNorm, InSyncFolder >; //
    using FileList    = std::unordered_map<ZstringNorm, InSyncFile   >; // key: file name (ignoring Unicode normal forms)
    using SymlinkList = std::unordered_map<ZstringNorm, InSyncSymlink>; //
    //------------------------------------------------------------------

    FolderList  folders;
    FileList    files;
    SymlinkList symlinks; //non-followed symlinks

    //convenience
    InSyncFolder& addFolder(const Zstring& folderName)
    {
        const auto [it, inserted] = folders.try_emplace(folderName);
        assert(inserted);
        return it->second;
    }

    void addFile(const Zstring& fileName, const InSyncDescrFile& descrL, const InSyncDescrFile& descrR, CompareVariant cmpVar, uint64_t fileSize)
    {
            files.emplace(fileName, InSyncFile {descrL, descrR, cmpVar, fileSize});
        assert(inserted);
    }

    void addSymlink(const Zstring& linkName, const InSyncDescrLink& descrL, const InSyncDescrLink& descrR, CompareVariant cmpVar)
    {
            symlinks.emplace(linkName, InSyncSymlink {descrL, descrR, cmpVar});
        assert(inserted);
    }
};


std::unordered_map<const BaseFolderPair*, zen::SharedRef<const InSyncFolder>> loadLastSynchronousState(const std::vector<const BaseFolderPair*>& baseFolders,
                                                                           PhaseCallback& callback /*throw X*/); //throw X

void saveLastSynchronousState(const BaseFolderPair& baseFolder, bool transactionalCopy, //throw X
                              PhaseCallback& callback /*throw X*/);
}

#endif //DB_FILE_H_834275398588021574

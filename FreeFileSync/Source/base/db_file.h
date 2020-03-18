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
const Zchar SYNC_DB_FILE_ENDING[] = Zstr(".ffs_db"); //don't use Zstring as global constant: avoid static initialization order problem in global namespace!

struct InSyncDescrFile //subset of FileAttributes
{
    InSyncDescrFile(time_t modTimeIn, const AFS::FileId& idIn) :
        modTime(modTimeIn),
        fileId(idIn) {}

    time_t modTime = 0;
    AFS::FileId fileId; // == file id: optional! (however, always set on Linux, and *generally* available on Windows)
};

struct InSyncDescrLink
{
    explicit InSyncDescrLink(time_t modTimeIn) : modTime(modTimeIn) {}

    time_t modTime = 0;
};


//artificial hierarchy of last synchronous state:
struct InSyncFile
{
    InSyncFile(const InSyncDescrFile& l, const InSyncDescrFile& r, CompareVariant cv, uint64_t fileSizeIn) : left(l), right(r), cmpVar(cv), fileSize(fileSizeIn) {}
    InSyncDescrFile left;  //support flip()!
    InSyncDescrFile right; //
    CompareVariant cmpVar = CompareVariant::timeSize; //the one active while finding "file in sync"
    uint64_t fileSize = 0; //file size must be identical on both sides!
};

struct InSyncSymlink
{
    InSyncSymlink(const InSyncDescrLink& l, const InSyncDescrLink& r, CompareVariant cv) : left(l), right(r), cmpVar(cv) {}
    InSyncDescrLink left;
    InSyncDescrLink right;
    CompareVariant cmpVar = CompareVariant::timeSize;
};

struct InSyncFolder
{
    //for directories we have a logical problem: we cannot have "not existent" as an indicator for
    //"no last synchronous state" since this precludes child elements that may be in sync!
    enum InSyncStatus
    {
        DIR_STATUS_IN_SYNC,
        DIR_STATUS_STRAW_MAN //no last synchronous state, but used as container only
    };
    InSyncFolder(InSyncStatus statusIn) : status(statusIn) {}

    InSyncStatus status = DIR_STATUS_STRAW_MAN;

    //------------------------------------------------------------------
    using FolderList  = std::map<Zstring, InSyncFolder,  LessUnicodeNormal>; //
    using FileList    = std::map<Zstring, InSyncFile,    LessUnicodeNormal>; // key: file name (ignoring Unicode normal forms)
    using SymlinkList = std::map<Zstring, InSyncSymlink, LessUnicodeNormal>; //
    //------------------------------------------------------------------

    FolderList  folders;
    FileList    files;
    SymlinkList symlinks; //non-followed symlinks

    //convenience
    InSyncFolder& addFolder(const Zstring& folderName, InSyncStatus st)
    {
        return folders.emplace(folderName, InSyncFolder(st)).first->second;
    }

    void addFile(const Zstring& fileName, const InSyncDescrFile& dataL, const InSyncDescrFile& dataR, CompareVariant cmpVar, uint64_t fileSize)
    {
        files.emplace(fileName, InSyncFile(dataL, dataR, cmpVar, fileSize));
    }

    void addSymlink(const Zstring& linkName, const InSyncDescrLink& dataL, const InSyncDescrLink& dataR, CompareVariant cmpVar)
    {
        symlinks.emplace(linkName, InSyncSymlink(dataL, dataR, cmpVar));
    }
};


std::unordered_map<const BaseFolderPair*, zen::SharedRef<const InSyncFolder>> loadLastSynchronousState(const std::vector<const BaseFolderPair*>& baseFolders,
                                                                           PhaseCallback& callback /*throw X*/); //throw X

void saveLastSynchronousState(const BaseFolderPair& baseFolder, bool transactionalCopy, //throw X
                              PhaseCallback& callback /*throw X*/);
}

#endif //DB_FILE_H_834275398588021574

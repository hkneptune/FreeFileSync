// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "algorithm.h"
#include <set>
#include <unordered_map>
#include <zen/perf.h>
#include <zen/crc.h>
#include <zen/guid.h>
#include <zen/file_access.h> //needed for TempFileBuffer only
#include <zen/serialize.h>
#include "norm_filter.h"
#include "db_file.h"
#include "cmp_filetime.h"
#include "status_handler_impl.h"
#include "../afs/concrete.h"
#include "../afs/native.h"


using namespace zen;
using namespace fff;



namespace
{
class RecursiveObjectVisitorImpl
{
public:
    RecursiveObjectVisitorImpl(std::function<void (FolderPair&   folder)> onFolder,
                               std::function<void (FilePair&       file)> onFile,
                               std::function<void (SymlinkPair& symlink)> onSymlink) :
        onFolder_(onFolder), onFile_(onFile), onSymlink_(onSymlink) {}

    void execute(FileSystemObject& fsObj)
    {
        visitFSObject(fsObj,
        [&](const FolderPair&   folder) { visit(const_cast<FolderPair& >(folder )); },  //
        [&](const FilePair&       file) { visit(const_cast<FilePair&   >(file   )); },  //physical object is not const anyway
        [&](const SymlinkPair& symlink) { visit(const_cast<SymlinkPair&>(symlink)); }); //
    }

private:
    void visit(FolderPair& folder)
    {
        if (onFolder_)
            onFolder_(folder);

        for (FilePair& file : folder.refSubFiles())
            visit(file);
        for (SymlinkPair& symlink : folder.refSubLinks())
            visit(symlink);
        for (FolderPair& subFolder : folder.refSubFolders())
            visit(subFolder);
    }

    void visit(FilePair& file)
    {
        if (onFile_)
            onFile_(file);
    }

    void visit(SymlinkPair& symlink)
    {
        if (onSymlink_)
            onSymlink_(symlink);
    }

    std::function<void (FolderPair&   folder)> onFolder_;
    std::function<void (FilePair&       file)> onFile_;
    std::function<void (SymlinkPair& symlink)> onSymlink_;
};
}


void fff::recursiveObjectVisitor(FileSystemObject& fsObj,
                                 std::function<void (FolderPair&   folder)> onFolder,
                                 std::function<void (FilePair&       file)> onFile,
                                 std::function<void (SymlinkPair& symlink)> onSymlink)
{
    RecursiveObjectVisitorImpl(onFolder, onFile, onSymlink).execute(fsObj);
}


void fff::swapGrids(const MainConfiguration& mainCfg, FolderComparison& folderCmp) //throw FileError
{
    std::for_each(begin(folderCmp), end(folderCmp), [](BaseFolderPair& baseFolder) { baseFolder.flip(); });

    redetermineSyncDirection(extractDirectionCfg(mainCfg), folderCmp, nullptr /*notifyStatus*/); //throw FileError
}

//----------------------------------------------------------------------------------------------

namespace
{
class Redetermine
{
public:
    static void execute(const DirectionSet& dirCfgIn, ContainerObject& hierObj) { Redetermine(dirCfgIn).recurse(hierObj); }

private:
    Redetermine(const DirectionSet& dirCfgIn) : dirCfg(dirCfgIn) {}

    void recurse(ContainerObject& hierObj) const
    {
        for (FilePair& file : hierObj.refSubFiles())
            processFile(file);
        for (SymlinkPair& link : hierObj.refSubLinks())
            processLink(link);
        for (FolderPair& folder : hierObj.refSubFolders())
            processFolder(folder);
    }

    void processFile(FilePair& file) const
    {
        const CompareFilesResult cat = file.getCategory();

        //##################### schedule old temporary files for deletion ####################
        if (cat == FILE_LEFT_SIDE_ONLY && endsWith(file.getItemName<LEFT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::LEFT);
        else if (cat == FILE_RIGHT_SIDE_ONLY && endsWith(file.getItemName<RIGHT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::RIGHT);
        //####################################################################################

        switch (cat)
        {
            case FILE_LEFT_SIDE_ONLY:
                file.setSyncDir(dirCfg.exLeftSideOnly);
                break;
            case FILE_RIGHT_SIDE_ONLY:
                file.setSyncDir(dirCfg.exRightSideOnly);
                break;
            case FILE_RIGHT_NEWER:
                file.setSyncDir(dirCfg.rightNewer);
                break;
            case FILE_LEFT_NEWER:
                file.setSyncDir(dirCfg.leftNewer);
                break;
            case FILE_DIFFERENT_CONTENT:
                file.setSyncDir(dirCfg.different);
                break;
            case FILE_CONFLICT:
            case FILE_DIFFERENT_METADATA: //use setting from "conflict/cannot categorize"
                if (dirCfg.conflict == SyncDirection::NONE)
                    file.setSyncDirConflict(file.getCatExtraDescription()); //take over category conflict
                else
                    file.setSyncDir(dirCfg.conflict);
                break;
            case FILE_EQUAL:
                file.setSyncDir(SyncDirection::NONE);
                break;
        }
    }

    void processLink(SymlinkPair& symlink) const
    {
        switch (symlink.getLinkCategory())
        {
            case SYMLINK_LEFT_SIDE_ONLY:
                symlink.setSyncDir(dirCfg.exLeftSideOnly);
                break;
            case SYMLINK_RIGHT_SIDE_ONLY:
                symlink.setSyncDir(dirCfg.exRightSideOnly);
                break;
            case SYMLINK_LEFT_NEWER:
                symlink.setSyncDir(dirCfg.leftNewer);
                break;
            case SYMLINK_RIGHT_NEWER:
                symlink.setSyncDir(dirCfg.rightNewer);
                break;
            case SYMLINK_CONFLICT:
            case SYMLINK_DIFFERENT_METADATA: //use setting from "conflict/cannot categorize"
                if (dirCfg.conflict == SyncDirection::NONE)
                    symlink.setSyncDirConflict(symlink.getCatExtraDescription()); //take over category conflict
                else
                    symlink.setSyncDir(dirCfg.conflict);
                break;
            case SYMLINK_DIFFERENT_CONTENT:
                symlink.setSyncDir(dirCfg.different);
                break;
            case SYMLINK_EQUAL:
                symlink.setSyncDir(SyncDirection::NONE);
                break;
        }
    }

    void processFolder(FolderPair& folder) const
    {
        const CompareDirResult cat = folder.getDirCategory();

        //########### schedule abandoned temporary recycle bin directory for deletion  ##########
        if (cat == DIR_LEFT_SIDE_ONLY && endsWith(folder.getItemName<LEFT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::LEFT, folder); //
        else if (cat == DIR_RIGHT_SIDE_ONLY && endsWith(folder.getItemName<RIGHT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::RIGHT, folder); //don't recurse below!
        //#######################################################################################

        switch (cat)
        {
            case DIR_LEFT_SIDE_ONLY:
                folder.setSyncDir(dirCfg.exLeftSideOnly);
                break;
            case DIR_RIGHT_SIDE_ONLY:
                folder.setSyncDir(dirCfg.exRightSideOnly);
                break;
            case DIR_EQUAL:
                folder.setSyncDir(SyncDirection::NONE);
                break;
            case DIR_CONFLICT:
            case DIR_DIFFERENT_METADATA: //use setting from "conflict/cannot categorize"
                if (dirCfg.conflict == SyncDirection::NONE)
                    folder.setSyncDirConflict(folder.getCatExtraDescription()); //take over category conflict
                else
                    folder.setSyncDir(dirCfg.conflict);
                break;
        }

        recurse(folder);
    }

    const DirectionSet dirCfg;
};

//---------------------------------------------------------------------------------------------------------------

//test if non-equal items exist in scanned data
bool allItemsCategoryEqual(const ContainerObject& hierObj)
{
    return std::all_of(hierObj.refSubFiles().begin(), hierObj.refSubFiles().end(),
    [](const FilePair& file) { return file.getCategory() == FILE_EQUAL; })&&

    std::all_of(hierObj.refSubLinks().begin(), hierObj.refSubLinks().end(),
    [](const SymlinkPair& link) { return link.getLinkCategory() == SYMLINK_EQUAL; })&&

    std::all_of(hierObj.refSubFolders().begin(), hierObj.refSubFolders().end(),
                [](const FolderPair& folder)
    {
        return folder.getDirCategory() == DIR_EQUAL && allItemsCategoryEqual(folder); //short-circuit behavior!
    });
}
}

bool fff::allElementsEqual(const FolderComparison& folderCmp)
{
    return std::all_of(begin(folderCmp), end(folderCmp), [](const BaseFolderPair& baseFolder) { return allItemsCategoryEqual(baseFolder); });
}

//---------------------------------------------------------------------------------------------------------------

namespace
{
template <SelectedSide side> inline
bool matchesDbEntry(const FilePair& file, const InSyncFile* dbFile, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    if (file.isEmpty<side>())
        return !dbFile;
    else if (!dbFile)
        return false;

    const InSyncDescrFile& descrDb = SelectParam<side>::ref(dbFile->left, dbFile->right);

    //respect 2 second FAT/FAT32 precision! copying a file to a FAT32 drive changes it's modification date by up to 2 seconds
    return //we're not interested in "fileTimeTolerance" here!
        sameFileTime(file.getLastWriteTime<side>(), descrDb.modTime, 2, ignoreTimeShiftMinutes) &&
        file.getFileSize<side>() == dbFile->fileSize;
    //note: we do *not* consider FileId here, but are only interested in *visual* changes. Consider user moving data to some other medium, this is not a change!
}


//check whether database entry is in sync considering *current* comparison settings
inline
bool stillInSync(const InSyncFile& dbFile, CompareVariant compareVar, int fileTimeTolerance, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    switch (compareVar)
    {
        case CompareVariant::TIME_SIZE:
            if (dbFile.cmpVar == CompareVariant::CONTENT) return true; //special rule: this is certainly "good enough" for CompareVariant::TIME_SIZE!

            //case-sensitive short name match is a database invariant!
            return sameFileTime(dbFile.left.modTime, dbFile.right.modTime, fileTimeTolerance, ignoreTimeShiftMinutes);

        case CompareVariant::CONTENT:
            //case-sensitive short name match is a database invariant!
            return dbFile.cmpVar == CompareVariant::CONTENT;
        //in contrast to comparison, we don't care about modification time here!

        case CompareVariant::SIZE: //file size/case-sensitive short name always matches on both sides for an "in-sync" database entry
            return true;
    }
    assert(false);
    return false;
}

//--------------------------------------------------------------------

//check whether database entry and current item match: *irrespective* of current comparison settings
template <SelectedSide side> inline
bool matchesDbEntry(const SymlinkPair& symlink, const InSyncSymlink* dbSymlink, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    if (symlink.isEmpty<side>())
        return !dbSymlink;
    else if (!dbSymlink)
        return false;

    const InSyncDescrLink& descrDb = SelectParam<side>::ref(dbSymlink->left, dbSymlink->right);

    //respect 2 second FAT/FAT32 precision! copying a file to a FAT32 drive changes its modification date by up to 2 seconds
    return sameFileTime(symlink.getLastWriteTime<side>(), descrDb.modTime, 2, ignoreTimeShiftMinutes);
}


//check whether database entry is in sync considering *current* comparison settings
inline
bool stillInSync(const InSyncSymlink& dbLink, CompareVariant compareVar, int fileTimeTolerance, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    switch (compareVar)
    {
        case CompareVariant::TIME_SIZE:
            if (dbLink.cmpVar == CompareVariant::CONTENT || dbLink.cmpVar == CompareVariant::SIZE)
                return true; //special rule: this is already "good enough" for CompareVariant::TIME_SIZE!

            //case-sensitive short name match is a database invariant!
            return sameFileTime(dbLink.left.modTime, dbLink.right.modTime, fileTimeTolerance, ignoreTimeShiftMinutes);

        case CompareVariant::CONTENT:
        case CompareVariant::SIZE: //== categorized by content! see comparison.cpp, ComparisonBuffer::compareBySize()
            //case-sensitive short name match is a database invariant!
            return dbLink.cmpVar == CompareVariant::CONTENT || dbLink.cmpVar == CompareVariant::SIZE;
    }
    assert(false);
    return false;
}

//--------------------------------------------------------------------

//check whether database entry and current item match: *irrespective* of current comparison settings
template <SelectedSide side> inline
bool matchesDbEntry(const FolderPair& folder, const InSyncFolder* dbFolder)
{
    const bool haveDbEntry = dbFolder && dbFolder->status != InSyncFolder::DIR_STATUS_STRAW_MAN;
    return haveDbEntry == !folder.isEmpty<side>();
}


inline
bool stillInSync(const InSyncFolder& dbFolder)
{
    //case-sensitive short name match is a database invariant!
    //InSyncFolder::DIR_STATUS_STRAW_MAN considered
    return true;
}

//----------------------------------------------------------------------------------------------

class DetectMovedFiles
{
public:
    static void execute(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder) { DetectMovedFiles(baseFolder, dbFolder); }

private:
    DetectMovedFiles(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder) :
        cmpVar_           (baseFolder.getCompVariant()),
        fileTimeTolerance_(baseFolder.getFileTimeTolerance()),
        ignoreTimeShiftMinutes_(baseFolder.getIgnoredTimeShift())
    {
        recurse(baseFolder, &dbFolder, &dbFolder);

        if ((!exLeftOnlyById_ .empty() || !exLeftOnlyByPath_ .empty()) &&
            (!exRightOnlyById_.empty() || !exRightOnlyByPath_.empty()))
            detectMovePairs(dbFolder);
    }

    void recurse(ContainerObject& hierObj, const InSyncFolder* dbFolderL, const InSyncFolder* dbFolderR)
    {
        for (FilePair& file : hierObj.refSubFiles())
        {
            auto getDbEntry = [](const InSyncFolder* dbFolder, const Zstring& fileName) -> const InSyncFile*
            {
                if (dbFolder)
                {
                    auto it = dbFolder->files.find(fileName);
                    if (it != dbFolder->files.end())
                        return &it->second;
                }
                return nullptr;
            };

            const CompareFilesResult cat = file.getCategory();

            if (cat == FILE_LEFT_SIDE_ONLY)
            {
                if (const InSyncFile* dbEntry = getDbEntry(dbFolderL, file.getItemName<LEFT_SIDE>()))
                    exLeftOnlyByPath_.emplace(dbEntry, &file);
                else if (!file.getFileId<LEFT_SIDE>().empty())
                {
                    const auto [it, inserted] = exLeftOnlyById_.emplace(file.getFileId<LEFT_SIDE>(), &file);
                    if (!inserted) //duplicate file ID! NTFS hard link/symlink?
                        it->second = nullptr;
                }
            }
            else if (cat == FILE_RIGHT_SIDE_ONLY)
            {
                if (const InSyncFile* dbEntry = getDbEntry(dbFolderR, file.getItemName<RIGHT_SIDE>()))
                    exRightOnlyByPath_.emplace(dbEntry, &file);
                else if (!file.getFileId<RIGHT_SIDE>().empty())
                {
                    const auto [it, inserted] = exRightOnlyById_.emplace(file.getFileId<RIGHT_SIDE>(), &file);
                    if (!inserted) //duplicate file ID! NTFS hard link/symlink?
                        it->second = nullptr;
                }
            }
        }

        for (FolderPair& folder : hierObj.refSubFolders())
        {
            auto getDbEntry = [](const InSyncFolder* dbFolder, const Zstring& folderName) -> const InSyncFolder*
            {
                if (dbFolder)
                {
                    auto it = dbFolder->folders.find(folderName);
                    if (it != dbFolder->folders.end())
                        return &it->second;
                }
                return nullptr;
            };
            const InSyncFolder* dbEntryL = getDbEntry(dbFolderL, folder.getItemName<LEFT_SIDE>());
            const InSyncFolder* dbEntryR = dbEntryL;
            if (dbFolderL != dbFolderR || getUnicodeNormalForm(folder.getItemName<LEFT_SIDE>()) != getUnicodeNormalForm(folder.getItemName<RIGHT_SIDE>()))
                dbEntryR = getDbEntry(dbFolderR, folder.getItemName<RIGHT_SIDE>());

            recurse(folder, dbEntryL, dbEntryR);
        }
    }

    void detectMovePairs(const InSyncFolder& container) const
    {
        for (const auto& [fileName, dbAttrib] : container.files)
            findAndSetMovePair(dbAttrib);

        for (const auto& [folderName, subFolder] : container.folders)
            detectMovePairs(subFolder);
    }

    template <SelectedSide side>
    static bool sameSizeAndDate(const FilePair& file, const InSyncFile& dbFile)
    {
        return file.getFileSize<side>() == dbFile.fileSize &&
               sameFileTime(file.getLastWriteTime<side>(), SelectParam<side>::ref(dbFile.left, dbFile.right).modTime, 2, {});
        //- respect 2 second FAT/FAT32 precision! not user-configurable!
        //- "ignoreTimeShiftMinutes" may lead to false positive move detections => let's be conservative and not allow it
        //  (time shift is only ever required during FAT DST switches)

        //PS: *never* allow 2 sec tolerance as container predicate!!
        // => no strict weak ordering relation! reason: no transitivity of equivalence!
    }

    template <SelectedSide side>
    FilePair* getAssocFilePair(const InSyncFile& dbFile) const
    {
        const std::unordered_map<AFS::FileId,       FilePair*, StringHash>& exOneSideById   = SelectParam<side>::ref(exLeftOnlyById_,   exRightOnlyById_);
        const std::unordered_map<const InSyncFile*, FilePair*            >& exOneSideByPath = SelectParam<side>::ref(exLeftOnlyByPath_, exRightOnlyByPath_);
        {
            auto it = exOneSideByPath.find(&dbFile);
            if (it != exOneSideByPath.end())
                return it->second; //if there is an association by path, don't care if there is also an association by id,
            //even if the association by path doesn't match time and size while the association by id does!
            //- there doesn't seem to be (any?) value in allowing this!
            //- note: exOneSideById isn't filled in this case, see recurse()
        }

        const AFS::FileId fileId = SelectParam<side>::ref(dbFile.left, dbFile.right).fileId;
        if (!fileId.empty())
        {
            auto it = exOneSideById.find(fileId);
            if (it != exOneSideById.end())
                return it->second; //= nullptr, if duplicate ID!
        }
        return nullptr;
    }

    void findAndSetMovePair(const InSyncFile& dbFile) const
    {
        if (stillInSync(dbFile, cmpVar_, fileTimeTolerance_, ignoreTimeShiftMinutes_))
            if (FilePair* fileLeftOnly = getAssocFilePair<LEFT_SIDE>(dbFile))
                if (sameSizeAndDate<LEFT_SIDE>(*fileLeftOnly, dbFile))
                    if (FilePair* fileRightOnly = getAssocFilePair<RIGHT_SIDE>(dbFile))
                        if (sameSizeAndDate<RIGHT_SIDE>(*fileRightOnly, dbFile))
                            if (fileLeftOnly ->getMoveRef() == nullptr && //don't let a row participate in two move pairs!
                                fileRightOnly->getMoveRef() == nullptr)   //
                            {
                                fileLeftOnly ->setMoveRef(fileRightOnly->getId()); //found a pair, mark it!
                                fileRightOnly->setMoveRef(fileLeftOnly ->getId()); //
                            }
    }

    const CompareVariant cmpVar_;
    const int fileTimeTolerance_;
    const std::vector<unsigned int> ignoreTimeShiftMinutes_;

    std::unordered_map<AFS::FileId, FilePair*, StringHash> exLeftOnlyById_;  //FilePair* == nullptr for duplicate ids! => consider aliasing through symlinks!
    std::unordered_map<AFS::FileId, FilePair*, StringHash> exRightOnlyById_; //=> avoid ambiguity for mixtures of files/symlinks on one side and allow 1-1 mapping only!
    //MSVC: std::unordered_map: about twice as fast as std::map for 1 million items!

    std::unordered_map<const InSyncFile*, FilePair*> exLeftOnlyByPath_; //MSVC: only 4% faster than std::map for 1 million items!
    std::unordered_map<const InSyncFile*, FilePair*> exRightOnlyByPath_;
    /*
    detect renamed files:

     X  ->  |_|      Create right
    |_| ->   Y       Delete right

    is detected as:

    Rename Y to X on right

    Algorithm:
    ----------
    DB-file left  <--- (name, size, date) --->  DB-file right
          |                                          |
          |  (file ID, size, date)                   |  (file ID, size, date)
          |            or                            |            or
          |  (file path, size, date)                 |  (file path, size, date)
         \|/                                        \|/
    file left only                             file right only

       FAT caveat: File Ids are generally not stable when file is either moved or renamed!
       => 1. Move/rename operations on FAT cannot be detected reliably.
       => 2. database generally contains wrong file ID on FAT after renaming from .ffs_tmp files => correct file IDs in database only after next sync
       => 3. even exFAT screws up (but less than FAT) and changes IDs after file move. Did they learn nothing from the past?
    */
};

//----------------------------------------------------------------------------------------------

class RedetermineTwoWay
{
public:
    static void execute(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder) { RedetermineTwoWay(baseFolder, dbFolder); }

private:
    RedetermineTwoWay(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder) :
        cmpVar_                (baseFolder.getCompVariant()),
        fileTimeTolerance_     (baseFolder.getFileTimeTolerance()),
        ignoreTimeShiftMinutes_(baseFolder.getIgnoredTimeShift())
    {
        //-> considering filter not relevant:
        //  if stricter filter than last time: all ok;
        //  if less strict filter (if file ex on both sides -> conflict, fine; if file ex. on one side: copy to other side: fine)
        recurse(baseFolder, &dbFolder, &dbFolder);
    }

    void recurse(ContainerObject& hierObj, const InSyncFolder* dbFolderL, const InSyncFolder* dbFolderR) const
    {
        for (FilePair& file : hierObj.refSubFiles())
            processFile(file, dbFolderL, dbFolderR);
        for (SymlinkPair& link : hierObj.refSubLinks())
            processSymlink(link, dbFolderL, dbFolderR);
        for (FolderPair& folder : hierObj.refSubFolders())
            processDir(folder, dbFolderL, dbFolderR);
    }

    void processFile(FilePair& file, const InSyncFolder* dbFolderL, const InSyncFolder* dbFolderR) const
    {
        const CompareFilesResult cat = file.getCategory();
        if (cat == FILE_EQUAL)
            return;

        //##################### schedule old temporary files for deletion ####################
        if (cat == FILE_LEFT_SIDE_ONLY && endsWith(file.getItemName<LEFT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::LEFT);
        else if (cat == FILE_RIGHT_SIDE_ONLY && endsWith(file.getItemName<RIGHT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::RIGHT);
        //####################################################################################

        //try to find corresponding database entry
        auto getDbEntry = [](const InSyncFolder* dbFolder, const Zstring& fileName) -> const InSyncFile*
        {
            if (dbFolder)
            {
                auto it = dbFolder->files.find(fileName);
                if (it != dbFolder->files.end())
                    return &it->second;
            }
            return nullptr;
        };
        const InSyncFile* dbEntryL = getDbEntry(dbFolderL, file.getItemName<LEFT_SIDE>());
        const InSyncFile* dbEntryR = dbEntryL;
        if (dbFolderL != dbFolderR || getUnicodeNormalForm(file.getItemName<LEFT_SIDE>()) != getUnicodeNormalForm(file.getItemName<RIGHT_SIDE>()))
            dbEntryR = getDbEntry(dbFolderR, file.getItemName<RIGHT_SIDE>());

        //evaluation
        const bool changeOnLeft  = !matchesDbEntry< LEFT_SIDE>(file, dbEntryL, ignoreTimeShiftMinutes_);
        const bool changeOnRight = !matchesDbEntry<RIGHT_SIDE>(file, dbEntryR, ignoreTimeShiftMinutes_);

        if (changeOnLeft != changeOnRight)
        {
            //if database entry not in sync according to current settings! -> set direction based on sync status only!
            if ((dbEntryL && !stillInSync(*dbEntryL, cmpVar_, fileTimeTolerance_, ignoreTimeShiftMinutes_)) ||
                (dbEntryR && !stillInSync(*dbEntryR, cmpVar_, fileTimeTolerance_, ignoreTimeShiftMinutes_)))
                file.setSyncDirConflict(txtDbNotInSync_);
            else
                file.setSyncDir(changeOnLeft ? SyncDirection::RIGHT : SyncDirection::LEFT);
        }
        else
        {
            if (changeOnLeft)
                file.setSyncDirConflict(txtBothSidesChanged_);
            else
                file.setSyncDirConflict(txtNoSideChanged_);
        }
    }

    void processSymlink(SymlinkPair& symlink, const InSyncFolder* dbFolderL, const InSyncFolder* dbFolderR) const
    {
        const CompareSymlinkResult cat = symlink.getLinkCategory();
        if (cat == SYMLINK_EQUAL)
            return;

        //try to find corresponding database entry
        auto getDbEntry = [](const InSyncFolder* dbFolder, const Zstring& linkName) -> const InSyncSymlink*
        {
            if (dbFolder)
            {
                auto it = dbFolder->symlinks.find(linkName);
                if (it != dbFolder->symlinks.end())
                    return &it->second;
            }
            return nullptr;
        };
        const InSyncSymlink* dbEntryL = getDbEntry(dbFolderL, symlink.getItemName<LEFT_SIDE>());
        const InSyncSymlink* dbEntryR = dbEntryL;
        if (dbFolderL != dbFolderR || getUnicodeNormalForm(symlink.getItemName<LEFT_SIDE>()) != getUnicodeNormalForm(symlink.getItemName<RIGHT_SIDE>()))
            dbEntryR = getDbEntry(dbFolderR, symlink.getItemName<RIGHT_SIDE>());

        //evaluation
        const bool changeOnLeft  = !matchesDbEntry< LEFT_SIDE>(symlink, dbEntryL, ignoreTimeShiftMinutes_);
        const bool changeOnRight = !matchesDbEntry<RIGHT_SIDE>(symlink, dbEntryR, ignoreTimeShiftMinutes_);

        if (changeOnLeft != changeOnRight)
        {
            //if database entry not in sync according to current settings! -> set direction based on sync status only!
            if ((dbEntryL && !stillInSync(*dbEntryL, cmpVar_, fileTimeTolerance_, ignoreTimeShiftMinutes_)) ||
                (dbEntryR && !stillInSync(*dbEntryR, cmpVar_, fileTimeTolerance_, ignoreTimeShiftMinutes_)))
                symlink.setSyncDirConflict(txtDbNotInSync_);
            else
                symlink.setSyncDir(changeOnLeft ? SyncDirection::RIGHT : SyncDirection::LEFT);
        }
        else
        {
            if (changeOnLeft)
                symlink.setSyncDirConflict(txtBothSidesChanged_);
            else
                symlink.setSyncDirConflict(txtNoSideChanged_);
        }
    }

    void processDir(FolderPair& folder, const InSyncFolder* dbFolderL, const InSyncFolder* dbFolderR) const
    {
        const CompareDirResult cat = folder.getDirCategory();

        //########### schedule abandoned temporary recycle bin directory for deletion  ##########
        if (cat == DIR_LEFT_SIDE_ONLY && endsWith(folder.getItemName<LEFT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::LEFT, folder); //
        else if (cat == DIR_RIGHT_SIDE_ONLY && endsWith(folder.getItemName<RIGHT_SIDE>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::RIGHT, folder); //don't recurse below!
        //#######################################################################################

        //try to find corresponding database entry
        auto getDbEntry = [](const InSyncFolder* dbFolder, const Zstring& folderName) -> const InSyncFolder*
        {
            if (dbFolder)
            {
                auto it = dbFolder->folders.find(folderName);
                if (it != dbFolder->folders.end())
                    return &it->second;
            }
            return nullptr;
        };
        const InSyncFolder* dbEntryL = getDbEntry(dbFolderL, folder.getItemName<LEFT_SIDE>());
        const InSyncFolder* dbEntryR = dbEntryL;
        if (dbFolderL != dbFolderR || getUnicodeNormalForm(folder.getItemName<LEFT_SIDE>()) != getUnicodeNormalForm(folder.getItemName<RIGHT_SIDE>()))
            dbEntryR = getDbEntry(dbFolderR, folder.getItemName<RIGHT_SIDE>());

        if (cat != DIR_EQUAL)
        {
            //evaluation
            const bool changeOnLeft  = !matchesDbEntry< LEFT_SIDE>(folder, dbEntryL);
            const bool changeOnRight = !matchesDbEntry<RIGHT_SIDE>(folder, dbEntryR);

            if (changeOnLeft != changeOnRight)
            {
                //if database entry not in sync according to current settings! -> set direction based on sync status only!
                if ((dbEntryL && !stillInSync(*dbEntryL)) ||
                    (dbEntryR && !stillInSync(*dbEntryR)))
                    folder.setSyncDirConflict(txtDbNotInSync_);
                else
                    folder.setSyncDir(changeOnLeft ? SyncDirection::RIGHT : SyncDirection::LEFT);
            }
            else
            {
                if (changeOnLeft)
                    folder.setSyncDirConflict(txtBothSidesChanged_);
                else
                    folder.setSyncDirConflict(txtNoSideChanged_);
            }
        }

        recurse(folder, dbEntryL, dbEntryR);
    }

    const std::wstring txtBothSidesChanged_ = _("Both sides have changed since last synchronization.");
    const std::wstring txtNoSideChanged_    = _("Cannot determine sync-direction:") + L" \n" + _("No change since last synchronization.");
    const std::wstring txtDbNotInSync_      = _("Cannot determine sync-direction:") + L" \n" + _("The database entry is not in sync considering current settings.");

    const CompareVariant cmpVar_;
    const int fileTimeTolerance_;
    const std::vector<unsigned int> ignoreTimeShiftMinutes_;
};
}

//---------------------------------------------------------------------------------------------------------------

std::vector<DirectionConfig> fff::extractDirectionCfg(const MainConfiguration& mainCfg)
{
    //merge first and additional pairs
    std::vector<LocalPairConfig> allPairs;
    allPairs.push_back(mainCfg.firstPair);
    allPairs.insert(allPairs.end(),
                    mainCfg.additionalPairs.begin(), //add additional pairs
                    mainCfg.additionalPairs.end());

    std::vector<DirectionConfig> output;
    for (const LocalPairConfig& lpc : allPairs)
        output.push_back(lpc.localSyncCfg ? lpc.localSyncCfg->directionCfg : mainCfg.syncCfg.directionCfg);

    return output;
}


void fff::redetermineSyncDirection(const DirectionConfig& dirCfg, //throw FileError
                                   BaseFolderPair& baseFolder,
                                   const std::function<void(const std::wstring& msg)>& notifyStatus)
{
    std::optional<FileError> dbLoadError; //defer until after default directions have been set!

    //try to load sync-database files
    std::shared_ptr<InSyncFolder> lastSyncState;
    if (dirCfg.var == DirectionConfig::TWO_WAY || detectMovedFilesEnabled(dirCfg))
        try
        {
            if (allItemsCategoryEqual(baseFolder))
                return; //nothing to do: abort and don't even try to open db files

            lastSyncState = loadLastSynchronousState(baseFolder, notifyStatus); //throw FileError, FileErrorDatabaseNotExisting
        }
        catch (FileErrorDatabaseNotExisting&) {} //let's ignore this error, there's no value in reporting it other than to confuse users
        catch (const FileError& e) //e.g. incompatible database version
        {
            if (dirCfg.var == DirectionConfig::TWO_WAY)
                dbLoadError = FileError(e.toString(), _("Setting default synchronization directions: Old files will be overwritten with newer files."));
            else
                dbLoadError = e;
        }

    //set sync directions
    if (dirCfg.var == DirectionConfig::TWO_WAY)
    {
        if (lastSyncState)
            RedetermineTwoWay::execute(baseFolder, *lastSyncState);
        else //default fallback
            Redetermine::execute(getTwoWayUpdateSet(), baseFolder);
    }
    else
        Redetermine::execute(extractDirections(dirCfg), baseFolder);

    //detect renamed files
    if (lastSyncState)
        DetectMovedFiles::execute(baseFolder, *lastSyncState);

    //error reporting: not any time earlier
    if (dbLoadError)
        throw* dbLoadError;
}


void fff::redetermineSyncDirection(const std::vector<DirectionConfig>& directCfgs, //throw FileError
                                   FolderComparison& folderCmp,
                                   const std::function<void(const std::wstring& msg)>& notifyStatus)
{
    if (folderCmp.empty())
        return;

    if (folderCmp.size() != directCfgs.size())
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    std::optional<FileError> dbLoadError; //defer until after default directions have been set!

    for (auto it = folderCmp.begin(); it != folderCmp.end(); ++it)
        try
        {
            redetermineSyncDirection(directCfgs[it - folderCmp.begin()], **it, notifyStatus); //throw FileError
        }
        catch (const FileError& e)
        {
            if (!dbLoadError)
                dbLoadError = e;
        }

    if (dbLoadError)
        throw* dbLoadError;
}

//---------------------------------------------------------------------------------------------------------------

namespace
{
void setSyncDirectionImpl(FilePair& file, SyncDirection newDirection)
{
    if (file.getCategory() != FILE_EQUAL)
        file.setSyncDir(newDirection);
}

void setSyncDirectionImpl(SymlinkPair& symlink, SyncDirection newDirection)
{
    if (symlink.getLinkCategory() != SYMLINK_EQUAL)
        symlink.setSyncDir(newDirection);
}

void setSyncDirectionImpl(FolderPair& folder, SyncDirection newDirection)
{
    if (folder.getDirCategory() != DIR_EQUAL)
        folder.setSyncDir(newDirection);

    for (FilePair& file : folder.refSubFiles())
        setSyncDirectionImpl(file, newDirection);
    for (SymlinkPair& link : folder.refSubLinks())
        setSyncDirectionImpl(link, newDirection);
    for (FolderPair& subFolder : folder.refSubFolders())
        setSyncDirectionImpl(subFolder, newDirection);
}
}


void fff::setSyncDirectionRec(SyncDirection newDirection, FileSystemObject& fsObj)
{
    //process subdirectories also!
    visitFSObject(fsObj,
    [&](const FolderPair&   folder) { setSyncDirectionImpl(const_cast<FolderPair& >(folder ), newDirection); },  //
    [&](const FilePair&       file) { setSyncDirectionImpl(const_cast<FilePair&   >(file   ), newDirection); },  //physical object is not const anyway
    [&](const SymlinkPair& symlink) { setSyncDirectionImpl(const_cast<SymlinkPair&>(symlink), newDirection); }); //

}

//--------------- functions related to filtering ------------------------------------------------------------------------------------

namespace
{
template <bool include>
void inOrExcludeAllRows(ContainerObject& hierObj)
{
    for (FilePair& file : hierObj.refSubFiles())
        file.setActive(include);
    for (SymlinkPair& link : hierObj.refSubLinks())
        link.setActive(include);
    for (FolderPair& folder : hierObj.refSubFolders())
    {
        folder.setActive(include);
        inOrExcludeAllRows<include>(folder); //recurse
    }
}
}


void fff::setActiveStatus(bool newStatus, FolderComparison& folderCmp)
{
    if (newStatus)
        std::for_each(begin(folderCmp), end(folderCmp), [](BaseFolderPair& baseFolder) { inOrExcludeAllRows<true>(baseFolder); }); //include all rows
    else
        std::for_each(begin(folderCmp), end(folderCmp), [](BaseFolderPair& baseFolder) { inOrExcludeAllRows<false>(baseFolder); }); //exclude all rows
}


void fff::setActiveStatus(bool newStatus, FileSystemObject& fsObj)
{
    fsObj.setActive(newStatus);

    //process subdirectories also!
    visitFSObject(fsObj, [&](const FolderPair& folder)
    {
        if (newStatus)
            inOrExcludeAllRows<true>(const_cast<FolderPair&>(folder)); //object is not physically const here anyway
        else
            inOrExcludeAllRows<false>(const_cast<FolderPair&>(folder)); //
    },
    [](const FilePair& file) {}, [](const SymlinkPair& symlink) {});
}

namespace
{
enum FilterStrategy
{
    STRATEGY_SET,
    STRATEGY_AND
    //STRATEGY_OR ->  usage of inOrExcludeAllRows doesn't allow for strategy "or"
};

template <FilterStrategy strategy> struct Eval;

template <>
struct Eval<STRATEGY_SET> //process all elements
{
    template <class T>
    static bool process(const T& obj) { return true; }
};

template <>
struct Eval<STRATEGY_AND>
{
    template <class T>
    static bool process(const T& obj) { return obj.isActive(); }
};


template <FilterStrategy strategy>
class ApplyHardFilter
{
public:
    static void execute(ContainerObject& hierObj, const PathFilter& filterProcIn) { ApplyHardFilter(hierObj, filterProcIn); }

private:
    ApplyHardFilter(ContainerObject& hierObj, const PathFilter& filterProcIn) : filterProc(filterProcIn)  { recurse(hierObj); }

    void recurse(ContainerObject& hierObj) const
    {
        for (FilePair& file : hierObj.refSubFiles())
            processFile(file);
        for (SymlinkPair& link : hierObj.refSubLinks())
            processLink(link);
        for (FolderPair& folder : hierObj.refSubFolders())
            processDir(folder);
    }

    void processFile(FilePair& file) const
    {
        if (Eval<strategy>::process(file))
            file.setActive(filterProc.passFileFilter(file.getRelativePathAny()));
    }

    void processLink(SymlinkPair& symlink) const
    {
        if (Eval<strategy>::process(symlink))
            symlink.setActive(filterProc.passFileFilter(symlink.getRelativePathAny()));
    }

    void processDir(FolderPair& folder) const
    {
        bool childItemMightMatch = true;
        const bool filterPassed = filterProc.passDirFilter(folder.getRelativePathAny(), &childItemMightMatch);

        if (Eval<strategy>::process(folder))
            folder.setActive(filterPassed);

        if (!childItemMightMatch) //use same logic like directory traversing here: evaluate filter in subdirs only if objects could match
        {
            inOrExcludeAllRows<false>(folder); //exclude all files dirs in subfolders => incompatible with STRATEGY_OR!
            return;
        }

        recurse(folder);
    }

    const PathFilter& filterProc;
};


template <FilterStrategy strategy>
class ApplySoftFilter //falsify only! -> can run directly after "hard/base filter"
{
public:
    static void execute(ContainerObject& hierObj, const SoftFilter& timeSizeFilter) { ApplySoftFilter(hierObj, timeSizeFilter); }

private:
    ApplySoftFilter(ContainerObject& hierObj, const SoftFilter& timeSizeFilter) : timeSizeFilter_(timeSizeFilter) { recurse(hierObj); }

    void recurse(fff::ContainerObject& hierObj) const
    {
        for (FilePair& file : hierObj.refSubFiles())
            processFile(file);
        for (SymlinkPair& link : hierObj.refSubLinks())
            processLink(link);
        for (FolderPair& folder : hierObj.refSubFolders())
            processDir(folder);
    }

    void processFile(FilePair& file) const
    {
        if (Eval<strategy>::process(file))
        {
            if (file.isEmpty<LEFT_SIDE>())
                file.setActive(matchSize<RIGHT_SIDE>(file) &&
                               matchTime<RIGHT_SIDE>(file));
            else if (file.isEmpty<RIGHT_SIDE>())
                file.setActive(matchSize<LEFT_SIDE>(file) &&
                               matchTime<LEFT_SIDE>(file));
            else
            {
                //the only case with partially unclear semantics:
                //file and time filters may match or not match on each side, leaving a total of 16 combinations for both sides!
                /*
                               ST S T -       ST := match size and time
                               ---------       S := match size only
                            ST |I|I|I|I|       T := match time only
                            ------------       - := no match
                             S |I|E|?|E|
                            ------------       I := include row
                             T |I|?|E|E|       E := exclude row
                            ------------       ? := unclear
                             - |I|E|E|E|
                            ------------
                */
                //let's set ? := E
                file.setActive((matchSize<RIGHT_SIDE>(file) &&
                                matchTime<RIGHT_SIDE>(file)) ||
                               (matchSize<LEFT_SIDE>(file) &&
                                matchTime<LEFT_SIDE>(file)));
            }
        }
    }

    void processLink(SymlinkPair& symlink) const
    {
        if (Eval<strategy>::process(symlink))
        {
            if (symlink.isEmpty<LEFT_SIDE>())
                symlink.setActive(matchTime<RIGHT_SIDE>(symlink));
            else if (symlink.isEmpty<RIGHT_SIDE>())
                symlink.setActive(matchTime<LEFT_SIDE>(symlink));
            else
                symlink.setActive(matchTime<RIGHT_SIDE>(symlink) ||
                                  matchTime<LEFT_SIDE> (symlink));
        }
    }

    void processDir(FolderPair& folder) const
    {
        if (Eval<strategy>::process(folder))
            folder.setActive(timeSizeFilter_.matchFolder()); //if date filter is active we deactivate all folders: effectively gets rid of empty folders!

        recurse(folder);
    }

    template <SelectedSide side, class T>
    bool matchTime(const T& obj) const
    {
        return timeSizeFilter_.matchTime(obj.template getLastWriteTime<side>());
    }

    template <SelectedSide side, class T>
    bool matchSize(const T& obj) const
    {
        return timeSizeFilter_.matchSize(obj.template getFileSize<side>());
    }

    const SoftFilter timeSizeFilter_;
};
}


void fff::addHardFiltering(BaseFolderPair& baseFolder, const Zstring& excludeFilter)
{
    ApplyHardFilter<STRATEGY_AND>::execute(baseFolder, NameFilter(FilterConfig().includeFilter, excludeFilter));
}


void fff::addSoftFiltering(BaseFolderPair& baseFolder, const SoftFilter& timeSizeFilter)
{
    if (!timeSizeFilter.isNull()) //since we use STRATEGY_AND, we may skip a "null" filter
        ApplySoftFilter<STRATEGY_AND>::execute(baseFolder, timeSizeFilter);
}


void fff::applyFiltering(FolderComparison& folderCmp, const MainConfiguration& mainCfg)
{
    if (folderCmp.empty())
        return;
    else if (folderCmp.size() != mainCfg.additionalPairs.size() + 1)
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    //merge first and additional pairs
    std::vector<LocalPairConfig> allPairs;
    allPairs.push_back(mainCfg.firstPair);
    allPairs.insert(allPairs.end(),
                    mainCfg.additionalPairs.begin(), //add additional pairs
                    mainCfg.additionalPairs.end());

    for (auto it = allPairs.begin(); it != allPairs.end(); ++it)
    {
        BaseFolderPair& baseFolder = *folderCmp[it - allPairs.begin()];

        const NormalizedFilter normFilter = normalizeFilters(mainCfg.globalFilter, it->localFilter);

        //"set" hard filter
        ApplyHardFilter<STRATEGY_SET>::execute(baseFolder, normFilter.nameFilter.ref());

        //"and" soft filter
        addSoftFiltering(baseFolder, normFilter.timeSizeFilter);
    }
}


class FilterByTimeSpan
{
public:
    static void execute(ContainerObject& hierObj, time_t timeFrom, time_t timeTo) { FilterByTimeSpan(hierObj, timeFrom, timeTo); }

private:
    FilterByTimeSpan(ContainerObject& hierObj,
                     time_t timeFrom,
                     time_t timeTo) :
        timeFrom_(timeFrom),
        timeTo_(timeTo) { recurse(hierObj); }

    void recurse(ContainerObject& hierObj) const
    {
        for (FilePair& file : hierObj.refSubFiles())
            processFile(file);
        for (SymlinkPair& link : hierObj.refSubLinks())
            processLink(link);
        for (FolderPair& folder : hierObj.refSubFolders())
            processDir(folder);
    }

    void processFile(FilePair& file) const
    {
        if (file.isEmpty<LEFT_SIDE>())
            file.setActive(matchTime<RIGHT_SIDE>(file));
        else if (file.isEmpty<RIGHT_SIDE>())
            file.setActive(matchTime<LEFT_SIDE>(file));
        else
            file.setActive(matchTime<RIGHT_SIDE>(file) ||
                           matchTime<LEFT_SIDE>(file));
    }

    void processLink(SymlinkPair& link) const
    {
        if (link.isEmpty<LEFT_SIDE>())
            link.setActive(matchTime<RIGHT_SIDE>(link));
        else if (link.isEmpty<RIGHT_SIDE>())
            link.setActive(matchTime<LEFT_SIDE>(link));
        else
            link.setActive(matchTime<RIGHT_SIDE>(link) ||
                           matchTime<LEFT_SIDE> (link));
    }

    void processDir(FolderPair& folder) const
    {
        folder.setActive(false);
        recurse(folder);
    }

    template <SelectedSide side, class T>
    bool matchTime(const T& obj) const
    {
        return timeFrom_ <= obj.template getLastWriteTime<side>() &&
               obj.template getLastWriteTime<side>() <= timeTo_;
    }

    const time_t timeFrom_;
    const time_t timeTo_;
};


void fff::applyTimeSpanFilter(FolderComparison& folderCmp, time_t timeFrom, time_t timeTo)
{
    std::for_each(begin(folderCmp), end(folderCmp), [&](BaseFolderPair& baseFolder) { FilterByTimeSpan::execute(baseFolder, timeFrom, timeTo); });
}


std::optional<PathDependency> fff::getPathDependency(const AbstractPath& basePathL, const PathFilter& filterL,
                                                     const AbstractPath& basePathR, const PathFilter& filterR)
{
    if (!AFS::isNullPath(basePathL) && !AFS::isNullPath(basePathR))
    {
        if (basePathL.afsDevice == basePathR.afsDevice)
        {
            const std::vector<Zstring> relPathL = split(basePathL.afsPath.value, FILE_NAME_SEPARATOR, SplitType::SKIP_EMPTY);
            const std::vector<Zstring> relPathR = split(basePathR.afsPath.value, FILE_NAME_SEPARATOR, SplitType::SKIP_EMPTY);

            const bool leftParent = relPathL.size() <= relPathR.size();

            const auto& relPathP = leftParent ? relPathL : relPathR;
            const auto& relPathC = leftParent ? relPathR : relPathL;

            if (std::equal(relPathP.begin(), relPathP.end(), relPathC.begin(), [](const Zstring& lhs, const Zstring& rhs) { return equalNoCase(lhs, rhs); }))
            {
                Zstring relDirPath;
                std::for_each(relPathC.begin() + relPathP.size(), relPathC.end(), [&](const Zstring& itemName)
                {
                    relDirPath = nativeAppendPaths(relDirPath, itemName);
                });
                const AbstractPath& basePathP = leftParent ? basePathL : basePathR;
                const AbstractPath& basePathC = leftParent ? basePathR : basePathL;

                const PathFilter& filterP = leftParent ? filterL : filterR;
                //if there's a dependency, check if the sub directory is (fully) excluded via filter
                //=> easy to check but still insufficient in general:
                // - one folder may have a *.txt include-filter, the other a *.lng include filter => no dependencies, but "childItemMightMatch = true" below!
                // - user may have manually excluded the conflicting items or changed the filter settings without running a re-compare
                bool childItemMightMatch = true;
                if (relDirPath.empty() || filterP.passDirFilter(relDirPath, &childItemMightMatch) || childItemMightMatch)
                    return PathDependency({ basePathP, basePathC, relDirPath });
            }
        }
    }
    return {};
}

//############################################################################################################

std::pair<std::wstring, int> fff::getSelectedItemsAsString(std::span<const FileSystemObject* const> selectionLeft,
                                                           std::span<const FileSystemObject* const> selectionRight)
{
    //don't use wxString! its dumb linear allocation strategy brings perf down to a crawl!
    std::wstring fileList; //
    int totalDelCount = 0;

    for (const FileSystemObject* fsObj : selectionLeft)
        if (!fsObj->isEmpty<LEFT_SIDE>())
        {
            fileList += AFS::getDisplayPath(fsObj->getAbstractPath<LEFT_SIDE>()) + L'\n';
            ++totalDelCount;
        }

    for (const FileSystemObject* fsObj : selectionRight)
        if (!fsObj->isEmpty<RIGHT_SIDE>())
        {
            fileList += AFS::getDisplayPath(fsObj->getAbstractPath<RIGHT_SIDE>()) + L'\n';
            ++totalDelCount;
        }

    return { fileList, totalDelCount };
}


namespace
{
template <SelectedSide side>
void copyToAlternateFolderFrom(const std::vector<const FileSystemObject*>& rowsToCopy,
                               const AbstractPath& targetFolderPath,
                               bool keepRelPaths,
                               bool overwriteIfExists,
                               ProcessCallback& callback)
{
    auto notifyItemCopy = [&](const std::wstring& statusText, const std::wstring& displayPath)
    {
        callback.reportInfo(replaceCpy(statusText, L"%x", fmtPath(displayPath))); //throw X
    };
    const std::wstring txtCreatingFile  (_("Creating file %x"         ));
    const std::wstring txtCreatingFolder(_("Creating folder %x"       ));
    const std::wstring txtCreatingLink  (_("Creating symbolic link %x"));

    auto copyItem = [&](const AbstractPath& targetPath, ItemStatReporter<>& statReporter, //throw FileError
                        const std::function<void(const std::function<void()>& deleteTargetItem)>& copyItemPlain) //throw FileError
    {
        //start deleting existing target as required by copyFileTransactional():
        //best amortized performance if "target existing" is the most common case
        std::exception_ptr deletionError;
        auto tryDeleteTargetItem = [&]
        {
            if (overwriteIfExists)
                try { AFS::removeFilePlain(targetPath); /*throw FileError*/ }
                catch (FileError&) { deletionError = std::current_exception(); } //probably "not existing" error, defer evaluation
            //else: copyFileTransactional() undefined behavior (fail/overwrite/auto-rename)
        };

        try
        {
            copyItemPlain(tryDeleteTargetItem); //throw FileError
        }
        catch (FileError&)
        {
            try
            {
                AFS::getItemType(targetPath); //throw FileError
                //already existing! =>
                if (deletionError)
                    std::rethrow_exception(deletionError);
                throw;
            }
            catch (FileError&) {} //not yet existing or access error

            if (const std::optional<AbstractPath> targetParentPath = AFS::getParentPath(targetPath))
                AFS::createFolderIfMissingRecursion(*targetParentPath); //throw FileError

            //retry:
            copyItemPlain(nullptr /*deleteTargetItem*/); //throw FileError
            return;
        }
    };

    for (const FileSystemObject* fsObj : rowsToCopy)
        tryReportingError([&]
    {
        const Zstring& relPath = keepRelPaths ? fsObj->getRelativePath<side>() : fsObj->getItemName<side>();
        const AbstractPath sourcePath = fsObj->getAbstractPath<side>();
        const AbstractPath targetPath = AFS::appendRelPath(targetFolderPath, relPath);

        visitFSObject(*fsObj, [&](const FolderPair& folder)
        {
            ItemStatReporter<> statReporter(1, 0, callback);
            notifyItemCopy(txtCreatingFolder, AFS::getDisplayPath(targetPath));

            AFS::createFolderIfMissingRecursion(targetPath); //throw FileError
            statReporter.reportDelta(1, 0);
            //folder might already exist: see creation of intermediate directories below
        },

        [&](const FilePair& file)
        {
            ItemStatReporter<> statReporter(1, file.getFileSize<side>(), callback);
            notifyItemCopy(txtCreatingFile, AFS::getDisplayPath(targetPath));

            const FileAttributes attr = file.getAttributes<side>();
            const AFS::StreamAttributes sourceAttr{ attr.modTime, attr.fileSize, attr.fileId };

            copyItem(targetPath, statReporter, [&](const std::function<void()>& deleteTargetItem) //throw FileError
            {
                auto notifyUnbufferedIO = [&](int64_t bytesDelta)

                {
                    statReporter.reportDelta(0, bytesDelta);
                    callback.requestUiRefresh(); //throw X
                };
                /*const AFS::FileCopyResult result =*/ AFS::copyFileTransactional(sourcePath, sourceAttr, targetPath, //throw FileError, ErrorFileLocked, X
                                                                                  false /*copyFilePermissions*/, true /*transactionalCopy*/, deleteTargetItem, notifyUnbufferedIO);
                //result.errorModTime? => probably irrelevant (behave like Windows Explorer)
            });
            statReporter.reportDelta(1, 0);
        },

        [&](const SymlinkPair& symlink)
        {
            ItemStatReporter<> statReporter(1, 0, callback);
            notifyItemCopy(txtCreatingLink, AFS::getDisplayPath(targetPath));

            copyItem(targetPath, statReporter, [&](const std::function<void()>& deleteTargetItem) //throw FileError
            {
                deleteTargetItem(); //throw FileError
                AFS::copySymlink(sourcePath, targetPath, false /*copyFilePermissions*/); //throw FileError
            });
            statReporter.reportDelta(1, 0);
        });

        callback.requestUiRefresh(); //throw X
    }, callback); //throw X
}
}


void fff::copyToAlternateFolder(std::span<const FileSystemObject* const> rowsToCopyOnLeft,
                                std::span<const FileSystemObject* const> rowsToCopyOnRight,
                                const Zstring& targetFolderPathPhrase,
                                bool keepRelPaths,
                                bool overwriteIfExists,
                                WarningDialogs& warnings,
                                ProcessCallback& callback)
{
    std::vector<const FileSystemObject*> itemSelectionLeft (rowsToCopyOnLeft .begin(), rowsToCopyOnLeft .end());
    std::vector<const FileSystemObject*> itemSelectionRight(rowsToCopyOnRight.begin(), rowsToCopyOnRight.end());
    eraseIf(itemSelectionLeft,  [](const FileSystemObject* fsObj) { return fsObj->isEmpty< LEFT_SIDE>(); }); //needed for correct stats!
    eraseIf(itemSelectionRight, [](const FileSystemObject* fsObj) { return fsObj->isEmpty<RIGHT_SIDE>(); }); //

    const int itemTotal = static_cast<int>(itemSelectionLeft.size() + itemSelectionRight.size());
    int64_t bytesTotal = 0;

    for (const FileSystemObject* fsObj : itemSelectionLeft)
        visitFSObject(*fsObj, [](const FolderPair& folder) {},
    [&](const FilePair& file) { bytesTotal += static_cast<int64_t>(file.getFileSize<LEFT_SIDE>()); }, [](const SymlinkPair& symlink) {});

    for (const FileSystemObject* fsObj : itemSelectionRight)
        visitFSObject(*fsObj, [](const FolderPair& folder) {},
    [&](const FilePair& file) { bytesTotal += static_cast<int64_t>(file.getFileSize<RIGHT_SIDE>()); }, [](const SymlinkPair& symlink) {});

    callback.initNewPhase(itemTotal, bytesTotal, ProcessCallback::PHASE_SYNCHRONIZING); //throw X

    //------------------------------------------------------------------------------

    const AbstractPath targetFolderPath = createAbstractPath(targetFolderPathPhrase);

    copyToAlternateFolderFrom< LEFT_SIDE>(itemSelectionLeft,  targetFolderPath, keepRelPaths, overwriteIfExists, callback);
    copyToAlternateFolderFrom<RIGHT_SIDE>(itemSelectionRight, targetFolderPath, keepRelPaths, overwriteIfExists, callback);
}

//############################################################################################################

namespace
{
template <SelectedSide side>
void deleteFromGridAndHDOneSide(std::vector<FileSystemObject*>& rowsToDelete,
                                bool useRecycleBin,
                                ProcessCallback& callback)
{
    auto notifyItemDeletion = [&](const std::wstring& statusText, const std::wstring& displayPath)
    {
        callback.reportInfo(replaceCpy(statusText, L"%x", fmtPath(displayPath))); //throw X
    };

    std::wstring txtRemovingFile;
    std::wstring txtRemovingDirectory;
    std::wstring txtRemovingSymlink;

    if (useRecycleBin)
    {
        txtRemovingFile      = _("Moving file %x to the recycle bin");
        txtRemovingDirectory = _("Moving folder %x to the recycle bin");
        txtRemovingSymlink   = _("Moving symbolic link %x to the recycle bin");
    }
    else
    {
        txtRemovingFile      = _("Deleting file %x");
        txtRemovingDirectory = _("Deleting folder %x");
        txtRemovingSymlink   = _("Deleting symbolic link %x");
    }


    for (FileSystemObject* fsObj : rowsToDelete) //all pointers are required(!) to be bound
        tryReportingError([&]
    {
        ItemStatReporter<> statReporter(1, 0, callback);

        if (!fsObj->isEmpty<side>()) //element may be implicitly deleted, e.g. if parent folder was deleted first
        {
            visitFSObject(*fsObj,
                          [&](const FolderPair& folder)
            {
                if (useRecycleBin)
                {
                    notifyItemDeletion(txtRemovingDirectory, AFS::getDisplayPath(folder.getAbstractPath<side>())); //throw X

                    AFS::recycleItemIfExists(folder.getAbstractPath<side>()); //throw FileError
                    statReporter.reportDelta(1, 0);
                }
                else
                {
                    auto onBeforeFileDeletion = [&](const std::wstring& displayPath)
                    {
                        notifyItemDeletion(txtRemovingFile, displayPath); //throw X
                        statReporter.reportDelta(1, 0);
                    };
                    auto onBeforeDirDeletion = [&](const std::wstring& displayPath)
                    {
                        notifyItemDeletion(txtRemovingDirectory, displayPath); //throw X
                        statReporter.reportDelta(1, 0);
                    };

                    AFS::removeFolderIfExistsRecursion(folder.getAbstractPath<side>(), onBeforeFileDeletion, onBeforeDirDeletion); //throw FileError
                }
            },

            [&](const FilePair& file)
            {
                notifyItemDeletion(txtRemovingFile, AFS::getDisplayPath(file.getAbstractPath<side>())); //throw X

                if (useRecycleBin)
                    AFS::recycleItemIfExists(file.getAbstractPath<side>()); //throw FileError
                else
                    AFS::removeFileIfExists(file.getAbstractPath<side>()); //throw FileError
                statReporter.reportDelta(1, 0);
            },

            [&](const SymlinkPair& symlink)
            {
                notifyItemDeletion(txtRemovingSymlink, AFS::getDisplayPath(symlink.getAbstractPath<side>())); //throw X

                if (useRecycleBin)
                    AFS::recycleItemIfExists(symlink.getAbstractPath<side>()); //throw FileError
                else
                    AFS::removeSymlinkIfExists(symlink.getAbstractPath<side>()); //throw FileError
                statReporter.reportDelta(1, 0);
            });

            fsObj->removeObject<side>(); //if directory: removes recursively!
        }

        //remain transactional as much as possible => allow for abort only *after* updating file model
        callback.requestUiRefresh(); //throw X
    }, callback); //throw X
}


template <SelectedSide side>
void categorize(const std::vector<FileSystemObject*>& rows,
                std::vector<FileSystemObject*>& deletePermanent,
                std::vector<FileSystemObject*>& deleteRecyler,
                bool useRecycleBin,
                std::map<AbstractPath, bool>& recyclerSupported,
                ProcessCallback& callback) //throw X
{
    auto hasRecycler = [&](const AbstractPath& baseFolderPath) -> bool
    {
        auto it = recyclerSupported.find(baseFolderPath); //perf: avoid duplicate checks!
        if (it != recyclerSupported.end())
            return it->second;

        const std::wstring msg = replaceCpy(_("Checking recycle bin availability for folder %x..."), L"%x", fmtPath(AFS::getDisplayPath(baseFolderPath)));

        bool recSupported = false;
        tryReportingError([&]{
            recSupported = AFS::supportsRecycleBin(baseFolderPath); //throw FileError
        }, callback); //throw X

        recyclerSupported.emplace(baseFolderPath, recSupported);
        return recSupported;
    };

    for (FileSystemObject* row : rows)
        if (!row->isEmpty<side>())
        {
            if (useRecycleBin && hasRecycler(row->base().getAbstractPath<side>())) //Windows' ::SHFileOperation() will delete permanently anyway, but we have a superior deletion routine
                deleteRecyler.push_back(row);
            else
                deletePermanent.push_back(row);
        }
}
}


void fff::deleteFromGridAndHD(const std::vector<FileSystemObject*>& rowsToDeleteOnLeft,  //refresh GUI grid after deletion to remove invalid rows
                              const std::vector<FileSystemObject*>& rowsToDeleteOnRight, //all pointers need to be bound!
                              FolderComparison& folderCmp,                         //attention: rows will be physically deleted!
                              const std::vector<DirectionConfig>& directCfgs,
                              bool useRecycleBin,
                              bool& warnRecyclerMissing,
                              ProcessCallback& callback)
{
    if (folderCmp.empty())
        return;
    else if (folderCmp.size() != directCfgs.size())
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    //build up mapping from base directory to corresponding direction config
    std::unordered_map<const BaseFolderPair*, DirectionConfig> baseFolderCfgs;
    for (auto it = folderCmp.begin(); it != folderCmp.end(); ++it)
        baseFolderCfgs[&** it] = directCfgs[it - folderCmp.begin()];

    std::vector<FileSystemObject*> deleteLeft  = rowsToDeleteOnLeft;
    std::vector<FileSystemObject*> deleteRight = rowsToDeleteOnRight;

    eraseIf(deleteLeft,  [](const FileSystemObject* fsObj) { return fsObj->isEmpty< LEFT_SIDE>(); }); //needed?
    eraseIf(deleteRight, [](const FileSystemObject* fsObj) { return fsObj->isEmpty<RIGHT_SIDE>(); }); //yes, for correct stats:

    const int itemCount = static_cast<int>(deleteLeft.size() + deleteRight.size());
    callback.initNewPhase(itemCount, 0, ProcessCallback::PHASE_SYNCHRONIZING); //throw X

    //------------------------------------------------------------------------------

    //ensure cleanup: redetermination of sync-directions and removal of invalid rows
    auto updateDirection = [&]
    {
        //update sync direction: we cannot do a full redetermination since the user may already have entered manual changes
        std::vector<FileSystemObject*> rowsToDelete;
        append(rowsToDelete, deleteLeft);
        append(rowsToDelete, deleteRight);
        removeDuplicates(rowsToDelete);

        for (auto it = rowsToDelete.begin(); it != rowsToDelete.end(); ++it)
        {
            FileSystemObject& fsObj = **it; //all pointers are required(!) to be bound

            if (fsObj.isEmpty<LEFT_SIDE>() != fsObj.isEmpty<RIGHT_SIDE>()) //make sure objects exists on one side only
            {
                auto cfgIter = baseFolderCfgs.find(&fsObj.base());
                assert(cfgIter != baseFolderCfgs.end());
                if (cfgIter != baseFolderCfgs.end())
                {
                    SyncDirection newDir = SyncDirection::NONE;

                    if (cfgIter->second.var == DirectionConfig::TWO_WAY)
                        newDir = fsObj.isEmpty<LEFT_SIDE>() ? SyncDirection::RIGHT : SyncDirection::LEFT;
                    else
                    {
                        const DirectionSet& dirCfg = extractDirections(cfgIter->second);
                        newDir = fsObj.isEmpty<LEFT_SIDE>() ? dirCfg.exRightSideOnly : dirCfg.exLeftSideOnly;
                    }

                    setSyncDirectionRec(newDir, fsObj); //set new direction (recursively)
                }
            }
        }

        //last step: cleanup empty rows: this one invalidates all pointers!
        std::for_each(begin(folderCmp), end(folderCmp), BaseFolderPair::removeEmpty);
    };
    ZEN_ON_SCOPE_EXIT(updateDirection()); //MSVC: assert is a macro and it doesn't play nice with ZEN_ON_SCOPE_EXIT, surprise... wasn't there something about macros being "evil"?

    //categorize rows into permanent deletion and recycle bin
    std::vector<FileSystemObject*> deletePermanentLeft;
    std::vector<FileSystemObject*> deletePermanentRight;
    std::vector<FileSystemObject*> deleteRecylerLeft;
    std::vector<FileSystemObject*> deleteRecylerRight;

    std::map<AbstractPath, bool> recyclerSupported;
    categorize< LEFT_SIDE>(deleteLeft,  deletePermanentLeft,  deleteRecylerLeft,  useRecycleBin, recyclerSupported, callback); //throw X
    categorize<RIGHT_SIDE>(deleteRight, deletePermanentRight, deleteRecylerRight, useRecycleBin, recyclerSupported, callback); //

    //windows: check if recycle bin really exists; if not, Windows will silently delete, which is wrong
    if (useRecycleBin &&
    std::any_of(recyclerSupported.begin(), recyclerSupported.end(), [](const auto& item) { return !item.second; }))
    {
        std::wstring msg = _("The recycle bin is not supported by the following folders. Deleted or overwritten files will not be able to be restored:") + L"\n";

        for (const auto& [folderPath, supported] : recyclerSupported)
            if (!supported)
                msg += L"\n" + AFS::getDisplayPath(folderPath);

        callback.reportWarning(msg, warnRecyclerMissing); //throw?
    }

    deleteFromGridAndHDOneSide<LEFT_SIDE>(deleteRecylerLeft,   true,  callback);
    deleteFromGridAndHDOneSide<LEFT_SIDE>(deletePermanentLeft, false, callback);

    deleteFromGridAndHDOneSide<RIGHT_SIDE>(deleteRecylerRight,   true,  callback);
    deleteFromGridAndHDOneSide<RIGHT_SIDE>(deletePermanentRight, false, callback);
}

//############################################################################################################

bool fff::operator<(const FileDescriptor& lhs, const FileDescriptor& rhs)
{
    if (lhs.attr.modTime != rhs.attr.modTime)
        return lhs.attr.modTime < rhs.attr.modTime;

    if (lhs.attr.fileSize != rhs.attr.fileSize)
        return lhs.attr.fileSize < rhs.attr.fileSize;

    if (lhs.attr.isFollowedSymlink != rhs.attr.isFollowedSymlink)
        return lhs.attr.isFollowedSymlink < rhs.attr.isFollowedSymlink;

    if (lhs.attr.fileId != rhs.attr.fileId)
        return lhs.attr.fileId < rhs.attr.fileId;

    if (!lhs.attr.fileId.empty())
        return false; //when (non-empty) file IDs match we don't have to check the path => pre-mature optimization?
    else
        return lhs.path < rhs.path;
}


TempFileBuffer::~TempFileBuffer()
{
    if (!tempFolderPath_.empty())
        try
        {
            removeDirectoryPlainRecursion(tempFolderPath_); //throw FileError
        }
        catch (FileError&) { assert(false); }
}


//returns empty if not available (item not existing, error during copy)
Zstring TempFileBuffer::getTempPath(const FileDescriptor& descr) const
{
    auto it = tempFilePaths_.find(descr);
    if (it != tempFilePaths_.end())
        return it->second;
    return Zstring();
}


void TempFileBuffer::createTempFiles(const std::set<FileDescriptor>& workLoad, ProcessCallback& callback)
{
    const int itemTotal = static_cast<int>(workLoad.size());
    int64_t bytesTotal = 0;

    for (const FileDescriptor& descr : workLoad)
        bytesTotal += descr.attr.fileSize;

    callback.initNewPhase(itemTotal, bytesTotal, ProcessCallback::PHASE_SYNCHRONIZING); //throw X

    //------------------------------------------------------------------------------

    if (tempFolderPath_.empty())
    {
        const std::wstring errMsg = tryReportingError([&]
        {
            //generate random temp folder path e.g. C:\Users\Zenju\AppData\Local\Temp\FFS-068b2e88
            Zstring tempPathTmp = appendSeparator(getTempFolderPath()); //throw FileError
            tempPathTmp += Zstr("FFS-");

            const uint32_t shortGuid = getCrc32(generateGUID()); //no need for full-blown (pseudo-)random numbers for this one-time invocation
            tempPathTmp += printNumber<Zstring>(Zstr("%08x"), static_cast<unsigned int>(shortGuid));

            createDirectoryIfMissingRecursion(tempPathTmp); //throw FileError

            tempFolderPath_ = tempPathTmp;
        }, callback); //throw X
        if (!errMsg.empty()) return;
    }

    for (const FileDescriptor& descr : workLoad)
    {
        assert(tempFilePaths_.find(descr) == tempFilePaths_.end()); //ensure correct stats, NO overwrite-copy => caller-contract!

        MemoryStreamOut<std::string> cookie; //create hash to distinguish different versions and file locations
        writeNumber   (cookie, descr.attr.modTime);
        writeNumber   (cookie, descr.attr.fileSize);
        writeContainer(cookie, descr.attr.fileId);
        writeNumber   (cookie, descr.attr.isFollowedSymlink);
        writeContainer(cookie, AFS::getInitPathPhrase(descr.path));

        const uint16_t crc16 = getCrc16(cookie.ref());
        const Zstring descrHash = printNumber<Zstring>(Zstr("%04x"), static_cast<unsigned int>(crc16));

        const Zstring fileName = AFS::getItemName(descr.path);

        auto it = findLast(fileName.begin(), fileName.end(), Zstr('.')); //gracefully handle case of missing "."
        const Zstring tempFileName = Zstring(fileName.begin(), it) + Zstr('-') + descrHash + Zstring(it, fileName.end());

        const Zstring tempFilePath = appendSeparator(tempFolderPath_) + tempFileName;
        const AFS::StreamAttributes sourceAttr{ descr.attr.modTime, descr.attr.fileSize, descr.attr.fileId };

        tryReportingError([&]
        {
            ItemStatReporter<> statReporter(1, descr.attr.fileSize, callback);

            callback.reportInfo(replaceCpy(_("Creating file %x"), L"%x", fmtPath(tempFilePath))); //throw X

            auto notifyUnbufferedIO = [&](int64_t bytesDelta)
            {
                statReporter.reportDelta(0, bytesDelta);
                callback.requestUiRefresh(); //throw X
            };
            /*const AFS::FileCopyResult result =*/ AFS::copyFileTransactional(descr.path, sourceAttr, //throw FileError, ErrorFileLocked, X
                                                                              createItemPathNative(tempFilePath),
                                                                              false /*copyFilePermissions*/, true /*transactionalCopy*/, nullptr /*onDeleteTargetFile*/, notifyUnbufferedIO);
            //result.errorModTime? => irrelevant for temp files!
            statReporter.reportDelta(1, 0);

            tempFilePaths_[descr] = tempFilePath;
        }, callback); //throw X

        callback.requestUiRefresh(); //throw X
    }
}

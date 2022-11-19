// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "algorithm.h"
#include <zen/perf.h>
#include <zen/crc.h>
#include <zen/guid.h>
#include <zen/file_access.h> //needed for TempFileBuffer only
#include "norm_filter.h"
#include "db_file.h"
#include "cmp_filetime.h"
#include "status_handler_impl.h"
#include "../afs/concrete.h"
#include "../afs/native.h"

using namespace zen;
using namespace fff;


void fff::swapGrids(const MainConfiguration& mainCfg, FolderComparison& folderCmp,
                    PhaseCallback& callback /*throw X*/) //throw X
{
    std::for_each(begin(folderCmp), end(folderCmp), [](BaseFolderPair& baseFolder) { baseFolder.flip(); });

    redetermineSyncDirection(extractDirectionCfg(folderCmp, mainCfg),
                             callback); //throw FileError
}

//----------------------------------------------------------------------------------------------

namespace
{
//visitFSObjectRecursively? nope, see premature end of traversal in processFolder()
class SetSyncDirectionByConfig
{
public:
    static void execute(const DirectionSet& dirCfgIn, ContainerObject& hierObj) { SetSyncDirectionByConfig(dirCfgIn).recurse(hierObj); }

private:
    SetSyncDirectionByConfig(const DirectionSet& dirCfgIn) : dirCfg_(dirCfgIn) {}

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
        const CompareFileResult cat = file.getCategory();

        //##################### schedule old temporary files for deletion ####################
        if (cat == FILE_LEFT_SIDE_ONLY && endsWith(file.getItemName<SelectSide::left>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::left);
        else if (cat == FILE_RIGHT_SIDE_ONLY && endsWith(file.getItemName<SelectSide::right>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::right);
        //####################################################################################

        switch (cat)
        {
            case FILE_LEFT_SIDE_ONLY:
                file.setSyncDir(dirCfg_.exLeftSideOnly);
                break;
            case FILE_RIGHT_SIDE_ONLY:
                file.setSyncDir(dirCfg_.exRightSideOnly);
                break;
            case FILE_RIGHT_NEWER:
                file.setSyncDir(dirCfg_.rightNewer);
                break;
            case FILE_LEFT_NEWER:
                file.setSyncDir(dirCfg_.leftNewer);
                break;
            case FILE_DIFFERENT_CONTENT:
                file.setSyncDir(dirCfg_.different);
                break;
            case FILE_CONFLICT:
            case FILE_DIFFERENT_METADATA: //use setting from "conflict/cannot categorize"
                if (dirCfg_.conflict == SyncDirection::none)
                    file.setSyncDirConflict(file.getCatExtraDescription()); //take over category conflict
                else
                    file.setSyncDir(dirCfg_.conflict);
                break;
            case FILE_EQUAL:
                file.setSyncDir(SyncDirection::none);
                break;
        }
    }

    void processLink(SymlinkPair& symlink) const
    {
        switch (symlink.getLinkCategory())
        {
            case SYMLINK_LEFT_SIDE_ONLY:
                symlink.setSyncDir(dirCfg_.exLeftSideOnly);
                break;
            case SYMLINK_RIGHT_SIDE_ONLY:
                symlink.setSyncDir(dirCfg_.exRightSideOnly);
                break;
            case SYMLINK_LEFT_NEWER:
                symlink.setSyncDir(dirCfg_.leftNewer);
                break;
            case SYMLINK_RIGHT_NEWER:
                symlink.setSyncDir(dirCfg_.rightNewer);
                break;
            case SYMLINK_CONFLICT:
            case SYMLINK_DIFFERENT_METADATA: //use setting from "conflict/cannot categorize"
                if (dirCfg_.conflict == SyncDirection::none)
                    symlink.setSyncDirConflict(symlink.getCatExtraDescription()); //take over category conflict
                else
                    symlink.setSyncDir(dirCfg_.conflict);
                break;
            case SYMLINK_DIFFERENT_CONTENT:
                symlink.setSyncDir(dirCfg_.different);
                break;
            case SYMLINK_EQUAL:
                symlink.setSyncDir(SyncDirection::none);
                break;
        }
    }

    void processFolder(FolderPair& folder) const
    {
        const CompareDirResult cat = folder.getDirCategory();

        //########### schedule abandoned temporary recycle bin directory for deletion  ##########
        if (cat == DIR_LEFT_SIDE_ONLY && endsWith(folder.getItemName<SelectSide::left>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::left, folder); //
        else if (cat == DIR_RIGHT_SIDE_ONLY && endsWith(folder.getItemName<SelectSide::right>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::right, folder); //don't recurse below!
        //#######################################################################################

        switch (cat)
        {
            case DIR_LEFT_SIDE_ONLY:
                folder.setSyncDir(dirCfg_.exLeftSideOnly);
                break;
            case DIR_RIGHT_SIDE_ONLY:
                folder.setSyncDir(dirCfg_.exRightSideOnly);
                break;
            case DIR_EQUAL:
                folder.setSyncDir(SyncDirection::none);
                break;
            case DIR_CONFLICT:
            case DIR_DIFFERENT_METADATA: //use setting from "conflict/cannot categorize"
                if (dirCfg_.conflict == SyncDirection::none)
                    folder.setSyncDirConflict(folder.getCatExtraDescription()); //take over category conflict
                else
                    folder.setSyncDir(dirCfg_.conflict);
                break;
        }

        recurse(folder);
    }

    const DirectionSet dirCfg_;
};

//---------------------------------------------------------------------------------------------------------------

//test if non-equal items exist in scanned data
bool allItemsCategoryEqual(const ContainerObject& hierObj)
{
    return std::all_of(hierObj.refSubFiles().begin(), hierObj.refSubFiles().end(),
    [](const FilePair& file) { return file.getCategory() == FILE_EQUAL; })&&

    std::all_of(hierObj.refSubLinks().begin(), hierObj.refSubLinks().end(),
    [](const SymlinkPair& symlink) { return symlink.getLinkCategory() == SYMLINK_EQUAL; })&&

    std::all_of(hierObj.refSubFolders().begin(), hierObj.refSubFolders().end(), [](const FolderPair& folder)
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
template <SelectSide side> inline
bool matchesDbEntry(const FilePair& file, const InSyncFile* dbFile, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    if (file.isEmpty<side>())
        return !dbFile;
    else if (!dbFile)
        return false;

    const InSyncDescrFile& descrDb = selectParam<side>(dbFile->left, dbFile->right);

    return //we're not interested in "fileTimeTolerance" here!
        sameFileTime(file.getLastWriteTime<side>(), descrDb.modTime, FAT_FILE_TIME_PRECISION_SEC, ignoreTimeShiftMinutes) &&
        file.getFileSize<side>() == dbFile->fileSize;
    //note: we do *not* consider file ID here, but are only interested in *visual* changes. Consider user moving data to some other medium, this is not a change!
}


//check whether database entry is in sync considering *current* comparison settings
inline
bool stillInSync(const InSyncFile& dbFile, CompareVariant compareVar, int fileTimeTolerance, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    switch (compareVar)
    {
        case CompareVariant::timeSize:
            if (dbFile.cmpVar == CompareVariant::content) return true; //special rule: this is certainly "good enough" for CompareVariant::timeSize!

            //case-sensitive short name match is a database invariant!
            return sameFileTime(dbFile.left.modTime, dbFile.right.modTime, fileTimeTolerance, ignoreTimeShiftMinutes);

        case CompareVariant::content:
            //case-sensitive short name match is a database invariant!
            return dbFile.cmpVar == CompareVariant::content;
        //in contrast to comparison, we don't care about modification time here!

        case CompareVariant::size: //file size/case-sensitive short name always matches on both sides for an "in-sync" database entry
            return true;
    }
    assert(false);
    return false;
}

//--------------------------------------------------------------------

//check whether database entry and current item match: *irrespective* of current comparison settings
template <SelectSide side> inline
bool matchesDbEntry(const SymlinkPair& symlink, const InSyncSymlink* dbSymlink, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    if (symlink.isEmpty<side>())
        return !dbSymlink;
    else if (!dbSymlink)
        return false;

    const InSyncDescrLink& descrDb = selectParam<side>(dbSymlink->left, dbSymlink->right);

    return sameFileTime(symlink.getLastWriteTime<side>(), descrDb.modTime, FAT_FILE_TIME_PRECISION_SEC, ignoreTimeShiftMinutes);
}


//check whether database entry is in sync considering *current* comparison settings
inline
bool stillInSync(const InSyncSymlink& dbLink, CompareVariant compareVar, int fileTimeTolerance, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    switch (compareVar)
    {
        case CompareVariant::timeSize:
            if (dbLink.cmpVar == CompareVariant::content || dbLink.cmpVar == CompareVariant::size)
                return true; //special rule: this is already "good enough" for CompareVariant::timeSize!

            //case-sensitive short name match is a database invariant!
            return sameFileTime(dbLink.left.modTime, dbLink.right.modTime, fileTimeTolerance, ignoreTimeShiftMinutes);

        case CompareVariant::content:
        case CompareVariant::size: //== categorized by content! see comparison.cpp, ComparisonBuffer::compareBySize()
            //case-sensitive short name match is a database invariant!
            return dbLink.cmpVar == CompareVariant::content || dbLink.cmpVar == CompareVariant::size;
    }
    assert(false);
    return false;
}

//--------------------------------------------------------------------

//check whether database entry and current item match: *irrespective* of current comparison settings
template <SelectSide side> inline
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

        purgeDuplicates<SelectSide::left >(filesL_,  exLeftOnlyById_);
        purgeDuplicates<SelectSide::right>(filesR_, exRightOnlyById_);

        if ((!exLeftOnlyById_ .empty() || !exLeftOnlyByPath_ .empty()) &&
            (!exRightOnlyById_.empty() || !exRightOnlyByPath_.empty()))
            detectMovePairs(dbFolder);
    }

    void recurse(ContainerObject& hierObj, const InSyncFolder* dbFolderL, const InSyncFolder* dbFolderR)
    {
        for (FilePair& file : hierObj.refSubFiles())
        {
            const AFS::FingerPrint filePrintL = file.getFilePrint<SelectSide::left >();
            const AFS::FingerPrint filePrintR = file.getFilePrint<SelectSide::right>();

            if (filePrintL != 0) filesL_.push_back(&file); //collect *all* prints for uniqueness check!
            if (filePrintR != 0) filesR_.push_back(&file); //

            auto getDbEntry = [](const InSyncFolder* dbFolder, const Zstring& fileName) -> const InSyncFile*
            {
                if (dbFolder)
                    if (const auto it = dbFolder->files.find(fileName);
                        it != dbFolder->files.end())
                        return &it->second;
                return nullptr;
            };

            if (const CompareFileResult cat = file.getCategory();
                cat == FILE_LEFT_SIDE_ONLY)
            {
                if (const InSyncFile* dbEntry = getDbEntry(dbFolderL, file.getItemName<SelectSide::left>()))
                    exLeftOnlyByPath_.emplace(dbEntry, &file);
            }
            else if (cat == FILE_RIGHT_SIDE_ONLY)
            {
                if (const InSyncFile* dbEntry = getDbEntry(dbFolderR, file.getItemName<SelectSide::right>()))
                    exRightOnlyByPath_.emplace(dbEntry, &file);
            }
        }

        for (FolderPair& folder : hierObj.refSubFolders())
        {
            auto getDbEntry = [](const InSyncFolder* dbFolder, const ZstringNorm& folderName) -> const InSyncFolder*
            {
                if (dbFolder)
                    if (const auto it = dbFolder->folders.find(folderName);
                        it != dbFolder->folders.end())
                        return &it->second;
                return nullptr;
            };
            const ZstringNorm itemNameL = folder.getItemName<SelectSide::left >();
            const ZstringNorm itemNameR = folder.getItemName<SelectSide::right>();

            const InSyncFolder* dbEntryL = getDbEntry(dbFolderL, itemNameL);
            const InSyncFolder* dbEntryR = dbFolderL == dbFolderR && itemNameL == itemNameR ?
                                           dbEntryL : getDbEntry(dbFolderR, itemNameR);

            recurse(folder, dbEntryL, dbEntryR);
        }
    }

    template <SelectSide side>
    static void purgeDuplicates(std::vector<FilePair*>& files,
                                std::unordered_map<AFS::FingerPrint, FilePair*>& exOneSideById)
    {
        if (!files.empty())
        {
            std::sort(files.begin(), files.end(), [](const FilePair* lhs, const FilePair* rhs)
            { return lhs->getFilePrint<side>() < rhs->getFilePrint<side>(); });

            AFS::FingerPrint prevPrint = files[0]->getFilePrint<side>();

            for (auto it = files.begin() + 1; it != files.end(); ++it)
                if (const AFS::FingerPrint filePrint = (*it)->getFilePrint<side>();
                    prevPrint != filePrint)
                    prevPrint = filePrint;
                else //duplicate file ID! NTFS hard link/symlink?
                {
                    const auto dupFirst = it - 1;
                    const auto dupLast = std::find_if(it + 1, files.end(), [prevPrint](const FilePair* file)
                    { return file->getFilePrint<side>() != prevPrint; });

                    //remove from model: do *not* store invalid file prints in sync.ffs_db!
                    std::for_each(dupFirst, dupLast, [](FilePair* file) { file->clearFilePrint<side>(); });
                    it = dupLast - 1;
                }

            //collect unique file prints for files existing on one side only:
            constexpr CompareFileResult oneSideOnlyTag = side == SelectSide::left ? FILE_LEFT_SIDE_ONLY : FILE_RIGHT_SIDE_ONLY;

            for (FilePair* file : files)
                if (file->getCategory() == oneSideOnlyTag)
                    if (const AFS::FingerPrint filePrint = file->getFilePrint<side>();
                        filePrint != 0) //skip duplicates marked by clearFilePrint()
                        exOneSideById.emplace(filePrint, file);
        }
    }

    void detectMovePairs(const InSyncFolder& container) const
    {
        for (const auto& [fileName, dbAttrib] : container.files)
            findAndSetMovePair(dbAttrib);

        for (const auto& [folderName, subFolder] : container.folders)
            detectMovePairs(subFolder);
    }

    template <SelectSide side>
    static bool sameSizeAndDate(const FilePair& file, const InSyncFile& dbFile)
    {
        return file.getFileSize<side>() == dbFile.fileSize &&
               file.getLastWriteTime<side>() == selectParam<side>(dbFile.left, dbFile.right).modTime;
        /* do NOT consider FAT_FILE_TIME_PRECISION_SEC:
            1. if DB contains file metadata collected during folder comparison we can be as precise as we want here
            2. if DB contains file metadata *estimated* directly after file copy:
                - most file systems store file times with sub-second precision...
                - ...except for FAT, but FAT does not have stable file IDs after file copy anyway (see comment below)
            => file time comparison with seconds precision is fine!

        PS: *never* allow a tolerance as container predicate!!
            => no strict weak ordering relation! reason: no transitivity of equivalence!          */
    }

    template <SelectSide side>
    FilePair* getAssocFilePair(const InSyncFile& dbFile) const
    {
        const std::unordered_map<const InSyncFile*, FilePair*>& exOneSideByPath = selectParam<side>(exLeftOnlyByPath_, exRightOnlyByPath_);
        const std::unordered_map<AFS::FingerPrint,  FilePair*>& exOneSideById   = selectParam<side>(exLeftOnlyById_,   exRightOnlyById_);

        if (const auto it = exOneSideByPath.find(&dbFile);
            it != exOneSideByPath.end())
            return it->second; //if there is an association by path, don't care if there is also an association by ID,
        //even if the association by path doesn't match time and size while the association by ID does!
        //there doesn't seem to be (any?) value in allowing this!

        if (const AFS::FingerPrint filePrint = selectParam<side>(dbFile.left, dbFile.right).filePrint;
            filePrint != 0)
            if (const auto it = exOneSideById.find(filePrint);
                it != exOneSideById.end())
                return it->second;

        return nullptr;
    }

    void findAndSetMovePair(const InSyncFile& dbFile) const
    {
        if (stillInSync(dbFile, cmpVar_, fileTimeTolerance_, ignoreTimeShiftMinutes_))
            if (FilePair* fileLeftOnly = getAssocFilePair<SelectSide::left>(dbFile))
                if (sameSizeAndDate<SelectSide::left>(*fileLeftOnly, dbFile))
                    if (FilePair* fileRightOnly = getAssocFilePair<SelectSide::right>(dbFile))
                        if (sameSizeAndDate<SelectSide::right>(*fileRightOnly, dbFile))
                        {
                            assert((!fileLeftOnly ->getMoveRef() &&
                                    !fileRightOnly->getMoveRef()) ||
                                   (fileLeftOnly ->getMoveRef() == fileRightOnly->getId() &&
                                    fileRightOnly->getMoveRef() == fileLeftOnly ->getId()));

                            if (fileLeftOnly ->getMoveRef() == nullptr && //needless check!? file prints are unique in this context!
                                fileRightOnly->getMoveRef() == nullptr)   //
                            {
                                fileLeftOnly ->setMoveRef(fileRightOnly->getId()); //found a pair, mark it!
                                fileRightOnly->setMoveRef(fileLeftOnly ->getId()); //
                            }
                        }
    }

    const CompareVariant cmpVar_;
    const int fileTimeTolerance_;
    const std::vector<unsigned int> ignoreTimeShiftMinutes_;

    std::vector<FilePair*> filesL_; //collection of *all* file items (with non-null filePrint)
    std::vector<FilePair*> filesR_; // => detect duplicate file IDs

    std::unordered_map<AFS::FingerPrint, FilePair*>  exLeftOnlyById_; //MSVC: twice as fast as std::map for 1 million items!
    std::unordered_map<AFS::FingerPrint, FilePair*> exRightOnlyById_;

    std::unordered_map<const InSyncFile*, FilePair*>  exLeftOnlyByPath_; //MSVC: only 4% faster than std::map for 1 million items!
    std::unordered_map<const InSyncFile*, FilePair*> exRightOnlyByPath_;

    /*  Detect Renamed Files:

         X  ->  |_|      Create right
        |_| ->   Y       Delete right

        resolve as: Rename Y to X on right

        Algorithm:
        ----------
        DB-file left  <--- (name, size, date) --->  DB-file right
              |                                          |
              |  (file ID, size, date)                   |  (file ID, size, date)
              |            or                            |            or
              |  (file path, size, date)                 |  (file path, size, date)
             \|/                                        \|/
        file left only                             file right only

       FAT caveat: file IDs are generally not stable when file is either moved or renamed!
         1. Move/rename operations on FAT cannot be detected reliably.
         2. database generally contains wrong file ID on FAT after renaming from .ffs_tmp files => correct file IDs in database only after next sync
         3. even exFAT screws up (but less than FAT) and changes IDs after file move. Did they learn nothing from the past?           */
};

//----------------------------------------------------------------------------------------------

class SetSyncDirectionsTwoWay
{
public:
    static void execute(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder) { SetSyncDirectionsTwoWay(baseFolder, dbFolder); }

private:
    SetSyncDirectionsTwoWay(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder) :
        cmpVar_                (baseFolder.getCompVariant()),
        fileTimeTolerance_     (baseFolder.getFileTimeTolerance()),
        ignoreTimeShiftMinutes_(baseFolder.getIgnoredTimeShift())
    {
        //-> considering filter not relevant:
        //  if stricter filter than last time: all ok;
        //  if less strict filter (if file ex on both sides -> conflict, fine; if file ex. on one side: copy to other side: fine)
        recurse(baseFolder, &dbFolder);
    }

    void recurse(ContainerObject& hierObj, const InSyncFolder* dbFolder) const
    {
        for (FilePair& file : hierObj.refSubFiles())
            processFile(file, dbFolder);
        for (SymlinkPair& symlink : hierObj.refSubLinks())
            processSymlink(symlink, dbFolder);
        for (FolderPair& folder : hierObj.refSubFolders())
            processDir(folder, dbFolder);
    }

    void processFile(FilePair& file, const InSyncFolder* dbFolder) const
    {
        const CompareFileResult cat = file.getCategory();
        if (cat == FILE_EQUAL)
            return;

        //##################### schedule old temporary files for deletion ####################
        if (cat == FILE_LEFT_SIDE_ONLY && endsWith(file.getItemName<SelectSide::left>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::left);
        else if (cat == FILE_RIGHT_SIDE_ONLY && endsWith(file.getItemName<SelectSide::right>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::right);
        //####################################################################################

        //try to find corresponding database entry
        auto getDbEntry = [dbFolder](const ZstringNorm& fileName) -> const InSyncFile*
        {
            if (dbFolder)
                if (auto it = dbFolder->files.find(fileName);
                    it != dbFolder->files.end())
                    return &it->second;
            return nullptr;
        };
        const ZstringNorm itemNameL = file.getItemName<SelectSide::left >();
        const ZstringNorm itemNameR = file.getItemName<SelectSide::right>();

        const InSyncFile* dbEntryL = getDbEntry(itemNameL);
        const InSyncFile* dbEntryR = itemNameL == itemNameR ? dbEntryL : getDbEntry(itemNameR);

        if (dbEntryL && dbEntryR && dbEntryL != dbEntryR) //conflict: which db entry to use?
            return file.setSyncDirConflict(txtDbAmbiguous_);

        if (const InSyncFile* dbEntry = dbEntryL ? dbEntryL : dbEntryR;
            dbEntry && !stillInSync(*dbEntry, cmpVar_, fileTimeTolerance_, ignoreTimeShiftMinutes_)) //check *before* misleadingly reporting txtNoSideChanged_
            return file.setSyncDirConflict(txtDbNotInSync_);

        const bool changeOnLeft  = !matchesDbEntry<SelectSide::left >(file, dbEntryL, ignoreTimeShiftMinutes_);
        const bool changeOnRight = !matchesDbEntry<SelectSide::right>(file, dbEntryR, ignoreTimeShiftMinutes_);

        if (changeOnLeft == changeOnRight)
            file.setSyncDirConflict(changeOnLeft ? txtBothSidesChanged_ : txtNoSideChanged_);
        else
            file.setSyncDir(changeOnLeft ? SyncDirection::right : SyncDirection::left);
    }

    void processSymlink(SymlinkPair& symlink, const InSyncFolder* dbFolder) const
    {
        const CompareSymlinkResult cat = symlink.getLinkCategory();
        if (cat == SYMLINK_EQUAL)
            return;

        //try to find corresponding database entry
        auto getDbEntry = [dbFolder](const ZstringNorm& linkName) -> const InSyncSymlink*
        {
            if (dbFolder)
                if (auto it = dbFolder->symlinks.find(linkName);
                    it != dbFolder->symlinks.end())
                    return &it->second;
            return nullptr;
        };
        const ZstringNorm itemNameL = symlink.getItemName<SelectSide::left >();
        const ZstringNorm itemNameR = symlink.getItemName<SelectSide::right>();

        const InSyncSymlink* dbEntryL = getDbEntry(itemNameL);
        const InSyncSymlink* dbEntryR = itemNameL == itemNameR ? dbEntryL : getDbEntry(itemNameR);

        if (dbEntryL && dbEntryR && dbEntryL != dbEntryR) //conflict: which db entry to use?
            return symlink.setSyncDirConflict(txtDbAmbiguous_);

        if (const InSyncSymlink* dbEntry = dbEntryL ? dbEntryL : dbEntryR;
            dbEntry && !stillInSync(*dbEntry, cmpVar_, fileTimeTolerance_, ignoreTimeShiftMinutes_))
            return symlink.setSyncDirConflict(txtDbNotInSync_);

        const bool changeOnLeft  = !matchesDbEntry<SelectSide::left >(symlink, dbEntryL, ignoreTimeShiftMinutes_);
        const bool changeOnRight = !matchesDbEntry<SelectSide::right>(symlink, dbEntryR, ignoreTimeShiftMinutes_);

        if (changeOnLeft == changeOnRight)
            symlink.setSyncDirConflict(changeOnLeft ? txtBothSidesChanged_ : txtNoSideChanged_);
        else
            symlink.setSyncDir(changeOnLeft ? SyncDirection::right : SyncDirection::left);
    }

    void processDir(FolderPair& folder, const InSyncFolder* dbFolder) const
    {
        const CompareDirResult cat = folder.getDirCategory();

        //########### schedule abandoned temporary recycle bin directory for deletion  ##########
        if (cat == DIR_LEFT_SIDE_ONLY && endsWith(folder.getItemName<SelectSide::left>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::left, folder); //
        else if (cat == DIR_RIGHT_SIDE_ONLY && endsWith(folder.getItemName<SelectSide::right>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::right, folder); //don't recurse below!
        //#######################################################################################

        //try to find corresponding database entry
        auto getDbEntry = [dbFolder](const ZstringNorm& folderName) -> const InSyncFolder*
        {
            if (dbFolder)
                if (auto it = dbFolder->folders.find(folderName);
                    it != dbFolder->folders.end())
                    return &it->second;
            return nullptr;
        };

        const ZstringNorm itemNameL = folder.getItemName<SelectSide::left >();
        const ZstringNorm itemNameR = folder.getItemName<SelectSide::right>();

        const InSyncFolder* dbEntryL = getDbEntry(itemNameL);
        const InSyncFolder* dbEntryR = itemNameL == itemNameR ? dbEntryL : getDbEntry(itemNameR);

        if (dbEntryL && dbEntryR && dbEntryL != dbEntryR) //conflict: which db entry to use?
        {
            auto onFsItem = [&](FileSystemObject& fsObj)
            {
                if (fsObj.getCategory() != FILE_EQUAL)
                    fsObj.setSyncDirConflict(txtDbAmbiguous_);
            };
            return visitFSObjectRecursively(static_cast<FileSystemObject&>(folder), onFsItem, onFsItem, onFsItem);
        }
        const InSyncFolder* dbEntry = dbEntryL ? dbEntryL : dbEntryR; //exactly one side nullptr? => change in upper/lower case!

        if (cat != DIR_EQUAL)
        {
            if (dbEntry && !stillInSync(*dbEntry))
                folder.setSyncDirConflict(txtDbNotInSync_);
            else
            {
                const bool changeOnLeft  = !matchesDbEntry<SelectSide::left >(folder, dbEntryL);
                const bool changeOnRight = !matchesDbEntry<SelectSide::right>(folder, dbEntryR);

                if (changeOnLeft == changeOnRight)
                    folder.setSyncDirConflict(changeOnLeft ? txtBothSidesChanged_ : txtNoSideChanged_);
                else
                    folder.setSyncDir(changeOnLeft ? SyncDirection::right : SyncDirection::left);
            }
        }

        recurse(folder, dbEntry);
    }

    //need ref-counted strings! see FileSystemObject::syncDirectionConflict_
    const Zstringc txtBothSidesChanged_ = utfTo<Zstringc>(_("Both sides have changed since last synchronization."));
    const Zstringc txtNoSideChanged_    = utfTo<Zstringc>(_("Cannot determine sync-direction:") + L'\n' + TAB_SPACE + _("No change since last synchronization."));
    const Zstringc txtDbNotInSync_      = utfTo<Zstringc>(_("Cannot determine sync-direction:") + L'\n' + TAB_SPACE + _("The database entry is not in sync considering current settings."));
    const Zstringc txtDbAmbiguous_      = utfTo<Zstringc>(_("Cannot determine sync-direction:") + L'\n' + TAB_SPACE + _("The database entry is ambiguous."));

    const CompareVariant cmpVar_;
    const int fileTimeTolerance_;
    const std::vector<unsigned int> ignoreTimeShiftMinutes_;
};
}


std::vector<std::pair<BaseFolderPair*, SyncDirectionConfig>> fff::extractDirectionCfg(FolderComparison& folderCmp, const MainConfiguration& mainCfg)
{
    if (folderCmp.empty())
        return {};

    //merge first and additional pairs
    std::vector<LocalPairConfig> allPairs;
    allPairs.push_back(mainCfg.firstPair);
    allPairs.insert(allPairs.end(),
                    mainCfg.additionalPairs.begin(), //add additional pairs
                    mainCfg.additionalPairs.end());

    if (folderCmp.size() != allPairs.size())
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));

    std::vector<std::pair<BaseFolderPair*, SyncDirectionConfig>> output;

    for (auto it = folderCmp.begin(); it != folderCmp.end(); ++it)
    {
        BaseFolderPair& baseFolder = **it;
        const LocalPairConfig& lpc = allPairs[it - folderCmp.begin()];

        output.emplace_back(&baseFolder, lpc.localSyncCfg ? lpc.localSyncCfg->directionCfg : mainCfg.syncCfg.directionCfg);
    }
    return output;
}


void fff::redetermineSyncDirection(const std::vector<std::pair<BaseFolderPair*, SyncDirectionConfig>>& directCfgs,
                                   PhaseCallback& callback /*throw X*/) //throw X
{
    if (directCfgs.empty())
        return;

    std::unordered_set<const BaseFolderPair*> allEqualPairs;
    std::unordered_map<const BaseFolderPair*, SharedRef<const InSyncFolder>> lastSyncStates;

    //best effort: always set sync directions (even on DB load error and when user cancels during file loading)
    ZEN_ON_SCOPE_EXIT
    (
        //*INDENT-OFF*
        for (const auto& [baseFolder, dirCfg] : directCfgs)
            if (!allEqualPairs.contains(baseFolder))
            {
                auto it = lastSyncStates.find(baseFolder);
                const InSyncFolder* lastSyncState = it != lastSyncStates.end() ? &it->second.ref() : nullptr;

                //set sync directions
                if (dirCfg.var == SyncVariant::twoWay)
                {
                    if (lastSyncState)
                        SetSyncDirectionsTwoWay::execute(*baseFolder, *lastSyncState);
                    else //default fallback
                    {
                        std::wstring msg = _("Setting directions for first synchronization: Old files will be overwritten with newer files.");
                        if (directCfgs.size() > 1)
                            msg += L'\n' + AFS::getDisplayPath(baseFolder->getAbstractPath<SelectSide::left >()) + L' ' + getVariantNameWithSymbol(dirCfg.var) + L' ' +
                                          AFS::getDisplayPath(baseFolder->getAbstractPath<SelectSide::right>());

                        try { callback.logMessage(msg, PhaseCallback::MsgType::warning); /*throw X*/} catch (...) {};

                        SetSyncDirectionByConfig::execute(getTwoWayUpdateSet(), *baseFolder);
                    }
                }
                else
                    SetSyncDirectionByConfig::execute(extractDirections(dirCfg), *baseFolder);

                //detect renamed files
                if (lastSyncState)
                    DetectMovedFiles::execute(*baseFolder, *lastSyncState);
            }
        //*INDENT-ON*
    );

    std::vector<const BaseFolderPair*> baseFoldersForDbLoad;
    for (const auto& [baseFolder, dirCfg] : directCfgs)
        if (dirCfg.var == SyncVariant::twoWay || detectMovedFilesEnabled(dirCfg))
        {
            if (allItemsCategoryEqual(*baseFolder)) //nothing to do: don't even try to open DB files
                allEqualPairs.insert(baseFolder);
            else
                baseFoldersForDbLoad.push_back(baseFolder);
        }

    //(try to) load sync-database files
    lastSyncStates = loadLastSynchronousState(baseFoldersForDbLoad,
                                              callback /*throw X*/); //throw X

    callback.updateStatus(_("Calculating sync directions...")); //throw X
    callback.requestUiUpdate(true /*force*/); //throw X
}

//---------------------------------------------------------------------------------------------------------------

void fff::setSyncDirectionRec(SyncDirection newDirection, FileSystemObject& fsObj)
{
    auto onFsItem = [newDirection](FileSystemObject& fsObj2)
    {
        if (fsObj2.getCategory() != FILE_EQUAL)
            fsObj2.setSyncDir(newDirection);
    };
    visitFSObjectRecursively(fsObj, onFsItem, onFsItem, onFsItem);
}

//--------------- functions related to filtering ------------------------------------------------------------------------------------

void fff::setActiveStatus(bool newStatus, FolderComparison& folderCmp)
{
    auto onFsItem = [newStatus](FileSystemObject& fsObj) { fsObj.setActive(newStatus); };

    std::for_each(begin(folderCmp), end(folderCmp), [onFsItem](BaseFolderPair& baseFolder)
    {
        visitFSObjectRecursively(baseFolder, onFsItem, onFsItem, onFsItem);
    });
}


void fff::setActiveStatus(bool newStatus, FileSystemObject& fsObj)
{
    auto onFsItem = [newStatus](FileSystemObject& fsObj2) { fsObj2.setActive(newStatus); };

    visitFSObjectRecursively(fsObj, onFsItem, onFsItem, onFsItem);
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
        for (SymlinkPair& symlink : hierObj.refSubLinks())
            processLink(symlink);
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
            //exclude all files dirs in subfolders => incompatible with STRATEGY_OR!
            auto onFsItem = [](FileSystemObject& fsObj) { fsObj.setActive(false); };
            visitFSObjectRecursively(static_cast<ContainerObject&>(folder), onFsItem, onFsItem, onFsItem);
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
        for (SymlinkPair& symlink : hierObj.refSubLinks())
            processLink(symlink);
        for (FolderPair& folder : hierObj.refSubFolders())
            processDir(folder);
    }

    void processFile(FilePair& file) const
    {
        if (Eval<strategy>::process(file))
        {
            if (file.isEmpty<SelectSide::left>())
                file.setActive(matchSize<SelectSide::right>(file) &&
                               matchTime<SelectSide::right>(file));
            else if (file.isEmpty<SelectSide::right>())
                file.setActive(matchSize<SelectSide::left>(file) &&
                               matchTime<SelectSide::left>(file));
            else
                /* the only case with partially unclear semantics:
                   file and time filters may match or not match on each side, leaving a total of 16 combinations for both sides!

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
                  let's set ? := E                                          */
                file.setActive((matchSize<SelectSide::right>(file) &&
                                matchTime<SelectSide::right>(file)) ||
                               (matchSize<SelectSide::left>(file) &&
                                matchTime<SelectSide::left>(file)));
        }
    }

    void processLink(SymlinkPair& symlink) const
    {
        if (Eval<strategy>::process(symlink))
        {
            if (symlink.isEmpty<SelectSide::left>())
                symlink.setActive(matchTime<SelectSide::right>(symlink));
            else if (symlink.isEmpty<SelectSide::right>())
                symlink.setActive(matchTime<SelectSide::left>(symlink));
            else
                symlink.setActive(matchTime<SelectSide::right>(symlink) ||
                                  matchTime<SelectSide::left> (symlink));
        }
    }

    void processDir(FolderPair& folder) const
    {
        if (Eval<strategy>::process(folder))
            folder.setActive(timeSizeFilter_.matchFolder()); //if date filter is active we deactivate all folders: effectively gets rid of empty folders!

        recurse(folder);
    }

    template <SelectSide side, class T>
    bool matchTime(const T& obj) const
    {
        return timeSizeFilter_.matchTime(obj.template getLastWriteTime<side>());
    }

    template <SelectSide side, class T>
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
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));

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


namespace
{
template <SelectSide side, class T> inline
bool matchesTime(const T& obj, time_t timeFrom, time_t timeTo)
{
    return timeFrom <= obj.template getLastWriteTime<side>() &&
           /**/        obj.template getLastWriteTime<side>() <= timeTo;
}
}


void fff::applyTimeSpanFilter(FolderComparison& folderCmp, time_t timeFrom, time_t timeTo)
{
    std::for_each(begin(folderCmp), end(folderCmp), [timeFrom, timeTo](BaseFolderPair& baseFolder)
    {
        visitFSObjectRecursively(baseFolder, [](FolderPair& folder) { folder.setActive(false); },

        [timeFrom, timeTo](FilePair& file)
        {
            if (file.isEmpty<SelectSide::left>())
                file.setActive(matchesTime<SelectSide::right>(file, timeFrom, timeTo));
            else if (file.isEmpty<SelectSide::right>())
                file.setActive(matchesTime<SelectSide::left>(file, timeFrom, timeTo));
            else
                file.setActive(matchesTime<SelectSide::right>(file, timeFrom, timeTo) ||
                               matchesTime<SelectSide::left>(file, timeFrom, timeTo));
        },

        [timeFrom, timeTo](SymlinkPair& symlink)
        {
            if (symlink.isEmpty<SelectSide::left>())
                symlink.setActive(matchesTime<SelectSide::right>(symlink, timeFrom, timeTo));
            else if (symlink.isEmpty<SelectSide::right>())
                symlink.setActive(matchesTime<SelectSide::left>(symlink, timeFrom, timeTo));
            else
                symlink.setActive(matchesTime<SelectSide::right>(symlink, timeFrom, timeTo) ||
                                  matchesTime<SelectSide::left> (symlink, timeFrom, timeTo));
        });
    });
}


std::optional<PathDependency> fff::getPathDependency(const AbstractPath& folderPathL, const PathFilter& filterL,
                                                     const AbstractPath& folderPathR, const PathFilter& filterR)
{
    if (!AFS::isNullPath(folderPathL) && !AFS::isNullPath(folderPathR))
    {
        if (folderPathL.afsDevice == folderPathR.afsDevice)
        {
            const std::vector<Zstring> relPathL = splitCpy(folderPathL.afsPath.value, FILE_NAME_SEPARATOR, SplitOnEmpty::skip);
            const std::vector<Zstring> relPathR = splitCpy(folderPathR.afsPath.value, FILE_NAME_SEPARATOR, SplitOnEmpty::skip);

            const bool leftParent = relPathL.size() <= relPathR.size();

            const auto& relPathP = leftParent ? relPathL : relPathR;
            const auto& relPathC = leftParent ? relPathR : relPathL;

            if (std::equal(relPathP.begin(), relPathP.end(), relPathC.begin(), [](const Zstring& lhs, const Zstring& rhs) { return equalNoCase(lhs, rhs); }))
            {
                Zstring relDirPath;
                std::for_each(relPathC.begin() + relPathP.size(), relPathC.end(), [&](const Zstring& itemName)
                {
                    relDirPath = appendPath(relDirPath, itemName);
                });

                const PathFilter& filterP = leftParent ? filterL : filterR;
                //if there's a dependency, check if the sub directory is (fully) excluded via filter
                //=> easy to check but still insufficient in general:
                // - one folder may have a *.txt include-filter, the other a *.lng include filter => no dependencies, but "childItemMightMatch = true" below!
                // - user may have manually excluded the conflicting items or changed the filter settings without running a re-compare
                bool childItemMightMatch = true;
                if (relDirPath.empty() || filterP.passDirFilter(relDirPath, &childItemMightMatch) || childItemMightMatch)
                    return PathDependency{leftParent ? folderPathL : folderPathR, relDirPath};
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
        if (!fsObj->isEmpty<SelectSide::left>())
        {
            fileList += AFS::getDisplayPath(fsObj->getAbstractPath<SelectSide::left>()) + L'\n';
            ++totalDelCount;
        }

    for (const FileSystemObject* fsObj : selectionRight)
        if (!fsObj->isEmpty<SelectSide::right>())
        {
            fileList += AFS::getDisplayPath(fsObj->getAbstractPath<SelectSide::right>()) + L'\n';
            ++totalDelCount;
        }

    return {fileList, totalDelCount};
}


namespace
{
template <SelectSide side>
void copyToAlternateFolderFrom(const std::vector<const FileSystemObject*>& rowsToCopy,
                               const AbstractPath& targetFolderPath,
                               bool keepRelPaths,
                               bool overwriteIfExists,
                               ProcessCallback& callback /*throw X*/) //throw X
{
    auto reportItemInfo = [&](const std::wstring& msgTemplate, const AbstractPath& itemPath) //throw X
    {
        reportInfo(replaceCpy(msgTemplate, L"%x", fmtPath(AFS::getDisplayPath(itemPath))), callback); //throw X
    };
    const std::wstring txtCreatingFile  (_("Creating file %x"         ));
    const std::wstring txtCreatingFolder(_("Creating folder %x"       ));
    const std::wstring txtCreatingLink  (_("Creating symbolic link %x"));

    auto copyItem = [&](const AbstractPath& targetPath, //throw FileError
                        const std::function<void(const std::function<void()>& deleteTargetItem)>& copyItemPlain) //throw FileError
    {
        //start deleting existing target as required by copyFileTransactional():
        //best amortized performance if "already existing" is the most common case
        std::exception_ptr deletionError;
        auto tryDeleteTargetItem = [&]
        {
            if (overwriteIfExists)
                try { AFS::removeFilePlain(targetPath); /*throw FileError*/ }
                catch (FileError&) { deletionError = std::current_exception(); } //probably "not existing" error, defer evaluation
            //else: copyFileTransactional() => undefined behavior! (e.g. fail/overwrite/auto-rename)
        };

        try
        {
            copyItemPlain(tryDeleteTargetItem); //throw FileError
        }
        catch (FileError&)
        {
            bool alreadyExisting = false;
            try
            {
                AFS::getItemType(targetPath); //throw FileError
                alreadyExisting = true;
            }
            catch (FileError&) {} //=> not yet existing (=> fine, no path issue) or access error:
            //- let's pretend it doesn't happen :> if it does, worst case: the retry fails with (useless) already existing error
            //- itemStillExists()? too expensive, considering that "already existing" is the most common case

            if (alreadyExisting)
            {
                if (deletionError)
                    std::rethrow_exception(deletionError);
                throw;
            }

            //parent folder missing  => create + retry
            //parent folder existing (maybe externally created shortly after copy attempt) => retry
            if (const std::optional<AbstractPath>& targetParentPath = AFS::getParentPath(targetPath))
                AFS::createFolderIfMissingRecursion(*targetParentPath); //throw FileError

            //retry:
            copyItemPlain(nullptr /*deleteTargetItem*/); //throw FileError
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
            ItemStatReporter statReporter(1, 0, callback);
            reportItemInfo(txtCreatingFolder, targetPath); //throw X

            AFS::createFolderIfMissingRecursion(targetPath); //throw FileError
            statReporter.reportDelta(1, 0);
            //folder might already exist: see creation of intermediate directories below
        },

        [&](const FilePair& file)
        {
            ItemStatReporter statReporter(1, file.getFileSize<side>(), callback);
            reportItemInfo(txtCreatingFile, targetPath); //throw X

            std::wstring statusMsg = replaceCpy(txtCreatingFile, L"%x", fmtPath(AFS::getDisplayPath(targetPath)));
            PercentStatReporter percentReporter(statusMsg, file.getFileSize<side>(), statReporter);

            const FileAttributes attr = file.getAttributes<side>();
            const AFS::StreamAttributes sourceAttr{attr.modTime, attr.fileSize, attr.filePrint};

            copyItem(targetPath, [&](const std::function<void()>& deleteTargetItem) //throw FileError
            {
                //already existing + !overwriteIfExists: undefined behavior! (e.g. fail/overwrite/auto-rename)
                const AFS::FileCopyResult result = AFS::copyFileTransactional(sourcePath, sourceAttr, targetPath, //throw FileError, ErrorFileLocked, X
                                                                              false /*copyFilePermissions*/, true /*transactionalCopy*/, deleteTargetItem,
                                                                              [&](int64_t bytesDelta)
                {
                    percentReporter.updateDeltaAndStatus(bytesDelta); //throw X
                    callback.requestUiUpdate(); //throw X  => not reliably covered by PercentStatReporter::updateDeltaAndStatus()! e.g. during first few seconds: STATUS_PERCENT_DELAY!
                });

                if (result.errorModTime) //log only; no popup
                    callback.logMessage(result.errorModTime->toString(), PhaseCallback::MsgType::warning);
            });
            statReporter.reportDelta(1, 0);
        },

        [&](const SymlinkPair& symlink)
        {
            ItemStatReporter statReporter(1, 0, callback);
            reportItemInfo(txtCreatingLink, targetPath); //throw X

            copyItem(targetPath, [&](const std::function<void()>& deleteTargetItem) //throw FileError
            {
                deleteTargetItem(); //throw FileError
                AFS::copySymlink(sourcePath, targetPath, false /*copyFilePermissions*/); //throw FileError
            });
            statReporter.reportDelta(1, 0);
        });
    }, callback); //throw X
}
}


void fff::copyToAlternateFolder(std::span<const FileSystemObject* const> rowsToCopyOnLeft,
                                std::span<const FileSystemObject* const> rowsToCopyOnRight,
                                const Zstring& targetFolderPathPhrase,
                                bool keepRelPaths,
                                bool overwriteIfExists,
                                WarningDialogs& warnings,
                                ProcessCallback& callback /*throw X*/) //throw X
{
    std::vector<const FileSystemObject*> itemSelectionLeft (rowsToCopyOnLeft .begin(), rowsToCopyOnLeft .end());
    std::vector<const FileSystemObject*> itemSelectionRight(rowsToCopyOnRight.begin(), rowsToCopyOnRight.end());
    std::erase_if(itemSelectionLeft,  [](const FileSystemObject* fsObj) { return fsObj->isEmpty<SelectSide::left >(); }); //needed for correct stats!
    std::erase_if(itemSelectionRight, [](const FileSystemObject* fsObj) { return fsObj->isEmpty<SelectSide::right>(); }); //

    const int itemTotal = static_cast<int>(itemSelectionLeft.size() + itemSelectionRight.size());
    int64_t bytesTotal = 0;

    for (const FileSystemObject* fsObj : itemSelectionLeft)
        visitFSObject(*fsObj, [](const FolderPair& folder) {},
    [&](const FilePair& file) { bytesTotal += static_cast<int64_t>(file.getFileSize<SelectSide::left>()); }, [](const SymlinkPair& symlink) {});

    for (const FileSystemObject* fsObj : itemSelectionRight)
        visitFSObject(*fsObj, [](const FolderPair& folder) {},
    [&](const FilePair& file) { bytesTotal += static_cast<int64_t>(file.getFileSize<SelectSide::right>()); }, [](const SymlinkPair& symlink) {});

    callback.initNewPhase(itemTotal, bytesTotal, ProcessPhase::none); //throw X

    //------------------------------------------------------------------------------

    const AbstractPath targetFolderPath = createAbstractPath(targetFolderPathPhrase);

    copyToAlternateFolderFrom<SelectSide::left >(itemSelectionLeft,  targetFolderPath, keepRelPaths, overwriteIfExists, callback);
    copyToAlternateFolderFrom<SelectSide::right>(itemSelectionRight, targetFolderPath, keepRelPaths, overwriteIfExists, callback);
}

//############################################################################################################

namespace
{
template <SelectSide side>
void deleteFromGridAndHDOneSide(std::vector<FileSystemObject*>& rowsToDelete,
                                bool moveToRecycler,
                                bool& recyclerMissingReportOnce,
                                bool& warnRecyclerMissing, //WarningDialogs::warnRecyclerMissing
                                PhaseCallback& callback /*throw X*/) //throw X
{
    const std::wstring txtDelFilePermanent_ = _("Deleting file %x");
    const std::wstring txtDelFileRecycler_  = _("Moving file %x to the recycle bin");

    const std::wstring txtDelSymlinkPermanent_ = _("Deleting symbolic link %x");
    const std::wstring txtDelSymlinkRecycler_  = _("Moving symbolic link %x to the recycle bin");

    const std::wstring txtDelFolderPermanent_ = _("Deleting folder %x");
    const std::wstring txtDelFolderRecycler_  = _("Moving folder %x to the recycle bin");

    for (FileSystemObject* fsObj : rowsToDelete) //all pointers are required(!) to be bound
        tryReportingError([&]
    {
        ItemStatReporter statReporter(1, 0, callback);

        if (!fsObj->isEmpty<side>()) //element may be implicitly deleted, e.g. if parent folder was deleted first
        {
            visitFSObject(*fsObj, [&](const FolderPair& folder)
            {
                auto removeFolderPermanently = [&]
                {
                    auto notifyDeletion = [&](const std::wstring& msgTemplate, const std::wstring& displayPath)
                    {
                        reportInfo(replaceCpy(msgTemplate, L"%x", fmtPath(displayPath)), statReporter); //throw X
                        statReporter.reportDelta(1, 0); //it would be more correct to report *after* work was done!
                    };

                    auto onBeforeFileDeletion = [&](const std::wstring& displayPath) { notifyDeletion(txtDelFilePermanent_,   displayPath); };
                    auto onBeforeDirDeletion  = [&](const std::wstring& displayPath) { notifyDeletion(txtDelFolderPermanent_, displayPath); };
                    AFS::removeFolderIfExistsRecursion(folder.getAbstractPath<side>(), onBeforeFileDeletion, onBeforeDirDeletion); //throw FileError
                };

                if (moveToRecycler)
                    try
                    {
                        reportInfo(replaceCpy(txtDelFolderRecycler_, L"%x", fmtPath(AFS::getDisplayPath(folder.getAbstractPath<side>()))), statReporter); //throw X
                        AFS::moveToRecycleBinIfExists(folder.getAbstractPath<side>()); //throw FileError, RecycleBinUnavailable
                        statReporter.reportDelta(1, 0);
                    }
                    catch (const RecycleBinUnavailable& e)
                    {
                        if (!recyclerMissingReportOnce)
                        {
                            recyclerMissingReportOnce = true;
                            callback.reportWarning(e.toString() + L"\n\n" + _("Ignore and delete permanently each time recycle bin is unavailable?"), warnRecyclerMissing); //throw X
                        }
                        callback.logMessage(replaceCpy(txtDelFolderPermanent_, L"%x", fmtPath(AFS::getDisplayPath(folder.getAbstractPath<side>()))) +
                                            L" [" + _("The recycle bin is not available") + L']', PhaseCallback::MsgType::warning); //throw X
                        removeFolderPermanently(); //throw FileError, X
                    }
                else
                {
                    reportInfo(replaceCpy(txtDelFolderPermanent_, L"%x", fmtPath(AFS::getDisplayPath(folder.getAbstractPath<side>()))), statReporter); //throw X
                    removeFolderPermanently(); //throw FileError, X
                }
            },

            [&](const FilePair& file)
            {
                if (moveToRecycler)
                    try
                    {
                        reportInfo(replaceCpy(txtDelFileRecycler_, L"%x", fmtPath(AFS::getDisplayPath(file.getAbstractPath<side>()))), statReporter); //throw X
                        AFS::moveToRecycleBinIfExists(file.getAbstractPath<side>()); //throw FileError, RecycleBinUnavailable
                    }
                    catch (const RecycleBinUnavailable& e)
                    {
                        if (!recyclerMissingReportOnce)
                        {
                            recyclerMissingReportOnce = true;
                            callback.reportWarning(e.toString() + L"\n\n" + _("Ignore and delete permanently each time recycle bin is unavailable?"), warnRecyclerMissing); //throw X
                        }
                        callback.logMessage(replaceCpy(txtDelFilePermanent_, L"%x", fmtPath(AFS::getDisplayPath(file.getAbstractPath<side>()))) +
                                            L" [" + _("The recycle bin is not available") + L']', PhaseCallback::MsgType::warning); //throw X
                        AFS::removeFileIfExists(file.getAbstractPath<side>()); //throw FileError
                    }
                else
                {
                    reportInfo(replaceCpy(txtDelFilePermanent_, L"%x", fmtPath(AFS::getDisplayPath(file.getAbstractPath<side>()))), statReporter); //throw X
                    AFS::removeFileIfExists(file.getAbstractPath<side>()); //throw FileError
                }
                statReporter.reportDelta(1, 0);
            },

            [&](const SymlinkPair& symlink)
            {
                if (moveToRecycler)
                    try
                    {
                        reportInfo(replaceCpy(txtDelSymlinkRecycler_, L"%x", fmtPath(AFS::getDisplayPath(symlink.getAbstractPath<side>()))), statReporter); //throw X
                        AFS::moveToRecycleBinIfExists(symlink.getAbstractPath<side>()); //throw FileError, RecycleBinUnavailable
                    }
                    catch (const RecycleBinUnavailable& e)
                    {
                        if (!recyclerMissingReportOnce)
                        {
                            recyclerMissingReportOnce = true;
                            callback.reportWarning(e.toString() + L"\n\n" + _("Ignore and delete permanently each time recycle bin is unavailable?"), warnRecyclerMissing); //throw X
                        }
                        callback.logMessage(replaceCpy(txtDelSymlinkPermanent_, L"%x", fmtPath(AFS::getDisplayPath(symlink.getAbstractPath<side>()))) +
                                            L" [" + _("The recycle bin is not available") + L']', PhaseCallback::MsgType::warning); //throw X
                        AFS::removeSymlinkIfExists(symlink.getAbstractPath<side>()); //throw FileError
                    }
                else
                {
                    reportInfo(replaceCpy(txtDelSymlinkPermanent_, L"%x", fmtPath(AFS::getDisplayPath(symlink.getAbstractPath<side>()))), statReporter); //throw X
                    AFS::removeSymlinkIfExists(symlink.getAbstractPath<side>()); //throw FileError
                }
                statReporter.reportDelta(1, 0);
            });

            fsObj->removeObject<side>(); //if directory: removes recursively!
        }
    }, callback); //throw X
}
}


void fff::deleteFromGridAndHD(const std::vector<FileSystemObject*>& rowsToDeleteOnLeft,  //refresh GUI grid after deletion to remove invalid rows
                              const std::vector<FileSystemObject*>& rowsToDeleteOnRight, //all pointers need to be bound!
                              const std::vector<std::pair<BaseFolderPair*, SyncDirectionConfig>>& directCfgs, //attention: rows will be physically deleted!
                              bool moveToRecycler,
                              bool& warnRecyclerMissing,
                              ProcessCallback& callback /*throw X*/) //throw X
{
    if (directCfgs.empty())
        return;

    //build up mapping from base directory to corresponding direction config
    std::unordered_map<const BaseFolderPair*, SyncDirectionConfig> baseFolderCfgs;
    for (const auto& [baseFolder, dirCfg] : directCfgs)
        baseFolderCfgs[baseFolder] = dirCfg;

    std::vector<FileSystemObject*> deleteLeft  = rowsToDeleteOnLeft;
    std::vector<FileSystemObject*> deleteRight = rowsToDeleteOnRight;

    std::erase_if(deleteLeft,  [](const FileSystemObject* fsObj) { return fsObj->isEmpty<SelectSide::left >(); }); //needed?
    std::erase_if(deleteRight, [](const FileSystemObject* fsObj) { return fsObj->isEmpty<SelectSide::right>(); }); //yes, for correct stats:

    const int itemCount = static_cast<int>(deleteLeft.size() + deleteRight.size());
    callback.initNewPhase(itemCount, 0, ProcessPhase::none); //throw X

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

            if (fsObj.isEmpty<SelectSide::left>() != fsObj.isEmpty<SelectSide::right>()) //make sure objects exists on one side only
            {
                auto cfgIter = baseFolderCfgs.find(&fsObj.base());
                assert(cfgIter != baseFolderCfgs.end());
                if (cfgIter != baseFolderCfgs.end())
                {
                    SyncDirection newDir = SyncDirection::none;

                    if (cfgIter->second.var == SyncVariant::twoWay)
                        newDir = fsObj.isEmpty<SelectSide::left>() ? SyncDirection::right : SyncDirection::left;
                    else
                    {
                        const DirectionSet& dirCfg = extractDirections(cfgIter->second);
                        newDir = fsObj.isEmpty<SelectSide::left>() ? dirCfg.exRightSideOnly : dirCfg.exLeftSideOnly;
                    }

                    setSyncDirectionRec(newDir, fsObj); //set new direction (recursively)
                }
            }
        }

        //last step: cleanup empty rows: this one invalidates all pointers!
        for (const auto& [baseFolder, dirCfg] : directCfgs)
            BaseFolderPair::removeEmpty(*baseFolder);
    };
    ZEN_ON_SCOPE_EXIT(updateDirection()); //MSVC: assert is a macro and it doesn't play nice with ZEN_ON_SCOPE_EXIT, surprise... wasn't there something about macros being "evil"?

    bool recyclerMissingReportOnce = false;
    deleteFromGridAndHDOneSide<SelectSide::left >(deleteLeft,  moveToRecycler, recyclerMissingReportOnce, warnRecyclerMissing, callback); //throw X
    deleteFromGridAndHDOneSide<SelectSide::right>(deleteRight, moveToRecycler, recyclerMissingReportOnce, warnRecyclerMissing, callback); //
}

//############################################################################################################
void fff::deleteListOfFiles(const std::vector<Zstring>& filesToDeletePaths,
                            std::vector<Zstring>& deletedPaths,
                            bool moveToRecycler,
                            bool& warnRecyclerMissing,
                            ProcessCallback& cb /*throw X*/) //throw X
{
    assert(deletedPaths.empty());

    cb.initNewPhase(filesToDeletePaths.size(), 0 /*bytesTotal*/, ProcessPhase::none); //throw X

    bool recyclerMissingReportOnce = false;

    for (const Zstring& filePath : filesToDeletePaths)
        tryReportingError([&]
    {
        const AbstractPath cfgPath = createItemPathNative(filePath);
        ItemStatReporter statReporter(1, 0, cb);

        if (moveToRecycler)
            try
            {
                reportInfo(replaceCpy(_("Moving file %x to the recycle bin"), L"%x", fmtPath(AFS::getDisplayPath(cfgPath))), cb); //throw X
                AFS::moveToRecycleBinIfExists(cfgPath); //throw FileError, RecycleBinUnavailable
            }
            catch (const RecycleBinUnavailable& e)
            {
                if (!recyclerMissingReportOnce)
                {
                    recyclerMissingReportOnce = true;
                    cb.reportWarning(e.toString() + L"\n\n" + _("Ignore and delete permanently each time recycle bin is unavailable?"), warnRecyclerMissing); //throw X
                }
                cb.logMessage(replaceCpy(_("Deleting file %x"), L"%x", fmtPath(AFS::getDisplayPath(cfgPath))) +
                              L" [" + _("The recycle bin is not available") + L']', PhaseCallback::MsgType::warning); //throw X
                AFS::removeFileIfExists(cfgPath); //throw FileError
            }
        else
        {
            reportInfo(replaceCpy(_("Deleting file %x"), L"%x", fmtPath(AFS::getDisplayPath(cfgPath))), cb); //throw X
            AFS::removeFileIfExists(cfgPath); //throw FileError
        }

        statReporter.reportDelta(1, 0);
        deletedPaths.push_back(filePath);
    }, cb); //throw X
}

//############################################################################################################

TempFileBuffer::~TempFileBuffer()
{
    if (!tempFolderPath_.empty())
        try
        {
            removeDirectoryPlainRecursion(tempFolderPath_); //throw FileError
        }
        catch (FileError&) { assert(false); }
    warn_static("log, maybe?")
}


void TempFileBuffer::createTempFolderPath() //throw FileError
{
    if (tempFolderPath_.empty())
    {
        //generate random temp folder path e.g. C:\Users\Zenju\AppData\Local\Temp\FFS-068b2e88
        const uint32_t shortGuid = getCrc32(generateGUID()); //no need for full-blown (pseudo-)random numbers for this one-time invocation

        const Zstring& tempPathTmp = appendPath(getTempFolderPath(), //throw FileError
                                                Zstr("FFS-") + printNumber<Zstring>(Zstr("%08x"), static_cast<unsigned int>(shortGuid)));

        createDirectoryIfMissingRecursion(tempPathTmp); //throw FileError

        tempFolderPath_ = tempPathTmp;
    }
}


Zstring TempFileBuffer::getAndCreateFolderPath() //throw FileError
{
    createTempFolderPath(); //throw FileError
    return tempFolderPath_;
}


//returns empty if not available (item not existing, error during copy)
Zstring TempFileBuffer::getTempPath(const FileDescriptor& descr) const
{
    auto it = tempFilePaths_.find(descr);
    if (it != tempFilePaths_.end())
        return it->second;
    return Zstring();
}


void TempFileBuffer::createTempFiles(const std::set<FileDescriptor>& workLoad, ProcessCallback& callback /*throw X*/) //throw X
{
    const int itemTotal = static_cast<int>(workLoad.size());
    int64_t bytesTotal = 0;

    for (const FileDescriptor& descr : workLoad)
        bytesTotal += descr.attr.fileSize;

    callback.initNewPhase(itemTotal, bytesTotal, ProcessPhase::none); //throw X
    //------------------------------------------------------------------------------

    const std::wstring errMsg = tryReportingError([&]
    {
        createTempFolderPath(); //throw FileError
    }, callback); //throw X
    if (!errMsg.empty()) return;

    for (const FileDescriptor& descr : workLoad)
    {
        assert(!tempFilePaths_.contains(descr)); //ensure correct stats, NO overwrite-copy => caller-contract!

        MemoryStreamOut cookie; //create hash to distinguish different versions and file locations
        writeNumber   (cookie, descr.attr.modTime);
        writeNumber   (cookie, descr.attr.fileSize);
        writeNumber   (cookie, descr.attr.filePrint);
        writeNumber   (cookie, descr.attr.isFollowedSymlink);
        writeContainer(cookie, AFS::getInitPathPhrase(descr.path));

        const uint16_t crc16 = getCrc16(cookie.ref());
        const Zstring descrHash = printNumber<Zstring>(Zstr("%04x"), static_cast<unsigned int>(crc16));

        const Zstring fileName = AFS::getItemName(descr.path);

        auto it = findLast(fileName.begin(), fileName.end(), Zstr('.')); //gracefully handle case of missing "."
        const Zstring tempFileName = Zstring(fileName.begin(), it) + Zstr('~') + descrHash + Zstring(it, fileName.end());

        const Zstring tempFilePath = appendPath(tempFolderPath_, tempFileName);
        const AFS::StreamAttributes sourceAttr{descr.attr.modTime, descr.attr.fileSize, descr.attr.filePrint};

        tryReportingError([&]
        {
            std::wstring statusMsg = replaceCpy(_("Creating file %x"), L"%x", fmtPath(tempFilePath));

            ItemStatReporter statReporter(1, descr.attr.fileSize, callback);
            PercentStatReporter percentReporter(statusMsg, descr.attr.fileSize, statReporter);

            reportInfo(std::move(statusMsg), callback); //throw X

            //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
            /*const AFS::FileCopyResult result =*/
            AFS::copyFileTransactional(descr.path, sourceAttr, //throw FileError, ErrorFileLocked, X
                                       createItemPathNative(tempFilePath),
                                       false /*copyFilePermissions*/, true /*transactionalCopy*/, nullptr /*onDeleteTargetFile*/,
                                       [&](int64_t bytesDelta)
            {
                percentReporter.updateDeltaAndStatus(bytesDelta); //throw X
                callback.requestUiUpdate(); //throw X  => not reliably covered by PercentStatReporter::updateDeltaAndStatus()! e.g. during first few seconds: STATUS_PERCENT_DELAY!
            });
            //result.errorModTime? => irrelevant for temp files!
            statReporter.reportDelta(1, 0);

            tempFilePaths_[descr] = tempFilePath;
        }, callback); //throw X
    }
}

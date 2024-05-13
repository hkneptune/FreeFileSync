// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "algorithm.h"
//#include <zen/perf.h>
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
class SetSyncDirViaDifferences
{
public:
    static void execute(const DirectionByDiff& dirs, ContainerObject& conObj) { SetSyncDirViaDifferences(dirs).recurse(conObj); }

private:
    SetSyncDirViaDifferences(const DirectionByDiff& dirs) : dirs_(dirs) {}

    void recurse(ContainerObject& conObj) const
    {
        for (FilePair& file : conObj.refSubFiles())
            processFile(file);
        for (SymlinkPair& link : conObj.refSubLinks())
            processLink(link);
        for (FolderPair& folder : conObj.refSubFolders())
            processFolder(folder);
    }

    void processFile(FilePair& file) const
    {
        const CompareFileResult cat = file.getCategory();

        //##################### schedule old temporary files for deletion ####################
        if (cat == FILE_LEFT_ONLY && endsWith(file.getItemName<SelectSide::left>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::left);
        else if (cat == FILE_RIGHT_ONLY && endsWith(file.getItemName<SelectSide::right>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::right);
        //####################################################################################

        switch (cat)
        {
            case FILE_EQUAL:
                //file.setSyncDir(SyncDirection::none);
                break;
            case FILE_RENAMED:
                if (dirs_.leftNewer == dirs_.rightNewer)
                    file.setSyncDir(dirs_.leftNewer); //treat "rename" like a "file update"
                else
                    file.setSyncDirConflict(txtDiffName_);
                break;
            case FILE_LEFT_ONLY:
                file.setSyncDir(dirs_.leftOnly);
                break;
            case FILE_RIGHT_ONLY:
                file.setSyncDir(dirs_.rightOnly);
                break;
            case FILE_LEFT_NEWER:
                file.setSyncDir(dirs_.leftNewer);
                break;
            case FILE_RIGHT_NEWER:
                file.setSyncDir(dirs_.rightNewer);
                break;
            case FILE_TIME_INVALID:
                if (dirs_.leftNewer == dirs_.rightNewer) //e.g. "Mirror" sync variant
                    file.setSyncDir(dirs_.leftNewer);
                else
                    file.setSyncDirConflict(file.getCategoryCustomDescription());
                break;
            case FILE_DIFFERENT_CONTENT:
                if (dirs_.leftNewer == dirs_.rightNewer)
                    file.setSyncDir(dirs_.leftNewer);
                else
                    file.setSyncDirConflict(txtDiffContent_);
                break;
            case FILE_CONFLICT:
                file.setSyncDirConflict(file.getCategoryCustomDescription()); //take over category conflict: allow *manual* resolution only!
                break;
        }
    }

    void processLink(SymlinkPair& symlink) const
    {
        switch (symlink.getLinkCategory())
        {
            case SYMLINK_EQUAL:
                //symlink.setSyncDir(SyncDirection::none);
                break;
            case SYMLINK_RENAMED:
                if (dirs_.leftNewer == dirs_.rightNewer)
                    symlink.setSyncDir(dirs_.leftNewer);
                else
                    symlink.setSyncDirConflict(txtDiffName_);
                break;
            case SYMLINK_LEFT_ONLY:
                symlink.setSyncDir(dirs_.leftOnly);
                break;
            case SYMLINK_RIGHT_ONLY:
                symlink.setSyncDir(dirs_.rightOnly);
                break;
            case SYMLINK_LEFT_NEWER:
                symlink.setSyncDir(dirs_.leftNewer);
                break;
            case SYMLINK_RIGHT_NEWER:
                symlink.setSyncDir(dirs_.rightNewer);
                break;
            case SYMLINK_TIME_INVALID:
                if (dirs_.leftNewer == dirs_.rightNewer)
                    symlink.setSyncDir(dirs_.leftNewer);
                else
                    symlink.setSyncDirConflict(symlink.getCategoryCustomDescription());
                break;
            case SYMLINK_DIFFERENT_CONTENT:
                if (dirs_.leftNewer == dirs_.rightNewer)
                    symlink.setSyncDir(dirs_.leftNewer);
                else
                    symlink.setSyncDirConflict(txtDiffContent_);
                break;
            case SYMLINK_CONFLICT:
                symlink.setSyncDirConflict(symlink.getCategoryCustomDescription()); //take over category conflict: allow *manual* resolution only!
                break;
        }
    }

    void processFolder(FolderPair& folder) const
    {
        const CompareDirResult cat = folder.getDirCategory();

        //########### schedule abandoned temporary recycle bin directory for deletion  ##########
        if (cat == DIR_LEFT_ONLY && endsWith(folder.getItemName<SelectSide::left>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::left, folder); //
        else if (cat == DIR_RIGHT_ONLY && endsWith(folder.getItemName<SelectSide::right>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::right, folder); //don't recurse below!
        //#######################################################################################

        switch (cat)
        {
            case DIR_EQUAL:
                //folder.setSyncDir(SyncDirection::none);
                break;
            case DIR_RENAMED:
                if (dirs_.leftNewer == dirs_.rightNewer)
                    folder.setSyncDir(dirs_.leftNewer);
                else
                    folder.setSyncDirConflict(txtDiffName_);
                break;
            case DIR_LEFT_ONLY:
                folder.setSyncDir(dirs_.leftOnly);
                break;
            case DIR_RIGHT_ONLY:
                folder.setSyncDir(dirs_.rightOnly);
                break;
            case DIR_CONFLICT:
                folder.setSyncDirConflict(folder.getCategoryCustomDescription()); //take over category conflict: allow *manual* resolution only!
                break;
        }

        recurse(folder);
    }

    const DirectionByDiff dirs_;
    const Zstringc txtDiffName_ = utfTo<Zstringc>(_("Cannot determine sync-direction:") + L'\n' + TAB_SPACE +
                                                  _("The items have different names, but it's unknown which side was renamed."));
    const Zstringc txtDiffContent_ = utfTo<Zstringc>(_("Cannot determine sync-direction:") + L'\n' + TAB_SPACE +
                                                     _("The items have different content, but it's unknown which side has changed."));
};

//---------------------------------------------------------------------------------------------------------------

//test if non-equal items exist in scanned data
bool allItemsCategoryEqual(const ContainerObject& conObj)
{
    return std::all_of(conObj.refSubFiles().begin(), conObj.refSubFiles().end(),
    [](const FilePair& file) { return file.getCategory() == FILE_EQUAL; })&&

    std::all_of(conObj.refSubLinks().begin(), conObj.refSubLinks().end(),
    [](const SymlinkPair& symlink) { return symlink.getLinkCategory() == SYMLINK_EQUAL; })&&

    std::all_of(conObj.refSubFolders().begin(), conObj.refSubFolders().end(), [](const FolderPair& folder)
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
CudAction compareDbEntry(const FilePair& file, const InSyncFile* dbFile, unsigned int fileTimeTolerance,
                         const std::vector<unsigned int>& ignoreTimeShiftMinutes, bool renamedOrMoved)
{
    if (file.isEmpty<side>())
        return dbFile ? (renamedOrMoved ? CudAction::update: CudAction::delete_) : CudAction::noChange;
    else if (!dbFile)
        return (renamedOrMoved ? CudAction::update : CudAction::create);

    const InSyncDescrFile& descrDb = selectParam<side>(dbFile->left, dbFile->right);

    return sameFileTime(file.getLastWriteTime<side>(), descrDb.modTime, fileTimeTolerance, ignoreTimeShiftMinutes) &&
           //- we do *not* consider file ID, but only *user-visual* changes. E.g. user moving data to some other medium should not be considered a change!
           file.getFileSize<side>() == dbFile->fileSize ?
           CudAction::noChange : CudAction::update;
}


//check whether database entry is in sync considering *current* comparison settings
inline
bool stillInSync(const InSyncFile& dbFile, CompareVariant compareVar, unsigned int fileTimeTolerance, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    switch (compareVar)
    {
        case CompareVariant::timeSize:
            if (dbFile.cmpVar == CompareVariant::content) return true; //special rule: this is certainly "good enough" for CompareVariant::timeSize!

            //case-sensitive file name match is a database invariant!
            return sameFileTime(dbFile.left.modTime, dbFile.right.modTime, fileTimeTolerance, ignoreTimeShiftMinutes);

        case CompareVariant::content:
            //case-sensitive file name match is a database invariant!
            return dbFile.cmpVar == CompareVariant::content;
        //in contrast to comparison, we don't care about modification time here!

        case CompareVariant::size: //file size/case-sensitive file name always matches on both sides for an "in-sync" database entry
            return true;
    }
    assert(false);
    return false;
}

//--------------------------------------------------------------------

//check whether database entry and current item match: *irrespective* of current comparison settings
template <SelectSide side> inline
CudAction compareDbEntry(const SymlinkPair& symlink, const InSyncSymlink* dbSymlink, unsigned int fileTimeTolerance,
                         const std::vector<unsigned int>& ignoreTimeShiftMinutes, bool renamedOrMoved)
{
    if (symlink.isEmpty<side>())
        return dbSymlink ? (renamedOrMoved ? CudAction::update: CudAction::delete_) : CudAction::noChange;
    else if (!dbSymlink)
        return (renamedOrMoved ? CudAction::update : CudAction::create);

    const InSyncDescrLink& descrDb = selectParam<side>(dbSymlink->left, dbSymlink->right);

    return sameFileTime(symlink.getLastWriteTime<side>(), descrDb.modTime, fileTimeTolerance, ignoreTimeShiftMinutes) ?
           CudAction::noChange : CudAction::update;
}


//check whether database entry is in sync considering *current* comparison settings
inline
bool stillInSync(const InSyncSymlink& dbLink, CompareVariant compareVar, unsigned int fileTimeTolerance, const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    switch (compareVar)
    {
        case CompareVariant::timeSize:
            if (dbLink.cmpVar == CompareVariant::content || dbLink.cmpVar == CompareVariant::size)
                return true; //special rule: this is already "good enough" for CompareVariant::timeSize!

            //case-sensitive symlink name match is a database invariant!
            return sameFileTime(dbLink.left.modTime, dbLink.right.modTime, fileTimeTolerance, ignoreTimeShiftMinutes);

        case CompareVariant::content:
        case CompareVariant::size: //== categorized by content! see comparison.cpp, ComparisonBuffer::compareBySize()
            //case-sensitive symlink name match is a database invariant!
            return dbLink.cmpVar == CompareVariant::content || dbLink.cmpVar == CompareVariant::size;
    }
    assert(false);
    return false;
}

//--------------------------------------------------------------------

//check whether database entry and current item match: *irrespective* of current comparison settings
template <SelectSide side> inline
CudAction compareDbEntry(const FolderPair& folder, const InSyncFolder* dbFolder, bool renamedOrMoved)
{
    if (folder.isEmpty<side>())
        return dbFolder ? (renamedOrMoved ? CudAction::update: CudAction::delete_) : CudAction::noChange;
    else if (!dbFolder)
        return (renamedOrMoved ? CudAction::update : CudAction::create);

    return CudAction::noChange;
}


inline
bool stillInSync(const InSyncFolder& dbFolder)
{
    //case-sensitive folder name match is a database invariant!
    return true;
}

//----------------------------------------------------------------------------------------------

class DetectMovedFiles
{
public:
    static void execute(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder)
    {
        DetectMovedFiles(baseFolder, dbFolder);
        baseFolder.removeDoubleEmpty(); //see findAndSetMovePair()
    }

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

    void recurse(ContainerObject& conObj, const InSyncFolder* dbFolderL, const InSyncFolder* dbFolderR)
    {
        for (FilePair& file : conObj.refSubFiles())
        {
            file.setMoveRef(nullptr); //discard remnants from previous move detection and start fresh (e.g. consider manual folder rename)

            const AFS::FingerPrint filePrintL = file.isEmpty<SelectSide::left >() ? 0 : file.getFilePrint<SelectSide::left >();
            const AFS::FingerPrint filePrintR = file.isEmpty<SelectSide::right>() ? 0 : file.getFilePrint<SelectSide::right>();

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
                cat == FILE_LEFT_ONLY)
            {
                if (const InSyncFile* dbEntry = getDbEntry(dbFolderL, file.getItemName<SelectSide::left>()))
                    exLeftOnlyByPath_.emplace(dbEntry, &file);
            }
            else if (cat == FILE_RIGHT_ONLY)
            {
                if (const InSyncFile* dbEntry = getDbEntry(dbFolderR, file.getItemName<SelectSide::right>()))
                    exRightOnlyByPath_.emplace(dbEntry, &file);
            }
        }

        for (FolderPair& folder : conObj.refSubFolders())
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
            constexpr CompareFileResult oneSideOnlyTag = side == SelectSide::left ? FILE_LEFT_ONLY : FILE_RIGHT_ONLY;

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
                            if (fileLeftOnly ->getMoveRef() == nullptr &&              //needless checks? (file prints are unique in this context)
                                fileRightOnly->getMoveRef() == nullptr &&              //
                                fileLeftOnly ->getCategory() == FILE_LEFT_ONLY && //is it possible we could get conflicting matches!?
                                fileRightOnly->getCategory() == FILE_RIGHT_ONLY)  //=> likely 'yes', but only in obscure cases
                                //--------------- found a match ---------------
                            {
                                //move pair is just a 'rename' => combine:
                                if (&fileLeftOnly->parent() == &fileRightOnly->parent())
                                {
                                    fileLeftOnly->setSyncedTo<SelectSide::right>(fileLeftOnly->getFileSize<SelectSide::left>(),
                                                                                 fileRightOnly->getLastWriteTime<SelectSide::right>(), //lastWriteTimeTrg
                                                                                 fileLeftOnly ->getLastWriteTime<SelectSide::left >(), //lastWriteTimeSrc

                                                                                 fileRightOnly->getFilePrint<SelectSide::right>(), //filePrintTrg
                                                                                 fileLeftOnly ->getFilePrint<SelectSide::left >(), //filePrintSrc

                                                                                 fileRightOnly->isFollowedSymlink<SelectSide::right>(),  //isSymlinkTrg
                                                                                 fileLeftOnly ->isFollowedSymlink<SelectSide::left >()); //isSymlinkSrc

                                    fileLeftOnly->setItemName<SelectSide::right>(fileRightOnly->getItemName<SelectSide::right>());

                                    assert(fileLeftOnly->isActive() && fileRightOnly->isActive()); //can this fail? excluded files are not added during comparison...
                                    if (fileLeftOnly->isActive() != fileRightOnly->isActive()) //just in case
                                        fileLeftOnly->setActive(false);

                                    fileRightOnly->removeItem<SelectSide::right>(); //=> call ContainerObject::removeDoubleEmpty() later!
                                }
                                else //regular move pair: mark it!
                                {
                                    fileLeftOnly ->setMoveRef(fileRightOnly->getId());
                                    fileRightOnly->setMoveRef(fileLeftOnly ->getId());
                                }
                            }
                            else
                                assert(fileLeftOnly ->getMoveRef() == fileRightOnly->getId() &&
                                       fileRightOnly->getMoveRef() == fileLeftOnly ->getId());
                        }
    }

    const CompareVariant cmpVar_;
    const unsigned int fileTimeTolerance_;
    const std::vector<unsigned int> ignoreTimeShiftMinutes_;

    std::vector<FilePair*> filesL_; //collection of *all* file items (with non-null filePrint)
    std::vector<FilePair*> filesR_; // => detect duplicate file IDs

    std::unordered_map<AFS::FingerPrint, FilePair*>  exLeftOnlyById_;
    std::unordered_map<AFS::FingerPrint, FilePair*> exRightOnlyById_;

    std::unordered_map<const InSyncFile*, FilePair*>  exLeftOnlyByPath_;
    std::unordered_map<const InSyncFile*, FilePair*> exRightOnlyByPath_;

    /*  Detect Renamed Files:

         X  ->  |_|      Create right
        |_| ->   Y       Delete right

        resolve as: Move/Rename Y to X on right

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

class SetSyncDirViaChanges
{
public:
    static void execute(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder, const DirectionByChange& dirs)
    { SetSyncDirViaChanges(baseFolder, dbFolder, dirs); }

private:
    SetSyncDirViaChanges(BaseFolderPair& baseFolder, const InSyncFolder& dbFolder, const DirectionByChange& dirs) :
        dirs_(dirs),
        cmpVar_                (baseFolder.getCompVariant()),
        fileTimeTolerance_     (baseFolder.getFileTimeTolerance()),
        ignoreTimeShiftMinutes_(baseFolder.getIgnoredTimeShift())
    {
        //-> considering filter not relevant:
        //  if stricter filter than last time: all ok;
        //  if less strict filter (if file ex on both sides -> conflict, fine; if file ex. on one side: copy to other side: fine)
        recurse(baseFolder, &dbFolder);
    }

    void recurse(ContainerObject& conObj, const InSyncFolder* dbFolder) const
    {
        for (FilePair& file : conObj.refSubFiles())
            processFile(file, dbFolder);
        for (SymlinkPair& symlink : conObj.refSubLinks())
            processSymlink(symlink, dbFolder);
        for (FolderPair& folder : conObj.refSubFolders())
            processDir(folder, dbFolder);
    }

    void processFile(FilePair& file, const InSyncFolder* dbFolder) const
    {
        const CompareFileResult cat = file.getCategory();
        if (cat == FILE_EQUAL)
            return;
        else if (cat == FILE_CONFLICT) //take over category conflict: allow *manual* resolution only!
            return file.setSyncDirConflict(file.getCategoryCustomDescription());

        //##################### schedule old temporary files for deletion ####################
        if (cat == FILE_LEFT_ONLY && endsWith(file.getItemName<SelectSide::left>(), AFS::TEMP_FILE_ENDING))
            return file.setSyncDir(SyncDirection::left);
        else if (cat == FILE_RIGHT_ONLY && endsWith(file.getItemName<SelectSide::right>(), AFS::TEMP_FILE_ENDING))
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

        //consider renamed/moved files as "updated" with regards to "changes"-based sync settings: https://freefilesync.org/forum/viewtopic.php?t=10594
        const bool renamedOrMoved = cat == FILE_RENAMED || [&file]
        {
            if (const FileSystemObject::ObjectId moveFileRef = file.getMoveRef())
                if (auto refFile = dynamic_cast<const FilePair*>(FileSystemObject::retrieve(moveFileRef)))
                {
                    if (refFile->getMoveRef() == file.getId()) //both ends should agree...
                        return true;
                    else assert(false); //...and why shouldn't they?
                }
            return false;
        }();
        const CudAction changeL = compareDbEntry<SelectSide::left >(file, dbEntryL, fileTimeTolerance_, ignoreTimeShiftMinutes_, renamedOrMoved);
        const CudAction changeR = compareDbEntry<SelectSide::right>(file, dbEntryR, fileTimeTolerance_, ignoreTimeShiftMinutes_, renamedOrMoved);

        setSyncDirForChange(file, changeL, changeR);
    }

    void processSymlink(SymlinkPair& symlink, const InSyncFolder* dbFolder) const
    {
        const CompareSymlinkResult cat = symlink.getLinkCategory();
        if (cat == SYMLINK_EQUAL)
            return;
        else if (cat == SYMLINK_CONFLICT) //take over category conflict: allow *manual* resolution only!
            return symlink.setSyncDirConflict(symlink.getCategoryCustomDescription());

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

        const bool renamedOrMoved = cat == SYMLINK_RENAMED;
        const CudAction changeL = compareDbEntry<SelectSide::left >(symlink, dbEntryL, fileTimeTolerance_, ignoreTimeShiftMinutes_, renamedOrMoved);
        const CudAction changeR = compareDbEntry<SelectSide::right>(symlink, dbEntryR, fileTimeTolerance_, ignoreTimeShiftMinutes_, renamedOrMoved);

        setSyncDirForChange(symlink, changeL, changeR);
    }

    void processDir(FolderPair& folder, const InSyncFolder* dbFolder) const
    {
        const CompareDirResult cat = folder.getDirCategory();

        //########### schedule abandoned temporary recycle bin directory for deletion  ##########
        if (cat == DIR_LEFT_ONLY && endsWith(folder.getItemName<SelectSide::left>(), AFS::TEMP_FILE_ENDING))
            return setSyncDirectionRec(SyncDirection::left, folder); //
        else if (cat == DIR_RIGHT_ONLY && endsWith(folder.getItemName<SelectSide::right>(), AFS::TEMP_FILE_ENDING))
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

        if (cat == DIR_EQUAL)
            ;
        else if (cat == DIR_CONFLICT) //take over category conflict: allow *manual* resolution only!
            folder.setSyncDirConflict(folder.getCategoryCustomDescription());
        else
        {
            if (dbEntry && !stillInSync(*dbEntry))
                folder.setSyncDirConflict(txtDbNotInSync_);
            else
            {
                const bool renamedOrMoved = cat == DIR_RENAMED;
                const CudAction changeL = compareDbEntry<SelectSide::left >(folder, dbEntryL, renamedOrMoved);
                const CudAction changeR = compareDbEntry<SelectSide::right>(folder, dbEntryR, renamedOrMoved);

                setSyncDirForChange(folder, changeL, changeR);
            }
        }

        recurse(folder, dbEntry);
    }

    template <SelectSide side>
    SyncDirection getSyncDirForChange(CudAction change) const
    {
        const auto& changedirs = selectParam<side>(dirs_.left, dirs_.right);
        switch (change)
        {
            //*INDENT-OFF*
            case CudAction::noChange: return SyncDirection::none;
            case CudAction::create:  return changedirs.create;
            case CudAction::update:  return changedirs.update;
            case CudAction::delete_: return changedirs.delete_;
            //*INDENT-ON*
        }
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
    }

    void setSyncDirForChange(FileSystemObject& fsObj, CudAction changeL, CudAction changeR) const
    {
        const SyncDirection dirL = getSyncDirForChange<SelectSide::left >(changeL);
        const SyncDirection dirR = getSyncDirForChange<SelectSide::right>(changeR);
        if (changeL != CudAction::noChange)
        {
            if (changeR != CudAction::noChange) //both sides changed
            {
                if (dirL == dirR) //but luckily agree on direction
                    fsObj.setSyncDir(dirL);
                else
                    fsObj.setSyncDirConflict(txtBothSidesChanged_);
            }
            else //change on left
                fsObj.setSyncDir(dirL);
        }
        else
        {
            if (changeR != CudAction::noChange) //change on right
                fsObj.setSyncDir(dirR);
            else //no change on either side
                fsObj.setSyncDirConflict(txtNoSideChanged_); //obscure, but possible if user widens "fileTimeTolerance"
        }
    }

    //need ref-counted strings! see FileSystemObject::syncDirectionConflict_
    const Zstringc txtBothSidesChanged_ = utfTo<Zstringc>(_("Both sides have changed since last synchronization."));
    const Zstringc txtNoSideChanged_    = utfTo<Zstringc>(_("Cannot determine sync-direction:") + L'\n' + TAB_SPACE + _("No change since last synchronization."));
    const Zstringc txtDbNotInSync_      = utfTo<Zstringc>(_("Cannot determine sync-direction:") + L'\n' + TAB_SPACE + _("The database entry is not in sync, considering current settings."));
    const Zstringc txtDbAmbiguous_      = utfTo<Zstringc>(_("Cannot determine sync-direction:") + L'\n' + TAB_SPACE + _("The database entry is ambiguous."));

    const DirectionByChange dirs_;
    const CompareVariant cmpVar_;
    const unsigned int fileTimeTolerance_;
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
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

    std::vector<std::pair<BaseFolderPair*, SyncDirectionConfig>> output;

    for (auto it = folderCmp.begin(); it != folderCmp.end(); ++it)
    {
        BaseFolderPair& baseFolder = it->ref();
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
                if (const DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&dirCfg.dirs))
                    SetSyncDirViaDifferences::execute(*diffDirs, *baseFolder);
                else
                {
                    const DirectionByChange& changeDirs = std::get<DirectionByChange>(dirCfg.dirs);

                    auto it = lastSyncStates.find(baseFolder);
                    if (const InSyncFolder* lastSyncState = it != lastSyncStates.end() ? &it->second.ref() : nullptr)
                    {
                        //detect moved files (*before* setting sync directions: might combine moved files into single file pairs, wich changes category!)
                        DetectMovedFiles::execute(*baseFolder, *lastSyncState);                

                        SetSyncDirViaChanges::execute(*baseFolder, *lastSyncState, changeDirs);
                    }
                    else //fallback:
                    {
                        std::wstring msg = _("Database file is not available: Setting default directions for synchronization.");
                        if (directCfgs.size() > 1)
                            msg += SPACED_DASH + getShortDisplayNameForFolderPair(baseFolder->getAbstractPath<SelectSide::left >(),
                                                                                  baseFolder->getAbstractPath<SelectSide::right>());                        
                        try { callback.logMessage(msg, PhaseCallback::MsgType::warning); /*throw X*/} catch (...) {};

                        SetSyncDirViaDifferences::execute(getDiffDirDefault(changeDirs), *baseFolder);
                    }
                }
            }
        //*INDENT-ON*
    );

    std::vector<const BaseFolderPair*> baseFoldersForDbLoad;
    for (const auto& [baseFolder, dirCfg] : directCfgs)
        if (std::get_if<DirectionByChange>(&dirCfg.dirs))
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
class ApplyPathFilter
{
public:
    static void execute(ContainerObject& conObj, const PathFilter& filter) { ApplyPathFilter(conObj, filter); }

private:
    ApplyPathFilter(ContainerObject& conObj, const PathFilter& filter) : filter_(filter)  { recurse(conObj); }

    void recurse(ContainerObject& conObj) const
    {
        for (FilePair& file : conObj.refSubFiles())
            processFile(file);
        for (SymlinkPair& symlink : conObj.refSubLinks())
            processLink(symlink);
        for (FolderPair& folder : conObj.refSubFolders())
            processDir(folder);
    }

    void processFile(FilePair& file) const
    {
        if (Eval<strategy>::process(file))
            file.setActive(file.passFileFilter(filter_));
    }

    void processLink(SymlinkPair& symlink) const
    {
        if (Eval<strategy>::process(symlink))
            symlink.setActive(symlink.passFileFilter(filter_));
    }

    void processDir(FolderPair& folder) const
    {
        bool childItemMightMatch = true;
        const bool filterPassed = folder.passDirFilter(filter_, &childItemMightMatch);

        if (Eval<strategy>::process(folder))
            folder.setActive(filterPassed);

        if (!childItemMightMatch) //use same logic like directory traversing: evaluate filter in subdirs only if objects *could* match
        {
            //exclude all files dirs in subfolders => incompatible with STRATEGY_OR!
            auto onFsItem = [](FileSystemObject& fsObj) { fsObj.setActive(false); };
            visitFSObjectRecursively(static_cast<ContainerObject&>(folder), onFsItem, onFsItem, onFsItem);
            return;
        }

        recurse(folder);
    }

    const PathFilter& filter_;
};


template <FilterStrategy strategy>
class ApplySoftFilter //falsify only! -> can run directly after "hard/base filter"
{
public:
    static void execute(ContainerObject& conObj, const SoftFilter& timeSizeFilter) { ApplySoftFilter(conObj, timeSizeFilter); }

private:
    ApplySoftFilter(ContainerObject& conObj, const SoftFilter& timeSizeFilter) : timeSizeFilter_(timeSizeFilter) { recurse(conObj); }

    void recurse(fff::ContainerObject& conObj) const
    {
        for (FilePair& file : conObj.refSubFiles())
            processFile(file);
        for (SymlinkPair& symlink : conObj.refSubLinks())
            processLink(symlink);
        for (FolderPair& folder : conObj.refSubFolders())
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
    ApplyPathFilter<STRATEGY_AND>::execute(baseFolder, NameFilter(FilterConfig().includeFilter, excludeFilter));
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
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

    //merge first and additional pairs
    std::vector<LocalPairConfig> allPairs;
    allPairs.push_back(mainCfg.firstPair);
    allPairs.insert(allPairs.end(),
                    mainCfg.additionalPairs.begin(), //add additional pairs
                    mainCfg.additionalPairs.end());

    for (auto it = allPairs.begin(); it != allPairs.end(); ++it)
    {
        BaseFolderPair& baseFolder = folderCmp[it - allPairs.begin()].ref();

        const NormalizedFilter normFilter = normalizeFilters(mainCfg.globalFilter, it->localFilter);

        //"set" hard filter
        ApplyPathFilter<STRATEGY_SET>::execute(baseFolder, normFilter.nameFilter.ref());

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


std::optional<PathDependency> fff::getPathDependency(const AbstractPath& itemPathL, const AbstractPath& itemPathR)
{
    if (!AFS::isNullPath(itemPathL) && !AFS::isNullPath(itemPathR))
    {
        if (itemPathL.afsDevice == itemPathR.afsDevice)
        {
            const std::vector<Zstring> relPathL = splitCpy(itemPathL.afsPath.value, FILE_NAME_SEPARATOR, SplitOnEmpty::skip);
            const std::vector<Zstring> relPathR = splitCpy(itemPathR.afsPath.value, FILE_NAME_SEPARATOR, SplitOnEmpty::skip);

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

                return PathDependency{leftParent ? itemPathL : itemPathR, relDirPath};
            }
        }
    }
    return {};
}


std::optional<PathDependency> fff::getFolderPathDependency(const AbstractPath& folderPathL, const PathFilter& filterL,
                                                           const AbstractPath& folderPathR, const PathFilter& filterR)
{
    if (std::optional<PathDependency> pd = getPathDependency(folderPathL, folderPathR))
    {
        const PathFilter& filterP = pd->itemPathParent == folderPathL ? filterL : filterR;
        //if there's a dependency, check if the sub directory is (fully) excluded via filter
        //=> easy to check but still insufficient in general:
        // - one folder may have a *.txt include-filter, the other a *.lng include filter => no dependencies, but "childItemMightMatch = true" below!
        // - user may have manually excluded the conflicting items or changed the filter settings without running a re-compare
        bool childItemMightMatch = true;
        if (pd->relPath.empty() || filterP.passDirFilter(pd->relPath, &childItemMightMatch) || childItemMightMatch)
            return pd;
    }
    return {};
}

//############################################################################################################

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
            //- itemExists()? too expensive, considering that "already existing" is the most common case

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


void fff::copyToAlternateFolder(const std::vector<const FileSystemObject*>& selectionL,
                                const std::vector<const FileSystemObject*>& selectionR,
                                const Zstring& targetFolderPathPhrase,
                                bool keepRelPaths, bool overwriteIfExists,
                                WarningDialogs& warnings,
                                ProcessCallback& callback /*throw X*/) //throw X
{
    assert(std::all_of(selectionL.begin(), selectionL.end(), [](const FileSystemObject* fsObj) { return !fsObj->isEmpty<SelectSide::left >(); }));
    assert(std::all_of(selectionR.begin(), selectionR.end(), [](const FileSystemObject* fsObj) { return !fsObj->isEmpty<SelectSide::right>(); }));

    const int itemTotal = static_cast<int>(selectionL.size() + selectionR.size());
    int64_t bytesTotal = 0;

    for (const FileSystemObject* fsObj : selectionL)
        visitFSObject(*fsObj, [](const FolderPair& folder) {},
    [&](const FilePair& file) { bytesTotal += static_cast<int64_t>(file.getFileSize<SelectSide::left>()); }, [](const SymlinkPair& symlink) {});

    for (const FileSystemObject* fsObj : selectionR)
        visitFSObject(*fsObj, [](const FolderPair& folder) {},
    [&](const FilePair& file) { bytesTotal += static_cast<int64_t>(file.getFileSize<SelectSide::right>()); }, [](const SymlinkPair& symlink) {});

    callback.initNewPhase(itemTotal, bytesTotal, ProcessPhase::none); //throw X
    //------------------------------------------------------------------------------

    const AbstractPath targetFolderPath = createAbstractPath(targetFolderPathPhrase);

    copyToAlternateFolderFrom<SelectSide::left >(selectionL, targetFolderPath, keepRelPaths, overwriteIfExists, callback);
    copyToAlternateFolderFrom<SelectSide::right>(selectionR, targetFolderPath, keepRelPaths, overwriteIfExists, callback);
}

//############################################################################################################

namespace
{
template <SelectSide side>
void deleteFilesOneSide(const std::vector<FileSystemObject*>& rowsToDelete,
                        bool moveToRecycler,
                        bool& recyclerMissingReportOnce,
                        bool& warnRecyclerMissing, //WarningDialogs::warnRecyclerMissing
                        const std::unordered_map<const BaseFolderPair*, SyncDirectionConfig>& baseFolderCfgs,
                        PhaseCallback& callback /*throw X*/) //throw X
{
    const std::wstring txtDelFilePermanent_ = _("Deleting file %x");
    const std::wstring txtDelFileRecycler_  = _("Moving file %x to the recycle bin");

    const std::wstring txtDelSymlinkPermanent_ = _("Deleting symbolic link %x");
    const std::wstring txtDelSymlinkRecycler_  = _("Moving symbolic link %x to the recycle bin");

    const std::wstring txtDelFolderPermanent_ = _("Deleting folder %x");
    const std::wstring txtDelFolderRecycler_  = _("Moving folder %x to the recycle bin");

    auto removeFile = [&](const AbstractPath& filePath, ItemStatReporter<PhaseCallback>& statReporter)
    {
        if (moveToRecycler)
            try
            {
                reportInfo(replaceCpy(txtDelFileRecycler_, L"%x", fmtPath(AFS::getDisplayPath(filePath))), statReporter); //throw X
                AFS::moveToRecycleBinIfExists(filePath); //throw FileError, RecycleBinUnavailable
            }
            catch (const RecycleBinUnavailable& e)
            {
                if (!recyclerMissingReportOnce)
                {
                    recyclerMissingReportOnce = true;
                    callback.reportWarning(e.toString() + L"\n\n" + _("Ignore and delete permanently each time recycle bin is unavailable?"), warnRecyclerMissing); //throw X
                }
                callback.logMessage(replaceCpy(txtDelFilePermanent_, L"%x", fmtPath(AFS::getDisplayPath(filePath))) +
                                    L" [" + _("Recycle bin unavailable") + L']', PhaseCallback::MsgType::warning); //throw X
                AFS::removeFileIfExists(filePath); //throw FileError
            }
        else
        {
            reportInfo(replaceCpy(txtDelFilePermanent_, L"%x", fmtPath(AFS::getDisplayPath(filePath))), statReporter); //throw X
            AFS::removeFileIfExists(filePath); //throw FileError
        }
        statReporter.reportDelta(1, 0);
    };

    auto removeSymlink = [&](const AbstractPath& symlinkPath, ItemStatReporter<PhaseCallback>& statReporter)
    {
        if (moveToRecycler)
            try
            {
                reportInfo(replaceCpy(txtDelSymlinkRecycler_, L"%x", fmtPath(AFS::getDisplayPath(symlinkPath))), statReporter); //throw X
                AFS::moveToRecycleBinIfExists(symlinkPath); //throw FileError, RecycleBinUnavailable
            }
            catch (const RecycleBinUnavailable& e)
            {
                if (!recyclerMissingReportOnce)
                {
                    recyclerMissingReportOnce = true;
                    callback.reportWarning(e.toString() + L"\n\n" + _("Ignore and delete permanently each time recycle bin is unavailable?"), warnRecyclerMissing); //throw X
                }
                callback.logMessage(replaceCpy(txtDelSymlinkPermanent_, L"%x", fmtPath(AFS::getDisplayPath(symlinkPath))) +
                                    L" [" + _("Recycle bin unavailable") + L']', PhaseCallback::MsgType::warning); //throw X
                AFS::removeSymlinkIfExists(symlinkPath); //throw FileError
            }
        else
        {
            reportInfo(replaceCpy(txtDelSymlinkPermanent_, L"%x", fmtPath(AFS::getDisplayPath(symlinkPath))), statReporter); //throw X
            AFS::removeSymlinkIfExists(symlinkPath); //throw FileError
        }
        statReporter.reportDelta(1, 0);
    };

    auto removeFolder = [&](const AbstractPath& folderPath, ItemStatReporter<PhaseCallback>& statReporter)
    {
        auto removeFolderPermanently = [&]
        {
            auto onBeforeDeletion = [&](const std::wstring& msgTemplate, const std::wstring& displayPath)
            {
                reportInfo(replaceCpy(msgTemplate, L"%x", fmtPath(displayPath)), statReporter); //throw X
                statReporter.reportDelta(1, 0); //it would be more correct to report *after* work was done!
            };

            AFS::removeFolderIfExistsRecursion(folderPath,
            [&](const std::wstring& displayPath) { onBeforeDeletion(txtDelFilePermanent_,    displayPath); },
            [&](const std::wstring& displayPath) { onBeforeDeletion(txtDelSymlinkPermanent_, displayPath); },
            [&](const std::wstring& displayPath) { onBeforeDeletion(txtDelFolderPermanent_,  displayPath); }); //throw FileError, X
        };

        if (moveToRecycler)
            try
            {
                reportInfo(replaceCpy(txtDelFolderRecycler_, L"%x", fmtPath(AFS::getDisplayPath(folderPath))), statReporter); //throw X
                AFS::moveToRecycleBinIfExists(folderPath); //throw FileError, RecycleBinUnavailable
                statReporter.reportDelta(1, 0);
            }
            catch (const RecycleBinUnavailable& e)
            {
                if (!recyclerMissingReportOnce)
                {
                    recyclerMissingReportOnce = true;
                    callback.reportWarning(e.toString() + L"\n\n" + _("Ignore and delete permanently each time recycle bin is unavailable?"), warnRecyclerMissing); //throw X
                }
                callback.logMessage(replaceCpy(txtDelFolderPermanent_, L"%x", fmtPath(AFS::getDisplayPath(folderPath))) +
                                    L" [" + _("Recycle bin unavailable") + L']', PhaseCallback::MsgType::warning); //throw X
                removeFolderPermanently(); //throw FileError, X
            }
        else
        {
            reportInfo(replaceCpy(txtDelFolderPermanent_, L"%x", fmtPath(AFS::getDisplayPath(folderPath))), statReporter); //throw X
            removeFolderPermanently(); //throw FileError, X
        }
    };


    for (FileSystemObject* fsObj : rowsToDelete) //all pointers are required(!) to be bound
        tryReportingError([&]
    {
        ItemStatReporter statReporter(1, 0, callback);

        if (!fsObj->isEmpty<side>()) //element may be implicitly deleted, e.g. if parent folder was deleted first
        {
            visitFSObject(*fsObj, [&](FolderPair& folder)
            {
                if (folder.isFollowedSymlink<side>())
                    removeSymlink(folder.getAbstractPath<side>(), statReporter); //throw FileError, X
                else
                    removeFolder(folder.getAbstractPath<side>(), statReporter); //throw FileError, X

                folder.removeItem<side>(); //removes recursively!
            },

            [&](FilePair& file)
            {
                if (file.isFollowedSymlink<side>())
                    removeSymlink(file.getAbstractPath<side>(), statReporter); //throw FileError, X
                else
                    removeFile(file.getAbstractPath<side>(), statReporter); //throw FileError, X

                file.removeItem<side>();
            },

            [&](SymlinkPair& symlink)
            {
                removeSymlink(symlink.getAbstractPath<side>(), statReporter); //throw FileError, X
                symlink.removeItem<side>();
            });
            //------- no-throw from here on -------
            const CompareFileResult catOld = fsObj->getCategory();

            //update sync direction: don't call redetermineSyncDirection() because user may have manually changed directions
            if (catOld == CompareFileResult::FILE_EQUAL)
            {
                const SyncDirection newDir = [&]
                {
                    const SyncDirectionConfig& dirCfg = baseFolderCfgs.find(&fsObj->base())->second; //not found? let it crash!

                    if (const DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&dirCfg.dirs))
                        return side == SelectSide::left ? diffDirs->rightOnly : diffDirs->leftOnly;
                    else
                    {
                        const DirectionByChange& changeDirs = std::get<DirectionByChange>(dirCfg.dirs);
                        return side == SelectSide::left ? changeDirs.left.delete_ : changeDirs.right.delete_;
                    }
                }();

                setSyncDirectionRec(newDir, *fsObj); //set new direction (recursively)
            }
            //else: keep old syncDir_
        }
    }, callback); //throw X
}
}


void fff::deleteFiles(const std::vector<FileSystemObject*>& selectionL,
                      const std::vector<FileSystemObject*>& selectionR,
                      const std::vector<std::pair<BaseFolderPair*, SyncDirectionConfig>>& directCfgs,
                      bool moveToRecycler,
                      bool& warnRecyclerMissing,
                      ProcessCallback& callback /*throw X*/) //throw X
{
    assert(std::all_of(selectionL.begin(), selectionL.end(), [](const FileSystemObject* fsObj) { return !fsObj->isEmpty<SelectSide::left >(); }));
    assert(std::all_of(selectionR.begin(), selectionR.end(), [](const FileSystemObject* fsObj) { return !fsObj->isEmpty<SelectSide::right>(); }));

    const int itemCount = static_cast<int>(selectionL.size() + selectionR.size());
    callback.initNewPhase(itemCount, 0, ProcessPhase::none); //throw X
    //------------------------------------------------------------------------------

    ZEN_ON_SCOPE_EXIT
    (
        //*INDENT-OFF*
        for (const auto& [baseFolder, dirCfg] : directCfgs)
            baseFolder->removeDoubleEmpty();
        //*INDENT-ON*
    );

    //build up mapping from base directory to corresponding direction config
    std::unordered_map<const BaseFolderPair*, SyncDirectionConfig> baseFolderCfgs;
    for (const auto& [baseFolder, dirCfg] : directCfgs)
        baseFolderCfgs[baseFolder] = dirCfg;

    bool recyclerMissingReportOnce = false;
    deleteFilesOneSide<SelectSide::left >(selectionL, moveToRecycler, recyclerMissingReportOnce, warnRecyclerMissing, baseFolderCfgs, callback); //throw X
    deleteFilesOneSide<SelectSide::right>(selectionR, moveToRecycler, recyclerMissingReportOnce, warnRecyclerMissing, baseFolderCfgs, callback); //
}

//############################################################################################################

namespace
{
template <SelectSide side>
void renameItemsOneSide(const std::vector<FileSystemObject*>& selection,
                        const std::span<const Zstring> newNames,
                        const std::unordered_map<const BaseFolderPair*, SyncDirectionConfig>& baseFolderCfgs,
                        PhaseCallback& callback /*throw X*/) //throw X
{
    assert(selection.size() == newNames.size());

    const std::wstring txtRenamingFileXtoY_  {_("Renaming file %x to %y")};
    const std::wstring txtRenamingLinkXtoY_  {_("Renaming symbolic link %x to %y")};
    const std::wstring txtRenamingFolderXtoY_{_("Renaming folder %x to %y")};

    for (size_t i = 0; i < selection.size(); ++i)
        tryReportingError([&]
    {
        FileSystemObject& fsObj = *selection[i];
        const Zstring& newName = newNames[i];

        assert(!fsObj.isEmpty<side>());

        auto haveNameClash = [newNameNorm = getUnicodeNormalForm(newName)](const FileSystemObject& fsObj2)
        {
            return !fsObj2.isEmpty<side>() && getUnicodeNormalForm(fsObj2.getItemName<side>()) == newNameNorm;
        };

        const bool nameAlreadyExisting = [&]
        {
            for (const FilePair& file : fsObj.parent().refSubFiles())
                if (haveNameClash(file))
                    return true;

            for (const SymlinkPair& symlink : fsObj.parent().refSubLinks())
                if (haveNameClash(symlink))
                    return true;

            for (const FolderPair& folder : fsObj.parent().refSubFolders())
                if (haveNameClash(folder))
                    return true;
            return false;
        }();

        //---------------------------------------------------------------
        ItemStatReporter statReporter(1, 0, callback);

        const std::wstring* txtRenamingXtoY_ = nullptr;
        visitFSObject(fsObj,
        [&](const FolderPair&   folder) { txtRenamingXtoY_ = &txtRenamingFolderXtoY_; },
        [&](const FilePair&       file) { txtRenamingXtoY_ = &txtRenamingFileXtoY_; },
        [&](const SymlinkPair& symlink) { txtRenamingXtoY_ = &txtRenamingLinkXtoY_; });

        reportInfo(replaceCpy(replaceCpy(*txtRenamingXtoY_, L"%x", fmtPath(AFS::getDisplayPath(fsObj.getAbstractPath<side>()))),
                              L"%y", fmtPath(newName)), statReporter); //throw X

        if (haveNameClash(fsObj))
            return assert(false); //theoretically possible, but practically showRenameDialog() won't return until there is an actual name change

        if (nameAlreadyExisting) //avoid inconsistent file model: expecting moveAndRenameItem() to fail (ERROR_ALREADY_EXISTS) is not good enough
            return callback.reportFatalError(replaceCpy(replaceCpy(_("Cannot rename %x to %y."),
                                                                   L"%x", fmtPath(AFS::getDisplayPath(fsObj.getAbstractPath<side>()))),
                                                        L"%y", fmtPath(newName)) + L"\n\n" +
                                             replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(newName))); //throw X

        AFS::moveAndRenameItem(fsObj.getAbstractPath<side>(),
                               AFS::appendRelPath(fsObj.parent().getAbstractPath<side>(), newName)); //throw FileError, (ErrorMoveUnsupported)
        //------- no-throw from here on -------
        statReporter.reportDelta(1, 0);

        const CompareFileResult catOld = fsObj.getCategory();

        fsObj.setItemName<side>(newName);

        warn_static("TODO: some users want to manually fix renamed folders/files: combine them here, don't require a re-compare!")

        //update sync direction: don't call redetermineSyncDirection() because user may have manually changed directions
        if (catOld == CompareFileResult::FILE_EQUAL)
        {
            const SyncDirection newDir = [&]
            {
                const SyncDirectionConfig& dirCfg = baseFolderCfgs.find(&fsObj.base())->second; //not found? let it crash!

                if (const DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&dirCfg.dirs))
                    return side == SelectSide::left ? diffDirs->leftNewer : diffDirs->rightNewer;
                else
                {
                    const DirectionByChange& changeDirs = std::get<DirectionByChange>(dirCfg.dirs);
                    return side == SelectSide::left ? changeDirs.left.update : changeDirs.right.update;
                }
            }();

            fsObj.setSyncDir(newDir); //folder? => do not recurse!
        }
        //else: keep old syncDir_
        else if (fsObj.getCategory() == FILE_EQUAL) //edge-case, but possible
            fsObj.setSyncDir(SyncDirection::none); //shouldn't matter, but avoids hitting some asserts

    }, callback); //throw X
}
}


void fff::renameItems(const std::vector<FileSystemObject*>& selectionL,
                      const std::span<const Zstring> newNamesL,
                      const std::vector<FileSystemObject*>& selectionR,
                      const std::span<const Zstring> newNamesR,
                      const std::vector<std::pair<BaseFolderPair*, SyncDirectionConfig>>& directCfgs,
                      ProcessCallback& callback /*throw X*/) //throw X
{
    assert(std::all_of(selectionL.begin(), selectionL.end(), [](const FileSystemObject* fsObj) { return !fsObj->isEmpty<SelectSide::left >(); }));
    assert(std::all_of(selectionR.begin(), selectionR.end(), [](const FileSystemObject* fsObj) { return !fsObj->isEmpty<SelectSide::right>(); }));

    const int itemCount = static_cast<int>(selectionL.size() + selectionR.size());
    callback.initNewPhase(itemCount, 0, ProcessPhase::none); //throw X
    //------------------------------------------------------------------------------

    //build up mapping from base directory to corresponding direction config
    std::unordered_map<const BaseFolderPair*, SyncDirectionConfig> baseFolderCfgs;
    for (const auto& [baseFolder, dirCfg] : directCfgs)
        baseFolderCfgs[baseFolder] = dirCfg;

    renameItemsOneSide<SelectSide::left >(selectionL, newNamesL, baseFolderCfgs, callback); //throw X
    renameItemsOneSide<SelectSide::right>(selectionR, newNamesR, baseFolderCfgs, callback); //
}

//############################################################################################################

void fff::deleteListOfFiles(const std::vector<Zstring>& filesToDeletePaths,
                            std::vector<Zstring>& deletedPaths,
                            bool moveToRecycler,
                            bool& warnRecyclerMissing,
                            ProcessCallback& cb /*throw X*/) //throw X
{
    assert(deletedPaths.empty());

    cb.initNewPhase(static_cast<int>(filesToDeletePaths.size()), 0 /*bytesTotal*/, ProcessPhase::none); //throw X

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
                              L" [" + _("Recycle bin unavailable") + L']', PhaseCallback::MsgType::warning); //throw X
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
        catch (const FileError& e) { logExtraError(e.toString()); }
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

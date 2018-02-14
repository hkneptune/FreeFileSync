// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "synchronization.h"
#include <tuple>
#include <zen/process_priority.h>
#include <zen/perf.h>
#include <zen/guid.h>
#include <zen/crc.h>
#include "algorithm.h"
#include "lib/db_file.h"
#include "lib/dir_exist_async.h"
#include "lib/status_handler_impl.h"
#include "lib/versioning.h"
#include "lib/binary.h"
#include "fs/concrete.h"
#include "fs/native.h"

    #include <unistd.h> //fsync
    #include <fcntl.h>  //open

using namespace zen;
using namespace fff;


namespace
{
inline
int getCUD(const SyncStatistics& stat)
{
    return stat.createCount() +
           stat.updateCount() +
           stat.deleteCount();
}
}


SyncStatistics::SyncStatistics(const FolderComparison& folderCmp)
{
    std::for_each(begin(folderCmp), end(folderCmp), [&](const BaseFolderPair& baseFolder) { recurse(baseFolder); });
}


SyncStatistics::SyncStatistics(const ContainerObject& hierObj)
{
    recurse(hierObj);
}


SyncStatistics::SyncStatistics(const FilePair& file)
{
    processFile(file);
    ++rowsTotal_;
}


inline
void SyncStatistics::recurse(const ContainerObject& hierObj)
{
    for (const FilePair& file : hierObj.refSubFiles())
        processFile(file);
    for (const SymlinkPair& link : hierObj.refSubLinks())
        processLink(link);
    for (const FolderPair& folder : hierObj.refSubFolders())
        processFolder(folder);

    rowsTotal_ += hierObj.refSubFolders().size();
    rowsTotal_ += hierObj.refSubFiles  ().size();
    rowsTotal_ += hierObj.refSubLinks  ().size();
}


inline
void SyncStatistics::processFile(const FilePair& file)
{
    switch (file.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_CREATE_NEW_LEFT:
            ++createLeft_;
            bytesToProcess_ += static_cast<int64_t>(file.getFileSize<RIGHT_SIDE>());
            break;

        case SO_CREATE_NEW_RIGHT:
            ++createRight_;
            bytesToProcess_ += static_cast<int64_t>(file.getFileSize<LEFT_SIDE>());
            break;

        case SO_DELETE_LEFT:
            ++deleteLeft_;
            physicalDeleteLeft_ = true;
            break;

        case SO_DELETE_RIGHT:
            ++deleteRight_;
            physicalDeleteRight_ = true;
            break;

        case SO_MOVE_LEFT_TO:
            ++updateLeft_;
            //physicalDeleteLeft_ ? -> usually, no; except when falling back to "copy + delete"
            break;

        case SO_MOVE_RIGHT_TO:
            ++updateRight_;
            break;

        case SO_MOVE_LEFT_FROM:  //ignore; already counted
        case SO_MOVE_RIGHT_FROM: //
            break;

        case SO_OVERWRITE_LEFT:
            ++updateLeft_;
            bytesToProcess_ += static_cast<int64_t>(file.getFileSize<RIGHT_SIDE>());
            physicalDeleteLeft_ = true;
            break;

        case SO_OVERWRITE_RIGHT:
            ++updateRight_;
            bytesToProcess_ += static_cast<int64_t>(file.getFileSize<LEFT_SIDE>());
            physicalDeleteRight_ = true;
            break;

        case SO_UNRESOLVED_CONFLICT:
            conflictMsgs_.push_back({ file.getPairRelativePath(), file.getSyncOpConflict() });
            break;

        case SO_COPY_METADATA_TO_LEFT:
            ++updateLeft_;
            break;

        case SO_COPY_METADATA_TO_RIGHT:
            ++updateRight_;
            break;

        case SO_DO_NOTHING:
        case SO_EQUAL:
            break;
    }
}


inline
void SyncStatistics::processLink(const SymlinkPair& link)
{
    switch (link.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_CREATE_NEW_LEFT:
            ++createLeft_;
            break;

        case SO_CREATE_NEW_RIGHT:
            ++createRight_;
            break;

        case SO_DELETE_LEFT:
            ++deleteLeft_;
            physicalDeleteLeft_ = true;
            break;

        case SO_DELETE_RIGHT:
            ++deleteRight_;
            physicalDeleteRight_ = true;
            break;

        case SO_OVERWRITE_LEFT:
        case SO_COPY_METADATA_TO_LEFT:
            ++updateLeft_;
            physicalDeleteLeft_ = true;
            break;

        case SO_OVERWRITE_RIGHT:
        case SO_COPY_METADATA_TO_RIGHT:
            ++updateRight_;
            physicalDeleteRight_ = true;
            break;

        case SO_UNRESOLVED_CONFLICT:
            conflictMsgs_.push_back({ link.getPairRelativePath(), link.getSyncOpConflict() });
            break;

        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_LEFT_TO:
        case SO_MOVE_RIGHT_TO:
            assert(false);
        case SO_DO_NOTHING:
        case SO_EQUAL:
            break;
    }
}


inline
void SyncStatistics::processFolder(const FolderPair& folder)
{
    switch (folder.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_CREATE_NEW_LEFT:
            ++createLeft_;
            break;

        case SO_CREATE_NEW_RIGHT:
            ++createRight_;
            break;

        case SO_DELETE_LEFT: //if deletion variant == versioning with user-defined directory existing on other volume, this results in a full copy + delete operation!
            ++deleteLeft_;    //however we cannot (reliably) anticipate this situation, fortunately statistics can be adapted during sync!
            physicalDeleteLeft_ = true;
            break;

        case SO_DELETE_RIGHT:
            ++deleteRight_;
            physicalDeleteRight_ = true;
            break;

        case SO_UNRESOLVED_CONFLICT:
            conflictMsgs_.push_back({ folder.getPairRelativePath(), folder.getSyncOpConflict() });
            break;

        case SO_OVERWRITE_LEFT:
        case SO_COPY_METADATA_TO_LEFT:
            ++updateLeft_;
            break;

        case SO_OVERWRITE_RIGHT:
        case SO_COPY_METADATA_TO_RIGHT:
            ++updateRight_;
            break;

        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_LEFT_TO:
        case SO_MOVE_RIGHT_TO:
            assert(false);
        case SO_DO_NOTHING:
        case SO_EQUAL:
            break;
    }

    recurse(folder); //since we model logical stats, we recurse, even if deletion variant is "recycler" or "versioning + same volume", which is a single physical operation!
}

//-----------------------------------------------------------------------------------------------------------

std::vector<FolderPairSyncCfg> fff::extractSyncCfg(const MainConfiguration& mainCfg)
{
    //merge first and additional pairs
    std::vector<FolderPairEnh> allPairs = { mainCfg.firstPair };
    append(allPairs, mainCfg.additionalPairs);

    std::vector<FolderPairSyncCfg> output;

    //process all pairs
    for (const FolderPairEnh& fp : allPairs)
    {
        SyncConfig syncCfg = fp.altSyncConfig.get() ? *fp.altSyncConfig : mainCfg.syncCfg;

        output.push_back(
            FolderPairSyncCfg(syncCfg.directionCfg.var == DirectionConfig::TWO_WAY || detectMovedFilesEnabled(syncCfg.directionCfg),
                              syncCfg.handleDeletion,
                              syncCfg.versioningStyle,
                              syncCfg.versioningFolderPhrase,
                              syncCfg.directionCfg.var));
    }
    return output;
}

//------------------------------------------------------------------------------------------------------------

namespace
{
inline
Opt<SelectedSide> getTargetDirection(SyncOperation syncOp)
{
    switch (syncOp)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_DELETE_LEFT:
        case SO_OVERWRITE_LEFT:
        case SO_COPY_METADATA_TO_LEFT:
        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_LEFT_TO:
            return LEFT_SIDE;

        case SO_CREATE_NEW_RIGHT:
        case SO_DELETE_RIGHT:
        case SO_OVERWRITE_RIGHT:
        case SO_COPY_METADATA_TO_RIGHT:
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_RIGHT_TO:
            return RIGHT_SIDE;

        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            break; //nothing to do
    }
    return NoValue();
}


//test if user accidentally selected the wrong folders to sync
bool significantDifferenceDetected(const SyncStatistics& folderPairStat)
{
    //initial file copying shall not be detected as major difference
    if ((folderPairStat.createCount< LEFT_SIDE>() == 0 ||
         folderPairStat.createCount<RIGHT_SIDE>() == 0) &&
        folderPairStat.updateCount  () == 0 &&
        folderPairStat.deleteCount  () == 0 &&
        folderPairStat.conflictCount() == 0)
        return false;

    const int nonMatchingRows = folderPairStat.createCount() +
                                folderPairStat.deleteCount();
    //folderPairStat.updateCount() +  -> not relevant when testing for "wrong folder selected"
    //folderPairStat.conflictCount();

    return nonMatchingRows >= 10 && nonMatchingRows > 0.5 * folderPairStat.rowCount();
}

//#################################################################################################################

class DeletionHandling //abstract deletion variants: permanently, recycle bin, user-defined directory
{
public:
    DeletionHandling(const AbstractPath& baseFolderPath,
                     DeletionPolicy handleDel, //nothrow!
                     const Zstring& versioningFolderPhrase,
                     VersioningStyle versioningStyle,
                     const TimeComp& timeStamp,
                     ProcessCallback& procCallback);
    ~DeletionHandling()
    {
        //always (try to) clean up, even if synchronization is aborted!
        try
        {
            tryCleanup(false /*allowCallbackException*/); //throw FileError, (throw X)
        }
        catch (FileError&) {}
        catch (...) { assert(false); } //what is this?
        /*
        may block heavily, but still do not allow user callback:
        -> avoid throwing user cancel exception again, leading to incomplete clean-up!
        */
    }

    //clean-up temporary directory (recycle bin optimization)
    void tryCleanup(bool allowCallbackException); //throw FileError; throw X -> call this in non-exceptional coding, i.e. somewhere after sync!

    template <class Function> void removeFileWithCallback (const FileDescriptor& fileDescr, const Zstring& relativePath, Function onNotifyItemDeletion, const IOCallback& notifyUnbufferedIO); //
    template <class Function> void removeDirWithCallback  (const AbstractPath& dirPath,     const Zstring& relativePath, Function onNotifyItemDeletion, const IOCallback& notifyUnbufferedIO); //throw FileError
    template <class Function> void removeLinkWithCallback (const AbstractPath& linkPath,    const Zstring& relativePath, Function onNotifyItemDeletion); //

    const std::wstring& getTxtRemovingFile   () const { return txtRemovingFile_;    } //
    const std::wstring& getTxtRemovingFolder () const { return txtRemovingFolder_;  } //buffered status texts
    const std::wstring& getTxtRemovingSymLink() const { return txtRemovingSymlink_; } //

private:
    DeletionHandling           (const DeletionHandling&) = delete;
    DeletionHandling& operator=(const DeletionHandling&) = delete;

    AFS::RecycleSession& getOrCreateRecyclerSession() //throw FileError => dont create in constructor!!!
    {
        assert(deletionPolicy_ == DeletionPolicy::RECYCLER);
        if (!recyclerSession_.get())
            recyclerSession_ =  AFS::createRecyclerSession(baseFolderPath_); //throw FileError
        return *recyclerSession_;
    }

    FileVersioner& getOrCreateVersioner() //throw FileError => dont create in constructor!!!
    {
        assert(deletionPolicy_ == DeletionPolicy::VERSIONING);
        if (!versioner_.get())
            versioner_ = std::make_unique<FileVersioner>(versioningFolderPath_, versioningStyle_, timeStamp_); //throw FileError
        return *versioner_;
    }

    ProcessCallback& procCallback_;

    const DeletionPolicy deletionPolicy_; //keep it invariant! e.g. consider getOrCreateVersioner() one-time construction!

    const AbstractPath baseFolderPath_;
    std::unique_ptr<AFS::RecycleSession> recyclerSession_;

    //used only for DeletionPolicy::VERSIONING:
    const AbstractPath versioningFolderPath_;
    const VersioningStyle versioningStyle_;
    const TimeComp timeStamp_;
    std::unique_ptr<FileVersioner> versioner_; //throw FileError in constructor => create on demand!

    //buffer status texts:
    std::wstring txtRemovingFile_;
    std::wstring txtRemovingSymlink_;
    std::wstring txtRemovingFolder_;

    const std::wstring txtMovingFile_;
    const std::wstring txtMovingFolder_;
};


DeletionHandling::DeletionHandling(const AbstractPath& baseFolderPath,
                                   DeletionPolicy handleDel, //nothrow!
                                   const Zstring& versioningFolderPhrase,
                                   VersioningStyle versioningStyle,
                                   const TimeComp& timeStamp,
                                   ProcessCallback& procCallback) :
    procCallback_(procCallback),
    deletionPolicy_(handleDel),
    baseFolderPath_(baseFolderPath),
    versioningFolderPath_(createAbstractPath(versioningFolderPhrase)),
    versioningStyle_(versioningStyle),
    timeStamp_(timeStamp),
    txtMovingFile_  (_("Moving file %x to %y")),
    txtMovingFolder_(_("Moving folder %x to %y"))
{
    switch (deletionPolicy_)
    {
        case DeletionPolicy::PERMANENT:
            txtRemovingFile_    = _("Deleting file %x"         );
            txtRemovingFolder_  = _("Deleting folder %x"       );
            txtRemovingSymlink_ = _("Deleting symbolic link %x");
            break;

        case DeletionPolicy::RECYCLER:
            txtRemovingFile_    = _("Moving file %x to the recycle bin"         );
            txtRemovingFolder_  = _("Moving folder %x to the recycle bin"       );
            txtRemovingSymlink_ = _("Moving symbolic link %x to the recycle bin");
            break;

        case DeletionPolicy::VERSIONING:
            txtRemovingFile_    = replaceCpy(_("Moving file %x to %y"         ), L"%y", fmtPath(AFS::getDisplayPath(versioningFolderPath_)));
            txtRemovingFolder_  = replaceCpy(_("Moving folder %x to %y"       ), L"%y", fmtPath(AFS::getDisplayPath(versioningFolderPath_)));
            txtRemovingSymlink_ = replaceCpy(_("Moving symbolic link %x to %y"), L"%y", fmtPath(AFS::getDisplayPath(versioningFolderPath_)));
            break;
    }
}


void DeletionHandling::tryCleanup(bool allowCallbackException) //throw FileError; throw X
{
    switch (deletionPolicy_)
    {
        case DeletionPolicy::PERMANENT:
            break;

        case DeletionPolicy::RECYCLER:
            if (recyclerSession_.get())
            {
                auto notifyDeletionStatus = [&](const std::wstring& displayPath)
                {
                    try
                    {
                        if (!displayPath.empty())
                            procCallback_.reportStatus(replaceCpy(txtRemovingFile_, L"%x", fmtPath(displayPath))); //throw X
                        else
                            procCallback_.requestUiRefresh(); //throw X
                    }
                    catch (...)
                    {
                        if (allowCallbackException)
                            throw;
                    }
                };

                //move content of temporary directory to recycle bin in a single call
                getOrCreateRecyclerSession().tryCleanup(notifyDeletionStatus); //throw FileError
            }
            break;

        case DeletionPolicy::VERSIONING:
            //if (versioner.get())
            //{
            //    if (allowUserCallback)
            //    {
            //        procCallback_.reportStatus(_("Removing old versions...")); //throw ?
            //        versioner->limitVersions([&] { procCallback_.requestUiRefresh(); /*throw ? */ }); //throw FileError
            //    }
            //    else
            //        versioner->limitVersions([] {}); //throw FileError
            //}
            break;
    }
}


template <class Function>
void DeletionHandling::removeDirWithCallback(const AbstractPath& folderPath,
                                             const Zstring& relativePath,
                                             Function onNotifyItemDeletion,
                                             const IOCallback& notifyUnbufferedIO) //throw FileError
{
    switch (deletionPolicy_)
    {
        case DeletionPolicy::PERMANENT:
        {
            auto notifyDeletion = [&](const std::wstring& statusText, const std::wstring& displayPath)
            {
                onNotifyItemDeletion(); //it would be more correct to report *after* work was done!
                procCallback_.reportStatus(replaceCpy(statusText, L"%x", fmtPath(displayPath)));
            };
            auto onBeforeFileDeletion = [&](const std::wstring& displayPath) { notifyDeletion(txtRemovingFile_,   displayPath); };
            auto onBeforeDirDeletion  = [&](const std::wstring& displayPath) { notifyDeletion(txtRemovingFolder_, displayPath); };

            AFS::removeFolderIfExistsRecursion(folderPath, onBeforeFileDeletion, onBeforeDirDeletion); //throw FileError
        }
        break;

        case DeletionPolicy::RECYCLER:
            if (getOrCreateRecyclerSession().recycleItem(folderPath, relativePath)) //throw FileError
                onNotifyItemDeletion(); //moving to recycler is ONE logical operation, irrespective of the number of child elements!
            break;

        case DeletionPolicy::VERSIONING:
        {
            auto notifyMove = [&](const std::wstring& statusText, const std::wstring& displayPathFrom, const std::wstring& displayPathTo)
            {
                onNotifyItemDeletion(); //it would be more correct to report *after* work was done!
                procCallback_.reportStatus(replaceCpy(replaceCpy(statusText, L"%x", L"\n" + fmtPath(displayPathFrom)), L"%y", L"\n" + fmtPath(displayPathTo)));
            };
            auto onBeforeFileMove   = [&](const std::wstring& displayPathFrom, const std::wstring& displayPathTo) { notifyMove(txtMovingFile_,   displayPathFrom, displayPathTo); };
            auto onBeforeFolderMove = [&](const std::wstring& displayPathFrom, const std::wstring& displayPathTo) { notifyMove(txtMovingFolder_, displayPathFrom, displayPathTo); };

            getOrCreateVersioner().revisionFolder(folderPath, relativePath, onBeforeFileMove, onBeforeFolderMove, notifyUnbufferedIO); //throw FileError
        }
        break;
    }
}


template <class Function>
void DeletionHandling::removeFileWithCallback(const FileDescriptor& fileDescr,
                                              const Zstring& relativePath,
                                              Function onNotifyItemDeletion,
                                              const IOCallback& notifyUnbufferedIO) //throw FileError
{
    bool deleted = false;

    if (endsWith(relativePath, AFS::TEMP_FILE_ENDING)) //special rule for .ffs_tmp files: always delete permanently!
        deleted = AFS::removeFileIfExists(fileDescr.path); //throw FileError
    else
        switch (deletionPolicy_)
        {
            case DeletionPolicy::PERMANENT:
                deleted = AFS::removeFileIfExists(fileDescr.path); //throw FileError
                break;
            case DeletionPolicy::RECYCLER:
                deleted = getOrCreateRecyclerSession().recycleItem(fileDescr.path, relativePath); //throw FileError
                break;
            case DeletionPolicy::VERSIONING:
                deleted = getOrCreateVersioner().revisionFile(fileDescr, relativePath, notifyUnbufferedIO); //throw FileError
                break;
        }
    if (deleted)
        onNotifyItemDeletion();
}


template <class Function> inline
void DeletionHandling::removeLinkWithCallback(const AbstractPath& linkPath, const Zstring& relativePath, Function onNotifyItemDeletion) //throw FileError
{
    bool deleted = false;

    switch (deletionPolicy_)
    {
        case DeletionPolicy::PERMANENT:
            deleted = AFS::removeSymlinkIfExists(linkPath); //throw FileError
            break;
        case DeletionPolicy::RECYCLER:
            deleted = getOrCreateRecyclerSession().recycleItem(linkPath, relativePath); //throw FileError
            break;
        case DeletionPolicy::VERSIONING:
            deleted = getOrCreateVersioner().revisionSymlink(linkPath, relativePath); //throw FileError
            break;
    }
    if (deleted)
        onNotifyItemDeletion();
}

//------------------------------------------------------------------------------------------------------------

/*
  DeletionPolicy::PERMANENT:  deletion frees space
  DeletionPolicy::RECYCLER:   won't free space until recycler is full, but then frees space
  DeletionPolicy::VERSIONING: depends on whether versioning folder is on a different volume
-> if deleted item is a followed symlink, no space is freed
-> created/updated/deleted item may be on a different volume than base directory: consider symlinks, junctions!

=> generally assume deletion frees space; may avoid false positive disk space warnings for recycler and versioning
*/
class MinimumDiskSpaceNeeded
{
public:
    static std::pair<int64_t, int64_t> calculate(const BaseFolderPair& baseFolder)
    {
        MinimumDiskSpaceNeeded inst;
        inst.recurse(baseFolder);
        return { inst.spaceNeededLeft_, inst.spaceNeededRight_ };
    }

private:
    void recurse(const ContainerObject& hierObj)
    {
        //don't process directories

        //process files
        for (const FilePair& file : hierObj.refSubFiles())
            switch (file.getSyncOperation()) //evaluate comparison result and sync direction
            {
                case SO_CREATE_NEW_LEFT:
                    spaceNeededLeft_ += static_cast<int64_t>(file.getFileSize<RIGHT_SIDE>());
                    break;

                case SO_CREATE_NEW_RIGHT:
                    spaceNeededRight_ += static_cast<int64_t>(file.getFileSize<LEFT_SIDE>());
                    break;

                case SO_DELETE_LEFT:
                    //if (freeSpaceDelLeft_)
                    spaceNeededLeft_ -= static_cast<int64_t>(file.getFileSize<LEFT_SIDE>());
                    break;

                case SO_DELETE_RIGHT:
                    //if (freeSpaceDelRight_)
                    spaceNeededRight_ -= static_cast<int64_t>(file.getFileSize<RIGHT_SIDE>());
                    break;

                case SO_OVERWRITE_LEFT:
                    //if (freeSpaceDelLeft_)
                    spaceNeededLeft_ -= static_cast<int64_t>(file.getFileSize<LEFT_SIDE>());
                    spaceNeededLeft_ += static_cast<int64_t>(file.getFileSize<RIGHT_SIDE>());
                    break;

                case SO_OVERWRITE_RIGHT:
                    //if (freeSpaceDelRight_)
                    spaceNeededRight_ -= static_cast<int64_t>(file.getFileSize<RIGHT_SIDE>());
                    spaceNeededRight_ += static_cast<int64_t>(file.getFileSize<LEFT_SIDE>());
                    break;

                case SO_DO_NOTHING:
                case SO_EQUAL:
                case SO_UNRESOLVED_CONFLICT:
                case SO_COPY_METADATA_TO_LEFT:
                case SO_COPY_METADATA_TO_RIGHT:
                case SO_MOVE_LEFT_FROM:
                case SO_MOVE_RIGHT_FROM:
                case SO_MOVE_LEFT_TO:
                case SO_MOVE_RIGHT_TO:
                    break;
            }

        //symbolic links
        //[...]

        //recurse into sub-dirs
        for (const FolderPair& folder : hierObj.refSubFolders())
            recurse(folder);
    }

    int64_t spaceNeededLeft_  = 0;
    int64_t spaceNeededRight_ = 0;
};

//----------------------------------------------------------------------------------------

class SynchronizeFolderPair
{
public:
    SynchronizeFolderPair(ProcessCallback& procCallback,
                          bool verifyCopiedFiles,
                          bool copyFilePermissions,
                          bool failSafeFileCopy,
                          std::vector<FileError>& errorsModTime,
                          DeletionHandling& delHandlingLeft,
                          DeletionHandling& delHandlingRight) :
        procCallback_(procCallback),
        errorsModTime_(errorsModTime),
        delHandlingLeft_(delHandlingLeft),
        delHandlingRight_(delHandlingRight),
        verifyCopiedFiles_(verifyCopiedFiles),
        copyFilePermissions_(copyFilePermissions),
        failSafeFileCopy_(failSafeFileCopy) {}

    void startSync(BaseFolderPair& baseFolder)
    {
        runZeroPass(baseFolder);       //first process file moves
        runPass<PASS_ONE>(baseFolder); //delete files (or overwrite big ones with smaller ones)
        runPass<PASS_TWO>(baseFolder); //copy rest
    }

private:
    enum PassNo
    {
        PASS_ONE, //delete files
        PASS_TWO, //create, modify
        PASS_NEVER //skip item
    };

    static PassNo getPass(const FilePair&    file);
    static PassNo getPass(const SymlinkPair& link);
    static PassNo getPass(const FolderPair&  folder);

    template <SelectedSide side>
    void prepare2StepMove(FilePair& sourceObj, FilePair& targetObj); //throw FileError
    bool createParentFolder(FileSystemObject& fsObj); //throw FileError
    template <SelectedSide side>
    void manageFileMove(FilePair& sourceObj, FilePair& targetObj); //throw FileError

    void runZeroPass(ContainerObject& hierObj);
    template <PassNo pass>
    void runPass(ContainerObject& hierObj); //throw X

    void synchronizeFile(FilePair& file);
    template <SelectedSide side> void synchronizeFileInt(FilePair& file, SyncOperation syncOp);

    void synchronizeLink(SymlinkPair& link);
    template <SelectedSide sideTrg> void synchronizeLinkInt(SymlinkPair& link, SyncOperation syncOp);

    void synchronizeFolder(FolderPair& folder);
    template <SelectedSide sideTrg> void synchronizeFolderInt(FolderPair& folder, SyncOperation syncOp);

    void reportStatus(const std::wstring& rawText, const std::wstring& displayPath) const { procCallback_.reportStatus(replaceCpy(rawText, L"%x", fmtPath(displayPath))); }
    void reportInfo  (const std::wstring& rawText, const std::wstring& displayPath) const { procCallback_.reportInfo  (replaceCpy(rawText, L"%x", fmtPath(displayPath))); }
    void reportInfo  (const std::wstring& rawText,
                      const std::wstring& displayPath1,
                      const std::wstring& displayPath2) const
    {
        procCallback_.reportInfo(replaceCpy(replaceCpy(rawText, L"%x", L"\n" + fmtPath(displayPath1)), L"%y", L"\n" + fmtPath(displayPath2)));
    }

    //target existing after onDeleteTargetFile(): undefined behavior! (fail/overwrite/auto-rename)
    AFS::FileCopyResult copyFileWithCallback(const FileDescriptor& sourceDescr, //throw FileError
                                             const AbstractPath& targetPath,
                                             const std::function<void()>& onDeleteTargetFile,
                                             const IOCallback& notifyUnbufferedIO) const;

    template <SelectedSide side>
    DeletionHandling& getDelHandling();

    ProcessCallback& procCallback_;
    std::vector<FileError>& errorsModTime_;

    DeletionHandling& delHandlingLeft_;
    DeletionHandling& delHandlingRight_;

    const bool verifyCopiedFiles_;
    const bool copyFilePermissions_;
    const bool failSafeFileCopy_;

    //preload status texts
    const std::wstring txtCreatingFile     {_("Creating file %x"         )};
    const std::wstring txtCreatingLink     {_("Creating symbolic link %x")};
    const std::wstring txtCreatingFolder   {_("Creating folder %x"       )};
    const std::wstring txtOverwritingFile  {_("Updating file %x"         )};
    const std::wstring txtOverwritingLink  {_("Updating symbolic link %x")};
    const std::wstring txtVerifying        {_("Verifying file %x"        )};
    const std::wstring txtWritingAttributes{_("Updating attributes of %x")};
    const std::wstring txtMovingFile       {_("Moving file %x to %y"     )};
};

//---------------------------------------------------------------------------------------------------------------

template <> inline
DeletionHandling& SynchronizeFolderPair::getDelHandling<LEFT_SIDE>() { return delHandlingLeft_; }

template <> inline
DeletionHandling& SynchronizeFolderPair::getDelHandling<RIGHT_SIDE>() { return delHandlingRight_; }

/*
__________________________
|Move algorithm, 0th pass|
--------------------------
1. loop over hierarchy and find "move source"

2. check whether parent directory of "move source" is going to be deleted or location of "move source" may lead to name clash with other dir/symlink
   -> no:  delay move until 2nd pass

3. create move target's parent directory recursively + execute move
   do we have name clash?
   -> prepare a 2-step move operation: 1. move source to base and update "move target" accordingly 2. delay move until 2nd pass

4. If any of the operations above did not succeed (even after retry), update statistics and revert to "copy + delete"
   Note: first pass may delete "move source"!!!

__________________
|killer-scenarios|
------------------
propagate the following move sequences:
I) a -> a/a      caveat sync'ing parent directory first leads to circular dependency!

II) a/a -> a     caveat: fixing name clash will remove source!

III) c -> d      caveat: move-sequence needs to be processed in correct order!
     b -> c/b
     a -> b/a
*/

template <class List> inline
bool haveNameClash(const Zstring& shortname, List& m)
{
    return std::any_of(m.begin(), m.end(),
    [&](const typename List::value_type& obj) { return equalFilePath(obj.getPairItemName(), shortname); });
}


template <SelectedSide side>
void SynchronizeFolderPair::prepare2StepMove(FilePair& sourceObj,
                                             FilePair& targetObj) //throw FileError
{
    //generate (hopefully) unique file name to avoid clashing with some remnant ffs_tmp file
    const Zstring shortGuid = printNumber<Zstring>(Zstr("%04x"), static_cast<unsigned int>(getCrc16(generateGUID())));
    const Zstring fileName = sourceObj.getItemName<side>();
    auto it = find_last(fileName.begin(), fileName.end(), Zchar('.')); //gracefully handle case of missing "."

    const Zstring sourceRelPathTmp = Zstring(fileName.begin(), it) + Zchar('.') + shortGuid + AFS::TEMP_FILE_ENDING;
    //-------------------------------------------------------------------------------------------
    //this could still lead to a name-clash in obscure cases, if some file exists on the other side with
    //the very same (.ffs_tmp) name and is copied before the second step of the move is executed
    //good news: even in this pathologic case, this may only prevent the copy of the other file, but not this move

    const AbstractPath sourcePathTmp = AFS::appendRelPath(sourceObj.base().getAbstractPath<side>(), sourceRelPathTmp);

    reportInfo(txtMovingFile,
               AFS::getDisplayPath(sourceObj.getAbstractPath<side>()),
               AFS::getDisplayPath(sourcePathTmp));

    AFS::renameItem(sourceObj.getAbstractPath<side>(), sourcePathTmp); //throw FileError, (ErrorDifferentVolume)

    //TODO: prepare2StepMove: consider ErrorDifferentVolume! e.g. symlink aliasing!

    //update file hierarchy
    FilePair& tempFile = sourceObj.base().addSubFile<side>(afterLast(sourceRelPathTmp, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL), sourceObj.getAttributes<side>());
    static_assert(IsSameType<FixedList<FilePair>, ContainerObject::FileList>::value,
                  "ATTENTION: we're adding to the file list WHILE looping over it! This is only working because FixedList iterators are not invalidated by insertion!");
    sourceObj.removeObject<side>(); //remove only *after* evaluating "sourceObj, side"!
    //note: this new item is *not* considered at the end of 0th pass because "!sourceWillBeDeleted && !haveNameClash"

    //prepare move in second pass
    tempFile.setSyncDir(side == LEFT_SIDE ? SyncDirection::LEFT : SyncDirection::RIGHT);

    targetObj.setMoveRef(tempFile .getId());
    tempFile .setMoveRef(targetObj.getId());

    //NO statistics update!
    procCallback_.requestUiRefresh(); //throw ?
}


bool SynchronizeFolderPair::createParentFolder(FileSystemObject& fsObj) //throw FileError, "false" on name clash
{
    if (auto parentFolder = dynamic_cast<FolderPair*>(&fsObj.parent()))
    {
        if (!createParentFolder(*parentFolder))
            return false;

        //detect (and try to resolve) file type conflicts: 1. symlinks 2. files
        const Zstring& shortname = parentFolder->getPairItemName();
        if (haveNameClash(shortname, parentFolder->parent().refSubLinks()) ||
            haveNameClash(shortname, parentFolder->parent().refSubFiles()))
            return false;

        //in this context "parentFolder" cannot be scheduled for deletion since it contains a "move target"!
        //note: if parentFolder were deleted, we'd end up destroying "fsObj"!
        assert(parentFolder->getSyncOperation() != SO_DELETE_LEFT &&
               parentFolder->getSyncOperation() != SO_DELETE_RIGHT);

        synchronizeFolder(*parentFolder); //throw FileError
    }
    return true;
}


template <SelectedSide side>
void SynchronizeFolderPair::manageFileMove(FilePair& sourceFile,
                                           FilePair& targetFile) //throw FileError
{
    assert((sourceFile.getSyncOperation() == SO_MOVE_LEFT_FROM  && targetFile.getSyncOperation() == SO_MOVE_LEFT_TO  && side == LEFT_SIDE) ||
           (sourceFile.getSyncOperation() == SO_MOVE_RIGHT_FROM && targetFile.getSyncOperation() == SO_MOVE_RIGHT_TO && side == RIGHT_SIDE));

    const bool sourceWillBeDeleted = [&]() -> bool
    {
        if (auto parentFolder = dynamic_cast<const FolderPair*>(&sourceFile.parent()))
        {
            switch (parentFolder->getSyncOperation()) //evaluate comparison result and sync direction
            {
                case SO_DELETE_LEFT:
                case SO_DELETE_RIGHT:
                    return true; //we need to do something about it
                case SO_MOVE_LEFT_FROM:
                case SO_MOVE_RIGHT_FROM:
                case SO_MOVE_LEFT_TO:
                case SO_MOVE_RIGHT_TO:
                case SO_OVERWRITE_LEFT:
                case SO_OVERWRITE_RIGHT:
                case SO_CREATE_NEW_LEFT:
                case SO_CREATE_NEW_RIGHT:
                case SO_DO_NOTHING:
                case SO_EQUAL:
                case SO_UNRESOLVED_CONFLICT:
                case SO_COPY_METADATA_TO_LEFT:
                case SO_COPY_METADATA_TO_RIGHT:
                    break;
            }
        }
        return false;
    }();

    auto haveNameClash = [](const FilePair& file)
    {
        return ::haveNameClash(file.getPairItemName(), file.parent().refSubLinks()) ||
               ::haveNameClash(file.getPairItemName(), file.parent().refSubFolders());
    };

    if (sourceWillBeDeleted || haveNameClash(sourceFile))
    {
        //prepare for move now: - revert to 2-step move on name clashes
        if (haveNameClash(targetFile) ||
            !createParentFolder(targetFile)) //throw FileError
            return prepare2StepMove<side>(sourceFile, targetFile); //throw FileError

        //finally start move! this should work now:
        synchronizeFile(targetFile); //throw FileError
        //SynchronizeFolderPair::synchronizeFileInt() is *not* expecting SO_MOVE_LEFT_FROM/SO_MOVE_RIGHT_FROM => start move from targetFile, not sourceFile!
    }
    //else: sourceFile will not be deleted, and is not standing in the way => delay to second pass
    //note: this case may include new "move sources" from two-step sub-routine!!!
}


//search for file move-operations
void SynchronizeFolderPair::runZeroPass(ContainerObject& hierObj)
{
    for (FilePair& file : hierObj.refSubFiles())
    {
        const SyncOperation syncOp = file.getSyncOperation();
        switch (syncOp) //evaluate comparison result and sync direction
        {
            case SO_MOVE_LEFT_FROM:
            case SO_MOVE_RIGHT_FROM:
                if (FilePair* targetObj = dynamic_cast<FilePair*>(FileSystemObject::retrieve(file.getMoveRef())))
                {
                    FilePair* sourceObj = &file;
                    assert(dynamic_cast<FilePair*>(FileSystemObject::retrieve(targetObj->getMoveRef())) == sourceObj);

                    zen::Opt<std::wstring> errMsg = tryReportingError([&]
                    {
                        if (syncOp == SO_MOVE_LEFT_FROM)
                            this->manageFileMove<LEFT_SIDE>(*sourceObj, *targetObj); //throw FileError
                        else
                            this->manageFileMove<RIGHT_SIDE>(*sourceObj, *targetObj); //
                    }, procCallback_); //throw X?

                    if (errMsg)
                    {
                        //move operation has failed! We cannot allow to continue and have move source's parent directory deleted, messing up statistics!
                        // => revert to ordinary "copy + delete"

                        auto getStats = [&]() -> std::pair<int, int64_t>
                        {
                            SyncStatistics statSrc(*sourceObj);
                            SyncStatistics statTrg(*targetObj);
                            return { getCUD(statSrc) + getCUD(statTrg), statSrc.getBytesToProcess() + statTrg.getBytesToProcess() };
                        };

                        const auto statBefore = getStats();
                        sourceObj->setMoveRef(nullptr);
                        targetObj->setMoveRef(nullptr);
                        const auto statAfter = getStats();
                        //fix statistics total to match "copy + delete"
                        procCallback_.updateTotalData(statAfter.first - statBefore.first, statAfter.second - statBefore.second);
                    }
                }
                else assert(false);
                break;

            case SO_MOVE_LEFT_TO:  //it's enough to try each move-pair *once*
            case SO_MOVE_RIGHT_TO: //
            case SO_DELETE_LEFT:
            case SO_DELETE_RIGHT:
            case SO_OVERWRITE_LEFT:
            case SO_OVERWRITE_RIGHT:
            case SO_CREATE_NEW_LEFT:
            case SO_CREATE_NEW_RIGHT:
            case SO_DO_NOTHING:
            case SO_EQUAL:
            case SO_UNRESOLVED_CONFLICT:
            case SO_COPY_METADATA_TO_LEFT:
            case SO_COPY_METADATA_TO_RIGHT:
                break;
        }
    }

    for (FolderPair& folder : hierObj.refSubFolders())
        runZeroPass(folder); //recurse
}

//---------------------------------------------------------------------------------------------------------------

//1st, 2nd pass requirements:
// - avoid disk space shortage: 1. delete files, 2. overwrite big with small files first
// - support change in type: overwrite file by directory, symlink by file, ect.

inline
SynchronizeFolderPair::PassNo SynchronizeFolderPair::getPass(const FilePair& file)
{
    switch (file.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            return PASS_ONE;

        case SO_OVERWRITE_LEFT:
            return file.getFileSize<LEFT_SIDE>() > file.getFileSize<RIGHT_SIDE>() ? PASS_ONE : PASS_TWO;

        case SO_OVERWRITE_RIGHT:
            return file.getFileSize<LEFT_SIDE>() < file.getFileSize<RIGHT_SIDE>() ? PASS_ONE : PASS_TWO;

        case SO_MOVE_LEFT_FROM:  //
        case SO_MOVE_RIGHT_FROM: // [!]
            return PASS_NEVER;
        case SO_MOVE_LEFT_TO:  //
        case SO_MOVE_RIGHT_TO: //make sure 2-step move is processed in second pass, after move *target* parent directory was created!
            return PASS_TWO;

        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            return PASS_TWO;

        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            return PASS_NEVER;
    }
    assert(false);
    return PASS_TWO; //dummy
}


inline
SynchronizeFolderPair::PassNo SynchronizeFolderPair::getPass(const SymlinkPair& link)
{
    switch (link.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            return PASS_ONE; //make sure to delete symlinks in first pass, and equally named file or dir in second pass: usecase "overwrite symlink with regular file"!

        case SO_OVERWRITE_LEFT:
        case SO_OVERWRITE_RIGHT:
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            return PASS_TWO;

        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_LEFT_TO:
        case SO_MOVE_RIGHT_TO:
            assert(false);
        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            return PASS_NEVER;
    }
    assert(false);
    return PASS_TWO; //dummy
}


inline
SynchronizeFolderPair::PassNo SynchronizeFolderPair::getPass(const FolderPair& folder)
{
    switch (folder.getSyncOperation()) //evaluate comparison result and sync direction
    {
        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            return PASS_ONE;

        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        case SO_OVERWRITE_LEFT:
        case SO_OVERWRITE_RIGHT:
        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            return PASS_TWO;

        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_LEFT_TO:
        case SO_MOVE_RIGHT_TO:
            assert(false);
        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            return PASS_NEVER;
    }
    assert(false);
    return PASS_TWO; //dummy
}


template <SynchronizeFolderPair::PassNo pass>
void SynchronizeFolderPair::runPass(ContainerObject& hierObj) //throw X
{
    //synchronize files:
    for (FilePair& file : hierObj.refSubFiles())
        if (pass == this->getPass(file)) //"this->" required by two-pass lookup as enforced by GCC 4.7
            tryReportingError([&] { synchronizeFile(file); }, procCallback_); //throw X

    //synchronize symbolic links:
    for (SymlinkPair& symlink : hierObj.refSubLinks())
        if (pass == this->getPass(symlink))
            tryReportingError([&] { synchronizeLink(symlink); }, procCallback_); //throw X

    //synchronize folders:
    for (FolderPair& folder : hierObj.refSubFolders())
    {
        if (pass == this->getPass(folder))
            tryReportingError([&] { synchronizeFolder(folder); }, procCallback_); //throw X

        this->runPass<pass>(folder); //recurse
    }
}

//---------------------------------------------------------------------------------------------------------------

inline
void SynchronizeFolderPair::synchronizeFile(FilePair& file)
{
    const SyncOperation syncOp = file.getSyncOperation();

    if (Opt<SelectedSide> sideTrg = getTargetDirection(syncOp))
    {
        if (*sideTrg == LEFT_SIDE)
            synchronizeFileInt<LEFT_SIDE>(file, syncOp);
        else
            synchronizeFileInt<RIGHT_SIDE>(file, syncOp);
    }
}


template <SelectedSide sideTrg>
void SynchronizeFolderPair::synchronizeFileInt(FilePair& file, SyncOperation syncOp)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    switch (syncOp)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        {
            if (auto parentFolder = dynamic_cast<const FolderPair*>(&file.parent()))
                if (parentFolder->isEmpty<sideTrg>()) //BaseFolderPair OTOH is always non-empty and existing in this context => else: fatal error in zen::synchronize()
                    return; //if parent directory creation failed, there's no reason to show more errors!

            //can't use "getAbstractPath<sideTrg>()" as file name is not available!
            const AbstractPath targetPath = file.getAbstractPath<sideTrg>();
            reportInfo(txtCreatingFile, AFS::getDisplayPath(targetPath));

            StatisticsReporter statReporter(1, file.getFileSize<sideSrc>(), procCallback_);
            try
            {
                auto notifyUnbufferedIO = [&](int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                const AFS::FileCopyResult result = copyFileWithCallback({ file.getAbstractPath<sideSrc>(), file.getAttributes<sideSrc>() },
                                                                        targetPath,
                                                                        nullptr, //onDeleteTargetFile: nothing to delete; if existing: undefined behavior! (fail/overwrite/auto-rename)
                                                                        notifyUnbufferedIO); //throw FileError
                if (result.errorModTime)
                    errorsModTime_.push_back(*result.errorModTime); //show all warnings later as a single message

                statReporter.reportDelta(1, 0);

                //update FilePair
                file.setSyncedTo<sideTrg>(file.getItemName<sideSrc>(), result.fileSize,
                                          result.modTime, //target time set from source
                                          result.modTime,
                                          result.targetFileId,
                                          result.sourceFileId,
                                          false, file.isFollowedSymlink<sideSrc>());
            }
            catch (FileError&)
            {
                bool sourceWasDeleted = false;
                try { sourceWasDeleted = !AFS::getItemTypeIfExists(file.getAbstractPath<sideSrc>()); /*throw FileError*/ }
                catch (FileError&) {} //previous exception is more relevant

                if (sourceWasDeleted)
                    file.removeObject<sideSrc>(); //source deleted meanwhile...nothing was done (logical point of view!)
                else
                    throw; //do not check on type (symlink, file, folder) -> if there is a type change, FFS should not be quiet about it!
            }
        }
        break;

        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            reportInfo(getDelHandling<sideTrg>().getTxtRemovingFile(), AFS::getDisplayPath(file.getAbstractPath<sideTrg>()));
            {
                StatisticsReporter statReporter(1, 0, procCallback_);

                auto onNotifyItemDeletion = [&] { statReporter.reportDelta(1, 0); };
                auto notifyUnbufferedIO   = [&](int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                getDelHandling<sideTrg>().removeFileWithCallback({ file.getAbstractPath<sideTrg>(), file.getAttributes<sideTrg>() },
                                                                 file.getPairRelativePath(), onNotifyItemDeletion, notifyUnbufferedIO); //throw FileError

                file.removeObject<sideTrg>(); //update FilePair
            }
            break;

        case SO_MOVE_LEFT_TO:
        case SO_MOVE_RIGHT_TO:
            if (FilePair* moveFrom = dynamic_cast<FilePair*>(FileSystemObject::retrieve(file.getMoveRef())))
            {
                FilePair* moveTo = &file;

                assert((moveFrom->getSyncOperation() == SO_MOVE_LEFT_FROM  && moveTo->getSyncOperation() == SO_MOVE_LEFT_TO  && sideTrg == LEFT_SIDE) ||
                       (moveFrom->getSyncOperation() == SO_MOVE_RIGHT_FROM && moveTo->getSyncOperation() == SO_MOVE_RIGHT_TO && sideTrg == RIGHT_SIDE));

                const AbstractPath pathFrom = moveFrom->getAbstractPath<sideTrg>();
                const AbstractPath pathTo   = moveTo  ->getAbstractPath<sideTrg>();

                reportInfo(txtMovingFile, AFS::getDisplayPath(pathFrom), AFS::getDisplayPath(pathTo));

                StatisticsReporter statReporter(1, 0, procCallback_);

                //TODO: synchronizeFileInt: consider ErrorDifferentVolume! e.g. symlink aliasing!

                AFS::renameItem(pathFrom, pathTo); //throw FileError, (ErrorDifferentVolume)

                statReporter.reportDelta(1, 0);

                //update FilePair
                assert(moveFrom->getFileSize<sideTrg>() == moveTo->getFileSize<sideSrc>());
                moveTo->setSyncedTo<sideTrg>(moveTo->getItemName<sideSrc>(), moveTo->getFileSize<sideSrc>(),
                                             moveFrom->getLastWriteTime<sideTrg>(), //awkward naming! moveFrom is renamed on "sideTrg" side!
                                             moveTo  ->getLastWriteTime<sideSrc>(),
                                             moveFrom->getFileId<sideTrg>(),
                                             moveTo  ->getFileId<sideSrc>(),
                                             moveFrom->isFollowedSymlink<sideTrg>(),
                                             moveTo  ->isFollowedSymlink<sideSrc>());
                moveFrom->removeObject<sideTrg>(); //remove only *after* evaluating "moveFrom, sideTrg"!
            }
            else (assert(false));
            break;

        case SO_OVERWRITE_LEFT:
        case SO_OVERWRITE_RIGHT:
        {
            //respect differences in case of source object:
            const AbstractPath targetPathLogical = AFS::appendRelPath(file.parent().getAbstractPath<sideTrg>(), file.getItemName<sideSrc>());

            AbstractPath targetPathResolvedOld = file.getAbstractPath<sideTrg>(); //support change in case when syncing to case-sensitive SFTP on Windows!
            AbstractPath targetPathResolvedNew = targetPathLogical;
            if (file.isFollowedSymlink<sideTrg>()) //follow link when updating file rather than delete it and replace with regular file!!!
                targetPathResolvedOld = targetPathResolvedNew = AFS::getSymlinkResolvedPath(file.getAbstractPath<sideTrg>()); //throw FileError

            reportInfo(txtOverwritingFile, AFS::getDisplayPath(targetPathResolvedOld));

            StatisticsReporter statReporter(1, file.getFileSize<sideSrc>(), procCallback_);

            if (file.isFollowedSymlink<sideTrg>()) //since we follow the link, we need to sync case sensitivity of the link manually!
                if (file.getItemName<sideTrg>() != file.getItemName<sideSrc>()) //have difference in case?
                    AFS::renameItem(file.getAbstractPath<sideTrg>(), targetPathLogical); //throw FileError, (ErrorDifferentVolume)

            auto notifyUnbufferedIO = [&](int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

            auto onDeleteTargetFile = [&] //delete target at appropriate time
            {
                //reportStatus(this->getDelHandling<sideTrg>().getTxtRemovingFile(), AFS::getDisplayPath(targetPathResolvedOld)); -> superfluous/confuses user

                FileAttributes followedTargetAttr = file.getAttributes<sideTrg>();
                followedTargetAttr.isFollowedSymlink = false;

                this->getDelHandling<sideTrg>().removeFileWithCallback({ targetPathResolvedOld, followedTargetAttr},
                file.getPairRelativePath(), [] {}, notifyUnbufferedIO); //throw FileError;
                //no (logical) item count update desired - but total byte count may change, e.g. move(copy) deleted file to versioning dir

                //file.removeObject<sideTrg>(); -> doesn't make sense for isFollowedSymlink(); "file, sideTrg" evaluated below!

                //if fail-safe file copy is active, then the next operation will be a simple "rename"
                //=> don't risk reportStatus() throwing AbortProcess() leaving the target deleted rather than updated!
                //=> if failSafeFileCopy_ : don't run callbacks that could throw
            };

            const AFS::FileCopyResult result = copyFileWithCallback({ file.getAbstractPath<sideSrc>(), file.getAttributes<sideSrc>() },
                                                                    targetPathResolvedNew,
                                                                    onDeleteTargetFile,
                                                                    notifyUnbufferedIO); //throw FileError
            if (result.errorModTime)
                errorsModTime_.push_back(*result.errorModTime); //show all warnings later as a single message

            statReporter.reportDelta(1, 0); //we model "delete + copy" as ONE logical operation

            //update FilePair
            file.setSyncedTo<sideTrg>(file.getItemName<sideSrc>(), result.fileSize,
                                      result.modTime, //target time set from source
                                      result.modTime,
                                      result.targetFileId,
                                      result.sourceFileId,
                                      file.isFollowedSymlink<sideTrg>(),
                                      file.isFollowedSymlink<sideSrc>());
        }
        break;

        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            //harmonize with file_hierarchy.cpp::getSyncOpDescription!!
            reportInfo(txtWritingAttributes, AFS::getDisplayPath(file.getAbstractPath<sideTrg>()));
            {
                StatisticsReporter statReporter(1, 0, procCallback_);

                assert(file.getItemName<sideTrg>() != file.getItemName<sideSrc>());
                if (file.getItemName<sideTrg>() != file.getItemName<sideSrc>()) //have difference in case?
                    AFS::renameItem(file.getAbstractPath<sideTrg>(), //throw FileError, (ErrorDifferentVolume)
                                    AFS::appendRelPath(file.parent().getAbstractPath<sideTrg>(), file.getItemName<sideSrc>()));

#if 0 //changing file time without copying content is not justified after CompareVariant::SIZE finds "equal" files! similar issue with CompareVariant::TIME_SIZE and FileTimeTolerance == -1
                //Bonus: some devices don't support setting (precise) file times anyway, e.g. FAT or MTP!
                if (file.getLastWriteTime<sideTrg>() != file.getLastWriteTime<sideSrc>())
                    //- no need to call sameFileTime() or respect 2 second FAT/FAT32 precision in this comparison
                    //- do NOT read *current* source file time, but use buffered value which corresponds to time of comparison!
                    AFS::setModTime(file.getAbstractPath<sideTrg>(), file.getLastWriteTime<sideSrc>()); //throw FileError
#endif
                statReporter.reportDelta(1, 0);

                //-> both sides *should* be completely equal now...
                assert(file.getFileSize<sideTrg>() == file.getFileSize<sideSrc>());
                file.setSyncedTo<sideTrg>(file.getItemName<sideSrc>(), file.getFileSize<sideSrc>(),
                                          file.getLastWriteTime<sideTrg>(),
                                          file.getLastWriteTime<sideSrc>(),
                                          file.getFileId       <sideTrg>(),
                                          file.getFileId       <sideSrc>(),
                                          file.isFollowedSymlink<sideTrg>(),
                                          file.isFollowedSymlink<sideSrc>());
            }
            break;

        case SO_MOVE_LEFT_FROM:  //use SO_MOVE_LEFT_TO/SO_MOVE_RIGHT_TO to execute move:
        case SO_MOVE_RIGHT_FROM: //=> makes sure parent directory has been created
        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            assert(false); //should have been filtered out by SynchronizeFolderPair::getPass()
            return; //no update on processed data!
    }

    procCallback_.requestUiRefresh(); //throw ?
}


inline
void SynchronizeFolderPair::synchronizeLink(SymlinkPair& link)
{
    const SyncOperation syncOp = link.getSyncOperation();

    if (Opt<SelectedSide> sideTrg = getTargetDirection(syncOp))
    {
        if (*sideTrg == LEFT_SIDE)
            synchronizeLinkInt<LEFT_SIDE>(link, syncOp);
        else
            synchronizeLinkInt<RIGHT_SIDE>(link, syncOp);
    }
}


template <SelectedSide sideTrg>
void SynchronizeFolderPair::synchronizeLinkInt(SymlinkPair& symlink, SyncOperation syncOp)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    switch (syncOp)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        {
            if (auto parentFolder = dynamic_cast<const FolderPair*>(&symlink.parent()))
                if (parentFolder->isEmpty<sideTrg>()) //BaseFolderPair OTOH is always non-empty and existing in this context => else: fatal error in zen::synchronize()
                    return; //if parent directory creation failed, there's no reason to show more errors!

            const AbstractPath targetPath = symlink.getAbstractPath<sideTrg>();
            reportInfo(txtCreatingLink, AFS::getDisplayPath(targetPath));

            StatisticsReporter statReporter(1, 0, procCallback_);
            try
            {
                AFS::copySymlink(symlink.getAbstractPath<sideSrc>(), targetPath, copyFilePermissions_); //throw FileError

                statReporter.reportDelta(1, 0);

                //update SymlinkPair
                symlink.setSyncedTo<sideTrg>(symlink.getItemName<sideSrc>(),
                                             symlink.getLastWriteTime<sideSrc>(), //target time set from source
                                             symlink.getLastWriteTime<sideSrc>());

            }
            catch (FileError&)
            {
                bool sourceWasDeleted = false;
                try { sourceWasDeleted = !AFS::getItemTypeIfExists(symlink.getAbstractPath<sideSrc>()); /*throw FileError*/ }
                catch (FileError&) {} //previous exception is more relevant

                if (sourceWasDeleted)
                    symlink.removeObject<sideSrc>(); //source deleted meanwhile...nothing was done (logical point of view!)
                else
                    throw; //do not check on type (symlink, file, folder) -> if there is a type change, FFS should not be quiet about it!
            }
        }
        break;

        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            reportInfo(getDelHandling<sideTrg>().getTxtRemovingSymLink(), AFS::getDisplayPath(symlink.getAbstractPath<sideTrg>()));
            {
                StatisticsReporter statReporter(1, 0, procCallback_);

                auto onNotifyItemDeletion = [&] { statReporter.reportDelta(1, 0); };

                getDelHandling<sideTrg>().removeLinkWithCallback(symlink.getAbstractPath<sideTrg>(), symlink.getPairRelativePath(), onNotifyItemDeletion); //throw FileError

                symlink.removeObject<sideTrg>(); //update SymlinkPair
            }
            break;

        case SO_OVERWRITE_LEFT:
        case SO_OVERWRITE_RIGHT:
            reportInfo(txtOverwritingLink, AFS::getDisplayPath(symlink.getAbstractPath<sideTrg>()));
            {
                StatisticsReporter statReporter(1, 0, procCallback_);

                //reportStatus(getDelHandling<sideTrg>().getTxtRemovingSymLink(), AFS::getDisplayPath(symlink.getAbstractPath<sideTrg>()));
                getDelHandling<sideTrg>().removeLinkWithCallback(symlink.getAbstractPath<sideTrg>(), symlink.getPairRelativePath(), [] {}); //throw FileError

                //symlink.removeObject<sideTrg>(); -> "symlink, sideTrg" evaluated below!

                //=> don't risk reportStatus() throwing AbortProcess() leaving the target deleted rather than updated:
                //reportStatus(txtOverwritingLink, AFS::getDisplayPath(symlink.getAbstractPath<sideTrg>())); //restore status text

                AFS::copySymlink(symlink.getAbstractPath<sideSrc>(),
                                 AFS::appendRelPath(symlink.parent().getAbstractPath<sideTrg>(), symlink.getItemName<sideSrc>()), //respect differences in case of source object
                                 copyFilePermissions_); //throw FileError

                statReporter.reportDelta(1, 0); //we model "delete + copy" as ONE logical operation

                //update SymlinkPair
                symlink.setSyncedTo<sideTrg>(symlink.getItemName<sideSrc>(),
                                             symlink.getLastWriteTime<sideSrc>(), //target time set from source
                                             symlink.getLastWriteTime<sideSrc>());
            }
            break;

        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            reportInfo(txtWritingAttributes, AFS::getDisplayPath(symlink.getAbstractPath<sideTrg>()));
            {
                StatisticsReporter statReporter(1, 0, procCallback_);

                if (symlink.getItemName<sideTrg>() != symlink.getItemName<sideSrc>()) //have difference in case?
                    AFS::renameItem(symlink.getAbstractPath<sideTrg>(), //throw FileError, (ErrorDifferentVolume)
                                    AFS::appendRelPath(symlink.parent().getAbstractPath<sideTrg>(), symlink.getItemName<sideSrc>()));

                //if (symlink.getLastWriteTime<sideTrg>() != symlink.getLastWriteTime<sideSrc>())
                //    //- no need to call sameFileTime() or respect 2 second FAT/FAT32 precision in this comparison
                //    //- do NOT read *current* source file time, but use buffered value which corresponds to time of comparison!
                //    AFS::setModTimeSymlink(symlink.getAbstractPath<sideTrg>(), symlink.getLastWriteTime<sideSrc>()); //throw FileError

                statReporter.reportDelta(1, 0);

                //-> both sides *should* be completely equal now...
                symlink.setSyncedTo<sideTrg>(symlink.getItemName<sideSrc>(),
                                             symlink.getLastWriteTime<sideTrg>(), //target time set from source
                                             symlink.getLastWriteTime<sideSrc>());
            }
            break;

        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_LEFT_TO:
        case SO_MOVE_RIGHT_TO:
        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            assert(false); //should have been filtered out by SynchronizeFolderPair::getPass()
            return; //no update on processed data!
    }

    procCallback_.requestUiRefresh(); //throw ?
}


inline
void SynchronizeFolderPair::synchronizeFolder(FolderPair& folder)
{
    const SyncOperation syncOp = folder.getSyncOperation();

    if (Opt<SelectedSide> sideTrg = getTargetDirection(syncOp))
    {
        if (*sideTrg == LEFT_SIDE)
            synchronizeFolderInt<LEFT_SIDE>(folder, syncOp);
        else
            synchronizeFolderInt<RIGHT_SIDE>(folder, syncOp);
    }
}


template <SelectedSide sideTrg>
void SynchronizeFolderPair::synchronizeFolderInt(FolderPair& folder, SyncOperation syncOp)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    switch (syncOp)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        {
            if (auto parentFolder = dynamic_cast<const FolderPair*>(&folder.parent()))
                if (parentFolder->isEmpty<sideTrg>()) //BaseFolderPair OTOH is always non-empty and existing in this context => else: fatal error in zen::synchronize()
                    return; //if parent directory creation failed, there's no reason to show more errors!

            const AbstractPath targetPath = folder.getAbstractPath<sideTrg>();
            reportInfo(txtCreatingFolder, AFS::getDisplayPath(targetPath));

            //shallow-"copying" a folder might not fail if source is missing, so we need to check this first:
            if (AFS::getItemTypeIfExists(folder.getAbstractPath<sideSrc>())) //throw FileError
            {
                StatisticsReporter statReporter(1, 0, procCallback_);
                try
                {
                    //target existing: undefined behavior! (fail/overwrite)
                    AFS::copyNewFolder(folder.getAbstractPath<sideSrc>(), targetPath, copyFilePermissions_); //throw FileError
                }
                catch (FileError&)
                {
                    bool folderAlreadyExists = false;
                    try { folderAlreadyExists = AFS::getItemType(targetPath) == AFS::ItemType::FOLDER; } /*throw FileError*/ catch (FileError&) {} //previous exception is more relevant

                    if (!folderAlreadyExists)
                        throw;
                }

                statReporter.reportDelta(1, 0);

                //update FolderPair
                folder.setSyncedTo<sideTrg>(folder.getItemName<sideSrc>(),
                                            false, //isSymlinkTrg
                                            folder.isFollowedSymlink<sideSrc>());
            }
            else //source deleted meanwhile...nothing was done (logical point of view!)
            {
                const SyncStatistics subStats(folder);
                StatisticsReporter statReporter(1 + getCUD(subStats), subStats.getBytesToProcess(), procCallback_);

                //remove only *after* evaluating folder!!
                folder.refSubFiles  ().clear(); //
                folder.refSubLinks  ().clear(); //update FolderPair
                folder.refSubFolders().clear(); //
                folder.removeObject<sideSrc>(); //
            }
        }
        break;

        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
            reportInfo(getDelHandling<sideTrg>().getTxtRemovingFolder(), AFS::getDisplayPath(folder.getAbstractPath<sideTrg>()));
            {
                const SyncStatistics subStats(folder); //counts sub-objects only!
                StatisticsReporter statReporter(1 + getCUD(subStats), subStats.getBytesToProcess(), procCallback_);

                auto onNotifyItemDeletion = [&] { statReporter.reportDelta(1, 0); };
                auto notifyUnbufferedIO   = [&](int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

                getDelHandling<sideTrg>().removeDirWithCallback(folder.getAbstractPath<sideTrg>(), folder.getPairRelativePath(), onNotifyItemDeletion, notifyUnbufferedIO); //throw FileError

                folder.refSubFiles  ().clear(); //
                folder.refSubLinks  ().clear(); //update FolderPair
                folder.refSubFolders().clear(); //
                folder.removeObject<sideTrg>(); //
            }
            break;

        case SO_OVERWRITE_LEFT:  //possible: e.g. manually-resolved dir-traversal conflict
        case SO_OVERWRITE_RIGHT: //
        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            reportInfo(txtWritingAttributes, AFS::getDisplayPath(folder.getAbstractPath<sideTrg>()));
            {
                StatisticsReporter statReporter(1, 0, procCallback_);

                assert(folder.getItemName<sideTrg>() != folder.getItemName<sideSrc>());
                if (folder.getItemName<sideTrg>() != folder.getItemName<sideSrc>()) //have difference in case?
                    AFS::renameItem(folder.getAbstractPath<sideTrg>(), //throw FileError, (ErrorDifferentVolume)
                                    AFS::appendRelPath(folder.parent().getAbstractPath<sideTrg>(), folder.getItemName<sideSrc>()));
                //copyFileTimes -> useless: modification time changes with each child-object creation/deletion

                statReporter.reportDelta(1, 0);

                //-> both sides *should* be completely equal now...
                folder.setSyncedTo<sideTrg>(folder.getItemName<sideSrc>(),
                                            folder.isFollowedSymlink<sideTrg>(),
                                            folder.isFollowedSymlink<sideSrc>());
            }
            break;

        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_LEFT_TO:
        case SO_MOVE_RIGHT_TO:
        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            assert(false); //should have been filtered out by SynchronizeFolderPair::getPass()
            return; //no update on processed data!
    }

    procCallback_.requestUiRefresh(); //throw ?
}

//###########################################################################################

//--------------------- data verification -------------------------
void verifyFiles(const AbstractPath& sourcePath, const AbstractPath& targetPath, const IOCallback& notifyUnbufferedIO)  //throw FileError
{
    try
    {
        //do like "copy /v": 1. flush target file buffers, 2. read again as usual (using OS buffers)
        // => it seems OS buffered are not invalidated by this: snake oil???
        if (Opt<Zstring> nativeTargetPath = AFS::getNativeItemPath(targetPath))
        {
            const int fileHandle = ::open(nativeTargetPath->c_str(), O_WRONLY | O_APPEND);
            if (fileHandle == -1)
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(*nativeTargetPath)), L"open");
            ZEN_ON_SCOPE_EXIT(::close(fileHandle));

            if (::fsync(fileHandle) != 0)
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(*nativeTargetPath)), L"fsync");
        } //close file handles!

        if (!filesHaveSameContent(sourcePath, targetPath, notifyUnbufferedIO)) //throw FileError
            throw FileError(replaceCpy(replaceCpy(_("%x and %y have different content."),
                                                  L"%x", L"\n" + fmtPath(AFS::getDisplayPath(sourcePath))),
                                       L"%y", L"\n" + fmtPath(AFS::getDisplayPath(targetPath))));
    }
    catch (const FileError& e) //add some context to error message
    {
        throw FileError(_("Data verification error:"), e.toString());
    }
}


AFS::FileCopyResult SynchronizeFolderPair::copyFileWithCallback(const FileDescriptor& sourceDescr, //throw FileError
                                                                const AbstractPath& targetPath,
                                                                const std::function<void()>& onDeleteTargetFile,
                                                                const IOCallback& notifyUnbufferedIO) const //returns current attributes of source file
{
    const AbstractPath& sourcePath = sourceDescr.path;
    const AFS::StreamAttributes sourceAttr{ sourceDescr.attr.modTime, sourceDescr.attr.fileSize, sourceDescr.attr.fileId };

    auto copyOperation = [this, &sourceAttr, &targetPath, &onDeleteTargetFile, &notifyUnbufferedIO](const AbstractPath& sourcePathTmp)
    {
        //target existing after onDeleteTargetFile(): undefined behavior! (fail/overwrite/auto-rename)
        const AFS::FileCopyResult result = AFS::copyFileTransactional(sourcePathTmp, sourceAttr, //throw FileError, ErrorFileLocked
                                                                      targetPath,
                                                                      copyFilePermissions_,
                                                                      failSafeFileCopy_,
                                                                      onDeleteTargetFile,
                                                                      notifyUnbufferedIO);
        //#################### Verification #############################
        if (verifyCopiedFiles_)
        {
            ZEN_ON_SCOPE_FAIL(try { AFS::removeFilePlain(targetPath); }
            catch (FileError&) {}); //delete target if verification fails

            procCallback_.reportInfo(replaceCpy(txtVerifying, L"%x", fmtPath(AFS::getDisplayPath(targetPath))));
            verifyFiles(sourcePathTmp, targetPath, [&](int64_t bytesDelta) { procCallback_.requestUiRefresh(); }); //throw FileError
        }
        //#################### /Verification #############################

        return result;
    };

    return copyOperation(sourcePath);
}

//###########################################################################################

template <SelectedSide side>
bool baseFolderDrop(BaseFolderPair& baseFolder, int folderAccessTimeout, ProcessCallback& callback)
{
    const AbstractPath folderPath = baseFolder.getAbstractPath<side>();

    if (baseFolder.isAvailable<side>())
        if (Opt<std::wstring> errMsg = tryReportingError([&]
    {
        const FolderStatus status = getFolderStatusNonBlocking({ folderPath }, folderAccessTimeout, false /*allowUserInteraction*/, callback);

            static_assert(IsSameType<decltype(status.failedChecks.begin()->second), FileError>::value, "");
            if (!status.failedChecks.empty())
                throw status.failedChecks.begin()->second;

            if (status.existing.find(folderPath) == status.existing.end())
                throw FileError(replaceCpy(_("Cannot find folder %x."), L"%x", fmtPath(AFS::getDisplayPath(folderPath))));
            //should really be logged as a "fatal error" if ignored by the user...
        }, callback)) //throw X?
    return true;

    return false;
}


template <SelectedSide side> //create base directories first (if not yet existing) -> no symlink or attribute copying!
bool createBaseFolder(BaseFolderPair& baseFolder, int folderAccessTimeout, ProcessCallback& callback) //nothrow; return false if fatal error occurred
{
    const AbstractPath baseFolderPath = baseFolder.getAbstractPath<side>();

    if (AFS::isNullPath(baseFolderPath))
        return true;

    if (!baseFolder.isAvailable<side>()) //create target directory: user presumably ignored error "dir existing" in order to have it created automatically
    {
        bool temporaryNetworkDrop = false;
        zen::Opt<std::wstring> errMsg = tryReportingError([&]
        {
            const FolderStatus status = getFolderStatusNonBlocking({ baseFolderPath }, folderAccessTimeout, false /*allowUserInteraction*/, callback);

            static_assert(IsSameType<decltype(status.failedChecks.begin()->second), FileError>::value, "");
            if (!status.failedChecks.empty())
                throw status.failedChecks.begin()->second;

            if (status.notExisting.find(baseFolderPath) != status.notExisting.end())
            {
                AFS::createFolderIfMissingRecursion(baseFolderPath); //throw FileError
                baseFolder.setAvailable<side>(true); //update our model!
            }
            else
            {
                //TEMPORARY network drop! base directory not found during comparison, but reappears during synchronization
                //=> sync-directions are based on false assumptions! Abort.
                callback.reportFatalError(replaceCpy(_("Target folder %x already existing."), L"%x", fmtPath(AFS::getDisplayPath(baseFolderPath))));
                temporaryNetworkDrop = true;

                //Is it possible we're catching a "false positive" here, could FFS have created the directory indirectly after comparison?
                //  1. deletion handling: recycler       -> no, temp directory created only at first deletion
                //  2. deletion handling: versioning     -> "
                //  3. log file creates containing folder -> no, log only created in batch mode, and only *before* comparison
            }
        }, callback); //throw X?
        return !errMsg && !temporaryNetworkDrop;
    }
    return true;
}


enum class FolderPairJobType
{
    PROCESS,
    ALREADY_IN_SYNC,
    SKIP,
};
}


void fff::synchronize(const std::chrono::system_clock::time_point& syncStartTime,
                      bool verifyCopiedFiles,
                      bool copyLockedFiles,
                      bool copyFilePermissions,
                      bool failSafeFileCopy,
                      bool runWithBackgroundPriority,
                      int folderAccessTimeout,
                      const std::vector<FolderPairSyncCfg>& syncConfig,
                      FolderComparison& folderCmp,
                      WarningDialogs& warnings,
                      ProcessCallback& callback)
{
    //PERF_START;

    if (syncConfig.size() != folderCmp.size())
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    //aggregate basic information
    std::vector<SyncStatistics> folderPairStats;
    {
        int     itemsTotal = 0;
        int64_t bytesTotal = 0;
        std::for_each(begin(folderCmp), end(folderCmp),
                      [&](const BaseFolderPair& baseFolder)
        {
            SyncStatistics fpStats(baseFolder);
            itemsTotal += getCUD(fpStats);
            bytesTotal += fpStats.getBytesToProcess();
            folderPairStats.push_back(fpStats);
        });

        //inform about the total amount of data that will be processed from now on
        //keep at beginning so that all gui elements are initialized properly
        callback.initNewPhase(itemsTotal, //throw X
                              bytesTotal,
                              ProcessCallback::PHASE_SYNCHRONIZING);
    }

    //-------------------------------------------------------------------------------

    //specify process and resource handling priorities
    std::unique_ptr<ScheduleForBackgroundProcessing> backgroundPrio;
    if (runWithBackgroundPriority)
        try
        {
            backgroundPrio = std::make_unique<ScheduleForBackgroundProcessing>(); //throw FileError
        }
        catch (const FileError& e) //not an error in this context
        {
            callback.reportInfo(e.toString()); //may throw!
        }

    //prevent operating system going into sleep state
    std::unique_ptr<PreventStandby> noStandby;
    try
    {
        noStandby = std::make_unique<PreventStandby>(); //throw FileError
    }
    catch (const FileError& e) //not an error in this context
    {
        callback.reportInfo(e.toString()); //may throw!
    }

    //-------------------execute basic checks all at once before starting sync--------------------------------------

    std::vector<FolderPairJobType> jobType(folderCmp.size(), FolderPairJobType::PROCESS); //folder pairs may be skipped after fatal errors were found

    std::vector<SyncStatistics::ConflictInfo> unresolvedConflicts;

    std::vector<std::tuple<AbstractPath, const HardFilter*, bool /*write access*/>> readWriteCheckBaseFolders;

    std::vector<std::pair<AbstractPath, AbstractPath>> significantDiffPairs;

    std::vector<std::pair<AbstractPath, std::pair<int64_t, int64_t>>> diskSpaceMissing; //base folder / space required / space available

    //status of base directories which are set to DeletionPolicy::RECYCLER (and contain actual items to be deleted)
    std::map<AbstractPath, bool, AFS::LessAbstractPath> recyclerSupported; //expensive to determine on Win XP => buffer + check recycle bin existence only once per base folder!

    std::set<AbstractPath, AFS::LessAbstractPath>           verCheckVersioningPaths;
    std::vector<std::pair<AbstractPath, const HardFilter*>> verCheckBaseFolderPaths; //hard filter creates new logical hierarchies for otherwise equal AbstractPath...

    //start checking folder pairs
    for (auto itBase = begin(folderCmp); itBase != end(folderCmp); ++itBase)
    {
        BaseFolderPair& baseFolder = *itBase;
        const size_t folderIndex = itBase - begin(folderCmp);
        const FolderPairSyncCfg& folderPairCfg  = syncConfig     [folderIndex];
        const SyncStatistics&    folderPairStat = folderPairStats[folderIndex];

        //aggregate all conflicts:
        append(unresolvedConflicts, folderPairStat.getConflicts());

        //exclude a few pathological cases (including empty left, right folders)
        if (AFS::equalAbstractPath(baseFolder.getAbstractPath< LEFT_SIDE>(),
                                   baseFolder.getAbstractPath<RIGHT_SIDE>()))
        {
            jobType[folderIndex] = FolderPairJobType::SKIP;
            continue;
        }

        //skip folder pair if there is nothing to do (except for two-way mode and move-detection, where DB files need to be updated)
        //-> skip creating (not yet existing) base directories in particular if there's no need
        if (getCUD(folderPairStat) == 0)
        {
            jobType[folderIndex] = FolderPairJobType::ALREADY_IN_SYNC;
            continue;
        }

        const bool writeLeft = folderPairStat.createCount<LEFT_SIDE>() +
                               folderPairStat.updateCount<LEFT_SIDE>() +
                               folderPairStat.deleteCount<LEFT_SIDE>() > 0;

        const bool writeRight = folderPairStat.createCount<RIGHT_SIDE>() +
                                folderPairStat.updateCount<RIGHT_SIDE>() +
                                folderPairStat.deleteCount<RIGHT_SIDE>() > 0;

        //check for empty target folder paths: this only makes sense if empty field is source (and no DB files need to be created)
        if ((AFS::isNullPath(baseFolder.getAbstractPath< LEFT_SIDE>()) && (writeLeft  || folderPairCfg.saveSyncDB_)) ||
            (AFS::isNullPath(baseFolder.getAbstractPath<RIGHT_SIDE>()) && (writeRight || folderPairCfg.saveSyncDB_)))
        {
            callback.reportFatalError(_("Target folder input field must not be empty."));
            jobType[folderIndex] = FolderPairJobType::SKIP;
            continue;
        }

        //check for network drops after comparison
        // - convenience: exit sync right here instead of showing tons of errors during file copy
        // - early failure! there's no point in evaluating subsequent warnings
        if (baseFolderDrop< LEFT_SIDE>(baseFolder, folderAccessTimeout, callback) ||
            baseFolderDrop<RIGHT_SIDE>(baseFolder, folderAccessTimeout, callback))
        {
            jobType[folderIndex] = FolderPairJobType::SKIP;
            continue;
        }

        //allow propagation of deletions only from *null-* or *existing* source folder:
        auto sourceFolderMissing = [&](const AbstractPath& baseFolderPath, bool wasAvailable) //we need to evaluate existence status from time of comparison!
        {
            if (!AFS::isNullPath(baseFolderPath))
                //PERMANENT network drop: avoid data loss when source directory is not found AND user chose to ignore errors (else we wouldn't arrive here)
                if (folderPairStat.deleteCount() > 0) //check deletions only... (respect filtered items!)
                    //folderPairStat.conflictCount() == 0 && -> there COULD be conflicts for <Two way> variant if directory existence check fails, but loading sync.ffs_db succeeds
                    //https://sourceforge.net/tracker/?func=detail&atid=1093080&aid=3531351&group_id=234430 -> fixed, but still better not consider conflicts!
                    if (!wasAvailable) //avoid race-condition: we need to evaluate existence status from time of comparison!
                    {
                        callback.reportFatalError(replaceCpy(_("Source folder %x not found."), L"%x", fmtPath(AFS::getDisplayPath(baseFolderPath))));
                        return true;
                    }
            return false;
        };
        if (sourceFolderMissing(baseFolder.getAbstractPath< LEFT_SIDE>(), baseFolder.isAvailable< LEFT_SIDE>()) ||
            sourceFolderMissing(baseFolder.getAbstractPath<RIGHT_SIDE>(), baseFolder.isAvailable<RIGHT_SIDE>()))
        {
            jobType[folderIndex] = FolderPairJobType::SKIP;
            continue;
        }

        if (folderPairCfg.handleDeletion == DeletionPolicy::VERSIONING)
        {
            const AbstractPath versioningFolderPath = createAbstractPath(folderPairCfg.versioningFolderPhrase);

            //check if user-defined directory for deletion was specified
            if (AFS::isNullPath(versioningFolderPath))
            {
                //should never arrive here: already checked in SyncCfgDialog
                callback.reportFatalError(_("Please enter a target folder for versioning."));
                jobType[folderIndex] = FolderPairJobType::SKIP;
                continue;
            }
            //===============================================================================================
            //================ end of checks that may skip folder pairs => begin of warnings ================
            //===============================================================================================

            //prepare: check if versioning path itself will be synchronized (and was not excluded via filter)
            verCheckVersioningPaths.insert(versioningFolderPath);;
            verCheckBaseFolderPaths.emplace_back(baseFolder.getAbstractPath<LEFT_SIDE >(), &baseFolder.getFilter());
            verCheckBaseFolderPaths.emplace_back(baseFolder.getAbstractPath<RIGHT_SIDE>(), &baseFolder.getFilter());
        }

        //prepare: check if folders are used by multiple pairs in read/write access
        readWriteCheckBaseFolders.emplace_back(baseFolder.getAbstractPath<LEFT_SIDE >(), &baseFolder.getFilter(), writeLeft);
        readWriteCheckBaseFolders.emplace_back(baseFolder.getAbstractPath<RIGHT_SIDE>(), &baseFolder.getFilter(), writeRight);

        //check if more than 50% of total number of files/dirs are to be created/overwritten/deleted
        if (!AFS::isNullPath(baseFolder.getAbstractPath< LEFT_SIDE>()) &&
            !AFS::isNullPath(baseFolder.getAbstractPath<RIGHT_SIDE>()))
            if (significantDifferenceDetected(folderPairStat))
                significantDiffPairs.emplace_back(baseFolder.getAbstractPath< LEFT_SIDE>(),
                                                  baseFolder.getAbstractPath<RIGHT_SIDE>());

        //check for sufficient free diskspace
        auto checkSpace = [&](const AbstractPath& baseFolderPath, int64_t minSpaceNeeded)
        {
            if (!AFS::isNullPath(baseFolderPath))
                try
                {
                    const int64_t freeSpace = AFS::getFreeDiskSpace(baseFolderPath); //throw FileError, returns 0 if not available

                    if (0 < freeSpace && //zero means "request not supported" (e.g. see WebDav)
                        freeSpace < minSpaceNeeded)
                        diskSpaceMissing.push_back({ baseFolderPath, { minSpaceNeeded, freeSpace } });
                }
                catch (FileError&) {} //for warning only => no need for tryReportingError()
        };
        const std::pair<int64_t, int64_t> spaceNeeded = MinimumDiskSpaceNeeded::calculate(baseFolder);
        checkSpace(baseFolder.getAbstractPath< LEFT_SIDE>(), spaceNeeded.first);
        checkSpace(baseFolder.getAbstractPath<RIGHT_SIDE>(), spaceNeeded.second);

        //windows: check if recycle bin really exists; if not, Windows will silently delete, which is wrong
        auto checkRecycler = [&](const AbstractPath& baseFolderPath)
        {
            assert(!AFS::isNullPath(baseFolderPath));
            if (!AFS::isNullPath(baseFolderPath))
                if (recyclerSupported.find(baseFolderPath) == recyclerSupported.end()) //perf: avoid duplicate checks!
                {
                    callback.reportStatus(replaceCpy(_("Checking recycle bin availability for folder %x..."), L"%x",
                                                     fmtPath(AFS::getDisplayPath(baseFolderPath))));
                    bool recSupported = false;
                    tryReportingError([&]
                    {
                        recSupported = AFS::supportsRecycleBin(baseFolderPath, [&]{ callback.requestUiRefresh(); /*may throw*/ }); //throw FileError
                    }, callback); //throw X?

                    recyclerSupported.emplace(baseFolderPath, recSupported);
                }
        };
        if (folderPairCfg.handleDeletion == DeletionPolicy::RECYCLER)
        {
            if (folderPairStat.expectPhysicalDeletion<LEFT_SIDE>())
                checkRecycler(baseFolder.getAbstractPath<LEFT_SIDE>());

            if (folderPairStat.expectPhysicalDeletion<RIGHT_SIDE>())
                checkRecycler(baseFolder.getAbstractPath<RIGHT_SIDE>());
        }
    }

    //check if unresolved conflicts exist
    if (!unresolvedConflicts.empty())
    {
        std::wstring msg = _("The following items have unresolved conflicts and will not be synchronized:");

        for (const SyncStatistics::ConflictInfo& item : unresolvedConflicts) //show *all* conflicts in warning message
            msg += L"\n\n" + fmtPath(item.relPath) + L": " + item.msg;

        callback.reportWarning(msg, warnings.warnUnresolvedConflicts);
    }

    //check if user accidentally selected wrong directories for sync
    if (!significantDiffPairs.empty())
    {
        std::wstring msg = _("The following folders are significantly different. Please check that the correct folders are selected for synchronization.");

        for (const auto& item : significantDiffPairs)
            msg += L"\n\n" +
                   AFS::getDisplayPath(item.first) + L" <-> " + L"\n" +
                   AFS::getDisplayPath(item.second);

        callback.reportWarning(msg, warnings.warnSignificantDifference);
    }

    //check for sufficient free diskspace
    if (!diskSpaceMissing.empty())
    {
        std::wstring msg = _("Not enough free disk space available in:");

        for (const auto& item : diskSpaceMissing)
            msg += L"\n\n" + AFS::getDisplayPath(item.first) + L"\n" +
                   _("Required:")  + L" " + formatFilesizeShort(item.second.first)  + L"\n" +
                   _("Available:") + L" " + formatFilesizeShort(item.second.second);

        callback.reportWarning(msg, warnings.warnNotEnoughDiskSpace);
    }

    //windows: check if recycle bin really exists; if not, Windows will silently delete, which is wrong
    {
        std::wstring msg;
        for (const auto& item : recyclerSupported)
            if (!item.second)
                msg += L"\n" + AFS::getDisplayPath(item.first);

        if (!msg.empty())
            callback.reportWarning(_("The recycle bin is not supported by the following folders. Deleted or overwritten files will not be able to be restored:") + L"\n" + msg,
                                   warnings.warnRecyclerMissing);
    }

    //check if folders are used by multiple pairs in read/write access
    {
        std::set<AbstractPath, AFS::LessAbstractPath> dependentFolders;

        //race condition := multiple accesses of which at least one is a write
        for (auto it = readWriteCheckBaseFolders.begin(); it != readWriteCheckBaseFolders.end(); ++it)
            if (std::get<bool>(*it)) //write access
                for (auto it2 = readWriteCheckBaseFolders.begin(); it2 != readWriteCheckBaseFolders.end(); ++it2)
                    if (!std::get<bool>(*it2) || it < it2) //avoid duplicate comparisons
                        if (Opt<PathDependency> pd = getPathDependency(std::get<AbstractPath>(*it),  *std::get<const HardFilter*>(*it),
                                                                       std::get<AbstractPath>(*it2), *std::get<const HardFilter*>(*it2)))
                        {
                            dependentFolders.insert(pd->basePathParent);
                            dependentFolders.insert(pd->basePathChild);
                        }

        if (!dependentFolders.empty())
        {
            std::wstring msg = _("Some files will be synchronized as part of multiple base folders.") + L"\n" +
                               _("To avoid conflicts, set up exclude filters so that each updated file is considered by only one base folder.") + L"\n";

            for (const AbstractPath& baseFolderPath : dependentFolders)
                msg += L"\n" + AFS::getDisplayPath(baseFolderPath);

            callback.reportWarning(msg, warnings.warnDependentBaseFolders);
        }
    }

    //check if versioning path itself will be synchronized (and was not excluded via filter)
    {
        std::wstring msg;
        for (const AbstractPath& versioningFolderPath : verCheckVersioningPaths)
        {
            std::map<AbstractPath, std::wstring, AFS::LessAbstractPath> uniqueMsgs; //=> at most one msg per base folder (*and* per versioningFolderPath)

            for (const auto& item : verCheckBaseFolderPaths) //may contain duplicate paths, but with *different* hard filter!
                if (Opt<PathDependency> pd = getPathDependency(versioningFolderPath, NullFilter(), item.first, *item.second))
                {
                    std::wstring line = L"\n\n" + _("Versioning folder:") + L" \t" + AFS::getDisplayPath(versioningFolderPath) +
                                        L"\n"   + _("Base folder:")       + L" \t" + AFS::getDisplayPath(item.first);
                    if (AFS::equalAbstractPath(pd->basePathParent, item.first) && !pd->relPath.empty())
                        line += L"\n" + _("Exclude:") + L" \t" + utfTo<std::wstring>(FILE_NAME_SEPARATOR + pd->relPath + FILE_NAME_SEPARATOR);

                    uniqueMsgs[item.first] = line;
                }
            for (const auto& item : uniqueMsgs)
                msg += item.second;
        }
        if (!msg.empty())
            callback.reportWarning(_("The versioning folder is contained in a base folder.") + L"\n" +
                                   _("The folder should be excluded from synchronization via filter.") + msg, warnings.warnVersioningFolderPartOfSync);
    }

    //-------------------end of basic checks------------------------------------------

    std::vector<FileError> errorsModTime; //show all warnings as a single message

    try
    {
        const TimeComp timeStamp = getLocalTime(std::chrono::system_clock::to_time_t(syncStartTime));
        if (timeStamp == TimeComp())
            throw std::runtime_error("Failed to determine current time: " + numberTo<std::string>(syncStartTime.time_since_epoch().count()));

        //loop through all directory pairs
        for (auto itBase = begin(folderCmp); itBase != end(folderCmp); ++itBase)
        {
            BaseFolderPair& baseFolder = *itBase;
            const size_t folderIndex = itBase - begin(folderCmp);
            const FolderPairSyncCfg& folderPairCfg  = syncConfig     [folderIndex];
            const SyncStatistics&    folderPairStat = folderPairStats[folderIndex];

            if (jobType[folderIndex] == FolderPairJobType::SKIP) //folder pairs may be skipped after fatal errors were found
                continue;

            //------------------------------------------------------------------------------------------
            callback.reportInfo(_("Synchronizing folder pair:") + L" " + getVariantNameForLog(folderPairCfg.syncVariant_) + L"\n" +
                                L"    " + AFS::getDisplayPath(baseFolder.getAbstractPath< LEFT_SIDE>()) + L"\n" +
                                L"    " + AFS::getDisplayPath(baseFolder.getAbstractPath<RIGHT_SIDE>()));
            //------------------------------------------------------------------------------------------

            //checking a second time: (a long time may have passed since folder comparison!)
            if (baseFolderDrop< LEFT_SIDE>(baseFolder, folderAccessTimeout, callback) ||
                baseFolderDrop<RIGHT_SIDE>(baseFolder, folderAccessTimeout, callback))
                continue;

            //create base folders if not yet existing
            if (folderPairStat.createCount() > 0 || folderPairCfg.saveSyncDB_) //else: temporary network drop leading to deletions already caught by "sourceFolderMissing" check!
                if (!createBaseFolder< LEFT_SIDE>(baseFolder, folderAccessTimeout, callback) || //+ detect temporary network drop!!
                    !createBaseFolder<RIGHT_SIDE>(baseFolder, folderAccessTimeout, callback))   //
                    continue;

            //------------------------------------------------------------------------------------------
            //execute synchronization recursively

            //update synchronization database in case of errors:
            auto guardDbSave = makeGuard<ScopeGuardRunMode::ON_FAIL>([&]
            {
                try
                {
                    if (folderPairCfg.saveSyncDB_)
                        saveLastSynchronousState(baseFolder, //throw FileError
                        [&](const std::wstring& statusMsg) { try { callback.reportStatus(statusMsg); /*throw X*/} catch (...) {}});
                }
                catch (FileError&) {}
            });

            if (jobType[folderIndex] == FolderPairJobType::PROCESS)
            {
                //guarantee removal of invalid entries (where element is empty on both sides)
                ZEN_ON_SCOPE_EXIT(BaseFolderPair::removeEmpty(baseFolder));

                bool copyPermissionsFp = false;
                tryReportingError([&]
                {
                    copyPermissionsFp = copyFilePermissions && //copy permissions only if asked for and supported by *both* sides!
                    !AFS::isNullPath(baseFolder.getAbstractPath< LEFT_SIDE>()) && //scenario: directory selected on one side only
                    !AFS::isNullPath(baseFolder.getAbstractPath<RIGHT_SIDE>()) && //
                    AFS::supportPermissionCopy(baseFolder.getAbstractPath<LEFT_SIDE>(),
                                               baseFolder.getAbstractPath<RIGHT_SIDE>()); //throw FileError
                }, callback); //throw X?


                auto getEffectiveDeletionPolicy = [&](const AbstractPath& baseFolderPath) -> DeletionPolicy
                {
                    if (folderPairCfg.handleDeletion == DeletionPolicy::RECYCLER)
                    {
                        auto it = recyclerSupported.find(baseFolderPath);
                        if (it != recyclerSupported.end()) //buffer filled during intro checks (but only if deletions are expected)
                            if (!it->second)
                                return DeletionPolicy::PERMANENT; //Windows' ::SHFileOperation() will do this anyway, but we have a better and faster deletion routine (e.g. on networks)
                    }
                    return folderPairCfg.handleDeletion;
                };

                DeletionHandling delHandlerL(baseFolder.getAbstractPath<LEFT_SIDE>(),
                                             getEffectiveDeletionPolicy(baseFolder.getAbstractPath<LEFT_SIDE>()),
                                             folderPairCfg.versioningFolderPhrase,
                                             folderPairCfg.versioningStyle_,
                                             timeStamp,
                                             callback);

                DeletionHandling delHandlerR(baseFolder.getAbstractPath<RIGHT_SIDE>(),
                                             getEffectiveDeletionPolicy(baseFolder.getAbstractPath<RIGHT_SIDE>()),
                                             folderPairCfg.versioningFolderPhrase,
                                             folderPairCfg.versioningStyle_,
                                             timeStamp,
                                             callback);


                SynchronizeFolderPair syncFP(callback, verifyCopiedFiles, copyPermissionsFp, failSafeFileCopy,
                                             errorsModTime,
                                             delHandlerL, delHandlerR);
                syncFP.startSync(baseFolder);

                //(try to gracefully) cleanup temporary Recycle bin folders and versioning -> will be done in ~DeletionHandling anyway...
                tryReportingError([&] { delHandlerL.tryCleanup(true /*allowCallbackException*/); /*throw FileError*/}, callback); //throw X?
                tryReportingError([&] { delHandlerR.tryCleanup(true                           ); /*throw FileError*/}, callback); //throw X?
            }

            //(try to gracefully) write database file
            if (folderPairCfg.saveSyncDB_)
            {
                callback.reportStatus(_("Generating database..."));
                callback.forceUiRefresh(); //throw X

                tryReportingError([&]
                {
                    saveLastSynchronousState(baseFolder, //throw FileError
                    [&](const std::wstring& statusMsg) { callback.reportStatus(statusMsg); /*throw X*/});
                }, callback); //throw X

                guardDbSave.dismiss(); //[!] after "graceful" try: user might have cancelled during DB write: ensure DB is still written
            }
        }

        //------------------- show warnings after end of synchronization --------------------------------------

        //TODO: mod time warnings are not shown if user cancelled sync before batch-reporting the warnings: problem?

        //show errors when setting modification time: warning, not an error
        if (!errorsModTime.empty())
        {
            std::wstring msg;
            for (const FileError& e : errorsModTime)
            {
                std::wstring singleMsg = replaceCpy(e.toString(), L"\n\n", L"\n");
                msg += singleMsg + L"\n\n";
            }
            msg.resize(msg.size() - 2);

            callback.reportWarning(msg, warnings.warnModificationTimeError); //throw X
        }
    }
    catch (const std::exception& e)
    {
        callback.reportFatalError(utfTo<std::wstring>(e.what()));
        callback.abortProcessNow(); //throw X
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
    }
}

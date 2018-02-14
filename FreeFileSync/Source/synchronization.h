// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SYNCHRONIZATION_H_8913470815943295
#define SYNCHRONIZATION_H_8913470815943295

#include <chrono>
#include "file_hierarchy.h"
#include "lib/process_xml.h"
#include "process_callback.h"


namespace fff
{
class SyncStatistics //this class counts *logical* operations, (create, update, delete + bytes), *not* disk accesses!
{
    //-> note the fundamental difference compared to counting disk accesses!
public:
    SyncStatistics(const FolderComparison& folderCmp);
    SyncStatistics(const ContainerObject& hierObj);
    SyncStatistics(const FilePair& file);

    template <SelectedSide side>
    int createCount() const { return SelectParam<side>::ref(createLeft_, createRight_); }
    int createCount() const { return createLeft_ + createRight_; }

    template <SelectedSide side>
    int updateCount() const { return SelectParam<side>::ref(updateLeft_, updateRight_); }
    int updateCount() const { return updateLeft_ + updateRight_; }

    template <SelectedSide side>
    int deleteCount() const { return SelectParam<side>::ref(deleteLeft_, deleteRight_); }
    int deleteCount() const { return deleteLeft_ + deleteRight_; }

    template <SelectedSide side>
    bool expectPhysicalDeletion() const { return SelectParam<side>::ref(physicalDeleteLeft_, physicalDeleteRight_); }

    int conflictCount() const { return static_cast<int>(conflictMsgs_.size()); }

    int64_t getBytesToProcess() const { return bytesToProcess_; }
    size_t  rowCount         () const { return rowsTotal_; }

    struct ConflictInfo
    {
        Zstring relPath;
        std::wstring msg;
    };
    const std::vector<ConflictInfo>& getConflicts() const { return conflictMsgs_; }

private:
    void recurse(const ContainerObject& hierObj);

    void processFile  (const FilePair& file);
    void processLink  (const SymlinkPair& link);
    void processFolder(const FolderPair& folder);

    int createLeft_  = 0;
    int createRight_ = 0;
    int updateLeft_  = 0;
    int updateRight_ = 0;
    int deleteLeft_  = 0;
    int deleteRight_ = 0;
    bool physicalDeleteLeft_  = false; //at least 1 item will be deleted; considers most "update" cases which also delete items
    bool physicalDeleteRight_ = false; //
    std::vector<ConflictInfo> conflictMsgs_; //conflict texts to display as a warning message
    int64_t bytesToProcess_ = 0;
    size_t rowsTotal_ = 0;
};


struct FolderPairSyncCfg
{
    FolderPairSyncCfg(bool saveSyncDB,
                      const DeletionPolicy handleDel,
                      VersioningStyle versioningStyle,
                      const Zstring& versioningPhrase,
                      DirectionConfig::Variant syncVariant) :
        saveSyncDB_(saveSyncDB),
        handleDeletion(handleDel),
        versioningStyle_(versioningStyle),
        versioningFolderPhrase(versioningPhrase),
        syncVariant_(syncVariant) {}

    bool saveSyncDB_; //save database if in automatic mode or dection of moved files is active
    DeletionPolicy handleDeletion;
    VersioningStyle versioningStyle_;
    Zstring versioningFolderPhrase; //unresolved directory names as entered by user!
    DirectionConfig::Variant syncVariant_;
};
std::vector<FolderPairSyncCfg> extractSyncCfg(const MainConfiguration& mainCfg);


//FFS core routine:
void synchronize(const std::chrono::system_clock::time_point& syncStartTime,
                 bool verifyCopiedFiles,
                 bool copyLockedFiles,
                 bool copyFilePermissions,
                 bool failSafeFileCopy,
                 bool runWithBackgroundPriority,
                 int folderAccessTimeout,
                 const std::vector<FolderPairSyncCfg>& syncConfig, //CONTRACT: syncConfig and folderCmp correspond row-wise!
                 FolderComparison& folderCmp,                      //
                 WarningDialogs& warnings,
                 ProcessCallback& callback);
}

#endif //SYNCHRONIZATION_H_8913470815943295

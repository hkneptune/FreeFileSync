// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SYNCHRONIZATION_H_8913470815943295
#define SYNCHRONIZATION_H_8913470815943295

#include <chrono>
#include "structures.h"
#include "file_hierarchy.h"
#include "process_callback.h"


namespace fff
{
class SyncStatistics //count *logical* operations, (create, update, delete + bytes), *not* disk accesses!
{
    //-> note the fundamental difference compared to counting disk accesses!
public:
    explicit SyncStatistics(const FolderComparison& folderCmp);
    explicit SyncStatistics(const ContainerObject& conObj);
    explicit SyncStatistics(const FilePair& file);

    template <SelectSide side>
    int createCount() const { return selectParam<side>(createLeft_, createRight_); }
    int createCount() const { return createLeft_ + createRight_; }

    template <SelectSide side>
    int updateCount() const { return selectParam<side>(updateLeft_, updateRight_); }
    int updateCount() const { return updateLeft_ + updateRight_; }

    template <SelectSide side>
    int deleteCount() const { return selectParam<side>(deleteLeft_, deleteRight_); }
    int deleteCount() const { return deleteLeft_ + deleteRight_; }

    int64_t getBytesToProcess() const { return bytesToProcess_; }
    size_t  rowCount         () const { return rowsTotal_; }

    const std::vector<std::wstring>& getConflictsPreview() const { return conflictsPreview_; }
    int conflictCount() const { return conflictCount_; }

private:
    void recurse(const ContainerObject& conObj);
    void logConflict(const FileSystemObject& fsObj);

    void processFile  (const FilePair& file);
    void processLink  (const SymlinkPair& symlink);
    void processFolder(const FolderPair& folder);

    int createLeft_  = 0;
    int createRight_ = 0;
    int updateLeft_  = 0;
    int updateRight_ = 0;
    int deleteLeft_  = 0;
    int deleteRight_ = 0;

    int64_t bytesToProcess_ = 0;
    size_t rowsTotal_ = 0;

    int conflictCount_ = 0;
    std::vector<std::wstring> conflictsPreview_; //conflict texts to display as a warning message
    //limit conflict count! e.g. there may be hundred thousands of "same date but a different size"
};


inline
int getCUD(const SyncStatistics& stat)
{
    return stat.createCount() +
           stat.updateCount() +
           stat.deleteCount();
}

struct FolderPairSyncCfg
{
    SyncVariant syncVar;
    bool saveSyncDB; //save database if in automatic mode or dection of moved files is active
    DeletionVariant handleDeletion;
    Zstring versioningFolderPhrase; //unresolved directory names as entered by user!
    VersioningStyle versioningStyle;
    int versionMaxAgeDays;
    int versionCountMin;
    int versionCountMax;
};
std::vector<FolderPairSyncCfg> extractSyncCfg(const MainConfiguration& mainCfg);


//FFS core routine:
void synchronize(const std::chrono::system_clock::time_point& syncStartTime,
                 bool verifyCopiedFiles,
                 bool copyLockedFiles,
                 bool copyFilePermissions,
                 bool failSafeFileCopy,
                 bool runWithBackgroundPriority,
                 const std::vector<FolderPairSyncCfg>& syncConfig, //CONTRACT: syncConfig and folderCmp correspond row-wise!
                 FolderComparison& folderCmp,                      //
                 WarningDialogs& warnings,
                 ProcessCallback& callback /*throw X*/); //throw X
}

#endif //SYNCHRONIZATION_H_8913470815943295

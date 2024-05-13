// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STRUCTURES_H_8210478915019450901745
#define STRUCTURES_H_8210478915019450901745

#include <variant>
#include <vector>
//#include <memory>
#include <chrono>
#include <zen/zstring.h>
#include "../afs/abstract.h"


namespace fff
{
using AFS = AbstractFileSystem;

enum class CompareVariant
{
    timeSize,
    content,
    size
};


enum class SymLinkHandling
{
    exclude,
    asLink,
    follow
};


enum class SyncDirection : unsigned char //save space for use in FileSystemObject!
{
    none,
    left,
    right
};


enum CompareFileResult
{
    FILE_EQUAL,
    FILE_RENAMED, //both sides equal, except for different file name
    FILE_LEFT_ONLY,
    FILE_RIGHT_ONLY,
    FILE_LEFT_NEWER,   //
    FILE_RIGHT_NEWER,  //CompareVariant::timeSize only!
    FILE_TIME_INVALID, //  -> sync dirction can be determined (if leftNewer/rightNewer agree), unlike with FILE_CONFLICT
    FILE_DIFFERENT_CONTENT, //CompareVariant::content, CompareVariant::size only!
    FILE_CONFLICT
};
//attention make sure these /|\  \|/ three enums match!!!
enum CompareDirResult
{
    DIR_EQUAL      = FILE_EQUAL,
    DIR_RENAMED    = FILE_RENAMED,
    DIR_LEFT_ONLY  = FILE_LEFT_ONLY,
    DIR_RIGHT_ONLY = FILE_RIGHT_ONLY,
    DIR_CONFLICT   = FILE_CONFLICT
};

enum CompareSymlinkResult
{
    SYMLINK_EQUAL             = FILE_EQUAL,
    SYMLINK_RENAMED           = FILE_RENAMED,
    SYMLINK_LEFT_ONLY         = FILE_LEFT_ONLY,
    SYMLINK_RIGHT_ONLY        = FILE_RIGHT_ONLY,
    SYMLINK_LEFT_NEWER        = FILE_LEFT_NEWER,
    SYMLINK_RIGHT_NEWER       = FILE_RIGHT_NEWER,
    SYMLINK_TIME_INVALID      = FILE_TIME_INVALID,
    SYMLINK_DIFFERENT_CONTENT = FILE_DIFFERENT_CONTENT,
    SYMLINK_CONFLICT          = FILE_CONFLICT
};


std::wstring getSymbol(CompareFileResult cmpRes);


enum SyncOperation
{
    SO_CREATE_LEFT,
    SO_CREATE_RIGHT,
    SO_DELETE_LEFT,
    SO_DELETE_RIGHT,

    SO_OVERWRITE_LEFT,
    SO_OVERWRITE_RIGHT,

    SO_MOVE_LEFT_FROM, //SO_DELETE_LEFT - optimization!
    SO_MOVE_LEFT_TO,   //SO_CREATE_LEFT

    SO_MOVE_RIGHT_FROM, //SO_DELETE_RIGHT - optimization!
    SO_MOVE_RIGHT_TO,   //SO_CREATE_RIGHT

    SO_RENAME_LEFT,  //items are otherwise equal
    SO_RENAME_RIGHT, //

    SO_DO_NOTHING, //nothing will be synced: both sides differ
    SO_EQUAL,      //nothing will be synced: both sides are equal
    SO_UNRESOLVED_CONFLICT
};

std::wstring getSymbol(SyncOperation op); //method used for exporting .csv file only!


enum class CudAction
{
    noChange,
    create,
    update,
    delete_, //"delete" is a reserved keyword :(
};

struct DirectionByDiff
{
    SyncDirection leftOnly   = SyncDirection::none;
    SyncDirection rightOnly  = SyncDirection::none;
    SyncDirection leftNewer  = SyncDirection::none;
    SyncDirection rightNewer = SyncDirection::none;

    bool operator==(const DirectionByDiff&) const = default;
};


struct DirectionByChange //=> requires sync.ffs_db
{
    struct Changes
    {
        SyncDirection create  = SyncDirection::none;
        SyncDirection update  = SyncDirection::none;
        SyncDirection delete_ = SyncDirection::none; //"delete" is a reserved keyword :(

        bool operator==(const Changes&) const = default;
    } left, right;

    bool operator==(const DirectionByChange&) const = default;
};


struct SyncDirectionConfig
{
    std::variant<DirectionByDiff, DirectionByChange> dirs;

    bool operator==(const SyncDirectionConfig&) const = default;
};


inline
bool effectivelyEqual(const SyncDirectionConfig& lhs, const SyncDirectionConfig& rhs) { return lhs == rhs; } //no change in behavior


enum class SyncVariant
{
    twoWay,
    mirror,
    update,
    custom,
};
SyncVariant getSyncVariant(const SyncDirectionConfig& cfg);

SyncDirectionConfig getDefaultSyncCfg(SyncVariant syncVar);

DirectionByDiff getDiffDirDefault(const DirectionByChange& changeDirs); //= when sync.ffs_db not yet available
DirectionByChange getChangesDirDefault(const DirectionByDiff& diffDirs);

std::wstring getVariantName(std::optional<CompareVariant> var);
std::wstring getVariantName(std::optional<SyncVariant> var);

std::wstring getVariantNameWithSymbol(SyncVariant var);


struct CompConfig
{
    CompareVariant compareVar = CompareVariant::timeSize;
    SymLinkHandling handleSymlinks = SymLinkHandling::exclude;
    std::vector<unsigned int> ignoreTimeShiftMinutes; //treat modification times with these offsets as equal

    bool operator==(const CompConfig&) const = default;
};

inline
bool effectivelyEqual(const CompConfig& lhs, const CompConfig& rhs) { return lhs == rhs; } //no change in behavior


enum class DeletionVariant
{
    permanent,
    recycler,
    versioning
};

enum class VersioningStyle
{
    replace,
    timestampFolder,
    timestampFile,
};

struct SyncConfig
{
    //sync direction settings
    SyncDirectionConfig directionCfg = getDefaultSyncCfg(SyncVariant::twoWay);

    DeletionVariant deletionVariant = DeletionVariant::recycler; //use Recycle Bin, delete permanently or move to user-defined location

    //versioning options
    Zstring versioningFolderPhrase;
    VersioningStyle versioningStyle = VersioningStyle::replace;

    //limit number of versions per file: (if versioningStyle != replace)
    int versionMaxAgeDays = 0; //<= 0 := no limit
    int versionCountMin   = 0; //only used if versionMaxAgeDays > 0 => < versionCountMax (if versionCountMax > 0)
    int versionCountMax   = 0; //<= 0 := no limit
};


inline
bool operator==(const SyncConfig& lhs, const SyncConfig& rhs)
{
    return lhs.directionCfg           == rhs.directionCfg      &&
           lhs.deletionVariant        == rhs.deletionVariant   &&      //!= DeletionVariant::versioning => still consider versioningFolderPhrase: e.g. user temporarily
           lhs.versioningFolderPhrase == rhs.versioningFolderPhrase && //switched to "permanent" deletion and accidentally saved cfg => versioning folder can be restored
           lhs.versioningStyle        == rhs.versioningStyle   &&
           (lhs.versioningStyle == VersioningStyle::replace ||
            (
                lhs.versionMaxAgeDays == rhs.versionMaxAgeDays &&
                (lhs.versionMaxAgeDays <= 0 ||
                 lhs.versionCountMin  == rhs.versionCountMin)  &&
                lhs.versionCountMax   == rhs.versionCountMax
            ));
    //adapt effectivelyEqual() on changes, too!
}


inline
bool effectivelyEqual(const SyncConfig& lhs, const SyncConfig& rhs)
{
    return effectivelyEqual(lhs.directionCfg, rhs.directionCfg) &&
           lhs.deletionVariant == rhs.deletionVariant &&
           (lhs.deletionVariant != DeletionVariant::versioning || //only evaluate versioning folder if required!
            (
                lhs.versioningFolderPhrase == rhs.versioningFolderPhrase &&
                lhs.versioningStyle        == rhs.versioningStyle        &&
                (lhs.versioningStyle == VersioningStyle::replace ||
                 (
                     lhs.versionMaxAgeDays == rhs.versionMaxAgeDays &&
                     (lhs.versionMaxAgeDays <= 0 ||
                      lhs.versionCountMin  == rhs.versionCountMin)  &&
                     lhs.versionCountMax   == rhs.versionCountMax
                 ))
            ));
}


enum class UnitSize
{
    none,
    byte,
    kb,
    mb
};

enum class UnitTime
{
    none,
    today,
    thisMonth,
    thisYear,
    lastDays
};

struct FilterConfig
{
    /* Semantics of PathFilter:
        1. using it creates a NEW folder hierarchy! -> must be considered by <Two way> variant! (fortunately it turns out, doing nothing already has perfect semantics :)
        2. it applies equally to both sides => it always matches either both sides or none! => can be used while traversing a single folder!    */
    Zstring includeFilter = Zstr("*");
    Zstring excludeFilter;

    /* Semantics of SoftFilter:
        1. It potentially may match only one side => it MUST NOT be applied while traversing a single folder to avoid mismatches
        2. => it is applied after traversing and just marks rows, (NO deletions after comparison are allowed)
        3. => equivalent to a user temporarily (de-)selecting rows -> not relevant for <Two way> variant! ;)    */
    unsigned int timeSpan = 0;
    UnitTime unitTimeSpan = UnitTime::none;

    uint64_t sizeMin = 0;
    UnitSize unitSizeMin = UnitSize::none;

    uint64_t sizeMax = 0;
    UnitSize unitSizeMax = UnitSize::none;

    bool operator==(const FilterConfig&) const = default;
};


void resolveUnits(size_t timeSpan, UnitTime unitTimeSpan,
                  uint64_t sizeMin,  UnitSize unitSizeMin,
                  uint64_t sizeMax,  UnitSize unitSizeMax,
                  time_t&   timeFrom,   //unit: UTC time, seconds
                  uint64_t& sizeMinBy,  //unit: bytes
                  uint64_t& sizeMaxBy); //unit: bytes


struct LocalPairConfig //enhanced folder pairs with (optional) alternate configuration
{
    Zstring folderPathPhraseLeft;  //unresolved directory names as entered by user!
    Zstring folderPathPhraseRight; //

    std::optional<CompConfig> localCmpCfg;
    std::optional<SyncConfig> localSyncCfg;
    FilterConfig              localFilter;

    bool operator==(const LocalPairConfig& rhs) const = default;
};


enum class ResultsNotification
{
    always,
    errorWarning,
    errorOnly,
};


enum class PostSyncCondition
{
    completion,
    errors,
    success
};


struct MainConfiguration
{
    CompConfig   cmpCfg;       //global compare settings:         may be overwritten by folder pair settings
    SyncConfig   syncCfg;      //global synchronisation settings: may be overwritten by folder pair settings
    FilterConfig globalFilter; //global filter settings:          combined with folder pair settings

    LocalPairConfig firstPair; //there needs to be at least one pair!
    std::vector<LocalPairConfig> additionalPairs;

    std::map<AfsDevice, size_t /*parallel operations*/> deviceParallelOps; //should only include devices with >= 2  parallel ops

    bool ignoreErrors = false; //true: errors will still be logged
    size_t autoRetryCount = 0;
    std::chrono::seconds autoRetryDelay{5};

    Zstring postSyncCommand; //user-defined command line
    PostSyncCondition postSyncCondition = PostSyncCondition::completion;

    Zstring altLogFolderPathPhrase; //fill to use different log file folder (other than the default %appdata%\FreeFileSync\Logs)

    std::string emailNotifyAddress; //optional
    ResultsNotification emailNotifyCondition = ResultsNotification::always;

    std::wstring notes;

    bool operator==(const MainConfiguration&) const = default;
};


size_t getDeviceParallelOps(const std::map<AfsDevice, size_t>& deviceParallelOps, const AfsDevice& afsDevice);
void   setDeviceParallelOps(      std::map<AfsDevice, size_t>& deviceParallelOps, const AfsDevice& afsDevice, size_t parallelOps);
size_t getDeviceParallelOps(const std::map<AfsDevice, size_t>& deviceParallelOps, const Zstring& folderPathPhrase);
void   setDeviceParallelOps(      std::map<AfsDevice, size_t>& deviceParallelOps, const Zstring& folderPathPhrase, size_t parallelOps);


std::optional<CompareVariant> getCommonCompVariant(const MainConfiguration& mainCfg);
std::optional<SyncVariant>    getCommonSyncVariant(const MainConfiguration& mainCfg);


struct WarningDialogs
{
    bool warnFolderNotExisting          = true;
    bool warnFoldersDifferInCase        = true;
    bool warnDependentFolderPair        = true;
    bool warnDependentBaseFolders       = true;
    bool warnSignificantDifference      = true;
    bool warnNotEnoughDiskSpace         = true;
    bool warnUnresolvedConflicts        = true;
    bool warnRecyclerMissing            = true;
    bool warnInputFieldEmpty            = true;
    bool warnDirectoryLockFailed        = true;
    bool warnVersioningFolderPartOfSync = true;

    bool operator==(const WarningDialogs&) const = default;
};
}

#endif //STRUCTURES_H_8210478915019450901745

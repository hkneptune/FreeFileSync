// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STRUCTURES_H_8210478915019450901745
#define STRUCTURES_H_8210478915019450901745

#include <vector>
#include <memory>
#include <zen/zstring.h>


namespace fff
{
enum class CompareVariant
{
    TIME_SIZE,
    CONTENT,
    SIZE
};

std::wstring getVariantName(CompareVariant var);


enum class SymLinkHandling
{
    EXCLUDE,
    DIRECT,
    FOLLOW
};


enum class SyncDirection : unsigned char //save space for use in FileSystemObject!
{
    LEFT,
    RIGHT,
    NONE
};


enum CompareFilesResult
{
    FILE_EQUAL,
    FILE_LEFT_SIDE_ONLY,
    FILE_RIGHT_SIDE_ONLY,
    FILE_LEFT_NEWER,  //CompareVariant::TIME_SIZE only!
    FILE_RIGHT_NEWER, //
    FILE_DIFFERENT_CONTENT, //CompareVariant::CONTENT, CompareVariant::SIZE only!
    FILE_DIFFERENT_METADATA, //both sides equal, but different metadata only: short name case
    FILE_CONFLICT
};
//attention make sure these /|\  \|/ three enums match!!!
enum CompareDirResult
{
    DIR_EQUAL              = FILE_EQUAL,
    DIR_LEFT_SIDE_ONLY     = FILE_LEFT_SIDE_ONLY,
    DIR_RIGHT_SIDE_ONLY    = FILE_RIGHT_SIDE_ONLY,
    DIR_DIFFERENT_METADATA = FILE_DIFFERENT_METADATA, //both sides equal, but different metadata only: short name case
    DIR_CONFLICT           = FILE_CONFLICT
};

enum CompareSymlinkResult
{
    SYMLINK_EQUAL           = FILE_EQUAL,
    SYMLINK_LEFT_SIDE_ONLY  = FILE_LEFT_SIDE_ONLY,
    SYMLINK_RIGHT_SIDE_ONLY = FILE_RIGHT_SIDE_ONLY,
    SYMLINK_LEFT_NEWER      = FILE_LEFT_NEWER,
    SYMLINK_RIGHT_NEWER     = FILE_RIGHT_NEWER,
    SYMLINK_DIFFERENT_CONTENT  = FILE_DIFFERENT_CONTENT,
    SYMLINK_DIFFERENT_METADATA = FILE_DIFFERENT_METADATA, //both sides equal, but different metadata only: short name case
    SYMLINK_CONFLICT        = FILE_CONFLICT
};


std::wstring getSymbol(CompareFilesResult cmpRes);


enum SyncOperation
{
    SO_CREATE_NEW_LEFT,
    SO_CREATE_NEW_RIGHT,
    SO_DELETE_LEFT,
    SO_DELETE_RIGHT,

    SO_MOVE_LEFT_FROM, //SO_DELETE_LEFT    - optimization!
    SO_MOVE_LEFT_TO, //SO_CREATE_NEW_LEFT

    SO_MOVE_RIGHT_FROM, //SO_DELETE_RIGHT    - optimization!
    SO_MOVE_RIGHT_TO, //SO_CREATE_NEW_RIGHT

    SO_OVERWRITE_LEFT,
    SO_OVERWRITE_RIGHT,
    SO_COPY_METADATA_TO_LEFT,  //objects are already equal: transfer metadata only - optimization
    SO_COPY_METADATA_TO_RIGHT, //

    SO_DO_NOTHING, //nothing will be synced: both sides differ
    SO_EQUAL,      //nothing will be synced: both sides are equal
    SO_UNRESOLVED_CONFLICT
};

std::wstring getSymbol(SyncOperation op); //method used for exporting .csv file only!


struct DirectionSet
{
    SyncDirection exLeftSideOnly  = SyncDirection::RIGHT;
    SyncDirection exRightSideOnly = SyncDirection::LEFT;
    SyncDirection leftNewer       = SyncDirection::RIGHT; //CompareVariant::TIME_SIZE only!
    SyncDirection rightNewer      = SyncDirection::LEFT;  //
    SyncDirection different       = SyncDirection::NONE; //CompareVariant::CONTENT, CompareVariant::SIZE only!
    SyncDirection conflict        = SyncDirection::NONE;
};

DirectionSet getTwoWayUpdateSet();

inline
bool operator==(const DirectionSet& lhs, const DirectionSet& rhs)
{
    return lhs.exLeftSideOnly  == rhs.exLeftSideOnly  &&
           lhs.exRightSideOnly == rhs.exRightSideOnly &&
           lhs.leftNewer       == rhs.leftNewer       &&
           lhs.rightNewer      == rhs.rightNewer      &&
           lhs.different       == rhs.different       &&
           lhs.conflict        == rhs.conflict;
}

struct DirectionConfig //technical representation of sync-config
{
    enum Variant
    {
        TWO_WAY, //use sync-database to determine directions
        MIRROR, //predefined
        UPDATE, //
        CUSTOM //use custom directions
    };

    Variant var = TWO_WAY;
    DirectionSet custom; //sync directions for variant CUSTOM
    bool detectMovedFiles = false; //dependent from Variant: e.g. always active for DirectionConfig::TWO_WAY! => use functions below for evaluation!
};

bool detectMovedFilesSelectable(const DirectionConfig& cfg);
bool detectMovedFilesEnabled   (const DirectionConfig& cfg);

DirectionSet extractDirections(const DirectionConfig& cfg); //get sync directions: DON'T call for DirectionConfig::TWO_WAY!

std::wstring getVariantName      (DirectionConfig::Variant var);
std::wstring getVariantNameForLog(DirectionConfig::Variant var);

inline
bool operator==(const DirectionConfig& lhs, const DirectionConfig& rhs)
{
    return lhs.var == rhs.var &&
           (lhs.var != DirectionConfig::CUSTOM || lhs.custom == rhs.custom) && //no need to consider custom directions if var != CUSTOM
           lhs.detectMovedFiles == rhs.detectMovedFiles; //useful to remember this setting even if the current sync variant does not need it
    //adapt effectivelyEqual() on changes, too!
}

inline
bool effectivelyEqual(const DirectionConfig& lhs, const DirectionConfig& rhs)
{
    return (lhs.var == DirectionConfig::TWO_WAY) == (rhs.var == DirectionConfig::TWO_WAY) && //either both two-way or none
           (lhs.var == DirectionConfig::TWO_WAY || extractDirections(lhs) == extractDirections(rhs)) &&
           detectMovedFilesEnabled(lhs) == detectMovedFilesEnabled(rhs);
}


struct CompConfig
{
    CompareVariant compareVar = CompareVariant::TIME_SIZE;
    SymLinkHandling handleSymlinks = SymLinkHandling::EXCLUDE;
    std::vector<unsigned int> ignoreTimeShiftMinutes; //treat modification times with these offsets as equal
};

inline
bool operator==(const CompConfig& lhs, const CompConfig& rhs)
{
    return lhs.compareVar             == rhs.compareVar &&
           lhs.handleSymlinks         == rhs.handleSymlinks &&
           lhs.ignoreTimeShiftMinutes == rhs.ignoreTimeShiftMinutes;
}
inline bool operator!=(const CompConfig& lhs, const CompConfig& rhs) { return !(lhs == rhs); }

inline
bool effectivelyEqual(const CompConfig& lhs, const CompConfig& rhs) { return lhs == rhs; } //no change in behavior

//convert "ignoreTimeShiftMinutes" into compact format:
std::vector<unsigned int> fromTimeShiftPhrase(const std::wstring& timeShiftPhrase);
std::wstring              toTimeShiftPhrase  (const std::vector<unsigned int>& ignoreTimeShiftMinutes);


enum class DeletionPolicy
{
    PERMANENT,
    RECYCLER,
    VERSIONING
};

enum class VersioningStyle
{
    REPLACE,
    ADD_TIMESTAMP,
};

struct SyncConfig
{
    //sync direction settings
    DirectionConfig directionCfg;

    DeletionPolicy handleDeletion = DeletionPolicy::RECYCLER; //use Recycle, delete permanently or move to user-defined location
    //versioning options
    VersioningStyle versioningStyle = VersioningStyle::REPLACE;
    Zstring versioningFolderPhrase;
    //int versionCountLimit; //max versions per file (DeletionPolicy::VERSIONING); < 0 := no limit
};


inline
bool operator==(const SyncConfig& lhs, const SyncConfig& rhs)
{
    return lhs.directionCfg           == rhs.directionCfg   &&
           lhs.handleDeletion         == rhs.handleDeletion &&
           lhs.versioningStyle        == rhs.versioningStyle &&
           lhs.versioningFolderPhrase == rhs.versioningFolderPhrase;
    //adapt effectivelyEqual() on changes, too!
}
inline bool operator!=(const SyncConfig& lhs, const SyncConfig& rhs) { return !(lhs == rhs); }


inline
bool effectivelyEqual(const SyncConfig& lhs, const SyncConfig& rhs)
{
    return effectivelyEqual(lhs.directionCfg, rhs.directionCfg) &&
           lhs.handleDeletion == rhs.handleDeletion &&
           (lhs.handleDeletion != DeletionPolicy::VERSIONING || //only compare deletion directory if required!
            (lhs.versioningStyle   == rhs.versioningStyle &&
             lhs.versioningFolderPhrase == rhs.versioningFolderPhrase));
}


enum class UnitSize
{
    NONE,
    BYTE,
    KB,
    MB
};

enum class UnitTime
{
    NONE,
    TODAY,
    //THIS_WEEK,
    THIS_MONTH,
    THIS_YEAR,
    LAST_X_DAYS
};

struct FilterConfig
{
    FilterConfig() {}
    FilterConfig(const Zstring& include,
                 const Zstring& exclude,
                 size_t   timeSpanIn,
                 UnitTime unitTimeSpanIn,
                 size_t   sizeMinIn,
                 UnitSize unitSizeMinIn,
                 size_t   sizeMaxIn,
                 UnitSize unitSizeMaxIn) :
        includeFilter(include),
        excludeFilter(exclude),
        timeSpan     (timeSpanIn),
        unitTimeSpan (unitTimeSpanIn),
        sizeMin      (sizeMinIn),
        unitSizeMin  (unitSizeMinIn),
        sizeMax      (sizeMaxIn),
        unitSizeMax  (unitSizeMaxIn) {}

    /*
    Semantics of HardFilter:
    1. using it creates a NEW folder hierarchy! -> must be considered by <Two way> variant! (fortunately it turns out, doing nothing already has perfect semantics :)
    2. it applies equally to both sides => it always matches either both sides or none! => can be used while traversing a single folder!
    */
    Zstring includeFilter = Zstr("*");
    Zstring excludeFilter;

    /*
    Semantics of SoftFilter:
    1. It potentially may match only one side => it MUST NOT be applied while traversing a single folder to avoid mismatches
    2. => it is applied after traversing and just marks rows, (NO deletions after comparison are allowed)
    3. => equivalent to a user temporarily (de-)selecting rows -> not relevant for <Two way> variant! ;)
    */
    size_t timeSpan = 0;
    UnitTime unitTimeSpan = UnitTime::NONE;

    size_t sizeMin = 0;
    UnitSize unitSizeMin = UnitSize::NONE;

    size_t sizeMax = 0;
    UnitSize unitSizeMax = UnitSize::NONE;
};

inline
bool operator==(const FilterConfig& lhs, const FilterConfig& rhs)
{
    return lhs.includeFilter == rhs.includeFilter &&
           lhs.excludeFilter == rhs.excludeFilter &&
           lhs.timeSpan      == rhs.timeSpan      &&
           lhs.unitTimeSpan  == rhs.unitTimeSpan  &&
           lhs.sizeMin       == rhs.sizeMin       &&
           lhs.unitSizeMin   == rhs.unitSizeMin   &&
           lhs.sizeMax       == rhs.sizeMax       &&
           lhs.unitSizeMax   == rhs.unitSizeMax;
}
inline bool operator!=(const FilterConfig& lhs, const FilterConfig& rhs) { return !(lhs == rhs); }

void resolveUnits(size_t timeSpan, UnitTime unitTimeSpan,
                  size_t sizeMin,  UnitSize unitSizeMin,
                  size_t sizeMax,  UnitSize unitSizeMax,
                  time_t&   timeFrom,   //unit: UTC time, seconds
                  uint64_t& sizeMinBy,  //unit: bytes
                  uint64_t& sizeMaxBy); //unit: bytes


struct FolderPairEnh //enhanced folder pairs with (optional) alternate configuration
{
    FolderPairEnh() {}

    FolderPairEnh(const Zstring& folderPathPhraseLeft,
                  const Zstring& folderPathPhraseRight,
                  const std::shared_ptr<const CompConfig>& cmpConfig,
                  const std::shared_ptr<const SyncConfig>& syncConfig,
                  const FilterConfig& filter) :
        folderPathPhraseLeft_ (folderPathPhraseLeft),
        folderPathPhraseRight_(folderPathPhraseRight),
        altCmpConfig(cmpConfig),
        altSyncConfig(syncConfig),
        localFilter(filter) {}

    Zstring folderPathPhraseLeft_;  //unresolved directory names as entered by user!
    Zstring folderPathPhraseRight_; //

    std::shared_ptr<const CompConfig> altCmpConfig;  //optional
    std::shared_ptr<const SyncConfig> altSyncConfig; //
    FilterConfig localFilter;
};


inline
bool operator==(const FolderPairEnh& lhs, const FolderPairEnh& rhs)
{
    return lhs.folderPathPhraseLeft_  == rhs.folderPathPhraseLeft_  &&
           lhs.folderPathPhraseRight_ == rhs.folderPathPhraseRight_ &&

           (lhs.altCmpConfig.get() && rhs.altCmpConfig.get() ?
            *lhs.altCmpConfig == *rhs.altCmpConfig :
            lhs.altCmpConfig.get() == rhs.altCmpConfig.get()) &&

           (lhs.altSyncConfig.get() && rhs.altSyncConfig.get() ?
            *lhs.altSyncConfig == *rhs.altSyncConfig :
            lhs.altSyncConfig.get() == rhs.altSyncConfig.get()) &&

           lhs.localFilter == rhs.localFilter;
}


enum class PostSyncCondition
{
    COMPLETION,
    ERRORS,
    SUCCESS
};


struct MainConfiguration
{
    CompConfig   cmpConfig;    //global compare settings:         may be overwritten by folder pair settings
    SyncConfig   syncCfg;      //global synchronisation settings: may be overwritten by folder pair settings
    FilterConfig globalFilter; //global filter settings:          combined with folder pair settings

    FolderPairEnh firstPair; //there needs to be at least one pair!
    std::vector<FolderPairEnh> additionalPairs;

    bool ignoreErrors = false; //true: errors will still be logged

    Zstring postSyncCommand; //user-defined command line
    PostSyncCondition postSyncCondition = PostSyncCondition::COMPLETION;

    std::wstring getCompVariantName() const;
    std::wstring getSyncVariantName() const;
};


inline
bool operator==(const MainConfiguration& lhs, const MainConfiguration& rhs)
{
    return lhs.cmpConfig         == rhs.cmpConfig       &&
           lhs.syncCfg           == rhs.syncCfg         &&
           lhs.globalFilter      == rhs.globalFilter    &&
           lhs.firstPair         == rhs.firstPair       &&
           lhs.additionalPairs   == rhs.additionalPairs &&
           lhs.ignoreErrors      == rhs.ignoreErrors    &&
           lhs.postSyncCommand   == rhs.postSyncCommand &&
           lhs.postSyncCondition == rhs.postSyncCondition;
}


//facilitate drag & drop config merge:
MainConfiguration merge(const std::vector<MainConfiguration>& mainCfgs);
}

#endif //STRUCTURES_H_8210478915019450901745

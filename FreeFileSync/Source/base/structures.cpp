// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "structures.h"
//#include <iterator>
//#include <ctime>
#include <zen/i18n.h>
#include <zen/time.h>
#include "../afs/concrete.h"

using namespace zen;
using namespace fff;


std::wstring fff::getVariantName(std::optional<CompareVariant> var)
{
    if (!var)
        return _("Multiple...");

    switch (*var)
    {
        //*INDENT-OFF*
        case CompareVariant::timeSize: return _("File time and size");
        case CompareVariant::content:  return _("File content");
        case CompareVariant::size:     return _("File size");
        //*INDENT-ON*
    }
    assert(false);
    return _("Error");
}


std::wstring fff::getVariantName(std::optional<SyncVariant> var)
{
    if (!var)
        return _("Multiple...");

    switch (*var)
    {
        //*INDENT-OFF*
        case SyncVariant::twoWay: return _("Two way");
        case SyncVariant::mirror: return _("Mirror");
        case SyncVariant::update: return _("Update");
        case SyncVariant::custom: return _("Custom");
        //*INDENT-ON*
    }
    assert(false);
    return _("Error");
}


//use in sync log files where users expect ANSI: https://freefilesync.org/forum/viewtopic.php?t=4647
std::wstring fff::getVariantNameWithSymbol(SyncVariant var)
{
    switch (var)
    {
        //*INDENT-OFF*
        case SyncVariant::twoWay: return _("Two way") + L" <->";
        case SyncVariant::mirror: return _("Mirror")  + L" ->";
        case SyncVariant::update: return _("Update")  + L" >";
        case SyncVariant::custom: return _("Custom")  + L" <>";
        //*INDENT-ON*
    }
    assert(false);
    return _("Error");
}


DirectionByDiff fff::getDiffDirDefault(const DirectionByChange& changeDirs)
{
    return
    {
        .leftOnly   = changeDirs.left.create,
        .rightOnly  = changeDirs.right.create,
        .leftNewer  = changeDirs.left.update,
        .rightNewer = changeDirs.right.update,
    };
}


DirectionByChange fff::getChangesDirDefault(const DirectionByDiff& diffDirs)
{
    return
    {
        .left
        {
            .create  = diffDirs.leftOnly,
            .update  = diffDirs.leftNewer,
            .delete_ = diffDirs.rightOnly,
        },
        .right
        {
            .create  = diffDirs.rightOnly,
            .update  = diffDirs.rightNewer,
            .delete_ = diffDirs.leftOnly,
        }
    };
}


namespace
{
DirectionByChange getTwoWayDirSet()
{
    return
    {
        .left
        {
            .create  = SyncDirection::right,
            .update  = SyncDirection::right,
            .delete_ = SyncDirection::right,
        },
        .right
        {
            .create  = SyncDirection::left,
            .update  = SyncDirection::left,
            .delete_ = SyncDirection::left,
        }
    };
}


DirectionByDiff getMirrorDirSet()
{
    return
    {
        .leftOnly   = SyncDirection::right,
        .rightOnly  = SyncDirection::right,
        .leftNewer  = SyncDirection::right,
        .rightNewer = SyncDirection::right,
    };
}


DirectionByChange getUpdateDirSet()
{
    return
    {
        .left
        {
            .create  = SyncDirection::right,
            .update  = SyncDirection::right,
            .delete_ = SyncDirection::none,
        },
        .right
        {
            .create  = SyncDirection::none,
            .update  = SyncDirection::none,
            .delete_ = SyncDirection::none,
        }
    };
}
}


SyncVariant fff::getSyncVariant(const SyncDirectionConfig& cfg)
{
    if (const DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&cfg.dirs))
    {
        if (*diffDirs == getMirrorDirSet())
            return SyncVariant::mirror;
        if (*diffDirs == getDiffDirDefault(getUpdateDirSet())) //poor man's "update", still deserves name on GUI
            return SyncVariant::update;
    }
    else
    {
        const DirectionByChange& changeDirs = std::get<DirectionByChange>(cfg.dirs);
        if (changeDirs == getTwoWayDirSet())
            return SyncVariant::twoWay;
        if (changeDirs == getChangesDirDefault(getMirrorDirSet())) //equivalent: "mirror" defined in terms of "changes"
            return SyncVariant::mirror;
        if (changeDirs == getUpdateDirSet())
            return SyncVariant::update;
    }
    return SyncVariant::custom;
}


SyncDirectionConfig fff::getDefaultSyncCfg(SyncVariant syncVar)
{
    switch (syncVar)
    {
        //*INDENT-OFF*
        case SyncVariant::twoWay: return { .dirs = getTwoWayDirSet() };
        case SyncVariant::mirror: return { .dirs = getMirrorDirSet() };
        case SyncVariant::update: return { .dirs = getUpdateDirSet() };
        case SyncVariant::custom: return { .dirs = getDiffDirDefault(getTwoWayDirSet()) };
        //*INDENT-ON*
    }
    throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
}


size_t fff::getDeviceParallelOps(const std::map<AfsDevice, size_t>& deviceParallelOps, const AfsDevice& afsDevice)
{
    auto it = deviceParallelOps.find(afsDevice);
    return std::max<size_t>(it != deviceParallelOps.end() ? it->second : 1, 1);
}


void fff::setDeviceParallelOps(std::map<AfsDevice, size_t>& deviceParallelOps, const AfsDevice& afsDevice, size_t parallelOps)
{
    assert(parallelOps > 0);
    if (!AFS::isNullDevice(afsDevice))
    {
        if (parallelOps > 1)
            deviceParallelOps[afsDevice] = parallelOps;
        else
            deviceParallelOps.erase(afsDevice);
    }
}


size_t fff::getDeviceParallelOps(const std::map<AfsDevice, size_t>& deviceParallelOps, const Zstring& folderPathPhrase)
{
    return getDeviceParallelOps(deviceParallelOps, createAbstractPath(folderPathPhrase).afsDevice);
}


void fff::setDeviceParallelOps(std::map<AfsDevice, size_t>& deviceParallelOps, const Zstring& folderPathPhrase, size_t parallelOps)
{
    setDeviceParallelOps(deviceParallelOps, createAbstractPath(folderPathPhrase).afsDevice, parallelOps);
}


std::wstring fff::getSymbol(CompareFileResult cmpRes)
{
    switch (cmpRes)
    {
        //*INDENT-OFF*
        case FILE_EQUAL:             return L"'="; //added quotation mark to avoid error in Excel cell when exporting to *.cvs
        case FILE_RENAMED:           return L"renamed"; 
        case FILE_LEFT_ONLY:         return L"only <-";
        case FILE_RIGHT_ONLY:        return L"only ->";
        case FILE_LEFT_NEWER:        return L"newer <-";
        case FILE_RIGHT_NEWER:       return L"newer ->";
        case FILE_DIFFERENT_CONTENT: return L"!=";
        case FILE_TIME_INVALID:
        case FILE_CONFLICT:          return L"conflict";
        //*INDENT-ON*
    }
    assert(false);
    return std::wstring();
}


std::wstring fff::getSymbol(SyncOperation op)
{
    switch (op)
    {
        //*INDENT-OFF*
        case SO_CREATE_LEFT:     return L"create <-";
        case SO_CREATE_RIGHT:    return L"create ->";
        case SO_DELETE_LEFT:     return L"delete <-";
        case SO_DELETE_RIGHT:    return L"delete ->";
        case SO_MOVE_LEFT_FROM:  return L"move from <-";
        case SO_MOVE_LEFT_TO:    return L"move to <-";
        case SO_MOVE_RIGHT_FROM: return L"move from ->";
        case SO_MOVE_RIGHT_TO:   return L"move to ->";
        case SO_OVERWRITE_LEFT:  return L"update <-";
        case SO_OVERWRITE_RIGHT: return L"update ->";
        case SO_RENAME_LEFT:     return L"rename <-";
        case SO_RENAME_RIGHT:    return L"rename ->";
        case SO_DO_NOTHING:      return L" -";
        case SO_EQUAL:           return L"'="; //added quotation mark to avoid error in Excel cell when exporting to *.cvs
        case SO_UNRESOLVED_CONFLICT: return L"conflict"; //portable Unicode symbol: ⚡
        //*INDENT-ON*
    };
    assert(false);
    return std::wstring();
}


namespace
{
time_t resolve(size_t value, UnitTime unit, time_t defaultVal)
{
    TimeComp tcLocal = getLocalTime(); //returns TimeComp() on error
    if (tcLocal != TimeComp())
        switch (unit)
        {
            case UnitTime::none:
                return defaultVal;

            case UnitTime::today:
            case UnitTime::lastDays:
                tcLocal.second = 0; //0-61
                tcLocal.minute = 0; //0-59
                tcLocal.hour   = 0; //0-23
                break;

            case UnitTime::thisMonth:
                tcLocal.second = 0; //0-61
                tcLocal.minute = 0; //0-59
                tcLocal.hour   = 0; //0-23
                tcLocal.day    = 1; //1-31
                break;

            case UnitTime::thisYear:
                tcLocal.second = 0; //0-61
                tcLocal.minute = 0; //0-59
                tcLocal.hour   = 0; //0-23
                tcLocal.day    = 1; //1-31
                tcLocal.month  = 1; //1-12
                break;
        }
    if (const auto [localTime, timeValid] = localToTimeT(tcLocal);//convert local time back to UTC
        timeValid)
    {
        if (unit == UnitTime::lastDays)
            return localTime - value * 24 * 3600;

        return localTime;
    }

    assert(false);
    return defaultVal;
}


uint64_t resolve(uint64_t value, UnitSize unit, uint64_t defaultVal)
{
    constexpr uint64_t maxVal = std::numeric_limits<uint64_t>::max();

    switch (unit)
    {
        case UnitSize::none:
            return defaultVal;
        case UnitSize::byte:
            return value;
        case UnitSize::kb:
            return value > maxVal / bytesPerKilo ? maxVal : //prevent overflow!!!
                   value * bytesPerKilo;
        case UnitSize::mb:
            return value > maxVal / (bytesPerKilo * bytesPerKilo) ? maxVal : //prevent overflow!!!
                   value * bytesPerKilo * bytesPerKilo;
    }
    assert(false);
    return defaultVal;
}
}

void fff::resolveUnits(size_t timeSpan, UnitTime unitTimeSpan,
                       uint64_t sizeMin,  UnitSize unitSizeMin,
                       uint64_t sizeMax,  UnitSize unitSizeMax,
                       time_t&   timeFrom,  //unit: UTC time, seconds
                       uint64_t& sizeMinBy, //unit: bytes
                       uint64_t& sizeMaxBy) //unit: bytes
{
    timeFrom  = resolve(timeSpan, unitTimeSpan, std::numeric_limits<time_t>::min());
    sizeMinBy = resolve(sizeMin,  unitSizeMin, 0U);
    sizeMaxBy = resolve(sizeMax,  unitSizeMax, std::numeric_limits<uint64_t>::max());
}


std::optional<CompareVariant> fff::getCommonCompVariant(const MainConfiguration& mainCfg)
{
    const CompareVariant firstVar = mainCfg.firstPair.localCmpCfg ?
                                    mainCfg.firstPair.localCmpCfg->compareVar :
                                    mainCfg.cmpCfg.compareVar; //fallback to main sync cfg

    //test if there's a deviating variant within the additional folder pairs
    for (const LocalPairConfig& lpc : mainCfg.additionalPairs)
    {
        const CompareVariant localVariant = lpc.localCmpCfg ?
                                            lpc.localCmpCfg->compareVar :
                                            mainCfg.cmpCfg.compareVar; //fallback to main sync cfg
        if (localVariant != firstVar)
            return std::nullopt;
    }
    return firstVar; //seems to be all in sync...
}


std::optional<SyncVariant> fff::getCommonSyncVariant(const MainConfiguration& mainCfg)
{
    const SyncVariant firstVar = getSyncVariant(mainCfg.firstPair.localSyncCfg ?
                                                mainCfg.firstPair.localSyncCfg->directionCfg :
                                                mainCfg.syncCfg.directionCfg); //fallback to main sync cfg

    //test if there's a deviating variant within the additional folder pairs
    for (const LocalPairConfig& lpc : mainCfg.additionalPairs)
    {
        const SyncVariant localVariant = getSyncVariant(lpc.localSyncCfg ?
                                                        lpc.localSyncCfg->directionCfg:
                                                        mainCfg.syncCfg.directionCfg);
        if (localVariant != firstVar)
            return std::nullopt;
    }
    return firstVar; //seems to be all in sync...
}

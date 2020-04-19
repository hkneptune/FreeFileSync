// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "structures.h"
#include <iterator>
#include <stdexcept>
#include <ctime>
#include <zen/i18n.h>
#include <zen/time.h>
//#include "path_filter.h"
#include "../afs/concrete.h"

using namespace zen;
using namespace fff;


//use in sync log files where users expect ANSI: https://freefilesync.org/forum/viewtopic.php?t=4647
std::wstring fff::getVariantNameForLog(DirectionConfig::Variant var)
{
    switch (var)
    {
        case DirectionConfig::TWO_WAY:
            return _("Two way") + L" <->";
        case DirectionConfig::MIRROR:
            return _("Mirror") + L" ->";
        case DirectionConfig::UPDATE:
            return _("Update") + L" >";
        case DirectionConfig::CUSTOM:
            return _("Custom");
    }
    assert(false);
    return _("Error");
}


DirectionSet fff::extractDirections(const DirectionConfig& cfg)
{
    DirectionSet output;
    switch (cfg.var)
    {
        case DirectionConfig::TWO_WAY:
            throw std::logic_error("there are no predefined directions for automatic mode! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));

        case DirectionConfig::MIRROR:
            output.exLeftSideOnly  = SyncDirection::RIGHT;
            output.exRightSideOnly = SyncDirection::RIGHT;
            output.leftNewer       = SyncDirection::RIGHT;
            output.rightNewer      = SyncDirection::RIGHT;
            output.different       = SyncDirection::RIGHT;
            output.conflict        = SyncDirection::RIGHT;
            break;

        case DirectionConfig::UPDATE:
            output.exLeftSideOnly  = SyncDirection::RIGHT;
            output.exRightSideOnly = SyncDirection::NONE;
            output.leftNewer       = SyncDirection::RIGHT;
            output.rightNewer      = SyncDirection::NONE;
            output.different       = SyncDirection::RIGHT;
            output.conflict        = SyncDirection::NONE;
            break;

        case DirectionConfig::CUSTOM:
            output = cfg.custom;
            break;
    }
    return output;
}


bool fff::detectMovedFilesSelectable(const DirectionConfig& cfg)
{
    if (cfg.var == DirectionConfig::TWO_WAY)
        return false; //moved files are always detected since we have the database file anyway

    const DirectionSet tmp = fff::extractDirections(cfg);
    return (tmp.exLeftSideOnly  == SyncDirection::RIGHT &&
            tmp.exRightSideOnly == SyncDirection::RIGHT) ||
           (tmp.exLeftSideOnly  == SyncDirection::LEFT &&
            tmp.exRightSideOnly == SyncDirection::LEFT);
}


bool fff::detectMovedFilesEnabled(const DirectionConfig& cfg)
{
    return detectMovedFilesSelectable(cfg) ? cfg.detectMovedFiles : cfg.var == DirectionConfig::TWO_WAY;
}


DirectionSet fff::getTwoWayUpdateSet()
{
    DirectionSet output;
    output.exLeftSideOnly  = SyncDirection::RIGHT;
    output.exRightSideOnly = SyncDirection::LEFT;
    output.leftNewer       = SyncDirection::RIGHT;
    output.rightNewer      = SyncDirection::LEFT;
    output.different       = SyncDirection::NONE;
    output.conflict        = SyncDirection::NONE;
    return output;
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
        case FILE_LEFT_SIDE_ONLY:
            return L"only <-";
        case FILE_RIGHT_SIDE_ONLY:
            return L"only ->";
        case FILE_LEFT_NEWER:
            return L"newer <-";
        case FILE_RIGHT_NEWER:
            return L"newer ->";
        case FILE_DIFFERENT_CONTENT:
            return L"!=";
        case FILE_EQUAL:
        case FILE_DIFFERENT_METADATA: //= sub-category of equal!
            return L"'=="; //added quotation mark to avoid error in Excel cell when exporting to *.cvs
        case FILE_CONFLICT:
            return L"conflict";
    }
    assert(false);
    return std::wstring();
}


std::wstring fff::getSymbol(SyncOperation op)
{
    switch (op)
    {
        case SO_CREATE_NEW_LEFT:
            return L"create <-";
        case SO_CREATE_NEW_RIGHT:
            return L"create ->";
        case SO_DELETE_LEFT:
            return L"delete <-";
        case SO_DELETE_RIGHT:
            return L"delete ->";
        case SO_MOVE_LEFT_FROM:
            return L"move from <-";
        case SO_MOVE_LEFT_TO:
            return L"move to <-";
        case SO_MOVE_RIGHT_FROM:
            return L"move from ->";
        case SO_MOVE_RIGHT_TO:
            return L"move to ->";
        case SO_OVERWRITE_LEFT:
        case SO_COPY_METADATA_TO_LEFT:
            return L"update <-";
        case SO_OVERWRITE_RIGHT:
        case SO_COPY_METADATA_TO_RIGHT:
            return L"update ->";
        case SO_DO_NOTHING:
            return L" -";
        case SO_EQUAL:
            return L"'=="; //added quotation mark to avoid error in Excel cell when exporting to *.cvs
        case SO_UNRESOLVED_CONFLICT:
            return L"conflict";
    };
    assert(false);
    return std::wstring();
}


namespace
{
time_t resolve(size_t value, UnitTime unit, time_t defaultVal)
{
    TimeComp tcLocal = getLocalTime();
    if (tcLocal != TimeComp())
        switch (unit)
        {
            case UnitTime::NONE:
                return defaultVal;

            case UnitTime::TODAY:
                tcLocal.second = 0; //0-61
                tcLocal.minute = 0; //0-59
                tcLocal.hour   = 0; //0-23
                return localToTimeT(tcLocal); //convert local time back to UTC

            case UnitTime::THIS_MONTH:
                tcLocal.second = 0; //0-61
                tcLocal.minute = 0; //0-59
                tcLocal.hour   = 0; //0-23
                tcLocal.day    = 1; //1-31
                return localToTimeT(tcLocal);

            case UnitTime::THIS_YEAR:
                tcLocal.second = 0; //0-61
                tcLocal.minute = 0; //0-59
                tcLocal.hour   = 0; //0-23
                tcLocal.day    = 1; //1-31
                tcLocal.month  = 1; //1-12
                return localToTimeT(tcLocal);

            case UnitTime::LAST_X_DAYS:
                tcLocal.second = 0; //0-61
                tcLocal.minute = 0; //0-59
                tcLocal.hour   = 0; //0-23
                return localToTimeT(tcLocal) - value * 24 * 3600;
        }
    assert(false);
    return defaultVal;
}


uint64_t resolve(size_t value, UnitSize unit, uint64_t defaultVal)
{
    const uint64_t maxVal = std::numeric_limits<uint64_t>::max();

    switch (unit)
    {
        case UnitSize::NONE:
            return defaultVal;
        case UnitSize::BYTE:
            return value;
        case UnitSize::KB:
            return value > maxVal / 1024U ? maxVal : //prevent overflow!!!
                   1024U * value;
        case UnitSize::MB:
            return value > maxVal / (1024 * 1024U) ? maxVal : //prevent overflow!!!
                   1024 * 1024U * value;
    }
    assert(false);
    return defaultVal;
}
}

void fff::resolveUnits(size_t timeSpan, UnitTime unitTimeSpan,
                       size_t sizeMin,  UnitSize unitSizeMin,
                       size_t sizeMax,  UnitSize unitSizeMax,
                       time_t&   timeFrom,  //unit: UTC time, seconds
                       uint64_t& sizeMinBy, //unit: bytes
                       uint64_t& sizeMaxBy) //unit: bytes
{
    timeFrom  = resolve(timeSpan, unitTimeSpan, std::numeric_limits<time_t>::min());
    sizeMinBy = resolve(sizeMin,  unitSizeMin, 0U);
    sizeMaxBy = resolve(sizeMax,  unitSizeMax, std::numeric_limits<uint64_t>::max());
}

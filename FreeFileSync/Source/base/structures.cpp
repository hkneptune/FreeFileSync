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
#include "path_filter.h"
#include "../afs/concrete.h"

using namespace zen;
using namespace fff;


std::vector<unsigned int> fff::fromTimeShiftPhrase(const std::wstring& timeShiftPhrase)
{
    std::wstring tmp = replaceCpy(timeShiftPhrase, L';', L','); //harmonize , and ;
    replace(tmp, L'-', L""); //there is no negative shift => treat as positive!

    std::set<unsigned int> minutes;
    for (const std::wstring& part : split(tmp, L',', SplitType::SKIP_EMPTY))
    {
        if (contains(part, L':'))
            minutes.insert(stringTo<unsigned int>(beforeFirst(part, L':', IF_MISSING_RETURN_NONE)) * 60 +
                           stringTo<unsigned int>(afterFirst (part, L':', IF_MISSING_RETURN_NONE)));
        else
            minutes.insert(stringTo<unsigned int>(part) * 60);
    }
    minutes.erase(0);

    return { minutes.begin(), minutes.end() };
}


std::wstring fff::toTimeShiftPhrase(const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    std::wstring phrase;
    for (auto it = ignoreTimeShiftMinutes.begin(); it != ignoreTimeShiftMinutes.end(); ++it)
    {
        if (it != ignoreTimeShiftMinutes.begin())
            phrase += L", ";

        phrase += numberTo<std::wstring>(*it / 60);
        if (*it % 60 != 0)
            phrase += L':' + printNumber<std::wstring>(L"%02d", static_cast<int>(*it % 60));
    }
    return phrase;
}


std::wstring fff::getVariantName(CompareVariant var)
{
    switch (var)
    {
        case CompareVariant::TIME_SIZE:
            return _("File time and size");
        case CompareVariant::CONTENT:
            return _("File content");
        case CompareVariant::SIZE:
            return _("File size");
    }
    assert(false);
    return _("Error");
}


namespace
{
std::wstring getVariantNameImpl(DirectionConfig::Variant var, const wchar_t* arrowLeft, const wchar_t* arrowRight, const wchar_t* angleRight)
{
    switch (var)
    {
        case DirectionConfig::TWO_WAY:
            return arrowLeft + _("Two way") + arrowRight;
        case DirectionConfig::MIRROR:
            return _("Mirror") + arrowRight;
        case DirectionConfig::UPDATE:
            return _("Update") + angleRight;
        case DirectionConfig::CUSTOM:
            return _("Custom");
    }
    assert(false);
    return _("Error");
}
}


std::wstring fff::getVariantName(DirectionConfig::Variant var)
{
#if 1
    const wchar_t arrowLeft [] = L"<\u2013 ";
    const wchar_t arrowRight[] = L" \u2013>";
    const wchar_t angleRight[] = L" >";
#else
    //const wchar_t arrowLeft [] = L"\u2190 "; //unicode arrows -> too small
    //const wchar_t arrowRight[] = L" \u2192"; //
    //const wchar_t arrowLeft [] = L"\u25C4\u2013 "; //black triangle pointer
    //const wchar_t arrowRight[] = L" \u2013\u25BA"; //
    const wchar_t arrowLeft [] = L"\uFF1C\u2013 "; //fullwidth less-than + en dash
    const wchar_t arrowRight[] = L" \u2013\uFF1E"; //en dash + fullwidth greater-than
    const wchar_t angleRight[] = L" \uFF1E";
    //=> drawback: - not drawn correctly before Vista
    //             - RTL: the full width less-than is not mirrored automatically (=> Windows Unicode bug!?)
#endif
    return getVariantNameImpl(var, arrowLeft, arrowRight, angleRight);
}


//use in sync log files where users expect ANSI: https://freefilesync.org/forum/viewtopic.php?t=4647
std::wstring fff::getVariantNameForLog(DirectionConfig::Variant var)
{
    return getVariantNameImpl(var, L"<-", L"->", L">");
}


DirectionSet fff::extractDirections(const DirectionConfig& cfg)
{
    DirectionSet output;
    switch (cfg.var)
    {
        case DirectionConfig::TWO_WAY:
            throw std::logic_error("there are no predefined directions for automatic mode! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

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


std::wstring fff::getCompVariantName(const MainConfiguration& mainCfg)
{
    const CompareVariant firstVariant = mainCfg.firstPair.localCmpCfg ?
                                        mainCfg.firstPair.localCmpCfg->compareVar :
                                        mainCfg.cmpCfg.compareVar; //fallback to main sync cfg

    //test if there's a deviating variant within the additional folder pairs
    for (const LocalPairConfig& lpc : mainCfg.additionalPairs)
    {
        const CompareVariant thisVariant = lpc.localCmpCfg ?
                                           lpc.localCmpCfg->compareVar :
                                           mainCfg.cmpCfg.compareVar; //fallback to main sync cfg
        if (thisVariant != firstVariant)
            return _("Multiple...");
    }

    //seems to be all in sync...
    return getVariantName(firstVariant);
}


std::wstring fff::getSyncVariantName(const MainConfiguration& mainCfg)
{
    const DirectionConfig::Variant firstVariant = mainCfg.firstPair.localSyncCfg ?
                                                  mainCfg.firstPair.localSyncCfg->directionCfg.var :
                                                  mainCfg.syncCfg.directionCfg.var; //fallback to main sync cfg

    //test if there's a deviating variant within the additional folder pairs
    for (const LocalPairConfig& lpc : mainCfg.additionalPairs)
    {
        const DirectionConfig::Variant thisVariant = lpc.localSyncCfg ?
                                                     lpc.localSyncCfg->directionCfg.var :
                                                     mainCfg.syncCfg.directionCfg.var;
        if (thisVariant != firstVariant)
            return _("Multiple...");
    }

    //seems to be all in sync...
    return getVariantName(firstVariant);
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


std::wstring fff::getSymbol(CompareFilesResult cmpRes)
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


namespace
{
FilterConfig mergeFilterConfig(const FilterConfig& global, const FilterConfig& local)
{
    FilterConfig out = local;

    //hard filter
    if (NameFilter::isNull(local.includeFilter, Zstring())) //fancy way of checking for "*" include
        out.includeFilter = global.includeFilter;
    //else : if both global and local include filters are set, only local filter is preserved

    out.excludeFilter = trimCpy(trimCpy(global.excludeFilter) + Zstr("\n\n") + trimCpy(local.excludeFilter));

    //soft filter
    time_t   loctimeFrom  = 0;
    uint64_t locSizeMinBy = 0;
    uint64_t locSizeMaxBy = 0;
    resolveUnits(out.timeSpan, out.unitTimeSpan,
                 out.sizeMin,  out.unitSizeMin,
                 out.sizeMax,  out.unitSizeMax,
                 loctimeFrom,   //unit: UTC time, seconds
                 locSizeMinBy,  //unit: bytes
                 locSizeMaxBy); //unit: bytes

    //soft filter
    time_t   glotimeFrom  = 0;
    uint64_t gloSizeMinBy = 0;
    uint64_t gloSizeMaxBy = 0;
    resolveUnits(global.timeSpan, global.unitTimeSpan,
                 global.sizeMin,  global.unitSizeMin,
                 global.sizeMax,  global.unitSizeMax,
                 glotimeFrom,
                 gloSizeMinBy,
                 gloSizeMaxBy);

    if (glotimeFrom > loctimeFrom)
    {
        out.timeSpan     = global.timeSpan;
        out.unitTimeSpan = global.unitTimeSpan;
    }
    if (gloSizeMinBy > locSizeMinBy)
    {
        out.sizeMin     = global.sizeMin;
        out.unitSizeMin = global.unitSizeMin;
    }
    if (gloSizeMaxBy < locSizeMaxBy)
    {
        out.sizeMax     = global.sizeMax;
        out.unitSizeMax = global.unitSizeMax;
    }
    return out;
}


inline
bool effectivelyEmpty(const LocalPairConfig& lpc)
{
    return trimCpy(lpc.folderPathPhraseLeft ).empty() &&
           trimCpy(lpc.folderPathPhraseRight).empty();
}
}


MainConfiguration fff::merge(const std::vector<MainConfiguration>& mainCfgs)
{
    assert(!mainCfgs.empty());
    if (mainCfgs.empty())
        return MainConfiguration();

    if (mainCfgs.size() == 1) //mergeConfigFilesImpl relies on this!
        return mainCfgs[0];   //

    //merge folder pair config
    std::vector<LocalPairConfig> mergedCfgs;
    for (const MainConfiguration& mainCfg : mainCfgs)
    {
        std::vector<LocalPairConfig> tmpCfgs;

        //skip empty folder pairs
        if (!effectivelyEmpty(mainCfg.firstPair))
            tmpCfgs.push_back(mainCfg.firstPair);

        for (const LocalPairConfig& lpc : mainCfg.additionalPairs)
            if (!effectivelyEmpty(lpc))
                tmpCfgs.push_back(lpc);

        //move all configuration down to item level
        for (LocalPairConfig& lpc : tmpCfgs)
        {
            if (!lpc.localCmpCfg)
                lpc.localCmpCfg = mainCfg.cmpCfg;

            if (!lpc.localSyncCfg)
                lpc.localSyncCfg = mainCfg.syncCfg;

            lpc.localFilter = mergeFilterConfig(mainCfg.globalFilter, lpc.localFilter);
        }
        append(mergedCfgs, tmpCfgs);
    }

    if (mergedCfgs.empty())
        return MainConfiguration();

    //optimization: remove redundant configuration

    //########################################################################################################################
    //find out which comparison and synchronization setting are used most often and use them as new "header"
    std::vector<std::pair<CompConfig, int>> cmpCfgStat;
    std::vector<std::pair<SyncConfig, int>> syncCfgStat;
    for (const LocalPairConfig& lpc : mergedCfgs)
    {
        //a rather inefficient algorithm, but it does not require a less-than operator:
        {
            const CompConfig& cmpCfg = *lpc.localCmpCfg;

            auto it = std::find_if(cmpCfgStat.begin(), cmpCfgStat.end(),
            [&](const std::pair<CompConfig, int>& entry) { return effectivelyEqual(entry.first, cmpCfg); });
            if (it == cmpCfgStat.end())
                cmpCfgStat.emplace_back(cmpCfg, 1);
            else
                ++(it->second);
        }
        {
            const SyncConfig& syncCfg = *lpc.localSyncCfg;

            auto it = std::find_if(syncCfgStat.begin(), syncCfgStat.end(),
            [&](const std::pair<SyncConfig, int>& entry) { return effectivelyEqual(entry.first, syncCfg); });
            if (it == syncCfgStat.end())
                syncCfgStat.emplace_back(syncCfg, 1);
            else
                ++(it->second);
        }
    }

    //set most-used comparison and synchronization settings as new header options
    const CompConfig cmpCfgHead = cmpCfgStat.empty() ? CompConfig() :
                                  std::max_element(cmpCfgStat.begin(), cmpCfgStat.end(),
    [](const std::pair<CompConfig, int>& lhs, const std::pair<CompConfig, int>& rhs) { return lhs.second < rhs.second; })->first;

    const SyncConfig syncCfgHead = syncCfgStat.empty() ? SyncConfig() :
                                   std::max_element(syncCfgStat.begin(), syncCfgStat.end(),
    [](const std::pair<SyncConfig, int>& lhs, const std::pair<SyncConfig, int>& rhs) { return lhs.second < rhs.second; })->first;
    //########################################################################################################################

    FilterConfig globalFilter;
    const bool allFiltersEqual = std::all_of(mergedCfgs.begin(), mergedCfgs.end(), [&](const LocalPairConfig& lpc) { return lpc.localFilter == mergedCfgs[0].localFilter; });
    if (allFiltersEqual)
        globalFilter = mergedCfgs[0].localFilter;

    //strip redundancy...
    for (LocalPairConfig& lpc : mergedCfgs)
    {
        //if local config matches output global config we don't need local one
        if (lpc.localCmpCfg &&
            effectivelyEqual(*lpc.localCmpCfg, cmpCfgHead))
            lpc.localCmpCfg = {};

        if (lpc.localSyncCfg &&
            effectivelyEqual(*lpc.localSyncCfg, syncCfgHead))
            lpc.localSyncCfg = {};

        if (allFiltersEqual) //use global filter in this case
            lpc.localFilter = FilterConfig();
    }

    std::map<AfsDevice, size_t> mergedParallelOps;
    for (const MainConfiguration& mainCfg : mainCfgs)
        for (const auto& [rootPath, parallelOps] : mainCfg.deviceParallelOps)
            mergedParallelOps[rootPath] = std::max(mergedParallelOps[rootPath], parallelOps);

    //final assembly
    MainConfiguration cfgOut;
    cfgOut.cmpCfg       = cmpCfgHead;
    cfgOut.syncCfg      = syncCfgHead;
    cfgOut.globalFilter = globalFilter;
    cfgOut.firstPair    = mergedCfgs[0];
    cfgOut.additionalPairs.assign(mergedCfgs.begin() + 1, mergedCfgs.end());
    cfgOut.deviceParallelOps = mergedParallelOps;

    cfgOut.ignoreErrors = std::all_of(mainCfgs.begin(), mainCfgs.end(), [](const MainConfiguration& mainCfg) { return mainCfg.ignoreErrors; });

    cfgOut.automaticRetryCount = std::max_element(mainCfgs.begin(), mainCfgs.end(),
    [](const MainConfiguration& lhs, const MainConfiguration& rhs) { return lhs.automaticRetryCount < rhs.automaticRetryCount; })->automaticRetryCount;

    cfgOut.automaticRetryDelay = std::max_element(mainCfgs.begin(), mainCfgs.end(),
    [](const MainConfiguration& lhs, const MainConfiguration& rhs) { return lhs.automaticRetryDelay < rhs.automaticRetryDelay; })->automaticRetryDelay;

    for (const MainConfiguration& mainCfg : mainCfgs)
        if (!mainCfg.altLogFolderPathPhrase.empty())
        {
            cfgOut.altLogFolderPathPhrase = mainCfg.altLogFolderPathPhrase;
            break;
        }

    //cfgOut.postSyncCommand   = mainCfgs[0].postSyncCommand;   -> better leave at default ... !?
    //cfgOut.postSyncCondition = mainCfgs[0].postSyncCondition; ->
    return cfgOut;
}

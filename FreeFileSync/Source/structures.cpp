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
#include "lib/hard_filter.h"

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


//use in sync log files where users expect ANSI: https://www.freefilesync.org/forum/viewtopic.php?t=4647
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


std::wstring MainConfiguration::getCompVariantName() const
{
    const CompareVariant firstVariant = firstPair.altCmpConfig.get() ?
                                        firstPair.altCmpConfig->compareVar :
                                        cmpConfig.compareVar; //fallback to main sync cfg

    //test if there's a deviating variant within the additional folder pairs
    for (const FolderPairEnh& fp : additionalPairs)
    {
        const CompareVariant thisVariant = fp.altCmpConfig.get() ?
                                           fp.altCmpConfig->compareVar :
                                           cmpConfig.compareVar; //fallback to main sync cfg
        if (thisVariant != firstVariant)
            return _("Multiple...");
    }

    //seems to be all in sync...
    return getVariantName(firstVariant);
}


std::wstring MainConfiguration::getSyncVariantName() const
{
    const DirectionConfig::Variant firstVariant = firstPair.altSyncConfig.get() ?
                                                  firstPair.altSyncConfig->directionCfg.var :
                                                  syncCfg.directionCfg.var; //fallback to main sync cfg

    //test if there's a deviating variant within the additional folder pairs
    for (const FolderPairEnh& fp : additionalPairs)
    {
        const DirectionConfig::Variant thisVariant = fp.altSyncConfig.get() ?
                                                     fp.altSyncConfig->directionCfg.var :
                                                     syncCfg.directionCfg.var;
        if (thisVariant != firstVariant)
            return _("Multiple...");
    }

    //seems to be all in sync...
    return getVariantName(firstVariant);
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
/*
int daysSinceBeginOfWeek(int dayOfWeek) //0-6, 0=Monday, 6=Sunday
{
    assert(0 <= dayOfWeek && dayOfWeek <= 6);
#ifdef ZEN_WIN
    DWORD firstDayOfWeek = 0;
    if (::GetLocaleInfo(LOCALE_USER_DEFAULT,                         //__in  LCID Locale,
                        LOCALE_IFIRSTDAYOFWEEK |                     // first day of week specifier, 0-6, 0=Monday, 6=Sunday
                        LOCALE_RETURN_NUMBER,                        //__in  LCTYPE LCType,
                        reinterpret_cast<LPTSTR>(&firstDayOfWeek),   //__out LPTSTR lpLCData,
                        sizeof(firstDayOfWeek) / sizeof(TCHAR)) > 0) //__in  int cchData
    {
        assert(firstDayOfWeek <= 6);
        return (dayOfWeek + (7 - firstDayOfWeek)) % 7;
    }
    else //default
#endif
        return dayOfWeek; //let all weeks begin with monday
}
*/


time_t resolve(size_t value, UnitTime unit, time_t defaultVal)
{
    TimeComp tcLocal = getLocalTime();
    if (tcLocal == TimeComp())
    {
        assert(false);
        return defaultVal;
    }

    switch (unit)
    {
        case UnitTime::NONE:
            return defaultVal;

        case UnitTime::TODAY:
            tcLocal.second = 0; //0-61
            tcLocal.minute = 0; //0-59
            tcLocal.hour   = 0; //0-23
            return localToTimeT(tcLocal); //convert local time back to UTC

        //case UnitTime::THIS_WEEK:
        //{
        //    localTimeFmt->tm_sec  = 0; //0-61
        //    localTimeFmt->tm_min  = 0; //0-59
        //    localTimeFmt->tm_hour = 0; //0-23
        //    const time_t timeFrom = ::mktime(localTimeFmt);

        //    int dayOfWeek = (localTimeFmt->tm_wday + 6) % 7; //tm_wday := days since Sunday   0-6
        //    // +6 == -1 in Z_7

        //    return int64_t(timeFrom) - daysSinceBeginOfWeek(dayOfWeek) * 24 * 3600;
        //}

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
    return localToTimeT(tcLocal);
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
    if (NameFilter::isNull(out.includeFilter, Zstring())) //fancy way of checking for "*" include
        out.includeFilter = global.includeFilter;
    //else: if both global and local include filter contain data, only local filter is preserved

    trim(out.excludeFilter, true, false);
    out.excludeFilter = global.excludeFilter + Zstr("\n") + out.excludeFilter;
    trim(out.excludeFilter, true, false);

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
bool effectivelyEmpty(const FolderPairEnh& fp)
{
    return trimCpy(fp.folderPathPhraseLeft_ ).empty() &&
           trimCpy(fp.folderPathPhraseRight_).empty();
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
    std::vector<FolderPairEnh> fpMerged;
    for (const MainConfiguration& mainCfg : mainCfgs)
    {
        std::vector<FolderPairEnh> fpTmp;

        //skip empty folder pairs
        if (!effectivelyEmpty(mainCfg.firstPair))
            fpTmp.push_back(mainCfg.firstPair);
        for (const FolderPairEnh& fp : mainCfg.additionalPairs)
            if (!effectivelyEmpty(fp))
                fpTmp.push_back(fp);

        //move all configuration down to item level
        for (FolderPairEnh& fp : fpTmp)
        {
            if (!fp.altCmpConfig.get())
                fp.altCmpConfig = std::make_shared<CompConfig>(mainCfg.cmpConfig);

            if (!fp.altSyncConfig.get())
                fp.altSyncConfig = std::make_shared<SyncConfig>(mainCfg.syncCfg);

            fp.localFilter = mergeFilterConfig(mainCfg.globalFilter, fp.localFilter);
        }
        append(fpMerged, fpTmp);
    }

    if (fpMerged.empty())
        return MainConfiguration();

    //optimization: remove redundant configuration

    //########################################################################################################################
    //find out which comparison and synchronization setting are used most often and use them as new "header"
    std::vector<std::pair<CompConfig, int>> cmpCfgStat;
    std::vector<std::pair<SyncConfig, int>> syncCfgStat;
    for (const FolderPairEnh& fp : fpMerged)
    {
        //a rather inefficient algorithm, but it does not require a less-than operator:
        {
            const CompConfig& cmpCfg = *fp.altCmpConfig;

            auto it = std::find_if(cmpCfgStat.begin(), cmpCfgStat.end(),
            [&](const std::pair<CompConfig, int>& entry) { return effectivelyEqual(entry.first, cmpCfg); });
            if (it == cmpCfgStat.end())
                cmpCfgStat.emplace_back(cmpCfg, 1);
            else
                ++(it->second);
        }
        {
            const SyncConfig& syncCfg = *fp.altSyncConfig;

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
    const bool allFiltersEqual = std::all_of(fpMerged.begin(), fpMerged.end(), [&](const FolderPairEnh& fp) { return fp.localFilter == fpMerged[0].localFilter; });
    if (allFiltersEqual)
        globalFilter = fpMerged[0].localFilter;

    //strip redundancy...
    for (FolderPairEnh& fp : fpMerged)
    {
        //if local config matches output global config we don't need local one
        if (fp.altCmpConfig &&
            effectivelyEqual(*fp.altCmpConfig, cmpCfgHead))
            fp.altCmpConfig.reset();

        if (fp.altSyncConfig &&
            effectivelyEqual(*fp.altSyncConfig, syncCfgHead))
            fp.altSyncConfig.reset();

        if (allFiltersEqual) //use global filter in this case
            fp.localFilter = FilterConfig();
    }

    //final assembly
    MainConfiguration cfgOut;
    cfgOut.cmpConfig    = cmpCfgHead;
    cfgOut.syncCfg      = syncCfgHead;
    cfgOut.globalFilter = globalFilter;
    cfgOut.firstPair    = fpMerged[0];
    cfgOut.additionalPairs.assign(fpMerged.begin() + 1, fpMerged.end());
    cfgOut.ignoreErrors = std::all_of(mainCfgs.begin(), mainCfgs.end(), [](const MainConfiguration& mainCfg) { return mainCfg.ignoreErrors; });
    //cfgOut.postSyncCommand   = mainCfgs[0].postSyncCommand;   -> better leave at default ... !?
    //cfgOut.postSyncCondition = mainCfgs[0].postSyncCondition; ->
    return cfgOut;
}

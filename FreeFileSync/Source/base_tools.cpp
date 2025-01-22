// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "base_tools.h"
#include "base/path_filter.h"

using namespace zen;
using namespace fff;


std::vector<unsigned int> fff::fromTimeShiftPhrase(const std::wstring_view timeShiftPhrase)
{
    std::vector<unsigned int> minutes;

    split2(timeShiftPhrase, [](wchar_t c) { return c == L',' || c == L';' || c == L' '; }, //delimiters
    [&minutes](const std::wstring_view block)
    {
        if (!block.empty())
        {
            std::wstring part(block);
            replace(part, L'-', L""); //there is no negative shift => treat as positive!

            const unsigned int timeShift = stringTo<unsigned int>(beforeFirst(part, L':', IfNotFoundReturn::all)) * 60 +
                                           stringTo<unsigned int>(afterFirst (part, L':', IfNotFoundReturn::none));
            if (timeShift > 0)
                minutes.push_back(timeShift);
        }
    });
    removeDuplicates(minutes);
    return minutes;
}


std::wstring fff::toTimeShiftPhrase(const std::vector<unsigned int>& ignoreTimeShiftMinutes)
{
    std::wstring phrase;
    for (const unsigned int timeShift : ignoreTimeShiftMinutes)
    {
        if (!phrase.empty())
            phrase += L", ";

        phrase += numberTo<std::wstring>(timeShift / 60);

        if (const unsigned int shiftRem = timeShift % 60;
            shiftRem != 0)
            phrase += L':' + printNumber<std::wstring>(L"%02d", static_cast<int>(shiftRem));
    }
    return phrase;
}


void fff::logNonDefaultSettings(const GlobalConfig& globalCfg, PhaseCallback& callback)
{
    const GlobalConfig defaultSettings;
    std::wstring changedSettingsMsg;

    if (globalCfg.failSafeFileCopy != defaultSettings.failSafeFileCopy)
        changedSettingsMsg += L"\n" + (TAB_SPACE + _("Fail-safe file copy")) + L": " + (globalCfg.failSafeFileCopy ? _("Enabled") : _("Disabled"));

    if (globalCfg.copyLockedFiles != defaultSettings.copyLockedFiles)
        changedSettingsMsg += L"\n" + (TAB_SPACE + _("Copy locked files")) + L": " + (globalCfg.copyLockedFiles ? _("Enabled") : _("Disabled"));

    if (globalCfg.copyFilePermissions != defaultSettings.copyFilePermissions)
        changedSettingsMsg += L"\n" + (TAB_SPACE + _("Copy file access permissions")) + L": " + (globalCfg.copyFilePermissions ? _("Enabled") : _("Disabled"));

    if (globalCfg.fileTimeTolerance != defaultSettings.fileTimeTolerance)
        changedSettingsMsg += L"\n" + (TAB_SPACE + _("File time tolerance")) + L": " + formatNumber(globalCfg.fileTimeTolerance);

    if (globalCfg.runWithBackgroundPriority != defaultSettings.runWithBackgroundPriority)
        changedSettingsMsg += L"\n" + (TAB_SPACE + _("Run with background priority")) + L": " + (globalCfg.runWithBackgroundPriority ? _("Enabled") : _("Disabled"));

    if (globalCfg.createLockFile != defaultSettings.createLockFile)
        changedSettingsMsg += L"\n" + (TAB_SPACE + _("Lock directories during sync")) + L": " + (globalCfg.createLockFile ? _("Enabled") : _("Disabled"));

    if (globalCfg.verifyFileCopy != defaultSettings.verifyFileCopy)
        changedSettingsMsg += L"\n" + (TAB_SPACE + _("Verify copied files")) + L": " + (globalCfg.verifyFileCopy ? _("Enabled") : _("Disabled"));

    if (!changedSettingsMsg.empty())
        callback.logMessage(_("Using non-default global settings:") + changedSettingsMsg, PhaseCallback::MsgType::info); //throw X
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
    return AFS::isNullPath(createAbstractPath(lpc.folderPathPhraseLeft)) &&
           AFS::isNullPath(createAbstractPath(lpc.folderPathPhraseRight));
}
}


FfsGuiConfig fff::merge(const std::vector<FfsGuiConfig>& guiCfgs)
{
    assert(!guiCfgs.empty());
    if (guiCfgs.empty())
        return FfsGuiConfig();

    if (guiCfgs.size() == 1) //
        return guiCfgs[0];   //return "as is"

    //merge folder pair config
    std::vector<LocalPairConfig> mergedCfgs;

    for (const FfsGuiConfig& guiCfg : guiCfgs)
    {
        std::vector<LocalPairConfig> tmpCfgs;

        //skip empty folder pairs
        if (!effectivelyEmpty(guiCfg.mainCfg.firstPair))
            tmpCfgs.push_back(guiCfg.mainCfg.firstPair);

        for (const LocalPairConfig& lpc : guiCfg.mainCfg.additionalPairs)
            if (!effectivelyEmpty(lpc))
                tmpCfgs.push_back(lpc);

        //move all configuration down to item level
        for (LocalPairConfig& lpc : tmpCfgs)
        {
            if (!lpc.localCmpCfg)
                lpc.localCmpCfg = guiCfg.mainCfg.cmpCfg;

            if (!lpc.localSyncCfg)
                lpc.localSyncCfg = guiCfg.mainCfg.syncCfg;

            lpc.localFilter = mergeFilterConfig(guiCfg.mainCfg.globalFilter, lpc.localFilter);
        }
        append(mergedCfgs, tmpCfgs);
    }

    if (mergedCfgs.empty())
        mergedCfgs.emplace_back();

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
    for (const FfsGuiConfig& guiCfg : guiCfgs)
        for (const auto& [rootPath, parallelOps] : guiCfg.mainCfg.deviceParallelOps)
            mergedParallelOps[rootPath] = std::max(mergedParallelOps[rootPath], parallelOps);

    //final assembly
    FfsGuiConfig cfgOut
    {
        .mainCfg = MainConfiguration
        {
            .cmpCfg       = cmpCfgHead,
            .syncCfg      = syncCfgHead,
            .globalFilter = globalFilter,
            .firstPair    = mergedCfgs[0],
            .deviceParallelOps = mergedParallelOps,

            .ignoreErrors = std::all_of(guiCfgs.begin(), guiCfgs.end(), [](const FfsGuiConfig& guiCfg) { return guiCfg.mainCfg.ignoreErrors; }),

            .autoRetryCount = std::max_element(guiCfgs.begin(), guiCfgs.end(),
            [](const FfsGuiConfig& lhs, const FfsGuiConfig& rhs) { return lhs.mainCfg.autoRetryCount < rhs.mainCfg.autoRetryCount; })->mainCfg.autoRetryCount,

            .autoRetryDelay = std::max_element(guiCfgs.begin(), guiCfgs.end(),
            [](const FfsGuiConfig& lhs, const FfsGuiConfig& rhs) { return lhs.mainCfg.autoRetryDelay < rhs.mainCfg.autoRetryDelay; })->mainCfg.autoRetryDelay,
        }
    };
    cfgOut.mainCfg.additionalPairs.assign(mergedCfgs.begin() + 1, mergedCfgs.end());


    for (const FfsGuiConfig& guiCfg : guiCfgs)
    {
        if (cfgOut.mainCfg.altLogFolderPathPhrase.empty())
            cfgOut.mainCfg.altLogFolderPathPhrase = guiCfg.mainCfg.altLogFolderPathPhrase;

        if (cfgOut.mainCfg.emailNotifyAddress.empty())
        {
            cfgOut.mainCfg.emailNotifyAddress   = guiCfg.mainCfg.emailNotifyAddress;
            cfgOut.mainCfg.emailNotifyCondition = guiCfg.mainCfg.emailNotifyCondition;
        }

        if (!guiCfg.notes.empty())
            cfgOut.notes += guiCfg.notes + L"\n\n";
    }
    trim(cfgOut.notes);

    //cfgOut.mainCfg.postSyncCommand   = -> better leave at default ... !?
    //cfgOut.mainCfg.postSyncCondition = ->
    //cfgOut.gridViewType        ->
    return cfgOut;
}

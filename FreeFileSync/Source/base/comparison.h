// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef COMPARISON_H_8032178534545426
#define COMPARISON_H_8032178534545426

#include "file_hierarchy.h"
#include "process_callback.h"
#include "norm_filter.h"
#include "lock_holder.h"


namespace fff
{
struct FolderPairCfg
{
    FolderPairCfg(const Zstring& folderPathPhraseLeft,
                  const Zstring& folderPathPhraseRight,
                  CompareVariant cmpVar,
                  SymLinkHandling handleSymlinksIn,
                  const std::vector<unsigned int>& ignoreTimeShiftMinutesIn,
                  const NormalizedFilter& filterIn,
                  const SyncDirectionConfig& directCfg) :
        folderPathPhraseLeft_ (folderPathPhraseLeft),
        folderPathPhraseRight_(folderPathPhraseRight),
        compareVar(cmpVar),
        handleSymlinks(handleSymlinksIn),
        ignoreTimeShiftMinutes(ignoreTimeShiftMinutesIn),
        filter(filterIn),
        directionCfg(directCfg) {}

    Zstring folderPathPhraseLeft_;  //unresolved directory names as entered by user!
    Zstring folderPathPhraseRight_; //

    CompareVariant compareVar;
    SymLinkHandling handleSymlinks;
    std::vector<unsigned int> ignoreTimeShiftMinutes;

    NormalizedFilter filter;

    SyncDirectionConfig directionCfg;
};

std::vector<FolderPairCfg> extractCompareCfg(const MainConfiguration& mainCfg); //fill FolderPairCfg and resolve folder pairs

//FFS core routine:     output.size() == fpCfgList.size() or 0 on fatal error
FolderComparison compare(WarningDialogs& warnings,
                         unsigned int fileTimeTolerance,
                         const AFS::RequestPasswordFun& requestPassword /*throw X*/,
                         bool runWithBackgroundPriority,
                         bool createDirLocks,
                         std::unique_ptr<LockHolder>& dirLocks, //out
                         const std::vector<FolderPairCfg>& fpCfgList,
                         ProcessCallback& callback /*throw X*/); //throw X
}

#endif //COMPARISON_H_8032178534545426

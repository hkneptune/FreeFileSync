// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "comparison.h"
#include <zen/process_priority.h>
#include <zen/perf.h>
#include <zen/time.h>
#include <wx/datetime.h>
#include "algorithm.h"
#include "parallel_scan.h"
#include "dir_exist_async.h"
#include "db_file.h"
#include "binary.h"
#include "cmp_filetime.h"
#include "status_handler_impl.h"
#include "../afs/concrete.h"
#include "../afs/native.h"

using namespace zen;
using namespace fff;


std::vector<FolderPairCfg> fff::extractCompareCfg(const MainConfiguration& mainCfg)
{
    //merge first and additional pairs
    std::vector<LocalPairConfig> localCfgs = {mainCfg.firstPair};
    append(localCfgs, mainCfg.additionalPairs);

    std::vector<FolderPairCfg> output;

    for (const LocalPairConfig& lpc : localCfgs)
    {
        const CompConfig cmpCfg  = lpc.localCmpCfg  ? *lpc.localCmpCfg  : mainCfg.cmpCfg;
        const SyncConfig syncCfg = lpc.localSyncCfg ? *lpc.localSyncCfg : mainCfg.syncCfg;
        NormalizedFilter filter = normalizeFilters(mainCfg.globalFilter, lpc.localFilter);

        //exclude sync.ffs_db and lock files
        //=> can't put inside fff::parallelDeviceTraversal() which is also used by versioning
        filter.nameFilter = filter.nameFilter.ref().copyFilterAddingExclusion(Zstring(Zstr("*")) + SYNC_DB_FILE_ENDING + Zstr("\n*") + LOCK_FILE_ENDING);

        output.push_back(
        {
            lpc.folderPathPhraseLeft, lpc.folderPathPhraseRight,
            cmpCfg.compareVar,
            cmpCfg.handleSymlinks,
            cmpCfg.ignoreTimeShiftMinutes,
            filter,
            syncCfg.directionCfg
        });
    }
    return output;
}

//------------------------------------------------------------------------------------------
namespace
{
struct ResolvedFolderPair
{
    AbstractPath folderPathLeft;
    AbstractPath folderPathRight;
};


struct ResolvedBaseFolders
{
    std::vector<ResolvedFolderPair> resolvedPairs;
    FolderStatus baseFolderStatus;
};


ResolvedBaseFolders initializeBaseFolders(const std::vector<FolderPairCfg>& fpCfgList,
                                          bool allowUserInteraction,
                                          WarningDialogs& warnings,
                                          PhaseCallback& callback /*throw X*/) //throw X
{
    ResolvedBaseFolders output;
    std::set<AbstractPath> allFolders;

    tryReportingError([&]
    {
        //support "retry" for environment variable and variable driver letter resolution!
        allFolders.clear();
        output.resolvedPairs.clear();
        for (const FolderPairCfg& fpCfg : fpCfgList)
        {
            output.resolvedPairs.push_back(
            {
                createAbstractPath(fpCfg.folderPathPhraseLeft_),
                createAbstractPath(fpCfg.folderPathPhraseRight_)});

            allFolders.insert(output.resolvedPairs.back().folderPathLeft);
            allFolders.insert(output.resolvedPairs.back().folderPathRight);
        }
        //---------------------------------------------------------------------------
        output.baseFolderStatus = getFolderStatusNonBlocking(allFolders,
                                                             allowUserInteraction, callback); //throw X
        if (!output.baseFolderStatus.failedChecks.empty())
        {
            std::wstring msg = _("Cannot find the following folders:") + L'\n';

            for (const auto& [folderPath, error] : output.baseFolderStatus.failedChecks)
                msg += L'\n' + AFS::getDisplayPath(folderPath);

            msg += L"\n___________________________________________";
            for (const auto& [folderPath, error] : output.baseFolderStatus.failedChecks)
                msg += L"\n\n" + replaceCpy(error.toString(), L"\n\n", L'\n');

            throw FileError(msg);
        }
    }, callback); //throw X


    if (!output.baseFolderStatus.notExisting.empty())
    {
        std::wstring msg = _("The following folders do not yet exist:") + L'\n';

        for (const AbstractPath& folderPath : output.baseFolderStatus.notExisting)
            msg += L'\n' + AFS::getDisplayPath(folderPath);

        msg += L"\n\n";
        msg +=  _("The folders are created automatically when needed.");

        callback.reportWarning(msg, warnings.warnFolderNotExisting); //throw X
    }

    //---------------------------------------------------------------------------
    std::map<std::pair<AfsDevice, ZstringNoCase>, std::set<AbstractPath>> ciPathAliases;

    for (const AbstractPath& ap : allFolders)
        ciPathAliases[std::pair(ap.afsDevice, ap.afsPath.value)].insert(ap);

    if (std::any_of(ciPathAliases.begin(), ciPathAliases.end(), [](const auto& item) { return item.second/*aliases*/.size() > 1; }))
    {
        std::wstring msg = _("The following folder paths differ in case. Please use a single form in order to avoid duplicate accesses.");
        for (const auto& [key, aliases] : ciPathAliases)
            if (aliases.size() > 1)
            {
                msg += L'\n';
                for (const AbstractPath& aliasPath : aliases)
                    msg += L'\n' + AFS::getDisplayPath(aliasPath);
            }

        callback.reportWarning(msg, warnings.warnFoldersDifferInCase); //throw X

        //what about /folder and /Folder/subfolder? => yes, inconsistent, but doesn't matter for FFS
    }
    //---------------------------------------------------------------------------

    return output;
}

//#############################################################################################################################

class ComparisonBuffer
{
public:
    ComparisonBuffer(const std::set<DirectoryKey>& folderKeys,
                     const FolderStatus& baseFolderStatus,
                     int fileTimeTolerance,
                     ProcessCallback& callback);

    //create comparison result table and fill category except for files existing on both sides: undefinedFiles and undefinedSymlinks are appended!
    std::shared_ptr<BaseFolderPair> compareByTimeSize(const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const;
    std::shared_ptr<BaseFolderPair> compareBySize    (const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const;
    std::vector<std::shared_ptr<BaseFolderPair>> compareByContent(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad) const;

private:
    ComparisonBuffer           (const ComparisonBuffer&) = delete;
    ComparisonBuffer& operator=(const ComparisonBuffer&) = delete;

    std::shared_ptr<BaseFolderPair> performComparison(const ResolvedFolderPair& fp,
                                                      const FolderPairCfg& fpCfg,
                                                      std::vector<FilePair*>& undefinedFiles,
                                                      std::vector<SymlinkPair*>& undefinedSymlinks) const;

    std::map<DirectoryKey, DirectoryValue> folderBuffer_; //contains entries for *all* scanned folders!
    const int fileTimeTolerance_;
    const FolderStatus& folderStatus_;
    ProcessCallback& cb_;
};


ComparisonBuffer::ComparisonBuffer(const std::set<DirectoryKey>& folderKeys,
                                   const FolderStatus& folderStatus,
                                   int fileTimeTolerance,
                                   ProcessCallback& callback) :
    fileTimeTolerance_(fileTimeTolerance),
    folderStatus_(folderStatus),
    cb_(callback)
{
    std::set<DirectoryKey> foldersToRead;
    for (const DirectoryKey& folderKey : folderKeys)
        if (folderStatus.existing.contains(folderKey.folderPath))
            foldersToRead.insert(folderKey); //only traverse *existing* folders

    //------------------------------------------------------------------
    const std::chrono::steady_clock::time_point compareStartTime = std::chrono::steady_clock::now();
    int itemsReported = 0;

    auto onStatusUpdate = [&, textScanning = _("Scanning:") + L' '](const std::wstring& statusLine, int itemsTotal)
    {
        callback.updateDataProcessed(itemsTotal - itemsReported, 0); //noexcept
        itemsReported = itemsTotal;

        callback.updateStatus(textScanning + statusLine); //throw X
    };

    folderBuffer_ = parallelDeviceTraversal(foldersToRead,
    [&](const PhaseCallback::ErrorInfo& errorInfo) { return callback.reportError(errorInfo); }, //throw X
    onStatusUpdate, //throw X
    UI_UPDATE_INTERVAL / 2); //every ~50 ms

    const int64_t totalTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - compareStartTime).count();

    callback.logInfo(_("Comparison finished:") + L' ' +
                     _P("1 item found", "%x items found", itemsReported) + SPACED_DASH +
                     _("Time elapsed:") + L' ' + copyStringTo<std::wstring>(wxTimeSpan::Seconds(totalTimeSec).Format())); //throw X
    //------------------------------------------------------------------

    //folderStatus_.existing already in buffer, now create entries for the rest:
    for (const DirectoryKey& folderKey : folderKeys)
        if (auto it = folderStatus_.failedChecks.find(folderKey.folderPath);
            it != folderStatus_.failedChecks.end())
            //make sure all items are disabled => avoid user panicking: https://freefilesync.org/forum/viewtopic.php?t=7582
            folderBuffer_[folderKey].failedFolderReads[Zstring() /*empty string for root*/] = utfTo<Zstringc>(it->second.toString());
        else
        {
            folderBuffer_[folderKey];
            assert(folderStatus_.existing   .contains(folderKey.folderPath) ||
                   folderStatus_.notExisting.contains(folderKey.folderPath) ||
                   AFS::isNullPath(folderKey.folderPath));
        }
}


//--------------------assemble conflict descriptions---------------------------

//const wchar_t arrowLeft [] = L"\u2190";
//const wchar_t arrowRight[] = L"\u2192"; unicode arrows -> too small
const wchar_t arrowLeft [] = L"<-";
const wchar_t arrowRight[] = L"->";

//NOTE: conflict texts are NOT expected to contain additional path info (already implicit through associated item!)
//      => only add path info if information is relevant, e.g. conflict is specific to left/right side only

template <SelectSide side, class FileOrLinkPair> inline
Zstringc getConflictInvalidDate(const FileOrLinkPair& file)
{
    return utfTo<Zstringc>(replaceCpy(_("File %x has an invalid date."), L"%x", fmtPath(AFS::getDisplayPath(file.template getAbstractPath<side>()))) + L'\n' +
                           TAB_SPACE + _("Date:") + L' ' + formatUtcToLocalTime(file.template getLastWriteTime<side>()));
}


Zstringc getConflictSameDateDiffSize(const FilePair& file)
{
    return utfTo<Zstringc>(_("Files have the same date but a different size.") + L'\n' +
                           TAB_SPACE + arrowLeft  + L' ' + _("Date:") + L' ' + formatUtcToLocalTime(file.getLastWriteTime<SelectSide::left >()) + TAB_SPACE + _("Size:") + L' ' + formatNumber(file.getFileSize<SelectSide::left>()) + L'\n' +
                           TAB_SPACE + arrowRight + L' ' + _("Date:") + L' ' + formatUtcToLocalTime(file.getLastWriteTime<SelectSide::right>()) + TAB_SPACE + _("Size:") + L' ' + formatNumber(file.getFileSize<SelectSide::right>()));
}


Zstringc getConflictSkippedBinaryComparison()
{
    return utfTo<Zstringc>(_("Content comparison was skipped for excluded files."));
}


Zstringc getDescrDiffMetaShortnameCase(const FileSystemObject& fsObj)
{
    return utfTo<Zstringc>(_("Items differ in attributes only") + L'\n' +
                           TAB_SPACE + arrowLeft  + L' ' + fmtPath(fsObj.getItemName<SelectSide::left >()) + L'\n' +
                           TAB_SPACE + arrowRight + L' ' + fmtPath(fsObj.getItemName<SelectSide::right>()));
}


#if 0
template <class FileOrLinkPair>
Zstringc getDescrDiffMetaData(const FileOrLinkPair& file)
{
    return utfTo<Zstringc>(_("Items differ in attributes only") + L'\n' +
                           TAB_SPACE + arrowLeft  + L' ' + _("Date:") + L' ' + formatUtcToLocalTime(file.template getLastWriteTime<SelectSide::left >()) + L'\n' +
                           TAB_SPACE + arrowRight + L' ' + _("Date:") + L' ' + formatUtcToLocalTime(file.template getLastWriteTime<SelectSide::right>()));
}
#endif


Zstringc getConflictAmbiguousItemName(const Zstring& itemName)
{
    return utfTo<Zstringc>(replaceCpy(_("The name %x is used by more than one item in the folder."), L"%x", fmtPath(itemName)));
}

//-----------------------------------------------------------------------------

void categorizeSymlinkByTime(SymlinkPair& symlink)
{
    //categorize symlinks that exist on both sides
    switch (compareFileTime(symlink.getLastWriteTime<SelectSide::left>(),
                            symlink.getLastWriteTime<SelectSide::right>(), symlink.base().getFileTimeTolerance(), symlink.base().getIgnoredTimeShift()))
    {
        case TimeResult::equal:
            //Caveat:
            //1. SYMLINK_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
            //2. harmonize with "bool stillInSync()" in algorithm.cpp

            if (getUnicodeNormalForm(symlink.getItemName<SelectSide::left >()) ==
                getUnicodeNormalForm(symlink.getItemName<SelectSide::right>()))
                symlink.setCategory<FILE_EQUAL>();
            else
                symlink.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(symlink));
            break;

        case TimeResult::leftNewer:
            symlink.setCategory<FILE_LEFT_NEWER>();
            break;

        case TimeResult::rightNewer:
            symlink.setCategory<FILE_RIGHT_NEWER>();
            break;

        case TimeResult::leftInvalid:
            symlink.setCategoryConflict(getConflictInvalidDate<SelectSide::left>(symlink));
            break;

        case TimeResult::rightInvalid:
            symlink.setCategoryConflict(getConflictInvalidDate<SelectSide::right>(symlink));
            break;
    }
}


std::shared_ptr<BaseFolderPair> ComparisonBuffer::compareByTimeSize(const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const
{
    //do basis scan and retrieve files existing on both sides as "compareCandidates"
    std::vector<FilePair*> uncategorizedFiles;
    std::vector<SymlinkPair*> uncategorizedLinks;
    std::shared_ptr<BaseFolderPair> output = performComparison(fp, fpConfig, uncategorizedFiles, uncategorizedLinks);

    //finish symlink categorization
    for (SymlinkPair* symlink : uncategorizedLinks)
        categorizeSymlinkByTime(*symlink);

    //categorize files that exist on both sides
    for (FilePair* file : uncategorizedFiles)
    {
        switch (compareFileTime(file->getLastWriteTime<SelectSide::left>(),
                                file->getLastWriteTime<SelectSide::right>(), fileTimeTolerance_, fpConfig.ignoreTimeShiftMinutes))
        {
            case TimeResult::equal:
                //Caveat:
                //1. FILE_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
                //2. FILE_EQUAL is expected to mean identical file sizes! See InSyncFile
                //3. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::setSyncedTo() in file_hierarchy.h
                if (file->getFileSize<SelectSide::left>() == file->getFileSize<SelectSide::right>())
                {
                    if (getUnicodeNormalForm(file->getItemName<SelectSide::left >()) ==
                        getUnicodeNormalForm(file->getItemName<SelectSide::right>()))
                        file->setCategory<FILE_EQUAL>();
                    else
                        file->setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(*file));
                }
                else
                    file->setCategoryConflict(getConflictSameDateDiffSize(*file)); //same date, different filesize
                break;

            case TimeResult::leftNewer:
                file->setCategory<FILE_LEFT_NEWER>();
                break;

            case TimeResult::rightNewer:
                file->setCategory<FILE_RIGHT_NEWER>();
                break;

            case TimeResult::leftInvalid:
                file->setCategoryConflict(getConflictInvalidDate<SelectSide::left>(*file));
                break;

            case TimeResult::rightInvalid:
                file->setCategoryConflict(getConflictInvalidDate<SelectSide::right>(*file));
                break;
        }
    }
    return output;
}


namespace
{
void categorizeSymlinkByContent(SymlinkPair& symlink, PhaseCallback& callback)
{
    //categorize symlinks that exist on both sides
    callback.updateStatus(replaceCpy(_("Resolving symbolic link %x"), L"%x", fmtPath(AFS::getDisplayPath(symlink.getAbstractPath<SelectSide::left >())))); //throw X
    callback.updateStatus(replaceCpy(_("Resolving symbolic link %x"), L"%x", fmtPath(AFS::getDisplayPath(symlink.getAbstractPath<SelectSide::right>())))); //throw X

    bool equalContent = false;
    const std::wstring errMsg = tryReportingError([&]
    {
        equalContent = AFS::equalSymlinkContent(symlink.getAbstractPath<SelectSide::left >(),
                                                symlink.getAbstractPath<SelectSide::right>()); //throw FileError
    }, callback); //throw X

    if (!errMsg.empty())
        symlink.setCategoryConflict(utfTo<Zstringc>(errMsg));
    else
    {
        if (equalContent)
        {
            //Caveat:
            //1. SYMLINK_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
            //2. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::setSyncedTo() in file_hierarchy.h

            if (getUnicodeNormalForm(symlink.getItemName<SelectSide::left >()) !=
                getUnicodeNormalForm(symlink.getItemName<SelectSide::right>()))
                symlink.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(symlink));
            //else if (!sameFileTime(symlink.getLastWriteTime<SelectSide::left>(),
            //                       symlink.getLastWriteTime<SelectSide::right>(), symlink.base().getFileTimeTolerance(), symlink.base().getIgnoredTimeShift()))
            //    symlink.setCategoryDiffMetadata(getDescrDiffMetaData(symlink));
            else
                symlink.setCategory<FILE_EQUAL>();
        }
        else
            symlink.setCategory<FILE_DIFFERENT_CONTENT>();
    }
}
}


std::shared_ptr<BaseFolderPair> ComparisonBuffer::compareBySize(const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const
{
    //do basis scan and retrieve files existing on both sides as "compareCandidates"
    std::vector<FilePair*> uncategorizedFiles;
    std::vector<SymlinkPair*> uncategorizedLinks;
    std::shared_ptr<BaseFolderPair> output = performComparison(fp, fpConfig, uncategorizedFiles, uncategorizedLinks);

    //finish symlink categorization
    for (SymlinkPair* symlink : uncategorizedLinks)
        categorizeSymlinkByContent(*symlink, cb_); //"compare by size" has the semantics of a quick content-comparison!
    //harmonize with algorithm.cpp, stillInSync()!

    //categorize files that exist on both sides
    for (FilePair* file : uncategorizedFiles)
    {
        //Caveat:
        //1. FILE_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
        //2. FILE_EQUAL is expected to mean identical file sizes! See InSyncFile
        //3. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::setSyncedTo() in file_hierarchy.h
        if (file->getFileSize<SelectSide::left>() == file->getFileSize<SelectSide::right>())
        {
            if (getUnicodeNormalForm(file->getItemName<SelectSide::left >()) ==
                getUnicodeNormalForm(file->getItemName<SelectSide::right>()))
                file->setCategory<FILE_EQUAL>();
            else
                file->setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(*file));
        }
        else
            file->setCategory<FILE_DIFFERENT_CONTENT>();
    }
    return output;
}


namespace parallel
{
//--------------------------------------------------------------
//ATTENTION CALLBACKS: they also run asynchronously *outside* the singleThread lock!
//--------------------------------------------------------------
inline
bool filesHaveSameContent(const AbstractPath& filePath1, const AbstractPath& filePath2, //throw FileError, X
                          const IoCallback& notifyUnbufferedIO /*throw X*/,
                          std::mutex& singleThread)
{ return parallelScope([=] { return filesHaveSameContent(filePath1, filePath2, notifyUnbufferedIO); /*throw FileError, X*/ }, singleThread); }
}


namespace
{
void categorizeFileByContent(FilePair& file, const std::wstring& txtComparingContentOfFiles, AsyncCallback& acb, std::mutex& singleThread) //throw ThreadStopRequest
{
    bool haveSameContent = false;
    const std::wstring errMsg = tryReportingError([&]
    {
        PercentStatReporter statReporter(replaceCpy(txtComparingContentOfFiles, L"%x", fmtPath(file.getRelativePathAny())),
                                         file.getFileSize<SelectSide::left>(), acb); //throw ThreadStopRequest

        //callbacks run *outside* singleThread_ lock! => fine
        auto notifyUnbufferedIO = [&statReporter](int64_t bytesDelta)
        {
            statReporter.updateStatus(0, bytesDelta); //throw ThreadStopRequest
            interruptionPoint(); //throw ThreadStopRequest => not reliably covered by AsyncPercentStatReporter::updateStatus()!
        };

        haveSameContent = parallel::filesHaveSameContent(file.getAbstractPath<SelectSide::left >(),
                                                         file.getAbstractPath<SelectSide::right>(), notifyUnbufferedIO, singleThread); //throw FileError, ThreadStopRequest
        statReporter.updateStatus(1, 0); //throw ThreadStopRequest
    }, acb); //throw ThreadStopRequest

    if (!errMsg.empty())
        file.setCategoryConflict(utfTo<Zstringc>(errMsg));
    else
    {
        if (haveSameContent)
        {
            //Caveat:
            //1. FILE_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
            //2. FILE_EQUAL is expected to mean identical file sizes! See InSyncFile
            //3. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::setSyncedTo() in file_hierarchy.h
            if (getUnicodeNormalForm(file.getItemName<SelectSide::left >()) !=
                getUnicodeNormalForm(file.getItemName<SelectSide::right>()))
                file.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(file));
#if 0 //don't synchronize modtime only see FolderPairSyncer::synchronizeFileInt(), SO_COPY_METADATA_TO_*
            else if (!sameFileTime(file.getLastWriteTime<SelectSide::left>(),
                                   file.getLastWriteTime<SelectSide::right>(), file.base().getFileTimeTolerance(), file.base().getIgnoredTimeShift()))
                file.setCategoryDiffMetadata(getDescrDiffMetaData(file));
#endif
            else
                file.setCategory<FILE_EQUAL>();
        }
        else
            file.setCategory<FILE_DIFFERENT_CONTENT>();
    }
}
}


std::vector<std::shared_ptr<BaseFolderPair>> ComparisonBuffer::compareByContent(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad) const
{
    struct ParallelOps
    {
        size_t current      = 0;
    };
    std::map<AfsDevice, ParallelOps> parallelOpsStatus;

    struct BinaryWorkload
    {
        ParallelOps& parallelOpsL; //
        ParallelOps& parallelOpsR; //consider aliasing!
        RingBuffer<FilePair*> filesToCompareBytewise;
    };
    std::vector<BinaryWorkload> fpWorkload;

    auto addToBinaryWorkload = [&](const AbstractPath& basePathL, const AbstractPath& basePathR, RingBuffer<FilePair*>&& filesToCompareBytewise)
    {
        ParallelOps& posL = parallelOpsStatus[basePathL.afsDevice];
        ParallelOps& posR = parallelOpsStatus[basePathR.afsDevice];
        fpWorkload.push_back({posL, posR, std::move(filesToCompareBytewise)});
    };

    std::vector<std::shared_ptr<BaseFolderPair>> output;

    const Zstringc txtConflictSkippedBinaryComparison = getConflictSkippedBinaryComparison(); //avoid premature pess.: save memory via ref-counted string

    for (const auto& [folderPair, fpCfg] : workLoad)
    {
        std::vector<FilePair*> undefinedFiles;
        std::vector<SymlinkPair*> uncategorizedLinks;
        //run basis scan and retrieve candidates for binary comparison (files existing on both sides)
        output.push_back(performComparison(folderPair, fpCfg, undefinedFiles, uncategorizedLinks));

        RingBuffer<FilePair*> filesToCompareBytewise;
        //content comparison of file content happens AFTER finding corresponding files and AFTER filtering
        //in order to separate into two processes (scanning and comparing)
        for (FilePair* file : undefinedFiles)
            //pre-check: files have different content if they have a different file size (must not be FILE_EQUAL: see InSyncFile)
            if (file->getFileSize<SelectSide::left>() != file->getFileSize<SelectSide::right>())
                file->setCategory<FILE_DIFFERENT_CONTENT>();
            else
            {
                //perf: skip binary comparison for excluded rows (e.g. via time span and size filter)!
                //both soft and hard filter were already applied in ComparisonBuffer::performComparison()!
                if (!file->isActive())
                    file->setCategoryConflict(txtConflictSkippedBinaryComparison);
                else
                    filesToCompareBytewise.push_back(file);
            }
        if (!filesToCompareBytewise.empty())
            addToBinaryWorkload(output.back()->getAbstractPath<SelectSide::left >(),
                                output.back()->getAbstractPath<SelectSide::right>(), std::move(filesToCompareBytewise));

        //finish symlink categorization
        for (SymlinkPair* symlink : uncategorizedLinks)
            categorizeSymlinkByContent(*symlink, cb_);
    }

    //finish categorization: compare files (that have same size) bytewise...
    if (!fpWorkload.empty()) //run ProcessPhase::comparingContent only when needed
    {
        int      itemsTotal = 0;
        uint64_t bytesTotal = 0;
        for (const BinaryWorkload& bwl : fpWorkload)
        {
            itemsTotal += bwl.filesToCompareBytewise.size();

            for (const FilePair* file : bwl.filesToCompareBytewise)
                bytesTotal += file->getFileSize<SelectSide::left>(); //left and right file sizes are equal
        }
        cb_.initNewPhase(itemsTotal, bytesTotal, ProcessPhase::comparingContent); //throw X

        //PERF_START;

        std::mutex singleThread; //only a single worker thread may run at a time, except for parallel file I/O

        AsyncCallback acb;                       //
        std::function<void()> scheduleMoreTasks; //manage life time: enclose ThreadGroup!

        ThreadGroup<std::function<void()>> tg(std::numeric_limits<size_t>::max(), Zstr("Binary Comparison"));

        scheduleMoreTasks = [&, txtComparingContentOfFiles = _("Comparing content of files %x")]
        {
            bool wereDone = true;

            for (size_t j = 0; j < fpWorkload.size(); ++j)
            {
                BinaryWorkload& bwl = fpWorkload[j];
                ParallelOps& posL = bwl.parallelOpsL;
                ParallelOps& posR = bwl.parallelOpsR;
                const size_t newTaskCount = std::min<size_t>({1                 - posL.current, 1                 - posR.current, bwl.filesToCompareBytewise.size()});
                if (&posL != &posR)
                    posL.current += newTaskCount; //
                posR.current += newTaskCount;     //consider aliasing!

                for (size_t i = 0; i < newTaskCount; ++i)
                {
                    tg.run([&, statusPrio = j, &file = *bwl.filesToCompareBytewise.front()]
                    {
                        acb.notifyTaskBegin(statusPrio); //prioritize status messages according to natural order of folder pairs
                        ZEN_ON_SCOPE_EXIT(acb.notifyTaskEnd());

                        std::lock_guard dummy(singleThread); //protect ALL variable accesses unless explicitly not needed ("parallel" scope)!
                        //---------------------------------------------------------------------------------------------------
                        ZEN_ON_SCOPE_SUCCESS(if (&posL != &posR) --posL.current;
                                             /**/                --posR.current;
                                             scheduleMoreTasks());

                        categorizeFileByContent(file, txtComparingContentOfFiles, acb, singleThread); //throw ThreadStopRequest
                    });

                    bwl.filesToCompareBytewise.pop_front();
                }
                if (posL.current != 0 || posR.current != 0 || !bwl.filesToCompareBytewise.empty())
                    wereDone = false;
            }
            if (wereDone)
                acb.notifyAllDone();
        };

        {
            std::lock_guard dummy(singleThread); //[!] potential race with worker threads!
            scheduleMoreTasks(); //set initial load
        }

        acb.waitUntilDone(UI_UPDATE_INTERVAL / 2 /*every ~50 ms*/, cb_); //throw X
    }

    return output;
}

//-----------------------------------------------------------------------------------------------

class MergeSides
{
public:
    MergeSides(const std::unordered_map<ZstringNoCase, Zstringc>& errorsByRelPath,
               std::vector<FilePair*>& undefinedFilesOut,
               std::vector<SymlinkPair*>& undefinedSymlinksOut) :
        errorsByRelPath_(errorsByRelPath),
        undefinedFiles_(undefinedFilesOut),
        undefinedSymlinks_(undefinedSymlinksOut) {}

    void execute(const FolderContainer& lhs, const FolderContainer& rhs, ContainerObject& output)
    {
        auto it = errorsByRelPath_.find(Zstring()); //empty path if read-error for whole base directory

        mergeTwoSides(lhs, rhs,
                      it != errorsByRelPath_.end() ? &it->second : nullptr,
                      output);
    }

private:
    void mergeTwoSides(const FolderContainer& lhs, const FolderContainer& rhs, const Zstringc* errorMsg, ContainerObject& output);

    template <SelectSide side>
    void fillOneSide(const FolderContainer& folderCont, const Zstringc* errorMsg, ContainerObject& output);

    const Zstringc* checkFailedRead(FileSystemObject& fsObj, const Zstringc* errorMsg);

    const std::unordered_map<ZstringNoCase, Zstringc>& errorsByRelPath_; //base-relative paths or empty if read-error for whole base directory
    std::vector<FilePair*>&    undefinedFiles_;
    std::vector<SymlinkPair*>& undefinedSymlinks_;
};


inline
const Zstringc* MergeSides::checkFailedRead(FileSystemObject& fsObj, const Zstringc* errorMsg)
{
    if (!errorMsg)
        if (const auto it = errorsByRelPath_.find(fsObj.getRelativePathAny());
            it != errorsByRelPath_.end())
            errorMsg = &it->second;

    if (errorMsg)
    {
        fsObj.setActive(false);
        fsObj.setCategoryConflict(*errorMsg); //peak memory: Zstringc is ref-counted, unlike std::string!
        static_assert(std::is_same_v<const Zstringc&, decltype(*errorMsg)>);
    }
    return errorMsg;
}


template <class MapType, class Function>
void forEachSorted(const MapType& fileMap, Function fun)
{
    using FileRef = const typename MapType::value_type*;

    std::vector<FileRef> fileList;
    fileList.reserve(fileMap.size());

    for (const auto& item : fileMap)
        fileList.push_back(&item);

   //sort for natural default sequence on UI file grid:
    std::sort(fileList.begin(), fileList.end(), [](const FileRef& lhs, const FileRef& rhs) { return compareNoCase(lhs->first /*item name*/, rhs->first) < 0; });

    for (const auto& item : fileList)
        fun(item->first, item->second);
}


template <SelectSide side>
void MergeSides::fillOneSide(const FolderContainer& folderCont, const Zstringc* errorMsg, ContainerObject& output)
{
    forEachSorted(folderCont.files, [&](const Zstring& fileName, const FileAttributes& attrib)
    {
        FilePair& newItem = output.addSubFile<side>(fileName, attrib);
        checkFailedRead(newItem, errorMsg);
    });

    forEachSorted(folderCont.symlinks, [&](const Zstring& linkName, const LinkAttributes& attrib)
    {
        SymlinkPair& newItem = output.addSubLink<side>(linkName, attrib);
        checkFailedRead(newItem, errorMsg);
    });

    forEachSorted(folderCont.folders, [&](const Zstring& folderName, const std::pair<FolderAttributes, FolderContainer>& attrib)
    {
        FolderPair& newFolder = output.addSubFolder<side>(folderName, attrib.first);
        const Zstringc* errorMsgNew = checkFailedRead(newFolder, errorMsg);

        fillOneSide<side>(attrib.second, errorMsgNew, newFolder); //recurse
    });
}


template <class MapType, class ProcessLeftOnly, class ProcessRightOnly, class ProcessBoth> inline
void matchFolders(const MapType& mapLeft, const MapType& mapRight, ProcessLeftOnly lo, ProcessRightOnly ro, ProcessBoth bo)
{
    struct FileRef
    {
        //perf: buffer ZstringNoCase instead of compareNoCase()/equalNoCase()? => makes no (significant) difference!
        const typename MapType::value_type* ref;
        bool leftSide;
    };
    std::vector<FileRef> fileList;
    fileList.reserve(mapLeft.size() + mapRight.size()); //perf: ~5% shorter runtime

    for (const auto& item : mapLeft ) fileList.push_back({&item, true });
    for (const auto& item : mapRight) fileList.push_back({&item, false});

    //primary sort: ignore Unicode normal form and upper/lower case
    //bonus: natural default sequence on UI file grid
    std::sort(fileList.begin(), fileList.end(), [](const FileRef& lhs, const FileRef& rhs) { return compareNoCase(lhs.ref->first /*item name*/, rhs.ref->first) < 0; });

    using ItType = typename std::vector<FileRef>::iterator;
    auto tryMatchRange = [&](ItType it, ItType itLast) //auto parameters? compiler error on VS 17.2...
    {
        const size_t equalCountL = std::count_if(it, itLast, [](const FileRef& fr) { return fr.leftSide; });
        const size_t equalCountR = itLast - it - equalCountL;

        if (equalCountL == 1 && equalCountR == 1) //we have a match
        {
            if (it->leftSide)
                bo(*it[0].ref, *it[1].ref);
            else
                bo(*it[1].ref, *it[0].ref);
        }
        else if (equalCountL == 1 && equalCountR == 0)
            lo(*it->ref, nullptr);
        else if (equalCountL == 0 && equalCountR == 1)
            ro(*it->ref, nullptr);
        else //ambiguous (yes, even if one side only, e.g. different Unicode normalization forms)
            return false;
        return true;
    };

    for (auto it = fileList.begin(); it != fileList.end();)
    {
        //find equal range: ignore case, ignore Unicode normalization
        auto itEndEq = std::find_if(it + 1, fileList.end(), [&](const FileRef& fr) { return !equalNoCase(fr.ref->first, it->ref->first); });
        if (!tryMatchRange(it, itEndEq))
        {
            //secondary sort: respect case, ignore unicode normal forms
            std::sort(it, itEndEq, [](const FileRef& lhs, const FileRef& rhs) { return getUnicodeNormalForm(lhs.ref->first) < getUnicodeNormalForm(rhs.ref->first); });

            for (auto itCase = it; itCase != itEndEq;)
            {
                //find equal range: respect case, ignore Unicode normalization
                auto itEndCase = std::find_if(itCase + 1, itEndEq, [&](const FileRef& fr) { return getUnicodeNormalForm(fr.ref->first) != getUnicodeNormalForm(itCase->ref->first); });
                if (!tryMatchRange(itCase, itEndCase))
                {
                    const Zstringc& conflictMsg = getConflictAmbiguousItemName(itCase->ref->first);
                    std::for_each(itCase, itEndCase, [&](const FileRef& fr)
                    {
                        if (fr.leftSide)
                            lo(*fr.ref, &conflictMsg);
                        else
                            ro(*fr.ref, &conflictMsg);
                    });
                }
                itCase = itEndCase;
            }
        }
        it = itEndEq;
    }
}


void MergeSides::mergeTwoSides(const FolderContainer& lhs, const FolderContainer& rhs, const Zstringc* errorMsg, ContainerObject& output)
{
    using FileData = FolderContainer::FileList::value_type;

    matchFolders(lhs.files, rhs.files, [&](const FileData& fileLeft, const Zstringc* conflictMsg)
    {
        FilePair& newItem = output.addSubFile<SelectSide::left >(fileLeft .first, fileLeft .second);
        checkFailedRead(newItem, conflictMsg ? conflictMsg : errorMsg);
    },
    [&](const FileData& fileRight, const Zstringc* conflictMsg)
    {
        FilePair& newItem = output.addSubFile<SelectSide::right>(fileRight.first, fileRight.second);
        checkFailedRead(newItem, conflictMsg ? conflictMsg : errorMsg);
    },
    [&](const FileData& fileLeft, const FileData& fileRight)
    {
        FilePair& newItem = output.addSubFile(fileLeft.first,
                                              fileLeft.second,
                                              FILE_CONFLICT, //dummy-value until categorization is finished later
                                              fileRight.first,
                                              fileRight.second);
        if (!checkFailedRead(newItem, errorMsg))
            undefinedFiles_.push_back(&newItem);
        static_assert(std::is_same_v<ContainerObject::FileList, std::list<FilePair>>); //ContainerObject::addSubFile() must NOT invalidate references used in "undefinedFiles"!
    });

    //-----------------------------------------------------------------------------------------------
    using SymlinkData = FolderContainer::SymlinkList::value_type;

    matchFolders(lhs.symlinks, rhs.symlinks, [&](const SymlinkData& symlinkLeft, const Zstringc* conflictMsg)
    {
        SymlinkPair& newItem = output.addSubLink<SelectSide::left >(symlinkLeft .first, symlinkLeft .second);
        checkFailedRead(newItem, conflictMsg ? conflictMsg : errorMsg);
    },
    [&](const SymlinkData& symlinkRight, const Zstringc* conflictMsg)
    {
        SymlinkPair& newItem = output.addSubLink<SelectSide::right>(symlinkRight.first, symlinkRight.second);
        checkFailedRead(newItem, conflictMsg ? conflictMsg : errorMsg);
    },
    [&](const SymlinkData& symlinkLeft, const SymlinkData& symlinkRight) //both sides
    {
        SymlinkPair& newItem = output.addSubLink(symlinkLeft.first,
                                                 symlinkLeft.second,
                                                 SYMLINK_CONFLICT, //dummy-value until categorization is finished later
                                                 symlinkRight.first,
                                                 symlinkRight.second);
        if (!checkFailedRead(newItem, errorMsg))
            undefinedSymlinks_.push_back(&newItem);
    });

    //-----------------------------------------------------------------------------------------------
    using FolderData = FolderContainer::FolderList::value_type;

    matchFolders(lhs.folders, rhs.folders, [&](const FolderData& dirLeft, const Zstringc* conflictMsg)
    {
        FolderPair& newFolder = output.addSubFolder<SelectSide::left>(dirLeft.first, dirLeft.second.first);
        const Zstringc* errorMsgNew = checkFailedRead(newFolder, conflictMsg ? conflictMsg : errorMsg);
        this->fillOneSide<SelectSide::left>(dirLeft.second.second, errorMsgNew, newFolder); //recurse
    },
    [&](const FolderData& dirRight, const Zstringc* conflictMsg)
    {
        FolderPair& newFolder = output.addSubFolder<SelectSide::right>(dirRight.first, dirRight.second.first);
        const Zstringc* errorMsgNew = checkFailedRead(newFolder, conflictMsg ? conflictMsg : errorMsg);
        this->fillOneSide<SelectSide::right>(dirRight.second.second, errorMsgNew, newFolder); //recurse
    },
    [&](const FolderData& dirLeft, const FolderData& dirRight)
    {
        FolderPair& newFolder = output.addSubFolder(dirLeft.first, dirLeft.second.first, DIR_EQUAL, dirRight.first, dirRight.second.first);
        const Zstringc* errorMsgNew = checkFailedRead(newFolder, errorMsg);

        if (!errorMsgNew)
            if (getUnicodeNormalForm(dirLeft.first) !=
                getUnicodeNormalForm(dirRight.first))
                newFolder.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(newFolder));

        mergeTwoSides(dirLeft.second.second, dirRight.second.second, errorMsgNew, newFolder); //recurse
    });
}

//-----------------------------------------------------------------------------------------------

//uncheck excluded directories (see parallelDeviceTraversal()) + remove superfluous excluded subdirectories
void stripExcludedDirectories(ContainerObject& hierObj, const PathFilter& filterProc)
{
    for (FolderPair& folder : hierObj.refSubFolders())
        stripExcludedDirectories(folder, filterProc);

    //remove superfluous directories:
    //   this does not invalidate "std::vector<FilePair*>& undefinedFiles", since we delete folders only
    //   and there is no side-effect for memory positions of FilePair and SymlinkPair thanks to std::list!
    static_assert(std::is_same_v<std::list<FolderPair>, ContainerObject::FolderList>);

    hierObj.refSubFolders().remove_if([&](FolderPair& folder)
    {
        const bool included = filterProc.passDirFilter(folder.getRelativePathAny(), nullptr); //childItemMightMatch is false, child items were already excluded during scanning

        if (!included) //falsify only! (e.g. might already be inactive due to read error!)
            folder.setActive(false);

        return !included && //don't check active status, but eval filter directly!
               folder.refSubFolders().empty() &&
               folder.refSubLinks  ().empty() &&
               folder.refSubFiles  ().empty();
    });
}


//create comparison result table and fill category except for files existing on both sides: undefinedFiles and undefinedSymlinks are appended!
std::shared_ptr<BaseFolderPair> ComparisonBuffer::performComparison(const ResolvedFolderPair& fp,
                                                                    const FolderPairCfg& fpCfg,
                                                                    std::vector<FilePair*>& undefinedFiles,
                                                                    std::vector<SymlinkPair*>& undefinedSymlinks) const
{
    cb_.updateStatus(_("Generating file list...")); //throw X
    cb_.requestUiUpdate(true /*force*/); //throw X

    std::unordered_map<ZstringNoCase, Zstringc> failedReads; //base-relative paths or empty if read-error for whole base directory

    auto evalFolderContent = [&](const AbstractPath& folderPath) -> const FolderContainer&
    {
        const DirectoryValue& dirVal = folderBuffer_.find({folderPath, fpCfg.filter.nameFilter, fpCfg.handleSymlinks})->second;
        //contract: folderBuffer_ has entries for *all* folders (existing or not)

        //mix failedFolderReads with failedItemReads:
        //associate folder traversing errors with folder (instead of child items only) to show on GUI! See "MergeSides"
        //=> minor pessimization for "excludefilterFailedRead" which needlessly excludes parent folders, too
        auto append = [&](const std::unordered_map<Zstring, Zstringc>& c)
        {
            for (const auto& [relPath, errorMsg] : c)
                failedReads.emplace(relPath, errorMsg);
        };
        append(dirVal.failedFolderReads);
        append(dirVal.failedItemReads);

        return dirVal.folderCont;
    };

    const FolderContainer& folderContL = evalFolderContent(fp.folderPathLeft);
    const FolderContainer& folderContR = evalFolderContent(fp.folderPathRight);

    //*after* evalFolderContent():
    Zstring excludefilterFailedRead;
    if (failedReads.contains(Zstring())) //empty path if read-error for whole base directory
        excludefilterFailedRead += Zstr("*\n");
    else
        for (const auto& [relPath, errorMsg] : failedReads)
            excludefilterFailedRead += relPath.upperCase + Zstr('\n'); //exclude item AND (potential) child items!

    //somewhat obscure, but it's possible on Linux file systems to have a backslash as part of a file name
    //=> avoid misinterpretation when parsing the filter phrase in PathFilter (see path_filter.cpp::parseFilterPhrase())
    if constexpr (FILE_NAME_SEPARATOR != Zstr('/' )) replace(excludefilterFailedRead, Zstr('/'),  Zstr('?'));
    if constexpr (FILE_NAME_SEPARATOR != Zstr('\\')) replace(excludefilterFailedRead, Zstr('\\'), Zstr('?'));


    auto getBaseFolderStatus = [&](const AbstractPath& folderPath)
    {
        if (folderStatus_.existing.contains(folderPath))
            return BaseFolderStatus::existing;
        if (folderStatus_.notExisting.contains(folderPath))
            return BaseFolderStatus::notExisting;
        if (folderStatus_.failedChecks.contains(folderPath))
            return BaseFolderStatus::failure;
        assert(AFS::isNullPath(folderPath));
        return BaseFolderStatus::notExisting;
    };

    std::shared_ptr<BaseFolderPair> output = std::make_shared<BaseFolderPair>(fp.folderPathLeft,
                                                                              getBaseFolderStatus(fp.folderPathLeft), //dir existence must be checked only once!
                                                                              fp.folderPathRight,
                                                                              getBaseFolderStatus(fp.folderPathRight),
                                                                              fpCfg.filter.nameFilter.ref().copyFilterAddingExclusion(excludefilterFailedRead),
                                                                              fpCfg.compareVar,
                                                                              fileTimeTolerance_,
                                                                              fpCfg.ignoreTimeShiftMinutes);
    //PERF_START;
    MergeSides(failedReads, undefinedFiles, undefinedSymlinks).execute(folderContL, folderContR, *output);
    //PERF_STOP;

    //##################### in/exclude rows according to filtering #####################
    //NOTE: we need to finish de-activating rows BEFORE binary comparison is run so that it can skip them!

    //attention: some excluded directories are still in the comparison result! (see include filter handling!)
    if (!fpCfg.filter.nameFilter.ref().isNull())
        stripExcludedDirectories(*output, fpCfg.filter.nameFilter.ref()); //mark excluded directories (see parallelDeviceTraversal()) + remove superfluous excluded subdirectories

    //apply soft filtering (hard filter already applied during traversal!)
    addSoftFiltering(*output, fpCfg.filter.timeSizeFilter);

    //##################################################################################
    return output;
}
}


FolderComparison fff::compare(WarningDialogs& warnings,
                              int fileTimeTolerance,
                              bool allowUserInteraction,
                              bool runWithBackgroundPriority,
                              bool createDirLocks,
                              std::unique_ptr<LockHolder>& dirLocks,
                              const std::vector<FolderPairCfg>& fpCfgList,
                              ProcessCallback& callback)
{
    //PERF_START;

    //indicator at the very beginning of the log to make sense of "total time"
    //init process: keep at beginning so that all gui elements are initialized properly
    callback.initNewPhase(-1, -1, ProcessPhase::scanning); //throw X; it's unknown how many files will be scanned => -1 objects
    //callback.logInfo(Comparison started")); -> still useful?

    //-------------------------------------------------------------------------------

    //specify process and resource handling priorities
    std::unique_ptr<ScheduleForBackgroundProcessing> backgroundPrio;
    if (runWithBackgroundPriority)
        tryReportingError([&]
    {
        backgroundPrio = std::make_unique<ScheduleForBackgroundProcessing>(); //throw FileError
    }, callback); //throw X

    //prevent operating system going into sleep state
    std::unique_ptr<PreventStandby> noStandby;
    try
    {
        noStandby = std::make_unique<PreventStandby>(); //throw FileError
    }
    catch (const FileError& e) //failure is not critical => log only
    {
        callback.logInfo(e.toString()); //throw X
    }

    const ResolvedBaseFolders& resInfo = initializeBaseFolders(fpCfgList,
                                                               allowUserInteraction, warnings, callback); //throw X
    //directory existence only checked *once* to avoid race conditions!
    if (resInfo.resolvedPairs.size() != fpCfgList.size())
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));

    std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>> workLoad;
    for (size_t i = 0; i < fpCfgList.size(); ++i)
        workLoad.emplace_back(resInfo.resolvedPairs[i], fpCfgList[i]);

    //-----------execute basic checks all at once before starting comparison----------

    //check for incomplete input
    {
        bool havePartialPair = false;
        bool haveFullPair    = false;

        for (const ResolvedFolderPair& fp : resInfo.resolvedPairs)
            if (AFS::isNullPath(fp.folderPathLeft) != AFS::isNullPath(fp.folderPathRight))
                havePartialPair = true;
            else if (!AFS::isNullPath(fp.folderPathLeft))
                haveFullPair = true;

        if (havePartialPair == haveFullPair) //error if: all empty or exist both full and partial pairs -> support single-folder comparison scenario
            callback.reportWarning(_("A folder input field is empty.") + L" \n\n" + //throw X
                                   _("The corresponding folder will be considered as empty."), warnings.warnInputFieldEmpty);
    }

    //Check whether one side is a sub directory of the other side (folder-pair-wise!)
    //The similar check (warnDependentBaseFolders) if one directory is read/written by multiple pairs not before beginning of synchronization
    {
        std::wstring msg;
        bool shouldExclude = false;

        for (const auto& [folderPair, fpCfg] : workLoad)
            if (std::optional<PathDependency> pd = getPathDependency(folderPair.folderPathLeft,  fpCfg.filter.nameFilter.ref(),
                                                                     folderPair.folderPathRight, fpCfg.filter.nameFilter.ref()))
            {
                msg += L"\n\n" +
                       AFS::getDisplayPath(folderPair.folderPathLeft) + L" <-> " + L'\n' +
                       AFS::getDisplayPath(folderPair.folderPathRight);
                if (!pd->relPath.empty())
                {
                    shouldExclude = true;
                    msg += std::wstring() + L'\n' + L"⇒ " +
                           _("Exclude:") + L' ' + utfTo<std::wstring>(FILE_NAME_SEPARATOR + pd->relPath + FILE_NAME_SEPARATOR);
                }
            }

        if (!msg.empty())
            callback.reportWarning(_("One base folder of a folder pair is contained in the other one.") +
                                   (shouldExclude ? L'\n' + _("The folder should be excluded from synchronization via filter.") : L"") +
                                   msg, warnings.warnDependentFolderPair); //throw X
    }
    //-------------------end of basic checks------------------------------------------

    //lock (existing) directories before comparison
    if (createDirLocks)
    {
        std::set<Zstring> folderPathsToLock;
        for (const AbstractPath& folderPath : resInfo.baseFolderStatus.existing)

            if (const Zstring& nativePath = getNativeItemPath(folderPath); //restrict directory locking to native paths until further
                !nativePath.empty())
                folderPathsToLock.insert(nativePath);

        dirLocks = std::make_unique<LockHolder>(folderPathsToLock, warnings.warnDirectoryLockFailed, callback);
    }

    try
    {
        FolderComparison output;
        //reduce peak memory by restricting lifetime of ComparisonBuffer to have ended when loading potentially huge InSyncFolder instance in redetermineSyncDirection()
        {
            //------------------- fill directory buffer: traverse/read folders --------------------------
            std::set<DirectoryKey> folderKeys;
            for (const auto& [folderPair, fpCfg] : workLoad)
            {
                folderKeys.emplace(DirectoryKey({folderPair.folderPathLeft,  fpCfg.filter.nameFilter, fpCfg.handleSymlinks}));
                folderKeys.emplace(DirectoryKey({folderPair.folderPathRight, fpCfg.filter.nameFilter, fpCfg.handleSymlinks}));
            }

            //PERF_START;
            ComparisonBuffer cmpBuff(folderKeys,
                                     resInfo.baseFolderStatus,
                                     fileTimeTolerance, callback);
            //PERF_STOP;

            //process binary comparison as one junk
            std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>> workLoadByContent;
            for (const auto& [folderPair, fpCfg] : workLoad)
                if (fpCfg.compareVar == CompareVariant::content)
                    workLoadByContent.push_back({folderPair, fpCfg});

            std::vector<std::shared_ptr<BaseFolderPair>> outputByContent = cmpBuff.compareByContent(workLoadByContent);
            auto itOByC = outputByContent.begin();

            //write output in expected order
            for (const auto& [folderPair, fpCfg] : workLoad)
                switch (fpCfg.compareVar)
                {
                    case CompareVariant::timeSize:
                        output.push_back(cmpBuff.compareByTimeSize(folderPair, fpCfg));
                        break;
                    case CompareVariant::size:
                        output.push_back(cmpBuff.compareBySize(folderPair, fpCfg));
                        break;
                    case CompareVariant::content:
                        assert(itOByC != outputByContent.end());
                        if (itOByC != outputByContent.end())
                            output.push_back(*itOByC++);
                        break;
                }
        }
        assert(output.size() == fpCfgList.size());

        //--------- set initial sync-direction --------------------------------------------------
        std::vector<std::pair<BaseFolderPair*, SyncDirectionConfig>> directCfgs;
        for (auto it = output.begin(); it != output.end(); ++it)
            directCfgs.emplace_back(&** it, fpCfgList[it - output.begin()].directionCfg);

        redetermineSyncDirection(directCfgs,
                                 callback); //throw X

        return output;
    }
    catch (const std::bad_alloc& e)
    {
        callback.reportFatalError(_("Out of memory.") + L' ' + utfTo<std::wstring>(e.what()));
        return {};
    }
}

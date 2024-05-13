// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "comparison.h"
#include <zen/process_priority.h>
//#include <zen/perf.h>
#include <zen/time.h>
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
                                          const AFS::RequestPasswordFun& requestPassword /*throw X*/,
                                          WarningDialogs& warnings,
                                          PhaseCallback& callback /*throw X*/) //throw X
{
    std::vector<Zstring> pathPhrases;
    for (const FolderPairCfg& fpCfg : fpCfgList)
    {
        pathPhrases.push_back(fpCfg.folderPathPhraseLeft_);
        pathPhrases.push_back(fpCfg.folderPathPhraseRight_);
    }

    ResolvedBaseFolders output;
    std::set<AbstractPath> allFolders;

    tryReportingError([&]
    {
        //createAbstractPath() -> tryExpandVolumeName() hangs for idle HDD! => run async to make it cancellable
        auto protCurrentPhrase = makeSharedRef<Protected<Zstring>>();

        std::future<std::vector<AbstractPath>> futFolderPaths = runAsync([pathPhrases, currentPhraseWeak = std::weak_ptr(protCurrentPhrase.ptr())]
        {
            setCurrentThreadName(Zstr("Normalizing folder paths"));

            std::vector<AbstractPath> folderPaths;
            for (const Zstring& pathPhrase : pathPhrases)
            {
                if (auto protCurrentPhrase2 = currentPhraseWeak.lock()) //[!] not owned by worker thread!
                    protCurrentPhrase2->access([&](Zstring& currentPathPhrase) { currentPathPhrase = pathPhrase; });
                else
                    throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Caller context gone!");

                folderPaths.push_back(createAbstractPath(pathPhrase));
            }
            return folderPaths;
        });

        while (futFolderPaths.wait_for(UI_UPDATE_INTERVAL / 2) == std::future_status::timeout)
        {
            const Zstring pathPhrase = protCurrentPhrase.ref().access([](const Zstring& currentPathPhrase) { return currentPathPhrase; });
            callback.updateStatus(_("Normalizing folder paths...") + L' ' + utfTo<std::wstring>(pathPhrase)); //throw X
        }

        const std::vector<AbstractPath>& folderPaths = futFolderPaths.get(); //throw (std::runtime_error)

        //support "retry" for environment variable and variable driver letter resolution!
        allFolders.clear();
        allFolders.insert(folderPaths.begin(), folderPaths.end());

        output.resolvedPairs.clear();
        for (size_t i = 0; i < folderPaths.size(); i += 2)
            output.resolvedPairs.push_back({folderPaths[i], folderPaths[i + 1]});
        //---------------------------------------------------------------------------

        output.baseFolderStatus = getFolderStatusParallel(allFolders,
                                                          true /*authenticateAccess*/, requestPassword, callback); //throw X
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

    for (const AbstractPath& folderPath : allFolders)
        ciPathAliases[std::pair(folderPath.afsDevice, folderPath.afsPath.value)].insert(folderPath);

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
    ComparisonBuffer(const FolderStatus& folderStatus,
                     unsigned int fileTimeTolerance,
                     ProcessCallback& callback) :
        fileTimeTolerance_(fileTimeTolerance),
        folderStatus_(folderStatus),
        cb_(callback) {}

    FolderComparison execute(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad);

private:
    ComparisonBuffer           (const ComparisonBuffer&) = delete;
    ComparisonBuffer& operator=(const ComparisonBuffer&) = delete;

    //create comparison result table and fill category except for files existing on both sides: undefinedFiles and undefinedSymlinks are appended!
    SharedRef<BaseFolderPair> compareByTimeSize(const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const;
    SharedRef<BaseFolderPair> compareBySize    (const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const;
    std::vector<SharedRef<BaseFolderPair>> compareByContent(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad) const;

    SharedRef<BaseFolderPair> performComparison(const ResolvedFolderPair& fp,
                                                const FolderPairCfg& fpCfg,
                                                std::vector<FilePair*>& undefinedFiles,
                                                std::vector<SymlinkPair*>& undefinedSymlinks) const;

    BaseFolderStatus getBaseFolderStatus(const AbstractPath& folderPath) const
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

    const unsigned int fileTimeTolerance_;
    const FolderStatus& folderStatus_;
    std::map<DirectoryKey, DirectoryValue> folderBuffer_; //contains entries for *all* scanned folders!
    ProcessCallback& cb_;
};


FolderComparison ComparisonBuffer::execute(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad)
{
    std::set<DirectoryKey> foldersToRead;
    for (const auto& [folderPair, fpCfg] : workLoad)
        if (getBaseFolderStatus(folderPair.folderPathLeft ) != BaseFolderStatus::failure && //no need to list or display one-sided results if
            getBaseFolderStatus(folderPair.folderPathRight) != BaseFolderStatus::failure)   //*either* folder existence check fails
        {
            //+ only traverse *existing* folders
            if (getBaseFolderStatus(folderPair.folderPathLeft) == BaseFolderStatus::existing)
                foldersToRead.emplace(DirectoryKey{folderPair.folderPathLeft,  fpCfg.filter.nameFilter, fpCfg.handleSymlinks});
            if (getBaseFolderStatus(folderPair.folderPathRight) == BaseFolderStatus::existing)
                foldersToRead.emplace(DirectoryKey{folderPair.folderPathRight, fpCfg.filter.nameFilter, fpCfg.handleSymlinks});
        }

    //------------------------------------------------------------------
    const std::chrono::steady_clock::time_point compareStartTime = std::chrono::steady_clock::now();
    int itemsReported = 0;

    auto onStatusUpdate = [&, textScanning = _("Scanning:") + L' '](const std::wstring& statusLine, int itemsTotal)
    {
        cb_.updateDataProcessed(itemsTotal - itemsReported, 0); //noexcept
        itemsReported = itemsTotal;

        cb_.updateStatus(textScanning + statusLine); //throw X
    };

    //PERF_START;
    folderBuffer_ = parallelDeviceTraversal(foldersToRead,
    [&](const PhaseCallback::ErrorInfo& errorInfo) { return cb_.reportError(errorInfo); }, //throw X
    onStatusUpdate, //throw X
    UI_UPDATE_INTERVAL / 2); //every ~50 ms
    //PERF_STOP;

    const int64_t totalTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - compareStartTime).count();
    cb_.logMessage(_("Comparison finished:") + L' ' +
                   _P("1 item found", "%x items found", itemsReported) + SPACED_DASH +
                   _("Time elapsed:") + L' ' + utfTo<std::wstring>(formatTimeSpan(totalTimeSec)),
                   PhaseCallback::MsgType::info); //throw X
    //------------------------------------------------------------------

    //process binary comparison as one junk
    std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>> workLoadByContent;
    for (const auto& [folderPair, fpCfg] : workLoad)
        if (fpCfg.compareVar == CompareVariant::content)
            workLoadByContent.push_back({folderPair, fpCfg});

    std::vector<SharedRef<BaseFolderPair>> outputByContent = compareByContent(workLoadByContent);
    auto itOByC = outputByContent.begin();

    FolderComparison output;

    //write output in expected order
    for (const auto& [folderPair, fpCfg] : workLoad)
        switch (fpCfg.compareVar)
        {
            case CompareVariant::timeSize:
                output.push_back(compareByTimeSize(folderPair, fpCfg));
                break;
            case CompareVariant::size:
                output.push_back(compareBySize(folderPair, fpCfg));
                break;
            case CompareVariant::content:
                assert(itOByC != outputByContent.end());
                if (itOByC != outputByContent.end())
                    output.push_back(*itOByC++);
                break;
        }
    return output;
}


//--------------------assemble conflict descriptions---------------------------

//const wchar_t arrowLeft [] = L"\u2190"; unicode arrows -> too small
//const wchar_t arrowRight[] = L"\u2192";
const wchar_t arrowLeft [] = L"<-";
const wchar_t arrowRight[] = L"->";

//NOTE: conflict texts are NOT expected to contain additional path info (already implicit through associated item!)
//      => only add path info if information is relevant, e.g. conflict is specific to left/right side only

template <SelectSide side, class FileOrLinkPair> inline
Zstringc getConflictInvalidDate(const FileOrLinkPair& file)
{
    return utfTo<Zstringc>(replaceCpy(_("File %x has an invalid date."), L"%x", fmtPath(AFS::getDisplayPath(file.template getAbstractPath<side>()))) + L'\n' +
                           _("Date:") + L' ' + formatUtcToLocalTime(file.template getLastWriteTime<side>()));
}


Zstringc getConflictSameDateDiffSize(const FilePair& file)
{
    return utfTo<Zstringc>(_("Files have the same date but a different size.") + L'\n' +
                           _("Date:") + L' ' + formatUtcToLocalTime(file.getLastWriteTime<SelectSide::left >()) + TAB_SPACE + _("Size:") + L' ' + formatNumber(file.getFileSize<SelectSide::left >()) + L' ' + arrowLeft + L'\n' +
                           _("Date:") + L' ' + formatUtcToLocalTime(file.getLastWriteTime<SelectSide::right>()) + TAB_SPACE + _("Size:") + L' ' + formatNumber(file.getFileSize<SelectSide::right>()) + L' ' + arrowRight);
}


Zstringc getConflictSkippedBinaryComparison()
{
    return utfTo<Zstringc>(_("Content comparison was skipped for excluded files."));
}


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
            symlink.setContentCategory(FileContentCategory::equal);
            break;

        case TimeResult::leftNewer:
            symlink.setContentCategory(FileContentCategory::leftNewer);
            break;

        case TimeResult::rightNewer:
            symlink.setContentCategory(FileContentCategory::rightNewer);
            break;

        case TimeResult::leftInvalid:
            symlink.setCategoryInvalidTime(getConflictInvalidDate<SelectSide::left>(symlink));
            break;

        case TimeResult::rightInvalid:
            symlink.setCategoryInvalidTime(getConflictInvalidDate<SelectSide::right>(symlink));
            break;
    }
}


SharedRef<BaseFolderPair> ComparisonBuffer::compareByTimeSize(const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const
{
    //do basis scan and retrieve files existing on both sides as "compareCandidates"
    std::vector<FilePair*> uncategorizedFiles;
    std::vector<SymlinkPair*> uncategorizedLinks;
    SharedRef<BaseFolderPair> output = performComparison(fp, fpConfig, uncategorizedFiles, uncategorizedLinks);

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
                if (file->getFileSize<SelectSide::left>() == file->getFileSize<SelectSide::right>())
                    file->setContentCategory(FileContentCategory::equal);
                else
                    file->setCategoryInvalidTime(getConflictSameDateDiffSize(*file));
                break;

            case TimeResult::leftNewer:
                file->setContentCategory(FileContentCategory::leftNewer);
                break;

            case TimeResult::rightNewer:
                file->setContentCategory(FileContentCategory::rightNewer);
                break;

            case TimeResult::leftInvalid:
                file->setCategoryInvalidTime(getConflictInvalidDate<SelectSide::left>(*file));
                break;

            case TimeResult::rightInvalid:
                file->setCategoryInvalidTime(getConflictInvalidDate<SelectSide::right>(*file));
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
        symlink.setContentCategory(equalContent ? FileContentCategory::equal : FileContentCategory::different);
}
}


SharedRef<BaseFolderPair> ComparisonBuffer::compareBySize(const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const
{
    //do basis scan and retrieve files existing on both sides as "compareCandidates"
    std::vector<FilePair*> uncategorizedFiles;
    std::vector<SymlinkPair*> uncategorizedLinks;
    SharedRef<BaseFolderPair> output = performComparison(fp, fpConfig, uncategorizedFiles, uncategorizedLinks);

    //finish symlink categorization
    for (SymlinkPair* symlink : uncategorizedLinks)
        categorizeSymlinkByContent(*symlink, cb_); //"compare by size" has the semantics of a quick content-comparison!
    //harmonize with algorithm.cpp, stillInSync()!

    //categorize files that exist on both sides
    for (FilePair* file : uncategorizedFiles)
    {
        //Caveat:
        //1. FILE_EQUAL may only be set if file names match in case: InSyncFolder's mapping tables use file name as a key! see db_file.cpp
        //2. FILE_EQUAL is expected to mean identical file sizes! See InSyncFile
        //3. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::setSyncedTo() in file_hierarchy.h
        if (file->getFileSize<SelectSide::left>() == file->getFileSize<SelectSide::right>())
            file->setContentCategory(FileContentCategory::equal);
        else
            file->setContentCategory(FileContentCategory::different);
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
        std::wstring statusMsg = replaceCpy(txtComparingContentOfFiles, L"%x", fmtPath(file.getRelativePath<SelectSide::left>()));
        //is it possible that right side has a different relPath? maybe, but who cares, it's a short-lived status message

        ItemStatReporter statReporter(1, file.getFileSize<SelectSide::left>(), acb);
        PercentStatReporter percentReporter(statusMsg, file.getFileSize<SelectSide::left>(), statReporter);

        acb.updateStatus(std::move(statusMsg)); //throw ThreadStopRequest

        //callbacks run *outside* singleThread lock! => fine
        auto notifyUnbufferedIO = [&percentReporter](int64_t bytesDelta)
        {
            percentReporter.updateDeltaAndStatus(bytesDelta); //throw ThreadStopRequest
            interruptionPoint(); //throw ThreadStopRequest => not reliably covered by PercentStatReporter::updateDeltaAndStatus()!
        };

        haveSameContent = parallel::filesHaveSameContent(file.getAbstractPath<SelectSide::left >(),
                                                         file.getAbstractPath<SelectSide::right>(), notifyUnbufferedIO, singleThread); //throw FileError, ThreadStopRequest
        statReporter.reportDelta(1, 0);
    }, acb); //throw ThreadStopRequest

    if (!errMsg.empty())
        file.setCategoryConflict(utfTo<Zstringc>(errMsg));
    else
        file.setContentCategory(haveSameContent ? FileContentCategory::equal : FileContentCategory::different);
}
}


std::vector<SharedRef<BaseFolderPair>> ComparisonBuffer::compareByContent(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad) const
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

    std::vector<SharedRef<BaseFolderPair>> output;

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
                file->setContentCategory(FileContentCategory::different);
            else
            {
                //perf: skip binary comparison for excluded rows (e.g. via time span and size filter)!
                //both soft and hard filter were already applied in ComparisonBuffer::performComparison()!
                assert(file->getContentCategory() == FileContentCategory::unknown); //=default
                if (!file->isActive())
                    file->setCategoryConflict(txtConflictSkippedBinaryComparison);
                else
                    filesToCompareBytewise.push_back(file);
            }
        if (!filesToCompareBytewise.empty())
            addToBinaryWorkload(output.back().ref().getAbstractPath<SelectSide::left >(),
                                output.back().ref().getAbstractPath<SelectSide::right>(), std::move(filesToCompareBytewise));

        //finish symlink categorization
        for (SymlinkPair* symlink : uncategorizedLinks)
            categorizeSymlinkByContent(*symlink, cb_);
    }

    //finish categorization: compare files (that have same size) bytewise...
    if (!fpWorkload.empty()) //run ProcessPhase::binaryCompare only when needed
    {
        int      itemsTotal = 0;
        uint64_t bytesTotal = 0;
        for (const BinaryWorkload& bwl : fpWorkload)
        {
            itemsTotal += static_cast<int>(bwl.filesToCompareBytewise.size());

            for (const FilePair* file : bwl.filesToCompareBytewise)
                bytesTotal += file->getFileSize<SelectSide::left>(); //left and right file sizes are equal
        }
        cb_.initNewPhase(itemsTotal, bytesTotal, ProcessPhase::binaryCompare); //throw X

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
    static void execute(const FolderContainer& lhs, const FolderContainer& rhs,
                        const std::unordered_map<Zstring, Zstringc>& errorsByRelPathL,
                        const std::unordered_map<Zstring, Zstringc>& errorsByRelPathR,
                        ContainerObject& output,
                        std::vector<FilePair*>& undefinedFilesOut,
                        std::vector<SymlinkPair*>& undefinedSymlinksOut)
    {
        MergeSides inst(errorsByRelPathL, errorsByRelPathR, undefinedFilesOut, undefinedSymlinksOut);

        const Zstringc* errorMsg = nullptr;
        if (auto it = inst.errorsByRelPathL_.find(Zstring()); //empty path if read-error for whole base directory
            it != inst.errorsByRelPathL_.end())
            errorMsg = &it->second;
        else if (auto it2 = inst.errorsByRelPathR_.find(Zstring());
                 it2 != inst.errorsByRelPathR_.end())
            errorMsg = &it2->second;

        inst.mergeFolders(lhs, rhs, errorMsg, output);
    }

private:
    MergeSides(const std::unordered_map<Zstring, Zstringc>& errorsByRelPathL,
               const std::unordered_map<Zstring, Zstringc>& errorsByRelPathR,
               std::vector<FilePair*>& undefinedFilesOut,
               std::vector<SymlinkPair*>& undefinedSymlinksOut) :
        errorsByRelPathL_(errorsByRelPathL),
        errorsByRelPathR_(errorsByRelPathR),
        undefinedFiles_(undefinedFilesOut),
        undefinedSymlinks_(undefinedSymlinksOut) {}

    void mergeFolders(const FolderContainer& lhs, const FolderContainer& rhs, const Zstringc* errorMsg, ContainerObject& output);

    template <SelectSide side>
    void fillOneSide(const FolderContainer& folderCont, const Zstringc* errorMsg, ContainerObject& output);

    template <SelectSide side>
    const Zstringc* checkFailedRead(FileSystemObject& fsObj, const Zstringc* errorMsg);

    const Zstringc* checkFailedRead(FileSystemObject& fsObj, const Zstringc* errorMsg);

    const std::unordered_map<Zstring, Zstringc>& errorsByRelPathL_; //base-relative paths or empty if read-error for whole base directory
    const std::unordered_map<Zstring, Zstringc>& errorsByRelPathR_; //
    std::vector<FilePair*>& undefinedFiles_;
    std::vector<SymlinkPair*>& undefinedSymlinks_;
};


template <SelectSide side> inline
const Zstringc* MergeSides::checkFailedRead(FileSystemObject& fsObj, const Zstringc* errorMsg)
{
    if (!errorMsg)
    {
        const std::unordered_map<Zstring, Zstringc>& errorsByRelPath = selectParam<side>(errorsByRelPathL_, errorsByRelPathR_);

        if (!errorsByRelPath.empty()) //only pay for relPath construction when needed
            if (const auto it = errorsByRelPath.find(fsObj.getRelativePath<side>());
                it != errorsByRelPath.end())
                errorMsg = &it->second;
    }

    if (errorMsg) //make sure all items are disabled => avoid user panicking: https://freefilesync.org/forum/viewtopic.php?t=7582
    {
        fsObj.setActive(false);
        fsObj.setCategoryConflict(*errorMsg); //peak memory: Zstringc is ref-counted, unlike std::string!
        static_assert(std::is_same_v<const Zstringc&, decltype(*errorMsg)>);
    }
    return errorMsg;
}


const Zstringc* MergeSides::checkFailedRead(FileSystemObject& fsObj, const Zstringc* errorMsg)
{
    if (const Zstringc* errorMsgNew = checkFailedRead<SelectSide::left>(fsObj, errorMsg))
        return errorMsgNew;

    return checkFailedRead<SelectSide::right>(fsObj, errorMsg);
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
        FilePair& newItem = output.addFile<side>(fileName, attrib);
        checkFailedRead<side>(newItem, errorMsg);
    });

    forEachSorted(folderCont.symlinks, [&](const Zstring& linkName, const LinkAttributes& attrib)
    {
        SymlinkPair& newItem = output.addLink<side>(linkName, attrib);
        checkFailedRead<side>(newItem, errorMsg);
    });

    forEachSorted(folderCont.folders, [&](const Zstring& folderName, const std::pair<FolderAttributes, FolderContainer>& attrib)
    {
        FolderPair& newFolder = output.addFolder<side>(folderName, attrib.first);
        const Zstringc* errorMsgNew = checkFailedRead<side>(newFolder, errorMsg);
        fillOneSide<side>(attrib.second, errorMsgNew, newFolder); //recurse
    });
}


template <class MapType, class ProcessLeftOnly, class ProcessRightOnly, class ProcessBoth> inline
void matchFolders(const MapType& mapLeft, const MapType& mapRight, ProcessLeftOnly lo, ProcessRightOnly ro, ProcessBoth bo)
{
    struct FileRef
    {
        Zstring canonicalName; //perf: buffer instead of compareNoCase()/equalNoCase()? => makes no (significant) difference!
        const typename MapType::value_type* ref;
        SelectSide side;
    };
    std::vector<FileRef> fileList;
    fileList.reserve(mapLeft.size() + mapRight.size()); //perf: ~5% shorter runtime

    auto getCanonicalName = [](const Zstring& name) { return trimCpy(getUpperCase(name)); };

    for (const auto& item : mapLeft ) fileList.push_back({getCanonicalName(item.first), &item, SelectSide::left});
    for (const auto& item : mapRight) fileList.push_back({getCanonicalName(item.first), &item, SelectSide::right});

    //primary sort: ignore upper/lower case, leading/trailing space, Unicode normal form
    //bonus: natural default sequence on UI file grid
    std::sort(fileList.begin(), fileList.end(), [](const FileRef& lhs, const FileRef& rhs) { return lhs.canonicalName < rhs.canonicalName; });

    using ItType = typename std::vector<FileRef>::iterator;
    auto tryMatchRange = [&](ItType it, ItType itLast) //auto parameters? compiler error on VS 17.2...
    {
        const size_t equalCountL = std::count_if(it, itLast, [](const FileRef& fr) { return fr.side == SelectSide::left; });
        const size_t equalCountR = itLast - it - equalCountL;

        if (equalCountL == 1 && equalCountR == 1) //we have a match
        {
            if (it->side == SelectSide::left)
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
        //find equal range: ignore upper/lower case, leading/trailing space, Unicode normal form
        auto itEndEq = std::find_if(it + 1, fileList.end(), [&](const FileRef& fr) { return fr.canonicalName != it->canonicalName; });
        if (!tryMatchRange(it, itEndEq))
        {
            //secondary sort: respect case, ignore Unicode normal forms
            std::sort(it, itEndEq, [](const FileRef& lhs, const FileRef& rhs) { return getUnicodeNormalForm(lhs.ref->first) < getUnicodeNormalForm(rhs.ref->first); });

            for (auto itCase = it; itCase != itEndEq;)
            {
                //find equal range: respect case, ignore Unicode normal forms
                auto itEndCase = std::find_if(itCase + 1, itEndEq, [&](const FileRef& fr) { return getUnicodeNormalForm(fr.ref->first) != getUnicodeNormalForm(itCase->ref->first); });
                if (!tryMatchRange(itCase, itEndCase))
                {
                    const Zstringc& conflictMsg = getConflictAmbiguousItemName(itCase->ref->first);
                    std::for_each(itCase, itEndCase, [&](const FileRef& fr)
                    {
                        if (fr.side == SelectSide::left)
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


void MergeSides::mergeFolders(const FolderContainer& lhs, const FolderContainer& rhs, const Zstringc* errorMsg, ContainerObject& output)
{
    using FileData = FolderContainer::FileList::value_type;

    matchFolders(lhs.files, rhs.files, [&](const FileData& fileLeft, const Zstringc* conflictMsg)
    {
        FilePair& newItem = output.addFile<SelectSide::left>(fileLeft.first, fileLeft.second);
        checkFailedRead(newItem, conflictMsg ? conflictMsg : errorMsg);
    },
    [&](const FileData& fileRight, const Zstringc* conflictMsg)
    {
        FilePair& newItem = output.addFile<SelectSide::right>(fileRight.first, fileRight.second);
        checkFailedRead(newItem, conflictMsg ? conflictMsg : errorMsg);
    },
    [&](const FileData& fileLeft, const FileData& fileRight)
    {
        FilePair& newItem = output.addFile(fileLeft.first,
                                           fileLeft.second,
                                           fileRight.first,
                                           fileRight.second);
        if (!checkFailedRead(newItem, errorMsg))
            undefinedFiles_.push_back(&newItem);
        static_assert(std::is_same_v<ContainerObject::FileList, std::list<FilePair>>); //ContainerObject::addFile() must NOT invalidate references used in "undefinedFiles"!
    });

    //-----------------------------------------------------------------------------------------------
    using SymlinkData = FolderContainer::SymlinkList::value_type;

    matchFolders(lhs.symlinks, rhs.symlinks, [&](const SymlinkData& symlinkLeft, const Zstringc* conflictMsg)
    {
        SymlinkPair& newItem = output.addLink<SelectSide::left>(symlinkLeft.first, symlinkLeft.second);
        checkFailedRead(newItem, conflictMsg ? conflictMsg : errorMsg);
    },
    [&](const SymlinkData& symlinkRight, const Zstringc* conflictMsg)
    {
        SymlinkPair& newItem = output.addLink<SelectSide::right>(symlinkRight.first, symlinkRight.second);
        checkFailedRead(newItem, conflictMsg ? conflictMsg : errorMsg);
    },
    [&](const SymlinkData& symlinkLeft, const SymlinkData& symlinkRight) //both sides
    {
        SymlinkPair& newItem = output.addLink(symlinkLeft.first,
                                              symlinkLeft.second,
                                              symlinkRight.first,
                                              symlinkRight.second);
        if (!checkFailedRead(newItem, errorMsg))
            undefinedSymlinks_.push_back(&newItem);
    });

    //-----------------------------------------------------------------------------------------------
    using FolderData = FolderContainer::FolderList::value_type;

    matchFolders(lhs.folders, rhs.folders, [&](const FolderData& dirLeft, const Zstringc* conflictMsg)
    {
        FolderPair& newFolder = output.addFolder<SelectSide::left>(dirLeft.first, dirLeft.second.first);
        const Zstringc* errorMsgNew = checkFailedRead(newFolder, conflictMsg ? conflictMsg : errorMsg);
        this->fillOneSide<SelectSide::left>(dirLeft.second.second, errorMsgNew, newFolder); //recurse
    },
    [&](const FolderData& dirRight, const Zstringc* conflictMsg)
    {
        FolderPair& newFolder = output.addFolder<SelectSide::right>(dirRight.first, dirRight.second.first);
        const Zstringc* errorMsgNew = checkFailedRead(newFolder, conflictMsg ? conflictMsg : errorMsg);
        this->fillOneSide<SelectSide::right>(dirRight.second.second, errorMsgNew, newFolder); //recurse
    },
    [&](const FolderData& dirLeft, const FolderData& dirRight)
    {
        FolderPair& newFolder = output.addFolder(dirLeft.first, dirLeft.second.first, dirRight.first, dirRight.second.first);
        const Zstringc* errorMsgNew = checkFailedRead(newFolder, errorMsg);
        mergeFolders(dirLeft.second.second, dirRight.second.second, errorMsgNew, newFolder); //recurse
    });
}

//-----------------------------------------------------------------------------------------------

//uncheck excluded directories (see parallelDeviceTraversal()) + remove superfluous excluded subdirectories
void stripExcludedDirectories(ContainerObject& conObj, const PathFilter& filter)
{
    for (FolderPair& folder : conObj.refSubFolders())
        stripExcludedDirectories(folder, filter);

    /*  remove superfluous directories:
            this does not invalidate "std::vector<FilePair*>& undefinedFiles", since we delete folders only
            and there is no side-effect for memory positions of FilePair and SymlinkPair thanks to std::list!     */
    static_assert(std::is_same_v<std::list<FolderPair>, ContainerObject::FolderList>);

    conObj.refSubFolders().remove_if([&](FolderPair& folder)
    {
        const bool included = folder.passDirFilter(filter, nullptr /*childItemMightMatch*/); //child items were already excluded during scanning

        if (!included) //falsify only! (e.g. might already be inactive due to read error!)
            folder.setActive(false);

        return !included && //don't check active status, but eval filter directly!
               folder.refSubFolders().empty() &&
               folder.refSubLinks  ().empty() &&
               folder.refSubFiles  ().empty();
    });
}


//create comparison result table and fill category except for files existing on both sides: undefinedFiles and undefinedSymlinks are appended!
SharedRef<BaseFolderPair> ComparisonBuffer::performComparison(const ResolvedFolderPair& fp,
                                                              const FolderPairCfg& fpCfg,
                                                              std::vector<FilePair*>& undefinedFiles,
                                                              std::vector<SymlinkPair*>& undefinedSymlinks) const
{
    cb_.updateStatus(_("Generating file list...")); //throw X
    cb_.requestUiUpdate(true /*force*/); //throw X

    const BaseFolderStatus folderStatusL = getBaseFolderStatus(fp.folderPathLeft);
    const BaseFolderStatus folderStatusR = getBaseFolderStatus(fp.folderPathRight);


    std::unordered_map<Zstring, Zstringc> failedReadsL; //base-relative paths or empty if read-error for whole base directory
    std::unordered_map<Zstring, Zstringc> failedReadsR; //
    const FolderContainer* folderContL = nullptr;
    const FolderContainer* folderContR = nullptr;


    const FolderContainer empty;
    if (folderStatusL == BaseFolderStatus::failure ||
        folderStatusR == BaseFolderStatus::failure)
    {
        auto it = folderStatus_.failedChecks.find(fp.folderPathLeft);
        if (it == folderStatus_.failedChecks.end())
            it = folderStatus_.failedChecks.find(fp.folderPathRight);

        failedReadsL[Zstring() /*empty string for root*/] = failedReadsR[Zstring()] = utfTo<Zstringc>(it->second.toString());

        folderContL = &empty; //no need to list or display one-sided results if
        folderContR = &empty; //*any* folder existence check failed (even if other side exists in folderBuffer_!)
    }
    else
    {
        auto evalBuffer = [&](const AbstractPath& folderPath, const FolderContainer*& folderCont, std::unordered_map<Zstring, Zstringc>& failedReads)
        {
            auto it = folderBuffer_.find({folderPath, fpCfg.filter.nameFilter, fpCfg.handleSymlinks});
            if (it != folderBuffer_.end())
            {
                const DirectoryValue& dirVal = it->second;

                //mix failedFolderReads with failedItemReads:
                //associate folder traversing errors with folder (instead of child items only) to show on GUI! See "MergeSides"
                //=> minor pessimization for "excludeFilterFailedRead" which needlessly excludes parent folders, too
                failedReads = dirVal.failedFolderReads; //failedReads.insert(dirVal.failedFolderReads.begin(), dirVal.failedFolderReads.end());
                failedReads.insert(dirVal.failedItemReads.begin(), dirVal.failedItemReads.end());

                assert(getBaseFolderStatus(folderPath) == BaseFolderStatus::existing);
                folderCont = &dirVal.folderCont;
            }
            else
            {
                assert(getBaseFolderStatus(folderPath) == BaseFolderStatus::notExisting); //including AFS::isNullPath()
                folderCont = &empty;
            }
        };
        evalBuffer(fp.folderPathLeft,  folderContL, failedReadsL);
        evalBuffer(fp.folderPathRight, folderContR, failedReadsR);
    }


    Zstring excludeFilterFailedRead;
    if (failedReadsL.contains(Zstring()) ||
        failedReadsR.contains(Zstring())) //empty path if read-error for whole base directory
        excludeFilterFailedRead += Zstr("*\n");
    else
    {
        for (const auto& [relPath, errorMsg] : failedReadsL)
            excludeFilterFailedRead += relPath + Zstr('\n'); //exclude item AND (potential) child items!

        for (const auto& [relPath, errorMsg] : failedReadsR)
            excludeFilterFailedRead += relPath + Zstr('\n');
    }

    //somewhat obscure, but it's possible on Linux file systems to have a backslash as part of a file name
    //=> avoid misinterpretation when parsing the filter phrase in PathFilter (see path_filter.cpp::parseFilterPhrase())
    if constexpr (FILE_NAME_SEPARATOR != Zstr('/' )) replace(excludeFilterFailedRead, Zstr('/'),  Zstr('?'));
    if constexpr (FILE_NAME_SEPARATOR != Zstr('\\')) replace(excludeFilterFailedRead, Zstr('\\'), Zstr('?'));


    SharedRef<BaseFolderPair> output = makeSharedRef<BaseFolderPair>(fp.folderPathLeft,
                                                                     folderStatusL, //check folder existence only once!
                                                                     fp.folderPathRight,
                                                                     folderStatusR, //
                                                                     fpCfg.filter.nameFilter.ref().copyFilterAddingExclusion(excludeFilterFailedRead),
                                                                     fpCfg.compareVar,
                                                                     fileTimeTolerance_,
                                                                     fpCfg.ignoreTimeShiftMinutes);
    //PERF_START;
    MergeSides::execute(*folderContL, *folderContR, failedReadsL, failedReadsR,
                        output.ref(), undefinedFiles, undefinedSymlinks);
    //PERF_STOP;

    //##################### in/exclude rows according to filtering #####################
    //NOTE: we need to finish de-activating rows BEFORE binary comparison is run so that it can skip them!

    //attention: some excluded directories are still in the comparison result! (see include filter handling!)
    if (!fpCfg.filter.nameFilter.ref().isNull())
        stripExcludedDirectories(output.ref(), fpCfg.filter.nameFilter.ref()); //mark excluded directories (see parallelDeviceTraversal()) + remove superfluous excluded subdirectories

    //apply soft filtering (hard filter already applied during traversal!)
    addSoftFiltering(output.ref(), fpCfg.filter.timeSizeFilter);

    //##################################################################################
    return output;
}
}


FolderComparison fff::compare(WarningDialogs& warnings,
                              unsigned int fileTimeTolerance,
                              const AFS::RequestPasswordFun& requestPassword /*throw X*/,
                              bool runWithBackgroundPriority,
                              bool createDirLocks,
                              std::unique_ptr<LockHolder>& dirLocks,
                              const std::vector<FolderPairCfg>& fpCfgList,
                              ProcessCallback& callback /*throw X*/) //throw X
{
    //indicator at the very beginning of the log to make sense of "total time"
    //init process: keep at beginning so that all gui elements are initialized properly
    callback.initNewPhase(-1, -1, ProcessPhase::scan); //throw X; it's unknown how many files will be scanned => -1 objects
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
        callback.logMessage(e.toString(), PhaseCallback::MsgType::warning); //throw X
    }

    const ResolvedBaseFolders& resInfo = initializeBaseFolders(fpCfgList,
                                                               requestPassword, warnings, callback); //throw X
    //directory existence only checked *once* to avoid race conditions!
    if (resInfo.resolvedPairs.size() != fpCfgList.size())
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

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
            callback.reportWarning(_("A folder input field is empty.") + L" \n\n" +
                                   _("The corresponding folder will be considered as empty."), warnings.warnInputFieldEmpty); //throw X
    }

    //Check whether one side is a sub directory of the other side (folder-pair-wise!)
    //The similar check (warnDependentBaseFolders) if one directory is read/written by multiple pairs not before beginning of synchronization
    {
        std::wstring msg;
        bool shouldExclude = false;

        for (const auto& [folderPair, fpCfg] : workLoad)
            if (std::optional<PathDependency> pd = getFolderPathDependency(folderPair.folderPathLeft,  fpCfg.filter.nameFilter.ref(),
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
            callback.reportWarning(_("One folder of the folder pair is a subfolder of the other.") +
                                   (shouldExclude ? L'\n' + _("The folder should be excluded via filter.") : L"") +
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
            ComparisonBuffer cmpBuf(resInfo.baseFolderStatus,
                                    fileTimeTolerance, callback);
            //PERF_START;
            output = cmpBuf.execute(workLoad);
            //PERF_STOP;
        }
        assert(output.size() == fpCfgList.size());

        //--------- set initial sync-direction --------------------------------------------------
        std::vector<std::pair<BaseFolderPair*, SyncDirectionConfig>> directCfgs;
        for (auto it = output.begin(); it != output.end(); ++it)
            directCfgs.emplace_back(&it->ref(), fpCfgList[it - output.begin()].directionCfg);

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

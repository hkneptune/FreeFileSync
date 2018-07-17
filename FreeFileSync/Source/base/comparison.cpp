// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "comparison.h"
#include <zen/process_priority.h>
#include <zen/perf.h>
#include "algorithm.h"
#include "parallel_scan.h"
#include "dir_exist_async.h"
#include "db_file.h"
#include "binary.h"
#include "cmp_filetime.h"
#include "status_handler_impl.h"
#include "../fs/concrete.h"

using namespace zen;
using namespace fff;


std::vector<FolderPairCfg> fff::extractCompareCfg(const MainConfiguration& mainCfg)
{
    //merge first and additional pairs
    std::vector<LocalPairConfig> localCfgs = { mainCfg.firstPair };
    append(localCfgs, mainCfg.additionalPairs);

    std::vector<FolderPairCfg> output;

    for (const LocalPairConfig& lpc : localCfgs)
    {
        const CompConfig cmpCfg  = lpc.localCmpCfg  ? *lpc.localCmpCfg  : mainCfg.cmpCfg;
        const SyncConfig syncCfg = lpc.localSyncCfg ? *lpc.localSyncCfg : mainCfg.syncCfg;
        NormalizedFilter filter = normalizeFilters(mainCfg.globalFilter, lpc.localFilter);

        //exclude the database file(s) sync.ffs_db, sync.x64.ffs_db, etc. and lock files
        //=> can't put inside fff::parallelDeviceTraversal() which is also used by versioning
        filter.nameFilter = filter.nameFilter->copyFilterAddingExclusion(Zstring(Zstr("*")) + SYNC_DB_FILE_ENDING + Zstr("\n*") + LOCK_FILE_ENDING);

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
    std::set<AbstractPath> existingBaseFolders;
};


ResolvedBaseFolders initializeBaseFolders(const std::vector<FolderPairCfg>& fpCfgList, const std::map<AbstractPath, size_t>& deviceParallelOps,
                                          int folderAccessTimeout,
                                          bool allowUserInteraction,
                                          bool& warnFolderNotExisting,
                                          ProcessCallback& callback /*throw X*/)
{
    ResolvedBaseFolders output;
    std::set<AbstractPath> notExisting;

    tryReportingError([&]
    {
        std::set<AbstractPath> uniqueBaseFolders;

        //support "retry" for environment variable and variable driver letter resolution!
        output.resolvedPairs.clear();
        for (const FolderPairCfg& fpCfg : fpCfgList)
        {
            AbstractPath folderPathLeft  = createAbstractPath(fpCfg.folderPathPhraseLeft_);
            AbstractPath folderPathRight = createAbstractPath(fpCfg.folderPathPhraseRight_);

            uniqueBaseFolders.insert(folderPathLeft);
            uniqueBaseFolders.insert(folderPathRight);

            output.resolvedPairs.push_back({ folderPathLeft, folderPathRight });
        }

        const FolderStatus status = getFolderStatusNonBlocking(uniqueBaseFolders, deviceParallelOps, //re-check *all* directories on each try!
                                                               folderAccessTimeout, allowUserInteraction, callback); //throw X
        output.existingBaseFolders = status.existing;
        notExisting                = status.notExisting;
        if (!status.failedChecks.empty())
        {
            std::wstring msg = _("Cannot find the following folders:") + L"\n";

            for (const auto& fc : status.failedChecks)
                msg += L"\n" + AFS::getDisplayPath(fc.first);

            msg += L"\n___________________________________________";
            for (const auto& fc : status.failedChecks)
                msg += L"\n\n" + replaceCpy(fc.second.toString(), L"\n\n", L"\n");

            throw FileError(msg);
        }
    }, callback); //throw X


    if (!notExisting.empty())
    {
        std::wstring msg = _("The following folders do not yet exist:") + L"\n";

        for (const AbstractPath& folderPath : notExisting)
            msg += L"\n" + AFS::getDisplayPath(folderPath);

        msg += L"\n\n";
        msg +=  _("The folders are created automatically when needed.");

        callback.reportWarning(msg, warnFolderNotExisting); //throw X
    }
    return output;
}

//#############################################################################################################################

class ComparisonBuffer
{
public:
    ComparisonBuffer(const std::set<DirectoryKey>& foldersToRead,
                     const std::map<AbstractPath, size_t>& deviceParallelOps,
                     int fileTimeTolerance,
                     ProcessCallback& callback);

    //create comparison result table and fill category except for files existing on both sides: undefinedFiles and undefinedSymlinks are appended!
    std::shared_ptr<BaseFolderPair> compareByTimeSize(const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const;
    std::shared_ptr<BaseFolderPair> compareBySize    (const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const;
    std::list<std::shared_ptr<BaseFolderPair>> compareByContent(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad) const;

private:
    ComparisonBuffer           (const ComparisonBuffer&) = delete;
    ComparisonBuffer& operator=(const ComparisonBuffer&) = delete;

    std::shared_ptr<BaseFolderPair> performComparison(const ResolvedFolderPair& fp,
                                                      const FolderPairCfg& fpCfg,
                                                      std::vector<FilePair*>& undefinedFiles,
                                                      std::vector<SymlinkPair*>& undefinedSymlinks) const;

    std::map<DirectoryKey, DirectoryValue> directoryBuffer_; //contains only *existing* directories
    const int fileTimeTolerance_;
    ProcessCallback& cb_;
    const std::map<AbstractPath, size_t> deviceParallelOps_;
};


ComparisonBuffer::ComparisonBuffer(const std::set<DirectoryKey>& foldersToRead,
                                   const std::map<AbstractPath, size_t>& deviceParallelOps,
                                   int fileTimeTolerance,
                                   ProcessCallback& callback) :
    fileTimeTolerance_(fileTimeTolerance), cb_(callback), deviceParallelOps_(deviceParallelOps)
{
    auto onError = [&](const std::wstring& msg, size_t retryNumber)
    {
        switch (callback.reportError(msg, retryNumber))
        {
            case ProcessCallback::IGNORE_ERROR:
                return AFS::TraverserCallback::ON_ERROR_CONTINUE;

            case ProcessCallback::RETRY:
                return AFS::TraverserCallback::ON_ERROR_RETRY;
        }
        assert(false);
        return AFS::TraverserCallback::ON_ERROR_CONTINUE;
    };

    const std::wstring textScanning = _("Scanning:") + L" ";
    int itemsReported = 0;

    auto onStatusUpdate = [&](const std::wstring& statusLine, int itemsTotal)
    {
        callback.updateDataProcessed(itemsTotal - itemsReported, 0);
        itemsReported = itemsTotal;

        callback.reportStatus(textScanning + statusLine); //throw X
    };

    parallelDeviceTraversal(foldersToRead, //in
                            directoryBuffer_, //out
                            deviceParallelOps,
                            onError, onStatusUpdate, //throw X
                            UI_UPDATE_INTERVAL / 2); //every ~50 ms

    callback.reportInfo(_("Comparison finished:") + L" " + _P("1 item found", "%x items found", itemsReported)); //throw X
}


//--------------------assemble conflict descriptions---------------------------

//const wchar_t arrowLeft [] = L"\u2190";
//const wchar_t arrowRight[] = L"\u2192"; unicode arrows -> too small
const wchar_t arrowLeft [] = L"<-";
const wchar_t arrowRight[] = L"->";

//NOTE: conflict texts are NOT expected to contain additional path info (already implicit through associated item!)
//      => only add path info if information is relevant, e.g. conflict is specific to left/right side only

template <SelectedSide side, class FileOrLinkPair> inline
Zstringw getConflictInvalidDate(const FileOrLinkPair& file)
{
    return copyStringTo<Zstringw>(replaceCpy(_("File %x has an invalid date."), L"%x", fmtPath(AFS::getDisplayPath(file.template getAbstractPath<side>()))) + L"\n" +
                                  _("Date:") + L" " + formatUtcToLocalTime(file.template getLastWriteTime<side>()));
}


Zstringw getConflictSameDateDiffSize(const FilePair& file)
{
    return copyStringTo<Zstringw>(_("Files have the same date but a different size.") + L"\n" +
                                  arrowLeft  + L" " + _("Date:") + L" " + formatUtcToLocalTime(file.getLastWriteTime< LEFT_SIDE>()) + L"    " + _("Size:") + L" " + formatNumber(file.getFileSize<LEFT_SIDE>()) + L"\n" +
                                  arrowRight + L" " + _("Date:") + L" " + formatUtcToLocalTime(file.getLastWriteTime<RIGHT_SIDE>()) + L"    " + _("Size:") + L" " + formatNumber(file.getFileSize<RIGHT_SIDE>()));
}


Zstringw getConflictSkippedBinaryComparison()
{
    return copyStringTo<Zstringw>(_("Content comparison was skipped for excluded files."));
}


Zstringw getDescrDiffMetaShortnameCase(const FileSystemObject& fsObj)
{
    return copyStringTo<Zstringw>(_("Items differ in attributes only") + L"\n" +
                                  arrowLeft  + L" " + fmtPath(fsObj.getItemName< LEFT_SIDE>()) + L"\n" +
                                  arrowRight + L" " + fmtPath(fsObj.getItemName<RIGHT_SIDE>()));
}


template <class FileOrLinkPair>
Zstringw getDescrDiffMetaDate(const FileOrLinkPair& file)
{
    return copyStringTo<Zstringw>(_("Items differ in attributes only") + L"\n" +
                                  arrowLeft  + L" " + _("Date:") + L" " + formatUtcToLocalTime(file.template getLastWriteTime< LEFT_SIDE>()) + L"\n" +
                                  arrowRight + L" " + _("Date:") + L" " + formatUtcToLocalTime(file.template getLastWriteTime<RIGHT_SIDE>()));
}

//-----------------------------------------------------------------------------

void categorizeSymlinkByTime(SymlinkPair& symlink)
{
    //categorize symlinks that exist on both sides
    switch (compareFileTime(symlink.getLastWriteTime<LEFT_SIDE>(),
                            symlink.getLastWriteTime<RIGHT_SIDE>(), symlink.base().getFileTimeTolerance(), symlink.base().getIgnoredTimeShift()))
    {
        case TimeResult::EQUAL:
            //Caveat:
            //1. SYMLINK_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
            //2. harmonize with "bool stillInSync()" in algorithm.cpp

            if (symlink.getItemName<LEFT_SIDE>() == symlink.getItemName<RIGHT_SIDE>())
                symlink.setCategory<FILE_EQUAL>();
            else
                symlink.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(symlink));
            break;

        case TimeResult::LEFT_NEWER:
            symlink.setCategory<FILE_LEFT_NEWER>();
            break;

        case TimeResult::RIGHT_NEWER:
            symlink.setCategory<FILE_RIGHT_NEWER>();
            break;

        case TimeResult::LEFT_INVALID:
            symlink.setCategoryConflict(getConflictInvalidDate<LEFT_SIDE>(symlink));
            break;

        case TimeResult::RIGHT_INVALID:
            symlink.setCategoryConflict(getConflictInvalidDate<RIGHT_SIDE>(symlink));
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
        switch (compareFileTime(file->getLastWriteTime<LEFT_SIDE>(),
                                file->getLastWriteTime<RIGHT_SIDE>(), fileTimeTolerance_, fpConfig.ignoreTimeShiftMinutes))
        {
            case TimeResult::EQUAL:
                //Caveat:
                //1. FILE_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
                //2. FILE_EQUAL is expected to mean identical file sizes! See InSyncFile
                //3. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::setSyncedTo() in file_hierarchy.h
                if (file->getFileSize<LEFT_SIDE>() == file->getFileSize<RIGHT_SIDE>())
                {
                    if (file->getItemName<LEFT_SIDE>() == file->getItemName<RIGHT_SIDE>())
                        file->setCategory<FILE_EQUAL>();
                    else
                        file->setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(*file));
                }
                else
                    file->setCategoryConflict(getConflictSameDateDiffSize(*file)); //same date, different filesize
                break;

            case TimeResult::LEFT_NEWER:
                file->setCategory<FILE_LEFT_NEWER>();
                break;

            case TimeResult::RIGHT_NEWER:
                file->setCategory<FILE_RIGHT_NEWER>();
                break;

            case TimeResult::LEFT_INVALID:
                file->setCategoryConflict(getConflictInvalidDate<LEFT_SIDE>(*file));
                break;

            case TimeResult::RIGHT_INVALID:
                file->setCategoryConflict(getConflictInvalidDate<RIGHT_SIDE>(*file));
                break;
        }
    }
    return output;
}


namespace
{
void categorizeSymlinkByContent(SymlinkPair& symlink, ProcessCallback& callback)
{
    //categorize symlinks that exist on both sides
    std::string binaryContentL;
    std::string binaryContentR;
    const std::wstring errMsg = tryReportingError([&]
    {
        callback.reportStatus(replaceCpy(_("Resolving symbolic link %x"), L"%x", fmtPath(AFS::getDisplayPath(symlink.getAbstractPath<LEFT_SIDE>())))); //throw X
        binaryContentL = AFS::getSymlinkBinaryContent(symlink.getAbstractPath<LEFT_SIDE>()); //throw FileError

        callback.reportStatus(replaceCpy(_("Resolving symbolic link %x"), L"%x", fmtPath(AFS::getDisplayPath(symlink.getAbstractPath<RIGHT_SIDE>())))); //throw X
        binaryContentR = AFS::getSymlinkBinaryContent(symlink.getAbstractPath<RIGHT_SIDE>()); //throw FileError
    }, callback); //throw X

    if (!errMsg.empty())
        symlink.setCategoryConflict(copyStringTo<Zstringw>(errMsg));
    else
    {
        if (binaryContentL == binaryContentR)
        {
            //Caveat:
            //1. SYMLINK_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
            //2. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::setSyncedTo() in file_hierarchy.h

            //symlinks have same "content"
            if (symlink.getItemName<LEFT_SIDE>() != symlink.getItemName<RIGHT_SIDE>())
                symlink.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(symlink));
            //else if (!sameFileTime(symlink.getLastWriteTime<LEFT_SIDE>(),
            //                       symlink.getLastWriteTime<RIGHT_SIDE>(), symlink.base().getFileTimeTolerance(), symlink.base().getIgnoredTimeShift()))
            //    symlink.setCategoryDiffMetadata(getDescrDiffMetaDate(symlink));
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
        if (file->getFileSize<LEFT_SIDE>() == file->getFileSize<RIGHT_SIDE>())
        {
            if (file->getItemName<LEFT_SIDE>() == file->getItemName<RIGHT_SIDE>())
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
bool filesHaveSameContent(const AbstractPath& filePath1, const AbstractPath& filePath2, //throw FileError
                          const zen::IOCallback& notifyUnbufferedIO, //may be nullptr
                          std::mutex& singleThread)
{ return parallelScope([=] { return filesHaveSameContent(filePath1, filePath2, notifyUnbufferedIO); /*throw FileError*/ }, singleThread); }
}


namespace
{
void categorizeFileByContent(FilePair& file, const std::wstring& txtComparingContentOfFiles, AsyncCallback& acb, std::mutex& singleThread) //throw ThreadInterruption
{
    acb.reportStatus(replaceCpy(txtComparingContentOfFiles, L"%x", fmtPath(file.getPairRelativePath()))); //throw ThreadInterruption

    bool haveSameContent = false;
    const std::wstring errMsg = tryReportingError([&]
    {
        AsyncItemStatReporter statReporter(1, file.getFileSize<LEFT_SIDE>(), acb);

        //callbacks run *outside* singleThread_ lock! => fine
        auto notifyUnbufferedIO = [&statReporter](int64_t bytesDelta)
        {
            statReporter.reportDelta(0, bytesDelta);
            interruptionPoint(); //throw ThreadInterruption
        };

        haveSameContent = parallel::filesHaveSameContent(file.getAbstractPath<LEFT_SIDE >(),
                                                         file.getAbstractPath<RIGHT_SIDE>(), notifyUnbufferedIO, singleThread); //throw FileError
        statReporter.reportDelta(1, 0);
    }, acb); //throw ThreadInterruption

    if (!errMsg.empty())
        file.setCategoryConflict(copyStringTo<Zstringw>(errMsg));
    else
    {
        if (haveSameContent)
        {
            //Caveat:
            //1. FILE_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
            //2. FILE_EQUAL is expected to mean identical file sizes! See InSyncFile
            //3. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::setSyncedTo() in file_hierarchy.h
            if (file.getItemName<LEFT_SIDE>() != file.getItemName<RIGHT_SIDE>())
                file.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(file));
#if 0 //don't synchronize modtime only see FolderPairSyncer::synchronizeFileInt(), SO_COPY_METADATA_TO_*
            else if (!sameFileTime(file.getLastWriteTime<LEFT_SIDE>(),
                                   file.getLastWriteTime<RIGHT_SIDE>(), file.base().getFileTimeTolerance(), file.base().getIgnoredTimeShift()))
                file.setCategoryDiffMetadata(getDescrDiffMetaDate(file));
#endif
            else
                file.setCategory<FILE_EQUAL>();
        }
        else
            file.setCategory<FILE_DIFFERENT_CONTENT>();
    }
}
}


std::list<std::shared_ptr<BaseFolderPair>> ComparisonBuffer::compareByContent(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad) const
{
    struct ParallelOps
    {
        size_t current      = 0;
        size_t effectiveMax = 0; //a folder pair is allowed to use the maximum parallelOps that left/right devices support
        //                          => consider max over all folder pairs, that a device is involved with!
    };
    std::map<AbstractPath, ParallelOps> parallelOpsStatus;

    struct BinaryWorkload
    {
        ParallelOps& parallelOpsL; //
        ParallelOps& parallelOpsR; //consider aliasing!
        RingBuffer<FilePair*> filesToCompareBytewise;
    };
    std::vector<BinaryWorkload> fpWorkload;

    auto addToBinaryWorkload = [&](const AbstractPath& basePathL, const AbstractPath& basePathR, RingBuffer<FilePair*>&& filesToCompareBytewise)
    {
        const AbstractPath rootPathL = AFS::getRootPath(basePathL);
        const AbstractPath rootPathR = AFS::getRootPath(basePathR);

        //calculate effective max parallelOps that devices must support
        const size_t parallelOpsFp = std::max(getDeviceParallelOps(deviceParallelOps_, rootPathL),
                                              getDeviceParallelOps(deviceParallelOps_, rootPathR));

        ParallelOps& posL = parallelOpsStatus[rootPathL];
        ParallelOps& posR = parallelOpsStatus[rootPathR];

        posL.effectiveMax = std::max(posL.effectiveMax, parallelOpsFp);
        posR.effectiveMax = std::max(posR.effectiveMax, parallelOpsFp);

        fpWorkload.push_back({ posL, posR, std::move(filesToCompareBytewise) });
    };

    //PERF_START;
    std::list<std::shared_ptr<BaseFolderPair>> output;

    const Zstringw txtConflictSkippedBinaryComparison = getConflictSkippedBinaryComparison(); //avoid premature pess.: save memory via ref-counted string

    for (const auto& w : workLoad)
    {
        std::vector<FilePair*> undefinedFiles;
        std::vector<SymlinkPair*> uncategorizedLinks;
        //run basis scan and retrieve candidates for binary comparison (files existing on both sides)
        output.push_back(performComparison(w.first, w.second, undefinedFiles, uncategorizedLinks));

        RingBuffer<FilePair*> filesToCompareBytewise;
        //content comparison of file content happens AFTER finding corresponding files and AFTER filtering
        //in order to separate into two processes (scanning and comparing)
        for (FilePair* file : undefinedFiles)
            //pre-check: files have different content if they have a different file size (must not be FILE_EQUAL: see InSyncFile)
            if (file->getFileSize<LEFT_SIDE>() != file->getFileSize<RIGHT_SIDE>())
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
            addToBinaryWorkload(output.back()->getAbstractPath<LEFT_SIDE >(),
                                output.back()->getAbstractPath<RIGHT_SIDE>(), std::move(filesToCompareBytewise));

        //finish symlink categorization
        for (SymlinkPair* symlink : uncategorizedLinks)
            categorizeSymlinkByContent(*symlink, cb_);
    }

    //finish categorization: compare files (that have same size) bytewise...
    if (!fpWorkload.empty()) //run PHASE_COMPARING_CONTENT only when needed
    {
        int      itemsTotal = 0;
        uint64_t bytesTotal = 0;
        for (const BinaryWorkload& bwl : fpWorkload)
        {
            itemsTotal += bwl.filesToCompareBytewise.size();

            for (const FilePair* file : bwl.filesToCompareBytewise)
                bytesTotal += file->getFileSize<LEFT_SIDE>(); //left and right file sizes are equal
        }
        cb_.initNewPhase(itemsTotal, bytesTotal, ProcessCallback::PHASE_COMPARING_CONTENT); //throw X

        //PERF_START;

        std::mutex singleThread; //only a single worker thread may run at a time, except for parallel file I/O

        AsyncCallback acb;                       //
        std::function<void()> scheduleMoreTasks; //manage life time: enclose ThreadGroup!
        const std::wstring txtComparingContentOfFiles = _("Comparing content of files %x"); //

        ThreadGroup<std::function<void()>> tg(std::numeric_limits<size_t>::max(), "Binary Comparison");

        scheduleMoreTasks = [&]
        {
            bool wereDone = true;

            for (size_t j = 0; j < fpWorkload.size(); ++j)
            {
                BinaryWorkload& bwl = fpWorkload[j];

                ParallelOps& posL = bwl.parallelOpsL;
                ParallelOps& posR = bwl.parallelOpsR;

                const size_t newTaskCount = numeric::min<size_t>(posL.effectiveMax - posL.current,
                                                                 posR.effectiveMax - posR.current,
                                                                 bwl.filesToCompareBytewise.size());
                if (&posL != &posR) posL.current += newTaskCount; //
                /**/                posR.current += newTaskCount; //consider aliasing!

                for (size_t i = 0; i < newTaskCount; ++i)
                {
                    tg.run([&, statusPrio = j, &file = *bwl.filesToCompareBytewise.front()]
                    {
                        acb.notifyTaskBegin(statusPrio); //prioritize status messages according to natural order of folder pairs
                        ZEN_ON_SCOPE_EXIT(acb.notifyTaskEnd());

                        std::lock_guard<std::mutex> dummy(singleThread); //protect ALL variable accesses unless explicitly not needed ("parallel" scope)!
                        //---------------------------------------------------------------------------------------------------
                        ZEN_ON_SCOPE_SUCCESS(if (&posL != &posR) --posL.current;
                                             /**/                --posR.current;
                                             scheduleMoreTasks(););

                        categorizeFileByContent(file, txtComparingContentOfFiles, acb, singleThread); //throw ThreadInterruption
                    });

                    bwl.filesToCompareBytewise.pop_front();
                }

                assert(0 <= posL.current && posL.current <= posL.effectiveMax);
                assert(0 <= posR.current && posR.current <= posR.effectiveMax);

                if (posL.current != 0 || posR.current != 0 || !bwl.filesToCompareBytewise.empty())
                    wereDone = false;
            }
            if (wereDone)
                acb.notifyAllDone();
        };

        {
            std::lock_guard<std::mutex> dummy(singleThread); //[!] potential race with worker threads!
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
    MergeSides(const std::map<Zstring, Zstringw, LessFilePath>& failedItemReads,
               std::vector<FilePair*>& undefinedFilesOut,
               std::vector<SymlinkPair*>& undefinedSymlinksOut) :
        failedItemReads_(failedItemReads),
        undefinedFiles_(undefinedFilesOut),
        undefinedSymlinks_(undefinedSymlinksOut) {}

    void execute(const FolderContainer& lhs, const FolderContainer& rhs, ContainerObject& output)
    {
        auto it = failedItemReads_.find(Zstring()); //empty path if read-error for whole base directory

        mergeTwoSides(lhs, rhs,
                      it != failedItemReads_.end() ? &it->second : nullptr,
                      output);
    }

private:
    void mergeTwoSides(const FolderContainer& lhs, const FolderContainer& rhs, const Zstringw* errorMsg, ContainerObject& output);

    template <SelectedSide side>
    void fillOneSide(const FolderContainer& folderCont, const Zstringw* errorMsg, ContainerObject& output);

    const Zstringw* checkFailedRead(FileSystemObject& fsObj, const Zstringw* errorMsg);

    const std::map<Zstring, Zstringw, LessFilePath>& failedItemReads_; //base-relative paths or empty if read-error for whole base directory
    std::vector<FilePair*>&   undefinedFiles_;
    std::vector<SymlinkPair*>& undefinedSymlinks_;
};


inline
const Zstringw* MergeSides::checkFailedRead(FileSystemObject& fsObj, const Zstringw* errorMsg)
{
    if (!errorMsg)
    {
        auto it = failedItemReads_.find(fsObj.getPairRelativePath());
        if (it != failedItemReads_.end())
            errorMsg = &it->second;
    }

    if (errorMsg)
    {
        fsObj.setActive(false);
        fsObj.setCategoryConflict(*errorMsg); //peak memory: Zstringw is ref-counted, unlike std::wstring!
        static_assert(std::is_same_v<const Zstringw&, decltype(*errorMsg)>);
    }
    return errorMsg;
}


template <SelectedSide side>
void MergeSides::fillOneSide(const FolderContainer& folderCont, const Zstringw* errorMsg, ContainerObject& output)
{
    for (const auto& file : folderCont.files)
    {
        FilePair& newItem = output.addSubFile<side>(file.first, file.second);
        checkFailedRead(newItem, errorMsg);
    }

    for (const auto& symlink : folderCont.symlinks)
    {
        SymlinkPair& newItem = output.addSubLink<side>(symlink.first, symlink.second);
        checkFailedRead(newItem, errorMsg);
    }

    for (const auto& dir : folderCont.folders)
    {
        FolderPair& newFolder = output.addSubFolder<side>(dir.first, dir.second.first);
        const Zstringw* errorMsgNew = checkFailedRead(newFolder, errorMsg);
        fillOneSide<side>(dir.second.second, errorMsgNew, newFolder); //recurse
    }
}


//perf: 70% faster than traversing over left and right containers + more natural default sequence
//- 2 x lessKey vs 1 x cmpFilePath() => no significant difference
//- simplify loop by placing the eob check at the beginning => slightly slower
template <class MapType, class ProcessLeftOnly, class ProcessRightOnly, class ProcessBoth> inline
void linearMerge(const MapType& mapLeft, const MapType& mapRight, ProcessLeftOnly lo, ProcessRightOnly ro, ProcessBoth bo)
{
    auto itL = mapLeft .begin();
    auto itR = mapRight.begin();

    auto finishLeft  = [&] { std::for_each(itL, mapLeft .end(), lo); };
    auto finishRight = [&] { std::for_each(itR, mapRight.end(), ro); };

    if (itL == mapLeft .end()) return finishRight();
    if (itR == mapRight.end()) return finishLeft ();

    const auto lessKey = typename MapType::key_compare();

    for (;;)
        if (lessKey(itL->first, itR->first))
        {
            lo(*itL);
            if (++itL == mapLeft.end())
                return finishRight();
        }
        else if (lessKey(itR->first, itL->first))
        {
            ro(*itR);
            if (++itR == mapRight.end())
                return finishLeft();
        }
        else
        {
            bo(*itL, *itR);
            ++itL; //
            ++itR; //increment BOTH before checking for end of range!
            if (itL == mapLeft .end()) return finishRight();
            if (itR == mapRight.end()) return finishLeft ();
        }
}


void MergeSides::mergeTwoSides(const FolderContainer& lhs, const FolderContainer& rhs, const Zstringw* errorMsg, ContainerObject& output)
{
    using FileData = FolderContainer::FileList::value_type;

    linearMerge(lhs.files, rhs.files,
    [&](const FileData& fileLeft ) { FilePair& newItem = output.addSubFile< LEFT_SIDE>(fileLeft .first, fileLeft .second); checkFailedRead(newItem, errorMsg); }, //left only
    [&](const FileData& fileRight) { FilePair& newItem = output.addSubFile<RIGHT_SIDE>(fileRight.first, fileRight.second); checkFailedRead(newItem, errorMsg); }, //right only

    [&](const FileData& fileLeft, const FileData& fileRight) //both sides
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

    linearMerge(lhs.symlinks, rhs.symlinks,
    [&](const SymlinkData& symlinkLeft ) { SymlinkPair& newItem = output.addSubLink< LEFT_SIDE>(symlinkLeft .first, symlinkLeft .second); checkFailedRead(newItem, errorMsg); }, //left only
    [&](const SymlinkData& symlinkRight) { SymlinkPair& newItem = output.addSubLink<RIGHT_SIDE>(symlinkRight.first, symlinkRight.second); checkFailedRead(newItem, errorMsg); }, //right only

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

    linearMerge(lhs.folders, rhs.folders,
                [&](const FolderData& dirLeft) //left only
    {
        FolderPair& newFolder = output.addSubFolder<LEFT_SIDE>(dirLeft.first, dirLeft.second.first);
        const Zstringw* errorMsgNew = checkFailedRead(newFolder, errorMsg);
        this->fillOneSide<LEFT_SIDE>(dirLeft.second.second, errorMsgNew, newFolder); //recurse
    },
    [&](const FolderData& dirRight) //right only
    {
        FolderPair& newFolder = output.addSubFolder<RIGHT_SIDE>(dirRight.first, dirRight.second.first);
        const Zstringw* errorMsgNew = checkFailedRead(newFolder, errorMsg);
        this->fillOneSide<RIGHT_SIDE>(dirRight.second.second, errorMsgNew, newFolder); //recurse
    },

    [&](const FolderData& dirLeft, const FolderData& dirRight) //both sides
    {
        FolderPair& newFolder = output.addSubFolder(dirLeft.first, dirLeft.second.first, DIR_EQUAL, dirRight.first, dirRight.second.first);
        const Zstringw* errorMsgNew = checkFailedRead(newFolder, errorMsg);

        if (!errorMsgNew)
            if (dirLeft.first != dirRight.first)
                newFolder.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(newFolder));

        mergeTwoSides(dirLeft.second.second, dirRight.second.second, errorMsgNew, newFolder); //recurse
    });
}

//-----------------------------------------------------------------------------------------------

//uncheck excluded directories (see parallelDeviceTraversal()) + remove superfluous excluded subdirectories
void stripExcludedDirectories(ContainerObject& hierObj, const HardFilter& filterProc)
{
    for (FolderPair& folder : hierObj.refSubFolders())
        stripExcludedDirectories(folder, filterProc);

    //remove superfluous directories:
    //   this does not invalidate "std::vector<FilePair*>& undefinedFiles", since we delete folders only
    //   and there is no side-effect for memory positions of FilePair and SymlinkPair thanks to std::list!
    static_assert(std::is_same_v<std::list<FolderPair>, ContainerObject::FolderList>);

    hierObj.refSubFolders().remove_if([&](FolderPair& folder)
    {
        const bool included = filterProc.passDirFilter(folder.getPairRelativePath(), nullptr); //childItemMightMatch is false, child items were already excluded during scanning

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
    cb_.reportStatus(_("Generating file list...")); //throw X
    cb_.forceUiRefresh(); //throw X

    auto getDirValue = [&](const AbstractPath& folderPath) -> const DirectoryValue*
    {
        auto it = directoryBuffer_.find({ folderPath, fpCfg.filter.nameFilter, fpCfg.handleSymlinks });
        return it != directoryBuffer_.end() ? &it->second : nullptr;
    };

    const DirectoryValue* bufValueLeft  = getDirValue(fp.folderPathLeft);
    const DirectoryValue* bufValueRight = getDirValue(fp.folderPathRight);

    std::map<Zstring, Zstringw, LessFilePath> failedReads; //base-relative paths or empty if read-error for whole base directory
    {
        auto append = [&](const std::map<Zstring, std::wstring, LessFilePath>& c)
        {
            for (const auto& item : c)
                failedReads.emplace(item.first, copyStringTo<Zstringw>(item.second));
        };
        //mix failedFolderReads with failedItemReads:
        //mark directory errors already at directory-level (instead for child items only) to show on GUI! See "MergeSides"
        //=> minor pessimization for "excludefilterFailedRead" which needlessly excludes parent folders, too
        if (bufValueLeft ) append(bufValueLeft ->failedFolderReads);
        if (bufValueRight) append(bufValueRight->failedFolderReads);

        if (bufValueLeft ) append(bufValueLeft ->failedItemReads);
        if (bufValueRight) append(bufValueRight->failedItemReads);
    }

    Zstring excludefilterFailedRead;
    if (failedReads.find(Zstring()) != failedReads.end()) //empty path if read-error for whole base directory
        excludefilterFailedRead += Zstr("*\n");
    else
        for (const auto& item : failedReads)
            excludefilterFailedRead += item.first + Zstr("\n"); //exclude item AND (potential) child items!

    std::shared_ptr<BaseFolderPair> output = std::make_shared<BaseFolderPair>(fp.folderPathLeft,
                                                                              bufValueLeft != nullptr, //dir existence must be checked only once: available iff buffer entry exists!
                                                                              fp.folderPathRight,
                                                                              bufValueRight != nullptr,
                                                                              fpCfg.filter.nameFilter->copyFilterAddingExclusion(excludefilterFailedRead),
                                                                              fpCfg.compareVar,
                                                                              fileTimeTolerance_,
                                                                              fpCfg.ignoreTimeShiftMinutes);

    //PERF_START;
    FolderContainer emptyFolderCont; //WTF!!! => using a temporary in the ternary conditional would implicitly call the FolderContainer copy-constructor!!!!!!
    MergeSides(failedReads, undefinedFiles, undefinedSymlinks).execute(bufValueLeft  ? bufValueLeft ->folderCont : emptyFolderCont,
                                                                       bufValueRight ? bufValueRight->folderCont : emptyFolderCont, *output);
    //PERF_STOP;

    //##################### in/exclude rows according to filtering #####################
    //NOTE: we need to finish de-activating rows BEFORE binary comparison is run so that it can skip them!

    //attention: some excluded directories are still in the comparison result! (see include filter handling!)
    if (!fpCfg.filter.nameFilter->isNull())
        stripExcludedDirectories(*output, *fpCfg.filter.nameFilter); //mark excluded directories (see parallelDeviceTraversal()) + remove superfluous excluded subdirectories

    //apply soft filtering (hard filter already applied during traversal!)
    addSoftFiltering(*output, fpCfg.filter.timeSizeFilter);

    //##################################################################################
    return output;
}
}


void fff::logNonDefaultSettings(const XmlGlobalSettings& activeSettings, ProcessCallback& callback)
{
    const XmlGlobalSettings defaultSettings;
    std::wstring changedSettingsMsg;

    if (activeSettings.failSafeFileCopy != defaultSettings.failSafeFileCopy)
        changedSettingsMsg += L"\n    " + _("Fail-safe file copy") + L" - " + (activeSettings.failSafeFileCopy ? _("Enabled") : _("Disabled"));

    if (activeSettings.copyLockedFiles != defaultSettings.copyLockedFiles)
        changedSettingsMsg += L"\n    " + _("Copy locked files") + L" - " + (activeSettings.copyLockedFiles ? _("Enabled") : _("Disabled"));

    if (activeSettings.copyFilePermissions != defaultSettings.copyFilePermissions)
        changedSettingsMsg += L"\n    " + _("Copy file access permissions") + L" - " + (activeSettings.copyFilePermissions ? _("Enabled") : _("Disabled"));

    if (activeSettings.fileTimeTolerance != defaultSettings.fileTimeTolerance)
        changedSettingsMsg += L"\n    " + _("File time tolerance") + L" - " + numberTo<std::wstring>(activeSettings.fileTimeTolerance);

    if (activeSettings.folderAccessTimeout != defaultSettings.folderAccessTimeout)
        changedSettingsMsg += L"\n    " + _("Folder access timeout") + L" - " + numberTo<std::wstring>(activeSettings.folderAccessTimeout);

    if (activeSettings.runWithBackgroundPriority != defaultSettings.runWithBackgroundPriority)
        changedSettingsMsg += L"\n    " + _("Run with background priority") + L" - " + (activeSettings.runWithBackgroundPriority ? _("Enabled") : _("Disabled"));

    if (activeSettings.createLockFile != defaultSettings.createLockFile)
        changedSettingsMsg += L"\n    " + _("Lock directories during sync") + L" - " + (activeSettings.createLockFile ? _("Enabled") : _("Disabled"));

    if (activeSettings.verifyFileCopy != defaultSettings.verifyFileCopy)
        changedSettingsMsg += L"\n    " + _("Verify copied files") + L" - " + (activeSettings.verifyFileCopy ? _("Enabled") : _("Disabled"));

    if (!changedSettingsMsg.empty())
        callback.reportInfo(_("Using non-default global settings:") + changedSettingsMsg); //throw X
}


FolderComparison fff::compare(WarningDialogs& warnings,
                              int fileTimeTolerance,
                              bool allowUserInteraction,
                              bool runWithBackgroundPriority,
                              int folderAccessTimeout,
                              bool createDirLocks,
                              std::unique_ptr<LockHolder>& dirLocks,
                              const std::vector<FolderPairCfg>& fpCfgList,
                              const std::map<AbstractPath, size_t>& deviceParallelOps,
                              ProcessCallback& callback)
{
    //PERF_START;

    //indicator at the very beginning of the log to make sense of "total time"
    //init process: keep at beginning so that all gui elements are initialized properly
    callback.initNewPhase(-1, 0, ProcessCallback::PHASE_SCANNING); //throw X; it's unknown how many files will be scanned => -1 objects
    //callback.reportInfo(Comparison started")); -> still useful?

    //-------------------------------------------------------------------------------

    //specify process and resource handling priorities
    std::unique_ptr<ScheduleForBackgroundProcessing> backgroundPrio;
    if (runWithBackgroundPriority)
        try
        {
            backgroundPrio = std::make_unique<ScheduleForBackgroundProcessing>(); //throw FileError
        }
        catch (const FileError& e) //not an error in this context
        {
            callback.reportInfo(e.toString()); //throw X
        }

    //prevent operating system going into sleep state
    std::unique_ptr<PreventStandby> noStandby;
    try
    {
        noStandby = std::make_unique<PreventStandby>(); //throw FileError
    }
    catch (const FileError& e) //not an error in this context
    {
        callback.reportInfo(e.toString()); //throw X
    }

    const ResolvedBaseFolders& resInfo = initializeBaseFolders(fpCfgList, deviceParallelOps, folderAccessTimeout, allowUserInteraction, warnings.warnFolderNotExisting, callback); //throw X
    //directory existence only checked *once* to avoid race conditions!
    if (resInfo.resolvedPairs.size() != fpCfgList.size())
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    auto basefolderExisting = [&](const AbstractPath& folderPath) { return resInfo.existingBaseFolders.find(folderPath) != resInfo.existingBaseFolders.end(); };


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

    //check whether one side is a sub directory of the other side (folder-pair-wise!)
    //similar check (warnDependentBaseFolders) if one directory is read/written by multiple pairs not before beginning of synchronization
    {
        std::wstring msg;

        for (const auto& w : workLoad)
            if (Opt<PathDependency> pd = getPathDependency(w.first.folderPathLeft,  *w.second.filter.nameFilter,
                                                           w.first.folderPathRight, *w.second.filter.nameFilter))
            {
                msg += L"\n\n" +
                       AFS::getDisplayPath(w.first.folderPathLeft) + L"\n" +
                       AFS::getDisplayPath(w.first.folderPathRight);
                if (!pd->relPath.empty())
                    msg += L"\n" + _("Exclude:") + L" " + utfTo<std::wstring>(FILE_NAME_SEPARATOR + pd->relPath + FILE_NAME_SEPARATOR);
            }

        if (!msg.empty())
            callback.reportWarning(_("One base folder of a folder pair is contained in the other one.") + L"\n" + //throw X
                                   _("The folder should be excluded from synchronization via filter.") + msg, warnings.warnDependentFolderPair);
    }

    //-------------------end of basic checks------------------------------------------

    //lock (existing) directories before comparison
    if (createDirLocks)
    {
        std::set<Zstring, LessFilePath> dirPathsExisting;
        for (const AbstractPath& folderPath : resInfo.existingBaseFolders)
            if (Opt<Zstring> nativePath = AFS::getNativeItemPath(folderPath)) //restrict directory locking to native paths until further
                dirPathsExisting.insert(*nativePath);

        dirLocks = std::make_unique<LockHolder>(dirPathsExisting, warnings.warnDirectoryLockFailed, callback);
    }

    try
    {
        //------------------- fill directory buffer ---------------------------------------------------
        std::set<DirectoryKey> foldersToRead;

        for (const auto& w : workLoad)
        {
            if (basefolderExisting(w.first.folderPathLeft)) //only traverse *currently existing* folders: at this point user is aware that non-ex + empty string are seen as empty folder!
                foldersToRead.emplace(DirectoryKey({ w.first.folderPathLeft, w.second.filter.nameFilter, w.second.handleSymlinks }));
            if (basefolderExisting(w.first.folderPathRight))
                foldersToRead.emplace(DirectoryKey({ w.first.folderPathRight, w.second.filter.nameFilter, w.second.handleSymlinks }));
        }

        FolderComparison output;

        //reduce peak memory by restricting lifetime of ComparisonBuffer to have ended when loading potentially huge InSyncFolder instance in redetermineSyncDirection()
        {
            //------------ traverse/read folders -----------------------------------------------------
            //PERF_START;
            ComparisonBuffer cmpBuff(foldersToRead, deviceParallelOps, fileTimeTolerance, callback);
            //PERF_STOP;

            //process binary comparison as one junk
            std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>> workLoadByContent;
            for (const auto& w : workLoad)
                if (w.second.compareVar == CompareVariant::CONTENT)
                    workLoadByContent.push_back(w);

            std::list<std::shared_ptr<BaseFolderPair>> outputByContent = cmpBuff.compareByContent(workLoadByContent);

            //write output in expected order
            for (const auto& w : workLoad)
                switch (w.second.compareVar)
                {
                    case CompareVariant::TIME_SIZE:
                        output.push_back(cmpBuff.compareByTimeSize(w.first, w.second));
                        break;
                    case CompareVariant::SIZE:
                        output.push_back(cmpBuff.compareBySize(w.first, w.second));
                        break;
                    case CompareVariant::CONTENT:
                        assert(!outputByContent.empty());
                        if (!outputByContent.empty())
                        {
                            output.push_back(outputByContent.front());
                            /**/             outputByContent.pop_front();
                        }
                        break;
                }
        }
        assert(output.size() == fpCfgList.size());

        //--------- set initial sync-direction --------------------------------------------------

        for (auto it = begin(output); it != end(output); ++it)
        {
            const FolderPairCfg& fpCfg = fpCfgList[it - output.begin()];

            callback.reportStatus(_("Calculating sync directions...")); //throw X
            callback.forceUiRefresh(); //throw X

            tryReportingError([&]
            {
                redetermineSyncDirection(fpCfg.directionCfg, *it, //throw FileError
                [&](const std::wstring& msg) { callback.reportStatus(msg); }); //throw X

            }, callback); //throw X
        }

        return output;
    }
    catch (const std::bad_alloc& e)
    {
        callback.reportFatalError(_("Out of memory.") + L" " + utfTo<std::wstring>(e.what()));
        //we need to maintain the "output.size() == fpCfgList.size()" contract in ALL cases! => abort
        callback.abortProcessNow(); //throw X
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
    }
}

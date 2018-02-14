// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "comparison.h"
#include <zen/process_priority.h>
#include <zen/perf.h>
#include "algorithm.h"
#include "lib/parallel_scan.h"
#include "lib/dir_exist_async.h"
#include "lib/binary.h"
#include "lib/cmp_filetime.h"
#include "lib/status_handler_impl.h"
#include "fs/concrete.h"

using namespace zen;
using namespace fff;


std::vector<FolderPairCfg> fff::extractCompareCfg(const MainConfiguration& mainCfg)
{
    //merge first and additional pairs
    std::vector<FolderPairEnh> allPairs = { mainCfg.firstPair };
    append(allPairs, mainCfg.additionalPairs);

    std::vector<FolderPairCfg> output;
    std::transform(allPairs.begin(), allPairs.end(), std::back_inserter(output),
                   [&](const FolderPairEnh& enhPair) -> FolderPairCfg
    {
        return FolderPairCfg(enhPair.folderPathPhraseLeft_, enhPair.folderPathPhraseRight_,
                             enhPair.altCmpConfig.get() ? enhPair.altCmpConfig->compareVar       : mainCfg.cmpConfig.compareVar,
                             enhPair.altCmpConfig.get() ? enhPair.altCmpConfig->handleSymlinks   : mainCfg.cmpConfig.handleSymlinks,
                             enhPair.altCmpConfig.get() ? enhPair.altCmpConfig->ignoreTimeShiftMinutes : mainCfg.cmpConfig.ignoreTimeShiftMinutes,

                             normalizeFilters(mainCfg.globalFilter, enhPair.localFilter),

                             enhPair.altSyncConfig.get() ? enhPair.altSyncConfig->directionCfg : mainCfg.syncCfg.directionCfg);
    });
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
    std::set<AbstractPath, AFS::LessAbstractPath> existingBaseFolders;
};


ResolvedBaseFolders initializeBaseFolders(const std::vector<FolderPairCfg>& cfgList,
                                          int folderAccessTimeout,
                                          bool allowUserInteraction,
                                          ProcessCallback& callback)
{
    ResolvedBaseFolders output;

    tryReportingError([&]
    {
        std::set<AbstractPath, AFS::LessAbstractPath> uniqueBaseFolders;

        //support "retry" for environment variable and and variable driver letter resolution!
        output.resolvedPairs.clear();
        for (const FolderPairCfg& fpCfg : cfgList)
        {
            AbstractPath folderPathLeft  = createAbstractPath(fpCfg.folderPathPhraseLeft_);
            AbstractPath folderPathRight = createAbstractPath(fpCfg.folderPathPhraseRight_);

            uniqueBaseFolders.insert(folderPathLeft);
            uniqueBaseFolders.insert(folderPathRight);

            output.resolvedPairs.push_back({ folderPathLeft, folderPathRight });
        }

        const FolderStatus status = getFolderStatusNonBlocking(uniqueBaseFolders, folderAccessTimeout, allowUserInteraction, callback); //re-check *all* directories on each try!
        output.existingBaseFolders = status.existing;

        if (!status.notExisting.empty() || !status.failedChecks.empty())
        {
            std::wstring msg = _("Cannot find the following folders:") + L"\n";

            for (const AbstractPath& folderPath : status.notExisting)
                msg += L"\n" + AFS::getDisplayPath(folderPath);

            for (const auto& fc : status.failedChecks)
                msg += L"\n" + AFS::getDisplayPath(fc.first);

            msg += L"\n\n";
            msg +=  _("If this error is ignored the folders will be considered empty. Missing folders are created automatically when needed.");

            if (!status.failedChecks.empty())
            {
                msg += L"\n___________________________________________";
                for (const auto& fc : status.failedChecks)
                    msg += L"\n\n" + replaceCpy(fc.second.toString(), L"\n\n", L"\n");
            }

            throw FileError(msg);
        }
    }, callback); //throw X?

    return output;
}

//#############################################################################################################################

class ComparisonBuffer
{
public:
    ComparisonBuffer(const std::set<DirectoryKey>& keysToRead, int fileTimeTolerance, ProcessCallback& callback);

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
    ProcessCallback& callback_;
};


ComparisonBuffer::ComparisonBuffer(const std::set<DirectoryKey>& keysToRead, int fileTimeTolerance, ProcessCallback& callback) :
    fileTimeTolerance_(fileTimeTolerance), callback_(callback)
{
    class CbImpl : public FillBufferCallback
    {
    public:
        CbImpl(ProcessCallback& pcb) : callback_(pcb) {}

        void reportStatus(const std::wstring& statusMsg, int itemsTotal) override
        {
            callback_.updateProcessedData(itemsTotal - itemsReported_, 0); //processed bytes are reported in subfunctions!
            itemsReported_ = itemsTotal;

            callback_.reportStatus(statusMsg); //may throw
        }

        HandleError reportError(const std::wstring& msg, size_t retryNumber) override
        {
            switch (callback_.reportError(msg, retryNumber))
            {
                case ProcessCallback::IGNORE_ERROR:
                    return ON_ERROR_CONTINUE;

                case ProcessCallback::RETRY:
                    return ON_ERROR_RETRY;
            }

            assert(false);
            return ON_ERROR_CONTINUE;
        }

        int getItemsTotal() const { return itemsReported_; }

    private:
        ProcessCallback& callback_;
        int itemsReported_ = 0;
    } cb(callback);

    fillBuffer(keysToRead, //in
               directoryBuffer_, //out
               cb,
               UI_UPDATE_INTERVAL / 2); //every ~50 ms

    callback.reportInfo(_("Comparison finished:") + L" " + _P("1 item found", "%x items found", cb.getItemsTotal()));
}


//--------------------assemble conflict descriptions---------------------------

//const wchar_t arrowLeft [] = L"\u2190";
//const wchar_t arrowRight[] = L"\u2192"; unicode arrows -> too small
const wchar_t arrowLeft [] = L"<-";
const wchar_t arrowRight[] = L"->";

//NOTE: conflict texts are NOT expected to contain additional path info (already implicit through associated item!)
//      => only add path info if information is relevant, e.g. conflict is specific to left/right side only

template <SelectedSide side, class FileOrLinkPair> inline
std::wstring getConflictInvalidDate(const FileOrLinkPair& file)
{
    return replaceCpy(_("File %x has an invalid date."), L"%x", fmtPath(AFS::getDisplayPath(file.template getAbstractPath<side>()))) + L"\n" +
           _("Date:") + L" " + formatUtcToLocalTime(file.template getLastWriteTime<side>());
}


std::wstring getConflictSameDateDiffSize(const FilePair& file)
{
    return _("Files have the same date but a different size.") + L"\n" +
           arrowLeft  + L" " + _("Date:") + L" " + formatUtcToLocalTime(file.getLastWriteTime< LEFT_SIDE>()) + L"    " + _("Size:") + L" " + formatNumber(file.getFileSize<LEFT_SIDE>()) + L"\n" +
           arrowRight + L" " + _("Date:") + L" " + formatUtcToLocalTime(file.getLastWriteTime<RIGHT_SIDE>()) + L"    " + _("Size:") + L" " + formatNumber(file.getFileSize<RIGHT_SIDE>());
}


std::wstring getConflictSkippedBinaryComparison(const FilePair& file)
{
    return _("Content comparison was skipped for excluded files.");
}


std::wstring getDescrDiffMetaShortnameCase(const FileSystemObject& fsObj)
{
    return _("Items differ in attributes only") + L"\n" +
           arrowLeft  + L" " + fmtPath(fsObj.getItemName< LEFT_SIDE>()) + L"\n" +
           arrowRight + L" " + fmtPath(fsObj.getItemName<RIGHT_SIDE>());
}


template <class FileOrLinkPair>
std::wstring getDescrDiffMetaDate(const FileOrLinkPair& file)
{
    return _("Items differ in attributes only") + L"\n" +
           arrowLeft  + L" " + _("Date:") + L" " + formatUtcToLocalTime(file.template getLastWriteTime< LEFT_SIDE>()) + L"\n" +
           arrowRight + L" " + _("Date:") + L" " + formatUtcToLocalTime(file.template getLastWriteTime<RIGHT_SIDE>());
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


void categorizeSymlinkByContent(SymlinkPair& symlink, ProcessCallback& callback)
{
    //categorize symlinks that exist on both sides
    std::string binaryContentL;
    std::string binaryContentR;
    Opt<std::wstring> errMsg = tryReportingError([&]
    {
        callback.reportStatus(replaceCpy(_("Resolving symbolic link %x"), L"%x", fmtPath(AFS::getDisplayPath(symlink.getAbstractPath<LEFT_SIDE>()))));
        binaryContentL = AFS::getSymlinkBinaryContent(symlink.getAbstractPath<LEFT_SIDE>()); //throw FileError

        callback.reportStatus(replaceCpy(_("Resolving symbolic link %x"), L"%x", fmtPath(AFS::getDisplayPath(symlink.getAbstractPath<RIGHT_SIDE>()))));
        binaryContentR = AFS::getSymlinkBinaryContent(symlink.getAbstractPath<RIGHT_SIDE>()); //throw FileError
    }, callback); //throw X?

    if (errMsg)
        symlink.setCategoryConflict(*errMsg);
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


std::shared_ptr<BaseFolderPair> ComparisonBuffer::compareBySize(const ResolvedFolderPair& fp, const FolderPairCfg& fpConfig) const
{
    //do basis scan and retrieve files existing on both sides as "compareCandidates"
    std::vector<FilePair*> uncategorizedFiles;
    std::vector<SymlinkPair*> uncategorizedLinks;
    std::shared_ptr<BaseFolderPair> output = performComparison(fp, fpConfig, uncategorizedFiles, uncategorizedLinks);

    //finish symlink categorization
    for (SymlinkPair* symlink : uncategorizedLinks)
        categorizeSymlinkByContent(*symlink, callback_); //"compare by size" has the semantics of a quick content-comparison!
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


std::list<std::shared_ptr<BaseFolderPair>> ComparisonBuffer::compareByContent(const std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>>& workLoad) const
{
    std::list<std::shared_ptr<BaseFolderPair>> output;
    if (workLoad.empty())
        return output;

    //PERF_START;
    std::vector<FilePair*> filesToCompareBytewise;

    //process folder pairs one after another
    for (const auto& w : workLoad)
    {
        std::vector<FilePair*> undefinedFiles;
        std::vector<SymlinkPair*> uncategorizedLinks;
        //do basis scan and retrieve candidates for binary comparison (files existing on both sides)

        output.push_back(performComparison(w.first, w.second, undefinedFiles, uncategorizedLinks));

        //content comparison of file content happens AFTER finding corresponding files and AFTER filtering
        //in order to separate into two processes (scanning and comparing)
        for (FilePair* file : undefinedFiles)
            //pre-check: files have different content if they have a different filesize (must not be FILE_EQUAL: see InSyncFile)
            if (file->getFileSize<LEFT_SIDE>() != file->getFileSize<RIGHT_SIDE>())
                file->setCategory<FILE_DIFFERENT_CONTENT>();
            else
            {
                //perf: skip binary comparison for excluded rows (e.g. via time span and size filter)!
                //both soft and hard filter were already applied in ComparisonBuffer::performComparison()!
                if (!file->isActive())
                    file->setCategoryConflict(getConflictSkippedBinaryComparison(*file));
                else
                    filesToCompareBytewise.push_back(file);
            }

        //finish symlink categorization
        for (SymlinkPair* symlink : uncategorizedLinks)
            categorizeSymlinkByContent(*symlink, callback_);
    }

    //finish categorization...
    const int itemsTotal = static_cast<int>(filesToCompareBytewise.size());

    uint64_t bytesTotal = 0; //left and right filesizes are equal
    for (FilePair* file : filesToCompareBytewise)
        bytesTotal += file->getFileSize<LEFT_SIDE>();

    callback_.initNewPhase(itemsTotal, bytesTotal, ProcessCallback::PHASE_COMPARING_CONTENT); //may throw

    const std::wstring txtComparingContentOfFiles = _("Comparing content of files %x");

    //PERF_START;

    //compare files (that have same size) bytewise...
    for (FilePair* file : filesToCompareBytewise)
    {
        callback_.reportStatus(replaceCpy(txtComparingContentOfFiles, L"%x", fmtPath(file->getPairRelativePath())));

        //check files that exist in left and right model but have different content

        bool haveSameContent = false;
        Opt<std::wstring> errMsg = tryReportingError([&]
        {
            StatisticsReporter statReporter(1, file->getFileSize<LEFT_SIDE>(), callback_);

            auto notifyUnbufferedIO = [&](int64_t bytesDelta) { statReporter.reportDelta(0, bytesDelta); };

            haveSameContent = filesHaveSameContent(file->getAbstractPath<LEFT_SIDE>(),
                                                   file->getAbstractPath<RIGHT_SIDE>(), notifyUnbufferedIO); //throw FileError
            statReporter.reportDelta(1, 0);
        }, callback_); //throw X?

        if (errMsg)
            file->setCategoryConflict(*errMsg);
        else
        {
            if (haveSameContent)
            {
                //Caveat:
                //1. FILE_EQUAL may only be set if short names match in case: InSyncFolder's mapping tables use short name as a key! see db_file.cpp
                //2. FILE_EQUAL is expected to mean identical file sizes! See InSyncFile
                //3. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::setSyncedTo() in file_hierarchy.h
                if (file->getItemName<LEFT_SIDE>() != file->getItemName<RIGHT_SIDE>())
                    file->setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(*file));
#if 0 //don't synchronize modtime only see SynchronizeFolderPair::synchronizeFileInt(), SO_COPY_METADATA_TO_*
                else if (!sameFileTime(file->getLastWriteTime<LEFT_SIDE>(),
                                       file->getLastWriteTime<RIGHT_SIDE>(), file->base().getFileTimeTolerance(), file->base().getIgnoredTimeShift()))
                    file->setCategoryDiffMetadata(getDescrDiffMetaDate(*file));
#endif
                else
                    file->setCategory<FILE_EQUAL>();
            }
            else
                file->setCategory<FILE_DIFFERENT_CONTENT>();
        }
    }
    return output;
}

//-----------------------------------------------------------------------------------------------

class MergeSides
{
public:
    MergeSides(const std::map<Zstring, std::wstring, LessFilePath>& failedItemReads,
               std::vector<FilePair*>& undefinedFilesOut,
               std::vector<SymlinkPair*>& undefinedSymlinksOut) :
        failedItemReads_(failedItemReads),
        undefinedFiles(undefinedFilesOut),
        undefinedSymlinks(undefinedSymlinksOut) {}

    void execute(const FolderContainer& lhs, const FolderContainer& rhs, ContainerObject& output)
    {
        auto it = failedItemReads_.find(Zstring()); //empty path if read-error for whole base directory

        mergeTwoSides(lhs, rhs,
                      it != failedItemReads_.end() ? &it->second : nullptr,
                      output);
    }

private:
    void mergeTwoSides(const FolderContainer& lhs, const FolderContainer& rhs, const std::wstring* errorMsg, ContainerObject& output);

    template <SelectedSide side>
    void fillOneSide(const FolderContainer& folderCont, const std::wstring* errorMsg, ContainerObject& output);

    const std::wstring* checkFailedRead(FileSystemObject& fsObj, const std::wstring* errorMsg);

    const std::map<Zstring, std::wstring, LessFilePath>& failedItemReads_; //base-relative paths or empty if read-error for whole base directory
    std::vector<FilePair*>& undefinedFiles;
    std::vector<SymlinkPair*>& undefinedSymlinks;
};


inline
const std::wstring* MergeSides::checkFailedRead(FileSystemObject& fsObj, const std::wstring* errorMsg)
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
        fsObj.setCategoryConflict(*errorMsg);
    }
    return errorMsg;
}


template <SelectedSide side>
void MergeSides::fillOneSide(const FolderContainer& folderCont, const std::wstring* errorMsg, ContainerObject& output)
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
        const std::wstring* errorMsgNew = checkFailedRead(newFolder, errorMsg);
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


void MergeSides::mergeTwoSides(const FolderContainer& lhs, const FolderContainer& rhs, const std::wstring* errorMsg, ContainerObject& output)
{
    using FileData = const FolderContainer::FileList::value_type;

    linearMerge(lhs.files, rhs.files,
    [&](const FileData& fileLeft ) { FilePair& newItem = output.addSubFile< LEFT_SIDE>(fileLeft .first, fileLeft .second); checkFailedRead(newItem, errorMsg); }, //left only
    [&](const FileData& fileRight) { FilePair& newItem = output.addSubFile<RIGHT_SIDE>(fileRight.first, fileRight.second); checkFailedRead(newItem, errorMsg); }, //right only

    [&](const FileData& fileLeft, const FileData& fileRight) //both sides
    {
        FilePair& newItem = output.addSubFile(fileLeft.first,
                                              fileLeft.second,
                                              FILE_EQUAL, //dummy-value until categorization is finished later
                                              fileRight.first,
                                              fileRight.second);
        if (!checkFailedRead(newItem, errorMsg))
            undefinedFiles.push_back(&newItem);
        static_assert(IsSameType<ContainerObject::FileList, FixedList<FilePair>>::value, ""); //ContainerObject::addSubFile() must NOT invalidate references used in "undefinedFiles"!
    });

    //-----------------------------------------------------------------------------------------------
    using SymlinkData = const FolderContainer::SymlinkList::value_type;

    linearMerge(lhs.symlinks, rhs.symlinks,
    [&](const SymlinkData& symlinkLeft ) { SymlinkPair& newItem = output.addSubLink< LEFT_SIDE>(symlinkLeft .first, symlinkLeft .second); checkFailedRead(newItem, errorMsg); }, //left only
    [&](const SymlinkData& symlinkRight) { SymlinkPair& newItem = output.addSubLink<RIGHT_SIDE>(symlinkRight.first, symlinkRight.second); checkFailedRead(newItem, errorMsg); }, //right only

    [&](const SymlinkData& symlinkLeft, const SymlinkData& symlinkRight) //both sides
    {
        SymlinkPair& newItem = output.addSubLink(symlinkLeft.first,
                                                 symlinkLeft.second,
                                                 SYMLINK_EQUAL, //dummy-value until categorization is finished later
                                                 symlinkRight.first,
                                                 symlinkRight.second);
        if (!checkFailedRead(newItem, errorMsg))
            undefinedSymlinks.push_back(&newItem);
    });

    //-----------------------------------------------------------------------------------------------
    using FolderData = const FolderContainer::FolderList::value_type;

    linearMerge(lhs.folders, rhs.folders,
                [&](const FolderData& dirLeft) //left only
    {
        FolderPair& newFolder = output.addSubFolder<LEFT_SIDE>(dirLeft.first, dirLeft.second.first);
        const std::wstring* errorMsgNew = checkFailedRead(newFolder, errorMsg);
        this->fillOneSide<LEFT_SIDE>(dirLeft.second.second, errorMsgNew, newFolder); //recurse
    },
    [&](const FolderData& dirRight) //right only
    {
        FolderPair& newFolder = output.addSubFolder<RIGHT_SIDE>(dirRight.first, dirRight.second.first);
        const std::wstring* errorMsgNew = checkFailedRead(newFolder, errorMsg);
        this->fillOneSide<RIGHT_SIDE>(dirRight.second.second, errorMsgNew, newFolder); //recurse
    },

    [&](const FolderData& dirLeft, const FolderData& dirRight) //both sides
    {
        FolderPair& newFolder = output.addSubFolder(dirLeft.first, dirLeft.second.first, DIR_EQUAL, dirRight.first, dirRight.second.first);
        const std::wstring* errorMsgNew = checkFailedRead(newFolder, errorMsg);

        if (!errorMsgNew)
            if (dirLeft.first != dirRight.first)
                newFolder.setCategoryDiffMetadata(getDescrDiffMetaShortnameCase(newFolder));

        mergeTwoSides(dirLeft.second.second, dirRight.second.second, errorMsgNew, newFolder); //recurse
    });
}

//-----------------------------------------------------------------------------------------------

//uncheck excluded directories (see fillBuffer()) + remove superfluous excluded subdirectories
void stripExcludedDirectories(ContainerObject& hierObj, const HardFilter& filterProc)
{
    for (FolderPair& folder : hierObj.refSubFolders())
        stripExcludedDirectories(folder, filterProc);

    //remove superfluous directories:
    //   this does not invalidate "std::vector<FilePair*>& undefinedFiles", since we delete folders only
    //   and there is no side-effect for memory positions of FilePair and SymlinkPair thanks to zen::FixedList!
    static_assert(IsSameType<FixedList<FolderPair>, ContainerObject::FolderList>::value, "");

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
    callback_.reportStatus(_("Generating file list..."));
    callback_.forceUiRefresh(); //throw X

    auto getDirValue = [&](const AbstractPath& folderPath) -> const DirectoryValue*
    {
        auto it = directoryBuffer_.find({ folderPath, fpCfg.filter.nameFilter, fpCfg.handleSymlinks });
        return it != directoryBuffer_.end() ? &it->second : nullptr;
    };

    const DirectoryValue* bufValueLeft  = getDirValue(fp.folderPathLeft);
    const DirectoryValue* bufValueRight = getDirValue(fp.folderPathRight);

    std::map<Zstring, std::wstring, LessFilePath> failedReads; //base-relative paths or empty if read-error for whole base directory
    {
        //mix failedFolderReads with failedItemReads:
        //mark directory errors already at directory-level (instead for child items only) to show on GUI! See "MergeSides"
        //=> minor pessimization for "excludefilterFailedRead" which needlessly excludes parent folders, too
        if (bufValueLeft ) append(failedReads, bufValueLeft ->failedFolderReads);
        if (bufValueRight) append(failedReads, bufValueRight->failedFolderReads);

        if (bufValueLeft ) append(failedReads, bufValueLeft ->failedItemReads);
        if (bufValueRight) append(failedReads, bufValueRight->failedItemReads);
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
        stripExcludedDirectories(*output, *fpCfg.filter.nameFilter); //mark excluded directories (see fillBuffer()) + remove superfluous excluded subdirectories

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
        callback.reportInfo(_("Using non-default global settings:") + changedSettingsMsg);
}


FolderComparison fff::compare(WarningDialogs& warnings,
                              int fileTimeTolerance,
                              bool allowUserInteraction,
                              bool runWithBackgroundPriority,
                              int folderAccessTimeout,
                              bool createDirLocks,
                              std::unique_ptr<LockHolder>& dirLocks,
                              const std::vector<FolderPairCfg>& cfgList,
                              ProcessCallback& callback)
{
    //PERF_START;

    //indicator at the very beginning of the log to make sense of "total time"
    //init process: keep at beginning so that all gui elements are initialized properly
    callback.initNewPhase(-1, 0, ProcessCallback::PHASE_SCANNING); //may throw; it's unknown how many files will be scanned => -1 objects
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
            callback.reportInfo(e.toString()); //may throw!
        }

    //prevent operating system going into sleep state
    std::unique_ptr<PreventStandby> noStandby;
    try
    {
        noStandby = std::make_unique<PreventStandby>(); //throw FileError
    }
    catch (const FileError& e) //not an error in this context
    {
        callback.reportInfo(e.toString()); //may throw!
    }

    const ResolvedBaseFolders& resInfo = initializeBaseFolders(cfgList, folderAccessTimeout, allowUserInteraction, callback);
    //directory existence only checked *once* to avoid race conditions!
    if (resInfo.resolvedPairs.size() != cfgList.size())
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    auto basefolderExisting = [&](const AbstractPath& folderPath) { return resInfo.existingBaseFolders.find(folderPath) != resInfo.existingBaseFolders.end(); };


    std::vector<std::pair<ResolvedFolderPair, FolderPairCfg>> workLoad;
    for (size_t i = 0; i < cfgList.size(); ++i)
        workLoad.emplace_back(resInfo.resolvedPairs[i], cfgList[i]);

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
            callback.reportWarning(_("One base folder of a folder pair is contained in the other one.") + L"\n" +
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
        std::set<DirectoryKey> dirsToRead;

        for (const auto& w : workLoad)
        {
            if (basefolderExisting(w.first.folderPathLeft)) //only traverse *currently existing* folders: at this point user is aware that non-ex + empty string are seen as empty folder!
                dirsToRead.insert({ w.first.folderPathLeft,  w.second.filter.nameFilter, w.second.handleSymlinks });
            if (basefolderExisting(w.first.folderPathRight))
                dirsToRead.insert({ w.first.folderPathRight, w.second.filter.nameFilter, w.second.handleSymlinks });
        }

        FolderComparison output;

        //reduce peak memory by restricting lifetime of ComparisonBuffer to have ended when loading potentially huge InSyncFolder instance in redetermineSyncDirection()
        {
            //------------ traverse/read folders -----------------------------------------------------
            //PERF_START;
            ComparisonBuffer cmpBuff(dirsToRead, fileTimeTolerance, callback);
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
                            outputByContent.pop_front();
                        }
                        break;
                }
        }
        assert(output.size() == cfgList.size());

        //--------- set initial sync-direction --------------------------------------------------

        for (auto it = begin(output); it != end(output); ++it)
        {
            const FolderPairCfg& fpCfg = cfgList[it - output.begin()];

            callback.reportStatus(_("Calculating sync directions..."));
            callback.forceUiRefresh(); //throw X

            tryReportingError([&]
            {
                redetermineSyncDirection(fpCfg.directionCfg, *it, //throw FileError
                [&](const std::wstring& msg) { callback.reportStatus(msg); }); //throw X

            }, callback); //throw X?
        }

        return output;
    }
    catch (const std::bad_alloc& e)
    {
        callback.reportFatalError(_("Out of memory.") + L" " + utfTo<std::wstring>(e.what()));
        //we need to maintain the "output.size() == cfgList.size()" contract in ALL cases! => abort
        callback.abortProcessNow(); //throw X
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
    }
}

#include "versioning.h"
#include "parallel_scan.h"
#include "status_handler_impl.h"

using namespace zen;
using namespace fff;


namespace
{
inline
Zstring getDotExtension(const Zstring& filePath) //including "." if extension is existing, returns empty string otherwise
{
    //const Zstring& extension = getFileExtension(filePath);
    //return extension.empty() ? extension : Zstr('.') + extension;

    auto it = find_last(filePath.begin(), filePath.end(), FILE_NAME_SEPARATOR);
    if (it == filePath.end())
        it = filePath.begin();
    else
        ++it;

    return Zstring(find_last(it, filePath.end(), Zstr('.')), filePath.end());
};
}


//e.g. "Sample.txt 2012-05-15 131513.txt"
//or       "Sample 2012-05-15 131513"
std::pair<time_t, Zstring> fff::impl::parseVersionedFileName(const Zstring& fileName)
{
    const StringRef<const Zchar> ext(find_last(fileName.begin(), fileName.end(), Zstr('.')), fileName.end());

    if (fileName.size() < 2 * ext.length() + 18)
        return {};

    const auto itExt1 = fileName.end() - (2 * ext.length() + 18);
    const auto itTs   = itExt1 + ext.length();
    if (!strEqual(ext, StringRef<const Zchar>(itExt1, itTs), CmpFilePath()))
        return {};

    const TimeComp tc = parseTime(Zstr(" %Y-%m-%d %H%M%S"), StringRef<const Zchar>(itTs, itTs + 18)); //returns TimeComp() on error
    const time_t t = localToTimeT(tc); //returns -1 on error
    if (t == -1)
        return {};

    Zstring fileNameOrig(fileName.begin(), itTs);
    if (fileNameOrig.empty())
        return {};

    return { t, std::move(fileNameOrig) };
}


//e.g. "2012-05-15 131513"
time_t fff::impl::parseVersionedFolderName(const Zstring& folderName)
{
    const TimeComp tc = parseTime(Zstr("%Y-%m-%d %H%M%S"), folderName); //returns TimeComp() on error
    const time_t t = localToTimeT(tc); //returns -1 on error
    if (t == -1)
        return 0;

    return t;
}


AbstractPath FileVersioner::generateVersionedPath(const Zstring& relativePath) const
{
    assert(isValidRelPath(relativePath));
    assert(!relativePath.empty());

    Zstring versionedRelPath;
    switch (versioningStyle_)
    {
        case VersioningStyle::REPLACE:
            versionedRelPath = relativePath;
            break;
        case VersioningStyle::TIMESTAMP_FOLDER:
            versionedRelPath = timeStamp_ + FILE_NAME_SEPARATOR + relativePath;
            break;
        case VersioningStyle::TIMESTAMP_FILE: //assemble time-stamped version name
            versionedRelPath = relativePath + Zstr(' ') + timeStamp_ + getDotExtension(relativePath);
            assert(impl::parseVersionedFileName(versionedRelPath) == std::pair(syncStartTime_, relativePath));
            (void)syncStartTime_; //silence clang's "unused variable" arning
            break;
    }
    return AFS::appendRelPath(versioningFolderPath_, versionedRelPath);
}


namespace
{
/*
move source to target across volumes:
- source is expected to exist
- if target already exists, it is overwritten, unless it is of a different type, e.g. a directory!
- target parent directories are created if missing
*/
template <class Function>
void moveExistingItemToVersioning(const AbstractPath& sourcePath, const AbstractPath& targetPath, //throw FileError
                                  Function copyNewItemPlain /*throw FileError*/)
{
    //start deleting existing target as required by copyFileTransactional()/renameItem():
    //best amortized performance if "target existing" is the most common case
    std::exception_ptr deletionError;
    try { AFS::removeFilePlain(targetPath); /*throw FileError*/ }
    catch (FileError&) { deletionError = std::current_exception(); } //probably "not existing" error, defer evaluation
    //overwrite AFS::ItemType::FOLDER with FILE? => highly dubious, do not allow

    auto fixTargetPathIssues = [&](const FileError& prevEx) //throw FileError
    {
        Opt<AFS::PathStatus> psTmp;
        try { psTmp = AFS::getPathStatus(targetPath); /*throw FileError*/ }
        catch (const FileError& e) { throw FileError(prevEx.toString(), e.toString()); }
        const AFS::PathStatus& ps = *psTmp;
        //previous exception contains context-level information, but current exception is the immediate problem => combine both
        //=> e.g. prevEx might be about missing parent folder; FFS considers session faulty and tries to create a new one,
        //which might fail with: LIBSSH2_ERROR_AUTHENTICATION_FAILED (due to limit on #sessions?) https://freefilesync.org/forum/viewtopic.php?t=4765#p16016

        if (ps.relPath.empty()) //already existing
        {
            if (deletionError)
                std::rethrow_exception(deletionError);
            throw prevEx; //yes, slicing, but not relevant here
        }

        //parent folder missing  => create + retry
        //parent folder existing => maybe created shortly after move attempt by parallel thread! => retry
        AbstractPath intermediatePath = ps.existingPath;

        std::for_each(ps.relPath.begin(), ps.relPath.end() - 1, [&](const Zstring& itemName)
        {
            try
            {
                AFS::createFolderPlain(intermediatePath = AFS::appendRelPath(intermediatePath, itemName)); //throw FileError
            }
            catch (FileError&)
            {
                try //already existing => possible, if moveExistingItemToVersioning() is run in parallel
                {
                    if (AFS::getItemType(intermediatePath) != AFS::ItemType::FILE) //throw FileError
                        return; //=continue
                }
                catch (FileError&) {}

                throw;
            }
        });
    };

    try //first try to move directly without copying
    {
        AFS::renameItem(sourcePath, targetPath); //throw FileError, ErrorDifferentVolume
        //great, we get away cheaply!
    }
    catch (ErrorDifferentVolume&)
    {
        try
        {
            copyNewItemPlain(); //throw FileError
        }
        catch (const FileError& e)
        {
            fixTargetPathIssues(e); //throw FileError

            //retry
            copyNewItemPlain(); //throw FileError
        }
        //[!] remove source file AFTER handling target path errors!
        AFS::removeFilePlain(sourcePath); //throw FileError
    }
    catch (const FileError& e)
    {
        fixTargetPathIssues(e); //throw FileError

        try //retry
        {
            AFS::renameItem(sourcePath, targetPath); //throw FileError, ErrorDifferentVolume
        }
        catch (ErrorDifferentVolume&)
        {
            copyNewItemPlain(); //throw FileError
            AFS::removeFilePlain(sourcePath); //throw FileError
        }
    }
}
}


void FileVersioner::revisionFile(const FileDescriptor& fileDescr, const Zstring& relativePath, const IOCallback& notifyUnbufferedIO) const //throw FileError
{
    if (Opt<AFS::ItemType> type = AFS::getItemTypeIfExists(fileDescr.path)) //throw FileError
    {
        if (*type == AFS::ItemType::SYMLINK)
            revisionSymlinkImpl(fileDescr.path, relativePath, nullptr /*onBeforeMove*/); //throw FileError
        else
            revisionFileImpl(fileDescr, relativePath, nullptr /*onBeforeMove*/, notifyUnbufferedIO); //throw FileError
    }
    //else -> missing source item is not an error => check BEFORE deleting target
}


void FileVersioner::revisionFileImpl(const FileDescriptor& fileDescr, const Zstring& relativePath, //throw FileError
                                     const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeMove,
                                     const IOCallback& notifyUnbufferedIO) const
{
    const AbstractPath& filePath = fileDescr.path;

    const AbstractPath targetPath = generateVersionedPath(relativePath);
    const AFS::StreamAttributes fileAttr{ fileDescr.attr.modTime, fileDescr.attr.fileSize, fileDescr.attr.fileId };

    if (onBeforeMove)
        onBeforeMove(AFS::getDisplayPath(filePath), AFS::getDisplayPath(targetPath));

    moveExistingItemToVersioning(filePath, targetPath, [&] //throw FileError
    {
        //target existing: copyFileTransactional() undefined behavior! (fail/overwrite/auto-rename) => not expected, but possible if target deletion failed
        /*const AFS::FileCopyResult result =*/ AFS::copyFileTransactional(filePath, fileAttr, targetPath, //throw FileError, ErrorFileLocked
                                                                          false, //copyFilePermissions
                                                                          false,  //transactionalCopy: not needed for versioning! partial copy will be overwritten next time
                                                                          nullptr /*onDeleteTargetFile*/, notifyUnbufferedIO);
        //result.errorModTime? => irrelevant for versioning!
    });
}


void FileVersioner::revisionSymlink(const AbstractPath& linkPath, const Zstring& relativePath) const //throw FileError
{
    if (AFS::getItemTypeIfExists(linkPath)) //throw FileError
        revisionSymlinkImpl(linkPath, relativePath, nullptr /*onBeforeMove*/); //throw FileError
    //else -> missing source item is not an error => check BEFORE deleting target
}


void FileVersioner::revisionSymlinkImpl(const AbstractPath& linkPath, const Zstring& relativePath, //throw FileError
                                        const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeMove) const
{

    const AbstractPath targetPath = generateVersionedPath(relativePath);

    if (onBeforeMove)
        onBeforeMove(AFS::getDisplayPath(linkPath), AFS::getDisplayPath(targetPath));

    moveExistingItemToVersioning(linkPath, targetPath, [&] { AFS::copySymlink(linkPath, targetPath, false /*copy filesystem permissions*/); }); //throw FileError
}


void FileVersioner::revisionFolder(const AbstractPath& folderPath, const Zstring& relativePath, //throw FileError
                                   const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFileMove,
                                   const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFolderMove,
                                   const IOCallback& notifyUnbufferedIO) const
{
    //no error situation if directory is not existing! manual deletion relies on it!
    if (Opt<AFS::ItemType> type = AFS::getItemTypeIfExists(folderPath)) //throw FileError
    {
        if (*type == AFS::ItemType::SYMLINK) //on Linux there is just one type of symlink, and since we do revision file symlinks, we should revision dir symlinks as well!
            revisionSymlinkImpl(folderPath, relativePath, onBeforeFileMove); //throw FileError
        else
            revisionFolderImpl(folderPath, relativePath, onBeforeFileMove, onBeforeFolderMove, notifyUnbufferedIO); //throw FileError
    }
    else //even if the folder did not exist anymore, significant I/O work was done => report
        if (onBeforeFolderMove) onBeforeFolderMove(AFS::getDisplayPath(folderPath), AFS::getDisplayPath(AFS::appendRelPath(versioningFolderPath_, relativePath)));
}


void FileVersioner::revisionFolderImpl(const AbstractPath& folderPath, const Zstring& relativePath, //throw FileError
                                       const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFileMove,
                                       const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFolderMove,
                                       const IOCallback& notifyUnbufferedIO) const
{

    //create target directories only when needed in moveFileToVersioning(): avoid empty directories!
    std::vector<AFS::FileInfo>    files;
    std::vector<AFS::FolderInfo>  folders;
    std::vector<AFS::SymlinkInfo> symlinks;

    AFS::traverseFolderFlat(folderPath, //throw FileError
    [&](const AFS::FileInfo&    fi) { files   .push_back(fi); assert(!files.back().symlinkInfo); },
    [&](const AFS::FolderInfo&  fi) { folders .push_back(fi); },
    [&](const AFS::SymlinkInfo& si) { symlinks.push_back(si); });

    const Zstring relPathPf = appendSeparator(relativePath);

    for (const auto& fileInfo : files)
    {
        const FileDescriptor fileDescr{ AFS::appendRelPath(folderPath, fileInfo.itemName),
                                        FileAttributes(fileInfo.modTime, fileInfo.fileSize, fileInfo.fileId, false /*isSymlink*/)};

        revisionFileImpl(fileDescr, relPathPf + fileInfo.itemName, onBeforeFileMove, notifyUnbufferedIO); //throw FileError
    }

    for (const auto& linkInfo : symlinks)
        revisionSymlinkImpl(AFS::appendRelPath(folderPath, linkInfo.itemName),
                            relPathPf + linkInfo.itemName, onBeforeFileMove); //throw FileError

    //move folders recursively
    for (const auto& folderInfo : folders)
        revisionFolderImpl(AFS::appendRelPath(folderPath, folderInfo.itemName), //throw FileError
                           relPathPf + folderInfo.itemName,
                           onBeforeFileMove, onBeforeFolderMove, notifyUnbufferedIO);
    //delete source
    if (onBeforeFolderMove)
        onBeforeFolderMove(AFS::getDisplayPath(folderPath), AFS::getDisplayPath(AFS::appendRelPath(versioningFolderPath_, relativePath)));

    AFS::removeFolderPlain(folderPath); //throw FileError
}

//###########################################################################################

namespace
{
struct VersionInfo
{
    time_t versionTime;
    AbstractPath filePath;
    bool isSymlink;
};
using VersionInfoMap = std::map<Zstring, std::vector<VersionInfo>, LessFilePath>; //relPathOrig => <version infos>

//subfolder\Sample.txt 2012-05-15 131513.txt  =>  subfolder\Sample.txt     version:2012-05-15 131513
//2012-05-15 131513\subfolder\Sample.txt      =>          "                          "

void findFileVersions(VersionInfoMap& versions,
                      const FolderContainer& folderCont,
                      const AbstractPath& parentFolderPath,
                      const Zstring& relPathOrigParent,
                      const time_t* versionTimeParent)
{
    auto addVersion = [&](const Zstring& fileName, const Zstring& fileNameOrig, time_t versionTime, bool isSymlink)
    {
        const Zstring& relPathOrig   = AFS::appendPaths(relPathOrigParent, fileNameOrig, FILE_NAME_SEPARATOR);
        const AbstractPath& filePath = AFS::appendRelPath(parentFolderPath, fileName);

        versions[relPathOrig].push_back(VersionInfo{ versionTime, filePath, isSymlink });
    };

    auto extractFileVersion = [&](const Zstring& fileName, bool isSymlink)
    {
        if (versionTimeParent) //VersioningStyle::TIMESTAMP_FOLDER
            addVersion(fileName, fileName, *versionTimeParent, isSymlink);
        else
        {
            const std::pair<time_t, Zstring> vfn = fff::impl::parseVersionedFileName(fileName);
            if (vfn.first != 0) //VersioningStyle::TIMESTAMP_FILE
                addVersion(fileName, vfn.second, vfn.first, isSymlink);
        }
    };

    for (const auto& item : folderCont.files)
        extractFileVersion(item.first, false /*isSymlink*/);

    for (const auto& item : folderCont.symlinks)
        extractFileVersion(item.first, true /*isSymlink*/);

    for (const auto& item : folderCont.folders)
    {
        const Zstring& folderName = item.first;

        if (relPathOrigParent.empty() && !versionTimeParent) //VersioningStyle::TIMESTAMP_FOLDER?
        {
            assert(!versionTimeParent);
            const time_t versionTime = fff::impl::parseVersionedFolderName(folderName);
            if (versionTime != 0)
            {
                findFileVersions(versions, item.second.second,
                                 AFS::appendRelPath(parentFolderPath, folderName),
                                 Zstring(), //[!] skip time-stamped folder
                                 &versionTime);
                continue;
            }
        }

        findFileVersions(versions, item.second.second,
                         AFS::appendRelPath(parentFolderPath, folderName),
                         AFS::appendPaths(relPathOrigParent, folderName, FILE_NAME_SEPARATOR),
                         versionTimeParent);
    }
}


void getFolderItemCount(std::map<AbstractPath, size_t>& folderItemCount, const FolderContainer& folderCont, const AbstractPath& parentFolderPath)
{
    size_t& itemCount = folderItemCount[parentFolderPath];
    itemCount = std::max(itemCount, folderCont.files.size() + folderCont.symlinks.size() + folderCont.folders.size());
    //theoretically possible that the same folder is found in one case with items, in another case empty (due to an error)
    //e.g. "subfolder" for versioning folders c:\folder and c:\folder\subfolder

    for (const auto& item : folderCont.folders)
        getFolderItemCount(folderItemCount, item.second.second, AFS::appendRelPath(parentFolderPath, item.first));
}
}


bool fff::operator<(const VersioningLimitFolder& lhs, const VersioningLimitFolder& rhs)
{
    const int cmp = AFS::compareAbstractPath(lhs.versioningFolderPath, rhs.versioningFolderPath);
    if (cmp != 0)
        return cmp < 0;

    if (lhs.versionMaxAgeDays != rhs.versionMaxAgeDays)
        return lhs.versionMaxAgeDays < rhs.versionMaxAgeDays;

    if (lhs.versionMaxAgeDays > 0)
    {
        if (lhs.versionCountMin != rhs.versionCountMin)
            return lhs.versionCountMin < rhs.versionCountMin;
    }

    return lhs.versionCountMax < rhs.versionCountMax;
}


void fff::applyVersioningLimit(const std::set<VersioningLimitFolder>& limitFolders,
                               const std::map<AbstractPath, size_t>& deviceParallelOps,
                               ProcessCallback& callback /*throw X*/)
{
    warn_static("what if folder does not yet exist?")

    //--------- traverse all versioning folders ---------
    std::set<DirectoryKey> foldersToRead;
    for (const VersioningLimitFolder& vlf : limitFolders)
        if (vlf.versionMaxAgeDays > 0 || vlf.versionCountMax > 0) //only analyze versioning folders when needed!
            foldersToRead.emplace(DirectoryKey({ vlf.versioningFolderPath, std::make_shared<NullFilter>(), SymLinkHandling::DIRECT }));

    auto onError = [&](const std::wstring& msg, size_t retryNumber)
    {
        switch (callback.reportError(msg, retryNumber)) //throw X
        {
            case ProcessCallback::IGNORE_ERROR:
                return AFS::TraverserCallback::ON_ERROR_CONTINUE;

            case ProcessCallback::RETRY:
                return AFS::TraverserCallback::ON_ERROR_RETRY;
        }
        assert(false);
        return AFS::TraverserCallback::ON_ERROR_CONTINUE;
    };

    const std::wstring textScanning = _("Searching for excess file versions:") + L" ";

    auto onStatusUpdate = [&](const std::wstring& statusLine, int itemsTotal)
    {
        callback.reportStatus(textScanning + statusLine); //throw X
    };

    std::map<DirectoryKey, DirectoryValue> folderBuf;

    parallelDeviceTraversal(foldersToRead, folderBuf,
                            deviceParallelOps,
                            onError, onStatusUpdate, //throw X
                            UI_UPDATE_INTERVAL / 2); //every ~50 ms

    //--------- group versions per (original) relative path ---------
    std::map<AbstractPath, VersionInfoMap> versionDetails; //versioningFolderPath => <version details>
    std::map<AbstractPath, size_t> folderItemCount; //<folder path> => <item count> for determination of empty folders

    for (const auto& item : folderBuf)
    {
        const AbstractPath versioningFolderPath = item.first.folderPath;
        const DirectoryValue& dirVal            = item.second;

        assert(versionDetails.find(versioningFolderPath) == versionDetails.end());

        findFileVersions(versionDetails[versioningFolderPath],
                         dirVal.folderCont,
                         versioningFolderPath,
                         Zstring() /*relPathOrigParent*/,
                         nullptr /*versionTimeParent*/);

        //determine item count per folder for later detection and removal of empty folders:
        getFolderItemCount(folderItemCount, dirVal.folderCont, versioningFolderPath);

        //make sure the versioning folder is never found empty and is not deleted:
        ++folderItemCount[versioningFolderPath];

        //similarly, failed folder traversal should not make folders look empty:
        for (const auto& item2 : dirVal.failedFolderReads) ++folderItemCount[AFS::appendRelPath(versioningFolderPath, item2.first)];
        for (const auto& item2 : dirVal.failedItemReads  ) ++folderItemCount[AFS::appendRelPath(versioningFolderPath, beforeLast(item2.first, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE))];
    }

    //--------- calculate excess file versions ---------
    std::map<AbstractPath, bool /*isSymlink*/> itemsToDelete;

    const time_t lastMidnightTime = []
    {
        TimeComp tc = getLocalTime(); //returns TimeComp() on error
        tc.second = 0;
        tc.minute = 0;
        tc.hour   = 0;
        return localToTimeT(tc); //returns -1 on error => swallow => no versions trimmed by versionMaxAgeDays
    }();

    for (const VersioningLimitFolder& vlf : limitFolders)
        if (vlf.versionMaxAgeDays > 0 || vlf.versionCountMax > 0) //NOT redundant regarding the same check above
            for (auto& item : versionDetails.find(vlf.versioningFolderPath)->second) //exists after construction above!
            {
                std::vector<VersionInfo>& versions = item.second;

                size_t versionsToKeep = versions.size();
                if (vlf.versionMaxAgeDays > 0)
                {
                    const time_t cutOffTime = lastMidnightTime - vlf.versionMaxAgeDays * 24 * 3600;

                    versionsToKeep = std::count_if(versions.begin(), versions.end(), [cutOffTime](const VersionInfo& vi) { return vi.versionTime >= cutOffTime; });

                    if (vlf.versionCountMin > 0)
                        versionsToKeep = std::max<size_t>(versionsToKeep, vlf.versionCountMin);
                }
                if (vlf.versionCountMax > 0)
                    versionsToKeep = std::min<size_t>(versionsToKeep, vlf.versionCountMax);

                if (versions.size() > versionsToKeep)
                {
                    std::nth_element(versions.begin(), versions.end() - versionsToKeep, versions.end(),
                    [](const VersionInfo& lhs, const VersionInfo& rhs) { return lhs.versionTime < rhs.versionTime; });
                    //oldest versions sorted to the front

                    std::for_each(versions.begin(), versions.end() - versionsToKeep, [&](const VersionInfo& vi)
                    {
                        itemsToDelete.emplace(vi.filePath, vi.isSymlink);
                    });
                }
            }

    //--------- remove excess file versions ---------
    Protected<std::map<AbstractPath, size_t>&> folderItemCountShared(folderItemCount);
    const std::wstring textRemoving = _("Removing excess file versions:") + L" ";
    const std::wstring textDeletingFolder = _("Deleting folder %x");

    ParallelWorkItem deleteEmptyFolderTask;
    deleteEmptyFolderTask = [&textDeletingFolder, &folderItemCountShared, &deleteEmptyFolderTask](ParallelContext& ctx) /*throw ThreadInterruption*/
    {
        const std::wstring errMsg = tryReportingError([&] //throw ThreadInterruption
        {
            ctx.acb.reportStatus(replaceCpy(textDeletingFolder, L"%x", fmtPath(AFS::getDisplayPath(ctx.itemPath)))); //throw ThreadInterruption
            AFS::removeEmptyFolderIfExists(ctx.itemPath); //throw FileError
        }, ctx.acb);

        if (errMsg.empty())
            if (Opt<AbstractPath> parentPath = AFS::getParentFolderPath(ctx.itemPath))
            {
                bool scheduleDelete = false;
                folderItemCountShared.access([&](auto& folderItemCount2) { scheduleDelete = --folderItemCount2[*parentPath] == 0; });
                if (scheduleDelete)
                    ctx.scheduleExtraTask(AfsPath(AFS::getRootRelativePath(*parentPath)), deleteEmptyFolderTask); //throw ThreadInterruption
            }
    };

    std::vector<std::pair<AbstractPath, ParallelWorkItem>> parallelWorkload;

    for (const auto& item : folderItemCount)
        if (item.second == 0)
            parallelWorkload.emplace_back(item.first, deleteEmptyFolderTask);

    for (const auto& item : itemsToDelete)
        parallelWorkload.emplace_back(item.first, [isSymlink = item.second, &textRemoving, &folderItemCountShared, &deleteEmptyFolderTask](ParallelContext& ctx) /*throw ThreadInterruption*/
    {
        const std::wstring errMsg = tryReportingError([&] //throw ThreadInterruption
        {
            ctx.acb.reportInfo(textRemoving + AFS::getDisplayPath(ctx.itemPath)); //throw ThreadInterruption
            if (isSymlink)
                AFS::removeSymlinkIfExists(ctx.itemPath); //throw FileError
            else
                AFS::removeFileIfExists(ctx.itemPath); //throw FileError
        }, ctx.acb);

        if (errMsg.empty())
            if (Opt<AbstractPath> parentPath = AFS::getParentFolderPath(ctx.itemPath))
            {
                bool scheduleDelete = false;
                folderItemCountShared.access([&](auto& folderItemCount2) { scheduleDelete = --folderItemCount2[*parentPath] == 0; });
                if (scheduleDelete)
                    ctx.scheduleExtraTask(AfsPath(AFS::getRootRelativePath(*parentPath)), deleteEmptyFolderTask); //throw ThreadInterruption
                assert(AFS::getRootPath(*parentPath) == AFS::getRootPath(ctx.itemPath));
            }
    });

    massParallelExecute(parallelWorkload, deviceParallelOps, "Versioning Limit", callback /*throw X*/);
}

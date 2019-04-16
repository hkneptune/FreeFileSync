// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "abstract.h"
#include <zen/serialize.h>
#include <zen/guid.h>
#include <zen/crc.h>

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


const Zchar* AFS::TEMP_FILE_ENDING = Zstr(".ffs_tmp");


bool fff::isValidRelPath(const Zstring& relPath)
{
    return !contains(relPath, '\\') &&
           !startsWith(relPath, FILE_NAME_SEPARATOR) && !endsWith(relPath, FILE_NAME_SEPARATOR) &&
           !contains(relPath, Zstring() + FILE_NAME_SEPARATOR + FILE_NAME_SEPARATOR);
}


int AFS::compareDevice(const AbstractFileSystem& lhs, const AbstractFileSystem& rhs)
{
    //note: in worst case, order is guaranteed to be stable only during each program run
    if (typeid(lhs).before(typeid(rhs)))
        return -1;
    if (typeid(rhs).before(typeid(lhs)))
        return 1;
    assert(typeid(lhs) == typeid(rhs));
    //caveat: typeid returns static type for pointers, dynamic type for references!!!

    return lhs.compareDeviceSameAfsType(rhs);
}


int AFS::comparePath(const AbstractPath& lhs, const AbstractPath& rhs)
{
    const int rv = compareDevice(lhs.afsDevice.ref(), rhs.afsDevice.ref());
    if (rv != 0)
        return rv;

    return compareString(lhs.afsPath.value, rhs.afsPath.value);
}


std::optional<AbstractPath> AFS::getParentPath(const AbstractPath& ap)
{
    if (const std::optional<AfsPath> parentAfsPath = getParentPath(ap.afsPath))
        return AbstractPath(ap.afsDevice, *parentAfsPath);

    return {};
}


std::optional<AfsPath> AFS::getParentPath(const AfsPath& afsPath)
{
    if (afsPath.value.empty())
        return {};

    return AfsPath(beforeLast(afsPath.value, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE));
}


namespace
{
struct FlatTraverserCallback : public AFS::TraverserCallback
{
    FlatTraverserCallback(const std::function<void (const AFS::FileInfo&    fi)>& onFile,
                          const std::function<void (const AFS::FolderInfo&  fi)>& onFolder,
                          const std::function<void (const AFS::SymlinkInfo& si)>& onSymlink) :
        onFile_   (onFile),
        onFolder_ (onFolder),
        onSymlink_(onSymlink) {}

private:
    void                               onFile   (const AFS::FileInfo&    fi) override { if (onFile_)    onFile_   (fi); }
    std::shared_ptr<TraverserCallback> onFolder (const AFS::FolderInfo&  fi) override { if (onFolder_)  onFolder_ (fi); return nullptr; }
    HandleLink                         onSymlink(const AFS::SymlinkInfo& si) override { if (onSymlink_) onSymlink_(si); return TraverserCallback::LINK_SKIP; }

    HandleError reportDirError (const std::wstring& msg, size_t retryNumber)                          override { throw FileError(msg); }
    HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName) override { throw FileError(msg); }

    const std::function<void (const AFS::FileInfo&    fi)> onFile_;
    const std::function<void (const AFS::FolderInfo&  fi)> onFolder_;
    const std::function<void (const AFS::SymlinkInfo& si)> onSymlink_;
};
}


void AFS::traverseFolderFlat(const AfsPath& afsPath, //throw FileError
                             const std::function<void (const FileInfo&    fi)>& onFile,
                             const std::function<void (const FolderInfo&  fi)>& onFolder,
                             const std::function<void (const SymlinkInfo& si)>& onSymlink) const
{
    auto ft = std::make_shared<FlatTraverserCallback>(onFile, onFolder, onSymlink); //throw FileError
    traverseFolderRecursive({{ afsPath, ft }}, 1 /*parallelOps*/); //throw FileError
}


//target existing: undefined behavior! (fail/overwrite/auto-rename)
AFS::FileCopyResult AFS::copyFileAsStream(const AfsPath& afsPathSource, const StreamAttributes& attrSource, //throw FileError, ErrorFileLocked, X
                                          const AbstractPath& apTarget, const IOCallback& notifyUnbufferedIO /*throw X*/) const
{
    int64_t totalUnbufferedIO = 0;
    IOCallbackDivider cbd(notifyUnbufferedIO, totalUnbufferedIO);

    int64_t totalBytesRead    = 0;
    int64_t totalBytesWritten = 0;
    auto notifyUnbufferedRead  = [&](int64_t bytesDelta) { totalBytesRead    += bytesDelta; cbd(bytesDelta); };
    auto notifyUnbufferedWrite = [&](int64_t bytesDelta) { totalBytesWritten += bytesDelta; cbd(bytesDelta); };
    //--------------------------------------------------------------------------------------------------------

    auto streamIn = getInputStream(afsPathSource, notifyUnbufferedRead); //throw FileError, ErrorFileLocked

    StreamAttributes attrSourceNew = {};
    //try to get the most current attributes if possible (input file might have changed after comparison!)
    if (std::optional<StreamAttributes> attr = streamIn->getAttributesBuffered()) //throw FileError
        attrSourceNew = *attr; //Native/MTP/Google Drive
    else //use more stale ones:
        attrSourceNew = attrSource; //SFTP/FTP
    //TODO: evaluate: consequences of stale attributes

    //target existing: undefined behavior! (fail/overwrite/auto-rename)
    auto streamOut = getOutputStream(apTarget, attrSourceNew.fileSize, attrSourceNew.modTime, notifyUnbufferedWrite); //throw FileError

    bufferedStreamCopy(*streamIn, *streamOut); //throw FileError, ErrorFileLocked, X

    const AFS::FinalizeResult finResult = streamOut->finalize(); //throw FileError, X

    //catch file I/O bugs + read/write conflicts: (note: different check than inside AbstractFileSystem::OutputStream::finalize() => checks notifyUnbufferedIO()!)
    ZEN_ON_SCOPE_FAIL(try { removeFilePlain(apTarget); /*throw FileError*/ }
    catch (FileError& e) { (void)e; });   //after finalize(): not guarded by ~AFS::OutputStream() anymore!

    if (totalBytesRead != makeSigned(attrSourceNew.fileSize))
        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getDisplayPath(afsPathSource))),
                        replaceCpy(replaceCpy(_("Unexpected size of data stream.\nExpected: %x bytes\nActual: %y bytes"),
                                              L"%x", numberTo<std::wstring>(attrSourceNew.fileSize)),
                                   L"%y", numberTo<std::wstring>(totalBytesRead)) + L" [notifyUnbufferedRead]");

    if (totalBytesWritten != totalBytesRead)
        throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getDisplayPath(apTarget))),
                        replaceCpy(replaceCpy(_("Unexpected size of data stream.\nExpected: %x bytes\nActual: %y bytes"),
                                              L"%x", numberTo<std::wstring>(totalBytesRead)),
                                   L"%y", numberTo<std::wstring>(totalBytesWritten)) + L" [notifyUnbufferedWrite]");

    AFS::FileCopyResult cpResult;
    cpResult.fileSize     = attrSourceNew.fileSize;
    cpResult.modTime      = attrSourceNew.modTime;
    cpResult.sourceFileId = attrSourceNew.fileId;
    cpResult.targetFileId = finResult.fileId;
    cpResult.errorModTime = finResult.errorModTime;
    /* Failing to set modification time is not a serious problem from synchronization perspective (treated like external update)
            => Support additional scenarios:
            - GVFS failing to set modTime for FTP: https://freefilesync.org/forum/viewtopic.php?t=2372
            - GVFS failing to set modTime for MTP: https://freefilesync.org/forum/viewtopic.php?t=2803
            - MTP failing to set modTime in general: fail non-silently rather than silently during file creation
            - FTP failing to set modTime for servers without MFMT-support    */
    return cpResult;
}


//target existing: undefined behavior! (fail/overwrite/auto-rename)
AFS::FileCopyResult AFS::copyFileTransactional(const AbstractPath& apSource, const StreamAttributes& attrSource, //throw FileError, ErrorFileLocked, X
                                               const AbstractPath& apTarget,
                                               bool copyFilePermissions,
                                               bool transactionalCopy,
                                               const std::function<void()>& onDeleteTargetFile,
                                               const IOCallback& notifyUnbufferedIO /*throw X*/)
{
    auto copyFilePlain = [&](const AbstractPath& apTargetTmp)
    {
        //caveat: typeid returns static type for pointers, dynamic type for references!!!
        if (typeid(apSource.afsDevice.ref()) == typeid(apTargetTmp.afsDevice.ref()))
            return apSource.afsDevice.ref().copyFileForSameAfsType(apSource.afsPath, attrSource,
                                                                   apTargetTmp, copyFilePermissions, notifyUnbufferedIO); //throw FileError, ErrorFileLocked, X
        //target existing: undefined behavior! (fail/overwrite/auto-rename)

        //fall back to stream-based file copy:
        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(AFS::getDisplayPath(apTargetTmp))),
                            _("Operation not supported between different devices."));

        return apSource.afsDevice.ref().copyFileAsStream(apSource.afsPath, attrSource, apTargetTmp, notifyUnbufferedIO); //throw FileError, ErrorFileLocked, X
        //target existing: undefined behavior! (fail/overwrite/auto-rename)
    };

    if (transactionalCopy && !hasNativeTransactionalCopy(apTarget))
    {
        std::optional<AbstractPath> parentPath = AFS::getParentPath(apTarget);
        if (!parentPath)
            throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(AFS::getDisplayPath(apTarget))), L"Path is device root.");
        const Zstring fileName = AFS::getItemName(apTarget);

        //- generate (hopefully) unique file name to avoid clashing with some remnant ffs_tmp file
        //- do not loop: avoid pathological cases, e.g. https://freefilesync.org/forum/viewtopic.php?t=1592
        const Zstring& shortGuid = printNumber<Zstring>(Zstr("%04x"), static_cast<unsigned int>(getCrc16(generateGUID())));
        const Zstring& tmpExt    = Zstr('.') + shortGuid + TEMP_FILE_ENDING;

        Zstring tmpName = beforeLast(fileName, Zstr('.'), IF_MISSING_RETURN_ALL);

        //don't make the temp name longer than the original when hitting file system name length limitations: "lpMaximumComponentLength is commonly 255 characters"
        while (tmpName.size() > 200) //BUT don't trim short names! we want early failure on filename-related issues
            tmpName = getUnicodeSubstring(tmpName, 0 /*uniPosFirst*/, unicodeLength(tmpName) / 2 /*uniPosLast*/); //consider UTF encoding when cutting in the middle! (e.g. for macOS)

        const AbstractPath apTargetTmp = AFS::appendRelPath(*parentPath, tmpName + tmpExt);
        //-------------------------------------------------------------------------------------------

        const AFS::FileCopyResult result = copyFilePlain(apTargetTmp); //throw FileError, ErrorFileLocked

        //transactional behavior: ensure cleanup; not needed before copyFilePlain() which is already transactional
        ZEN_ON_SCOPE_FAIL( try { AFS::removeFilePlain(apTargetTmp); }
        catch (FileError&) {});

        //have target file deleted (after read access on source and target has been confirmed) => allow for almost transactional overwrite
        if (onDeleteTargetFile)
            onDeleteTargetFile(); //throw X

        //perf: this call is REALLY expensive on unbuffered volumes! ~40% performance decrease on FAT USB stick!
        moveAndRenameItem(apTargetTmp, apTarget); //throw FileError, (ErrorMoveUnsupported)

        /*
            CAVEAT on FAT/FAT32: the sequence of deleting the target file and renaming "file.txt.ffs_tmp" to "file.txt" does
            NOT PRESERVE the creation time of the .ffs_tmp file, but SILENTLY "reuses" whatever creation time the old "file.txt" had!
            This "feature" is called "File System Tunneling":
            https://devblogs.microsoft.com/oldnewthing/?p=34923
            http://support.microsoft.com/kb/172190/en-us
        */
        return result;
    }
    else
    {
        /*
           Note: non-transactional file copy solves at least four problems:
                -> skydrive - doesn't allow for .ffs_tmp extension and returns ERROR_INVALID_PARAMETER
                -> network renaming issues
                -> allow for true delete before copy to handle low disk space problems
                -> higher performance on unbuffered drives (e.g. USB-sticks)
        */
        if (onDeleteTargetFile)
            onDeleteTargetFile();

        return copyFilePlain(apTarget); //throw FileError, ErrorFileLocked
    }
}


void AFS::createFolderIfMissingRecursion(const AbstractPath& ap) //throw FileError
{
    const std::optional<AbstractPath> parentPath = getParentPath(ap);
    if (!parentPath) //device root
        return;

    try //generally we expect that path already exists (see: versioning, base folder, log file path) => check first
    {
        if (getItemType(ap) != ItemType::FILE) //throw FileError
            return;
    }
    catch (FileError&) {} //not yet existing or access error? let's find out...

    createFolderIfMissingRecursion(*parentPath); //throw FileError

    try
    {
        //target existing: fail/ignore
        createFolderPlain(ap); //throw FileError
    }
    catch (FileError&)
    {
        try
        {
            if (getItemType(ap) != ItemType::FILE) //throw FileError
                return; //already existing => possible, if createFolderIfMissingRecursion() is run in parallel
        }
        catch (FileError&) {} //not yet existing or access error

        throw;
    }
}


//default implementation: folder traversal
std::optional<AFS::ItemType> AFS::itemStillExists(const AfsPath& afsPath) const //throw FileError
{
    try
    {
        //fast check: 1. perf 2. expected by getFolderStatusNonBlocking() 3. traversing non-existing folder below MIGHT NOT FAIL (e.g. for SFTP on AWS)
        return getItemType(afsPath); //throw FileError
    }
    catch (const FileError& e) //not existing or access error
    {
        const std::optional<AfsPath> parentAfsPath = getParentPath(afsPath);
        if (!parentAfsPath) //device root
            throw;
        //else: let's dig deeper... don't bother checking Win32 codes; e.g. not existing item may have the codes:
        //  ERROR_FILE_NOT_FOUND, ERROR_PATH_NOT_FOUND, ERROR_INVALID_NAME, ERROR_INVALID_DRIVE,
        //  ERROR_NOT_READY, ERROR_INVALID_PARAMETER, ERROR_BAD_PATHNAME, ERROR_BAD_NETPATH => not reliable

        const Zstring itemName = getItemName(afsPath);
        assert(!itemName.empty());

        const std::optional<ItemType> parentType = AFS::itemStillExists(*parentAfsPath); //throw FileError
        if (parentType && *parentType != ItemType::FILE /*obscure, but possible (and not an error)*/)
            try
            {
                traverseFolderFlat(*parentAfsPath, //throw FileError
                [&](const    FileInfo& fi) { if (fi.itemName == itemName) throw ItemType::FILE;    },
                [&](const  FolderInfo& fi) { if (fi.itemName == itemName) throw ItemType::FOLDER;  },
                [&](const SymlinkInfo& si) { if (si.itemName == itemName) throw ItemType::SYMLINK; });
            }
            catch (const ItemType&) //finding the item after getItemType() previously failed is exceptional
            {
                throw e; //yes, slicing
            }
        return {};
    }
}


//default implementation: folder traversal
void AFS::removeFolderIfExistsRecursion(const AfsPath& afsPath, //throw FileError
                                        const std::function<void (const std::wstring& displayPath)>& onBeforeFileDeletion /*throw X*/, //optional
                                        const std::function<void (const std::wstring& displayPath)>& onBeforeFolderDeletion) const //one call for each object!
{
    //deferred recursion => save stack space and allow deletion of extremely deep hierarchies!
    std::function<void(const AfsPath& folderPath)> removeFolderRecursionImpl;
    removeFolderRecursionImpl = [this, &onBeforeFileDeletion, &onBeforeFolderDeletion, &removeFolderRecursionImpl](const AfsPath& folderPath) //throw FileError
    {
        std::vector<Zstring> fileNames;
        std::vector<Zstring> folderNames;
        std::vector<Zstring> symlinkNames;

        traverseFolderFlat(folderPath, //throw FileError
        [&](const    FileInfo& fi) {    fileNames.push_back(fi.itemName); },
        [&](const  FolderInfo& fi) {  folderNames.push_back(fi.itemName); },
        [&](const SymlinkInfo& si) { symlinkNames.push_back(si.itemName); });

        for (const Zstring& fileName : fileNames)
        {
            const AfsPath filePath(nativeAppendPaths(folderPath.value, fileName));
            if (onBeforeFileDeletion)
                onBeforeFileDeletion(getDisplayPath(filePath)); //throw X

            removeFilePlain(filePath); //throw FileError
        }

        for (const Zstring& symlinkName : symlinkNames)
        {
            const AfsPath linkPath(nativeAppendPaths(folderPath.value, symlinkName));
            if (onBeforeFileDeletion)
                onBeforeFileDeletion(getDisplayPath(linkPath)); //throw X

            removeSymlinkPlain(linkPath); //throw FileError
        }

        for (const Zstring& folderName : folderNames)
            removeFolderRecursionImpl(AfsPath(nativeAppendPaths(folderPath.value, folderName))); //throw FileError

        if (onBeforeFolderDeletion)
            onBeforeFolderDeletion(getDisplayPath(folderPath)); //throw X

        removeFolderPlain(folderPath); //throw FileError
    };
    //--------------------------------------------------------------------------------------------------------------
    warn_static("what about parallelOps?")

    //no error situation if directory is not existing! manual deletion relies on it!
    if (std::optional<ItemType> type = itemStillExists(afsPath)) //throw FileError
    {
        if (*type == AFS::ItemType::SYMLINK)
        {
            if (onBeforeFileDeletion)
                onBeforeFileDeletion(getDisplayPath(afsPath)); //throw X

            removeSymlinkPlain(afsPath); //throw FileError
        }
        else
            removeFolderRecursionImpl(afsPath); //throw FileError
    }
    else //even if the folder did not exist anymore, significant I/O work was done => report
        if (onBeforeFolderDeletion) onBeforeFolderDeletion(getDisplayPath(afsPath)); //throw X
}


void AFS::removeFileIfExists(const AbstractPath& ap) //throw FileError
{
    try
    {
        AFS::removeFilePlain(ap); //throw FileError
    }
    catch (const FileError&)
    {
        try
        {
            if (!AFS::itemStillExists(ap)) //throw FileError
                return;
        }
        catch (const FileError& e2) { throw FileError(replaceCpy(_("Cannot delete file %x."), L"%x", fmtPath(getDisplayPath(ap))), replaceCpy(e2.toString(), L"\n\n", L"\n")); }
        //more relevant than previous exception (which could be "item not found")
        throw;
    }
}


void AFS::removeSymlinkIfExists(const AbstractPath& ap) //throw FileError
{
    try
    {
        AFS::removeSymlinkPlain(ap); //throw FileError
    }
    catch (const FileError&)
    {
        try
        {
            if (!AFS::itemStillExists(ap)) //throw FileError
                return;
        }
        catch (const FileError& e2) { throw FileError(replaceCpy(_("Cannot delete symbolic link %x."), L"%x", fmtPath(getDisplayPath(ap))), replaceCpy(e2.toString(), L"\n\n", L"\n")); }
        //more relevant than previous exception (which could be "item not found")
        throw;
    }
}


void AFS::removeEmptyFolderIfExists(const AbstractPath& ap) //throw FileError
{
    try
    {
        AFS::removeFolderPlain(ap); //throw FileError
    }
    catch (const FileError&)
    {
        try
        {
            if (!AFS::itemStillExists(ap)) //throw FileError
                return;
        }
        catch (const FileError& e2) { throw FileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(getDisplayPath(ap))), replaceCpy(e2.toString(), L"\n\n", L"\n")); }
        //more relevant than previous exception (which could be "item not found")

        throw;
    }
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "abstract.h"
#include <zen/serialize.h>
#include <zen/guid.h>
#include <zen/crc.h>
#include <typeindex>

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


bool fff::isValidRelPath(const Zstring& relPath)
{
    //relPath is expected to use FILE_NAME_SEPARATOR!
    if constexpr (FILE_NAME_SEPARATOR != Zstr('/' )) if (contains(relPath, Zstr('/' ))) return false;
    if constexpr (FILE_NAME_SEPARATOR != Zstr('\\')) if (contains(relPath, Zstr('\\'))) return false;

    const Zchar doubleSep[] = { FILE_NAME_SEPARATOR, FILE_NAME_SEPARATOR, 0 };
    return !startsWith(relPath, FILE_NAME_SEPARATOR)&& !endsWith(relPath, FILE_NAME_SEPARATOR)&&
           !contains(relPath, doubleSep);
}


AfsPath fff::sanitizeDeviceRelativePath(Zstring relPath)
{
    if constexpr (FILE_NAME_SEPARATOR != Zstr('/' )) replace(relPath, Zstr('/'),  FILE_NAME_SEPARATOR);
    if constexpr (FILE_NAME_SEPARATOR != Zstr('\\')) replace(relPath, Zstr('\\'), FILE_NAME_SEPARATOR);
    trim(relPath, true, true, [](Zchar c) { return c == FILE_NAME_SEPARATOR; });
    return AfsPath(relPath);
}


std::weak_ordering AFS::compareDevice(const AbstractFileSystem& lhs, const AbstractFileSystem& rhs)
{
    //note: in worst case, order is guaranteed to be stable only during each program run
    //caveat: typeid returns static type for pointers, dynamic type for references!!!
    if (const std::strong_ordering cmp = std::type_index(typeid(lhs)) <=> std::type_index(typeid(rhs));
        std::is_neq(cmp))
        return cmp;

    return lhs.compareDeviceSameAfsType(rhs);
}


std::optional<AbstractPath> AFS::getParentPath(const AbstractPath& ap)
{
    if (const std::optional<AfsPath> parentAfsPath = getParentPath(ap.afsPath))
        return AbstractPath(ap.afsDevice, *parentAfsPath);

    return {};
}


std::optional<AfsPath> AFS::getParentPath(const AfsPath& afsPath)
{
    if (!afsPath.value.empty())
        return AfsPath(beforeLast(afsPath.value, FILE_NAME_SEPARATOR, IfNotFoundReturn::none));

    return {};
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

    HandleError reportDirError (const ErrorInfo& errorInfo)                          override { throw FileError(errorInfo.msg); }
    HandleError reportItemError(const ErrorInfo& errorInfo, const Zstring& itemName) override { throw FileError(errorInfo.msg); }

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


//already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
AFS::FileCopyResult AFS::copyFileAsStream(const AfsPath& afsSource, const StreamAttributes& attrSource, //throw FileError, ErrorFileLocked, X
                                          const AbstractPath& apTarget, const IOCallback& notifyUnbufferedIO /*throw X*/) const
{
    int64_t totalUnbufferedIO = 0;
    IOCallbackDivider cbd(notifyUnbufferedIO, totalUnbufferedIO);

    int64_t totalBytesRead    = 0;
    int64_t totalBytesWritten = 0;
    auto notifyUnbufferedRead  = [&](int64_t bytesDelta) { totalBytesRead    += bytesDelta; cbd(bytesDelta); };
    auto notifyUnbufferedWrite = [&](int64_t bytesDelta) { totalBytesWritten += bytesDelta; cbd(bytesDelta); };
    //--------------------------------------------------------------------------------------------------------

    auto streamIn = getInputStream(afsSource, notifyUnbufferedRead); //throw FileError, ErrorFileLocked

    StreamAttributes attrSourceNew = {};
    //try to get the most current attributes if possible (input file might have changed after comparison!)
    if (std::optional<StreamAttributes> attr = streamIn->getAttributesBuffered()) //throw FileError
        attrSourceNew = *attr; //Native/MTP/Google Drive
    else //use possibly stale ones:
        attrSourceNew = attrSource; //SFTP/FTP
    //TODO: evaluate: consequences of stale attributes

    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    auto streamOut = getOutputStream(apTarget, attrSourceNew.fileSize, attrSourceNew.modTime, notifyUnbufferedWrite); //throw FileError

    bufferedStreamCopy(*streamIn, *streamOut); //throw FileError, ErrorFileLocked, X

    //check incomplete input *before* failing with (slightly) misleading error message in OutputStream::finalize()
    if (totalBytesRead != makeSigned(attrSourceNew.fileSize))
        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getDisplayPath(afsSource))),
                        replaceCpy(replaceCpy(_("Unexpected size of data stream.\nExpected: %x bytes\nActual: %y bytes"),
                                              L"%x", numberTo<std::wstring>(attrSourceNew.fileSize)),
                                   L"%y", numberTo<std::wstring>(totalBytesRead)) + L" [notifyUnbufferedRead]");

    const FinalizeResult finResult = streamOut->finalize(); //throw FileError, X

    ZEN_ON_SCOPE_FAIL(try { removeFilePlain(apTarget); /*throw FileError*/ }
    catch (FileError&) {}); //after finalize(): not guarded by ~AFS::OutputStream() anymore!

    //catch file I/O bugs + read/write conflicts: (note: different check than inside AFS::OutputStream::finalize() => checks notifyUnbufferedIO()!)
    if (totalBytesWritten != totalBytesRead)
        throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getDisplayPath(apTarget))),
                        replaceCpy(replaceCpy(_("Unexpected size of data stream.\nExpected: %x bytes\nActual: %y bytes"),
                                              L"%x", numberTo<std::wstring>(totalBytesRead)),
                                   L"%y", numberTo<std::wstring>(totalBytesWritten)) + L" [notifyUnbufferedWrite]");

    FileCopyResult cpResult;
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


//already existing + no onDeleteTargetFile: undefined behavior! (e.g. fail/overwrite/auto-rename)
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
        //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)

        //fall back to stream-based file copy:
        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(getDisplayPath(apTargetTmp))),
                            _("Operation not supported between different devices."));

        //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
        return apSource.afsDevice.ref().copyFileAsStream(apSource.afsPath, attrSource, apTargetTmp, notifyUnbufferedIO); //throw FileError, ErrorFileLocked, X
    };

    if (transactionalCopy && !hasNativeTransactionalCopy(apTarget))
    {
        const std::optional<AbstractPath> parentPath = getParentPath(apTarget);
        if (!parentPath)
            throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getDisplayPath(apTarget))), L"Path is device root.");
        const Zstring fileName = getItemName(apTarget);

        //- generate (hopefully) unique file name to avoid clashing with some remnant ffs_tmp file
        //- do not loop: avoid pathological cases, e.g. https://freefilesync.org/forum/viewtopic.php?t=1592
        Zstring tmpName = beforeLast(fileName, Zstr('.'), IfNotFoundReturn::all);

        //don't make the temp name longer than the original when hitting file system name length limitations: "lpMaximumComponentLength is commonly 255 characters"
        while (tmpName.size() > 200) //BUT don't trim short names! we want early failure on filename-related issues
            tmpName = getUnicodeSubstring(tmpName, 0 /*uniPosFirst*/, unicodeLength(tmpName) / 2 /*uniPosLast*/); //consider UTF encoding when cutting in the middle! (e.g. for macOS)

        const Zstring& shortGuid = printNumber<Zstring>(Zstr("%04x"), static_cast<unsigned int>(getCrc16(generateGUID())));

        const AbstractPath apTargetTmp = appendRelPath(*parentPath, tmpName + Zstr('~') + shortGuid + TEMP_FILE_ENDING);
        //-------------------------------------------------------------------------------------------

        const FileCopyResult result = copyFilePlain(apTargetTmp); //throw FileError, ErrorFileLocked

        //transactional behavior: ensure cleanup; not needed before copyFilePlain() which is already transactional
        ZEN_ON_SCOPE_FAIL( try { removeFilePlain(apTargetTmp); }
        catch (FileError&) {});

        //have target file deleted (after read access on source and target has been confirmed) => allow for almost transactional overwrite
        if (onDeleteTargetFile)
            onDeleteTargetFile(); //throw X

        //already existing: undefined behavior! (e.g. fail/overwrite)
        moveAndRenameItem(apTargetTmp, apTarget); //throw FileError, (ErrorMoveUnsupported)
        //perf: this call is REALLY expensive on unbuffered volumes! ~40% performance decrease on FAT USB stick!

        /*  CAVEAT on FAT/FAT32: the sequence of deleting the target file and renaming "file.txt.ffs_tmp" to "file.txt" does
            NOT PRESERVE the creation time of the .ffs_tmp file, but SILENTLY "reuses" whatever creation time the old "file.txt" had!
            This "feature" is called "File System Tunneling":
            https://devblogs.microsoft.com/oldnewthing/?p=34923
            https://support.microsoft.com/kb/172190/en-us                                  */
        return result;
    }
    else
    {
        /* Note: non-transactional file copy solves at least four problems:
                -> skydrive - doesn't allow for .ffs_tmp extension and returns ERROR_INVALID_PARAMETER
                -> network renaming issues
                -> allow for true delete before copy to handle low disk space problems
                -> higher performance on unbuffered drives (e.g. USB-sticks)                     */
        if (onDeleteTargetFile)
            onDeleteTargetFile();

        return copyFilePlain(apTarget); //throw FileError, ErrorFileLocked
    }
}


bool AFS::createFolderIfMissingRecursion(const AbstractPath& ap) //throw FileError
{
    const std::optional<AbstractPath> parentPath = getParentPath(ap);
    if (!parentPath) //device root
        return false;

    try //generally we expect that path already exists (see: versioning, base folder, log file path) => check first
    {
        if (getItemType(ap) != ItemType::file) //throw FileError
            return false;
    }
    catch (FileError&) {} //not yet existing or access error? let's find out...

    createFolderIfMissingRecursion(*parentPath); //throw FileError

    try
    {
        //already existing: fail
        createFolderPlain(ap); //throw FileError
        return true;
    }
    catch (FileError&)
    {
        try
        {
            if (getItemType(ap) != ItemType::file) //throw FileError
                return true; //already existing => possible, if createFolderIfMissingRecursion() is run in parallel
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

        const std::optional<ItemType> parentType = itemStillExists(*parentAfsPath); //throw FileError

        if (parentType && *parentType != ItemType::file /*obscure, but possible (and not an error)*/)
            try
            {
                traverseFolderFlat(*parentAfsPath, //throw FileError
                [&](const    FileInfo& fi) { if (fi.itemName == itemName) throw ItemType::file;    },
                [&](const  FolderInfo& fi) { if (fi.itemName == itemName) throw ItemType::folder;  },
                [&](const SymlinkInfo& si) { if (si.itemName == itemName) throw ItemType::symlink; });
            }
            catch (const ItemType&) //finding the item after getItemType() previously failed is exceptional
            {
                throw FileError(_("Temporary access error:") + L' ' + e.toString());
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

    //no error situation if directory is not existing! manual deletion relies on it!
    if (std::optional<ItemType> type = itemStillExists(afsPath)) //throw FileError
    {
        if (*type == ItemType::symlink)
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
        removeFilePlain(ap); //throw FileError
    }
    catch (const FileError& e)
    {
        try
        {
            if (!itemStillExists(ap)) //throw FileError
                return;
        }
        //abstract context => unclear which exception is more relevant/useless:
        catch (const FileError& e2) { throw FileError(replaceCpy(e.toString(), L"\n\n", L'\n'), replaceCpy(e2.toString(), L"\n\n", L'\n')); }

        throw;
    }
}


void AFS::removeSymlinkIfExists(const AbstractPath& ap) //throw FileError
{
    try
    {
        removeSymlinkPlain(ap); //throw FileError
    }
    catch (const FileError& e)
    {
        try
        {
            if (!itemStillExists(ap)) //throw FileError
                return;
        }
        //abstract context => unclear which exception is more relevant/useless:
        catch (const FileError& e2) { throw FileError(replaceCpy(e.toString(), L"\n\n", L'\n'), replaceCpy(e2.toString(), L"\n\n", L'\n')); }

        throw;
    }
}


void AFS::removeEmptyFolderIfExists(const AbstractPath& ap) //throw FileError
{
    try
    {
        removeFolderPlain(ap); //throw FileError
    }
    catch (const FileError& e)
    {
        try
        {
            if (!itemStillExists(ap)) //throw FileError
                return;
        }
        //abstract context => unclear which exception is more relevant/useless:
        catch (const FileError& e2) { throw FileError(replaceCpy(e.toString(), L"\n\n", L'\n'), replaceCpy(e2.toString(), L"\n\n", L'\n')); }

        throw;
    }
}

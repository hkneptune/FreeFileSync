// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "abstract.h"
#include <zen/serialize.h>
#include <zen/guid.h>
#include <zen/crc.h>
#include <zen/ring_buffer.h>
#include <typeindex>

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


AfsPath fff::sanitizeDeviceRelativePath(Zstring relPath)
{
    if constexpr (FILE_NAME_SEPARATOR != Zstr('/' )) replace(relPath, Zstr('/'),  FILE_NAME_SEPARATOR);
    if constexpr (FILE_NAME_SEPARATOR != Zstr('\\')) replace(relPath, Zstr('\\'), FILE_NAME_SEPARATOR);
    trim(relPath, TrimSide::both, [](Zchar c) { return c == FILE_NAME_SEPARATOR; });
    return AfsPath(relPath);
}


std::weak_ordering AFS::compareDevice(const AbstractFileSystem& lhs, const AbstractFileSystem& rhs)
{
    //note: in worst case, order is guaranteed to be stable only during each program run
    //caveat: typeid returns static type for pointers, dynamic type for references!!!
    if (const std::strong_ordering cmp = std::type_index(typeid(lhs)) <=> std::type_index(typeid(rhs));
        cmp != std::strong_ordering::equal)
        return cmp;

    return lhs.compareDeviceSameAfsType(rhs);
}


std::optional<AbstractPath> AFS::getParentPath(const AbstractPath& itemPath)
{
    if (const std::optional<AfsPath> parentPath = getParentPath(itemPath.afsPath))
        return AbstractPath(itemPath.afsDevice, *parentPath);

    return {};
}


std::optional<AfsPath> AFS::getParentPath(const AfsPath& itemPath)
{
    if (!itemPath.value.empty())
        return AfsPath(beforeLast(itemPath.value, FILE_NAME_SEPARATOR, IfNotFoundReturn::none));

    return {};
}


namespace
{
struct FlatTraverserCallback : public AFS::TraverserCallback
{
    FlatTraverserCallback(const std::function<void(const AFS::FileInfo&    fi)>& onFile,
                          const std::function<void(const AFS::FolderInfo&  fi)>& onFolder,
                          const std::function<void(const AFS::SymlinkInfo& si)>& onSymlink) :
        onFile_   (onFile),
        onFolder_ (onFolder),
        onSymlink_(onSymlink) {}

private:
    void                               onFile   (const AFS::FileInfo&    fi) override { if (onFile_)    onFile_   (fi); }
    std::shared_ptr<TraverserCallback> onFolder (const AFS::FolderInfo&  fi) override { if (onFolder_)  onFolder_ (fi); return nullptr; }
    HandleLink                         onSymlink(const AFS::SymlinkInfo& si) override { if (onSymlink_) onSymlink_(si); return TraverserCallback::HandleLink::skip; }

    HandleError reportDirError (const ErrorInfo& errorInfo)                          override { throw FileError(errorInfo.msg); }
    HandleError reportItemError(const ErrorInfo& errorInfo, const Zstring& itemName) override { throw FileError(errorInfo.msg); }

    const std::function<void(const AFS::FileInfo&    fi)> onFile_;
    const std::function<void(const AFS::FolderInfo&  fi)> onFolder_;
    const std::function<void(const AFS::SymlinkInfo& si)> onSymlink_;
};
}


void AFS::traverseFolder(const AfsPath& folderPath, //throw FileError
                         const std::function<void(const FileInfo&    fi)>& onFile,
                         const std::function<void(const FolderInfo&  fi)>& onFolder,
                         const std::function<void(const SymlinkInfo& si)>& onSymlink) const
{
    auto ft = std::make_shared<FlatTraverserCallback>(onFile, onFolder, onSymlink); //throw FileError
    traverseFolderRecursive({{folderPath, ft}}, 1 /*parallelOps*/); //throw FileError
}


//already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
AFS::FileCopyResult AFS::copyFileAsStream(const AfsPath& sourcePath, const StreamAttributes& attrSource, //throw FileError, ErrorFileLocked, X
                                          const AbstractPath& targetPath, const IoCallback& notifyUnbufferedIO /*throw X*/) const
{
    int64_t totalBytesNotified = 0;
    IOCallbackDivider notifyIoDiv(notifyUnbufferedIO, totalBytesNotified);

    int64_t totalBytesRead    = 0;
    int64_t totalBytesWritten = 0;
    IoCallback /*[!] not auto!*/ notifyUnbufferedRead  = [&](int64_t bytesDelta) { totalBytesRead    += bytesDelta; notifyIoDiv(bytesDelta); };
    IoCallback                   notifyUnbufferedWrite = [&](int64_t bytesDelta) { totalBytesWritten += bytesDelta; notifyIoDiv(bytesDelta); };
    //--------------------------------------------------------------------------------------------------------

    auto streamIn = getInputStream(sourcePath); //throw FileError, ErrorFileLocked

    StreamAttributes attrSourceNew = {};
    //try to get the most current attributes if possible (input file might have changed after comparison!)
    if (std::optional<StreamAttributes> attr = streamIn->tryGetAttributesFast()) //throw FileError
        attrSourceNew = *attr; //Native/MTP/Google Drive
    else //use possibly stale ones:
        attrSourceNew = attrSource; //SFTP/FTP
    //TODO: evaluate: consequences of stale attributes

    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    auto streamOut = getOutputStream(targetPath, attrSourceNew.fileSize, attrSourceNew.modTime); //throw FileError


    unbufferedStreamCopy([&](void* buffer, size_t bytesToRead)
    {
        return streamIn->tryRead(buffer, bytesToRead, notifyUnbufferedRead); //throw FileError, ErrorFileLocked, X
    },
    streamIn->getBlockSize() /*throw FileError*/,

    [&](const void* buffer, size_t bytesToWrite)
    {
        return streamOut->tryWrite(buffer, bytesToWrite, notifyUnbufferedWrite); //throw FileError, X
    },
    streamOut->getBlockSize() /*throw FileError*/); //throw FileError, ErrorFileLocked, X


    //check incomplete input *before* failing with (slightly) misleading error message in OutputStream::finalize()
    if (totalBytesRead != makeSigned(attrSourceNew.fileSize))
        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(getDisplayPath(sourcePath))),
                        _("Unexpected size of data stream:") + L' ' + formatNumber(totalBytesRead) + L'\n' +
                        _("Expected:") + L' ' + formatNumber(attrSourceNew.fileSize) + L" [notifyUnbufferedRead]");

    const FinalizeResult finResult = streamOut->finalize(notifyUnbufferedWrite); //throw FileError, X

    ZEN_ON_SCOPE_FAIL(try { removeFilePlain(targetPath); }
    catch (const FileError& e) { logExtraError(e.toString()); }); //after finalize(): not guarded by ~AFS::OutputStream() anymore!

    //catch file I/O bugs + read/write conflicts: (note: different check than inside AFS::OutputStream::finalize() => checks notifyUnbufferedIO()!)
    if (totalBytesWritten != totalBytesRead)
        throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getDisplayPath(targetPath))),
                        _("Unexpected size of data stream:") + L' ' + formatNumber(totalBytesWritten) + L'\n' +
                        _("Expected:") + L' ' + formatNumber(totalBytesRead) + L" [notifyUnbufferedWrite]");
    return
    {
        .fileSize        = attrSourceNew.fileSize,
        .modTime         = attrSourceNew.modTime,
        .sourceFilePrint = attrSourceNew.filePrint,
        .targetFilePrint = finResult.filePrint,
        .errorModTime    = finResult.errorModTime,
        /* Failing to set modification time is not a fatal error from synchronization perspective (treat like external update)
                => Support additional scenarios:
                - GVFS failing to set modTime for FTP: https://freefilesync.org/forum/viewtopic.php?t=2372
                - GVFS failing to set modTime for MTP: https://freefilesync.org/forum/viewtopic.php?t=2803
                - MTP failing to set modTime in general: fail non-silently rather than silently during file creation
                - FTP failing to set modTime for servers without MFMT-support    */
    };
}


//already existing + no onDeleteTargetFile: undefined behavior! (e.g. fail/overwrite/auto-rename)
AFS::FileCopyResult AFS::copyFileTransactional(const AbstractPath& sourcePath, const StreamAttributes& attrSource, //throw FileError, ErrorFileLocked, X
                                               const AbstractPath& targetPath,
                                               bool copyFilePermissions,
                                               bool transactionalCopy,
                                               const std::function<void()>& onDeleteTargetFile,
                                               const IoCallback& notifyUnbufferedIO /*throw X*/)
{
    auto copyFilePlain = [&](const AbstractPath& targetPathTmp)
    {
        //caveat: typeid returns static type for pointers, dynamic type for references!!!
        if (typeid(sourcePath.afsDevice.ref()) == typeid(targetPathTmp.afsDevice.ref()))
            return sourcePath.afsDevice.ref().copyFileForSameAfsType(sourcePath.afsPath, attrSource,
                                                                     targetPathTmp, copyFilePermissions, notifyUnbufferedIO); //throw FileError, ErrorFileLocked, X
        //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)

        //fall back to stream-based file copy:
        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(getDisplayPath(targetPathTmp))),
                            _("Operation not supported between different devices."));

        //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
        return sourcePath.afsDevice.ref().copyFileAsStream(sourcePath.afsPath, attrSource, targetPathTmp, notifyUnbufferedIO); //throw FileError, ErrorFileLocked, X
    };

    if (transactionalCopy && !hasNativeTransactionalCopy(targetPath))
    {
        const std::optional<AbstractPath> parentPath = getParentPath(targetPath);
        if (!parentPath)
            throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getDisplayPath(targetPath))), L"Path is device root.");
        const Zstring fileName = getItemName(targetPath);

        //- generate (hopefully) unique file name to avoid clashing with some remnant ffs_tmp file
        //- do not loop: avoid pathological cases, e.g. https://freefilesync.org/forum/viewtopic.php?t=1592
        Zstring tmpName = beforeLast(fileName, Zstr('.'), IfNotFoundReturn::all);

        //don't make the temp name longer than the original when hitting file system name length limitations: "lpMaximumComponentLength is commonly 255 characters"
        while (tmpName.size() > 200) //BUT don't trim short names! we want early failure on filename-related issues
            tmpName = getUnicodeSubstring<Zstring>(tmpName, 0 /*uniPosFirst*/, unicodeLength(tmpName) / 2 /*uniPosLast*/); //consider UTF encoding when cutting in the middle! (e.g. for macOS)

        const Zstring& shortGuid = printNumber<Zstring>(Zstr("%04x"), static_cast<unsigned int>(getCrc16(generateGUID())));

        const AbstractPath targetPathTmp = appendRelPath(*parentPath, tmpName + Zstr('-') + //don't use '~': some FTP servers *silently* replace it with '_'!
                                                         shortGuid + TEMP_FILE_ENDING);
        //-------------------------------------------------------------------------------------------

        const FileCopyResult result = copyFilePlain(targetPathTmp); //throw FileError, ErrorFileLocked

        //transactional behavior: ensure cleanup; not needed before copyFilePlain() which is already transactional
        ZEN_ON_SCOPE_FAIL( try { removeFilePlain(targetPathTmp); }
        catch (const FileError& e) { logExtraError(e.toString()); });

        //have target file deleted (after read access on source and target has been confirmed) => allow for almost transactional overwrite
        if (onDeleteTargetFile)
            onDeleteTargetFile(); //throw X

        //already existing: undefined behavior! (e.g. fail/overwrite)
        moveAndRenameItem(targetPathTmp, targetPath); //throw FileError, (ErrorMoveUnsupported)
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

        return copyFilePlain(targetPath); //throw FileError, ErrorFileLocked
    }
}


void AFS::createFolderIfMissingRecursion(const AbstractPath& folderPath) //throw FileError
{
    auto getItemType2 = [&](const AbstractPath& itemPath) //throw FileError
    {
        try
        { return getItemType(itemPath); } //throw FileError
        catch (const FileError& e) //need to add context!
        {
            throw FileError(replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(getDisplayPath(folderPath))),
                            replaceCpy(e.toString(), L"\n\n", L'\n'));
        }
    };

    try
    {
        //- path most likely already exists (see: versioning, base folder, log file path) => check first
        //- do NOT use getItemTypeIfExists()! race condition when multiple threads are calling createDirectoryIfMissingRecursion(): https://freefilesync.org/forum/viewtopic.php?t=10137#p38062
        //- find first existing + accessible parent folder (backwards iteration):
        AbstractPath folderPathEx = folderPath;
        RingBuffer<Zstring> folderNames; //caveat: 1. might have been created in the meantime 2. getItemType2() may have failed with access error
        for (;;)
            try
            {
                if (getItemType2(folderPathEx) == ItemType::file /*obscure, but possible*/) //throw FileError
                    throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(getItemName(folderPathEx))));
                break;
            }
            catch (FileError&) //not yet existing or access error
            {
                const std::optional<AbstractPath> parentPath = getParentPath(folderPathEx);
                if (!parentPath)//device root => quick access test
                    throw;
                folderNames.push_front(getItemName(folderPathEx));
                folderPathEx = *parentPath;
            }
        //-----------------------------------------------------------

        AbstractPath folderPathNew = folderPathEx;
        for (const Zstring& folderName : folderNames)
            try
            {
                folderPathNew = appendRelPath(folderPathNew, folderName);

                createFolderPlain(folderPathNew); //throw FileError
            }
            catch (FileError&)
            {
                try
                {
                    if (getItemType2(folderPathNew) == ItemType::file /*obscure, but possible*/) //throw FileError
                        throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(getItemName(folderPathNew))));
                    else
                        continue; //already existing => possible, if createDirectoryIfMissingRecursion() is run in parallel
                }
                catch (FileError&) {} //not yet existing or access error

                throw;
            }
    }
    catch (const SysError& e)
    {
        throw FileError(replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(getDisplayPath(folderPath))), e.toString());
    }
}


//default implementation: folder traversal
void AFS::removeFolderIfExistsRecursion(const AfsPath& folderPath, //throw FileError
                                        const std::function<void(const std::wstring& displayPath)>& onBeforeFileDeletion    /*throw X*/, //
                                        const std::function<void(const std::wstring& displayPath)>& onBeforeSymlinkDeletion /*throw X*/, //optional; one call for each object!
                                        const std::function<void(const std::wstring& displayPath)>& onBeforeFolderDeletion  /*throw X*/) const
{
    std::function<void(const AfsPath& folderPath2)> removeFolderRecursionImpl;
    removeFolderRecursionImpl = [this, &onBeforeFileDeletion, &onBeforeSymlinkDeletion, &onBeforeFolderDeletion, &removeFolderRecursionImpl](const AfsPath& folderPath2) //throw FileError
    {
        std::vector<Zstring> folderNames;
        {
            std::vector<Zstring> fileNames;
            std::vector<Zstring> symlinkNames;
            try
            {
                traverseFolder(folderPath2, //throw FileError
                [&](const    FileInfo& fi) {    fileNames.push_back(fi.itemName); },
                [&](const  FolderInfo& fi) {  folderNames.push_back(fi.itemName); },
                [&](const SymlinkInfo& si) { symlinkNames.push_back(si.itemName); });
            }
            catch (const FileError& e) //add context
            {
                throw FileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(getDisplayPath(folderPath2))),
                                replaceCpy(e.toString(), L"\n\n", L'\n'));
            }

            for (const Zstring& fileName : fileNames)
            {
                const AfsPath filePath(appendPath(folderPath2.value, fileName));
                if (onBeforeFileDeletion)
                    onBeforeFileDeletion(getDisplayPath(filePath)); //throw X

                removeFilePlain(filePath); //throw FileError
            }

            for (const Zstring& symlinkName : symlinkNames)
            {
                const AfsPath linkPath(appendPath(folderPath2.value, symlinkName));
                if (onBeforeSymlinkDeletion)
                    onBeforeSymlinkDeletion(getDisplayPath(linkPath)); //throw X

                removeSymlinkPlain(linkPath); //throw FileError
            }
        } //=> save stack space and allow deletion of extremely deep hierarchies!

        for (const Zstring& folderName : folderNames)
            removeFolderRecursionImpl(AfsPath(appendPath(folderPath2.value, folderName))); //throw FileError

        if (onBeforeFolderDeletion)
            onBeforeFolderDeletion(getDisplayPath(folderPath2)); //throw X

        removeFolderPlain(folderPath2); //throw FileError
    };
    //--------------------------------------------------------------------------------------------------------------

    const std::optional<ItemType> type = [&]
    {
        try
        {
            return getItemTypeIfExists(folderPath); //throw FileError
        }
        catch (const FileError& e) //add context
        {
            throw FileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(getDisplayPath(folderPath))),
                            replaceCpy(e.toString(), L"\n\n", L'\n'));
        }
    }();

    if (type)
    {
        assert(*type != ItemType::symlink);

        if (*type == ItemType::symlink)
        {
            if (onBeforeSymlinkDeletion)
                onBeforeSymlinkDeletion(getDisplayPath(folderPath)); //throw X

            removeSymlinkPlain(folderPath); //throw FileError
        }
        else
            removeFolderRecursionImpl(folderPath); //throw FileError
    }
    else //no error situation if directory is not existing! manual deletion relies on it! significant I/O work was done => report:
        if (onBeforeFolderDeletion) onBeforeFolderDeletion(getDisplayPath(folderPath)); //throw X
}


void AFS::removeFileIfExists(const AbstractPath& filePath) //throw FileError
{
    try
    {
        removeFilePlain(filePath); //throw FileError
    }
    catch (const FileError& e)
    {
        try
        {
            if (!itemExists(filePath)) //throw FileError
                return;
        }
        //abstract context => unclear which exception is more relevant/useless:
        catch (const FileError& e2) { throw FileError(replaceCpy(e.toString(), L"\n\n", L'\n'), replaceCpy(e2.toString(), L"\n\n", L'\n')); }

        throw;
    }
}


void AFS::removeSymlinkIfExists(const AbstractPath& linkPath) //throw FileError
{
    try
    {
        removeSymlinkPlain(linkPath); //throw FileError
    }
    catch (const FileError& e)
    {
        try
        {
            if (!itemExists(linkPath)) //throw FileError
                return;
        }
        //abstract context => unclear which exception is more relevant/useless:
        catch (const FileError& e2) { throw FileError(replaceCpy(e.toString(), L"\n\n", L'\n'), replaceCpy(e2.toString(), L"\n\n", L'\n')); }

        throw;
    }
}


void AFS::removeEmptyFolderIfExists(const AbstractPath& folderPath) //throw FileError
{
    try
    {
        removeFolderPlain(folderPath); //throw FileError
    }
    catch (const FileError& e)
    {
        try
        {
            if (!itemExists(folderPath)) //throw FileError
                return;
        }
        //abstract context => unclear which exception is more relevant/useless:
        catch (const FileError& e2) { throw FileError(replaceCpy(e.toString(), L"\n\n", L'\n'), replaceCpy(e2.toString(), L"\n\n", L'\n')); }

        throw;
    }
}


void AFS::RecycleSession::moveToRecycleBinIfExists(const AbstractPath& itemPath, const Zstring& logicalRelPath) //throw FileError, RecycleBinUnavailable
{
    try
    {
        moveToRecycleBin(itemPath, logicalRelPath); //throw FileError, RecycleBinUnavailable
    }
    catch (RecycleBinUnavailable&) { throw; } //[!] no need for itemExists() file access!
    catch (const FileError& e)
    {
        try
        {
            if (!itemExists(itemPath)) //throw FileError
                return;
        }
        //abstract context => unclear which exception is more relevant/useless:
        catch (const FileError& e2) { throw FileError(replaceCpy(e.toString(), L"\n\n", L'\n'), replaceCpy(e2.toString(), L"\n\n", L'\n')); }

        throw;
    }
}


void AFS::moveToRecycleBinIfExists(const AbstractPath& itemPath) //throw FileError, RecycleBinUnavailable
{
    try
    {
        moveToRecycleBin(itemPath); //throw FileError, RecycleBinUnavailable
    }
    catch (RecycleBinUnavailable&) { throw; } //[!] no need for itemExists() file access!
    catch (const FileError& e)
    {
        try
        {
            if (!itemExists(itemPath)) //throw FileError
                return;
        }
        //abstract context => unclear which exception is more relevant/useless:
        catch (const FileError& e2) { throw FileError(replaceCpy(e.toString(), L"\n\n", L'\n'), replaceCpy(e2.toString(), L"\n\n", L'\n')); }

        throw;
    }
}

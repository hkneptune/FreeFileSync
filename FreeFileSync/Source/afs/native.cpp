// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "native.h"
#include <zen/file_access.h>
#include <zen/symlink_target.h>
#include <zen/file_io.h>
#include <zen/file_id_def.h>
#include <zen/stl_tools.h>
#include <zen/recycler.h>
#include <zen/thread.h>
#include <zen/guid.h>
#include <zen/crc.h>
#include "abstract_impl.h"
#include "../base/resolve_path.h"
#include "../base/icon_loader.h"


    #include <cstddef> //offsetof
    #include <sys/stat.h>
    #include <dirent.h>
    #include <fcntl.h> //fallocate, fcntl

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


namespace
{
void initComForThread() //throw FileError
{
}

//====================================================================================================
//====================================================================================================

inline
AFS::FileId convertToAbstractFileId(const zen::FileId& fid)
{
    if (fid == zen::FileId())
        return AFS::FileId();

    AFS::FileId out(reinterpret_cast<const char*>(&fid.volumeId),  sizeof(fid.volumeId));
    out.     append(reinterpret_cast<const char*>(&fid.fileIndex), sizeof(fid.fileIndex));
    return out;
}


struct NativeFileInfo
{
    time_t   modTime;
    uint64_t fileSize;
    FileId   fileId; //optional
};
NativeFileInfo getFileAttributes(FileBase::FileHandle fh) //throw SysError
{
    struct ::stat fileAttr = {};
    if (::fstat(fh, &fileAttr) != 0)
        THROW_LAST_SYS_ERROR(L"fstat");

    return
    {
        fileAttr.st_mtime,
        makeUnsigned(fileAttr.st_size),
        generateFileId(fileAttr)
    };
}


struct FsItemRaw
{
    Zstring itemName;
    Zstring itemPath;
};
std::vector<FsItemRaw> getDirContentFlat(const Zstring& dirPath) //throw FileError
{
    //no need to check for endless recursion:
    //1. Linux has a fixed limit on the number of symbolic links in a path
    //2. fails with "too many open files" or "path too long" before reaching stack overflow

    DIR* folder = ::opendir(dirPath.c_str()); //directory must NOT end with path separator, except "/"
    if (!folder)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot open directory %x."), L"%x", fmtPath(dirPath)), L"opendir");
    ZEN_ON_SCOPE_EXIT(::closedir(folder)); //never close nullptr handles! -> crash

    std::vector<FsItemRaw> output;
    for (;;)
    {
        /*
            Linux:
                http://man7.org/linux/man-pages/man3/readdir_r.3.html
                "It is recommended that applications use readdir(3) instead of readdir_r"
                "... in modern implementations (including the glibc implementation), concurrent calls to readdir(3) that specify different directory streams are thread-safe"

            macOS:
                - libc: readdir thread-safe already in code from 2000: https://opensource.apple.com/source/Libc/Libc-166/gen.subproj/readdir.c.auto.html
                - and in the latest version from 2017:                 https://opensource.apple.com/source/Libc/Libc-1244.30.3/gen/FreeBSD/readdir.c.auto.html
        */
        errno = 0;
        const struct ::dirent* dirEntry = ::readdir(folder);
        if (!dirEntry)
        {
            if (errno == 0) //errno left unchanged => no more items
                return output;

            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(dirPath)), L"readdir");
            //don't retry but restart dir traversal on error! https://devblogs.microsoft.com/oldnewthing/20140612-00/?p=753
        }

        const char* itemNameRaw = dirEntry->d_name; //evaluate dirEntry *before* going into recursion

        //skip "." and ".."
        if (itemNameRaw[0] == '.' &&
            (itemNameRaw[1] == 0 || (itemNameRaw[1] == '.' && itemNameRaw[2] == 0)))
            continue;

        /*
            Unicode normalization is file-system-dependent:

                OS                Accepts   Gives back
               ----------         -------   ----------
               macOS (HFS+)         all        NFD
               Linux                all      <input>
               Windows (NTFS, FAT)  all      <input>

            some file systems return precomposed others decomposed UTF8: https://developer.apple.com/library/mac/#qa/qa1173/_index.html
                  - OS X edit controls and text fields may return precomposed UTF as directly received by keyboard or decomposed UTF that was copy & pasted in!
                  - Posix APIs require decomposed form: https://freefilesync.org/forum/viewtopic.php?t=2480

            => General recommendation: always preserve input UNCHANGED (both unicode normalization and case sensitivity)
            => normalize only when needed during string comparison

            Create sample files on Linux: touch  decomposed-$'\x6f\xcc\x81'.txt
                                          touch precomposed-$'\xc3\xb3'.txt

            - SMB sharing case-sensitive or NFD file names is fundamentally broken on macOS:
                => the macOS SMB manager internally buffers file names as case-insensitive and NFC (= just like NTFS on Windows)
                => test: create SMB share from Linux => *boom* on macOS: "Error Code 2: No such file or directory [lstat]"
                    or WORSE: folders "test" and "Test" *both* incorrectly return the content of one of the two
        */
        const Zstring& itemName = itemNameRaw;
        if (itemName.empty())
            throw FileError(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(dirPath)), L"readdir: Data corruption; item with empty name.");

        const Zstring& itemPath = appendSeparator(dirPath) + itemName;

        output.push_back({ itemName, itemPath});
    }
}


struct ItemDetailsRaw
{
    ItemType type;
    time_t   modTime; //number of seconds since Jan. 1st 1970 UTC
    uint64_t fileSize; //unit: bytes!
    FileId   fileId;
};
ItemDetailsRaw getItemDetails(const Zstring& itemPath) //throw FileError
{
    struct ::stat statData = {};
    if (::lstat(itemPath.c_str(), &statData) != 0) //lstat() does not resolve symlinks
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(itemPath)), L"lstat");

    return { S_ISLNK(statData.st_mode) ? ItemType::SYMLINK : //on Linux there is no distinction between file and directory symlinks!
             (S_ISDIR(statData.st_mode) ? ItemType::FOLDER :
              ItemType::FILE), //a file or named pipe, etc. => dont't check using S_ISREG(): see comment in file_traverser.cpp
             statData.st_mtime, makeUnsigned(statData.st_size), generateFileId(statData) };
}

ItemDetailsRaw getSymlinkTargetDetails(const Zstring& linkPath) //throw FileError
{
    struct ::stat statData = {};
    if (::stat(linkPath.c_str(), &statData) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(linkPath)), L"stat");

    return { S_ISDIR(statData.st_mode) ? ItemType::FOLDER : ItemType::FILE, statData.st_mtime, makeUnsigned(statData.st_size), generateFileId(statData) };
}


class SingleFolderTraverser
{
public:
    SingleFolderTraverser(const std::vector<std::pair<Zstring, std::shared_ptr<AFS::TraverserCallback>>>& workload /*throw X*/)
    {
        for (const auto& [folderPath, cb] : workload)
            workload_.push_back({ folderPath, cb });

        while (!workload_.empty())
        {
            WorkItem wi = std::move(workload_.    back()); //yes, no strong exception guarantee (std::bad_alloc)
            /**/                    workload_.pop_back();  //

            tryReportingDirError([&] //throw X
            {
                traverseWithException(wi.dirPath, *wi.cb); //throw FileError, X
            }, *wi.cb);
        }
    }

private:
    SingleFolderTraverser           (const SingleFolderTraverser&) = delete;
    SingleFolderTraverser& operator=(const SingleFolderTraverser&) = delete;

    void traverseWithException(const Zstring& dirPath, AFS::TraverserCallback& cb) //throw FileError, X
    {
        for (const auto& [itemName, itemPath] : getDirContentFlat(dirPath)) //throw FileError
        {
            ItemDetailsRaw detailsRaw = {};
            if (!tryReportingItemError([&] //throw X
        {
            detailsRaw = getItemDetails(itemPath); //throw FileError
            }, cb, itemName))
            continue; //ignore error: skip file

            switch (detailsRaw.type)
            {
                case ItemType::FILE:
                    cb.onFile({ itemName, detailsRaw.fileSize, detailsRaw.modTime, convertToAbstractFileId(detailsRaw.fileId), nullptr /*symlinkInfo*/ }); //throw X
                    break;

                case ItemType::FOLDER:
                    if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb.onFolder({ itemName, nullptr /*symlinkInfo*/ })) //throw X
                        workload_.push_back({ itemPath, std::move(cbSub) });
                    break;

                case ItemType::SYMLINK:
                    switch (cb.onSymlink({ itemName, detailsRaw.modTime })) //throw X
                    {
                        case AFS::TraverserCallback::LINK_FOLLOW:
                        {
                            ItemDetailsRaw linkDetails = {};
                            if (!tryReportingItemError([&] //throw X
                        {
                            linkDetails = getSymlinkTargetDetails(itemPath); //throw FileError
                            }, cb, itemName))
                            continue;

                            const AFS::SymlinkInfo linkInfo = { itemName, linkDetails.modTime };

                            if (linkDetails.type == ItemType::FOLDER)
                            {
                                if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb.onFolder({ itemName, &linkInfo })) //throw X
                                    workload_.push_back({ itemPath, std::move(cbSub) });
                            }
                            else //a file or named pipe, etc.
                                cb.onFile({ itemName, linkDetails.fileSize, linkDetails.modTime, convertToAbstractFileId(linkDetails.fileId), &linkInfo }); //throw X
                        }
                        break;

                        case AFS::TraverserCallback::LINK_SKIP:
                            break;
                    }
                    break;
            }
        }
    }

    struct WorkItem
    {
        Zstring dirPath;
        std::shared_ptr<AFS::TraverserCallback> cb;
    };
    std::vector<WorkItem> workload_;
};


void traverseFolderRecursiveNative(const std::vector<std::pair<Zstring, std::shared_ptr<AFS::TraverserCallback>>>& workload /*throw X*/, size_t) //throw X
{
    SingleFolderTraverser dummy(workload); //throw X
}
//====================================================================================================
//====================================================================================================

class RecycleSessionNative : public AbstractFileSystem::RecycleSession
{
public:
    RecycleSessionNative(const Zstring& baseFolderPath) : baseFolderPath_(baseFolderPath) {}

    void recycleItemIfExists(const AbstractPath& itemPath, const Zstring& logicalRelPath) override; //throw FileError
    void tryCleanup(const std::function<void (const std::wstring& displayPath)>& notifyDeletionStatus /*throw X*/) override; //throw FileError, X

private:
    const Zstring baseFolderPath_; //ends with path separator
};

//===========================================================================================================================

struct InputStreamNative : public AbstractFileSystem::InputStream
{
    InputStreamNative(const Zstring& filePath, const IOCallback& notifyUnbufferedIO /*throw X*/) : fi_(filePath, notifyUnbufferedIO) {} //throw FileError, ErrorFileLocked

    size_t read(void* buffer, size_t bytesToRead) override { return fi_.read(buffer, bytesToRead); } //throw FileError, ErrorFileLocked, X; return "bytesToRead" bytes unless end of stream!
    size_t getBlockSize() const override { return fi_.getBlockSize(); } //non-zero block size is AFS contract!
    std::optional<AFS::StreamAttributes> getAttributesBuffered() override //throw FileError
    {
        try
        {
            const NativeFileInfo fileInfo = getFileAttributes(fi_.getHandle()); //throw SysError
            return
                AFS::StreamAttributes(
            {
                fileInfo.modTime,
                fileInfo.fileSize,
                convertToAbstractFileId(fileInfo.fileId)
            });
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(fi_.getFilePath())), e.toString()); }
    }

private:
    FileInput fi_;
};

//===========================================================================================================================

struct OutputStreamNative : public AbstractFileSystem::OutputStreamImpl
{
    OutputStreamNative(const Zstring& filePath,
                       std::optional<uint64_t> streamSize,
                       std::optional<time_t> modTime,
                       const IOCallback& notifyUnbufferedIO /*throw X*/) :
        fo_(FileOutput::ACC_CREATE_NEW, filePath, notifyUnbufferedIO), //throw FileError, ErrorTargetExisting
        modTime_(modTime)
    {
        if (streamSize) //pre-allocate file space, because we can
            fo_.preAllocateSpaceBestEffort(*streamSize); //throw FileError
    }

    void write(const void* buffer, size_t bytesToWrite) override { fo_.write(buffer, bytesToWrite); } //throw FileError, X

    AFS::FinalizeResult finalize() override //throw FileError, X
    {
        AFS::FinalizeResult result;
        try
        {
            result.fileId = convertToAbstractFileId(getFileAttributes(fo_.getHandle()).fileId); //throw SysError
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(fo_.getFilePath())), e.toString()); }

        fo_.finalize(); //throw FileError, X

        try
        {
            if (modTime_)
                zen::setFileTime(fo_.getFilePath(), *modTime_, ProcSymlink::FOLLOW); //throw FileError
            /* is setting modtime after closing the file handle a pessimization?
                Native: no, needed for functional correctness, see file_access.cpp */
        }
        catch (const FileError& e) { result.errorModTime = FileError(e.toString()); /*avoid slicing*/ }

        return result;
    }

private:
    FileOutput fo_;
    const std::optional<time_t> modTime_;
};

//===========================================================================================================================

class NativeFileSystem : public AbstractFileSystem
{
public:
    NativeFileSystem(const Zstring& rootPath) : rootPath_(rootPath) {}

private:
    Zstring getNativePath(const AfsPath& afsPath) const { return nativeAppendPaths(rootPath_, afsPath.value); }

    std::optional<Zstring> getNativeItemPath(const AfsPath& afsPath) const override { return getNativePath(afsPath); }

    Zstring getInitPathPhrase(const AfsPath& afsPath) const override { return getNativePath(afsPath); }

    std::wstring getDisplayPath(const AfsPath& afsPath) const override { return utfTo<std::wstring>(getNativePath(afsPath)); }

    bool isNullFileSystem() const override { return rootPath_.empty(); }

    int compareDeviceSameAfsType(const AbstractFileSystem& afsRhs) const override
    {
        const Zstring& rootPathRhs = static_cast<const NativeFileSystem&>(afsRhs).rootPath_;

        return compareNativePath(rootPath_, rootPathRhs);
    }

    //----------------------------------------------------------------------------------------------------------------
    ItemType getItemType(const AfsPath& afsPath) const override //throw FileError
    {
        initComForThread(); //throw FileError
        switch (zen::getItemType(getNativePath(afsPath))) //throw FileError
        {
            case zen::ItemType::FILE:
                return AFS::ItemType::FILE;
            case zen::ItemType::FOLDER:
                return AFS::ItemType::FOLDER;
            case zen::ItemType::SYMLINK:
                return AFS::ItemType::SYMLINK;
        }
        assert(false);
        return AFS::ItemType::FILE;
    }

    std::optional<ItemType> itemStillExists(const AfsPath& afsPath) const override //throw FileError
    {
        //default implementation: folder traversal
        return AbstractFileSystem::itemStillExists(afsPath); //throw FileError
    }
    //----------------------------------------------------------------------------------------------------------------

    //already existing: fail/ignore
    //=> Native will fail and give a clear error message
    void createFolderPlain(const AfsPath& afsPath) const override //throw FileError
    {
        initComForThread(); //throw FileError
        createDirectory(getNativePath(afsPath)); //throw FileError, ErrorTargetExisting
    }

    void removeFilePlain(const AfsPath& afsPath) const override //throw FileError
    {
        initComForThread(); //throw FileError
        zen::removeFilePlain(getNativePath(afsPath)); //throw FileError
    }

    void removeSymlinkPlain(const AfsPath& afsPath) const override //throw FileError
    {
        initComForThread(); //throw FileError
        zen::removeSymlinkPlain(getNativePath(afsPath)); //throw FileError
    }

    void removeFolderPlain(const AfsPath& afsPath) const override //throw FileError
    {
        initComForThread(); //throw FileError
        zen::removeDirectoryPlain(getNativePath(afsPath)); //throw FileError
    }

    void removeFolderIfExistsRecursion(const AfsPath& afsPath, //throw FileError
                                       const std::function<void (const std::wstring& displayPath)>& onBeforeFileDeletion /*throw X*/, //optional
                                       const std::function<void (const std::wstring& displayPath)>& onBeforeFolderDeletion) const override //one call for each object!
    {
        //default implementation: folder traversal
        AbstractFileSystem::removeFolderIfExistsRecursion(afsPath, onBeforeFileDeletion, onBeforeFolderDeletion); //throw FileError, X
    }

    //----------------------------------------------------------------------------------------------------------------
    AbstractPath getSymlinkResolvedPath(const AfsPath& afsPath) const override //throw FileError
    {
        initComForThread(); //throw FileError
        const Zstring nativePath = getNativePath(afsPath);

        const Zstring resolvedPath = zen::getSymlinkResolvedPath(nativePath); //throw FileError
        const std::optional<zen::PathComponents> comp = parsePathComponents(resolvedPath);
        if (!comp)
            throw FileError(replaceCpy(_("Cannot determine final path for %x."), L"%x", fmtPath(nativePath)),
                            replaceCpy<std::wstring>(L"Invalid path %x.", L"%x", fmtPath(resolvedPath)));

        return AbstractPath(makeSharedRef<NativeFileSystem>(comp->rootPath), AfsPath(comp->relPath));
    }

    std::string getSymlinkBinaryContent(const AfsPath& afsPath) const override //throw FileError
    {
        initComForThread(); //throw FileError
        const Zstring nativePath = getNativePath(afsPath);

        std::string content = utfTo<std::string>(getSymlinkTargetRaw(nativePath)); //throw FileError
        return content;
    }
    //----------------------------------------------------------------------------------------------------------------

    //return value always bound:
    std::unique_ptr<InputStream> getInputStream(const AfsPath& afsPath, const IOCallback& notifyUnbufferedIO /*throw X*/) const override //throw FileError, ErrorFileLocked
    {
        initComForThread(); //throw FileError
        return std::make_unique<InputStreamNative>(getNativePath(afsPath), notifyUnbufferedIO); //throw FileError, ErrorFileLocked
    }

    //target existing: undefined behavior! (fail/overwrite/auto-rename) => Native will fail and give a clear error message
    std::unique_ptr<OutputStreamImpl> getOutputStream(const AfsPath& afsPath, //throw FileError
                                                      std::optional<uint64_t> streamSize,
                                                      std::optional<time_t> modTime,
                                                      const IOCallback& notifyUnbufferedIO /*throw X*/) const override
    {
        initComForThread(); //throw FileError
        return std::make_unique<OutputStreamNative>(getNativePath(afsPath), streamSize, modTime, notifyUnbufferedIO); //throw FileError
    }

    //----------------------------------------------------------------------------------------------------------------
    void traverseFolderRecursive(const TraverserWorkload& workload /*throw X*/, size_t parallelOps) const override
    {
        //initComForThread() -> done on traverser worker threads

        std::vector<std::pair<Zstring, std::shared_ptr<TraverserCallback>>> initialWorkItems;
        for (const auto& [folderPath, cb] : workload)
            initialWorkItems.emplace_back(getNativePath(folderPath), cb);

        traverseFolderRecursiveNative(initialWorkItems, parallelOps); //throw X
    }
    //----------------------------------------------------------------------------------------------------------------

    //symlink handling: follow link!
    //target existing: undefined behavior! (fail/overwrite/auto-rename) => Native will fail and give a clear error message
    FileCopyResult copyFileForSameAfsType(const AfsPath& afsPathSource, const StreamAttributes& attrSource, //throw FileError, ErrorFileLocked, X
                                          const AbstractPath& apTarget, bool copyFilePermissions, const IOCallback& notifyUnbufferedIO /*throw X*/) const override
    {
        const Zstring nativePathTarget = static_cast<const NativeFileSystem&>(apTarget.afsDevice.ref()).getNativePath(apTarget.afsPath);

        initComForThread(); //throw FileError

        const zen::FileCopyResult nativeResult = copyNewFile(getNativePath(afsPathSource), nativePathTarget, //throw FileError, ErrorTargetExisting, ErrorFileLocked, X
                                                             copyFilePermissions, notifyUnbufferedIO);
        FileCopyResult result;
        result.fileSize     = nativeResult.fileSize;
        result.modTime      = nativeResult.modTime;
        result.sourceFileId = convertToAbstractFileId(nativeResult.sourceFileId);
        result.targetFileId = convertToAbstractFileId(nativeResult.targetFileId);
        result.errorModTime = nativeResult.errorModTime;
        return result;
    }

    //target existing: fail/ignore => Native will fail and give a clear error message
    //symlink handling: follow link!
    void copyNewFolderForSameAfsType(const AfsPath& afsPathSource, const AbstractPath& apTarget, bool copyFilePermissions) const override //throw FileError
    {
        initComForThread(); //throw FileError

        const Zstring& sourcePath = getNativePath(afsPathSource);
        const Zstring& targetPath = static_cast<const NativeFileSystem&>(apTarget.afsDevice.ref()).getNativePath(apTarget.afsPath);

        zen::createDirectory(targetPath); //throw FileError, ErrorTargetExisting

        ZEN_ON_SCOPE_FAIL(try { removeDirectoryPlain(targetPath); }
        catch (FileError&) {});

        //do NOT copy attributes for volume root paths which return as: FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY
        //https://freefilesync.org/forum/viewtopic.php?t=5550
        if (getParentPath(afsPathSource)) //=> not a root path
            tryCopyDirectoryAttributes(sourcePath, targetPath); //throw FileError

        if (copyFilePermissions)
            copyItemPermissions(sourcePath, targetPath, ProcSymlink::FOLLOW); //throw FileError
    }

    void copySymlinkForSameAfsType(const AfsPath& afsPathSource, const AbstractPath& apTarget, bool copyFilePermissions) const override //throw FileError
    {
        const Zstring nativePathTarget = static_cast<const NativeFileSystem&>(apTarget.afsDevice.ref()).getNativePath(apTarget.afsPath);

        initComForThread(); //throw FileError
        zen::copySymlink(getNativePath(afsPathSource), nativePathTarget, copyFilePermissions); //throw FileError
    }

    //target existing: undefined behavior! (fail/overwrite/auto-rename) => Native will fail and give a clear error message
    void moveAndRenameItemForSameAfsType(const AfsPath& pathFrom, const AbstractPath& pathTo) const override //throw FileError, ErrorMoveUnsupported
    {
        //perf test: detecting different volumes by path is ~30 times faster than having ::MoveFileEx() fail with ERROR_NOT_SAME_DEVICE (6µs vs 190µs)
        //=> maybe we can even save some actual I/O in some cases?
        if (compareDeviceSameAfsType(pathTo.afsDevice.ref()) != 0)
            throw ErrorMoveUnsupported(replaceCpy(replaceCpy(_("Cannot move file %x to %y."),
                                                             L"%x", L"\n" + fmtPath(getDisplayPath(pathFrom))),
                                                  L"%y", L"\n" + fmtPath(AFS::getDisplayPath(pathTo))),
                                       _("Operation not supported between different devices."));
        initComForThread(); //throw FileError
        const Zstring nativePathTarget = static_cast<const NativeFileSystem&>(pathTo.afsDevice.ref()).getNativePath(pathTo.afsPath);
        zen::moveAndRenameItem(getNativePath(pathFrom), nativePathTarget, false /*replaceExisting*/); //throw FileError, ErrorTargetExisting, ErrorMoveUnsupported
    }

    bool supportsPermissions(const AfsPath& afsPath) const override //throw FileError
    {
        initComForThread(); //throw FileError
        return zen::supportsPermissions(getNativePath(afsPath));
    }

    //----------------------------------------------------------------------------------------------------------------
    ImageHolder getFileIcon(const AfsPath& afsPath, int pixelSize) const override //noexcept; optional return value
    {
        try
        {
            initComForThread(); //throw FileError
            return fff::getFileIcon(getNativePath(afsPath), pixelSize);
        }
        catch (FileError&) { assert(false); return ImageHolder(); }
    }

    ImageHolder getThumbnailImage(const AfsPath& afsPath, int pixelSize) const override //noexcept; optional return value
    {
        try
        {
            initComForThread(); //throw FileError
            return fff::getThumbnailImage(getNativePath(afsPath), pixelSize);
        }
        catch (FileError&) { assert(false); return ImageHolder(); }
    }

    void authenticateAccess(bool allowUserInteraction) const override //throw FileError
    {
    }

    int getAccessTimeout() const override { return 0; } //returns "0" if no timeout in force

    bool hasNativeTransactionalCopy() const override { return false; }
    //----------------------------------------------------------------------------------------------------------------

    uint64_t getFreeDiskSpace(const AfsPath& afsPath) const override //throw FileError, returns 0 if not available
    {
        initComForThread(); //throw FileError
        return zen::getFreeDiskSpace(getNativePath(afsPath)); //throw FileError
    }

    bool supportsRecycleBin(const AfsPath& afsPath) const override //throw FileError
    {
        return true; //truth be told: no idea!!!
    }

    std::unique_ptr<RecycleSession> createRecyclerSession(const AfsPath& afsPath) const override //throw FileError, return value must be bound!
    {
        initComForThread(); //throw FileError
        assert(supportsRecycleBin(afsPath));
        return std::make_unique<RecycleSessionNative>(getNativePath(afsPath));
    }

    void recycleItemIfExists(const AfsPath& afsPath) const override //throw FileError
    {
        initComForThread(); //throw FileError
        zen::recycleOrDeleteIfExists(getNativePath(afsPath)); //throw FileError
    }

    const Zstring rootPath_;
};

//===========================================================================================================================



//- return true if item existed
//- multi-threaded access: internally synchronized!
void RecycleSessionNative::recycleItemIfExists(const AbstractPath& itemPath, const Zstring& logicalRelPath) //throw FileError
{
    assert(!startsWith(logicalRelPath, FILE_NAME_SEPARATOR));

    std::optional<Zstring> itemPathNative = AFS::getNativeItemPath(itemPath);
    if (!itemPathNative)
        throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    recycleOrDeleteIfExists(*itemPathNative); //throw FileError
}


void RecycleSessionNative::tryCleanup(const std::function<void (const std::wstring& displayPath)>& notifyDeletionStatus /*throw X*/) //throw FileError, X
{
}
}


//coordinate changes with getResolvedFilePath()!
bool fff::acceptsItemPathPhraseNative(const Zstring& itemPathPhrase) //noexcept
{
    Zstring path = expandMacros(itemPathPhrase); //expand before trimming!
    trim(path);


    if (startsWith(path, Zstr("["))) //drive letter by volume name syntax
        return true;

    //don't accept relative paths!!! indistinguishable from MTP paths as shown in Explorer's address bar!
    //don't accept empty paths (see drag & drop validation!)
    return static_cast<bool>(parsePathComponents(path));
}


AbstractPath fff::createItemPathNative(const Zstring& itemPathPhrase) //noexcept
{
    //TODO: get volume by name hangs for idle HDD! => run createItemPathNative during getFolderStatusNonBlocking() but getResolvedFilePath currently not thread-safe!
    const Zstring itemPath = getResolvedFilePath(itemPathPhrase);
    return createItemPathNativeNoFormatting(itemPath);
}


AbstractPath fff::createItemPathNativeNoFormatting(const Zstring& nativePath) //noexcept
{
    if (const std::optional<PathComponents> comp = parsePathComponents(nativePath))
        return AbstractPath(makeSharedRef<NativeFileSystem>(comp->rootPath), AfsPath(comp->relPath));
    else //broken path syntax
        return AbstractPath(makeSharedRef<NativeFileSystem>(nativePath), AfsPath());
}

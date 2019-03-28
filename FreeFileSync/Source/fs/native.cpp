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
            //don't retry but restart dir traversal on error! https://blogs.msdn.microsoft.com/oldnewthing/20140612-00/?p=753/
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

            some file systems return precomposed others decomposed UTF8: http://developer.apple.com/library/mac/#qa/qa1173/_index.html
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

    if (S_ISLNK(statData.st_mode)) //on Linux there is no distinction between file and directory symlinks!
        return { ItemType::SYMLINK, statData.st_mtime, 0, generateFileId(statData) };

    else if (S_ISDIR(statData.st_mode)) //a directory
        return { ItemType::FOLDER, statData.st_mtime, 0, generateFileId(statData) };

    else //a file or named pipe, etc. => dont't check using S_ISREG(): see comment in file_traverser.cpp
        return { ItemType::FILE, statData.st_mtime, makeUnsigned(statData.st_size), generateFileId(statData) };
}

ItemDetailsRaw getSymlinkTargetDetails(const Zstring& linkPath) //throw FileError
{
    struct ::stat statData = {};
    if (::stat(linkPath.c_str(), &statData) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot resolve symbolic link %x."), L"%x", fmtPath(linkPath)), L"stat");

    if (S_ISDIR(statData.st_mode)) //a directory
        return { ItemType::FOLDER, statData.st_mtime, 0, generateFileId(statData) };
    else //a file or named pipe, etc.
        return { ItemType::FILE, statData.st_mtime, makeUnsigned(statData.st_size), generateFileId(statData) };
}


struct GetDirDetails
{
    GetDirDetails(const Zstring& dirPath) : dirPath_(dirPath) {}

    using Result = std::vector<FsItemRaw>;
    Result operator()() const
    {
        return getDirContentFlat(dirPath_); //throw FileError
    }

private:
    Zstring dirPath_;
};


struct GetItemDetails //details not already retrieved by raw folder traversal
{
    GetItemDetails(const FsItemRaw& rawItem) : rawItem_(rawItem) {}

    struct Result
    {
        FsItemRaw raw;
        ItemDetailsRaw details;
    };
    Result operator()() const
    {
        return { rawItem_, getItemDetails(rawItem_.itemPath) }; //throw FileError
    }

private:
    FsItemRaw rawItem_;
};


struct GetLinkTargetDetails
{
    GetLinkTargetDetails(const FsItemRaw& rawItem, const ItemDetailsRaw& linkDetails) : rawItem_(rawItem), linkDetails_(linkDetails) {}

    struct Result
    {
        FsItemRaw raw;
        ItemDetailsRaw link;
        ItemDetailsRaw target;
    };
    Result operator()() const
    {
        return { rawItem_, linkDetails_, getSymlinkTargetDetails(rawItem_.itemPath) }; //throw FileError
    }

private:
    FsItemRaw rawItem_;
    ItemDetailsRaw linkDetails_;
};


void traverseFolderRecursiveNative(const std::vector<std::pair<Zstring, std::shared_ptr<AFS::TraverserCallback>>>& initialTasks /*throw X*/, size_t parallelOps)
{
    std::vector<Task<TravContext, GetDirDetails>> genItems;

    for (const auto& [folderPath, cb] : initialTasks)
        genItems.push_back({ GetDirDetails(folderPath),
                             TravContext{ Zstring() /*errorItemName*/, 0 /*errorRetryCount*/, cb /*TraverserCallback*/ }});

    GenericDirTraverser<GetDirDetails, GetItemDetails, GetLinkTargetDetails>(std::move(genItems), parallelOps, "Native Traverser"); //throw X
}
}


template <>
template <>
void GenericDirTraverser<GetDirDetails, GetItemDetails, GetLinkTargetDetails>::evalResultValue<GetDirDetails>(const GetDirDetails::Result& r, std::shared_ptr<AFS::TraverserCallback>& cb /*throw X*/)
{
    //attention: if we simply appended to the work queue this would repeatedly allow for situations where a large number of directories are traversed one after another
    //           without intermittent calls to evalResultValue<GetItemDetails>() => user incorrectly thinks the app is hanging! https://freefilesync.org/forum/viewtopic.php?t=5729
    //solution: *prepend* GetItemDetails() tasks (in correct order) to the work queue ASAP:
    std::for_each(r.rbegin(), r.rend(), [&](const FsItemRaw& rawItem)
    {
        scheduler_.run<GetItemDetails>({ GetItemDetails(rawItem), TravContext{ rawItem.itemName, 0 /*errorRetryCount*/, cb }},
                                       true /*insertFront*/);
    });
}


template <>
template <>
void GenericDirTraverser<GetDirDetails, GetItemDetails, GetLinkTargetDetails>::evalResultValue<GetItemDetails>(const GetItemDetails::Result& r, std::shared_ptr<AFS::TraverserCallback>& cb /*throw X*/)
{
    switch (r.details.type)
    {
        case ItemType::FILE:
            cb->onFile({ r.raw.itemName, r.details.fileSize, r.details.modTime, convertToAbstractFileId(r.details.fileId), nullptr /*symlinkInfo*/ }); //throw X
            break;

        case ItemType::FOLDER:
            if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb->onFolder({ r.raw.itemName, nullptr /*symlinkInfo*/ })) //throw X
                scheduler_.run<GetDirDetails>({ GetDirDetails(r.raw.itemPath), TravContext{ Zstring() /*errorItemName*/, 0 /*errorRetryCount*/, std::move(cbSub) }});
            break;

        case ItemType::SYMLINK:
            switch (cb->onSymlink({ r.raw.itemName, r.details.modTime })) //throw X
            {
                case AFS::TraverserCallback::LINK_FOLLOW:
                    scheduler_.run<GetLinkTargetDetails>({ GetLinkTargetDetails(r.raw, r.details), TravContext{ r.raw.itemName, 0 /*errorRetryCount*/, cb }});
                    break;

                case AFS::TraverserCallback::LINK_SKIP:
                    break;
            }
            break;
    }
}


template <>
template <>
void GenericDirTraverser<GetDirDetails, GetItemDetails, GetLinkTargetDetails>::evalResultValue<GetLinkTargetDetails>(const GetLinkTargetDetails::Result& r, std::shared_ptr<AFS::TraverserCallback>& cb /*throw X*/)
{
    assert(r.link.type == ItemType::SYMLINK && r.target.type != ItemType::SYMLINK);

    const AFS::SymlinkInfo linkInfo = { r.raw.itemName, r.link.modTime };

    if (r.target.type == ItemType::FOLDER)
    {
        if (std::shared_ptr<AFS::TraverserCallback> cbSub = cb->onFolder({ r.raw.itemName, &linkInfo })) //throw X
            scheduler_.run<GetDirDetails>({ GetDirDetails(r.raw.itemPath), TravContext{ Zstring() /*errorItemName*/, 0 /*errorRetryCount*/, std::move(cbSub) }});
    }
    else //a file or named pipe, etc.
        cb->onFile({ r.raw.itemName, r.target.fileSize, r.target.modTime, convertToAbstractFileId(r.target.fileId), &linkInfo }); //throw X
}


namespace
{

//====================================================================================================
//====================================================================================================

class RecycleSessionNative : public AbstractFileSystem::RecycleSession
{
public:
    RecycleSessionNative(const Zstring baseFolderPath) : baseFolderPath_(baseFolderPath) {}

    void recycleItemIfExists(const AbstractPath& itemPath, const Zstring& logicalRelPath) override; //throw FileError
    void tryCleanup(const std::function<void (const std::wstring& displayPath)>& notifyDeletionStatus) override; //throw FileError

private:
    const Zstring baseFolderPath_; //ends with path separator
};

//===========================================================================================================================

    typedef struct ::stat FileAttribs; //GCC 5.2 fails when "::" is used in "using FileAttribs = struct ::stat"


inline
FileAttribs getFileAttributes(FileBase::FileHandle fh, const Zstring& filePath) //throw FileError
{
    struct ::stat fileAttr = {};
    if (::fstat(fh, &fileAttr) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), L"fstat");
    return fileAttr;
}


struct InputStreamNative : public AbstractFileSystem::InputStream
{
    InputStreamNative(const Zstring& filePath, const IOCallback& notifyUnbufferedIO /*throw X*/) : fi_(filePath, notifyUnbufferedIO) {} //throw FileError, ErrorFileLocked

    size_t read(void* buffer, size_t bytesToRead) override { return fi_.read(buffer, bytesToRead); } //throw FileError, ErrorFileLocked, X; return "bytesToRead" bytes unless end of stream!
    size_t getBlockSize() const override { return fi_.getBlockSize(); } //non-zero block size is AFS contract!
    std::optional<AFS::StreamAttributes> getAttributesBuffered() override; //throw FileError

private:
    FileInput fi_;
};


std::optional<AFS::StreamAttributes> InputStreamNative::getAttributesBuffered() //throw FileError
{
    const FileAttribs fileAttr = getFileAttributes(fi_.getHandle(), fi_.getFilePath()); //throw FileError

    const time_t modTime = fileAttr.st_mtime;

    const uint64_t fileSize = makeUnsigned(fileAttr.st_size);

    const AFS::FileId fileId = convertToAbstractFileId(generateFileId(fileAttr));

    return AFS::StreamAttributes({ modTime, fileSize, fileId });
}

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
        result.fileId = convertToAbstractFileId(generateFileId(getFileAttributes(fo_.getHandle(), fo_.getFilePath()))); //throw FileError

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
        return itemStillExistsViaFolderTraversal(afsPath); //throw FileError
    }
    //----------------------------------------------------------------------------------------------------------------

    //target existing: fail/ignore => Native will fail and give a clear error message
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
    void moveAndRenameItemForSameAfsType(const AfsPath& afsPathSource, const AbstractPath& apTarget) const override //throw FileError, ErrorDifferentVolume
    {
        //perf test: detecting different volumes by path is ~30 times faster than having ::MoveFileEx() fail with ERROR_NOT_SAME_DEVICE (6µs vs 190µs)
        //=> maybe we can even save some actual I/O in some cases?
        if (compareDeviceSameAfsType(apTarget.afsDevice.ref()) != 0)
            throw ErrorDifferentVolume(replaceCpy(replaceCpy(_("Cannot move file %x to %y."),
                                                             L"%x", L"\n" + fmtPath(getDisplayPath(afsPathSource))),
                                                  L"%y", L"\n" + fmtPath(AFS::getDisplayPath(apTarget))),
                                       formatSystemError(L"compareDeviceRoot", EXDEV)
                                      );
        initComForThread(); //throw FileError
        const Zstring nativePathTarget = static_cast<const NativeFileSystem&>(apTarget.afsDevice.ref()).getNativePath(apTarget.afsPath);
        zen::moveAndRenameItem(getNativePath(afsPathSource), nativePathTarget, false /*replaceExisting*/); //throw FileError, ErrorTargetExisting, ErrorDifferentVolume
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

    void connectNetworkFolder(const AfsPath& afsPath, bool allowUserInteraction) const override //throw FileError
    {
        //TODO: clean-up/remove/re-think connectNetworkFolder()

    }

    int getAccessTimeout() const override { return 0; } //returns "0" if no timeout in force

    bool hasNativeTransactionalCopy() const override { return false; }
    //----------------------------------------------------------------------------------------------------------------

    uint64_t getFreeDiskSpace(const AfsPath& afsPath) const override //throw FileError, returns 0 if not available
    {
        initComForThread(); //throw FileError
        return zen::getFreeDiskSpace(getNativePath(afsPath)); //throw FileError
    }

    bool supportsRecycleBin(const AfsPath& afsPath, const std::function<void ()>& onUpdateGui) const override //throw FileError
    {
        return true; //truth be told: no idea!!!
    }

    std::unique_ptr<RecycleSession> createRecyclerSession(const AfsPath& afsPath) const override //throw FileError, return value must be bound!
    {
        initComForThread(); //throw FileError
        assert(supportsRecycleBin(afsPath, nullptr));
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


void RecycleSessionNative::tryCleanup(const std::function<void (const std::wstring& displayPath)>& notifyDeletionStatus) //throw FileError
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

    //don't accept relative paths!!! indistinguishable from Explorer MTP paths!
    //don't accept paths missing the shared folder! (see drag & drop validation!)
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
    else //path syntax broken
        return AbstractPath(makeSharedRef<NativeFileSystem>(nativePath), AfsPath());
}

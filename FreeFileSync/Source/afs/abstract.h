// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ABSTRACT_H_873450978453042524534234
#define ABSTRACT_H_873450978453042524534234

#include <functional>
#include <chrono>
#include <zen/file_error.h>
#include <zen/file_path.h>
#include <zen/serialize.h> //InputStream/OutputStream support buffered stream concept
#include <wx+/image_holder.h> //NOT a wxWidgets dependency!


namespace fff
{
struct AfsPath;
AfsPath sanitizeDeviceRelativePath(Zstring relPath);

struct AbstractFileSystem;

//==============================================================================================================
using AfsDevice = zen::SharedRef<const AbstractFileSystem>;

struct AfsPath //= path relative to the file system root folder (no leading/traling separator)
{
    AfsPath() {}
    explicit AfsPath(const Zstring& p) : value(p) { assert(zen::isValidRelPath(value)); }
    Zstring value;

    std::strong_ordering operator<=>(const AfsPath&) const = default;
};

struct AbstractPath //THREAD-SAFETY: like an int!
{
    AbstractPath(const AfsDevice& deviceIn, const AfsPath& pathIn) : afsDevice(deviceIn), afsPath(pathIn) {}

    //template <class T1, class T2> -> don't use forwarding constructor: it circumvents AfsPath's explicit constructor!
    //AbstractPath(T1&& deviceIn, T2&& pathIn) : afsDevice(std::forward<T1>(deviceIn)), afsPath(std::forward<T2>(pathIn)) {}

    AfsDevice afsDevice; //"const AbstractFileSystem" => all accesses expected to be thread-safe!!!
    AfsPath afsPath; //relative to device root
};
//==============================================================================================================

struct AbstractFileSystem //THREAD-SAFETY: "const" member functions must model thread-safe access!
{
    //=============== convenience =================
    static Zstring getItemName(const AbstractPath& itemPath) { assert(getParentPath(itemPath)); return getItemName(itemPath.afsPath); }
    static Zstring getItemName(const AfsPath& itemPath) { using namespace zen; return afterLast(itemPath.value, FILE_NAME_SEPARATOR, IfNotFoundReturn::all); }

    static bool isNullPath(const AbstractPath& itemPath) { return isNullDevice(itemPath.afsDevice) /*&& itemPath.afsPath.value.empty()*/; }

    static AbstractPath appendRelPath(const AbstractPath& itemPath, const Zstring& relPath);

    static std::optional<AbstractPath> getParentPath(const AbstractPath& itemPath);
    static std::optional<AfsPath>      getParentPath(const AfsPath& itemPath);
    //=============================================

    static std::weak_ordering compareDevice(const AbstractFileSystem& lhs, const AbstractFileSystem& rhs);

    static bool isNullDevice(const AfsDevice& afsDevice) { return afsDevice.ref().isNullFileSystem(); }

    static std::wstring getDisplayPath(const AbstractPath& itemPath) { return itemPath.afsDevice.ref().getDisplayPath(itemPath.afsPath); }

    static Zstring getInitPathPhrase(const AbstractPath& itemPath) { return itemPath.afsDevice.ref().getInitPathPhrase(itemPath.afsPath); }

    static std::vector<Zstring> getPathPhraseAliases(const AbstractPath& itemPath) { return itemPath.afsDevice.ref().getPathPhraseAliases(itemPath.afsPath); }

    //----------------------------------------------------------------------------------------------------------------
    using RequestPasswordFun = std::function<Zstring(const std::wstring& msg, const std::wstring& lastErrorMsg)>; //throw X
    static void authenticateAccess(const AfsDevice& afsDevice, const RequestPasswordFun& requestPassword /*throw X*/) //throw FileError, X
    { return afsDevice.ref().authenticateAccess(requestPassword); }

    static bool supportPermissionCopy(const AbstractPath& sourcePath, const AbstractPath& targetPath); //throw FileError

    static bool hasNativeTransactionalCopy(const AbstractPath& itemPath) { return itemPath.afsDevice.ref().hasNativeTransactionalCopy(); }
    //----------------------------------------------------------------------------------------------------------------

    using FingerPrint = uint64_t; //AfsDevice-dependent persistent unique ID

    enum class ItemType : unsigned char
    {
        file,
        folder,
        symlink,
    };
    //(hopefully) fast: does not distinguish between error/not existing
    //root path? => do access test
    static ItemType getItemType(const AbstractPath& itemPath) { return itemPath.afsDevice.ref().getItemType(itemPath.afsPath); } //throw FileError

    //assumes: - folder traversal access right (=> yes, because we can assume base path exist at this point; e.g. avoids problem when SFTP parent paths might deny access)
    //         - all child item path parts must correspond to folder traversal
    //           => conclude whether an item is *not* existing anymore by doing a *case-sensitive* name search => potentially SLOW!
    //         - root path? => do access test
    static std::optional<ItemType> getItemTypeIfExists(const AbstractPath& itemPath)
    { return itemPath.afsDevice.ref().getItemTypeIfExists(itemPath.afsPath); } //throw FileError

    static bool itemExists(const AbstractPath& itemPath) { return static_cast<bool>(getItemTypeIfExists(itemPath)); } //throw FileError
    //----------------------------------------------------------------------------------------------------------------

    //already existing: fail
    //does NOT create parent directories recursively if not existing
    static void createFolderPlain(const AbstractPath& folderPath) { folderPath.afsDevice.ref().createFolderPlain(folderPath.afsPath); } //throw FileError

    //creates directories recursively if not existing
    //returns false if folder already exists
    static void createFolderIfMissingRecursion(const AbstractPath& folderPath); //throw FileError

    static void removeFolderIfExistsRecursion(const AbstractPath& folderPath, //throw FileError
                                              const std::function<void(const std::wstring& displayPath)>& onBeforeFileDeletion    /*throw X*/, //
                                              const std::function<void(const std::wstring& displayPath)>& onBeforeSymlinkDeletion /*throw X*/, //optional; one call for each object!
                                              const std::function<void(const std::wstring& displayPath)>& onBeforeFolderDeletion  /*throw X*/) //
    { return folderPath.afsDevice.ref().removeFolderIfExistsRecursion(folderPath.afsPath, onBeforeFileDeletion, onBeforeSymlinkDeletion, onBeforeFolderDeletion); }

    static void removeFileIfExists       (const AbstractPath& filePath);   //
    static void removeSymlinkIfExists    (const AbstractPath& linkPath);   //throw FileError
    static void removeEmptyFolderIfExists(const AbstractPath& folderPath); //

    static void removeFilePlain   (const AbstractPath& filePath  ) { filePath  .afsDevice.ref().removeFilePlain   (filePath  .afsPath); } //
    static void removeSymlinkPlain(const AbstractPath& linkPath  ) { linkPath  .afsDevice.ref().removeSymlinkPlain(linkPath  .afsPath); } //throw FileError
    static void removeFolderPlain (const AbstractPath& folderPath) { folderPath.afsDevice.ref().removeFolderPlain (folderPath.afsPath); } //
    //----------------------------------------------------------------------------------------------------------------
    //static void setModTime(const AbstractPath& itemPath, time_t modTime) { itemPath.afsDevice.ref().setModTime(itemPath.afsPath, modTime); } //throw FileError, follows symlinks

    static AbstractPath getSymlinkResolvedPath(const AbstractPath& linkPath) { return linkPath.afsDevice.ref().getSymlinkResolvedPath(linkPath.afsPath); } //throw FileError
    static bool equalSymlinkContent(const AbstractPath& linkPathL, const AbstractPath& linkPathR); //throw FileError
    //----------------------------------------------------------------------------------------------------------------
    static zen::FileIconHolder getFileIcon      (const AbstractPath& filePath, int pixelSize) { return filePath.afsDevice.ref().getFileIcon      (filePath.afsPath, pixelSize); } //throw FileError; optional return value
    static zen::ImageHolder    getThumbnailImage(const AbstractPath& filePath, int pixelSize) { return filePath.afsDevice.ref().getThumbnailImage(filePath.afsPath, pixelSize); } //throw FileError; optional return value
    //----------------------------------------------------------------------------------------------------------------

    struct StreamAttributes
    {
        time_t modTime; //number of seconds since Jan. 1st 1970 GMT
        uint64_t fileSize;
        FingerPrint filePrint; //optional
    };

    //----------------------------------------------------------------------------------------------------------------
    struct InputStream
    {
        virtual ~InputStream() {}
        virtual size_t getBlockSize() = 0; //throw FileError; non-zero block size is AFS contract!
        virtual size_t tryRead(void* buffer, size_t bytesToRead, const zen::IoCallback& notifyUnbufferedIO /*throw X*/) = 0; //throw FileError, ErrorFileLocked, X
        //may return short; only 0 means EOF! CONTRACT: bytesToRead > 0!

        //only returns attributes if they are already buffered within stream handle and determination would be otherwise expensive (e.g. FTP/SFTP):
        virtual std::optional<StreamAttributes> tryGetAttributesFast() = 0; //throw FileError
    };
    //return value always bound:
    static std::unique_ptr<InputStream> getInputStream(const AbstractPath& filePath) { return filePath.afsDevice.ref().getInputStream(filePath.afsPath); } //throw FileError, ErrorFileLocked

    //----------------------------------------------------------------------------------------------------------------

    struct FinalizeResult
    {
        FingerPrint filePrint = 0; //optional
        std::optional<zen::FileError> errorModTime;
    };

    struct OutputStreamImpl
    {
        virtual ~OutputStreamImpl() {}
        virtual size_t getBlockSize() = 0; //throw FileError; non-zero block size is AFS contract
        virtual size_t tryWrite(const void* buffer, size_t bytesToWrite, const zen::IoCallback& notifyUnbufferedIO /*throw X*/) = 0; //throw FileError, X; may return short! CONTRACT: bytesToWrite > 0
        virtual FinalizeResult finalize(const zen::IoCallback& notifyUnbufferedIO /*throw X*/) = 0; //throw FileError, X
    };

    struct OutputStream //call finalize when done!
    {
        OutputStream(std::unique_ptr<OutputStreamImpl>&& outStream, const AbstractPath& filePath, std::optional<uint64_t> streamSize);
        ~OutputStream();
        size_t getBlockSize() { return outStream_->getBlockSize(); } //throw FileError
        size_t tryWrite(const void* buffer, size_t bytesToWrite, const zen::IoCallback& notifyUnbufferedIO /*throw X*/); //throw FileError, X may return short!
        FinalizeResult finalize(const zen::IoCallback& notifyUnbufferedIO /*throw X*/); //throw FileError, X

    private:
        std::unique_ptr<OutputStreamImpl> outStream_; //bound!
        const AbstractPath filePath_;
        bool finalizeSucceeded_ = false;
        const std::optional<uint64_t> bytesExpected_;
        uint64_t bytesWrittenTotal_ = 0;
    };
    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    static std::unique_ptr<OutputStream> getOutputStream(const AbstractPath& filePath, //throw FileError
                                                         std::optional<uint64_t> streamSize,
                                                         std::optional<time_t> modTime)
    { return std::make_unique<OutputStream>(filePath.afsDevice.ref().getOutputStream(filePath.afsPath, streamSize, modTime), filePath, streamSize); }
    //----------------------------------------------------------------------------------------------------------------

    struct SymlinkInfo
    {
        Zstring itemName;
        time_t modTime;
    };

    struct FileInfo
    {
        Zstring itemName;
        uint64_t fileSize; //unit: bytes!
        time_t modTime; //number of seconds since Jan. 1st 1970 GMT
        FingerPrint filePrint; //optional; persistent + unique (relative to device) or 0!
        bool isFollowedSymlink;
    };

    struct FolderInfo
    {
        Zstring itemName;
        bool isFollowedSymlink;
    };

    struct TraverserCallback
    {
        virtual ~TraverserCallback() {}

        enum class HandleLink
        {
            follow, //follows link, then calls "onFolder()" or "onFile()"
            skip
        };

        enum class HandleError
        {
            retry,
            ignore
        };

        virtual void                               onFile   (const FileInfo&    fi) = 0; //
        virtual HandleLink                         onSymlink(const SymlinkInfo& si) = 0; //throw X
        virtual std::shared_ptr<TraverserCallback> onFolder (const FolderInfo&  fi) = 0; //
        //nullptr: ignore directory, non-nullptr: traverse into, using the (new) callback

        struct ErrorInfo
        {
            std::wstring msg;
            std::chrono::steady_clock::time_point failTime;
            size_t retryNumber = 0;
        };

        virtual HandleError reportDirError (const ErrorInfo& errorInfo)                          = 0; //failed directory traversal -> consider directory data at current level as incomplete!
        virtual HandleError reportItemError(const ErrorInfo& errorInfo, const Zstring& itemName) = 0; //failed to get data for single file/dir/symlink only!
    };

    using TraverserWorkload = std::vector<std::pair<AfsPath, std::shared_ptr<TraverserCallback> /*throw X*/>>;

    //- client needs to handle duplicate file reports! (FilePlusTraverser fallback, retrying to read directory contents, ...)
    static void traverseFolderRecursive(const AfsDevice& afsDevice, const TraverserWorkload& workload /*throw X*/, size_t parallelOps) { afsDevice.ref().traverseFolderRecursive(workload, parallelOps); }

    static void traverseFolder(const AbstractPath& folderPath, //throw FileError
                               const std::function<void(const FileInfo&    fi)>& onFile,    //
                               const std::function<void(const FolderInfo&  fi)>& onFolder,  //optional
                               const std::function<void(const SymlinkInfo& si)>& onSymlink) //
    { folderPath.afsDevice.ref().traverseFolder(folderPath.afsPath, onFile, onFolder, onSymlink); }
    //----------------------------------------------------------------------------------------------------------------

    //already existing: undefined behavior! (e.g. fail/overwrite)
    static void moveAndRenameItem(const AbstractPath& pathFrom, const AbstractPath& pathTo); //throw FileError, ErrorMoveUnsupported

    static std::wstring generateMoveErrorMsg(const AbstractPath& pathFrom, const AbstractPath& pathTo) { return pathFrom.afsDevice.ref().generateMoveErrorMsg(pathFrom.afsPath, pathTo); }


    //Note: it MAY happen that copyFileTransactional() leaves temp files behind, e.g. temporary network drop.
    // => clean them up at an appropriate time (automatically set sync directions to delete them). They have the following ending:
    static inline constexpr ZstringView TEMP_FILE_ENDING = Zstr(".ffs_tmp"); //don't use Zstring as global constant: avoid static initialization order problem in global namespace!
    // caveat: ending is hard-coded by RealTimeSync

    struct FileCopyResult
    {
        uint64_t fileSize = 0;
        time_t modTime = 0; //number of seconds since Jan. 1st 1970 GMT
        FingerPrint sourceFilePrint = 0; //optional
        FingerPrint targetFilePrint = 0; //
        std::optional<zen::FileError> errorModTime; //failure to set modification time
    };

    //symlink handling: follow
    //already existing + no onDeleteTargetFile: undefined behavior! (e.g. fail/overwrite/auto-rename)
    //returns current attributes at the time of copy
    static FileCopyResult copyFileTransactional(const AbstractPath& sourcePath, const StreamAttributes& attrSource, //throw FileError, ErrorFileLocked, X
                                                const AbstractPath& targetPath,
                                                bool copyFilePermissions,
                                                bool transactionalCopy,
                                                //if target is existing user *must* implement deletion to avoid undefined behavior
                                                //if transactionalCopy == true, full read access on source had been proven at this point, so it's safe to delete it.
                                                const std::function<void()>& onDeleteTargetFile /*throw X*/,
                                                //accummulated delta != file size! consider ADS, sparse, compressed files
                                                const zen::IoCallback& notifyUnbufferedIO /*throw X*/);
    //already existing: fail
    //symlink handling: follow
    static void copyNewFolder(const AbstractPath& sourcePath, const AbstractPath& targetPath, bool copyFilePermissions); //throw FileError

    //already existing: fail
    static void copySymlink(const AbstractPath& sourcePath, const AbstractPath& targetPath, bool copyFilePermissions); //throw FileError

    //----------------------------------------------------------------------------------------------------------------

    //- returns < 0 if not available
    //- folderPath does not need to exist (yet)
    static int64_t getFreeDiskSpace(const AbstractPath& folderPath) { return folderPath.afsDevice.ref().getFreeDiskSpace(folderPath.afsPath); } //throw FileError

    struct RecycleSession
    {
        virtual ~RecycleSession() {}

        //- multi-threaded access: internally synchronized!
        void moveToRecycleBinIfExists(const AbstractPath& itemPath, const Zstring& logicalRelPath); //throw FileError, RecycleBinUnavailable

        //- fails if item is not existing: don't leave user wonder why it isn't in the recycle bin!
        //- multi-threaded access: internally synchronized!
        virtual void moveToRecycleBin(const AbstractPath& itemPath, const Zstring& logicalRelPath) = 0; //throw FileError, RecycleBinUnavailable

        virtual void tryCleanup(const std::function<void(const std::wstring& displayPath)>& notifyDeletionStatus /*throw X*; displayPath may be empty*/) = 0; //throw FileError, X
    };

    //- return value always bound!
    //- constructor will be running on main thread => *no* file I/O!
    static std::unique_ptr<RecycleSession> createRecyclerSession(const AbstractPath& folderPath) { return folderPath.afsDevice.ref().createRecyclerSession(folderPath.afsPath); } //throw FileError, RecycleBinUnavailable

    //- returns empty on success, item type if recycle bin is not available
    static void moveToRecycleBinIfExists(const AbstractPath& itemPath); //throw FileError, RecycleBinUnavailable

    //fails if item is not existing
    static void moveToRecycleBin(const AbstractPath& itemPath) { itemPath.afsDevice.ref().moveToRecycleBin(itemPath.afsPath); }; //throw FileError, RecycleBinUnavailable

    //================================================================================================================

    //no need to protect access:
    virtual ~AbstractFileSystem() {}


protected:
    //default implementation: folder traversal
    virtual void removeFolderIfExistsRecursion(const AfsPath& folderPath, //throw FileError
                                               const std::function<void(const std::wstring& displayPath)>& onBeforeFileDeletion,
                                               const std::function<void(const std::wstring& displayPath)>& onBeforeSymlinkDeletion,
                                               const std::function<void(const std::wstring& displayPath)>& onBeforeFolderDeletion) const = 0;

    void traverseFolder(const AfsPath& folderPath, //throw FileError
                        const std::function<void(const FileInfo&    fi)>& onFile,           //
                        const std::function<void(const FolderInfo&  fi)>& onFolder,         //optional
                        const std::function<void(const SymlinkInfo& si)>& onSymlink) const; //

    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    FileCopyResult copyFileAsStream(const AfsPath& sourcePath, const StreamAttributes& attrSource, //throw FileError, ErrorFileLocked, X
                                    const AbstractPath& targetPath, const zen::IoCallback& notifyUnbufferedIO /*throw X*/) const;


    std::wstring generateMoveErrorMsg(const AfsPath& pathFrom, const AbstractPath& pathTo) const
    {
        using namespace zen;

        if (getParentPath(pathFrom) == getParentPath(pathTo.afsPath)) //pure "rename"
            return replaceCpy(replaceCpy(_("Cannot rename %x to %y."),
                                         L"%x", fmtPath(getDisplayPath(pathFrom))),
                              L"%y", fmtPath(getItemName(pathTo)));
        else //"move" or "move + rename"
            return trimCpy(replaceCpy(replaceCpy(_("Cannot move %x to %y."),
                                                 L"%x", L'\n' + fmtPath(getDisplayPath(pathFrom))),
                                      L"%y", L'\n' + fmtPath(getDisplayPath(pathTo))));
    }

private:
    virtual std::optional<Zstring> getNativeItemPath(const AfsPath& itemPath) const { return {}; };

    virtual Zstring getInitPathPhrase(const AfsPath& itemPath) const = 0;

    virtual std::vector<Zstring> getPathPhraseAliases(const AfsPath& itemPath) const = 0;

    virtual std::wstring getDisplayPath(const AfsPath& itemPath) const = 0;

    virtual bool isNullFileSystem() const = 0;

    virtual std::weak_ordering compareDeviceSameAfsType(const AbstractFileSystem& afsRhs) const = 0;

    //----------------------------------------------------------------------------------------------------------------
    virtual ItemType getItemType(const AfsPath& itemPath) const = 0; //throw FileError

    virtual std::optional<ItemType> getItemTypeIfExists(const AfsPath& itemPath) const = 0; //throw FileError

    //already existing: fail
    virtual void createFolderPlain(const AfsPath& folderPath) const = 0; //throw FileError

    //non-recursive folder deletion:
    virtual void removeFilePlain   (const AfsPath& filePath  ) const = 0; //throw FileError
    virtual void removeSymlinkPlain(const AfsPath& linkPath  ) const = 0; //throw FileError
    virtual void removeFolderPlain (const AfsPath& folderPath) const = 0; //throw FileError

    //----------------------------------------------------------------------------------------------------------------
    //virtual void setModTime(const AfsPath& itemPath, time_t modTime) const = 0; //throw FileError, follows symlinks

    virtual AbstractPath getSymlinkResolvedPath(const AfsPath& linkPath) const = 0; //throw FileError
    virtual bool equalSymlinkContentForSameAfsType(const AfsPath& linkPathL, const AbstractPath& linkPathR) const = 0; //throw FileError

    //----------------------------------------------------------------------------------------------------------------
    virtual std::unique_ptr<InputStream> getInputStream(const AfsPath& filePath) const = 0; //throw FileError, ErrorFileLocked

    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    virtual std::unique_ptr<OutputStreamImpl> getOutputStream(const AfsPath& filePath, //throw FileError
                                                              std::optional<uint64_t> streamSize,
                                                              std::optional<time_t> modTime) const = 0;
    //----------------------------------------------------------------------------------------------------------------
    virtual void traverseFolderRecursive(const TraverserWorkload& workload /*throw X*/, size_t parallelOps) const = 0;
    //----------------------------------------------------------------------------------------------------------------
    virtual bool supportsPermissions(const AfsPath& folderPath) const = 0; //throw FileError

    //already existing: undefined behavior! (e.g. fail/overwrite)
    virtual void moveAndRenameItemForSameAfsType(const AfsPath& pathFrom, const AbstractPath& pathTo) const = 0; //throw FileError, ErrorMoveUnsupported

    //symlink handling: follow
    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    virtual FileCopyResult copyFileForSameAfsType(const AfsPath& sourcePath, const StreamAttributes& attrSource, //throw FileError, ErrorFileLocked, X
                                                  const AbstractPath& targetPath, bool copyFilePermissions,
                                                  //accummulated delta != file size! consider ADS, sparse, compressed files
                                                  const zen::IoCallback& notifyUnbufferedIO /*throw X*/) const = 0;


    //symlink handling: follow
    //already existing: fail
    virtual void copyNewFolderForSameAfsType(const AfsPath& sourcePath, const AbstractPath& targetPath, bool copyFilePermissions) const = 0; //throw FileError

    //already existing: fail
    virtual void copySymlinkForSameAfsType(const AfsPath& sourcePath, const AbstractPath& targetPath, bool copyFilePermissions) const = 0; //throw FileError

    //----------------------------------------------------------------------------------------------------------------
    virtual zen::FileIconHolder getFileIcon      (const AfsPath& filePath, int pixelSize) const = 0; //throw FileError; optional return value
    virtual zen::ImageHolder    getThumbnailImage(const AfsPath& filePath, int pixelSize) const = 0; //throw FileError; optional return value

    virtual void authenticateAccess(const RequestPasswordFun& requestPassword /*throw X*/) const = 0; //throw FileError, X

    virtual bool hasNativeTransactionalCopy() const = 0;
    //----------------------------------------------------------------------------------------------------------------

    virtual int64_t getFreeDiskSpace(const AfsPath& folderPath) const = 0; //throw FileError, returns < 0 if not available
    virtual std::unique_ptr<RecycleSession> createRecyclerSession(const AfsPath& folderPath) const = 0; //throw FileError, RecycleBinUnavailable
    virtual void moveToRecycleBin(const AfsPath& itemPath) const = 0; //throw FileError, RecycleBinUnavailable
};


inline std::weak_ordering operator<=>(const AfsDevice& lhs, const AfsDevice& rhs) { return AbstractFileSystem::compareDevice(lhs.ref(), rhs.ref()); }
inline bool               operator== (const AfsDevice& lhs, const AfsDevice& rhs) { return (lhs <=> rhs) == std::weak_ordering::equivalent; }

inline
std::weak_ordering operator<=>(const AbstractPath& lhs, const AbstractPath& rhs)
{
    return std::tie(lhs.afsDevice, lhs.afsPath) <=>
           std::tie(rhs.afsDevice, rhs.afsPath);
}

inline
bool operator==(const AbstractPath& lhs, const AbstractPath& rhs) { return lhs.afsPath == rhs.afsPath && lhs.afsDevice == rhs.afsDevice; }








//------------------------------------ implementation -----------------------------------------
inline
AbstractPath AbstractFileSystem::appendRelPath(const AbstractPath& itemPath, const Zstring& relPath)
{
    return AbstractPath(itemPath.afsDevice, AfsPath(appendPath(itemPath.afsPath.value, relPath)));
}

//---------------------------------------------------------------------------------------------

inline
AbstractFileSystem::OutputStream::OutputStream(std::unique_ptr<OutputStreamImpl>&& outStream, const AbstractPath& filePath, std::optional<uint64_t> streamSize) :
    outStream_(std::move(outStream)),
    filePath_(filePath),
    bytesExpected_(streamSize) {}


inline
AbstractFileSystem::OutputStream::~OutputStream()
{
    //we delete the file on errors: => file should not have existed prior to creating OutputStream instance!!
    outStream_.reset(); //close file handle *before* remove!

    if (!finalizeSucceeded_) //transactional output stream! => clean up!
        //- needed for Google Drive: e.g. user might cancel during OutputStreamImpl::finalize(), just after file was written transactionally
        //- also for Native: setFileTime() may fail *after* FileOutput::finalize()
        try { AbstractFileSystem::removeFilePlain(filePath_); /*throw FileError*/ }
        catch (const zen::FileError& e) { zen::logExtraError(e.toString()); }

        warn_static("we should not log if not existing anymore!?:       ERROR_FILE_NOT_FOUND: ddddddddddd [DeleteFile]")
    //solution: integrate cleanup into ~OutputStreamImpl() including appropriate loggin!
}


inline
size_t AbstractFileSystem::OutputStream::tryWrite(const void* buffer, size_t bytesToWrite, const zen::IoCallback& notifyUnbufferedIO /*throw X*/) //throw FileError, X
{
    const size_t bytesWritten = outStream_->tryWrite(buffer, bytesToWrite, notifyUnbufferedIO /*throw X*/); //throw FileError, X may return short!
    bytesWrittenTotal_ += bytesWritten;
    return bytesWritten;
}


inline
AbstractFileSystem::FinalizeResult AbstractFileSystem::OutputStream::finalize(const zen::IoCallback& notifyUnbufferedIO /*throw X*/) //throw FileError, X
{
    using namespace zen;

    //important check: catches corrupt SFTP download with libssh2!
    if (bytesExpected_ && *bytesExpected_ != bytesWrittenTotal_)
        throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getDisplayPath(filePath_))), //instead we should report the source file, but don't have it here...
                        _("Unexpected size of data stream:") + L' ' + formatNumber(bytesWrittenTotal_) + L'\n' +
                        _("Expected:") + L' ' + formatNumber(*bytesExpected_));

    const FinalizeResult result = outStream_->finalize(notifyUnbufferedIO); //throw FileError, X
    finalizeSucceeded_ = true;
    return result;
}

//--------------------------------------------------------------------------

inline
bool AbstractFileSystem::supportPermissionCopy(const AbstractPath& sourcePath, const AbstractPath& targetPath) //throw FileError
{
    if (typeid(sourcePath.afsDevice.ref()) != typeid(targetPath.afsDevice.ref()))
        return false;

    return sourcePath.afsDevice.ref().supportsPermissions(sourcePath.afsPath) && //throw FileError
           targetPath.afsDevice.ref().supportsPermissions(targetPath.afsPath);
}


inline
bool AbstractFileSystem::equalSymlinkContent(const AbstractPath& linkPathL, const AbstractPath& linkPathR) //throw FileError
{
    if (typeid(linkPathL.afsDevice.ref()) != typeid(linkPathR.afsDevice.ref()))
        return false;

    return linkPathL.afsDevice.ref().equalSymlinkContentForSameAfsType(linkPathL.afsPath, linkPathR); //throw FileError
}


inline
void AbstractFileSystem::moveAndRenameItem(const AbstractPath& pathFrom, const AbstractPath& pathTo) //throw FileError, ErrorMoveUnsupported
{
    using namespace zen;

    if (typeid(pathFrom.afsDevice.ref()) != typeid(pathTo.afsDevice.ref()))
        throw ErrorMoveUnsupported(generateMoveErrorMsg(pathFrom, pathTo), _("Operation not supported between different devices."));

    //already existing: undefined behavior! (e.g. fail/overwrite)
    pathFrom.afsDevice.ref().moveAndRenameItemForSameAfsType(pathFrom.afsPath, pathTo); //throw FileError, ErrorMoveUnsupported
}


inline
void AbstractFileSystem::copyNewFolder(const AbstractPath& sourcePath, const AbstractPath& targetPath, bool copyFilePermissions) //throw FileError
{
    using namespace zen;

    if (typeid(sourcePath.afsDevice.ref()) != typeid(targetPath.afsDevice.ref())) //fall back:
    {
        //already existing: fail
        createFolderPlain(targetPath); //throw FileError

        if (copyFilePermissions)
            throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(getDisplayPath(targetPath))),
                            _("Operation not supported between different devices."));
    }
    else
        sourcePath.afsDevice.ref().copyNewFolderForSameAfsType(sourcePath.afsPath, targetPath, copyFilePermissions); //throw FileError
}


//already existing: fail
inline
void AbstractFileSystem::copySymlink(const AbstractPath& sourcePath, const AbstractPath& targetPath, bool copyFilePermissions) //throw FileError
{
    using namespace zen;

    if (typeid(sourcePath.afsDevice.ref()) != typeid(targetPath.afsDevice.ref()))
        throw FileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."),
                                              L"%x", L'\n' + fmtPath(getDisplayPath(sourcePath))),
                                   L"%y", L'\n' + fmtPath(getDisplayPath(targetPath))), _("Operation not supported between different devices."));

    //already existing: fail
    sourcePath.afsDevice.ref().copySymlinkForSameAfsType(sourcePath.afsPath, targetPath, copyFilePermissions); //throw FileError
}
}

#endif //ABSTRACT_H_873450978453042524534234

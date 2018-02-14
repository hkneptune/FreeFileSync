// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ABSTRACT_H_873450978453042524534234
#define ABSTRACT_H_873450978453042524534234

#include <functional>
#include <zen/file_error.h>
#include <zen/zstring.h>
#include <zen/optional.h>
#include <zen/serialize.h> //InputStream/OutputStream support buffered stream concept
#include "../lib/icon_holder.h"


namespace fff
{
struct AbstractFileSystem;

bool isValidRelPath(const Zstring& relPath);

//==============================================================================================================
struct AfsPath //= path relative (no leading/traling separator) to the file system root folder
{
    explicit AfsPath(const Zstring& p) : value(p) { assert(isValidRelPath(value)); }
    Zstring value;
};

class AbstractPath //THREAD-SAFETY: like an int!
{
public:
    AbstractPath(const std::shared_ptr<const AbstractFileSystem>& afsIn, const AfsPath& afsPathIn) : afs(afsIn), afsPath(afsPathIn) {}

    //template <class T1, class T2> -> don't use forwarding constructor! => it circumvents AfsPath's explicit constructor
    //AbstractPath(T1&& afsIn, T2&& afsPathIn) : afs(std::forward<T1>(afsIn)), afsPath(std::forward<T2>(afsPathIn)) { assert(isValidRelPath(afsPathIn)); }

private:
    friend struct AbstractFileSystem;

    std::shared_ptr<const AbstractFileSystem> afs; //always bound; "const AbstractFileSystem" => all accesses expected to be thread-safe!!!
    AfsPath afsPath;
};
//==============================================================================================================

struct AbstractFileSystem //THREAD-SAFETY: "const" member functions must model thread-safe access!
{
    static int compareAbstractPath(const AbstractPath& lhs, const AbstractPath& rhs);

    struct LessAbstractPath
    {
        bool operator()(const AbstractPath& lhs, const AbstractPath& rhs) const { return compareAbstractPath(lhs, rhs) < 0; }
    };

    static bool equalAbstractPath(const AbstractPath& lhs, const AbstractPath& rhs) { return compareAbstractPath(lhs, rhs) == 0; }

    static Zstring getInitPathPhrase(const AbstractPath& ap) { return ap.afs->getInitPathPhrase(ap.afsPath); }

    static std::wstring getDisplayPath(const AbstractPath& ap) { return ap.afs->getDisplayPath(ap.afsPath); }

    static bool isNullPath(const AbstractPath& ap) { return ap.afs->isNullFileSystem() /*&& ap.afsPath.value.empty()*/; }

    static AbstractPath appendRelPath(const AbstractPath& ap, const Zstring& relPath);

    static Zstring getItemName(const AbstractPath& ap) { assert(getParentFolderPath(ap)); return getItemName(ap.afsPath); }

    static zen::Opt<Zstring> getNativeItemPath(const AbstractPath& ap) { return ap.afs->getNativeItemPath(ap.afsPath); }

    static zen::Opt<AbstractPath> getParentFolderPath(const AbstractPath& ap);

    struct PathComponents
    {
        AbstractPath rootPath;       //itemPath =: rootPath + relPath
        std::vector<Zstring> relPath;
    };
    static PathComponents getPathComponents(const AbstractPath& ap);
    //----------------------------------------------------------------------------------------------------------------
    enum class ItemType
    {
        FILE,
        FOLDER,
        SYMLINK,
    };
    struct PathStatus
    {
        ItemType existingType;
        AbstractPath existingPath;    //itemPath =: existingPath + relPath
        std::vector<Zstring> relPath; //
    };
    //(hopefully) fast: does not distinguish between error/not existing
    static ItemType getItemType(const AbstractPath& ap) { return ap.afs->getItemType(ap.afsPath); } //throw FileError
    //execute potentially SLOW folder traversal but distinguish error/not existing
    static zen::Opt<ItemType> getItemTypeIfExists(const AbstractPath& ap); //throw FileError
    static PathStatus getPathStatus(const AbstractPath& ap); //throw FileError
    //----------------------------------------------------------------------------------------------------------------

    //target existing: undefined behavior! (fail/overwrite)
    //does NOT create parent directories recursively if not existing
    static void createFolderPlain(const AbstractPath& ap) { ap.afs->createFolderPlain(ap.afsPath); } //throw FileError

    //no error if already existing
    //creates parent directories recursively if not existing
    static void createFolderIfMissingRecursion(const AbstractPath& ap); //throw FileError

    static bool removeFileIfExists   (const AbstractPath& ap); //throw FileError; return "false" if file is not existing
    static bool removeSymlinkIfExists(const AbstractPath& ap); //
    static void removeFolderIfExistsRecursion(const AbstractPath& ap, //throw FileError
                                              const std::function<void (const std::wstring& displayPath)>& onBeforeFileDeletion,    //optional
                                              const std::function<void (const std::wstring& displayPath)>& onBeforeFolderDeletion); //one call for each *existing* object!

    static void removeFilePlain   (const AbstractPath& ap) { ap.afs->removeFilePlain   (ap.afsPath); } //throw FileError
    static void removeSymlinkPlain(const AbstractPath& ap) { ap.afs->removeSymlinkPlain(ap.afsPath); } //throw FileError
    static void removeFolderPlain (const AbstractPath& ap) { ap.afs->removeFolderPlain (ap.afsPath); } //throw FileError
    //----------------------------------------------------------------------------------------------------------------
    static void setModTime(const AbstractPath& ap, time_t modTime) { ap.afs->setModTime(ap.afsPath, modTime); } //throw FileError, follows symlinks

    static AbstractPath getSymlinkResolvedPath(const AbstractPath& ap) { return ap.afs->getSymlinkResolvedPath(ap.afsPath); } //throw FileError
    static std::string getSymlinkBinaryContent(const AbstractPath& ap) { return ap.afs->getSymlinkBinaryContent(ap.afsPath); } //throw FileError
    //----------------------------------------------------------------------------------------------------------------
    //noexcept; optional return value:
    static ImageHolder getFileIcon      (const AbstractPath& ap, int pixelSize) { return ap.afs->getFileIcon      (ap.afsPath, pixelSize); }
    static ImageHolder getThumbnailImage(const AbstractPath& ap, int pixelSize) { return ap.afs->getThumbnailImage(ap.afsPath, pixelSize); }

    static void connectNetworkFolder(const AbstractPath& ap, bool allowUserInteraction) { return ap.afs->connectNetworkFolder(ap.afsPath, allowUserInteraction); } //throw FileError
    //----------------------------------------------------------------------------------------------------------------

    using FileId = zen::Zbase<char>;

    struct StreamAttributes
    {
        time_t modTime; //number of seconds since Jan. 1st 1970 UTC
        uint64_t fileSize;
        FileId fileId; //optional!
    };

    //----------------------------------------------------------------------------------------------------------------
    struct InputStream
    {
        virtual ~InputStream() {}
        virtual size_t read(void* buffer, size_t bytesToRead) = 0; //throw FileError, ErrorFileLocked, X; return "bytesToRead" bytes unless end of stream!
        virtual size_t getBlockSize() const = 0; //non-zero block size is AFS contract! it's implementer's job to always give a reasonable buffer size!

        //only returns attributes if they are already buffered within stream handle and determination would be otherwise expensive (e.g. FTP/SFTP):
        virtual zen::Opt<StreamAttributes> getAttributesBuffered() = 0; //throw FileError
    };

    struct OutputStreamImpl
    {
        virtual ~OutputStreamImpl() {}
        virtual void write(const void* buffer, size_t bytesToWrite) = 0; //throw FileError, X
        virtual FileId finalize() = 0;                                   //throw FileError, X
    };

    //TRANSACTIONAL output stream! => call finalize when done!
    struct OutputStream
    {
        OutputStream(std::unique_ptr<OutputStreamImpl>&& outStream, const AbstractPath& filePath, const uint64_t* streamSize);
        ~OutputStream();
        void write(const void* buffer, size_t bytesToWrite); //throw FileError, X
        FileId finalize();                                   //throw FileError, X

    private:
        std::unique_ptr<OutputStreamImpl> outStream_; //bound!
        const AbstractPath filePath_;
        bool finalizeSucceeded_ = false;
        zen::Opt<uint64_t> bytesExpected_;
        uint64_t bytesWrittenTotal_ = 0;
    };

    //return value always bound:
    static std::unique_ptr<InputStream> getInputStream(const AbstractPath& ap, const zen::IOCallback& notifyUnbufferedIO) //throw FileError, ErrorFileLocked, X
    { return ap.afs->getInputStream(ap.afsPath, notifyUnbufferedIO); }

    //target existing: undefined behavior! (fail/overwrite/auto-rename)
    static std::unique_ptr<OutputStream> getOutputStream(const AbstractPath& ap, //throw FileError
                                                         const uint64_t* streamSize,           //optional
                                                         const zen::IOCallback& notifyUnbufferedIO) //
    { return std::make_unique<OutputStream>(ap.afs->getOutputStream(ap.afsPath, streamSize, notifyUnbufferedIO), ap, streamSize); }
    //----------------------------------------------------------------------------------------------------------------

    struct TraverserCallback
    {
        virtual ~TraverserCallback() {}

        struct SymlinkInfo
        {
            Zstring itemName;
            time_t modTime; //number of seconds since Jan. 1st 1970 UTC
        };

        struct FileInfo
        {
            Zstring itemName;
            uint64_t fileSize; //unit: bytes!
            time_t modTime; //number of seconds since Jan. 1st 1970 UTC
            FileId fileId; //optional: empty if not supported!
            const SymlinkInfo* symlinkInfo; //only filled if file is a followed symlink
        };

        struct FolderInfo
        {
            Zstring itemName;
            const SymlinkInfo* symlinkInfo; //only filled if folder is a followed symlink
        };

        enum HandleLink
        {
            LINK_FOLLOW, //dereferences link, then calls "onFolder()" or "onFile()"
            LINK_SKIP
        };

        enum HandleError
        {
            ON_ERROR_RETRY,
            ON_ERROR_CONTINUE
        };

        virtual void                               onFile   (const FileInfo&    fi) = 0; //
        virtual HandleLink                         onSymlink(const SymlinkInfo& si) = 0; //throw X
        virtual std::unique_ptr<TraverserCallback> onFolder (const FolderInfo&  fi) = 0; //
        //nullptr: ignore directory, non-nullptr: traverse into, using the (new) callback

        virtual HandleError reportDirError (const std::wstring& msg, size_t retryNumber) = 0; //failed directory traversal -> consider directory data at current level as incomplete!
        virtual HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName) = 0; //failed to get data for single file/dir/symlink only!
    };

    //- client needs to handle duplicate file reports! (FilePlusTraverser fallback, retrying to read directory contents, ...)
    static void traverseFolder(const AbstractPath& ap, TraverserCallback& sink /*throw X*/) { ap.afs->traverseFolder(ap.afsPath, sink); } //throw X
    //----------------------------------------------------------------------------------------------------------------

    static bool supportPermissionCopy(const AbstractPath& apSource, const AbstractPath& apTarget); //throw FileError

    //target existing: undefined behavior! (fail/overwrite/auto-rename)
    static void renameItem(const AbstractPath& apSource, const AbstractPath& apTarget); //throw FileError, ErrorDifferentVolume

    //Note: it MAY happen that copyFileTransactional() leaves temp files behind, e.g. temporary network drop.
    // => clean them up at an appropriate time (automatically set sync directions to delete them). They have the following ending:
    static const Zchar* TEMP_FILE_ENDING; //don't use Zstring as global constant: avoid static initialization order problem in global namespace!

    struct FileCopyResult
    {
        uint64_t fileSize = 0;
        time_t modTime = 0; //number of seconds since Jan. 1st 1970 UTC
        FileId sourceFileId;
        FileId targetFileId;
        zen::Opt<zen::FileError> errorModTime; //failure to set modification time
    };

    //symlink handling: follow
    //target existing: undefined behavior! (fail/overwrite/auto-rename)
    //returns current attributes at the time of copy
    static FileCopyResult copyFileTransactional(const AbstractPath& apSource, const StreamAttributes& attrSource, //throw FileError, ErrorFileLocked
                                                const AbstractPath& apTarget,
                                                bool copyFilePermissions,
                                                bool transactionalCopy,
                                                //if target is existing user *must* implement deletion to avoid undefined behavior
                                                //if transactionalCopy == true, full read access on source had been proven at this point, so it's safe to delete it.
                                                const std::function<void()>& onDeleteTargetFile,
                                                //accummulated delta != file size! consider ADS, sparse, compressed files
                                                const zen::IOCallback& notifyUnbufferedIO);

    //target existing: undefined behavior! (fail/overwrite)
    //symlink handling: follow link!
    static void copyNewFolder(const AbstractPath& apSource, const AbstractPath& apTarget, bool copyFilePermissions); //throw FileError

    static void copySymlink  (const AbstractPath& apSource, const AbstractPath& apTarget, bool copyFilePermissions); //throw FileError

    //----------------------------------------------------------------------------------------------------------------

    static uint64_t getFreeDiskSpace(const AbstractPath& ap) { return ap.afs->getFreeDiskSpace(ap.afsPath); } //throw FileError, returns 0 if not available

    static bool supportsRecycleBin(const AbstractPath& ap, const std::function<void ()>& onUpdateGui) { return ap.afs->supportsRecycleBin(ap.afsPath, onUpdateGui); } //throw FileError

    struct RecycleSession
    {
        virtual ~RecycleSession() {}
        virtual bool recycleItem(const AbstractPath& itemPath, const Zstring& logicalRelPath) = 0; //throw FileError; return true if item existed
        virtual void tryCleanup(const std::function<void (const std::wstring& displayPath)>& notifyDeletionStatus /*optional; currentItem may be empty*/) = 0; //throw FileError
    };

    //precondition: supportsRecycleBin() must return true!
    static std::unique_ptr<RecycleSession> createRecyclerSession(const AbstractPath& ap) { return ap.afs->createRecyclerSession(ap.afsPath); } //throw FileError, return value must be bound!

    static void recycleItemIfExists(const AbstractPath& ap) { ap.afs->recycleItemIfExists(ap.afsPath); } //throw FileError

    //================================================================================================================

    //no need to protect access:
    static Zstring appendPaths(const Zstring& basePath, const Zstring& relPath, Zchar pathSep);

    virtual ~AbstractFileSystem() {}

protected: //grant derived classes access to AbstractPath:
    static const AbstractFileSystem& getAfs    (const AbstractPath& ap) { return *ap.afs; }
    static AfsPath                   getAfsPath(const AbstractPath& ap) { return ap.afsPath; }

    static Zstring getItemName(const AfsPath& afsPath) { using namespace zen; return afterLast(afsPath.value, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL); }
    static zen::Opt<AfsPath> getParentAfsPath(const AfsPath& afsPath);

    struct PathStatusImpl
    {
        ItemType existingType;
        AfsPath existingAfsPath;      //afsPath =: existingAfsPath + relPath
        std::vector<Zstring> relPath; //
    };
    PathStatusImpl getPathStatusViaFolderTraversal(const AfsPath& afsPath) const; //throw FileError

    //target existing: undefined behavior! (fail/overwrite/auto-rename)
    FileCopyResult copyFileAsStream(const AfsPath& afsPathSource, const StreamAttributes& attrSource, //throw FileError, ErrorFileLocked
                                    const AbstractPath& apTarget, const zen::IOCallback& notifyUnbufferedIO) const; //may be nullptr; throw X!

private:
    virtual zen::Opt<Zstring> getNativeItemPath(const AfsPath& afsPath) const { return zen::NoValue(); };

    virtual Zstring getInitPathPhrase(const AfsPath& afsPath) const = 0;

    virtual std::wstring getDisplayPath(const AfsPath& afsPath) const = 0;

    virtual bool isNullFileSystem() const = 0;

    virtual int compareDeviceRootSameAfsType(const AbstractFileSystem& afsRhs) const = 0;

    //----------------------------------------------------------------------------------------------------------------
    virtual ItemType getItemType(const AfsPath& afsPath) const = 0; //throw FileError
    virtual PathStatusImpl getPathStatus(const AfsPath& afsPath) const = 0; //throw FileError
    //----------------------------------------------------------------------------------------------------------------

    //target existing: undefined behavior! (fail/overwrite)
    virtual void createFolderPlain(const AfsPath& afsPath) const = 0; //throw FileError

    //non-recursive folder deletion:
    virtual void removeFilePlain   (const AfsPath& afsPath) const = 0; //throw FileError
    virtual void removeSymlinkPlain(const AfsPath& afsPath) const = 0; //throw FileError
    virtual void removeFolderPlain (const AfsPath& afsPath) const = 0; //throw FileError
    //----------------------------------------------------------------------------------------------------------------
    virtual void setModTime(const AfsPath& afsPath, time_t modTime) const = 0; //throw FileError, follows symlinks

    virtual AbstractPath getSymlinkResolvedPath(const AfsPath& afsPath) const = 0; //throw FileError
    virtual std::string getSymlinkBinaryContent(const AfsPath& afsPath) const = 0; //throw FileError
    //----------------------------------------------------------------------------------------------------------------
    virtual std::unique_ptr<InputStream> getInputStream (const AfsPath& afsPath, const zen::IOCallback& notifyUnbufferedIO) const = 0; //throw FileError, ErrorFileLocked, X

    //target existing: undefined behavior! (fail/overwrite/auto-rename)
    virtual std::unique_ptr<OutputStreamImpl> getOutputStream(const AfsPath& afsPath, //throw FileError
                                                              const uint64_t* streamSize,                      //optional
                                                              const zen::IOCallback& notifyUnbufferedIO) const = 0; //
    //----------------------------------------------------------------------------------------------------------------
    virtual void traverseFolder(const AfsPath& afsPath, TraverserCallback& sink /*throw X*/) const = 0; //throw X
    //----------------------------------------------------------------------------------------------------------------
    virtual bool supportsPermissions(const AfsPath& afsPath) const = 0; //throw FileError

    //target existing: undefined behavior! (fail/overwrite/auto-rename)
    virtual void renameItemForSameAfsType(const AfsPath& afsPathSource, const AbstractPath& apTarget) const = 0; //throw FileError, ErrorDifferentVolume

    //symlink handling: follow link!
    //target existing: undefined behavior! (fail/overwrite/auto-rename)
    virtual FileCopyResult copyFileForSameAfsType(const AfsPath& afsPathSource, const StreamAttributes& attrSource, //throw FileError, ErrorFileLocked
                                                  const AbstractPath& apTarget, bool copyFilePermissions,
                                                  //accummulated delta != file size! consider ADS, sparse, compressed files
                                                  const zen::IOCallback& notifyUnbufferedIO) const = 0; //may be nullptr; throw X!


    //target existing: undefined behavior! (fail/overwrite)
    //symlink handling: follow link!
    virtual void copyNewFolderForSameAfsType(const AfsPath& afsPathSource, const AbstractPath& apTarget, bool copyFilePermissions) const = 0; //throw FileError

    virtual void copySymlinkForSameAfsType(const AfsPath& afsPathSource, const AbstractPath& apTarget, bool copyFilePermissions) const = 0; //throw FileError

    //----------------------------------------------------------------------------------------------------------------
    virtual ImageHolder getFileIcon      (const AfsPath& afsPath, int pixelSize) const = 0; //noexcept; optional return value
    virtual ImageHolder getThumbnailImage(const AfsPath& afsPath, int pixelSize) const = 0; //

    virtual void connectNetworkFolder(const AfsPath& afsPath, bool allowUserInteraction) const = 0; //throw FileError
    //----------------------------------------------------------------------------------------------------------------

    virtual uint64_t getFreeDiskSpace(const AfsPath& afsPath) const = 0; //throw FileError, returns 0 if not available
    virtual bool supportsRecycleBin(const AfsPath& afsPath, const std::function<void ()>& onUpdateGui) const  = 0; //throw FileError
    virtual std::unique_ptr<RecycleSession> createRecyclerSession(const AfsPath& afsPath) const = 0; //throw FileError, return value must be bound!
    virtual void recycleItemIfExists(const AfsPath& afsPath) const = 0; //throw FileError
};


//implement "retry" in a generic way:
template <class Command> inline //function object expecting to throw FileError if operation fails
bool tryReportingDirError(Command cmd, AbstractFileSystem::TraverserCallback& callback) //throw X, return "true" on success, "false" if error was ignored
{
    for (size_t retryNumber = 0;; ++retryNumber)
        try
        {
            cmd(); //throw FileError
            return true;
        }
        catch (const zen::FileError& e)
        {
            switch (callback.reportDirError(e.toString(), retryNumber)) //throw X
            {
                case AbstractFileSystem::TraverserCallback::ON_ERROR_RETRY:
                    break;
                case AbstractFileSystem::TraverserCallback::ON_ERROR_CONTINUE:
                    return false;
            }
        }
}


template <class Command> inline //function object expecting to throw FileError if operation fails
bool tryReportingItemError(Command cmd, AbstractFileSystem::TraverserCallback& callback, const Zstring& itemName) //throw X, return "true" on success, "false" if error was ignored
{
    for (size_t retryNumber = 0;; ++retryNumber)
        try
        {
            cmd(); //throw FileError
            return true;
        }
        catch (const zen::FileError& e)
        {
            switch (callback.reportItemError(e.toString(), retryNumber, itemName)) //throw X
            {
                case AbstractFileSystem::TraverserCallback::ON_ERROR_RETRY:
                    break;
                case AbstractFileSystem::TraverserCallback::ON_ERROR_CONTINUE:
                    return false;
            }
        }
}








//------------------------------------ implementation -----------------------------------------
inline
AbstractPath AbstractFileSystem::appendRelPath(const AbstractPath& ap, const Zstring& relPath)
{
    using namespace zen;

    assert(isValidRelPath(relPath));
    return AbstractPath(ap.afs, AfsPath(appendPaths(ap.afsPath.value, relPath, FILE_NAME_SEPARATOR)));
}


inline
Zstring AbstractFileSystem::appendPaths(const Zstring& basePath, const Zstring& relPath, Zchar pathSep)
{
    using namespace zen;

    assert(!startsWith(relPath, pathSep) && !endsWith(relPath, pathSep));
    if (relPath.empty())
        return basePath;
    if (basePath.empty())
        return relPath;

    if (startsWith(relPath, pathSep))
    {
        if (relPath.size() == 1)
            return basePath;

        if (endsWith(basePath, pathSep))
            return basePath + (relPath.c_str() + 1);
    }
    else if (!endsWith(basePath, pathSep))
    {
        Zstring output = basePath;
        output.reserve(basePath.size() + 1 + relPath.size()); //append all three strings using a single memory allocation
        return std::move(output) + pathSep + relPath;         //
    }

    return basePath + relPath;
}

//--------------------------------------------------------------------------

inline
AbstractFileSystem::OutputStream::OutputStream(std::unique_ptr<OutputStreamImpl>&& outStream, const AbstractPath& filePath, const uint64_t* streamSize) :
    outStream_(std::move(outStream)), filePath_(filePath)
{
    if (streamSize)
        bytesExpected_ = *streamSize;
}


inline
AbstractFileSystem::OutputStream::~OutputStream()
{
    using namespace zen;

    //we delete the file on errors: => file should not have existed prior to creating OutputStream instance!!
    outStream_.reset(); //close file handle *before* remove!

    if (!finalizeSucceeded_) //transactional output stream! => clean up!
        try { AbstractFileSystem::removeFilePlain(filePath_); /*throw FileError*/ }
        catch (FileError& e) { (void)e; }
}


inline
void AbstractFileSystem::OutputStream::write(const void* data, size_t len) //throw FileError, X
{
    outStream_->write(data, len); //throw FileError, X
    bytesWrittenTotal_ += len;
}


inline
AbstractFileSystem::FileId AbstractFileSystem::OutputStream::finalize() //throw FileError, X
{
    using namespace zen;

    //important check: catches corrupt SFTP download with libssh2!
    if (bytesExpected_ && *bytesExpected_ != bytesWrittenTotal_)
        throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(getDisplayPath(filePath_))), //instead we should report the source file, but don't have it here...
                        replaceCpy(replaceCpy(_("Unexpected size of data stream.\nExpected: %x bytes\nActual: %y bytes"),
                                              L"%x", numberTo<std::wstring>(*bytesExpected_)),
                                   L"%y", numberTo<std::wstring>(bytesWrittenTotal_)));

    const FileId fileId = outStream_->finalize(); //throw FileError, X
    finalizeSucceeded_ = true;
    return fileId;
}

//--------------------------------------------------------------------------

inline
bool AbstractFileSystem::supportPermissionCopy(const AbstractPath& apSource, const AbstractPath& apTarget) //throw FileError
{
    if (typeid(*apSource.afs) != typeid(*apTarget.afs))
        return false;

    return apSource.afs->supportsPermissions(apSource.afsPath) && //throw FileError
           apTarget.afs->supportsPermissions(apTarget.afsPath);
}


inline
void AbstractFileSystem::renameItem(const AbstractPath& apSource, const AbstractPath& apTarget) //throw FileError, ErrorDifferentVolume
{
    using namespace zen;

    if (typeid(*apSource.afs) == typeid(*apTarget.afs))
        return apSource.afs->renameItemForSameAfsType(apSource.afsPath, apTarget); //throw FileError, ErrorDifferentVolume

    throw ErrorDifferentVolume(replaceCpy(replaceCpy(_("Cannot move file %x to %y."),
                                                     L"%x", L"\n" + fmtPath(getDisplayPath(apSource))),
                                          L"%y", L"\n" + fmtPath(getDisplayPath(apTarget))), _("Operation not supported for different base folder types."));
}



inline
void AbstractFileSystem::copyNewFolder(const AbstractPath& apSource, const AbstractPath& apTarget, bool copyFilePermissions) //throw FileError
{
    using namespace zen;

    if (typeid(*apSource.afs) == typeid(*apTarget.afs))
        return apSource.afs->copyNewFolderForSameAfsType(apSource.afsPath, apTarget, copyFilePermissions); //throw FileError

    //fall back:
    if (copyFilePermissions)
        throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(getDisplayPath(apTarget))),
                        _("Operation not supported for different base folder types."));

    //target existing: undefined behavior! (fail/overwrite)
    createFolderPlain(apTarget); //throw FileError
}


inline
void AbstractFileSystem::copySymlink(const AbstractPath& apSource, const AbstractPath& apTarget, bool copyFilePermissions) //throw FileError
{
    using namespace zen;

    if (typeid(*apSource.afs) == typeid(*apTarget.afs))
        return apSource.afs->copySymlinkForSameAfsType(apSource.afsPath, apTarget, copyFilePermissions); //throw FileError

    throw FileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."),
                                          L"%x", L"\n" + fmtPath(getDisplayPath(apSource))),
                               L"%y", L"\n" + fmtPath(getDisplayPath(apTarget))), _("Operation not supported for different base folder types."));
}
}

#endif //ABSTRACT_H_873450978453042524534234

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "file_access.h"
//#include <map>
#include <algorithm>
//#include <chrono>
#include <variant>
#include "file_traverser.h"
#include "scope_guard.h"
#include "symlink_target.h"
#include "file_io.h"
#include "crc.h"
#include "guid.h"
#include "ring_buffer.h"

    #include <sys/vfs.h> //statfs
    #ifdef HAVE_SELINUX
        #include <selinux/selinux.h>
    #endif


    #include <fcntl.h> //open, close, AT_SYMLINK_NOFOLLOW, UTIME_OMIT
    #include <sys/stat.h>

using namespace zen;


namespace
{


struct SysErrorCode : public zen::SysError
{
    SysErrorCode(const std::string& functionName, ErrorCode ec) : SysError(formatSystemError(functionName, ec)), errorCode(ec) {}

    const ErrorCode errorCode;
};


ItemType getItemTypeImpl(const Zstring& itemPath) //throw SysErrorCode
{
    struct stat itemInfo = {};
    if (::lstat(itemPath.c_str(), &itemInfo) != 0)
        throw SysErrorCode("lstat", errno);

    if (S_ISLNK(itemInfo.st_mode))
        return ItemType::symlink;
    if (S_ISDIR(itemInfo.st_mode))
        return ItemType::folder;
    return ItemType::file; //S_ISREG || S_ISCHR || S_ISBLK || S_ISFIFO || S_ISSOCK
}


std::variant<ItemType, Zstring /*last existing parent path*/> getItemTypeIfExistsImpl(const Zstring& itemPath) //throw SysError
{
    try
    {
        //fast check: 1. perf 2. expected by getFolderStatusNonBlocking()
        return getItemTypeImpl(itemPath); //throw SysErrorCode
    }
    catch (const SysErrorCode& e) //let's dig deeper, but *only* if error code sounds like "not existing"
    {
        const std::optional<Zstring>& parentPath = getParentFolderPath(itemPath);
        if (!parentPath) //device root => quick access test
            throw;
        if (e.errorCode == ENOENT)
        {
            const std::variant<ItemType, Zstring /*last existing parent path*/> parentTypeOrPath = getItemTypeIfExistsImpl(*parentPath); //throw SysError

            if (const ItemType* parentType = std::get_if<ItemType>(&parentTypeOrPath))
            {
                if (*parentType == ItemType::file /*obscure, but possible*/)
                    throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(getItemName(*parentPath))));

                const Zstring itemName = getItemName(itemPath);
                assert(!itemName.empty());

                try
                {
                    traverseFolder(*parentPath, //throw FileError
                    [&](const    FileInfo& fi) { if (fi.itemName == itemName) throw SysError(_("Temporary access error:") + L' ' + e.toString()); },
                    [&](const  FolderInfo& fi) { if (fi.itemName == itemName) throw SysError(_("Temporary access error:") + L' ' + e.toString()); },
                    [&](const SymlinkInfo& si) { if (si.itemName == itemName) throw SysError(_("Temporary access error:") + L' ' + e.toString()); });
                    //- case-sensitive comparison! itemPath must be normalized!
                    //- finding the item after getItemType() previously failed is exceptional
                }
                catch (const FileError& e2) { throw SysError(replaceCpy(e2.toString(), L"\n\n", L'\n')); }

                return *parentPath;
            }
            else
                return parentTypeOrPath;
        }
        else
            throw;
    }
}
}


ItemType zen::getItemType(const Zstring& itemPath) //throw FileError
{
    try
    {
        return getItemTypeImpl(itemPath); //throw SysErrorCode
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(itemPath)), e.toString()); }
}


std::optional<ItemType> zen::getItemTypeIfExists(const Zstring& itemPath) //throw FileError
{
    try
    {
        const std::variant<ItemType, Zstring /*last existing parent path*/> typeOrPath = getItemTypeIfExistsImpl(itemPath); //throw SysError
        if (const ItemType* type = std::get_if<ItemType>(&typeOrPath))
            return *type;
        else
            return std::nullopt;
    }
    catch (const SysError& e)
    {
        throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(itemPath)), e.toString());
    }
}


namespace
{
}


//- symlink handling: follow
//- returns < 0 if not available
//- folderPath does not need to exist (yet)
int64_t zen::getFreeDiskSpace(const Zstring& folderPath) //throw FileError
{
    try
    {
        const Zstring existingPath = [&]
        {
            const std::variant<ItemType, Zstring /*last existing parent path*/> typeOrPath = getItemTypeIfExistsImpl(folderPath); //throw SysError
            if (std::get_if<ItemType>(&typeOrPath))
                return folderPath;
            else
                return std::get<Zstring>(typeOrPath);
        }();
        struct statfs info = {};
        if (::statfs(existingPath.c_str(), &info) != 0) //follows symlinks!
            THROW_LAST_SYS_ERROR("statfs");
        //Linux: "Fields that are undefined for a particular file system are set to 0."
        //macOS: "Fields that are undefined for a particular file system are set to -1." - mkay :>
        if (makeSigned(info.f_bsize)  <= 0 ||
            makeSigned(info.f_bavail) <= 0)
            return -1;

        return static_cast<int64_t>(info.f_bsize) * info.f_bavail;
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot determine free disk space for %x."), L"%x", fmtPath(folderPath)), e.toString()); }
}


uint64_t zen::getFileSize(const Zstring& filePath) //throw FileError
{
    try
    {
        struct stat fileInfo = {};
        if (::stat(filePath.c_str(), &fileInfo) != 0)
            THROW_LAST_SYS_ERROR("stat");

        return fileInfo.st_size;
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), e.toString()); }
}


Zstring zen::getTempFolderPath() //throw FileError
{
    if (const std::optional<Zstring> tempDirPath = getEnvironmentVar("TMPDIR"))
        return *tempDirPath;
    //TMPDIR not set on CentOS 7, WTF!
    return P_tmpdir; //usually resolves to "/tmp"
}



namespace
{
}


void zen::removeFilePlain(const Zstring& filePath) //throw FileError
{
    try
    {
        if (::unlink(filePath.c_str()) != 0)
            THROW_LAST_SYS_ERROR("unlink");
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete file %x."), L"%x", fmtPath(filePath)), e.toString()); }
}



void zen::removeDirectoryPlain(const Zstring& dirPath) //throw FileError
{
    try
    {
        if (::rmdir(dirPath.c_str()) != 0)
            THROW_LAST_SYS_ERROR("rmdir");
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(dirPath)), e.toString()); }
}


void zen::removeSymlinkPlain(const Zstring& linkPath) //throw FileError
{
    try
    {
        if (::unlink(linkPath.c_str()) != 0)
            THROW_LAST_SYS_ERROR("unlink");
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete symbolic link %x."), L"%x", fmtPath(linkPath)), e.toString()); }
}


namespace
{
void removeDirectoryImpl(const Zstring& folderPath) //throw FileError
{
    std::vector<Zstring> folderPaths;
    {
        std::vector<Zstring> filePaths;
        std::vector<Zstring> symlinkPaths;

        //get all files and directories from current directory (WITHOUT subdirectories!)
        traverseFolder(folderPath,
        [&](const    FileInfo& fi) {    filePaths.push_back(fi.fullPath); },
        [&](const  FolderInfo& fi) {  folderPaths.push_back(fi.fullPath); },
        [&](const SymlinkInfo& si) { symlinkPaths.push_back(si.fullPath); }); //throw FileError

        for (const Zstring& filePath : filePaths)
            removeFilePlain(filePath); //throw FileError

        for (const Zstring& symlinkPath : symlinkPaths)
            removeSymlinkPlain(symlinkPath); //throw FileError
    } //=> save stack space and allow deletion of extremely deep hierarchies!

    //delete directories recursively
    for (const Zstring& subFolderPath : folderPaths)
        removeDirectoryImpl(subFolderPath); //throw FileError; call recursively to correctly handle symbolic links

    removeDirectoryPlain(folderPath); //throw FileError
}
}


void zen::removeDirectoryPlainRecursion(const Zstring& dirPath) //throw FileError
{
    try
    {
        if (getItemTypeImpl(dirPath) == ItemType::symlink) //throw SysErrorCode
            removeSymlinkPlain(dirPath); //throw FileError
        else
            removeDirectoryImpl(dirPath); //throw FileError
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(dirPath)), e.toString()); }
}


namespace
{
std::wstring generateMoveErrorMsg(const Zstring& pathFrom, const Zstring& pathTo)
{
    if (getParentFolderPath(pathFrom) == getParentFolderPath(pathTo)) //pure "rename"
        return replaceCpy(replaceCpy(_("Cannot rename %x to %y."),
                                     L"%x", fmtPath(pathFrom)),
                          L"%y", fmtPath(getItemName(pathTo)));
    else //"move" or "move + rename"
        return trimCpy(replaceCpy(replaceCpy(_("Cannot move %x to %y."),
                                             L"%x", L'\n' + fmtPath(pathFrom)),
                                  L"%y", L'\n' + fmtPath(pathTo)));
}

/* Usage overview: (avoid circular pattern!)

  moveAndRenameItem() --> moveAndRenameFileSub()
      |                            /|\
     \|/                            |
             Fix8Dot3NameClash()                */

//wrapper for file system rename function:
void moveAndRenameFileSub(const Zstring& pathFrom, const Zstring& pathTo, bool replaceExisting) //throw FileError, ErrorMoveUnsupported, ErrorTargetExisting
{
    auto getErrorMsg = [&] { return generateMoveErrorMsg(pathFrom, pathTo); };

    //rename() will never fail with EEXIST, but always (atomically) overwrite!
    //=> equivalent to SetFileInformationByHandle() + FILE_RENAME_INFO::ReplaceIfExists or ::MoveFileEx() + MOVEFILE_REPLACE_EXISTING
    //Linux: renameat2() with RENAME_NOREPLACE -> still new, probably buggy
    //macOS: no solution https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man2/rename.2.html
    if (!replaceExisting)
    {
        struct stat sourceInfo = {};
        if (::lstat(pathFrom.c_str(), &sourceInfo) != 0)
            throw FileError(getErrorMsg(), formatSystemError("lstat(source)", errno));

        struct stat targetInfo = {};
        if (::lstat(pathTo.c_str(), &targetInfo) != 0)
        {
            if (errno != ENOENT)
                throw FileError(getErrorMsg(), formatSystemError("lstat(target)", errno));
        }
        else
        {
            if (sourceInfo.st_dev != targetInfo.st_dev ||
                sourceInfo.st_ino != targetInfo.st_ino)
                throw ErrorTargetExisting(getErrorMsg(), replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(getItemName(pathTo))));
            //else: continue with a rename in case
            //caveat: if we have a hardlink referenced by two different paths, the source one will be unlinked => fine, but not exactly a "rename"...
        }
    }

    if (::rename(pathFrom.c_str(), pathTo.c_str()) != 0)
    {
        if (errno == EXDEV)
            throw ErrorMoveUnsupported(getErrorMsg(), formatSystemError("rename", errno));

        throw FileError(getErrorMsg(), formatSystemError("rename", errno));
    }
}


}


//rename file: no copying!!!
void zen::moveAndRenameItem(const Zstring& pathFrom, const Zstring& pathTo, bool replaceExisting) //throw FileError, ErrorMoveUnsupported, ErrorTargetExisting
{
    try
    {
        moveAndRenameFileSub(pathFrom, pathTo, replaceExisting); //throw FileError, ErrorMoveUnsupported, ErrorTargetExisting
    }
    catch (ErrorTargetExisting&)
    {
        throw;
    }
}

namespace
{
void setWriteTimeNative(const Zstring& itemPath, const timespec& modTime, ProcSymlink procSl) //throw FileError
{
    /* [2013-05-01] sigh, we can't use utimensat() on NTFS volumes on Ubuntu: silent failure!!! what morons are programming this shit???
        => fallback to "retarded-idiot version"! -- DarkByte

        [2015-03-09]
         - cannot reproduce issues with NTFS and utimensat() on Ubuntu
         - utimensat() is supposed to obsolete utime/utimes and is also used by "cp" and "touch"
            => let's give utimensat another chance:
            using open()/futimens() for regular files and utimensat(AT_SYMLINK_NOFOLLOW) for symlinks is consistent with "cp" and "touch"!
        cp:    https://github.com/coreutils/coreutils/blob/master/src/cp.c
            => utimens: https://github.com/coreutils/gnulib/blob/master/lib/utimens.c
        touch: https://github.com/coreutils/coreutils/blob/master/src/touch.c
            => fdutimensat: https://github.com/coreutils/gnulib/blob/master/lib/fdutimensat.c                  */
    const timespec newTimes[2]
    {
        {.tv_sec = ::time(nullptr)}, //access time; don't use UTIME_NOW/UTIME_OMIT: more bugs! https://freefilesync.org/forum/viewtopic.php?t=1701
        modTime,
    };
    //test: even modTime == 0 is correctly applied (no NOOP!) test2: same behavior for "utime()"

    //hell knows why files on gvfs-mounted Samba shares fail to open(O_WRONLY) returning EOPNOTSUPP:
    //https://freefilesync.org/forum/viewtopic.php?t=2803 => utimensat() works (but not for gvfs SFTP)
    if (::utimensat(AT_FDCWD, itemPath.c_str(), newTimes, procSl == ProcSymlink::asLink ? AT_SYMLINK_NOFOLLOW : 0) == 0)
        return;
    const ErrorCode ecUtimensat = errno;
    try
    {
        if (procSl == ProcSymlink::asLink)
        {
            if (getItemTypeImpl(itemPath) == ItemType::symlink) //throw SysErrorCode
                throw SysError(formatSystemError("utimensat(AT_SYMLINK_NOFOLLOW)", ecUtimensat)); //use lutimes()? just a wrapper around utimensat()!
            //else: fall back
        }

        //in other cases utimensat() returns EINVAL for CIFS/NTFS drives, but open+futimens works: https://freefilesync.org/forum/viewtopic.php?t=387
        //2017-07-04: O_WRONLY | O_APPEND seems to avoid EOPNOTSUPP on gvfs SFTP!
        const int fdFile = ::open(itemPath.c_str(), O_WRONLY | O_APPEND | O_CLOEXEC);
        if (fdFile == -1)
            THROW_LAST_SYS_ERROR("open");
        ZEN_ON_SCOPE_EXIT(::close(fdFile));

        if (::futimens(fdFile, newTimes) != 0)
            THROW_LAST_SYS_ERROR("futimens");

        //need  more fallbacks? e.g. futimes()? careful, bugs! futimes() rounds instead of truncates when falling back on utime()!
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(itemPath)), e.toString()); }
}


}


void zen::setFileTime(const Zstring& filePath, time_t modTime, ProcSymlink procSl) //throw FileError
{
    setWriteTimeNative(filePath, timetToNativeFileTime(modTime),
                       procSl); //throw FileError
}


bool zen::supportsPermissions(const Zstring& dirPath) //throw FileError
{
    return true;
}


namespace
{
#ifdef HAVE_SELINUX
//copy SELinux security context
void copySecurityContext(const Zstring& source, const Zstring& target, ProcSymlink procSl) //throw FileError
{
    security_context_t contextSource = nullptr;
    const int rv = procSl == ProcSymlink::follow ?
                   ::getfilecon (source.c_str(), &contextSource) :
                   ::lgetfilecon(source.c_str(), &contextSource);
    if (rv < 0)
    {
        if (errno == ENODATA ||  //no security context (allegedly) is not an error condition on SELinux
            errno == EOPNOTSUPP) //extended attributes are not supported by the filesystem
            return;

        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read security context of %x."), L"%x", fmtPath(source)), "getfilecon");
    }
    ZEN_ON_SCOPE_EXIT(::freecon(contextSource));

    {
        security_context_t contextTarget = nullptr;
        const int rv2 = procSl == ProcSymlink::follow ?
                        ::getfilecon(target.c_str(), &contextTarget) :
                        ::lgetfilecon(target.c_str(), &contextTarget);
        if (rv2 < 0)
        {
            if (errno == EOPNOTSUPP)
                return;
            //else: still try to set security context
        }
        else
        {
            ZEN_ON_SCOPE_EXIT(::freecon(contextTarget));

            if (::strcmp(contextSource, contextTarget) == 0) //nothing to do
                return;
        }
    }

    const int rv3 = procSl == ProcSymlink::follow ?
                    ::setfilecon(target.c_str(), contextSource) :
                    ::lsetfilecon(target.c_str(), contextSource);
    if (rv3 < 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write security context of %x."), L"%x", fmtPath(target)), "setfilecon");
}
#endif
}


//copy permissions for files, directories or symbolic links: requires admin rights
void zen::copyItemPermissions(const Zstring& sourcePath, const Zstring& targetPath, ProcSymlink procSl) //throw FileError
{

#ifdef HAVE_SELINUX  //copy SELinux security context
    copySecurityContext(sourcePath, targetPath, procSl); //throw FileError
#endif

    struct stat fileInfo = {};
    if (procSl == ProcSymlink::follow)
    {
        if (::stat(sourcePath.c_str(), &fileInfo) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtPath(sourcePath)), "stat");

        if (::chown(targetPath.c_str(), fileInfo.st_uid, fileInfo.st_gid) != 0) // may require admin rights!
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), "chown");

        if (::chmod(targetPath.c_str(), fileInfo.st_mode) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), "chmod");
    }
    else
    {
        if (::lstat(sourcePath.c_str(), &fileInfo) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtPath(sourcePath)), "lstat");

        if (::lchown(targetPath.c_str(), fileInfo.st_uid, fileInfo.st_gid) != 0) // may require admin rights!
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), "lchown");

        try
        {
            if (getItemTypeImpl(targetPath) != ItemType::symlink && //throw SysErrorCode
                //setting access permissions doesn't make sense for symlinks on Linux: there is no lchmod()
                ::chmod(targetPath.c_str(), fileInfo.st_mode) != 0)
                THROW_LAST_SYS_ERROR("chmod");
        }
        catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), e.toString()); }
    }

}


void zen::createDirectory(const Zstring& dirPath) //throw FileError, ErrorTargetExisting
{
    try
    {
        //don't allow creating irregular folders!
        const Zstring dirName = getItemName(dirPath);

        //e.g. "...." https://social.technet.microsoft.com/Forums/windows/en-US/ffee2322-bb6b-4fdf-86f9-8f93cf1fa6cb/
        if (std::all_of(dirName.begin(), dirName.end(), [](Zchar c) { return c == Zstr('.'); }))
        /**/throw SysError(replaceCpy<std::wstring>(L"Invalid folder name %x.", L"%x", fmtPath(dirName)));

#if 0 //not appreciated: https://freefilesync.org/forum/viewtopic.php?t=7509
        if (startsWith(dirName, Zstr(' ')) || //Windows can access these just fine once created!
            endsWith  (dirName, Zstr(' ')))   //
            throw SysError(replaceCpy<std::wstring>(L"Invalid folder name %x starts/ends with space character.", L"%x", fmtPath(dirName)));
#endif

        const mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO; //0777 => consider umask!

        if (::mkdir(dirPath.c_str(), mode) != 0)
        {
            const int ec = errno; //copy before directly or indirectly making other system calls!
            if (ec == EEXIST)
                throw ErrorTargetExisting(replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(dirPath)), formatSystemError("mkdir", ec));
            THROW_LAST_SYS_ERROR("mkdir");
        }
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(dirPath)), e.toString()); }
}


void zen::createDirectoryIfMissingRecursion(const Zstring& dirPath) //throw FileError
{
    auto getItemType2 = [&](const Zstring& itemPath) //throw FileError
    {
        try
        { return getItemTypeImpl(itemPath); } //throw SysErrorCode
        catch (const SysErrorCode& e) //need to add context!
        {
            throw FileError(replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(dirPath)),
                            replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(getParentFolderPath(itemPath) ? getItemName(itemPath) : itemPath)) + L'\n' +
                            e.toString());
        }
    };

    try
    {
        //- path most likely already exists (see: versioning, base folder, log file path) => check first
        //- do NOT use getItemTypeIfExists()! race condition when multiple threads are calling createDirectoryIfMissingRecursion(): https://freefilesync.org/forum/viewtopic.php?t=10137#p38062
        //- find first existing + accessible parent folder (backwards iteration):
        Zstring dirPathEx = dirPath;
        RingBuffer<Zstring> dirNames; //caveat: 1. might have been created in the meantime 2. getItemType2() may have failed with access error
        for (;;)
            try
            {
                if (getItemType2(dirPathEx) == ItemType::file /*obscure, but possible*/) //throw FileError
                    throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(getItemName(dirPathEx))));
                break;
            }
            catch (FileError&) //not yet existing or access error
            {
                const std::optional<Zstring>& parentPath = getParentFolderPath(dirPathEx);
                if (!parentPath)//device root => quick access test
                    throw;
                dirNames.push_front(getItemName(dirPathEx));
                dirPathEx = *parentPath;
            }
        //-----------------------------------------------------------

        Zstring dirPathNew = dirPathEx;
        for (const Zstring& dirName : dirNames)
            try
            {
                dirPathNew = appendPath(dirPathNew, dirName);

                createDirectory(dirPathNew); //throw FileError
            }
            catch (FileError&)
            {
                try
                {
                    if (getItemType2(dirPathNew) == ItemType::file /*obscure, but possible*/) //throw FileError
                        throw SysError(replaceCpy(_("The name %x is already used by another item."), L"%x", fmtPath(getItemName(dirPathNew))));
                    else
                        continue; //already existing => possible, if createDirectoryIfMissingRecursion() is run in parallel
                }
                catch (FileError&) {} //not yet existing or access error

                throw;
            }
    }
    catch (const SysError& e)
    {
        throw FileError(replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(dirPath)), e.toString());
    }
}


void zen::copyDirectoryAttributes(const Zstring& sourcePath, const Zstring& targetPath) //throw FileError
{
    //do NOT copy attributes for volume root paths which return as: FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY
    //https://freefilesync.org/forum/viewtopic.php?t=5550
    if (!getParentFolderPath(sourcePath)) //=> root path
        return;

}


void zen::copySymlink(const Zstring& sourcePath, const Zstring& targetPath) //throw FileError
{
    SymlinkRawContent linkContent{};
    try //harmonize with NativeFileSystem::equalSymlinkContentForSameAfsType()
    {
        linkContent = getSymlinkRawContent_impl(sourcePath); //throw SysError; accept broken symlinks

        if (::symlink(linkContent.targetPath.c_str(), targetPath.c_str()) != 0)
            THROW_LAST_SYS_ERROR("symlink");
    }
    catch (const SysError& e)
    {
        throw FileError(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."), L"%x", L'\n' + fmtPath(sourcePath)), L"%y", L'\n' + fmtPath(targetPath)), e.toString());
    }

    //allow only consistent objects to be created -> don't place before ::symlink(); targetPath may already exist!
    ZEN_ON_SCOPE_FAIL(try { removeSymlinkPlain(targetPath); }
    catch (const FileError& e) { logExtraError(e.toString()); });

    //file times: essential for syncing a symlink: enforce this! (don't just try!)
    struct stat sourceInfo = {};
    if (::lstat(sourcePath.c_str(), &sourceInfo) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(sourcePath)), "lstat");

    setWriteTimeNative(targetPath, sourceInfo.st_mtim, ProcSymlink::asLink); //throw FileError
}


FileCopyResult zen::copyNewFile(const Zstring& sourceFile, const Zstring& targetFile, //throw FileError, ErrorTargetExisting, (ErrorFileLocked), X
                                const IoCallback& notifyUnbufferedIO /*throw X*/)
{
    int64_t totalBytesNotified = 0;
    IOCallbackDivider notifyIoDiv(notifyUnbufferedIO, totalBytesNotified);

    FileInputPlain fileIn(sourceFile); //throw FileError, (ErrorFileLocked -> Windows-only)

    const struct stat& sourceInfo = fileIn.getStatBuffered(); //throw FileError

    //analog to "cp" which copies "mode" (considering umask) by default:
    const mode_t mode = (sourceInfo.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) |
                        S_IWUSR;//macOS: S_IWUSR apparently needed to write extended attributes (see copyfile() function)
    //Linux: not needed even for the setFileTime() below! (tested with source file having different user/group!)

    //=> need copyItemPermissions() only for "chown" and umask-agnostic permissions
    const int fdTarget = ::open(targetFile.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, mode);
    if (fdTarget == -1)
    {
        const int ec = errno; //copy before making other system calls!
        const std::wstring errorMsg = replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(targetFile));
        const std::wstring errorDescr = formatSystemError("open", ec);

        if (ec == EEXIST)
            throw ErrorTargetExisting(errorMsg, errorDescr);

        throw FileError(errorMsg, errorDescr);
    }
    FileOutputPlain fileOut(fdTarget, targetFile); //pass ownership

    //preallocate disk space + reduce fragmentation
    fileOut.reserveSpace(sourceInfo.st_size); //throw FileError

    unbufferedStreamCopy([&](void* buffer, size_t bytesToRead)
    {
        const size_t bytesRead = fileIn.tryRead(buffer, bytesToRead); //throw FileError, (ErrorFileLocked)
        notifyIoDiv(bytesRead); //throw X
        return bytesRead;
    },
    fileIn.getBlockSize() /*throw FileError*/,

    [&](const void* buffer, size_t bytesToWrite)
    {
        const size_t bytesWritten = fileOut.tryWrite(buffer, bytesToWrite); //throw FileError
        notifyIoDiv(bytesWritten); //throw X
        return bytesWritten;
    },
    fileOut.getBlockSize() /*throw FileError*/); //throw FileError, X

    //possible improvement: copy_file_range() performs an in-kernel copy: https://github.com/coreutils/coreutils/blob/17479ef60c8edbd2fe8664e31a7f69704f0cd221/src/copy.c#L342

#if 0
    //clean file system cache: needed at all? no user complaints at all so far!!!
    //posix_fadvise(POSIX_FADV_DONTNEED) does nothing, unless data was already read from/written to disk: https://insights.oetiker.ch/linux/fadvise/
    //    => should be "most" of the data at this point => good enough?
    if (::posix_fadvise(fileIn.getHandle(), 0 /*offset*/, 0 /*len*/, POSIX_FADV_DONTNEED) != 0) //"len == 0" means "end of the file"
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(sourceFile)), "posix_fadvise(POSIX_FADV_DONTNEED)");
    if (::posix_fadvise(fileOut.getHandle(), 0 /*offset*/, 0 /*len*/, POSIX_FADV_DONTNEED) != 0) //"len == 0" means "end of the file"
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(targetFile)), "posix_fadvise(POSIX_FADV_DONTNEED)");
#endif


    const auto targetFileIdx = fileOut.getStatBuffered().st_ino; //throw FileError

    //close output file handle before setting file time; also good place to catch errors when closing stream!
    fileOut.close(); //throw FileError
    //==========================================================================================================
    //take over fileOut ownership => from this point on, WE are responsible for calling removeFilePlain() on error!!
    // not needed *currently*! see below: ZEN_ON_SCOPE_FAIL(try { removeFilePlain(targetFile); } catch (FileError&) {});
    //===========================================================================================================
    std::optional<FileError> errorModTime;
    try
    {
        /*  we cannot set the target file times (::futimes) while the file descriptor is still open after a write operation:
            this triggers bugs on Samba shares where the modification time is set to current time instead.
            Linux: https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=340236
                   http://comments.gmane.org/gmane.linux.file-systems.cifs/2854
            macOS: https://freefilesync.org/forum/viewtopic.php?t=356             */
        setWriteTimeNative(targetFile, sourceInfo.st_mtim, ProcSymlink::follow); //throw FileError
    }
    catch (const FileError& e) { errorModTime = e; /*might slice derived class?*/ }

    return
    {
        .fileSize      = makeUnsigned(sourceInfo.st_size),
        .sourceModTime = sourceInfo.st_mtim,
        .sourceFileIdx = sourceInfo.st_ino,
        .targetFileIdx = targetFileIdx,
        .errorModTime  = errorModTime,
    };
}



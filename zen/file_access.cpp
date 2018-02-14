// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "file_access.h"
#include <map>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include "file_traverser.h"
#include "scope_guard.h"
#include "symlink_target.h"
#include "file_id_def.h"
#include "file_io.h"
#include "crc.h"  //boost dependency!
#include "guid.h" //

    #include <sys/vfs.h> //statfs
    #include <sys/time.h> //lutimes
    #ifdef HAVE_SELINUX
        #include <selinux/selinux.h>
    #endif


    #include <fcntl.h> //open, close, AT_SYMLINK_NOFOLLOW, UTIME_OMIT
    #include <sys/stat.h>

using namespace zen;


Opt<PathComponents> zen::parsePathComponents(const Zstring& itemPath)
{
    if (startsWith(itemPath, "/"))
    {
        Zstring relPath(itemPath.c_str() + 1);
        if (endsWith(relPath, FILE_NAME_SEPARATOR))
            relPath.pop_back();
        return PathComponents({ "/", relPath });
    }
    //we do NOT support relative paths!
    return NoValue();
}



Opt<Zstring> zen::getParentFolderPath(const Zstring& itemPath)
{
    if (const Opt<PathComponents> comp = parsePathComponents(itemPath))
    {
        if (comp->relPath.empty())
            return NoValue();

        const Zstring parentRelPath = beforeLast(comp->relPath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
        if (parentRelPath.empty())
            return comp->rootPath;
        return appendSeparator(comp->rootPath) + parentRelPath;
    }
    assert(false);
    return NoValue();
}


ItemType zen::getItemType(const Zstring& itemPath) //throw FileError
{
    struct ::stat itemInfo = {};
    if (::lstat(itemPath.c_str(), &itemInfo) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(itemPath)), L"lstat");

    if (S_ISLNK(itemInfo.st_mode))
        return ItemType::SYMLINK;
    if (S_ISDIR(itemInfo.st_mode))
        return ItemType::FOLDER;
    return ItemType::FILE; //S_ISREG || S_ISCHR || S_ISBLK || S_ISFIFO || S_ISSOCK
}


PathStatus zen::getPathStatus(const Zstring& itemPath) //throw FileError
{
    const Opt<Zstring> parentPath = getParentFolderPath(itemPath);
    try
    {
        return { getItemType(itemPath), itemPath, {} }; //throw FileError
    }
    catch (FileError&)
    {
        if (!parentPath) //device root
            throw;
        //else: let's dig deeper... don't bother checking Win32 codes; e.g. not existing item may have the codes:
        //  ERROR_FILE_NOT_FOUND, ERROR_PATH_NOT_FOUND, ERROR_INVALID_NAME, ERROR_INVALID_DRIVE,
        //  ERROR_NOT_READY, ERROR_INVALID_PARAMETER, ERROR_BAD_PATHNAME, ERROR_BAD_NETPATH => not reliable
    }
    const Zstring itemName = afterLast(itemPath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL);
    assert(!itemName.empty());

    PathStatus ps = getPathStatus(*parentPath); //throw FileError
    if (ps.relPath.empty() &&
        ps.existingType != ItemType::FILE) //obscure, but possible (and not an error)
        try
        {
            traverseFolder(*parentPath,
            [&](const FileInfo&    fi) { if (equalFilePath(fi.itemName, itemName)) throw ItemType::FILE; },
            [&](const FolderInfo&  fi) { if (equalFilePath(fi.itemName, itemName)) throw ItemType::FOLDER; },
            [&](const SymlinkInfo& si) { if (equalFilePath(si.itemName, itemName)) throw ItemType::SYMLINK; },
            [](const std::wstring& errorMsg) { throw FileError(errorMsg); });
        }
        catch (const ItemType& type) { return { type, itemPath, {} }; } //yes, exceptions for control-flow are bad design... but, but...
    //we're not CPU-bound here and finding the item after getItemType() previously failed is exceptional (even C:\pagefile.sys should be found)

    ps.relPath.push_back(itemName);
    return ps;
}


Opt<ItemType> zen::getItemTypeIfExists(const Zstring& itemPath) //throw FileError
{
    const PathStatus pd = getPathStatus(itemPath); //throw FileError
    if (pd.relPath.empty())
        return pd.existingType;
    return NoValue();
}


bool zen::fileAvailable(const Zstring& filePath) //noexcept
{
    //symbolic links (broken or not) are also treated as existing files!
    struct ::stat fileInfo = {};
    if (::stat(filePath.c_str(), &fileInfo) == 0) //follow symlinks!
        return S_ISREG(fileInfo.st_mode);
    return false;
}


bool zen::dirAvailable(const Zstring& dirPath) //noexcept
{
    //symbolic links (broken or not) are also treated as existing directories!
    struct ::stat dirInfo = {};
    if (::stat(dirPath.c_str(), &dirInfo) == 0) //follow symlinks!
        return S_ISDIR(dirInfo.st_mode);
    return false;
}


bool zen::itemNotExisting(const Zstring& itemPath)
{
    try
    {
        return !getItemTypeIfExists(itemPath); //throw FileError
    }
    catch (FileError&) { return false; }
}


namespace
{
}


uint64_t zen::getFileSize(const Zstring& filePath) //throw FileError
{
    struct ::stat fileInfo = {};
    if (::stat(filePath.c_str(), &fileInfo) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(filePath)), L"stat");

    return fileInfo.st_size;
}


uint64_t zen::getFreeDiskSpace(const Zstring& path) //throw FileError, returns 0 if not available
{
    struct ::statfs info = {};
    if (::statfs(path.c_str(), &info) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot determine free disk space for %x."), L"%x", fmtPath(path)), L"statfs");

    return static_cast<uint64_t>(info.f_bsize) * info.f_bavail;
}


VolumeId zen::getVolumeId(const Zstring& itemPath) //throw FileError
{
    struct ::stat fileInfo = {};
    if (::stat(itemPath.c_str(), &fileInfo) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(itemPath)), L"stat");

    return fileInfo.st_dev;
}


Zstring zen::getTempFolderPath() //throw FileError
{
    const char* buf = ::getenv("TMPDIR"); //no extended error reporting
    if (!buf)
        throw FileError(_("Cannot get process information."), L"getenv: TMPDIR not found.");
    return buf;
}


void zen::removeFilePlain(const Zstring& filePath) //throw FileError
{
    const wchar_t functionName[] = L"unlink";
    if (::unlink(filePath.c_str()) != 0)
    {
        ErrorCode ec = getLastError(); //copy before directly/indirectly making other system calls!
        //begin of "regular" error reporting
        std::wstring errorDescr = formatSystemError(functionName, ec);

        throw FileError(replaceCpy(_("Cannot delete file %x."), L"%x", fmtPath(filePath)), errorDescr);
    }
}


void zen::removeSymlinkPlain(const Zstring& linkPath) //throw FileError
{
    removeFilePlain(linkPath); //throw FileError
}


void zen::removeDirectoryPlain(const Zstring& dirPath) //throw FileError
{
    const wchar_t functionName[] = L"rmdir";
    if (::rmdir(dirPath.c_str()) != 0)
    {
        ErrorCode ec = getLastError(); //copy before making other system calls!
        bool symlinkExists = false;
        try { symlinkExists = getItemType(dirPath) == ItemType::SYMLINK; } /*throw FileError*/ catch (FileError&) {} //previous exception is more relevant

        if (symlinkExists)
        {
            if (::unlink(dirPath.c_str()) != 0)
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(dirPath)), L"unlink");
            return;
        }
        throw FileError(replaceCpy(_("Cannot delete directory %x."), L"%x", fmtPath(dirPath)), formatSystemError(functionName, ec));
    }
    /*
    Windows: may spuriously fail with ERROR_DIR_NOT_EMPTY(145) even though all child items have
    successfully been *marked* for deletion, but some application still has a handle open!
    e.g. Open "C:\Test\Dir1\Dir2" (filled with lots of files) in Explorer, then delete "C:\Test\Dir1" via ::RemoveDirectory() => Error 145
    Sample code: http://us.generation-nt.com/answer/createfile-directory-handles-removing-parent-help-29126332.html
    Alternatives: 1. move file/empty folder to some other location, then DeleteFile()/RemoveDirectory()
                  2. use CreateFile/FILE_FLAG_DELETE_ON_CLOSE *without* FILE_SHARE_DELETE instead of DeleteFile() => early failure
    */
}


namespace
{
void removeDirectoryImpl(const Zstring& folderPath) //throw FileError
{
    std::vector<Zstring> filePaths;
    std::vector<Zstring> symlinkPaths;
    std::vector<Zstring> folderPaths;

    //get all files and directories from current directory (WITHOUT subdirectories!)
    traverseFolder(folderPath,
    [&](const FileInfo&    fi) { filePaths   .push_back(fi.fullPath); },
    [&](const FolderInfo&  fi) { folderPaths .push_back(fi.fullPath); }, //defer recursion => save stack space and allow deletion of extremely deep hierarchies!
    [&](const SymlinkInfo& si) { symlinkPaths.push_back(si.fullPath); },
    [](const std::wstring& errorMsg) { throw FileError(errorMsg); });

    for (const Zstring& filePath : filePaths)
        removeFilePlain(filePath); //throw FileError

    for (const Zstring& symlinkPath : symlinkPaths)
        removeSymlinkPlain(symlinkPath); //throw FileError

    //delete directories recursively
    for (const Zstring& subFolderPath : folderPaths)
        removeDirectoryImpl(subFolderPath); //throw FileError; call recursively to correctly handle symbolic links

    removeDirectoryPlain(folderPath); //throw FileError
}
}


void zen::removeDirectoryPlainRecursion(const Zstring& dirPath) //throw FileError
{
    if (getItemType(dirPath) == ItemType::SYMLINK) //throw FileError
        removeSymlinkPlain(dirPath); //throw FileError
    else
        removeDirectoryImpl(dirPath); //throw FileError
}


namespace
{

/* Usage overview: (avoid circular pattern!)

  renameFile()  -->  renameFile_sub()
      |               /|\
     \|/               |
      Fix8Dot3NameClash()
*/
//wrapper for file system rename function:
void renameFile_sub(const Zstring& pathSource, const Zstring& pathTarget) //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
{
    //rename() will never fail with EEXIST, but always (atomically) overwrite!
    //=> equivalent to SetFileInformationByHandle() + FILE_RENAME_INFO::ReplaceIfExists or ::MoveFileEx + MOVEFILE_REPLACE_EXISTING
    //=> Linux: renameat2() with RENAME_NOREPLACE -> still new, probably buggy
    //=> OS X: no solution

    auto throwException = [&](int ec)
    {
        const std::wstring errorMsg = replaceCpy(replaceCpy(_("Cannot move file %x to %y."), L"%x", L"\n" + fmtPath(pathSource)), L"%y", L"\n" + fmtPath(pathTarget));
        const std::wstring errorDescr = formatSystemError(L"rename", ec);

        if (ec == EXDEV)
            throw ErrorDifferentVolume(errorMsg, errorDescr);
        if (ec == EEXIST)
            throw ErrorTargetExisting(errorMsg, errorDescr);
        throw FileError(errorMsg, errorDescr);
    };

    if (!equalFilePath(pathSource, pathTarget)) //exception for OS X: changing file name case is not an "already exists" situation!
    {
        bool alreadyExists = true;
        try { /*ItemType type = */getItemType(pathTarget); } /*throw FileError*/ catch (FileError&) { alreadyExists = false; }

        if (alreadyExists)
            throwException(EEXIST);
        //else: nothing exists or other error (hopefully ::rename will also fail!)
    }

    if (::rename(pathSource.c_str(), pathTarget.c_str()) != 0)
        throwException(errno);
}


}


//rename file: no copying!!!
void zen::renameFile(const Zstring& pathSource, const Zstring& pathTarget) //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
{
    try
    {
        renameFile_sub(pathSource, pathTarget); //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
    }
    catch (ErrorTargetExisting&)
    {
#if 0 //"Work around pen drive failing to change file name case" => enable if needed: https://www.freefilesync.org/forum/viewtopic.php?t=4279
        const Zstring fileNameSrc   = afterLast (pathSource, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL);
        const Zstring fileNameTrg   = afterLast (pathTarget, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL);
        const Zstring parentPathSrc = beforeLast(pathSource, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
        const Zstring parentPathTrg = beforeLast(pathTarget, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
        //some (broken) devices may fail to rename case directly:
        if (equalFilePath(parentPathSrc, parentPathTrg))
        {
            if (fileNameSrc == fileNameTrg)
                return; //non-sensical request

            const Zstring tempFilePath = getTemporaryPath8Dot3(pathSource); //throw FileError
            renameFile_sub(pathSource, tempFilePath); //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
            ZEN_ON_SCOPE_FAIL(renameFile_sub(tempFilePath, pathSource)); //"try" our best to be "atomic" :>
            renameFile_sub(tempFilePath, pathTarget); //throw FileError, ErrorDifferentVolume, ErrorTargetExisting
            return;
        }
#endif

        throw;
    }
}


namespace
{
void setWriteTimeNative(const Zstring& itemPath, const struct ::timespec& modTime, ProcSymlink procSl) //throw FileError
{
    /*
    [2013-05-01] sigh, we can't use utimensat() on NTFS volumes on Ubuntu: silent failure!!! what morons are programming this shit???
    => fallback to "retarded-idiot version"! -- DarkByte

    [2015-03-09]
     - cannot reproduce issues with NTFS and utimensat() on Ubuntu
     - utimensat() is supposed to obsolete utime/utimes and is also used by "cp" and "touch"
        => let's give utimensat another chance:
        using open()/futimens() for regular files and utimensat(AT_SYMLINK_NOFOLLOW) for symlinks is consistent with "cp" and "touch"!
    */
    struct ::timespec newTimes[2] = {};
    newTimes[0].tv_sec = ::time(nullptr); //access time; using UTIME_OMIT for tv_nsec would trigger even more bugs: https://www.freefilesync.org/forum/viewtopic.php?t=1701
    newTimes[1] = modTime; //modification time

    if (procSl == ProcSymlink::FOLLOW)
    {
        //hell knows why files on gvfs-mounted Samba shares fail to open(O_WRONLY) returning EOPNOTSUPP:
        //https://www.freefilesync.org/forum/viewtopic.php?t=2803 => utimensat() works (but not for gvfs SFTP)
        if (::utimensat(AT_FDCWD, itemPath.c_str(), newTimes, 0) == 0)
            return;

        //in other cases utimensat() returns EINVAL for CIFS/NTFS drives, but open+futimens works: https://www.freefilesync.org/forum/viewtopic.php?t=387
        const int fdFile = ::open(itemPath.c_str(), O_WRONLY | O_APPEND); //2017-07-04: O_WRONLY | O_APPEND seems to avoid EOPNOTSUPP on gvfs SFTP!
        if (fdFile == -1)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(itemPath)), L"open");
        ZEN_ON_SCOPE_EXIT(::close(fdFile));

        if (::futimens(fdFile, newTimes) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(itemPath)), L"futimens");
    }
    else
    {
        if (::utimensat(AT_FDCWD, itemPath.c_str(), newTimes, AT_SYMLINK_NOFOLLOW) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write modification time of %x."), L"%x", fmtPath(itemPath)), L"utimensat");
    }
}


}


void zen::setFileTime(const Zstring& filePath, time_t modTime, ProcSymlink procSl) //throw FileError
{
    struct ::timespec writeTime = {};
    writeTime.tv_sec = modTime;
    setWriteTimeNative(filePath, writeTime, procSl); //throw FileError

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
    const int rv = procSl == ProcSymlink::FOLLOW ?
                   ::getfilecon(source.c_str(), &contextSource) :
                   ::lgetfilecon(source.c_str(), &contextSource);
    if (rv < 0)
    {
        if (errno == ENODATA ||  //no security context (allegedly) is not an error condition on SELinux
            errno == EOPNOTSUPP) //extended attributes are not supported by the filesystem
            return;

        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read security context of %x."), L"%x", fmtPath(source)), L"getfilecon");
    }
    ZEN_ON_SCOPE_EXIT(::freecon(contextSource));

    {
        security_context_t contextTarget = nullptr;
        const int rv2 = procSl == ProcSymlink::FOLLOW ?
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

    const int rv3 = procSl == ProcSymlink::FOLLOW ?
                    ::setfilecon(target.c_str(), contextSource) :
                    ::lsetfilecon(target.c_str(), contextSource);
    if (rv3 < 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write security context of %x."), L"%x", fmtPath(target)), L"setfilecon");
}
#endif
}


//copy permissions for files, directories or symbolic links: requires admin rights
void zen::copyItemPermissions(const Zstring& sourcePath, const Zstring& targetPath, ProcSymlink procSl) //throw FileError
{

#ifdef HAVE_SELINUX  //copy SELinux security context
    copySecurityContext(sourcePath, targetPath, procSl); //throw FileError
#endif

    struct ::stat fileInfo = {};
    if (procSl == ProcSymlink::FOLLOW)
    {
        if (::stat(sourcePath.c_str(), &fileInfo) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtPath(sourcePath)), L"stat");

        if (::chown(targetPath.c_str(), fileInfo.st_uid, fileInfo.st_gid) != 0) // may require admin rights!
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), L"chown");

        if (::chmod(targetPath.c_str(), fileInfo.st_mode) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), L"chmod");
    }
    else
    {
        if (::lstat(sourcePath.c_str(), &fileInfo) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read permissions of %x."), L"%x", fmtPath(sourcePath)), L"lstat");

        if (::lchown(targetPath.c_str(), fileInfo.st_uid, fileInfo.st_gid) != 0) // may require admin rights!
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), L"lchown");

        const bool isSymlinkTarget = getItemType(targetPath) == ItemType::SYMLINK; //throw FileError
        if (!isSymlinkTarget && //setting access permissions doesn't make sense for symlinks on Linux: there is no lchmod()
            ::chmod(targetPath.c_str(), fileInfo.st_mode) != 0)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot write permissions of %x."), L"%x", fmtPath(targetPath)), L"chmod");
    }

}


void zen::createDirectory(const Zstring& dirPath) //throw FileError, ErrorTargetExisting
{
    const mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO; //0777, default for newly created directories

    if (::mkdir(dirPath.c_str(), mode) != 0)
    {
        const int lastError = errno; //copy before directly or indirectly making other system calls!
        const std::wstring errorMsg = replaceCpy(_("Cannot create directory %x."), L"%x", fmtPath(dirPath));
        const std::wstring errorDescr = formatSystemError(L"mkdir", lastError);

        if (lastError == EEXIST)
            throw ErrorTargetExisting(errorMsg, errorDescr);
        //else if (lastError == ENOENT)
        //    throw ErrorTargetPathMissing(errorMsg, errorDescr);
        throw FileError(errorMsg, errorDescr);
    }
}


void zen::createDirectoryIfMissingRecursion(const Zstring& dirPath) //throw FileError
{
    if (!getParentFolderPath(dirPath)) //device root
        return static_cast<void>(/*ItemType =*/ getItemType(dirPath)); //throw FileError

    try
    {
        createDirectory(dirPath); //throw FileError, ErrorTargetExisting
    }
    catch (FileError&)
    {
        Opt<PathStatus> pd;
        try { pd = getPathStatus(dirPath); /*throw FileError*/ }
        catch (FileError&) {} //previous exception is more relevant

        if (pd &&
            pd->existingType != ItemType::FILE &&
            pd->relPath.size() != 1) //don't repeat the very same createDirectory() call from above!
        {
            Zstring intermediatePath = pd->existingPath;
            for (const Zstring& itemName : pd->relPath)
            {
                intermediatePath = appendSeparator(intermediatePath) + itemName;
                createDirectory(intermediatePath); //throw FileError, (ErrorTargetExisting)
            }
            return;
        }
        throw;
    }
}


void zen::tryCopyDirectoryAttributes(const Zstring& sourcePath, const Zstring& targetPath) //throw FileError
{
}


void zen::copySymlink(const Zstring& sourceLink, const Zstring& targetLink, bool copyFilePermissions) //throw FileError
{
    const Zstring linkPath = getSymlinkTargetRaw(sourceLink); //throw FileError; accept broken symlinks

    const wchar_t functionName[] = L"symlink";
    if (::symlink(linkPath.c_str(), targetLink.c_str()) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(replaceCpy(_("Cannot copy symbolic link %x to %y."), L"%x", L"\n" + fmtPath(sourceLink)), L"%y", L"\n" + fmtPath(targetLink)), functionName);

    //allow only consistent objects to be created -> don't place before ::symlink, targetLink may already exist!
    ZEN_ON_SCOPE_FAIL(try { removeSymlinkPlain(targetLink); /*throw FileError*/ }
    catch (FileError&) {});

    //file times: essential for sync'ing a symlink: enforce this! (don't just try!)
    struct ::stat sourceInfo = {};
    if (::lstat(sourceLink.c_str(), &sourceInfo) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(sourceLink)), L"lstat");

    setWriteTimeNative(targetLink, sourceInfo.st_mtim, ProcSymlink::DIRECT); //throw FileError

    if (copyFilePermissions)
        copyItemPermissions(sourceLink, targetLink, ProcSymlink::DIRECT); //throw FileError
}


namespace
{
FileCopyResult copyFileOsSpecific(const Zstring& sourceFile, //throw FileError, ErrorTargetExisting
                                  const Zstring& targetFile,
                                  const IOCallback& notifyUnbufferedIO)
{
    int64_t totalUnbufferedIO = 0;

    FileInput fileIn(sourceFile, IOCallbackDivider(notifyUnbufferedIO, totalUnbufferedIO)); //throw FileError, (ErrorFileLocked -> Windows-only)

    struct ::stat sourceInfo = {};
    if (::fstat(fileIn.getHandle(), &sourceInfo) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(sourceFile)), L"fstat");

    const mode_t mode = sourceInfo.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO); //analog to "cp" which copies "mode" (considering umask) by default
    //it seems we don't need S_IWUSR, not even for the setFileTime() below! (tested with source file having different user/group!)

    //=> need copyItemPermissions() only for "chown" and umask-agnostic permissions
    const int fdTarget = ::open(targetFile.c_str(), O_WRONLY | O_CREAT | O_EXCL, mode);
    if (fdTarget == -1)
    {
        const int ec = errno; //copy before making other system calls!
        const std::wstring errorMsg = replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(targetFile));
        const std::wstring errorDescr = formatSystemError(L"open", ec);

        if (ec == EEXIST)
            throw ErrorTargetExisting(errorMsg, errorDescr);

        throw FileError(errorMsg, errorDescr);
    }
    ZEN_ON_SCOPE_FAIL( try { removeFilePlain(targetFile); }
    catch (FileError&) {} );
    //place guard AFTER ::open() and BEFORE lifetime of FileOutput:
    //=> don't delete file that existed previously!!!
    FileOutput fileOut(fdTarget, targetFile, IOCallbackDivider(notifyUnbufferedIO, totalUnbufferedIO)); //pass ownership

    //fileOut.preAllocateSpaceBestEffort(sourceInfo.st_size); //throw FileError
    //=> perf: seems like no real benefit...

    bufferedStreamCopy(fileIn, fileOut); //throw FileError, (ErrorFileLocked), X

    //flush intermediate buffers before fiddling with the raw file handle
    fileOut.flushBuffers(); //throw FileError, X

    struct ::stat targetInfo = {};
    if (::fstat(fileOut.getHandle(), &targetInfo) != 0)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(targetFile)), L"fstat");

    //close output file handle before setting file time; also good place to catch errors when closing stream!
    fileOut.finalize(); //throw FileError, (X)  essentially a close() since  buffers were already flushed

    Opt<FileError> errorModTime;
    try
    {
        //we cannot set the target file times (::futimes) while the file descriptor is still open after a write operation:
        //this triggers bugs on samba shares where the modification time is set to current time instead.
        //Linux: http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=340236
        //       http://comments.gmane.org/gmane.linux.file-systems.cifs/2854
        //OS X:  https://www.freefilesync.org/forum/viewtopic.php?t=356
        setWriteTimeNative(targetFile, sourceInfo.st_mtim, ProcSymlink::FOLLOW); //throw FileError
    }
    catch (const FileError& e)
    {
        errorModTime = FileError(e.toString()); //avoid slicing
    }

    FileCopyResult result;
    result.fileSize = sourceInfo.st_size;
    result.modTime = sourceInfo.st_mtim.tv_sec; //
    result.sourceFileId = extractFileId(sourceInfo);
    result.targetFileId = extractFileId(targetInfo);
    result.errorModTime = errorModTime;
    return result;
}

/*                  ------------------
                    |File Copy Layers|
                    ------------------
                       copyNewFile
                            |
                   copyFileOsSpecific (solve 8.3 issue on Windows)
                            |
                  copyFileWindowsSelectRoutine
                  /                           \
copyFileWindowsDefault(::CopyFileEx)  copyFileWindowsStream(::BackupRead/::BackupWrite)
*/
}


FileCopyResult zen::copyNewFile(const Zstring& sourceFile, const Zstring& targetFile, bool copyFilePermissions, //throw FileError, ErrorTargetExisting, ErrorFileLocked
                                const IOCallback& notifyUnbufferedIO)
{
    const FileCopyResult result = copyFileOsSpecific(sourceFile, targetFile, notifyUnbufferedIO); //throw FileError, ErrorTargetExisting, ErrorFileLocked

    //at this point we know we created a new file, so it's fine to delete it for cleanup!
    ZEN_ON_SCOPE_FAIL(try { removeFilePlain(targetFile); }
    catch (FileError&) {});

    if (copyFilePermissions)
        copyItemPermissions(sourceFile, targetFile, ProcSymlink::FOLLOW); //throw FileError

    return result;
}

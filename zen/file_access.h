// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_ACCESS_H_8017341345614857
#define FILE_ACCESS_H_8017341345614857

//#include <functional>
#include "file_path.h"
#include "file_error.h"
#include "serialize.h" //IoCallback
    #include <sys/stat.h>

namespace zen
{
//note: certain functions require COM initialization! (vista_file_op.h)

//FAT/FAT32: "Why does the timestamp of a file *increase* by up to 2 seconds when I copy it to a USB thumb drive?"
const int FAT_FILE_TIME_PRECISION_SEC = 2; //https://devblogs.microsoft.com/oldnewthing/?p=83
//https://web.archive.org/web/20141127143832/http://support.microsoft.com/kb/127830

using FileIndex = ino_t;
using FileTimeNative = timespec;

inline time_t nativeFileTimeToTimeT(const timespec& ft) { return ft.tv_sec; } //follow Windows Explorer: always round down!
inline timespec timetToNativeFileTime(time_t utcTime) { return {.tv_sec = utcTime}; }

enum class ItemType
{
    file,
    folder,
    symlink,
};
//(hopefully) fast: does not distinguish between error/not existing
ItemType getItemType(const Zstring& itemPath); //throw FileError
//execute potentially SLOW folder traversal but distinguish error/not existing:
//  - all child item path parts must correspond to folder traversal
//  => we can conclude whether an item is *not* existing anymore by doing a *case-sensitive* name search => potentially SLOW!
std::optional<ItemType> getItemTypeIfExists(const Zstring& itemPath); //throw FileError

inline bool itemExists(const Zstring& itemPath) { return static_cast<bool>(getItemTypeIfExists(itemPath)); } //throw FileError

enum class ProcSymlink
{
    asLink,
    follow
};
void setFileTime(const Zstring& filePath, time_t modTime, ProcSymlink procSl); //throw FileError


int64_t getFreeDiskSpace(const Zstring& folderPath); //throw FileError, returns < 0 if not available
//- symlink handling: follow
//- returns < 0 if not available
//- folderPath does not need to exist (yet)

//symlink handling: follow
uint64_t getFileSize(const Zstring& filePath); //throw FileError

//get per-user directory designated for temporary files:
Zstring getTempFolderPath(); //throw FileError

void removeFilePlain     (const Zstring& filePath);         //throw FileError; ERROR if not existing
void removeSymlinkPlain  (const Zstring& linkPath);         //throw FileError; ERROR if not existing
void removeDirectoryPlain(const Zstring& dirPath );         //throw FileError; ERROR if not existing
void removeDirectoryPlainRecursion(const Zstring& dirPath); //throw FileError; ERROR if not existing

void moveAndRenameItem(const Zstring& pathFrom, const Zstring& pathTo, bool replaceExisting); //throw FileError, ErrorMoveUnsupported, ErrorTargetExisting

bool supportsPermissions(const Zstring& dirPath); //throw FileError, follows symlinks
//copy permissions for files, directories or symbolic links: requires admin rights
void copyItemPermissions(const Zstring& sourcePath, const Zstring& targetPath, ProcSymlink procSl); //throw FileError

void createDirectory(const Zstring& dirPath); //throw FileError, ErrorTargetExisting

//creates directories recursively if not existing
void createDirectoryIfMissingRecursion(const Zstring& dirPath); //throw FileError

//symlink handling: follow
//expects existing source/target directories
void copyDirectoryAttributes(const Zstring& sourcePath, const Zstring& targetPath); //throw FileError

void copySymlink(const Zstring& sourcePath, const Zstring& targetPath); //throw FileError

struct FileCopyResult
{
    uint64_t fileSize = 0;
    FileTimeNative sourceModTime = {};
    FileIndex sourceFileIdx = 0;
    FileIndex targetFileIdx = 0;
    std::optional<FileError> errorModTime; //failure to set modification time
};

FileCopyResult copyNewFile(const Zstring& sourceFile, const Zstring& targetFile, //throw FileError, ErrorTargetExisting, ErrorFileLocked, X
                           //accummulated delta != file size! consider ADS, sparse, compressed files
                           const IoCallback& notifyUnbufferedIO /*throw X*/);
}

#endif //FILE_ACCESS_H_8017341345614857

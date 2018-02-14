// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_ACCESS_H_8017341345614857
#define FILE_ACCESS_H_8017341345614857

#include <functional>
#include "zstring.h"
#include "file_error.h"
#include "file_id_def.h"
#include "serialize.h"


namespace zen
{
//note: certain functions require COM initialization! (vista_file_op.h)

struct PathComponents
{
    Zstring rootPath; //itemPath = rootPath + (FILE_NAME_SEPARATOR?) + relPath
    Zstring relPath;  //
};
Opt<PathComponents> parsePathComponents(const Zstring& itemPath); //no value on failure

Opt<Zstring> getParentFolderPath(const Zstring& itemPath);

//POSITIVE existence checks; if false: 1. item not existing 2. different type 3.device access error or similar
bool fileAvailable(const Zstring& filePath); //noexcept
bool dirAvailable (const Zstring& dirPath ); //
//NEGATIVE existence checks; if false: 1. item existing 2.device access error or similar
bool itemNotExisting(const Zstring& itemPath);

enum class ItemType
{
    FILE,
    FOLDER,
    SYMLINK,
};
//(hopefully) fast: does not distinguish between error/not existing
ItemType      getItemType        (const Zstring& itemPath); //throw FileError
//execute potentially SLOW folder traversal but distinguish error/not existing
Opt<ItemType> getItemTypeIfExists(const Zstring& itemPath); //throw FileError

struct PathStatus
{
    ItemType existingType;
    Zstring existingPath;         //itemPath =: existingPath + relPath
    std::vector<Zstring> relPath; //
};
PathStatus getPathStatus(const Zstring& itemPath); //throw FileError

enum class ProcSymlink
{
    DIRECT,
    FOLLOW
};
void setFileTime(const Zstring& filePath, time_t modTime, ProcSymlink procSl); //throw FileError

//symlink handling: always evaluate target
uint64_t getFileSize(const Zstring& filePath); //throw FileError
uint64_t getFreeDiskSpace(const Zstring& path); //throw FileError, returns 0 if not available
VolumeId getVolumeId(const Zstring& itemPath); //throw FileError
//get per-user directory designated for temporary files:
Zstring getTempFolderPath(); //throw FileError

void removeFilePlain     (const Zstring& filePath);         //throw FileError; ERROR if not existing
void removeSymlinkPlain  (const Zstring& linkPath);         //throw FileError; ERROR if not existing
void removeDirectoryPlain(const Zstring& dirPath );         //throw FileError; ERROR if not existing
void removeDirectoryPlainRecursion(const Zstring& dirPath); //throw FileError; ERROR if not existing

//rename file or directory: no copying!!!
void renameFile(const Zstring& itemPathOld, const Zstring& itemPathNew); //throw FileError, ErrorDifferentVolume, ErrorTargetExisting

bool supportsPermissions(const Zstring& dirPath); //throw FileError, follows symlinks
//copy permissions for files, directories or symbolic links: requires admin rights
void copyItemPermissions(const Zstring& sourcePath, const Zstring& targetPath, ProcSymlink procSl); //throw FileError

void createDirectory(const Zstring& dirPath); //throw FileError, ErrorTargetExisting

//- no error if already existing
//- create recursively if parent directory is not existing
void createDirectoryIfMissingRecursion(const Zstring& dirPath); //throw FileError

//symlink handling: follow link!
//expects existing source/target directories
//reports note-worthy errors only
void tryCopyDirectoryAttributes(const Zstring& sourcePath, const Zstring& targetPath); //throw FileError

void copySymlink(const Zstring& sourceLink, const Zstring& targetLink, bool copyFilePermissions); //throw FileError

struct FileCopyResult
{
    uint64_t fileSize = 0;
    time_t modTime = 0; //number of seconds since Jan. 1st 1970 UTC
    FileId sourceFileId;
    FileId targetFileId;
    Opt<FileError> errorModTime; //failure to set modification time
};

FileCopyResult copyNewFile(const Zstring& sourceFile, const Zstring& targetFile, bool copyFilePermissions, //throw FileError, ErrorTargetExisting, ErrorFileLocked
                           //accummulated delta != file size! consider ADS, sparse, compressed files
                           const IOCallback& notifyUnbufferedIO); //may be nullptr; throw X!
}

#endif //FILE_ACCESS_H_8017341345614857

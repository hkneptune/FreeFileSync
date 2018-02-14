// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "file_traverser.h"
#include "file_error.h"


    #include <cstddef> //offsetof
    #include <unistd.h> //::pathconf()
    #include <sys/stat.h>
    #include <dirent.h>

using namespace zen;


void zen::traverseFolder(const Zstring& dirPath,
                         const std::function<void (const FileInfo&    fi)>& onFile,
                         const std::function<void (const FolderInfo&  fi)>& onFolder,
                         const std::function<void (const SymlinkInfo& si)>& onSymlink,
                         const std::function<void (const std::wstring& errorMsg)>& onError)
{
    try
    {
        /* quote: "Since POSIX.1 does not specify the size of the d_name field, and other nonstandard fields may precede
                   that field within the dirent structure, portable applications that use readdir_r() should allocate
                   the buffer whose address is passed in entry as follows:
                       len = offsetof(struct dirent, d_name) + pathconf(dirPath, _PC_NAME_MAX) + 1
                       entryp = malloc(len); */
        const size_t nameMax = std::max<long>(::pathconf(dirPath.c_str(), _PC_NAME_MAX), 10000); //::pathconf may return long(-1)
        std::vector<char> buffer(offsetof(struct ::dirent, d_name) + nameMax + 1);

        DIR* folder = ::opendir(dirPath.c_str()); //directory must NOT end with path separator, except "/"
        if (!folder)
            THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot open directory %x."), L"%x", fmtPath(dirPath)), L"opendir");
        ZEN_ON_SCOPE_EXIT(::closedir(folder)); //never close nullptr handles! -> crash

        for (;;)
        {
            struct ::dirent* dirEntry = nullptr;
            if (::readdir_r(folder, reinterpret_cast< ::dirent*>(&buffer[0]), &dirEntry) != 0)
                THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(dirPath)), L"readdir_r");
            //don't retry but restart dir traversal on error! https://blogs.msdn.microsoft.com/oldnewthing/20140612-00/?p=753/

            if (!dirEntry) //no more items
                return;

            //don't return "." and ".."
            const char* itemNameRaw = dirEntry->d_name;

            if (itemNameRaw[0] == '.' &&
                (itemNameRaw[1] == 0 || (itemNameRaw[1] == '.' && itemNameRaw[2] == 0)))
                continue;

            const Zstring& itemName = itemNameRaw;
            if (itemName.empty()) //checks result of normalizeUtfForPosix, too!
                throw FileError(replaceCpy(_("Cannot read directory %x."), L"%x", fmtPath(dirPath)), L"readdir_r: Data corruption; item with empty name.");

            const Zstring& itemPath = appendSeparator(dirPath) + itemName;

            struct ::stat statData = {};
            try
            {
                if (::lstat(itemPath.c_str(), &statData) != 0) //lstat() does not resolve symlinks
                    THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot read file attributes of %x."), L"%x", fmtPath(itemPath)), L"lstat");
            }
            catch (const FileError& e)
            {
                if (onError)
                    onError(e.toString());
                continue; //ignore error: skip file
            }

            if (S_ISLNK(statData.st_mode)) //on Linux there is no distinction between file and directory symlinks!
            {
                if (onSymlink)
                    onSymlink({ itemName, itemPath, statData.st_mtime});
            }
            else if (S_ISDIR(statData.st_mode)) //a directory
            {
                if (onFolder)
                    onFolder({ itemName, itemPath });
            }
            else //a file or named pipe, ect.
            {
                if (onFile)
                    onFile({ itemName, itemPath, makeUnsigned(statData.st_size), statData.st_mtime });
            }
            /*
            It may be a good idea to not check "S_ISREG(statData.st_mode)" explicitly and to not issue an error message on other types to support these scenarios:
            - RTS setup watch (essentially wants to read directories only)
            - removeDirectory (wants to delete everything; pipes can be deleted just like files via "unlink")

            However an "open" on a pipe will block (https://sourceforge.net/p/freefilesync/bugs/221/), so the copy routines need to be smarter!!
            */
        }
    }
    catch (const FileError& e)
    {
        if (onError)
            onError(e.toString());
    }
}

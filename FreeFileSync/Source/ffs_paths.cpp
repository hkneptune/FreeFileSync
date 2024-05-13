// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "ffs_paths.h"
#include <zen/file_access.h>
//#include <zen/thread.h>
#include <zen/sys_info.h>

    #include <iostream> //std::cerr

using namespace zen;


namespace
{
Zstring getProcessParentFolderPath()
{
    //buffer getSymlinkResolvedPath()!
    //note: compiler generates magic-statics code => fine, we don't expect accesses during shutdown => don't need FunStatGlobal<>
    static const Zstring exeFolderParentPath = []
    {
        try
        {
            const Zstring& processPath = getProcessPath(); //throw FileError
            /*  no need for getSymlinkResolvedPath():
                => support file systems with buggy GetFinalPathNameByHandle() implementation, e.g. Dokany-based: https://freefilesync.org/forum/viewtopic.php?t=8828
                => we're already supporting calling FFS via symlink for launcher executable, which guarantees: */
            assert(getItemType(                     processPath)  != ItemType::symlink); //throw FileError
            assert(getItemType(*getParentFolderPath(processPath)) != ItemType::symlink); //throw FileError

            return *getParentFolderPath(*getParentFolderPath(processPath)); //no parent folder!!? => let it crash!
        }
        catch (const FileError& e)
        {
            throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Failed to get process parent folder. " + utfTo<std::string>(e.toString()));
        }
    }();
    return exeFolderParentPath;
}
}


Zstring fff::getInstallDirPath()
{
    return getProcessParentFolderPath();

}




Zstring fff::getResourceDirPath()
{
    return appendPath(getProcessParentFolderPath(), Zstr("Resources"));
}


Zstring fff::getConfigDirPath()
{
    //note: compiler generates magic-statics code => fine, we don't expect accesses during shutdown
    static const Zstring ffsConfigPath = []
    {
        /*  Windows:            %AppData%\FreeFileSync
            macOS:              ~/Library/Application Support/FreeFileSync
            Linux (XDG layout): ~/.config/FreeFileSync                        */
        const Zstring& configPath = []
        {
            try
            {
                return appendPath(getUserDataPath(), Zstr("FreeFileSync")); //throw FileError
            }
            catch (const FileError& e)
            {
                throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Failed to get config path. " + utfTo<std::string>(e.toString()));
            }
        }();

        try
        {
            createDirectoryIfMissingRecursion(configPath); //throw FileError
        }
        catch (const FileError& e) { logExtraError(e.toString()); }

        return configPath;
    }();
    return ffsConfigPath;
}


//this function is called by RealTimeSync!!!
Zstring fff::getFreeFileSyncLauncherPath() //throw FileError
{
    return appendPath(getInstallDirPath(), Zstr("FreeFileSync"));

}

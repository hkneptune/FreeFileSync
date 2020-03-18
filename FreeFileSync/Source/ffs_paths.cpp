// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "ffs_paths.h"
#include <zen/file_access.h>
#include <zen/thread.h>
#include <zen/symlink_target.h>
#include <wx/stdpaths.h>
#include <wx/app.h>


using namespace zen;


namespace
{
Zstring getProcessParentFolderPath()
{
    //buffer getSymlinkResolvedPath()!
    //note: compiler generates magic-statics code => fine, we don't expect accesses during shutdown => don't need FunStatGlobal<>
    static const Zstring exeFolderParentPath = []
    {
        Zstring exeFolderPath = beforeLast(utfTo<Zstring>(wxStandardPaths::Get().GetExecutablePath()), FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
        try
        {
            //get rid of relative path fragments, e.g.: C:\Data\Projects\FreeFileSync\Source\..\Build\Bin
            exeFolderPath = getSymlinkResolvedPath(exeFolderPath); //throw FileError
        }
        catch (FileError&) { assert(false); }

        return beforeLast(exeFolderPath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
    }();
    return exeFolderParentPath;
}
}




namespace
{
//don't make this a function-scope static (avoid code-gen for "magic static")
//getFfsVolumeId() might be called during static destruction, e.g. async update check
std::once_flag onceFlagGetFfsVolumeId;
}

VolumeId fff::getFfsVolumeId() //throw FileError
{
    static VolumeId volumeId; //POD => no "magic static" code gen
    std::call_once(onceFlagGetFfsVolumeId, [] { volumeId = getVolumeId(getProcessParentFolderPath()); }); //throw FileError
    return volumeId;
}


bool fff::isPortableVersion()
{
    return false; //users want local installation type: https://freefilesync.org/forum/viewtopic.php?t=5750

}


Zstring fff::getResourceDirPf()
{
    return getProcessParentFolderPath() + FILE_NAME_SEPARATOR + Zstr("Resources") + FILE_NAME_SEPARATOR;
}


Zstring fff::getConfigDirPathPf()
{
    //note: compiler generates magic-statics code => fine, we don't expect accesses during shutdown
    static const Zstring cfgFolderPathPf = []
    {
        //make independent from wxWidgets global variable "appname"; support being called by RealTimeSync
        auto appName = wxTheApp->GetAppName();
        wxTheApp->SetAppName(L"FreeFileSync");
        ZEN_ON_SCOPE_EXIT(wxTheApp->SetAppName(appName));

        //OS standard path (XDG layout): ~/.config/FreeFileSync
        //wxBug: wxStandardPaths::GetUserDataDir() does not honor FileLayout_XDG flag
        wxStandardPaths::Get().SetFileLayout(wxStandardPaths::FileLayout_XDG);
        const Zstring cfgFolderPath = appendSeparator(utfTo<Zstring>(wxStandardPaths::Get().GetUserConfigDir())) + "FreeFileSync";

        try //create the config folder if not existing + create "Logs" subfolder while we're at it
        {
            createDirectoryIfMissingRecursion(appendSeparator(cfgFolderPath) + Zstr("Logs")); //throw FileError
        }
        catch (FileError&) { assert(false); }

        return appendSeparator(cfgFolderPath);
    }();
    return cfgFolderPathPf;
}


//this function is called by RealTimeSync!!!
Zstring fff::getFreeFileSyncLauncherPath()
{
    return getProcessParentFolderPath() + Zstr("/FreeFileSync");

}

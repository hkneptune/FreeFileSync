// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "ffs_paths.h"
#include <zen/file_access.h>
#include <zen/thread.h>
#include <wx/stdpaths.h>
#include <wx/app.h>


using namespace zen;


namespace
{
inline
Zstring getExeFolderPath() //directory containing executable WITH path separator at end
{
    return beforeLast(utfTo<Zstring>(wxStandardPaths::Get().GetExecutablePath()), FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
}


inline
Zstring getExeFolderParentPath()
{
    return beforeLast(getExeFolderPath(), FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
}
}




VolumeId fff::getVolumeSerialOs() //throw FileError
{
    return getVolumeId("/"); //throw FileError
}


VolumeId fff::getVolumeSerialFfs() //throw FileError
{
    return getVolumeId(getExeFolderPath()); //throw FileError
}


bool fff::isPortableVersion()
{
    return false; //users want local installation type: https://freefilesync.org/forum/viewtopic.php?t=5750

}


Zstring fff::getResourceDirPf()
{
    //make independent from wxWidgets global variable "appname"; support being called by RealTimeSync
    auto appName = wxTheApp->GetAppName();
    wxTheApp->SetAppName(L"FreeFileSync");
    ZEN_ON_SCOPE_EXIT(wxTheApp->SetAppName(appName));

    //if (isPortableVersion())
    return appendSeparator(getExeFolderParentPath());
    //else //use OS' standard paths
    //    return appendSeparator(utfTo<Zstring>(wxStandardPathsBase::Get().GetResourcesDir()));
}


namespace
{
std::once_flag onceFlagCreateCfgPath;
}


Zstring fff::getConfigDirPathPf()
{
    //make independent from wxWidgets global variable "appname"; support being called by RealTimeSync
    auto appName = wxTheApp->GetAppName();
    wxTheApp->SetAppName(L"FreeFileSync");
    ZEN_ON_SCOPE_EXIT(wxTheApp->SetAppName(appName));

    Zstring cfgFolderPath;
    if (isPortableVersion())
        cfgFolderPath = getExeFolderParentPath();
    else //OS standard path (XDG layout): ~/.config/FreeFileSync
    {
        //wxBug: wxStandardPaths::GetUserDataDir() does not honor FileLayout_XDG flag
        wxStandardPaths::Get().SetFileLayout(wxStandardPaths::FileLayout_XDG);
        cfgFolderPath = appendSeparator(utfTo<Zstring>(wxStandardPaths::Get().GetUserConfigDir())) + "FreeFileSync";
    }

    std::call_once(onceFlagCreateCfgPath, [&]
    {
        try //create the config folder if not existing + create "Logs" subfolder while we're at it
        {
            createDirectoryIfMissingRecursion(appendSeparator(cfgFolderPath) + Zstr("Logs")); //throw FileError
        }
        catch (FileError&) { assert(false); }
    });
    return appendSeparator(cfgFolderPath);
}


//this function is called by RealTimeSync!!!
Zstring fff::getFreeFileSyncLauncherPath()
{
    return getExeFolderParentPath() + Zstr("/FreeFileSync");

}

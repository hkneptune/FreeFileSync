// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FFS_PATHS_H_842759083425342534253
#define FFS_PATHS_H_842759083425342534253

#include <zen/zstring.h>
#include <zen/file_id_def.h>


namespace fff
{
//------------------------------------------------------------------------------
//global program directories
//------------------------------------------------------------------------------
Zstring getResourceDirPf  (); //resource directory WITH trailing path separator
Zstring getConfigDirPathPf(); //  config directory WITH trailing path separator
//------------------------------------------------------------------------------

bool isPortableVersion();


zen::VolumeId getFfsVolumeId(); //throw FileError

Zstring getFreeFileSyncLauncherPath(); //full path to application launcher C:\...\FreeFileSync.exe
}

#endif //FFS_PATHS_H_842759083425342534253

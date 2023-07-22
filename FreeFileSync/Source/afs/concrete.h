// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FS_CONCRETE_348787329573243
#define FS_CONCRETE_348787329573243

#include "abstract.h"

namespace fff
{
struct AfsConfig
{
    Zstring resourceDirPath; //directory to read AFS-specific files
    Zstring configDirPath;   //directory to store AFS-specific files
};
void initAfs(const AfsConfig& cfg);
void teardownAfs();

AbstractPath getNullPath();
AbstractPath createAbstractPath(const Zstring& itemPathPhrase); //noexcept
}

#endif //FS_CONCRETE_348787329573243

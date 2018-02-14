// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ICON_LOADER_H_1348701985713445
#define ICON_LOADER_H_1348701985713445

#include <zen/zstring.h>
#include "icon_holder.h"


namespace fff
{
//=> all functions are safe to call from multiple threads!
//!!!Note: init COM + system image list before loading icons!!!

//return null icon on failure:
ImageHolder getIconByTemplatePath(const Zstring& templatePath, int pixelSize);
ImageHolder genericFileIcon(int pixelSize);
ImageHolder genericDirIcon(int pixelSize);
ImageHolder getFileIcon(const Zstring& filePath, int pixelSize);
ImageHolder getThumbnailImage(const Zstring& filePath, int pixelSize);
}

#endif //ICON_LOADER_H_1348701985713445

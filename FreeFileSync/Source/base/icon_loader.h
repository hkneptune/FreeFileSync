// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ICON_LOADER_H_1348701985713445
#define ICON_LOADER_H_1348701985713445

#include <zen/zstring.h>
#include <wx+/image_holder.h>


namespace fff
{
//=> all functions are safe to call from multiple threads!
//COM needs to be initialized before calling any of these functions! CoInitializeEx/CoUninitialize
//=> don't call from WM_PAINT handler! https://blogs.msdn.microsoft.com/yvesdolc/2009/08/06/do-you-receive-wm_paint-when-waiting-for-a-com-call-to-return/

//return null icon on failure:
zen::ImageHolder getIconByTemplatePath(const Zstring& templatePath, int pixelSize);
zen::ImageHolder genericFileIcon(int pixelSize);
zen::ImageHolder genericDirIcon (int pixelSize);
zen::ImageHolder getFileIcon      (const Zstring& filePath, int pixelSize);
zen::ImageHolder getThumbnailImage(const Zstring& filePath, int pixelSize);
}

#endif //ICON_LOADER_H_1348701985713445

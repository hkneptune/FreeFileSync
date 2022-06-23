// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ICON_LOADER_H_1348701985713445
#define ICON_LOADER_H_1348701985713445

#include <zen/zstring.h>
#include <wx+/image_holder.h>
#include <wx/image.h>


namespace fff
{
//=> all functions are safe to call from multiple threads!
//COM needs to be initialized before calling any of these functions! CoInitializeEx/CoUninitialize
//=> don't call from WM_PAINT handler! https://docs.microsoft.com/en-us/archive/blogs/yvesdolc/do-you-receive-wm_paint-when-waiting-for-a-com-call-to-return

zen::FileIconHolder getIconByTemplatePath(const Zstring& templatePath, int maxSize); //throw SysError
zen::FileIconHolder genericFileIcon(int maxSize); //throw SysError
zen::FileIconHolder genericDirIcon (int maxSize); //throw SysError
zen::FileIconHolder getTrashIcon   (int maxSize); //throw SysError
zen::FileIconHolder getFileManagerIcon(int maxSize); //throw SysError
zen::FileIconHolder getFileIcon(const Zstring& filePath, int maxSize); //throw SysError
zen::ImageHolder getThumbnailImage(const Zstring& filePath, int maxSize); //throw SysError

//invalidates image holder! call from GUI thread only!
wxImage extractWxImage(zen::ImageHolder&& ih);
wxImage extractWxImage(zen::FileIconHolder&& fih); //might fail if icon theme is missing a MIME type!
}

#endif //ICON_LOADER_H_1348701985713445

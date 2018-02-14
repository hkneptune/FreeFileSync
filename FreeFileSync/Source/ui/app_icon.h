// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef APP_ICON_H_6748179634932174683214
#define APP_ICON_H_6748179634932174683214

#include <wx/icon.h>
#include <wx+/image_resources.h>


namespace fff
{
inline
wxIcon getFfsIcon()
{
    using namespace zen;
    //wxWidgets' bitmap to icon conversion on OS X can only deal with very specific sizes => check on all platforms!
    assert(getResourceImage(L"FreeFileSync").GetWidth () == getResourceImage(L"FreeFileSync").GetHeight() &&
           getResourceImage(L"FreeFileSync").GetWidth() % 128 == 0);
    wxIcon icon;
    icon.CopyFromBitmap(getResourceImage(L"FreeFileSync")); //use big logo bitmap for better quality
    return icon;

}
}


#endif //APP_ICON_H_6748179634932174683214

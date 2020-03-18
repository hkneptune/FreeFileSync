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
    //wxWidgets' bitmap to icon conversion on macOS can only deal with very specific sizes => check on all platforms!
    assert(getResourceImage(L"FreeFileSync").GetWidth () == getResourceImage(L"FreeFileSync").GetHeight() &&
           getResourceImage(L"FreeFileSync").GetWidth() == 128);
    wxIcon icon; //Ubuntu-Linux does a bad job at down-scaling in Unity dash (blocky icons!) => prepare:
    icon.CopyFromBitmap(getResourceImage(L"FreeFileSync").ConvertToImage().Scale(fastFromDIP(64), fastFromDIP(64), wxIMAGE_QUALITY_HIGH));
    //no discernable difference bewteen wxIMAGE_QUALITY_HIGH/wxIMAGE_QUALITY_BILINEAR in this case
    return icon;

}
}

#endif //APP_ICON_H_6748179634932174683214

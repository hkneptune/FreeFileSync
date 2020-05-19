// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef IMAGE_RESOURCES_H_8740257825342532457
#define IMAGE_RESOURCES_H_8740257825342532457

#include <wx/bitmap.h>
#include <wx/animate.h>
#include <zen/zstring.h>


namespace zen
{
//pass resources .zip file at application startup
void initResourceImages(const Zstring& zipPath); //throw FileError
void cleanupResourceImages();

const wxBitmap&    getResourceImage    (const std::string& name);
const wxAnimation& getResourceAnimation(const std::string& name);
}

#endif //IMAGE_RESOURCES_H_8740257825342532457

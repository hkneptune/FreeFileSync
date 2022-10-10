// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FS_NATIVE_183247018532434563465
#define FS_NATIVE_183247018532434563465

#include "abstract.h"

namespace fff
{
bool  acceptsItemPathPhraseNative(const Zstring& itemPathPhrase); //noexcept
AbstractPath createItemPathNative(const Zstring& itemPathPhrase); //noexcept

//-------------------------------------------------------

AbstractPath createItemPathNativeNoFormatting(const Zstring& nativePath); //noexcept

//return empty, if not a native path
Zstring getNativeItemPath(const AbstractPath& itemPath);
}

#endif //FS_NATIVE_183247018532434563465

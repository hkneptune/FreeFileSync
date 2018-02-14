// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "concrete.h"
#include "native.h"

using namespace fff;


AbstractPath fff::createAbstractPath(const Zstring& itemPathPhrase) //noexcept
{
    //greedy: try native evaluation first
    if (acceptsItemPathPhraseNative(itemPathPhrase)) //noexcept
        return createItemPathNative(itemPathPhrase); //noexcept

    //then the rest:


    //no idea? => native!
    return createItemPathNative(itemPathPhrase);
}

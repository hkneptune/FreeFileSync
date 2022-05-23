// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef RESOLVE_PATH_H_817402834713454
#define RESOLVE_PATH_H_817402834713454

#include "file_error.h"


namespace zen
{
/*  - expand macros
    - trim whitespace
    - expand volume path by name
    - convert relative paths into absolute

    => may block for slow USB sticks and idle HDDs     */
Zstring getResolvedFilePath(const Zstring& pathPhrase); //noexcept

//macro substitution only
Zstring expandMacros(const Zstring& text);

std::vector<Zstring> getPathPhraseAliases(const Zstring& itemPath);
Zstring makePathPhrase(const Zstring& itemPath);

}

#endif //RESOLVE_PATH_H_817402834713454

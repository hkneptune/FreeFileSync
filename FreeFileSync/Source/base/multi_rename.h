// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef MULTI_RENAME_H_489572039485723453425
#define MULTI_RENAME_H_489572039485723453425

#include <string>
#include <zen/stl_tools.h>

namespace fff
{
struct RenameBuf;

std::pair<std::wstring /*phrase*/, zen::SharedRef<const RenameBuf>> getPlaceholderPhrase(const std::vector<std::wstring>& strings);
const std::vector<std::wstring> resolvePlaceholderPhrase(const std::wstring_view phrase, const RenameBuf& buf);

bool isRenamePlaceholderChar(wchar_t c);
}

#endif //MULTI_RENAME_H_489572039485723453425

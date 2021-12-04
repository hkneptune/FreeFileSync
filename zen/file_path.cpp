// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "file_path.h"

using namespace zen;


std::optional<PathComponents> zen::parsePathComponents(const Zstring& itemPath)
{
    auto doParse = [&](int sepCountVolumeRoot, bool rootWithSep) -> std::optional<PathComponents>
    {
        const Zstring itemPathFmt = appendSeparator(itemPath); //simplify analysis of root without separator, e.g. \\server-name\share
        int sepCount = 0;
        for (auto it = itemPathFmt.begin(); it != itemPathFmt.end(); ++it)
            if (*it == FILE_NAME_SEPARATOR)
                if (++sepCount == sepCountVolumeRoot)
                {
                    Zstring rootPath(itemPathFmt.begin(), rootWithSep ? it + 1 : it);

                    Zstring relPath(it + 1, itemPathFmt.end());
                    trim(relPath, true, true, [](Zchar c) { return c == FILE_NAME_SEPARATOR; });

                    return PathComponents({rootPath, relPath});
                }
        return {};
    };

    std::optional<PathComponents> pc; //"/media/zenju/" and "/Volumes/" should not fail to parse

    if (!pc && startsWith(itemPath, "/mnt/")) //e.g. /mnt/DEVICE_NAME
        pc = doParse(3 /*sepCountVolumeRoot*/, false /*rootWithSep*/);

    if (!pc && startsWith(itemPath, "/media/")) //Ubuntu: e.g. /media/zenju/DEVICE_NAME
        if (const char* username = ::getenv("USER"))
            if (startsWith(itemPath, std::string("/media/") + username + "/"))
                pc = doParse(4 /*sepCountVolumeRoot*/, false /*rootWithSep*/);

    if (!pc && startsWith(itemPath, "/run/media/")) //CentOS, Suse: e.g. /run/media/zenju/DEVICE_NAME
        if (const char* username = ::getenv("USER"))
            if (startsWith(itemPath, std::string("/run/media/") + username + "/"))
                pc = doParse(5 /*sepCountVolumeRoot*/, false /*rootWithSep*/);

    if (!pc && startsWith(itemPath, "/run/user/")) //Ubuntu, e.g.: /run/user/1000/gvfs/smb-share:server=192.168.62.145,share=folder
    {
        Zstring tmp(itemPath.begin() + strLength("/run/user/"), itemPath.end());
        tmp = beforeFirst(tmp, "/gvfs/", IfNotFoundReturn::none);
        if (!tmp.empty() && std::all_of(tmp.begin(), tmp.end(), [](char c) { return isDigit(c); }))
        /**/pc = doParse(6 /*sepCountVolumeRoot*/, false /*rootWithSep*/);
    }


    if (!pc && startsWith(itemPath, "/"))
        pc = doParse(1 /*sepCountVolumeRoot*/, true /*rootWithSep*/);

    return pc;
}


std::optional<Zstring> zen::getParentFolderPath(const Zstring& itemPath)
{
    if (const std::optional<PathComponents> comp = parsePathComponents(itemPath))
    {
        if (comp->relPath.empty())
            return std::nullopt;

        const Zstring parentRelPath = beforeLast(comp->relPath, FILE_NAME_SEPARATOR, IfNotFoundReturn::none);
        if (parentRelPath.empty())
            return comp->rootPath;
        return appendSeparator(comp->rootPath) + parentRelPath;
    }
    assert(false);
    return std::nullopt;
}



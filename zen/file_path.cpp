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
        const Zstring itemPathPf = appendSeparator(itemPath); //simplify analysis of root without separator, e.g. \\server-name\share
        int sepCount = 0;
        for (auto it = itemPathPf.begin(); it != itemPathPf.end(); ++it)
            if (*it == FILE_NAME_SEPARATOR)
                if (++sepCount == sepCountVolumeRoot)
                {
                    Zstring rootPath(itemPathPf.begin(), rootWithSep ? it + 1 : it);

                    Zstring relPath(it + 1, itemPathPf.end());
                    trim(relPath, true, true, [](Zchar c) { return c == FILE_NAME_SEPARATOR; });

                    return PathComponents{std::move(rootPath), std::move(relPath)};
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
    if (const std::optional<PathComponents> pc = parsePathComponents(itemPath))
    {
        if (pc->relPath.empty())
            return std::nullopt;

        return appendPath(pc->rootPath, beforeLast(pc->relPath, FILE_NAME_SEPARATOR, IfNotFoundReturn::none));
    }
    assert(false);
    return std::nullopt;
}


Zstring zen::appendSeparator(Zstring path) //support rvalue references!
{
    if (!endsWith(path, FILE_NAME_SEPARATOR))
        path += FILE_NAME_SEPARATOR;
    return path; //returning a by-value parameter => RVO if possible, r-value otherwise!
}


bool zen::isValidRelPath(const Zstring& relPath)
{
    //relPath is expected to use FILE_NAME_SEPARATOR!
    if constexpr (FILE_NAME_SEPARATOR != Zstr('/' )) if (contains(relPath, Zstr('/' ))) return false;
    if constexpr (FILE_NAME_SEPARATOR != Zstr('\\')) if (contains(relPath, Zstr('\\'))) return false;

    const Zchar doubleSep[] = {FILE_NAME_SEPARATOR, FILE_NAME_SEPARATOR, 0};
    return !startsWith(relPath, FILE_NAME_SEPARATOR)&& !endsWith(relPath, FILE_NAME_SEPARATOR)&&
           !contains(relPath, doubleSep);
}


Zstring zen::appendPath(const Zstring& basePath, const Zstring& relPath)
{
    assert(isValidRelPath(relPath));
    if (relPath.empty())
        return basePath; //with or without path separator, e.g. C:\ or C:\folder

    if (basePath.empty()) //basePath might be a relative path, too!
        return relPath;

    if (endsWith(basePath, FILE_NAME_SEPARATOR))
        return basePath + relPath;

    Zstring output = basePath;
    output.reserve(basePath.size() + 1 + relPath.size());     //append all three strings using a single memory allocation
    return std::move(output) + FILE_NAME_SEPARATOR + relPath; //
}


Zstring zen::getFileExtension(const Zstring& filePath)
{
    //const Zstring fileName = afterLast(filePath, FILE_NAME_SEPARATOR, IfNotFoundReturn::all);
    //return afterLast(fileName, Zstr('.'), IfNotFoundReturn::none);

    auto it = zen::findLast(filePath.begin(), filePath.end(), FILE_NAME_SEPARATOR);
    if (it == filePath.end())
        it = filePath.begin();
    else
        ++it;

    auto it2 = zen::findLast(it, filePath.end(), Zstr('.'));
    if (it2 != filePath.end())
        ++it2;

    return Zstring(it2, filePath.end());
}


/* https://docs.microsoft.com/de-de/windows/desktop/Intl/handling-sorting-in-your-applications

    Perf test: compare strings 10 mio times; 64 bit build
    -----------------------------------------------------
        string a = "Fjk84$%kgfj$%T\\\\Gffg\\gsdgf\\fgsx----------d-"
        string b = "fjK84$%kgfj$%T\\\\gfFg\\gsdgf\\fgSy----------dfdf"

    Windows (UTF16 wchar_t)
      4 ns | wcscmp
     67 ns | CompareStringOrdinalFunc+ + bIgnoreCase
    314 ns | LCMapString + wmemcmp

    OS X (UTF8 char)
       6 ns | strcmp
      98 ns | strcasecmp
     120 ns | strncasecmp + std::min(sizeLhs, sizeRhs);
     856 ns | CFStringCreateWithCString       + CFStringCompare(kCFCompareCaseInsensitive)
    1110 ns | CFStringCreateWithCStringNoCopy + CFStringCompare(kCFCompareCaseInsensitive)
    ________________________
    time per call | function                                                   */

std::weak_ordering zen::compareNativePath(const Zstring& lhs, const Zstring& rhs)
{
    assert(lhs.find(Zchar('\0')) == Zstring::npos); //don't expect embedded nulls!
    assert(rhs.find(Zchar('\0')) == Zstring::npos); //

    return lhs <=> rhs;

}



// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "path_filter.h"
#include <typeindex>
#include <zen/file_path.h>

using namespace zen;
using namespace fff;


std::strong_ordering fff::operator<=>(const FilterRef& lhs, const FilterRef& rhs)
{
    //caveat: typeid returns static type for pointers, dynamic type for references!!!
    if (const std::strong_ordering cmp = std::type_index(typeid(lhs.ref())) <=>
                                         std::type_index(typeid(rhs.ref()));
        cmp != std::strong_ordering::equal)
        return cmp;

    return lhs.ref().compareSameType(rhs.ref());
}


std::vector<Zstring> fff::splitByDelimiter(const Zstring& filterPhrase)
{
    std::vector<Zstring> output;

    split2(filterPhrase, [](Zchar c) { return c == FILTER_ITEM_SEPARATOR || c == Zstr('\n'); }, //delimiters
    [&output](const Zchar* blockFirst, const Zchar* blockLast)
    {
        std::tie(blockFirst, blockLast) = trimCpy(blockFirst, blockLast, true /*fromLeft*/, true /*fromRight*/, [](Zchar c) { return isWhiteSpace(c); });
        if (blockFirst != blockLast)
            output.emplace_back(blockFirst, blockLast);
    });

    return output;
}


void NameFilter::parseFilterPhrase(const Zstring& filterPhrase, FilterSet& filter)
{
    //normalize filter: 1. ignore Unicode normalization form 2. ignore case
    Zstring filterPhraseFmt = getUpperCase(filterPhrase);
    //3. fix path separator
    if constexpr (FILE_NAME_SEPARATOR != Zstr('/' )) replace(filterPhraseFmt, Zstr('/'),  FILE_NAME_SEPARATOR);
    if constexpr (FILE_NAME_SEPARATOR != Zstr('\\')) replace(filterPhraseFmt, Zstr('\\'), FILE_NAME_SEPARATOR);

    const Zstring sepAsterisk = Zstr("/*");
    const Zstring asteriskSep = Zstr("*/");
    static_assert(FILE_NAME_SEPARATOR == '/');

    auto processTail = [&](const Zstring& phrase)
    {
        if (endsWith(phrase, FILE_NAME_SEPARATOR) || //only relevant for folder filtering
            endsWith(phrase, sepAsterisk)) // abc\*
        {
            const Zstring dirPhrase = beforeLast(phrase, FILE_NAME_SEPARATOR, IfNotFoundReturn::none);
            if (!dirPhrase.empty())
                filter.folderMasks.insert(dirPhrase);
        }
        else if (!phrase.empty())
            filter.fileFolderMasks.insert(phrase);
    };

    for (const Zstring& itemPhrase : splitByDelimiter(filterPhraseFmt))
    {
        /*    phrase  | action
            +---------+--------
            | \blah   | remove \
            | \*blah  | remove \
            | \*\blah | remove \
            | \*\*    | remove \
            +---------+--------
            | *blah   |
            | *\blah  | -> add blah
            | *\*blah | -> add *blah
            +---------+--------
            | blah\   | remove \; folder only
            | blah*\  | remove \; folder only
            | blah\*\ | remove \; folder only
            +---------+--------
            | blah*   |
            | blah\*  | remove \*; folder only
            | blah*\* | remove \*; folder only
            +---------+--------                    */

        if (startsWith(itemPhrase, FILE_NAME_SEPARATOR)) // \abc
            processTail(afterFirst(itemPhrase, FILE_NAME_SEPARATOR, IfNotFoundReturn::none));
        else
        {
            processTail(itemPhrase);
            if (startsWith(itemPhrase, asteriskSep)) // *\abc
                processTail(afterFirst(itemPhrase, asteriskSep, IfNotFoundReturn::none));
        }
    }
}


void NameFilter::MaskMatcher::insert(const Zstring& mask)
{
    assert(mask == getUpperCase(mask));
    if (contains(mask, Zstr('?')) ||
        contains(mask, Zstr('*')))
        realMasks_.insert(mask);
    else
    {
        relPaths_   .insert(mask);
        relPathsCmp_.insert(mask); //little memory wasted thanks to COW string!
    }
}


namespace
{
//"true" if path or any parent path matches the mask
bool matchesMask(const Zchar* path, const Zchar* const pathEnd, const Zchar* mask /*0-terminated*/)
{
    for (;; ++mask, ++path)
    {
        Zchar m = *mask;
        switch (m)
        {
            case 0:
                return path == pathEnd || *path == FILE_NAME_SEPARATOR; //"full" or parent path match

            case Zstr('?'): //should not match FILE_NAME_SEPARATOR
                if (path == pathEnd || *path == FILE_NAME_SEPARATOR)
                    return false;
                break;

            case Zstr('*'):
                do //advance mask to next non-* char
                {
                    m = *++mask;
                }
                while (m == Zstr('*'));

                if (m == 0) //mask ends with '*':
                    return true;

                ++mask;
                if (m == Zstr('?')) //*? pattern
                {
                    while (path != pathEnd)
                        if (*path++ != FILE_NAME_SEPARATOR)
                            if (matchesMask(path, pathEnd, mask))
                                return true;
                }
                else //*[letter or /] pattern
                    while (path != pathEnd)
                        if (*path++ == m)
                            if (matchesMask(path, pathEnd, mask))
                                return true;
                return false;

            default:
                if (path == pathEnd || *path != m)
                    return false;
        }
    }
}


//"true" if path matches (only!) the beginning of mask
template <bool haveWildcards> bool matchesMaskBegin(const Zstring& relPath, const Zstring& mask);

template <> inline
bool matchesMaskBegin<true /*haveWildcards*/>(const Zstring& relPath, const Zstring& mask)
{
    auto itP = relPath.begin();
    for (auto itM = mask.begin(); itM != mask.end(); ++itM, ++itP)
    {
        const Zchar m = *itM;
        switch (m)
        {
            case Zstr('?'):
                if (itP == relPath.end() || *itP == FILE_NAME_SEPARATOR)
                    return false;
                break;

            case Zstr('*'):
                return true;

            default:
                if (itP == relPath.end())
                    return m == FILE_NAME_SEPARATOR && mask.end() - itM > 1; //require strict sub match

                if (*itP != m)
                    return false;
        }
    }
    return false; //not a strict sub match
}

template <> inline //perf: going overboard? remaining fruits are hanging higher and higher...
bool matchesMaskBegin<false /*haveWildcards*/>(const Zstring& relPath, const Zstring& mask)
{
    return mask.size() > relPath.size() + 1 && //room for FILE_NAME_SEPARATOR *and* at least one more char
           mask[relPath.size()] == FILE_NAME_SEPARATOR &&
           startsWith(mask, relPath);
}
}


bool NameFilter::MaskMatcher::matches(const Zchar* pathFirst, const Zchar* pathLast) const
{
    if (std::any_of(realMasks_.begin(), realMasks_.end(), [&](const Zstring& mask) { return matchesMask(pathFirst, pathLast, mask.c_str()); }))
    /**/return true;

    //perf: for relPaths_ we can go from linear to *constant* time!!! => annihilates https://freefilesync.org/forum/viewtopic.php?t=7768#p26519
    const Zchar* sepPos = pathFirst;
    for (;;) //check all parent paths!
    {
        sepPos = std::find(sepPos, pathLast, FILE_NAME_SEPARATOR);

        if (relPaths_.contains(makeStringView(pathFirst, sepPos))) //heterogenous lookup!
            return true;

        if (sepPos == pathLast)
            return false;

        ++sepPos;
    }
}

bool NameFilter::MaskMatcher::matchesBegin(const Zstring& relPath) const
{
    return std::any_of(realMasks_.begin(), realMasks_.end(), [&](const Zstring& mask) { return matchesMaskBegin<true  /*haveWildcards*/>(relPath, mask); }) ||
    /**/   std::any_of(relPaths_ .begin(), relPaths_ .end(), [&](const Zstring& mask) { return matchesMaskBegin<false /*haveWildcards*/>(relPath, mask); });
}

//#################################################################################################

NameFilter::NameFilter(const Zstring& includePhrase, const Zstring& excludePhrase)
{
    parseFilterPhrase(includePhrase, includeFilter);
    parseFilterPhrase(excludePhrase, excludeFilter);
}


void NameFilter::addExclusion(const Zstring& excludePhrase)
{
    parseFilterPhrase(excludePhrase, excludeFilter);
}


bool NameFilter::passFileFilter(const Zstring& relFilePath) const
{
    assert(!startsWith(relFilePath, FILE_NAME_SEPARATOR));

    //normalize input: 1. ignore Unicode normalization form 2. ignore case
    const Zstring& pathFmt = getUpperCase(relFilePath);

    const Zchar* sepPos = findLast(pathFmt.begin(), pathFmt.end(), FILE_NAME_SEPARATOR);

    if (excludeFilter.fileFolderMasks.matches(pathFmt.begin(), pathFmt.end()) || //either match on file or any parent folder
        (sepPos != pathFmt.end() && excludeFilter.folderMasks.matches(pathFmt.begin(), sepPos))) //match on any parent folder only
        return false;

    return includeFilter.fileFolderMasks.matches(pathFmt.begin(), pathFmt.end()) ||
           (sepPos != pathFmt.end() && includeFilter.folderMasks.matches(pathFmt.begin(), sepPos));
}


bool NameFilter::passDirFilter(const Zstring& relDirPath, bool* childItemMightMatch) const
{
    assert(!startsWith(relDirPath, FILE_NAME_SEPARATOR));
    assert(!childItemMightMatch || *childItemMightMatch); //check correct usage

    //normalize input: 1. ignore Unicode normalization form 2. ignore case
    const Zstring& pathFmt = getUpperCase(relDirPath);

    if (excludeFilter.fileFolderMasks.matches(pathFmt.begin(), pathFmt.end()) ||
        excludeFilter.folderMasks    .matches(pathFmt.begin(), pathFmt.end()))
    {
        if (childItemMightMatch)
            *childItemMightMatch = false; //perf: no need to traverse deeper; subfolders/subfiles would be excluded by filter anyway!

        /* Attention: If *childItemMightMatch == false, then any direct filter evaluation for a child item must also return "false"!

           This is not a problem for folder traversal which stops at the first *childItemMightMatch == false anyway, but other code continues recursing further,
           e.g. the database update code in db_file.cpp recurses unconditionally without *childItemMightMatch check! */
        return false;
    }

    if (includeFilter.fileFolderMasks.matches(pathFmt.begin(), pathFmt.end()) ||
        includeFilter.folderMasks    .matches(pathFmt.begin(), pathFmt.end()))
        return true;

    if (childItemMightMatch)
        *childItemMightMatch = includeFilter.fileFolderMasks.matchesBegin(pathFmt) || //might match a file  or folder in subdirectory
                               includeFilter.folderMasks    .matchesBegin(pathFmt);   //
    return false;
}


bool NameFilter::isNull(const Zstring& includePhrase, const Zstring& excludePhrase)
{
    return trimCpy(includePhrase) == Zstr("*") &&
           trimCpy(excludePhrase).empty();
    //return NameFilter(includePhrase, excludePhrase).isNull(); -> very expensive for huge lists
}


bool NameFilter::isNull() const
{
    return compareSameType(NameFilter(Zstr("*"), Zstr(""))) == std::strong_ordering::equal;
    //avoid static non-POD null-NameFilter instance
}


std::strong_ordering NameFilter::compareSameType(const PathFilter& other) const
{
    assert(typeid(*this) == typeid(other)); //always given in this context!

    const NameFilter& lhs = *this;
    const NameFilter& rhs = static_cast<const NameFilter&>(other);

    return std::tie(lhs.includeFilter, lhs.excludeFilter) <=>
           std::tie(rhs.includeFilter, rhs.excludeFilter);
}

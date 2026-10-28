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


void NameFilter::parseFilterPhrase(const Zstring& filterPhrase, FilterSet& filter)
{
    //normalize filter: 1. ignore Unicode normalization form 2. ignore case
    Zstring filterPhraseNorm = getUpperCase(filterPhrase);
    //3. fix path separator
    if constexpr (FILE_NAME_SEPARATOR != Zstr('/' )) replace(filterPhraseNorm, Zstr('/'),  FILE_NAME_SEPARATOR);
    if constexpr (FILE_NAME_SEPARATOR != Zstr('\\')) replace(filterPhraseNorm, Zstr('\\'), FILE_NAME_SEPARATOR);

    static_assert(FILE_NAME_SEPARATOR == '/');
    const Zstring sepAsterisk = Zstr("/*");
    const Zstring asteriskSep = Zstr("*/");

    auto processTail = [&](const ZstringView phrase)
    {
        if (endsWith(phrase, Zstr(':'))) //file-only tag
            filter.fileMasks.insert({phrase.begin(), phrase.end() - 1});
        else if (endsWith(phrase, FILE_NAME_SEPARATOR) || //folder-only tag
                 endsWith(phrase, sepAsterisk)) // abc\*
            filter.folderMasks.insert(Zstring(beforeLast(phrase, FILE_NAME_SEPARATOR, IfNotFoundReturn::none)));
        else
        {
            Zstring phrase2(phrase); //ref-count! :>
            filter.fileMasks  .insert(phrase2);
            filter.folderMasks.insert(phrase2);
        }
    };

    split2(filterPhraseNorm, [](Zchar c) { return c == FILTER_ITEM_SEPARATOR || c == Zstr('\n'); }, //delimiters
    [&](ZstringView itemPhrase)
    {
        itemPhrase = trimCpy(itemPhrase);
        if (!itemPhrase.empty())
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
                | blah:   | remove : (file only)
                | blah\*: | remove : (file only)
                +---------+--------
                | blah\   | remove \ (folder only)
                | blah*\  | remove \ (folder only)
                | blah\*\ | remove \ (folder only)
                +---------+--------
                | blah*   |
                | blah\*  | remove \* (folder only)
                | blah*\* | remove \* (folder only)
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
    });
}


void NameFilter::MaskMatcher::insert(const Zstring& mask)
{
    assert(mask == getUpperCase(mask));
    if (mask.empty())
        return;

    if (contains(mask, Zstr('?')) ||
        contains(mask, Zstr('*')))
        realMasks_.insert(mask);
    else
    {
        relPaths_   .insert(mask);
        relPathsCmp_.insert(mask); //no memory wasted thanks to ref-counting string!
    }
}


namespace
{
template <bool allowParentMatch>
bool matchesMask(const Zchar* path, const Zchar* const pathEnd, const Zchar* mask /*0-terminated*/)
{
    for (;; ++mask, ++path)
    {
        Zchar m = *mask;
        switch (m)
        {
            case 0:
                if constexpr (allowParentMatch)
                    return path == pathEnd || *path == FILE_NAME_SEPARATOR; //"full" or parent path match
                else
                    return path == pathEnd;

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
                            if (matchesMask<allowParentMatch>(path, pathEnd, mask))
                                return true;
                }
                else //*[letter or /] pattern
                    while (path != pathEnd)
                        if (*path++ == m)
                            if (matchesMask<allowParentMatch>(path, pathEnd, mask))
                                return true;
                return false;

            default:
                if (path == pathEnd || *path != m)
                    return false;
        }
    }
}


//"true" if path matches (only!) the beginning of mask
template <bool haveWildcards> inline
bool matchesMaskBegin(const ZstringView relPath, const Zstring& mask)
{
    if constexpr (haveWildcards)
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
    else //perf: going overboard? remaining fruits are hanging higher and higher...
        return mask.size() > relPath.size() + 1 && //room for FILE_NAME_SEPARATOR *and* at least one more char
               mask[relPath.size()] == FILE_NAME_SEPARATOR &&
               std::equal(relPath.begin(), relPath.end(), mask.begin());
}
}


bool NameFilter::MaskMatcher::matchesBegin(const ZstringView relPath) const
{
    return std::any_of(realMasks_.begin(), realMasks_.end(), [&](const Zstring& mask) { return matchesMaskBegin<true  /*haveWildcards*/>(relPath, mask); }) ||
    /**/   std::any_of(relPaths_ .begin(), relPaths_ .end(), [&](const Zstring& mask) { return matchesMaskBegin<false /*haveWildcards*/>(relPath, mask); });
}


template <bool allowParentMatch>
bool NameFilter::MaskMatcher::matches(const ZstringView relPath) const
{
    assert(!relPath.empty());

    if (std::any_of(realMasks_.begin(), realMasks_.end(),
    /**/[&](const Zstring& mask) { return matchesMask<allowParentMatch>(relPath.data(), relPath.data() + relPath.size(), mask.c_str()); }))
    /**/return true;

    //perf: for relPaths_ we can go from linear to *constant* time!!! => annihilates https://freefilesync.org/forum/viewtopic.php?t=7768#p26519

    if constexpr (allowParentMatch)
    {
        ZstringView parentPath = relPath;
        for (;;)
        {
            if (relPaths_.contains(parentPath)) //heterogenous lookup!
                return true;

            parentPath = beforeLast(parentPath, FILE_NAME_SEPARATOR, IfNotFoundReturn::none);
            if (parentPath.empty())
                return false;
        }
    }
    else
        return relPaths_.contains(relPath);
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


bool NameFilter::passFileFilter(const ZstringNoCase& relFilePath) const
{
    assert(!startsWith(relFilePath.upperCase, FILE_NAME_SEPARATOR));
    //require normalized input: 1. ignore Unicode normalization form 2. ignore case

    const ZstringView parentPath = beforeLast<ZstringView>(relFilePath.upperCase, FILE_NAME_SEPARATOR, IfNotFoundReturn::none);

    if (excludeFilter.fileMasks.matches<false /*allowParentMatch*/>(relFilePath.upperCase) || //either match on file or any parent folder
        (!parentPath.empty() && excludeFilter.folderMasks.matches<true /*allowParentMatch*/>(parentPath))) //match on any parent folder only
        return false;

    return includeFilter.fileMasks.matches<false /*allowParentMatch*/>(relFilePath.upperCase) ||
           (!parentPath.empty() && includeFilter.folderMasks.matches<true /*allowParentMatch*/>(parentPath));
}


bool NameFilter::passDirFilter(const ZstringNoCase& relDirPath, bool* childItemMightMatch) const
{
    assert(!startsWith(relDirPath.upperCase, FILE_NAME_SEPARATOR));
    assert(!childItemMightMatch || *childItemMightMatch); //check correct usage
    //require normalized input: 1. ignore Unicode normalization form 2. ignore case

    if (excludeFilter.folderMasks.matches<true /*allowParentMatch*/>(relDirPath.upperCase))
    {
        if (childItemMightMatch)
            *childItemMightMatch = false; //perf: no need to traverse deeper; subfolders/subfiles would be excluded by filter anyway!

        /* Attention: If *childItemMightMatch == false, then any direct filter evaluation for a child item must also return "false"!

           This is not a problem for folder traversal which stops at the first *childItemMightMatch == false anyway, but other code continues recursing further,
           e.g. the database update code in db_file.cpp recurses unconditionally without *childItemMightMatch check! */
        return false;
    }

    if (includeFilter.folderMasks.matches<true /*allowParentMatch*/>(relDirPath.upperCase))
        return true;

    if (childItemMightMatch)
        *childItemMightMatch = includeFilter.fileMasks  .matchesBegin(relDirPath.upperCase) || //might match a file  or folder in subdirectory
                               includeFilter.folderMasks.matchesBegin(relDirPath.upperCase);   //
    return false;
}


bool NameFilter::isNull(const Zstring& includePhrase, const Zstring& excludePhrase)
{
    return trimCpy<ZstringView>(includePhrase) == Zstr("*") && //harmonize with ui/folder_pair.cpp tooltip
           trimCpy<ZstringView>(excludePhrase).empty();
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

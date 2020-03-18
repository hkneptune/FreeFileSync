// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "path_filter.h"
#include <set>
#include <stdexcept>
#include <vector>
#include <typeinfo>
#include <iterator>

using namespace zen;
using namespace fff;


bool fff::operator<(const PathFilter& lhs, const PathFilter& rhs)
{
    if (typeid(lhs) != typeid(rhs))
        return typeid(lhs).before(typeid(rhs)); //in worst case, order is guaranteed to be stable only during each program run

    //lhs, rhs have same type:
    return lhs.cmpLessSameType(rhs);
}


namespace
{
//constructing Zstrings of these in addFilterEntry becomes perf issue for large filter lists => use global POD!
const Zchar sepAsterisk[] = Zstr("/*");
const Zchar asteriskSep[] = Zstr("*/");
static_assert(FILE_NAME_SEPARATOR == '/');


void addFilterEntry(const Zstring& filterPhrase, std::vector<Zstring>& masksFileFolder, std::vector<Zstring>& masksFolder)
{
    //normalize filter input: 1. ignore Unicode normalization form 2. ignore case 3. ignore path separator
    Zstring filterFmt = makeUpperCopy(filterPhrase);
    if constexpr (FILE_NAME_SEPARATOR != Zstr('/' )) replace(filterFmt, Zstr('/'),  FILE_NAME_SEPARATOR);
    if constexpr (FILE_NAME_SEPARATOR != Zstr('\\')) replace(filterFmt, Zstr('\\'), FILE_NAME_SEPARATOR);
    /*        phrase  | action
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
    auto processTail = [&masksFileFolder, &masksFolder](const Zstring& phrase)
    {
        if (endsWith(phrase, FILE_NAME_SEPARATOR) || //only relevant for folder filtering
            endsWith(phrase, sepAsterisk)) // abc\*
        {
            const Zstring dirPhrase = beforeLast(phrase, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
            if (!dirPhrase.empty())
                masksFolder.push_back(dirPhrase);
        }
        else if (!phrase.empty())
            masksFileFolder.push_back(phrase);
    };

    if (startsWith(filterFmt, FILE_NAME_SEPARATOR)) // \abc
        processTail(afterFirst(filterFmt, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE));
    else
    {
        processTail(filterFmt);
        if (startsWith(filterFmt, asteriskSep)) // *\abc
            processTail(afterFirst(filterFmt, asteriskSep, IF_MISSING_RETURN_NONE));
    }
}


template <class Char> inline
const Char* cStringFind(const Char* str, Char ch) //= strchr(), wcschr()
{
    for (;;)
    {
        const Char s = *str;
        if (s == ch) //ch is allowed to be 0 by contract! must return end of string in this case
            return str;

        if (s == 0)
            return nullptr;
        ++str;
    }
}


/*
struct FullMatch
{
    static bool matchesMaskEnd (const Zchar* path) { return *path == 0; }
    static bool matchesMaskStar(const Zchar* path) { return true; }
};
*/

struct ParentFolderMatch //strict match of parent folder path!
{
    static bool matchesMaskEnd (const Zchar* path) { return *path == FILE_NAME_SEPARATOR; }
    static bool matchesMaskStar(const Zchar* path) { return cStringFind(path, FILE_NAME_SEPARATOR) != nullptr; }
};

struct AnyMatch
{
    static bool matchesMaskEnd (const Zchar* path) { return *path == 0 || *path == FILE_NAME_SEPARATOR; }
    static bool matchesMaskStar(const Zchar* path) { return true; }
};


template <class PathEndMatcher>
bool matchesMask(const Zchar* path, const Zchar* mask)
{
    for (;; ++mask, ++path)
    {
        Zchar m = *mask;
        switch (m)
        {
            case 0:
                return PathEndMatcher::matchesMaskEnd(path);

            case Zstr('?'):
                if (*path == 0)
                    return false;
                break;

            case Zstr('*'):
                do //advance mask to next non-* char
                {
                    m = *++mask;
                }
                while (m == Zstr('*'));

                if (m == 0) //mask ends with '*':
                    return PathEndMatcher::matchesMaskStar(path);

                //*? - pattern
                if (m == Zstr('?'))
                {
                    ++mask;
                    while (*path++ != 0)
                        if (matchesMask<PathEndMatcher>(path, mask))
                            return true;
                    return false;
                }

                //*[letter] - pattern
                ++mask;
                for (;;)
                {
                    path = cStringFind(path, m);
                    if (!path)
                        return false;

                    ++path;
                    if (matchesMask<PathEndMatcher>(path, mask))
                        return true;
                }

            default:
                if (*path != m)
                    return false;
        }
    }
}


//returns true if string matches at least the beginning of mask
inline
bool matchesMaskBegin(const Zchar* str, const Zchar* mask)
{
    for (;; ++mask, ++str)
    {
        const Zchar m = *mask;
        switch (m)
        {
            case 0:
                return *str == 0;

            case Zstr('?'):
                if (*str == 0)
                    return true;
                break;

            case Zstr('*'):
                return true;

            default:
                if (*str != m)
                    return *str == 0;
        }
    }
}


template <class PathEndMatcher> inline
bool matchesMask(const Zstring& name, const std::vector<Zstring>& masks)
{
    return std::any_of(masks.begin(), masks.end(), [&](const Zstring& mask) { return matchesMask<PathEndMatcher>(name.c_str(), mask.c_str()); });
}


inline
bool matchesMaskBegin(const Zstring& name, const std::vector<Zstring>& masks)
{
    return std::any_of(masks.begin(), masks.end(), [&](const Zstring& mask) { return matchesMaskBegin(name.c_str(), mask.c_str()); });
}
}


std::vector<Zstring> fff::splitByDelimiter(const Zstring& filterPhrase)
{
    //delimiters may be FILTER_ITEM_SEPARATOR or '\n'
    std::vector<Zstring> output;

    for (const Zstring& str : split(filterPhrase, FILTER_ITEM_SEPARATOR, SplitType::SKIP_EMPTY)) //split by less common delimiter first (create few, large strings)
        for (Zstring entry : split(str, Zstr('\n'), SplitType::SKIP_EMPTY))
        {
            trim(entry);
            if (!entry.empty())
                output.push_back(std::move(entry));
        }

    return output;
}

//#################################################################################################

NameFilter::NameFilter(const Zstring& includePhrase, const Zstring& excludePhrase)
{
    //setup include/exclude filters for files and directories
    for (const Zstring& entry : splitByDelimiter(includePhrase)) addFilterEntry(entry, includeMasksFileFolder, includeMasksFolder);
    for (const Zstring& entry : splitByDelimiter(excludePhrase)) addFilterEntry(entry, excludeMasksFileFolder, excludeMasksFolder);

    removeDuplicates(includeMasksFileFolder);
    removeDuplicates(includeMasksFolder);
    removeDuplicates(excludeMasksFileFolder);
    removeDuplicates(excludeMasksFolder);
}


void NameFilter::addExclusion(const Zstring& excludePhrase)
{
    for (const Zstring& entry : splitByDelimiter(excludePhrase)) addFilterEntry(entry, excludeMasksFileFolder, excludeMasksFolder);

    removeDuplicates(excludeMasksFileFolder);
    removeDuplicates(excludeMasksFolder);
}


bool NameFilter::passFileFilter(const Zstring& relFilePath) const
{
    assert(!startsWith(relFilePath, FILE_NAME_SEPARATOR));

    //normalize input: 1. ignore Unicode normalization form 2. ignore case
    const Zstring& pathFmt = makeUpperCopy(relFilePath);

    if (matchesMask<AnyMatch         >(pathFmt, excludeMasksFileFolder) || //either full match on file or partial match on any parent folder
        matchesMask<ParentFolderMatch>(pathFmt, excludeMasksFolder))       //partial match on any parent folder only
        return false;

    return matchesMask<AnyMatch         >(pathFmt, includeMasksFileFolder) ||
           matchesMask<ParentFolderMatch>(pathFmt, includeMasksFolder);
}


bool NameFilter::passDirFilter(const Zstring& relDirPath, bool* childItemMightMatch) const
{
    assert(!startsWith(relDirPath, FILE_NAME_SEPARATOR));
    assert(!childItemMightMatch || *childItemMightMatch); //check correct usage

    //normalize input: 1. ignore Unicode normalization form 2. ignore case
    const Zstring& pathFmt = makeUpperCopy(relDirPath);

    if (matchesMask<AnyMatch>(pathFmt, excludeMasksFileFolder) ||
        matchesMask<AnyMatch>(pathFmt, excludeMasksFolder))
    {
        if (childItemMightMatch)
            *childItemMightMatch = false; //perf: no need to traverse deeper; subfolders/subfiles would be excluded by filter anyway!
        /*
        Attention: the design choice that "childItemMightMatch" is optional implies that the filter must provide correct results no matter if this
        value is considered by the client!
        In particular, if *childItemMightMatch == false, then any filter evaluations for child items must also return "false"!
        This is not a problem for folder traversal which stops at the first *childItemMightMatch == false anyway, but other code continues recursing further,
        e.g. the database update code in db_file.cpp recurses unconditionally without filter check! It's possible to construct edge cases with incorrect
        behavior if "childItemMightMatch" were not optional:
            1. two folders including a subfolder with some files are in sync with up-to-date database files
            2. deny access to this subfolder on both sides and start sync ignoring errors
            3. => database entries of this subfolder are incorrectly deleted! (if sub-folder is excluded, but child items are not!)
        */
        return false;
    }

    if (!matchesMask<AnyMatch>(pathFmt, includeMasksFileFolder) &&
        !matchesMask<AnyMatch>(pathFmt, includeMasksFolder))
    {
        if (childItemMightMatch)
        {
            const Zstring& childPathBegin = pathFmt + FILE_NAME_SEPARATOR;

            *childItemMightMatch = matchesMaskBegin(childPathBegin, includeMasksFileFolder) || //might match a file  or folder in subdirectory
                                   matchesMaskBegin(childPathBegin, includeMasksFolder);       //
        }
        return false;
    }

    return true;
}


bool NameFilter::isNull(const Zstring& includePhrase, const Zstring& excludePhrase)
{
    const Zstring include = trimCpy(includePhrase);
    const Zstring exclude = trimCpy(excludePhrase);

    return include == Zstr("*") && exclude.empty();
    //return NameFilter(includePhrase, excludePhrase).isNull(); -> very expensive for huge lists
}


bool NameFilter::isNull() const
{
    return includeMasksFileFolder.size() == 1 && includeMasksFileFolder[0] == Zstr("*") &&
           includeMasksFolder    .empty() &&
           excludeMasksFileFolder.empty() &&
           excludeMasksFolder    .empty();
    //avoid static non-POD null-NameFilter instance; instead test manually and verify function on startup:
}



bool NameFilter::cmpLessSameType(const PathFilter& other) const
{
    assert(typeid(*this) == typeid(other)); //always given in this context!

    const NameFilter& otherNameFilt = static_cast<const NameFilter&>(other);

    if (includeMasksFileFolder != otherNameFilt.includeMasksFileFolder)
        return includeMasksFileFolder < otherNameFilt.includeMasksFileFolder;

    if (includeMasksFolder != otherNameFilt.includeMasksFolder)
        return includeMasksFolder < otherNameFilt.includeMasksFolder;

    if (excludeMasksFileFolder != otherNameFilt.excludeMasksFileFolder)
        return excludeMasksFileFolder < otherNameFilt.excludeMasksFileFolder;

    if (excludeMasksFolder != otherNameFilt.excludeMasksFolder)
        return excludeMasksFolder < otherNameFilt.excludeMasksFolder;

    return false; //all equal
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "resolve_path.h"
#include "time.h"
#include "thread.h"
#include "file_access.h"

    #include <zen/sys_info.h>
    #include <unistd.h> //getcwd()

using namespace zen;


namespace
{
Zstring resolveRelativePath(const Zstring& relativePath)
{
    if (relativePath.empty())
        return relativePath;

    Zstring pathTmp = relativePath;
    //https://linux.die.net/man/2/path_resolution
    if (!startsWith(pathTmp, FILE_NAME_SEPARATOR)) //absolute names are exactly those starting with a '/'
    {
        /* basic support for '~': strictly speaking this is a shell-layer feature, so "realpath()" won't handle it
            https://www.gnu.org/software/bash/manual/html_node/Tilde-Expansion.html               */
        if (startsWith(pathTmp, "~/") || pathTmp == "~")
        {
            try
            {
                const Zstring& homePath = getUserHome(); //throw FileError

                if (startsWith(pathTmp, "~/"))
                    pathTmp = appendPath(homePath, pathTmp.c_str() + 2);
                else //pathTmp == "~"
                    pathTmp = homePath;
            }
            catch (FileError&) {}
            //else: error! no further processing!
        }
        else
        {
            //we cannot use ::realpath() which only resolves *existing* relative paths!
            if (char* dirPath = ::getcwd(nullptr, 0))
            {
                ZEN_ON_SCOPE_EXIT(::free(dirPath));
                pathTmp = appendPath(dirPath, pathTmp);
            }
        }
    }
    //get rid of some cruft (just like GetFullPathName())
    replace(pathTmp, "/./", '/');
    if (endsWith(pathTmp, "/."))
        pathTmp.pop_back(); //keep the "/" => consider pathTmp == "/."

    //what about "/../"? might be relative to symlinks => preserve!

    return pathTmp;
}




//returns value if resolved
std::optional<Zstring> tryResolveMacro(const ZstringView macro) //macro without %-characters
{
    Zstring timeStr;
    auto resolveTimePhrase = [&](const Zchar* phrase, const Zchar* format) -> bool
    {
        if (!equalAsciiNoCase(macro, phrase))
            return false;

        timeStr = formatTime(format);
        return true;
    };

    //https://en.cppreference.com/w/cpp/chrono/c/strftime
    //there exist environment variables named %TIME%, %DATE% so check for our internal macros first!
    if (resolveTimePhrase(Zstr("Date"),        Zstr("%Y-%m-%d")))        return timeStr;
    if (resolveTimePhrase(Zstr("Time"),        Zstr("%H%M%S")))          return timeStr;
    if (resolveTimePhrase(Zstr("TimeStamp"),   Zstr("%Y-%m-%d %H%M%S"))) return timeStr; //e.g. "2012-05-15 131513"
    if (resolveTimePhrase(Zstr("Year"),        Zstr("%Y")))              return timeStr;
    if (resolveTimePhrase(Zstr("Month"),       Zstr("%m")))              return timeStr;
    if (resolveTimePhrase(Zstr("MonthName"),   Zstr("%b")))              return timeStr; //e.g. "Jan"
    if (resolveTimePhrase(Zstr("Day"),         Zstr("%d")))              return timeStr;
    if (resolveTimePhrase(Zstr("Hour"),        Zstr("%H")))              return timeStr;
    if (resolveTimePhrase(Zstr("Min"),         Zstr("%M")))              return timeStr;
    if (resolveTimePhrase(Zstr("Sec"),         Zstr("%S")))              return timeStr;
    if (resolveTimePhrase(Zstr("WeekDayName"), Zstr("%a")))              return timeStr; //e.g. "Mon"
    if (resolveTimePhrase(Zstr("Week"),        Zstr("%V")))              return timeStr; //ISO 8601 week of the year

    if (equalAsciiNoCase(macro, Zstr("WeekDay")))
    {
        const int weekDayStartSunday = stringTo<int>(formatTime(Zstr("%w"))); //[0 (Sunday), 6 (Saturday)] => not localized!
        //alternative 1: use "%u": ISO 8601 weekday as number with Monday as 1 (1-7) => newer standard than %w
        //alternative 2: ::mktime() + std::tm::tm_wday

        const int weekDayStartMonday = (weekDayStartSunday + 6) % 7; //+6 == -1 in Z_7
        // [0-Monday, 6-Sunday]

        const int weekDayStartLocal = ((weekDayStartMonday + 7 - static_cast<int>(getFirstDayOfWeek())) % 7) + 1;
        //[1 (local first day of week), 7 (local last day of week)]

        return numberTo<Zstring>(weekDayStartLocal);
    }

    //try to resolve as environment variables
    if (std::optional<Zstring> value = getEnvironmentVar(macro))
        return *value;

    return {};
}

const Zchar MACRO_SEP = Zstr('%');
}


//returns expanded or original string
Zstring zen::expandMacros(const Zstring& text)
{
    if (contains(text, MACRO_SEP))
    {
        Zstring prefix = beforeFirst(text, MACRO_SEP, IfNotFoundReturn::none);
        Zstring rest   = afterFirst (text, MACRO_SEP, IfNotFoundReturn::none);
        if (contains(rest, MACRO_SEP))
        {
            Zstring potentialMacro = beforeFirst(rest, MACRO_SEP, IfNotFoundReturn::none);
            Zstring postfix        = afterFirst (rest, MACRO_SEP, IfNotFoundReturn::none); //text == prefix + MACRO_SEP + potentialMacro + MACRO_SEP + postfix

            if (std::optional<Zstring> value = tryResolveMacro(potentialMacro))
                return prefix + *value + expandMacros(postfix);
            else
                return prefix + MACRO_SEP + potentialMacro + expandMacros(MACRO_SEP + postfix);
        }
    }
    return text;
}


namespace
{


//expand volume name if possible, return original input otherwise
Zstring tryExpandVolumeName(Zstring pathPhrase)  // [volname]:\folder    [volname]\folder    [volname]folder    -> C:\folder
{
    //we only expect the [.*] pattern at the beginning => do not touch dir names like "C:\somedir\[stuff]"
    trim(pathPhrase, TrimSide::left);

    if (startsWith(pathPhrase, Zstr('[')))
    {
        return "/.../" + pathPhrase;
    }
    return pathPhrase;
}
}


std::vector<Zstring> zen::getPathPhraseAliases(const Zstring& itemPath)
{
    assert(!itemPath.empty());
    std::vector<Zstring> pathAliases{makePathPhrase(itemPath)};

    {

        //environment variables: C:\Users\<user> -> %UserProfile%
        auto substByMacro = [&](const ZstringView macroName, const Zstring& macroPath)
        {
            //should use a replaceCpy() that considers "local path" case-sensitivity (if only we had one...)
            if (contains(itemPath, macroPath))
                pathAliases.push_back(makePathPhrase(replaceCpyAsciiNoCase(itemPath, macroPath, Zstring() + MACRO_SEP + macroName + MACRO_SEP)));
        };

        for (const ZstringView envName :
             {
                 "HOME", //Linux: /home/<user>  Mac: /Users/<user>
                 //"USER",  -> any benefit?
             })
            if (const std::optional<Zstring> envPath = getEnvironmentVar(envName))
                substByMacro(envName, *envPath);

    }
    //removeDuplicates()? should not be needed...

    std::sort(pathAliases.begin(), pathAliases.end(), LessNaturalSort() /*even on Linux*/);
    return pathAliases;
}


Zstring zen::makePathPhrase(const Zstring& itemPath)
{
    if (endsWith(itemPath, Zstr(' '))) //path phrase concept must survive trimming!
        return itemPath + FILE_NAME_SEPARATOR;
    return itemPath;
}


//coordinate changes with acceptsFolderPathPhraseNative()!
Zstring zen::getResolvedFilePath(const Zstring& pathPhrase) //noexcept
{
    Zstring path = pathPhrase;

    path = expandMacros(path); //expand before trimming!

    trim(path); //remove leading/trailing whitespace before allowing misinterpretation in applyLongPathPrefix()

    {
        path = tryExpandVolumeName(path); //may block for slow USB sticks and idle HDDs!

        /* need to resolve relative paths:
             WINDOWS:
              - \\?\-prefix requires absolute names
              - Volume Shadow Copy: volume name needs to be part of each file path
              - file icon buffer (at least for extensions that are actually read from disk, like "exe")
             WINDOWS/LINUX:
              - detection of dependent directories, e.g. "\" and "C:\test"                       */
        path = resolveRelativePath(path);
    }

    //remove trailing slash, unless volume root:
    if (const std::optional<PathComponents> pc = parsePathComponents(path))
        path = appendPath(pc->rootPath, pc->relPath);

    return path;
}



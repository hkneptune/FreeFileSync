// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "resolve_path.h"
#include <set> //not necessarily included by <map>!
#include <map>
#include <zen/time.h>
#include <zen/thread.h>
#include <zen/utf.h>
#include <zen/scope_guard.h>
#include <zen/globals.h>
#include <zen/file_access.h>

    #include <stdlib.h> //getenv()
    #include <unistd.h> //getcwd

using namespace zen;


namespace
{
std::optional<Zstring> getEnvironmentVar(const Zstring& name)
{
    assert(runningMainThread()); //getenv() is not thread-safe!

    const char* buffer = ::getenv(name.c_str()); //no extended error reporting
    if (!buffer)
        return {};
    Zstring value(buffer);

    //some postprocessing:
    trim(value); //remove leading, trailing blanks

    //remove leading, trailing double-quotes
    if (startsWith(value, Zstr('"')) &&
        endsWith  (value, Zstr('"')) &&
        value.length() >= 2)
        value = Zstring(value.c_str() + 1, value.length() - 2);

    return value;
}


Zstring resolveRelativePath(const Zstring& relativePath)
{
    assert(runningMainThread()); //GetFullPathName() is documented to NOT be thread-safe!
    /* MSDN: "Multithreaded applications and shared library code should not use the GetFullPathName function
        and should avoid using relative path names.
        The current directory state written by the SetCurrentDirectory function is stored as a global variable in each process,      */

    if (relativePath.empty())
        return relativePath;

    Zstring pathTmp = relativePath;
    //https://linux.die.net/man/2/path_resolution
    if (!startsWith(pathTmp, FILE_NAME_SEPARATOR)) //absolute names are exactly those starting with a '/'
    {
        /* basic support for '~': strictly speaking this is a shell-layer feature, so "realpath()" won't handle it
            https://www.gnu.org/software/bash/manual/html_node/Tilde-Expansion.html

            https://linux.die.net/man/3/getpwuid: An application that wants to determine its user's home directory
            should inspect the value of HOME (rather than the value getpwuid(getuid())->pw_dir) since this allows
            the user to modify their notion of "the home directory" during a login session.                       */
        if (startsWith(pathTmp, "~/") || pathTmp == "~")
        {
            if (const std::optional<Zstring> homeDir = getEnvironmentVar("HOME"))
            {
                if (startsWith(pathTmp, "~/"))
                    pathTmp = appendSeparator(*homeDir) + afterFirst(pathTmp, '/', IF_MISSING_RETURN_NONE);
                else //pathTmp == "~"
                    pathTmp = *homeDir;
            }
            //else: error! no further processing!
        }
        else
        {
            //we cannot use ::realpath() since it resolves *existing* relative paths only!
            if (char* dirPath = ::getcwd(nullptr, 0))
            {
                ZEN_ON_SCOPE_EXIT(::free(dirPath));
                pathTmp = appendSeparator(dirPath) + pathTmp;
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
std::optional<Zstring> tryResolveMacro(const Zstring& macro) //macro without %-characters
{
    //there exist environment variables named %TIME%, %DATE% so check for our internal macros first!
    if (equalAsciiNoCase(macro, "time"))
        return formatTime(Zstr("%H%M%S"));

    if (equalAsciiNoCase(macro, "date"))
        return formatTime(formatIsoDateTag);

    if (equalAsciiNoCase(macro, "timestamp"))
        return formatTime(Zstr("%Y-%m-%d %H%M%S")); //e.g. "2012-05-15 131513"

    Zstring timeStr;
    auto resolveTimePhrase = [&](const Zchar* phrase, const Zchar* format) -> bool
    {
        if (!equalAsciiNoCase(macro, phrase))
            return false;

        timeStr = formatTime(format);
        return true;
    };

    //https://en.cppreference.com/w/cpp/chrono/c/strftime
    if (resolveTimePhrase(Zstr("weekday"), Zstr("%A"))) return timeStr;
    if (resolveTimePhrase(Zstr("day"    ), Zstr("%d"))) return timeStr;
    if (resolveTimePhrase(Zstr("month"  ), Zstr("%m"))) return timeStr;
    if (resolveTimePhrase(Zstr("week"   ), Zstr("%V"))) return timeStr; //ISO 8601 week of the year
    if (resolveTimePhrase(Zstr("year"   ), Zstr("%Y"))) return timeStr;
    if (resolveTimePhrase(Zstr("hour"   ), Zstr("%H"))) return timeStr;
    if (resolveTimePhrase(Zstr("min"    ), Zstr("%M"))) return timeStr;
    if (resolveTimePhrase(Zstr("sec"    ), Zstr("%S"))) return timeStr;

    //try to resolve as environment variable
    if (std::optional<Zstring> value = getEnvironmentVar(macro))
        return *value;


    return {};
}

const Zchar MACRO_SEP = Zstr('%');
}

//returns expanded or original string
Zstring fff::expandMacros(const Zstring& text)
{
    if (contains(text, MACRO_SEP))
    {
        Zstring prefix = beforeFirst(text, MACRO_SEP, IF_MISSING_RETURN_NONE);
        Zstring rest   = afterFirst (text, MACRO_SEP, IF_MISSING_RETURN_NONE);
        if (contains(rest, MACRO_SEP))
        {
            Zstring potentialMacro = beforeFirst(rest, MACRO_SEP, IF_MISSING_RETURN_NONE);
            Zstring postfix        = afterFirst (rest, MACRO_SEP, IF_MISSING_RETURN_NONE); //text == prefix + MACRO_SEP + potentialMacro + MACRO_SEP + postfix

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
Zstring expandVolumeName(Zstring pathPhrase)  // [volname]:\folder       [volname]\folder       [volname]folder     -> C:\folder
{
    //use C++11 regex?

    //we only expect the [.*] pattern at the beginning => do not touch dir names like "C:\somedir\[stuff]"
    trim(pathPhrase, true, false);
    if (startsWith(pathPhrase, Zstr('[')))
    {
        const size_t posEnd = pathPhrase.find(Zstr(']'));
        if (posEnd != Zstring::npos)
        {
            Zstring volName = Zstring(pathPhrase.c_str() + 1, posEnd - 1);
            Zstring relPath = Zstring(pathPhrase.c_str() + posEnd + 1);

            if (startsWith(relPath, FILE_NAME_SEPARATOR))
                relPath = afterFirst(relPath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
            else if (startsWith(relPath, Zstr(":\\"))) //Win-only
                relPath = afterFirst(relPath, Zstr('\\'), IF_MISSING_RETURN_NONE);
            return "/.../[" + volName + "]/" + relPath;
        }
    }
    return pathPhrase;
}


void getFolderAliasesRecursive(const Zstring& pathPhrase, std::set<Zstring, LessNativePath>& output)
{

    //3. environment variables: C:\Users\<user> -> %UserProfile%
    {
        std::vector<std::pair<Zstring, Zstring>> macroList;

        //get list of useful variables
        auto addEnvVar = [&](const Zstring& envName)
        {
            if (std::optional<Zstring> value = getEnvironmentVar(envName))
                macroList.emplace_back(envName, *value);
        };
        addEnvVar("HOME"); //Linux: /home/<user>  Mac: /Users/<user>
        //addEnvVar("USER");  -> any benefit?
        //substitute paths by symbolic names
        for (const auto& [macroName, macroPath] : macroList)
        {
            //should use a replaceCpy() that considers "local path" case-sensitivity (if only we had one...)
            const Zstring pathSubst = replaceCpyAsciiNoCase(pathPhrase, macroPath, MACRO_SEP + macroName + MACRO_SEP);
            if (pathSubst != pathPhrase)
                output.insert(pathSubst);
        }
    }

    //4. replace (all) macros: %UserProfile% -> C:\Users\<user>
    {
        const Zstring pathExp = fff::expandMacros(pathPhrase);
        if (pathExp != pathPhrase)
            if (output.insert(pathExp).second)
                getFolderAliasesRecursive(pathExp, output); //recurse!
    }
}
}


std::vector<Zstring> fff::getFolderPathAliases(const Zstring& folderPathPhrase)
{
    const Zstring dirPath = trimCpy(folderPathPhrase);
    if (dirPath.empty())
        return {};

    std::set<Zstring, LessNativePath> tmp;
    getFolderAliasesRecursive(dirPath, tmp);

    tmp.erase(dirPath);
    tmp.erase(Zstring());

    return { tmp.begin(), tmp.end() };
}


//coordinate changes with acceptsFolderPathPhraseNative()!
Zstring fff::getResolvedFilePath(const Zstring& pathPhrase) //noexcept
{
    Zstring path = pathPhrase;

    path = expandMacros(path); //expand before trimming!

    //remove leading/trailing whitespace before allowing misinterpretation in applyLongPathPrefix()
    trim(path); //attention: don't remove all whitespace from right, e.g. 0xa0 may be used as part of a folder name


    path = expandVolumeName(path); //may block for slow USB sticks and idle HDDs!

    /* need to resolve relative paths:
         WINDOWS:
          - \\?\-prefix requires absolute names
          - Volume Shadow Copy: volume name needs to be part of each file path
          - file icon buffer (at least for extensions that are actually read from disk, like "exe")
          - Use of relative path names is not thread safe! (e.g. SHFileOperation)
         WINDOWS/LINUX:
          - detection of dependent directories, e.g. "\" and "C:\test"                       */
    path = resolveRelativePath(path);

    //remove trailing slash, unless volume root:
    if (std::optional<PathComponents> pc = parsePathComponents(path))
    {
        if (pc->relPath.empty())
            path = pc->rootPath;
        else
            path = appendSeparator(pc->rootPath) + pc->relPath;
    } //keep this brace for GCC: -Wparentheses

    return path;
}



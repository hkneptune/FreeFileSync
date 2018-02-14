#include "resolve_path.h"
#include <set> //not necessarily included by <map>!
#include <map>
#include <zen/time.h>
#include <zen/thread.h>
#include <zen/utf.h>
#include <zen/optional.h>
#include <zen/scope_guard.h>
#include <zen/globals.h>
#include <zen/file_access.h>

    #include <stdlib.h> //getenv()
    #include <unistd.h> //getcwd

using namespace zen;


namespace
{


Opt<Zstring> getEnvironmentVar(const Zstring& name)
{
    assert(std::this_thread::get_id() == mainThreadId); //getenv() is not thread-safe!

    const char* buffer = ::getenv(name.c_str()); //no extended error reporting
    if (!buffer)
        return NoValue();
    Zstring value(buffer);

    //some postprocessing:
    trim(value); //remove leading, trailing blanks

    //remove leading, trailing double-quotes
    if (startsWith(value, Zstr('\"')) &&
        endsWith  (value, Zstr('\"')) &&
        value.length() >= 2)
        value = Zstring(value.c_str() + 1, value.length() - 2);

    return value;
}


Zstring resolveRelativePath(const Zstring& relativePath)
{
    assert(std::this_thread::get_id() == mainThreadId); //GetFullPathName() is documented to NOT be thread-safe!

    //http://linux.die.net/man/2/path_resolution
    if (!startsWith(relativePath, FILE_NAME_SEPARATOR)) //absolute names are exactly those starting with a '/'
    {
        /*
        basic support for '~': strictly speaking this is a shell-layer feature, so "realpath()" won't handle it
        http://www.gnu.org/software/bash/manual/html_node/Tilde-Expansion.html

        http://linux.die.net/man/3/getpwuid: An application that wants to determine its user's home directory
        should inspect the value of HOME (rather than the value getpwuid(getuid())->pw_dir) since this allows
        the user to modify their notion of "the home directory" during a login session.
        */
        if (startsWith(relativePath, "~/") || relativePath == "~")
        {
            Opt<Zstring> homeDir = getEnvironmentVar("HOME");
            if (!homeDir)
                return relativePath; //error! no further processing!

            if (startsWith(relativePath, "~/"))
                return appendSeparator(*homeDir) + afterFirst(relativePath, '/', IF_MISSING_RETURN_NONE);
            else if (relativePath == "~")
                return *homeDir;
        }

        //we cannot use ::realpath() since it resolves *existing* relative paths only!
        if (char* dirPath = ::getcwd(nullptr, 0))
        {
            ZEN_ON_SCOPE_EXIT(::free(dirPath));
            return appendSeparator(dirPath) + relativePath;
        }
    }
    return relativePath;
}



//returns value if resolved
Opt<Zstring> tryResolveMacro(const Zstring& macro) //macro without %-characters
{
    //there exist environment variables named %TIME%, %DATE% so check for our internal macros first!
    if (strEqual(macro, Zstr("time"), CmpAsciiNoCase()))
        return formatTime<Zstring>(Zstr("%H%M%S"));

    if (strEqual(macro, Zstr("date"), CmpAsciiNoCase()))
        return formatTime<Zstring>(FORMAT_ISO_DATE);

    if (strEqual(macro, Zstr("timestamp"), CmpAsciiNoCase()))
        return formatTime<Zstring>(Zstr("%Y-%m-%d %H%M%S")); //e.g. "2012-05-15 131513"

    Zstring timeStr;
    auto resolveTimePhrase = [&](const Zchar* phrase, const Zchar* format) -> bool
    {
        if (!strEqual(macro, phrase, CmpAsciiNoCase()))
            return false;

        timeStr = formatTime<Zstring>(format);
        return true;
    };

    if (resolveTimePhrase(Zstr("weekday"), Zstr("%A"))) return timeStr;
    if (resolveTimePhrase(Zstr("day"    ), Zstr("%d"))) return timeStr;
    if (resolveTimePhrase(Zstr("month"  ), Zstr("%m"))) return timeStr;
    if (resolveTimePhrase(Zstr("week"   ), Zstr("%U"))) return timeStr;
    if (resolveTimePhrase(Zstr("year"   ), Zstr("%Y"))) return timeStr;
    if (resolveTimePhrase(Zstr("hour"   ), Zstr("%H"))) return timeStr;
    if (resolveTimePhrase(Zstr("min"    ), Zstr("%M"))) return timeStr;
    if (resolveTimePhrase(Zstr("sec"    ), Zstr("%S"))) return timeStr;

    //try to resolve as environment variable
    if (Opt<Zstring> value = getEnvironmentVar(macro))
        return *value;


    return NoValue();
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

            if (Opt<Zstring> value = tryResolveMacro(potentialMacro))
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
    //this would be a nice job for a C++11 regex...

    //we only expect the [.*] pattern at the beginning => do not touch dir names like "C:\somedir\[stuff]"
    trim(pathPhrase, true, false);
    if (startsWith(pathPhrase, Zstr("[")))
    {
        size_t posEnd = pathPhrase.find(Zstr("]"));
        if (posEnd != Zstring::npos)
        {
            Zstring volName = Zstring(pathPhrase.c_str() + 1, posEnd - 1);
            Zstring relPath = Zstring(pathPhrase.c_str() + posEnd + 1);

            if (startsWith(relPath, Zstr(':')))
                relPath = afterFirst(relPath, Zstr(':'), IF_MISSING_RETURN_NONE);
            if (startsWith(relPath, FILE_NAME_SEPARATOR))
                relPath = afterFirst(relPath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
            return "/.../[" + volName + "]/" + relPath;
        }
    }
    return pathPhrase;
}


void getDirectoryAliasesRecursive(const Zstring& pathPhrase, std::set<Zstring, LessFilePath>& output)
{

    //3. environment variables: C:\Users\<user> -> %UserProfile%
    {
        std::vector<std::pair<Zstring, Zstring>> macroList;

        //get list of useful variables
        auto addEnvVar = [&](const Zstring& envName)
        {
            if (Opt<Zstring> value = getEnvironmentVar(envName))
                macroList.emplace_back(envName, *value);
        };
        addEnvVar("HOME"); //Linux: /home/<user>  Mac: /Users/<user>
        //addEnvVar("USER");  -> any benefit?
        //substitute paths by symbolic names
        for (const auto& item : macroList)
        {
            const Zstring& macroName = item.first;
            const Zstring& macroPath = item.second;

            const Zstring pathSubst = ciReplaceCpy(pathPhrase, macroPath, MACRO_SEP + macroName + MACRO_SEP); //ci on Linux, too? okay
            if (pathSubst != pathPhrase)
                output.insert(pathSubst);
        }
    }

    //4. replace (all) macros: %UserProfile% -> C:\Users\<user>
    {
        const Zstring pathExp = fff::expandMacros(pathPhrase);
        if (pathExp != pathPhrase)
            if (output.insert(pathExp).second)
                getDirectoryAliasesRecursive(pathExp, output); //recurse!
    }
}
}


std::vector<Zstring> fff::getDirectoryAliases(const Zstring& folderPathPhrase)
{
    const Zstring dirPath = trimCpy(folderPathPhrase, true, false);
    if (dirPath.empty())
        return {};

    std::set<Zstring, LessFilePath> tmp;
    getDirectoryAliasesRecursive(dirPath, tmp);

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
    trim(path, true, false);
    //don't remove all whitespace from right, e.g. 0xa0 may be used as part of folder name
    trim(path, false, true, [](Zchar c) { return c == Zstr(' '); });


    path = expandVolumeName(path); //may block for slow USB sticks and idle HDDs!

    if (path.empty()) //an empty string would later be resolved as "\"; this is not desired
        return Zstring();
    /*
    need to resolve relative paths:
    WINDOWS:
     - \\?\-prefix requires absolute names
     - Volume Shadow Copy: volume name needs to be part of each file path
     - file icon buffer (at least for extensions that are actually read from disk, like "exe")
     - Use of relative path names is not thread safe! (e.g. SHFileOperation)
    WINDOWS/LINUX:
     - detection of dependent directories, e.g. "\" and "C:\test"
     */
    path = resolveRelativePath(path);

    //remove trailing slash, unless volume root:
    if (Opt<PathComponents> pc = parsePathComponents(path))
    {
        if (pc->relPath.empty())
            path = pc->rootPath;
        else
            path = appendSeparator(pc->rootPath) + pc->relPath;
    } //keep this brace for GCC: -Wparentheses

    return path;
}



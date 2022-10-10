// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "config.h"
#include <zen/file_access.h>
#include <zen/process_exec.h>
#include <zenxml/xml.h>
#include <wx/intl.h>
#include "../ffs_paths.h"
#include "../localization.h"

using namespace zen;
using namespace rts;

//-------------------------------------------------------------------------------------------------------------------------------
const int XML_FORMAT_RTS_CFG = 2; //2020-04-14
//-------------------------------------------------------------------------------------------------------------------------------


namespace zen
{
template <> inline
bool readText(const std::string& input, wxLanguage& value)
{
    if (const wxLanguageInfo* lngInfo = wxLocale::FindLanguageInfo(utfTo<wxString>(input)))
    {
        value = static_cast<wxLanguage>(lngInfo->Language);
        return true;
    }
    return false;
}
}


namespace
{
std::string getConfigType(const XmlDoc& doc)
{
    if (doc.root().getName() == "FreeFileSync")
    {
        std::string type;
        if (doc.root().getAttribute("XmlType", type))
            return type;
    }
    return {};
}


void readConfig(const XmlIn& in, XmlRealConfig& cfg, int formatVer)
{
    in["Directories"](cfg.directories);
    in["Delay"      ](cfg.delay);
    in["Commandline"](cfg.commandline);

    //TODO: remove if clause after migration! 2020-04-14
    if (formatVer < 2)
        if (startsWithAsciiNoCase(cfg.commandline, "cmd /c ") ||
            startsWithAsciiNoCase(cfg.commandline, "cmd.exe /c "))
            cfg.commandline = afterFirst(cfg.commandline, Zstr("/c "), IfNotFoundReturn::all);
}


void writeConfig(const XmlRealConfig& cfg, XmlOut& out)
{
    out["Directories"](cfg.directories);
    out["Delay"      ](cfg.delay);
    out["Commandline"](cfg.commandline);
}
}


void rts::readConfig(const Zstring& filePath, XmlRealConfig& cfg, std::wstring& warningMsg) //throw FileError
{
    XmlDoc doc = loadXml(filePath); //throw FileError

    if (getConfigType(doc) != "REAL")
        throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));

    int formatVer = 0;
    /*bool success =*/ doc.root().getAttribute("XmlFormat", formatVer);

    XmlIn in(doc);
    ::readConfig(in, cfg, formatVer);

    try
    {
        checkXmlMappingErrors(in, filePath); //throw FileError

        //(try to) migrate old configuration automatically
        if (formatVer < XML_FORMAT_RTS_CFG)
            try { rts::writeConfig(cfg, filePath); /*throw FileError*/ }
            catch (FileError&) { assert(false); } //don't bother user!
        warn_static("at least log on failure!")
    }
    catch (const FileError& e) { warningMsg = e.toString(); }
}


void rts::writeConfig(const XmlRealConfig& cfg, const Zstring& filePath) //throw FileError
{
    XmlDoc doc("FreeFileSync");
    doc.root().setAttribute("XmlType", "REAL");
    doc.root().setAttribute("XmlFormat", XML_FORMAT_RTS_CFG);

    XmlOut out(doc);
    ::writeConfig(cfg, out);

    saveXml(doc, filePath); //throw FileError
}


void rts::readRealOrBatchConfig(const Zstring& filePath, XmlRealConfig& cfg, std::wstring& warningMsg) //throw FileError
{
    XmlDoc doc = loadXml(filePath); //throw FileError
    //quick exit if file is not an FFS XML

    //convert batch config to RealTimeSync config
    if (getConfigType(doc) == "BATCH")
    {
        XmlIn in(doc);

        //read folder pairs
        std::set<Zstring, LessNativePath> uniqueFolders;

        for (XmlIn inPair = in["FolderPairs"]["Pair"]; inPair; inPair.next())
        {
            Zstring folderPathPhraseLeft;
            Zstring folderPathPhraseRight;
            inPair["Left" ](folderPathPhraseLeft);
            inPair["Right"](folderPathPhraseRight);

            uniqueFolders.insert(folderPathPhraseLeft);
            uniqueFolders.insert(folderPathPhraseRight);
        }

        //don't report failure as warning only:
        checkXmlMappingErrors(in, filePath); //throw FileError
        //---------------------------------------------------------------------------------------

        std::erase_if(uniqueFolders, [](const Zstring& str) { return trimCpy(str).empty(); });
        cfg.directories.assign(uniqueFolders.begin(), uniqueFolders.end());
        cfg.commandline = escapeCommandArg(fff::getFreeFileSyncLauncherPath()) + Zstr(' ') + escapeCommandArg(filePath);
    }
    else
        return readConfig(filePath, cfg, warningMsg); //throw FileError
}


wxLanguage rts::getProgramLanguage() //throw FileError
{
    const Zstring& filePath = appendPath(fff::getConfigDirPath(), Zstr("GlobalSettings.xml"));

    XmlDoc doc;
    try
    {
        doc = loadXml(filePath); //throw FileError
    }
    catch (FileError&)
    {
        if (!itemStillExists(filePath)) //throw FileError
            return fff::getDefaultLanguage();
        throw;
    }

    if (getConfigType(doc) != "GLOBAL")
        throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));

    XmlIn in(doc);

    wxLanguage lng = wxLANGUAGE_UNKNOWN;
    in["Language"].attribute("Code", lng);

    checkXmlMappingErrors(in, filePath); //throw FileError
    return lng;
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "config.h"
#include <zen/file_access.h>
#include <zen/process_exec.h>
#include <zenxml/xml.h>
#include <wx/uilocale.h>
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
    if (const wxLanguageInfo* lngInfo = wxUILocale::FindLanguageInfo(utfTo<wxString>(input)))
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


std::pair<XmlRealConfig, std::wstring /*warningMsg*/> rts::readConfig(const Zstring& filePath) //throw FileError
{
    XmlDoc doc = loadXml(filePath); //throw FileError

    if (getConfigType(doc) != "REAL")
        throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));

    int formatVer = 0;
    /*bool success =*/ doc.root().getAttribute("XmlFormat", formatVer);

    XmlIn in(doc);
    XmlRealConfig cfg;
    ::readConfig(in, cfg, formatVer);

    std::wstring warningMsg;
    if (const std::wstring& errors = in.getErrors();
        !errors.empty())
        warningMsg = replaceCpy(_("Configuration file %x is incomplete. The missing elements have been set to their default values."), L"%x", fmtPath(filePath)) + L"\n\n" +
                     _("The following XML elements could not be read:") + L'\n' + errors;
    else //(try to) migrate old configuration automatically
        if (formatVer < XML_FORMAT_RTS_CFG)
            try
            {
                rts::writeConfig(cfg, filePath); //throw FileError
            }
            catch (const FileError& e) { warningMsg = e.toString(); }

    return {cfg, warningMsg};
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


std::pair<XmlRealConfig, std::wstring /*warningMsg*/> rts::readRealOrBatchConfig(const Zstring& filePath) //throw FileError
{
    XmlDoc doc = loadXml(filePath); //throw FileError
    //quick exit if file is not an FFS XML

    //convert batch config to RealTimeSync config
    if (getConfigType(doc) == "BATCH")
    {
        XmlIn in(doc);

        //read folder pairs
        std::set<Zstring, LessNativePath> uniqueFolders;

        in["FolderPairs"].visitChildren([&](const XmlIn& inPair)
        {
            assert(*inPair.getName() == "Pair");

            Zstring folderPathPhraseLeft;
            Zstring folderPathPhraseRight;
            inPair["Left" ](folderPathPhraseLeft);
            inPair["Right"](folderPathPhraseRight);

            uniqueFolders.insert(folderPathPhraseLeft);
            uniqueFolders.insert(folderPathPhraseRight);
        });

        if (const std::wstring& errors = in.getErrors();
            !errors.empty())
            throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)),
                            _("The following XML elements could not be read:") + L'\n' + errors);

        //---------------------------------------------------------------------------------------

        std::erase_if(uniqueFolders, [](const Zstring& str) { return trimCpy(str).empty(); });

        std::wstring warningMsg;
        const Zstring ffsLaunchPath = [&]() -> Zstring
        {
            try
            {
                return fff::getFreeFileSyncLauncherPath(); //throw FileError
            }
            catch (const FileError& e)
            {
                warningMsg = e.toString();
                return Zstr("FreeFileSync"); //fallback: at least give some hint...
            }
        }();

        XmlRealConfig cfg
        {
            .directories = {uniqueFolders.begin(), uniqueFolders.end()},
            .commandline = escapeCommandArg(ffsLaunchPath) + Zstr(' ') + escapeCommandArg(filePath),
        };
        return {cfg, warningMsg};
    }
    else
        return readConfig(filePath); //throw FileError
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
        if (!itemExists(filePath)) //throw FileError
            return fff::getDefaultLanguage();
        throw;
    }

    if (getConfigType(doc) != "GLOBAL")
        throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));

    XmlIn in(doc);

    wxLanguage lng = wxLANGUAGE_UNKNOWN;
    in["Language"].attribute("Code", lng);


    if (const std::wstring& errors = in.getErrors();
        !errors.empty())
        throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)),
                        _("The following XML elements could not be read:") + L'\n' + errors);

    return lng;
}

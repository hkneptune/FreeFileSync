// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "xml_proc.h"
#include <zen/file_access.h>
#include <zenxml/xml.h>
#include <wx/intl.h>
#include "../base/ffs_paths.h"
#include "../base/localization.h"

using namespace zen;
using namespace rts;


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
enum class RtsXmlType
{
    REAL,
    BATCH,
    GLOBAL,
    OTHER
};
RtsXmlType getXmlTypeNoThrow(const XmlDoc& doc) //throw()
{
    if (doc.root().getNameAs<std::string>() == "FreeFileSync")
    {
        std::string type;
        if (doc.root().getAttribute("XmlType", type))
        {
            if (type == "REAL")
                return RtsXmlType::REAL;
            else if (type == "BATCH")
                return RtsXmlType::BATCH;
            else if (type == "GLOBAL")
                return RtsXmlType::GLOBAL;
        }
    }
    return RtsXmlType::OTHER;
}


void readConfig(const XmlIn& in, XmlRealConfig& config)
{
    in["Directories"](config.directories);
    in["Delay"      ](config.delay);
    in["Commandline"](config.commandline);
}

void writeConfig(const XmlRealConfig& config, XmlOut& out)
{
    out["Directories"](config.directories);
    out["Delay"      ](config.delay);
    out["Commandline"](config.commandline);
}


template <class ConfigType>
void readConfig(const Zstring& filePath, RtsXmlType type, ConfigType& cfg, std::wstring& warningMsg) //throw FileError
{
    XmlDoc doc = loadXml(filePath); //throw FileError

    if (getXmlTypeNoThrow(doc) != type) //noexcept
        throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));

    XmlIn in(doc);
    ::readConfig(in, cfg);

    try
    {
        checkXmlMappingErrors(in, filePath); //throw FileError
    }
    catch (const FileError& e) { warningMsg = e.toString(); }
}
}


void rts::readConfig(const Zstring& filePath, XmlRealConfig& config, std::wstring& warningMsg) //throw FileError
{
    ::readConfig(filePath, RtsXmlType::REAL, config, warningMsg); //throw FileError
}


void rts::writeConfig(const XmlRealConfig& config, const Zstring& filepath) //throw FileError
{
    XmlDoc doc("FreeFileSync");
    doc.root().setAttribute("XmlType", "REAL");

    XmlOut out(doc);
    ::writeConfig(config, out);

    saveXml(doc, filepath); //throw FileError
}


void rts::readRealOrBatchConfig(const Zstring& filePath, XmlRealConfig& config, std::wstring& warningMsg) //throw FileError
{
    XmlDoc doc = loadXml(filePath); //throw FileError
    //quick exit if file is not an FFS XML

    const RtsXmlType xmlType = ::getXmlTypeNoThrow(doc);

    //convert batch config to RealTimeSync config
    if (xmlType == RtsXmlType::BATCH)
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

        //don't consider failure a warning only:
        checkXmlMappingErrors(in, filePath); //throw FileError

        //---------------------------------------------------------------------------------------

        eraseIf(uniqueFolders, [](const Zstring& str) { return trimCpy(str).empty(); });
        config.directories.assign(uniqueFolders.begin(), uniqueFolders.end());
        config.commandline = Zstr('"') + fff::getFreeFileSyncLauncherPath() + Zstr("\" \"") + filePath + Zstr('"');
    }
    else
        return readConfig(filePath, config, warningMsg); //throw FileError
}


wxLanguage rts::getProgramLanguage() //throw FileError
{
    const Zstring& filePath = fff::getConfigDirPathPf() + Zstr("GlobalSettings.xml");

    XmlDoc doc;
    try
    {
        doc = loadXml(filePath); //throw FileError
    }
    catch (FileError&)
    {
        if (!itemStillExists(filePath)) //throw FileError
            return fff::getSystemLanguage();
        throw;
    }

    if (getXmlTypeNoThrow(doc) != RtsXmlType::GLOBAL) //noexcept
        throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));

    XmlIn in(doc);

    wxLanguage lng = wxLANGUAGE_UNKNOWN;
    in["General"]["Language"].attribute("Name", lng);

    checkXmlMappingErrors(in, filePath); //throw FileError
    return lng;
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "config.h"
#include <zenxml/xml.h>
#include <zen/file_access.h>
#include <zen/file_io.h>
#include <zen/time.h>
#include <zen/process_exec.h>
#include <wx/intl.h>
#include "ffs_paths.h"
#include "base_tools.h"
#include "afs/native.h"


using namespace zen;
using namespace fff; //functionally needed for correct overload resolution!!!


namespace
{
//-------------------------------------------------------------------------------------------------------------------------------
const int XML_FORMAT_GLOBAL_CFG = 23; //2021-12-02
const int XML_FORMAT_SYNC_CFG   = 17; //2020-10-14
//-------------------------------------------------------------------------------------------------------------------------------
}


const ExternalApp fff::extCommandFileBrowse
//"xdg-open \"%parent_path%\"" -> not good enough: we need %local_path% for proper MTP/Google Drive handling
{L"Browse directory", "xdg-open \"$(dirname \"%local_path%\")\""};
//mark for extraction: _("Browse directory") Linux doesn't use the term "folder"


const ExternalApp fff::extCommandOpenDefault
//"xdg-open \"%parent_path%\"" -> not good enough: we need %local_path% for proper MTP/Google Drive handling
{L"Open with default application", "xdg-open \"%local_path%\""};


XmlType getXmlTypeNoThrow(const XmlDoc& doc) //throw()
{
    if (doc.root().getName() == "FreeFileSync")
    {
        std::string type;
        if (doc.root().getAttribute("XmlType", type))
        {
            if (type == "GUI")
                return XmlType::gui;
            else if (type == "BATCH")
                return XmlType::batch;
            else if (type == "GLOBAL")
                return XmlType::global;
        }
    }
    return XmlType::other;
}


XmlType fff::getXmlType(const Zstring& filePath) //throw FileError
{
    //quick exit if file is not an XML
    XmlDoc doc = loadXml(filePath); //throw FileError
    return ::getXmlTypeNoThrow(doc);
}


void setXmlType(XmlDoc& doc, XmlType type) //throw()
{
    switch (type)
    {
        case XmlType::gui:
            doc.root().setAttribute("XmlType", "GUI");
            break;
        case XmlType::batch:
            doc.root().setAttribute("XmlType", "BATCH");
            break;
        case XmlType::global:
            doc.root().setAttribute("XmlType", "GLOBAL");
            break;
        case XmlType::other:
            assert(false);
            break;
    }
}




XmlGlobalSettings::XmlGlobalSettings() :
    soundFileSyncFinished(getResourceDirPf() + Zstr("bell.wav")),
    soundFileAlertPending(getResourceDirPf() + Zstr("remind.wav"))
{
}

//################################################################################################################

Zstring fff::getGlobalConfigFile()
{
    return getConfigDirPathPf() + Zstr("GlobalSettings.xml");
}


XmlGuiConfig fff::convertBatchToGui(const XmlBatchConfig& batchCfg) //noexcept
{
    XmlGuiConfig output;
    output.mainCfg = batchCfg.mainCfg;
    return output;
}


XmlBatchConfig fff::convertGuiToBatch(const XmlGuiConfig& guiCfg, const BatchExclusiveConfig& batchExCfg) //noexcept
{
    XmlBatchConfig output;
    output.mainCfg = guiCfg.mainCfg;
    output.batchExCfg = batchExCfg;
    return output;
}


namespace
{
std::vector<Zstring> splitFilterByLines(Zstring filterPhrase)
{
    trim(filterPhrase);
    if (filterPhrase.empty())
        return {};

    return split(filterPhrase, Zstr('\n'), SplitOnEmpty::allow);
}

Zstring mergeFilterLines(const std::vector<Zstring>& filterLines)
{
    Zstring out;
    for (const Zstring& line : filterLines)
    {
        out += line;
        out += Zstr('\n');
    }
    return trimCpy(out);
}
}

namespace zen
{
template <> inline
void writeText(const wxLanguage& value, std::string& output)
{
    //use description as unique wxLanguage identifier, see localization.cpp
    //=> handle changes to wxLanguage enum between wxWidgets versions
    const wxLanguageInfo* lngInfo = wxLocale::GetLanguageInfo(value);
    assert(lngInfo);
    if (!lngInfo)
        lngInfo = wxLocale::GetLanguageInfo(wxLANGUAGE_ENGLISH_US);

    if (lngInfo)
        output = utfTo<std::string>(lngInfo->Description);
}

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


template <> inline
void writeText(const CompareVariant& value, std::string& output)
{
    switch (value)
    {
        case CompareVariant::timeSize:
            output = "TimeAndSize";
            break;
        case CompareVariant::content:
            output = "Content";
            break;
        case CompareVariant::size:
            output = "Size";
            break;
    }
}

template <> inline
bool readText(const std::string& input, CompareVariant& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "TimeAndSize")
        value = CompareVariant::timeSize;
    else if (tmp == "Content")
        value = CompareVariant::content;
    else if (tmp == "Size")
        value = CompareVariant::size;
    else
        return false;
    return true;
}


template <> inline
void writeText(const SyncDirection& value, std::string& output)
{
    switch (value)
    {
        case SyncDirection::left:
            output = "left";
            break;
        case SyncDirection::right:
            output = "right";
            break;
        case SyncDirection::none:
            output = "none";
            break;
    }
}

template <> inline
bool readText(const std::string& input, SyncDirection& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "left")
        value = SyncDirection::left;
    else if (tmp == "right")
        value = SyncDirection::right;
    else if (tmp == "none")
        value = SyncDirection::none;
    else
        return false;
    return true;
}


template <> inline
void writeText(const BatchErrorHandling& value, std::string& output)
{
    switch (value)
    {
        case BatchErrorHandling::showPopup:
            output = "Show";
            break;
        case BatchErrorHandling::cancel:
            output = "Cancel";
            break;
    }
}

template <> inline
bool readText(const std::string& input, ResultsNotification& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Always")
        value = ResultsNotification::always;
    else if (tmp == "ErrorWarning")
        value = ResultsNotification::errorWarning;
    else if (tmp == "ErrorOnly")
        value = ResultsNotification::errorOnly;
    else
        return false;
    return true;
}


template <> inline
void writeText(const ResultsNotification& value, std::string& output)
{
    switch (value)
    {
        case ResultsNotification::always:
            output = "Always";
            break;
        case ResultsNotification::errorWarning:
            output = "ErrorWarning";
            break;
        case ResultsNotification::errorOnly:
            output = "ErrorOnly";
            break;
    }
}


template <> inline
bool readText(const std::string& input, BatchErrorHandling& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Show")
        value = BatchErrorHandling::showPopup;
    else if (tmp == "Cancel")
        value = BatchErrorHandling::cancel;
    else
        return false;
    return true;
}


template <> inline
void writeText(const PostSyncCondition& value, std::string& output)
{
    switch (value)
    {
        case PostSyncCondition::completion:
            output = "Completion";
            break;
        case PostSyncCondition::errors:
            output = "Errors";
            break;
        case PostSyncCondition::success:
            output = "Success";
            break;
    }
}

template <> inline
bool readText(const std::string& input, PostSyncCondition& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Completion")
        value = PostSyncCondition::completion;
    else if (tmp == "Errors")
        value = PostSyncCondition::errors;
    else if (tmp == "Success")
        value = PostSyncCondition::success;
    else
        return false;
    return true;
}


template <> inline
void writeText(const PostSyncAction& value, std::string& output)
{
    switch (value)
    {
        case PostSyncAction::none:
            output = "None";
            break;
        case PostSyncAction::sleep:
            output = "Sleep";
            break;
        case PostSyncAction::shutdown:
            output = "Shutdown";
            break;
    }
}

template <> inline
bool readText(const std::string& input, PostSyncAction& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "None")
        value = PostSyncAction::none;
    else if (tmp == "Sleep")
        value = PostSyncAction::sleep;
    else if (tmp == "Shutdown")
        value = PostSyncAction::shutdown;
    else
        return false;
    return true;
}


template <> inline
void writeText(const FileIconSize& value, std::string& output)
{
    switch (value)
    {
        case FileIconSize::small:
            output = "Small";
            break;
        case FileIconSize::medium:
            output = "Medium";
            break;
        case FileIconSize::large:
            output = "Large";
            break;
    }
}

template <> inline
bool readText(const std::string& input, FileIconSize& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Small")
        value = FileIconSize::small;
    else if (tmp == "Medium")
        value = FileIconSize::medium;
    else if (tmp == "Large")
        value = FileIconSize::large;
    else
        return false;
    return true;
}


template <> inline
void writeText(const DeletionPolicy& value, std::string& output)
{
    switch (value)
    {
        case DeletionPolicy::permanent:
            output = "Permanent";
            break;
        case DeletionPolicy::recycler:
            output = "RecycleBin";
            break;
        case DeletionPolicy::versioning:
            output = "Versioning";
            break;
    }
}

template <> inline
bool readText(const std::string& input, DeletionPolicy& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Permanent")
        value = DeletionPolicy::permanent;
    else if (tmp == "RecycleBin")
        value = DeletionPolicy::recycler;
    else if (tmp == "Versioning")
        value = DeletionPolicy::versioning;
    else
        return false;
    return true;
}


template <> inline
void writeText(const SymLinkHandling& value, std::string& output)
{
    switch (value)
    {
        case SymLinkHandling::exclude:
            output = "Exclude";
            break;
        case SymLinkHandling::direct:
            output = "Direct";
            break;
        case SymLinkHandling::follow:
            output = "Follow";
            break;
    }
}

template <> inline
bool readText(const std::string& input, SymLinkHandling& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Exclude")
        value = SymLinkHandling::exclude;
    else if (tmp == "Direct")
        value = SymLinkHandling::direct;
    else if (tmp == "Follow")
        value = SymLinkHandling::follow;
    else
        return false;
    return true;
}


template <> inline
void writeText(const GridViewType& value, std::string& output)
{
    switch (value)
    {
        case GridViewType::difference:
            output = "Difference";
            break;
        case GridViewType::action:
            output = "Action";
            break;
    }
}

template <> inline
bool readText(const std::string& input, GridViewType& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Difference")
        value = GridViewType::difference;
    else if (tmp == "Action")
        value = GridViewType::action;
    else
        return false;
    return true;
}


template <> inline
void writeText(const ColumnTypeRim& value, std::string& output)
{
    switch (value)
    {
        case ColumnTypeRim::path:
            output = "Path";
            break;
        case ColumnTypeRim::size:
            output = "Size";
            break;
        case ColumnTypeRim::date:
            output = "Date";
            break;
        case ColumnTypeRim::extension:
            output = "Ext";
            break;
    }
}

template <> inline
bool readText(const std::string& input, ColumnTypeRim& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Path")
        value = ColumnTypeRim::path;
    else if (tmp == "Size")
        value = ColumnTypeRim::size;
    else if (tmp == "Date")
        value = ColumnTypeRim::date;
    else if (tmp == "Ext")
        value = ColumnTypeRim::extension;
    else
        return false;
    return true;
}


template <> inline
void writeText(const ItemPathFormat& value, std::string& output)
{
    switch (value)
    {
        case ItemPathFormat::name:
            output = "Item";
            break;
        case ItemPathFormat::relative:
            output = "Relative";
            break;
        case ItemPathFormat::full:
            output = "Full";
            break;
    }
}

template <> inline
bool readText(const std::string& input, ItemPathFormat& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Item")
        value = ItemPathFormat::name;
    else if (tmp == "Relative")
        value = ItemPathFormat::relative;
    else if (tmp == "Full")
        value = ItemPathFormat::full;
    else
        return false;
    return true;
}

template <> inline
void writeText(const ColumnTypeCfg& value, std::string& output)
{
    switch (value)
    {
        case ColumnTypeCfg::name:
            output = "Name";
            break;
        case ColumnTypeCfg::lastSync:
            output = "Last";
            break;
        case ColumnTypeCfg::lastLog:
            output = "Log";
            break;
    }
}

template <> inline
bool readText(const std::string& input, ColumnTypeCfg& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Name")
        value = ColumnTypeCfg::name;
    else if (tmp == "Last")
        value = ColumnTypeCfg::lastSync;
    else if (tmp == "Log")
        value = ColumnTypeCfg::lastLog;
    else
        return false;
    return true;
}


template <> inline
void writeText(const ColumnTypeOverview& value, std::string& output)
{
    switch (value)
    {
        case ColumnTypeOverview::folder:
            output = "Tree";
            break;
        case ColumnTypeOverview::itemCount:
            output = "Count";
            break;
        case ColumnTypeOverview::bytes:
            output = "Bytes";
            break;
    }
}

template <> inline
bool readText(const std::string& input, ColumnTypeOverview& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Tree")
        value = ColumnTypeOverview::folder;
    else if (tmp == "Count")
        value = ColumnTypeOverview::itemCount;
    else if (tmp == "Bytes")
        value = ColumnTypeOverview::bytes;
    else
        return false;
    return true;
}


template <> inline
void writeText(const UnitSize& value, std::string& output)
{
    switch (value)
    {
        case UnitSize::none:
            output = "None";
            break;
        case UnitSize::byte:
            output = "Byte";
            break;
        case UnitSize::kb:
            output = "KB";
            break;
        case UnitSize::mb:
            output = "MB";
            break;
    }
}

template <> inline
bool readText(const std::string& input, UnitSize& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "None")
        value = UnitSize::none;
    else if (tmp == "Byte")
        value = UnitSize::byte;
    else if (tmp == "KB")
        value = UnitSize::kb;
    else if (tmp == "MB")
        value = UnitSize::mb;
    else
        return false;
    return true;
}

template <> inline
void writeText(const UnitTime& value, std::string& output)
{
    switch (value)
    {
        case UnitTime::none:
            output = "None";
            break;
        case UnitTime::today:
            output = "Today";
            break;
        case UnitTime::thisMonth:
            output = "Month";
            break;
        case UnitTime::thisYear:
            output = "Year";
            break;
        case UnitTime::lastDays:
            output = "x-days";
            break;
    }
}

template <> inline
bool readText(const std::string& input, UnitTime& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "None")
        value = UnitTime::none;
    else if (tmp == "Today")
        value = UnitTime::today;
    else if (tmp == "Month")
        value = UnitTime::thisMonth;
    else if (tmp == "Year")
        value = UnitTime::thisYear;
    else if (tmp == "x-days")
        value = UnitTime::lastDays;
    else
        return false;
    return true;
}


template <> inline
void writeText(const LogFileFormat& value, std::string& output)
{
    switch (value)
    {
        case LogFileFormat::html:
            output = "HTML";
            break;
        case LogFileFormat::text:
            output = "Text";
            break;
    }
}

template <> inline
bool readText(const std::string& input, LogFileFormat& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "HTML")
        value = LogFileFormat::html;
    else if (tmp == "Text")
        value = LogFileFormat::text;
    else
        return false;
    return true;
}


template <> inline
void writeText(const VersioningStyle& value, std::string& output)
{
    switch (value)
    {
        case VersioningStyle::replace:
            output = "Replace";
            break;
        case VersioningStyle::timestampFolder:
            output = "TimeStamp-Folder";
            break;
        case VersioningStyle::timestampFile:
            output = "TimeStamp-File";
            break;
    }
}

template <> inline
bool readText(const std::string& input, VersioningStyle& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Replace")
        value = VersioningStyle::replace;
    else if (tmp == "TimeStamp-Folder")
        value = VersioningStyle::timestampFolder;
    else if (tmp == "TimeStamp-File")
        value = VersioningStyle::timestampFile;
    else
        return false;
    return true;
}


template <> inline
void writeText(const SyncVariant& value, std::string& output)
{
    switch (value)
    {
        case SyncVariant::twoWay:
            output = "TwoWay";
            break;
        case SyncVariant::mirror:
            output = "Mirror";
            break;
        case SyncVariant::update:
            output = "Update";
            break;
        case SyncVariant::custom:
            output = "Custom";
            break;
    }
}

template <> inline
bool readText(const std::string& input, SyncVariant& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "TwoWay")
        value = SyncVariant::twoWay;
    else if (tmp == "Mirror")
        value = SyncVariant::mirror;
    else if (tmp == "Update")
        value = SyncVariant::update;
    else if (tmp == "Custom")
        value = SyncVariant::custom;
    else
        return false;
    return true;
}


template <> inline
void writeStruc(const ColAttributesRim& value, XmlElement& output)
{
    output.setAttribute("Type",    value.type);
    output.setAttribute("Visible", value.visible);
    output.setAttribute("Width",   value.offset);
    output.setAttribute("Stretch", value.stretch);
}

template <> inline
bool readStruc(const XmlElement& input, ColAttributesRim& value)
{
    bool success = true;
    success = input.getAttribute("Type",    value.type)    && success;
    success = input.getAttribute("Visible", value.visible) && success;
    success = input.getAttribute("Width",   value.offset)  && success; //offset == width if stretch is 0
    success = input.getAttribute("Stretch", value.stretch) && success;
    return success; //[!] avoid short-circuit evaluation
}


template <> inline
void writeStruc(const ColAttributesCfg& value, XmlElement& output)
{
    output.setAttribute("Type",    value.type);
    output.setAttribute("Visible", value.visible);
    output.setAttribute("Width",   value.offset);
    output.setAttribute("Stretch", value.stretch);
}

template <> inline
bool readStruc(const XmlElement& input, ColAttributesCfg& value)
{
    bool success = true;
    success = input.getAttribute("Type",    value.type)    && success;
    success = input.getAttribute("Visible", value.visible) && success;
    success = input.getAttribute("Width",   value.offset)  && success; //offset == width if stretch is 0
    success = input.getAttribute("Stretch", value.stretch) && success;
    return success; //[!] avoid short-circuit evaluation
}


template <> inline
void writeStruc(const ColumnAttribOverview& value, XmlElement& output)
{
    output.setAttribute("Type",    value.type);
    output.setAttribute("Visible", value.visible);
    output.setAttribute("Width",   value.offset);
    output.setAttribute("Stretch", value.stretch);
}

template <> inline
bool readStruc(const XmlElement& input, ColumnAttribOverview& value)
{
    bool success = true;
    success = input.getAttribute("Type",    value.type)    && success;
    success = input.getAttribute("Visible", value.visible) && success;
    success = input.getAttribute("Width",   value.offset)  && success; //offset == width if stretch is 0
    success = input.getAttribute("Stretch", value.stretch) && success;
    return success; //[!] avoid short-circuit evaluation
}


template <> inline
void writeStruc(const ExternalApp& value, XmlElement& output)
{
    output.setValue(value.cmdLine);
    output.setAttribute("Label", value.description);
}

template <> inline
bool readStruc(const XmlElement& input, ExternalApp& value)
{
    const bool rv1 = input.getValue(value.cmdLine);
    const bool rv2 = input.getAttribute("Label", value.description);
    return rv1 && rv2;
}


template <> inline
void writeText(const SyncResult& value, std::string& output)
{
    switch (value)
    {
        case SyncResult::finishedSuccess:
            output = "Success";
            break;
        case SyncResult::finishedWarning:
            output = "Warning";
            break;
        case SyncResult::finishedError:
            output = "Error";
            break;
        case SyncResult::aborted:
            output = "Stopped";
            break;
    }
}

template <> inline
bool readText(const std::string& input, SyncResult& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Success")
        value = SyncResult::finishedSuccess;
    else if (tmp == "Warning")
        value = SyncResult::finishedWarning;
    else if (tmp == "Error")
        value = SyncResult::finishedError;
    else if (tmp == "Stopped")
        value = SyncResult::aborted;
    else
        return false;
    return true;
}
}


namespace
{


Zstring substituteFreeFileSyncDriveLetter(const Zstring& cfgFilePath)
{
    return cfgFilePath;
}

Zstring resolveFreeFileSyncDriveMacro(const Zstring& cfgFilePhrase)
{
    return cfgFilePhrase;
}


Zstring substituteFfsResourcePath(const Zstring& filePath)
{
    const Zstring resPathPf = getResourceDirPf();
    if (startsWith(trimCpy(filePath, true, false), resPathPf))
        return Zstring(Zstr("%ffs_resource%")) + FILE_NAME_SEPARATOR + afterFirst(filePath, resPathPf, IfNotFoundReturn::none);
    return filePath;
}

Zstring resolveFfsResourceMacro(const Zstring& filePhrase)
{
    if (startsWith(trimCpy(filePhrase, true, false), Zstring(Zstr("%ffs_resource%")) + FILE_NAME_SEPARATOR))
        return getResourceDirPf() + afterFirst(filePhrase, FILE_NAME_SEPARATOR, IfNotFoundReturn::none);
    return filePhrase;
}
}


namespace zen
{
template <> inline
bool readStruc(const XmlElement& input, ConfigFileItem& value)
{
    bool success = true;
    success = input.getAttribute("Result",  value.logResult) && success;

    Zstring cfgPathRaw;
    if (input.hasAttribute("CfgPath")) //TODO: remove after migration! 2020-02-09
        success = input.getAttribute("CfgPath", cfgPathRaw) && success; //
    else
        success = input.getAttribute("Config", cfgPathRaw) && success;

    //FFS portable: use special syntax for config file paths: e.g. "FFS:\SyncJob.ffs_gui"
    value.cfgFilePath = resolveFreeFileSyncDriveMacro(cfgPathRaw);

    success = input.getAttribute("LastSync", value.lastSyncTime) && success;

    Zstring logPathPhrase;
    if (input.hasAttribute("LogPath")) //TODO: remove after migration! 2020-02-09
        success = input.getAttribute("LogPath", logPathPhrase) && success; //
    else
        success = input.getAttribute("Log", logPathPhrase) && success;

    value.logFilePath = createAbstractPath(resolveFreeFileSyncDriveMacro(logPathPhrase));

    std::string hexColor; //optional XML attribute!
    if (input.getAttribute("Color", hexColor) && hexColor.size() == 6)
        value.backColor.Set(unhexify(hexColor[0], hexColor[1]),
                            unhexify(hexColor[2], hexColor[3]),
                            unhexify(hexColor[4], hexColor[5]));
    return success; //[!] avoid short-circuit evaluation
}

template <> inline
void writeStruc(const ConfigFileItem& value, XmlElement& output)
{
    output.setAttribute("Result", value.logResult);
    output.setAttribute("Config", substituteFreeFileSyncDriveLetter(value.cfgFilePath));
    output.setAttribute("LastSync", value.lastSyncTime);

    if (const Zstring& nativePath = getNativeItemPath(value.logFilePath);
        !nativePath.empty())
        output.setAttribute("Log", substituteFreeFileSyncDriveLetter(nativePath));
    else
        output.setAttribute("Log", AFS::getInitPathPhrase(value.logFilePath));

    if (value.backColor.IsOk())
    {
        const auto& [highR, lowR] = hexify(value.backColor.Red  ());
        const auto& [highG, lowG] = hexify(value.backColor.Green());
        const auto& [highB, lowB] = hexify(value.backColor.Blue ());
        output.setAttribute("Color", std::string({highR, lowR, highG, lowG, highB, lowB}));
    }
}

//TODO: remove after migration! 2018-07-27
struct ConfigFileItemV9
{
    Zstring filePath;
    time_t lastSyncTime = 0;
};
template <> inline
bool readStruc(const XmlElement& input, ConfigFileItemV9& value)
{
    Zstring rawPath;
    const bool rv1 = input.getValue(rawPath);
    if (rv1) value.filePath = resolveFreeFileSyncDriveMacro(rawPath);

    const bool rv2 = input.getAttribute("LastSync", value.lastSyncTime);
    return rv1 && rv2;
}
}


namespace
{
void readConfig(const XmlIn& in, CompConfig& cmpCfg)
{
    in["Variant" ](cmpCfg.compareVar);
    in["Symlinks"](cmpCfg.handleSymlinks);

    std::wstring timeShiftPhrase;
    if (in["IgnoreTimeShift"](timeShiftPhrase))
        cmpCfg.ignoreTimeShiftMinutes = fromTimeShiftPhrase(timeShiftPhrase);
}


void readConfig(const XmlIn& in, SyncDirectionConfig& dirCfg)
{
    in["Variant"](dirCfg.var);

    if (dirCfg.var == SyncVariant::custom)
    {
        XmlIn inCustDir = in["CustomDirections"];
        inCustDir["LeftOnly"  ](dirCfg.custom.exLeftSideOnly);
        inCustDir["RightOnly" ](dirCfg.custom.exRightSideOnly);
        inCustDir["LeftNewer" ](dirCfg.custom.leftNewer);
        inCustDir["RightNewer"](dirCfg.custom.rightNewer);
        inCustDir["Different" ](dirCfg.custom.different);
        inCustDir["Conflict"  ](dirCfg.custom.conflict);
    }
    //else
    //    dirCfg.custom = DirectionSet();

    in["DetectMovedFiles"](dirCfg.detectMovedFiles);
}


void readConfig(const XmlIn& in, SyncConfig& syncCfg, std::map<AfsDevice, size_t>& deviceParallelOps, int formatVer)
{
    readConfig(in, syncCfg.directionCfg);

    in["DeletionPolicy"  ](syncCfg.handleDeletion);
    in["VersioningFolder"](syncCfg.versioningFolderPhrase);

    if (formatVer < 12) //TODO: remove if parameter migration after some time! 2018-06-21
    {
        std::string tmp;
        in["VersioningFolder"].attribute("Style", tmp);

        trim(tmp);
        if (tmp == "Replace")
            syncCfg.versioningStyle = VersioningStyle::replace;
        else if (tmp == "TimeStamp")
            syncCfg.versioningStyle = VersioningStyle::timestampFile;

        if (syncCfg.versioningStyle == VersioningStyle::replace)
        {
            if (endsWithAsciiNoCase(syncCfg.versioningFolderPhrase, "/%timestamp%") ||
                endsWithAsciiNoCase(syncCfg.versioningFolderPhrase, "\\%timestamp%"))
            {
                syncCfg.versioningFolderPhrase.resize(syncCfg.versioningFolderPhrase.size() - strLength(Zstr("/%timestamp%")));
                syncCfg.versioningStyle = VersioningStyle::timestampFolder;

                if (syncCfg.versioningFolderPhrase.size() == 2 && isAsciiAlpha(syncCfg.versioningFolderPhrase[0]) && syncCfg.versioningFolderPhrase[1] == Zstr(':'))
                    syncCfg.versioningFolderPhrase += Zstr('\\');
            }
        }
    }
    else
    {
        XmlIn verFolder = in["VersioningFolder"];

        size_t parallelOps = 1;
        if (verFolder.hasAttribute("Threads")) //*no error* if not available
            verFolder.attribute("Threads", parallelOps); //try to get attribute

        const size_t parallelOpsPrev = getDeviceParallelOps(deviceParallelOps, syncCfg.versioningFolderPhrase);
        /**/                           setDeviceParallelOps(deviceParallelOps, syncCfg.versioningFolderPhrase, std::max(parallelOps, parallelOpsPrev));

        in["VersioningFolder"].attribute("Style", syncCfg.versioningStyle);

        if (syncCfg.versioningStyle != VersioningStyle::replace)
        {
            if (verFolder.hasAttribute("MaxAge")) //try to get attributes if available => *no error* if not available
                verFolder.attribute("MaxAge", syncCfg.versionMaxAgeDays);

            //TODO: remove if clause after migration! 2018-07-12
            if (formatVer < 13)
            {
                if (verFolder.hasAttribute("CountMin"))
                    verFolder.attribute("CountMin", syncCfg.versionCountMin); // => *no error* if not available
                if (verFolder.hasAttribute("CountMax"))
                    verFolder.attribute("CountMax", syncCfg.versionCountMax); //
            }
            else
            {
                if (verFolder.hasAttribute("MinCount"))
                    verFolder.attribute("MinCount", syncCfg.versionCountMin); // => *no error* if not available
                if (verFolder.hasAttribute("MaxCount"))
                    verFolder.attribute("MaxCount", syncCfg.versionCountMax); //
            }
        }
    }
}


void readConfig(const XmlIn& in, FilterConfig& filter /*int formatVer? but which one; Filter is used by XmlGlobalSettings and XmlGuiConfig! :( */)
{
    std::vector<Zstring> tmpIn;
    if (in["Include"](tmpIn)) //else: keep default value
        filter.includeFilter = mergeFilterLines(tmpIn);

    std::vector<Zstring> tmpEx;
    if (in["Exclude"](tmpEx)) //else: keep default value
        filter.excludeFilter = mergeFilterLines(tmpEx);

    in["TimeSpan"](filter.timeSpan);
    in["TimeSpan"].attribute("Type", filter.unitTimeSpan);

    in["SizeMin"](filter.sizeMin);
    in["SizeMin"].attribute("Unit", filter.unitSizeMin);

    in["SizeMax"](filter.sizeMax);
    in["SizeMax"].attribute("Unit", filter.unitSizeMax);
}


void readConfig(const XmlIn& in, LocalPairConfig& lpc, std::map<AfsDevice, size_t>& deviceParallelOps, int formatVer)
{
    //read folder pairs
    in["Left" ](lpc.folderPathPhraseLeft);
    in["Right"](lpc.folderPathPhraseRight);

    size_t parallelOpsL = 1;
    size_t parallelOpsR = 1;

    //TODO: remove old parameter after migration! 2018-04-14
    if (formatVer < 11)
    {
        auto getParallelOps = [&](const Zstring& folderPathPhrase, size_t& parallelOps)
        {
            if (startsWithAsciiNoCase(folderPathPhrase, "sftp:") ||
                startsWithAsciiNoCase(folderPathPhrase,  "ftp:"))
            {
                for (const Zstring& optPhrase : split(folderPathPhrase, Zstr('|'), SplitOnEmpty::skip))
                    if (startsWith(optPhrase, Zstr("con=")))
                        parallelOps = stringTo<int>(afterFirst(optPhrase, Zstr("con="), IfNotFoundReturn::none));
            }
        };
        getParallelOps(lpc.folderPathPhraseLeft,  parallelOpsL);
        getParallelOps(lpc.folderPathPhraseRight, parallelOpsR);
    }
    else
    {
        if (in["Left" ].hasAttribute("Threads")) in["Left" ].attribute("Threads", parallelOpsL); //try to get attributes:
        if (in["Right"].hasAttribute("Threads")) in["Right"].attribute("Threads", parallelOpsR); // => *no error* if not available
    }

    auto setParallelOps = [&](const Zstring& folderPathPhrase, size_t parallelOps)
    {
        const size_t parallelOpsPrev = getDeviceParallelOps(deviceParallelOps, folderPathPhrase);
        /**/                           setDeviceParallelOps(deviceParallelOps, folderPathPhrase, std::max(parallelOps, parallelOpsPrev));
    };
    setParallelOps(lpc.folderPathPhraseLeft,  parallelOpsL);
    setParallelOps(lpc.folderPathPhraseRight, parallelOpsR);

    //TODO: remove after migration - 2016-07-24
    auto ciReplace = [](Zstring& pathPhrase, const Zstring& oldTerm, const Zstring& newTerm) { pathPhrase = replaceCpyAsciiNoCase(pathPhrase, oldTerm, newTerm); };
    ciReplace(lpc.folderPathPhraseLeft,  Zstr("%csidl_MyDocuments%"), Zstr("%csidl_Documents%"));
    ciReplace(lpc.folderPathPhraseLeft,  Zstr("%csidl_MyMusic%"    ), Zstr("%csidl_Music%"));
    ciReplace(lpc.folderPathPhraseLeft,  Zstr("%csidl_MyPictures%" ), Zstr("%csidl_Pictures%"));
    ciReplace(lpc.folderPathPhraseLeft,  Zstr("%csidl_MyVideos%"   ), Zstr("%csidl_Videos%"));
    ciReplace(lpc.folderPathPhraseRight, Zstr("%csidl_MyDocuments%"), Zstr("%csidl_Documents%"));
    ciReplace(lpc.folderPathPhraseRight, Zstr("%csidl_MyMusic%"    ), Zstr("%csidl_Music%"));
    ciReplace(lpc.folderPathPhraseRight, Zstr("%csidl_MyPictures%" ), Zstr("%csidl_Pictures%"));
    ciReplace(lpc.folderPathPhraseRight, Zstr("%csidl_MyVideos%"   ), Zstr("%csidl_Videos%"));

    //TODO: remove after migration 2016-09-27
    if (formatVer < 6) //the-base64-encoded password is now stored as an option at the string end
    {
        //sftp://username:[base64]c2VjcmV0c@private.example.com ->
        //sftp://username@private.example.com|pass64=c2VjcmV0c
        auto updateSftpSyntax = [](Zstring& pathPhrase)
        {
            const size_t pos = pathPhrase.find(Zstr(":[base64]"));
            if (pos != Zstring::npos)
            {
                const size_t posEnd = pathPhrase.find(Zstr("@"), pos);
                if (posEnd != Zstring::npos)
                    pathPhrase = Zstring(pathPhrase.begin(), pathPhrase.begin() + pos) + (pathPhrase.c_str() + posEnd) +
                                 Zstr("|pass64=") + Zstring(pathPhrase.begin() + pos + strLength(Zstr(":[base64]")), pathPhrase.begin() + posEnd);
            }
        };
        updateSftpSyntax(lpc.folderPathPhraseLeft);
        updateSftpSyntax(lpc.folderPathPhraseRight);
    }

    //TODO: remove after migration! 2020-04-24
    if (formatVer < 16)
    {
        lpc.folderPathPhraseLeft  = replaceCpyAsciiNoCase(lpc.folderPathPhraseLeft,  Zstr("%weekday%"), Zstr("%WeekDayName%"));
        lpc.folderPathPhraseRight = replaceCpyAsciiNoCase(lpc.folderPathPhraseRight, Zstr("%weekday%"), Zstr("%WeekDayName%"));
    }

    //###########################################################
    //alternate comp configuration (optional)
    if (XmlIn inLocalCmp = in[formatVer < 10 ? "CompareConfig" : "Compare"]) //TODO: remove if parameter migration after some time! 2018-02-25
    {
        CompConfig cmpCfg;
        readConfig(inLocalCmp, cmpCfg);

        lpc.localCmpCfg = cmpCfg;
    }
    //###########################################################
    //alternate sync configuration (optional)
    if (XmlIn inLocalSync = in[formatVer < 10 ? "SyncConfig" : "Synchronize"]) //TODO: remove if parameter migration after some time! 2018-02-25
    {
        SyncConfig syncCfg;
        readConfig(inLocalSync, syncCfg, deviceParallelOps, formatVer);

        lpc.localSyncCfg = syncCfg;
    }

    //###########################################################
    //alternate filter configuration
    if (XmlIn inLocFilter = in[formatVer < 10 ? "LocalFilter" : "Filter"]) //TODO: remove if parameter migration after some time! 2018-02-25
        readConfig(inLocFilter, lpc.localFilter);
}


void readConfig(const XmlIn& in, MainConfiguration& mainCfg, int formatVer)
{
    XmlIn inMain = formatVer < 10 ? in["MainConfig"] : in; //TODO: remove if parameter migration after some time! 2018-02-25

    if (formatVer < 10) //TODO: remove if parameter migration after some time! 2018-02-25
        readConfig(inMain["Comparison"], mainCfg.cmpCfg);
    else
        readConfig(inMain["Compare"], mainCfg.cmpCfg);
    //###########################################################

    //read sync configuration
    if (formatVer < 10) //TODO: remove if parameter migration after some time! 2018-02-25
        readConfig(inMain["SyncConfig"], mainCfg.syncCfg, mainCfg.deviceParallelOps, formatVer);
    else
        readConfig(inMain["Synchronize"], mainCfg.syncCfg, mainCfg.deviceParallelOps, formatVer);

    //###########################################################

    //read filter settings
    if (formatVer < 10) //TODO: remove if parameter migration after some time! 2018-02-25
        readConfig(inMain["GlobalFilter"], mainCfg.globalFilter);
    else
        readConfig(inMain["Filter"], mainCfg.globalFilter);

    //###########################################################
    //read folder pairs
    bool firstItem = true;
    for (XmlIn inPair = inMain["FolderPairs"]["Pair"]; inPair; inPair.next())
    {
        LocalPairConfig lpc;
        readConfig(inPair, lpc, mainCfg.deviceParallelOps, formatVer);

        if (firstItem)
        {
            firstItem = false;
            mainCfg.firstPair = lpc;
            mainCfg.additionalPairs.clear();
        }
        else
            mainCfg.additionalPairs.push_back(lpc);
    }

    //TODO: remove if parameter migration after some time! 2017-10-24
    if (formatVer < 8)
        ;
    else
        //TODO: remove if parameter migration after some time! 2018-02-24
        if (formatVer < 10)
            inMain["IgnoreErrors"](mainCfg.ignoreErrors);
        else
        {
            inMain["Errors"].attribute("Ignore", mainCfg.ignoreErrors);
            inMain["Errors"].attribute("Retry",  mainCfg.autoRetryCount);
            inMain["Errors"].attribute("Delay",  mainCfg.autoRetryDelay);
        }

    //TODO: remove if parameter migration after some time! 2017-10-24
    if (formatVer < 8)
        inMain["OnCompletion"](mainCfg.postSyncCommand);
    else
    {
        inMain["PostSyncCommand"](mainCfg.postSyncCommand);
        inMain["PostSyncCommand"].attribute("Condition", mainCfg.postSyncCondition);
    }

    //TODO: remove if parameter migration after some time! 2018-08-13
    if (formatVer < 14)
        ; //path will be extracted from BatchExclusiveConfig
    else
        inMain["LogFolder"](mainCfg.altLogFolderPathPhrase);

    //TODO: remove after migration! 2020-04-24
    if (formatVer < 16)
        mainCfg.altLogFolderPathPhrase= replaceCpyAsciiNoCase(mainCfg.altLogFolderPathPhrase,  Zstr("%weekday%"), Zstr("%WeekDayName%"));

    //TODO: remove if parameter migration after some time! 2020-01-30
    if (formatVer < 15)
        ;
    else
    {
        inMain["EmailNotification"](mainCfg.emailNotifyAddress);
        inMain["EmailNotification"].attribute("Condition", mainCfg.emailNotifyCondition);
    }
}


void readConfig(const XmlIn& in, XmlGuiConfig& cfg, int formatVer)
{
    //read main config
    readConfig(in, cfg.mainCfg, formatVer);

    //read GUI specific config data
    XmlIn inGuiCfg = in[formatVer < 10 ? "GuiConfig" : "Gui"]; //TODO: remove if parameter migration after some time! 2018-02-25

    //TODO: remove after migration! 2020-10-14
    if (formatVer < 17)
    {
        if (inGuiCfg["MiddleGridView"])
        {
            std::string tmp;
            inGuiCfg["MiddleGridView"](tmp);

            if (tmp == "Category")
                cfg.gridViewType = GridViewType::difference;
            else if (tmp == "Action")
                cfg.gridViewType = GridViewType::action;
        }
    }
    else
        inGuiCfg["GridViewType"](cfg.gridViewType);

    //TODO: remove if clause after migration! 2017-10-24
    if (formatVer < 8)
    {
        std::string str;
        if (inGuiCfg["HandleError"](str))
            cfg.mainCfg.ignoreErrors = str == "Ignore";

        str = trimCpy(utfTo<std::string>(cfg.mainCfg.postSyncCommand));
        if (equalAsciiNoCase(str, "Close progress dialog"))
            cfg.mainCfg.postSyncCommand.clear();
    }
}


void readConfig(const XmlIn& in, BatchExclusiveConfig& cfg, int formatVer)
{
    XmlIn inBatchCfg = in[formatVer < 10 ? "BatchConfig" : "Batch"]; //TODO: remove if parameter migration after some time! 2018-02-25

    //TODO: remove if clause after migration! 2018-02-01
    if (formatVer < 9)
        inBatchCfg["RunMinimized"](cfg.runMinimized);
    else
        inBatchCfg["ProgressDialog"].attribute("Minimized", cfg.runMinimized);

    //TODO: remove if clause after migration! 2018-02-01
    if (formatVer < 9)
        ; //n/a
    else
        inBatchCfg["ProgressDialog"].attribute("AutoClose", cfg.autoCloseSummary);

    //TODO: remove if clause after migration! 2017-10-24
    if (formatVer < 8)
    {
        std::string str;
        if (inBatchCfg["HandleError"](str))
            cfg.batchErrorHandling = str == "Stop" ? BatchErrorHandling::cancel : BatchErrorHandling::showPopup;
    }
    else
        inBatchCfg["ErrorDialog"](cfg.batchErrorHandling);

    //TODO: remove if clause after migration! 2017-10-24
    if (formatVer < 8)
        ; //n/a
    //TODO: remove if clause after migration! 2018-02-01
    else if (formatVer == 8)
    {
        std::string tmp;
        if (inBatchCfg["PostSyncAction"](tmp))
        {
            tmp = trimCpy(tmp);
            if (tmp == "Summary")
                cfg.postSyncAction = PostSyncAction::none;
            else if (tmp == "Exit")
                cfg.autoCloseSummary = true;
            else if (tmp == "Sleep")
                cfg.postSyncAction = PostSyncAction::sleep;
            else if (tmp == "Shutdown")
                cfg.postSyncAction = PostSyncAction::shutdown;
        }
    }
    else
        inBatchCfg["PostSyncAction"](cfg.postSyncAction);
}


void readConfig(const XmlIn& in, XmlBatchConfig& cfg, int formatVer)
{
    readConfig(in, cfg.mainCfg,    formatVer);
    readConfig(in, cfg.batchExCfg, formatVer);

    //TODO: remove if clause after migration! 2018-08-13
    if (formatVer < 14)
    {
        XmlIn inBatchCfg = in[formatVer < 10 ? "BatchConfig" : "Batch"];
        inBatchCfg["LogfileFolder"](cfg.mainCfg.altLogFolderPathPhrase);
    }

    //TODO: remove if clause after migration! 2017-10-24
    if (formatVer < 8)
    {
        std::string str;
        if (in["BatchConfig"]["HandleError"](str))
            cfg.mainCfg.ignoreErrors = str == "Ignore";

        str = trimCpy(utfTo<std::string>(cfg.mainCfg.postSyncCommand));
        if (equalAsciiNoCase(str, "Close progress dialog"))
        {
            cfg.batchExCfg.autoCloseSummary = true;
            cfg.mainCfg.postSyncCommand.clear();
        }
        else if (str == "rundll32.exe powrprof.dll,SetSuspendState Sleep" ||
                 str == "rundll32.exe powrprof.dll,SetSuspendState" ||
                 str == "systemctl suspend" ||
                 str == "osascript -e 'tell application \"System Events\" to sleep'")
        {
            cfg.batchExCfg.postSyncAction = PostSyncAction::sleep;
            cfg.mainCfg.postSyncCommand.clear();
        }
        else if (str == "shutdown /s /t 60"  ||
                 str == "shutdown -s -t 60"  ||
                 str == "systemctl poweroff" ||
                 str == "osascript -e 'tell application \"System Events\" to shut down'")
        {
            cfg.batchExCfg.postSyncAction = PostSyncAction::shutdown;
            cfg.mainCfg.postSyncCommand.clear();
        }
        else if (cfg.batchExCfg.runMinimized)
            cfg.batchExCfg.autoCloseSummary = true;
    }
}


void readConfig(const XmlIn& in, XmlGlobalSettings& cfg, int formatVer)
{
    assert(cfg.dpiLayouts.empty());

    XmlIn in2 = in;

    if (in["Shared"]) //TODO: remove old parameter after migration! 2016-01-18
        in2 = in["Shared"];
    else if (in["General"]) //TODO: remove old parameter after migration! 2020-12-03
        in2 = in["General"];

    in2["Language"].attribute("Name", cfg.programLanguage);

    in2["FailSafeFileCopy"         ].attribute("Enabled", cfg.failSafeFileCopy);
    in2["CopyLockedFiles"          ].attribute("Enabled", cfg.copyLockedFiles);
    in2["CopyFilePermissions"      ].attribute("Enabled", cfg.copyFilePermissions);
    in2["FileTimeTolerance"        ].attribute("Seconds", cfg.fileTimeTolerance);
    in2["RunWithBackgroundPriority"].attribute("Enabled", cfg.runWithBackgroundPriority);
    in2["LockDirectoriesDuringSync"].attribute("Enabled", cfg.createLockFile);
    in2["VerifyCopiedFiles"        ].attribute("Enabled", cfg.verifyFileCopy);
    in2["LogFiles"                 ].attribute("MaxAge",  cfg.logfilesMaxAgeDays);
    in2["LogFiles"                 ].attribute("Format",  cfg.logFormat);

    //TODO: remove old parameter after migration! 2021-03-06
    if (formatVer < 21)
    {
        in2["ProgressDialog"].attribute("Width",     cfg.dpiLayouts[getDpiScalePercent()].progressDlg.dlgSize.x);
        in2["ProgressDialog"].attribute("Height",    cfg.dpiLayouts[getDpiScalePercent()].progressDlg.dlgSize.y);
        in2["ProgressDialog"].attribute("Maximized", cfg.dpiLayouts[getDpiScalePercent()].progressDlg.isMaximized);
    }

    in2["ProgressDialog"].attribute("AutoClose", cfg.progressDlgAutoClose);

    //TODO: remove if parameter migration after some time! 2018-08-13
    if (formatVer < 14)
        if (cfg.logfilesMaxAgeDays == 14) //default value was too small
            cfg.logfilesMaxAgeDays = XmlGlobalSettings().logfilesMaxAgeDays;

    //TODO: remove old parameter after migration! 2018-02-04
    if (formatVer < 8)
    {
        XmlIn inOpt = in2["OptionalDialogs"];
        inOpt["ConfirmStartSync"                ].attribute("Enabled", cfg.confirmDlgs.confirmSyncStart);
        inOpt["ConfirmSaveConfig"               ].attribute("Enabled", cfg.confirmDlgs.confirmSaveConfig);
        inOpt["ConfirmExternalCommandMassInvoke"].attribute("Enabled", cfg.confirmDlgs.confirmCommandMassInvoke);
        inOpt["WarnUnresolvedConflicts"         ].attribute("Enabled", cfg.warnDlgs.warnUnresolvedConflicts);
        inOpt["WarnNotEnoughDiskSpace"          ].attribute("Enabled", cfg.warnDlgs.warnNotEnoughDiskSpace);
        inOpt["WarnSignificantDifference"       ].attribute("Enabled", cfg.warnDlgs.warnSignificantDifference);
        inOpt["WarnRecycleBinNotAvailable"      ].attribute("Enabled", cfg.warnDlgs.warnRecyclerMissing);
        inOpt["WarnInputFieldEmpty"             ].attribute("Enabled", cfg.warnDlgs.warnInputFieldEmpty);
        inOpt["WarnModificationTimeError"       ].attribute("Enabled", cfg.warnDlgs.warnModificationTimeError);
        inOpt["WarnDependentFolderPair"         ].attribute("Enabled", cfg.warnDlgs.warnDependentFolderPair);
        inOpt["WarnDependentBaseFolders"        ].attribute("Enabled", cfg.warnDlgs.warnDependentBaseFolders);
        inOpt["WarnDirectoryLockFailed"         ].attribute("Enabled", cfg.warnDlgs.warnDirectoryLockFailed);
        inOpt["WarnVersioningFolderPartOfSync"  ].attribute("Enabled", cfg.warnDlgs.warnVersioningFolderPartOfSync);
    }
    else
    {
        XmlIn inOpt = in2["OptionalDialogs"];
        inOpt["ConfirmStartSync"                ].attribute("Show", cfg.confirmDlgs.confirmSyncStart);
        inOpt["ConfirmSaveConfig"               ].attribute("Show", cfg.confirmDlgs.confirmSaveConfig);
        if (formatVer < 12) //TODO: remove old parameter after migration! 2019-02-09
            inOpt["ConfirmExternalCommandMassInvoke"].attribute("Show", cfg.confirmDlgs.confirmCommandMassInvoke);
        else
            inOpt["ConfirmCommandMassInvoke"].attribute("Show", cfg.confirmDlgs.confirmCommandMassInvoke);
        inOpt["WarnFolderNotExisting"         ].attribute("Show", cfg.warnDlgs.warnFolderNotExisting);
        inOpt["WarnFoldersDifferInCase"       ].attribute("Show", cfg.warnDlgs.warnFoldersDifferInCase);
        inOpt["WarnUnresolvedConflicts"       ].attribute("Show", cfg.warnDlgs.warnUnresolvedConflicts);
        inOpt["WarnNotEnoughDiskSpace"        ].attribute("Show", cfg.warnDlgs.warnNotEnoughDiskSpace);
        inOpt["WarnSignificantDifference"     ].attribute("Show", cfg.warnDlgs.warnSignificantDifference);
        inOpt["WarnRecycleBinNotAvailable"    ].attribute("Show", cfg.warnDlgs.warnRecyclerMissing);
        inOpt["WarnInputFieldEmpty"           ].attribute("Show", cfg.warnDlgs.warnInputFieldEmpty);
        inOpt["WarnModificationTimeError"     ].attribute("Show", cfg.warnDlgs.warnModificationTimeError);
        inOpt["WarnDependentFolderPair"       ].attribute("Show", cfg.warnDlgs.warnDependentFolderPair);
        inOpt["WarnDependentBaseFolders"      ].attribute("Show", cfg.warnDlgs.warnDependentBaseFolders);
        inOpt["WarnDirectoryLockFailed"       ].attribute("Show", cfg.warnDlgs.warnDirectoryLockFailed);
        inOpt["WarnVersioningFolderPartOfSync"].attribute("Show", cfg.warnDlgs.warnVersioningFolderPartOfSync);
    }

    //TODO: remove after migration! 2021-12-02
    if (formatVer < 23)
    {
        in2["NotificationSound"].attribute("CompareFinished", cfg.soundFileCompareFinished);
        in2["NotificationSound"].attribute("SyncFinished",    cfg.soundFileSyncFinished);
    }
    else
    {
        in2["Sounds"]["CompareFinished"].attribute("Path", cfg.soundFileCompareFinished);
        in2["Sounds"]["SyncFinished"   ].attribute("Path", cfg.soundFileSyncFinished);
        in2["Sounds"]["AlertPending"   ].attribute("Path", cfg.soundFileAlertPending);
    }

    //TODO: remove if parameter migration after some time! 2019-05-29
    if (formatVer < 13)
    {
        if (!cfg.soundFileCompareFinished.empty()) cfg.soundFileCompareFinished = getResourceDirPf() + cfg.soundFileCompareFinished;
        if (!cfg.soundFileSyncFinished   .empty()) cfg.soundFileSyncFinished    = getResourceDirPf() + cfg.soundFileSyncFinished;
    }
    else
    {
        cfg.soundFileCompareFinished = resolveFfsResourceMacro(cfg.soundFileCompareFinished);
        cfg.soundFileSyncFinished    = resolveFfsResourceMacro(cfg.soundFileSyncFinished);
        cfg.soundFileAlertPending    = resolveFfsResourceMacro(cfg.soundFileAlertPending);
    }

    XmlIn inMainWin = in["MainDialog"];

    //TODO: remove old parameter after migration! 2020-12-03
    if (in["Gui"])
        inMainWin = in["Gui"]["MainDialog"];

    //TODO: remove old parameter after migration! 2021-03-06
    if (formatVer < 21)
    {
        inMainWin.attribute("Width",     cfg.dpiLayouts[getDpiScalePercent()].mainDlg.dlgSize.x);
        inMainWin.attribute("Height",    cfg.dpiLayouts[getDpiScalePercent()].mainDlg.dlgSize.y);
        inMainWin.attribute("PosX",      cfg.dpiLayouts[getDpiScalePercent()].mainDlg.dlgPos.x);
        inMainWin.attribute("PosY",      cfg.dpiLayouts[getDpiScalePercent()].mainDlg.dlgPos.y);
        inMainWin.attribute("Maximized", cfg.dpiLayouts[getDpiScalePercent()].mainDlg.isMaximized);
    }

    //###########################################################

    //TODO: remove old parameter after migration! 2018-02-04
    if (formatVer < 8)
        inMainWin["CaseSensitiveSearch"].attribute("Enabled", cfg.mainDlg.textSearchRespectCase);
    else
        //TODO: remove if parameter migration after some time! 2018-09-09
        if (formatVer < 11)
            inMainWin["Search"].attribute("CaseSensitive", cfg.mainDlg.textSearchRespectCase);
        else
            inMainWin["SearchPanel"].attribute("CaseSensitive", cfg.mainDlg.textSearchRespectCase);

    //TODO: remove if parameter migration after some time! 2018-09-09
    if (formatVer < 11)
        inMainWin["FolderPairsVisible" ].attribute("Max", cfg.mainDlg.folderPairsVisibleMax);

    //###########################################################

    XmlIn inConfig = inMainWin["ConfigPanel"];
    inConfig.attribute("ScrollPos",     cfg.mainDlg.config.topRowPos);
    inConfig.attribute("SyncOverdue",   cfg.mainDlg.config.syncOverdueDays);
    inConfig.attribute("SortByColumn",  cfg.mainDlg.config.lastSortColumn);
    inConfig.attribute("SortAscending", cfg.mainDlg.config.lastSortAscending);

    //TODO: remove old parameter after migration! 2021-03-06
    if (formatVer < 21)
        inConfig["Columns"](cfg.dpiLayouts[getDpiScalePercent()].configColumnAttribs);

    //TODO: remove after migration! 2018-07-27
    if (formatVer < 10) //reset once to show the new log column
        cfg.dpiLayouts[getDpiScalePercent()].configColumnAttribs = DpiLayout().configColumnAttribs;

    //TODO: remove parameter migration after some time! 2018-01-08
    if (formatVer < 6)
    {
        in["Gui"]["ConfigHistory"].attribute("MaxSize", cfg.mainDlg.config.histItemsMax);

        //TODO: remove parameter migration after some time! 2016-09-23
        if (formatVer < 4)
            cfg.mainDlg.config.histItemsMax = std::max<size_t>(cfg.mainDlg.config.histItemsMax, 100);

        std::vector<Zstring> cfgHist;
        in["Gui"]["ConfigHistory"](cfgHist);

        for (const Zstring& cfgPath : cfgHist)
            cfg.mainDlg.config.fileHistory.emplace_back(
                cfgPath,
                0, getNullPath(), SyncResult::finishedSuccess, wxNullColour);
    }
    //TODO: remove after migration! 2018-07-27
    else if (formatVer < 10)
    {
        inConfig["Configurations"].attribute("MaxSize", cfg.mainDlg.config.histItemsMax);

        std::vector<ConfigFileItemV9> cfgFileHistory;
        inConfig["Configurations"](cfgFileHistory);

        for (const ConfigFileItemV9& item : cfgFileHistory)
            cfg.mainDlg.config.fileHistory.emplace_back(item.filePath, item.lastSyncTime, getNullPath(), SyncResult::finishedSuccess, wxNullColour);
    }
    else
    {
        inConfig["Configurations"].attribute("MaxSize", cfg.mainDlg.config.histItemsMax);
        inConfig["Configurations"].attribute("LastSelected", cfg.mainDlg.config.lastSelectedFile);
        inConfig["Configurations"](cfg.mainDlg.config.fileHistory);
    }
    //TODO: remove after migration! 2019-11-30
    if (formatVer < 15)
    {
        const Zstring lastRunConfigPath = getConfigDirPathPf() + Zstr("LastRun.ffs_gui");
        for (ConfigFileItem& item : cfg.mainDlg.config.fileHistory)
            if (equalNativePath(item.cfgFilePath, lastRunConfigPath))
                item.backColor = wxColor(0xdd, 0xdd, 0xdd); //light grey from onCfgGridContext()
    }

    //TODO: remove parameter migration after some time! 2018-01-08
    if (formatVer < 6)
    {
        in["Gui"]["LastUsedConfig"](cfg.mainDlg.config.lastUsedFiles);
    }
    else
    {
        std::vector<Zstring> cfgPaths;
        if (inConfig["LastUsed"](cfgPaths))
        {
            for (Zstring& filePath : cfgPaths)
                filePath = resolveFreeFileSyncDriveMacro(filePath);

            cfg.mainDlg.config.lastUsedFiles = cfgPaths;
        }
    }

    //###########################################################

    XmlIn inOverview = inMainWin["OverviewPanel"];
    inOverview.attribute("ShowPercentage", cfg.mainDlg.overview.showPercentBar);
    inOverview.attribute("SortByColumn",   cfg.mainDlg.overview.lastSortColumn);
    inOverview.attribute("SortAscending",  cfg.mainDlg.overview.lastSortAscending);

    //TODO: remove old parameter after migration! 2021-03-06
    if (formatVer < 21)
        inOverview["Columns"](cfg.dpiLayouts[getDpiScalePercent()].overviewColumnAttribs);

    XmlIn inFileGrid = inMainWin["FilePanel"];
    //TODO: remove parameter migration after some time! 2018-01-08
    if (formatVer < 6)
        inFileGrid = inMainWin["CenterPanel"];

    //TODO: remove after migration! 2020-10-13
    if (formatVer < 19)
        ; //new icon layout => let user re-evaluate settings
    else
    {
        inFileGrid.attribute("ShowIcons",  cfg.mainDlg.showIcons);
        inFileGrid.attribute("IconSize",   cfg.mainDlg.iconSize);
    }
    inFileGrid.attribute("SashOffset", cfg.mainDlg.sashOffset);

    //TODO: remove if parameter migration after some time! 2018-09-09
    if (formatVer < 11)
        ;
    //TODO: remove if parameter migration after some time! 2020-01-30
    else if (formatVer < 16)
        inFileGrid.attribute("MaxFolderPairsShown", cfg.mainDlg.folderPairsVisibleMax);
    else
        inFileGrid.attribute("FolderPairsMax", cfg.mainDlg.folderPairsVisibleMax);

    //TODO: remove old parameter after migration! 2021-03-06
    if (formatVer < 21)
    {
        inFileGrid["ColumnsLeft"](cfg.dpiLayouts[getDpiScalePercent()].fileColumnAttribsLeft);
        inFileGrid["ColumnsRight"](cfg.dpiLayouts[getDpiScalePercent()].fileColumnAttribsRight);

        inFileGrid["ColumnsLeft"].attribute("PathFormat", cfg.mainDlg.itemPathFormatLeftGrid);
        inFileGrid["ColumnsRight"].attribute("PathFormat", cfg.mainDlg.itemPathFormatRightGrid);
    }
    else
    {
        inFileGrid.attribute("PathFormatLeft",  cfg.mainDlg.itemPathFormatLeftGrid);
        inFileGrid.attribute("PathFormatRight", cfg.mainDlg.itemPathFormatRightGrid);
    }

    inFileGrid["FolderHistoryLeft" ](cfg.mainDlg.folderHistoryLeft);
    inFileGrid["FolderHistoryRight"](cfg.mainDlg.folderHistoryRight);

    inFileGrid["FolderHistoryLeft" ].attribute("LastSelected", cfg.mainDlg.folderLastSelectedLeft);
    inFileGrid["FolderHistoryRight"].attribute("LastSelected", cfg.mainDlg.folderLastSelectedRight);

    //TODO: remove parameter migration after some time! 2018-01-08
    if (formatVer < 6)
    {
        in["Gui"]["FolderHistoryLeft" ](cfg.mainDlg.folderHistoryLeft);
        in["Gui"]["FolderHistoryRight"](cfg.mainDlg.folderHistoryRight);
    }

    //###########################################################
    XmlIn inCopyTo = inMainWin["ManualCopyTo"];
    inCopyTo.attribute("KeepRelativePaths", cfg.mainDlg.copyToCfg.keepRelPaths);
    inCopyTo.attribute("OverwriteIfExists", cfg.mainDlg.copyToCfg.overwriteIfExists);

    XmlIn inCopyToHistory = inCopyTo["FolderHistory"];
    inCopyToHistory(cfg.mainDlg.copyToCfg.folderHistory);
    inCopyToHistory.attribute("TargetFolder", cfg.mainDlg.copyToCfg.targetFolderPath);
    inCopyToHistory.attribute("LastSelected", cfg.mainDlg.copyToCfg.targetFolderLastSelected);
    //###########################################################

    XmlIn inDefFilter = inMainWin["DefaultViewFilter"];
    //TODO: remove old parameter after migration! 2018-02-04
    if (formatVer < 8)
        inDefFilter = inMainWin["DefaultViewFilter"]["Shared"];

    inDefFilter.attribute("Equal",    cfg.mainDlg.viewFilterDefault.equal);
    inDefFilter.attribute("Conflict", cfg.mainDlg.viewFilterDefault.conflict);
    inDefFilter.attribute("Excluded", cfg.mainDlg.viewFilterDefault.excluded);

    XmlIn diffView = inDefFilter["Difference"];
    //TODO: remove after migration! 2020-10-13
    if (formatVer < 19)
        diffView = inDefFilter["CategoryView"];

    diffView.attribute("LeftOnly",   cfg.mainDlg.viewFilterDefault.leftOnly);
    diffView.attribute("RightOnly",  cfg.mainDlg.viewFilterDefault.rightOnly);
    diffView.attribute("LeftNewer",  cfg.mainDlg.viewFilterDefault.leftNewer);
    diffView.attribute("RightNewer", cfg.mainDlg.viewFilterDefault.rightNewer);
    diffView.attribute("Different",  cfg.mainDlg.viewFilterDefault.different);

    XmlIn actView = inDefFilter["Action"];
    //TODO: remove after migration! 2020-10-13
    if (formatVer < 19)
        actView = inDefFilter["ActionView"];

    actView.attribute("CreateLeft",  cfg.mainDlg.viewFilterDefault.createLeft);
    actView.attribute("CreateRight", cfg.mainDlg.viewFilterDefault.createRight);
    actView.attribute("UpdateLeft",  cfg.mainDlg.viewFilterDefault.updateLeft);
    actView.attribute("UpdateRight", cfg.mainDlg.viewFilterDefault.updateRight);
    actView.attribute("DeleteLeft",  cfg.mainDlg.viewFilterDefault.deleteLeft);
    actView.attribute("DeleteRight", cfg.mainDlg.viewFilterDefault.deleteRight);
    actView.attribute("DoNothing",   cfg.mainDlg.viewFilterDefault.doNothing);


    //TODO: remove old parameter after migration! 2018-01-16
    if (formatVer < 7)
        inMainWin["Perspective5"](cfg.dpiLayouts[getDpiScalePercent()].mainDlg.panelLayout);
    //TODO: remove old parameter after migration! 2021-03-06
    else if (formatVer < 21)
        inMainWin["Perspective"](cfg.dpiLayouts[getDpiScalePercent()].mainDlg.panelLayout);

    //TODO: remove after migration! 2019-11-30
    auto splitEditMerge = [](wxString& perspective, wchar_t delim, const std::function<void(wxString& item)>& editItem)
    {
        std::vector<wxString> v = split(perspective, delim, SplitOnEmpty::allow);
        assert(!v.empty());
        perspective.clear();

        std::for_each(v.begin(), v.end() - 1, [&](wxString& item)
        {
            editItem(item);
            perspective += item;
            perspective += delim;
        });
        editItem(v.back());
        perspective += v.back();
    };

    //TODO: remove after migration! 2018-07-27
    if (formatVer < 10)
        splitEditMerge(cfg.dpiLayouts[getDpiScalePercent()].mainDlg.panelLayout, L'|', [&](wxString& paneCfg)
    {
        if (contains(paneCfg, L"name=TopPanel"))
            replace(paneCfg, L";row=2;", L";row=3;");
    });

    //TODO: remove after migration! 2019-11-30
    if (formatVer < 15)
    {
        //set minimal TopPanel height => search and set actual height to 0 and let MainDialog's min-size handling kick in:
        std::optional<int> tpDir;
        std::optional<int> tpLayer;
        std::optional<int> tpRow;
        splitEditMerge(cfg.dpiLayouts[getDpiScalePercent()].mainDlg.panelLayout, L'|', [&](wxString& paneCfg)
        {
            if (contains(paneCfg, L"name=TopPanel"))
                splitEditMerge(paneCfg, L';', [&](wxString& paneAttr)
            {
                if (startsWith(paneAttr, L"dir="))
                    tpDir = stringTo<int>(afterFirst(paneAttr, L'=', IfNotFoundReturn::none));
                else if (startsWith(paneAttr, L"layer="))
                    tpLayer = stringTo<int>(afterFirst(paneAttr, L'=', IfNotFoundReturn::none));
                else if (startsWith(paneAttr, L"row="))
                    tpRow = stringTo<int>(afterFirst(paneAttr, L'=', IfNotFoundReturn::none));
            });
        });

        if (tpDir && tpLayer && tpRow)
        {
            const wxString tpSize = L"dock_size(" +
                                    numberTo<wxString>(*tpDir  ) + L"," +
                                    numberTo<wxString>(*tpLayer) + L"," +
                                    numberTo<wxString>(*tpRow  ) + L")=";

            splitEditMerge(cfg.dpiLayouts[getDpiScalePercent()].mainDlg.panelLayout, L'|', [&](wxString& paneCfg)
            {
                if (startsWith(paneCfg, tpSize))
                    paneCfg = tpSize + L"0";
            });
        }
    }

    //TODO: remove if parameter migration after some time! 2020-01-30
    if (formatVer < 16)
        ;
    else if (formatVer < 20) //TODO: remove old parameter after migration! 2020-12-03
        in["Gui"]["FolderHistory" ].attribute("MaxSize", cfg.folderHistoryMax);
    else
        in["FolderHistory" ].attribute("MaxSize", cfg.folderHistoryMax);

    if (formatVer < 20) //TODO: remove old parameter after migration! 2020-12-03
    {
        in["Gui"]["SftpKeyFile"].attribute("LastSelected", cfg.sftpKeyFileLastSelected);
    }
    else
    {
        in["SftpKeyFile"].attribute("LastSelected", cfg.sftpKeyFileLastSelected);
    }

    if (formatVer < 22) //TODO: remove old parameter after migration! 2021-07-31
    {
    }
    else
        readConfig(in["DefaultFilter"], cfg.defaultFilter);

    if (formatVer < 20) //TODO: remove old parameter after migration! 2020-12-03
    {
        in["Gui"]["VersioningFolderHistory"](cfg.versioningFolderHistory);
        in["Gui"]["VersioningFolderHistory"].attribute("LastSelected", cfg.versioningFolderLastSelected);
    }
    else
    {
        in["VersioningFolderHistory"](cfg.versioningFolderHistory);
        in["VersioningFolderHistory"].attribute("LastSelected", cfg.versioningFolderLastSelected);
    }

    if (formatVer < 20) //TODO: remove old parameter after migration! 2020-12-03
    {
        in["Gui"]["LogFolderHistory"](cfg.logFolderHistory);
        in["Gui"]["LogFolderHistory"].attribute("LastSelected", cfg.logFolderLastSelected);
    }
    else
    {
        in["LogFolderHistory"](cfg.logFolderHistory);
        in["LogFolderHistory"].attribute("LastSelected", cfg.logFolderLastSelected);
    }

    if (formatVer < 20) //TODO: remove old parameter after migration! 2020-12-03
    {
        in["Gui"]["EmailHistory"](cfg.emailHistory);
        in["Gui"]["EmailHistory"].attribute("MaxSize", cfg.emailHistoryMax);
    }
    else
    {
        in["EmailHistory"](cfg.emailHistory);
        in["EmailHistory"].attribute("MaxSize", cfg.emailHistoryMax);
    }

    //TODO: remove if clause after migration! 2017-10-24
    if (formatVer < 5)
    {
        in["Gui"]["OnCompletionHistory"](cfg.commandHistory);
        in["Gui"]["OnCompletionHistory"].attribute("MaxSize", cfg.commandHistoryMax);
    }
    else if (formatVer < 20) //TODO: remove old parameter after migration! 2020-12-03
    {
        in["Gui"]["CommandHistory"](cfg.commandHistory);
        in["Gui"]["CommandHistory"].attribute("MaxSize", cfg.commandHistoryMax);
    }
    else
    {
        in["CommandHistory"](cfg.commandHistory);
        in["CommandHistory"].attribute("MaxSize", cfg.commandHistoryMax);
    }

    //TODO: remove if parameter migration after some time! 2020-01-30
    if (formatVer < 15)
        if (cfg.commandHistoryMax <= 8)
            cfg.commandHistoryMax = XmlGlobalSettings().commandHistoryMax;


    //TODO: remove old parameter after migration! 2018-01-16
    if (formatVer < 7)
        ; //reset this old crap
    else if (formatVer < 20) //TODO: remove old parameter after migration! 2020-12-03
        in["Gui"]["ExternalApps"](cfg.externalApps);
    else
        in["ExternalApps"](cfg.externalApps);

    //TODO: remove after migration! 2019-11-30
    if (formatVer < 15)
        for (ExternalApp& item : cfg.externalApps)
        {
            replace(item.cmdLine, Zstr("%folder_path%"),  Zstr("%parent_path%"));
            replace(item.cmdLine, Zstr("%folder_path2%"), Zstr("%parent_path2%"));
        }

    //TODO: remove after migration! 2020-06-13
    if (formatVer < 18)
        for (ExternalApp& item : cfg.externalApps)
        {
            trim(item.cmdLine);
            if (item.cmdLine == "xdg-open \"%parent_path%\"")
                item.cmdLine = "xdg-open \"$(dirname \"%local_path%\")\"";
        }

    if (formatVer < 20) //TODO: remove old parameter after migration! 2020-12-03
    {
        in["Gui"]["LastOnlineCheck"  ](cfg.lastUpdateCheck);
        in["Gui"]["LastOnlineVersion"](cfg.lastOnlineVersion);
    }
    else
    {
        in["LastOnlineCheck"  ](cfg.lastUpdateCheck);
        in["LastOnlineVersion"](cfg.lastOnlineVersion);
    }


    //cfg.dpiLayouts.clear(); -> NO: honor migration code above!

    for (XmlIn inLayout = in["DpiLayouts"]["Layout"]; inLayout; inLayout.next())
        if (std::string scaleTxt;
            inLayout.attribute("Scale", scaleTxt))
        {
            const int scalePercent = stringTo<int>(beforeLast(scaleTxt, '%', IfNotFoundReturn::none));
            DpiLayout layout;

            XmlIn inLayoutMain = inLayout["MainDialog"];
            inLayoutMain.attribute("Width",     layout.mainDlg.dlgSize.x);
            inLayoutMain.attribute("Height",    layout.mainDlg.dlgSize.y);
            inLayoutMain.attribute("PosX",      layout.mainDlg.dlgPos.x);
            inLayoutMain.attribute("PosY",      layout.mainDlg.dlgPos.y);
            inLayoutMain.attribute("Maximized", layout.mainDlg.isMaximized);

            inLayoutMain["PanelLayout"   ](layout.mainDlg.panelLayout);
            inLayoutMain["ConfigPanel"   ](layout.configColumnAttribs);
            inLayoutMain["OverviewPanel" ](layout.overviewColumnAttribs);
            inLayoutMain["FilePanelLeft" ](layout.fileColumnAttribsLeft);
            inLayoutMain["FilePanelRight"](layout.fileColumnAttribsRight);

            XmlIn inLayoutProgress = inLayout["ProgressDialog"];
            inLayoutProgress.attribute("Width",     layout.progressDlg.dlgSize.x);
            inLayoutProgress.attribute("Height",    layout.progressDlg.dlgSize.y);
            inLayoutProgress.attribute("Maximized", layout.progressDlg.isMaximized);

            cfg.dpiLayouts.emplace(scalePercent, std::move(layout));
        }

    //TODO: remove parameter migration after some time! 2018-03-14
    if (formatVer < 9)
        if (fastFromDIP(96) > 96) //high-DPI monitor => one-time migration
            cfg.dpiLayouts[getDpiScalePercent()] = DpiLayout();
}


template <class ConfigType>
std::pair<ConfigType, std::wstring /*warningMsg*/>  readConfig(const Zstring& filePath, XmlType type, int currentXmlFormatVer) //throw FileError
{
    XmlDoc doc = loadXml(filePath); //throw FileError

    if (getXmlTypeNoThrow(doc) != type) //noexcept
        throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));

    int formatVer = 0;
    /*bool success =*/ doc.root().getAttribute("XmlFormat", formatVer);

    XmlIn in(doc);
    ConfigType cfg;
    ::readConfig(in, cfg, formatVer);

    std::wstring warningMsg;
    try
    {
        checkXmlMappingErrors(in, filePath); //throw FileError

        //(try to) migrate old configuration automatically
        if (formatVer < currentXmlFormatVer)
            try { fff::writeConfig(cfg, filePath); /*throw FileError*/ }
            catch (FileError&) { assert(false); } //don't bother user!
    }
    catch (const FileError& e) { warningMsg = e.toString(); }

    return {cfg, warningMsg};
}
}


std::pair<XmlGuiConfig, std::wstring /*warningMsg*/> fff::readGuiConfig(const Zstring& filePath)
{
    return ::readConfig<XmlGuiConfig>(filePath, XmlType::gui, XML_FORMAT_SYNC_CFG); //throw FileError
}


std::pair<XmlBatchConfig, std::wstring /*warningMsg*/> fff::readBatchConfig(const Zstring& filePath)
{
    return ::readConfig<XmlBatchConfig>(filePath, XmlType::batch, XML_FORMAT_SYNC_CFG); //throw FileError
}


std::pair<XmlGlobalSettings, std::wstring /*warningMsg*/> fff::readGlobalConfig(const Zstring& filePath)
{
    return ::readConfig<XmlGlobalSettings>(filePath, XmlType::global, XML_FORMAT_GLOBAL_CFG); //throw FileError
}


namespace
{
template <class ConfigType>
ConfigType parseConfig(const XmlDoc& doc, const Zstring& filePath, int currentXmlFormatVer, std::wstring& warningMsg) //nothrow
{
    int formatVer = 0;
    /*bool success =*/ doc.root().getAttribute("XmlFormat", formatVer);

    XmlIn in(doc);
    ConfigType cfg;
    ::readConfig(in, cfg, formatVer);

    try
    {
        checkXmlMappingErrors(in, filePath); //throw FileError

        //(try to) migrate old configuration if needed
        if (formatVer < currentXmlFormatVer)
            try { fff::writeConfig(cfg, filePath); /*throw FileError*/ }
            catch (FileError&) { assert(false); } //don't bother user!
    }
    catch (const FileError& e)
    {
        if (warningMsg.empty())
            warningMsg = e.toString();
    }
    return cfg;
}
}


std::pair<XmlGuiConfig, std::wstring /*warningMsg*/> fff::readAnyConfig(const std::vector<Zstring>& filePaths) //throw FileError
{
    assert(!filePaths.empty());

    XmlGuiConfig cfg;
    std::wstring warningMsg;
    std::vector<MainConfiguration> mainCfgs;

    for (auto it = filePaths.begin(); it != filePaths.end(); ++it)
    {
        const Zstring& filePath = *it;
        const bool firstItem = it == filePaths.begin(); //init all non-"mainCfg" settings with first config file

        XmlDoc doc = loadXml(filePath); //throw FileError

        switch (getXmlTypeNoThrow(doc))
        {
            case XmlType::gui:
            {
                XmlGuiConfig guiCfg = parseConfig<XmlGuiConfig>(doc, filePath, XML_FORMAT_SYNC_CFG, warningMsg); //nothrow
                if (firstItem)
                    cfg = guiCfg;
                mainCfgs.push_back(guiCfg.mainCfg);
            }
            break;

            case XmlType::batch:
            {
                XmlBatchConfig batchCfg = parseConfig<XmlBatchConfig>(doc, filePath, XML_FORMAT_SYNC_CFG, warningMsg); //nothrow
                if (firstItem)
                    cfg = convertBatchToGui(batchCfg);
                mainCfgs.push_back(batchCfg.mainCfg);
            }
            break;

            case XmlType::global:
            case XmlType::other:
                throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));
        }
    }
    cfg.mainCfg = merge(mainCfgs);

    return {cfg, warningMsg};
}

//################################################################################################

namespace
{
void writeConfig(const CompConfig& cmpCfg, XmlOut& out)
{
    out["Variant" ](cmpCfg.compareVar);
    out["Symlinks"](cmpCfg.handleSymlinks);
    out["IgnoreTimeShift"](toTimeShiftPhrase(cmpCfg.ignoreTimeShiftMinutes));
}


void writeConfig(const SyncDirectionConfig& dirCfg, XmlOut& out)
{
    out["Variant"](dirCfg.var);

    if (dirCfg.var == SyncVariant::custom)
    {
        XmlOut outCustDir = out["CustomDirections"];
        outCustDir["LeftOnly"  ](dirCfg.custom.exLeftSideOnly);
        outCustDir["RightOnly" ](dirCfg.custom.exRightSideOnly);
        outCustDir["LeftNewer" ](dirCfg.custom.leftNewer);
        outCustDir["RightNewer"](dirCfg.custom.rightNewer);
        outCustDir["Different" ](dirCfg.custom.different);
        outCustDir["Conflict"  ](dirCfg.custom.conflict);
    }

    out["DetectMovedFiles"](dirCfg.detectMovedFiles);
}


void writeConfig(const SyncConfig& syncCfg, const std::map<AfsDevice, size_t>& deviceParallelOps, XmlOut& out)
{
    writeConfig(syncCfg.directionCfg, out);

    out["DeletionPolicy"  ](syncCfg.handleDeletion);
    out["VersioningFolder"](syncCfg.versioningFolderPhrase);

    const size_t parallelOps = getDeviceParallelOps(deviceParallelOps, syncCfg.versioningFolderPhrase);
    if (parallelOps > 1) out["VersioningFolder"].attribute("Threads", parallelOps);

    out["VersioningFolder"].attribute("Style", syncCfg.versioningStyle);

    if (syncCfg.versioningStyle != VersioningStyle::replace)
    {
        if (syncCfg.versionMaxAgeDays > 0) out["VersioningFolder"].attribute("MaxAge",   syncCfg.versionMaxAgeDays);
        if (syncCfg.versionCountMin   > 0) out["VersioningFolder"].attribute("MinCount", syncCfg.versionCountMin);
        if (syncCfg.versionCountMax   > 0) out["VersioningFolder"].attribute("MaxCount", syncCfg.versionCountMax);
    }
}


void writeConfig(const FilterConfig& filter, XmlOut& out)
{
    out["Include"](splitFilterByLines(filter.includeFilter));
    out["Exclude"](splitFilterByLines(filter.excludeFilter));

    out["TimeSpan"](filter.timeSpan);
    out["TimeSpan"].attribute("Type", filter.unitTimeSpan);

    out["SizeMin"](filter.sizeMin);
    out["SizeMin"].attribute("Unit", filter.unitSizeMin);

    out["SizeMax"](filter.sizeMax);
    out["SizeMax"].attribute("Unit", filter.unitSizeMax);
}


void writeConfig(const LocalPairConfig& lpc, const std::map<AfsDevice, size_t>& deviceParallelOps, XmlOut& out)
{
    XmlOut outPair = out.addChild("Pair");

    //read folder pairs
    outPair["Left" ](lpc.folderPathPhraseLeft);
    outPair["Right"](lpc.folderPathPhraseRight);

    const size_t parallelOpsL = getDeviceParallelOps(deviceParallelOps, lpc.folderPathPhraseLeft);
    const size_t parallelOpsR = getDeviceParallelOps(deviceParallelOps, lpc.folderPathPhraseRight);

    if (parallelOpsL > 1) outPair["Left" ].attribute("Threads", parallelOpsL);
    if (parallelOpsR > 1) outPair["Right"].attribute("Threads", parallelOpsR);

    //avoid "fake" changed configs by only storing "real" parallel-enabled devices in deviceParallelOps
    assert(std::all_of(deviceParallelOps.begin(), deviceParallelOps.end(), [](const auto& item) { return item.second > 1; }));

    //###########################################################
    //alternate comp configuration (optional)
    if (lpc.localCmpCfg)
    {
        XmlOut outLocalCmp = outPair["Compare"];
        writeConfig(*lpc.localCmpCfg, outLocalCmp);
    }
    //###########################################################
    //alternate sync configuration (optional)
    if (lpc.localSyncCfg)
    {
        XmlOut outLocalSync = outPair["Synchronize"];
        writeConfig(*lpc.localSyncCfg, deviceParallelOps, outLocalSync);
    }

    //###########################################################
    //alternate filter configuration
    if (lpc.localFilter != FilterConfig()) //don't spam .ffs_gui file with default filter entries
    {
        XmlOut outFilter = outPair["Filter"];
        writeConfig(lpc.localFilter, outFilter);
    }
}


void writeConfig(const MainConfiguration& mainCfg, XmlOut& out)
{
    XmlOut outMain = out;

    XmlOut outCmp = outMain["Compare"];

    writeConfig(mainCfg.cmpCfg, outCmp);
    //###########################################################

    XmlOut outSync = outMain["Synchronize"];

    writeConfig(mainCfg.syncCfg, mainCfg.deviceParallelOps, outSync);
    //###########################################################

    XmlOut outFilter = outMain["Filter"];
    //write filter settings
    writeConfig(mainCfg.globalFilter, outFilter);

    //###########################################################
    XmlOut outFp = outMain["FolderPairs"];
    //write folder pairs
    writeConfig(mainCfg.firstPair, mainCfg.deviceParallelOps, outFp);

    for (const LocalPairConfig& lpc : mainCfg.additionalPairs)
        writeConfig(lpc, mainCfg.deviceParallelOps, outFp);

    outMain["Errors"].attribute("Ignore", mainCfg.ignoreErrors);
    outMain["Errors"].attribute("Retry",  mainCfg.autoRetryCount);
    outMain["Errors"].attribute("Delay",  mainCfg.autoRetryDelay);

    outMain["PostSyncCommand"](mainCfg.postSyncCommand);
    outMain["PostSyncCommand"].attribute("Condition", mainCfg.postSyncCondition);

    outMain["LogFolder"](mainCfg.altLogFolderPathPhrase);

    outMain["EmailNotification"](mainCfg.emailNotifyAddress);
    outMain["EmailNotification"].attribute("Condition", mainCfg.emailNotifyCondition);
}


void writeConfig(const XmlGuiConfig& cfg, XmlOut& out)
{
    writeConfig(cfg.mainCfg, out); //write main config

    //write GUI specific config data
    XmlOut outGuiCfg = out["Gui"];

    outGuiCfg["GridViewType"](cfg.gridViewType);
}


void writeConfig(const BatchExclusiveConfig& cfg, XmlOut& out)
{
    XmlOut outBatchCfg = out["Batch"];

    outBatchCfg["ProgressDialog"].attribute("Minimized", cfg.runMinimized);
    outBatchCfg["ProgressDialog"].attribute("AutoClose", cfg.autoCloseSummary);
    outBatchCfg["ErrorDialog"   ](cfg.batchErrorHandling);
    outBatchCfg["PostSyncAction"](cfg.postSyncAction);
}


void writeConfig(const XmlBatchConfig& cfg, XmlOut& out)
{
    writeConfig(cfg.mainCfg,    out);
    writeConfig(cfg.batchExCfg, out);
}


void writeConfig(const XmlGlobalSettings& cfg, XmlOut& out)
{
    out["Language"].attribute("Name", cfg.programLanguage);

    out["FailSafeFileCopy"         ].attribute("Enabled", cfg.failSafeFileCopy);
    out["CopyLockedFiles"          ].attribute("Enabled", cfg.copyLockedFiles);
    out["CopyFilePermissions"      ].attribute("Enabled", cfg.copyFilePermissions);
    out["FileTimeTolerance"        ].attribute("Seconds", cfg.fileTimeTolerance);
    out["RunWithBackgroundPriority"].attribute("Enabled", cfg.runWithBackgroundPriority);
    out["LockDirectoriesDuringSync"].attribute("Enabled", cfg.createLockFile);
    out["VerifyCopiedFiles"        ].attribute("Enabled", cfg.verifyFileCopy);
    out["LogFiles"                 ].attribute("MaxAge",  cfg.logfilesMaxAgeDays);
    out["LogFiles"                 ].attribute("Format",  cfg.logFormat);

    out["ProgressDialog"].attribute("AutoClose", cfg.progressDlgAutoClose);

    XmlOut outOpt = out["OptionalDialogs"];
    outOpt["ConfirmStartSync"              ].attribute("Show", cfg.confirmDlgs.confirmSyncStart);
    outOpt["ConfirmSaveConfig"             ].attribute("Show", cfg.confirmDlgs.confirmSaveConfig);
    outOpt["ConfirmCommandMassInvoke"      ].attribute("Show", cfg.confirmDlgs.confirmCommandMassInvoke);
    outOpt["WarnFolderNotExisting"         ].attribute("Show", cfg.warnDlgs.warnFolderNotExisting);
    outOpt["WarnFoldersDifferInCase"       ].attribute("Show", cfg.warnDlgs.warnFoldersDifferInCase);
    outOpt["WarnUnresolvedConflicts"       ].attribute("Show", cfg.warnDlgs.warnUnresolvedConflicts);
    outOpt["WarnNotEnoughDiskSpace"        ].attribute("Show", cfg.warnDlgs.warnNotEnoughDiskSpace);
    outOpt["WarnSignificantDifference"     ].attribute("Show", cfg.warnDlgs.warnSignificantDifference);
    outOpt["WarnRecycleBinNotAvailable"    ].attribute("Show", cfg.warnDlgs.warnRecyclerMissing);
    outOpt["WarnInputFieldEmpty"           ].attribute("Show", cfg.warnDlgs.warnInputFieldEmpty);
    outOpt["WarnModificationTimeError"     ].attribute("Show", cfg.warnDlgs.warnModificationTimeError);
    outOpt["WarnDependentFolderPair"       ].attribute("Show", cfg.warnDlgs.warnDependentFolderPair);
    outOpt["WarnDependentBaseFolders"      ].attribute("Show", cfg.warnDlgs.warnDependentBaseFolders);
    outOpt["WarnDirectoryLockFailed"       ].attribute("Show", cfg.warnDlgs.warnDirectoryLockFailed);
    outOpt["WarnVersioningFolderPartOfSync"].attribute("Show", cfg.warnDlgs.warnVersioningFolderPartOfSync);

    out["Sounds"]["CompareFinished"].attribute("Path", substituteFfsResourcePath(cfg.soundFileCompareFinished));
    out["Sounds"]["SyncFinished"   ].attribute("Path", substituteFfsResourcePath(cfg.soundFileSyncFinished));
    out["Sounds"]["AlertPending"   ].attribute("Path", substituteFfsResourcePath(cfg.soundFileAlertPending));

    //gui specific global settings (optional)
    XmlOut outMainWin = out["MainDialog"];

    //###########################################################
    outMainWin["SearchPanel"].attribute("CaseSensitive", cfg.mainDlg.textSearchRespectCase);
    //###########################################################

    XmlOut outConfig = outMainWin["ConfigPanel"];
    outConfig.attribute("ScrollPos",     cfg.mainDlg.config.topRowPos);
    outConfig.attribute("SyncOverdue",   cfg.mainDlg.config.syncOverdueDays);
    outConfig.attribute("SortByColumn",  cfg.mainDlg.config.lastSortColumn);
    outConfig.attribute("SortAscending", cfg.mainDlg.config.lastSortAscending);

    outConfig["Configurations"].attribute("MaxSize", cfg.mainDlg.config.histItemsMax);
    outConfig["Configurations"].attribute("LastSelected", cfg.mainDlg.config.lastSelectedFile);
    outConfig["Configurations"](cfg.mainDlg.config.fileHistory);
    {
        std::vector<Zstring> cfgPaths = cfg.mainDlg.config.lastUsedFiles;
        for (Zstring& filePath : cfgPaths)
            filePath = substituteFreeFileSyncDriveLetter(filePath);

        outConfig["LastUsed"](cfgPaths);
    }

    //###########################################################

    XmlOut outOverview = outMainWin["OverviewPanel"];
    outOverview.attribute("ShowPercentage", cfg.mainDlg.overview.showPercentBar);
    outOverview.attribute("SortByColumn",   cfg.mainDlg.overview.lastSortColumn);
    outOverview.attribute("SortAscending",  cfg.mainDlg.overview.lastSortAscending);

    XmlOut outFileGrid = outMainWin["FilePanel"];
    outFileGrid.attribute("ShowIcons",  cfg.mainDlg.showIcons);
    outFileGrid.attribute("IconSize",   cfg.mainDlg.iconSize);
    outFileGrid.attribute("SashOffset", cfg.mainDlg.sashOffset);
    outFileGrid.attribute("FolderPairsMax", cfg.mainDlg.folderPairsVisibleMax);
    outFileGrid.attribute("PathFormatLeft",  cfg.mainDlg.itemPathFormatLeftGrid);
    outFileGrid.attribute("PathFormatRight", cfg.mainDlg.itemPathFormatRightGrid);

    outFileGrid["FolderHistoryLeft" ](cfg.mainDlg.folderHistoryLeft);
    outFileGrid["FolderHistoryRight"](cfg.mainDlg.folderHistoryRight);

    outFileGrid["FolderHistoryLeft" ].attribute("LastSelected", cfg.mainDlg.folderLastSelectedLeft);
    outFileGrid["FolderHistoryRight"].attribute("LastSelected", cfg.mainDlg.folderLastSelectedRight);

    //###########################################################
    XmlOut outCopyTo = outMainWin["ManualCopyTo"];
    outCopyTo.attribute("KeepRelativePaths", cfg.mainDlg.copyToCfg.keepRelPaths);
    outCopyTo.attribute("OverwriteIfExists", cfg.mainDlg.copyToCfg.overwriteIfExists);

    XmlOut outCopyToHistory = outCopyTo["FolderHistory"];
    outCopyToHistory(cfg.mainDlg.copyToCfg.folderHistory);
    outCopyToHistory.attribute("TargetFolder", cfg.mainDlg.copyToCfg.targetFolderPath);
    outCopyToHistory.attribute("LastSelected", cfg.mainDlg.copyToCfg.targetFolderLastSelected);
    //###########################################################

    XmlOut outDefFilter = outMainWin["DefaultViewFilter"];
    outDefFilter.attribute("Equal",    cfg.mainDlg.viewFilterDefault.equal);
    outDefFilter.attribute("Conflict", cfg.mainDlg.viewFilterDefault.conflict);
    outDefFilter.attribute("Excluded", cfg.mainDlg.viewFilterDefault.excluded);

    XmlOut catView = outDefFilter["Difference"];
    catView.attribute("LeftOnly",   cfg.mainDlg.viewFilterDefault.leftOnly);
    catView.attribute("RightOnly",  cfg.mainDlg.viewFilterDefault.rightOnly);
    catView.attribute("LeftNewer",  cfg.mainDlg.viewFilterDefault.leftNewer);
    catView.attribute("RightNewer", cfg.mainDlg.viewFilterDefault.rightNewer);
    catView.attribute("Different",  cfg.mainDlg.viewFilterDefault.different);

    XmlOut actView = outDefFilter["Action"];
    actView.attribute("CreateLeft",  cfg.mainDlg.viewFilterDefault.createLeft);
    actView.attribute("CreateRight", cfg.mainDlg.viewFilterDefault.createRight);
    actView.attribute("UpdateLeft",  cfg.mainDlg.viewFilterDefault.updateLeft);
    actView.attribute("UpdateRight", cfg.mainDlg.viewFilterDefault.updateRight);
    actView.attribute("DeleteLeft",  cfg.mainDlg.viewFilterDefault.deleteLeft);
    actView.attribute("DeleteRight", cfg.mainDlg.viewFilterDefault.deleteRight);
    actView.attribute("DoNothing",   cfg.mainDlg.viewFilterDefault.doNothing);

    out["FolderHistory" ].attribute("MaxSize", cfg.folderHistoryMax);

    out["SftpKeyFile"].attribute("LastSelected", cfg.sftpKeyFileLastSelected);

    XmlOut outFileFilter = out["DefaultFilter"];
    writeConfig(cfg.defaultFilter, outFileFilter);

    out["VersioningFolderHistory"](cfg.versioningFolderHistory);
    out["VersioningFolderHistory"].attribute("LastSelected", cfg.versioningFolderLastSelected);

    out["LogFolderHistory"](cfg.logFolderHistory);
    out["LogFolderHistory"].attribute("LastSelected", cfg.logFolderLastSelected);

    out["EmailHistory"](cfg.emailHistory);
    out["EmailHistory"].attribute("MaxSize", cfg.emailHistoryMax);

    out["CommandHistory"](cfg.commandHistory);
    out["CommandHistory"].attribute("MaxSize", cfg.commandHistoryMax);

    //external applications
    out["ExternalApps"](cfg.externalApps);

    //last update check
    out["LastOnlineCheck"  ](cfg.lastUpdateCheck);
    out["LastOnlineVersion"](cfg.lastOnlineVersion);


    for (const auto& [scalePercent, layout] : cfg.dpiLayouts)
    {
        XmlOut outLayout = out["DpiLayouts"].addChild("Layout");
        outLayout.attribute("Scale", numberTo<std::string>(scalePercent) + '%');

        XmlOut outLayoutMain = outLayout["MainDialog"];
        outLayoutMain.attribute("Width",     layout.mainDlg.dlgSize.x);
        outLayoutMain.attribute("Height",    layout.mainDlg.dlgSize.y);
        outLayoutMain.attribute("PosX",      layout.mainDlg.dlgPos.x);
        outLayoutMain.attribute("PosY",      layout.mainDlg.dlgPos.y);
        outLayoutMain.attribute("Maximized", layout.mainDlg.isMaximized);

        outLayoutMain["PanelLayout"   ](layout.mainDlg.panelLayout);
        outLayoutMain["ConfigPanel"   ](layout.configColumnAttribs);
        outLayoutMain["OverviewPanel" ](layout.overviewColumnAttribs);
        outLayoutMain["FilePanelLeft" ](layout.fileColumnAttribsLeft);
        outLayoutMain["FilePanelRight"](layout.fileColumnAttribsRight);

        XmlOut outLayoutProgress = outLayout["ProgressDialog"];
        outLayoutProgress.attribute("Width",     layout.progressDlg.dlgSize.x);
        outLayoutProgress.attribute("Height",    layout.progressDlg.dlgSize.y);
        outLayoutProgress.attribute("Maximized", layout.progressDlg.isMaximized);
    }
}


template <class ConfigType>
void writeConfig(const ConfigType& cfg, XmlType type, int xmlFormatVer, const Zstring& filePath)
{
    XmlDoc doc("FreeFileSync");
    setXmlType(doc, type); //throw()

    doc.root().setAttribute("XmlFormat", xmlFormatVer);

    XmlOut out(doc);
    writeConfig(cfg, out);

    saveXml(doc, filePath); //throw FileError
}
}

void fff::writeConfig(const XmlGuiConfig& cfg, const Zstring& filePath)
{
    ::writeConfig(cfg, XmlType::gui, XML_FORMAT_SYNC_CFG, filePath); //throw FileError
}


void fff::writeConfig(const XmlBatchConfig& cfg, const Zstring& filePath)
{
    ::writeConfig(cfg, XmlType::batch, XML_FORMAT_SYNC_CFG, filePath); //throw FileError
}


void fff::writeConfig(const XmlGlobalSettings& cfg, const Zstring& filePath)
{
    ::writeConfig(cfg, XmlType::global, XML_FORMAT_GLOBAL_CFG, filePath); //throw FileError
}


std::wstring fff::extractJobName(const Zstring& cfgFilePath)
{
    const Zstring fileName = afterLast(cfgFilePath, FILE_NAME_SEPARATOR, IfNotFoundReturn::all);
    const Zstring jobName  = beforeLast(fileName, Zstr('.'), IfNotFoundReturn::all);
    return utfTo<std::wstring>(jobName);
}

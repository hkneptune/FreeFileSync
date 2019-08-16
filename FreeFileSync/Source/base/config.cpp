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
#include <wx/intl.h>
#include "ffs_paths.h"
#include "../afs/concrete.h"


using namespace zen;
using namespace fff; //functionally needed for correct overload resolution!!!
//using AFS = AbstractFileSystem;


namespace
{
//-------------------------------------------------------------------------------------------------------------------------------
const int XML_FORMAT_GLOBAL_CFG = 13; //2019-05-29
const int XML_FORMAT_SYNC_CFG   = 14; //2018-08-13
//-------------------------------------------------------------------------------------------------------------------------------
}

XmlType getXmlTypeNoThrow(const XmlDoc& doc) //throw()
{
    if (doc.root().getNameAs<std::string>() == "FreeFileSync")
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
    soundFileSyncFinished(getResourceDirPf() + Zstr("bell.wav"))
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
std::vector<Zstring> splitFilterByLines(const Zstring& filterPhrase)
{
    if (filterPhrase.empty())
        return {};
    return split(filterPhrase, Zstr('\n'), SplitType::ALLOW_EMPTY);
}

Zstring mergeFilterLines(const std::vector<Zstring>& filterLines)
{
    if (filterLines.empty())
        return Zstring();
    Zstring out = filterLines[0];
    std::for_each(filterLines.begin() + 1, filterLines.end(), [&](const Zstring& line) { out += Zstr('\n'); out += line; });
    return out;
}
}

namespace zen
{
template <> inline
void writeText(const wxLanguage& value, std::string& output)
{
    //use description as unique wxLanguage identifier, see localization.cpp
    //=> handle changes to wxLanguage enum between wxWidgets versions
    if (const wxLanguageInfo* lngInfo = wxLocale::GetLanguageInfo(value))
        output = utfTo<std::string>(lngInfo->Description);
    else
    {
        assert(false);
        output = "English (U.S.)";
    }
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
        case CompareVariant::TIME_SIZE:
            output = "TimeAndSize";
            break;
        case CompareVariant::CONTENT:
            output = "Content";
            break;
        case CompareVariant::SIZE:
            output = "Size";
            break;
    }
}

template <> inline
bool readText(const std::string& input, CompareVariant& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "TimeAndSize")
        value = CompareVariant::TIME_SIZE;
    else if (tmp == "Content")
        value = CompareVariant::CONTENT;
    else if (tmp == "Size")
        value = CompareVariant::SIZE;
    else
        return false;
    return true;
}


template <> inline
void writeText(const SyncDirection& value, std::string& output)
{
    switch (value)
    {
        case SyncDirection::LEFT:
            output = "left";
            break;
        case SyncDirection::RIGHT:
            output = "right";
            break;
        case SyncDirection::NONE:
            output = "none";
            break;
    }
}

template <> inline
bool readText(const std::string& input, SyncDirection& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "left")
        value = SyncDirection::LEFT;
    else if (tmp == "right")
        value = SyncDirection::RIGHT;
    else if (tmp == "none")
        value = SyncDirection::NONE;
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
        case PostSyncCondition::COMPLETION:
            output = "Completion";
            break;
        case PostSyncCondition::ERRORS:
            output = "Errors";
            break;
        case PostSyncCondition::SUCCESS:
            output = "Success";
            break;
    }
}

template <> inline
bool readText(const std::string& input, PostSyncCondition& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Completion")
        value = PostSyncCondition::COMPLETION;
    else if (tmp == "Errors")
        value = PostSyncCondition::ERRORS;
    else if (tmp == "Success")
        value = PostSyncCondition::SUCCESS;
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
        case FileIconSize::SMALL:
            output = "Small";
            break;
        case FileIconSize::MEDIUM:
            output = "Medium";
            break;
        case FileIconSize::LARGE:
            output = "Large";
            break;
    }
}

template <> inline
bool readText(const std::string& input, FileIconSize& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Small")
        value = FileIconSize::SMALL;
    else if (tmp == "Medium")
        value = FileIconSize::MEDIUM;
    else if (tmp == "Large")
        value = FileIconSize::LARGE;
    else
        return false;
    return true;
}


template <> inline
void writeText(const DeletionPolicy& value, std::string& output)
{
    switch (value)
    {
        case DeletionPolicy::PERMANENT:
            output = "Permanent";
            break;
        case DeletionPolicy::RECYCLER:
            output = "RecycleBin";
            break;
        case DeletionPolicy::VERSIONING:
            output = "Versioning";
            break;
    }
}

template <> inline
bool readText(const std::string& input, DeletionPolicy& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Permanent")
        value = DeletionPolicy::PERMANENT;
    else if (tmp == "RecycleBin")
        value = DeletionPolicy::RECYCLER;
    else if (tmp == "Versioning")
        value = DeletionPolicy::VERSIONING;
    else
        return false;
    return true;
}


template <> inline
void writeText(const SymLinkHandling& value, std::string& output)
{
    switch (value)
    {
        case SymLinkHandling::EXCLUDE:
            output = "Exclude";
            break;
        case SymLinkHandling::DIRECT:
            output = "Direct";
            break;
        case SymLinkHandling::FOLLOW:
            output = "Follow";
            break;
    }
}

template <> inline
bool readText(const std::string& input, SymLinkHandling& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Exclude")
        value = SymLinkHandling::EXCLUDE;
    else if (tmp == "Direct")
        value = SymLinkHandling::DIRECT;
    else if (tmp == "Follow")
        value = SymLinkHandling::FOLLOW;
    else
        return false;
    return true;
}


template <> inline
void writeText(const ColumnTypeRim& value, std::string& output)
{
    switch (value)
    {
        case ColumnTypeRim::ITEM_PATH:
            output = "Path";
            break;
        case ColumnTypeRim::SIZE:
            output = "Size";
            break;
        case ColumnTypeRim::DATE:
            output = "Date";
            break;
        case ColumnTypeRim::EXTENSION:
            output = "Ext";
            break;
    }
}

template <> inline
bool readText(const std::string& input, ColumnTypeRim& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Path")
        value = ColumnTypeRim::ITEM_PATH;
    else if (tmp == "Size")
        value = ColumnTypeRim::SIZE;
    else if (tmp == "Date")
        value = ColumnTypeRim::DATE;
    else if (tmp == "Ext")
        value = ColumnTypeRim::EXTENSION;
    else
        return false;
    return true;
}


template <> inline
void writeText(const ItemPathFormat& value, std::string& output)
{
    switch (value)
    {
        case ItemPathFormat::FULL_PATH:
            output = "Full";
            break;
        case ItemPathFormat::RELATIVE_PATH:
            output = "Relative";
            break;
        case ItemPathFormat::ITEM_NAME:
            output = "Item";
            break;
    }
}

template <> inline
bool readText(const std::string& input, ItemPathFormat& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Full")
        value = ItemPathFormat::FULL_PATH;
    else if (tmp == "Relative")
        value = ItemPathFormat::RELATIVE_PATH;
    else if (tmp == "Item")
        value = ItemPathFormat::ITEM_NAME;
    else
        return false;
    return true;
}

template <> inline
void writeText(const ColumnTypeCfg& value, std::string& output)
{
    switch (value)
    {
        case ColumnTypeCfg::NAME:
            output = "Name";
            break;
        case ColumnTypeCfg::LAST_SYNC:
            output = "Last";
            break;
        case ColumnTypeCfg::LAST_LOG:
            output = "Log";
            break;
    }
}

template <> inline
bool readText(const std::string& input, ColumnTypeCfg& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Name")
        value = ColumnTypeCfg::NAME;
    else if (tmp == "Last")
        value = ColumnTypeCfg::LAST_SYNC;
    else if (tmp == "Log")
        value = ColumnTypeCfg::LAST_LOG;
    else
        return false;
    return true;
}


template <> inline
void writeText(const ColumnTypeTree& value, std::string& output)
{
    switch (value)
    {
        case ColumnTypeTree::FOLDER_NAME:
            output = "Tree";
            break;
        case ColumnTypeTree::ITEM_COUNT:
            output = "Count";
            break;
        case ColumnTypeTree::BYTES:
            output = "Bytes";
            break;
    }
}

template <> inline
bool readText(const std::string& input, ColumnTypeTree& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Tree")
        value = ColumnTypeTree::FOLDER_NAME;
    else if (tmp == "Count")
        value = ColumnTypeTree::ITEM_COUNT;
    else if (tmp == "Bytes")
        value = ColumnTypeTree::BYTES;
    else
        return false;
    return true;
}


template <> inline
void writeText(const UnitSize& value, std::string& output)
{
    switch (value)
    {
        case UnitSize::NONE:
            output = "None";
            break;
        case UnitSize::BYTE:
            output = "Byte";
            break;
        case UnitSize::KB:
            output = "KB";
            break;
        case UnitSize::MB:
            output = "MB";
            break;
    }
}

template <> inline
bool readText(const std::string& input, UnitSize& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "None")
        value = UnitSize::NONE;
    else if (tmp == "Byte")
        value = UnitSize::BYTE;
    else if (tmp == "KB")
        value = UnitSize::KB;
    else if (tmp == "MB")
        value = UnitSize::MB;
    else
        return false;
    return true;
}

template <> inline
void writeText(const UnitTime& value, std::string& output)
{
    switch (value)
    {
        case UnitTime::NONE:
            output = "None";
            break;
        case UnitTime::TODAY:
            output = "Today";
            break;
        case UnitTime::THIS_MONTH:
            output = "Month";
            break;
        case UnitTime::THIS_YEAR:
            output = "Year";
            break;
        case UnitTime::LAST_X_DAYS:
            output = "x-days";
            break;
    }
}

template <> inline
bool readText(const std::string& input, UnitTime& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "None")
        value = UnitTime::NONE;
    else if (tmp == "Today")
        value = UnitTime::TODAY;
    else if (tmp == "Month")
        value = UnitTime::THIS_MONTH;
    else if (tmp == "Year")
        value = UnitTime::THIS_YEAR;
    else if (tmp == "x-days")
        value = UnitTime::LAST_X_DAYS;
    else
        return false;
    return true;
}

template <> inline
void writeText(const VersioningStyle& value, std::string& output)
{
    switch (value)
    {
        case VersioningStyle::REPLACE:
            output = "Replace";
            break;
        case VersioningStyle::TIMESTAMP_FOLDER:
            output = "TimeStamp-Folder";
            break;
        case VersioningStyle::TIMESTAMP_FILE:
            output = "TimeStamp-File";
            break;
    }
}

template <> inline
bool readText(const std::string& input, VersioningStyle& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Replace")
        value = VersioningStyle::REPLACE;
    else if (tmp == "TimeStamp-Folder")
        value = VersioningStyle::TIMESTAMP_FOLDER;
    else if (tmp == "TimeStamp-File")
        value = VersioningStyle::TIMESTAMP_FILE;
    else
        return false;
    return true;
}


template <> inline
void writeText(const DirectionConfig::Variant& value, std::string& output)
{
    switch (value)
    {
        case DirectionConfig::TWO_WAY:
            output = "TwoWay";
            break;
        case DirectionConfig::MIRROR:
            output = "Mirror";
            break;
        case DirectionConfig::UPDATE:
            output = "Update";
            break;
        case DirectionConfig::CUSTOM:
            output = "Custom";
            break;
    }
}

template <> inline
bool readText(const std::string& input, DirectionConfig::Variant& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "TwoWay")
        value = DirectionConfig::TWO_WAY;
    else if (tmp == "Mirror")
        value = DirectionConfig::MIRROR;
    else if (tmp == "Update")
        value = DirectionConfig::UPDATE;
    else if (tmp == "Custom")
        value = DirectionConfig::CUSTOM;
    else
        return false;
    return true;
}


template <> inline
bool readStruc(const XmlElement& input, ColAttributesRim& value)
{
    XmlIn in(input);
    bool rv1 = in.attribute("Type",    value.type);
    bool rv2 = in.attribute("Visible", value.visible);
    bool rv3 = in.attribute("Width",   value.offset); //offset == width if stretch is 0
    bool rv4 = in.attribute("Stretch", value.stretch);
    return rv1 && rv2 && rv3 && rv4;
}

template <> inline
void writeStruc(const ColAttributesRim& value, XmlElement& output)
{
    XmlOut out(output);
    out.attribute("Type",    value.type);
    out.attribute("Visible", value.visible);
    out.attribute("Width",   value.offset);
    out.attribute("Stretch", value.stretch);
}


template <> inline
bool readStruc(const XmlElement& input, ColAttributesCfg& value)
{
    XmlIn in(input);
    bool rv1 = in.attribute("Type",    value.type);
    bool rv2 = in.attribute("Visible", value.visible);
    bool rv3 = in.attribute("Width",   value.offset); //offset == width if stretch is 0
    bool rv4 = in.attribute("Stretch", value.stretch);
    return rv1 && rv2 && rv3 && rv4;
}

template <> inline
void writeStruc(const ColAttributesCfg& value, XmlElement& output)
{
    XmlOut out(output);
    out.attribute("Type",    value.type);
    out.attribute("Visible", value.visible);
    out.attribute("Width",   value.offset);
    out.attribute("Stretch", value.stretch);
}


template <> inline
bool readStruc(const XmlElement& input, ColAttributesTree& value)
{
    XmlIn in(input);
    bool rv1 = in.attribute("Type",    value.type);
    bool rv2 = in.attribute("Visible", value.visible);
    bool rv3 = in.attribute("Width",   value.offset); //offset == width if stretch is 0
    bool rv4 = in.attribute("Stretch", value.stretch);
    return rv1 && rv2 && rv3 && rv4;
}

template <> inline
void writeStruc(const ColAttributesTree& value, XmlElement& output)
{
    XmlOut out(output);
    out.attribute("Type",    value.type);
    out.attribute("Visible", value.visible);
    out.attribute("Width",   value.offset);
    out.attribute("Stretch", value.stretch);
}


template <> inline
bool readStruc(const XmlElement& input, ViewFilterDefault& value)
{
    XmlIn in(input);

    bool success = true;
    auto readAttr = [&](XmlIn& elemIn, const char name[], bool& v)
    {
        if (!elemIn.attribute(name, v))
            success = false;
    };

    readAttr(in, "Equal",    value.equal);
    readAttr(in, "Conflict", value.conflict);
    readAttr(in, "Excluded", value.excluded);

    XmlIn catView = in["CategoryView"];
    readAttr(catView, "LeftOnly",   value.leftOnly);
    readAttr(catView, "RightOnly",  value.rightOnly);
    readAttr(catView, "LeftNewer",  value.leftNewer);
    readAttr(catView, "RightNewer", value.rightNewer);
    readAttr(catView, "Different",  value.different);

    XmlIn actView = in["ActionView"];
    readAttr(actView, "CreateLeft",  value.createLeft);
    readAttr(actView, "CreateRight", value.createRight);
    readAttr(actView, "UpdateLeft",  value.updateLeft);
    readAttr(actView, "UpdateRight", value.updateRight);
    readAttr(actView, "DeleteLeft",  value.deleteLeft);
    readAttr(actView, "DeleteRight", value.deleteRight);
    readAttr(actView, "DoNothing",   value.doNothing);

    return success; //[!] avoid short-circuit evaluation above
}

template <> inline
void writeStruc(const ViewFilterDefault& value, XmlElement& output)
{
    XmlOut out(output);

    out.attribute("Equal",    value.equal);
    out.attribute("Conflict", value.conflict);
    out.attribute("Excluded", value.excluded);

    XmlOut catView = out["CategoryView"];
    catView.attribute("LeftOnly",   value.leftOnly);
    catView.attribute("RightOnly",  value.rightOnly);
    catView.attribute("LeftNewer",  value.leftNewer);
    catView.attribute("RightNewer", value.rightNewer);
    catView.attribute("Different",  value.different);

    XmlOut actView = out["ActionView"];
    actView.attribute("CreateLeft",  value.createLeft);
    actView.attribute("CreateRight", value.createRight);
    actView.attribute("UpdateLeft",  value.updateLeft);
    actView.attribute("UpdateRight", value.updateRight);
    actView.attribute("DeleteLeft",  value.deleteLeft);
    actView.attribute("DeleteRight", value.deleteRight);
    actView.attribute("DoNothing",   value.doNothing);
}


template <> inline
bool readStruc(const XmlElement& input, ExternalApp& value)
{
    XmlIn in(input);
    const bool rv1 = in(value.cmdLine);
    const bool rv2 = in.attribute("Label", value.description);
    return rv1 && rv2;
}

template <> inline
void writeStruc(const ExternalApp& value, XmlElement& output)
{
    XmlOut out(output);
    out(value.cmdLine);
    out.attribute("Label", value.description);
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
        return Zstring(Zstr("%ffs_resource%")) + FILE_NAME_SEPARATOR + afterFirst(filePath, resPathPf, IF_MISSING_RETURN_NONE);
    return filePath;
}

Zstring resolveFfsResourceMacro(const Zstring& filePhrase)
{
    if (startsWith(trimCpy(filePhrase, true, false), Zstring(Zstr("%ffs_resource%")) + FILE_NAME_SEPARATOR))
        return getResourceDirPf() + afterFirst(filePhrase, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE);
    return filePhrase;
}
}


namespace zen
{
template <> inline
bool readStruc(const XmlElement& input, ConfigFileItem& value)
{
    XmlIn in(input);

    const bool rv1 = in.attribute("Result",  value.logResult);

    //FFS portable: use special syntax for config file paths: e.g. "FFS:\SyncJob.ffs_gui"
    Zstring cfgPathRaw;
    const bool rv2 = in.attribute("CfgPath", cfgPathRaw);
    if (rv2) value.cfgFilePath = resolveFreeFileSyncDriveMacro(cfgPathRaw);

    const bool rv3 = in.attribute("LastSync", value.lastSyncTime);

    Zstring logPathPhrase;
    const bool rv4 = in.attribute("LogPath", logPathPhrase);
    if (rv4) value.logFilePath = createAbstractPath(resolveFreeFileSyncDriveMacro(logPathPhrase));

    return rv1 && rv2 && rv3 && rv4;
}

template <> inline
void writeStruc(const ConfigFileItem& value, XmlElement& output)
{
    XmlOut out(output);
    out.attribute("Result",  value.logResult);
    out.attribute("CfgPath", substituteFreeFileSyncDriveLetter(value.cfgFilePath));
    out.attribute("LastSync", value.lastSyncTime);

    if (std::optional<Zstring> nativePath = AFS::getNativeItemPath(value.logFilePath))
        out.attribute("LogPath", substituteFreeFileSyncDriveLetter(*nativePath));
    else
        out.attribute("LogPath", AFS::getInitPathPhrase(value.logFilePath));
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
    XmlIn in(input);

    Zstring rawPath;
    const bool rv1 = in(rawPath);
    if (rv1) value.filePath = resolveFreeFileSyncDriveMacro(rawPath);

    const bool rv2 = in.attribute("LastSync", value.lastSyncTime);
    return rv1 && rv2;
}
}


namespace
{
void readConfig(const XmlIn& in, CompConfig& cmpCfg)
{
    in["Variant" ](cmpCfg.compareVar);
    in["Symlinks"](cmpCfg.handleSymlinks);

    //TODO: remove old parameter after migration! 2015-11-05
    if (in["TimeShift"])
    {
        std::wstring timeShiftPhrase;
        if (in["TimeShift"](timeShiftPhrase))
            cmpCfg.ignoreTimeShiftMinutes = fromTimeShiftPhrase(timeShiftPhrase);
    }
    else
    {
        std::wstring timeShiftPhrase;
        if (in["IgnoreTimeShift"](timeShiftPhrase))
            cmpCfg.ignoreTimeShiftMinutes = fromTimeShiftPhrase(timeShiftPhrase);
    }
}


void readConfig(const XmlIn& in, DirectionConfig& dirCfg)
{
    in["Variant"](dirCfg.var);

    if (dirCfg.var == DirectionConfig::CUSTOM)
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
            syncCfg.versioningStyle = VersioningStyle::REPLACE;
        else if (tmp == "TimeStamp")
            syncCfg.versioningStyle = VersioningStyle::TIMESTAMP_FILE;

        if (syncCfg.versioningStyle == VersioningStyle::REPLACE)
        {
            if (endsWithAsciiNoCase(syncCfg.versioningFolderPhrase, Zstr("/%timestamp%")) ||
                endsWithAsciiNoCase(syncCfg.versioningFolderPhrase, Zstr("\\%timestamp%")))
            {
                syncCfg.versioningFolderPhrase.resize(syncCfg.versioningFolderPhrase.size() - strLength(Zstr("/%timestamp%")));
                syncCfg.versioningStyle = VersioningStyle::TIMESTAMP_FOLDER;

                if (syncCfg.versioningFolderPhrase.size() == 2 && isAsciiAlpha(syncCfg.versioningFolderPhrase[0]) && syncCfg.versioningFolderPhrase[1] == Zstr(':'))
                    syncCfg.versioningFolderPhrase += Zstr('\\');
            }
        }
    }
    else
    {
        size_t parallelOps = 1;
        if (const XmlElement* e = in["VersioningFolder"].get()) e->getAttribute("Threads", parallelOps); //try to get attribute

        const size_t parallelOpsPrev = getDeviceParallelOps(deviceParallelOps, syncCfg.versioningFolderPhrase);
        /**/                           setDeviceParallelOps(deviceParallelOps, syncCfg.versioningFolderPhrase, std::max(parallelOps, parallelOpsPrev));

        in["VersioningFolder"].attribute("Style", syncCfg.versioningStyle);

        if (syncCfg.versioningStyle != VersioningStyle::REPLACE)
            if (const XmlElement* e = in["VersioningFolder"].get())
            {
                e->getAttribute("MaxAge", syncCfg.versionMaxAgeDays); //try to get attributes if available

                //TODO: remove if clause after migration! 2018-07-12
                if (formatVer < 13)
                {
                    e->getAttribute("CountMin", syncCfg.versionCountMin);   // => *no error* if not available
                    e->getAttribute("CountMax", syncCfg.versionCountMax);   //
                }
                else
                {
                    e->getAttribute("MinCount", syncCfg.versionCountMin);   // => *no error* if not available
                    e->getAttribute("MaxCount", syncCfg.versionCountMax);   //
                }
            }
    }
}


void readConfig(const XmlIn& in, FilterConfig& filter, int formatVer)
{
    std::vector<Zstring> tmpIn = splitFilterByLines(filter.includeFilter); //consider default value
    in["Include"](tmpIn);
    filter.includeFilter = mergeFilterLines(tmpIn);

    std::vector<Zstring> tmpEx = splitFilterByLines(filter.excludeFilter); //consider default value
    in["Exclude"](tmpEx);
    filter.excludeFilter = mergeFilterLines(tmpEx);

    //TODO: remove macro migration after some time! 2017-02-16
    if (formatVer <= 6) replace(filter.includeFilter, Zstr(';'), Zstr('|'));
    if (formatVer <= 6) replace(filter.excludeFilter, Zstr(';'), Zstr('|'));

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
            if (startsWithAsciiNoCase(folderPathPhrase, Zstr("sftp:")) ||
                startsWithAsciiNoCase(folderPathPhrase, Zstr( "ftp:")))
            {
                for (const Zstring& optPhrase : split(folderPathPhrase, Zstr("|"), SplitType::SKIP_EMPTY))
                    if (startsWith(optPhrase, Zstr("con=")))
                        parallelOps = stringTo<int>(afterFirst(optPhrase, Zstr("con="), IF_MISSING_RETURN_NONE));
            }
        };
        getParallelOps(lpc.folderPathPhraseLeft,  parallelOpsL);
        getParallelOps(lpc.folderPathPhraseRight, parallelOpsR);
    }
    else
    {
        if (const XmlElement* e = in["Left" ].get()) e->getAttribute("Threads", parallelOpsL); //try to get attributes:
        if (const XmlElement* e = in["Right"].get()) e->getAttribute("Threads", parallelOpsR); // => *no error* if not available
        //in["Left" ].attribute("Threads", parallelOpsL);
        //in["Right"].attribute("Threads", parallelOpsR);
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
        readConfig(inLocFilter, lpc.localFilter, formatVer);
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
        readConfig(inMain["GlobalFilter"], mainCfg.globalFilter, formatVer);
    else
        readConfig(inMain["Filter"], mainCfg.globalFilter, formatVer);

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
        inMain["OnCompletion"](mainCfg.postSyncCommand);
    else
        //TODO: remove if parameter migration after some time! 2018-02-24
        if (formatVer < 10)
            inMain["IgnoreErrors"](mainCfg.ignoreErrors);
        else
        {
            inMain["Errors"].attribute("Ignore", mainCfg.ignoreErrors);
            inMain["Errors"].attribute("Retry",  mainCfg.automaticRetryCount);
            inMain["Errors"].attribute("Delay",  mainCfg.automaticRetryDelay);
        }

    //TODO: remove if parameter migration after some time! 2018-08-13
    if (formatVer < 14)
        ; //path will be extracted from BatchExclusiveConfig
    else
        inMain["LogFolder"](mainCfg.altLogFolderPathPhrase);

    //TODO: remove if parameter migration after some time! 2017-10-24
    if (formatVer < 8)
        inMain["OnCompletion"](mainCfg.postSyncCommand);
    else
    {
        inMain["PostSyncCommand"](mainCfg.postSyncCommand);
        inMain["PostSyncCommand"].attribute("Condition", mainCfg.postSyncCondition);
    }
}


void readConfig(const XmlIn& in, XmlGuiConfig& cfg, int formatVer)
{
    //read main config
    readConfig(in, cfg.mainCfg, formatVer);

    //read GUI specific config data
    XmlIn inGuiCfg = in[formatVer < 10 ? "GuiConfig" : "Gui"]; //TODO: remove if parameter migration after some time! 2018-02-25

    std::string val;
    if (inGuiCfg["MiddleGridView"](val)) //refactor into enum!?
        cfg.highlightSyncAction = val == "Action";

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
    XmlIn inGeneral = in["General"];

    //TODO: remove old parameter after migration! 2016-01-18
    if (in["Shared"])
        inGeneral = in["Shared"];

    inGeneral["Language"].attribute("Name", cfg.programLanguage);

    inGeneral["FailSafeFileCopy"         ].attribute("Enabled", cfg.failSafeFileCopy);
    inGeneral["CopyLockedFiles"          ].attribute("Enabled", cfg.copyLockedFiles);
    inGeneral["CopyFilePermissions"      ].attribute("Enabled", cfg.copyFilePermissions);
    inGeneral["FileTimeTolerance"        ].attribute("Seconds", cfg.fileTimeTolerance);
    inGeneral["RunWithBackgroundPriority"].attribute("Enabled", cfg.runWithBackgroundPriority);
    inGeneral["LockDirectoriesDuringSync"].attribute("Enabled", cfg.createLockFile);
    inGeneral["VerifyCopiedFiles"        ].attribute("Enabled", cfg.verifyFileCopy);
    inGeneral["LogFiles"                 ].attribute("MaxAge",  cfg.logfilesMaxAgeDays);
    inGeneral["NotificationSound"        ].attribute("CompareFinished", cfg.soundFileCompareFinished);
    inGeneral["NotificationSound"        ].attribute("SyncFinished",    cfg.soundFileSyncFinished);
    inGeneral["ProgressDialog"           ].attribute("AutoClose",       cfg.autoCloseProgressDialog);

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
    }

    //TODO: remove if parameter migration after some time! 2018-08-13
    if (formatVer < 14)
        if (cfg.logfilesMaxAgeDays == 14) //default value was too small
            cfg.logfilesMaxAgeDays = XmlGlobalSettings().logfilesMaxAgeDays;

    //TODO: remove old parameter after migration! 2018-02-04
    if (formatVer < 8)
    {
        XmlIn inOpt = inGeneral["OptionalDialogs"];
        inOpt["ConfirmStartSync"                ].attribute("Enabled", cfg.confirmDlgs.confirmSyncStart);
        inOpt["ConfirmSaveConfig"               ].attribute("Enabled", cfg.confirmDlgs.popupOnConfigChange);
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
        XmlIn inOpt = inGeneral["OptionalDialogs"];
        inOpt["ConfirmStartSync"                ].attribute("Show", cfg.confirmDlgs.confirmSyncStart);
        inOpt["ConfirmSaveConfig"               ].attribute("Show", cfg.confirmDlgs.popupOnConfigChange);
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

    //GUI-specific global settings (optional)
    XmlIn inGui = in["Gui"];
    XmlIn inWnd = inGui["MainDialog"];

    //read application window size and position
    inWnd.attribute("Width",     cfg.gui.mainDlg.dlgSize.x);
    inWnd.attribute("Height",    cfg.gui.mainDlg.dlgSize.y);
    inWnd.attribute("PosX",      cfg.gui.mainDlg.dlgPos.x);
    inWnd.attribute("PosY",      cfg.gui.mainDlg.dlgPos.y);
    inWnd.attribute("Maximized", cfg.gui.mainDlg.isMaximized);

    //###########################################################

    //TODO: remove old parameter after migration! 2018-02-04
    if (formatVer < 8)
        inWnd["CaseSensitiveSearch"].attribute("Enabled", cfg.gui.mainDlg.textSearchRespectCase);
    else
        //TODO: remove if parameter migration after some time! 2018-09-09
        if (formatVer < 11)
            inWnd["Search"].attribute("CaseSensitive", cfg.gui.mainDlg.textSearchRespectCase);
        else
            inWnd["SearchPanel"].attribute("CaseSensitive", cfg.gui.mainDlg.textSearchRespectCase);

    //TODO: remove if parameter migration after some time! 2018-09-09
    if (formatVer < 11)
        inWnd["FolderPairsVisible" ].attribute("Max", cfg.gui.mainDlg.maxFolderPairsVisible);

    //TODO: remove if parameter migration after some time! 2018-09-09
    if (formatVer < 11)
        ;
    else
        inWnd["FolderHistory" ].attribute("MaxSize", cfg.gui.mainDlg.folderHistItemsMax);

    //###########################################################

    XmlIn inConfig = inWnd["ConfigPanel"];
    inConfig.attribute("ScrollPos",     cfg.gui.mainDlg.cfgGridTopRowPos);
    inConfig.attribute("SyncOverdue",   cfg.gui.mainDlg.cfgGridSyncOverdueDays);
    inConfig.attribute("SortByColumn",  cfg.gui.mainDlg.cfgGridLastSortColumn);
    inConfig.attribute("SortAscending", cfg.gui.mainDlg.cfgGridLastSortAscending);

    inConfig["Columns"](cfg.gui.mainDlg.cfgGridColumnAttribs);

    //TODO: remove after migration! 2018-07-27
    if (formatVer < 10) //reset once to show the new log column
        cfg.gui.mainDlg.cfgGridColumnAttribs = XmlGlobalSettings().gui.mainDlg.cfgGridColumnAttribs;

    //TODO: remove parameter migration after some time! 2018-01-08
    if (formatVer < 6)
    {
        inGui["ConfigHistory"].attribute("MaxSize", cfg.gui.mainDlg.cfgHistItemsMax);

        std::vector<Zstring> cfgHist;
        inGui["ConfigHistory"](cfgHist);

        for (const Zstring& cfgPath : cfgHist)
            cfg.gui.mainDlg.cfgFileHistory.emplace_back(cfgPath, 0, getNullPath(), SyncResult::finishedSuccess);
    }
    //TODO: remove after migration! 2018-07-27
    else if (formatVer < 10)
    {
        inConfig["Configurations"].attribute("MaxSize", cfg.gui.mainDlg.cfgHistItemsMax);

        std::vector<ConfigFileItemV9> cfgFileHistory;
        inConfig["Configurations"](cfgFileHistory);

        for (const ConfigFileItemV9& item : cfgFileHistory)
            cfg.gui.mainDlg.cfgFileHistory.emplace_back(item.filePath, item.lastSyncTime, getNullPath(), SyncResult::finishedSuccess);
    }
    else
    {
        inConfig["Configurations"].attribute("MaxSize", cfg.gui.mainDlg.cfgHistItemsMax);
        inConfig["Configurations"](cfg.gui.mainDlg.cfgFileHistory);
    }

    //TODO: remove parameter migration after some time! 2018-01-08
    if (formatVer < 6)
    {
        inGui["LastUsedConfig"](cfg.gui.mainDlg.lastUsedConfigFiles);
    }
    else
    {
        std::vector<Zstring> cfgPaths;
        if (inConfig["LastUsed"](cfgPaths))
        {
            for (Zstring& filePath : cfgPaths)
                filePath = resolveFreeFileSyncDriveMacro(filePath);

            cfg.gui.mainDlg.lastUsedConfigFiles = cfgPaths;
        }
    }

    //###########################################################

    XmlIn inOverview = inWnd["OverviewPanel"];
    inOverview.attribute("ShowPercentage", cfg.gui.mainDlg.treeGridShowPercentBar);
    inOverview.attribute("SortByColumn",   cfg.gui.mainDlg.treeGridLastSortColumn);
    inOverview.attribute("SortAscending",  cfg.gui.mainDlg.treeGridLastSortAscending);

    //read column attributes
    XmlIn inColTree = inOverview["Columns"];
    inColTree(cfg.gui.mainDlg.treeGridColumnAttribs);

    XmlIn inFileGrid = inWnd["FilePanel"];
    //TODO: remove parameter migration after some time! 2018-01-08
    if (formatVer < 6)
        inFileGrid = inWnd["CenterPanel"];

    inFileGrid.attribute("ShowIcons",  cfg.gui.mainDlg.showIcons);
    inFileGrid.attribute("IconSize",   cfg.gui.mainDlg.iconSize);
    inFileGrid.attribute("SashOffset", cfg.gui.mainDlg.sashOffset);

    //TODO: remove if parameter migration after some time! 2018-09-09
    if (formatVer < 11)
        ;
    else
        inFileGrid.attribute("MaxFolderPairsShown", cfg.gui.mainDlg.maxFolderPairsVisible);

    //TODO: remove if parameter migration after some time! 2018-09-09
    if (formatVer < 11)
        inFileGrid.attribute("HistoryMaxSize", cfg.gui.mainDlg.folderHistItemsMax);

    inFileGrid["ColumnsLeft"].attribute("PathFormat", cfg.gui.mainDlg.itemPathFormatLeftGrid);
    inFileGrid["ColumnsLeft"](cfg.gui.mainDlg.columnAttribLeft);

    inFileGrid["FolderHistoryLeft" ](cfg.gui.mainDlg.folderHistoryLeft);

    inFileGrid["ColumnsRight"].attribute("PathFormat", cfg.gui.mainDlg.itemPathFormatRightGrid);
    inFileGrid["ColumnsRight"](cfg.gui.mainDlg.columnAttribRight);

    inFileGrid["FolderHistoryRight"](cfg.gui.mainDlg.folderHistoryRight);

    //TODO: remove parameter migration after some time! 2018-01-08
    if (formatVer < 6)
    {
        inGui["FolderHistoryLeft" ](cfg.gui.mainDlg.folderHistoryLeft);
        inGui["FolderHistoryRight"](cfg.gui.mainDlg.folderHistoryRight);
        inGui["FolderHistoryLeft"].attribute("MaxSize", cfg.gui.mainDlg.folderHistItemsMax);
    }

    //TODO: remove if parameter migration after some time! 2018-09-09
    if (formatVer < 11)
        if (cfg.gui.mainDlg.folderHistItemsMax == 15) //default value was too small
            cfg.gui.mainDlg.folderHistItemsMax = XmlGlobalSettings().gui.mainDlg.folderHistItemsMax;

    //###########################################################
    XmlIn inCopyTo = inWnd["ManualCopyTo"];
    inCopyTo.attribute("KeepRelativePaths", cfg.gui.mainDlg.copyToCfg.keepRelPaths);
    inCopyTo.attribute("OverwriteIfExists", cfg.gui.mainDlg.copyToCfg.overwriteIfExists);

    XmlIn inCopyToHistory = inCopyTo["FolderHistory"];
    inCopyToHistory(cfg.gui.mainDlg.copyToCfg.folderHistory);
    inCopyToHistory.attribute("LastUsedPath", cfg.gui.mainDlg.copyToCfg.lastUsedPath);
    //###########################################################

    inWnd["DefaultViewFilter"](cfg.gui.mainDlg.viewFilterDefault);

    //TODO: remove old parameter after migration! 2018-02-04
    if (formatVer < 8)
    {
        XmlIn sharedView = inWnd["DefaultViewFilter"]["Shared"];
        sharedView.attribute("Equal",    cfg.gui.mainDlg.viewFilterDefault.equal);
        sharedView.attribute("Conflict", cfg.gui.mainDlg.viewFilterDefault.conflict);
        sharedView.attribute("Excluded", cfg.gui.mainDlg.viewFilterDefault.excluded);
    }

    //TODO: remove old parameter after migration! 2018-01-16
    if (formatVer < 7)
        inWnd["Perspective5"](cfg.gui.mainDlg.guiPerspectiveLast);
    else
        inWnd["Perspective"](cfg.gui.mainDlg.guiPerspectiveLast);

    //TODO: remove after migration! 2018-07-27
    if (formatVer < 10)
    {
        wxString newPersp;
        for (wxString& item : split(cfg.gui.mainDlg.guiPerspectiveLast, L"|", SplitType::SKIP_EMPTY))
        {
            if (contains(item, L"name=SearchPanel;"))
                replace(item, L";row=2;", L";row=3;");

            newPersp += (newPersp.empty() ? L"" : L"|") + item;
        }
        cfg.gui.mainDlg.guiPerspectiveLast = newPersp;
    }

    std::vector<Zstring> tmp = splitFilterByLines(cfg.gui.defaultExclusionFilter); //default value
    inGui["DefaultExclusionFilter"](tmp);
    cfg.gui.defaultExclusionFilter = mergeFilterLines(tmp);

    //TODO: remove parameter migration after some time! 2016-09-23
    if (formatVer < 4)
        cfg.gui.mainDlg.cfgHistItemsMax = std::max<size_t>(cfg.gui.mainDlg.cfgHistItemsMax, 100);

    //TODO: remove if clause after migration! 2017-10-24
    if (formatVer < 5)
    {
        inGui["OnCompletionHistory"](cfg.gui.commandHistory);
        inGui["OnCompletionHistory"].attribute("MaxSize", cfg.gui.commandHistItemsMax);
    }
    else
    {
        inGui["CommandHistory"](cfg.gui.commandHistory);
        inGui["CommandHistory"].attribute("MaxSize", cfg.gui.commandHistItemsMax);
    }

    //external applications
    //TODO: remove old parameter after migration! 2016-05-28
    if (inGui["ExternalApplications"])
    {
        inGui["ExternalApplications"](cfg.gui.externalApps);
        if (cfg.gui.externalApps.empty()) //who knows, let's repair some old failed data migrations
            cfg.gui.externalApps = XmlGlobalSettings().gui.externalApps;
        else
        {
        }
    }
    else
    {
        //TODO: remove old parameter after migration! 2018-01-16
        if (formatVer < 7)
        {
            std::vector<std::pair<std::wstring, Zstring>> extApps;
            if (inGui["ExternalApps"](extApps))
            {
                cfg.gui.externalApps.clear();
                for (const auto& [description, cmdLine] : extApps)
                    cfg.gui.externalApps.push_back({ description, cmdLine });
            }
        }
        else
            inGui["ExternalApps"](cfg.gui.externalApps);
    }

    //TODO: remove macro migration after some time! 2016-06-30
    if (formatVer < 3)
        for (ExternalApp& item : cfg.gui.externalApps)
        {
            replace(item.cmdLine, Zstr("%item2_path%"),   Zstr("%item_path2%"));
            replace(item.cmdLine, Zstr("%item_folder%"),  Zstr("%folder_path%"));
            replace(item.cmdLine, Zstr("%item2_folder%"), Zstr("%folder_path2%"));

            replace(item.cmdLine, Zstr("explorer /select, \"%item_path%\""), Zstr("explorer /select, \"%local_path%\""));
            replace(item.cmdLine, Zstr("\"%item_path%\""), Zstr("\"%local_path%\""));
            replace(item.cmdLine, Zstr("xdg-open \"%item_path%\""), Zstr("xdg-open \"%local_path%\""));
            replace(item.cmdLine, Zstr("open -R \"%item_path%\""), Zstr("open -R \"%local_path%\""));
            replace(item.cmdLine, Zstr("open \"%item_path%\""), Zstr("open \"%local_path%\""));

            if (contains(makeUpperCopy(item.cmdLine), Zstr("WINMERGEU.EXE")) ||
                contains(makeUpperCopy(item.cmdLine), Zstr("PSPAD.EXE")))
            {
                replace(item.cmdLine, Zstr("%item_path%"),  Zstr("%local_path%"));
                replace(item.cmdLine, Zstr("%item_path2%"), Zstr("%local_path2%"));
            }
        }
    //TODO: remove macro migration after some time! 2016-07-18
    for (ExternalApp& item : cfg.gui.externalApps)
        replace(item.cmdLine, Zstr("%item_folder%"),  Zstr("%folder_path%"));

    //last update check
    inGui["LastOnlineCheck"  ](cfg.gui.lastUpdateCheck);
    inGui["LastOnlineVersion"](cfg.gui.lastOnlineVersion);

    //batch specific global settings
    //XmlIn inBatch = in["Batch"];


    //TODO: remove parameter migration after some time! 2018-03-14
    if (formatVer < 9)
        if (fastFromDIP(96) > 96) //high-DPI monitor => one-time migration
        {
            const XmlGlobalSettings defaultCfg;
            cfg.gui.mainDlg.dlgSize               = defaultCfg.gui.mainDlg.dlgSize;
            cfg.gui.mainDlg.guiPerspectiveLast    = defaultCfg.gui.mainDlg.guiPerspectiveLast;
            cfg.gui.mainDlg.cfgGridColumnAttribs  = defaultCfg.gui.mainDlg.cfgGridColumnAttribs;
            cfg.gui.mainDlg.treeGridColumnAttribs = defaultCfg.gui.mainDlg.treeGridColumnAttribs;
            cfg.gui.mainDlg.columnAttribLeft      = defaultCfg.gui.mainDlg.columnAttribLeft;
            cfg.gui.mainDlg.columnAttribRight     = defaultCfg.gui.mainDlg.columnAttribRight;
        }
}


template <class ConfigType>
void readConfig(const Zstring& filePath, XmlType type, ConfigType& cfg, int currentXmlFormatVer, std::wstring& warningMsg) //throw FileError
{
    XmlDoc doc = loadXml(filePath); //throw FileError

    if (getXmlTypeNoThrow(doc) != type) //noexcept
        throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));

    int formatVer = 0;
    /*bool success =*/ doc.root().getAttribute("XmlFormat", formatVer);

    XmlIn in(doc);
    ::readConfig(in, cfg, formatVer);

    try
    {
        checkXmlMappingErrors(in, filePath); //throw FileError

        //(try to) migrate old configuration automatically
        if (formatVer < currentXmlFormatVer)
            try { fff::writeConfig(cfg, filePath); /*throw FileError*/ }
            catch (FileError&) { assert(false); } //don't bother user!
    }
    catch (const FileError& e) { warningMsg = e.toString(); }
}
}


void fff::readConfig(const Zstring& filePath, XmlGuiConfig& cfg, std::wstring& warningMsg)
{
    ::readConfig(filePath, XmlType::gui, cfg, XML_FORMAT_SYNC_CFG, warningMsg); //throw FileError
}


void fff::readConfig(const Zstring& filePath, XmlBatchConfig& cfg, std::wstring& warningMsg)
{
    ::readConfig(filePath, XmlType::batch, cfg, XML_FORMAT_SYNC_CFG, warningMsg); //throw FileError
}


void fff::readConfig(const Zstring& filePath, XmlGlobalSettings& cfg, std::wstring& warningMsg)
{
    ::readConfig(filePath, XmlType::global, cfg, XML_FORMAT_GLOBAL_CFG, warningMsg); //throw FileError
}


namespace
{
template <class XmlCfg>
XmlCfg parseConfig(const XmlDoc& doc, const Zstring& filePath, int currentXmlFormatVer, std::wstring& warningMsg) //nothrow
{
    int formatVer = 0;
    /*bool success =*/ doc.root().getAttribute("XmlFormat", formatVer);

    XmlIn in(doc);
    XmlCfg cfg;
    ::readConfig(in, cfg, formatVer);

    try
    {
        checkXmlMappingErrors(in, filePath); //throw FileError

        //(try to) migrate old configuration if needed
        if (formatVer < currentXmlFormatVer)
            try { fff::writeConfig(cfg, filePath); /*throw FileError*/ }
            catch (FileError&) { assert(false); }     //don't bother user!
    }
    catch (const FileError& e)
    {
        if (warningMsg.empty())
            warningMsg = e.toString();
    }
    return cfg;
}
}


void fff::readAnyConfig(const std::vector<Zstring>& filePaths, XmlGuiConfig& cfg, std::wstring& warningMsg) //throw FileError
{
    assert(!filePaths.empty());

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


void writeConfig(const DirectionConfig& dirCfg, XmlOut& out)
{
    out["Variant"](dirCfg.var);

    if (dirCfg.var == DirectionConfig::CUSTOM)
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

    if (syncCfg.versioningStyle != VersioningStyle::REPLACE)
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
    XmlOut outPair = out.ref().addChild("Pair");

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
    outMain["Errors"].attribute("Retry",  mainCfg.automaticRetryCount);
    outMain["Errors"].attribute("Delay",  mainCfg.automaticRetryDelay);

    outMain["LogFolder"](mainCfg.altLogFolderPathPhrase);

    outMain["PostSyncCommand"](mainCfg.postSyncCommand);
    outMain["PostSyncCommand"].attribute("Condition", mainCfg.postSyncCondition);
}


void writeConfig(const XmlGuiConfig& cfg, XmlOut& out)
{
    writeConfig(cfg.mainCfg, out); //write main config

    //write GUI specific config data
    XmlOut outGuiCfg = out["Gui"];

    outGuiCfg["MiddleGridView"](cfg.highlightSyncAction ? "Action" : "Category"); //refactor into enum!?
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
    XmlOut outGeneral = out["General"];

    outGeneral["Language"].attribute("Name", cfg.programLanguage);

    outGeneral["FailSafeFileCopy"         ].attribute("Enabled", cfg.failSafeFileCopy);
    outGeneral["CopyLockedFiles"          ].attribute("Enabled", cfg.copyLockedFiles);
    outGeneral["CopyFilePermissions"      ].attribute("Enabled", cfg.copyFilePermissions);
    outGeneral["FileTimeTolerance"        ].attribute("Seconds", cfg.fileTimeTolerance);
    outGeneral["RunWithBackgroundPriority"].attribute("Enabled", cfg.runWithBackgroundPriority);
    outGeneral["LockDirectoriesDuringSync"].attribute("Enabled", cfg.createLockFile);
    outGeneral["VerifyCopiedFiles"        ].attribute("Enabled", cfg.verifyFileCopy);
    outGeneral["LogFiles"                 ].attribute("MaxAge",  cfg.logfilesMaxAgeDays);
    outGeneral["NotificationSound"        ].attribute("CompareFinished", substituteFfsResourcePath(cfg.soundFileCompareFinished));
    outGeneral["NotificationSound"        ].attribute("SyncFinished",    substituteFfsResourcePath(cfg.soundFileSyncFinished));
    outGeneral["ProgressDialog"           ].attribute("AutoClose",       cfg.autoCloseProgressDialog);

    XmlOut outOpt = outGeneral["OptionalDialogs"];
    outOpt["ConfirmStartSync"              ].attribute("Show", cfg.confirmDlgs.confirmSyncStart);
    outOpt["ConfirmSaveConfig"             ].attribute("Show", cfg.confirmDlgs.popupOnConfigChange);
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

    //gui specific global settings (optional)
    XmlOut outGui = out["Gui"];
    XmlOut outWnd = outGui["MainDialog"];

    //write application window size and position
    outWnd.attribute("Width",     cfg.gui.mainDlg.dlgSize.x);
    outWnd.attribute("Height",    cfg.gui.mainDlg.dlgSize.y);
    outWnd.attribute("PosX",      cfg.gui.mainDlg.dlgPos.x);
    outWnd.attribute("PosY",      cfg.gui.mainDlg.dlgPos.y);
    outWnd.attribute("Maximized", cfg.gui.mainDlg.isMaximized);

    //###########################################################
    outWnd["SearchPanel"  ].attribute("CaseSensitive", cfg.gui.mainDlg.textSearchRespectCase);
    outWnd["FolderHistory"].attribute("MaxSize",       cfg.gui.mainDlg.folderHistItemsMax);
    //###########################################################

    XmlOut outConfig = outWnd["ConfigPanel"];
    outConfig.attribute("ScrollPos",     cfg.gui.mainDlg.cfgGridTopRowPos);
    outConfig.attribute("SyncOverdue",   cfg.gui.mainDlg.cfgGridSyncOverdueDays);
    outConfig.attribute("SortByColumn",  cfg.gui.mainDlg.cfgGridLastSortColumn);
    outConfig.attribute("SortAscending", cfg.gui.mainDlg.cfgGridLastSortAscending);

    outConfig["Columns"](cfg.gui.mainDlg.cfgGridColumnAttribs);
    outConfig["Configurations"].attribute("MaxSize", cfg.gui.mainDlg.cfgHistItemsMax);
    outConfig["Configurations"](cfg.gui.mainDlg.cfgFileHistory);
    {
        std::vector<Zstring> cfgPaths = cfg.gui.mainDlg.lastUsedConfigFiles;
        for (Zstring& filePath : cfgPaths)
            filePath = substituteFreeFileSyncDriveLetter(filePath);

        outConfig["LastUsed"](cfgPaths);
    }

    //###########################################################

    XmlOut outOverview = outWnd["OverviewPanel"];
    outOverview.attribute("ShowPercentage", cfg.gui.mainDlg.treeGridShowPercentBar);
    outOverview.attribute("SortByColumn",   cfg.gui.mainDlg.treeGridLastSortColumn);
    outOverview.attribute("SortAscending",  cfg.gui.mainDlg.treeGridLastSortAscending);

    //write column attributes
    XmlOut outColTree = outOverview["Columns"];
    outColTree(cfg.gui.mainDlg.treeGridColumnAttribs);

    XmlOut outFileGrid = outWnd["FilePanel"];
    outFileGrid.attribute("ShowIcons",  cfg.gui.mainDlg.showIcons);
    outFileGrid.attribute("IconSize",   cfg.gui.mainDlg.iconSize);
    outFileGrid.attribute("SashOffset", cfg.gui.mainDlg.sashOffset);
    outFileGrid.attribute("MaxFolderPairsShown", cfg.gui.mainDlg.maxFolderPairsVisible);

    outFileGrid["ColumnsLeft"].attribute("PathFormat", cfg.gui.mainDlg.itemPathFormatLeftGrid);
    outFileGrid["ColumnsLeft"](cfg.gui.mainDlg.columnAttribLeft);

    outFileGrid["FolderHistoryLeft" ](cfg.gui.mainDlg.folderHistoryLeft);

    outFileGrid["ColumnsRight"].attribute("PathFormat", cfg.gui.mainDlg.itemPathFormatRightGrid);
    outFileGrid["ColumnsRight"](cfg.gui.mainDlg.columnAttribRight);

    outFileGrid["FolderHistoryRight"](cfg.gui.mainDlg.folderHistoryRight);

    //###########################################################
    XmlOut outCopyTo = outWnd["ManualCopyTo"];
    outCopyTo.attribute("KeepRelativePaths", cfg.gui.mainDlg.copyToCfg.keepRelPaths);
    outCopyTo.attribute("OverwriteIfExists", cfg.gui.mainDlg.copyToCfg.overwriteIfExists);

    XmlOut outCopyToHistory = outCopyTo["FolderHistory"];
    outCopyToHistory(cfg.gui.mainDlg.copyToCfg.folderHistory);
    outCopyToHistory.attribute("LastUsedPath", cfg.gui.mainDlg.copyToCfg.lastUsedPath);
    //###########################################################

    outWnd["DefaultViewFilter"](cfg.gui.mainDlg.viewFilterDefault);
    outWnd["Perspective"      ](cfg.gui.mainDlg.guiPerspectiveLast);

    outGui["DefaultExclusionFilter"](splitFilterByLines(cfg.gui.defaultExclusionFilter));

    outGui["CommandHistory"](cfg.gui.commandHistory);
    outGui["CommandHistory"].attribute("MaxSize", cfg.gui.commandHistItemsMax);

    //external applications
    outGui["ExternalApps"](cfg.gui.externalApps);

    //last update check
    outGui["LastOnlineCheck"  ](cfg.gui.lastUpdateCheck);
    outGui["LastOnlineVersion"](cfg.gui.lastOnlineVersion);

    //batch specific global settings
    //XmlOut outBatch = out["Batch"];
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
    const Zstring fileName = afterLast(cfgFilePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL);
    const Zstring jobName  = beforeLast(fileName, Zstr('.'), IF_MISSING_RETURN_ALL);
    return utfTo<std::wstring>(jobName);
}

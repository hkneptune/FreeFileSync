// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "config.h"
#include <zenxml/xml.h>
#include <zen/file_access.h>
//#include <zen/file_io.h>
#include <zen/time.h>
#include <zen/process_exec.h>
#include <wx/uilocale.h>
#include "ffs_paths.h"
#include "base_tools.h"
//#include "afs/native.h"


using namespace zen;
using namespace fff; //required for correct overload resolution!


namespace
{
//-------------------------------------------------------------------------------------------------------------------------------
const int XML_FORMAT_GLOBAL_CFG = 27; //2023-05-13
const int XML_FORMAT_SYNC_CFG   = 23; //2023-08-24
//-------------------------------------------------------------------------------------------------------------------------------
}


const ExternalApp fff::extCommandFileManager
//"xdg-open \"%parent_path%\"" -> not good enough: we need %local_path% for proper MTP/Google Drive handling
{L"Show in file manager", "xdg-open \"$(dirname \"%local_path%\")\""};
//mark for extraction: _("Show in file manager") Linux doesn't use the term "folder"


const ExternalApp fff::extCommandOpenDefault
{L"Open with default application", "xdg-open \"%local_path%\""};




XmlGlobalSettings::XmlGlobalSettings() :
    soundFileSyncFinished(appendPath(getResourceDirPath(), Zstr("bell.wav"))),
    soundFileAlertPending(appendPath(getResourceDirPath(), Zstr("remind.wav")))
{
}

//################################################################################################################

Zstring fff::getGlobalConfigDefaultPath() { return appendPath(getConfigDirPath(), Zstr("GlobalSettings.xml")); }
Zstring fff::getLogFolderDefaultPath   () { return appendPath(getConfigDirPath(), Zstr("Logs")); }

namespace
{
std::vector<Zstring> splitFilterByLines(Zstring filterPhrase)
{
    trim(filterPhrase);
    if (filterPhrase.empty())
        return {};

    return splitCpy(filterPhrase, Zstr('\n'), SplitOnEmpty::allow);
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

    const wxString& canonicalName = wxUILocale::GetLanguageCanonicalName(value);
    assert(!canonicalName.empty());
    if (!canonicalName.empty())
        output = utfTo<std::string>(canonicalName);
    else
        output = utfTo<std::string>(wxUILocale::GetLanguageCanonicalName(wxLANGUAGE_ENGLISH_US));
}

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
void writeText(const PostBatchAction& value, std::string& output)
{
    switch (value)
    {
        case PostBatchAction::none:
            output = "None";
            break;
        case PostBatchAction::sleep:
            output = "Sleep";
            break;
        case PostBatchAction::shutdown:
            output = "Shutdown";
            break;
    }
}

template <> inline
bool readText(const std::string& input, PostBatchAction& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "None")
        value = PostBatchAction::none;
    else if (tmp == "Sleep")
        value = PostBatchAction::sleep;
    else if (tmp == "Shutdown")
        value = PostBatchAction::shutdown;
    else
        return false;
    return true;
}


template <> inline
void writeText(const GridIconSize& value, std::string& output)
{
    switch (value)
    {
        case GridIconSize::small:
            output = "Small";
            break;
        case GridIconSize::medium:
            output = "Medium";
            break;
        case GridIconSize::large:
            output = "Large";
            break;
    }
}

template <> inline
bool readText(const std::string& input, GridIconSize& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Small")
        value = GridIconSize::small;
    else if (tmp == "Medium")
        value = GridIconSize::medium;
    else if (tmp == "Large")
        value = GridIconSize::large;
    else
        return false;
    return true;
}


template <> inline
void writeText(const DeletionVariant& value, std::string& output)
{
    switch (value)
    {
        case DeletionVariant::permanent:
            output = "Permanent";
            break;
        case DeletionVariant::recycler:
            output = "RecycleBin";
            break;
        case DeletionVariant::versioning:
            output = "Versioning";
            break;
    }
}

template <> inline
bool readText(const std::string& input, DeletionVariant& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Permanent")
        value = DeletionVariant::permanent;
    else if (tmp == "RecycleBin")
        value = DeletionVariant::recycler;
    else if (tmp == "Versioning")
        value = DeletionVariant::versioning;
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
        case SymLinkHandling::asLink:
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
        value = SymLinkHandling::asLink;
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
void writeText(const TaskResult& value, std::string& output)
{
    switch (value)
    {
        case TaskResult::success:
            output = "Success";
            break;
        case TaskResult::warning:
            output = "Warning";
            break;
        case TaskResult::error:
            output = "Error";
            break;
        case TaskResult::cancelled:
            output = "Stopped";
            break;
    }
}

template <> inline
bool readText(const std::string& input, TaskResult& value)
{
    const std::string tmp = trimCpy(input);
    if (tmp == "Success")
        value = TaskResult::success;
    else if (tmp == "Warning")
        value = TaskResult::warning;
    else if (tmp == "Error")
        value = TaskResult::error;
    else if (tmp == "Stopped")
        value = TaskResult::cancelled;
    else
        return false;
    return true;
}
}


namespace
{


Zstring makePortablePath(const Zstring& pathPhrase)
{
    const Zstring& pathTrm = trimCpy(pathPhrase);
    const Zstring& ffsPath = getInstallDirPath();

    if (pathTrm == ffsPath)
        return Zstr("%ffs_path%");

    if (startsWith(pathTrm, appendSeparator(ffsPath))) //don't allow *partial* component match!
        return Zstring(Zstr("%ffs_path%")) + (pathTrm.c_str() + appendSeparator(ffsPath).size() - 1);

    return pathPhrase;
}


Zstring resolvePortablePath(const Zstring& portablePathPhrase)
{
    const Zstring& pathTrm = trimCpy(portablePathPhrase);

    if (startsWith(pathTrm, Zstr("%ffs_path%")))
        return appendPath(getInstallDirPath(), afterFirst(pathTrm, FILE_NAME_SEPARATOR, IfNotFoundReturn::none)); //caveat: appendPath() requires relPath!

    //TODO: remove parameter migration after some time! 2022-06-14
    if (startsWith(pathTrm, Zstr("%ffs_resource%")))
        return appendPath(getResourceDirPath(), afterFirst(pathTrm, FILE_NAME_SEPARATOR, IfNotFoundReturn::none));

    return portablePathPhrase;
}


std::vector<Zstring> makePortablePath(std::vector<Zstring> pathPhrases)
{
    for (Zstring& pathPhrase : pathPhrases)
        pathPhrase = makePortablePath(pathPhrase);
    return pathPhrases;
}


std::vector<Zstring> resolvePortablePath(std::vector<Zstring> pathPhrases)
{
    for (Zstring& pathPhrase : pathPhrases)
        pathPhrase = resolvePortablePath(pathPhrase);
    return pathPhrases;
}
}


namespace zen
{
template <> inline
bool readStruc(const XmlElement& input, ConfigFileItem& value)
{
    bool success = true;
    success = input.getAttribute("LastSync",  value.lastRunStats.startTime) && success;
    success = input.getAttribute("Result",    value.lastRunStats.syncResult) && success;

    if (input.hasAttribute("CfgPath")) //TODO: remove after migration! 2020-02-09
        success = input.getAttribute("CfgPath", value.cfgFilePath) && success; //
    else
        success = input.getAttribute("Config", value.cfgFilePath) && success;

    //FFS portable: use special syntax for config file paths: e.g. "%ffs_drive%\SyncJob.ffs_gui"
    value.cfgFilePath = resolvePortablePath(value.cfgFilePath);

    Zstring logFilePhrase;
    if (input.hasAttribute("LogPath")) //TODO: remove after migration! 2020-02-09
        success = input.getAttribute("LogPath", logFilePhrase) && success; //
    else
        success = input.getAttribute("Log", logFilePhrase) && success;

    value.lastRunStats.logFilePath = createAbstractPath(resolvePortablePath(logFilePhrase));

    if (!input.hasAttribute("Items")) //TODO: remove after migration! 2023-05-13
        ;
    else
        success = input.getAttribute("Items", value.lastRunStats.itemsProcessed) && success;

    if (!input.hasAttribute("Bytes")) //TODO: remove after migration! 2023-05-13
        ;
    else
        success = input.getAttribute("Bytes", value.lastRunStats.bytesProcessed) && success;

    if (!input.hasAttribute("TotalTime")) //TODO: remove after migration! 2023-05-13
        ;
    else
        success = input.getAttribute("TotalTime", value.lastRunStats.totalTime) && success;

    if (!input.hasAttribute("Errors")) //TODO: remove after migration! 2023-05-13
        ;
    else
        success = input.getAttribute("Errors", value.lastRunStats.errors) && success;

    if (!input.hasAttribute("Warnings")) //TODO: remove after migration! 2023-05-13
        ;
    else
        success = input.getAttribute("Warnings", value.lastRunStats.warnings) && success;

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
    output.setAttribute("LastSync",  value.lastRunStats.startTime);
    output.setAttribute("Result",    value.lastRunStats.syncResult);

    output.setAttribute("Config", makePortablePath(value.cfgFilePath));
    output.setAttribute("Log",    makePortablePath(AFS::getInitPathPhrase(value.lastRunStats.logFilePath)));

    output.setAttribute("Items",     value.lastRunStats.itemsProcessed);
    output.setAttribute("Bytes",     value.lastRunStats.bytesProcessed);

    output.setAttribute("TotalTime", value.lastRunStats.totalTime);

    output.setAttribute("Errors",   value.lastRunStats.errors);
    output.setAttribute("Warnings", value.lastRunStats.warnings);

    if (value.backColor.IsOk())
    {
        assert(value.backColor.Alpha() == 255);
        const auto& [rh, rl] = hexify(value.backColor.Red  ());
        const auto& [gh, gl] = hexify(value.backColor.Green());
        const auto& [bh, bl] = hexify(value.backColor.Blue ());
        output.setAttribute("Color", std::string({rh, rl, gh, gl, bh, bl}));
    }
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


void readConfig(const XmlIn& in, SyncDirectionConfig& dirCfg, int formatVer)
{
    if (formatVer < 21) //TODO: remove if parameter migration after some time! 2023-08-09
    {
        std::string varName;
        in["Variant"](varName);
        trim(varName);

        if (varName == "TwoWay")
            dirCfg = getDefaultSyncCfg(SyncVariant::twoWay);
        else if (varName == "Mirror")
        {
            dirCfg = getDefaultSyncCfg(SyncVariant::mirror);

            bool detectMovedFiles = false;
            in["DetectMovedFiles"](detectMovedFiles);
            if (detectMovedFiles)
            {
                if (const DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&dirCfg.dirs))
                    dirCfg.dirs = getChangesDirDefault(*diffDirs); //convert to "changes"-based mirror, so that move detection is enabled
                else assert(false);
            }
        }
        else if (varName == "Update")
            dirCfg.dirs = DirectionByDiff
        {
            .leftOnly   = SyncDirection::right,
            .rightOnly  = SyncDirection::none,
            .leftNewer  = SyncDirection::right,
            .rightNewer = SyncDirection::none, //note: will be fixed below for CompareVariant::content/size
        };
        else
        {
            assert(varName == "Custom");

            dirCfg.dirs = DirectionByDiff();

            XmlIn inCustDir = in["CustomDirections"];
            inCustDir["LeftOnly"  ](std::get<DirectionByDiff>(dirCfg.dirs).leftOnly);
            inCustDir["RightOnly" ](std::get<DirectionByDiff>(dirCfg.dirs).rightOnly);
            inCustDir["LeftNewer" ](std::get<DirectionByDiff>(dirCfg.dirs).leftNewer);  //note: will be fixed below for CompareVariant::content/size
            inCustDir["RightNewer"](std::get<DirectionByDiff>(dirCfg.dirs).rightNewer); //
        }
    }
    else
    {
        if (XmlIn inDirs = in["Differences"])
        {
            dirCfg.dirs = DirectionByDiff();
            inDirs.attribute("LeftOnly",   std::get<DirectionByDiff>(dirCfg.dirs).leftOnly);
            inDirs.attribute("LeftNewer",  std::get<DirectionByDiff>(dirCfg.dirs).leftNewer);
            inDirs.attribute("RightNewer", std::get<DirectionByDiff>(dirCfg.dirs).rightNewer);
            inDirs.attribute("RightOnly",  std::get<DirectionByDiff>(dirCfg.dirs).rightOnly);
        }
        else
        {
            assert(in["Changes"]);
            dirCfg.dirs = DirectionByChange();

            XmlIn inDirsL = in["Changes"]["Left"];
            inDirsL.attribute("Create", std::get<DirectionByChange>(dirCfg.dirs).left.create);
            inDirsL.attribute("Update", std::get<DirectionByChange>(dirCfg.dirs).left.update);
            inDirsL.attribute("Delete", std::get<DirectionByChange>(dirCfg.dirs).left.delete_);

            XmlIn inDirsR = in["Changes"]["Right"];
            inDirsR.attribute("Create", std::get<DirectionByChange>(dirCfg.dirs).right.create);
            inDirsR.attribute("Update", std::get<DirectionByChange>(dirCfg.dirs).right.update);
            inDirsR.attribute("Delete", std::get<DirectionByChange>(dirCfg.dirs).right.delete_);
        }
    }
}


void readConfig(const XmlIn& in, SyncConfig& syncCfg, std::map<AfsDevice, size_t>& deviceParallelOps, int formatVer)
{
    readConfig(in, syncCfg.directionCfg, formatVer);

    in["DeletionPolicy"  ](syncCfg.deletionVariant);
    in["VersioningFolder"](syncCfg.versioningFolderPhrase);

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

        if (verFolder.hasAttribute("MinCount"))
            verFolder.attribute("MinCount", syncCfg.versionCountMin); // => *no error* if not available
        if (verFolder.hasAttribute("MaxCount"))
            verFolder.attribute("MaxCount", syncCfg.versionCountMax); //
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

    in["SizeMin"](filter.sizeMin);
    in["SizeMin"].attribute("Unit", filter.unitSizeMin);

    in["SizeMax"](filter.sizeMax);
    in["SizeMax"].attribute("Unit", filter.unitSizeMax);

    in["TimeSpan"](filter.timeSpan);
    in["TimeSpan"].attribute("Type", filter.unitTimeSpan);
}


void readConfig(const XmlIn& in, LocalPairConfig& lpc, std::map<AfsDevice, size_t>& deviceParallelOps, int formatVer)
{
    //read folder pairs
    in["Left" ](lpc.folderPathPhraseLeft);
    in["Right"](lpc.folderPathPhraseRight);

    size_t parallelOpsL = 1;
    size_t parallelOpsR = 1;
    if (in["Left" ].hasAttribute("Threads")) in["Left" ].attribute("Threads", parallelOpsL); //try to get attributes:
    if (in["Right"].hasAttribute("Threads")) in["Right"].attribute("Threads", parallelOpsR); // => *no error* if not available

    auto setParallelOps = [&](const Zstring& folderPathPhrase, size_t parallelOps)
    {
        const size_t parallelOpsPrev = getDeviceParallelOps(deviceParallelOps, folderPathPhrase);
        /**/                           setDeviceParallelOps(deviceParallelOps, folderPathPhrase, std::max(parallelOps, parallelOpsPrev));
    };
    setParallelOps(lpc.folderPathPhraseLeft,  parallelOpsL);
    setParallelOps(lpc.folderPathPhraseRight, parallelOpsR);

    //TODO: remove after migration! 2020-04-24
    if (formatVer < 16)
    {
        replaceAsciiNoCase(lpc.folderPathPhraseLeft,  Zstr("%weekday%"), Zstr("%WeekDayName%"));
        replaceAsciiNoCase(lpc.folderPathPhraseRight, Zstr("%weekday%"), Zstr("%WeekDayName%"));
    }

    //###########################################################
    //alternate comp configuration (optional)
    if (XmlIn inLocalCmp = in["Compare"])
    {
        CompConfig cmpCfg;
        readConfig(inLocalCmp, cmpCfg);

        lpc.localCmpCfg = cmpCfg;
    }
    //###########################################################
    //alternate sync configuration (optional)
    if (XmlIn inLocalSync = in["Synchronize"])
    {
        SyncConfig syncCfg;
        readConfig(inLocalSync, syncCfg, deviceParallelOps, formatVer);

        lpc.localSyncCfg = syncCfg;
    }

    //###########################################################
    //alternate filter configuration
    if (XmlIn inLocFilter = in["Filter"])
        readConfig(inLocFilter, lpc.localFilter);
}


void readConfig(const XmlIn& in, MainConfiguration& mainCfg, int formatVer)
{
    if (formatVer < 18) //TODO: remove if parameter migration after some time! 2023-05-15
        ;
    else
        in["Notes"](mainCfg.notes);

    readConfig(in["Compare"], mainCfg.cmpCfg);

    readConfig(in["Synchronize"], mainCfg.syncCfg, mainCfg.deviceParallelOps, formatVer);

    if (formatVer < 20) //TODO: remove if parameter migration after some time! 2023-08-09
        if (mainCfg.cmpCfg.compareVar == CompareVariant::content ||
            mainCfg.cmpCfg.compareVar == CompareVariant::size)
            if (std::string varName;
                in["Synchronize"]["Variant"](varName))
            {
                if (varName == "Update")
                    std::get<DirectionByDiff>(mainCfg.syncCfg.directionCfg.dirs).rightNewer = SyncDirection::right;
                else if (varName == "Custom")
                {
                    SyncDirection different = SyncDirection::none;
                    in["Synchronize"]["CustomDirections"]["Different"](different);

                    std::get<DirectionByDiff>(mainCfg.syncCfg.directionCfg.dirs).leftNewer =
                        std::get<DirectionByDiff>(mainCfg.syncCfg.directionCfg.dirs).rightNewer = different;
                }
            }

    if (formatVer < 23) //TODO: remove if parameter migration after some time! 2023-08-24
    {
        bool detectMovedFiles = false;
        in["Synchronize"]["DetectMovedFiles"](detectMovedFiles);
        if (detectMovedFiles)
            if (getSyncVariant(mainCfg.syncCfg.directionCfg) == SyncVariant::mirror)
            {
                if (const DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&mainCfg.syncCfg.directionCfg.dirs))
                    mainCfg.syncCfg.directionCfg.dirs = getChangesDirDefault(*diffDirs); //convert to "changes"-based mirror, so that move detection is enabled
                else assert(false);
            }
    }

    readConfig(in["Filter"], mainCfg.globalFilter);

    //###########################################################
    //read folder pairs
    bool firstItem = true;
    in["FolderPairs"].visitChildren([&](const XmlIn& inPair)
    {
        assert(*inPair.getName() == "Pair");

        LocalPairConfig lpc;
        readConfig(inPair, lpc, mainCfg.deviceParallelOps, formatVer);

        if (formatVer < 20) //TODO: remove if parameter migration after some time! 2023-08-09
            if (lpc.localSyncCfg)
            {
                const CompConfig& cmpCfg = lpc.localCmpCfg ? *lpc.localCmpCfg : mainCfg.cmpCfg;
                if (cmpCfg.compareVar == CompareVariant::content ||
                    cmpCfg.compareVar == CompareVariant::size)
                    if (std::string varName;
                        inPair["Synchronize"]["Variant"](varName))
                    {
                        if (varName == "Update")
                            std::get<DirectionByDiff>(lpc.localSyncCfg->directionCfg.dirs).rightNewer = SyncDirection::right;
                        else if (varName == "Custom")
                            if (inPair["Synchronize"]["CustomDirections"]["Different"])
                            {
                                SyncDirection different = SyncDirection::none;
                                inPair["Synchronize"]["CustomDirections"]["Different"](different);

                                std::get<DirectionByDiff>(lpc.localSyncCfg->directionCfg.dirs).leftNewer =
                                    std::get<DirectionByDiff>(lpc.localSyncCfg->directionCfg.dirs).rightNewer = different;
                            }
                    }
            }
        if (formatVer < 23) //TODO: remove if parameter migration after some time! 2023-08-24
            if (lpc.localSyncCfg)
            {
                bool detectMovedFiles = false;
                inPair["Synchronize"]["DetectMovedFiles"](detectMovedFiles);
                if (detectMovedFiles)
                    if (getSyncVariant(lpc.localSyncCfg->directionCfg) == SyncVariant::mirror)
                    {
                        if (const DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&lpc.localSyncCfg->directionCfg.dirs))
                            lpc.localSyncCfg->directionCfg.dirs = getChangesDirDefault(*diffDirs); //convert to "changes"-based mirror, so that move detection is enabled
                        else assert(false);
                    }
            }

        if (firstItem)
        {
            firstItem = false;
            mainCfg.firstPair = lpc;
            mainCfg.additionalPairs.clear();
        }
        else
            mainCfg.additionalPairs.push_back(lpc);
    });

    in["Errors"].attribute("Ignore", mainCfg.ignoreErrors);
    in["Errors"].attribute("Retry",  mainCfg.autoRetryCount);
    in["Errors"].attribute("Delay",  mainCfg.autoRetryDelay);

    in["PostSyncCommand"](mainCfg.postSyncCommand);
    in["PostSyncCommand"].attribute("Condition", mainCfg.postSyncCondition);

    in["LogFolder"](mainCfg.altLogFolderPathPhrase);

    //TODO: remove after migration! 2020-04-24
    if (formatVer < 16)
        replaceAsciiNoCase(mainCfg.altLogFolderPathPhrase,  Zstr("%weekday%"), Zstr("%WeekDayName%"));

    //TODO: remove if parameter migration after some time! 2020-01-30
    if (formatVer < 15)
        ;
    else
    {
        in["EmailNotification"](mainCfg.emailNotifyAddress);
        in["EmailNotification"].attribute("Condition", mainCfg.emailNotifyCondition);
    }
}


void readConfig(const XmlIn& in, XmlGuiConfig& cfg, int formatVer)
{
    readConfig(in, cfg.mainCfg, formatVer);

    if (formatVer < 19) //TODO: remove after migration! 2023-06-09
    {
        XmlIn inGui = in["Gui"];
        //TODO: remove after migration! 2020-10-14
        if (formatVer < 17)
        {
            if (inGui["MiddleGridView"])
            {
                std::string tmp;
                inGui["MiddleGridView"](tmp);

                if (tmp == "Category")
                    cfg.gridViewType = GridViewType::difference;
                else if (tmp == "Action")
                    cfg.gridViewType = GridViewType::action;
            }
        }
        else
            inGui["GridViewType"](cfg.gridViewType);
    }
    else
        in["GridViewType"](cfg.gridViewType);
}


void readConfig(const XmlIn& in, XmlBatchConfig& cfg, int formatVer)
{
    if (formatVer < 19) //TODO: remove after migration! 2023-06-09
        readConfig(in, cfg.guiCfg.mainCfg, formatVer);
    else
        readConfig(in, cfg.guiCfg, formatVer);

    XmlIn inBatch = in["Batch"];
    inBatch["ProgressDialog"].attribute("Minimized", cfg.batchExCfg.runMinimized);
    inBatch["ProgressDialog"].attribute("AutoClose", cfg.batchExCfg.autoCloseSummary);
    inBatch["ErrorDialog"](cfg.batchExCfg.batchErrorHandling);
    inBatch["PostSyncAction"](cfg.batchExCfg.postBatchAction);
}


void readConfig(const XmlIn& in, XmlGlobalSettings& cfg, int formatVer)
{
    assert(cfg.dpiLayouts.empty());

    XmlIn in2 = in;

    if (in["General"]) //TODO: remove old parameter after migration! 2020-12-03
        in2 = in["General"];

    //TODO: remove after migration! 2022-04-18
    if (in2["Language"].hasAttribute("Name"))
    {
        std::string lngName;
        in2["Language"].attribute("Name", lngName);

        if (lngName == "English (US)")
            cfg.programLanguage = wxLANGUAGE_ENGLISH_US;
        else if (lngName == "Chinese (Simplified)")
            cfg.programLanguage = wxLANGUAGE_CHINESE_CHINA;
        else if (lngName == "Chinese (Traditional)")
            cfg.programLanguage = wxLANGUAGE_CHINESE_TAIWAN;
        else if (lngName == "English (U.K.)")
            cfg.programLanguage = wxLANGUAGE_ENGLISH_UK;
        else if (lngName == "Norwegian (Bokmal)")
            cfg.programLanguage = wxLANGUAGE_NORWEGIAN;
        else if (lngName == "Portuguese (Brazilian)")
            cfg.programLanguage = wxLANGUAGE_PORTUGUESE_BRAZILIAN;
        else if (const wxLanguageInfo* lngInfo = wxUILocale::FindLanguageInfo(utfTo<wxString>(lngName)))
            cfg.programLanguage = static_cast<wxLanguage>(lngInfo->Language);
    }
    else
        in2["Language"].attribute("Code", cfg.programLanguage);

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
        cfg.dpiLayouts[getDpiScalePercent()].progressDlg.size = wxSize();
        in2["ProgressDialog"].attribute("Width",     cfg.dpiLayouts[getDpiScalePercent()].progressDlg.size->x);
        in2["ProgressDialog"].attribute("Height",    cfg.dpiLayouts[getDpiScalePercent()].progressDlg.size->y);
        in2["ProgressDialog"].attribute("Maximized", cfg.dpiLayouts[getDpiScalePercent()].progressDlg.isMaximized);
    }

    in2["ProgressDialog"].attribute("AutoClose", cfg.progressDlgAutoClose);

    XmlIn inOpt = in2["OptionalDialogs"];
    inOpt["ConfirmStartSync"                ].attribute("Show", cfg.confirmDlgs.confirmSyncStart);
    inOpt["ConfirmSaveConfig"               ].attribute("Show", cfg.confirmDlgs.confirmSaveConfig);
    inOpt["ConfirmSwapSides"                ].attribute("Show", cfg.confirmDlgs.confirmSwapSides);
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
    inOpt["WarnDependentFolderPair"       ].attribute("Show", cfg.warnDlgs.warnDependentFolderPair);
    inOpt["WarnDependentBaseFolders"      ].attribute("Show", cfg.warnDlgs.warnDependentBaseFolders);
    inOpt["WarnDirectoryLockFailed"       ].attribute("Show", cfg.warnDlgs.warnDirectoryLockFailed);
    inOpt["WarnVersioningFolderPartOfSync"].attribute("Show", cfg.warnDlgs.warnVersioningFolderPartOfSync);

    //TODO: remove after migration! 2022-08-26
    if (formatVer < 25)
        cfg.warnDlgs.warnDependentBaseFolders = true; //new semantics! should not be ignored

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
        if (!cfg.soundFileCompareFinished.empty()) cfg.soundFileCompareFinished = appendPath(getResourceDirPath(), cfg.soundFileCompareFinished);
        if (!cfg.soundFileSyncFinished   .empty()) cfg.soundFileSyncFinished    = appendPath(getResourceDirPath(), cfg.soundFileSyncFinished);
    }
    else
    {
        cfg.soundFileCompareFinished = resolvePortablePath(cfg.soundFileCompareFinished);
        cfg.soundFileSyncFinished    = resolvePortablePath(cfg.soundFileSyncFinished);
        cfg.soundFileAlertPending    = resolvePortablePath(cfg.soundFileAlertPending);
    }

    XmlIn inMainWin = in["MainDialog"];

    //TODO: remove old parameter after migration! 2020-12-03
    if (in["Gui"])
        inMainWin = in["Gui"]["MainDialog"];

    //TODO: remove old parameter after migration! 2021-03-06
    if (formatVer < 21)
    {
        cfg.dpiLayouts[getDpiScalePercent()].mainDlg.size = wxSize();
        inMainWin.attribute("Width",     cfg.dpiLayouts[getDpiScalePercent()].mainDlg.size->x);
        inMainWin.attribute("Height",    cfg.dpiLayouts[getDpiScalePercent()].mainDlg.size->y);
        cfg.dpiLayouts[getDpiScalePercent()].mainDlg.pos  = wxPoint();
        inMainWin.attribute("PosX",      cfg.dpiLayouts[getDpiScalePercent()].mainDlg.pos->x);
        inMainWin.attribute("PosY",      cfg.dpiLayouts[getDpiScalePercent()].mainDlg.pos->y);
        inMainWin.attribute("Maximized", cfg.dpiLayouts[getDpiScalePercent()].mainDlg.isMaximized);
    }

    //###########################################################

    inMainWin["SearchPanel"].attribute("CaseSensitive", cfg.mainDlg.textSearchRespectCase);

    //###########################################################

    XmlIn inConfig = inMainWin["ConfigPanel"];
    inConfig.attribute("ScrollPos",     cfg.mainDlg.config.topRowPos);
    inConfig.attribute("SyncOverdue",   cfg.mainDlg.config.syncOverdueDays);
    inConfig.attribute("SortByColumn",  cfg.mainDlg.config.lastSortColumn);
    inConfig.attribute("SortAscending", cfg.mainDlg.config.lastSortAscending);

    //TODO: remove old parameter after migration! 2021-03-06
    if (formatVer < 21)
        inConfig["Columns"](cfg.dpiLayouts[getDpiScalePercent()].configColumnAttribs);

    inConfig["Configurations"].attribute("MaxSize",      cfg.mainDlg.config.histItemsMax);
    inConfig["Configurations"].attribute("LastSelected", cfg.mainDlg.config.lastSelectedFile);
    cfg.mainDlg.config.lastSelectedFile  = resolvePortablePath(cfg.mainDlg.config.lastSelectedFile);

    inConfig["Configurations"](cfg.mainDlg.config.fileHistory);

    //TODO: remove after migration! 2019-11-30
    if (formatVer < 15)
    {
        const Zstring lastRunConfigPath = appendPath(getConfigDirPath(), Zstr("LastRun.ffs_gui"));
        for (ConfigFileItem& item : cfg.mainDlg.config.fileHistory)
            if (equalNativePath(item.cfgFilePath, lastRunConfigPath))
                item.backColor = wxColor(0xdd, 0xdd, 0xdd); //light grey from onCfgGridContext()
    }

    inConfig["LastUsed"](cfg.mainDlg.config.lastUsedFiles);
    cfg.mainDlg.config.lastUsedFiles = resolvePortablePath(cfg.mainDlg.config.lastUsedFiles);

    //###########################################################

    XmlIn inOverview = inMainWin["OverviewPanel"];
    inOverview.attribute("ShowPercentage", cfg.mainDlg.overview.showPercentBar);
    inOverview.attribute("SortByColumn",   cfg.mainDlg.overview.lastSortColumn);
    inOverview.attribute("SortAscending",  cfg.mainDlg.overview.lastSortAscending);

    //TODO: remove old parameter after migration! 2021-03-06
    if (formatVer < 21)
        inOverview["Columns"](cfg.dpiLayouts[getDpiScalePercent()].overviewColumnAttribs);

    XmlIn inFilePanel = inMainWin["FilePanel"];

    //TODO: remove after migration! 2020-10-13
    if (formatVer < 19)
        ; //new icon layout => let user re-evaluate settings
    else
    {
        inFilePanel.attribute("ShowIcons", cfg.mainDlg.showIcons);
        inFilePanel.attribute("IconSize",  cfg.mainDlg.iconSize);
    }
    inFilePanel.attribute("SashOffset", cfg.mainDlg.sashOffset);

    //TODO: remove if parameter migration after some time! 2020-01-30
    if (formatVer < 16)
        inFilePanel.attribute("MaxFolderPairsShown", cfg.mainDlg.folderPairsVisibleMax);
    else
        inFilePanel.attribute("FolderPairsMax", cfg.mainDlg.folderPairsVisibleMax);

    //TODO: remove old parameter after migration! 2021-03-06
    if (formatVer < 21)
    {
        inFilePanel["ColumnsLeft" ](cfg.dpiLayouts[getDpiScalePercent()].fileColumnAttribsLeft);
        inFilePanel["ColumnsRight"](cfg.dpiLayouts[getDpiScalePercent()].fileColumnAttribsRight);

        inFilePanel["ColumnsLeft" ].attribute("PathFormat", cfg.mainDlg.itemPathFormatLeftGrid);
        inFilePanel["ColumnsRight"].attribute("PathFormat", cfg.mainDlg.itemPathFormatRightGrid);
    }
    else
    {
        inFilePanel.attribute("PathFormatLeft",  cfg.mainDlg.itemPathFormatLeftGrid);
        inFilePanel.attribute("PathFormatRight", cfg.mainDlg.itemPathFormatRightGrid);
    }

    inFilePanel["FolderHistoryLeft" ](cfg.mainDlg.folderHistoryLeft);
    inFilePanel["FolderHistoryRight"](cfg.mainDlg.folderHistoryRight);
    cfg.mainDlg.folderHistoryLeft  = resolvePortablePath(cfg.mainDlg.folderHistoryLeft);
    cfg.mainDlg.folderHistoryRight = resolvePortablePath(cfg.mainDlg.folderHistoryRight);

    inFilePanel["FolderHistoryLeft" ].attribute("LastSelected", cfg.mainDlg.folderLastSelectedLeft);
    inFilePanel["FolderHistoryRight"].attribute("LastSelected", cfg.mainDlg.folderLastSelectedRight);
    cfg.mainDlg.folderLastSelectedLeft  = resolvePortablePath(cfg.mainDlg.folderLastSelectedLeft);
    cfg.mainDlg.folderLastSelectedRight = resolvePortablePath(cfg.mainDlg.folderLastSelectedRight);

    //###########################################################
    XmlIn inCopyTo = inMainWin["ManualCopyTo"];
    inCopyTo.attribute("KeepRelativePaths", cfg.mainDlg.copyToCfg.keepRelPaths);
    inCopyTo.attribute("OverwriteIfExists", cfg.mainDlg.copyToCfg.overwriteIfExists);

    XmlIn inCopyToHistory = inCopyTo["FolderHistory"];

    inCopyToHistory(cfg.mainDlg.copyToCfg.folderHistory);
    inCopyToHistory.attribute("TargetFolder", cfg.mainDlg.copyToCfg.targetFolderPath);
    inCopyToHistory.attribute("LastSelected", cfg.mainDlg.copyToCfg.targetFolderLastSelected);
    cfg.mainDlg.copyToCfg.folderHistory            = resolvePortablePath(cfg.mainDlg.copyToCfg.folderHistory);
    cfg.mainDlg.copyToCfg.targetFolderPath         = resolvePortablePath(cfg.mainDlg.copyToCfg.targetFolderPath);
    cfg.mainDlg.copyToCfg.targetFolderLastSelected = resolvePortablePath(cfg.mainDlg.copyToCfg.targetFolderLastSelected);
    //###########################################################

    XmlIn inDefFilter = inMainWin["DefaultViewFilter"];

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


    //TODO: remove old parameter after migration! 2021-03-06
    if (formatVer < 21)
        inMainWin["Perspective"](cfg.dpiLayouts[getDpiScalePercent()].panelLayout);

    //TODO: remove after migration! 2019-11-30
    auto splitEditMerge = [](wxString& perspective, wchar_t delim, const std::function<void(wxString& item)>& editItem)
    {
        std::vector<wxString> v = splitCpy(perspective, delim, SplitOnEmpty::allow);
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

    //TODO: remove after migration! 2019-11-30
    if (formatVer < 15)
    {
        //set minimal TopPanel height => search and set actual height to 0 and let MainDialog's min-size handling kick in:
        std::optional<int> tpDir;
        std::optional<int> tpLayer;
        std::optional<int> tpRow;
        splitEditMerge(cfg.dpiLayouts[getDpiScalePercent()].panelLayout, L'|', [&](wxString& paneCfg)
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

            splitEditMerge(cfg.dpiLayouts[getDpiScalePercent()].panelLayout, L'|', [&](wxString& paneCfg)
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
        cfg.sftpKeyFileLastSelected = resolvePortablePath(cfg.sftpKeyFileLastSelected);
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
        cfg.versioningFolderLastSelected = resolvePortablePath(cfg.versioningFolderLastSelected);
    }
    in["LogFolder"](cfg.logFolderPhrase);
    cfg.logFolderPhrase = resolvePortablePath(cfg.logFolderPhrase);

    if (formatVer < 20) //TODO: remove old parameter after migration! 2020-12-03
    {
        in["Gui"]["LogFolderHistory"](cfg.logFolderHistory);
        in["Gui"]["LogFolderHistory"].attribute("LastSelected", cfg.logFolderLastSelected);
    }
    else
    {
        in["LogFolderHistory"](cfg.logFolderHistory);
        in["LogFolderHistory"].attribute("LastSelected", cfg.logFolderLastSelected);
        cfg.logFolderHistory      = resolvePortablePath(cfg.logFolderHistory);
        cfg.logFolderLastSelected = resolvePortablePath(cfg.logFolderLastSelected);
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

    if (formatVer < 20) //TODO: remove old parameter after migration! 2020-12-03
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


    if (formatVer < 20) //TODO: remove old parameter after migration! 2020-12-03
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

    //TODO: remove after migration! 2022-04-29
    if (formatVer < 24)
        for (ExternalApp& item : cfg.externalApps)
            if (item.description == L"Browse directory")
                item.description = L"Show in file manager";

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

    in["WelcomeDialogVersion"](cfg.welcomeDialogLastVersion);

    //cfg.dpiLayouts.clear(); -> NO: honor migration code above!

    in["DpiLayouts"].visitChildren([&](const XmlIn& inLayout)
    {
        assert(*inLayout.getName() == "Layout");
        if (std::string scaleTxt;
            inLayout.attribute("Scale", scaleTxt))
        {
            const int scalePercent = stringTo<int>(beforeLast(scaleTxt, '%', IfNotFoundReturn::none));
            DpiLayout layout;

            //TODO: remove parameter migration after some time! 2023-02-18
            if (formatVer < 26)
            {
                XmlIn inLayoutMain = inLayout["MainDialog"];
                layout.mainDlg.size = wxSize();
                inLayoutMain.attribute("Width",     layout.mainDlg.size->x);
                inLayoutMain.attribute("Height",    layout.mainDlg.size->y);

                layout.mainDlg.pos = wxPoint();
                inLayoutMain.attribute("PosX",      layout.mainDlg.pos->x);
                inLayoutMain.attribute("PosY",      layout.mainDlg.pos->y);

                inLayoutMain.attribute("Maximized", layout.mainDlg.isMaximized);

                inLayoutMain["PanelLayout"   ](layout.panelLayout);
                inLayoutMain["ConfigPanel"   ](layout.configColumnAttribs);
                inLayoutMain["OverviewPanel" ](layout.overviewColumnAttribs);
                inLayoutMain["FilePanelLeft" ](layout.fileColumnAttribsLeft);
                inLayoutMain["FilePanelRight"](layout.fileColumnAttribsRight);

                XmlIn inLayoutProgress = inLayout["ProgressDialog"];
                layout.progressDlg.size = wxSize();
                inLayoutProgress.attribute("Width",  layout.progressDlg.size->x);
                inLayoutProgress.attribute("Height", layout.progressDlg.size->y);

                inLayoutProgress.attribute("Maximized", layout.progressDlg.isMaximized);
            }
            else
            {
                XmlIn inLayoutMain = inLayout["MainWindow"];
                if (inLayoutMain.hasAttribute("Width") &&
                    inLayoutMain.hasAttribute("Height"))
                {
                    layout.mainDlg.size = wxSize();
                    inLayoutMain.attribute("Width",  layout.mainDlg.size->x);
                    inLayoutMain.attribute("Height", layout.mainDlg.size->y);
                }
                if (inLayoutMain.hasAttribute("PosX") &&
                    inLayoutMain.hasAttribute("PosY"))
                {
                    layout.mainDlg.pos = wxPoint();
                    inLayoutMain.attribute("PosX", layout.mainDlg.pos->x);
                    inLayoutMain.attribute("PosY", layout.mainDlg.pos->y);
                }
                inLayoutMain.attribute("Maximized", layout.mainDlg.isMaximized);

                XmlIn inLayoutProgress = inLayout["ProgressDialog"];
                if (inLayoutProgress.hasAttribute("Width") &&
                    inLayoutProgress.hasAttribute("Height"))
                {
                    layout.progressDlg.size = wxSize();
                    inLayoutProgress.attribute("Width",  layout.progressDlg.size->x);
                    inLayoutProgress.attribute("Height", layout.progressDlg.size->y);
                }
                inLayoutProgress.attribute("Maximized", layout.progressDlg.isMaximized);

                inLayout["Panels"        ](layout.panelLayout);
                inLayout["ConfigPanel"   ](layout.configColumnAttribs);
                inLayout["OverviewPanel" ](layout.overviewColumnAttribs);
                inLayout["FilePanelLeft" ](layout.fileColumnAttribsLeft);
                inLayout["FilePanelRight"](layout.fileColumnAttribsRight);
            }

            cfg.dpiLayouts.emplace(scalePercent, std::move(layout));
        }
    });
}


template <class ConfigType>
std::pair<ConfigType, std::wstring /*warningMsg*/> parseConfig(const XmlDoc& doc, const Zstring& filePath, int currentXmlFormatVer) //noexcept
{
    int formatVer = 0;
    /*bool success =*/ doc.root().getAttribute("XmlFormat", formatVer);

    XmlIn in(doc);
    ConfigType cfg;
    readConfig(in, cfg, formatVer);

    std::wstring warningMsg;

    if (const std::wstring& errors = in.getErrors();
        !errors.empty())
        warningMsg = replaceCpy(_("Configuration file %x is incomplete. The missing elements have been set to their default values."), L"%x", fmtPath(filePath)) + L"\n\n" +
                     _("The following XML elements could not be read:") + L'\n' + errors;
    else //(try to) migrate old configuration if needed
        if (formatVer < currentXmlFormatVer)
            try
            {
                fff::writeConfig(cfg, filePath); //throw FileError
            }
            catch (const FileError& e) { warningMsg = e.toString(); }

    return {cfg, warningMsg};
}


template <class ConfigType>
std::pair<ConfigType, std::wstring /*warningMsg*/> readConfig(const Zstring& filePath, const char* expectedCfgType, int currentXmlFormatVer) //throw FileError
{
    XmlDoc doc = loadXml(filePath); //throw FileError

    const std::string cfgType = [&]
    {
        if (doc.root().getName() == "FreeFileSync")
        {
            std::string type;
            if (doc.root().getAttribute("XmlType", type))
                return type;
        }
        return std::string();
    }();
    if (cfgType != expectedCfgType)
        throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));

    return parseConfig<ConfigType>(doc, filePath, currentXmlFormatVer);
}
}


std::pair<XmlGuiConfig, std::wstring /*warningMsg*/> fff::readGuiConfig(const Zstring& filePath)
{
    return readConfig<XmlGuiConfig>(filePath, "GUI", XML_FORMAT_SYNC_CFG); //throw FileError
}


std::pair<XmlBatchConfig, std::wstring /*warningMsg*/> fff::readBatchConfig(const Zstring& filePath)
{
    return readConfig<XmlBatchConfig>(filePath, "BATCH", XML_FORMAT_SYNC_CFG); //throw FileError
}


std::pair<XmlGlobalSettings, std::wstring /*warningMsg*/> fff::readGlobalConfig(const Zstring& filePath)
{
    return readConfig<XmlGlobalSettings>(filePath, "GLOBAL", XML_FORMAT_GLOBAL_CFG); //throw FileError
}


std::pair<XmlGuiConfig, std::wstring /*warningMsg*/> fff::readAnyConfig(const std::vector<Zstring>& filePaths) //throw FileError
{
    assert(!filePaths.empty());

    XmlGuiConfig cfg;
    std::wstring warningMsgAll;
    std::vector<MainConfiguration> mainCfgs;

    for (auto it = filePaths.begin(); it != filePaths.end(); ++it)
    {
        const bool firstItem = it == filePaths.begin(); //init all non-"mainCfg" settings with first config file
        const Zstring& filePath = *it;

        if (endsWithAsciiNoCase(filePath, Zstr(".ffs_gui")))
        {
            const auto& [guiCfg, warningMsg] = readGuiConfig(filePath); //throw FileError
            if (firstItem)
                cfg = guiCfg;
            mainCfgs.push_back(guiCfg.mainCfg);

            if (!warningMsg.empty())
                warningMsgAll += warningMsg + L"\n\n";
        }
        else if (endsWithAsciiNoCase(filePath, Zstr(".ffs_batch")))
        {
            const auto& [batchCfg, warningMsg] = readBatchConfig(filePath); //throw FileError
            if (firstItem)
                cfg = batchCfg.guiCfg;
            mainCfgs.push_back(batchCfg.guiCfg.mainCfg);

            if (!warningMsg.empty())
                warningMsgAll += warningMsg + L"\n\n";
        }
        else
            throw FileError(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(filePath)),
                            _("Unexpected file extension:") + L' ' + fmtPath(getFileExtension(filePath)) + L'\n' +
                            _("Expected:") + L" ffs_gui, ffs_batch");
    }
    cfg.mainCfg = merge(mainCfgs);

    return {cfg, trimCpy(warningMsgAll)};
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
    if (const DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&dirCfg.dirs))
    {
        XmlOut outDirs = out["Differences"];
        outDirs.attribute("LeftOnly",   diffDirs->leftOnly);
        outDirs.attribute("LeftNewer",  diffDirs->leftNewer);
        outDirs.attribute("RightNewer", diffDirs->rightNewer);
        outDirs.attribute("RightOnly",  diffDirs->rightOnly);
    }
    else
    {
        const DirectionByChange& changeDirs = std::get<DirectionByChange>(dirCfg.dirs);

        XmlOut outDirsL = out["Changes"]["Left"];
        outDirsL.attribute("Create", changeDirs.left.create);
        outDirsL.attribute("Update", changeDirs.left.update);
        outDirsL.attribute("Delete", changeDirs.left.delete_);

        XmlOut outDirsR = out["Changes"]["Right"];
        outDirsR.attribute("Create", changeDirs.right.create);
        outDirsR.attribute("Update", changeDirs.right.update);
        outDirsR.attribute("Delete", changeDirs.right.delete_);
    }
}


void writeConfig(const SyncConfig& syncCfg, const std::map<AfsDevice, size_t>& deviceParallelOps, XmlOut& out)
{
    writeConfig(syncCfg.directionCfg, out);

    out["DeletionPolicy"  ](syncCfg.deletionVariant);
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

    out["SizeMin"](filter.sizeMin);
    out["SizeMin"].attribute("Unit", filter.unitSizeMin);

    out["SizeMax"](filter.sizeMax);
    out["SizeMax"].attribute("Unit", filter.unitSizeMax);

    out["TimeSpan"](filter.timeSpan);
    out["TimeSpan"].attribute("Type", filter.unitTimeSpan);
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
    out["Notes"](mainCfg.notes);

    XmlOut outCmp = out["Compare"];
    writeConfig(mainCfg.cmpCfg, outCmp);
    //###########################################################

    XmlOut outSync = out["Synchronize"];
    writeConfig(mainCfg.syncCfg, mainCfg.deviceParallelOps, outSync);
    //###########################################################

    XmlOut outFilter = out["Filter"];
    writeConfig(mainCfg.globalFilter, outFilter);

    //###########################################################
    XmlOut outFp = out["FolderPairs"];
    //write folder pairs
    writeConfig(mainCfg.firstPair, mainCfg.deviceParallelOps, outFp);

    for (const LocalPairConfig& lpc : mainCfg.additionalPairs)
        writeConfig(lpc, mainCfg.deviceParallelOps, outFp);

    out["Errors"].attribute("Ignore", mainCfg.ignoreErrors);
    out["Errors"].attribute("Retry",  mainCfg.autoRetryCount);
    out["Errors"].attribute("Delay",  mainCfg.autoRetryDelay);

    out["PostSyncCommand"](mainCfg.postSyncCommand);
    out["PostSyncCommand"].attribute("Condition", mainCfg.postSyncCondition);

    out["LogFolder"](mainCfg.altLogFolderPathPhrase);

    out["EmailNotification"](mainCfg.emailNotifyAddress);
    out["EmailNotification"].attribute("Condition", mainCfg.emailNotifyCondition);
}


void writeConfig(const XmlGuiConfig& cfg, XmlOut& out)
{
    writeConfig(cfg.mainCfg, out); //write main config

    out["GridViewType"](cfg.gridViewType);
}


void writeConfig(const XmlBatchConfig& cfg, XmlOut& out)
{
    writeConfig(cfg.guiCfg, out);

    XmlOut outBatch = out["Batch"];
    outBatch["ProgressDialog"].attribute("Minimized", cfg.batchExCfg.runMinimized);
    outBatch["ProgressDialog"].attribute("AutoClose", cfg.batchExCfg.autoCloseSummary);
    outBatch["ErrorDialog"   ](cfg.batchExCfg.batchErrorHandling);
    outBatch["PostSyncAction"](cfg.batchExCfg.postBatchAction);
}


void writeConfig(const XmlGlobalSettings& cfg, XmlOut& out)
{
    out["Language"].attribute("Code", cfg.programLanguage);

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
    outOpt["ConfirmSwapSides"              ].attribute("Show", cfg.confirmDlgs.confirmSwapSides);
    outOpt["ConfirmCommandMassInvoke"      ].attribute("Show", cfg.confirmDlgs.confirmCommandMassInvoke);
    outOpt["WarnFolderNotExisting"         ].attribute("Show", cfg.warnDlgs.warnFolderNotExisting);
    outOpt["WarnFoldersDifferInCase"       ].attribute("Show", cfg.warnDlgs.warnFoldersDifferInCase);
    outOpt["WarnUnresolvedConflicts"       ].attribute("Show", cfg.warnDlgs.warnUnresolvedConflicts);
    outOpt["WarnNotEnoughDiskSpace"        ].attribute("Show", cfg.warnDlgs.warnNotEnoughDiskSpace);
    outOpt["WarnSignificantDifference"     ].attribute("Show", cfg.warnDlgs.warnSignificantDifference);
    outOpt["WarnRecycleBinNotAvailable"    ].attribute("Show", cfg.warnDlgs.warnRecyclerMissing);
    outOpt["WarnInputFieldEmpty"           ].attribute("Show", cfg.warnDlgs.warnInputFieldEmpty);
    outOpt["WarnDependentFolderPair"       ].attribute("Show", cfg.warnDlgs.warnDependentFolderPair);
    outOpt["WarnDependentBaseFolders"      ].attribute("Show", cfg.warnDlgs.warnDependentBaseFolders);
    outOpt["WarnDirectoryLockFailed"       ].attribute("Show", cfg.warnDlgs.warnDirectoryLockFailed);
    outOpt["WarnVersioningFolderPartOfSync"].attribute("Show", cfg.warnDlgs.warnVersioningFolderPartOfSync);

    out["Sounds"]["CompareFinished"].attribute("Path", makePortablePath(cfg.soundFileCompareFinished));
    out["Sounds"]["SyncFinished"   ].attribute("Path", makePortablePath(cfg.soundFileSyncFinished));
    out["Sounds"]["AlertPending"   ].attribute("Path", makePortablePath(cfg.soundFileAlertPending));

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
    outConfig["Configurations"].attribute("LastSelected", makePortablePath(cfg.mainDlg.config.lastSelectedFile));
    outConfig["Configurations"](cfg.mainDlg.config.fileHistory);

    outConfig["LastUsed"](makePortablePath(cfg.mainDlg.config.lastUsedFiles));

    //###########################################################

    XmlOut outOverview = outMainWin["OverviewPanel"];
    outOverview.attribute("ShowPercentage", cfg.mainDlg.overview.showPercentBar);
    outOverview.attribute("SortByColumn",   cfg.mainDlg.overview.lastSortColumn);
    outOverview.attribute("SortAscending",  cfg.mainDlg.overview.lastSortAscending);

    XmlOut outFilePanel = outMainWin["FilePanel"];
    outFilePanel.attribute("ShowIcons",  cfg.mainDlg.showIcons);
    outFilePanel.attribute("IconSize",   cfg.mainDlg.iconSize);
    outFilePanel.attribute("SashOffset", cfg.mainDlg.sashOffset);
    outFilePanel.attribute("FolderPairsMax", cfg.mainDlg.folderPairsVisibleMax);
    outFilePanel.attribute("PathFormatLeft",  cfg.mainDlg.itemPathFormatLeftGrid);
    outFilePanel.attribute("PathFormatRight", cfg.mainDlg.itemPathFormatRightGrid);

    outFilePanel["FolderHistoryLeft" ](makePortablePath(cfg.mainDlg.folderHistoryLeft));
    outFilePanel["FolderHistoryRight"](makePortablePath(cfg.mainDlg.folderHistoryRight));

    outFilePanel["FolderHistoryLeft" ].attribute("LastSelected", makePortablePath(cfg.mainDlg.folderLastSelectedLeft));
    outFilePanel["FolderHistoryRight"].attribute("LastSelected", makePortablePath(cfg.mainDlg.folderLastSelectedRight));

    //###########################################################
    XmlOut outCopyTo = outMainWin["ManualCopyTo"];
    outCopyTo.attribute("KeepRelativePaths", cfg.mainDlg.copyToCfg.keepRelPaths);
    outCopyTo.attribute("OverwriteIfExists", cfg.mainDlg.copyToCfg.overwriteIfExists);

    XmlOut outCopyToHistory = outCopyTo["FolderHistory"];

    outCopyToHistory(makePortablePath(cfg.mainDlg.copyToCfg.folderHistory));
    outCopyToHistory.attribute("TargetFolder", makePortablePath(cfg.mainDlg.copyToCfg.targetFolderPath));
    outCopyToHistory.attribute("LastSelected", makePortablePath(cfg.mainDlg.copyToCfg.targetFolderLastSelected));
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

    out["SftpKeyFile"].attribute("LastSelected", makePortablePath(cfg.sftpKeyFileLastSelected));

    XmlOut outFileFilter = out["DefaultFilter"];
    writeConfig(cfg.defaultFilter, outFileFilter);

    out["VersioningFolderHistory"](cfg.versioningFolderHistory);
    out["VersioningFolderHistory"].attribute("LastSelected", makePortablePath(cfg.versioningFolderLastSelected));

    out["LogFolder"](makePortablePath(cfg.logFolderPhrase));
    out["LogFolderHistory"](makePortablePath(cfg.logFolderHistory));
    out["LogFolderHistory"].attribute("LastSelected", makePortablePath(cfg.logFolderLastSelected));

    out["EmailHistory"](cfg.emailHistory);
    out["EmailHistory"].attribute("MaxSize", cfg.emailHistoryMax);

    out["CommandHistory"](cfg.commandHistory);
    out["CommandHistory"].attribute("MaxSize", cfg.commandHistoryMax);

    //external applications
    out["ExternalApps"](cfg.externalApps);

    //last update check
    out["LastOnlineCheck"  ](cfg.lastUpdateCheck);
    out["LastOnlineVersion"](cfg.lastOnlineVersion);

    out["WelcomeDialogVersion"](cfg.welcomeDialogLastVersion);


    for (const auto& [scalePercent, layout] : cfg.dpiLayouts)
    {
        XmlOut outLayout = out["DpiLayouts"].addChild("Layout");
        outLayout.attribute("Scale", numberTo<std::string>(scalePercent) + '%');

        XmlOut outLayoutMain = outLayout["MainWindow"];
        if (layout.mainDlg.size)
        {
            outLayoutMain.attribute("Width",  layout.mainDlg.size->x);
            outLayoutMain.attribute("Height", layout.mainDlg.size->y);
        }
        if (layout.mainDlg.pos)
        {
            outLayoutMain.attribute("PosX", layout.mainDlg.pos->x);
            outLayoutMain.attribute("PosY", layout.mainDlg.pos->y);
        }
        outLayoutMain.attribute("Maximized", layout.mainDlg.isMaximized);

        XmlOut outLayoutProgress = outLayout["ProgressDialog"];
        if (layout.progressDlg.size)
        {
            outLayoutProgress.attribute("Width",  layout.progressDlg.size->x);
            outLayoutProgress.attribute("Height", layout.progressDlg.size->y);
        }
        outLayoutProgress.attribute("Maximized", layout.progressDlg.isMaximized);

        outLayout["Panels"        ](layout.panelLayout);
        outLayout["ConfigPanel"   ](layout.configColumnAttribs);
        outLayout["OverviewPanel" ](layout.overviewColumnAttribs);
        outLayout["FilePanelLeft" ](layout.fileColumnAttribsLeft);
        outLayout["FilePanelRight"](layout.fileColumnAttribsRight);
    }
}


template <class ConfigType>
void writeConfig(const ConfigType& cfg, const char* cfgType, int xmlFormatVer, const Zstring& filePath)
{
    XmlDoc doc("FreeFileSync");
    doc.root().setAttribute("XmlType", cfgType);
    doc.root().setAttribute("XmlFormat", xmlFormatVer);

    XmlOut out(doc);
    writeConfig(cfg, out);

    saveXml(doc, filePath); //throw FileError
}
}

void fff::writeConfig(const XmlGuiConfig& cfg, const Zstring& filePath)
{
    ::writeConfig(cfg, "GUI", XML_FORMAT_SYNC_CFG, filePath); //throw FileError
}


void fff::writeConfig(const XmlBatchConfig& cfg, const Zstring& filePath)
{
    ::writeConfig(cfg, "BATCH", XML_FORMAT_SYNC_CFG, filePath); //throw FileError
}


void fff::writeConfig(const XmlGlobalSettings& cfg, const Zstring& filePath)
{
    ::writeConfig(cfg, "GLOBAL", XML_FORMAT_GLOBAL_CFG, filePath); //throw FileError
}


std::wstring fff::extractJobName(const Zstring& cfgFilePath)
{
    const Zstring fileName = getItemName(cfgFilePath);
    const Zstring jobName  = beforeLast(fileName, Zstr('.'), IfNotFoundReturn::all);
    return utfTo<std::wstring>(jobName);
}


std::string fff::serializeFilter(const FilterConfig& filterCfg)
{
    XmlDoc doc("Filter");
    doc.setEncoding("");

    XmlOut out(doc);
    ::writeConfig(filterCfg, out);

    return serializeXml(doc); //noexcept
}


std::optional<FilterConfig> fff::parseFilterBuf(const std::string& filterBuf)
{
    try
    {
        XmlDoc doc = parseXml(filterBuf); //throw XmlParsingError
        XmlIn in(doc);

        FilterConfig filterCfg;
        ::readConfig(in, filterCfg);
        if (in.getErrors().empty())
            return filterCfg;
    }
    catch (XmlParsingError&) {}

    return std::nullopt;
}


void fff::saveErrorLog(const ErrorLog& log, const Zstring& filePath) //throw FileError
{
    XmlDoc doc("Log");
    doc.setEncoding("");

    XmlOut out(doc);

    for (const LogEntry& e : log)
    {
        XmlOut outMsg = out.addChild(e.type == MessageType::MSG_TYPE_ERROR ? "Error" : (e.type == MessageType::MSG_TYPE_WARNING ? "Warning" : "Info"));
        outMsg.attribute("Time", formatTime(formatIsoDateTimeTag, getLocalTime(e.time)));
        outMsg(e.message);
    }

    saveXml(doc, filePath); //throw FileError
}


ErrorLog fff::loadErrorLog(const Zstring& filePath) //throw FileError
{
    XmlDoc doc = loadXml(filePath); //throw FileError

    XmlIn in(doc);
    ErrorLog log;

    in.visitChildren([&](const XmlIn& inMsg)
    {
        Zstring timeStr;
        inMsg.attribute("Time", timeStr);

        Zstringc msg;
        inMsg(msg);

        log.push_back(
        {
            .time = localToTimeT(parseTime(formatIsoDateTimeTag, timeStr)).first,
            .type = *inMsg.getName() == "Error" ? MessageType::MSG_TYPE_ERROR : (*inMsg.getName() == "Warning" ? MessageType::MSG_TYPE_WARNING : MessageType::MSG_TYPE_INFO),
            .message = std::move(msg),
        });
    });

    if (const std::wstring& errors = in.getErrors();
        !errors.empty())
        throw FileError(replaceCpy(_("Cannot read file %x."), L"%x", fmtPath(filePath)),
                        _("The following XML elements could not be read:") + L'\n' + errors);
    return log;
}

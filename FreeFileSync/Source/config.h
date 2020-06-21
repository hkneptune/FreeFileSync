// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PROCESS_XML_H_28345825704254262435
#define PROCESS_XML_H_28345825704254262435

#include <wx/gdicmn.h>
#include "localization.h"
#include "base/structures.h"
#include "ui/file_grid_attr.h"
#include "ui/tree_grid_attr.h" //RTS: avoid tree grid's "file_hierarchy.h" dependency!
#include "ui/cfg_grid.h"
#include "log_file.h"


namespace fff
{
enum class XmlType
{
    gui,
    batch,
    global,
    other
};
XmlType getXmlType(const Zstring& filePath); //throw FileError


enum class BatchErrorHandling
{
    showPopup,
    cancel
};


enum class PostSyncAction
{
    none,
    sleep,
    shutdown
};

struct ExternalApp
{
    std::wstring description;
    Zstring cmdLine;
};

//---------------------------------------------------------------------
struct XmlGuiConfig
{
    MainConfiguration mainCfg;
    bool highlightSyncAction = true;
};


inline
bool operator==(const XmlGuiConfig& lhs, const XmlGuiConfig& rhs)
{
    return lhs.mainCfg             == rhs.mainCfg &&
           lhs.highlightSyncAction == rhs.highlightSyncAction;
}
inline bool operator!=(const XmlGuiConfig& lhs, const XmlGuiConfig& rhs) { return !(lhs == rhs); }


struct BatchExclusiveConfig
{
    BatchErrorHandling batchErrorHandling = BatchErrorHandling::showPopup;
    bool runMinimized = false;
    bool autoCloseSummary = false;
    PostSyncAction postSyncAction = PostSyncAction::none;
};


struct XmlBatchConfig
{
    MainConfiguration mainCfg;
    BatchExclusiveConfig batchExCfg;
};


struct ConfirmationDialogs
{
    bool popupOnConfigChange      = true;
    bool confirmSyncStart         = true;
    bool confirmCommandMassInvoke = true;
};
inline bool operator==(const ConfirmationDialogs& lhs, const ConfirmationDialogs& rhs)
{
    return lhs.popupOnConfigChange      == rhs.popupOnConfigChange &&
           lhs.confirmSyncStart         == rhs.confirmSyncStart    &&
           lhs.confirmCommandMassInvoke == rhs.confirmCommandMassInvoke;
}
inline bool operator!=(const ConfirmationDialogs& lhs, const ConfirmationDialogs& rhs) { return !(lhs == rhs); }


enum class FileIconSize
{
    SMALL,
    MEDIUM,
    LARGE
};


struct ViewFilterDefault
{
    //shared
    bool equal    = false;
    bool conflict = true;
    bool excluded = false;
    //category view
    bool leftOnly   = true;
    bool rightOnly  = true;
    bool leftNewer  = true;
    bool rightNewer = true;
    bool different  = true;
    //action view
    bool createLeft  = true;
    bool createRight = true;
    bool updateLeft  = true;
    bool updateRight = true;
    bool deleteLeft  = true;
    bool deleteRight = true;
    bool doNothing   = true;
};


Zstring getGlobalConfigFile();


struct XmlGlobalSettings
{
    XmlGlobalSettings(); //clang needs this anyway

    //---------------------------------------------------------------------
    //Shared (GUI/BATCH) settings
    wxLanguage programLanguage = getSystemLanguage();
    bool failSafeFileCopy = true;
    bool copyLockedFiles  = false; //safer default: avoid copies of partially written files
    bool copyFilePermissions = false;

    int fileTimeTolerance = 2; //max. allowed file time deviation; < 0 means unlimited tolerance; default 2s: FAT vs NTFS
    bool runWithBackgroundPriority = false;
    bool createLockFile = true;
    bool verifyFileCopy = false;
    int logfilesMaxAgeDays = 30; //<= 0 := no limit; for log files under %AppData%\FreeFileSync\Logs
    LogFileFormat logFormat = LogFileFormat::html;

    Zstring soundFileCompareFinished;
    Zstring soundFileSyncFinished;

    bool autoCloseProgressDialog = false;
    ConfirmationDialogs confirmDlgs;
    WarningDialogs warnDlgs;

    //---------------------------------------------------------------------
    struct Gui
    {
        Gui() {} //clang needs this anyway
        struct
        {
            wxPoint dlgPos;
            wxSize dlgSize;
            bool isMaximized = false;

            bool textSearchRespectCase = false; //good default for Linux, too!
            int folderPairsVisibleMax = 6;

            size_t        cfgGridTopRowPos = 0;
            int           cfgGridSyncOverdueDays = 7;
            ColumnTypeCfg cfgGridLastSortColumn    = cfgGridLastSortColumnDefault;
            bool          cfgGridLastSortAscending = getDefaultSortDirection(cfgGridLastSortColumnDefault);
            std::vector<ColAttributesCfg> cfgGridColumnAttribs = getCfgGridDefaultColAttribs();
            size_t cfgHistItemsMax = 100;
            std::vector<ConfigFileItem> cfgFileHistory;
            std::vector<Zstring>        lastUsedConfigFiles;

            bool treeGridShowPercentBar = treeGridShowPercentageDefault;
            ColumnTypeTree treeGridLastSortColumn    = treeGridLastSortColumnDefault;    //remember sort on overview panel
            bool           treeGridLastSortAscending = getDefaultSortDirection(treeGridLastSortColumnDefault); //
            std::vector<ColAttributesTree> treeGridColumnAttribs = getTreeGridDefaultColAttribs();

            struct
            {
                bool keepRelPaths      = false;
                bool overwriteIfExists = false;
                Zstring lastUsedPath;
                std::vector<Zstring> folderHistory;
            } copyToCfg;

            std::vector<Zstring> folderHistoryLeft;
            std::vector<Zstring> folderHistoryRight;
            bool showIcons = true;
            FileIconSize iconSize = FileIconSize::SMALL;
            int sashOffset = 0;

            ItemPathFormat itemPathFormatLeftGrid  = defaultItemPathFormatLeftGrid;
            ItemPathFormat itemPathFormatRightGrid = defaultItemPathFormatRightGrid;

            std::vector<ColAttributesRim>  columnAttribLeft  = getFileGridDefaultColAttribsLeft();
            std::vector<ColAttributesRim>  columnAttribRight = getFileGridDefaultColAttribsRight();

            ViewFilterDefault viewFilterDefault;
            wxString guiPerspectiveLast; //used by wxAuiManager
        } mainDlg;

        Zstring defaultExclusionFilter = "*/.Trash-*/" "\n"
                                         "*/.recycle/";
        size_t folderHistoryMax = 20;

        std::vector<Zstring> versioningFolderHistory;
        std::vector<Zstring> logFolderHistory;

        std::vector<Zstring> emailHistory;
        size_t emailHistoryMax = 10;

        std::vector<Zstring> commandHistory;
        size_t commandHistoryMax = 10;

        std::vector<ExternalApp> externalApps
        {
            /* CONTRACT: first entry: show item in file browser

            default external app descriptions will be translated "on the fly"!!!           */
            //"xdg-open \"%parent_path%\"" -> not good enough: we need %local_path% for proper MTP/Google Drive handling
            { L"Browse directory", "xdg-open \"$(dirname \"%local_path%\")\"" },
            { L"Open with default application", "xdg-open \"%local_path%\""   },
            //mark for extraction: _("Browse directory") Linux doesn't use the term "folder"
        };

        time_t lastUpdateCheck = 0; //number of seconds since 00:00 hours, Jan 1, 1970 UTC
        std::string lastOnlineVersion;
    } gui;
};

//read/write specific config types
void readConfig(const Zstring& filePath, XmlGuiConfig&      cfg, std::wstring& warningMsg); //
void readConfig(const Zstring& filePath, XmlBatchConfig&    cfg, std::wstring& warningMsg); //throw FileError
void readConfig(const Zstring& filePath, XmlGlobalSettings& cfg, std::wstring& warningMsg); //

void writeConfig(const XmlGuiConfig&      cfg, const Zstring& filePath); //
void writeConfig(const XmlBatchConfig&    cfg, const Zstring& filePath); //throw FileError
void writeConfig(const XmlGlobalSettings& cfg, const Zstring& filePath); //

//convert (multiple) *.ffs_gui, *.ffs_batch files or combinations of both into target config structure:
void readAnyConfig(const std::vector<Zstring>& filePaths, XmlGuiConfig& cfg, std::wstring& warningMsg); //throw FileError

//config conversion utilities
XmlGuiConfig   convertBatchToGui(const XmlBatchConfig& batchCfg); //noexcept
XmlBatchConfig convertGuiToBatch(const XmlGuiConfig&   guiCfg, const BatchExclusiveConfig& batchExCfg); //

std::wstring extractJobName(const Zstring& cfgFilePath);
}

#endif //PROCESS_XML_H_28345825704254262435

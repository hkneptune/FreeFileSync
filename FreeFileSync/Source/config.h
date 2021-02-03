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
    GridViewType gridViewType = GridViewType::action;

    bool operator==(const XmlGuiConfig&) const = default;
};


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
    bool confirmSaveConfig        = true;
    bool confirmSyncStart         = true;
    bool confirmCommandMassInvoke = true;
    bool confirmSwapSides         = true;

    bool operator==(const ConfirmationDialogs&) const = default;
};


enum class FileIconSize
{
    small,
    medium,
    large
};


struct ViewFilterDefault
{
    //shared
    bool equal    = false;
    bool conflict = true;
    bool excluded = false;
    //difference view
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

    ConfirmationDialogs confirmDlgs;
    WarningDialogs warnDlgs;

    //---------------------------------------------------------------------
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
        Zstring cfgFileLastSelected;
        std::vector<ConfigFileItem> cfgFileHistory;
        std::vector<Zstring>        cfgFilesLastUsed;

        bool treeGridShowPercentBar = treeGridShowPercentageDefault;
        ColumnTypeTree treeGridLastSortColumn    = treeGridLastSortColumnDefault;    //remember sort on overview panel
        bool           treeGridLastSortAscending = getDefaultSortDirection(treeGridLastSortColumnDefault); //
        std::vector<ColAttributesTree> treeGridColumnAttribs = getTreeGridDefaultColAttribs();

        struct
        {
            bool keepRelPaths      = false;
            bool overwriteIfExists = false;
            Zstring targetFolderPath;
            Zstring targetFolderLastSelected;
            std::vector<Zstring> folderHistory;
        } copyToCfg;

        std::vector<Zstring> folderHistoryLeft;
        std::vector<Zstring> folderHistoryRight;
        Zstring folderLastSelectedLeft;
        Zstring folderLastSelectedRight;

        bool showIcons = true;
        FileIconSize iconSize = FileIconSize::small;
        int sashOffset = 0;

        ItemPathFormat itemPathFormatLeftGrid  = defaultItemPathFormatLeftGrid;
        ItemPathFormat itemPathFormatRightGrid = defaultItemPathFormatRightGrid;

        std::vector<ColAttributesRim> columnAttribLeft  = getFileGridDefaultColAttribsLeft();
        std::vector<ColAttributesRim> columnAttribRight = getFileGridDefaultColAttribsRight();

        ViewFilterDefault viewFilterDefault;
        wxString guiPerspectiveLast; //for wxAuiManager
    } mainDlg;

    struct
    {
        wxSize dlgSize;
        bool isMaximized = false;
        bool autoClose = false;
    } progressDlg;

    Zstring defaultExclusionFilter = "*/.Trash-*/" "\n"
                                     "*/.recycle/";
    size_t folderHistoryMax = 20;

    Zstring csvFileLastSelected;
    Zstring sftpKeyFileLastSelected;

    std::vector<Zstring> versioningFolderHistory;
    Zstring versioningFolderLastSelected;

    std::vector<Zstring> logFolderHistory;
    Zstring logFolderLastSelected;

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
};

//read/write specific config types
std::pair<XmlGuiConfig,      std::wstring /*warningMsg*/> readGuiConfig   (const Zstring& filePath); //
std::pair<XmlBatchConfig,    std::wstring /*warningMsg*/> readBatchConfig (const Zstring& filePath); //throw FileError
std::pair<XmlGlobalSettings, std::wstring /*warningMsg*/> readGlobalConfig(const Zstring& filePath); //

void writeConfig(const XmlGuiConfig&      cfg, const Zstring& filePath); //
void writeConfig(const XmlBatchConfig&    cfg, const Zstring& filePath); //throw FileError
void writeConfig(const XmlGlobalSettings& cfg, const Zstring& filePath); //

//convert (multiple) *.ffs_gui, *.ffs_batch files or combinations of both into target config structure:
std::pair<XmlGuiConfig, std::wstring /*warningMsg*/> readAnyConfig(const std::vector<Zstring>& filePaths); //throw FileError

//config conversion utilities
XmlGuiConfig   convertBatchToGui(const XmlBatchConfig& batchCfg); //noexcept
XmlBatchConfig convertGuiToBatch(const XmlGuiConfig&   guiCfg, const BatchExclusiveConfig& batchExCfg); //

std::wstring extractJobName(const Zstring& cfgFilePath);
}

#endif //PROCESS_XML_H_28345825704254262435

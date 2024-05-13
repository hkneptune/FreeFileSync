// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PROCESS_XML_H_28345825704254262435
#define PROCESS_XML_H_28345825704254262435

#include <wx/gdicmn.h>
#include <zen/file_access.h>
#include "localization.h"
#include "base/structures.h"
#include "ui/file_grid_attr.h"
#include "ui/tree_grid_attr.h" //RTS: avoid tree grid's "file_hierarchy.h" dependency!
#include "ui/cfg_grid.h"
#include "log_file.h"
//#include "version/version.h"


namespace fff
{
enum class BatchErrorHandling
{
    showPopup,
    cancel
};


enum class PostBatchAction
{
    none,
    sleep,
    shutdown
};

struct ExternalApp
{
    std::wstring description; //must be translated *after* loading from config file
    Zstring cmdLine;
};

extern const ExternalApp extCommandFileManager;
extern const ExternalApp extCommandOpenDefault;

//---------------------------------------------------------------------
struct XmlGuiConfig
{
    MainConfiguration mainCfg;
    GridViewType gridViewType = GridViewType::action; //keep "visual" setting out of "MainConfiguration"

    bool operator==(const XmlGuiConfig&) const = default;
};


struct BatchExclusiveConfig
{
    bool runMinimized = false;
    bool autoCloseSummary = false;
    BatchErrorHandling batchErrorHandling = BatchErrorHandling::showPopup;
    PostBatchAction postBatchAction = PostBatchAction::none;
};


struct XmlBatchConfig
{
    XmlGuiConfig guiCfg; //batch config can be used in GUI (but not the other way round)
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


enum class GridIconSize
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


Zstring getGlobalConfigDefaultPath();
Zstring getLogFolderDefaultPath();

struct DpiLayout
{
    struct
    {
        std::optional<wxSize> size;
        std::optional<wxPoint> pos;
        bool isMaximized = false;
    } mainDlg; //WindowLayout::getBeforeClose()

    struct
    {
        std::optional<wxSize> size;
        //std::optional<wxPoint> pos; -> most users probably want it centered, but others at a fixed (relative??) location
        bool isMaximized = false;
    } progressDlg;

    wxString panelLayout; //for wxAuiManager::LoadPerspective

    std::vector<ColAttributesCfg> configColumnAttribs = getCfgGridDefaultColAttribs();
    std::vector<ColumnAttribOverview> overviewColumnAttribs = getOverviewDefaultColAttribs();
    std::vector<ColAttributesRim> fileColumnAttribsLeft  = getFileGridDefaultColAttribsLeft();
    std::vector<ColAttributesRim> fileColumnAttribsRight = getFileGridDefaultColAttribsRight();
};


struct XmlGlobalSettings
{
    XmlGlobalSettings();

    //---------------------------------------------------------------------
    //Shared (GUI/BATCH) settings
    wxLanguage programLanguage = getDefaultLanguage();
    bool failSafeFileCopy = true;
    bool copyLockedFiles  = false; //safer default: avoid copies of partially written files
    bool copyFilePermissions = false;

    unsigned int fileTimeTolerance = zen::FAT_FILE_TIME_PRECISION_SEC; //default 2s: FAT vs NTFS
    bool runWithBackgroundPriority = false;
    bool createLockFile = true;
    bool verifyFileCopy = false;
    int logfilesMaxAgeDays = 30; //<= 0 := no limit; for log files under %AppData%\FreeFileSync\Logs
    LogFileFormat logFormat = LogFileFormat::html;

    Zstring soundFileCompareFinished;
    Zstring soundFileSyncFinished;
    Zstring soundFileAlertPending;

    ConfirmationDialogs confirmDlgs;
    WarningDialogs warnDlgs;

    //---------------------------------------------------------------------

    struct
    {
        bool textSearchRespectCase = false; //good default for Linux, too!
        int folderPairsVisibleMax = 6;

        struct
        {
            size_t        topRowPos = 0;
            int           syncOverdueDays = 7;
            ColumnTypeCfg lastSortColumn = cfgGridLastSortColumnDefault;
            bool          lastSortAscending = getDefaultSortDirection(cfgGridLastSortColumnDefault);
            size_t histItemsMax = 100; //do we need to limit config items at all?
            Zstring lastSelectedFile;
            std::vector<ConfigFileItem> fileHistory;
            std::vector<Zstring>        lastUsedFiles;
        } config;

        struct
        {
            bool               showPercentBar = overviewPanelShowPercentageDefault;
            ColumnTypeOverview lastSortColumn = overviewPanelLastSortColumnDefault; //remember sort on overview panel
            bool               lastSortAscending = getDefaultSortDirection(overviewPanelLastSortColumnDefault); //
        } overview;

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
        GridIconSize iconSize = GridIconSize::small;
        int sashOffset = 0;

        ItemPathFormat itemPathFormatLeftGrid  = defaultItemPathFormatLeftGrid;
        ItemPathFormat itemPathFormatRightGrid = defaultItemPathFormatRightGrid;

        ViewFilterDefault viewFilterDefault;
    } mainDlg;

    bool progressDlgAutoClose = false;

    FilterConfig defaultFilter = []
    {
        FilterConfig def;
        assert(def.excludeFilter.empty());
        def.excludeFilter =
        "*/.Trash-*/" "\n"
        "*/.recycle/";
        return def;
    }();

    size_t folderHistoryMax = 20;

    Zstring sftpKeyFileLastSelected;

    std::vector<Zstring> versioningFolderHistory;
    Zstring versioningFolderLastSelected;

    Zstring logFolderPhrase = getLogFolderDefaultPath();
    std::vector<Zstring> logFolderHistory;
    Zstring logFolderLastSelected;

    std::vector<Zstring> emailHistory;
    size_t emailHistoryMax = 10;

    std::vector<Zstring> commandHistory;
    size_t commandHistoryMax = 10;

    std::vector<ExternalApp> externalApps{extCommandFileManager, extCommandOpenDefault};

    time_t lastUpdateCheck = 0; //number of seconds since Jan 1, 1970 GMT
    std::string lastOnlineVersion;

    std::string welcomeDialogLastVersion;

    std::unordered_map<int /*scale percent*/, DpiLayout> dpiLayouts;
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


std::wstring extractJobName(const Zstring& cfgFilePath);

//human-readable/editable format suitable for clipboard
std::string serializeFilter(const FilterConfig& filterCfg);
std::optional<FilterConfig> parseFilterBuf(const std::string& filterBuf);

void saveErrorLog(const zen::ErrorLog& log, const Zstring& filePath); //throw FileError
zen::ErrorLog loadErrorLog(const Zstring& filePath); //throw FileError
}

#endif //PROCESS_XML_H_28345825704254262435

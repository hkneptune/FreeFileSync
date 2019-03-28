// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PROCESS_XML_H_28345825704254262435
#define PROCESS_XML_H_28345825704254262435

#include <wx/gdicmn.h>
#include "localization.h"
#include "structures.h"
#include "../ui/file_grid_attr.h"
#include "../ui/tree_grid_attr.h" //RTS: avoid tree grid's "file_hierarchy.h" dependency!
#include "../ui/cfg_grid.h"


namespace fff
{
enum XmlType
{
    XML_TYPE_GUI,
    XML_TYPE_BATCH,
    XML_TYPE_GLOBAL,
    XML_TYPE_OTHER
};

XmlType getXmlType(const Zstring& filePath); //throw FileError


enum class BatchErrorHandling
{
    SHOW_POPUP,
    CANCEL
};


enum class PostSyncAction
{
    NONE,
    SLEEP,
    SHUTDOWN
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
    BatchErrorHandling batchErrorHandling = BatchErrorHandling::SHOW_POPUP;
    bool runMinimized = false;
    bool autoCloseSummary = false;
    PostSyncAction postSyncAction = PostSyncAction::NONE;
};


struct XmlBatchConfig
{
    MainConfiguration mainCfg;
    BatchExclusiveConfig batchExCfg;
};


struct ConfirmationDialogs
{
    bool popupOnConfigChange              = true;
    bool confirmSyncStart                 = true;
    bool confirmCommandMassInvoke = true;
};
inline bool operator==(const ConfirmationDialogs& lhs, const ConfirmationDialogs& rhs)
{
    return lhs.popupOnConfigChange              == rhs.popupOnConfigChange &&
           lhs.confirmSyncStart                 == rhs.confirmSyncStart    &&
           lhs.confirmCommandMassInvoke == rhs.confirmCommandMassInvoke;
}
inline bool operator!=(const ConfirmationDialogs& lhs, const ConfirmationDialogs& rhs) { return !(lhs == rhs); }


struct WarningDialogs
{
    bool warnFolderNotExisting          = true;
    bool warnFoldersDifferInCase        = true;
    bool warnDependentFolderPair        = true;
    bool warnDependentBaseFolders       = true;
    bool warnSignificantDifference      = true;
    bool warnNotEnoughDiskSpace         = true;
    bool warnUnresolvedConflicts        = true;
    bool warnModificationTimeError      = true;
    bool warnRecyclerMissing            = true;
    bool warnInputFieldEmpty            = true;
    bool warnDirectoryLockFailed        = true;
    bool warnVersioningFolderPartOfSync = true;
};
inline bool operator==(const WarningDialogs& lhs, const WarningDialogs& rhs)
{
    return lhs.warnFolderNotExisting          == rhs.warnFolderNotExisting     &&
           lhs.warnFoldersDifferInCase        == rhs.warnFoldersDifferInCase   &&
           lhs.warnDependentFolderPair        == rhs.warnDependentFolderPair   &&
           lhs.warnDependentBaseFolders       == rhs.warnDependentBaseFolders  &&
           lhs.warnSignificantDifference      == rhs.warnSignificantDifference &&
           lhs.warnNotEnoughDiskSpace         == rhs.warnNotEnoughDiskSpace    &&
           lhs.warnUnresolvedConflicts        == rhs.warnUnresolvedConflicts   &&
           lhs.warnModificationTimeError      == rhs.warnModificationTimeError &&
           lhs.warnRecyclerMissing            == rhs.warnRecyclerMissing       &&
           lhs.warnInputFieldEmpty            == rhs.warnInputFieldEmpty       &&
           lhs.warnDirectoryLockFailed        == rhs.warnDirectoryLockFailed   &&
           lhs.warnVersioningFolderPartOfSync == rhs.warnVersioningFolderPartOfSync;
}
inline bool operator!=(const WarningDialogs& lhs, const WarningDialogs& rhs) { return !(lhs == rhs); }



enum FileIconSize
{
    ICON_SIZE_SMALL,
    ICON_SIZE_MEDIUM,
    ICON_SIZE_LARGE
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

    Zstring soundFileCompareFinished;
    Zstring soundFileSyncFinished = Zstr("gong.wav");

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
            int maxFolderPairsVisible = 6;

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

            size_t folderHistItemsMax = 20;

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
            FileIconSize iconSize = ICON_SIZE_SMALL;
            int sashOffset = 0;

            ItemPathFormat itemPathFormatLeftGrid  = defaultItemPathFormatLeftGrid;
            ItemPathFormat itemPathFormatRightGrid = defaultItemPathFormatRightGrid;

            std::vector<ColAttributesRim>  columnAttribLeft  = getFileGridDefaultColAttribsLeft();
            std::vector<ColAttributesRim>  columnAttribRight = getFileGridDefaultColAttribsRight();

            ViewFilterDefault viewFilterDefault;
            wxString guiPerspectiveLast; //used by wxAuiManager
        } mainDlg;

        Zstring defaultExclusionFilter = Zstr("/.Trash-*/") Zstr("\n")
                                         Zstr("/.recycle/");

        std::vector<Zstring> commandHistory;
        size_t commandHistItemsMax = 8;

        std::vector<ExternalApp> externalApps
        {
            //default external app descriptions will be translated "on the fly"!!!
            //CONTRACT: first entry will be used for [Enter] or mouse double-click!
            { L"Browse directory",              Zstr("xdg-open \"%folder_path%\"") },
            { L"Open with default application", Zstr("xdg-open \"%local_path%\"")   },
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

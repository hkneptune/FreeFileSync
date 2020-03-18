// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef CONFIG_HISTORY_3248789479826359832
#define CONFIG_HISTORY_3248789479826359832

#include <wx+/grid.h>
#include <wx+/dc.h>
#include <zen/zstring.h>
#include "../return_codes.h"
#include "../afs/concrete.h"


namespace fff
{
struct ConfigFileItem
{
    ConfigFileItem() {}
    ConfigFileItem(const Zstring& filePath,
                   time_t syncTime,
                   const AbstractPath& logPath,
                   SyncResult result,
                   wxColor bcol) :
        cfgFilePath(filePath),
        lastSyncTime(syncTime),
        logFilePath(logPath),
        logResult(result),
        backColor(bcol) {}

    Zstring    cfgFilePath;
    time_t     lastSyncTime = 0;  //last COMPLETED sync (aborted syncs don't count)
    AbstractPath logFilePath = getNullPath();     //ANY last sync attempt (including aborted syncs)
    SyncResult   logResult = SyncResult::aborted; //
    wxColor      backColor;
};


enum class ColumnTypeCfg
{
    name,
    lastSync,
    lastLog,
};

struct ColAttributesCfg
{
    ColumnTypeCfg type    = ColumnTypeCfg::name;
    int           offset  = 0;
    int           stretch = 0;
    bool          visible = false;
};

inline
std::vector<ColAttributesCfg> getCfgGridDefaultColAttribs()
{
    using namespace zen;
    return
    {
        { ColumnTypeCfg::name,     fastFromDIP(0 - 75 - 42), 1, true },
        { ColumnTypeCfg::lastSync, fastFromDIP(75), 0, true },
        { ColumnTypeCfg::lastLog,  fastFromDIP(42), 0, true }, //leave some room for the sort direction indicator
    };
}

const ColumnTypeCfg cfgGridLastSortColumnDefault = ColumnTypeCfg::name;

inline
bool getDefaultSortDirection(ColumnTypeCfg colType)
{
    switch (colType)
    {
        case ColumnTypeCfg::name:
            return true;
        case ColumnTypeCfg::lastSync: //actual sort order is "time since last sync"
            return false;
        case ColumnTypeCfg::lastLog:
            return true;
    }
    assert(false);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
Zstring getLastRunConfigPath();


class ConfigView
{
public:
    ConfigView() {}

    std::vector<ConfigFileItem> get() const;
    void set(const std::vector<ConfigFileItem>& cfgItems);

    void addCfgFiles(const std::vector<Zstring>& filePaths);
    void removeItems(const std::vector<Zstring>& filePaths);

    struct LastRunStats
    {
        time_t       lastRunTime = 0;
        SyncResult   result = SyncResult::aborted;
        AbstractPath logFilePath; //optional
    };
    void setLastRunStats(const std::vector<Zstring>& filePaths, const LastRunStats& lastRun);
    void setBackColor(const std::vector<Zstring>& filePaths, const wxColor& col);

    struct Details
    {
        ConfigFileItem cfgItem;

        Zstring name;
        int lastUseIndex = 0; //support truncating the config list size via last usage, the higher the index the more recent the usage
        bool isLastRunCfg = false; //LastRun.ffs_gui

        enum ConfigType
        {
            CFG_TYPE_NONE,
            CFG_TYPE_GUI,
            CFG_TYPE_BATCH,
        } cfgType = CFG_TYPE_NONE;
    };

    const Details* getItem(size_t row) const;
    size_t getRowCount() const { assert(cfgList_.size() == cfgListView_.size()); return cfgListView_.size(); }

    void setSortDirection(ColumnTypeCfg colType, bool ascending);
    std::pair<ColumnTypeCfg, bool> getSortDirection() { return { sortColumn_, sortAscending_ }; }

private:
    ConfigView           (const ConfigView&) = delete;
    ConfigView& operator=(const ConfigView&) = delete;

    void addCfgFilesImpl(const std::vector<Zstring>& filePaths);

    void sortListView();
    template <bool ascending> void sortListViewImpl();

    const Zstring lastRunConfigPath_ = getLastRunConfigPath(); //let's not use another static...

    using CfgFileList = std::map<Zstring /*file path*/, Details, LessNativePath>;

    CfgFileList cfgList_;
    std::vector<CfgFileList::iterator> cfgListView_; //sorted view on cfgList_

    ColumnTypeCfg sortColumn_ = cfgGridLastSortColumnDefault;
    bool sortAscending_       = getDefaultSortDirection(cfgGridLastSortColumnDefault);
};


namespace cfggrid
{
void init(zen::Grid& grid);
ConfigView& getDataView(zen::Grid& grid); //grid.Refresh() after making changes!

void addAndSelect(zen::Grid& grid, const std::vector<Zstring>& filePaths, bool scrollToSelection);

int  getSyncOverdueDays(zen::Grid& grid);
void setSyncOverdueDays(zen::Grid& grid, int syncOverdueDays);
}
}

#endif //CONFIG_HISTORY_3248789479826359832

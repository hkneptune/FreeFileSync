// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef CONFIG_HISTORY_3248789479826359832
#define CONFIG_HISTORY_3248789479826359832

#include <wx+/grid.h>
#include <zen/zstring.h>


namespace fff
{
enum class ColumnTypeCfg
{
    NAME,
    LAST_SYNC,
};


struct ColAttributesCfg
{
    ColumnTypeCfg type    = ColumnTypeCfg::NAME;
    int           offset  = 0;
    int           stretch = 0;
    bool          visible = false;
};

inline
std::vector<ColAttributesCfg> getCfgGridDefaultColAttribs()
{
    return
    {
        { ColumnTypeCfg::NAME,     -75, 1, true },
        { ColumnTypeCfg::LAST_SYNC, 75, 0, true },
    };
}

const ColumnTypeCfg cfgGridLastSortColumnDefault = ColumnTypeCfg::NAME;

inline
bool getDefaultSortDirection(ColumnTypeCfg colType)
{
    switch (colType)
    {
        case ColumnTypeCfg::NAME:
            return true;
        case ColumnTypeCfg::LAST_SYNC: //actual sort order is "time since last sync"
            return false;
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

    void addCfgFiles(const std::vector<Zstring>& filePaths);
    void removeItems(const std::vector<Zstring>& filePaths);

    void setLastSyncTime(const std::vector<std::pair<Zstring /*filePath*/, time_t /*lastSyncTime*/>>& syncTimes);

    struct Details
    {
        Zstring filePath;
        Zstring name;
        time_t lastSyncTime = 0;
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

    void sortListView();
    template <bool ascending> void sortListViewImpl();

    const Zstring lastRunConfigPath_ = getLastRunConfigPath(); //let's not use another static...

    using CfgFileList = std::map<Zstring /*file path*/, Details, LessFilePath>;

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

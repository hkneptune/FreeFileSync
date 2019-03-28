// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "cfg_grid.h"
#include <zen/time.h>
#include <zen/basic_math.h>
#include <zen/shell_execute.h>
#include <wx+/dc.h>
#include <wx+/rtl.h>
#include <wx+/image_resources.h>
#include <wx+/popup_dlg.h>
#include <wx/settings.h>
#include "../base/icon_buffer.h"
#include "../base/ffs_paths.h"
//#include "../fs/mtp.h"

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


Zstring fff::getLastRunConfigPath()
{
    return getConfigDirPathPf() + Zstr("LastRun.ffs_gui");
}


std::vector<ConfigFileItem> ConfigView::get() const
{
    std::map<int, ConfigFileItem, std::greater<>> itemsSorted; //sort by last use; put most recent items *first* (looks better in XML than reverted)

    for (const auto& [filePath, details] : cfgList_)
        itemsSorted.emplace(details.lastUseIndex, details.cfgItem);

    std::vector<ConfigFileItem> cfgHistory;
    for (const auto& [lastUseIndex, cfgItem] : itemsSorted)
        cfgHistory.emplace_back(cfgItem);

    return cfgHistory;
}


void ConfigView::set(const std::vector<ConfigFileItem>& cfgItems)
{
    std::vector<Zstring> filePaths;
    for (const ConfigFileItem& item : cfgItems)
        filePaths.push_back(item.cfgFilePath);

    //list is stored with last used files first in XML, however m_gridCfgHistory expects them last!!!
    std::reverse(filePaths.begin(), filePaths.end());

    //make sure <Last session> is always part of history list (if existing)
    filePaths.push_back(lastRunConfigPath_);

    cfgList_    .clear();
    cfgListView_.clear();
    addCfgFiles(filePaths);

    for (const ConfigFileItem& item : cfgItems)
        cfgList_.find(item.cfgFilePath)->second.cfgItem = item; //cfgFilePath must exist after addCfgFiles()!

    sortListView(); //needed if sorted by last sync time
}


void ConfigView::addCfgFiles(const std::vector<Zstring>& filePaths)
{
    //determine highest "last use" index number of m_listBoxHistory
    int lastUseIndexMax = 0;
    for (const auto& [filePath, details] : cfgList_)
        lastUseIndexMax = std::max(lastUseIndexMax, details.lastUseIndex);

    for (const Zstring& filePath : filePaths)
    {
        auto it = cfgList_.find(filePath);
        if (it == cfgList_.end())
        {
            Details detail = {};
            detail.cfgItem.cfgFilePath = filePath;
            detail.lastUseIndex = ++lastUseIndexMax;

            std::tie(detail.name, detail.cfgType, detail.isLastRunCfg) = [&]
            {
                if (equalNativePath(filePath, lastRunConfigPath_))
                    return std::make_tuple(utfTo<Zstring>(L"<" + _("Last session") + L">"), Details::CFG_TYPE_GUI, true);

                const Zstring fileName = afterLast(filePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL);

                if (endsWithAsciiNoCase(fileName, Zstr(".ffs_gui")))
                    return std::make_tuple(beforeLast(fileName, Zstr('.'), IF_MISSING_RETURN_NONE), Details::CFG_TYPE_GUI, false);
                else if (endsWithAsciiNoCase(fileName, Zstr(".ffs_batch")))
                    return std::make_tuple(beforeLast(fileName, Zstr('.'), IF_MISSING_RETURN_NONE), Details::CFG_TYPE_BATCH, false);
                else
                    return std::make_tuple(fileName, Details::CFG_TYPE_NONE, false);
            }();

            auto itNew = cfgList_.emplace_hint(cfgList_.end(), filePath, std::move(detail));
            cfgListView_.push_back(itNew);
        }
        else
            it->second.lastUseIndex = ++lastUseIndexMax;
    }

    sortListView();
}


void ConfigView::removeItems(const std::vector<Zstring>& filePaths)
{
    const std::set<Zstring, LessNativePath> pathsSorted(filePaths.begin(), filePaths.end());

    eraseIf(cfgListView_, [&](auto it) { return pathsSorted.find(it->first) != pathsSorted.end(); });

    for (const Zstring& filePath : filePaths)
        cfgList_.erase(filePath);

    assert(cfgList_.size() == cfgListView_.size());
}


//coordinate with similar code in application.cpp
void ConfigView::setLastRunStats(const std::vector<Zstring>& filePaths, const LastRunStats& lastRun)
{
    for (const Zstring& filePath : filePaths)
    {
        auto it = cfgList_.find(filePath);
        assert(it != cfgList_.end());
        if (it != cfgList_.end())
        {
            if (lastRun.result != SyncResult::ABORTED)
                it->second.cfgItem.lastSyncTime = lastRun.lastRunTime;

            if (!AFS::isNullPath(lastRun.logFilePath))
            {
                it->second.cfgItem.logFilePath = lastRun.logFilePath;
                it->second.cfgItem.logResult   = lastRun.result;
            }
        }
    }
    sortListView(); //needed if sorted by last sync time
}


const ConfigView::Details* ConfigView::getItem(size_t row) const
{
    if (row < cfgListView_.size())
        return &cfgListView_[row]->second;
    return nullptr;
}


void ConfigView::setSortDirection(ColumnTypeCfg colType, bool ascending)
{
    sortColumn_    = colType;
    sortAscending_ = ascending;

    sortListView();
}


template <bool ascending>
void ConfigView::sortListViewImpl()
{
    const auto lessCfgName = [](CfgFileList::iterator lhs, CfgFileList::iterator rhs)
    {
        if (lhs->second.isLastRunCfg != rhs->second.isLastRunCfg)
            return lhs->second.isLastRunCfg > rhs->second.isLastRunCfg; //"last session" label should be at top position!

        return LessNaturalSort()(lhs->second.name, rhs->second.name);
    };

    const auto lessLastSync = [](CfgFileList::iterator lhs, CfgFileList::iterator rhs)
    {
        if (lhs->second.isLastRunCfg != rhs->second.isLastRunCfg)
            return lhs->second.isLastRunCfg < rhs->second.isLastRunCfg; //"last session" label should be (always) last

        return makeSortDirection(std::greater<>(), std::bool_constant<ascending>())(lhs->second.cfgItem.lastSyncTime, rhs->second.cfgItem.lastSyncTime);
        //[!] ascending LAST_SYNC shows lowest "days past" first <=> highest lastSyncTime first
    };

    const auto lessLastLog = [](CfgFileList::iterator lhs, CfgFileList::iterator rhs)
    {
        if (lhs->second.isLastRunCfg != rhs->second.isLastRunCfg)
            return lhs->second.isLastRunCfg < rhs->second.isLastRunCfg; //"last session" label should be (always) last

        const bool hasLogL = !AFS::isNullPath(lhs->second.cfgItem.logFilePath);
        const bool hasLogR = !AFS::isNullPath(rhs->second.cfgItem.logFilePath);
        if (hasLogL != hasLogR)
            return hasLogL > hasLogR; //move sync jobs that were never run to the back

        //primary sort order
        if (hasLogL && lhs->second.cfgItem.logResult != rhs->second.cfgItem.logResult)
            return makeSortDirection(std::greater<>(), std::bool_constant<ascending>())(lhs->second.cfgItem.logResult, rhs->second.cfgItem.logResult);

        //secondary sort order
        return LessNaturalSort()(lhs->second.name, rhs->second.name);
    };

    switch (sortColumn_)
    {
        case ColumnTypeCfg::NAME:
            std::sort(cfgListView_.begin(), cfgListView_.end(), makeSortDirection(lessCfgName, std::bool_constant<ascending>()));
            break;
        case ColumnTypeCfg::LAST_SYNC:
            std::sort(cfgListView_.begin(), cfgListView_.end(), lessLastSync);
            break;
        case ColumnTypeCfg::LAST_LOG:
            std::sort(cfgListView_.begin(), cfgListView_.end(), lessLastLog);
            break;
    }
}


void ConfigView::sortListView()
{
    if (sortAscending_)
        sortListViewImpl<true>();
    else
        sortListViewImpl<false>();
}

//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------

namespace
{
class GridDataCfg : private wxEvtHandler, public GridData
{
public:
    GridDataCfg(Grid& grid) : grid_(grid)
    {
        grid.Connect(EVENT_GRID_MOUSE_LEFT_DOWN,   GridClickEventHandler(GridDataCfg::onMouseLeft),      nullptr, this);
        grid.Connect(EVENT_GRID_MOUSE_LEFT_DOUBLE, GridClickEventHandler(GridDataCfg::onMouseLeftDouble), nullptr, this);
    }

    ConfigView& getDataView() { return cfgView_; }

    static int getRowDefaultHeight(const Grid& grid)
    {
        return std::max(getResourceImage(L"msg_error_sicon").GetHeight(), grid.getMainWin().GetCharHeight()) + fastFromDIP(1); //+ some space
    }

    int  getSyncOverdueDays() const { return syncOverdueDays_; }
    void setSyncOverdueDays(int syncOverdueDays) { syncOverdueDays_ = syncOverdueDays; }

private:
    size_t getRowCount() const override { return cfgView_.getRowCount(); }

    static int getDaysPast(time_t last)
    {
        time_t now = std::time(nullptr);

        const TimeComp tcNow  = getLocalTime(now);
        const TimeComp tcLast = getLocalTime(last);
        if (tcNow  == TimeComp() || tcLast  == TimeComp())
        {
            assert(false);
            return 0;
        }

        //truncate down to midnight => incorrect during DST switches, but doesn't matter due to numeric::round() below
        now  -= tcNow .hour * 3600 + tcNow .minute * 60 + tcNow .second;
        last -= tcLast.hour * 3600 + tcLast.minute * 60 + tcLast.second;

        return numeric::round((now - last) / (24.0 * 3600));
    }

    std::wstring getValue(size_t row, ColumnType colType) const override
    {
        if (const ConfigView::Details* item = cfgView_.getItem(row))
            switch (static_cast<ColumnTypeCfg>(colType))
            {
                case ColumnTypeCfg::NAME:
                    return utfTo<std::wstring>(item->name);

                case ColumnTypeCfg::LAST_SYNC:
                    if (!item->isLastRunCfg)
                    {
                        if (item->cfgItem.lastSyncTime == 0)
                            return std::wstring(1, EN_DASH);

                        const int daysPast = getDaysPast(item->cfgItem.lastSyncTime);
                        return daysPast == 0 ? _("Today") : _P("1 day", "%x days", daysPast);
                        //return formatTime<std::wstring>(FORMAT_DATE_TIME, getLocalTime(item->lastSyncTime));
                    }
                    break;

                case ColumnTypeCfg::LAST_LOG:
                    if (!item->isLastRunCfg &&
                        !AFS::isNullPath(item->cfgItem.logFilePath))
                        return getFinalStatusLabel(item->cfgItem.logResult);
                    break;
            }
        return std::wstring();
    }

    enum class HoverAreaLog
    {
        LINK,
    };

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover) override
    {
        wxRect rectTmp = rect;

        wxDCTextColourChanger dummy(dc); //accessibility: always set both foreground AND background colors!
        if (selected)
            dummy.Set(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
        else
        {
            //if (enabled)
            dummy.Set(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
            //else
            //    dummy.Set(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
        }

        if (const ConfigView::Details* item = cfgView_.getItem(row))
            switch (static_cast<ColumnTypeCfg>(colType))
            {
                case ColumnTypeCfg::NAME:
                {
                    rectTmp.x     += getColumnGapLeft();
                    rectTmp.width -= getColumnGapLeft();

                    const wxBitmap cfgIcon = [&]
                    {
                        switch (item->cfgType)
                        {
                            case ConfigView::Details::CFG_TYPE_NONE:
                                return wxNullBitmap;
                            case ConfigView::Details::CFG_TYPE_GUI:
                                return getResourceImage(L"file_sync_sicon");
                            case ConfigView::Details::CFG_TYPE_BATCH:
                                return getResourceImage(L"file_batch_sicon");
                        }
                        assert(false);
                        return wxNullBitmap;
                    }();
                    if (cfgIcon.IsOk())
                        drawBitmapRtlNoMirror(dc, enabled ? cfgIcon : cfgIcon.ConvertToDisabled(), rectTmp, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

                    rectTmp.x     += fileIconSize_ + getColumnGapLeft();
                    rectTmp.width -= fileIconSize_ + getColumnGapLeft();

                    drawCellText(dc, rectTmp, getValue(row, colType));
                }
                break;

                case ColumnTypeCfg::LAST_SYNC:
                {
                    wxDCTextColourChanger dummy2(dc);
                    if (syncOverdueDays_ > 0)
                        if (getDaysPast(item->cfgItem.lastSyncTime) >= syncOverdueDays_)
                            dummy2.Set(*wxRED);

                    drawCellText(dc, rectTmp, getValue(row, colType), wxALIGN_CENTER);
                }
                break;

                case ColumnTypeCfg::LAST_LOG:
                    if (!item->isLastRunCfg &&
                        !AFS::isNullPath(item->cfgItem.logFilePath))
                    {
                        const wxBitmap statusIcon = [&]
                        {
                            switch (item->cfgItem.logResult)
                            {
                                case SyncResult::FINISHED_WITH_SUCCESS:
                                    return getResourceImage(L"msg_finished_sicon");
                                case SyncResult::FINISHED_WITH_WARNINGS:
                                    return getResourceImage(L"msg_warning_sicon");
                                case SyncResult::FINISHED_WITH_ERROR:
                                case SyncResult::ABORTED:
                                    return getResourceImage(L"msg_error_sicon");
                            }
                            assert(false);
                            return wxNullBitmap;
                        }();
                        drawBitmapRtlNoMirror(dc, enabled ? statusIcon : statusIcon.ConvertToDisabled(), rectTmp, wxALIGN_CENTER);
                    }
                    if (static_cast<HoverAreaLog>(rowHover) == HoverAreaLog::LINK)
                        drawBitmapRtlNoMirror(dc, getResourceImage(L"link_16"), rectTmp, wxALIGN_CENTER);
                    break;
            }
    }

    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        // -> synchronize renderCell() <-> getBestSize()

        switch (static_cast<ColumnTypeCfg>(colType))
        {
            case ColumnTypeCfg::NAME:
                return getColumnGapLeft() + fileIconSize_ + getColumnGapLeft() + dc.GetTextExtent(getValue(row, colType)).GetWidth() + getColumnGapLeft();

            case ColumnTypeCfg::LAST_SYNC:
                return getColumnGapLeft() + dc.GetTextExtent(getValue(row, colType)).GetWidth() + getColumnGapLeft();

            case ColumnTypeCfg::LAST_LOG:
                return fileIconSize_;
        }
        assert(false);
        return 0;
    }

    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected) override
    {
        if (selected)
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
        else
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    }

    HoverArea getRowMouseHover(size_t row, ColumnType colType, int cellRelativePosX, int cellWidth) override
    {
        if (const ConfigView::Details* item = cfgView_.getItem(row))
            switch (static_cast<ColumnTypeCfg>(colType))
            {
                case ColumnTypeCfg::NAME:
                case ColumnTypeCfg::LAST_SYNC:
                    break;
                case ColumnTypeCfg::LAST_LOG:

                    if (!item->isLastRunCfg &&
                        !AFS::isNullPath(item->cfgItem.logFilePath) &&
                        AFS::getNativeItemPath(item->cfgItem.logFilePath))
                        return static_cast<HoverArea>(HoverAreaLog::LINK);
                    break;
            }
        return HoverArea::NONE;
    }

    void renderColumnLabel(Grid& tree, wxDC& dc, const wxRect& rect, ColumnType colType, bool highlighted) override
    {
        const wxRect rectInner = drawColumnLabelBackground(dc, rect, highlighted);
        wxRect rectRemain = rectInner;

        wxBitmap sortMarker;

        const auto sortInfo = cfgView_.getSortDirection();
        if (colType == static_cast<ColumnType>(sortInfo.first))
            sortMarker = getResourceImage(sortInfo.second ? L"sort_ascending" : L"sort_descending");

        switch (static_cast<ColumnTypeCfg>(colType))
        {
            case ColumnTypeCfg::NAME:
            case ColumnTypeCfg::LAST_SYNC:
                rectRemain.x     += getColumnGapLeft();
                rectRemain.width -= getColumnGapLeft();
                drawColumnLabelText(dc, rectRemain, getColumnLabel(colType));

                if (sortMarker.IsOk())
                    drawBitmapRtlNoMirror(dc, sortMarker, rectInner, wxALIGN_CENTER_HORIZONTAL);
                break;

            case ColumnTypeCfg::LAST_LOG:
                drawBitmapRtlNoMirror(dc, getResourceImage(L"log_file_sicon"), rectInner, wxALIGN_CENTER);

                if (sortMarker.IsOk())
                {
                    const int gapLeft = (rectInner.width + getResourceImage(L"log_file_sicon").GetWidth()) / 2;
                    rectRemain.x     += gapLeft;
                    rectRemain.width -= gapLeft;

                    drawBitmapRtlNoMirror(dc, sortMarker, rectRemain, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
                }
                break;
        }
    }

    std::wstring getColumnLabel(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeCfg>(colType))
        {
            case ColumnTypeCfg::NAME:
                return _("Name");
            case ColumnTypeCfg::LAST_SYNC:
                return _("Last sync");
            case ColumnTypeCfg::LAST_LOG:
                return _("Log");
        }
        return std::wstring();
    }

    std::wstring getToolTip(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeCfg>(colType))
        {
            case ColumnTypeCfg::NAME:
            case ColumnTypeCfg::LAST_SYNC:
                break;
            case ColumnTypeCfg::LAST_LOG:
                return getColumnLabel(colType);
        }
        return std::wstring();
    }

    std::wstring getToolTip(size_t row, ColumnType colType) const override
    {
        if (const ConfigView::Details* item = cfgView_.getItem(row))
            switch (static_cast<ColumnTypeCfg>(colType))
            {
                case ColumnTypeCfg::NAME:
                case ColumnTypeCfg::LAST_SYNC:
                    break;
                case ColumnTypeCfg::LAST_LOG:

                    if (!item->isLastRunCfg &&
                        !AFS::isNullPath(item->cfgItem.logFilePath))
                        return getFinalStatusLabel(item->cfgItem.logResult) + SPACED_DASH + AFS::getDisplayPath(item->cfgItem.logFilePath);
                    break;
            }
        return std::wstring();
    }

    void onMouseLeft(GridClickEvent& event)
    {
        if (const ConfigView::Details* item = cfgView_.getItem(event.row_))
            switch (static_cast<HoverAreaLog>(event.hoverArea_))
            {
                case HoverAreaLog::LINK:
                    try
                    {
                        if (std::optional<Zstring> nativePath = AFS::getNativeItemPath(item->cfgItem.logFilePath))
                            openWithDefaultApplication(*nativePath); //throw FileError
                        else
                            assert(false);
                        assert(!AFS::isNullPath(item->cfgItem.logFilePath)); //see getRowMouseHover()
                    }
                    catch (const FileError& e) { showNotificationDialog(&grid_, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString())); }
                    return;
            }
        event.Skip();
    }

    void onMouseLeftDouble(GridClickEvent& event)
    {
        switch (static_cast<HoverAreaLog>(event.hoverArea_))
        {
            case HoverAreaLog::LINK:
                return; //swallow event here before MainDialog considers it as a request to start comparison
        }
        event.Skip();
    }

private:
    Grid& grid_;
    ConfigView cfgView_;
    int syncOverdueDays_ = 0;
    const int fileIconSize_ = getResourceImage(L"msg_error_sicon").GetHeight();
};
}


void cfggrid::init(Grid& grid)
{
    const int rowHeight = GridDataCfg::getRowDefaultHeight(grid);

    grid.setDataProvider(std::make_shared<GridDataCfg>(grid));
    grid.showRowLabel(false);
    grid.setRowHeight(rowHeight);
    grid.setColumnLabelHeight(rowHeight + fastFromDIP(2));
}


ConfigView& cfggrid::getDataView(Grid& grid)
{
    if (auto* prov = dynamic_cast<GridDataCfg*>(grid.getDataProvider()))
        return prov->getDataView();
    throw std::runtime_error(std::string(__FILE__) + "[" + numberTo<std::string>(__LINE__) + "] cfggrid was not initialized.");
}


void cfggrid::addAndSelect(Grid& grid, const std::vector<Zstring>& filePaths, bool scrollToSelection)
{
    getDataView(grid).addCfgFiles(filePaths);
    grid.Refresh(); //[!] let Grid know about changed row count *before* fiddling with selection!!!

    grid.clearSelection(GridEventPolicy::DENY);

    const std::set<Zstring, LessNativePath> pathsSorted(filePaths.begin(), filePaths.end());
    std::optional<size_t> selectionTopRow;

    for (size_t i = 0; i < grid.getRowCount(); ++i)
        if (pathsSorted.find(getDataView(grid).getItem(i)->cfgItem.cfgFilePath) != pathsSorted.end())
        {
            if (!selectionTopRow)
                selectionTopRow = i;

            grid.selectRow(i, GridEventPolicy::DENY);
        }

    if (scrollToSelection && selectionTopRow)
        grid.makeRowVisible(*selectionTopRow);
}


int cfggrid::getSyncOverdueDays(Grid& grid)
{
    if (auto* prov = dynamic_cast<GridDataCfg*>(grid.getDataProvider()))
        return prov->getSyncOverdueDays();
    throw std::runtime_error(std::string(__FILE__) + "[" + numberTo<std::string>(__LINE__) + "] cfggrid was not initialized.");
}


void cfggrid::setSyncOverdueDays(Grid& grid, int syncOverdueDays)
{
    auto* prov = dynamic_cast<GridDataCfg*>(grid.getDataProvider());
    if (!prov)
        throw std::runtime_error(std::string(__FILE__) + "[" + numberTo<std::string>(__LINE__) + "] cfggrid was not initialized.");

    prov->setSyncOverdueDays(syncOverdueDays);
    grid.Refresh();
}

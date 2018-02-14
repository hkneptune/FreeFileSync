// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "cfg_grid.h"
#include <zen/time.h>
#include <zen/basic_math.h>
#include <wx+/dc.h>
#include <wx+/rtl.h>
#include <wx+/image_resources.h>
#include <wx/settings.h>
#include "../lib/icon_buffer.h"
#include "../lib/ffs_paths.h"

using namespace zen;
using namespace fff;


Zstring fff::getLastRunConfigPath()
{
    return getConfigDirPathPf() + Zstr("LastRun.ffs_gui");
}


void ConfigView::addCfgFiles(const std::vector<Zstring>& filePaths)
{
    //determine highest "last use" index number of m_listBoxHistory
    int lastUseIndexMax = 0;
    for (const auto& item : cfgList_)
        lastUseIndexMax = std::max(lastUseIndexMax, item.second.lastUseIndex);

    for (const Zstring& filePath : filePaths)
    {
        auto it = cfgList_.find(filePath);
        if (it == cfgList_.end())
        {
            Details detail = {};
            detail.filePath  = filePath;
            detail.lastUseIndex = ++lastUseIndexMax;

            std::tie(detail.name, detail.cfgType, detail.isLastRunCfg) = [&]
            {
                if (equalFilePath(filePath, lastRunConfigPath_))
                    return std::make_tuple(utfTo<Zstring>(L"<" + _("Last session") + L">"), Details::CFG_TYPE_GUI, true);

                const Zstring fileName = afterLast(filePath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL);

                if (endsWith(fileName, Zstr(".ffs_gui"), CmpFilePath()))
                    return std::make_tuple(beforeLast(fileName, Zstr('.'), IF_MISSING_RETURN_NONE), Details::CFG_TYPE_GUI, false);
                else if (endsWith(fileName, Zstr(".ffs_batch"), CmpFilePath()))
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
    const std::set<Zstring, LessFilePath> pathsSorted(filePaths.begin(), filePaths.end());

    erase_if(cfgListView_, [&](auto it) { return pathsSorted.find(it->first) != pathsSorted.end(); });

    for (const Zstring& filePath : filePaths)
        cfgList_.erase(filePath);

    assert(cfgList_.size() == cfgListView_.size());
}


void ConfigView::setLastSyncTime(const std::vector<std::pair<Zstring, time_t>>& syncTimes)
{
    for (const auto& st : syncTimes)
    {
        auto it = cfgList_.find(st.first);
        if (it != cfgList_.end())
            it->second.lastSyncTime = st.second;
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

        return makeSortDirection(std::greater<>(), Int2Type<ascending>())(lhs->second.lastSyncTime, rhs->second.lastSyncTime);
        //[!] ascending LAST_SYNC shows lowest "days past" first <=> highest lastSyncTime first
    };

    switch (sortColumn_)
    {
        case ColumnTypeCfg::NAME:
            std::sort(cfgListView_.begin(), cfgListView_.end(), makeSortDirection(lessCfgName, Int2Type<ascending>()));
            break;
        case ColumnTypeCfg::LAST_SYNC:
            std::sort(cfgListView_.begin(), cfgListView_.end(), lessLastSync);
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
class GridDataCfg : public GridData
{
public:
    GridDataCfg(int fileIconSize) : fileIconSize_(fileIconSize) {}

    ConfigView& getDataView() { return cfgView_; }

    static int getRowDefaultHeight(const Grid& grid)
    {
        return grid.getMainWin().GetCharHeight();
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
                {
                    if (item->isLastRunCfg)
                        return std::wstring();

                    if (item->lastSyncTime == 0)
                        return std::wstring(1, EN_DASH);

                    const int daysPast = getDaysPast(item->lastSyncTime);
                    if (daysPast == 0)
                        return _("Today");

                    return _P("1 day", "%x days", daysPast);
                }
                    //return formatTime<std::wstring>(FORMAT_DATE_TIME, getLocalTime(item->lastSyncTime));
            }
        return std::wstring();
    }

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover) override
    {
        wxRect rectTmp = rect;

        wxDCTextColourChanger dummy(dc); //accessibility: always set both foreground AND background colors!
        if (selected)
            dummy.Set(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
        else
        {
            if (enabled)
                dummy.Set(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
            else
                dummy.Set(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
        }

        if (const ConfigView::Details* item = cfgView_.getItem(row))
            switch (static_cast<ColumnTypeCfg>(colType))
            {
                case ColumnTypeCfg::NAME:
                    rectTmp.x     += COLUMN_GAP_LEFT;
                    rectTmp.width -= COLUMN_GAP_LEFT;

                    switch (item->cfgType)
                    {
                        case ConfigView::Details::CFG_TYPE_NONE:
                            break;
                        case ConfigView::Details::CFG_TYPE_GUI:
                            drawBitmapRtlNoMirror(dc, enabled ? syncIconSmall_ : syncIconSmall_.ConvertToDisabled(), rectTmp, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
                            break;
                        case ConfigView::Details::CFG_TYPE_BATCH:
                            drawBitmapRtlNoMirror(dc, enabled ? batchIconSmall_ : batchIconSmall_.ConvertToDisabled(), rectTmp, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
                            break;
                    }
                    rectTmp.x     += fileIconSize_ + COLUMN_GAP_LEFT;
                    rectTmp.width -= fileIconSize_ + COLUMN_GAP_LEFT;

                    drawCellText(dc, rectTmp, getValue(row, colType));
                    break;

                case ColumnTypeCfg::LAST_SYNC:
                {
                    wxDCTextColourChanger dummy2(dc);
                    if (syncOverdueDays_ > 0)
                        if (getDaysPast(item->lastSyncTime) >= syncOverdueDays_)
                            dummy2.Set(*wxRED);

                    drawCellText(dc, rectTmp, getValue(row, colType), wxALIGN_CENTER);
                }
                break;
            }
    }

    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        // -> synchronize renderCell() <-> getBestSize()

        switch (static_cast<ColumnTypeCfg>(colType))
        {
            case ColumnTypeCfg::NAME:
                return COLUMN_GAP_LEFT + fileIconSize_ + COLUMN_GAP_LEFT + dc.GetTextExtent(getValue(row, colType)).GetWidth() + COLUMN_GAP_LEFT;

            case ColumnTypeCfg::LAST_SYNC:
                return COLUMN_GAP_LEFT + dc.GetTextExtent(getValue(row, colType)).GetWidth() + COLUMN_GAP_LEFT;
        }
        return 0;
    }

    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected) override
    {
        if (selected)
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
        else
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    }

    void renderColumnLabel(Grid& tree, wxDC& dc, const wxRect& rect, ColumnType colType, bool highlighted) override
    {
        wxRect rectInside = drawColumnLabelBorder(dc, rect);
        drawColumnLabelBackground(dc, rectInside, highlighted);

        rectInside.x     += COLUMN_GAP_LEFT;
        rectInside.width -= COLUMN_GAP_LEFT;
        drawColumnLabelText(dc, rectInside, getColumnLabel(colType));

        auto sortInfo = cfgView_.getSortDirection();
        if (colType == static_cast<ColumnType>(sortInfo.first))
        {
            const wxBitmap& marker = getResourceImage(sortInfo.second ? L"sortAscending" : L"sortDescending");
            drawBitmapRtlNoMirror(dc, marker, rectInside, wxALIGN_CENTER_HORIZONTAL);
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
        }
        return std::wstring();
    }

private:
    ConfigView cfgView_;
    int syncOverdueDays_ = 0;
    const int fileIconSize_;
    const wxBitmap syncIconSmall_  = getResourceImage(L"sync" ).ConvertToImage().Scale(fileIconSize_, fileIconSize_, wxIMAGE_QUALITY_BILINEAR); //looks sharper than wxIMAGE_QUALITY_HIGH!
    const wxBitmap batchIconSmall_ = getResourceImage(L"batch").ConvertToImage().Scale(fileIconSize_, fileIconSize_, wxIMAGE_QUALITY_BILINEAR);
};
}


void cfggrid::init(Grid& grid)
{
    const int rowHeight    = GridDataCfg::getRowDefaultHeight(grid);

    auto prov = std::make_shared<GridDataCfg>(rowHeight /*fileIconSize*/);

    grid.setDataProvider(prov);
    grid.showRowLabel(false);
    grid.setRowHeight(rowHeight);

    grid.setColumnLabelHeight(rowHeight + 2);
}


ConfigView& cfggrid::getDataView(Grid& grid)
{
    if (auto* prov = dynamic_cast<GridDataCfg*>(grid.getDataProvider()))
        return prov->getDataView();
    throw std::runtime_error("cfggrid was not initialized! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
}


void cfggrid::addAndSelect(Grid& grid, const std::vector<Zstring>& filePaths, bool scrollToSelection)
{
    auto* prov = dynamic_cast<GridDataCfg*>(grid.getDataProvider());
    if (!prov)
        throw std::runtime_error("cfggrid was not initialized! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    prov->getDataView().addCfgFiles(filePaths);
    grid.Refresh(); //[!] let Grid know about changed row count *before* fiddling with selection!!!

    grid.clearSelection(GridEventPolicy::DENY_GRID_EVENT);

    const std::set<Zstring, LessFilePath> pathsSorted(filePaths.begin(), filePaths.end());
    ptrdiff_t selectionTopRow = -1;

    for (size_t i = 0; i < grid.getRowCount(); ++i)
        if (const ConfigView::Details* cfg = prov->getDataView().getItem(i))
        {
            if (pathsSorted.find(cfg->filePath) != pathsSorted.end())
            {
                if (selectionTopRow < 0)
                    selectionTopRow = i;

                grid.selectRow(i, GridEventPolicy::DENY_GRID_EVENT);
            }
        }
        else
            assert(false);

    if (scrollToSelection && selectionTopRow >= 0)
        grid.makeRowVisible(selectionTopRow);
}


int cfggrid::getSyncOverdueDays(Grid& grid)
{
    if (auto* prov = dynamic_cast<GridDataCfg*>(grid.getDataProvider()))
        return prov->getSyncOverdueDays();
    throw std::runtime_error("cfggrid was not initialized! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
}


void cfggrid::setSyncOverdueDays(Grid& grid, int syncOverdueDays)
{
    auto* prov = dynamic_cast<GridDataCfg*>(grid.getDataProvider());
    if (!prov)
        throw std::runtime_error("cfggrid was not initialized! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

    prov->setSyncOverdueDays(syncOverdueDays);
    grid.Refresh();
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "cfg_grid.h"
#include <zen/time.h>
#include <zen/process_exec.h>
#include <wx+/dc.h>
#include <wx+/rtl.h>
#include <wx+/image_resources.h>
#include <wx+/popup_dlg.h>
//#include <wx+/image_tools.h>
#include <wx+/std_button_layout.h>
#include <wx/settings.h>
//#include "../icon_buffer.h"
#include "../ffs_paths.h"
#include "../afs/native.h"

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


Zstring fff::getLastRunConfigPath()
{
    return appendPath(getConfigDirPath(), Zstr("LastRun.ffs_gui"));
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

    //list is stored with last used files first in XML, however addCfgFilesImpl() expects them last!!!
    std::reverse(filePaths.begin(), filePaths.end());

    cfgList_    .clear();
    cfgListView_.clear();
    addCfgFilesImpl(filePaths);

    for (const ConfigFileItem& item : cfgItems)
        cfgList_.find(item.cfgFilePath)->second.cfgItem = item; //cfgFilePath must exist after addCfgFilesImpl()!

    sortListView();
}


void ConfigView::addCfgFiles(const std::vector<Zstring>& filePaths)
{
    addCfgFilesImpl(filePaths);
    sortListView();
}


void ConfigView::addCfgFilesImpl(const std::vector<Zstring>& filePaths)
{
    //determine highest "last use" index number of m_listBoxHistory
    int lastUseIndexMax = 0;
    for (const auto& [filePath, details] : cfgList_)
        lastUseIndexMax = std::max(lastUseIndexMax, details.lastUseIndex);

    for (const Zstring& filePath : filePaths)
        if (auto it = cfgList_.find(filePath);
            it == cfgList_.end())
        {
            Details detail{.lastUseIndex = ++lastUseIndexMax};
            detail.cfgItem.cfgFilePath = filePath;

            std::tie(detail.name, detail.cfgType, detail.isLastRunCfg) = [&]
            {
                if (equalNativePath(filePath, lastRunConfigPath_))
                    return std::make_tuple(utfTo<Zstring>(L'[' + _("Last session") + L']'), Details::CFG_TYPE_GUI, true);

                const Zstring fileName = getItemName(filePath);

                if (endsWithAsciiNoCase(fileName, ".ffs_gui"))
                    return std::make_tuple(beforeLast(fileName, Zstr('.'), IfNotFoundReturn::none), Details::CFG_TYPE_GUI, false);
                else if (endsWithAsciiNoCase(fileName, ".ffs_batch"))
                    return std::make_tuple(beforeLast(fileName, Zstr('.'), IfNotFoundReturn::none), Details::CFG_TYPE_BATCH, false);
                else
                    return std::make_tuple(fileName, Details::CFG_TYPE_NONE, false);
            }();

            auto itNew = cfgList_.emplace_hint(cfgList_.end(), filePath, std::move(detail));
            cfgListView_.push_back(itNew);
        }
        else
            it->second.lastUseIndex = ++lastUseIndexMax;
}


void ConfigView::removeItems(const std::vector<Zstring>& filePaths)
{
    for (const Zstring& filePath : filePaths)
        if (auto it = cfgList_.find(filePath);
            it != cfgList_.end())
        {
            std::erase(cfgListView_, it);
            cfgList_.erase(it);
        }
        else assert(false);

    assert(cfgList_.size() == cfgListView_.size());

    if (sortColumn_ == ColumnTypeCfg::name)
        sortListView(); //needed if top element of colored-group is removed
}


void ConfigView::renameItem(const Zstring& pathFrom, const Zstring& pathTo)
{
    auto it = cfgList_.find(pathFrom);
    assert(it != cfgList_.end());
    if (it != cfgList_.end())
    {
        const Details detailsOld = it->second;

        std::erase(cfgListView_, it);
        cfgList_.erase(it);
        assert(cfgList_.size() == cfgListView_.size());

        addCfgFilesImpl({pathTo});

        //let's not lose certain metadata after renaming!
        auto it2 = cfgList_.find(pathTo);
        assert(it2 != cfgList_.end());
        if (it2 != cfgList_.end())
        {
            it2->second.cfgItem.lastRunStats = detailsOld.cfgItem.lastRunStats;
            it2->second.cfgItem.backColor    = detailsOld.cfgItem.backColor;
            it2->second.lastUseIndex         = detailsOld.lastUseIndex;
            it2->second.notes                = detailsOld.notes;
        }
        sortListView();
    }
}


void ConfigView::setNotes(const Zstring& filePath, const std::wstring& notes)
{
    if (auto it = cfgList_.find(filePath);
        it != cfgList_.end())
        it->second.notes = notes;
    else assert(false);
}


void ConfigView::setLastRunStats(const std::vector<Zstring>& filePaths, const LastRunStats& lastRun)
{
    for (const Zstring& filePath : filePaths)
    {
        auto it = cfgList_.find(filePath);
        assert(it != cfgList_.end());
        if (it != cfgList_.end())
            it->second.cfgItem.lastRunStats = lastRun;
    }

    if (sortColumn_ != ColumnTypeCfg::name)
        sortListView(); //needed if sorted by last sync time, or log
}


void ConfigView::setLastInSyncTime(const std::vector<Zstring>& filePaths, time_t lastRunTime)
{
    for (const Zstring& filePath : filePaths)
    {
        auto it = cfgList_.find(filePath);
        assert(it != cfgList_.end());
        if (it != cfgList_.end())
            it->second.cfgItem.lastRunStats.startTime = lastRunTime;
    }

    if (sortColumn_ != ColumnTypeCfg::name)
        sortListView(); //needed if sorted by last sync time, or log
}


void ConfigView::setBackColor(const std::vector<Zstring>& filePaths, const wxColor& col, bool previewOnly)
{
    for (const Zstring& filePath : filePaths)
        if (auto it = cfgList_.find(filePath);
            it != cfgList_.end())
        {
            if (previewOnly)
                it->second.cfgItem.backColorPreview = col;
            else
            {
                it->second.cfgItem.backColor = col;
                it->second.cfgItem.backColorPreview = wxNullColour;
            }
        }
        else assert(false);

    if (!previewOnly && sortColumn_ == ColumnTypeCfg::name)
        sortListView(); //needed if top element of colored-group is removed
}


const ConfigView::Details* ConfigView::getItem(size_t row) const
{
    if (row < cfgListView_.size())
        return &cfgListView_[row]->second;
    return nullptr;
}


std::pair<const ConfigView::Details*, size_t/*row*/> ConfigView::getItem(const Zstring& filePath) const
{
    if (auto it = cfgList_.find(filePath);
        it != cfgList_.end())
        return {&it->second, std::find(cfgListView_.begin(), cfgListView_.end(), it) - cfgListView_.begin()};
    return {};
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
            return lhs->second.isLastRunCfg; //"last session" should be at top position!

        return LessNaturalSort()(lhs->second.name, rhs->second.name);
    };

    const auto lessLastSync = [](CfgFileList::iterator lhs, CfgFileList::iterator rhs)
    {
        if (lhs->second.isLastRunCfg != rhs->second.isLastRunCfg)
            return lhs->second.isLastRunCfg < rhs->second.isLastRunCfg; //"last session" label should be (always) last

        return makeSortDirection(std::greater(), std::bool_constant<ascending>())(
                   lhs->second.cfgItem.lastRunStats.startTime,
                   rhs->second.cfgItem.lastRunStats.startTime);
        //[!] ascending lastSync shows lowest "days past" first <=> highest lastSyncTime first
    };

    const auto lessSyncResult = [](CfgFileList::iterator lhs, CfgFileList::iterator rhs)
    {
        if (lhs->second.isLastRunCfg != rhs->second.isLastRunCfg)
            return lhs->second.isLastRunCfg < rhs->second.isLastRunCfg; //"last session" label should be (always) last

        const bool wasRunL = lhs->second.cfgItem.lastRunStats.startTime != 0;
        const bool wasRunR = rhs->second.cfgItem.lastRunStats.startTime != 0;
        if (wasRunL != wasRunR)
            return wasRunL > wasRunR; //move sync jobs that were never run to the back

        //primary sort order
        if (wasRunL && lhs->second.cfgItem.lastRunStats.syncResult != rhs->second.cfgItem.lastRunStats.syncResult)
            return makeSortDirection(std::greater(), std::bool_constant<ascending>())(lhs->second.cfgItem.lastRunStats.syncResult, rhs->second.cfgItem.lastRunStats.syncResult);

        //secondary sort order
        return LessNaturalSort()(lhs->second.name, rhs->second.name);
    };

    switch (sortColumn_)
    {
        case ColumnTypeCfg::name:
            //pre-sort by name
            std::sort(cfgListView_.begin(), cfgListView_.end(), lessCfgName);

            //aggregate groups by color (*almost* like a std::stable_sort)
            for (auto it = cfgListView_.begin(); it != cfgListView_.end(); )
                if ((*it)->second.cfgItem.backColor.IsOk())
                    it = std::stable_partition(it + 1, cfgListView_.end(),
                    [&groupCol = (*it)->second.cfgItem.backColor](CfgFileList::iterator item) { return item->second.cfgItem.backColor == groupCol; });
            else
                ++it;

            //simplify aggregation logic by not having to consider "ascending/descending"
            if (!ascending)
                std::reverse(cfgListView_.begin(), cfgListView_.end());
            break;

        case ColumnTypeCfg::lastSync:
            std::sort(cfgListView_.begin(), cfgListView_.end(), lessLastSync);
            break;

        case ColumnTypeCfg::lastLog:
            std::sort(cfgListView_.begin(), cfgListView_.end(), lessSyncResult);
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
        grid.Bind(EVENT_GRID_MOUSE_LEFT_DOWN,   [this](GridClickEvent& event) { onMouseLeft      (event); });
        grid.Bind(EVENT_GRID_MOUSE_LEFT_DOUBLE, [this](GridClickEvent& event) { onMouseLeftDouble(event); });
    }

    ConfigView& getDataView() { return cfgView_; }

    static int getRowDefaultHeight(const Grid& grid)
    {
        return std::max(dipToWxsize(getMenuIconDipSize()),
                        grid.getMainWin().GetCharHeight()) + dipToWxsize(1) /*extra space*/;
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

        //truncate down to midnight => incorrect during DST switches, but doesn't matter due to rounding below
        now  -= tcNow .hour * 3600 + tcNow .minute * 60 + tcNow .second;
        last -= tcLast.hour * 3600 + tcLast.minute * 60 + tcLast.second;

        return numeric::intDivRound(now - last, 24 * 3600);
    }

    std::wstring getValue(size_t row, ColumnType colType) const override
    {
        if (const ConfigView::Details* item = cfgView_.getItem(row))
            switch (static_cast<ColumnTypeCfg>(colType))
            {
                case ColumnTypeCfg::name:
                    return utfTo<std::wstring>(item->name);

                case ColumnTypeCfg::lastSync:
                    if (!item->isLastRunCfg)
                    {
                        if (item->cfgItem.lastRunStats.startTime == 0)
                            return std::wstring(1, EN_DASH);

                        //return utfTo<std::wstring>(formatTime(formatDateTimeTag, getLocalTime(item->cfgItem.lastRunStats.startTime)));

                        const int daysPast = getDaysPast(item->cfgItem.lastRunStats.startTime);
                        return daysPast == 0 ?
                               utfTo<std::wstring>(formatTime(Zstr("%R") /*equivalent to "%H:%M"*/, getLocalTime(item->cfgItem.lastRunStats.startTime))) :
                               //_("Today") :
                               _P("1 day", "%x days", daysPast);
                    }
                    break;

                case ColumnTypeCfg::lastLog:
                    if (!item->isLastRunCfg && item->cfgItem.lastRunStats.startTime != 0)
                        return getSyncResultLabel(item->cfgItem.lastRunStats.syncResult);
                    break;
            }
        return std::wstring();
    }

    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected, HoverArea rowHover) override
    {
        if (selected)
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
        //else: wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW) already the default!
    }

    enum class HoverAreaConfig
    {
        name,
        link,
    };

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover) override
    {
        wxDCTextColourChanger textColor(dc); //accessibility: always set both foreground AND background colors!
        if (selected)
            textColor.Set(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
        //else: wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT) already the default!

        if (const ConfigView::Details* item = cfgView_.getItem(row))
            switch (static_cast<ColumnTypeCfg>(colType))
            {
                case ColumnTypeCfg::name:
                {
                    wxColor backColor = item->cfgItem.backColor;
                    if (item->cfgItem.backColorPreview.IsOk())
                        backColor = item->cfgItem.backColorPreview;

                    if (backColor.IsOk())
                    {
                        wxRect rectTmp = rect;
                        if (!selected || item->cfgItem.backColorPreview.IsOk())
                        {
                            rectTmp.width = rect.width * 2 / 3;
                            clearArea(dc, rectTmp, backColor); //accessibility: always set both foreground AND background colors!
                            textColor.Set(*wxBLACK);            //

                            rectTmp.x += rectTmp.width;
                            rectTmp.width = rect.width - rectTmp.width;
                            dc.GradientFillLinear(rectTmp, backColor, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW), wxEAST);
                        }
                        else //always show a glimpse of the background color
                        {
                            rectTmp.width = getColumnGapLeft() + dipToWxsize(getMenuIconDipSize());
                            clearArea(dc, rectTmp, backColor);

                            rectTmp.x += rectTmp.width;
                            rectTmp.width = getColumnGapLeft();
                            dc.GradientFillLinear(rectTmp, backColor, wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT), wxEAST);
                        }
                    }
                    if (!selected && static_cast<HoverAreaConfig>(rowHover) == HoverAreaConfig::name)
                        drawRectangleBorder(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT), dipToWxsize(1));

                    //-------------------------------------------------------------------------------------
                    wxRect rectTmp = rect;
                    rectTmp.x     += getColumnGapLeft();
                    rectTmp.width -= getColumnGapLeft();

                    const wxImage cfgIcon = [&]
                    {
                        switch (item->cfgType)
                        {
                            case ConfigView::Details::CFG_TYPE_NONE:
                                return wxNullImage;
                            case ConfigView::Details::CFG_TYPE_GUI:
                                return loadImage("start_sync", dipToScreen(getMenuIconDipSize()));
                            case ConfigView::Details::CFG_TYPE_BATCH:
                                return loadImage("cfg_batch", dipToScreen(getMenuIconDipSize()));
                        }
                        assert(false);
                        return wxNullImage;
                    }();
                    if (cfgIcon.IsOk())
                        drawBitmapRtlNoMirror(dc, enabled ? cfgIcon : cfgIcon.ConvertToDisabled(), rectTmp, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

                    rectTmp.x     += dipToWxsize(getMenuIconDipSize()) + getColumnGapLeft();
                    rectTmp.width -= dipToWxsize(getMenuIconDipSize()) + getColumnGapLeft();

                    if (!item->notes.empty())
                        rectTmp.width -= dipToWxsize(getMenuIconDipSize()) + getColumnGapLeft();

                    drawCellText(dc, rectTmp, getValue(row, colType));

                    if (!item->notes.empty())
                    {
                        rectTmp.x += rectTmp.width;
                        rectTmp.width = dipToWxsize(getMenuIconDipSize());

                        const wxImage notesIcon = loadImage("notes", dipToScreen(getMenuIconDipSize()));
                        drawBitmapRtlNoMirror(dc, enabled ? notesIcon : notesIcon.ConvertToDisabled(), rectTmp, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
                    }
                }
                break;

                case ColumnTypeCfg::lastSync:
                {
                    wxDCTextColourChanger textColor2(dc);
                    if (syncOverdueDays_ > 0)
                        if (getDaysPast(item->cfgItem.lastRunStats.startTime) >= syncOverdueDays_)
                            textColor2.Set(*wxRED);

                    drawCellText(dc, rect, getValue(row, colType), wxALIGN_CENTER);
                }
                break;

                case ColumnTypeCfg::lastLog:
                    if (!item->isLastRunCfg && item->cfgItem.lastRunStats.startTime != 0)
                    {
                        const wxImage statusIcon = [&]
                        {
                            switch (item->cfgItem.lastRunStats.syncResult)
                            {
                                case TaskResult::success:
                                    return loadImage("msg_success", dipToScreen(getMenuIconDipSize()));
                                case TaskResult::warning:
                                    return loadImage("msg_warning", dipToScreen(getMenuIconDipSize()));
                                case TaskResult::error:
                                case TaskResult::cancelled:
                                    return loadImage("msg_error", dipToScreen(getMenuIconDipSize()));
                            }
                            assert(false);
                            return wxNullImage;
                        }();
                        drawBitmapRtlNoMirror(dc, enabled ? statusIcon : statusIcon.ConvertToDisabled(), rect, wxALIGN_CENTER);
                    }
                    if (static_cast<HoverAreaConfig>(rowHover) == HoverAreaConfig::link)
                        drawBitmapRtlNoMirror(dc, loadImage("file_link_16"), rect, wxALIGN_CENTER);
                    break;
            }
    }

    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        // -> synchronize renderCell() <-> getBestSize()

        switch (static_cast<ColumnTypeCfg>(colType))
        {
            case ColumnTypeCfg::name:
                return getColumnGapLeft() + dipToWxsize(getMenuIconDipSize()) + getColumnGapLeft() + dc.GetTextExtent(getValue(row, colType)).GetWidth() + getColumnGapLeft();

            case ColumnTypeCfg::lastSync:
                return getColumnGapLeft() + dc.GetTextExtent(getValue(row, colType)).GetWidth() + getColumnGapLeft();

            case ColumnTypeCfg::lastLog:
                return dipToWxsize(getMenuIconDipSize());
        }
        assert(false);
        return 0;
    }

    HoverArea getMouseHover(wxDC& dc, size_t row, ColumnType colType, int cellRelativePosX, int cellWidth) override
    {
        if (const ConfigView::Details* item = cfgView_.getItem(row))
        {
            switch (static_cast<ColumnTypeCfg>(colType))
            {
                case ColumnTypeCfg::name:
                case ColumnTypeCfg::lastSync:
                    //if (!item->notes.empty() && cellRelativePosX >= cellWidth - (getColumnGapLeft() + dipToWxsize(getMenuIconDipSize()) + getColumnGapLeft()))
                    break;
                case ColumnTypeCfg::lastLog:
                    if (!item->isLastRunCfg && !getNativeItemPath(item->cfgItem.lastRunStats.logFilePath).empty())
                        return static_cast<HoverArea>(HoverAreaConfig::link);
                    break;
            }
            return static_cast<HoverArea>(HoverAreaConfig::name);
        }
        return HoverArea::none;
    }

    void renderColumnLabel(wxDC& dc, const wxRect& rect, ColumnType colType, bool enabled, bool highlighted) override
    {
        const auto colTypeCfg = static_cast<ColumnTypeCfg>(colType);

        const wxRect rectInner = drawColumnLabelBackground(dc, rect, highlighted);
        wxRect rectRemain = rectInner;

        wxImage sortMarker;
        if (const auto [sortCol, ascending] = cfgView_.getSortDirection();
            colTypeCfg == sortCol)
        {
            sortMarker = loadImage(ascending ? "sort_ascending" : "sort_descending");
            if (!enabled)
                sortMarker = sortMarker.ConvertToDisabled();
        }

        switch (colTypeCfg)
        {
            case ColumnTypeCfg::name:
            case ColumnTypeCfg::lastSync:
                rectRemain.x     += getColumnGapLeft();
                rectRemain.width -= getColumnGapLeft();
                drawColumnLabelText(dc, rectRemain, getColumnLabel(colType), enabled);

                if (sortMarker.IsOk())
                    drawBitmapRtlNoMirror(dc, sortMarker, rectInner, wxALIGN_CENTER_HORIZONTAL);
                break;

            case ColumnTypeCfg::lastLog:
            {
                const wxImage logIcon = loadImage("log_file", dipToScreen(getMenuIconDipSize()));
                drawBitmapRtlNoMirror(dc, enabled ? logIcon : logIcon.ConvertToDisabled(), rectInner, wxALIGN_CENTER);

                if (sortMarker.IsOk())
                {
                    const int gapLeft = (rectInner.width + logIcon.GetWidth()) / 2;
                    rectRemain.x     += gapLeft;
                    rectRemain.width -= gapLeft;

                    drawBitmapRtlNoMirror(dc, sortMarker, rectRemain, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
                }
            }
            break;
        }
    }

    std::wstring getColumnLabel(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeCfg>(colType))
        {
            case ColumnTypeCfg::name:
                return _("Name");
            case ColumnTypeCfg::lastSync:
                return _("Last sync");
            case ColumnTypeCfg::lastLog:
                return _("Log");
        }
        return std::wstring();
    }

    std::wstring getToolTip(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeCfg>(colType))
        {
            case ColumnTypeCfg::name:
            case ColumnTypeCfg::lastSync:
                break;
            case ColumnTypeCfg::lastLog:
                return getColumnLabel(colType);
        }
        return std::wstring();
    }

    std::wstring getToolTip(size_t row, ColumnType colType, HoverArea rowHover) override
    {
        if (const ConfigView::Details* item = cfgView_.getItem(row))
        {
            switch (static_cast<ColumnTypeCfg>(colType))
            {
                case ColumnTypeCfg::name:
                case ColumnTypeCfg::lastSync:
                    break;

                case ColumnTypeCfg::lastLog:
                    if (!item->isLastRunCfg && item->cfgItem.lastRunStats.startTime != 0)
                    {
                        std::wstring tooltip = getSyncResultLabel(item->cfgItem.lastRunStats.syncResult) + L"\n";

                        if (item->cfgItem.lastRunStats.errors   > 0) tooltip += TAB_SPACE + _("Errors:")   + L' ' + formatNumber(item->cfgItem.lastRunStats.errors) + L"\n";
                        if (item->cfgItem.lastRunStats.warnings > 0) tooltip += TAB_SPACE + _("Warnings:") + L' ' + formatNumber(item->cfgItem.lastRunStats.warnings) + L"\n";

                        tooltip += TAB_SPACE + _("Items processed:") + L' ' + formatNumber(item->cfgItem.lastRunStats.itemsProcessed) +
                                   L" (" + formatFilesizeShort(item->cfgItem.lastRunStats.bytesProcessed) + L")\n";

                        const int64_t totalTimeSec = std::chrono::duration_cast<std::chrono::seconds>(item->cfgItem.lastRunStats.totalTime).count();
                        tooltip += TAB_SPACE + _("Total time:") + L' ' + utfTo<std::wstring>(formatTimeSpan(totalTimeSec));

                        //if (!AFS::isNullPath(item->cfgItem.lastRunStats.logFilePath))
                        //    tooltip += L"\n" + AFS::getDisplayPath(item->cfgItem.lastRunStats.logFilePath);
                        return tooltip;
                    }
                    break;
            }
            return item->notes;
        }
        return std::wstring();
    }

    void onMouseLeft(GridClickEvent& event)
    {
        if (const ConfigView::Details* item = cfgView_.getItem(event.row_))
            switch (static_cast<HoverAreaConfig>(event.hoverArea_))
            {
                case HoverAreaConfig::name:
                    break;
                case HoverAreaConfig::link:
                    try
                    {
                        if (const Zstring& nativePath = getNativeItemPath(item->cfgItem.lastRunStats.logFilePath);
                            !nativePath.empty())
                            openWithDefaultApp(nativePath); //throw FileError
                        else
                            assert(false); //see getMouseHover()
                    }
                    catch (const FileError& e) { showNotificationDialog(&grid_, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString())); }
                    return;
            }
        event.Skip();
    }

    void onMouseLeftDouble(GridClickEvent& event)
    {
        switch (static_cast<HoverAreaConfig>(event.hoverArea_))
        {
            case HoverAreaConfig::name:
                break;
            case HoverAreaConfig::link:
                return; //swallow event here before MainDialog considers it as a request to start comparison
        }
        event.Skip();
    }

private:
    Grid& grid_;
    ConfigView cfgView_;
    int syncOverdueDays_ = 0;
};
}


void cfggrid::init(Grid& grid)
{
    const int rowHeight = GridDataCfg::getRowDefaultHeight(grid);

    grid.setDataProvider(std::make_shared<GridDataCfg>(grid));
    grid.showRowLabel(false);
    grid.setRowHeight(rowHeight);
    grid.setColumnLabelHeight(rowHeight + dipToWxsize(2));
}


ConfigView& cfggrid::getDataView(Grid& grid)
{
    if (auto* prov = dynamic_cast<GridDataCfg*>(grid.getDataProvider()))
        return prov->getDataView();
    throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] cfggrid was not initialized.");
}


void cfggrid::addAndSelect(Grid& grid, const std::vector<Zstring>& filePaths, bool scrollToSelection)
{
    getDataView(grid).addCfgFiles(filePaths);
    grid.Refresh(); //[!] let Grid know about changed row count *before* fiddling with selection!!!

    const std::set<Zstring, LessNativePath> pathsSorted(filePaths.begin(), filePaths.end());
    std::vector<size_t> rowsToSelect;

    for (size_t row = 0; row < grid.getRowCount(); ++row)
        if (pathsSorted.contains(getDataView(grid).getItem(row)->cfgItem.cfgFilePath))
            rowsToSelect.push_back(row);

    if (scrollToSelection && !rowsToSelect.empty())
        grid.makeRowVisible(rowsToSelect[0]); //don't also set grid cursor: will confuse keyboard selection using shift and arrow keys

    grid.clearSelection(GridEventPolicy::deny);

    for (size_t row : rowsToSelect)
        grid.selectRow(row, GridEventPolicy::deny);
}


int cfggrid::getSyncOverdueDays(Grid& grid)
{
    if (auto* prov = dynamic_cast<GridDataCfg*>(grid.getDataProvider()))
        return prov->getSyncOverdueDays();
    throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] cfggrid was not initialized.");
}


void cfggrid::setSyncOverdueDays(Grid& grid, int syncOverdueDays)
{
    auto* prov = dynamic_cast<GridDataCfg*>(grid.getDataProvider());
    if (!prov)
        throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] cfggrid was not initialized.");

    prov->setSyncOverdueDays(syncOverdueDays);
    grid.Refresh();
}

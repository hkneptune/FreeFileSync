// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "file_grid.h"
#include <set>
#include <wx/dc.h>
#include <wx/settings.h>
#include <zen/i18n.h>
#include <zen/file_error.h>
#include <zen/basic_math.h>
#include <zen/format_unit.h>
#include <zen/scope_guard.h>
#include <wx+/tooltip.h>
#include <wx+/rtl.h>
#include <wx+/dc.h>
#include <wx+/image_tools.h>
#include <wx+/image_resources.h>
#include "../file_hierarchy.h"

using namespace zen;
using namespace fff;


const wxEventType fff::EVENT_GRID_CHECK_ROWS     = wxNewEventType();
const wxEventType fff::EVENT_GRID_SYNC_DIRECTION = wxNewEventType();


namespace
{
//let's NOT create wxWidgets objects statically:
inline wxColor getColorOrange   () { return { 238, 201,   0 }; }
inline wxColor getColorGrey     () { return { 212, 208, 200 }; }
inline wxColor getColorYellow   () { return { 247, 252,  62 }; }
//inline wxColor getColorYellowLight() { return { 253, 252, 169 }; }
inline wxColor getColorCmpRed   () { return { 255, 185, 187 }; }
inline wxColor getColorSyncBlue () { return { 185, 188, 255 }; }
inline wxColor getColorSyncGreen() { return { 196, 255, 185 }; }
inline wxColor getColorNotActive() { return { 228, 228, 228 }; } //light grey
inline wxColor getColorGridLine () { return { 192, 192, 192 }; } //light grey

const size_t ROW_COUNT_IF_NO_DATA = 0;

/*
class hierarchy:
                                GridDataBase
                                    /|\
                     ________________|________________
                    |                                |
               GridDataRim                           |
                   /|\                               |
          __________|__________                      |
         |                    |                      |
   GridDataLeft         GridDataRight          GridDataCenter
*/

std::pair<ptrdiff_t, ptrdiff_t> getVisibleRows(const Grid& grid) //returns range [from, to)
{
    const wxSize clientSize = grid.getMainWin().GetClientSize();
    if (clientSize.GetHeight() > 0)
    {
        const wxPoint topLeft = grid.CalcUnscrolledPosition(wxPoint(0, 0));
        const wxPoint bottom  = grid.CalcUnscrolledPosition(wxPoint(0, clientSize.GetHeight() - 1));

        const ptrdiff_t rowCount = grid.getRowCount();
        const ptrdiff_t rowFrom  = grid.getRowAtPos(topLeft.y); //return -1 for invalid position, rowCount if out of range
        const ptrdiff_t rowTo    = grid.getRowAtPos(bottom.y);
        if (rowFrom >= 0 && rowTo >= 0)
            return { rowFrom, std::min(rowTo + 1, rowCount) };
    }
    assert(false);
    return {};
}


void fillBackgroundDefaultColorAlternating(wxDC& dc, const wxRect& rect, bool evenRowNumber)
{
    //alternate background color to improve readability (while lacking cell borders)
    if (!evenRowNumber)
    {
        //accessibility, support high-contrast schemes => work with user-defined background color!
        const auto backCol = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

        auto incChannel = [](unsigned char c, int diff) { return static_cast<unsigned char>(std::max(0, std::min(255, c + diff))); };

        auto getAdjustedColor = [&](int diff)
        {
            return wxColor(incChannel(backCol.Red  (), diff),
                           incChannel(backCol.Green(), diff),
                           incChannel(backCol.Blue (), diff));
        };

        auto colorDist = [](const wxColor& lhs, const wxColor& rhs) //just some metric
        {
            return numeric::power<2>(static_cast<int>(lhs.Red  ()) - static_cast<int>(rhs.Red  ())) +
                   numeric::power<2>(static_cast<int>(lhs.Green()) - static_cast<int>(rhs.Green())) +
                   numeric::power<2>(static_cast<int>(lhs.Blue ()) - static_cast<int>(rhs.Blue ()));
        };

        const int signLevel = colorDist(backCol, *wxBLACK) < colorDist(backCol, *wxWHITE) ? 1 : -1; //brighten or darken

        const wxColor colOutter = getAdjustedColor(signLevel * 14); //just some very faint gradient to avoid visual distraction
        const wxColor colInner  = getAdjustedColor(signLevel * 11); //

        //clearArea(dc, rect, backColAlt);

        //add some nice background gradient
        wxRect rectUpper = rect;
        rectUpper.height /= 2;
        wxRect rectLower = rect;
        rectLower.y += rectUpper.height;
        rectLower.height -= rectUpper.height;
        dc.GradientFillLinear(rectUpper, colOutter, colInner, wxSOUTH);
        dc.GradientFillLinear(rectLower, colOutter, colInner, wxNORTH);
    }
    else
        clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
}


class IconUpdater;
class GridEventManager;
class GridDataLeft;
class GridDataRight;

struct IconManager
{
    IconManager(GridDataLeft& provLeft, GridDataRight& provRight, IconBuffer::IconSize sz) :
        iconBuffer(sz),
        dirIcon        (IconBuffer::genericDirIcon (sz)),
        linkOverlayIcon(IconBuffer::linkOverlayIcon(sz)),
        iconUpdater(std::make_unique<IconUpdater>(provLeft, provRight, iconBuffer)) {}

    void startIconUpdater();
    IconBuffer& refIconBuffer() { return iconBuffer; }

    const wxBitmap& getGenericDirIcon () const { return dirIcon;         }
    const wxBitmap& getLinkOverlayIcon() const { return linkOverlayIcon; }

private:
    IconBuffer iconBuffer;
    const wxBitmap dirIcon;
    const wxBitmap linkOverlayIcon;

    std::unique_ptr<IconUpdater> iconUpdater; //bind ownership to GridDataRim<>!
};

//########################################################################################################

class GridDataBase : public GridData
{
public:
    GridDataBase(Grid& grid, const std::shared_ptr<FileView>& gridDataView) : grid_(grid), gridDataView_(gridDataView) {}

    void holdOwnership(const std::shared_ptr<GridEventManager>& evtMgr) { evtMgr_ = evtMgr; }
    GridEventManager* getEventManager() { return evtMgr_.get(); }

    FileView& getDataView() { return *gridDataView_; }

protected:
    Grid& refGrid() { return grid_; }
    const Grid& refGrid() const { return grid_; }

    const FileView* getGridDataView() const { return gridDataView_.get(); }

    const FileSystemObject* getRawData(size_t row) const
    {
        if (auto view = getGridDataView())
            return view->getObject(row);
        return nullptr;
    }

private:
    size_t getRowCount() const override
    {
        if (!gridDataView_ || gridDataView_->rowsTotal() == 0)
            return ROW_COUNT_IF_NO_DATA;

        return gridDataView_->rowsOnView();
        //return std::max(MIN_ROW_COUNT, gridDataView_ ? gridDataView_->rowsOnView() : 0);
    }

    std::shared_ptr<GridEventManager> evtMgr_;
    Grid& grid_;
    const std::shared_ptr<FileView> gridDataView_;
};

//########################################################################################################

template <SelectedSide side>
class GridDataRim : public GridDataBase
{
public:
    GridDataRim(const std::shared_ptr<FileView>& gridDataView, Grid& grid) : GridDataBase(grid, gridDataView) {}

    void setIconManager(const std::shared_ptr<IconManager>& iconMgr) { iconMgr_ = iconMgr; }

    void setItemPathForm(ItemPathFormat fmt) { itemPathFormat = fmt; }

    void getUnbufferedIconsForPreload(std::vector<std::pair<ptrdiff_t, AbstractPath>>& newLoad) //return (priority, filepath) list
    {
        if (iconMgr_)
        {
            const auto& rowsOnScreen = getVisibleRows(refGrid());
            const ptrdiff_t visibleRowCount = rowsOnScreen.second - rowsOnScreen.first;

            //preload icons not yet on screen:
            const int preloadSize = 2 * std::max<ptrdiff_t>(20, visibleRowCount); //:= sum of lines above and below of visible range to preload
            //=> use full visible height to handle "next page" command and a minimum of 20 for excessive mouse wheel scrolls

            for (ptrdiff_t i = 0; i < preloadSize; ++i)
            {
                const ptrdiff_t currentRow = rowsOnScreen.first - (preloadSize + 1) / 2 + getAlternatingPos(i, visibleRowCount + preloadSize); //for odd preloadSize start one row earlier

                const IconInfo ii = getIconInfo(currentRow);
                if (ii.type == IconInfo::ICON_PATH)
                    if (!iconMgr_->refIconBuffer().readyForRetrieval(ii.fsObj->template getAbstractPath<side>()))
                        newLoad.emplace_back(i, ii.fsObj->template getAbstractPath<side>()); //insert least-important items on outer rim first
            }
        }
    }

    void updateNewAndGetUnbufferedIcons(std::vector<AbstractPath>& newLoad) //loads all not yet drawn icons
    {
        if (iconMgr_)
        {
            const auto& rowsOnScreen = getVisibleRows(refGrid());
            const ptrdiff_t visibleRowCount = rowsOnScreen.second - rowsOnScreen.first;

            //loop over all visible rows
            for (ptrdiff_t i = 0; i < visibleRowCount; ++i)
            {
                //alternate when adding rows: first, last, first + 1, last - 1 ...
                const ptrdiff_t currentRow = rowsOnScreen.first + getAlternatingPos(i, visibleRowCount);

                if (isFailedLoad(currentRow)) //find failed attempts to load icon
                {
                    const IconInfo ii = getIconInfo(currentRow);
                    if (ii.type == IconInfo::ICON_PATH)
                    {
                        //test if they are already loaded in buffer:
                        if (iconMgr_->refIconBuffer().readyForRetrieval(ii.fsObj->template getAbstractPath<side>()))
                        {
                            //do a *full* refresh for *every* failed load to update partial DC updates while scrolling
                            refGrid().refreshCell(currentRow, static_cast<ColumnType>(ColumnTypeRim::ITEM_PATH));
                            setFailedLoad(currentRow, false);
                        }
                        else //not yet in buffer: mark for async. loading
                            newLoad.push_back(ii.fsObj->template getAbstractPath<side>());
                    }
                }
            }
        }
    }

private:
    bool isFailedLoad(size_t row) const { return row < failedLoads.size() ? failedLoads[row] != 0 : false; }

    void setFailedLoad(size_t row, bool failed = true)
    {
        if (failedLoads.size() != refGrid().getRowCount())
            failedLoads.resize(refGrid().getRowCount());

        if (row < failedLoads.size())
            failedLoads[row] = failed;
    }

    //icon buffer will load reversely, i.e. if we want to go from inside out, we need to start from outside in
    static size_t getAlternatingPos(size_t pos, size_t total)
    {
        assert(pos < total);
        return pos % 2 == 0 ? pos / 2 : total - 1 - pos / 2;
    }

protected:
    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected) override
    {
        if (enabled)
        {
            if (selected)
                dc.GradientFillLinear(rect, Grid::getColorSelectionGradientFrom(), Grid::getColorSelectionGradientTo(), wxEAST);
            //ignore focus
            else
            {
                //alternate background color to improve readability (while lacking cell borders)
                if (getRowDisplayType(row) == DisplayType::NORMAL)
                    fillBackgroundDefaultColorAlternating(dc, rect, row % 2 == 0);
                else
                    clearArea(dc, rect, getBackGroundColor(row));

                //draw horizontal border if required
                DisplayType dispTp = getRowDisplayType(row);
                if (dispTp != DisplayType::NORMAL &&
                    dispTp == getRowDisplayType(row + 1))
                {
                    wxDCPenChanger dummy2(dc, getColorGridLine());
                    dc.DrawLine(rect.GetBottomLeft(),  rect.GetBottomRight() + wxPoint(1, 0));
                }
            }
        }
        else
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    }

    wxColor getBackGroundColor(size_t row) const
    {
        //accessibility: always set both foreground AND background colors!
        // => harmonize with renderCell()!

        switch (getRowDisplayType(row))
        {
            case DisplayType::NORMAL:
                break;
            case DisplayType::FOLDER:
                return getColorGrey();
            case DisplayType::SYMLINK:
                return getColorOrange();
            case DisplayType::INACTIVE:
                return getColorNotActive();
        }
        return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    }

private:
    enum class DisplayType
    {
        NORMAL,
        FOLDER,
        SYMLINK,
        INACTIVE,
    };

    DisplayType getRowDisplayType(size_t row) const
    {
        const FileSystemObject* fsObj = getRawData(row);
        if (!fsObj )
            return DisplayType::NORMAL;

        //mark filtered rows
        if (!fsObj->isActive())
            return DisplayType::INACTIVE;

        if (fsObj->isEmpty<side>()) //always show not existing files/dirs/symlinks as empty
            return DisplayType::NORMAL;

        DisplayType output = DisplayType::NORMAL;
        //mark directories and symlinks
        visitFSObject(*fsObj, [&](const FolderPair& folder) { output = DisplayType::FOLDER; },
        [](const FilePair& file) {},
        [&](const SymlinkPair& symlink) { output = DisplayType::SYMLINK; });

        return output;
    }

    std::wstring getValue(size_t row, ColumnType colType) const override
    {
        if (const FileSystemObject* fsObj = getRawData(row))
        {
            const ColumnTypeRim colTypeRim = static_cast<ColumnTypeRim>(colType);

            std::wstring value;
            visitFSObject(*fsObj, [&](const FolderPair& folder)
            {
                value = [&]
                {
                    if (folder.isEmpty<side>())
                        return std::wstring();

                    switch (colTypeRim)
                    {
                        case ColumnTypeRim::ITEM_PATH:
                            switch (itemPathFormat)
                            {
                                case ItemPathFormat::FULL_PATH:
                                    return AFS::getDisplayPath(folder.getAbstractPath<side>());
                                case ItemPathFormat::RELATIVE_PATH:
                                    return utfTo<std::wstring>(folder.getRelativePath<side>());
                                case ItemPathFormat::ITEM_NAME:
                                    return utfTo<std::wstring>(folder.getItemName<side>());
                            }
                            break;
                        case ColumnTypeRim::SIZE:
                            return L"<" + _("Folder") + L">";
                        case ColumnTypeRim::DATE:
                            return std::wstring();
                        case ColumnTypeRim::EXTENSION:
                            return std::wstring();
                    }
                    assert(false);
                    return std::wstring();
                }();
            },

            [&](const FilePair& file)
            {
                value = [&]
                {
                    if (file.isEmpty<side>())
                        return std::wstring();

                    switch (colTypeRim)
                    {
                        case ColumnTypeRim::ITEM_PATH:
                            switch (itemPathFormat)
                            {
                                case ItemPathFormat::FULL_PATH:
                                    return AFS::getDisplayPath(file.getAbstractPath<side>());
                                case ItemPathFormat::RELATIVE_PATH:
                                    return utfTo<std::wstring>(file.getRelativePath<side>());
                                case ItemPathFormat::ITEM_NAME:
                                    return utfTo<std::wstring>(file.getItemName<side>());
                            }
                            break;
                        case ColumnTypeRim::SIZE:
                            //return utfTo<std::wstring>(file.getFileId<side>()); // -> test file id
                            return formatNumber(file.getFileSize<side>());
                        case ColumnTypeRim::DATE:
                            return formatUtcToLocalTime(file.getLastWriteTime<side>());
                        case ColumnTypeRim::EXTENSION:
                            return utfTo<std::wstring>(getFileExtension(file.getItemName<side>()));
                    }
                    assert(false);
                    return std::wstring();
                }();
            },

            [&](const SymlinkPair& symlink)
            {
                value = [&]
                {
                    if (symlink.isEmpty<side>())
                        return std::wstring();

                    switch (colTypeRim)
                    {
                        case ColumnTypeRim::ITEM_PATH:
                            switch (itemPathFormat)
                            {
                                case ItemPathFormat::FULL_PATH:
                                    return AFS::getDisplayPath(symlink.getAbstractPath<side>());
                                case ItemPathFormat::RELATIVE_PATH:
                                    return utfTo<std::wstring>(symlink.getRelativePath<side>());
                                case ItemPathFormat::ITEM_NAME:
                                    return utfTo<std::wstring>(symlink.getItemName<side>());
                            }
                            break;
                        case ColumnTypeRim::SIZE:
                            return L"<" + _("Symlink") + L">";
                        case ColumnTypeRim::DATE:
                            return formatUtcToLocalTime(symlink.getLastWriteTime<side>());
                        case ColumnTypeRim::EXTENSION:
                            return utfTo<std::wstring>(getFileExtension(symlink.getItemName<side>()));
                    }
                    assert(false);
                    return std::wstring();
                }();
            });
            return value;
        }
        //if data is not found:
        return std::wstring();
    }

    static const int GAP_SIZE = 2;

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover) override
    {
        //don't forget to harmonize with getBestSize()!!!

        const bool isActive = [&]
        {
            if (const FileSystemObject* fsObj = this->getRawData(row))
                return fsObj->isActive();
            return true;
        }();

        wxDCTextColourChanger dummy(dc);
        if (!isActive)
            dummy.Set(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
        else if (getRowDisplayType(row) != DisplayType::NORMAL)
            dummy.Set(*wxBLACK); //accessibility: always set both foreground AND background colors!

        wxRect rectTmp = rect;

        auto drawTextBlock = [&](const std::wstring& text)
        {
            rectTmp.x     += GAP_SIZE;
            rectTmp.width -= GAP_SIZE;
            const wxSize extent = drawCellText(dc, rectTmp, text, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
            rectTmp.x     += extent.GetWidth();
            rectTmp.width -= extent.GetWidth();
        };

        const std::wstring cellValue = getValue(row, colType);

        switch (static_cast<ColumnTypeRim>(colType))
        {
            case ColumnTypeRim::ITEM_PATH:
            {
                if (!iconMgr_)
                    drawTextBlock(cellValue);
                else
                {
                    auto it = cellValue.end();
                    while (it != cellValue.begin()) //reverse iteration: 1. check 2. decrement 3. evaluate
                    {
                        --it;
                        if (*it == '\\' || *it == '/')
                        {
                            ++it;
                            break;
                        }
                    }
                    const std::wstring pathPrefix(cellValue.begin(), it);
                    const std::wstring itemName(it, cellValue.end());

                    //  Partitioning:
                    //   __________________________________________________
                    //  | gap | path prefix | gap | icon | gap | item name |
                    //   --------------------------------------------------
                    if (!pathPrefix.empty())
                        drawTextBlock(pathPrefix);

                    //draw file icon
                    rectTmp.x     += GAP_SIZE;
                    rectTmp.width -= GAP_SIZE;

                    const int iconSize = iconMgr_->refIconBuffer().getSize();
                    if (rectTmp.GetWidth() >= iconSize)
                    {
                        //whenever there's something new to render on screen, start up watching for failed icon drawing:
                        //=> ideally it would suffice to start watching only when scrolling grid or showing new grid content, but this solution is more robust
                        //and the icon updater will stop automatically when finished anyway
                        //Note: it's not sufficient to start up on failed icon loads only, since we support prefetching of not yet visible rows!!!
                        iconMgr_->startIconUpdater();

                        const IconInfo ii = getIconInfo(row);

                        wxBitmap fileIcon;
                        switch (ii.type)
                        {
                            case IconInfo::FOLDER:
                                fileIcon = iconMgr_->getGenericDirIcon();
                                break;

                            case IconInfo::ICON_PATH:
                                if (Opt<wxBitmap> tmpIco = iconMgr_->refIconBuffer().retrieveFileIcon(ii.fsObj->template getAbstractPath<side>()))
                                    fileIcon = *tmpIco;
                                else
                                {
                                    setFailedLoad(row); //save status of failed icon load -> used for async. icon loading
                                    //falsify only! we want to avoid writing incorrect success values when only partially updating the DC, e.g. when scrolling,
                                    //see repaint behavior of ::ScrollWindow() function!
                                    fileIcon = iconMgr_->refIconBuffer().getIconByExtension(ii.fsObj->template getItemName<side>()); //better than nothing
                                }
                                break;

                            case IconInfo::EMPTY:
                                break;
                        }

                        if (fileIcon.IsOk())
                        {
                            wxRect rectIcon = rectTmp;
                            rectIcon.width = iconSize; //support small thumbnail centering

                            auto drawIcon = [&](const wxBitmap& icon)
                            {
                                if (isActive)
                                    drawBitmapRtlNoMirror(dc, icon, rectIcon, wxALIGN_CENTER);
                                else
                                    drawBitmapRtlNoMirror(dc, wxBitmap(icon.ConvertToImage().ConvertToGreyscale(1.0 / 3, 1.0 / 3, 1.0 / 3)), //treat all channels equally!
                                                          rectIcon, wxALIGN_CENTER);
                            };

                            drawIcon(fileIcon);

                            if (ii.drawAsLink)
                                drawIcon(iconMgr_->getLinkOverlayIcon());
                        }
                    }
                    rectTmp.x     += iconSize;
                    rectTmp.width -= iconSize;

                    drawTextBlock(itemName);
                }
            }
            break;

            case ColumnTypeRim::SIZE:
                if (refGrid().GetLayoutDirection() != wxLayout_RightToLeft)
                {
                    rectTmp.width -= GAP_SIZE; //have file size right-justified (but don't change for RTL languages)
                    drawCellText(dc, rectTmp, cellValue, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
                }
                else
                    drawTextBlock(cellValue);
                break;

            case ColumnTypeRim::DATE:
            case ColumnTypeRim::EXTENSION:
                drawTextBlock(cellValue);
                break;
        }
    }

    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        //  Partitioning:
        //   ________________________________________________________
        //  | gap | path prefix | gap | icon | gap | item name | gap |
        //   --------------------------------------------------------

        const std::wstring cellValue = getValue(row, colType);

        if (static_cast<ColumnTypeRim>(colType) == ColumnTypeRim::ITEM_PATH && iconMgr_)
        {
            auto it = cellValue.end();
            while (it != cellValue.begin()) //reverse iteration: 1. check 2. decrement 3. evaluate
            {
                --it;
                if (*it == '\\' || *it == '/')
                {
                    ++it;
                    break;
                }
            }
            const std::wstring pathPrefix(cellValue.begin(), it);
            const std::wstring itemName(it, cellValue.end());

            int bestSize = 0;
            if (!pathPrefix.empty())
                bestSize += GAP_SIZE + dc.GetTextExtent(pathPrefix).GetWidth();

            bestSize += GAP_SIZE + iconMgr_->refIconBuffer().getSize();
            bestSize += GAP_SIZE + dc.GetTextExtent(itemName).GetWidth() + GAP_SIZE;
            return bestSize;
        }
        else
            return GAP_SIZE + dc.GetTextExtent(cellValue).GetWidth() + GAP_SIZE;
        // + 1 pix for cell border line ? -> not used anymore!
    }

    std::wstring getColumnLabel(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeRim>(colType))
        {
            case ColumnTypeRim::ITEM_PATH:
                switch (itemPathFormat)
                {
                    case ItemPathFormat::FULL_PATH:
                        return _("Full path");
                    case ItemPathFormat::RELATIVE_PATH:
                        return _("Relative path");
                    case ItemPathFormat::ITEM_NAME:
                        return _("Item name");
                }
                assert(false);
                break;
            case ColumnTypeRim::SIZE:
                return _("Size");
            case ColumnTypeRim::DATE:
                return _("Date");
            case ColumnTypeRim::EXTENSION:
                return _("Extension");
        }
        //assert(false); may be ColumnType::NONE
        return std::wstring();
    }

    void renderColumnLabel(Grid& tree, wxDC& dc, const wxRect& rect, ColumnType colType, bool highlighted) override
    {
        wxRect rectInside = drawColumnLabelBorder(dc, rect);
        drawColumnLabelBackground(dc, rectInside, highlighted);

        rectInside.x     += COLUMN_GAP_LEFT;
        rectInside.width -= COLUMN_GAP_LEFT;
        drawColumnLabelText(dc, rectInside, getColumnLabel(colType));

        //draw sort marker
        if (getGridDataView())
        {
            auto sortInfo = getGridDataView()->getSortInfo();
            if (sortInfo)
            {
                if (colType == static_cast<ColumnType>(sortInfo->type) && (side == LEFT_SIDE) == sortInfo->onLeft)
                {
                    const wxBitmap& marker = getResourceImage(sortInfo->ascending ? L"sortAscending" : L"sortDescending");
                    drawBitmapRtlNoMirror(dc, marker, rectInside, wxALIGN_CENTER_HORIZONTAL);
                }
            }
        }
    }

    struct IconInfo
    {
        enum IconType
        {
            EMPTY,
            FOLDER,
            ICON_PATH,
        };
        IconType type = EMPTY;
        const FileSystemObject* fsObj = nullptr; //only set if type != EMPTY
        bool drawAsLink = false;
    };

    IconInfo getIconInfo(size_t row) const //return ICON_FILE_FOLDER if row points to a folder
    {
        IconInfo out;

        const FileSystemObject* fsObj = getRawData(row);
        if (fsObj && !fsObj->isEmpty<side>())
        {
            out.fsObj = fsObj;

            visitFSObject(*fsObj, [&](const FolderPair& folder)
            {
                out.type = IconInfo::FOLDER;
                out.drawAsLink = folder.isFollowedSymlink<side>();
            },

            [&](const FilePair& file)
            {
                out.type       = IconInfo::ICON_PATH;
                out.drawAsLink = file.isFollowedSymlink<side>() || hasLinkExtension(file.getItemName<side>());
            },

            [&](const SymlinkPair& symlink)
            {
                out.type       = IconInfo::ICON_PATH;
                out.drawAsLink = true;
            });
        }
        return out;
    }

    std::wstring getToolTip(size_t row, ColumnType colType) const override
    {
        std::wstring toolTip;

        if (const FileSystemObject* fsObj = getRawData(row))
            if (!fsObj->isEmpty<side>())
            {
                toolTip = getGridDataView() && getGridDataView()->getFolderPairCount() > 1 ?
                          AFS::getDisplayPath(fsObj->getAbstractPath<side>()) :
                          utfTo<std::wstring>(fsObj->getRelativePath<side>());

                visitFSObject(*fsObj, [](const FolderPair& folder) {},
                [&](const FilePair& file)
                {
                    toolTip += L"\n" +
                               _("Size:") + L" " + zen::formatFilesizeShort(file.getFileSize<side>()) + L"\n" +
                               _("Date:") + L" " + zen::formatUtcToLocalTime(file.getLastWriteTime<side>());
                },

                [&](const SymlinkPair& symlink)
                {
                    toolTip += L"\n" +
                               _("Date:") + L" " + zen::formatUtcToLocalTime(symlink.getLastWriteTime<side>());
                });
            }
        return toolTip;
    }

    std::shared_ptr<IconManager> iconMgr_; //optional
    ItemPathFormat itemPathFormat = ItemPathFormat::FULL_PATH;

    std::vector<char> failedLoads; //effectively a vector<bool> of size "number of rows"
    Opt<wxBitmap> renderBuf; //avoid costs of recreating this temporary variable
};


class GridDataLeft : public GridDataRim<LEFT_SIDE>
{
public:
    GridDataLeft(const std::shared_ptr<FileView>& gridDataView, Grid& grid) : GridDataRim<LEFT_SIDE>(gridDataView, grid) {}

    void setNavigationMarker(std::unordered_set<const FileSystemObject*>&& markedFilesAndLinks,
                             std::unordered_set<const ContainerObject*>&& markedContainer)
    {
        markedFilesAndLinks_.swap(markedFilesAndLinks);
        markedContainer_    .swap(markedContainer);
    }

private:
    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected) override
    {
        GridDataRim<LEFT_SIDE>::renderRowBackgound(dc, rect, row, enabled, selected);

        //mark rows selected on overview panel:
        if (enabled && !selected)
        {
            const bool markRow = [&]
            {
                if (const FileSystemObject* fsObj = getRawData(row))
                {
                    if (markedFilesAndLinks_.find(fsObj) != markedFilesAndLinks_.end()) //mark files/links directly
                        return true;

                    if (auto folder = dynamic_cast<const FolderPair*>(fsObj))
                    {
                        if (markedContainer_.find(folder) != markedContainer_.end()) //mark directories which *are* the given ContainerObject*
                            return true;
                    }

                    //mark all objects which have the ContainerObject as *any* matching ancestor
                    const ContainerObject* parent = &(fsObj->parent());
                    for (;;)
                    {
                        if (markedContainer_.find(parent) != markedContainer_.end())
                            return true;

                        if (auto folder = dynamic_cast<const FolderPair*>(parent))
                            parent = &(folder->parent());
                        else
                            break;
                    }
                }
                return false;
            }();

            if (markRow)
            {
                wxRect rectTmp = rect;
                rectTmp.width /= 20;
                dc.GradientFillLinear(rectTmp, Grid::getColorSelectionGradientFrom(), GridDataRim<LEFT_SIDE>::getBackGroundColor(row), wxEAST);
            }
        }
    }

    std::unordered_set<const FileSystemObject*> markedFilesAndLinks_; //mark files/symlinks directly within a container
    std::unordered_set<const ContainerObject*> markedContainer_;      //mark full container including all child-objects
    //DO NOT DEREFERENCE!!!! NOT GUARANTEED TO BE VALID!!!
};


class GridDataRight : public GridDataRim<RIGHT_SIDE>
{
public:
    GridDataRight(const std::shared_ptr<FileView>& gridDataView, Grid& grid) : GridDataRim<RIGHT_SIDE>(gridDataView, grid) {}
};

//########################################################################################################

class GridDataCenter : public GridDataBase
{
public:
    GridDataCenter(const std::shared_ptr<FileView>& gridDataView, Grid& grid) :
        GridDataBase(grid, gridDataView),
        toolTip_(grid) {} //tool tip must not live longer than grid!

    void onSelectBegin()
    {
        selectionInProgress_ = true;
        refGrid().clearSelection(DENY_GRID_EVENT); //don't emit event, prevent recursion!
        toolTip_.hide(); //handle custom tooltip
    }

    void onSelectEnd(size_t rowFirst, size_t rowLast, HoverArea rowHover, ptrdiff_t clickInitRow)
    {
        refGrid().clearSelection(DENY_GRID_EVENT); //don't emit event, prevent recursion!

        //issue custom event
        if (selectionInProgress_) //don't process selections initiated by right-click
            if (rowFirst < rowLast && rowLast <= refGrid().getRowCount()) //empty? probably not in this context
                if (wxEvtHandler* evtHandler = refGrid().GetEventHandler())
                    switch (static_cast<HoverAreaCenter>(rowHover))
                    {
                        case HoverAreaCenter::CHECK_BOX:
                            if (const FileSystemObject* fsObj = getRawData(clickInitRow))
                            {
                                const bool setIncluded = !fsObj->isActive();
                                CheckRowsEvent evt(rowFirst, rowLast, setIncluded);
                                evtHandler->ProcessEvent(evt);
                            }
                            break;
                        case HoverAreaCenter::DIR_LEFT:
                        {
                            SyncDirectionEvent evt(rowFirst, rowLast, SyncDirection::LEFT);
                            evtHandler->ProcessEvent(evt);
                        }
                        break;
                        case HoverAreaCenter::DIR_NONE:
                        {
                            SyncDirectionEvent evt(rowFirst, rowLast, SyncDirection::NONE);
                            evtHandler->ProcessEvent(evt);
                        }
                        break;
                        case HoverAreaCenter::DIR_RIGHT:
                        {
                            SyncDirectionEvent evt(rowFirst, rowLast, SyncDirection::RIGHT);
                            evtHandler->ProcessEvent(evt);
                        }
                        break;
                    }
        selectionInProgress_ = false;

        //update highlight_ and tooltip: on OS X no mouse movement event is generated after a mouse button click (unlike on Windows)
        wxPoint clientPos = refGrid().getMainWin().ScreenToClient(wxGetMousePosition());
        onMouseMovement(clientPos);
    }

    void onMouseMovement(const wxPoint& clientPos)
    {
        //manage block highlighting and custom tooltip
        if (!selectionInProgress_)
        {
            const wxPoint& topLeftAbs = refGrid().CalcUnscrolledPosition(clientPos);
            const size_t row = refGrid().getRowAtPos(topLeftAbs.y); //return -1 for invalid position, rowCount if one past the end
            const Grid::ColumnPosInfo cpi = refGrid().getColumnAtPos(topLeftAbs.x); //returns ColumnType::NONE if no column at x position!

            if (row < refGrid().getRowCount() && cpi.colType != ColumnType::NONE &&
                refGrid().getMainWin().GetClientRect().Contains(clientPos)) //cursor might have moved outside visible client area
                showToolTip(row, static_cast<ColumnTypeCenter>(cpi.colType), refGrid().getMainWin().ClientToScreen(clientPos));
            else
                toolTip_.hide();
        }
    }

    void onMouseLeave() //wxEVT_LEAVE_WINDOW does not respect mouse capture!
    {
        toolTip_.hide(); //handle custom tooltip
    }

    void highlightSyncAction(bool value) { highlightSyncAction_ = value; }

private:
    enum class HoverAreaCenter //each cell can be divided into four blocks concerning mouse selections
    {
        CHECK_BOX,
        DIR_LEFT,
        DIR_NONE,
        DIR_RIGHT
    };

    std::wstring getValue(size_t row, ColumnType colType) const override
    {
        if (const FileSystemObject* fsObj = getRawData(row))
            switch (static_cast<ColumnTypeCenter>(colType))
            {
                case ColumnTypeCenter::CHECKBOX:
                    break;
                case ColumnTypeCenter::CMP_CATEGORY:
                    return getSymbol(fsObj->getCategory());
                case ColumnTypeCenter::SYNC_ACTION:
                    return getSymbol(fsObj->getSyncOperation());
            }
        return std::wstring();
    }

    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected) override
    {
        if (enabled)
        {
            if (selected)
                dc.GradientFillLinear(rect, Grid::getColorSelectionGradientFrom(), Grid::getColorSelectionGradientTo(), wxEAST);
            else
            {
                if (const FileSystemObject* fsObj = getRawData(row))
                {
                    if (fsObj->isActive())
                        fillBackgroundDefaultColorAlternating(dc, rect, row % 2 == 0);
                    else
                        clearArea(dc, rect, getColorNotActive());
                }
                else
                    clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
            }
        }
        else
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    }

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover) override
    {
        auto drawHighlightBackground = [&](const FileSystemObject& fsObj, const wxColor& col)
        {
            if (enabled && !selected && fsObj.isActive()) //coordinate with renderRowBackgound()!
                clearArea(dc, rect, col);
        };

        switch (static_cast<ColumnTypeCenter>(colType))
        {
            case ColumnTypeCenter::CHECKBOX:
                if (const FileSystemObject* fsObj = getRawData(row))
                {
                    const bool drawMouseHover = static_cast<HoverAreaCenter>(rowHover) == HoverAreaCenter::CHECK_BOX;

                    if (fsObj->isActive())
                        drawBitmapRtlMirror(dc, getResourceImage(drawMouseHover ? L"checkbox_true_hover" : L"checkbox_true"), rect, wxALIGN_CENTER, renderBuf_);
                    else //default
                        drawBitmapRtlMirror(dc, getResourceImage(drawMouseHover ?  L"checkbox_false_hover" : L"checkbox_false"), rect, wxALIGN_CENTER, renderBuf_);
                }
                break;

            case ColumnTypeCenter::CMP_CATEGORY:
                if (const FileSystemObject* fsObj = getRawData(row))
                {
                    if (!highlightSyncAction_)
                        drawHighlightBackground(*fsObj, getBackGroundColorCmpCategory(fsObj));

                    wxRect rectTmp = rect;
                    {
                        //draw notch on left side
                        if (notch_.GetHeight() != rectTmp.GetHeight())
                            notch_.Rescale(notch_.GetWidth(), rectTmp.GetHeight());

                        //wxWidgets screws up again and has wxALIGN_RIGHT off by one pixel! -> use wxALIGN_LEFT instead
                        const wxRect rectNotch(rectTmp.x + rectTmp.width - notch_.GetWidth(), rectTmp.y, notch_.GetWidth(), rectTmp.height);
                        drawBitmapRtlMirror(dc, notch_, rectNotch, wxALIGN_LEFT, renderBuf_);
                        rectTmp.width -= notch_.GetWidth();
                    }

                    if (!highlightSyncAction_)
                        drawBitmapRtlMirror(dc, getCmpResultImage(fsObj->getCategory()), rectTmp, wxALIGN_CENTER, renderBuf_);
                    else if (fsObj->getCategory() != FILE_EQUAL) //don't show = in both middle columns
                        drawBitmapRtlMirror(dc, greyScale(getCmpResultImage(fsObj->getCategory())), rectTmp, wxALIGN_CENTER, renderBuf_);
                }
                break;

            case ColumnTypeCenter::SYNC_ACTION:
                if (const FileSystemObject* fsObj = getRawData(row))
                {
                    if (highlightSyncAction_)
                        drawHighlightBackground(*fsObj, getBackGroundColorSyncAction(fsObj));

                    //synchronization preview
                    const auto rowHoverCenter = rowHover == HoverArea::NONE ? HoverAreaCenter::CHECK_BOX : static_cast<HoverAreaCenter>(rowHover);
                    switch (rowHoverCenter)
                    {
                        case HoverAreaCenter::DIR_LEFT:
                            drawBitmapRtlMirror(dc, getSyncOpImage(fsObj->testSyncOperation(SyncDirection::LEFT)), rect, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, renderBuf_);
                            break;
                        case HoverAreaCenter::DIR_NONE:
                            drawBitmapRtlMirror(dc, getSyncOpImage(fsObj->testSyncOperation(SyncDirection::NONE)), rect, wxALIGN_CENTER, renderBuf_);
                            break;
                        case HoverAreaCenter::DIR_RIGHT:
                            drawBitmapRtlMirror(dc, getSyncOpImage(fsObj->testSyncOperation(SyncDirection::RIGHT)), rect, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, renderBuf_);
                            break;
                        case HoverAreaCenter::CHECK_BOX:
                            if (highlightSyncAction_)
                                drawBitmapRtlMirror(dc, getSyncOpImage(fsObj->getSyncOperation()), rect, wxALIGN_CENTER, renderBuf_);
                            else if (fsObj->getSyncOperation() != SO_EQUAL) //don't show = in both middle columns
                                drawBitmapRtlMirror(dc, greyScale(getSyncOpImage(fsObj->getSyncOperation())), rect, wxALIGN_CENTER, renderBuf_);
                            break;
                    }
                }
                break;
        }
    }

    HoverArea getRowMouseHover(size_t row, ColumnType colType, int cellRelativePosX, int cellWidth) override
    {
        if (const FileSystemObject* const fsObj = getRawData(row))
            switch (static_cast<ColumnTypeCenter>(colType))
            {
                case ColumnTypeCenter::CHECKBOX:
                case ColumnTypeCenter::CMP_CATEGORY:
                    return static_cast<HoverArea>(HoverAreaCenter::CHECK_BOX);

                case ColumnTypeCenter::SYNC_ACTION:
                    if (fsObj->getSyncOperation() == SO_EQUAL) //in sync-preview equal files shall be treated like a checkbox
                        return static_cast<HoverArea>(HoverAreaCenter::CHECK_BOX);
                    // cell:
                    //  -----------------------
                    // | left | middle | right|
                    //  -----------------------
                    if (0 <= cellRelativePosX)
                    {
                        if (cellRelativePosX < cellWidth / 3)
                            return static_cast<HoverArea>(HoverAreaCenter::DIR_LEFT);
                        else if (cellRelativePosX < 2 * cellWidth / 3)
                            return static_cast<HoverArea>(HoverAreaCenter::DIR_NONE);
                        else if  (cellRelativePosX < cellWidth)
                            return static_cast<HoverArea>(HoverAreaCenter::DIR_RIGHT);
                    }
                    break;
            }
        return HoverArea::NONE;
    }

    std::wstring getColumnLabel(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeCenter>(colType))
        {
            case ColumnTypeCenter::CHECKBOX:
                break;
            case ColumnTypeCenter::CMP_CATEGORY:
                return _("Category") + L" (F10)";
            case ColumnTypeCenter::SYNC_ACTION:
                return _("Action")   + L" (F10)";
        }
        return std::wstring();
    }

    std::wstring getToolTip(ColumnType colType) const override { return getColumnLabel(colType); }

    void renderColumnLabel(Grid& tree, wxDC& dc, const wxRect& rect, ColumnType colType, bool highlighted) override
    {
        switch (static_cast<ColumnTypeCenter>(colType))
        {
            case ColumnTypeCenter::CHECKBOX:
                drawColumnLabelBackground(dc, rect, false);
                break;

            case ColumnTypeCenter::CMP_CATEGORY:
            {
                wxRect rectInside = drawColumnLabelBorder(dc, rect);
                drawColumnLabelBackground(dc, rectInside, highlighted);

                const wxBitmap& cmpIcon = getResourceImage(L"compare_small");
                drawBitmapRtlNoMirror(dc, highlightSyncAction_ ? greyScale(cmpIcon) : cmpIcon, rectInside, wxALIGN_CENTER);
            }
            break;

            case ColumnTypeCenter::SYNC_ACTION:
            {
                wxRect rectInside = drawColumnLabelBorder(dc, rect);
                drawColumnLabelBackground(dc, rectInside, highlighted);

                const wxBitmap& syncIcon = getResourceImage(L"sync_small");
                drawBitmapRtlNoMirror(dc, highlightSyncAction_ ? syncIcon : greyScale(syncIcon), rectInside, wxALIGN_CENTER);
            }
            break;
        }
    }

    static wxColor getBackGroundColorSyncAction(const FileSystemObject* fsObj)
    {
        if (fsObj)
        {
            if (!fsObj->isActive())
                return getColorNotActive();

            switch (fsObj->getSyncOperation()) //evaluate comparison result and sync direction
            {
                case SO_DO_NOTHING:
                    return getColorNotActive();
                case SO_EQUAL:
                    break; //usually white

                case SO_CREATE_NEW_LEFT:
                case SO_OVERWRITE_LEFT:
                case SO_DELETE_LEFT:
                case SO_MOVE_LEFT_FROM:
                case SO_MOVE_LEFT_TO:
                case SO_COPY_METADATA_TO_LEFT:
                    return getColorSyncBlue();

                case SO_CREATE_NEW_RIGHT:
                case SO_OVERWRITE_RIGHT:
                case SO_DELETE_RIGHT:
                case SO_MOVE_RIGHT_FROM:
                case SO_MOVE_RIGHT_TO:
                case SO_COPY_METADATA_TO_RIGHT:
                    return getColorSyncGreen();

                case SO_UNRESOLVED_CONFLICT:
                    return getColorYellow();
            }
        }
        return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    }

    static wxColor getBackGroundColorCmpCategory(const FileSystemObject* fsObj)
    {
        if (fsObj)
        {
            if (!fsObj->isActive())
                return getColorNotActive();

            switch (fsObj->getCategory())
            {
                case FILE_LEFT_SIDE_ONLY:
                case FILE_LEFT_NEWER:
                    return getColorSyncBlue(); //COLOR_CMP_BLUE;

                case FILE_RIGHT_SIDE_ONLY:
                case FILE_RIGHT_NEWER:
                    return getColorSyncGreen(); //COLOR_CMP_GREEN;

                case FILE_DIFFERENT_CONTENT:
                    return getColorCmpRed();
                case FILE_EQUAL:
                    break; //usually white
                case FILE_CONFLICT:
                case FILE_DIFFERENT_METADATA: //= sub-category of equal, but hint via background that sync direction follows conflict-setting
                    return getColorYellow();
                    //return getColorYellowLight();
            }
        }
        return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    }

    void showToolTip(size_t row, ColumnTypeCenter colType, wxPoint posScreen)
    {
        if (const FileSystemObject* fsObj = getRawData(row))
        {
            switch (colType)
            {
                case ColumnTypeCenter::CHECKBOX:
                case ColumnTypeCenter::CMP_CATEGORY:
                {
                    const wchar_t* imageName = [&]
                    {
                        const CompareFilesResult cmpRes = fsObj->getCategory();
                        switch (cmpRes)
                        {
                            case FILE_LEFT_SIDE_ONLY:
                                return L"cat_left_only";
                            case FILE_RIGHT_SIDE_ONLY:
                                return L"cat_right_only";
                            case FILE_LEFT_NEWER:
                                return L"cat_left_newer";
                            case FILE_RIGHT_NEWER:
                                return L"cat_right_newer";
                            case FILE_DIFFERENT_CONTENT:
                                return L"cat_different";
                            case FILE_EQUAL:
                            case FILE_DIFFERENT_METADATA: //= sub-category of equal
                                return L"cat_equal";
                            case FILE_CONFLICT:
                                return L"cat_conflict";
                        }
                        assert(false);
                        return L"";
                    }();
                    const auto& img = mirrorIfRtl(getResourceImage(imageName));
                    toolTip_.show(getCategoryDescription(*fsObj), posScreen, &img);
                }
                break;

                case ColumnTypeCenter::SYNC_ACTION:
                {
                    const wchar_t* imageName = [&]
                    {
                        const SyncOperation syncOp = fsObj->getSyncOperation();
                        switch (syncOp)
                        {
                            case SO_CREATE_NEW_LEFT:
                                return L"so_create_left";
                            case SO_CREATE_NEW_RIGHT:
                                return L"so_create_right";
                            case SO_DELETE_LEFT:
                                return L"so_delete_left";
                            case SO_DELETE_RIGHT:
                                return L"so_delete_right";
                            case SO_MOVE_LEFT_FROM:
                                return L"so_move_left_source";
                            case SO_MOVE_LEFT_TO:
                                return L"so_move_left_target";
                            case SO_MOVE_RIGHT_FROM:
                                return L"so_move_right_source";
                            case SO_MOVE_RIGHT_TO:
                                return L"so_move_right_target";
                            case SO_OVERWRITE_LEFT:
                                return L"so_update_left";
                            case SO_OVERWRITE_RIGHT:
                                return L"so_update_right";
                            case SO_COPY_METADATA_TO_LEFT:
                                return L"so_move_left";
                            case SO_COPY_METADATA_TO_RIGHT:
                                return L"so_move_right";
                            case SO_DO_NOTHING:
                                return L"so_none";
                            case SO_EQUAL:
                                return L"cat_equal";
                            case SO_UNRESOLVED_CONFLICT:
                                return L"cat_conflict";
                        };
                        assert(false);
                        return L"";
                    }();
                    const auto& img = mirrorIfRtl(getResourceImage(imageName));
                    toolTip_.show(getSyncOpDescription(*fsObj), posScreen, &img);
                }
                break;
            }
        }
        else
            toolTip_.hide(); //if invalid row...
    }

    bool highlightSyncAction_ = false;
    bool selectionInProgress_ = false;

    Opt<wxBitmap> renderBuf_; //avoid costs of recreating this temporary variable
    Tooltip toolTip_;
    wxImage notch_ = getResourceImage(L"notch").ConvertToImage();
};

//########################################################################################################

const wxEventType EVENT_ALIGN_SCROLLBARS = wxNewEventType();

class GridEventManager : private wxEvtHandler
{
public:
    GridEventManager(Grid& gridL,
                     Grid& gridC,
                     Grid& gridR,
                     GridDataCenter& provCenter) :
        gridL_(gridL), gridC_(gridC), gridR_(gridR),
        provCenter_(provCenter)
    {
        gridL_.Connect(EVENT_GRID_COL_RESIZE, GridColumnResizeEventHandler(GridEventManager::onResizeColumnL), nullptr, this);
        gridR_.Connect(EVENT_GRID_COL_RESIZE, GridColumnResizeEventHandler(GridEventManager::onResizeColumnR), nullptr, this);

        gridL_.getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(GridEventManager::onKeyDownL), nullptr, this);
        gridC_.getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(GridEventManager::onKeyDownC), nullptr, this);
        gridR_.getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(GridEventManager::onKeyDownR), nullptr, this);

        gridC_.getMainWin().Connect(wxEVT_MOTION,       wxMouseEventHandler(GridEventManager::onCenterMouseMovement), nullptr, this);
        gridC_.getMainWin().Connect(wxEVT_LEAVE_WINDOW, wxMouseEventHandler(GridEventManager::onCenterMouseLeave   ), nullptr, this);

        gridC_.Connect(EVENT_GRID_MOUSE_LEFT_DOWN, GridClickEventHandler (GridEventManager::onCenterSelectBegin), nullptr, this);
        gridC_.Connect(EVENT_GRID_SELECT_RANGE,    GridSelectEventHandler(GridEventManager::onCenterSelectEnd  ), nullptr, this);

        //clear selection of other grid when selecting on
        gridL_.Connect(EVENT_GRID_SELECT_RANGE, GridSelectEventHandler(GridEventManager::onGridSelectionL), nullptr, this);
        gridR_.Connect(EVENT_GRID_SELECT_RANGE, GridSelectEventHandler(GridEventManager::onGridSelectionR), nullptr, this);

        //parallel grid scrolling: do NOT use DoPrepareDC() to align grids! GDI resource leak! Use regular paint event instead:
        gridL_.getMainWin().Connect(wxEVT_PAINT, wxEventHandler(GridEventManager::onPaintGridL), nullptr, this);
        gridC_.getMainWin().Connect(wxEVT_PAINT, wxEventHandler(GridEventManager::onPaintGridC), nullptr, this);
        gridR_.getMainWin().Connect(wxEVT_PAINT, wxEventHandler(GridEventManager::onPaintGridR), nullptr, this);

        auto connectGridAccess = [&](Grid& grid, wxObjectEventFunction func)
        {
            grid.Connect(wxEVT_SCROLLWIN_TOP,        func, nullptr, this);
            grid.Connect(wxEVT_SCROLLWIN_BOTTOM,     func, nullptr, this);
            grid.Connect(wxEVT_SCROLLWIN_LINEUP,     func, nullptr, this);
            grid.Connect(wxEVT_SCROLLWIN_LINEDOWN,   func, nullptr, this);
            grid.Connect(wxEVT_SCROLLWIN_PAGEUP,     func, nullptr, this);
            grid.Connect(wxEVT_SCROLLWIN_PAGEDOWN,   func, nullptr, this);
            grid.Connect(wxEVT_SCROLLWIN_THUMBTRACK, func, nullptr, this);
            //wxEVT_KILL_FOCUS -> there's no need to reset "scrollMaster"
            //wxEVT_SET_FOCUS -> not good enough:
            //e.g.: left grid has input, right grid is "scrollMaster" due to dragging scroll thumb via mouse.
            //=> Next keyboard input on left does *not* emit focus change event, but still "scrollMaster" needs to change
            //=> hook keyboard input instead of focus event:
            grid.getMainWin().Connect(wxEVT_CHAR,     func, nullptr, this);
            grid.getMainWin().Connect(wxEVT_KEY_UP,   func, nullptr, this);
            grid.getMainWin().Connect(wxEVT_KEY_DOWN, func, nullptr, this);

            grid.getMainWin().Connect(wxEVT_LEFT_DOWN,   func, nullptr, this);
            grid.getMainWin().Connect(wxEVT_LEFT_DCLICK, func, nullptr, this);
            grid.getMainWin().Connect(wxEVT_RIGHT_DOWN,  func, nullptr, this);
            //grid.getMainWin().Connect(wxEVT_MOUSEWHEEL, func, nullptr, this); -> should be covered by wxEVT_SCROLLWIN_*
        };
        connectGridAccess(gridL_, wxEventHandler(GridEventManager::onGridAccessL)); //
        connectGridAccess(gridC_, wxEventHandler(GridEventManager::onGridAccessC)); //connect *after* onKeyDown() in order to receive callback *before*!!!
        connectGridAccess(gridR_, wxEventHandler(GridEventManager::onGridAccessR)); //

        Connect(EVENT_ALIGN_SCROLLBARS, wxEventHandler(GridEventManager::onAlignScrollBars), NULL, this);
    }

    ~GridEventManager() { assert(!scrollbarUpdatePending_); }

    void setScrollMaster(const Grid& grid) { scrollMaster_ = &grid; }

private:
    void onCenterSelectBegin(GridClickEvent& event)
    {
        provCenter_.onSelectBegin();
        event.Skip();
    }

    void onCenterSelectEnd(GridSelectEvent& event)
    {
        if (event.positive_)
        {
            if (event.mouseSelect_)
                provCenter_.onSelectEnd(event.rowFirst_, event.rowLast_, event.mouseSelect_->click.hoverArea_, event.mouseSelect_->click.row_);
            else
                provCenter_.onSelectEnd(event.rowFirst_, event.rowLast_, HoverArea::NONE, -1);
        }
        event.Skip();
    }

    void onCenterMouseMovement(wxMouseEvent& event)
    {
        provCenter_.onMouseMovement(event.GetPosition());
        event.Skip();
    }

    void onCenterMouseLeave(wxMouseEvent& event)
    {
        provCenter_.onMouseLeave();
        event.Skip();
    }

    void onGridSelectionL(GridSelectEvent& event) { onGridSelection(gridL_, gridR_); event.Skip(); }
    void onGridSelectionR(GridSelectEvent& event) { onGridSelection(gridR_, gridL_); event.Skip(); }

    void onGridSelection(const Grid& grid, Grid& other)
    {
        if (!wxGetKeyState(WXK_CONTROL)) //clear other grid unless user is holding CTRL
            other.clearSelection(DENY_GRID_EVENT); //don't emit event, prevent recursion!
    }

    void onKeyDownL(wxKeyEvent& event) {  onKeyDown(event, gridL_); }
    void onKeyDownC(wxKeyEvent& event) {  onKeyDown(event, gridC_); }
    void onKeyDownR(wxKeyEvent& event) {  onKeyDown(event, gridR_); }

    void onKeyDown(wxKeyEvent& event, const Grid& grid)
    {
        int keyCode = event.GetKeyCode();
        if (wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft)
        {
            if (keyCode == WXK_LEFT || keyCode == WXK_NUMPAD_LEFT)
                keyCode = WXK_RIGHT;
            else if (keyCode == WXK_RIGHT || keyCode == WXK_NUMPAD_RIGHT)
                keyCode = WXK_LEFT;
        }

        //skip middle component when navigating via keyboard
        const size_t row = grid.getGridCursor();

        if (event.ShiftDown())
            ;
        else if (event.ControlDown())
            ;
        else
            switch (keyCode)
            {
                case WXK_LEFT:
                case WXK_NUMPAD_LEFT:
                    gridL_.setGridCursor(row);
                    gridL_.SetFocus();
                    //since key event is likely originating from right grid, we need to set scrollMaster manually!
                    scrollMaster_ = &gridL_; //onKeyDown is called *after* onGridAccessL()!
                    return; //swallow event

                case WXK_RIGHT:
                case WXK_NUMPAD_RIGHT:
                    gridR_.setGridCursor(row);
                    gridR_.SetFocus();
                    scrollMaster_ = &gridR_;
                    return; //swallow event
            }

        event.Skip();
    }

    void onResizeColumnL(GridColumnResizeEvent& event) { resizeOtherSide(gridL_, gridR_, event.colType_, event.offset_); }
    void onResizeColumnR(GridColumnResizeEvent& event) { resizeOtherSide(gridR_, gridL_, event.colType_, event.offset_); }

    void resizeOtherSide(const Grid& src, Grid& trg, ColumnType type, int offset)
    {
        //find stretch factor of resized column: type is unique due to makeConsistent()!
        std::vector<Grid::ColAttributes> cfgSrc = src.getColumnConfig();
        auto it = std::find_if(cfgSrc.begin(), cfgSrc.end(), [&](Grid::ColAttributes& ca) { return ca.type == type; });
        if (it == cfgSrc.end())
            return;
        const int stretchSrc = it->stretch;

        //we do not propagate resizings on stretched columns to the other side: awkward user experience
        if (stretchSrc > 0)
            return;

        //apply resized offset to other side, but only if stretch factors match!
        std::vector<Grid::ColAttributes> cfgTrg = trg.getColumnConfig();
        for (Grid::ColAttributes& ca : cfgTrg)
            if (ca.type == type && ca.stretch == stretchSrc)
                ca.offset = offset;
        trg.setColumnConfig(cfgTrg);
    }

    void onGridAccessL(wxEvent& event) { scrollMaster_ = &gridL_; event.Skip(); }
    void onGridAccessC(wxEvent& event) { scrollMaster_ = &gridC_; event.Skip(); }
    void onGridAccessR(wxEvent& event) { scrollMaster_ = &gridR_; event.Skip(); }

    void onPaintGridL(wxEvent& event) { onPaintGrid(gridL_); event.Skip(); }
    void onPaintGridC(wxEvent& event) { onPaintGrid(gridC_); event.Skip(); }
    void onPaintGridR(wxEvent& event) { onPaintGrid(gridR_); event.Skip(); }

    void onPaintGrid(const Grid& grid)
    {
        //align scroll positions of all three grids *synchronously* during paint event! (wxGTK has visible delay when this is done asynchronously, no delay on Windows)

        //determine lead grid
        const Grid* lead = nullptr;
        Grid* follow1    = nullptr;
        Grid* follow2    = nullptr;
        auto setGrids = [&](const Grid& l, Grid& f1, Grid& f2) { lead = &l; follow1 = &f1; follow2 = &f2; };

        if (&gridC_ == scrollMaster_)
            setGrids(gridC_, gridL_, gridR_);
        else if (&gridR_ == scrollMaster_)
            setGrids(gridR_, gridL_, gridC_);
        else //default: left panel
            setGrids(gridL_, gridC_, gridR_);

        //align other grids only while repainting the lead grid to avoid scrolling and updating a grid at the same time!
        if (lead != &grid) return;

        auto scroll = [](Grid& target, int y) //support polling
        {
            //scroll vertically only - scrolling horizontally becomes annoying if left and right sides have different widths;
            //e.g. h-scroll on left would be undone when scrolling vertically on right which doesn't have a h-scrollbar
            int yOld = 0;
            target.GetViewStart(nullptr, &yOld);
            if (yOld != y)
                target.Scroll(-1, y); //empirical test Windows/Ubuntu: this call does NOT trigger a wxEVT_SCROLLWIN event, which would incorrectly set "scrollMaster" to "&target"!
        };
        int y = 0;
        lead->GetViewStart(nullptr, &y);
        scroll(*follow1, y);
        scroll(*follow2, y);

        //harmonize placement of horizontal scrollbar to avoid grids getting out of sync!
        //since this affects the grid that is currently repainted as well, we do work asynchronously!
        //avoids at least this problem: remaining graphics artifact when changing from Grid::SB_SHOW_ALWAYS to Grid::SB_SHOW_NEVER at location of old scrollbar (Windows only)

        //perf note: send one async event at most, else they may accumulate and create perf issues, see grid.cpp
        if (!scrollbarUpdatePending_)
        {
            scrollbarUpdatePending_ = true;
            wxCommandEvent alignEvent(EVENT_ALIGN_SCROLLBARS);
            AddPendingEvent(alignEvent); //waits until next idle event - may take up to a second if the app is busy on wxGTK!
        }
    }

    void onAlignScrollBars(wxEvent& event)
    {
        ZEN_ON_SCOPE_EXIT(scrollbarUpdatePending_ = false);
        assert(scrollbarUpdatePending_);

        auto needsHorizontalScrollbars = [](const Grid& grid) -> bool
        {
            const wxWindow& mainWin = grid.getMainWin();
            return mainWin.GetVirtualSize().GetWidth() > mainWin.GetClientSize().GetWidth();
            //assuming Grid::updateWindowSizes() does its job well, this should suffice!
            //CAVEAT: if horizontal and vertical scrollbar are circular dependent from each other
            //(h-scrollbar is shown due to v-scrollbar consuming horizontal width, ect...)
            //while in fact both are NOT needed, this special case results in a bogus need for scrollbars!
            //see https://sourceforge.net/tracker/?func=detail&aid=3514183&group_id=234430&atid=1093083
            // => since we're outside the Grid abstraction, we should not duplicate code to handle this special case as it seems to be insignificant
        };

        Grid::ScrollBarStatus sbStatusX = needsHorizontalScrollbars(gridL_) ||
                                          needsHorizontalScrollbars(gridR_) ?
                                          Grid::SB_SHOW_ALWAYS : Grid::SB_SHOW_NEVER;
        gridL_.showScrollBars(sbStatusX, Grid::SB_SHOW_NEVER);
        gridC_.showScrollBars(sbStatusX, Grid::SB_SHOW_NEVER);
        gridR_.showScrollBars(sbStatusX, Grid::SB_SHOW_AUTOMATIC);
    }

    Grid& gridL_;
    Grid& gridC_;
    Grid& gridR_;

    const Grid* scrollMaster_ = nullptr; //for address check only; this needn't be the grid having focus!
    //e.g. mouse wheel events should set window under cursor as scrollMaster, but *not* change focus

    GridDataCenter& provCenter_;
    bool scrollbarUpdatePending_ = false;
};
}

//########################################################################################################

void filegrid::init(Grid& gridLeft, Grid& gridCenter, Grid& gridRight)
{
    const auto gridDataView = std::make_shared<FileView>();

    auto provLeft_   = std::make_shared<GridDataLeft  >(gridDataView, gridLeft);
    auto provCenter_ = std::make_shared<GridDataCenter>(gridDataView, gridCenter);
    auto provRight_  = std::make_shared<GridDataRight >(gridDataView, gridRight);

    gridLeft  .setDataProvider(provLeft_);   //data providers reference grid =>
    gridCenter.setDataProvider(provCenter_); //ownership must belong *exclusively* to grid!
    gridRight .setDataProvider(provRight_);

    auto evtMgr = std::make_shared<GridEventManager>(gridLeft, gridCenter, gridRight, *provCenter_);
    provLeft_  ->holdOwnership(evtMgr);
    provCenter_->holdOwnership(evtMgr);
    provRight_ ->holdOwnership(evtMgr);

    gridCenter.enableColumnMove  (false);
    gridCenter.enableColumnResize(false);

    gridCenter.showRowLabel(false);
    gridRight .showRowLabel(false);

    //gridLeft  .showScrollBars(Grid::SB_SHOW_AUTOMATIC, Grid::SB_SHOW_NEVER); -> redundant: configuration happens in GridEventManager::onAlignScrollBars()
    //gridCenter.showScrollBars(Grid::SB_SHOW_NEVER,     Grid::SB_SHOW_NEVER);

    const int widthCheckbox = getResourceImage(L"checkbox_true").GetWidth() + 4 + getResourceImage(L"notch").GetWidth();
    const int widthCategory = 30;
    const int widthAction   = 45;
    gridCenter.SetSize(widthCategory + widthCheckbox + widthAction, -1);

    gridCenter.setColumnConfig(
    {
        { static_cast<ColumnType>(ColumnTypeCenter::CHECKBOX    ), widthCheckbox, 0, true },
        { static_cast<ColumnType>(ColumnTypeCenter::CMP_CATEGORY), widthCategory, 0, true },
        { static_cast<ColumnType>(ColumnTypeCenter::SYNC_ACTION ), widthAction,   0, true },
    });
}


FileView& filegrid::getDataView(Grid& grid)
{
    if (auto* prov = dynamic_cast<GridDataBase*>(grid.getDataProvider()))
        return prov->getDataView();

    throw std::runtime_error("filegrid was not initialized! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
}


namespace
{
class IconUpdater : private wxEvtHandler //update file icons periodically: use SINGLE instance to coordinate left and right grids in parallel
{
public:
    IconUpdater(GridDataLeft& provLeft, GridDataRight& provRight, IconBuffer& iconBuffer) : provLeft_(provLeft), provRight_(provRight), iconBuffer_(iconBuffer)
    {
        timer_.Connect(wxEVT_TIMER, wxEventHandler(IconUpdater::loadIconsAsynchronously), nullptr, this);
    }

    void start() { if (!timer_.IsRunning()) timer_.Start(100); } //timer interval in [ms]
    //don't check too often! give worker thread some time to fetch data

private:
    void stop() { if (timer_.IsRunning()) timer_.Stop(); }

    void loadIconsAsynchronously(wxEvent& event) //loads all (not yet) drawn icons
    {
        std::vector<std::pair<ptrdiff_t, AbstractPath>> prefetchLoad;
        provLeft_ .getUnbufferedIconsForPreload(prefetchLoad);
        provRight_.getUnbufferedIconsForPreload(prefetchLoad);

        //make sure least-important prefetch rows are inserted first into workload (=> processed last)
        //priority index nicely considers both grids at the same time!
        std::sort(prefetchLoad.begin(), prefetchLoad.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        //last inserted items are processed first in icon buffer:
        std::vector<AbstractPath> newLoad;
        for (const auto& item : prefetchLoad)
            newLoad.push_back(item.second);

        provRight_.updateNewAndGetUnbufferedIcons(newLoad);
        provLeft_ .updateNewAndGetUnbufferedIcons(newLoad);

        iconBuffer_.setWorkload(newLoad);

        if (newLoad.empty()) //let's only pay for IconUpdater when needed
            stop();
    }

    GridDataLeft&  provLeft_;
    GridDataRight& provRight_;
    IconBuffer& iconBuffer_;
    wxTimer timer_;
};


//resolve circular linker dependencies
inline
void IconManager::startIconUpdater() { if (iconUpdater) iconUpdater->start(); }
}


void filegrid::setupIcons(Grid& gridLeft, Grid& gridCenter, Grid& gridRight, bool show, IconBuffer::IconSize sz)
{
    auto* provLeft  = dynamic_cast<GridDataLeft*>(gridLeft .getDataProvider());
    auto* provRight = dynamic_cast<GridDataRight*>(gridRight.getDataProvider());

    if (provLeft && provRight)
    {
        int iconHeight = 0;
        if (show)
        {
            auto iconMgr = std::make_shared<IconManager>(*provLeft, *provRight, sz);
            provLeft ->setIconManager(iconMgr);
            provRight->setIconManager(iconMgr);
            iconHeight = iconMgr->refIconBuffer().getSize();
        }
        else
        {
            provLeft ->setIconManager(nullptr);
            provRight->setIconManager(nullptr);
            iconHeight = IconBuffer::getSize(IconBuffer::SIZE_SMALL);
        }

        const int newRowHeight = std::max(iconHeight, gridLeft.getMainWin().GetCharHeight()) + 1; //add some space

        gridLeft  .setRowHeight(newRowHeight);
        gridCenter.setRowHeight(newRowHeight);
        gridRight .setRowHeight(newRowHeight);
    }
    else
        assert(false);
}


void filegrid::setItemPathForm(Grid& grid, ItemPathFormat fmt)
{
    if (auto* provLeft  = dynamic_cast<GridDataLeft*>(grid.getDataProvider()))
        provLeft->setItemPathForm(fmt);
    else if (auto* provRight = dynamic_cast<GridDataRight*>(grid.getDataProvider()))
        provRight->setItemPathForm(fmt);
    else
        assert(false);
    grid.Refresh();
}


void filegrid::refresh(Grid& gridLeft, Grid& gridCenter, Grid& gridRight)
{
    gridLeft  .Refresh();
    gridCenter.Refresh();
    gridRight .Refresh();
}


void filegrid::setScrollMaster(Grid& grid)
{
    if (auto prov = dynamic_cast<GridDataBase*>(grid.getDataProvider()))
        if (auto evtMgr = prov->getEventManager())
        {
            evtMgr->setScrollMaster(grid);
            return;
        }
    assert(false);
}


void filegrid::setNavigationMarker(Grid& gridLeft,
                                   std::unordered_set<const FileSystemObject*>&& markedFilesAndLinks,
                                   std::unordered_set<const ContainerObject*>&& markedContainer)
{
    if (auto provLeft = dynamic_cast<GridDataLeft*>(gridLeft.getDataProvider()))
        provLeft->setNavigationMarker(std::move(markedFilesAndLinks), std::move(markedContainer));
    else
        assert(false);
    gridLeft.Refresh();
}


void filegrid::highlightSyncAction(Grid& gridCenter, bool value)
{
    if (auto provCenter = dynamic_cast<GridDataCenter*>(gridCenter.getDataProvider()))
        provCenter->highlightSyncAction(value);
    else
        assert(false);
    gridCenter.Refresh();
}


wxBitmap fff::getSyncOpImage(SyncOperation syncOp)
{
    switch (syncOp) //evaluate comparison result and sync direction
    {
        case SO_CREATE_NEW_LEFT:
            return getResourceImage(L"so_create_left_small");
        case SO_CREATE_NEW_RIGHT:
            return getResourceImage(L"so_create_right_small");
        case SO_DELETE_LEFT:
            return getResourceImage(L"so_delete_left_small");
        case SO_DELETE_RIGHT:
            return getResourceImage(L"so_delete_right_small");
        case SO_MOVE_LEFT_FROM:
            return getResourceImage(L"so_move_left_source_small");
        case SO_MOVE_LEFT_TO:
            return getResourceImage(L"so_move_left_target_small");
        case SO_MOVE_RIGHT_FROM:
            return getResourceImage(L"so_move_right_source_small");
        case SO_MOVE_RIGHT_TO:
            return getResourceImage(L"so_move_right_target_small");
        case SO_OVERWRITE_LEFT:
            return getResourceImage(L"so_update_left_small");
        case SO_OVERWRITE_RIGHT:
            return getResourceImage(L"so_update_right_small");
        case SO_COPY_METADATA_TO_LEFT:
            return getResourceImage(L"so_move_left_small");
        case SO_COPY_METADATA_TO_RIGHT:
            return getResourceImage(L"so_move_right_small");
        case SO_DO_NOTHING:
            return getResourceImage(L"so_none_small");
        case SO_EQUAL:
            return getResourceImage(L"cat_equal_small");
        case SO_UNRESOLVED_CONFLICT:
            return getResourceImage(L"cat_conflict_small");
    }
    assert(false);
    return wxNullBitmap;
}


wxBitmap fff::getCmpResultImage(CompareFilesResult cmpResult)
{
    switch (cmpResult)
    {
        case FILE_LEFT_SIDE_ONLY:
            return getResourceImage(L"cat_left_only_small");
        case FILE_RIGHT_SIDE_ONLY:
            return getResourceImage(L"cat_right_only_small");
        case FILE_LEFT_NEWER:
            return getResourceImage(L"cat_left_newer_small");
        case FILE_RIGHT_NEWER:
            return getResourceImage(L"cat_right_newer_small");
        case FILE_DIFFERENT_CONTENT:
            return getResourceImage(L"cat_different_small");
        case FILE_EQUAL:
        case FILE_DIFFERENT_METADATA: //= sub-category of equal
            return getResourceImage(L"cat_equal_small");
        case FILE_CONFLICT:
            return getResourceImage(L"cat_conflict_small");
    }
    assert(false);
    return wxNullBitmap;
}

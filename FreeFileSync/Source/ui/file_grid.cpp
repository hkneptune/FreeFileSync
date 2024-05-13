// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "file_grid.h"
//#include <set>
#include <wx/dc.h>
#include <wx/settings.h>
#include <wx/timer.h>
#include <zen/i18n.h>
//#include <zen/file_error.h>
//#include <zen/format_unit.h>
#include <zen/scope_guard.h>
#include <wx+/tooltip.h>
#include <wx+/rtl.h>
#include <wx+/dc.h>
#include <wx+/image_tools.h>
#include <wx+/image_resources.h>
#include <wx+/std_button_layout.h>
#include "../base/file_hierarchy.h"

using namespace zen;
using namespace fff;


namespace fff
{
wxDEFINE_EVENT(EVENT_GRID_CHECK_ROWS,     CheckRowsEvent);
wxDEFINE_EVENT(EVENT_GRID_SYNC_DIRECTION, SyncDirectionEvent);
}


namespace
{
//let's NOT create wxWidgets objects statically:
inline wxColor getColorSyncBlue (bool faint) { if (faint) return {0xed, 0xee, 0xff}; return {185, 188, 255}; }
inline wxColor getColorSyncGreen(bool faint) { if (faint) return {0xf1, 0xff, 0xed}; return {196, 255, 185}; }

inline wxColor getColorConflictBackground (bool faint) { if (faint) return {0xfe, 0xfe, 0xda}; return {247, 252,  62}; } //yellow
inline wxColor getColorDifferentBackground(bool faint) { if (faint) return {0xff, 0xed, 0xee}; return {255, 185, 187}; } //red

inline wxColor getColorSymlinkBackground() { return {238, 201, 0}; } //orange
//inline wxColor getColorItemMissing() { return {212, 208, 200}; } //medium grey

inline wxColor getColorInactiveBack(bool faint) { if (faint) return {0xf6, 0xf6, 0xf6}; return {0xe4, 0xe4, 0xe4}; } //light grey
inline wxColor getColorInactiveText() { return {0x40, 0x40, 0x40}; } //dark grey

inline wxColor getColorGridLine() { return wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW); }

const int FILE_GRID_GAP_SIZE_DIP = 2;
const int FILE_GRID_GAP_SIZE_WIDE_DIP = 6;

/* class hierarchy:            GridDataBase
                                    /|\
                     ________________|________________
                    |                                |
               GridDataRim                           |
                   /|\                               |
          __________|_________                       |
         |                    |                      |
   GridDataLeft         GridDataRight          GridDataCenter               */


//accessibility, support high-contrast schemes => work with user-defined background color!
wxColor getAlternateBackgroundColor()
{
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

    //just some very faint gradient to avoid visual distraction
    const wxColor altCol = getAdjustedColor(signLevel * 10);
    return altCol;
}


//improve readability (while lacking cell borders)
wxColor getDefaultBackgroundColorAlternating(bool wantStandardColor)
{
    if (wantStandardColor)
        return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    else
        return getAlternateBackgroundColor();
}


std::pair<CudAction, SelectSide> getCudAction(SyncOperation so)
{
    switch (so)
    {
        //*INDENT-OFF*
        case SO_CREATE_LEFT:
        case SO_MOVE_LEFT_TO: return {CudAction::create, SelectSide::left};

        case SO_CREATE_RIGHT:
        case SO_MOVE_RIGHT_TO: return {CudAction::create, SelectSide::right};

        case SO_DELETE_LEFT:
        case SO_MOVE_LEFT_FROM: return {CudAction::delete_, SelectSide::left};

        case SO_DELETE_RIGHT:
        case SO_MOVE_RIGHT_FROM: return {CudAction::delete_, SelectSide::right};

        case SO_OVERWRITE_LEFT:
        case SO_RENAME_LEFT: return {CudAction::update, SelectSide::left};

        case SO_OVERWRITE_RIGHT:
        case SO_RENAME_RIGHT: return {CudAction::update, SelectSide::right};

        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT: return {CudAction::noChange, SelectSide::left};
        //*INDENT-ON*
    }
    assert(false);
    return {CudAction::noChange, SelectSide::left};
}


wxColor getBackGroundColorSyncAction(SyncOperation so)
{
    switch (so)
    {
        case SO_CREATE_LEFT:
        case SO_OVERWRITE_LEFT:
        case SO_DELETE_LEFT:
        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_LEFT_TO:
        case SO_RENAME_LEFT:
            return getColorSyncBlue(false /*faint*/);

        case SO_CREATE_RIGHT:
        case SO_OVERWRITE_RIGHT:
        case SO_DELETE_RIGHT:
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_RIGHT_TO:
        case SO_RENAME_RIGHT:
            return getColorSyncGreen(false /*faint*/);

        case SO_DO_NOTHING:
            return getColorInactiveBack(false /*faint*/);
        case SO_EQUAL:
            break; //usually white
        case SO_UNRESOLVED_CONFLICT:
            return getColorConflictBackground(false /*faint*/);
    }
    return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
}


wxColor getBackGroundColorCmpDifference(CompareFileResult cmpResult)
{
    switch (cmpResult)
    {
        //*INDENT-OFF*
        case FILE_EQUAL:
            break; //usually white
        case FILE_LEFT_ONLY: return getColorSyncBlue(false /*faint*/);
        case FILE_LEFT_NEWER:     return getColorSyncBlue(true  /*faint*/);

        case FILE_RIGHT_ONLY: return getColorSyncGreen(false /*faint*/);
        case FILE_RIGHT_NEWER:     return getColorSyncGreen(true  /*faint*/);

        case FILE_DIFFERENT_CONTENT:
            return getColorDifferentBackground(false /*faint*/);
        case FILE_RENAMED: //similar to both "equal" and "conflict": give hint via background color
        case FILE_TIME_INVALID:
        case FILE_CONFLICT:
            return getColorConflictBackground(false /*faint*/);
        //*INDENT-ON*
    }
    return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
}


class GridEventManager;
class GridDataLeft;
class GridDataRight;

class IconUpdater : private wxEvtHandler //update file icons periodically: use SINGLE instance to coordinate left and right grids in parallel
{
public:
    IconUpdater(GridDataLeft& provLeft, GridDataRight& provRight, IconBuffer& iconBuffer) : provLeft_(provLeft), provRight_(provRight), iconBuffer_(iconBuffer)
    {
        timer_.Bind(wxEVT_TIMER, [this](wxTimerEvent& event) { loadIconsAsynchronously(event); });
    }

    void start() { if (!timer_.IsRunning()) timer_.Start(100); } //timer interval in [ms]
    //don't check too often! give worker thread some time to fetch data

private:
    void stop() { if (timer_.IsRunning()) timer_.Stop(); }

    void loadIconsAsynchronously(wxEvent& event); //loads all (not yet) drawn icons

    GridDataLeft&  provLeft_;
    GridDataRight& provRight_;
    IconBuffer& iconBuffer_;
    wxTimer timer_;
};


struct IconManager
{
    IconManager() {}

    IconManager(GridDataLeft& provLeft, GridDataRight& provRight, IconBuffer::IconSize sz, bool showFileIcons) :
        fileIcon_        (IconBuffer::genericFileIcon (showFileIcons ? sz : IconBuffer::IconSize::small)),
        dirIcon_         (IconBuffer::genericDirIcon  (showFileIcons ? sz : IconBuffer::IconSize::small)),
        linkOverlayIcon_ (IconBuffer::linkOverlayIcon (showFileIcons ? sz : IconBuffer::IconSize::small)),
        plusOverlayIcon_ (IconBuffer::plusOverlayIcon (showFileIcons ? sz : IconBuffer::IconSize::small)),
        minusOverlayIcon_(IconBuffer::minusOverlayIcon(showFileIcons ? sz : IconBuffer::IconSize::small))
    {
        if (showFileIcons)
        {
            iconBuffer_  = std::make_unique<IconBuffer>(sz);
            iconUpdater_ = std::make_unique<IconUpdater>(provLeft, provRight, *iconBuffer_);
        }
    }

    int getIconWxsize() const { return screenToWxsize(iconBuffer_ ? iconBuffer_->getPixSize() : IconBuffer::getPixSize(IconBuffer::IconSize::small)); }

    IconBuffer* getIconBuffer() { return iconBuffer_.get(); }
    void startIconUpdater() { assert(iconUpdater_); if (iconUpdater_) iconUpdater_->start(); }

    const wxImage& getGenericFileIcon () const { return fileIcon_;         }
    const wxImage& getGenericDirIcon  () const { return dirIcon_;          }
    const wxImage& getLinkOverlayIcon () const { return linkOverlayIcon_;  }
    const wxImage& getPlusOverlayIcon () const { return plusOverlayIcon_;  }
    const wxImage& getMinusOverlayIcon() const { return minusOverlayIcon_; }

private:
    const wxImage fileIcon_;
    const wxImage dirIcon_;
    const wxImage linkOverlayIcon_;
    const wxImage plusOverlayIcon_;
    const wxImage minusOverlayIcon_;

    std::unique_ptr<IconBuffer> iconBuffer_;
    std::unique_ptr<IconUpdater> iconUpdater_; //bind ownership to GridDataRim<>!
};


//mark rows selected on overview panel
class NavigationMarker
{
public:
    NavigationMarker() {}

    void set(std::unordered_set<const FileSystemObject*>&& markedFilesAndLinks,
             std::unordered_set<const ContainerObject*>&& markedContainer)
    {
        markedFilesAndLinks_.swap(markedFilesAndLinks);
        markedContainer_    .swap(markedContainer);
    }

    bool isMarked(const FileSystemObject& fsObj) const
    {
        if (markedFilesAndLinks_.contains(&fsObj)) //mark files/links directly
            return true;

        if (auto folder = dynamic_cast<const FolderPair*>(&fsObj))
            if (markedContainer_.contains(folder)) //mark folders which *are* the given ContainerObject*
                return true;

        //also mark all items with any matching ancestors
        for (const FileSystemObject* fsObj2 = &fsObj;;)
        {
            const ContainerObject& parent = fsObj2->parent();
            if (markedContainer_.contains(&parent))
                return true;

            fsObj2 = dynamic_cast<const FolderPair*>(&parent);
            if (!fsObj2)
                return false;
        }
    }

private:
    std::unordered_set<const FileSystemObject*> markedFilesAndLinks_; //mark files/symlinks directly within a container
    std::unordered_set<const ContainerObject*> markedContainer_;      //mark full container including all child-objects
    //DO NOT DEREFERENCE!!!! NOT GUARANTEED TO BE VALID!!!
};


struct SharedComponents //...between left, center, and right grids
{
    SharedRef<FileView> gridDataView = makeSharedRef<FileView>();
    SharedRef<IconManager> iconMgr = makeSharedRef<IconManager>();
    NavigationMarker navMarker;
    std::unique_ptr<GridEventManager> evtMgr;
    GridViewType gridViewType = GridViewType::action;
    std::unordered_map<std::wstring, wxSize, StringHash, StringEqual> compExtentsBuf_; //buffer expensive wxDC::GetTextExtent() calls!
    //StringHash, StringEqual => heterogenous lookup by std::wstring_view
};

//########################################################################################################

class GridDataBase : public GridData
{
public:
    GridDataBase(Grid& grid, const SharedRef<SharedComponents>& sharedComp) :
        grid_(grid), sharedComp_(sharedComp) {}

    void setData(FolderComparison& folderCmp)
    {
        sharedComp_.ref().gridDataView = makeSharedRef<FileView>(); //clear old data view first! avoid memory peaks!
        sharedComp_.ref().gridDataView = makeSharedRef<FileView>(folderCmp);
        sharedComp_.ref().compExtentsBuf_.clear(); //doesn't become stale! but still: re-calculate and save some memory...
    }

    GridEventManager* getEventManager() { return sharedComp_.ref().evtMgr.get(); }

    /**/  FileView& getDataView()       { return sharedComp_.ref().gridDataView.ref(); }
    const FileView& getDataView() const { return sharedComp_.ref().gridDataView.ref(); }

    void setIconManager(const SharedRef<IconManager>& iconMgr) { sharedComp_.ref().iconMgr = iconMgr; }

    IconManager& getIconManager() { return sharedComp_.ref().iconMgr.ref(); }

    GridViewType getViewType() const { return sharedComp_.ref().gridViewType; }
    void setViewType(GridViewType vt) { sharedComp_.ref().gridViewType = vt; }

    bool isNavMarked(const FileSystemObject& fsObj) const { return sharedComp_.ref().navMarker.isMarked(fsObj); }

    void setNavigationMarker(std::unordered_set<const FileSystemObject*>&& markedFilesAndLinks,
                             std::unordered_set<const ContainerObject*>&& markedContainer)
    {
        sharedComp_.ref().navMarker.set(std::move(markedFilesAndLinks), std::move(markedContainer));
    }

    Grid& refGrid() { return grid_; }
    const Grid& refGrid() const { return grid_; }

    const FileSystemObject* getFsObject(size_t row) const { return getDataView().getFsObject(row); }

    const wxSize& getTextExtentBuffered(wxDC& dc, const std::wstring_view& text)
    {
        auto& compExtentsBuf = sharedComp_.ref().compExtentsBuf_;
        //- only used for parent path names and file names on view => should not grow "too big"
        //- cleaned up during GridDataBase::setData()
        assert(!contains(text, L'\n'));

        auto it = compExtentsBuf.find(text);
        if (it == compExtentsBuf.end())
            it = compExtentsBuf.emplace(text, dc.GetTextExtent(copyStringTo<wxString>(text))).first;
        //GetTextExtent() returns (0, 0) for empty string!
        return it->second;
    }

    //- trim while leaving path components intact
    //- *always* returns at least one component, even if > maxWidth
    size_t getPathTrimmedSize(wxDC& dc, const std::wstring_view& itemPath, int maxWidth)
    {
        if (itemPath.size() <= 1)
            return itemPath.size();

        std::vector<std::wstring_view> subComp;

        //split path by components, but skip slash at beginning or end
        for (auto it = itemPath.begin() + 1; it != itemPath.end() - 1; ++it)
            if (*it == L'/' ||
                *it == L'\\')
                subComp.push_back(makeStringView(itemPath.begin(), it));

        subComp.push_back(itemPath);

        if (maxWidth <= 0)
            return subComp[0].size();

        size_t low  = 0;
        size_t high = subComp.size();

        for (;;)
        {
            if (high - low == 1)
                return subComp[low].size();

            const size_t middle = (low + high) / 2; //=> never 0 when "high - low > 1"

            if (getTextExtentBuffered(dc, subComp[middle]).GetWidth() <= maxWidth)
                low = middle;
            else
                high = middle;
        }
    }

private:
    size_t getRowCount() const override { return getDataView().rowsOnView(); }

    Grid& grid_;
    SharedRef<SharedComponents> sharedComp_;
};

//########################################################################################################

template <SelectSide side>
class GridDataRim : public GridDataBase
{
public:
    GridDataRim(Grid& grid, const SharedRef<SharedComponents>& sharedComp) : GridDataBase(grid, sharedComp) {}

    void setItemPathForm(ItemPathFormat fmt) { itemPathFormat_ = fmt; groupItemNamesWidthBuf_.clear(); }

    void getUnbufferedIconsForPreload(std::vector<std::pair<ptrdiff_t, AbstractPath>>& newLoad) //return (priority, filepath) list
    {
        if (IconBuffer* iconBuf = getIconManager().getIconBuffer())
        {
            const auto& [rowFirst, rowLast] = refGrid().getVisibleRows(refGrid().getMainWin().GetClientSize());
            const ptrdiff_t visibleRowCount = rowLast - rowFirst;

            //preload icons not yet on screen:
            const int preloadSize = 2 * std::max<ptrdiff_t>(20, visibleRowCount); //:= sum of lines above and below of visible range to preload
            //=> use full visible height to handle "next page" command and a minimum of 20 for excessive mouse wheel scrolls

            for (ptrdiff_t i = 0; i < preloadSize; ++i)
            {
                const ptrdiff_t currentRow = rowFirst - (preloadSize + 1) / 2 + getAlternatingPos(i, visibleRowCount + preloadSize); //for odd preloadSize start one row earlier

                if (const FileSystemObject* fsObj = getFsObject(currentRow))
                    if (getIconInfo(*fsObj).type == IconType::standard)
                        if (!iconBuf->readyForRetrieval(fsObj->template getAbstractPath<side>()))
                            newLoad.emplace_back(i, fsObj->template getAbstractPath<side>()); //insert least-important items on outer rim first
            }
        }
        else assert(false);
    }

    void updateNewAndGetUnbufferedIcons(std::vector<AbstractPath>& newLoad) //loads all not yet drawn icons
    {
        if (IconBuffer* iconBuf = getIconManager().getIconBuffer())
        {
            const auto& [rowFirst, rowLast] = refGrid().getVisibleRows(refGrid().getMainWin().GetClientSize());
            const ptrdiff_t visibleRowCount = rowLast - rowFirst;

            for (ptrdiff_t i = 0; i < visibleRowCount; ++i)
            {
                //alternate when adding rows: first, last, first + 1, last - 1 ...
                const ptrdiff_t currentRow = rowFirst + getAlternatingPos(i, visibleRowCount);

                if (isFailedLoad(currentRow)) //find failed attempts to load icon
                    if (const FileSystemObject* fsObj = getFsObject(currentRow))
                        if (getIconInfo(*fsObj).type == IconType::standard)
                        {
                            //test if they are already loaded in buffer:
                            if (iconBuf->readyForRetrieval(fsObj->template getAbstractPath<side>()))
                            {
                                //do a *full* refresh for *every* failed load to update partial DC updates while scrolling
                                refGrid().refreshCell(currentRow, static_cast<ColumnType>(ColumnTypeRim::path));
                                setFailedLoad(currentRow, false);
                            }
                            else //not yet in buffer: mark for async. loading
                                newLoad.push_back(fsObj->template getAbstractPath<side>());
                        }
            }
        }
        else assert(false);
    }

private:
    bool isFailedLoad(size_t row) const { return row < failedLoads_.size() ? failedLoads_[row] != 0 : false; }

    void setFailedLoad(size_t row, bool failed = true)
    {
        if (failedLoads_.size() != refGrid().getRowCount())
            failedLoads_.resize(refGrid().getRowCount());

        if (row < failedLoads_.size())
            failedLoads_[row] = failed;
    }

    //icon buffer will load reversely, i.e. if we want to go from inside out, we need to start from outside in
    static size_t getAlternatingPos(size_t pos, size_t total)
    {
        assert(pos < total);
        return pos % 2 == 0 ? pos / 2 : total - 1 - pos / 2;
    }

private:
    enum class DisplayType
    {
        inactive,
        normal,
        symlink,
    };
    static DisplayType getObjectDisplayType(const FileSystemObject& fsObj)
    {
        if (!fsObj.isActive())
            return DisplayType::inactive;

        DisplayType output = DisplayType::normal;

        visitFSObject(fsObj, [](const FolderPair& folder) {},
        [](const FilePair& file) {},
        [&](const SymlinkPair& symlink) { output = DisplayType::symlink; });

        return output;
    }


    std::wstring getValue(size_t row, ColumnType colType) const override
    {
        if (const FileSystemObject* fsObj = getFsObject(row))
            if (!fsObj->isEmpty<side>())
            {
                if (static_cast<ColumnTypeRim>(colType) == ColumnTypeRim::path)
                    switch (itemPathFormat_)
                    {
                        case ItemPathFormat::name:
                            return utfTo<std::wstring>(fsObj->getItemName<side>());
                        case ItemPathFormat::relative:
                            return utfTo<std::wstring>(fsObj->getRelativePath<side>());
                        case ItemPathFormat::full:
                            return AFS::getDisplayPath(fsObj->getAbstractPath<side>());
                    }

                std::wstring value; //dynamically allocates 16 byte memory! but why? shouldn't SSO make this superfluous?! or is it only in debug?
                switch (static_cast<ColumnTypeRim>(colType))
                {
                    case ColumnTypeRim::path:
                        assert(false);
                        break;

                    case ColumnTypeRim::size:
                        visitFSObject(*fsObj, [&](const FolderPair& folder) { /*value = L'<' + _("Folder") + L'>'; -> redundant!? */ },
                        [&](const FilePair& file) { value = formatNumber(file.getFileSize<side>()); },
                        //[&](const FilePair& file) { value = numberTo<std::wstring>(file.getFilePrint<side>()); }, // -> test file id
                        [&](const SymlinkPair& symlink) { value = L'<' + _("Symlink") + L'>'; });
                        break;

                    case ColumnTypeRim::date:
                        visitFSObject(*fsObj, [](const FolderPair& folder) {},
                        [&](const FilePair&       file) { value = formatUtcToLocalTime(file   .getLastWriteTime<side>()); },
                        [&](const SymlinkPair& symlink) { value = formatUtcToLocalTime(symlink.getLastWriteTime<side>()); });
                        break;

                    case ColumnTypeRim::extension:
                        visitFSObject(*fsObj, [](const FolderPair& folder) {},
                        [&](const FilePair&       file) { value = utfTo<std::wstring>(getFileExtension(file   .getItemName<side>())); },
                        [&](const SymlinkPair& symlink) { value = utfTo<std::wstring>(getFileExtension(symlink.getItemName<side>())); });
                        break;
                }
                return value;
            }
        return {};
    }

    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected, HoverArea rowHover) override
    {
        const FileView::PathDrawInfo pdi = getDataView().getDrawInfo(row);

        if (!enabled || !selected)
        {
            const wxColor backCol = [&]
            {
                if (pdi.fsObj && !pdi.fsObj->isEmpty<side>()) //do we need color indication for *inactive* empty rows? probably not...
                    switch (getObjectDisplayType(*pdi.fsObj))
                    {
                        //*INDENT-OFF*
                        case DisplayType::normal: break;
                        case DisplayType::symlink:  return getColorSymlinkBackground();
                        case DisplayType::inactive: return getColorInactiveBack(false /*faint*/);
                        //*INDENT-ON*
                    }
                return getDefaultBackgroundColorAlternating(pdi.groupIdx % 2 == 0);
            }();
            if (backCol != wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW) /*already the default!*/)
                clearArea(dc, rect, backCol);
        }
        else
            GridData::renderRowBackgound(dc, rect, row, true /*enabled*/, true /*selected*/, rowHover);

        //----------------------------------------------------------------------------------
        const wxRect rectLine(rect.x, rect.y + rect.height - dipToWxsize(1), rect.width, dipToWxsize(1));
        clearArea(dc, rectLine, row == pdi.groupLastRow - 1 || //last group item
                  (pdi.fsObj == pdi.folderGroupObj &&  //folder item => distinctive separation color against subsequent file items
                   itemPathFormat_ != ItemPathFormat::name) ?
                  getColorGridLine() : getDefaultBackgroundColorAlternating(pdi.groupIdx % 2 != 0));
    }


    int getGroupItemNamesWidth(wxDC& dc, const FileView::PathDrawInfo& pdi)
    {
        //FileView::updateView() called? => invalidates group item render buffer
        if (pdi.viewUpdateId != viewUpdateIdLast_)
        {
            viewUpdateIdLast_ = pdi.viewUpdateId;
            groupItemNamesWidthBuf_.clear();
        }

        auto& widthBuf = groupItemNamesWidthBuf_;
        if (pdi.groupIdx >= widthBuf.size())
            widthBuf.resize(pdi.groupIdx + 1, -1 /*sentinel value*/);

        int& itemNamesWidth = widthBuf[pdi.groupIdx];
        if (itemNamesWidth < 0)
        {
            itemNamesWidth = 0;
            //const int ellipsisWidth = getTextExtentBuffered(dc, ELLIPSIS).x;

            std::vector<int> itemWidths;
            for (size_t row2 = pdi.groupFirstRow; row2 < pdi.groupLastRow; ++row2)
                if (const FileSystemObject* fsObj = getDataView().getFsObject(row2))
                    if (itemPathFormat_ == ItemPathFormat::name || fsObj != pdi.folderGroupObj)
#if 0 //render same layout even when items don't exist
                        if (fsObj->isEmpty<side>())
                            itemNamesWidth = ellipsisWidth;
                        else
#endif
                            itemWidths.push_back(getTextExtentBuffered(dc, utfTo<std::wstring>(fsObj->getItemName<side>())).x);

            if (!itemWidths.empty())
            {
                //ignore (small number of) excessive file name widths:
                auto itPercentile = itemWidths.begin() + itemWidths.size() * 8 / 10; //80th percentile
                std::nth_element(itemWidths.begin(), itPercentile, itemWidths.end()); //complexity: O(n)
                itemNamesWidth = std::max(itemNamesWidth, *itPercentile);
            }
            assert(itemNamesWidth >= 0);

            //Note: A better/faster solution would be to get 80th percentile of all std::wstring::size(), then do a *single* getTextExtentBuffered()!
            //      However, we need all the getTextExtentBuffered(itemName) later anyway, so above is fine.
        }
        return itemNamesWidth;
    }


    struct GroupRowLayout
    {
        std::wstring groupParentPart; //... if distributed over multiple rows, otherwise full group parent folder
        std::wstring groupName; //only filled for first row of a group
        std::wstring itemName;
        int groupParentWidth;
        int groupNameWidth;
    };
    GroupRowLayout getGroupRowLayout(wxDC& dc, size_t row, const FileView::PathDrawInfo& pdi, int maxWidth)
    {
        assert(pdi.fsObj);

        const bool drawFileIcons = getIconManager().getIconBuffer();
        const int iconSize       = getIconManager().getIconWxsize();

        //--------------------------------------------------------------------
        const int ellipsisWidth       = getTextExtentBuffered(dc, ELLIPSIS).x;
        const int arrowRightDownWidth = getTextExtentBuffered(dc, rightArrowDown_).x;
        const int groupItemNamesWidth = getGroupItemNamesWidth(dc, pdi);
        //--------------------------------------------------------------------

        //exception for readability: top row is always group start!
        const size_t groupFirstRow = std::max<size_t>(pdi.groupFirstRow, refGrid().getRowAtWinPos(0));

        const size_t groupRowCount = pdi.groupLastRow - groupFirstRow;

        std::wstring itemName;
        if (itemPathFormat_ == ItemPathFormat::name || //hack: show folder name in item colum since groupName/groupParentFolder are unused!
            pdi.fsObj != pdi.folderGroupObj)           //=> consider groupItemNamesWidth!
            itemName = utfTo<std::wstring>(pdi.fsObj->getItemName<side>());
        //=> doesn't matter if isEmpty()! => only indicates if component should be drawn

        std::wstring groupName;
        std::wstring groupParentFolder;
        switch (itemPathFormat_)
        {
            case ItemPathFormat::name:
                break;

            case ItemPathFormat::relative:
                if (pdi.folderGroupObj)
                {
                    groupName         = utfTo<std::wstring>(pdi.folderGroupObj         ->template getItemName    <side>());
                    groupParentFolder = utfTo<std::wstring>(pdi.folderGroupObj->parent().template getRelativePath<side>());
                }
                break;

            case ItemPathFormat::full:
                if (pdi.folderGroupObj)
                {
                    groupName         = utfTo<std::wstring>(pdi.folderGroupObj         ->template getItemName    <side>());
                    groupParentFolder = AFS::getDisplayPath(pdi.folderGroupObj->parent().template getAbstractPath<side>());
                }
                else //=> BaseFolderPair
                    groupParentFolder = AFS::getDisplayPath(pdi.fsObj->base().getAbstractPath<side>());
                break;
        }

        if (!groupParentFolder.empty())
        {
            const wchar_t pathSep = [&]
            {
                for (auto it = groupParentFolder.end(); it != groupParentFolder.begin();) //reverse iteration: 1. check 2. decrement 3. evaluate
                {
                    --it; //

                    if (*it ==  L'/' ||
                        *it ==  L'\\')
                        return *it;
                }
                return static_cast<wchar_t>(FILE_NAME_SEPARATOR);
            }();
            if (!endsWith(groupParentFolder, pathSep)) //visual hint that this is a parent folder only
                groupParentFolder += pathSep;          //
        }

        /*  group details: single row
            ________________________  ___________________________________  _____________________________________________________
            | (gap | group parent) |  | (gap | icon | gap | group name) |  | (2x gap | vline) | (gap | icon) | gap | item name |
            ------------------------  -----------------------------------  -----------------------------------------------------

            group details: stacked
            __________________________________                  ___________________________________  ___________________________________________________
            | gap | group parent, part 1 | ⤵️ |  <right-aligned> | (gap | icon | gap | group name) |  |                | (gap | icon) | gap | item name |
            |-------------------------------------------------------------------------------------|  | 2x gap | vline |--------------------------------|
            | gap | group parent, part n                                                          |  |                | (gap | icon) | gap | item name |
            ---------------------------------------------------------------------------------------  ---------------------------------------------------

                -> group name on first row
                -> parent name distributed over multiple rows, if needed                                                        */

        int groupParentWidth = groupParentFolder.empty() ? 0 : (gapSize_ + getTextExtentBuffered(dc, groupParentFolder).x);

        int       groupNameWidth    = groupName.empty() ? 0 : (gapSize_ + iconSize + gapSize_ + getTextExtentBuffered(dc, groupName).x);
        const int groupNameMinWidth = groupName.empty() ? 0 : (gapSize_ + iconSize + gapSize_ + ellipsisWidth);

        const int groupSepWidth = (groupParentFolder.empty() && groupName.empty()) ? 0 : (2 * gapSize_ + dipToWxsize(1));

        int       groupItemsWidth    = groupSepWidth + (drawFileIcons ? gapSize_ + iconSize : 0) + gapSize_ + groupItemNamesWidth;
        const int groupItemsMinWidth = groupSepWidth + (drawFileIcons ? gapSize_ + iconSize : 0) + gapSize_ + ellipsisWidth;

        std::wstring groupParentPart;

        //not enough space? => trim or render on multiple rows
        if (int excessWidth = groupParentWidth + groupNameWidth + groupItemsWidth - maxWidth;
            excessWidth > 0)
        {
            //1. shrink group parent
            if (!groupParentFolder.empty())
            {
                const int groupParentMinWidth = !groupName.empty() && groupRowCount > 1 ? //group parent details (possibly) on multiple rows
                                                0 : gapSize_ + ellipsisWidth;

                groupParentWidth = std::max(groupParentWidth - excessWidth, groupParentMinWidth);
                excessWidth = groupParentWidth + groupNameWidth + groupItemsWidth - maxWidth;
            }

            if (excessWidth > 0)
            {
                //2. shrink item rendering
                groupItemsWidth = std::max(groupItemsWidth - excessWidth, groupItemsMinWidth);
                excessWidth = groupParentWidth + groupNameWidth + groupItemsWidth - maxWidth;

                if (excessWidth > 0)
                    //3. shrink group name
                    if (!groupName.empty())
                        groupNameWidth = std::max(groupNameWidth - excessWidth, groupNameMinWidth);
            }

            //group parent details on multiple lines
            if (!groupParentFolder.empty())
            {
                //let's not waste empty row space for medium + large icon sizes: print multiple lines per row!
                const int linesPerRow = std::max(refGrid().getRowHeight() / charHeight_, 1);

                size_t compPos = 0;
                for (size_t i = groupFirstRow; i <= row; ++i)
                    for (int l = 0; l < linesPerRow; ++l)
                    {
                        const size_t compLen = i == pdi.groupLastRow - 1 && l == linesPerRow - 1 ? //not enough rows to show remaining parent folder components?
                                               groupParentFolder.size() - compPos : //=> append the rest: will be truncated with ellipsis
                                               getPathTrimmedSize(dc, makeStringView(groupParentFolder.begin() + compPos, groupParentFolder.end()),
                                                                  groupParentWidth + (i == groupFirstRow ? 0 : groupNameWidth) - gapSize_ - arrowRightDownWidth);

                        if (i == groupFirstRow && !groupName.empty() && groupRowCount > 1 &&
                            getTextExtentBuffered(dc, makeStringView(groupParentFolder.begin() + compPos, compLen)).x > groupParentWidth - gapSize_ - arrowRightDownWidth)
                        {
                            if (i == row && l != 0)
                                groupParentPart.insert(groupParentPart.begin(), linesPerRow - l, L'\n'); //effectively: "align bottom" for first row
                            break; //exception: never truncate parent component on first row, but continue on second row instead
                        }

                        if (i == row)
                            groupParentPart += compPos + compLen == groupParentFolder.size() ?
                                               groupParentFolder.substr(compPos) :
                                               groupParentFolder.substr(compPos, compLen) + rightArrowDown_ + L'\n';
                        compPos += compLen;

                        if (compPos == groupParentFolder.size())
                            goto break2;
                    }
break2:
                if (endsWith(groupParentPart, L'\n'))
                    groupParentPart.pop_back();
            }
        }
        else
        {
            if (row == groupFirstRow)
                groupParentPart = groupParentFolder;
        }

        //path components should follow the app layout direction and are NOT a single piece of text!
        //caveat: - add Bidi support only during rendering and not in getValue() or AFS::getDisplayPath(): e.g. support "open file in Explorer"
        //        - add *after* getPathTrimmedSize(), otherwise LTR-mark can be confused for path component, e.g. "<LTR>/home" would be two components!
        assert(!contains(groupParentPart, slashBidi_) && !contains(groupParentPart, bslashBidi_));
        replace(groupParentPart, L'/',   slashBidi_);
        replace(groupParentPart, L'\\', bslashBidi_);

        return
        {
            std::move(groupParentPart),
            row == groupFirstRow ? std::move(groupName) : std::wstring{},
            std::move(itemName),
            row == groupFirstRow ? groupParentWidth : groupParentWidth + groupNameWidth,
            row == groupFirstRow ? groupNameWidth : 0,
        };
    }


    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover) override
    {
        //-----------------------------------------------
        //don't forget: harmonize with getBestSize()!!!
        //-----------------------------------------------

        if (const FileView::PathDrawInfo pdi = getDataView().getDrawInfo(row);
            pdi.fsObj)
        {
            //accessibility: always set both foreground AND background colors!
            wxDCTextColourChanger textColor(dc);
            if (enabled && selected) //=> coordinate with renderRowBackgound()
                textColor.Set(*wxBLACK);
            else if (!pdi.fsObj->isEmpty<side>())
                switch (getObjectDisplayType(*pdi.fsObj))
                {
                    //*INDENT-OFF*
                    case DisplayType::normal: break;
                    case DisplayType::symlink:  textColor.Set(*wxBLACK); break;
                    case DisplayType::inactive: textColor.Set(getColorInactiveText()); break;
                    //*INDENT-ON*
                }

            wxRect rectTmp = rect;

            switch (static_cast<ColumnTypeRim>(colType))
            {
                case ColumnTypeRim::path:
                {
                    auto drawCudHighlight = [&](wxRect rectCud, SyncOperation syncOp)
                    {
                        if (getViewType() == GridViewType::action)
                            if (!enabled || !selected)
                                if (const auto& [cudAction, cudSide] = getCudAction(syncOp);
                                    cudAction != CudAction::noChange && side == cudSide)
                                {
                                    rectCud.width = gapSize_ + screenToWxsize(IconBuffer::getPixSize(IconBuffer::IconSize::small));
                                    //fixed-size looks fine for all icon sizes! use same width even if file icons are disabled!
                                    clearArea(dc, rectCud, getBackGroundColorSyncAction(syncOp));

                                    rectCud.x += rectCud.width;
                                    rectCud.width = gapSize_ + dipToWxsize(2);

#if 0 //wxDC::GetPixel() is broken in GTK3! https://github.com/wxWidgets/wxWidgets/issues/14067
                                    wxColor backCol = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
                                    dc.GetPixel(rectCud.GetTopRight(), &backCol);
#else
                                    const wxColor backCol = getDefaultBackgroundColorAlternating(pdi.groupIdx % 2 == 0);
#endif
                                    dc.GradientFillLinear(rectCud, getBackGroundColorSyncAction(syncOp), backCol, wxEAST);
                                }
                    };

                    bool navMarkerDrawn = false;
                    auto tryDrawNavMarker = [&](wxRect rectNav)
                    {
                        if (!navMarkerDrawn &&
                            rectNav.x == rect.x && //draw marker *only* if current render group (group parent, group name, item name) is at beginning of a row!
                            isNavMarked(*pdi.fsObj) &&
                            (!enabled || !selected))
                        {
                            rectNav.width = std::min(rectNav.width, dipToWxsize(10));

                            if (row == pdi.groupLastRow - 1 /*last group item*/) //preserve the group separation line!
                                rectNav.height -= dipToWxsize(1);

                            dc.GradientFillLinear(rectNav, getColorSelectionGradientFrom(), getColorSelectionGradientTo(), wxEAST);
                            navMarkerDrawn = true;
                        }
                    };

                    auto drawIcon = [&](wxImage icon, wxRect rectIcon, bool drawActive)
                    {
                        if (!drawActive)
                            icon = icon.ConvertToGreyscale(1.0 / 3, 1.0 / 3, 1.0 / 3); //treat all channels equally!

                        if (!enabled)
                            icon = icon.ConvertToDisabled();

                        rectIcon.x += gapSize_;
                        rectIcon.width = getIconManager().getIconWxsize(); //center smaller-than-default icons
                        drawBitmapRtlNoMirror(dc, icon, rectIcon, wxALIGN_CENTER);
                    };

                    auto drawFileIcon = [this, &drawIcon](const wxImage& fileIcon, bool drawAsLink, const wxRect& rectIcon, const FileSystemObject& fsObj)
                    {
                        if (fileIcon.IsOk())
                            drawIcon(fileIcon, rectIcon, fsObj.isActive());

                        if (drawAsLink)
                            drawIcon(getIconManager().getLinkOverlayIcon(), rectIcon, fsObj.isActive());

                        if (getViewType() == GridViewType::action)
                            if (const auto& [cudAction, cudSide] = getCudAction(fsObj.getSyncOperation());
                                side == cudSide)
                                switch (cudAction)
                                {
                                    case CudAction::create:
                                        assert(!fileIcon.IsOk() && !drawAsLink);
                                        if (const bool isFolder = dynamic_cast<const FolderPair*>(&fsObj) != nullptr)
                                            drawIcon(getIconManager().getGenericDirIcon().ConvertToGreyscale(1.0 / 3, 1.0 / 3, 1.0 / 3). //treat all channels equally!
                                                     ConvertToDisabled(), rectIcon, true /*drawActive: [!]*/); //visual hint to distinguish file/folder creation

                                        //too much clutter? => drawIcon(getIconManager().getPlusOverlayIcon(), rectIcon,
                                        //                              true /*drawActive: [!] e.g. disabled folder, exists left only, where child item is copied*/);
                                        break;
                                    case CudAction::delete_:
                                        drawIcon(getIconManager().getMinusOverlayIcon(), rectIcon, true /*drawActive: [!]*/);
                                        break;
                                    case CudAction::noChange:
                                    case CudAction::update:
                                        break;
                                };
                    };
                    //-------------------------------------------------------------------------

                    const auto& [groupParentPart,
                                 groupName,
                                 itemName,
                                 groupParentWidth,
                                 groupNameWidth] = getGroupRowLayout(dc, row, pdi, rectTmp.width);

                    wxRect rectGroup, rectGroupParent, rectGroupName;
                    rectGroup = rectGroupParent = rectGroupName = rectTmp;

                    rectGroup      .width = groupParentWidth + groupNameWidth;
                    rectGroupParent.width = groupParentWidth;
                    rectGroupName  .width = groupNameWidth;
                    rectGroupName.x += groupParentWidth;

                    rectTmp.x     += rectGroup.width;
                    rectTmp.width -= rectGroup.width;

                    wxRect rectGroupItems = rectTmp;

                    if (itemName.empty()) //expand group name to include unused item area (e.g. bigger selection border)
                    {
                        rectGroupName.width += rectGroupItems.width;
                        rectGroupItems.width = 0;
                    }

                    //-------------------------------------------------------------------------
                    {
                        //clear background below parent path => harmonize with renderRowBackgound()
                        wxDCTextColourChanger textColorGroup(dc);
                        if (rectGroup.width > 0 &&
                            (!enabled || !selected))
                        {
                            wxRect rectGroupBack = rectGroup;
                            rectGroupBack.width += 2 * gapSize_; //include gap before vline

                            if (row == pdi.groupLastRow - 1 /*last group item*/) //preserve the group separation line!
                                rectGroupBack.height -= dipToWxsize(1);

                            clearArea(dc, rectGroupBack, getDefaultBackgroundColorAlternating(pdi.groupIdx % 2 == 0));
                            //clearArea() is surprisingly expensive => call just once!
                            textColorGroup.Set(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
                            //accessibility: always set *both* foreground AND background colors!
                        }

                        if (!groupParentPart.empty() &&
                            (!pdi.folderGroupObj || !pdi.folderGroupObj->isEmpty<side>())) //don't show for missing folders
                        {
                            tryDrawNavMarker(rectGroupParent);

                            wxRect rectGroupParentText = rectGroupParent;
                            rectGroupParentText.x     += gapSize_;
                            rectGroupParentText.width -= gapSize_;

                            //let's not waste empty row space for medium + large icon sizes: print multiple lines per row!
                            split(groupParentPart, L'\n', [&, linesPerRow = std::max(refGrid().getRowHeight() / charHeight_, 1),
                                                              lineNo = 0](const std::wstring_view line) mutable
                            {
                                drawCellText(dc, {
                                    rectGroupParentText.x, //distribute lines evenly across multiple rows:
                                    rectGroupParentText.y + (rectGroupParentText.height * (1 + lineNo++ * 2) - linesPerRow * charHeight_) / (linesPerRow * 2),
                                    rectGroupParentText.width, charHeight_
                                }, line, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, &getTextExtentBuffered(dc, line));
                            });
#if 0
                            drawCellText(dc, rectGroupParentText, groupParentPart, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, &getTextExtentBuffered(dc, groupParentPart));
#endif
                        }

                        if (!groupName.empty())
                        {
                            wxRect rectGroupNameBack = rectGroupName;

                            if (!itemName.empty())
                                rectGroupNameBack.width += 2 * gapSize_; //include gap left of item vline
                            rectGroupNameBack.height -= dipToWxsize(1); //harmonize with item separation lines

                            wxDCTextColourChanger textColorGroupName(dc);
                            //folder background: coordinate with renderRowBackgound()
                            if (!enabled || !selected)
                                if (!pdi.folderGroupObj->isEmpty<side>() &&
                                    !pdi.folderGroupObj->isActive())
                                {
                                    clearArea(dc, rectGroupNameBack, getColorInactiveBack(false /*faint*/));
                                    textColorGroupName.Set(getColorInactiveText());
                                }
                            drawCudHighlight(rectGroupNameBack, pdi.folderGroupObj->getSyncOperation());
                            tryDrawNavMarker(rectGroupName);

                            wxImage folderIcon;
                            bool drawAsLink = false;
                            if (!pdi.folderGroupObj->isEmpty<side>())
                            {
                                folderIcon = getIconManager().getGenericDirIcon();
                                drawAsLink = pdi.folderGroupObj->isFollowedSymlink<side>();
                            }
                            drawFileIcon(folderIcon, drawAsLink, rectGroupName, *pdi.folderGroupObj);
                            rectGroupName.x     += gapSize_ + getIconManager().getIconWxsize() + gapSize_;
                            rectGroupName.width -= gapSize_ + getIconManager().getIconWxsize() + gapSize_;

                            //mouse highlight: group name
                            if (static_cast<HoverAreaGroup>(rowHover) == HoverAreaGroup::groupName ||
                                (static_cast<HoverAreaGroup>(rowHover) == HoverAreaGroup::item && pdi.fsObj == pdi.folderGroupObj /*exception: extend highlight*/))
                                drawRectangleBorder(dc, rectGroupNameBack, *wxBLUE, dipToWxsize(1));

                            if (!pdi.folderGroupObj->isEmpty<side>())
                                drawCellText(dc, rectGroupName, groupName, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, &getTextExtentBuffered(dc, groupName));
                        }
                    }

                    //-------------------------------------------------------------------------
                    if (!itemName.empty())
                    {
                        //draw group/items separation line
                        if (rectGroup.width > 0)
                        {
                            rectGroupItems.x     += 2 * gapSize_;
                            rectGroupItems.width -= 2 * gapSize_;

                            wxRect rectLine = rectGroupItems;
                            rectLine.width = dipToWxsize(1);
                            clearArea(dc, rectLine, getColorGridLine());

                            rectGroupItems.x     += dipToWxsize(1);
                            rectGroupItems.width -= dipToWxsize(1);
                        }
                        //-------------------------------------------------------------------------

                        wxRect rectItemsBack = rectGroupItems;
                        rectItemsBack.height -= dipToWxsize(1); //preserve item separation lines!

                        drawCudHighlight(rectItemsBack, pdi.fsObj->getSyncOperation());
                        tryDrawNavMarker(rectGroupItems);

                        if (IconBuffer* iconBuf = getIconManager().getIconBuffer()) //=> draw file icons
                        {
                            /* whenever there's something new to render on screen, start up watching for failed icon drawing:
                               => ideally it would suffice to start watching only when scrolling grid or showing new grid content, but this solution is more robust
                               and the icon updater will stop automatically when finished anyway
                               Note: it's not sufficient to start up on failed icon loads only, since we support prefetching of not yet visible rows!!! */
                            getIconManager().startIconUpdater();

                            wxImage fileIcon;
                            const IconInfo ii = getIconInfo(*pdi.fsObj);
                            switch (ii.type)
                            {
                                case IconType::folder:
                                    fileIcon = getIconManager().getGenericDirIcon();
                                    break;

                                case IconType::standard:
                                    if (std::optional<wxImage> tmpIco = iconBuf->retrieveFileIcon(pdi.fsObj->template getAbstractPath<side>()))
                                        fileIcon = *tmpIco;
                                    else
                                    {
                                        setFailedLoad(row); //save status of failed icon load -> used for async. icon loading
                                        //falsify only! avoid writing incorrect success status when only partially updating the DC, e.g. during scrolling,
                                        //see repaint behavior of ::ScrollWindow() function!
                                        fileIcon = iconBuf->getIconByExtension(pdi.fsObj->template getItemName<side>()); //better than nothing
                                    }
                                    break;

                                case IconType::none:
                                    break;
                            }
                            drawFileIcon(fileIcon, ii.drawAsLink, rectGroupItems, *pdi.fsObj);
                            rectGroupItems.x     += gapSize_ + getIconManager().getIconWxsize();
                            rectGroupItems.width -= gapSize_ + getIconManager().getIconWxsize();
                        }

                        rectGroupItems.x     += gapSize_;
                        rectGroupItems.width -= gapSize_;

                        //mouse highlight: item name
                        if (static_cast<HoverAreaGroup>(rowHover) == HoverAreaGroup::item)
                            drawRectangleBorder(dc, rectItemsBack, *wxBLUE, dipToWxsize(1));

                        if (!pdi.fsObj->isEmpty<side>())
                            drawCellText(dc, rectGroupItems, itemName, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, &getTextExtentBuffered(dc, itemName));
                    }

                    //if not done yet:
                    tryDrawNavMarker(rect);
                }
                break;

                case ColumnTypeRim::size:
                    if (refGrid().GetLayoutDirection() != wxLayout_RightToLeft)
                    {
                        rectTmp.width -= gapSize_; //have file size right-justified (but don't change for RTL languages)
                        drawCellText(dc, rectTmp, getValue(row, colType), wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
                    }
                    else
                    {
                        rectTmp.x     += gapSize_;
                        rectTmp.width -= gapSize_;
                        drawCellText(dc, rectTmp, getValue(row, colType), wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
                    }
                    break;

                case ColumnTypeRim::date:
                case ColumnTypeRim::extension:
                    rectTmp.x     += gapSize_;
                    rectTmp.width -= gapSize_;
                    drawCellText(dc, rectTmp, getValue(row, colType), wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
                    break;
            }
        }
    }


    HoverArea getMouseHover(wxDC& dc, size_t row, ColumnType colType, int cellRelativePosX, int cellWidth) override
    {
        if (static_cast<ColumnTypeRim>(colType) == ColumnTypeRim::path)
            if (const FileView::PathDrawInfo pdi = getDataView().getDrawInfo(row);
                pdi.fsObj)
            {
                const auto& [groupParentPart,
                             groupName,
                             itemName,
                             groupParentWidth,
                             groupNameWidth] = getGroupRowLayout(dc, row, pdi, cellWidth);

                if (!groupName.empty() && pdi.fsObj != pdi.folderGroupObj)
                {
                    const int groupNameCellBeginX = groupParentWidth;

                    if (groupNameCellBeginX <= cellRelativePosX && cellRelativePosX < groupNameCellBeginX + groupNameWidth + 2 * gapSize_ /*include gap before vline*/)
                        return static_cast<HoverArea>(HoverAreaGroup::groupName);
                }
            }
        return static_cast<HoverArea>(HoverAreaGroup::item);
    }


    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        if (static_cast<ColumnTypeRim>(colType) == ColumnTypeRim::path)
        {
            int bestSize = 0;

            if (const FileView::PathDrawInfo pdi = getDataView().getDrawInfo(row);
                pdi.fsObj)
            {
                const int insanelyHugeWidth = 1000'000'000; //(hopefully) still small enough to avoid integer overflows
                /* ________________________  ___________________________________  _____________________________________________________
                   | (gap | group parent) |  | (gap | icon | gap | group name) |  | (2x gap | vline) | (gap | icon) | gap | item name |
                   ------------------------  -----------------------------------  ----------------------------------------------------- */
                const auto& [groupParentPart,
                             groupName,
                             itemName,
                             groupParentWidth,
                             groupNameWidth] = getGroupRowLayout(dc, row, pdi, insanelyHugeWidth);

                const int groupSepWidth = groupParentWidth + groupNameWidth <= 0 ? 0 : (2 * gapSize_ + dipToWxsize(1));
                const int fileIconWidth = getIconManager().getIconBuffer() ? gapSize_ + getIconManager().getIconWxsize() : 0;
                const int ellipsisWidth = getTextExtentBuffered(dc, ELLIPSIS).x;
                const int itemWidth = itemName.empty() ? 0 :
                                      (groupSepWidth + fileIconWidth + gapSize_ +
                                       (pdi.fsObj->isEmpty<side>() ? ellipsisWidth : getTextExtentBuffered(dc, itemName).x));

                bestSize += groupParentWidth + groupNameWidth + itemWidth + gapSize_ /*[!]*/;
            }
            return bestSize;
        }
        else
        {
            const std::wstring cellValue = getValue(row, colType);
            return gapSize_ + dc.GetTextExtent(cellValue).GetWidth() + gapSize_;
        }
    }


    std::wstring getColumnLabel(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeRim>(colType))
        {
            case ColumnTypeRim::path:
                switch (itemPathFormat_)
                {
                    case ItemPathFormat::name:
                        return _("Item name");
                    case ItemPathFormat::relative:
                        return _("Relative path");
                    case ItemPathFormat::full:
                        return _("Full path");
                }
                assert(false);
                break;
            case ColumnTypeRim::size:
                return _("Size");
            case ColumnTypeRim::date:
                return _("Date");
            case ColumnTypeRim::extension:
                return _("Extension");
        }
        //assert(false); may be ColumnType::none
        return std::wstring();
    }

    void renderColumnLabel(wxDC& dc, const wxRect& rect, ColumnType colType, bool enabled, bool highlighted) override
    {
        const wxRect rectInner = drawColumnLabelBackground(dc, rect, highlighted);
        wxRect rectRemain = rectInner;

        rectRemain.x     += getColumnGapLeft();
        rectRemain.width -= getColumnGapLeft();
        drawColumnLabelText(dc, rectRemain, getColumnLabel(colType), enabled);

        //draw sort marker
        if (auto sortInfo = getDataView().getSortConfig())
            if (const ColumnTypeRim* sortType = std::get_if<ColumnTypeRim>(&sortInfo->sortCol))
                if (*sortType == static_cast<ColumnTypeRim>(colType) && sortInfo->onLeft == (side == SelectSide::left))
                {
                    bool ascending = sortInfo->ascending; //work around MSVC 17.4 compiler bug :( "error C2039: 'sortCol': is not a member of 'fff::FileView::SortInfo'"

                    const wxImage sortMarker = loadImage(ascending ? "sort_ascending" : "sort_descending");
                    drawBitmapRtlNoMirror(dc, enabled ? sortMarker : sortMarker.ConvertToDisabled(), rectInner, wxALIGN_CENTER_HORIZONTAL);
                }
    }

    std::wstring getToolTip(size_t row, ColumnType colType, HoverArea rowHover) override
    {
        const FileView::PathDrawInfo pdi = getDataView().getDrawInfo(row);

        std::wstring toolTip;

        if (const FileSystemObject* tipObj = static_cast<HoverAreaGroup>(rowHover) == HoverAreaGroup::groupName ? pdi.folderGroupObj : pdi.fsObj)
        {
            toolTip = getDataView().getEffectiveFolderPairCount() > 1 ?
                      AFS::getDisplayPath(tipObj->getAbstractPath<side>()) :
                      utfTo<std::wstring>(tipObj->getRelativePath<side>());

            //path components should follow the app layout direction and are NOT a single piece of text!
            //caveat: add Bidi support only during rendering and not in getValue() or AFS::getDisplayPath(): e.g. support "open file in Explorer"
            assert(!contains(toolTip, slashBidi_) && !contains(toolTip, bslashBidi_));
            replace(toolTip, L'/',   slashBidi_);
            replace(toolTip, L'\\', bslashBidi_);

            if (tipObj->isEmpty<side>())
                toolTip += L"\n<" + _("Item not existing") + L'>';
            else
                visitFSObject(*tipObj, [&](const FolderPair& folder)
            {
                //toolTip += L"\n<" + _("Folder") + L'>'; -> redundant!?
            },
            [&](const FilePair& file)
            {
                toolTip += L'\n' + _("Size:") + L' ' + formatFilesizeShort (file.getFileSize     <side>()) +
                           L'\n' + _("Date:") + L' ' + formatUtcToLocalTime(file.getLastWriteTime<side>());
            },
            [&](const SymlinkPair& symlink)
            {
                toolTip +=  L"\n<" + _("Symlink") + L'>' +
                            L'\n'  + _("Date:") + L' ' + formatUtcToLocalTime(symlink.getLastWriteTime<side>());
            });
        }
        return toolTip;
    }


    enum class IconType
    {
        none,
        folder,
        standard,
    };
    struct IconInfo
    {
        IconType type = IconType::none;
        bool drawAsLink = false;
    };
    static IconInfo getIconInfo(const FileSystemObject& fsObj)
    {
        IconInfo out;

        if (!fsObj.isEmpty<side>())
            visitFSObject(fsObj, [&](const FolderPair& folder)
        {
            out.type       = IconType::folder;
            out.drawAsLink = folder.isFollowedSymlink<side>();
        },

        [&](const FilePair& file)
        {
            out.type       = IconType::standard;
            out.drawAsLink = file.isFollowedSymlink<side>() || hasLinkExtension(file.getItemName<side>());
        },

        [&](const SymlinkPair& symlink)
        {
            out.type       = IconType::standard;
            out.drawAsLink = true;
        });
        return out;
    }

    const int gapSize_     = dipToWxsize(FILE_GRID_GAP_SIZE_DIP);
    const int gapSizeWide_ = dipToWxsize(FILE_GRID_GAP_SIZE_WIDE_DIP);

    const int charHeight_ = refGrid().getMainWin().GetCharHeight();

    ItemPathFormat itemPathFormat_ = ItemPathFormat::full;

    std::vector<unsigned char> failedLoads_; //effectively a vector<bool> of size "number of rows"

    const std::wstring  slashBidi_ = (wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft ? RTL_MARK : LTR_MARK) + std::wstring(L"/");
    const std::wstring bslashBidi_ = (wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft ? RTL_MARK : LTR_MARK) + std::wstring(L"\\");
    //no need for LTR/RTL marks on both sides: text follows main direction if slash is between two strong characters with different directions

    const std::wstring rightArrowDown_ = wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft ?
                                         std::wstring() + RTL_MARK + LEFT_ARROW_ANTICLOCK :
                                         std::wstring() + LTR_MARK + RIGHT_ARROW_CURV_DOWN;
    //Windows bug: RIGHT_ARROW_CURV_DOWN rendering and extent calculation is buggy (see wx+\tooltip.cpp) => need LTR mark!

    std::vector<int> groupItemNamesWidthBuf_; //buffer! groupItemNamesWidths essentially only depends on (groupIdx, side)
    uint64_t viewUpdateIdLast_ = 0;           //
};


class GridDataLeft : public GridDataRim<SelectSide::left>
{
public:
    GridDataLeft(Grid& grid, const SharedRef<SharedComponents>& sharedComp) : GridDataRim<SelectSide::left>(grid, sharedComp) {}
};

class GridDataRight : public GridDataRim<SelectSide::right>
{
public:
    GridDataRight(Grid& grid, const SharedRef<SharedComponents>& sharedComp) : GridDataRim<SelectSide::right>(grid, sharedComp) {}
};

//########################################################################################################

class GridDataCenter : public GridDataBase
{
public:
    GridDataCenter(Grid& grid, const SharedRef<SharedComponents>& sharedComp) : GridDataBase(grid, sharedComp),
        toolTip_(grid) {} //tool tip must not live longer than grid!

    void onSelectBegin()
    {
        selectionInProgress_ = true;
        refGrid().clearSelection(GridEventPolicy::deny); //don't emit event, prevent recursion!
        toolTip_.hide(); //handle custom tooltip
    }

    void onSelectEnd(size_t rowFirst, size_t rowLast, HoverArea rowHover, ptrdiff_t clickInitRow)
    {
        refGrid().clearSelection(GridEventPolicy::deny); //don't emit event, prevent recursion!

        //issue custom event
        if (selectionInProgress_) //don't process selections initiated by right-click
            if (rowFirst < rowLast && rowLast <= refGrid().getRowCount()) //empty? probably not in this context
                switch (static_cast<HoverAreaCenter>(rowHover))
                {
                    case HoverAreaCenter::checkbox:
                        if (const FileSystemObject* fsObj = getFsObject(clickInitRow))
                        {
                            const bool setIncluded = !fsObj->isActive();
                            CheckRowsEvent evt(rowFirst, rowLast, setIncluded);
                            refGrid().GetEventHandler()->ProcessEvent(evt);
                        }
                        break;
                    case HoverAreaCenter::dirLeft:
                    {
                        SyncDirectionEvent evt(rowFirst, rowLast, SyncDirection::left);
                        refGrid().GetEventHandler()->ProcessEvent(evt);
                    }
                    break;
                    case HoverAreaCenter::dirNone:
                    {
                        SyncDirectionEvent evt(rowFirst, rowLast, SyncDirection::none);
                        refGrid().GetEventHandler()->ProcessEvent(evt);
                    }
                    break;
                    case HoverAreaCenter::dirRight:
                    {
                        SyncDirectionEvent evt(rowFirst, rowLast, SyncDirection::right);
                        refGrid().GetEventHandler()->ProcessEvent(evt);
                    }
                    break;
                }
        selectionInProgress_ = false;

        //update highlight_ and tooltip: on OS X no mouse movement event is generated after a mouse button click (unlike on Windows)
        wxPoint clientPos = refGrid().getMainWin().ScreenToClient(wxGetMousePosition());
        evalMouseMovement(clientPos);
    }

    void evalMouseMovement(const wxPoint& clientPos)
    {
        //manage block highlighting and custom tooltip
        if (!selectionInProgress_)
        {
            const size_t              row = refGrid().getRowAtWinPos   (clientPos.y); //return -1 for invalid position, rowCount if past the end
            const Grid::ColumnPosInfo cpi = refGrid().getColumnAtWinPos(clientPos.x); //returns ColumnType::none if no column at x position!

            if (row < refGrid().getRowCount() && cpi.colType != ColumnType::none &&
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

private:
    std::wstring getValue(size_t row, ColumnType colType) const override
    {
        if (const FileSystemObject* fsObj = getFsObject(row))
            switch (static_cast<ColumnTypeCenter>(colType))
            {
                case ColumnTypeCenter::checkbox:
                    break;
                case ColumnTypeCenter::difference:
                    return getSymbol(fsObj->getCategory());
                case ColumnTypeCenter::action:
                    return getSymbol(fsObj->getSyncOperation());
            }
        return std::wstring();
    }

    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected, HoverArea rowHover) override
    {
        const FileView::PathDrawInfo pdi = getDataView().getDrawInfo(row);

        if (!enabled || !selected)
        {
            const wxColor backCol = [&]
            {
                if (!pdi.fsObj)
                    return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

                if (!pdi.fsObj->isActive())
                    return getColorInactiveBack(false /*faint*/);

                return getDefaultBackgroundColorAlternating(pdi.groupIdx % 2 == 0);
            }();
            if (backCol != wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW) /*already the default!*/)
                clearArea(dc, rect, backCol);
        }
        else
            GridData::renderRowBackgound(dc, rect, row, true /*enabled*/, true /*selected*/, rowHover);

        //----------------------------------------------------------------------------------
        const wxRect rectLine(rect.x, rect.y + rect.height - dipToWxsize(1), rect.width, dipToWxsize(1));
        clearArea(dc, rectLine, row == pdi.groupLastRow - 1 /*last group item*/ ?
                  getColorGridLine() : getDefaultBackgroundColorAlternating(pdi.groupIdx % 2 != 0));
    }

    enum class HoverAreaCenter //each cell can be divided into four blocks concerning mouse selections
    {
        checkbox,
        dirLeft,
        dirNone,
        dirRight
    };

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover) override
    {
        if (const FileView::PathDrawInfo pdi = getDataView().getDrawInfo(row);
            pdi.fsObj)
        {
            auto drawHighlightBackground = [&](const wxColor& col)
            {
                if ((!enabled || !selected) && pdi.fsObj->isActive()) //coordinate with renderRowBackgound()!
                {
                    wxRect rectBack = rect;
                    if (row == pdi.groupLastRow - 1 /*last group item*/) //preserve the group separation line!
                        rectBack.height -= dipToWxsize(1);

                    clearArea(dc, rectBack, col);
                }
            };

            switch (static_cast<ColumnTypeCenter>(colType))
            {
                case ColumnTypeCenter::checkbox:
                {
                    const bool drawMouseHover = static_cast<HoverAreaCenter>(rowHover) == HoverAreaCenter::checkbox;

                    wxImage icon = loadImage(pdi.fsObj->isActive() ?
                                             (drawMouseHover ? "checkbox_true_hover"  : "checkbox_true") :
                                             (drawMouseHover ? "checkbox_false_hover" : "checkbox_false"));
                    if (!enabled)
                        icon = icon.ConvertToDisabled();

                    drawBitmapRtlNoMirror(dc, icon, rect, wxALIGN_CENTER);
                }
                break;

                case ColumnTypeCenter::difference:
                {
                    if (getViewType() == GridViewType::difference)
                        drawHighlightBackground(getBackGroundColorCmpDifference(pdi.fsObj->getCategory()));

                    wxRect rectTmp = rect;
                    {
                        //draw notch on left side
                        if (notch_.GetHeight() != wxsizeToScreen(rectTmp.height))
                            notch_ = notch_.Scale(notch_.GetWidth(), wxsizeToScreen(rectTmp.height));

                        //wxWidgets screws up again and has wxALIGN_RIGHT off by one pixel! -> use wxALIGN_LEFT instead
                        const wxRect rectNotch(rectTmp.x + rectTmp.width - screenToWxsize(notch_.GetWidth()), rectTmp.y,
                                               screenToWxsize(notch_.GetWidth()), rectTmp.height);
                        drawBitmapRtlNoMirror(dc, notch_, rectNotch, wxALIGN_LEFT);
                        rectTmp.width -= screenToWxsize(notch_.GetWidth());
                    }

                    auto drawIcon = [&](wxImage icon, int alignment)
                    {
                        if (!enabled)
                            icon = icon.ConvertToDisabled();

                        drawBitmapRtlMirror(dc, icon, rectTmp, alignment, renderBufCmp_);
                    };

                    if (getViewType() == GridViewType::difference)
                        drawIcon(getCmpResultImage(pdi.fsObj->getCategory()), wxALIGN_CENTER);
                    else if (pdi.fsObj->getCategory() != FILE_EQUAL) //don't show = in both middle columns
                        drawIcon(greyScale(getCmpResultImage(pdi.fsObj->getCategory())), wxALIGN_CENTER);
                }
                break;

                case ColumnTypeCenter::action:
                {
                    if (getViewType() == GridViewType::action)
                        drawHighlightBackground(getBackGroundColorSyncAction(pdi.fsObj->getSyncOperation()));

                    auto drawIcon = [&](wxImage icon, int alignment)
                    {
                        if (!enabled)
                            icon = icon.ConvertToDisabled();

                        drawBitmapRtlMirror(dc, icon, rect, alignment, renderBufSync_);
                    };

                    //synchronization preview
                    const auto rowHoverCenter = rowHover == HoverArea::none ? HoverAreaCenter::checkbox : static_cast<HoverAreaCenter>(rowHover);
                    switch (rowHoverCenter)
                    {
                        case HoverAreaCenter::dirLeft:
                            drawIcon(getSyncOpImage(pdi.fsObj->testSyncOperation(SyncDirection::left)), wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
                            break;
                        case HoverAreaCenter::dirNone:
                            drawIcon(getSyncOpImage(pdi.fsObj->testSyncOperation(SyncDirection::none)), wxALIGN_CENTER);
                            break;
                        case HoverAreaCenter::dirRight:
                            drawIcon(getSyncOpImage(pdi.fsObj->testSyncOperation(SyncDirection::right)), wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
                            break;
                        case HoverAreaCenter::checkbox:
                            if (getViewType() == GridViewType::action)
                                drawIcon(getSyncOpImage(pdi.fsObj->getSyncOperation()), wxALIGN_CENTER);
                            else if (pdi.fsObj->getSyncOperation() != SO_EQUAL) //don't show = in both middle columns
                                drawIcon(greyScale(getSyncOpImage(pdi.fsObj->getSyncOperation())), wxALIGN_CENTER);
                            break;
                    }
                }
                break;
            }
        }
    }

    HoverArea getMouseHover(wxDC& dc, size_t row, ColumnType colType, int cellRelativePosX, int cellWidth) override
    {
        if (const FileSystemObject* const fsObj = getFsObject(row))
            switch (static_cast<ColumnTypeCenter>(colType))
            {
                case ColumnTypeCenter::checkbox:
                case ColumnTypeCenter::difference:
                    return static_cast<HoverArea>(HoverAreaCenter::checkbox);

                case ColumnTypeCenter::action:
                    if (fsObj->getSyncOperation() == SO_EQUAL) //in sync-preview equal files shall be treated like a checkbox
                        return static_cast<HoverArea>(HoverAreaCenter::checkbox);
                    /* cell: ------------------------
                             | left | middle | right|
                             ------------------------    */
                    if (0 <= cellRelativePosX)
                    {
                        if (cellRelativePosX < cellWidth / 3)
                            return static_cast<HoverArea>(HoverAreaCenter::dirLeft);
                        else if (cellRelativePosX < 2 * cellWidth / 3)
                            return static_cast<HoverArea>(HoverAreaCenter::dirNone);
                        else if  (cellRelativePosX < cellWidth)
                            return static_cast<HoverArea>(HoverAreaCenter::dirRight);
                    }
                    break;
            }
        return HoverArea::none;
    }

    std::wstring getColumnLabel(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeCenter>(colType))
        {
            case ColumnTypeCenter::checkbox:
                break;
            case ColumnTypeCenter::difference:
                return _("Difference");
            case ColumnTypeCenter::action:
                return _("Action");
        }
        return std::wstring();
    }

    std::wstring getToolTip(ColumnType colType) const override { return getColumnLabel(colType) + L" (F11)"; }

    void renderColumnLabel(wxDC& dc, const wxRect& rect, ColumnType colType, bool enabled, bool highlighted) override
    {
        const auto colTypeCenter = static_cast<ColumnTypeCenter>(colType);

        const wxRect rectInner = drawColumnLabelBackground(dc, rect, highlighted && colTypeCenter != ColumnTypeCenter::checkbox);

        wxImage colIcon;
        switch (colTypeCenter)
        {
            case ColumnTypeCenter::checkbox:
                break;

            case ColumnTypeCenter::difference:
                colIcon = greyScaleIfDisabled(loadImage("compare", dipToScreen(getMenuIconDipSize())), getViewType() == GridViewType::difference);
                break;

            case ColumnTypeCenter::action:
                colIcon = greyScaleIfDisabled(loadImage("start_sync", dipToScreen(getMenuIconDipSize())), getViewType() == GridViewType::action);
                break;
        }

        if (colIcon.IsOk())
            drawBitmapRtlNoMirror(dc, enabled ? colIcon : colIcon.ConvertToDisabled(), rectInner, wxALIGN_CENTER);

        //draw sort marker
        if (auto sortInfo = getDataView().getSortConfig())
            if (const ColumnTypeCenter* sortType = std::get_if<ColumnTypeCenter>(&sortInfo->sortCol))
                if (*sortType == colTypeCenter)
                {
                    const int gapLeft = (rectInner.width + screenToWxsize(colIcon.GetWidth())) / 2;
                    wxRect rectRemain = rectInner;
                    rectRemain.x     += gapLeft;
                    rectRemain.width -= gapLeft;

                    const wxImage sortMarker = loadImage(sortInfo->ascending ? "sort_ascending" : "sort_descending");
                    drawBitmapRtlNoMirror(dc, enabled ? sortMarker : sortMarker.ConvertToDisabled(), rectRemain, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
                }
    }

    //*INDENT-OFF*
    void showToolTip(size_t row, ColumnTypeCenter colType, wxPoint posScreen)
    {
        if (const FileSystemObject* fsObj = getFsObject(row))
        {
            switch (colType)
            {
                case ColumnTypeCenter::checkbox:
                case ColumnTypeCenter::difference:
                {
                    const char* imageName = [&]
                    {
                        switch (fsObj->getCategory())
                        {
                            case FILE_RENAMED:  //similar to both "equal" and "conflict"
                            case FILE_EQUAL:             return "cat_equal";
                            case FILE_LEFT_ONLY:         return "cat_left_only";
                            case FILE_RIGHT_ONLY:        return "cat_right_only";
                            case FILE_LEFT_NEWER:        return "cat_left_newer";
                            case FILE_RIGHT_NEWER:       return "cat_right_newer";
                            case FILE_DIFFERENT_CONTENT: return "cat_different";
                            case FILE_TIME_INVALID: 
                            case FILE_CONFLICT:          return "cat_conflict";
                        }
                        assert(false);
                        return "";
                    }();
                    const auto& img = mirrorIfRtl(loadImage(imageName));
                    toolTip_.show(getCategoryDescription(*fsObj), posScreen, &img);
                }
                break;

                case ColumnTypeCenter::action:
                {
                    const char* imageName = [&]
                    {
                        switch (fsObj->getSyncOperation())
                        {
                            case SO_CREATE_LEFT:         return "so_create_left";
                            case SO_CREATE_RIGHT:        return "so_create_right";
                            case SO_DELETE_LEFT:         return "so_delete_left";
                            case SO_DELETE_RIGHT:        return "so_delete_right";
                            case SO_MOVE_LEFT_FROM:      return "so_move_left_source";
                            case SO_MOVE_LEFT_TO:        return "so_move_left_target";
                            case SO_MOVE_RIGHT_FROM:     return "so_move_right_source";
                            case SO_MOVE_RIGHT_TO:       return "so_move_right_target";
                            case SO_OVERWRITE_LEFT:      return "so_update_left";
                            case SO_OVERWRITE_RIGHT:     return "so_update_right";
                            case SO_RENAME_LEFT:         return "so_move_left";
                            case SO_RENAME_RIGHT:        return "so_move_right";
                            case SO_DO_NOTHING:          return "so_none";
                            case SO_EQUAL:               return "cat_equal";
                            case SO_UNRESOLVED_CONFLICT: return "cat_conflict";
                        };
                        assert(false);
                        return "";
                    }();
                    const auto& img = mirrorIfRtl(loadImage(imageName));
                    toolTip_.show(getSyncOpDescription(*fsObj), posScreen, &img);
                }
                break;
            }
        }
        else
            toolTip_.hide(); //if invalid row...
    }
    //*INDENT-ON*

    bool selectionInProgress_ = false;

    std::optional<wxBitmap> renderBufCmp_; //avoid costs of recreating this temporary variable
    std::optional<wxBitmap> renderBufSync_;
    Tooltip toolTip_;
    wxImage notch_ = loadImage("notch");
};

//########################################################################################################

wxDEFINE_EVENT(EVENT_ALIGN_SCROLLBARS, wxCommandEvent);


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
        gridL_.Bind(EVENT_GRID_COL_RESIZE, [this](GridColumnResizeEvent& event) { onResizeColumn(event, gridL_, gridR_); });
        gridR_.Bind(EVENT_GRID_COL_RESIZE, [this](GridColumnResizeEvent& event) { onResizeColumn(event, gridR_, gridL_); });

        gridL_.Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) { onKeyDown(event, gridL_); });
        gridC_.Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) { onKeyDown(event, gridC_); });
        gridR_.Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) { onKeyDown(event, gridR_); });

        gridC_.getMainWin().Bind(wxEVT_MOTION,       [this](wxMouseEvent& event) { onCenterMouseMovement(event); });
        gridC_.getMainWin().Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& event) { onCenterMouseLeave   (event); });

        gridC_.Bind(EVENT_GRID_MOUSE_LEFT_DOWN, [this](GridClickEvent&  event) { onCenterSelectBegin(event); });
        gridC_.Bind(EVENT_GRID_SELECT_RANGE,    [this](GridSelectEvent& event) { onCenterSelectEnd  (event); });

        gridL_.Bind(EVENT_GRID_MOUSE_LEFT_DOWN, [this](GridClickEvent& event) { onGridClickRim(event, gridL_); });
        gridR_.Bind(EVENT_GRID_MOUSE_LEFT_DOWN, [this](GridClickEvent& event) { onGridClickRim(event, gridR_); });

        //clear selection of other grid when selecting on
        gridL_.Bind(EVENT_GRID_MOUSE_LEFT_DOWN,  [this]( GridClickEvent& event) { onGridLeftClick(event, gridR_); }); //clear immediately,
        gridL_.Bind(EVENT_GRID_MOUSE_RIGHT_DOWN, [this]( GridClickEvent& event) { onGridRightClick(event, gridR_, gridL_); }); //don't wait for GridSelectEvent
        gridL_.Bind(EVENT_GRID_SELECT_RANGE,     [this](GridSelectEvent& event) { onGridSelection(event, gridR_); });

        gridR_.Bind(EVENT_GRID_MOUSE_LEFT_DOWN,  [this]( GridClickEvent& event) { onGridLeftClick(event, gridL_); });
        gridR_.Bind(EVENT_GRID_MOUSE_RIGHT_DOWN, [this]( GridClickEvent& event) { onGridRightClick(event, gridL_, gridR_); });
        gridR_.Bind(EVENT_GRID_SELECT_RANGE,     [this](GridSelectEvent& event) { onGridSelection(event, gridL_); });

        //parallel grid scrolling: do NOT use DoPrepareDC() to align grids! GDI resource leak! Use regular paint event instead:
        gridL_.getMainWin().Bind(wxEVT_PAINT, [this](wxPaintEvent& event) { onPaintGrid(gridL_); event.Skip(); });
        gridC_.getMainWin().Bind(wxEVT_PAINT, [this](wxPaintEvent& event) { onPaintGrid(gridC_); event.Skip(); });
        gridR_.getMainWin().Bind(wxEVT_PAINT, [this](wxPaintEvent& event) { onPaintGrid(gridR_); event.Skip(); });

        //-----------------------------------------------------------------------------------------------------
        //scroll master event handling: connect LAST, so that scrollMaster_ is set BEFORE other event handling!
        //-----------------------------------------------------------------------------------------------------
        auto connectGridAccess = [&](Grid& grid, std::function<void(wxEvent& event)> handler)
        {
            grid.Bind(wxEVT_SCROLLWIN_TOP,        handler);
            grid.Bind(wxEVT_SCROLLWIN_BOTTOM,     handler);
            grid.Bind(wxEVT_SCROLLWIN_LINEUP,     handler);
            grid.Bind(wxEVT_SCROLLWIN_LINEDOWN,   handler);
            grid.Bind(wxEVT_SCROLLWIN_PAGEUP,     handler);
            grid.Bind(wxEVT_SCROLLWIN_PAGEDOWN,   handler);
            grid.Bind(wxEVT_SCROLLWIN_THUMBTRACK, handler);
            //wxEVT_KILL_FOCUS -> there's no need to reset "scrollMaster"
            //wxEVT_SET_FOCUS -> not good enough:
            //e.g.: left grid has input, right grid is "scrollMaster" due to dragging scroll thumb via mouse.
            //=> Next keyboard input on left does *not* emit focus change event, but still "scrollMaster" needs to change
            //=> hook keyboard input instead of focus event:
            grid.getMainWin().Bind(wxEVT_CHAR,     handler);
            grid.Bind(wxEVT_KEY_DOWN, handler);
            //grid.getMainWin().Bind(wxEVT_KEY_UP, handler); -> superfluous?

            grid.getMainWin().Bind(wxEVT_LEFT_DOWN,   handler);
            grid.getMainWin().Bind(wxEVT_LEFT_DCLICK, handler);
            grid.getMainWin().Bind(wxEVT_RIGHT_DOWN,  handler);
            grid.getMainWin().Bind(wxEVT_MOUSEWHEEL,  handler);
        };
        connectGridAccess(gridL_, [this](wxEvent& event) { scrollMaster_ = &gridL_; event.Skip(); }); //
        connectGridAccess(gridC_, [this](wxEvent& event) { scrollMaster_ = &gridC_; event.Skip(); }); //connect *after* onKeyDown() in order to receive callback *before*!!!
        connectGridAccess(gridR_, [this](wxEvent& event) { scrollMaster_ = &gridR_; event.Skip(); }); //

        Bind(EVENT_ALIGN_SCROLLBARS, [this](wxCommandEvent& event) { onAlignScrollBars(event); });
    }

    ~GridEventManager()
    {
        //assert(!scrollbarUpdatePending_); => false-positives: e.g. start ffs, right-click on grid, close dialog by clicking X
    }

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
            if (event.mouseClick_)
                provCenter_.onSelectEnd(event.rowFirst_, event.rowLast_, event.mouseClick_->hoverArea_, event.mouseClick_->row_);
            else
                provCenter_.onSelectEnd(event.rowFirst_, event.rowLast_, HoverArea::none, -1);
        }
        event.Skip();
    }

    void onCenterMouseMovement(wxMouseEvent& event)
    {
        provCenter_.evalMouseMovement(event.GetPosition());
        event.Skip();
    }

    void onCenterMouseLeave(wxMouseEvent& event)
    {
        provCenter_.onMouseLeave();
        event.Skip();
    }

    void onGridClickRim(GridClickEvent& event, Grid& grid)
    {
        if (static_cast<HoverAreaGroup>(event.hoverArea_) == HoverAreaGroup::groupName)
            if (const FileView::PathDrawInfo pdi = provCenter_.getDataView().getDrawInfo(event.row_);
                pdi.fsObj)
            {
                const ptrdiff_t topRowOld = grid.getRowAtWinPos(0);
                grid.makeRowVisible(pdi.groupFirstRow);
                const ptrdiff_t topRowNew = grid.getRowAtWinPos(0);

                if (topRowNew != topRowOld) //=> grid was scrolled: prevent AddPendingEvent() recursion!
                {
                    assert(topRowNew == makeSigned(pdi.groupFirstRow));
                    assert(topRowNew == grid.getRowAtWinPos((event.mousePos_ - grid.getMainWin().GetPosition()).y));
                    //don't waste a click: simulate start of new selection at Grid::MainWin-relative position (0/0):
                    grid.getMainWin().GetEventHandler()->AddPendingEvent(wxMouseEvent(wxEVT_LEFT_DOWN));
                    return;
                }
            }
        event.Skip();
    }

    void onGridLeftClick(GridClickEvent& event, Grid& gridOther)
    {
        //see grid.cpp Grid::MainWin::onMouseDown():
        if (!wxGetKeyState(WXK_CONTROL) && !wxGetKeyState(WXK_SHIFT)) //clear other grid unless user is holding CTRL, or SHIFT
            gridOther.clearSelection(GridEventPolicy::deny); //don't emit event, prevent recursion!
        event.Skip();
    }

    void onGridRightClick(GridClickEvent& event, Grid& gridOther, Grid& gridThis)
    {
        const std::vector<size_t>& selectedRows = gridThis.getSelectedRows();
        const bool rowSelected = std::find(selectedRows.begin(), selectedRows.end(), makeUnsigned(event.row_)) != selectedRows.end();

        //clear other grid unless GridContextMenuEvent is about to happen, or user is holding CTRL, or SHIFT
        if (!rowSelected && !wxGetKeyState(WXK_CONTROL) && !wxGetKeyState(WXK_SHIFT))
            gridOther.clearSelection(GridEventPolicy::deny); //don't emit event, prevent recursion!
        event.Skip();
    }

    void onGridSelection(GridSelectEvent& event, Grid& gridOther)
    {
        if (!event.mouseClick_ && !wxGetKeyState(WXK_SHIFT)) //clear other grid during keyboard selection, unless user is holding SHIFT
            gridOther.clearSelection(GridEventPolicy::deny); //don't emit event, prevent recursion!
        event.Skip();
    }

    void onKeyDown(wxKeyEvent& event, const Grid& grid)
    {
        int keyCode = event.GetKeyCode();
        if (grid.GetLayoutDirection() == wxLayout_RightToLeft)
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
                    gridL_.setGridCursor(row, GridEventPolicy::allow);
                    gridL_.SetFocus();
                    //since key event is likely originating from right grid, we need to set scrollMaster manually!
                    scrollMaster_ = &gridL_; //onKeyDown is called *after* onGridAccessL()!
                    return; //swallow event

                case WXK_RIGHT:
                case WXK_NUMPAD_RIGHT:
                    gridR_.setGridCursor(row, GridEventPolicy::allow);
                    gridR_.SetFocus();
                    scrollMaster_ = &gridR_;
                    return; //swallow event
            }

        event.Skip();
    }

    void onResizeColumn(GridColumnResizeEvent& event, const Grid& grid, Grid& gridOther)
    {
        //find stretch factor of resized column: type is unique due to makeConsistent()!
        std::vector<Grid::ColAttributes> cfgSrc = grid.getColumnConfig();
        auto it = std::find_if(cfgSrc.begin(), cfgSrc.end(), [&](Grid::ColAttributes& ca) { return ca.type == event.colType_; });
        if (it == cfgSrc.end())
            return;
        const int stretchSrc = it->stretch;

        //we do not propagate resizings on stretched columns to the other side: awkward user experience
        if (stretchSrc > 0)
            return;

        //apply resized offset to other side, but only if stretch factors match!
        std::vector<Grid::ColAttributes> cfgTrg = gridOther.getColumnConfig();
        for (Grid::ColAttributes& ca : cfgTrg)
            if (ca.type == event.colType_ && ca.stretch == stretchSrc)
                ca.offset = event.offset_;
        gridOther.setColumnConfig(cfgTrg);
    }

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
        if (lead == &grid)
        {
            auto scroll = [](Grid& target, int y) //support polling
            {
                //scroll vertically only - scrolling horizontally becomes annoying if left and right sides have different widths;
                //e.g. h-scroll on left would be undone when scrolling vertically on right which doesn't have a h-scrollbar
                int yOld = 0;
                target.GetViewStart(nullptr, &yOld);
                if (yOld != y)
                    target.Scroll(-1, y); //empirical test Windows/Ubuntu: this call does NOT trigger a wxEVT_SCROLLWIN event, which would incorrectly set "scrollMaster" to "&target"!
                //CAVEAT: wxScrolledWindow::Scroll() internally calls wxWindow::Update(), leading to immediate WM_PAINT handling in the target grid!
                //        an this while we're still in our WM_PAINT handler! => no recursion, fine (hopefully)
            };
            int y = 0;
            lead->GetViewStart(nullptr, &y);
            scroll(*follow1, y);
            scroll(*follow2, y);
        }

        //harmonize placement of horizontal scrollbar to avoid grids getting out of sync!
        //since this affects the grid that is currently repainted as well, we do work asynchronously!
        if (!scrollbarUpdatePending_) //send one async event at most, else they may accumulate and create perf issues, see grid.cpp
        {
            scrollbarUpdatePending_ = true;
            wxCommandEvent alignEvent(EVENT_ALIGN_SCROLLBARS);
            AddPendingEvent(alignEvent); //waits until next idle event - may take up to a second if the app is busy on wxGTK!
        }
    }

    void onAlignScrollBars(wxEvent& event)
    {
        assert(scrollbarUpdatePending_);
        ZEN_ON_SCOPE_EXIT(scrollbarUpdatePending_ = false);

        auto needsHorizontalScrollbars = [](const Grid& grid) -> bool
        {
            const wxWindow& mainWin = grid.getMainWin();
            return mainWin.GetVirtualSize().GetWidth() > mainWin.GetClientSize().GetWidth();
            //assuming Grid::updateWindowSizes() does its job well, this should suffice!
            //CAVEAT: if horizontal and vertical scrollbar are circular dependent from each other
            //(h-scrollbar is shown due to v-scrollbar consuming horizontal width, etc...)
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
    auto sharedComp = makeSharedRef<SharedComponents>();

    auto provLeft_   = std::make_shared<GridDataLeft  >(gridLeft,   sharedComp);
    auto provCenter_ = std::make_shared<GridDataCenter>(gridCenter, sharedComp);
    auto provRight_  = std::make_shared<GridDataRight >(gridRight,  sharedComp);

    sharedComp.ref().evtMgr = std::make_unique<GridEventManager>(gridLeft, gridCenter, gridRight, *provCenter_);

    gridLeft  .setDataProvider(provLeft_);   //data providers reference grid =>
    gridCenter.setDataProvider(provCenter_); //ownership must belong *exclusively* to grid!
    gridRight .setDataProvider(provRight_);

    gridCenter.enableColumnMove  (false);
    gridCenter.enableColumnResize(false);

    gridCenter.showRowLabel(false);
    gridRight .showRowLabel(false);

    //gridLeft  .showScrollBars(Grid::SB_SHOW_AUTOMATIC, Grid::SB_SHOW_NEVER); -> redundant: configuration happens in GridEventManager::onAlignScrollBars()
    //gridCenter.showScrollBars(Grid::SB_SHOW_NEVER,     Grid::SB_SHOW_NEVER);

    const int widthCheckbox   = screenToWxsize(    loadImage("checkbox_true").GetWidth() + dipToScreen(3));
    const int widthDifference = screenToWxsize(2 * loadImage("sort_ascending").GetWidth() + loadImage("cat_left_only_sicon").GetWidth() + loadImage("notch").GetWidth());
    const int widthAction     = screenToWxsize(3 * loadImage("so_create_left_sicon").GetWidth());
    gridCenter.SetSize(widthDifference + widthCheckbox + widthAction, -1);

    gridCenter.setColumnConfig(
    {
        {static_cast<ColumnType>(ColumnTypeCenter::checkbox),   widthCheckbox,   0, true},
        {static_cast<ColumnType>(ColumnTypeCenter::difference), widthDifference, 0, true},
        {static_cast<ColumnType>(ColumnTypeCenter::action),     widthAction,     0, true},
    });
}


void filegrid::setData(Grid& grid, FolderComparison& folderCmp)
{
    if (auto* prov = dynamic_cast<GridDataBase*>(grid.getDataProvider()))
        return prov->setData(folderCmp);

    throw std::runtime_error("filegrid was not initialized! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));
}


FileView& filegrid::getDataView(Grid& grid)
{
    if (auto* prov = dynamic_cast<GridDataBase*>(grid.getDataProvider()))
        return prov->getDataView();

    throw std::runtime_error("filegrid was not initialized! " + std::string(__FILE__) + ':' + numberTo<std::string>(__LINE__));
}


namespace
{
//resolve circular linker dependencies
void IconUpdater::loadIconsAsynchronously(wxEvent& event) //loads all (not yet) drawn icons
{
    std::vector<std::pair<ptrdiff_t, AbstractPath>> prefetchLoad;
    provLeft_ .getUnbufferedIconsForPreload(prefetchLoad);
    provRight_.getUnbufferedIconsForPreload(prefetchLoad);

    //make sure least-important prefetch rows are inserted first into workload (=> processed last)
    //priority index nicely considers both grids at the same time!
    std::sort(prefetchLoad.begin(), prefetchLoad.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

    //last inserted items are processed first in icon buffer:
    std::vector<AbstractPath> newLoad;
    for (const auto& [priority, filePath] : prefetchLoad)
        newLoad.push_back(filePath);

    provRight_.updateNewAndGetUnbufferedIcons(newLoad);
    provLeft_ .updateNewAndGetUnbufferedIcons(newLoad);

    iconBuffer_.setWorkload(newLoad);

    if (newLoad.empty()) //let's only pay for IconUpdater while needed
        stop();
}
}


void filegrid::setupIcons(Grid& gridLeft, Grid& gridCenter, Grid& gridRight, bool showFileIcons, IconBuffer::IconSize sz)
{
    auto* provLeft  = dynamic_cast<GridDataLeft*>(gridLeft .getDataProvider());
    auto* provRight = dynamic_cast<GridDataRight*>(gridRight.getDataProvider());

    if (provLeft && provRight)
    {
        auto iconMgr = makeSharedRef<IconManager>(*provLeft, *provRight, sz, showFileIcons);
        provLeft ->setIconManager(iconMgr);

        const int newRowHeight = std::max(iconMgr.ref().getIconWxsize(), gridLeft.getMainWin().GetCharHeight()) + dipToWxsize(1); //add some space

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
                                   zen::Grid& gridRight,
                                   std::unordered_set<const FileSystemObject*>&& markedFilesAndLinks,
                                   std::unordered_set<const ContainerObject*>&& markedContainer)
{
    if (auto grid = dynamic_cast<GridDataBase*>(gridLeft.getDataProvider()))
        grid->setNavigationMarker(std::move(markedFilesAndLinks), std::move(markedContainer));
    else
        assert(false);
    gridLeft .Refresh();
    gridRight.Refresh();
}


void filegrid::setViewType(Grid& gridCenter, GridViewType vt)
{
    if (auto prov = dynamic_cast<GridDataBase*>(gridCenter.getDataProvider()))
        prov->setViewType(vt);
    else
        assert(false);
    gridCenter.Refresh();
}


wxImage fff::getSyncOpImage(SyncOperation syncOp)
{
    switch (syncOp) //evaluate comparison result and sync direction
    {
        //*INDENT-OFF*
        case SO_CREATE_LEFT:     return loadImage("so_create_left_sicon");
        case SO_CREATE_RIGHT:    return loadImage("so_create_right_sicon");
        case SO_DELETE_LEFT:         return loadImage("so_delete_left_sicon");
        case SO_DELETE_RIGHT:        return loadImage("so_delete_right_sicon");
        case SO_MOVE_LEFT_FROM:      return loadImage("so_move_left_source_sicon");
        case SO_MOVE_LEFT_TO:        return loadImage("so_move_left_target_sicon");
        case SO_MOVE_RIGHT_FROM:     return loadImage("so_move_right_source_sicon");
        case SO_MOVE_RIGHT_TO:       return loadImage("so_move_right_target_sicon");
        case SO_OVERWRITE_LEFT:      return loadImage("so_update_left_sicon");
        case SO_OVERWRITE_RIGHT:     return loadImage("so_update_right_sicon");
        case SO_RENAME_LEFT:         return loadImage("so_move_left_sicon");
        case SO_RENAME_RIGHT:        return loadImage("so_move_right_sicon");
        case SO_DO_NOTHING:          return loadImage("so_none_sicon");
        case SO_EQUAL:               return loadImage("cat_equal_sicon");
        case SO_UNRESOLVED_CONFLICT: return loadImage("cat_conflict_small");
        //*INDENT-ON*
    }
    assert(false);
    return wxNullImage;
}


wxImage fff::getCmpResultImage(CompareFileResult cmpResult)
{
    switch (cmpResult)
    {
        //*INDENT-OFF*
        case FILE_RENAMED: //similar to both "equal" and "conflict"
        case FILE_EQUAL:             return loadImage("cat_equal_sicon"); 
        case FILE_LEFT_ONLY:         return loadImage("cat_left_only_sicon");
        case FILE_RIGHT_ONLY:        return loadImage("cat_right_only_sicon");
        case FILE_LEFT_NEWER:        return loadImage("cat_left_newer_sicon");
        case FILE_RIGHT_NEWER:       return loadImage("cat_right_newer_sicon");
        case FILE_DIFFERENT_CONTENT: return loadImage("cat_different_sicon");
        case FILE_TIME_INVALID:
        case FILE_CONFLICT:          return loadImage("cat_conflict_small");
        //*INDENT-ON*
    }
    assert(false);
    return wxNullImage;
}

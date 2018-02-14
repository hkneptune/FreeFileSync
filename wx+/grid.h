// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef GRID_H_834702134831734869987
#define GRID_H_834702134831734869987

#include <memory>
#include <numeric>
#include <vector>
#include <wx/scrolwin.h>
#include <zen/basic_math.h>
#include <zen/optional.h>


//a user-friendly, extensible and high-performance grid control
namespace zen
{
enum class ColumnType { NONE = -1 }; //user-defiend column type
enum class HoverArea  { NONE = -1 }; //user-defined area for mouse selections for a given row (may span multiple columns or split a single column into multiple areas)

//------------------------ events ------------------------------------------------
extern const wxEventType EVENT_GRID_MOUSE_LEFT_DOUBLE; //
extern const wxEventType EVENT_GRID_MOUSE_LEFT_DOWN;   //
extern const wxEventType EVENT_GRID_MOUSE_LEFT_UP;     //generates: GridClickEvent
extern const wxEventType EVENT_GRID_MOUSE_RIGHT_DOWN;  //
extern const wxEventType EVENT_GRID_MOUSE_RIGHT_UP;    //

extern const wxEventType EVENT_GRID_SELECT_RANGE; //generates: GridSelectEvent
//NOTE: neither first nor second row need to match EVENT_GRID_MOUSE_LEFT_DOWN/EVENT_GRID_MOUSE_LEFT_UP: user holding SHIFT; moving out of window...

extern const wxEventType EVENT_GRID_COL_LABEL_MOUSE_LEFT;  //generates: GridLabelClickEvent
extern const wxEventType EVENT_GRID_COL_LABEL_MOUSE_RIGHT; //
extern const wxEventType EVENT_GRID_COL_RESIZE; //generates: GridColumnResizeEvent

//example: wnd.Connect(EVENT_GRID_COL_LABEL_LEFT_CLICK, GridClickEventHandler(MyDlg::OnLeftClick), nullptr, this);

struct GridClickEvent : public wxMouseEvent
{
    GridClickEvent(wxEventType et, const wxMouseEvent& me, ptrdiff_t row, HoverArea hoverArea) :
        wxMouseEvent(me), row_(row), hoverArea_(hoverArea) { SetEventType(et); }
    GridClickEvent* Clone() const override { return new GridClickEvent(*this); }

    const ptrdiff_t row_; //-1 for invalid position, >= rowCount if out of range
    const HoverArea hoverArea_; //may be HoverArea::NONE
};

struct MouseSelect
{
    GridClickEvent click;
    bool complete = false; //false if this is a preliminary "clear range" event for mouse-down, before the actual selection has happened during mouse-up
};

struct GridSelectEvent : public wxCommandEvent
{
    GridSelectEvent(size_t rowFirst, size_t rowLast, bool positive, const MouseSelect* mouseSelect) :
        wxCommandEvent(EVENT_GRID_SELECT_RANGE), rowFirst_(rowFirst), rowLast_(rowLast), positive_(positive),
        mouseSelect_(mouseSelect ? *mouseSelect : Opt<MouseSelect>()) { assert(rowFirst <= rowLast); }
    GridSelectEvent* Clone() const override { return new GridSelectEvent(*this); }

    const size_t rowFirst_; //selected range: [rowFirst_, rowLast_)
    const size_t rowLast_;
    const bool   positive_; //"false" when clearing selection!
    const Opt<MouseSelect> mouseSelect_; //filled unless selection was performed via keyboard shortcuts
};

struct GridLabelClickEvent : public wxMouseEvent
{
    GridLabelClickEvent(wxEventType et, const wxMouseEvent& me, ColumnType colType) : wxMouseEvent(me), colType_(colType) { SetEventType(et); }
    GridLabelClickEvent* Clone() const override { return new GridLabelClickEvent(*this); }

    const ColumnType colType_; //may be ColumnType::NONE
};

struct GridColumnResizeEvent : public wxCommandEvent
{
    GridColumnResizeEvent(int offset, ColumnType colType) : wxCommandEvent(EVENT_GRID_COL_RESIZE), colType_(colType), offset_(offset) {}
    GridColumnResizeEvent* Clone() const override { return new GridColumnResizeEvent(*this); }

    const ColumnType colType_;
    const int        offset_;
};

using GridClickEventFunction        = void (wxEvtHandler::*)(GridClickEvent&);
using GridSelectEventFunction       = void (wxEvtHandler::*)(GridSelectEvent&);
using GridLabelClickEventFunction   = void (wxEvtHandler::*)(GridLabelClickEvent&);
using GridColumnResizeEventFunction = void (wxEvtHandler::*)(GridColumnResizeEvent&);

#define GridClickEventHandler(func)       (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(GridClickEventFunction,        &func)
#define GridSelectEventHandler(func)      (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(GridSelectEventFunction,       &func)
#define GridLabelClickEventHandler(func)  (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(GridLabelClickEventFunction,   &func)
#define GridColumnResizeEventHandler(func)(wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(GridColumnResizeEventFunction, &func)

//------------------------------------------------------------------------------------------------------------

class Grid;


class GridData
{
public:
    virtual ~GridData() {}

    virtual size_t getRowCount() const = 0;

    //cell area:
    virtual std::wstring getValue(size_t row, ColumnType colType) const = 0;
    virtual void         renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row,                     bool enabled, bool selected); //default implementation
    virtual void         renderCell        (wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover);
    virtual int          getBestSize       (wxDC& dc, size_t row, ColumnType colType); //must correspond to renderCell()!
    virtual std::wstring getToolTip        (size_t row, ColumnType colType) const { return std::wstring(); }
    virtual HoverArea    getRowMouseHover(size_t row, ColumnType colType, int cellRelativePosX, int cellWidth) { return HoverArea::NONE; }

    //label area:
    virtual std::wstring getColumnLabel(ColumnType colType) const = 0;
    virtual void renderColumnLabel(Grid& grid, wxDC& dc, const wxRect& rect, ColumnType colType, bool highlighted); //default implementation
    virtual std::wstring getToolTip(ColumnType colType) const { return std::wstring(); }

    static const int COLUMN_GAP_LEFT; //for left-aligned text

    //optional helper routines:
    static wxSize drawCellText      (wxDC& dc, const wxRect& rect, const std::wstring& text, int alignment = wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL); //returns text extent
    static wxRect drawCellBorder    (wxDC& dc, const wxRect& rect); //returns inner rectangle
    static void   drawCellBackground(wxDC& dc, const wxRect& rect, bool enabled, bool selected, const wxColor& backgroundColor);

    static wxRect drawColumnLabelBorder    (wxDC& dc, const wxRect& rect); //returns inner rectangle
    static void   drawColumnLabelBackground(wxDC& dc, const wxRect& rect, bool highlighted);
    static void   drawColumnLabelText      (wxDC& dc, const wxRect& rect, const std::wstring& text);
};


enum GridEventPolicy
{
    ALLOW_GRID_EVENT,
    DENY_GRID_EVENT
};


class Grid : public wxScrolledWindow
{
public:
    Grid(wxWindow* parent,
         wxWindowID id        = wxID_ANY,
         const wxPoint& pos   = wxDefaultPosition,
         const wxSize& size   = wxDefaultSize,
         long style           = wxTAB_TRAVERSAL | wxNO_BORDER,
         const wxString& name = wxPanelNameStr);

    size_t getRowCount() const;

    void setRowHeight(int height);

    struct ColAttributes
    {
        ColumnType type = ColumnType::NONE;
        //first, client width is partitioned according to all available stretch factors, then "offset_" is added
        //universal model: a non-stretched column has stretch factor 0 with the "offset" becoming identical to final width!
        int offset  = 0;
        int stretch = 0; //>= 0
        bool visible = false;
    };

    void setColumnConfig(const std::vector<ColAttributes>& attr); //set column count + widths
    std::vector<ColAttributes> getColumnConfig() const;

    void setDataProvider(const std::shared_ptr<GridData>& dataView) { dataView_ = dataView; }
    /**/  GridData* getDataProvider()       { return dataView_.get(); }
    const GridData* getDataProvider() const { return dataView_.get(); }
    //-----------------------------------------------------------------------------

    void setColumnLabelHeight(int height);
    void showRowLabel(bool visible);

    enum ScrollBarStatus
    {
        SB_SHOW_AUTOMATIC,
        SB_SHOW_ALWAYS,
        SB_SHOW_NEVER,
    };
    //alternative until wxScrollHelper::ShowScrollbars() becomes available in wxWidgets 2.9
    void showScrollBars(ScrollBarStatus horizontal, ScrollBarStatus vertical);

    std::vector<size_t> getSelectedRows() const { return selection_.get(); }
    void selectRow(size_t row, GridEventPolicy rangeEventPolicy);
    void selectAllRows (GridEventPolicy rangeEventPolicy); //turn off range selection event when calling this function in an event handler to avoid recursion!
    void clearSelection(GridEventPolicy rangeEventPolicy) { clearSelectionImpl(nullptr /*mouseSelect*/, rangeEventPolicy); } //

    void scrollDelta(int deltaX, int deltaY); //in scroll units

    wxWindow& getCornerWin  ();
    wxWindow& getRowLabelWin();
    wxWindow& getColLabelWin();
    wxWindow& getMainWin    ();
    const wxWindow& getMainWin() const;

    ptrdiff_t getRowAtPos(int posY) const; //return -1 for invalid position, >= rowCount if out of range; absolute coordinates!

    struct ColumnPosInfo
    {
        ColumnType colType   = ColumnType::NONE; //ColumnType::NONE no column at x position!
        int cellRelativePosX = 0;
        int colWidth         = 0;
    };
    ColumnPosInfo getColumnAtPos(int posX) const; //absolute position!

    void refreshCell(size_t row, ColumnType colType);

    void enableColumnMove  (bool value) { allowColumnMove_   = value; }
    void enableColumnResize(bool value) { allowColumnResize_ = value; }

    void setGridCursor(size_t row); //set + show + select cursor (+ emit range selection event)
    size_t getGridCursor() const; //returns row

    void scrollTo(size_t row);
    size_t getTopRow() const;

    void makeRowVisible(size_t row);

    void Refresh(bool eraseBackground = true, const wxRect* rect = nullptr) override;
    bool Enable(bool enable = true) override;

    //############################################################################################################

    static wxColor getColorSelectionGradientFrom();
    static wxColor getColorSelectionGradientTo();

private:
    void onPaintEvent(wxPaintEvent& event);
    void onEraseBackGround(wxEraseEvent& event) {} //[!]
    void onSizeEvent(wxSizeEvent& event) { updateWindowSizes(); event.Skip(); }
    void onKeyDown(wxKeyEvent& event);

    void updateWindowSizes(bool updateScrollbar = true);

    void selectWithCursor(ptrdiff_t row);

    void redirectRowLabelEvent(wxMouseEvent& event);

    wxSize GetSizeAvailableForScrollTarget(const wxSize& size) override; //required since wxWidgets 2.9 if SetTargetWindow() is used


    int getBestColumnSize(size_t col) const; //return -1 on error

    void autoSizeColumns(GridEventPolicy columnResizeEventPolicy);

    friend class GridData;
    class SubWindow;
    class CornerWin;
    class RowLabelWin;
    class ColLabelWin;
    class MainWin;

    class Selection
    {
    public:
        void init(size_t rowCount) { selected_.resize(rowCount); clear(); }

        size_t maxSize() const { return selected_.size(); }

        std::vector<size_t> get() const
        {
            std::vector<size_t> result;
            for (size_t row = 0; row < selected_.size(); ++row)
                if (selected_[row] != 0)
                    result.push_back(row);
            return result;
        }

        void selectRow(size_t row) { selectRange(row, row + 1,        true); }
        void selectAll          () { selectRange(0, selected_.size(), true); }
        void clear              () { selectRange(0, selected_.size(), false); }

        bool isSelected(size_t row) const { return row < selected_.size() ? selected_[row] != 0 : false; }

        void selectRange(size_t rowFirst, size_t rowLast, bool positive = true) //select [rowFirst, rowLast), trims if required!
        {
            if (rowFirst <= rowLast)
            {
                numeric::clamp<size_t>(rowFirst, 0, selected_.size());
                numeric::clamp<size_t>(rowLast,  0, selected_.size());

                std::fill(selected_.begin() + rowFirst, selected_.begin() + rowLast, positive);
            }
            else assert(false);
        }

    private:
        std::vector<char> selected_; //effectively a vector<bool> of size "number of rows"
    };

    struct VisibleColumn
    {
        ColumnType type = ColumnType::NONE;
        int offset  = 0;
        int stretch = 0; //>= 0
    };

    struct ColumnWidth
    {
        ColumnType type = ColumnType::NONE;
        int width = 0;
    };
    std::vector<ColumnWidth> getColWidths()                 const; //
    std::vector<ColumnWidth> getColWidths(int mainWinWidth) const; //evaluate stretched columns
    int                      getColWidthsSum(int mainWinWidth) const;
    std::vector<int> getColStretchedWidths(int clientWidth) const; //final width = (normalized) (stretchedWidth + offset)

    Opt<int> getColWidth(size_t col) const
    {
        const auto& widths = getColWidths();
        if (col < widths.size())
            return widths[col].width;
        return NoValue();
    }

    void setColumnWidth(int width, size_t col, GridEventPolicy columnResizeEventPolicy, bool notifyAsync = false);

    wxRect getColumnLabelArea(ColumnType colType) const; //returns empty rect if column not found

    void selectRangeAndNotify(ptrdiff_t rowFrom, ptrdiff_t rowTo, bool positive, const MouseSelect* mouseSelect); //select inclusive range [rowFrom, rowTo] + notify event!

    void clearSelectionImpl(const MouseSelect* mouseSelect, GridEventPolicy rangeEventPolicy);

    bool isSelected(size_t row) const { return selection_.isSelected(row); }

    struct ColAction
    {
        bool wantResize = false; //"!wantResize" means "move" or "single click"
        size_t col = 0;
    };
    Opt<ColAction> clientPosToColumnAction(const wxPoint& pos) const;
    void moveColumn(size_t colFrom, size_t colTo);
    ptrdiff_t clientPosToMoveTargetColumn(const wxPoint& pos) const; //return < 0 on error

    ColumnType colToType(size_t col) const; //returns ColumnType::NONE on error

    /*
    Visual layout:
        --------------------------------
        |CornerWin   | ColLabelWin     |
        |------------------------------|
        |RowLabelWin | MainWin         |
        |            |                 |
        --------------------------------
    */
    CornerWin*   cornerWin_;
    RowLabelWin* rowLabelWin_;
    ColLabelWin* colLabelWin_;
    MainWin*     mainWin_;

    ScrollBarStatus showScrollbarX_ = SB_SHOW_AUTOMATIC;
    ScrollBarStatus showScrollbarY_ = SB_SHOW_AUTOMATIC;

    int colLabelHeight_ = 0;
    bool drawRowLabel_ = true;

    std::shared_ptr<GridData> dataView_;
    Selection selection_;
    bool allowColumnMove_   = true;
    bool allowColumnResize_ = true;

    std::vector<VisibleColumn> visibleCols_; //individual widths, type and total column count
    std::vector<ColAttributes> oldColAttributes_; //visible + nonvisible columns; use for conversion in setColumnConfig()/getColumnConfig() *only*!

    size_t rowCountOld_ = 0; //at the time of last Grid::Refresh()
};

//------------------------------------------------------------------------------------------------------------

template <class ColAttrReal>
std::vector<ColAttrReal> makeConsistent(const std::vector<ColAttrReal>& attribs, const std::vector<ColAttrReal>& defaults)
{
    using ColTypeReal = decltype(ColAttrReal().type);
    std::vector<ColAttrReal> output;

    std::set<ColTypeReal> usedTypes; //remove duplicates
    auto appendUnique = [&](const std::vector<ColAttrReal>& attr)
    {
        std::copy_if(attr.begin(), attr.end(), std::back_inserter(output),
        [&](const ColAttrReal& a) { return usedTypes.insert(a.type).second; });
    };
    appendUnique(attribs);
    appendUnique(defaults); //make sure each type is existing!

    return output;
}


template <class ColAttrReal>
std::vector<Grid::ColAttributes> convertColAttributes(const std::vector<ColAttrReal>& attribs, const std::vector<ColAttrReal>& defaults)
{
    std::vector<Grid::ColAttributes> output;
    for (const ColAttrReal& ca : makeConsistent(attribs, defaults))
        output.push_back({ static_cast<ColumnType>(ca.type), ca.offset, ca.stretch, ca.visible });
    return output;
}


template <class ColAttrReal>
std::vector<ColAttrReal> convertColAttributes(const std::vector<Grid::ColAttributes>& attribs)
{
    using ColTypeReal = decltype(ColAttrReal().type);

    std::vector<ColAttrReal> output;
    for (const Grid::ColAttributes& ca : attribs)
        output.push_back({ static_cast<ColTypeReal>(ca.type), ca.offset, ca.stretch, ca.visible });
    return output;
}
}

#endif //GRID_H_834702134831734869987

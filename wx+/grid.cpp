// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "grid.h"
#include <cassert>
//#include <set>
#include <chrono>
#include <wx/settings.h>
//#include <wx/listbox.h>
#include <wx/tooltip.h>
#include <wx/timer.h>
//#include <wx/utils.h>
#include <zen/basic_math.h>
#include <zen/string_tools.h>
#include <zen/scope_guard.h>
#include <zen/utf.h>
#include <zen/zstring.h>
#include <zen/format_unit.h>
#include "dc.h"

    #include <gtk/gtk.h>

using namespace zen;


//let's NOT create wxWidgets objects statically:
wxColor GridData::getColorSelectionGradientFrom() { return {137, 172, 255}; } //blue: HSL: 158, 255, 196   HSV: 222, 0.46, 1
wxColor GridData::getColorSelectionGradientTo  () { return {225, 234, 255}; } //      HSL: 158, 255, 240   HSV: 222, 0.12, 1

int GridData::getColumnGapLeft() { return dipToWxsize(4); }


namespace
{
//------------------------------ Grid Parameters --------------------------------
inline wxColor getColorLabelText(bool enabled) { return wxSystemSettings::GetColour(enabled ? wxSYS_COLOUR_BTNTEXT : wxSYS_COLOUR_GRAYTEXT); }
inline wxColor getColorGridLine() { return wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW); }

inline wxColor getColorLabelGradientFrom() { return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW); }
inline wxColor getColorLabelGradientTo  () { return wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE); }

inline wxColor getColorLabelGradientFocusFrom() { return getColorLabelGradientFrom(); }
inline wxColor getColorLabelGradientFocusTo  () { return GridData::getColorSelectionGradientFrom(); }

const double MOUSE_DRAG_ACCELERATION_DIP = 1.5; //unit: [rows / (DIP * sec)] -> same value like Explorer!
const int DEFAULT_COL_LABEL_BORDER_DIP   =  6; //top + bottom border in addition to label height
const int COLUMN_MOVE_DELAY_DIP          =  5; //unit: [pixel] (from Explorer)
const int COLUMN_MIN_WIDTH_DIP           = 40; //only honored when resizing manually!
const int ROW_LABEL_BORDER_DIP           =  3;
const int COLUMN_RESIZE_TOLERANCE_DIP    =  6; //unit [pixel]
const int COLUMN_FILL_GAP_TOLERANCE_DIP  = 10; //enlarge column to fill full width when resizing
const int COLUMN_MOVE_MARKER_WIDTH_DIP   =  3;

const bool fillGapAfterColumns = true; //draw rows/column label to fill full window width; may become an instance variable some time?

/* IsEnabled() vs IsThisEnabled() since wxWidgets 2.9.5:

    void wxWindowBase::NotifyWindowOnEnableChange(), called from bool wxWindowBase::Enable(), fails to refresh
    child elements when disabling a IsTopLevel() dialog, e.g. when showing a modal dialog.
    The unfortunate effect on XP for using IsEnabled() when rendering the grid is that the user can move the modal dialog
    and *draw* with it on the background while the grid refreshes as disabled incrementally!

    => Don't use IsEnabled() since it considers the top level window, but a disabled top-level should NOT
    lead to child elements being rendered disabled!

    => IsThisEnabled() OTOH is too shallow and does not consider parent windows which are not top level.

    The perfect solution would be a bool renderAsEnabled() { return "IsEnabled() but ignore effects of showing a modal dialog"; }

    However "IsThisEnabled()" is good enough (same as old IsEnabled() on wxWidgets 2.8.12) and it avoids this pathetic behavior on XP.
    (Similar problem on Win 7: e.g. directly click sync button without comparing first)

    => 2018-07-30: roll our own:                            */
bool renderAsEnabled(wxWindow& win)
{
    if (win.IsTopLevel())
        return true;

    if (wxWindow* parent = win.GetParent())
        return win.IsThisEnabled() && renderAsEnabled(*parent);
    else
        return win.IsThisEnabled();
}
}

//----------------------------------------------------------------------------------------------------------------
namespace zen
{
wxDEFINE_EVENT(EVENT_GRID_MOUSE_LEFT_DOUBLE, GridClickEvent);
wxDEFINE_EVENT(EVENT_GRID_MOUSE_LEFT_DOWN,   GridClickEvent);
wxDEFINE_EVENT(EVENT_GRID_MOUSE_RIGHT_DOWN,  GridClickEvent);
wxDEFINE_EVENT(EVENT_GRID_SELECT_RANGE, GridSelectEvent);
wxDEFINE_EVENT(EVENT_GRID_COL_LABEL_MOUSE_LEFT,  GridLabelClickEvent);
wxDEFINE_EVENT(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, GridLabelClickEvent);
wxDEFINE_EVENT(EVENT_GRID_COL_RESIZE, GridColumnResizeEvent);
wxDEFINE_EVENT(EVENT_GRID_CONTEXT_MENU, GridContextMenuEvent);
}
//----------------------------------------------------------------------------------------------------------------

void GridData::renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected, HoverArea rowHover)
{
    if (enabled)
    {
        if (selected)
            dc.GradientFillLinear(rect, getColorSelectionGradientFrom(), getColorSelectionGradientTo(), wxEAST);
        //else: clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); -> already the default
    }
    else
        clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
}


void GridData::renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover)
{
    wxDCTextColourChanger textColor(dc);
    if (enabled && selected) //accessibility: always set *both* foreground AND background colors!
        textColor.Set(*wxBLACK);

    wxRect rectTmp = drawCellBorder(dc, rect);

    rectTmp.x     += getColumnGapLeft();
    rectTmp.width -= getColumnGapLeft();
    drawCellText(dc, rectTmp, getValue(row, colType));
}


int GridData::getBestSize(wxDC& dc, size_t row, ColumnType colType)
{
    return dc.GetTextExtent(getValue(row, colType)).GetWidth() + 2 * getColumnGapLeft() + dipToWxsize(1); //gap on left and right side + border
}


wxRect GridData::drawCellBorder(wxDC& dc, const wxRect& rect) //returns remaining rectangle
{
    clearArea(dc, {rect.x + rect.width - dipToWxsize(1), rect.y, dipToWxsize(1), rect.height}, getColorGridLine()); //right border
    clearArea(dc, {rect.x, rect.y + rect.height - dipToWxsize(1), rect.width, dipToWxsize(1)}, getColorGridLine()); //bottom border

    return {rect.x, rect.y, rect.width - dipToWxsize(1), rect.height - dipToWxsize(1)};
}


void GridData::drawCellText(wxDC& dc, const wxRect& rect, const std::wstring_view text, int alignment, const wxSize* textExtentHint)
{
    /* Performance Notes (Windows):
        - wxDC::GetTextExtent() is by far the most expensive call (20x more expensive than wxDC::DrawText())
        - wxDC::DrawLabel() is inefficiently implemented; internally calls: wxDC::GetMultiLineTextExtent(), wxDC::GetTextExtent(), wxDC::DrawText()
        - wxDC::GetMultiLineTextExtent() calls wxDC::GetTextExtent()
        - wxDC::DrawText also calls wxDC::GetTextExtent()!!
        => wxDC::DrawLabel() boils down to 3(!) calls to wxDC::GetTextExtent()!!!
        - wxDC::DrawLabel results in GetTextExtent() call even for empty strings!!!
        => NEVER EVER call wxDC::DrawLabel() cruft and directly call wxDC::DrawText()!                   */
    assert(!contains(text, L'\n'));
    if (rect.width <= 0 || rect.height <= 0 || text.empty())
        return;

    //truncate large texts and add ellipsis
    wxString textTrunc(&text[0], text.size());
    wxSize extentTrunc = textExtentHint ? *textExtentHint : dc.GetTextExtent(textTrunc);
    assert(!textExtentHint || *textExtentHint == dc.GetTextExtent(textTrunc)); //"trust, but verify" :>

    if (extentTrunc.GetWidth() > rect.width)
    {
        //unlike Windows Explorer, we truncate UTF-16 correctly: e.g. CJK-Ideograph encodes to TWO wchar_t: utfTo<std::wstring>("\xf0\xa4\xbd\x9c");
        size_t low  = 0;                   //number of Unicode chars!
        size_t high = unicodeLength(text); //
        if (high > 1)
            for (;;)
            {
                if (high - low <= 1)
                {
                    if (low == 0)
                    {
                        textTrunc   = ELLIPSIS;
                        extentTrunc = dc.GetTextExtent(ELLIPSIS);
                    }
                    break;
                }
                const size_t middle = (low + high) / 2; //=> never 0 when "high - low > 1"

                /*const*/ wxString candidate = getUnicodeSubstring<wxString>(text, 0, middle) + ELLIPSIS;
                const wxSize extentCand = dc.GetTextExtent(candidate); //perf: most expensive call of this routine!

                if (extentCand.GetWidth() <= rect.width)
                {
                    low = middle;
                    textTrunc   = std::move(candidate);
                    extentTrunc = extentCand;
                }
                else
                    high = middle;
            }
    }

    wxPoint pt = rect.GetTopLeft();
    if (alignment & wxALIGN_RIGHT) //note: wxALIGN_LEFT == 0!
        pt.x += rect.width - extentTrunc.GetWidth();
    else if (alignment & wxALIGN_CENTER_HORIZONTAL)
        pt.x += numeric::intDivFloor(rect.width - extentTrunc.GetWidth(), 2); //round down negative values, too!

    if (alignment & wxALIGN_BOTTOM) //note: wxALIGN_TOP == 0!
        pt.y += rect.height - extentTrunc.GetHeight();
    else if (alignment & wxALIGN_CENTER_VERTICAL)
        pt.y += numeric::intDivFloor(rect.height - extentTrunc.GetHeight(), 2); //round down negative values, too!

    //std::unique_ptr<RecursiveDcClipper> clip; -> redundant!? RecursiveDcClipper already used during grid cell rendering
    //if (extentTrunc.GetWidth() > rect.width)
    //    clip = std::make_unique<RecursiveDcClipper>(dc, rect);

    dc.DrawText(textTrunc, pt);
}


void GridData::renderColumnLabel(wxDC& dc, const wxRect& rect, ColumnType colType, bool enabled, bool highlighted)
{
    wxRect rectRemain = drawColumnLabelBackground(dc, rect, highlighted);

    rectRemain.x     += getColumnGapLeft();
    rectRemain.width -= getColumnGapLeft();
    drawColumnLabelText(dc, rectRemain, getColumnLabel(colType), enabled);
}


wxRect GridData::drawColumnLabelBackground(wxDC& dc, const wxRect& rect, bool highlighted)
{
    if (highlighted)
        dc.GradientFillLinear(rect, getColorLabelGradientFocusFrom(), getColorLabelGradientFocusTo(), wxSOUTH);
    else //regular background gradient
        dc.GradientFillLinear(rect, getColorLabelGradientFrom(), getColorLabelGradientTo(), wxSOUTH);

    //left border
    clearArea(dc, wxRect(rect.GetTopLeft(), wxSize(dipToWxsize(1), rect.height)), wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

    //right border
    dc.GradientFillLinear(wxRect(rect.x + rect.width - dipToWxsize(1), rect.y, dipToWxsize(1), rect.height),
                          getColorLabelGradientFrom(), wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW), wxSOUTH);

    //bottom border
    clearArea(dc, wxRect(rect.x, rect.y + rect.height - dipToWxsize(1), rect.width, dipToWxsize(1)), wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));

    return rect.Deflate(dipToWxsize(1), dipToWxsize(1));
}


void GridData::drawColumnLabelText(wxDC& dc, const wxRect& rect, const std::wstring& text, bool enabled)
{
    wxDCTextColourChanger textColor(dc, getColorLabelText(enabled)); //accessibility: always set both foreground AND background colors!
    drawCellText(dc, rect, text, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
}

//----------------------------------------------------------------------------------------------------------------
/*                    SubWindow
                         /|\
        __________________|__________________
        |           |            |          |
    CornerWin  RowLabelWin  ColLabelWin  MainWin        */

class Grid::SubWindow : public wxWindow
{
public:
    SubWindow(Grid& parent) :
        wxWindow(&parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBORDER_NONE, wxASCII_STR(wxPanelNameStr)),
        parent_(parent)
    {
        Bind(wxEVT_PAINT, [this](wxPaintEvent& event) { onPaintEvent(event); });
        Bind(wxEVT_SIZE,  [this](wxSizeEvent&  event) { Refresh(); event.Skip(); });
        Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent& event) {}); //https://wiki.wxwidgets.org/Flicker-Free_Drawing
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        //SetDoubleBuffered(true); -> slow as hell!

        Bind(wxEVT_CHILD_FOCUS, [](wxChildFocusEvent& event) {}); //wxGTK::wxScrolledWindow automatically scrolls to child window when child gets focus -> prevent!

        Bind(wxEVT_LEFT_DOWN,    [this](wxMouseEvent& event) { onMouseLeftDown  (event); });
        Bind(wxEVT_LEFT_UP,      [this](wxMouseEvent& event) { onMouseLeftUp    (event); });
        Bind(wxEVT_LEFT_DCLICK,  [this](wxMouseEvent& event) { onMouseLeftDouble(event); });
        Bind(wxEVT_RIGHT_DOWN,   [this](wxMouseEvent& event) { onMouseRightDown (event); });
        Bind(wxEVT_RIGHT_UP,     [this](wxMouseEvent& event) { onMouseRightUp   (event); });
        Bind(wxEVT_MOTION,       [this](wxMouseEvent& event) { onMouseMovement  (event); });
        Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& event) { onLeaveWindow    (event); });
        Bind(wxEVT_MOUSEWHEEL,   [this](wxMouseEvent& event) { onMouseWheel     (event); });
        Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent& event) { onMouseCaptureLost(event); });

        Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event)
        {
            if (!sendEventToParent(event)) //let parent collect all key events
                event.Skip();
        });
        //Bind(wxEVT_KEY_UP,   [this](wxKeyEvent& event) { onKeyUp  (event); }); -> superfluous?

        assert(GetClientAreaOrigin() == wxPoint()); //generally assumed when dealing with coordinates below
    }
    Grid&       refParent()       { return parent_; }
    const Grid& refParent() const { return parent_; }

    template <class T>
    bool sendEventToParent(T&& event) //take both "rvalue + lvalues", return "true" if a suitable event handler function was found and executed, and the function did not call wxEvent::Skip.
    {
        return parent_.GetEventHandler()->ProcessEvent(event);
    }

protected:
    void setToolTip(const std::wstring& text) //proper fix for wxWindow
    {
        if (text != GetToolTipText())
        {
            if (text.empty())
                UnsetToolTip(); //wxGTK doesn't allow wxToolTip with empty text!
            else
            {
                wxToolTip* tt = GetToolTip();
                if (!tt)
                {
                    //wxWidgets bug: tooltip multiline property is defined by first tooltip text containing newlines or not (same is true for maximum width)
                    tt = new wxToolTip(L"a                                                                b\n\
                                                           a                                                                b"); //ugly, but working (on Windows)
                    SetToolTip(tt); //pass ownership
                }
                tt->SetTip(text);
            }
        }
    }

private:
    virtual void render(wxDC& dc, const wxRect& rect) = 0;

    virtual void onMouseLeftDown  (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseLeftUp    (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseLeftDouble(wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseRightDown (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseRightUp   (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseMovement  (wxMouseEvent& event) { event.Skip(); }
    virtual void onLeaveWindow    (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseCaptureLost(wxMouseCaptureLostEvent& event) { event.Skip(); }

    void onMouseWheel(wxMouseEvent& event)
    {
        /*  MSDN, WM_MOUSEWHEEL: "Sent to the focus window when the mouse wheel is rotated.
            The DefWindowProc function propagates the message to the window's parent.
            There should be no internal forwarding of the message, since DefWindowProc propagates
            it up the parent chain until it finds a window that processes it."

            On macOS there is no such propagation! => we need a redirection (the same wxGrid implements)

            new wxWidgets 3.0 screw-up for GTK2: wxScrollHelperEvtHandler::ProcessEvent() ignores wxEVT_MOUSEWHEEL events
            thereby breaking the scenario of redirection to parent we need here (but also breaking their very own wxGrid sample)
            => call wxScrolledWindow mouse wheel handler directly                          */

        //wxWidgets never ceases to amaze: multi-line scrolling is implemented maximally inefficient by repeating wxEVT_SCROLLWIN_LINEUP!! => WTF!
        if (event.GetWheelAxis() == wxMOUSE_WHEEL_VERTICAL && //=> reimplement wxScrollHelperBase::HandleOnMouseWheel() in a non-retarded way
            !event.IsPageScroll())
        {
            mouseRotateRemainder_ += -event.GetWheelRotation();
            int rotations = mouseRotateRemainder_ / event.GetWheelDelta();
            mouseRotateRemainder_ -= rotations * event.GetWheelDelta();

            if (rotations == 0) //macOS generates tiny GetWheelRotation()! => don't allow! Always scroll a single row at least!
            {
                rotations = -numeric::sign(event.GetWheelRotation());
                mouseRotateRemainder_ = 0;
            }

            const int rowsDelta = rotations * event.GetLinesPerAction();
            parent_.scrollDelta(0, rowsDelta);
        }
        else
            parent_.HandleOnMouseWheel(event);

        onMouseMovement(event);
        event.Skip(false);

        //if (!sendEventToParent(event))
        //   event.Skip();
    }

    void onPaintEvent(wxPaintEvent& event)
    {
        //wxAutoBufferedPaintDC dc(this); -> this one happily fucks up for RTL layout by not drawing the first column (x = 0)!
        BufferedPaintDC dc(*this, doubleBuffer_);

        assert(GetSize() == GetClientSize());

        const wxRegion& updateReg = GetUpdateRegion();
        for (wxRegionIterator it = updateReg; it; ++it)
            render(dc, it.GetRect());
    }

    Grid& parent_;
    std::optional<wxBitmap> doubleBuffer_;
    int mouseRotateRemainder_ = 0;
};

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

class Grid::CornerWin : public SubWindow
{
public:
    explicit CornerWin(Grid& parent) : SubWindow(parent) {}

private:
    bool AcceptsFocus() const override { return false; }

    void render(wxDC& dc, const wxRect& /*rect*/) override
    {
        const wxRect& rect = GetClientRect(); //would be overkill to support GetUpdateRegion()!

        clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        //caveat: wxSYS_COLOUR_BTNSHADOW is partially transparent on macOS!

        dc.GradientFillLinear(rect, getColorLabelGradientFrom(), getColorLabelGradientTo(), wxSOUTH);

        //left border
        dc.GradientFillLinear(wxRect(rect.GetTopLeft(), wxSize(dipToWxsize(1), rect.height)),
                              getColorLabelGradientFrom(), wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW), wxSOUTH);

        //left border2
        clearArea(dc, wxRect(rect.x + dipToWxsize(1), rect.y, dipToWxsize(1), rect.height),
                  wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

        //right border
        dc.GradientFillLinear(wxRect(rect.x + rect.width - dipToWxsize(1), rect.y, dipToWxsize(1), rect.height),
                              getColorLabelGradientFrom(), wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW), wxSOUTH);

        //bottom border
        clearArea(dc, wxRect(rect.x, rect.y + rect.height - dipToWxsize(1), rect.width, dipToWxsize(1)),
                  wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));
    }
};

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

class Grid::RowLabelWin : public SubWindow
{
public:
    explicit RowLabelWin(Grid& parent) :
        SubWindow(parent),
        rowHeight_(parent.GetCharHeight() + dipToWxsize(2) + dipToWxsize(1)) {} //default height; don't call any functions on "parent" other than those from wxWindow during construction!
    //2 for some more space, 1 for bottom border (gives 15 + 2 + 1 on Windows, 17 + 2 + 1 on Ubuntu)

    int getBestWidth(ptrdiff_t rowFrom, ptrdiff_t rowTo)
    {
        wxClientDC dc(this);

        dc.SetFont(GetFont()); //harmonize with RowLabelWin::render()!

        int bestWidth = 0;
        for (ptrdiff_t i = rowFrom; i <= rowTo; ++i)
            bestWidth = std::max(bestWidth, dc.GetTextExtent(formatRowNum(i)).GetWidth() + dipToWxsize(2 * ROW_LABEL_BORDER_DIP));
        return bestWidth;
    }

    size_t getLogicalHeight() const { return refParent().getRowCount() * rowHeight_; }

    ptrdiff_t getRowAtPos(ptrdiff_t posY) const //returns < 0 on invalid input, else row number within: [0, rowCount]; rowCount if out of range
    {
        if (posY < 0)
            return -1;

        const size_t row = posY / rowHeight_;
        return std::min(row, refParent().getRowCount());
    }

    int getRowHeight() const { return rowHeight_; } //guarantees to return size >= 1 !
    void setRowHeight(int height) { assert(height > 0); rowHeight_ = std::max(1, height); }

    wxRect getRowLabelArea(size_t row) const //returns empty rect if row not found
    {
        assert(GetClientAreaOrigin() == wxPoint());
        if (row < refParent().getRowCount())
            return wxRect(wxPoint(0, rowHeight_ * row),
                          wxSize(GetClientSize().GetWidth(), rowHeight_));
        return wxRect();
    }

private:
    static std::wstring formatRowNum(size_t row) { return formatNumber(row + 1); } //convert number to std::wstring including thousands separator

    bool AcceptsFocus() const override { return false; }

    void render(wxDC& dc, const wxRect& rect) override
    {
        clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

        const bool enabled = renderAsEnabled(*this);

        dc.SetFont(GetFont()); //harmonize with RowLabelWin::getBestWidth()!

        const auto& [rowFirst, rowLast] = refParent().getVisibleRows(rect);
        for (auto row = rowFirst; row < rowLast; ++row)
        {
            wxRect rectRowLabel = getRowLabelArea(row); //returns empty rect if row not found
            if (rectRowLabel.height > 0)
            {
                rectRowLabel.y = refParent().CalcScrolledPosition(rectRowLabel.GetTopLeft()).y;
                drawRowLabel(dc, rectRowLabel, row, enabled);
            }
        }
    }

    void drawRowLabel(wxDC& dc, const wxRect& rect, size_t row, bool enabled)
    {
        //clearArea(dc, rect, getColorRowLabel());
        dc.GradientFillLinear(rect, getColorLabelGradientFrom(), getColorLabelGradientTo(), wxEAST); //clear overlapping cells

        //top border
        clearArea(dc, wxRect(rect.x, rect.y, rect.width, dipToWxsize(1)), wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

        //left border
        clearArea(dc, wxRect(rect.x, rect.y, dipToWxsize(1), rect.height), wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));

        //right border
        clearArea(dc, wxRect(rect.x + rect.width - dipToWxsize(1), rect.y, dipToWxsize(1), rect.height), wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));

        //bottom border
        clearArea(dc, wxRect(rect.x, rect.y + rect.height - dipToWxsize(1), rect.width, dipToWxsize(1)), wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));

        //label text
        wxRect textRect = rect;
        textRect.Deflate(dipToWxsize(1));

        wxDCTextColourChanger textColor(dc, getColorLabelText(enabled)); //accessibility: always set both foreground AND background colors!
        GridData::drawCellText(dc, textRect, formatRowNum(row), wxALIGN_CENTRE);
    }

    void onMouseLeftDown(wxMouseEvent& event) override { redirectMouseEvent(event); }
    void onMouseLeftUp  (wxMouseEvent& event) override { redirectMouseEvent(event); }
    void onMouseMovement(wxMouseEvent& event) override { redirectMouseEvent(event); }
    void onLeaveWindow  (wxMouseEvent& event) override { redirectMouseEvent(event); }
    void onMouseCaptureLost(wxMouseCaptureLostEvent& event) override { refParent().getMainWin().GetEventHandler()->ProcessEvent(event); }

    void redirectMouseEvent(wxMouseEvent& event)
    {
        event.m_x = 0; //simulate click on left side of mainWin_!

        wxWindow& mainWin = refParent().getMainWin();
        mainWin.GetEventHandler()->ProcessEvent(event);

        if (event.ButtonDown() && wxWindow::FindFocus() != &mainWin)
            mainWin.SetFocus();
    }

    int rowHeight_;
};


namespace
{
class ColumnResizing
{
public:
    ColumnResizing(wxWindow& wnd, size_t col, int startWidth, int clientPosX) :
        wnd_(wnd), col_(col), startWidth_(startWidth), clientPosX_(clientPosX)
    {
        wnd_.CaptureMouse();
    }
    ~ColumnResizing()
    {
        if (wnd_.HasCapture())
            wnd_.ReleaseMouse();
    }

    size_t getColumn      () const { return col_; }
    int    getStartWidth  () const { return startWidth_; }
    int    getStartPosX   () const { return clientPosX_; }

private:
    wxWindow& wnd_;
    const size_t col_;
    const int    startWidth_;
    const int    clientPosX_;
};


class ColumnMove
{
public:
    ColumnMove(wxWindow& wnd, size_t colFrom, int clientPosX) :
        wnd_(wnd),
        colFrom_(colFrom),
        colTo_(colFrom),
        clientPosX_(clientPosX) { wnd_.CaptureMouse(); }
    ~ColumnMove() { if (wnd_.HasCapture()) wnd_.ReleaseMouse(); }

    size_t  getColumnFrom() const { return colFrom_; }
    size_t& refColumnTo() { return colTo_; }
    int     getStartPosX () const { return clientPosX_; }

    bool isRealMove() const { return !singleClick_; }
    void setRealMove() { singleClick_ = false; }

private:
    wxWindow& wnd_;
    const size_t colFrom_;
    size_t colTo_;
    const int clientPosX_;
    bool singleClick_ = true;
};
}

//----------------------------------------------------------------------------------------------------------------

class Grid::ColLabelWin : public SubWindow
{
public:
    explicit ColLabelWin(Grid& parent) : SubWindow(parent),
        labelFont_(GetFont().Bold())
    {
        //coordinate with ColLabelWin::render():
        colLabelHeight_ = dipToWxsize(2 * DEFAULT_COL_LABEL_BORDER_DIP) + labelFont_.GetPixelSize().GetHeight();
    }

    int getColumnLabelHeight() const { return colLabelHeight_; }
    void setColumnLabelHeight(int height) { colLabelHeight_ = std::max(0, height); }

private:
    bool AcceptsFocus() const override { return false; }

    void render(wxDC& dc, const wxRect& rect) override
    {
        clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        //caveat: system colors can be partially transparent on macOS

        dc.SetFont(labelFont_); //coordinate with "colLabelHeight" in Grid constructor
        dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));

        const bool enabled = renderAsEnabled(*this);

        wxPoint labelAreaTL(refParent().CalcScrolledPosition(wxPoint(0, 0)).x, 0); //client coordinates

        const std::vector<ColumnWidth>& absWidths = refParent().getColWidths(); //resolve stretched widths
        for (size_t col = 0; col < absWidths.size(); ++col)
        {
            const int width  = absWidths[col].width; //don't use unsigned for calculations!

            if (labelAreaTL.x > rect.GetRight())
                return; //done, rect is fully covered
            if (labelAreaTL.x + width > rect.x)
                drawColumnLabel(dc, wxRect(labelAreaTL, wxSize(width, colLabelHeight_)), col, absWidths[col].type, enabled);
            labelAreaTL.x += width;
        }
        if (labelAreaTL.x > rect.GetRight())
            return; //done, rect is fully covered

        //fill gap after columns and cover full width
        if (fillGapAfterColumns)
        {
            int totalWidth = 0;
            for (const ColumnWidth& cw : absWidths)
                totalWidth += cw.width;
            const int clientWidth = GetClientSize().GetWidth(); //need reliable, stable width in contrast to rect.width

            if (totalWidth < clientWidth)
                drawColumnLabel(dc, wxRect(labelAreaTL, wxSize(clientWidth - totalWidth, colLabelHeight_)), absWidths.size(), ColumnType::none, enabled);
        }
    }

    void drawColumnLabel(wxDC& dc, const wxRect& rect, size_t col, ColumnType colType, bool enabled)
    {
        if (auto prov = refParent().getDataProvider())
        {
            const bool isHighlighted = activeResizing_    ? col == activeResizing_   ->getColumn    () : //highlight_ column on mouse-over
                                       activeClickOrMove_ ? col == activeClickOrMove_->getColumnFrom() :
                                       highlightCol_      ? col == *highlightCol_                      :
                                       false;

            RecursiveDcClipper clip(dc, rect);
            prov->renderColumnLabel(dc, rect, colType, enabled, isHighlighted);

            //draw move target location
            if (refParent().allowColumnMove_)
                if (activeClickOrMove_ && activeClickOrMove_->isRealMove())
                {
                    const int markerWidth = dipToWxsize(COLUMN_MOVE_MARKER_WIDTH_DIP);

                    if (col + 1 == activeClickOrMove_->refColumnTo()) //handle pos 1, 2, .. up to "at end" position
                        dc.GradientFillLinear(wxRect(rect.x + rect.width - markerWidth, rect.y, markerWidth, rect.height), getColorLabelGradientFrom(), *wxBLUE, wxSOUTH);
                    else if (col == activeClickOrMove_->refColumnTo() && col == 0) //pos 0
                        dc.GradientFillLinear(wxRect(rect.GetTopLeft(), wxSize(markerWidth, rect.height)), getColorLabelGradientFrom(), *wxBLUE, wxSOUTH);
                }
        }
    }

    std::optional<ColAction> clientPosToColumnAction(const wxPoint& pos) const
    {
        if (0 <= pos.y && pos.y < colLabelHeight_)
            if (const int absPosX = refParent().CalcUnscrolledPosition(pos).x;
                absPosX >= 0)
            {
                const int resizeTolerance = refParent().allowColumnResize_ ? dipToWxsize(COLUMN_RESIZE_TOLERANCE_DIP) : 0;
                const std::vector<ColumnWidth>& absWidths = refParent().getColWidths(); //resolve stretched widths

                int accuWidth = 0;
                for (size_t col = 0; col < absWidths.size(); ++col)
                {
                    accuWidth += absWidths[col].width;
                    if (std::abs(absPosX - accuWidth) < resizeTolerance)
                    {
                        ColAction out;
                        out.wantResize = true;
                        out.col        = col;
                        return out;
                    }
                    else if (absPosX < accuWidth)
                    {
                        ColAction out;
                        out.wantResize = false;
                        out.col        = col;
                        return out;
                    }
                }
            }
        return {};
    }

    size_t clientPosToMoveTargetColumn(const wxPoint& pos) const
    {
        const int absPosX = refParent().CalcUnscrolledPosition(pos).x;
        const std::vector<ColumnWidth>& absWidths = refParent().getColWidths(); //resolve negative/stretched widths

        int accWidth = 0;
        for (size_t col = 0; col < absWidths.size(); ++col)
        {
            const int width = absWidths[col].width; //beware dreaded unsigned conversions!
            accWidth += width;

            if (absPosX < accWidth - width / 2)
                return col;
        }
        return absWidths.size();
    }

    void onMouseLeftDown(wxMouseEvent& event) override
    {
        //if (FindFocus() != &refParent().getMainWin()) -> clicking column label shouldn't change input focus, right!? e.g. resizing column, sorting...(other grid)
        //    refParent().getMainWin().SetFocus();

        activeResizing_   .reset();
        activeClickOrMove_.reset();

        if (std::optional<ColAction> action = clientPosToColumnAction(event.GetPosition()))
        {
            if (action->wantResize)
            {
                if (!event.LeftDClick()) //double-clicks never seem to arrive here; why is this checked at all???
                    if (std::optional<int> colWidth = refParent().getColWidth(action->col))
                        activeResizing_ = std::make_unique<ColumnResizing>(*this, action->col, *colWidth, event.GetPosition().x);
            }
            else //a move or single click
                activeClickOrMove_ = std::make_unique<ColumnMove>(*this, action->col, event.GetPosition().x);
        }
        event.Skip();
    }

    void onMouseLeftUp(wxMouseEvent& event) override
    {
        activeResizing_.reset(); //nothing else to do, actual work done by onMouseMovement()

        if (activeClickOrMove_)
        {
            if (activeClickOrMove_->isRealMove())
            {
                if (refParent().allowColumnMove_)
                {
                    const size_t colFrom = activeClickOrMove_->getColumnFrom();
                    size_t       colTo   = activeClickOrMove_->refColumnTo();

                    if (colTo > colFrom) //simulate "colFrom" deletion
                        --colTo;

                    refParent().moveColumn(colFrom, colTo);
                }
            }
            else //notify single label click
            {
                const wxPoint mousePos = GetPosition() + event.GetPosition();
                if (const std::optional<ColumnType> colType = refParent().colToType(activeClickOrMove_->getColumnFrom()))
                    sendEventToParent(GridLabelClickEvent(EVENT_GRID_COL_LABEL_MOUSE_LEFT, *colType, mousePos));
            }
            activeClickOrMove_.reset();
        }

        refParent().updateWindowSizes(); //looks strange if done during onMouseMovement()
        refParent().Refresh();
        event.Skip();
    }

    void onMouseLeftDouble(wxMouseEvent& event) override
    {
        if (std::optional<ColAction> action = clientPosToColumnAction(event.GetPosition()))
            if (action->wantResize)
            {
                //auto-size visible range on double-click
                const int bestWidth = refParent().getBestColumnSize(action->col); //return -1 on error
                if (bestWidth >= 0)
                {
                    refParent().setColumnWidth(bestWidth, action->col, GridEventPolicy::allow);
                    refParent().Refresh(); //refresh main grid as well!
                }
            }
        event.Skip();
    }

    void onMouseRightDown(wxMouseEvent& event) override
    {
        evalMouseMovement(event.GetPosition()); //update highlight in obscure cases (e.g. right-click while other context menu is open)

        const wxPoint mousePos = GetPosition() + event.GetPosition();

        if (const std::optional<ColAction> action = clientPosToColumnAction(event.GetPosition()))
        {
            if (const std::optional<ColumnType> colType = refParent().colToType(action->col))
                sendEventToParent(GridLabelClickEvent(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, *colType, mousePos)); //notify right click
            else assert(false);
        }
        else
            //notify right click (on free space after last column)
            if (fillGapAfterColumns)
                sendEventToParent(GridLabelClickEvent(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, ColumnType::none, mousePos));

        //update mouse highlight (e.g. mouse position changed after showing context menu) => needed on Linux/macOS
        evalMouseMovement(ScreenToClient(wxGetMousePosition()));

        event.Skip();
    }

    void onMouseMovement(wxMouseEvent& event) override
    {
        evalMouseMovement(event.GetPosition());
        event.Skip();
    }

    void evalMouseMovement(wxPoint clientPos)
    {
        if (activeResizing_)
        {
            const auto col     = activeResizing_->getColumn();
            const int newWidth = activeResizing_->getStartWidth() + clientPos.x - activeResizing_->getStartPosX();

            //set width tentatively
            refParent().setColumnWidth(newWidth, col, GridEventPolicy::allow);

            //check if there's a small gap after last column, if yes, fill it
            const int gapWidth = GetClientSize().GetWidth() - refParent().getColWidthsSum(GetClientSize().GetWidth());
            if (std::abs(gapWidth) < dipToWxsize(COLUMN_FILL_GAP_TOLERANCE_DIP))
                refParent().setColumnWidth(newWidth + gapWidth, col, GridEventPolicy::allow);

            Refresh();
            refParent().Refresh(); //refresh columns on main grid as well!
        }
        else if (activeClickOrMove_)
        {
            const int clientPosX = clientPos.x;
            if (std::abs(clientPosX - activeClickOrMove_->getStartPosX()) > dipToWxsize(COLUMN_MOVE_DELAY_DIP)) //real move (not a single click)
            {
                activeClickOrMove_->setRealMove();
                activeClickOrMove_->refColumnTo() = clientPosToMoveTargetColumn(clientPos);
                Refresh();
            }
        }
        else
        {
            if (const std::optional<ColAction> action = clientPosToColumnAction(clientPos))
            {
                setMouseHighlight(action->col);

                if (action->wantResize)
                    SetCursor(wxCURSOR_SIZEWE); //window-local only! :)
                else
                    SetCursor(*wxSTANDARD_CURSOR); //NOOP when setting same cursor
            }
            else
            {
                setMouseHighlight(std::nullopt);
                SetCursor(*wxSTANDARD_CURSOR);
            }
        }

        const std::wstring toolTip = [&]
        {
            if (const ColumnType colType = refParent().getColumnAtWinPos(clientPos.x).colType; //returns ColumnType::none if no column at x position!
                colType != ColumnType::none)
                if (auto prov = refParent().getDataProvider())
                    return prov->getToolTip(colType);
            return std::wstring();
        }();
        setToolTip(toolTip);
    }

    void onMouseCaptureLost(wxMouseCaptureLostEvent& event) override
    {
        if (activeResizing_ || activeClickOrMove_)
        {
            activeResizing_   .reset();
            activeClickOrMove_.reset();
            Refresh();
        }
        setMouseHighlight(std::nullopt);
        //event.Skip(); -> we DID handle it!
    }

    void onLeaveWindow(wxMouseEvent& event) override
    {
        if (!activeResizing_ && !activeClickOrMove_)
            //wxEVT_LEAVE_WINDOW does not respect mouse capture! -> however highlight is drawn unconditionally during move/resize!
            setMouseHighlight(std::nullopt);

        event.Skip();
    }

    void setMouseHighlight(const std::optional<size_t>& hl)
    {
        if (highlightCol_ != hl)
        {
            highlightCol_ = hl;
            Refresh();
        }
    }

    std::unique_ptr<ColumnResizing> activeResizing_;
    std::unique_ptr<ColumnMove>     activeClickOrMove_;
    std::optional<size_t>           highlightCol_;

    int colLabelHeight_ = 0;
    const wxFont labelFont_;
};

//----------------------------------------------------------------------------------------------------------------
namespace
{
wxDEFINE_EVENT(EVENT_GRID_HAS_SCROLLED, wxCommandEvent);
}
//----------------------------------------------------------------------------------------------------------------

class Grid::MainWin : public SubWindow
{
public:
    MainWin(Grid& parent,
            RowLabelWin& rowLabelWin,
            ColLabelWin& colLabelWin) : SubWindow(parent),
        rowLabelWin_(rowLabelWin),
        colLabelWin_(colLabelWin)
    {
        Bind(EVENT_GRID_HAS_SCROLLED, [this](wxCommandEvent& event) { onRequestWindowUpdate(event); });

        Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event)
        {
            if (event.GetKeyCode() == WXK_ESCAPE && activeSelection_) //allow Escape key to cancel active selection!
            {
                wxMouseCaptureLostEvent evt;
                GetEventHandler()->ProcessEvent(evt); //better integrate into event handling rather than calling onMouseCaptureLost() directly!?
                return;
            }

            /* using keyboard: => clear distracting mouse highlights

               wxEVT_KEY_DOWN evaluation order:
                 1. this callback
                 2. Grid::SubWindow ... sendEventToParent()
                 3. clients binding to Grid wxEVT_KEY_DOWN
                 4. Grid::onKeyDown()                           */
            setMouseHighlight(std::nullopt);

            event.Skip();
        });
    }

    ~MainWin() { assert(!gridUpdatePending_); }

    size_t getCursor() const { return cursorRow_; }
    size_t getAnchor() const { return selectionAnchor_; }

    void setCursor(size_t newCursorRow, size_t newAnchorRow)
    {
        cursorRow_       = newCursorRow;
        selectionAnchor_ = newAnchorRow;
        activeSelection_.reset(); //e.g. user might search with F3 while holding down left mouse button
    }

private:
    void render(wxDC& dc, const wxRect& rect) override
    {
        clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); //CONTRACT! expected by GridData::renderRowBackgound()!

        const bool enabled = renderAsEnabled(*this);

        if (auto prov = refParent().getDataProvider())
        {
            dc.SetFont(GetFont()); //harmonize with Grid::getBestColumnSize()
            dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));

            const std::vector<ColumnWidth>& absWidths = refParent().getColWidths(); //resolve stretched widths

            int totalRowWidth = 0;
            for (const ColumnWidth& cw : absWidths)
                totalRowWidth += cw.width;

            //fill gap after columns and cover full width
            if (fillGapAfterColumns)
                totalRowWidth = std::max(totalRowWidth, GetClientSize().GetWidth());

            RecursiveDcClipper dummy(dc, rect); //do NOT draw background on cells outside of invalidated rect invalidating foreground text!

            const wxPoint gridAreaTL(refParent().CalcScrolledPosition(wxPoint(0, 0))); //client coordinates
            const int rowHeight = rowLabelWin_.getRowHeight();
            const auto& [rowFirst, rowLast] = refParent().getVisibleRows(rect);

            for (auto row = rowFirst; row < rowLast; ++row)
            {
                //draw background lines
                const wxRect rowRect(gridAreaTL + wxPoint(0, row * rowHeight), wxSize(totalRowWidth, rowHeight));
                const bool drawSelected = drawAsSelected(row);
                const HoverArea rowHover = getRowHoverToDraw(row);

                RecursiveDcClipper dummy2(dc, rowRect);
                prov->renderRowBackgound(dc, rowRect, row, enabled, drawSelected, rowHover);

                //draw cells column by column
                wxRect cellRect = rowRect;
                for (const ColumnWidth& cw : absWidths)
                {
                    cellRect.width = cw.width;

                    if (cellRect.x > rect.GetRight())
                        break; //done

                    if (cellRect.x + cw.width > rect.x)
                    {
                        RecursiveDcClipper dummy3(dc, cellRect);
                        prov->renderCell(dc, cellRect, row, cw.type, enabled, drawSelected, rowHover);
                    }
                    cellRect.x += cw.width;
                }
            }
        }
    }

    HoverArea getRowHoverToDraw(ptrdiff_t row) const
    {
        if (activeSelection_)
        {
            if (activeSelection_->getFirstClick().row_ == row)
                return activeSelection_->getFirstClick().hoverArea_;
        }
        else if (highlight_)
        {
            if (makeSigned(highlight_->row) == row)
                return highlight_->rowHover;
        }
        return HoverArea::none;
    }

    bool drawAsSelected(size_t row) const
    {
        if (activeSelection_) //check if user is currently selecting with mouse
        {
            const size_t rowFrom = std::min(activeSelection_->getStartRow(), activeSelection_->getCurrentRow());
            const size_t rowTo   = std::max(activeSelection_->getStartRow(), activeSelection_->getCurrentRow());

            if (rowFrom <= row && row <= rowTo)
                return activeSelection_->isPositiveSelect(); //overwrite default
        }
        return refParent().isSelected(row);
    }

    void onMouseLeftDown (wxMouseEvent& event) override { onMouseDown(event); }
    void onMouseLeftUp   (wxMouseEvent& event) override { onMouseUp  (event); }
    void onMouseRightDown(wxMouseEvent& event) override { onMouseDown(event); }
    void onMouseRightUp  (wxMouseEvent& event) override { onMouseUp  (event); }

    void onMouseLeftDouble(wxMouseEvent& event) override
    {
        if (auto prov = refParent().getDataProvider())
        {
            const wxPoint   mousePos = GetPosition() + event.GetPosition();
            const ptrdiff_t rowCount = refParent().getRowCount();
            const ptrdiff_t      row = refParent().getRowAtWinPos   (event.GetPosition().y); //return -1 for invalid position; >= rowCount if out of range
            const ColumnPosInfo  cpi = refParent().getColumnAtWinPos(event.GetPosition().x); //returns ColumnType::none if no column at x position!
            const HoverArea rowHover = [&]
            {
                if (0 <= row && row < rowCount && cpi.colType != ColumnType::none)
                {
                    wxClientDC dc(this);
                    dc.SetFont(GetFont());
                    return prov->getMouseHover(dc, row, cpi.colType, cpi.cellRelativePosX, cpi.colWidth);
                }
                return HoverArea::none;
            }();

            //client is interested in all double-clicks, even those outside of the grid!
            sendEventToParent(GridClickEvent(EVENT_GRID_MOUSE_LEFT_DOUBLE, row, rowHover, mousePos));
        }
        event.Skip();
    }

    void onMouseDown(wxMouseEvent& event) //handle left and right mouse button clicks (almost) the same
    {
        if (activeSelection_) //allow other mouse button to cancel active selection!
        {
            wxMouseCaptureLostEvent evt;
            GetEventHandler()->ProcessEvent(evt);
            return;
        }

        if (auto prov = refParent().getDataProvider())
        {
            evalMouseMovement(event.GetPosition()); //update highlight in obscure cases (e.g. right-click while other context menu is open)

            const wxPoint   mousePos = GetPosition() + event.GetPosition();
            const ptrdiff_t rowCount = refParent().getRowCount();
            const ptrdiff_t      row = refParent().getRowAtWinPos   (event.GetPosition().y); //return -1 for invalid position; >= rowCount if out of range
            const ColumnPosInfo  cpi = refParent().getColumnAtWinPos(event.GetPosition().x); //returns ColumnType::none if no column at x position!
            const HoverArea rowHover = [&]
            {
                if (0 <= row && row < rowCount && cpi.colType != ColumnType::none)
                {
                    wxClientDC dc(this);
                    dc.SetFont(GetFont());
                    return prov->getMouseHover(dc, row, cpi.colType, cpi.cellRelativePosX, cpi.colWidth);
                }
                return HoverArea::none;
            }();

            assert(row >= 0);
            //row < 0 was possible in older wxWidgets: https://github.com/wxWidgets/wxWidgets/commit/2c69d27c0d225d3a331c773da466686153185320#diff-9f11c8f2cb1f734f7c0c1071aba491a5
            //=> pressing "Menu Key" simulated mouse-right-button down + up at position 0xffff/0xffff!

            GridClickEvent mouseEvent(event.RightDown() ? EVENT_GRID_MOUSE_RIGHT_DOWN : EVENT_GRID_MOUSE_LEFT_DOWN, row, rowHover, mousePos);

            if (const bool processed = sendEventToParent(mouseEvent); //allow client to swallow event!
                !processed)
            {
                if (wxWindow::FindFocus() != this) //doesn't seem to happen automatically for right mouse button
                    SetFocus();

                if (event.RightDown() && (row < 0 || refParent().isSelected(row))) //=> open context menu *immediately* and do *not* start a new selection
                    sendEventToParent(GridContextMenuEvent(mousePos));
                else if (row >= 0)
                {
                    if (event.ControlDown())
                        activeSelection_ = std::make_unique<MouseSelection>(*this, row, !refParent().isSelected(row) /*positive*/, false /*gridWasCleared*/, mouseEvent);
                    else if (event.ShiftDown())
                    {
                        refParent().clearSelection(GridEventPolicy::deny);
                        activeSelection_ = std::make_unique<MouseSelection>(*this, selectionAnchor_, true /*positive*/, true /*gridWasCleared*/, mouseEvent);
                    }
                    else
                    {
                        refParent().clearSelection(GridEventPolicy::deny);
                        activeSelection_ = std::make_unique<MouseSelection>(*this, row, true /*positive*/, true /*gridWasCleared*/, mouseEvent);
                        //DO NOT emit range event for clearing selection! would be inconsistent with keyboard handling (moving cursor neither emits range event)
                        //and is also harmful when range event is considered a final action
                        //e.g. cfg grid would prematurely show a modal dialog after changed config
                    }
                }
            }

            //update mouse highlight (e.g. mouse position changed after showing context menu) => needed on Linux/macOS
            evalMouseMovement(ScreenToClient(wxGetMousePosition()));
        }
        event.Skip(); //allow changing focus
    }

    void onMouseUp(wxMouseEvent& event)
    {
        if (activeSelection_)
        {
            const size_t rowCount = refParent().getRowCount();
            if (rowCount > 0)
            {
                if (activeSelection_->getCurrentRow() < rowCount)
                {
                    cursorRow_ = activeSelection_->getCurrentRow();
                    selectionAnchor_ = activeSelection_->getStartRow(); //allowed to be "out of range"
                }
                else if (activeSelection_->getStartRow() < rowCount) //don't change cursor if "to" and "from" are out of range
                {
                    cursorRow_ = rowCount - 1;
                    selectionAnchor_ = activeSelection_->getStartRow(); //allowed to be "out of range"
                }
                else //total selection "out of range"
                    selectionAnchor_ = cursorRow_;
            }
            //slight deviation from Explorer: change cursor while dragging mouse! -> unify behavior with shift + direction keys
            const wxPoint mousePos = GetPosition() + event.GetPosition();
            const size_t            rowFrom = activeSelection_->getStartRow();
            const size_t              rowTo = activeSelection_->getCurrentRow();
            const bool             positive = activeSelection_->isPositiveSelect();
            const GridClickEvent mouseClick = activeSelection_->getFirstClick();
            assert((mouseClick.GetEventType() == EVENT_GRID_MOUSE_RIGHT_DOWN) == event.RightUp());

            activeSelection_.reset(); //release mouse capture *before* sending the event (which might show a modal popup dialog requiring the mouse!!!)

            const size_t rowFirst = std::min(rowFrom, rowTo);     //sort + convert to half-open range
            const size_t rowLast  = std::max(rowFrom, rowTo) + 1; //
            refParent().selectRange2(rowFirst, rowLast, positive, &mouseClick, GridEventPolicy::allow);

            if (mouseClick.GetEventType() == EVENT_GRID_MOUSE_RIGHT_DOWN)
                sendEventToParent(GridContextMenuEvent(mousePos)); //... *not* mouseClick.mousePos_
        }
#if 0
        if (!event.RightUp())
            if (auto prov = refParent().getDataProvider())
            {
                //this one may point to row which is not in visible area!
                const wxPoint   mousePos = GetPosition() + event.GetPosition();
                const ptrdiff_t rowCount = refParent().getRowCount();
                const ptrdiff_t      row = refParent().getRowAtWinPos   (event.GetPosition().y); //return -1 for invalid position; >= rowCount if out of range
                const ColumnPosInfo  cpi = refParent().getColumnAtWinPos(event.GetPosition().x); //returns ColumnType::none if no column at x position!
                const HoverArea rowHover = [&]
                {
                    if (0 <= row && row < rowCount && cpi.colType != ColumnType::none)
                    {
                        wxClientDC dc(this);
                        dc.SetFont(GetFont());
                        return prov->getMouseHover(dc, row, cpi.colType, cpi.cellRelativePosX, cpi.colWidth);
                    }
                    return HoverArea::none;
                }();
                //notify click event after the range selection! e.g. this makes sure the selection is applied before showing a context menu
                sendEventToParent(GridClickEvent(EVENT_GRID_MOUSE_LEFT_UP, row, rowHover, mousePos));
            }
#endif
        //update mouse highlight (e.g. mouse position changed after showing context menu)
        //=> macOS no mouse movement event is generated after a mouse button click (unlike on Windows)
        evalMouseMovement(ScreenToClient(wxGetMousePosition()));

        event.Skip(); //allow changing focus
    }

    void onMouseMovement(wxMouseEvent& event) override
    {
        evalMouseMovement(event.GetPosition());
        event.Skip();
    }

    void evalMouseMovement(wxPoint clientPos)
    {
        if (auto prov = refParent().getDataProvider())
        {
            const ptrdiff_t rowCount = refParent().getRowCount();
            const ptrdiff_t      row = refParent().getRowAtWinPos   (clientPos.y); //return -1 for invalid position; >= rowCount if out of range
            const ColumnPosInfo  cpi = refParent().getColumnAtWinPos(clientPos.x); //returns ColumnType::none if no column at x position!
            const HoverArea rowHover = [&]
            {
                if (0 <= row && row < rowCount && cpi.colType != ColumnType::none)
                {
                    wxClientDC dc(this);
                    dc.SetFont(GetFont());
                    return prov->getMouseHover(dc, row, cpi.colType, cpi.cellRelativePosX, cpi.colWidth);
                }
                return HoverArea::none;
            }();

            const std::wstring toolTip = [&]
            {
                if (0 <= row && row < rowCount && cpi.colType != ColumnType::none)
                    return prov->getToolTip(row, cpi.colType, rowHover);
                return std::wstring();
            }();
            setToolTip(toolTip); //change even during mouse selection!

            if (activeSelection_)
                activeSelection_->evalMousePos(); //call on both mouse movement + timer event!
            else
                setMouseHighlight(rowHover != HoverArea::none ? std::make_optional<MouseHighlight>({static_cast<size_t>(row), rowHover}) : std::nullopt);
        }
    }

    void onMouseCaptureLost(wxMouseCaptureLostEvent& event) override
    {
        if (activeSelection_)
        {
            if (activeSelection_->gridWasCleared())
                refParent().clearSelection(GridEventPolicy::allow); //see onMouseDown(); selection is "completed" => emit GridSelectEvent

            activeSelection_.reset();
            Refresh();
        }
        setMouseHighlight(std::nullopt);
        //event.Skip(); -> we DID handle it!
    }

    void onLeaveWindow(wxMouseEvent& event) override
    {
        if (!activeSelection_) //wxEVT_LEAVE_WINDOW does not respect mouse capture!
            setMouseHighlight(std::nullopt);

        //CAVEAT: we can get wxEVT_MOTION *after* wxEVT_LEAVE_WINDOW: see RowLabelWin::redirectMouseEvent()
        //        => therefore we also redirect wxEVT_LEAVE_WINDOW, but user will see a little flicker when moving between RowLabelWin and MainWin
        event.Skip();
    }

    class MouseSelection : private wxEvtHandler
    {
    public:
        MouseSelection(MainWin& wnd, size_t rowStart, bool positive, bool gridWasCleared, const GridClickEvent& firstClick) :
            wnd_(wnd), rowStart_(rowStart), rowCurrent_(rowStart), positiveSelect_(positive), gridWasCleared_(gridWasCleared), firstClick_(firstClick)
        {
            wnd_.CaptureMouse();
            timer_.Bind(wxEVT_TIMER, [this](wxTimerEvent& event) { evalMousePos(); });
            timer_.Start(100); //timer interval in ms
            evalMousePos();
            wnd_.Refresh();
        }
        ~MouseSelection() { if (wnd_.HasCapture()) wnd_.ReleaseMouse(); }

        size_t getStartRow     () const { return rowStart_;       }
        size_t getCurrentRow   () const { return rowCurrent_;     }
        bool   isPositiveSelect() const { return positiveSelect_; } //are we selecting or unselecting?
        bool   gridWasCleared  () const { return gridWasCleared_; }

        const GridClickEvent& getFirstClick() const { return firstClick_; }

        void evalMousePos()
        {
            const auto now = std::chrono::steady_clock::now();
            const double deltaSecs = std::chrono::duration<double>(now - lastEvalTime_).count(); //unit: [sec]
            lastEvalTime_ = now;

            const wxPoint clientPos = wnd_.ScreenToClient(wxGetMousePosition());
            const wxSize clientSize = wnd_.GetClientSize();
            assert(wnd_.GetClientAreaOrigin() == wxPoint());

            //scroll while dragging mouse
            const int overlapPixY = clientPos.y < 0 ? clientPos.y :
                                    clientPos.y >= clientSize.GetHeight() ? clientPos.y - (clientSize.GetHeight() - 1) : 0;
            const int overlapPixX = clientPos.x < 0 ? clientPos.x :
                                    clientPos.x >= clientSize.GetWidth() ? clientPos.x - (clientSize.GetWidth() - 1) : 0;

            int pixelsPerUnitY = 0;
            wnd_.refParent().GetScrollPixelsPerUnit(nullptr, &pixelsPerUnitY);
            assert(pixelsPerUnitY > 0);
            if (pixelsPerUnitY <= 0)
                return;

            const double mouseDragSpeedIncScrollU = MOUSE_DRAG_ACCELERATION_DIP * wnd_.rowLabelWin_.getRowHeight() / pixelsPerUnitY; //unit: [scroll units / (DIP * sec)]
            //design alternative: "Dynamic autoscroll based on escape velocity": https://devblogs.microsoft.com/oldnewthing/20210128-00/?p=104768

            auto autoScroll = [&](int overlapPix, double& toScroll)
            {
                if (overlapPix != 0)
                {
                    const double scrollSpeed = wnd_.ToDIP(overlapPix) * mouseDragSpeedIncScrollU; //unit: [scroll units / sec]
                    toScroll += scrollSpeed * deltaSecs;
                }
                else
                    toScroll = 0;
            };

            autoScroll(overlapPixX, toScrollX_);
            autoScroll(overlapPixY, toScrollY_);

            if (static_cast<int>(toScrollX_) != 0 || static_cast<int>(toScrollY_) != 0)
            {
                wnd_.refParent().scrollDelta(static_cast<int>(toScrollX_), static_cast<int>(toScrollY_)); //
                toScrollX_ -= static_cast<int>(toScrollX_); //rounds down for positive numbers, up for negative,
                toScrollY_ -= static_cast<int>(toScrollY_); //exactly what we want
            }

            //select current row *after* scrolling
            wxPoint clientPosTrimmed = clientPos;
            clientPosTrimmed.y = std::clamp(clientPosTrimmed.y, 0, clientSize.GetHeight() - 1); //do not select row outside client window!

            const ptrdiff_t newRow = wnd_.refParent().getRowAtWinPos(clientPosTrimmed.y); //return -1 for invalid position; >= rowCount if out of range
            assert(newRow >= 0);
            if (newRow >= 0)
                if (rowCurrent_ != newRow)
                {
                    rowCurrent_ = newRow;
                    wnd_.Refresh();
                }
        }

    private:
        MainWin& wnd_;
        const size_t rowStart_;
        ptrdiff_t rowCurrent_;
        const bool positiveSelect_;
        const bool gridWasCleared_;
        const GridClickEvent firstClick_;
        wxTimer timer_;
        double toScrollX_ = 0; //count outstanding scroll unit fractions while dragging mouse
        double toScrollY_ = 0; //
        std::chrono::steady_clock::time_point lastEvalTime_ = std::chrono::steady_clock::now();
    };

    void ScrollWindow(int dx, int dy, const wxRect* rect) override
    {
        wxWindow::ScrollWindow(dx, dy, rect);
        rowLabelWin_.ScrollWindow(0, dy, rect);
        colLabelWin_.ScrollWindow(dx, 0, rect);

        //attention, wxGTK call sequence: wxScrolledWindow::Scroll() -> wxScrolledHelperNative::Scroll() -> wxScrolledHelperNative::DoScroll()
        //which *first* calls us, MainWin::ScrollWindow(), and *then* internally updates m_yScrollPosition
        //=> we cannot use CalcUnscrolledPosition() here which gives the wrong/outdated value!!!
        //=> we need to update asynchronously:
        //=> don't send async event repeatedly => severe performance issues on wxGTK!
        //=> can't use idle event neither: too few idle events on Windows, e.g. NO idle events while mouse drag-scrolling!
        //=> solution: send single async event at most!
        if (!gridUpdatePending_) //without guarding, the number of outstanding async events can become very high during scrolling!! test case: Ubuntu: 170; Windows: 20
        {
            gridUpdatePending_ = true;
            GetEventHandler()->AddPendingEvent(wxCommandEvent(EVENT_GRID_HAS_SCROLLED)); //asynchronously call updateAfterScroll()
        }
    }

    void onRequestWindowUpdate(wxEvent& event)
    {
        assert(gridUpdatePending_);
        ZEN_ON_SCOPE_EXIT(gridUpdatePending_ = false);

        refParent().updateWindowSizes(false); //row label width has changed -> do *not* update scrollbars: recursion on wxGTK! -> still a problem, now that this function is called async??
        rowLabelWin_.Update(); //update while dragging scroll thumb
    }

    void refreshRow(size_t row)
    {
        const wxRect& rowArea = rowLabelWin_.getRowLabelArea(row); //returns empty rect if row not found
        const wxPoint topLeft = refParent().CalcScrolledPosition(wxPoint(0, rowArea.y)); //logical -> window coordinates
        wxRect cellArea(topLeft, wxSize(refParent().getColWidthsSum(GetClientSize().GetWidth()), rowArea.height));
        RefreshRect(cellArea);
    }

    struct MouseHighlight
    {
        size_t row = 0;
        HoverArea rowHover = HoverArea::none;

        bool operator==(const MouseHighlight&) const = default;
    };

    void setMouseHighlight(const std::optional<MouseHighlight>& hl)
    {
        assert(!hl || (hl->row < refParent().getRowCount() && hl->rowHover != HoverArea::none));
        if (highlight_ != hl)
        {
            if (highlight_)
                refreshRow(highlight_->row);

            highlight_ = hl;

            if (highlight_)
                refreshRow(highlight_->row);
        }
    }


    RowLabelWin& rowLabelWin_;
    ColLabelWin& colLabelWin_;

    std::unique_ptr<MouseSelection> activeSelection_; //bound while user is selecting with mouse
    std::optional<MouseHighlight> highlight_;

    size_t cursorRow_ = 0;
    size_t selectionAnchor_ = 0;
    bool gridUpdatePending_ = false;
};

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

Grid::Grid(wxWindow* parent,
           wxWindowID id,
           const wxPoint& pos,
           const wxSize& size,
           long style,
           const wxString& name) : wxScrolledWindow(parent, id, pos, size, style | wxWANTS_CHARS, name)
{
    cornerWin_   = new CornerWin  (*this); //
    rowLabelWin_ = new RowLabelWin(*this); //owership handled by "this"
    colLabelWin_ = new ColLabelWin(*this); //
    mainWin_     = new MainWin    (*this, *rowLabelWin_, *colLabelWin_); //

    SetTargetWindow(mainWin_);

    SetInitialSize(size); //"Most controls will use this to set their initial size" -> why not

    assert(GetClientSize() == GetSize() && GetWindowBorderSize() == wxSize()); //borders are NOT allowed for Grid
    //reason: updateWindowSizes() wants to use "GetSize()" as a "GetClientSize()" including scrollbars

    Bind(wxEVT_PAINT, [this](wxPaintEvent& event) { wxPaintDC dc(this); });
    Bind(wxEVT_SIZE,  [this](wxSizeEvent&  event) { updateWindowSizes(); event.Skip(); });
    Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent& event) {}); //https://wiki.wxwidgets.org/Flicker-Free_Drawing

    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) { onKeyDown(event); });
}


void Grid::updateWindowSizes(bool updateScrollbar)
{
    /* We have to deal with TWO nasty circular dependencies:
    1.
        rowLabelWidth
            /|\
        mainWin::client width
            /|\
        SetScrollbars -> show/hide horizontal scrollbar depending on client width
            /|\
        mainWin::client height -> possibly trimmed by horizontal scrollbars
            /|\
        rowLabelWidth

    2.
        mainWin_->GetClientSize()
            /|\
        SetScrollbars -> show/hide scrollbars depending on whether client size is big enough
            /|\
        GetClientSize(); -> possibly trimmed by scrollbars
            /|\
        mainWin_->GetClientSize()  -> also trimmed, since it's a sub-window!
    */

    //break this vicious circle:

    //harmonize with Grid::GetSizeAvailableForScrollTarget()!

    //1. calculate row label width independent from scrollbars
    const int mainWinHeightGross = std::max(0, GetSize().GetHeight() - getColumnLabelHeight()); //independent from client sizes and scrollbars!
    const ptrdiff_t logicalHeight = rowLabelWin_->getLogicalHeight();                           //

    const int rowLabelWidth = [&]
    {
        if (drawRowLabel_ && logicalHeight > 0)
        {
            ptrdiff_t yFrom = CalcUnscrolledPosition(wxPoint(0, 0)).y;
            ptrdiff_t yTo   = CalcUnscrolledPosition(wxPoint(0, mainWinHeightGross - 1)).y ;
            yFrom = std::clamp<ptrdiff_t>(yFrom, 0, logicalHeight - 1);
            yTo   = std::clamp<ptrdiff_t>(yTo,   0, logicalHeight - 1);

            const ptrdiff_t rowFrom = rowLabelWin_->getRowAtPos(yFrom);
            const ptrdiff_t rowTo   = rowLabelWin_->getRowAtPos(yTo);
            if (rowFrom >= 0 && rowTo >= 0)
                return rowLabelWin_->getBestWidth(rowFrom, rowTo);
        }
        return 0;
    }();

    //2. update managed windows' sizes: just assume scrollbars are already set correctly, even if they may not be (yet)!
    //this ensures mainWin_->SetVirtualSize() and AdjustScrollbars() are working with the correct main window size, unless sb change later, which triggers a recalculation anyway!
    const wxSize mainWinSize(std::max(0, GetClientSize().GetWidth () - rowLabelWidth),
                             std::max(0, GetClientSize().GetHeight() - getColumnLabelHeight()));

    cornerWin_  ->SetSize(0, 0, rowLabelWidth, getColumnLabelHeight());
    rowLabelWin_->SetSize(0, getColumnLabelHeight(), rowLabelWidth, mainWinSize.GetHeight());
    colLabelWin_->SetSize(rowLabelWidth, 0, mainWinSize.GetWidth(), getColumnLabelHeight());
    mainWin_    ->SetSize(rowLabelWidth, getColumnLabelHeight(), mainWinSize.GetWidth(), mainWinSize.GetHeight());

    //avoid flicker in wxWindowMSW::HandleSize() when calling ::EndDeferWindowPos() where the sub-windows are moved only although they need to be redrawn!
    colLabelWin_->Refresh();
    mainWin_    ->Refresh();

    //3. update scrollbars: "guide wxScrolledHelper to not screw up too much"
    if (updateScrollbar)
    {
        auto setScrollbars2 = [&](int logWidth, int logHeight) //replace SetScrollbars, which loses precision of pixelsPerUnitX for some brain-dead reason
        {
            mainWin_->SetVirtualSize(logWidth, logHeight); //set before calling SetScrollRate():
            //else SetScrollRate() would fail to preserve scroll position when "new virtual pixel-pos > old virtual height"

            int ppsuX = 0; //pixel per scroll unit
            int ppsuY = 0;
            GetScrollPixelsPerUnit(&ppsuX, &ppsuY);

            const int ppsuNew = rowLabelWin_->getRowHeight();
            if (ppsuX != ppsuNew || ppsuY != ppsuNew) //support polling!
                SetScrollRate(ppsuNew, ppsuNew); //internally calls AdjustScrollbars() and GetVirtualSize()!

            AdjustScrollbars(); //lousy wxWidgets design decision: internally calls mainWin_->GetClientSize() without considering impact of scrollbars!
            //Attention: setting scrollbars triggers *synchronous* resize event if scrollbars are shown or hidden! => updateWindowSizes() recursion! (Windows)
        };

        const int mainWinWidthGross = std::max(0, GetSize().GetWidth() - rowLabelWidth);

        if (logicalHeight                      <= mainWinHeightGross &&
            getColWidthsSum(mainWinWidthGross) <= mainWinWidthGross &&
            //this special case needs to be considered *only* when both scrollbars are flexible:
            showScrollbarH_ == SB_SHOW_AUTOMATIC &&
            showScrollbarV_ == SB_SHOW_AUTOMATIC)
            setScrollbars2(0, 0); //no scrollbars required at all! -> wxScrolledWindow requires active help to detect this special case!
        else
        {
            const int logicalWidthTmp = getColWidthsSum(mainWinSize.GetWidth()); //assuming vertical scrollbar stays as it is...
            setScrollbars2(logicalWidthTmp, logicalHeight); //if scrollbars are shown or hidden a new resize event recurses into updateWindowSizes()
            /*
            is there a risk of endless recursion? No, 2-level recursion at most, consider the following 6 cases:

            <----------gw---------->
            <----------nw------>
            ------------------------  /|\   /|\
            |                   |  |   |     |
            |     main window   |  |   nh    |
            |                   |  |   |     gh
            ------------------------  \|/    |
            |                   |  |         |
            ------------------------        \|/
                gw := gross width
                nw := net width := gross width - sb size
                gh := gross height
                nh := net height := gross height - sb size

            There are 6 cases that can occur:
            ---------------------------------
                lw := logical width
                lh := logical height

            1. lw <= gw && lh <= gh  => no scrollbars needed

            2. lw > gw  && lh > gh   => need both scrollbars

            lh > gh
                3. lw <= nw         => need vertical scrollbar only
                4. nw < lw <= gw    => need both scrollbars

            lw > gw
                5. lh <= nh         => need horizontal scrollbar only
                6. nh < lh <= gh    => need both scrollbars
            */
        }
    }
}


wxSize Grid::GetSizeAvailableForScrollTarget(const wxSize& size)
{
    //1. "size == GetSize() == (0, 0)" happens temporarily during initialization
    //2. often it's even (0, 20)
    //3. fuck knows why, but we *temporarily* get "size == GetSize() == (1, 1)" when wxAUI panel containing Grid is dropped
    if (size.x <= 1 || size.y <= 1)
        return {}; //probably best considering calling code in generic/scrlwing.cpp: wxScrollHelper::AdjustScrollbars()

    //1. calculate row label width independent from scrollbars
    const int mainWinHeightGross = std::max(0, size.GetHeight() - getColumnLabelHeight()); //independent from client sizes and scrollbars!
    const ptrdiff_t logicalHeight = rowLabelWin_->getLogicalHeight();                      //

    const int rowLabelWidth = [&]
    {
        if (drawRowLabel_ && logicalHeight > 0)
        {
            ptrdiff_t yFrom = CalcUnscrolledPosition(wxPoint(0, 0)).y;
            ptrdiff_t yTo   = CalcUnscrolledPosition(wxPoint(0, mainWinHeightGross - 1)).y ;
            yFrom = std::clamp<ptrdiff_t>(yFrom, 0, logicalHeight - 1);
            yTo   = std::clamp<ptrdiff_t>(yTo,   0, logicalHeight - 1);

            const ptrdiff_t rowFrom = rowLabelWin_->getRowAtPos(yFrom);
            const ptrdiff_t rowTo   = rowLabelWin_->getRowAtPos(yTo);
            if (rowFrom >= 0 && rowTo >= 0)
                return rowLabelWin_->getBestWidth(rowFrom, rowTo);
        }
        return 0;
    }();

    //2. try(!) to determine scrollbar sizes:
#if GTK_MAJOR_VERSION == 2
    /* Ubuntu 19.10: "scrollbar-spacing" has a default value of 3: https://developer.gnome.org/gtk2/stable/GtkScrolledWindow.html#GtkScrolledWindow--s-scrollbar-spacing
        => the default Ubuntu theme (but also our Gtk2Styles.rc) set it to 0, but still the first call to gtk_widget_style_get() returns 3: why?
        => maybe styles are applied asynchronously? GetClientSize() is affected by this, so can't use!
        => always ignore spacing to get consistent scrollbar dimensions!  */
    GtkScrolledWindow* scrollWin = GTK_SCROLLED_WINDOW(wxWindow::m_widget);
    assert(scrollWin);
    GtkWidget* rangeH = ::gtk_scrolled_window_get_hscrollbar(scrollWin);
    GtkWidget* rangeV = ::gtk_scrolled_window_get_vscrollbar(scrollWin);

    GtkRequisition reqH = {};
    GtkRequisition reqV = {};
    if (rangeH) ::gtk_widget_size_request(rangeH, &reqH);
    if (rangeV) ::gtk_widget_size_request(rangeV, &reqV);
    assert(reqH.width > 0 && reqH.height > 0);
    assert(reqV.width > 0 && reqV.height > 0);

    const wxSize scrollBarSizeTmp(reqV.width, reqH.height);
    assert(scrollBarHeightH_ == 0 || scrollBarHeightH_ == scrollBarSizeTmp.y);
    assert(scrollBarWidthV_  == 0 || scrollBarWidthV_  == scrollBarSizeTmp.x);

#elif GTK_MAJOR_VERSION == 3
    //scrollbar size increases dynamically on mouse-hover!
    //see "overlay scrolling": https://developer.gnome.org/gtk3/stable/GtkScrolledWindow.html#gtk-scrolled-window-set-overlay-scrolling
    //luckily "scrollbar-spacing" is stable on GTK3
    const wxSize scrollBarSizeTmp = GetSize() - GetClientSize();

    //lame hard-coded numbers (from Ubuntu 19.10) and openSuse
    //=> let's have a *close* eye on scrollbar fluctuation!
    assert(scrollBarSizeTmp.x == 0 ||
           scrollBarSizeTmp.x == 6 || scrollBarSizeTmp.x == 13 || //Ubuntu 19.10
           scrollBarSizeTmp.x == 16); //openSuse
    assert(scrollBarSizeTmp.y == 0 ||
           scrollBarSizeTmp.y == 6 || scrollBarSizeTmp.y == 13 || //Ubuntu 19.10
           scrollBarSizeTmp.y == 16); //openSuse
#else
#error unknown GTK version!
#endif
    scrollBarHeightH_ = std::max(scrollBarHeightH_, scrollBarSizeTmp.y);
    scrollBarWidthV_  = std::max(scrollBarWidthV_,  scrollBarSizeTmp.x);
    //this function is called again by wxScrollHelper::AdjustScrollbars() if SB_SHOW_ALWAYS-scrollbars are not yet shown => scrollbar size > 0 eventually!

    //-----------------------------------------------------------------------------
    //harmonize with Grid::updateWindowSizes()!
    wxSize sizeAvail = size - wxSize(rowLabelWidth, getColumnLabelHeight());

    //EXCEPTION: space consumed by SB_SHOW_ALWAYS-scrollbars is *never* available for "scroll target"; see wxScrollHelper::AdjustScrollbars()
    if (showScrollbarH_ == SB_SHOW_ALWAYS)
        sizeAvail.y -= (scrollBarHeightH_ > 0 ? scrollBarHeightH_ : /*fallback:*/ scrollBarWidthV_);
    if (showScrollbarV_ == SB_SHOW_ALWAYS)
        sizeAvail.x -= (scrollBarWidthV_ > 0 ? scrollBarWidthV_ : /*fallback:*/ scrollBarHeightH_);

    return wxSize(std::max(0, sizeAvail.x),
                  std::max(0, sizeAvail.y));
}


void Grid::onKeyDown(wxKeyEvent& event)
{
    int keyCode = event.GetKeyCode();
    if (GetLayoutDirection() == wxLayout_RightToLeft)
    {
        if (keyCode == WXK_LEFT || keyCode == WXK_NUMPAD_LEFT)
            keyCode = WXK_RIGHT;
        else if (keyCode == WXK_RIGHT || keyCode == WXK_NUMPAD_RIGHT)
            keyCode = WXK_LEFT;
    }
    if (event.ShiftDown() && keyCode == WXK_F10) //== alias for menu key
        keyCode = WXK_WINDOWS_MENU;

    const ptrdiff_t rowCount  = getRowCount();
    const ptrdiff_t cursorRow = mainWin_->getCursor();

    auto moveCursorTo = [&](ptrdiff_t row)
    {
        if (rowCount > 0)
            setGridCursor(std::clamp<ptrdiff_t>(row, 0, rowCount - 1), GridEventPolicy::allow);
    };

    auto selectWithCursorTo = [&](ptrdiff_t row)
    {
        if (rowCount > 0)
        {
            row = std::clamp<ptrdiff_t>(row, 0, rowCount - 1);
            const ptrdiff_t anchorRow = mainWin_->getAnchor();

            mainWin_->setCursor(row, anchorRow);
            makeRowVisible(row);

            selection_.clear(); //clear selection, do NOT fire event

            const ptrdiff_t rowFirst = std::min(anchorRow, row);     //sort + convert to half-open range
            const ptrdiff_t rowLast  = std::max(anchorRow, row) + 1; //
            selectRange(rowFirst, rowLast, true /*positive*/, GridEventPolicy::allow); //set new selection + fire event
        }
    };

    switch (keyCode)
    {
        case WXK_MENU:         //simulate right mouse click at cursor row position (on lower edge)
        case WXK_WINDOWS_MENU: //(but truncate to window if cursor is out of view)
        {
            const size_t row = std::min(mainWin_->getCursor(), getRowCount());

            const int clientPosMainWinY = std::clamp(CalcScrolledPosition(wxPoint(0, rowLabelWin_->getRowHeight() * (row + 1))).y - 1, //logical -> window coordinates
                                                     0, mainWin_->GetClientSize().GetHeight() - 1);

            const wxPoint mousePos = mainWin_->GetPosition() + wxPoint(0, clientPosMainWinY); //mainWin_-relative to Grid-relative

            GridContextMenuEvent contextEvent(mousePos);
            GetEventHandler()->ProcessEvent(contextEvent);
        }
        return;

        //case WXK_TAB:
        //    if (Navigate(event.ShiftDown() ? wxNavigationKeyEvent::IsBackward : wxNavigationKeyEvent::IsForward))
        //        return;
        //    break;

        case WXK_UP:
        case WXK_NUMPAD_UP:
            if (event.ShiftDown())
                selectWithCursorTo(cursorRow - 1);
            else if (event.ControlDown())
                scrollDelta(0, -1);
            else
                moveCursorTo(cursorRow - 1);
            return; //swallow event: wxScrolledWindow, wxWidgets 2.9.3 on Kubuntu x64 processes arrow keys: prevent this!

        case WXK_DOWN:
        case WXK_NUMPAD_DOWN:
            if (event.ShiftDown())
                selectWithCursorTo(cursorRow + 1);
            else if (event.ControlDown())
                scrollDelta(0, 1);
            else
                moveCursorTo(cursorRow + 1);
            return; //swallow event

        case WXK_LEFT:
        case WXK_NUMPAD_LEFT:
            if (event.ControlDown())
                scrollDelta(-1, 0);
            else if (event.ShiftDown())
                ;
            else
                moveCursorTo(cursorRow);
            return;

        case WXK_RIGHT:
        case WXK_NUMPAD_RIGHT:
            if (event.ControlDown())
                scrollDelta(1, 0);
            else if (event.ShiftDown())
                ;
            else
                moveCursorTo(cursorRow);
            return;

        case WXK_HOME:
        case WXK_NUMPAD_HOME:
            if (event.ShiftDown())
                selectWithCursorTo(0);
            //else if (event.ControlDown())
            //    ;
            else
                moveCursorTo(0);
            return;

        case WXK_END:
        case WXK_NUMPAD_END:
            if (event.ShiftDown())
                selectWithCursorTo(rowCount - 1);
            //else if (event.ControlDown())
            //    ;
            else
                moveCursorTo(rowCount - 1);
            return;

        case WXK_PAGEUP:
        case WXK_NUMPAD_PAGEUP:
            if (event.ShiftDown())
                selectWithCursorTo(cursorRow - rowLabelWin_->GetClientSize().GetHeight() / rowLabelWin_->getRowHeight());
            //else if (event.ControlDown())
            //    ;
            else
                moveCursorTo(cursorRow - rowLabelWin_->GetClientSize().GetHeight() / rowLabelWin_->getRowHeight());
            return;

        case WXK_PAGEDOWN:
        case WXK_NUMPAD_PAGEDOWN:
            if (event.ShiftDown())
                selectWithCursorTo(cursorRow + rowLabelWin_->GetClientSize().GetHeight() / rowLabelWin_->getRowHeight());
            //else if (event.ControlDown())
            //    ;
            else
                moveCursorTo(cursorRow + rowLabelWin_->GetClientSize().GetHeight() / rowLabelWin_->getRowHeight());
            return;

        case 'A':  //Ctrl + A - select all
            if (event.ControlDown())
                selectRange(0, rowCount, true /*positive*/, GridEventPolicy::allow);
            break;

        case WXK_NUMPAD_ADD: //CTRL + '+' - auto-size all
            if (event.ControlDown())
                autoSizeColumns(GridEventPolicy::allow);
            return;
    }

    event.Skip();
}


void Grid::setColumnLabelHeight(int height)
{
    colLabelWin_->setColumnLabelHeight(height);
    updateWindowSizes();
}


int Grid::getColumnLabelHeight() const { return colLabelWin_->getColumnLabelHeight(); }


void Grid::showRowLabel(bool show)
{
    drawRowLabel_ = show;
    updateWindowSizes();
}


void Grid::selectRange(size_t rowFirst, size_t rowLast, bool positive, GridEventPolicy rangeEventPolicy)
{
    selectRange2(rowFirst, rowLast, positive, nullptr /*mouseClick*/, rangeEventPolicy);
}


void Grid::selectRange2(size_t rowFirst, size_t rowLast, bool positive, const GridClickEvent* mouseClick, GridEventPolicy rangeEventPolicy)
{
    assert(rowFirst <= rowLast);
    assert(getRowCount() == selection_.gridSize());
    rowFirst = std::clamp<size_t>(rowFirst, 0, selection_.gridSize());
    rowLast  = std::clamp<size_t>(rowLast,  0, selection_.gridSize());

    if (rowFirst < rowLast && !selection_.matchesRange(rowFirst, rowLast, positive))
    {
        selection_.selectRange(rowFirst, rowLast, positive);
        mainWin_->Refresh();
    }

    //issue event even for unchanged selection! e.g. MainWin::onMouseDown() temporarily clears range with GridEventPolicy::deny!
    if (rangeEventPolicy == GridEventPolicy::allow)
    {
        GridSelectEvent selEvent(rowFirst, rowLast, positive, mouseClick);
        [[maybe_unused]] const bool processed = GetEventHandler()->ProcessEvent(selEvent);
    }
}

void Grid::selectRow(size_t row, GridEventPolicy rangeEventPolicy) { selectRange(row, row + 1,             true  /*positive*/, rangeEventPolicy); }
void Grid::selectAllRows        (GridEventPolicy rangeEventPolicy) { selectRange(0, selection_.gridSize(), true  /*positive*/, rangeEventPolicy); }
void Grid::clearSelection       (GridEventPolicy rangeEventPolicy) { selectRange(0, selection_.gridSize(), false /*positive*/, rangeEventPolicy); }


void Grid::scrollDelta(int deltaX, int deltaY)
{
    const wxPoint scrollPosOld = GetViewStart();

    wxPoint scrollPosNew = scrollPosOld;
    scrollPosNew.x += deltaX;
    scrollPosNew.y += deltaY;

    scrollPosNew.x = std::max(0, scrollPosNew.x); //wxScrollHelper::Scroll() will exit prematurely if input happens to be "-1"!
    scrollPosNew.y = std::max(0, scrollPosNew.y); //

    if (scrollPosNew != scrollPosOld)
    {
        Scroll(scrollPosNew); //internally calls wxWindows::Update()!
        updateWindowSizes(); //may show horizontal scroll bar if row column gets wider
    }
}


size_t Grid::getRowCount() const
{
    return dataView_ ? dataView_->getRowCount() : 0;
}


void Grid::Refresh(bool eraseBackground, const wxRect* rect)
{
    const size_t rowCountNew = getRowCount();
    if (rowCountOld_ != rowCountNew)
    {
        rowCountOld_ = rowCountNew;
        updateWindowSizes();
    }

    if (selection_.gridSize() != rowCountNew)
    {
        const bool priorSelection = !selection_.matchesRange(0, selection_.gridSize(), false /*positive*/);

        selection_.resize(rowCountNew);

        if (priorSelection) //clear selection only when needed
        {
            //clearSelection(GridEventPolicy::allow); -> no, we need async event to make filegrid::refresh(*m_gridMainL, *m_gridMainC, *m_gridMainR) work
            selection_.clear();
            GetEventHandler()->AddPendingEvent(GridSelectEvent(0, rowCountNew, false /*positive*/, nullptr /*mouseClick*/));
        }
    }

    wxScrolledWindow::Refresh(eraseBackground, rect);
}


void Grid::setRowHeight(int height)
{
    rowLabelWin_->setRowHeight(height);
    updateWindowSizes();
    Refresh();
}


int Grid::getRowHeight() const { return rowLabelWin_->getRowHeight(); }


void Grid::setColumnConfig(const std::vector<Grid::ColAttributes>& attr)
{
    //hold ownership of non-visible columns
    oldColAttributes_ = attr;

    std::vector<VisibleColumn> visCols;
    for (const ColAttributes& ca : attr)
    {
        assert(ca.stretch >= 0);
        assert(ca.type != ColumnType::none);

        if (ca.visible)
            visCols.push_back({ca.type, ca.offset, std::max(ca.stretch, 0)});
    }

    //"ownership" of visible columns is now within Grid
    visibleCols_ = std::move(visCols);

    updateWindowSizes();
    Refresh();
}


std::vector<Grid::ColAttributes> Grid::getColumnConfig() const
{
    //get non-visible columns (+ outdated visible ones)
    std::vector<ColAttributes> output = oldColAttributes_;

    auto itVcols    = visibleCols_.begin();
    auto itVcolsend = visibleCols_.end();

    //update visible columns but keep order of non-visible ones!
    for (ColAttributes& ca : output)
        if (ca.visible)
        {
            if (itVcols != itVcolsend)
            {
                ca.type    = itVcols->type;
                ca.stretch = itVcols->stretch;
                ca.offset  = itVcols->offset;
                ++itVcols;
            }
            else
                assert(false);
        }
    assert(itVcols == itVcolsend);

    return output;
}


void Grid::showScrollBars(Grid::ScrollBarStatus horizontal, Grid::ScrollBarStatus vertical)
{
    if (showScrollbarH_ == horizontal &&
        showScrollbarV_ == vertical) return; //support polling!

    showScrollbarH_ = horizontal;
    showScrollbarV_ = vertical;

    //the following wxGTK approach is pretty much identical to wxWidgets 2.9 ShowScrollbars() code!

    auto mapStatus = [](ScrollBarStatus sbStatus) -> GtkPolicyType
    {
        switch (sbStatus)
        {
            case SB_SHOW_AUTOMATIC:
                return GTK_POLICY_AUTOMATIC;
            case SB_SHOW_ALWAYS:
                return GTK_POLICY_ALWAYS;
            case SB_SHOW_NEVER:
                return GTK_POLICY_NEVER;
        }
        assert(false);
        return GTK_POLICY_AUTOMATIC;
    };

    GtkScrolledWindow* scrollWin = GTK_SCROLLED_WINDOW(wxWindow::m_widget);
    assert(scrollWin);
    ::gtk_scrolled_window_set_policy(scrollWin,
                                     mapStatus(horizontal),
                                     mapStatus(vertical));

    updateWindowSizes();
}



wxWindow& Grid::getCornerWin  () { return *cornerWin_;   }
wxWindow& Grid::getRowLabelWin() { return *rowLabelWin_; }
wxWindow& Grid::getColLabelWin() { return *colLabelWin_; }
wxWindow& Grid::getMainWin    () { return *mainWin_;     }
const wxWindow& Grid::getMainWin() const { return *mainWin_; }


void Grid::moveColumn(size_t colFrom, size_t colTo)
{
    if (colFrom < visibleCols_.size() &&
        colTo   < visibleCols_.size() &&
        colTo != colFrom)
    {
        const VisibleColumn colAtt = visibleCols_[colFrom];
        visibleCols_.erase (visibleCols_.begin() + colFrom);
        visibleCols_.insert(visibleCols_.begin() + colTo, colAtt);
    }
}


ColumnType Grid::colToType(size_t col) const
{
    if (col < visibleCols_.size())
        return visibleCols_[col].type;
    return ColumnType::none;
}


Grid::ColumnPosInfo Grid::getColumnAtWinPos(int posX) const
{
    if (const int absX = CalcUnscrolledPosition(wxPoint(posX, 0)).x;
        absX >= 0)
    {
        int accWidth = 0;
        for (const ColumnWidth& cw : getColWidths())
        {
            accWidth += cw.width;
            if (absX < accWidth)
                return {cw.type, absX + cw.width - accWidth, cw.width};
        }
    }
    return {ColumnType::none, 0, 0};
}


ptrdiff_t Grid::getRowAtWinPos(int posY) const
{
    const int absY = CalcUnscrolledPosition(wxPoint(0, posY)).y;
    return rowLabelWin_->getRowAtPos(absY); //return -1 for invalid position, rowCount if past the end
}


std::pair<ptrdiff_t, ptrdiff_t> Grid::getVisibleRows(const wxRect& clientRect) const //returns range [begin, end)
{
    if (clientRect.height > 0)
    {
        const int rowFrom = getRowAtWinPos(clientRect.y);
        const int rowTo   = getRowAtWinPos(clientRect.GetBottom());

        return {std::max(rowFrom, 0),
                std::min<ptrdiff_t>((rowTo) + 1, getRowCount())};
    }
    return {};
}


wxRect Grid::getColumnLabelArea(ColumnType colType) const
{
    const std::vector<ColumnWidth>& absWidths = getColWidths(); //resolve negative/stretched widths

    //colType is not unique in general, but *this* function expects it!
    assert(std::count_if(absWidths.begin(), absWidths.end(), [&](const ColumnWidth& cw) { return cw.type == colType; }) <= 1);

    auto itCol = std::find_if(absWidths.begin(), absWidths.end(), [&](const ColumnWidth& cw) { return cw.type == colType; });
    if (itCol != absWidths.end())
    {
        ptrdiff_t posX = 0;
        std::for_each(absWidths.begin(), itCol,
        [&](const ColumnWidth& cw) { posX += cw.width; });

        return wxRect(wxPoint(posX, 0), wxSize(itCol->width, getColumnLabelHeight()));
    }
    return wxRect();
}


void Grid::refreshCell(size_t row, ColumnType colType)
{
    const wxRect& colArea = getColumnLabelArea(colType); //returns empty rect if column not found
    const wxRect& rowArea = rowLabelWin_->getRowLabelArea(row); //returns empty rect if row not found
    if (colArea.width > 0 && rowArea.height > 0)
    {
        const wxPoint topLeft = CalcScrolledPosition(wxPoint(colArea.x, rowArea.y)); //logical -> window coordinates
        const wxRect cellArea(topLeft, wxSize(colArea.width, rowArea.height));

        getMainWin().RefreshRect(cellArea);
    }
}


void Grid::setGridCursor(size_t row, GridEventPolicy rangeEventPolicy)
{
    mainWin_->setCursor(row, row);
    makeRowVisible(row);

    selection_.clear(); //clear selection, do NOT fire event
    selectRow(row, rangeEventPolicy); //set new selection + fire event
}


void Grid::makeRowVisible(size_t row)
{
    const wxRect labelRect = rowLabelWin_->getRowLabelArea(row); //returns empty rect if row not found
    if (labelRect.height > 0)
    {
        int pixelsPerUnitY = 0;
        GetScrollPixelsPerUnit(nullptr, &pixelsPerUnitY);
        if (pixelsPerUnitY > 0)
        {
            const wxPoint scrollPosOld = GetViewStart();

            const int clientPosY = CalcScrolledPosition(labelRect.GetTopLeft()).y;
            if (clientPosY < 0)
            {
                const int scrollPosNewY = labelRect.y / pixelsPerUnitY;
                Scroll(scrollPosOld.x, scrollPosNewY); //internally calls wxWindows::Update()!
                updateWindowSizes(); //may show horizontal scroll bar if row column gets wider
                Refresh();
            }
            else if (clientPosY + labelRect.height > rowLabelWin_->GetClientSize().GetHeight())
            {
                auto execScroll = [&](int clientHeight)
                {
                    const int scrollPosNewY = numeric::intDivCeil(labelRect.y + labelRect.height - clientHeight, pixelsPerUnitY);
                    Scroll(scrollPosOld.x, scrollPosNewY);
                    updateWindowSizes(); //may show horizontal scroll bar if row column gets wider
                    Refresh();
                };

                const int clientHeightBefore = rowLabelWin_->GetClientSize().GetHeight();
                execScroll(clientHeightBefore);

                //client height may decrease after scroll due to a new horizontal scrollbar, resulting in a partially visible last row
                const int clientHeightAfter = rowLabelWin_->GetClientSize().GetHeight();
                if (clientHeightAfter < clientHeightBefore)
                    execScroll(clientHeightAfter);
            }
        }
    }
}


void Grid::scrollTo(size_t row)
{
    const wxRect labelRect = rowLabelWin_->getRowLabelArea(row); //returns empty rect if row not found
    if (labelRect.height > 0)
    {
        int pixelsPerUnitY = 0;
        GetScrollPixelsPerUnit(nullptr, &pixelsPerUnitY);
        if (pixelsPerUnitY > 0)
        {
            const int scrollPosNewY = labelRect.y / pixelsPerUnitY;
            const wxPoint scrollPosOld = GetViewStart();

            if (scrollPosOld.y != scrollPosNewY) //support polling
            {
                Scroll(scrollPosOld.x, scrollPosNewY); //internally calls wxWindows::Update()!
                updateWindowSizes(); //may show horizontal scroll bar if row column gets wider
                Refresh();
            }
        }
    }
}


bool Grid::Enable(bool enable)
{
    Refresh();
    return wxScrolledWindow::Enable(enable);
}


size_t Grid::getGridCursor() const
{
    return mainWin_->getCursor();
}


int Grid::getBestColumnSize(size_t col) const
{
    if (dataView_ && col < visibleCols_.size())
    {
        const ColumnType type = visibleCols_[col].type;

        wxClientDC dc(mainWin_);
        dc.SetFont(mainWin_->GetFont()); //harmonize with MainWin::render()

        const auto& [rowFirst, rowLast] = getVisibleRows(mainWin_->GetClientRect());

        int maxSize = 0;
        for (auto row = rowFirst; row < rowLast; ++row)
            maxSize = std::max(maxSize, dataView_->getBestSize(dc, row, type));

        return maxSize;
    }
    return -1;
}


void Grid::setColumnWidth(int width, size_t col, GridEventPolicy columnResizeEventPolicy, bool notifyAsync)
{
    if (col < visibleCols_.size())
    {
        VisibleColumn& vcRs = visibleCols_[col];

        const std::vector<int> stretchedWidths = getColStretchedWidths(mainWin_->GetClientSize().GetWidth());
        if (stretchedWidths.size() != visibleCols_.size())
        {
            assert(false);
            return;
        }
        //CAVEATS:
        //I. fixed-size columns: normalize offset so that resulting width is at least COLUMN_MIN_WIDTH_DIP: this is NOT enforced by getColWidths()!
        //II. stretched columns: do not allow user to set offsets so small that they result in negative (non-normalized) widths: this gives an
        //unusual delay when enlarging the column again later
        width = std::max(width, dipToWxsize(COLUMN_MIN_WIDTH_DIP));

        vcRs.offset = width - stretchedWidths[col]; //width := stretchedWidth + offset

        //III. resizing any column should normalize *all* other stretched columns' offsets considering current mainWinWidth!
        // test case:
        //1. have columns, both fixed-size and stretched, fit whole window width
        //2. shrink main window width so that horizontal scrollbars are shown despite the streched column
        //3. shrink a fixed-size column so that the scrollbars vanish and columns cover full width again
        //4. now verify that the stretched column is resizing immediately if main window is enlarged again
        for (size_t col2 = 0; col2 < visibleCols_.size(); ++col2)
            if (visibleCols_[col2].stretch > 0) //normalize stretched columns only
                visibleCols_[col2].offset = std::max(visibleCols_[col2].offset, dipToWxsize(COLUMN_MIN_WIDTH_DIP) - stretchedWidths[col2]);

        if (columnResizeEventPolicy == GridEventPolicy::allow)
        {
            GridColumnResizeEvent sizeEvent(vcRs.offset, vcRs.type);
            if (notifyAsync)
                GetEventHandler()->AddPendingEvent(sizeEvent);
            else
                GetEventHandler()->ProcessEvent(sizeEvent);
        }
    }
    else
        assert(false);
}


void Grid::autoSizeColumns(GridEventPolicy columnResizeEventPolicy)
{
    if (allowColumnResize_)
    {
        for (size_t col = 0; col < visibleCols_.size(); ++col)
        {
            const int bestWidth = getBestColumnSize(col); //return -1 on error
            if (bestWidth >= 0)
                setColumnWidth(bestWidth, col, columnResizeEventPolicy, true /*notifyAsync*/);
        }
        updateWindowSizes();
        Refresh();
    }
}


std::vector<int> Grid::getColStretchedWidths(int clientWidth) const //final width = (normalized) (stretchedWidth + offset)
{
    assert(clientWidth >= 0);
    clientWidth = std::max(clientWidth, 0);
    int stretchTotal = 0;
    for (const VisibleColumn& vc : visibleCols_)
    {
        assert(vc.stretch >= 0);
        stretchTotal += vc.stretch;
    }

    int remainingWidth = clientWidth;

    std::vector<int> output;

    if (stretchTotal <= 0)
        output.resize(visibleCols_.size()); //fill with zeros
    else
    {
        for (const VisibleColumn& vc : visibleCols_)
        {
            const int width = clientWidth * vc.stretch / stretchTotal; //rounds down!
            output.push_back(width);
            remainingWidth -= width;
        }

        //distribute *all* of clientWidth: should suffice to enlarge the first few stretched columns; no need to minimize total absolute error of distribution
        if (remainingWidth > 0)
            for (size_t col2 = 0; col2 < visibleCols_.size(); ++col2)
                if (visibleCols_[col2].stretch > 0)
                {
                    ++output[col2];
                    if (--remainingWidth == 0)
                        break;
                }
        assert(remainingWidth == 0);
    }
    return output;
}


std::vector<Grid::ColumnWidth> Grid::getColWidths() const
{
    return getColWidths(mainWin_->GetClientSize().GetWidth());
}


std::vector<Grid::ColumnWidth> Grid::getColWidths(int mainWinWidth) const //evaluate stretched columns
{
    const std::vector<int> stretchedWidths = getColStretchedWidths(mainWinWidth);
    assert(stretchedWidths.size() == visibleCols_.size());

    std::vector<ColumnWidth> output;
    for (size_t col2 = 0; col2 < visibleCols_.size(); ++col2)
    {
        const auto& vc = visibleCols_[col2];
        int width = stretchedWidths[col2] + vc.offset;

        if (vc.stretch > 0)
            width = std::max(width, dipToWxsize(COLUMN_MIN_WIDTH_DIP)); //normalization really needed here: e.g. smaller main window would result in negative width
        else
            width = std::max(width, 0); //support smaller width than COLUMN_MIN_WIDTH_DIP if set via configuration

        output.push_back({vc.type, width});
    }
    return output;
}


int Grid::getColWidthsSum(int mainWinWidth) const
{
    int sum = 0;
    for (const ColumnWidth& cw : getColWidths(mainWinWidth))
        sum += cw.width;
    return sum;
}

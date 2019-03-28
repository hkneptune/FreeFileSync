// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "grid.h"
#include <cassert>
#include <set>
#include <chrono>
#include <wx/settings.h>
#include <wx/listbox.h>
#include <wx/tooltip.h>
#include <wx/timer.h>
#include <wx/utils.h>
#include <zen/string_tools.h>
#include <zen/scope_guard.h>
#include <zen/utf.h>
#include <zen/zstring.h>
#include <zen/format_unit.h>
#include "dc.h"

    #include <gtk/gtk.h>

using namespace zen;


//let's NOT create wxWidgets objects statically:
wxColor GridData::getColorSelectionGradientFrom() { return { 137, 172, 255 }; } //blue: HSL: 158, 255, 196   HSV: 222, 0.46, 1
wxColor GridData::getColorSelectionGradientTo  () { return { 225, 234, 255 }; } //      HSL: 158, 255, 240   HSV: 222, 0.12, 1

int GridData::getColumnGapLeft() { return fastFromDIP(4); }


namespace
{
//------------------------------ Grid Parameters --------------------------------
inline wxColor getColorLabelText() { return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT); }
inline wxColor getColorGridLine() { return { 192, 192, 192 }; } //light grey

inline wxColor getColorLabelGradientFrom() { return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW); }
inline wxColor getColorLabelGradientTo  () { return { 200, 200, 200 }; } //light grey

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

/*
IsEnabled() vs IsThisEnabled() since wxWidgets 2.9.5:

void wxWindowBase::NotifyWindowOnEnableChange(), called from bool wxWindowBase::Enable(), fails to refresh
child elements when disabling a IsTopLevel() dialog, e.g. when showing a modal dialog.
The unfortunate effect on XP for using IsEnabled() when rendering the grid is that the user can move the modal dialog
and *draw* with it on the background while the grid refreshes as disabled incrementally!

=> Don't use IsEnabled() since it considers the top level window, but a disabled top-level should NOT
lead to child elements being rendered disabled!

=> IsThisEnabled() OTOH is too shallow and does not consider parent windows which are not top level.

The perfect solution would be a bool renderAsEnabled() { return "IsEnabled() but ignore effects of showing a modal dialog"; }

However "IsThisEnabled()" is good enough (same like the old IsEnabled() on wxWidgets 2.8.12) and it avoids this pathetic behavior on XP.
(Similar problem on Win 7: e.g. directly click sync button without comparing first)

=> 2018-07-30: roll our own:
*/
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
const wxEventType zen::EVENT_GRID_MOUSE_LEFT_DOUBLE     = wxNewEventType();
const wxEventType zen::EVENT_GRID_MOUSE_LEFT_DOWN       = wxNewEventType();
const wxEventType zen::EVENT_GRID_MOUSE_LEFT_UP         = wxNewEventType();
const wxEventType zen::EVENT_GRID_MOUSE_RIGHT_DOWN      = wxNewEventType();
const wxEventType zen::EVENT_GRID_MOUSE_RIGHT_UP        = wxNewEventType();
const wxEventType zen::EVENT_GRID_SELECT_RANGE          = wxNewEventType();
const wxEventType zen::EVENT_GRID_COL_LABEL_MOUSE_LEFT  = wxNewEventType();
const wxEventType zen::EVENT_GRID_COL_LABEL_MOUSE_RIGHT = wxNewEventType();
const wxEventType zen::EVENT_GRID_COL_RESIZE            = wxNewEventType();
//----------------------------------------------------------------------------------------------------------------

void GridData::renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected)
{
    drawCellBackground(dc, rect, enabled, selected, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
}


void GridData::renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover)
{
    wxRect rectTmp = drawCellBorder(dc, rect);

    rectTmp.x     += getColumnGapLeft();
    rectTmp.width -= getColumnGapLeft();
    drawCellText(dc, rectTmp, getValue(row, colType));
}


int GridData::getBestSize(wxDC& dc, size_t row, ColumnType colType)
{
    return dc.GetTextExtent(getValue(row, colType)).GetWidth() + 2 * getColumnGapLeft() + 1; //gap on left and right side + border
}


wxRect GridData::drawCellBorder(wxDC& dc, const wxRect& rect) //returns remaining rectangle
{
    wxDCPenChanger dummy2(dc, getColorGridLine());
    dc.DrawLine(rect.GetBottomLeft(),  rect.GetBottomRight());
    dc.DrawLine(rect.GetBottomRight(), rect.GetTopRight() + wxPoint(0, -1));

    return wxRect(rect.GetTopLeft(), wxSize(rect.width - 1, rect.height - 1));
}


void GridData::drawCellBackground(wxDC& dc, const wxRect& rect, bool enabled, bool selected, const wxColor& backgroundColor)
{
    if (enabled)
    {
        if (selected)
            dc.GradientFillLinear(rect, getColorSelectionGradientFrom(), getColorSelectionGradientTo(), wxEAST);
        else
            clearArea(dc, rect, backgroundColor);
    }
    else
        clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
}


wxSize GridData::drawCellText(wxDC& dc, const wxRect& rect, const std::wstring& text, int alignment)
{
    /*
    performance notes (Windows):
    - wxDC::GetTextExtent() is by far the most expensive call (20x more expensive than wxDC::DrawText())
    - wxDC::DrawLabel() is inefficiently implemented; internally calls: wxDC::GetMultiLineTextExtent(), wxDC::GetTextExtent(), wxDC::DrawText()
    - wxDC::GetMultiLineTextExtent() calls wxDC::GetTextExtent()
    - wxDC::DrawText also calls wxDC::GetTextExtent()!!
    => wxDC::DrawLabel() boils down to 3(!) calls to wxDC::GetTextExtent()!!!
    - wxDC::DrawLabel results in GetTextExtent() call even for empty strings!!!
    => skip the wxDC::DrawLabel() cruft and directly call wxDC::DrawText!
    */

    //truncate large texts and add ellipsis
    assert(!contains(text, L"\n"));

    std::wstring textTrunc = text;
    wxSize extentTrunc = dc.GetTextExtent(textTrunc);
    if (extentTrunc.GetWidth() > rect.width)
    {
        //unlike Windows 7 Explorer, we truncate UTF-16 correctly: e.g. CJK-Ideogramm encodes to TWO wchar_t: utfTo<std::wstring>("\xf0\xa4\xbd\x9c");
        size_t low  = 0;                   //number of unicode chars!
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

                const std::wstring& candidate = getUnicodeSubstring(text, 0, middle) + ELLIPSIS;
                const wxSize extentCand = dc.GetTextExtent(candidate); //perf: most expensive call of this routine!

                if (extentCand.GetWidth() <= rect.width)
                {
                    low = middle;
                    textTrunc   = candidate;
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
        pt.x += static_cast<int>(std::floor((rect.width - extentTrunc.GetWidth()) / 2.0)); //round down negative values, too!

    if (alignment & wxALIGN_BOTTOM) //note: wxALIGN_TOP == 0!
        pt.y += rect.height - extentTrunc.GetHeight();
    else if (alignment & wxALIGN_CENTER_VERTICAL)
        pt.y += static_cast<int>(std::floor((rect.height - extentTrunc.GetHeight()) / 2.0)); //round down negative values, too!

    RecursiveDcClipper clip(dc, rect);
    dc.DrawText(textTrunc, pt);
    return extentTrunc;
}


void GridData::renderColumnLabel(Grid& grid, wxDC& dc, const wxRect& rect, ColumnType colType, bool highlighted)
{
    wxRect rectRemain = drawColumnLabelBackground(dc, rect, highlighted);

    rectRemain.x     += getColumnGapLeft();
    rectRemain.width -= getColumnGapLeft();
    drawColumnLabelText(dc, rectRemain, getColumnLabel(colType));
}


wxRect GridData::drawColumnLabelBackground(wxDC& dc, const wxRect& rect, bool highlighted)
{
    //left border
    {
        wxDCPenChanger dummy(dc, *wxWHITE_PEN);
        dc.DrawLine(rect.GetTopLeft(), rect.GetBottomLeft());
    }
    //bottom, right border
    {
        wxDCPenChanger dummy(dc, wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW));
        dc.GradientFillLinear(wxRect(rect.GetTopRight(), rect.GetBottomRight()), getColorLabelGradientFrom(), dc.GetPen().GetColour(), wxSOUTH);
        dc.DrawLine(rect.GetBottomLeft(), rect.GetBottomRight() + wxPoint(1, 0));
    }

    wxRect rectInside(rect.x + 1, rect.y, rect.width - 2, rect.height - 1);
    if (highlighted)
        dc.GradientFillLinear(rectInside, getColorLabelGradientFocusFrom(), getColorLabelGradientFocusTo(), wxSOUTH);
    else //regular background gradient
        dc.GradientFillLinear(rectInside, getColorLabelGradientFrom(), getColorLabelGradientTo(), wxSOUTH);

    return wxRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2); //we really don't like wxRect::Deflate, do we?
}


void GridData::drawColumnLabelText(wxDC& dc, const wxRect& rect, const std::wstring& text)
{
    wxDCTextColourChanger dummy(dc, getColorLabelText()); //accessibility: always set both foreground AND background colors!
    drawCellText(dc, rect, text, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
}

//----------------------------------------------------------------------------------------------------------------
/*
                  SubWindow
                     /|\
                      |
     -----------------------------------
    |           |            |          |
CornerWin  RowLabelWin  ColLabelWin  MainWin

*/
class Grid::SubWindow : public wxWindow
{
public:
    SubWindow(Grid& parent) :
        wxWindow(&parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBORDER_NONE, wxPanelNameStr),
        parent_(parent)
    {
        Connect(wxEVT_PAINT, wxPaintEventHandler(SubWindow::onPaintEvent), nullptr, this);
        Connect(wxEVT_SIZE,  wxSizeEventHandler (SubWindow::onSizeEvent),  nullptr, this);
        Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent& event) {}); //http://wiki.wxwidgets.org/Flicker-Free_Drawing

        //SetDoubleBuffered(true); slow as hell!

        SetBackgroundStyle(wxBG_STYLE_PAINT);

        Connect(wxEVT_SET_FOCUS,   wxFocusEventHandler(SubWindow::onFocus), nullptr, this);
        Connect(wxEVT_KILL_FOCUS,  wxFocusEventHandler(SubWindow::onFocus), nullptr, this);
        Connect(wxEVT_CHILD_FOCUS, wxEventHandler(SubWindow::onChildFocus), nullptr, this);

        Connect(wxEVT_LEFT_DOWN,    wxMouseEventHandler(SubWindow::onMouseLeftDown  ), nullptr, this);
        Connect(wxEVT_LEFT_UP,      wxMouseEventHandler(SubWindow::onMouseLeftUp    ), nullptr, this);
        Connect(wxEVT_LEFT_DCLICK,  wxMouseEventHandler(SubWindow::onMouseLeftDouble), nullptr, this);
        Connect(wxEVT_RIGHT_DOWN,   wxMouseEventHandler(SubWindow::onMouseRightDown ), nullptr, this);
        Connect(wxEVT_RIGHT_UP,     wxMouseEventHandler(SubWindow::onMouseRightUp   ), nullptr, this);
        Connect(wxEVT_MOTION,       wxMouseEventHandler(SubWindow::onMouseMovement  ), nullptr, this);
        Connect(wxEVT_LEAVE_WINDOW, wxMouseEventHandler(SubWindow::onLeaveWindow    ), nullptr, this);
        Connect(wxEVT_MOUSEWHEEL,   wxMouseEventHandler(SubWindow::onMouseWheel     ), nullptr, this);
        Connect(wxEVT_MOUSE_CAPTURE_LOST, wxMouseCaptureLostEventHandler(SubWindow::onMouseCaptureLost), nullptr, this);

        Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(SubWindow::onKeyDown), nullptr, this);
        Connect(wxEVT_KEY_UP,   wxKeyEventHandler(SubWindow::onKeyUp  ), nullptr, this);

        assert(GetClientAreaOrigin() == wxPoint()); //generally assumed when dealing with coordinates below
    }
    Grid& refParent() { return parent_; }
    const Grid& refParent() const { return parent_; }

    template <class T>
    bool sendEventToParent(T&& event) //take both "rvalue + lvalues", return "true" if a suitable event handler function was found and executed, and the function did not call wxEvent::Skip.
    {
        if (wxEvtHandler* evtHandler = parent_.GetEventHandler())
            return evtHandler->ProcessEvent(event);
        return false;
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

    virtual void onFocus(wxFocusEvent& event) { event.Skip(); }
    virtual void onChildFocus(wxEvent& event) {} //wxGTK::wxScrolledWindow automatically scrolls to child window when child gets focus -> prevent!

    virtual void onMouseLeftDown  (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseLeftUp    (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseLeftDouble(wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseRightDown (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseRightUp   (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseMovement  (wxMouseEvent& event) { event.Skip(); }
    virtual void onLeaveWindow    (wxMouseEvent& event) { event.Skip(); }
    virtual void onMouseCaptureLost(wxMouseCaptureLostEvent& event) { event.Skip(); }

    void onKeyDown(wxKeyEvent& event)
    {
        if (!sendEventToParent(event)) //let parent collect all key events
            event.Skip();
    }

    void onKeyUp(wxKeyEvent& event)
    {
        if (!sendEventToParent(event)) //let parent collect all key events
            event.Skip();
    }

    void onMouseWheel(wxMouseEvent& event)
    {
        /*
          MSDN, WM_MOUSEWHEEL: "Sent to the focus window when the mouse wheel is rotated.
          The DefWindowProc function propagates the message to the window's parent.
          There should be no internal forwarding of the message, since DefWindowProc propagates
          it up the parent chain until it finds a window that processes it."

          On OS X there is no such propagation! => we need a redirection (the same wxGrid implements)
         */

        //new wxWidgets 3.0 screw-up for GTK2: wxScrollHelperEvtHandler::ProcessEvent() ignores wxEVT_MOUSEWHEEL events
        //thereby breaking the scenario of redirection to parent we need here (but also breaking their very own wxGrid sample)
        //=> call wxScrolledWindow mouse wheel handler directly
        parent_.HandleOnMouseWheel(event);

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

    void onSizeEvent(wxSizeEvent& event)
    {
        Refresh();
        event.Skip();
    }

    Grid& parent_;
    std::optional<wxBitmap> doubleBuffer_;
};

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

class Grid::CornerWin : public SubWindow
{
public:
    CornerWin(Grid& parent) : SubWindow(parent) {}

private:
    bool AcceptsFocus() const override { return false; }

    void render(wxDC& dc, const wxRect& rect) override
    {
        const wxRect& clientRect = GetClientRect();

        dc.GradientFillLinear(clientRect, getColorLabelGradientFrom(), getColorLabelGradientTo(), wxSOUTH);

        dc.SetPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW));

        {
            wxDCPenChanger dummy(dc, getColorLabelGradientFrom());
            dc.DrawLine(clientRect.GetTopLeft(), clientRect.GetTopRight());
        }

        dc.GradientFillLinear(wxRect(clientRect.GetBottomLeft (), clientRect.GetTopLeft ()), getColorLabelGradientFrom(), dc.GetPen().GetColour(), wxSOUTH);
        dc.GradientFillLinear(wxRect(clientRect.GetBottomRight(), clientRect.GetTopRight()), getColorLabelGradientFrom(), dc.GetPen().GetColour(), wxSOUTH);

        dc.DrawLine(clientRect.GetBottomLeft(), clientRect.GetBottomRight());

        wxRect rectShrinked = clientRect;
        rectShrinked.Deflate(1);
        dc.SetPen(*wxWHITE_PEN);

        //dc.DrawLine(clientRect.GetTopLeft(), clientRect.GetTopRight() + wxPoint(1, 0));
        dc.DrawLine(rectShrinked.GetTopLeft(), rectShrinked.GetBottomLeft() + wxPoint(0, 1));
    }
};

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

class Grid::RowLabelWin : public SubWindow
{
public:
    RowLabelWin(Grid& parent) :
        SubWindow(parent),
        rowHeight_(parent.GetCharHeight() + 2 + 1) {} //default height; don't call any functions on "parent" other than those from wxWindow during construction!
    //2 for some more space, 1 for bottom border (gives 15 + 2 + 1 on Windows, 17 + 2 + 1 on Ubuntu)

    int getBestWidth(ptrdiff_t rowFrom, ptrdiff_t rowTo)
    {
        wxClientDC dc(this);

        wxFont labelFont = GetFont();
        //labelFont.SetWeight(wxFONTWEIGHT_BOLD);
        dc.SetFont(labelFont); //harmonize with RowLabelWin::render()!

        int bestWidth = 0;
        for (ptrdiff_t i = rowFrom; i <= rowTo; ++i)
            bestWidth = std::max(bestWidth, dc.GetTextExtent(formatRow(i)).GetWidth() + fastFromDIP(2 * ROW_LABEL_BORDER_DIP));
        return bestWidth;
    }

    size_t getLogicalHeight() const { return refParent().getRowCount() * rowHeight_; }

    ptrdiff_t getRowAtPos(ptrdiff_t posY) const //returns < 0 on invalid input, else row number within: [0, rowCount]; rowCount if out of range
    {
        if (posY >= 0 && rowHeight_ > 0)
        {
            const size_t row = posY / rowHeight_;
            return std::min(row, refParent().getRowCount());
        }
        return -1;
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

    std::pair<ptrdiff_t, ptrdiff_t> getRowsOnClient(const wxRect& clientRect) const //returns range [begin, end)
    {
        const int yFrom = refParent().CalcUnscrolledPosition(clientRect.GetTopLeft    ()).y;
        const int yTo   = refParent().CalcUnscrolledPosition(clientRect.GetBottomRight()).y;

        return { std::max(yFrom / rowHeight_, 0),
                 std::min<ptrdiff_t>((yTo  / rowHeight_) + 1, refParent().getRowCount()) };
    }

private:
    static std::wstring formatRow(size_t row) { return formatNumber(row + 1); } //convert number to std::wstring including thousands separator

    bool AcceptsFocus() const override { return false; }

    void render(wxDC& dc, const wxRect& rect) override
    {
        clearArea(dc, rect, wxSystemSettings::GetColour(/*!renderAsEnabled(*this) ? wxSYS_COLOUR_BTNFACE :*/wxSYS_COLOUR_WINDOW));

        wxFont labelFont = GetFont();
        //labelFont.SetWeight(wxFONTWEIGHT_BOLD);
        dc.SetFont(labelFont); //harmonize with RowLabelWin::getBestWidth()!

        auto rowRange = getRowsOnClient(rect); //returns range [begin, end)
        for (auto row = rowRange.first; row < rowRange.second; ++row)
        {
            wxRect singleLabelArea = getRowLabelArea(row); //returns empty rect if row not found
            if (singleLabelArea.height > 0)
            {
                singleLabelArea.y = refParent().CalcScrolledPosition(singleLabelArea.GetTopLeft()).y;
                drawRowLabel(dc, singleLabelArea, row);
            }
        }
    }

    void drawRowLabel(wxDC& dc, const wxRect& rect, size_t row)
    {
        //clearArea(dc, rect, getColorRowLabel());
        dc.GradientFillLinear(rect, getColorLabelGradientFrom(), getColorLabelGradientTo(), wxEAST); //clear overlapping cells
        wxDCTextColourChanger dummy3(dc, getColorLabelText()); //accessibility: always set both foreground AND background colors!

        //label text
        wxRect textRect = rect;
        textRect.Deflate(1);

        GridData::drawCellText(dc, textRect, formatRow(row), wxALIGN_CENTRE);

        //border lines
        {
            wxDCPenChanger dummy(dc, *wxWHITE_PEN);
            dc.DrawLine(rect.GetTopLeft(), rect.GetTopRight());
        }
        {
            wxDCPenChanger dummy(dc, wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW));
            dc.DrawLine(rect.GetTopLeft(),     rect.GetBottomLeft());
            dc.DrawLine(rect.GetBottomLeft(),  rect.GetBottomRight());
            dc.DrawLine(rect.GetBottomRight(), rect.GetTopRight() + wxPoint(0, -1));
        }
    }

    void onMouseLeftDown(wxMouseEvent& event) override { refParent().redirectRowLabelEvent(event); }
    void onMouseMovement(wxMouseEvent& event) override { refParent().redirectRowLabelEvent(event); }
    void onMouseLeftUp  (wxMouseEvent& event) override { refParent().redirectRowLabelEvent(event); }

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
    ColLabelWin(Grid& parent) : SubWindow(parent) {}

private:
    bool AcceptsFocus() const override { return false; }

    void render(wxDC& dc, const wxRect& rect) override
    {
        clearArea(dc, rect, wxSystemSettings::GetColour(/*!renderAsEnabled(*this) ? wxSYS_COLOUR_BTNFACE :*/wxSYS_COLOUR_WINDOW));

        //coordinate with "colLabelHeight" in Grid constructor:
        wxFont labelFont = GetFont();
        labelFont.SetWeight(wxFONTWEIGHT_BOLD);
        dc.SetFont(labelFont);

        wxDCTextColourChanger dummy(dc, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)); //use user setting for labels

        const int colLabelHeight = refParent().colLabelHeight_;

        wxPoint labelAreaTL(refParent().CalcScrolledPosition(wxPoint(0, 0)).x, 0); //client coordinates

        std::vector<ColumnWidth> absWidths = refParent().getColWidths(); //resolve stretched widths
        for (auto it = absWidths.begin(); it != absWidths.end(); ++it)
        {
            const size_t col = it - absWidths.begin();
            const int width  = it->width; //don't use unsigned for calculations!

            if (labelAreaTL.x > rect.GetRight())
                return; //done, rect is fully covered
            if (labelAreaTL.x + width > rect.x)
                drawColumnLabel(dc, wxRect(labelAreaTL, wxSize(width, colLabelHeight)), col, it->type);
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
                drawColumnLabel(dc, wxRect(labelAreaTL, wxSize(clientWidth - totalWidth, colLabelHeight)), absWidths.size(), ColumnType::NONE);
        }
    }

    void drawColumnLabel(wxDC& dc, const wxRect& rect, size_t col, ColumnType colType)
    {
        if (auto dataView = refParent().getDataProvider())
        {
            const bool isHighlighted = activeResizing_    ? col == activeResizing_   ->getColumn    () : //highlight_ column on mouse-over
                                       activeClickOrMove_ ? col == activeClickOrMove_->getColumnFrom() :
                                       highlightCol_      ? col == *highlightCol_                      :
                                       false;

            RecursiveDcClipper clip(dc, rect);
            dataView->renderColumnLabel(refParent(), dc, rect, colType, isHighlighted);

            //draw move target location
            if (refParent().allowColumnMove_)
                if (activeClickOrMove_ && activeClickOrMove_->isRealMove())
                {
                    const int markerWidth = fastFromDIP(COLUMN_MOVE_MARKER_WIDTH_DIP);

                    if (col + 1 == activeClickOrMove_->refColumnTo()) //handle pos 1, 2, .. up to "at end" position
                        dc.GradientFillLinear(wxRect(rect.x + rect.width - markerWidth, rect.y, markerWidth, rect.height), getColorLabelGradientFrom(), *wxBLUE, wxSOUTH);
                    else if (col == activeClickOrMove_->refColumnTo() && col == 0) //pos 0
                        dc.GradientFillLinear(wxRect(rect.GetTopLeft(), wxSize(markerWidth, rect.height)), getColorLabelGradientFrom(), *wxBLUE, wxSOUTH);
                }
        }
    }

    void onMouseLeftDown(wxMouseEvent& event) override
    {
        if (FindFocus() != &refParent().getMainWin())
            refParent().getMainWin().SetFocus();

        activeResizing_.reset();
        activeClickOrMove_.reset();

        if (std::optional<ColAction> action = refParent().clientPosToColumnAction(event.GetPosition()))
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
                if (const std::optional<ColumnType> colType = refParent().colToType(activeClickOrMove_->getColumnFrom()))
                    sendEventToParent(GridLabelClickEvent(EVENT_GRID_COL_LABEL_MOUSE_LEFT, *colType));
            }
            activeClickOrMove_.reset();
        }

        refParent().updateWindowSizes(); //looks strange if done during onMouseMovement()
        refParent().Refresh();
        event.Skip();
    }

    void onMouseCaptureLost(wxMouseCaptureLostEvent& event) override
    {
        activeResizing_.reset();
        activeClickOrMove_.reset();
        Refresh();
        //event.Skip(); -> we DID handle it!
    }

    void onMouseLeftDouble(wxMouseEvent& event) override
    {
        if (std::optional<ColAction> action = refParent().clientPosToColumnAction(event.GetPosition()))
            if (action->wantResize)
            {
                //auto-size visible range on double-click
                const int bestWidth = refParent().getBestColumnSize(action->col); //return -1 on error
                if (bestWidth >= 0)
                {
                    refParent().setColumnWidth(bestWidth, action->col, GridEventPolicy::ALLOW);
                    refParent().Refresh(); //refresh main grid as well!
                }
            }
        event.Skip();
    }

    void onMouseMovement(wxMouseEvent& event) override
    {
        if (activeResizing_)
        {
            const auto col     = activeResizing_->getColumn();
            const int newWidth = activeResizing_->getStartWidth() + event.GetPosition().x - activeResizing_->getStartPosX();

            //set width tentatively
            refParent().setColumnWidth(newWidth, col, GridEventPolicy::ALLOW);

            //check if there's a small gap after last column, if yes, fill it
            const int gapWidth = GetClientSize().GetWidth() - refParent().getColWidthsSum(GetClientSize().GetWidth());
            if (std::abs(gapWidth) < fastFromDIP(COLUMN_FILL_GAP_TOLERANCE_DIP))
                refParent().setColumnWidth(newWidth + gapWidth, col, GridEventPolicy::ALLOW);

            refParent().Refresh(); //refresh columns on main grid as well!
        }
        else if (activeClickOrMove_)
        {
            const int clientPosX = event.GetPosition().x;
            if (std::abs(clientPosX - activeClickOrMove_->getStartPosX()) > fastFromDIP(COLUMN_MOVE_DELAY_DIP)) //real move (not a single click)
            {
                activeClickOrMove_->setRealMove();

                const ptrdiff_t col = refParent().clientPosToMoveTargetColumn(event.GetPosition());
                if (col >= 0)
                    activeClickOrMove_->refColumnTo() = col;
            }
        }
        else
        {
            if (const std::optional<ColAction> action = refParent().clientPosToColumnAction(event.GetPosition()))
            {
                highlightCol_ = action->col;

                if (action->wantResize)
                    SetCursor(wxCURSOR_SIZEWE); //set window-local only! :)
                else
                    SetCursor(*wxSTANDARD_CURSOR);
            }
            else
            {
                highlightCol_ = {};
                SetCursor(*wxSTANDARD_CURSOR);
            }
        }

        const std::wstring toolTip = [&]
        {
            const wxPoint absPos = refParent().CalcUnscrolledPosition(event.GetPosition());
            const ColumnType colType = refParent().getColumnAtPos(absPos.x).colType; //returns ColumnType::NONE if no column at x position!
            if (colType != ColumnType::NONE)
                if (auto prov = refParent().getDataProvider())
                    return prov->getToolTip(colType);
            return std::wstring();
        }();
        setToolTip(toolTip);

        Refresh();
        event.Skip();
    }

    void onLeaveWindow(wxMouseEvent& event) override
    {
        highlightCol_ = {}; //wxEVT_LEAVE_WINDOW does not respect mouse capture! -> however highlight_ is drawn unconditionally during move/resize!

        Refresh();
        event.Skip();
    }

    void onMouseRightDown(wxMouseEvent& event) override
    {
        if (const std::optional<ColAction> action = refParent().clientPosToColumnAction(event.GetPosition()))
        {
            if (const std::optional<ColumnType> colType = refParent().colToType(action->col))
                sendEventToParent(GridLabelClickEvent(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, *colType)); //notify right click
            else assert(false);
        }
        else
            //notify right click (on free space after last column)
            if (fillGapAfterColumns)
                sendEventToParent(GridLabelClickEvent(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, ColumnType::NONE));

        event.Skip();
    }

    std::unique_ptr<ColumnResizing> activeResizing_;
    std::unique_ptr<ColumnMove>     activeClickOrMove_;
    std::optional<size_t>                     highlightCol_; //column during mouse-over
};

//----------------------------------------------------------------------------------------------------------------
namespace
{
const wxEventType EVENT_GRID_HAS_SCROLLED = wxNewEventType(); //internal to Grid::MainWin::ScrollWindow()
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
        Connect(EVENT_GRID_HAS_SCROLLED, wxEventHandler(MainWin::onRequestWindowUpdate), nullptr, this);
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
        clearArea(dc, rect, wxSystemSettings::GetColour(/*!renderAsEnabled(*this) ? wxSYS_COLOUR_BTNFACE :*/wxSYS_COLOUR_WINDOW));

        dc.SetFont(GetFont()); //harmonize with Grid::getBestColumnSize()

        wxDCTextColourChanger dummy(dc, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)); //use user setting for labels

        std::vector<ColumnWidth> absWidths = refParent().getColWidths(); //resolve stretched widths
        {
            int totalRowWidth = 0;
            for (const ColumnWidth& cw : absWidths)
                totalRowWidth += cw.width;

            //fill gap after columns and cover full width
            if (fillGapAfterColumns)
                totalRowWidth = std::max(totalRowWidth, GetClientSize().GetWidth());

            if (auto prov = refParent().getDataProvider())
            {
                RecursiveDcClipper dummy2(dc, rect); //do NOT draw background on cells outside of invalidated rect invalidating foreground text!

                wxPoint cellAreaTL(refParent().CalcScrolledPosition(wxPoint(0, 0))); //client coordinates
                const int rowHeight = rowLabelWin_.getRowHeight();
                const auto rowRange = rowLabelWin_.getRowsOnClient(rect); //returns range [begin, end)

                //draw background lines
                for (auto row = rowRange.first; row < rowRange.second; ++row)
                {
                    const wxRect rowRect(cellAreaTL + wxPoint(0, row * rowHeight), wxSize(totalRowWidth, rowHeight));
                    RecursiveDcClipper dummy3(dc, rowRect);
                    prov->renderRowBackgound(dc, rowRect, row, renderAsEnabled(*this), drawAsSelected(row));
                }

                //draw single cells, column by column
                for (const ColumnWidth& cw : absWidths)
                {
                    if (cellAreaTL.x > rect.GetRight())
                        return; //done

                    if (cellAreaTL.x + cw.width > rect.x)
                        for (auto row = rowRange.first; row < rowRange.second; ++row)
                        {
                            const wxRect cellRect(cellAreaTL.x, cellAreaTL.y + row * rowHeight, cw.width, rowHeight);
                            RecursiveDcClipper dummy3(dc, cellRect);
                            prov->renderCell(dc, cellRect, row, cw.type, renderAsEnabled(*this), drawAsSelected(row), getRowHoverToDraw(row));
                        }
                    cellAreaTL.x += cw.width;
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
        else if (highlight_.row == row)
            return highlight_.rowHover;
        return HoverArea::NONE;
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
            const wxPoint     absPos = refParent().CalcUnscrolledPosition(event.GetPosition());
            const ptrdiff_t      row = rowLabelWin_.getRowAtPos(absPos.y); //return -1 for invalid position; >= rowCount if out of range
            const ColumnPosInfo  cpi = refParent().getColumnAtPos(absPos.x); //returns ColumnType::NONE if no column at x position!
            const HoverArea rowHover = prov->getRowMouseHover(row, cpi.colType, cpi.cellRelativePosX, cpi.colWidth);
            const wxPoint   mousePos = GetPosition() + event.GetPosition();

            //client is interested in all double-clicks, even those outside of the grid!
            sendEventToParent(GridClickEvent(EVENT_GRID_MOUSE_LEFT_DOUBLE, row, rowHover, mousePos));
        }
        event.Skip();
    }

    void onMouseDown(wxMouseEvent& event) //handle left and right mouse button clicks (almost) the same
    {
        if (auto prov = refParent().getDataProvider())
        {
            const wxPoint    absPos = refParent().CalcUnscrolledPosition(event.GetPosition());
            const ptrdiff_t     row = rowLabelWin_.getRowAtPos(absPos.y); //return -1 for invalid position; >= rowCount if out of range
            const ColumnPosInfo cpi = refParent().getColumnAtPos(absPos.x); //returns ColumnType::NONE if no column at x position!
            const HoverArea rowHover = prov->getRowMouseHover(row, cpi.colType, cpi.cellRelativePosX, cpi.colWidth);
            const wxPoint   mousePos = GetPosition() + event.GetPosition();
            //row < 0 possible!!! Pressing "Menu Key" simulates mouse-right-button down + up at position 0xffff/0xffff!

            GridClickEvent mouseEvent(event.RightDown() ? EVENT_GRID_MOUSE_RIGHT_DOWN : EVENT_GRID_MOUSE_LEFT_DOWN, row, rowHover, mousePos);
            if (!sendEventToParent(mouseEvent)) //allow client to swallow event!
            {
                if (wxWindow::FindFocus() != this) //doesn't seem to happen automatically for right mouse button
                    SetFocus();

                if (row >= 0)
                    if (!event.RightDown() || !refParent().isSelected(row)) //do NOT start a new selection if user right-clicks on a selected area!
                    {
                        if (event.ControlDown())
                            activeSelection_ = std::make_unique<MouseSelection>(*this, row, !refParent().isSelected(row) /*positive*/, false /*gridWasCleared*/, mouseEvent);
                        else if (event.ShiftDown())
                        {
                            refParent().clearSelection(GridEventPolicy::DENY);
                            activeSelection_ = std::make_unique<MouseSelection>(*this, selectionAnchor_, true /*positive*/, true /*gridWasCleared*/, mouseEvent);
                        }
                        else
                        {
                            refParent().clearSelection(GridEventPolicy::DENY);
                            activeSelection_ = std::make_unique<MouseSelection>(*this, row, true /*positive*/, true /*gridWasCleared*/, mouseEvent);
                            //DO NOT emit range event for clearing selection! would be inconsistent with keyboard handling (moving cursor neither emits range event)
                            //and is also harmful when range event is considered a final action
                            //e.g. cfg grid would prematurely show a modal dialog after changed config
                        }
                    }
                Refresh();
            }
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

            const ptrdiff_t      rowFrom    = activeSelection_->getStartRow();
            const ptrdiff_t      rowTo      = activeSelection_->getCurrentRow();
            const bool           positive   = activeSelection_->isPositiveSelect();
            const GridClickEvent mouseClick = activeSelection_->getFirstClick();

            activeSelection_.reset(); //release mouse capture *before* sending the event (which might show a modal popup dialog requiring the mouse!!!)

            refParent().selectRange(rowFrom, rowTo, positive, &mouseClick, GridEventPolicy::ALLOW);
        }

        if (auto prov = refParent().getDataProvider())
        {
            //this one may point to row which is not in visible area!
            const wxPoint    absPos = refParent().CalcUnscrolledPosition(event.GetPosition());
            const ptrdiff_t     row = rowLabelWin_.getRowAtPos(absPos.y); //return -1 for invalid position; >= rowCount if out of range
            const ColumnPosInfo cpi = refParent().getColumnAtPos(absPos.x); //returns ColumnType::NONE if no column at x position!
            const HoverArea rowHover = prov->getRowMouseHover(row, cpi.colType, cpi.cellRelativePosX, cpi.colWidth);
            const wxPoint   mousePos = GetPosition() + event.GetPosition();
            //notify click event after the range selection! e.g. this makes sure the selection is applied before showing a context menu
            sendEventToParent(GridClickEvent(event.RightUp() ? EVENT_GRID_MOUSE_RIGHT_UP : EVENT_GRID_MOUSE_LEFT_UP, row, rowHover, mousePos));
        }

        //update highlight_ and tooltip: on OS X no mouse movement event is generated after a mouse button click (unlike on Windows)
        event.SetPosition(ScreenToClient(wxGetMousePosition())); //mouse position may have changed within above callbacks (e.g. context menu was shown)!
        onMouseMovement(event);

        Refresh();
        event.Skip(); //allow changing focus
    }

    void onMouseCaptureLost(wxMouseCaptureLostEvent& event) override
    {
        if (activeSelection_)
        {
            if (activeSelection_->gridWasCleared())
                refParent().clearSelection(GridEventPolicy::ALLOW); //see onMouseDown(); selection is "completed" => emit GridSelectEvent

            activeSelection_.reset();
        }
        highlight_.row = -1;
        Refresh();
        //event.Skip(); -> we DID handle it!
    }

    void onMouseMovement(wxMouseEvent& event) override
    {
        if (auto prov = refParent().getDataProvider())
        {
            const ptrdiff_t rowCount = refParent().getRowCount();
            const wxPoint     absPos = refParent().CalcUnscrolledPosition(event.GetPosition());
            const ptrdiff_t      row = rowLabelWin_.getRowAtPos(absPos.y); //return -1 for invalid position; >= rowCount if out of range
            const ColumnPosInfo  cpi = refParent().getColumnAtPos(absPos.x); //returns ColumnType::NONE if no column at x position!
            const HoverArea rowHover = prov->getRowMouseHover(row, cpi.colType, cpi.cellRelativePosX, cpi.colWidth);

            const std::wstring toolTip = [&]
            {
                if (cpi.colType != ColumnType::NONE && 0 <= row && row < rowCount)
                    return prov->getToolTip(row, cpi.colType);
                return std::wstring();
            }();
            setToolTip(toolTip); //show even during mouse selection!

            if (activeSelection_)
                activeSelection_->evalMousePos(); //call on both mouse movement + timer event!
            else
            {
                refreshHighlight(highlight_);
                highlight_.row      = row;
                highlight_.rowHover = rowHover;
                refreshHighlight(highlight_); //multiple Refresh() calls are condensed into single one!
            }
        }
        event.Skip();
    }

    void onLeaveWindow(wxMouseEvent& event) override //wxEVT_LEAVE_WINDOW does not respect mouse capture!
    {
        if (!activeSelection_)
        {
            refreshHighlight(highlight_);
            highlight_.row = -1;
        }

        event.Skip();
    }


    void onFocus(wxFocusEvent& event) override { Refresh(); event.Skip(); }

    class MouseSelection : private wxEvtHandler
    {
    public:
        MouseSelection(MainWin& wnd, size_t rowStart, bool positive, bool gridWasCleared, const GridClickEvent& firstClick) :
            wnd_(wnd), rowStart_(rowStart), rowCurrent_(rowStart), positiveSelect_(positive), gridWasCleared_(gridWasCleared), firstClick_(firstClick)
        {
            wnd_.CaptureMouse();
            timer_.Connect(wxEVT_TIMER, wxEventHandler(MouseSelection::onTimer), nullptr, this);
            timer_.Start(100); //timer interval in ms
            evalMousePos();
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
            if (pixelsPerUnitY <= 0) return;

            const double mouseDragSpeedIncScrollU = pixelsPerUnitY > 0 ? MOUSE_DRAG_ACCELERATION_DIP * wnd_.rowLabelWin_.getRowHeight() / pixelsPerUnitY : 0; //unit: [scroll units / (DIP * sec)]

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

            const wxPoint absPos = wnd_.refParent().CalcUnscrolledPosition(clientPosTrimmed);
            const ptrdiff_t newRow = wnd_.rowLabelWin_.getRowAtPos(absPos.y); //return -1 for invalid position; >= rowCount if out of range
            if (newRow >= 0)
                if (rowCurrent_ != newRow)
                {
                    rowCurrent_ = newRow;
                    wnd_.Refresh();
                }
        }

    private:
        void onTimer(wxEvent& event) { evalMousePos(); }

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

    struct MouseHighlight
    {
        ptrdiff_t row = -1;
        HoverArea rowHover = HoverArea::NONE;
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
            wxCommandEvent scrollEvent(EVENT_GRID_HAS_SCROLLED);
            AddPendingEvent(scrollEvent); //asynchronously call updateAfterScroll()
        }
    }

    void onRequestWindowUpdate(wxEvent& event)
    {
        assert(gridUpdatePending_);
        ZEN_ON_SCOPE_EXIT(gridUpdatePending_ = false);

        refParent().updateWindowSizes(false); //row label width has changed -> do *not* update scrollbars: recursion on wxGTK! -> still a problem, now that we're called async??
        rowLabelWin_.Update(); //update while dragging scroll thumb
    }

    void refreshRow(size_t row)
    {
        const wxRect& rowArea = rowLabelWin_.getRowLabelArea(row); //returns empty rect if row not found
        const wxPoint topLeft = refParent().CalcScrolledPosition(wxPoint(0, rowArea.y)); //absolute -> client coordinates
        wxRect cellArea(topLeft, wxSize(refParent().getColWidthsSum(GetClientSize().GetWidth()), rowArea.height));
        RefreshRect(cellArea, false);
    }

    void refreshHighlight(const MouseHighlight& hl)
    {
        const ptrdiff_t rowCount = refParent().getRowCount();
        if (0 <= hl.row && hl.row < rowCount && hl.rowHover != HoverArea::NONE) //no highlight_? => NOP!
            refreshRow(hl.row);
    }

    RowLabelWin& rowLabelWin_;
    ColLabelWin& colLabelWin_;

    std::unique_ptr<MouseSelection> activeSelection_; //bound while user is selecting with mouse
    MouseHighlight highlight_; //current mouse highlight_ (superseded by activeSelection_ if available)

    ptrdiff_t cursorRow_ = 0;
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

    colLabelHeight_ = fastFromDIP(2 * DEFAULT_COL_LABEL_BORDER_DIP) + [&]
    {
        //coordinate with ColLabelWin::render():
        wxFont labelFont = colLabelWin_->GetFont();
        labelFont.SetWeight(wxFONTWEIGHT_BOLD);
        return labelFont.GetPixelSize().GetHeight();
    }();

    SetTargetWindow(mainWin_);

    SetInitialSize(size); //"Most controls will use this to set their initial size" -> why not

    assert(GetClientSize() == GetSize()); //borders are NOT allowed for Grid
    //reason: updateWindowSizes() wants to use "GetSize()" as a "GetClientSize()" including scrollbars

    Connect(wxEVT_PAINT, wxPaintEventHandler(Grid::onPaintEvent), nullptr, this);
    Connect(wxEVT_SIZE,  wxSizeEventHandler (Grid::onSizeEvent ), nullptr, this);
    Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent& event) {}); //http://wiki.wxwidgets.org/Flicker-Free_Drawing

    Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(Grid::onKeyDown), nullptr, this);
    Connect(wxEVT_KEY_UP,   wxKeyEventHandler(Grid::onKeyUp  ), nullptr, this);
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
    const int mainWinHeightGross = std::max(GetSize().GetHeight() - colLabelHeight_, 0); //independent from client sizes and scrollbars!
    const ptrdiff_t logicalHeight = rowLabelWin_->getLogicalHeight();                   //

    int rowLabelWidth = 0;
    if (drawRowLabel_ && logicalHeight > 0)
    {
        ptrdiff_t yFrom = CalcUnscrolledPosition(wxPoint(0, 0)).y;
        ptrdiff_t yTo   = CalcUnscrolledPosition(wxPoint(0, mainWinHeightGross - 1)).y ;
        yFrom = std::clamp<ptrdiff_t>(yFrom, 0, logicalHeight - 1);
        yTo   = std::clamp<ptrdiff_t>(yTo,   0, logicalHeight - 1);

        const ptrdiff_t rowFrom = rowLabelWin_->getRowAtPos(yFrom);
        const ptrdiff_t rowTo   = rowLabelWin_->getRowAtPos(yTo);
        if (rowFrom >= 0 && rowTo >= 0)
            rowLabelWidth = rowLabelWin_->getBestWidth(rowFrom, rowTo);
    }

    auto getMainWinSize = [&](const wxSize& clientSize) { return wxSize(std::max(0, clientSize.GetWidth() - rowLabelWidth), std::max(0, clientSize.GetHeight() - colLabelHeight_)); };

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

    //2. update managed windows' sizes: just assume scrollbars are already set correctly, even if they may not be (yet)!
    //this ensures mainWin_->SetVirtualSize() and AdjustScrollbars() are working with the correct main window size, unless sb change later, which triggers a recalculation anyway!
    const wxSize mainWinSize = getMainWinSize(GetClientSize());

    cornerWin_  ->SetSize(0, 0, rowLabelWidth, colLabelHeight_);
    rowLabelWin_->SetSize(0, colLabelHeight_, rowLabelWidth, mainWinSize.GetHeight());
    colLabelWin_->SetSize(rowLabelWidth, 0, mainWinSize.GetWidth(), colLabelHeight_);
    mainWin_    ->SetSize(rowLabelWidth, colLabelHeight_, mainWinSize.GetWidth(), mainWinSize.GetHeight());

    //avoid flicker in wxWindowMSW::HandleSize() when calling ::EndDeferWindowPos() where the sub-windows are moved only although they need to be redrawn!
    colLabelWin_->Refresh();
    mainWin_    ->Refresh();

    //3. update scrollbars: "guide wxScrolledHelper to not screw up too much"
    if (updateScrollbar)
    {
        const int mainWinWidthGross = getMainWinSize(GetSize()).GetWidth();

        if (logicalHeight <= mainWinHeightGross &&
            getColWidthsSum(mainWinWidthGross) <= mainWinWidthGross &&
            //this special case needs to be considered *only* when both scrollbars are flexible:
            showScrollbarX_ == SB_SHOW_AUTOMATIC &&
            showScrollbarY_ == SB_SHOW_AUTOMATIC)
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

            3. lh > gh
                4.1 lw <= nw         => need vertical scrollbar only
                4.2 nw < lw <= gw    => need both scrollbars

            4. lw > gw
                3.1 lh <= nh         => need horizontal scrollbar only
                3.2 nh < lh <= gh    => need both scrollbars
            */
        }
    }
}


wxSize Grid::GetSizeAvailableForScrollTarget(const wxSize& size)
{
    //harmonize with Grid::updateWindowSizes()!

    //1. calculate row label width independent from scrollbars
    const int mainWinHeightGross = std::max(size.GetHeight() - colLabelHeight_, 0); //independent from client sizes and scrollbars!
    const ptrdiff_t logicalHeight = rowLabelWin_->getLogicalHeight();              //

    int rowLabelWidth = 0;
    if (drawRowLabel_ && logicalHeight > 0)
    {
        ptrdiff_t yFrom = CalcUnscrolledPosition(wxPoint(0, 0)).y;
        ptrdiff_t yTo   = CalcUnscrolledPosition(wxPoint(0, mainWinHeightGross - 1)).y ;
        yFrom = std::clamp<ptrdiff_t>(yFrom, 0, logicalHeight - 1);
        yTo   = std::clamp<ptrdiff_t>(yTo,   0, logicalHeight - 1);

        const ptrdiff_t rowFrom = rowLabelWin_->getRowAtPos(yFrom);
        const ptrdiff_t rowTo   = rowLabelWin_->getRowAtPos(yTo);
        if (rowFrom >= 0 && rowTo >= 0)
            rowLabelWidth = rowLabelWin_->getBestWidth(rowFrom, rowTo);
    }

    return size - wxSize(rowLabelWidth, colLabelHeight_);
}


void Grid::onPaintEvent(wxPaintEvent& event) { wxPaintDC dc(this); }


void Grid::onKeyUp(wxKeyEvent& event)
{
    int keyCode = event.GetKeyCode();
    if (event.ShiftDown() && keyCode == WXK_F10) //== alias for menu key
        keyCode = WXK_WINDOWS_MENU;

    switch (keyCode)
    {
        case WXK_MENU:         //
        case WXK_WINDOWS_MENU: //simulate right mouse click at cursor(+1) position
        {
            const int cursorNextPosY = rowLabelWin_->getRowHeight() * std::min(getRowCount(), mainWin_->getCursor() + 1);
            const int clientPosMainWinY = std::clamp(CalcScrolledPosition(wxPoint(0, cursorNextPosY)).y, //absolute -> client coordinates
                                                     0, mainWin_->GetClientSize().GetHeight() - 1);
            const wxPoint mousePos = mainWin_->GetPosition() + wxPoint(0, clientPosMainWinY); //mainWin_-relative to Grid-relative

            GridClickEvent clickEvent(EVENT_GRID_MOUSE_RIGHT_UP, -1, HoverArea::NONE, mousePos);
            if (wxEvtHandler* evtHandler = GetEventHandler())
                evtHandler->ProcessEvent(clickEvent);
        }
        return;
    }
    event.Skip();
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
        {
            row = std::clamp<ptrdiff_t>(row, 0, rowCount - 1);
            setGridCursor(row, GridEventPolicy::ALLOW);
        }
    };

    auto selectWithCursorTo = [&](ptrdiff_t row)
    {
        if (rowCount > 0)
        {
            row = std::clamp<ptrdiff_t>(row, 0, rowCount - 1);
            selectWithCursor(row); //emits GridSelectEvent
        }
    };

    switch (keyCode)
    {
        case WXK_MENU:         //
        case WXK_WINDOWS_MENU: //simulate right mouse click at cursor(+1) position
        {
            const int cursorNextPosY = rowLabelWin_->getRowHeight() * std::min(getRowCount(), mainWin_->getCursor() + 1);
            const int clientPosMainWinY = std::clamp(CalcScrolledPosition(wxPoint(0, cursorNextPosY)).y, //absolute -> client coordinates
                                                     0, mainWin_->GetClientSize().GetHeight() - 1);
            const wxPoint mousePos = mainWin_->GetPosition() + wxPoint(0, clientPosMainWinY); //mainWin_-relative to Grid-relative

            GridClickEvent clickEvent(EVENT_GRID_MOUSE_RIGHT_DOWN, -1, HoverArea::NONE, mousePos);
            if (wxEvtHandler* evtHandler = GetEventHandler())
                evtHandler->ProcessEvent(clickEvent);
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
                selectWithCursorTo(cursorRow - GetClientSize().GetHeight() / rowLabelWin_->getRowHeight());
            //else if (event.ControlDown())
            //    ;
            else
                moveCursorTo(cursorRow - GetClientSize().GetHeight() / rowLabelWin_->getRowHeight());
            return;

        case WXK_PAGEDOWN:
        case WXK_NUMPAD_PAGEDOWN:
            if (event.ShiftDown())
                selectWithCursorTo(cursorRow + GetClientSize().GetHeight() / rowLabelWin_->getRowHeight());
            //else if (event.ControlDown())
            //    ;
            else
                moveCursorTo(cursorRow + GetClientSize().GetHeight() / rowLabelWin_->getRowHeight());
            return;

        case 'A':  //Ctrl + A - select all
            if (event.ControlDown())
                selectRange(0, rowCount, true /*positive*/, nullptr /*mouseInitiated*/, GridEventPolicy::ALLOW);
            break;

        case WXK_NUMPAD_ADD: //CTRL + '+' - auto-size all
            if (event.ControlDown())
                autoSizeColumns(GridEventPolicy::ALLOW);
            return;
    }

    event.Skip();
}


void Grid::setColumnLabelHeight(int height)
{
    colLabelHeight_ = std::max(0, height);
    updateWindowSizes();
}


void Grid::showRowLabel(bool show)
{
    drawRowLabel_ = show;
    updateWindowSizes();
}


void Grid::selectRow(size_t row, GridEventPolicy rangeEventPolicy)
{
    selection_.selectRow(row);
    mainWin_->Refresh();

    if (rangeEventPolicy == GridEventPolicy::ALLOW)
    {
        GridSelectEvent selEvent(row, row + 1, true, nullptr /*mouseClick*/);
        if (wxEvtHandler* evtHandler = GetEventHandler())
            evtHandler->ProcessEvent(selEvent);
    }
}


void Grid::selectAllRows(GridEventPolicy rangeEventPolicy)
{
    selection_.selectAll();
    mainWin_->Refresh();

    if (rangeEventPolicy == GridEventPolicy::ALLOW)
    {
        GridSelectEvent selEvent(0, getRowCount(), true /*positive*/, nullptr /*mouseClick*/);
        if (wxEvtHandler* evtHandler = GetEventHandler())
            evtHandler->ProcessEvent(selEvent);
    }
}


void Grid::clearSelection(GridEventPolicy rangeEventPolicy)
{
    selection_.clear();
    mainWin_->Refresh();

    if (rangeEventPolicy == GridEventPolicy::ALLOW)
    {
        GridSelectEvent unselectionEvent(0, getRowCount(), false /*positive*/, nullptr /*mouseClick*/);
        if (wxEvtHandler* evtHandler = GetEventHandler())
            evtHandler->ProcessEvent(unselectionEvent);
    }
}


void Grid::scrollDelta(int deltaX, int deltaY)
{
    wxPoint scrollPos = GetViewStart();

    scrollPos.x += deltaX;
    scrollPos.y += deltaY;

    scrollPos.x = std::max(0, scrollPos.x); //wxScrollHelper::Scroll() will exit prematurely if input happens to be "-1"!
    scrollPos.y = std::max(0, scrollPos.y); //

    Scroll(scrollPos); //internally calls wxWindows::Update()!
    updateWindowSizes(); //may show horizontal scroll bar if row column gets wider
}


void Grid::redirectRowLabelEvent(wxMouseEvent& event)
{
    event.m_x = 0;
    if (wxEvtHandler* evtHandler = mainWin_->GetEventHandler())
        evtHandler->ProcessEvent(event);

    if (event.ButtonDown() && wxWindow::FindFocus() != mainWin_)
        mainWin_->SetFocus();
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

    if (selection_.maxSize() != rowCountNew) //clear selection only when needed (consider setSelectedRows())
        selection_.init(rowCountNew);

    wxScrolledWindow::Refresh(eraseBackground, rect);
}


void Grid::setRowHeight(int height)
{
    rowLabelWin_->setRowHeight(height);
    updateWindowSizes();
    Refresh();
}


void Grid::setColumnConfig(const std::vector<Grid::ColAttributes>& attr)
{
    //hold ownership of non-visible columns
    oldColAttributes_ = attr;

    std::vector<VisibleColumn> visCols;
    for (const ColAttributes& ca : attr)
    {
        assert(ca.stretch >= 0);
        assert(ca.type != ColumnType::NONE);

        if (ca.visible)
            visCols.push_back({ ca.type, ca.offset, std::max(ca.stretch, 0) });
    }

    //"ownership" of visible columns is now within Grid
    visibleCols_ = visCols;

    updateWindowSizes();
    Refresh();
}


std::vector<Grid::ColAttributes> Grid::getColumnConfig() const
{
    //get non-visible columns (+ outdated visible ones)
    std::vector<ColAttributes> output = oldColAttributes_;

    auto iterVcols    = visibleCols_.begin();
    auto iterVcolsend = visibleCols_.end();

    //update visible columns but keep order of non-visible ones!
    for (ColAttributes& ca : output)
        if (ca.visible)
        {
            if (iterVcols != iterVcolsend)
            {
                ca.type    = iterVcols->type;
                ca.stretch = iterVcols->stretch;
                ca.offset  = iterVcols->offset;
                ++iterVcols;
            }
            else
                assert(false);
        }
    assert(iterVcols == iterVcolsend);

    return output;
}


void Grid::showScrollBars(Grid::ScrollBarStatus horizontal, Grid::ScrollBarStatus vertical)
{
    if (showScrollbarX_ == horizontal &&
        showScrollbarY_ == vertical) return; //support polling!

    showScrollbarX_ = horizontal;
    showScrollbarY_ = vertical;

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

    GtkWidget* gridWidget = wxWindow::m_widget;
    GtkScrolledWindow* scrolledWindow = GTK_SCROLLED_WINDOW(gridWidget);
    ::gtk_scrolled_window_set_policy(scrolledWindow,
                                     mapStatus(horizontal),
                                     mapStatus(vertical));

    updateWindowSizes();
}



wxWindow& Grid::getCornerWin  () { return *cornerWin_;   }
wxWindow& Grid::getRowLabelWin() { return *rowLabelWin_; }
wxWindow& Grid::getColLabelWin() { return *colLabelWin_; }
wxWindow& Grid::getMainWin    () { return *mainWin_;     }
const wxWindow& Grid::getMainWin() const { return *mainWin_; }


std::optional<Grid::ColAction> Grid::clientPosToColumnAction(const wxPoint& pos) const
{
    const int absPosX = CalcUnscrolledPosition(pos).x;
    if (absPosX >= 0)
    {
        const int resizeTolerance = allowColumnResize_ ? fastFromDIP(COLUMN_RESIZE_TOLERANCE_DIP) : 0;
        std::vector<ColumnWidth> absWidths = getColWidths(); //resolve stretched widths

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


ptrdiff_t Grid::clientPosToMoveTargetColumn(const wxPoint& pos) const
{

    const int absPosX = CalcUnscrolledPosition(pos).x;

    int accWidth = 0;
    std::vector<ColumnWidth> absWidths = getColWidths(); //resolve negative/stretched widths
    for (auto itCol = absWidths.begin(); itCol != absWidths.end(); ++itCol)
    {
        const int width = itCol->width; //beware dreaded unsigned conversions!
        accWidth += width;

        if (absPosX < accWidth - width / 2)
            return itCol - absWidths.begin();
    }
    return absWidths.size();
}


ColumnType Grid::colToType(size_t col) const
{
    if (col < visibleCols_.size())
        return visibleCols_[col].type;
    return ColumnType::NONE;
}


ptrdiff_t Grid::getRowAtPos(int posY) const { return rowLabelWin_->getRowAtPos(posY); }


Grid::ColumnPosInfo Grid::getColumnAtPos(int posX) const
{
    if (posX >= 0)
    {
        int accWidth = 0;
        for (const ColumnWidth& cw : getColWidths())
        {
            accWidth += cw.width;
            if (posX < accWidth)
                return { cw.type, posX + cw.width - accWidth, cw.width };
        }
    }
    return { ColumnType::NONE, 0, 0 };
}


wxRect Grid::getColumnLabelArea(ColumnType colType) const
{
    std::vector<ColumnWidth> absWidths = getColWidths(); //resolve negative/stretched widths

    //colType is not unique in general, but *this* function expects it!
    assert(std::count_if(absWidths.begin(), absWidths.end(), [&](const ColumnWidth& cw) { return cw.type == colType; }) <= 1);

    auto itCol = std::find_if(absWidths.begin(), absWidths.end(), [&](const ColumnWidth& cw) { return cw.type == colType; });
    if (itCol != absWidths.end())
    {
        ptrdiff_t posX = 0;
        for (auto it = absWidths.begin(); it != itCol; ++it)
            posX += it->width;

        return wxRect(wxPoint(posX, 0), wxSize(itCol->width, colLabelHeight_));
    }
    return wxRect();
}


void Grid::refreshCell(size_t row, ColumnType colType)
{
    const wxRect& colArea = getColumnLabelArea(colType); //returns empty rect if column not found
    const wxRect& rowArea = rowLabelWin_->getRowLabelArea(row); //returns empty rect if row not found
    if (colArea.height > 0 && rowArea.height > 0)
    {
        const wxPoint topLeft = CalcScrolledPosition(wxPoint(colArea.x, rowArea.y)); //absolute -> client coordinates
        const wxRect cellArea(topLeft, wxSize(colArea.width, rowArea.height));

        getMainWin().RefreshRect(cellArea, false);
    }
}


void Grid::setGridCursor(size_t row, GridEventPolicy rangeEventPolicy)
{
    mainWin_->setCursor(row, row);
    makeRowVisible(row);

    selection_.clear(); //clear selection, do NOT fire event
    selectRange(row, row, true /*positive*/, nullptr /*mouseInitiated*/, rangeEventPolicy); //set new selection + fire event
}


void Grid::selectWithCursor(ptrdiff_t row) //emits GridSelectEvent
{
    const size_t anchorRow = mainWin_->getAnchor();

    mainWin_->setCursor(row, anchorRow);
    makeRowVisible(row);

    selection_.clear(); //clear selection, do NOT fire event
    selectRange(anchorRow, row, true /*positive*/, nullptr /*mouseInitiated*/, GridEventPolicy::ALLOW); //set new selection + fire event
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
                    const int scrollPosNewY = std::ceil((labelRect.y - clientHeight +
                                                         labelRect.height) / static_cast<double>(pixelsPerUnitY));
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


void Grid::selectRange(ptrdiff_t rowFrom, ptrdiff_t rowTo, bool positive, const GridClickEvent* mouseClick, GridEventPolicy rangeEventPolicy)
{
    //sort + convert to half-open range
    auto rowFirst = std::min(rowFrom, rowTo);
    auto rowLast  = std::max(rowFrom, rowTo) + 1;

    const size_t rowCount = getRowCount();
    rowFirst = std::clamp<ptrdiff_t>(rowFirst, 0, rowCount);
    rowLast  = std::clamp<ptrdiff_t>(rowLast,  0, rowCount);

    selection_.selectRange(rowFirst, rowLast, positive);
    mainWin_->Refresh();

    if (rangeEventPolicy == GridEventPolicy::ALLOW)
    {
        GridSelectEvent selectionEvent(rowFirst, rowLast, positive, mouseClick);
        if (wxEvtHandler* evtHandler = GetEventHandler())
            evtHandler->ProcessEvent(selectionEvent);
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


size_t Grid::getTopRow() const
{
    const wxPoint   absPos = CalcUnscrolledPosition(wxPoint(0, 0));
    const ptrdiff_t row = rowLabelWin_->getRowAtPos(absPos.y); //return -1 for invalid position; >= rowCount if out of range
    assert((getRowCount() == 0 && row == 0) || (0 <= row && row < static_cast<ptrdiff_t>(getRowCount())));
    return row;
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

        int maxSize = 0;

        auto rowRange = rowLabelWin_->getRowsOnClient(mainWin_->GetClientRect()); //returns range [begin, end)
        for (auto row = rowRange.first; row < rowRange.second; ++row)
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
        width = std::max(width, fastFromDIP(COLUMN_MIN_WIDTH_DIP));

        vcRs.offset = width - stretchedWidths[col]; //width := stretchedWidth + offset

        //III. resizing any column should normalize *all* other stretched columns' offsets considering current mainWinWidth!
        // test case:
        //1. have columns, both fixed-size and stretched, fit whole window width
        //2. shrink main window width so that horizontal scrollbars are shown despite the streched column
        //3. shrink a fixed-size column so that the scrollbars vanish and columns cover full width again
        //4. now verify that the stretched column is resizing immediately if main window is enlarged again
        for (size_t col2 = 0; col2 < visibleCols_.size(); ++col2)
            if (visibleCols_[col2].stretch > 0) //normalize stretched columns only
                visibleCols_[col2].offset = std::max(visibleCols_[col2].offset, fastFromDIP(COLUMN_MIN_WIDTH_DIP) - stretchedWidths[col2]);

        if (columnResizeEventPolicy == GridEventPolicy::ALLOW)
        {
            GridColumnResizeEvent sizeEvent(vcRs.offset, vcRs.type);
            if (wxEvtHandler* evtHandler = GetEventHandler())
            {
                if (notifyAsync)
                    evtHandler->AddPendingEvent(sizeEvent);
                else
                    evtHandler->ProcessEvent(sizeEvent);
            }
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
            width = std::max(width, fastFromDIP(COLUMN_MIN_WIDTH_DIP)); //normalization really needed here: e.g. smaller main window would result in negative width
        else
            width = std::max(width, 0); //support smaller width than COLUMN_MIN_WIDTH_DIP if set via configuration

        output.push_back({ vc.type, width });
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

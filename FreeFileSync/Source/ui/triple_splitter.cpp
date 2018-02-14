// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "triple_splitter.h"
#include <algorithm>
#include <zen/stl_tools.h>

using namespace zen;
using namespace fff;


namespace
{
//------------ Grid Constants -------------------------------
const int SASH_HIT_TOLERANCE = 5; //currently only a placebo!
const int SASH_SIZE = 10;
const double SASH_GRAVITY = 0.5; //value within [0, 1]; 1 := resize left only, 0 := resize right only
const int CHILD_WINDOW_MIN_SIZE = 50; //min. size of managed windows

//let's NOT create wxWidgets objects statically:
inline wxColor getColorSashGradientFrom() { return { 192, 192, 192 }; } //light grey
inline wxColor getColorSashGradientTo  () { return *wxWHITE; }
}


TripleSplitter::TripleSplitter(wxWindow* parent,
                               wxWindowID id,
                               const wxPoint& pos,
                               const wxSize& size,
                               long style) : wxWindow(parent, id, pos, size, style | wxTAB_TRAVERSAL) //tab between windows
{
    Connect(wxEVT_PAINT,            wxPaintEventHandler(TripleSplitter::onPaintEvent     ), nullptr, this);
    Connect(wxEVT_SIZE,             wxSizeEventHandler (TripleSplitter::onSizeEvent      ), nullptr, this);
    //http://wiki.wxwidgets.org/Flicker-Free_Drawing
    Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(TripleSplitter::onEraseBackGround), nullptr, this);

    SetBackgroundStyle(wxBG_STYLE_PAINT);

    Connect(wxEVT_LEFT_DOWN,    wxMouseEventHandler(TripleSplitter::onMouseLeftDown  ), nullptr, this);
    Connect(wxEVT_LEFT_UP,      wxMouseEventHandler(TripleSplitter::onMouseLeftUp    ), nullptr, this);
    Connect(wxEVT_MOTION,       wxMouseEventHandler(TripleSplitter::onMouseMovement  ), nullptr, this);
    Connect(wxEVT_LEAVE_WINDOW, wxMouseEventHandler(TripleSplitter::onLeaveWindow    ), nullptr, this);
    Connect(wxEVT_MOUSE_CAPTURE_LOST, wxMouseCaptureLostEventHandler(TripleSplitter::onMouseCaptureLost), nullptr, this);
    Connect(wxEVT_LEFT_DCLICK,  wxMouseEventHandler(TripleSplitter::onMouseLeftDouble), nullptr, this);
}


TripleSplitter::~TripleSplitter() {} //make sure correct destructor gets created for std::unique_ptr<SashMove>


void TripleSplitter::updateWindowSizes()
{
    if (windowL_ && windowC_ && windowR_)
    {
        const int centerPosX  = getCenterPosX();
        const int centerWidth = getCenterWidth();

        const wxRect clientRect = GetClientRect();

        const int widthL = centerPosX;
        const int windowRposX = widthL + centerWidth;
        const int widthR = clientRect.width - windowRposX;

        windowL_->SetSize(0,                  0, widthL,                         clientRect.height);
        windowC_->SetSize(widthL + SASH_SIZE, 0, windowC_->GetSize().GetWidth(), clientRect.height);
        windowR_->SetSize(windowRposX,        0, widthR,                         clientRect.height);

        wxClientDC dc(this);
        drawSash(dc);
    }
}


class TripleSplitter::SashMove
{
public:
    SashMove(wxWindow& wnd, int mousePosX, int centerOffset) : wnd_(wnd), mousePosX_(mousePosX), centerOffset_(centerOffset)
    {
        wnd_.SetCursor(wxCURSOR_SIZEWE);
        wnd_.CaptureMouse();
    }
    ~SashMove()
    {
        wnd_.SetCursor(*wxSTANDARD_CURSOR);
        if (wnd_.HasCapture())
            wnd_.ReleaseMouse();
    }
    int getMousePosXStart   () const { return mousePosX_; }
    int getCenterOffsetStart() const { return centerOffset_; }

private:
    wxWindow& wnd_;
    const int mousePosX_;
    const int centerOffset_;
};


inline
int TripleSplitter::getCenterWidth() const
{
    return 2 * SASH_SIZE + (windowC_ ? windowC_->GetSize().GetWidth() : 0);
}


int TripleSplitter::getCenterPosXOptimal() const
{
    const wxRect clientRect = GetClientRect();
    const int centerWidth = getCenterWidth();
    return (clientRect.width - centerWidth) * SASH_GRAVITY; //allowed to be negative for extreme client widths!
}


int TripleSplitter::getCenterPosX() const
{
    const wxRect clientRect = GetClientRect();
    const int centerWidth = getCenterWidth();
    const int centerPosXOptimal = getCenterPosXOptimal();

    //normalize "centerPosXOptimal + centerOffset"
    if (clientRect.width <  2 * CHILD_WINDOW_MIN_SIZE + centerWidth)
        //use fixed "centeroffset" when "clientRect.width == 2 * CHILD_WINDOW_MIN_SIZE + centerWidth"
        return centerPosXOptimal + CHILD_WINDOW_MIN_SIZE - static_cast<int>(2 * CHILD_WINDOW_MIN_SIZE * SASH_GRAVITY); //avoid rounding error
    //make sure transition between conditional branches is continuous!
    return std::max(CHILD_WINDOW_MIN_SIZE, //make sure centerPosXOptimal + offset is within bounds
                    std::min(centerPosXOptimal + centerOffset_, clientRect.width - CHILD_WINDOW_MIN_SIZE - centerWidth));
}


void TripleSplitter::drawSash(wxDC& dc)
{
    const int centerPosX  = getCenterPosX();
    const int centerWidth = getCenterWidth();

    auto draw = [&](wxRect rect)
    {
        const int sash2ndHalf = 3;
        rect.width -= sash2ndHalf;
        dc.GradientFillLinear(rect, getColorSashGradientFrom(), getColorSashGradientTo(), wxEAST);

        rect.x += rect.width;
        rect.width = sash2ndHalf;
        dc.GradientFillLinear(rect, getColorSashGradientFrom(), getColorSashGradientTo(), wxWEST);

        static_assert(SASH_SIZE > sash2ndHalf, "");
    };

    const wxRect rectSashL(centerPosX,                           0, SASH_SIZE, GetClientRect().height);
    const wxRect rectSashR(centerPosX + centerWidth - SASH_SIZE, 0, SASH_SIZE, GetClientRect().height);

    draw(rectSashL);
    draw(rectSashR);
}


bool TripleSplitter::hitOnSashLine(int posX) const
{
    const int centerPosX  = getCenterPosX();
    const int centerWidth = getCenterWidth();

    //we don't get events outside of sash, so SASH_HIT_TOLERANCE is currently *useless*
    auto hitSash = [&](int sashX) { return sashX - SASH_HIT_TOLERANCE <= posX && posX < sashX + SASH_SIZE + SASH_HIT_TOLERANCE; };

    return hitSash(centerPosX) || hitSash(centerPosX + centerWidth - SASH_SIZE); //hit one of the two sash lines
}


void TripleSplitter::onMouseLeftDown(wxMouseEvent& event)
{
    activeMove_.reset();

    const int posX = event.GetPosition().x;
    if (hitOnSashLine(posX))
        activeMove_ = std::make_unique<SashMove>(*this, posX, centerOffset_);
    event.Skip();
}


void TripleSplitter::onMouseLeftUp(wxMouseEvent& event)
{
    activeMove_.reset(); //nothing else to do, actual work done by onMouseMovement()
    event.Skip();
}


void TripleSplitter::onMouseMovement(wxMouseEvent& event)
{
    if (activeMove_)
    {
        centerOffset_ = activeMove_->getCenterOffsetStart() + event.GetPosition().x - activeMove_->getMousePosXStart();

        //CAVEAT: function getCenterPosX() normalizes centerPosX *not* centerOffset!
        //This can lead to the strange effect of window not immediately resizing when centerOffset is extremely off limits
        //=> normalize centerOffset right here
        centerOffset_ = getCenterPosX() - getCenterPosXOptimal();

        updateWindowSizes();
        Update(); //no time to wait until idle event!
    }
    else
    {
        //we receive those only while above the sash, not the managed windows (except when the managed windows are disabled!)
        const int posX = event.GetPosition().x;
        if (hitOnSashLine(posX))
            SetCursor(wxCURSOR_SIZEWE); //set window-local only!
        else
            SetCursor(*wxSTANDARD_CURSOR);
    }
    event.Skip();
}


void TripleSplitter::onLeaveWindow(wxMouseEvent& event)
{
    //even called when moving from sash over to managed windows!
    if (!activeMove_)
        SetCursor(*wxSTANDARD_CURSOR);
    event.Skip();
}


void TripleSplitter::onMouseCaptureLost(wxMouseCaptureLostEvent& event)
{
    activeMove_.reset();
    updateWindowSizes();
    //event.Skip(); -> we DID handle it!
}


void TripleSplitter::onMouseLeftDouble(wxMouseEvent& event)
{
    const int posX = event.GetPosition().x;
    if (hitOnSashLine(posX))
    {
        centerOffset_ = 0; //reset sash according to gravity
        updateWindowSizes();
    }
    event.Skip();
}

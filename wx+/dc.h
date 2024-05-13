// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef DC_H_4987123956832143243214
#define DC_H_4987123956832143243214

#include <unordered_map>
#include <optional>
//#include <zen/basic_math.h>
#include <wx/dcbuffer.h> //for macro: wxALWAYS_NATIVE_DOUBLE_BUFFER
#include <wx/dcscreen.h>
//#include <wx/bmpbndl.h>
    //    #include <gtk/gtk.h>


namespace zen
{
/*  1. wxDCClipper does *not* stack: another fix for yet another poor wxWidgets implementation

    class RecursiveDcClipper
    {
        RecursiveDcClipper(wxDC& dc, const wxRect& r)
    };

    2. wxAutoBufferedPaintDC skips one pixel on left side when RTL layout is active: a fix for a poor wxWidgets implementation

    class BufferedPaintDC
    {
        BufferedPaintDC(wxWindow& wnd, std::unique_ptr<wxBitmap>& buffer)
    };                                                                */

inline
void clearArea(wxDC& dc, const wxRect& rect, const wxColor& col)
{
    assert(col.IsSolid());
    if (rect.width  > 0 && //clearArea() is surprisingly expensive
        rect.height > 0)
    {
        //wxDC::DrawRectangle() just widens inner area if wxTRANSPARENT_PEN is used!
        //bonus: wxTRANSPARENT_PEN is about 2x faster than redundantly drawing with col!
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(col);
        dc.DrawRectangle(rect);
    }
}


//properly draw rectangle respecting high DPI (and avoiding wxPen position fuzzyness)
inline
void drawFilledRectangle(wxDC& dc, wxRect rect, const wxColor& innerCol, const wxColor& borderCol, int borderSize)
{
    assert(innerCol.IsSolid() && borderCol.IsSolid());
    if (rect.width  > 0 &&
        rect.height > 0)
    {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(borderCol);
        dc.DrawRectangle(rect);

        rect.Deflate(borderSize); //more wxWidgets design mistakes: behavior of wxRect::Deflate depends on object being const/non-const!!!

        if (rect.width  > 0 &&
            rect.height > 0)
        {
            dc.SetBrush(innerCol);
            dc.DrawRectangle(rect);
        }
    }
}


inline
void drawRectangleBorder(wxDC& dc, const wxRect& rect, const wxColor& col, int borderSize)
{
    assert(col.IsSolid());
    if (rect.width  > 0 &&
        rect.height > 0)
    {
        if (2 * borderSize >= std::min(rect.width, rect.height))
            return clearArea(dc, rect, col);

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(col);
        dc.DrawRectangle(rect.x, rect.y,                           borderSize, rect.height); //left
        dc.DrawRectangle(rect.x + rect.width - borderSize, rect.y, borderSize, rect.height); //right
        dc.DrawRectangle(rect.x, rect.y,                            rect.width, borderSize); //top
        dc.DrawRectangle(rect.x, rect.y + rect.height - borderSize, rect.width, borderSize); //bottom
    }
}


/*  figure out wxWidgets cross-platform high DPI mess:

    1. "wxsize"    := what wxWidgets is using: device-dependent on Windows, device-indepent on macOS (...mostly)
    2. screen unit := device-dependent size in pixels
    3. DIP         := device-independent pixels

    corollary:
        macOS:   "wxsize = DIP"
        Windows: "wxsize = screen unit"
        cross-platform: images are in "screen unit"         */

inline
double getScreenDpiScale()
{
    //GTK2 doesn't properly support high DPI: https://freefilesync.org/forum/viewtopic.php?t=6114
    //=> requires general fix at wxWidgets-level

    //https://github.com/wxWidgets/wxWidgets/blob/d9d05c2bb201078f5e762c42458ca2f74af5b322/include/wx/window.h#L2060
    const double scale = 1.0; //e.g. macOS, GTK3

    return scale;
}


inline
double getWxsizeDpiScale()
{
#ifndef wxHAS_DPI_INDEPENDENT_PIXELS
#error why is wxHAS_DPI_INDEPENDENT_PIXELS not defined?
#endif
    return 1.0; //e.g. macOS, GTK3
}


//similar to wxWindow::FromDIP (but tied to primary monitor and buffered)
inline int dipToWxsize   (int d) { return std::round(d * getWxsizeDpiScale() - 0.1 /*round values like 1.5 down => 1 pixel on 150% scale*/); }
inline int dipToScreen   (int d) { return std::round(d * getScreenDpiScale()); }
inline int wxsizeToScreen(int u) { return std::round(u / getWxsizeDpiScale() * getScreenDpiScale()); }
inline int screenToWxsize(int s) { return std::round(s / getScreenDpiScale() * getWxsizeDpiScale()); }

int dipToWxsize   (double d) = delete;
int dipToScreen   (double d) = delete;
int wxsizeToScreen(double d) = delete;
int screenToWxsize(double d) = delete;


inline
int getDpiScalePercent()
{
    return std::round(100 * getScreenDpiScale());
}


inline
wxBitmap toScaledBitmap(const wxImage& img /*expected to be DPI-scaled!*/)
{
    //wxBitmap(const wxImage& image, int depth = -1, double WXUNUSED(scale) = 1.0) => wxWidgets just ignores scale parameter! WTF!
    wxBitmap bmpScaled(img);
    bmpScaled.SetScaleFactor(getScreenDpiScale());
    return bmpScaled; //when testing use 175% scaling: wxWidgets' scaling logic doesn't kick in for 150% only
}


//all this shit just because wxDC::SetScaleFactor() is missing:
inline
void setScaleFactor(wxDC& dc, double scale)
{
    struct wxDcSurgeon : public wxDCImpl
    {
        void setScaleFactor(double scale) { m_contentScaleFactor = scale; }
    };
    static_cast<wxDcSurgeon*>(dc.GetImpl())->setScaleFactor(scale);
}


//add some sanity to moronic const/non-const wxRect::Intersect()
inline
wxRect getIntersection(const wxRect& rect1, const wxRect& rect2)
{
    return rect1.Intersect(rect2);
}


//---------------------- implementation ------------------------
class RecursiveDcClipper
{
public:
    RecursiveDcClipper(wxDC& dc, const wxRect& r) : dc_(dc)
    {
        if (auto it = clippingAreas_.find(&dc_);
            it != clippingAreas_.end())
        {
            oldRect_ = it->second;

            const wxRect tmp = getIntersection(r, *oldRect_); //better safe than sorry

            assert(!tmp.IsEmpty()); //"setting an empty clipping region is equivalent to DestroyClippingRegion()"

            dc_.SetClippingRegion(tmp); //new clipping region is intersection of given and previously set regions
            it->second = tmp;
        }
        else
        {
            //caveat: actual clipping region is smaller when rect is not fully inside the DC
            //=> ensure consistency for validateClippingBuffer()
            const wxRect tmp = getIntersection(r, wxRect(dc.GetSize()));

            dc_.SetClippingRegion(tmp);
            clippingAreas_.emplace(&dc_, tmp);
        }
    }

    ~RecursiveDcClipper()
    {
        dc_.DestroyClippingRegion();
        if (oldRect_)
        {
            dc_.SetClippingRegion(*oldRect_);
            clippingAreas_[&dc_] = *oldRect_;
        }
        else
            clippingAreas_.erase(&dc_);
    }

private:
    RecursiveDcClipper           (const RecursiveDcClipper&) = delete;
    RecursiveDcClipper& operator=(const RecursiveDcClipper&) = delete;


    //associate "active" clipping area with each DC
    inline static std::unordered_map<wxDC*, wxRect> clippingAreas_;

    std::optional<wxRect> oldRect_;
    wxDC& dc_;
};


#ifndef wxALWAYS_NATIVE_DOUBLE_BUFFER
    #error we need this one!
#endif

//CAVEAT: wxPaintDC on wxGTK/wxMAC does not implement SetLayoutDirection()!!! => GetLayoutDirection() == wxLayout_Default
#if wxALWAYS_NATIVE_DOUBLE_BUFFER
struct BufferedPaintDC : public wxPaintDC { BufferedPaintDC(wxWindow& wnd, std::optional<wxBitmap>& buffer) : wxPaintDC(&wnd) {} };

#else
class BufferedPaintDC : public wxMemoryDC
{
public:
    BufferedPaintDC(wxWindow& wnd, std::optional<wxBitmap>& buffer) : buffer_(buffer), paintDc_(&wnd)
    {
        const wxSize clientSize = wnd.GetClientSize();
        if (clientSize.GetWidth() > 0 && clientSize.GetHeight() > 0) //wxBitmap asserts this!! width can be 0; test case "Grid::CornerWin": compare both sides, then change config
        {
            if (!buffer_ || buffer->GetSize() != clientSize)
                buffer.emplace(clientSize);

            if (buffer->GetScaleFactor() != wnd.GetDPIScaleFactor())
                buffer->SetScaleFactor(wnd.GetDPIScaleFactor());

            SelectObject(*buffer); //copies scale factor from wxBitmap

            if (paintDc_.IsOk() && paintDc_.GetLayoutDirection() == wxLayout_RightToLeft)
                SetLayoutDirection(wxLayout_RightToLeft);
        }
        else
            buffer = {};
    }

    ~BufferedPaintDC()
    {
        if (buffer_)
        {
            if (GetLayoutDirection() == wxLayout_RightToLeft)
            {
                paintDc_.SetLayoutDirection(wxLayout_LeftToRight); //work around bug in wxDC::Blit()
                SetLayoutDirection(wxLayout_LeftToRight);          //
            }

            const wxPoint origin = GetDeviceOrigin();
            paintDc_.Blit(0, 0, buffer_->GetWidth(), buffer_->GetHeight(), this, -origin.x, -origin.y);
        }
    }

private:
    BufferedPaintDC           (const BufferedPaintDC&) = delete;
    BufferedPaintDC& operator=(const BufferedPaintDC&) = delete;

    std::optional<wxBitmap>& buffer_;
    wxPaintDC paintDc_;
};
#endif
}

#endif //DC_H_4987123956832143243214

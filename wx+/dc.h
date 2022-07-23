// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef DC_H_4987123956832143243214
#define DC_H_4987123956832143243214

#include <unordered_map>
#include <optional>
#include <zen/basic_math.h>
#include <wx/dcbuffer.h> //for macro: wxALWAYS_NATIVE_DOUBLE_BUFFER
#include <wx/dcscreen.h>
#include <wx/bmpbndl.h>
    #include <gtk/gtk.h>


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
    if (rect.width  > 0 && //clearArea() is surprisingly expensive
        rect.height > 0)
    {
        assert(col.IsSolid());
        //wxDC::DrawRectangle() just widens inner area if wxTRANSPARENT_PEN is used!
        //bonus: wxTRANSPARENT_PEN is about 2x faster than redundantly drawing with col!
        wxDCPenChanger   areaPen  (dc, *wxTRANSPARENT_PEN);
        wxDCBrushChanger areaBrush(dc, col);
        dc.DrawRectangle(rect);
    }
}


//properly draw rectangle respecting high DPI (and avoiding wxPen position fuzzyness)
inline
void drawInsetRectangle(wxDC& dc, wxRect rect, int borderWidth, const wxColor& borderCol, const wxColor& innerCol)
{
    if (rect.width  > 0 &&
        rect.height > 0)
    {
        assert(borderCol.IsSolid() && innerCol.IsSolid());
        wxDCPenChanger   rectPen  (dc, *wxTRANSPARENT_PEN);
        wxDCBrushChanger rectBrush(dc, borderCol);
        dc.DrawRectangle(rect);
        rect.Deflate(borderWidth); //attention, more wxWidgets design mistakes: behavior of wxRect::Deflate depends on object being const/non-const!!!

        dc.SetBrush(innerCol);
        dc.DrawRectangle(rect);
    }
}


inline
void drawInsetRectangle(wxDC& dc, const wxRect& rect, int borderWidth, const wxColor& col)
{
    if (rect.width  > 0 &&
        rect.height > 0)
    {
        assert(col.IsSolid());
        wxDCPenChanger   areaPen  (dc, *wxTRANSPARENT_PEN);
        wxDCBrushChanger areaBrush(dc, col);
        dc.DrawRectangle(rect.GetTopLeft(),                                         {borderWidth, rect.height});
        dc.DrawRectangle(rect.GetTopLeft() + wxPoint{rect.width - borderWidth, 0},  {borderWidth, rect.height});
        dc.DrawRectangle(rect.GetTopLeft(),                                         {rect.width, borderWidth});
        dc.DrawRectangle(rect.GetTopLeft() + wxPoint{0, rect.height - borderWidth}, {rect.width, borderWidth});
    }
}


/* Standard DPI:
     Windows/Ubuntu: 96 x 96
     macOS: wxWidgets uses DIP (note: wxScreenDC().GetPPI() returns 72 x 72 which is a lie; looks like 96 x 96)       */
constexpr int defaultDpi = 96; //on Windows same as wxDisplay::GetStdPPIValue() (however returns 72 on macOS!)

inline
int getDPI()
{
#ifndef wxHAS_DPI_INDEPENDENT_PIXELS
#error why is wxHAS_DPI_INDEPENDENT_PIXELS not defined?
#endif
    //GTK2 doesn't properly support high DPI: https://freefilesync.org/forum/viewtopic.php?t=6114
    //=> requires general fix at wxWidgets-level

    //https://github.com/wxWidgets/wxWidgets/blob/d9d05c2bb201078f5e762c42458ca2f74af5b322/include/wx/window.h#L2060
    return defaultDpi; //e.g. macOS, GTK3
}


inline
double getDisplayScaleFactor()
{
    return static_cast<double>(getDPI()) / defaultDpi;
}


inline
int fastFromDIP(int d) //like wxWindow::FromDIP (but tied to primary monitor and buffered)
{
    return numeric::intDivRound(d * getDPI() - 10 /*round values like 1.5 down => 1 pixel on 150% scale*/, defaultDpi);
}
int fastFromDIP(double d) = delete;


inline
int getDpiScalePercent()
{
    return numeric::intDivRound(100 * getDPI(), defaultDpi);
}


inline
wxBitmap toScaledBitmap(const wxImage& img /*expected to be DPI-scaled!*/)
{
    //wxBitmap(const wxImage& image, int depth = -1, double WXUNUSED(scale) = 1.0) => wxWidgets just ignores scale parameter! WTF!
    wxBitmap bmpScaled(img);
    bmpScaled.SetScaleFactor(getDisplayScaleFactor());
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


//---------------------- implementation ------------------------
class RecursiveDcClipper
{
public:
    RecursiveDcClipper(wxDC& dc, const wxRect& r) : dc_(dc)
    {
        if (auto it = clippingAreas_.find(&dc);
            it != clippingAreas_.end())
        {
            oldRect_ = it->second;

            wxRect tmp = r;
            tmp.Intersect(*oldRect_); //better safe than sorry

            assert(!tmp.IsEmpty()); //"setting an empty clipping region is equivalent to DestroyClippingRegion()"

            dc_.SetClippingRegion(tmp); //new clipping region is intersection of given and previously set regions
            it->second = tmp;
        }
        else
        {
            dc_.SetClippingRegion(r);
            clippingAreas_.emplace(&dc_, r);
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
        if (clientSize.GetWidth() > 0 && clientSize.GetHeight() > 0) //wxBitmap asserts this!! width may be 0; test case "Grid::CornerWin": compare both sides, then change config
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

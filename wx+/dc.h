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
        //wxDC::DrawRectangle() just widens inner area if wxTRANSPARENT_PEN is used!
        //bonus: wxTRANSPARENT_PEN is about 2x faster than redundantly drawing with col!
        wxDCPenChanger   dummy (dc, *wxTRANSPARENT_PEN);
        wxDCBrushChanger dummy2(dc, col);
        dc.DrawRectangle(rect);
    }
}


//properly draw rectangle respecting high DPI (and avoiding wxPen position fuzzyness)
inline
void drawFilledRectangle(wxDC& dc, wxRect rect, int borderWidth, const wxColor& borderCol, const wxColor& innerCol)
{
    assert(borderCol.IsSolid() && innerCol.IsSolid());
    wxDCPenChanger   graphPen  (dc, *wxTRANSPARENT_PEN);
    wxDCBrushChanger graphBrush(dc, borderCol);
    dc.DrawRectangle(rect);
    rect.Deflate(borderWidth); //attention, more wxWidgets design mistakes: behavior of wxRect::Deflate depends on object being const/non-const!!!

    dc.SetBrush(innerCol);
    dc.DrawRectangle(rect);
}


/* Standard DPI:
     Windows/Ubuntu: 96 x 96
     macOS: wxWidgets uses DIP (note: wxScreenDC().GetPPI() returns 72 x 72 which is a lie; looks like 96 x 96)       */

inline
int fastFromDIP(int d) //like wxWindow::FromDIP (but tied to primary monitor and buffered)
{
#ifndef wxHAVE_DPI_INDEPENDENT_PIXELS
#error why is wxHAVE_DPI_INDEPENDENT_PIXELS not defined?
#endif
    //GTK2 doesn't properly support high DPI: https://freefilesync.org/forum/viewtopic.php?t=6114
    //=> requires general fix at wxWidgets-level

    //https://github.com/wxWidgets/wxWidgets/blob/d9d05c2bb201078f5e762c42458ca2f74af5b322/include/wx/window.h#L2060
    return d; //e.g. macOS, GTK3
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
            tmp.Intersect(*oldRect_);   //better safe than sorry
            dc_.SetClippingRegion(tmp); //
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
            if (!buffer_ || clientSize != wxSize(buffer->GetWidth(), buffer->GetHeight()))
                buffer = wxBitmap(clientSize.GetWidth(), clientSize.GetHeight());

            SelectObject(*buffer);

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
                paintDc_.SetLayoutDirection(wxLayout_LeftToRight); //workaround bug in wxDC::Blit()
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

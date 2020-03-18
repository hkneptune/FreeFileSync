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
/*
    1. wxDCClipper does *not* stack: another fix for yet another poor wxWidgets implementation

        class RecursiveDcClipper
        {
            RecursiveDcClipper(wxDC& dc, const wxRect& r)
        };

    2. wxAutoBufferedPaintDC skips one pixel on left side when RTL layout is active: a fix for a poor wxWidgets implementation

        class BufferedPaintDC
        {
            BufferedPaintDC(wxWindow& wnd, std::unique_ptr<wxBitmap>& buffer)
        };
*/


inline
void clearArea(wxDC& dc, const wxRect& rect, const wxColor& col)
{
    wxDCPenChanger   dummy (dc, col);
    wxDCBrushChanger dummy2(dc, col);
    dc.DrawRectangle(rect);
}


/*
Standard DPI:
    Windows/Ubuntu: 96 x 96
    macOS: wxWidgets uses DIP (note: wxScreenDC().GetPPI() returns 72 x 72 which is a lie; looks like 96 x 96)
*/
inline
int fastFromDIP(int d) //like wxWindow::FromDIP (but tied to primary monitor and buffered)
{
#ifdef wxHAVE_DPI_INDEPENDENT_PIXELS //pulled from wx/window.h: https://github.com/wxWidgets/wxWidgets/blob/master/include/wx/window.h#L2029
    return d; //e.g. macOS, GTK3
#else //https://github.com/wxWidgets/wxWidgets/blob/master/src/common/wincmn.cpp#L2865
    static_assert(GTK_MAJOR_VERSION == 2);
    //GTK2 doesn't properly support high DPI: https://freefilesync.org/forum/viewtopic.php?t=6114
    //=> requires general fix at wxWidgets-level
    assert(wxTheApp); //only call after wxWidgets was initalized!
    static const int dpiY = wxScreenDC().GetPPI().y; //perf: buffering for calls to ::GetDeviceCaps() needed!?
    const int defaultDpi = 96;
    return numeric::round(1.0 * d * dpiY / defaultDpi);
#endif
}




//---------------------- implementation ------------------------
class RecursiveDcClipper
{
public:
    RecursiveDcClipper(wxDC& dc, const wxRect& r) : dc_(dc)
    {
        auto it = refDcToAreaMap().find(&dc);
        if (it != refDcToAreaMap().end())
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
            refDcToAreaMap().emplace(&dc_, r);
        }
    }

    ~RecursiveDcClipper()
    {
        dc_.DestroyClippingRegion();
        if (oldRect_)
        {
            dc_.SetClippingRegion(*oldRect_);
            refDcToAreaMap()[&dc_] = *oldRect_;
        }
        else
            refDcToAreaMap().erase(&dc_);
    }

private:
    //associate "active" clipping area with each DC
    static std::unordered_map<wxDC*, wxRect>& refDcToAreaMap() { static std::unordered_map<wxDC*, wxRect> clippingAreas; return clippingAreas; }

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
    std::optional<wxBitmap>& buffer_;
    wxPaintDC paintDc_;
};
#endif
}

#endif //DC_H_4987123956832143243214

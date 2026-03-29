// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "tray_icon.h"
#include <zen/i18n.h>
#include <wx/taskbar.h>
#include <wx/menu.h>
#include <wx/icon.h> //req. by Linux
#include <wx+/dc.h>
#include <wx+/image_tools.h>
#include <wx+/image_resources.h>

using namespace zen;
using namespace fff;


namespace
{
void fillRange(wxImage& img, int pixFirst, int pixLast, const wxColor& col) //tolerant input range
{
    const int width  = img.GetWidth ();
    const int height = img.GetHeight();

    if (width > 0 && height > 0)
    {
        pixFirst = std::max(pixFirst, 0);
        pixLast  = std::min(pixLast, width * height);

        if (pixFirst < pixLast)
        {
            const unsigned char r = col.Red  (); //
            const unsigned char g = col.Green(); //getting RGB involves virtual function calls!
            const unsigned char b = col.Blue (); //

            unsigned char* rgb    = img.GetData() + pixFirst * 3;
            unsigned char* rgbEnd = img.GetData() + pixLast  * 3;
            while (rgb < rgbEnd)
            {
                *rgb++ = r;
                *rgb++ = g;
                *rgb++ = b;
            }

            if (img.HasAlpha()) //make progress indicator fully opaque:
                ::memset(img.GetAlpha() + pixFirst, wxIMAGE_ALPHA_OPAQUE, pixLast - pixFirst);
        }
    }
}
}
//------------------------------------------------------------------------------------------------


//generate icon with progress indicator
class FfsTrayIcon::ProgressIconGenerator
{
public:
    explicit ProgressIconGenerator(const wxImage& logo) : logo_(logo) {}

    wxBitmap get(double fraction);

private:
    const wxImage logo_;
    wxBitmap iconBuf_;
    int startPixBuf_ = -1;
};


wxBitmap FfsTrayIcon::ProgressIconGenerator::get(double fraction)
{
    if (!logo_.IsOk() || logo_.GetWidth() <= 0 || logo_.GetHeight() <= 0)
        return wxIcon();

    const int pixCount = logo_.GetWidth() * logo_.GetHeight();
    const int pixStart = std::clamp<int>(std::floor(fraction * pixCount), 0, pixCount);

    if (startPixBuf_ != pixStart)
    {
        wxImage genImage(logo_.Copy()); //workaround wxWidgets' screwed-up design from hell: their copy-construction implements reference-counting WITHOUT copy-on-write!

        //gradually make FFS icon brighter while nearing completion
        brighten(genImage, -200 * (1 - fraction));

        //fill black border row
        if (pixStart <= pixCount - genImage.GetWidth())
        {
            /*    --------
                  ---bbbbb
                  bbbbSyyy  S : start yellow remainder
                  yyyyyyyy                          */

            int bStart = pixStart - genImage.GetWidth();
            if (bStart % genImage.GetWidth() != 0) //add one more black pixel, see ascii-art
                --bStart;
            fillRange(genImage, bStart, pixStart, *wxBLACK);
        }
        else if (pixStart < pixCount)
        {
            /* special handling for last row:
                  --------
                  --------
                  ---bbbbb
                  ---bSyyy  S : start yellow remainder            */

            int bStart = pixStart - genImage.GetWidth() - 1;
            int bEnd = (bStart / genImage.GetWidth() + 1) * genImage.GetWidth();

            fillRange(genImage, bStart, bEnd, *wxBLACK);
            fillRange(genImage, pixStart - 1, pixStart, *wxBLACK);
        }

        //fill yellow remainder
        fillRange(genImage, pixStart, pixCount, wxColor(240, 200, 0));

        iconBuf_ = toScaledBitmap(genImage);
        startPixBuf_ = pixStart;
    }

    return iconBuf_;
}


class FfsTrayIcon::TrayIconImpl : public wxTaskBarIcon
{
public:
    TrayIconImpl(const std::function<void()>& requestResume) : requestResume_(requestResume)
    {
        Bind(wxEVT_TASKBAR_LEFT_UP, [this](wxTaskBarIconEvent& event) { if (requestResume_) requestResume_(); });
    }

    void disconnectCallbacks() { requestResume_ = nullptr; }

private:
    wxMenu* CreatePopupMenu() override
    {
        if (!requestResume_)
            return nullptr;

        wxMenu* contextMenu = new wxMenu;

        wxMenuItem* defaultItem = new wxMenuItem(contextMenu, wxID_ANY, _("&Restore"));
        //wxWidgets font mess-up:
        //1. font must be set *before* wxMenu::Append()!
        //2. don't use defaultItem->GetFont(); making it bold creates a huge font size for some reason
        contextMenu->Append(defaultItem);

        contextMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent& event) { if (requestResume_) requestResume_(); }, defaultItem->GetId());

        return contextMenu; //ownership transferred to caller
    }

    //void onLeftDownClick(wxEvent& event)
    //{
    //  //copied from wxTaskBarIconBase::OnRightButtonDown()
    //   if (wxMenu* menu = CreatePopupMenu())
    //   {
    //       PopupMenu(menu);
    //       delete menu;
    //   }
    //}

    std::function<void()> requestResume_;
};


FfsTrayIcon::FfsTrayIcon(const std::function<void()>& requestResume) :
    trayIcon_(new TrayIconImpl(requestResume)),
    progressIcon_(std::make_unique<ProgressIconGenerator>(loadImage("start_sync", dipToScreen(24))))
{
    [[maybe_unused]] const bool rv = trayIcon_->SetIcon(progressIcon_->get(activeFraction_), activeToolTip_);
    assert(rv); //caveat wxTaskBarIcon::SetIcon() can return true, even if not wxTaskBarIcon::IsAvailable()!!!
}


FfsTrayIcon::~FfsTrayIcon()
{
    trayIcon_->disconnectCallbacks(); //TrayIconImpl has longer lifetime than FfsTrayIcon: avoid callback!

    trayIcon_->RemoveIcon();

    //*schedule* for destruction: delete during next idle event (handle late window messages, e.g. when double-clicking)
    trayIcon_->Destroy(); //uses wxPendingDelete
}


void FfsTrayIcon::setToolTip(const wxString& toolTip)
{
    activeToolTip_ = toolTip;
    trayIcon_->SetIcon(progressIcon_->get(activeFraction_), activeToolTip_); //another wxWidgets design bug: non-orthogonal method!
}


void FfsTrayIcon::setProgress(double fraction)
{
    activeFraction_ = fraction;
    trayIcon_->SetIcon(progressIcon_->get(activeFraction_), activeToolTip_);
}

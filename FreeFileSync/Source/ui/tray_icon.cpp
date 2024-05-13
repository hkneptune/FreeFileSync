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
void fillRange(wxImage& img, int pixelFirst, int pixelLast, const wxColor& col) //tolerant input range
{
    if (img.IsOk())
    {
        const int width  = img.GetWidth ();
        const int height = img.GetHeight();

        if (width > 0 && height > 0)
        {
            pixelFirst = std::max(pixelFirst, 0);
            pixelLast  = std::min(pixelLast, width * height);

            if (pixelFirst < pixelLast)
            {
                unsigned char* const bytesBegin = img.GetData() + pixelFirst * 3;
                unsigned char* const bytesEnd   = img.GetData() + pixelLast  * 3;

                for (unsigned char* bytePos = bytesBegin; bytePos < bytesEnd; bytePos += 3)
                {
                    bytePos[0] = col.Red  ();
                    bytePos[1] = col.Green();
                    bytePos[2] = col.Blue ();
                }

                if (img.HasAlpha()) //make progress indicator fully opaque:
                    std::fill(img.GetAlpha() + pixelFirst, img.GetAlpha() + pixelLast, wxIMAGE_ALPHA_OPAQUE);
            }
        }
    }
}
}
//------------------------------------------------------------------------------------------------


//generate icon with progress indicator
class FfsTrayIcon::ProgressIconGenerator
{
public:
    ProgressIconGenerator(const wxImage& logo) : logo_(logo) {}

    wxIcon get(double fraction);

private:
    const wxImage logo_;
    wxIcon iconBuf_;
    int startPixBuf_ = -1;
};


wxIcon FfsTrayIcon::ProgressIconGenerator::get(double fraction)
{
    if (!logo_.IsOk() || logo_.GetWidth() <= 0 || logo_.GetHeight() <= 0)
        return wxIcon();

    const int pixelCount = logo_.GetWidth() * logo_.GetHeight();
    const int startFillPixel = std::clamp<int>(std::round(fraction * pixelCount), 0, pixelCount);

    if (startPixBuf_ != startFillPixel)
    {
        wxImage genImage(logo_.Copy()); //workaround wxWidgets' screwed-up design from hell: their copy-construction implements reference-counting WITHOUT copy-on-write!

        //gradually make FFS icon brighter while nearing completion
        zen::brighten(genImage, -200 * (1 - fraction));

        //fill black border row
        if (startFillPixel <= pixelCount - genImage.GetWidth())
        {
            /*    --------
                  ---bbbbb
                  bbbbSyyy  S : start yellow remainder
                  yyyyyyyy                          */

            int bStart = startFillPixel - genImage.GetWidth();
            if (bStart % genImage.GetWidth() != 0) //add one more black pixel, see ascii-art
                --bStart;
            fillRange(genImage, bStart, startFillPixel, *wxBLACK);
        }
        else if (startFillPixel < pixelCount)
        {
            /* special handling for last row:
                  --------
                  --------
                  ---bbbbb
                  ---bSyyy  S : start yellow remainder            */

            int bStart = startFillPixel - genImage.GetWidth() - 1;
            int bEnd = (bStart / genImage.GetWidth() + 1) * genImage.GetWidth();

            fillRange(genImage, bStart, bEnd, *wxBLACK);
            fillRange(genImage, startFillPixel - 1, startFillPixel, *wxBLACK);
        }

        //fill yellow remainder
        fillRange(genImage, startFillPixel, pixelCount, wxColor(240, 200, 0));

        iconBuf_.CopyFromBitmap(genImage);
        startPixBuf_ = startFillPixel;
    }

    return iconBuf_;
}


class FfsTrayIcon::TaskBarImpl : public wxTaskBarIcon
{
public:
    TaskBarImpl(const std::function<void()>& requestResume) : requestResume_(requestResume)
    {
        Bind(wxEVT_TASKBAR_LEFT_DCLICK, [this](wxTaskBarIconEvent& event) { onDoubleClick(event); });

        //Windows User Experience Guidelines: show the context menu rather than doing *nothing* on single left clicks; however:
        //MSDN: "Double-clicking the left mouse button actually generates a sequence of four messages: WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK, and WM_LBUTTONUP."
        //Reference: https://docs.microsoft.com/en-us/windows/win32/inputdev/wm-lbuttondblclk
        //=> the only way to distinguish single left click and double-click is to wait wxSystemSettings::GetMetric(wxSYS_DCLICK_MSEC) (480ms) which is way too long!
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

    void onDoubleClick(wxEvent& event)
    {
        if (requestResume_)
            requestResume_();
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
    trayIcon_(new TaskBarImpl(requestResume)),
    iconGenerator_(std::make_unique<ProgressIconGenerator>(loadImage("start_sync", dipToScreen(24))))
{
    [[maybe_unused]] const bool rv = trayIcon_->SetIcon(iconGenerator_->get(activeFraction_), activeToolTip_);
    assert(rv); //caveat wxTaskBarIcon::SetIcon() can return true, even if not wxTaskBarIcon::IsAvailable()!!!
}


FfsTrayIcon::~FfsTrayIcon()
{
    trayIcon_->disconnectCallbacks(); //TaskBarImpl has longer lifetime than FfsTrayIcon: avoid callback!

    /*  This is not working correctly on OS X! It seems both wxTaskBarIcon::RemoveIcon() and ~wxTaskBarIcon() are broken and do NOT immediately
        remove the icon from the system tray! Only some time later in the event loop which called these functions they will be removed.
        Maybe some system component has still shared ownership? Objective C auto release pools are freed at the end of the current event loop...
        Anyway, wxWidgets fails to disconnect the wxTaskBarIcon event handlers before calling "[m_statusitem release]"!

        => !!!clicking on the icon after ~wxTaskBarIcon ran crashes the application!!!

        - if ~wxTaskBarIcon() ran from the SyncProgressDialog::updateGui() event loop (e.g. user manually clicking the icon) => icon removed on return
        - if ~wxTaskBarIcon() ran from SyncProgressDialog::closeDirectly() => leaves the icon dangling until user closes this dialog and outter event loop runs!

        2021_01-04: yes the system indeed has a reference because wxWidgets forgets to call NSStatusBar::removeStatusItem in wxTaskBarIconCustomStatusItemImpl::RemoveIcon()
            => when this call is added, all these defered deletion shenanigans are NOT NEEDED ANYMORE! (still wxWidgets should really add [m_statusItem setTarget:nil]!)
        */
    trayIcon_->RemoveIcon();

    //*schedule* for destruction: delete during next idle loop iteration (handle late window messages, e.g. when double-clicking)
    trayIcon_->Destroy();
}


void FfsTrayIcon::setToolTip(const wxString& toolTip)
{
    activeToolTip_ = toolTip;
    trayIcon_->SetIcon(iconGenerator_->get(activeFraction_), activeToolTip_); //another wxWidgets design bug: non-orthogonal method!
}


void FfsTrayIcon::setProgress(double fraction)
{
    activeFraction_ = fraction;
    trayIcon_->SetIcon(iconGenerator_->get(activeFraction_), activeToolTip_);
}

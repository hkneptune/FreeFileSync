// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FOCUS_1084731021985757843
#define FOCUS_1084731021985757843

#include <wx/toplevel.h>
#include <wx/display.h>


namespace zen
{
//pretty much the same like "bool wxWindowBase::IsDescendant(wxWindowBase* child) const" but without the obvious misnomer
inline
bool isComponentOf(const wxWindow* child, const wxWindow* top)
{
    for (const wxWindow* wnd = child; wnd != nullptr; wnd = wnd->GetParent())
        if (wnd == top)
            return true;
    return false;
}


inline
wxWindow& getRootWindow(wxWindow& child)
{
    wxWindow* root = &child;
    for (;;)
        if (wxWindow* parent = root->GetParent())
            root = parent;
        else
            return *root;
}


inline
wxTopLevelWindow* getTopLevelWindow(wxWindow* child)
{
    for (wxWindow* wnd = child; wnd != nullptr; wnd = wnd->GetParent())
        if (auto tlw = dynamic_cast<wxTopLevelWindow*>(wnd)) //why does wxWidgets use wxWindows::IsTopLevel() ??
            return tlw;
    return nullptr;
}


/* Preserving input focus has to be more clever than:
     wxWindow* oldFocus = wxWindow::FindFocus();
     ZEN_ON_SCOPE_EXIT(if (oldFocus) oldFocus->SetFocus());

=> wxWindow::SetFocus() internally calls Win32 ::SetFocus, which calls ::SetActiveWindow, which - lord knows why - changes the foreground window to the focus window
    even if the user is currently busy using a different app! More curiosity: this foreground focus stealing happens only during the *first* SetFocus() after app start!
    It also can be avoided by changing focus back and forth with some other app after start => wxWidgets bug or Win32 feature???                                      */

struct FocusPreserver
{
    FocusPreserver()
    {
        if (wxWindow* win = wxWindow::FindFocus())
            setFocus(win);
    }

    ~FocusPreserver()
    {
        //wxTopLevelWindow::IsActive() does NOT call Win32 ::GetActiveWindow()!
        //Instead it checks if ::GetFocus() is set somewhere inside the top level
        //Note: Both Win32 active and focus windows are *thread-local* values, while foreground window is global! https://devblogs.microsoft.com/oldnewthing/20131016-00/?p=2913

        if (oldFocusId_ != wxID_ANY)
            if (wxWindow* oldFocusWin = wxWindow::FindWindowById(oldFocusId_))
                if (wxTopLevelWindow* topWin = getTopLevelWindow(oldFocusWin))
                    if (topWin->IsActive()) //Linux/macOS: already behaves just like ::GetForegroundWindow() on Windows!
                        oldFocusWin->SetFocus();
    }

    wxWindowID getFocusId() const { return oldFocusId_; }

    void setFocus(wxWindow* win)
    {
        oldFocusId_ = win->GetId();
        assert(oldFocusId_ != wxID_ANY);
    }

private:
    wxWindowID oldFocusId_ = wxID_ANY;
    //don't store wxWindow* which may be dangling during ~FocusPreserver()!
    //test: click on delete folder pair and immediately press F5 => focus window (= FP del button) is defer-deleted during sync
};


namespace
{
void setInitialWindowSize(wxTopLevelWindow& topWin, wxSize size, std::optional<wxPoint> pos, bool isMaximized, wxSize defaultSize)
{
    wxSize newSize = defaultSize;
    std::optional<wxPoint> newPos;
    //set dialog size and position:
    // - width/height are invalid if the window is minimized (eg x,y = -32000; width = 160, height = 28)
    // - multi-monitor setup: dialog may be placed on second monitor which is currently turned off
    if (size.GetWidth () > 0 &&
        size.GetHeight() > 0)
    {
        if (pos)
        {
            //calculate how much of the dialog will be visible on screen
            const int dlgArea = size.GetWidth() * size.GetHeight();
            int dlgAreaMaxVisible = 0;

            const int monitorCount = wxDisplay::GetCount();
            for (int i = 0; i < monitorCount; ++i)
            {
                wxRect overlap = wxDisplay(i).GetClientArea().Intersect(wxRect(*pos, size));
                dlgAreaMaxVisible = std::max(dlgAreaMaxVisible, overlap.GetWidth() * overlap.GetHeight());
            }

            if (dlgAreaMaxVisible > 0.1 * dlgArea //at least 10% of the dialog should be visible!
               )
            {
                newSize = size;
                newPos  = pos;
            }
        }
        else
            newSize = size;
    }

    //old comment: "wxGTK's wxWindow::SetSize seems unreliable and behaves like a wxWindow::SetClientSize
    //              => use wxWindow::SetClientSize instead (for the record: no such issue on Windows/macOS)
    //2018-10-15: Weird new problem on CentOS/Ubuntu: SetClientSize() + SetPosition() fail to set correct dialog *position*, but SetSize() + SetPosition() do!
    //              => old issues with SetSize() seem to be gone... => revert to SetSize()
    if (newPos)
        topWin.SetSize(wxRect(*newPos, newSize));
    else
    {
        topWin.SetSize(newSize);
        topWin.Center();
    }

    if (isMaximized) //no real need to support both maximize and full screen functions
    {
        topWin.Maximize(true);
    }
}


struct WindowLayoutWeak
{
    std::optional<wxSize> size;
    std::optional<wxPoint> pos;
    bool isMaximized = false;
};
//destructive! changes window size!
WindowLayoutWeak getWindowSizeBeforeClose(wxTopLevelWindow& topWin)
{
    //we need to portably retrieve non-iconized, non-maximized size and position
    //  non-portable: Win32 GetWindowPlacement(); wxWidgets take: wxTopLevelWindow::RestoreToGeometry()
    if (topWin.IsIconized())
        topWin.Iconize(false);

    WindowLayoutWeak layout;
    if (topWin.IsMaximized()) //evaluate AFTER uniconizing!
    {
        topWin.Maximize(false);
        layout.isMaximized = true;
    }

    layout.size = topWin.GetSize();
    layout.pos  = topWin.GetPosition();

    if (layout.isMaximized)
        if (!topWin.IsShown() //=> Win: can't trust size GetSize()/GetPosition(): still at full screen size!
            //wxGTK: returns full screen size and strange position (65/-4)
            //OS X 10.9 (but NO issue on 10.11!) returns full screen size and strange position (0/-22)
            || layout.pos->y < 0
           )
        {
            layout.size = std::nullopt;
            layout.pos  = std::nullopt;
        }

    return layout;
}
}
}

#endif //FOCUS_1084731021985757843

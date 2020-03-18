// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FOCUS_1084731021985757843
#define FOCUS_1084731021985757843

#include <wx/toplevel.h>


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
wxTopLevelWindow* getTopLevelWindow(wxWindow* child)
{
    for (wxWindow* wnd = child; wnd != nullptr; wnd = wnd->GetParent())
        if (auto tlw = dynamic_cast<wxTopLevelWindow*>(wnd)) //why does wxWidgets use wxWindows::IsTopLevel() ??
            return tlw;
    return nullptr;
}


/*
Preserving input focus has to be more clever than:
    wxWindow* oldFocus = wxWindow::FindFocus();
    ZEN_ON_SCOPE_EXIT(if (oldFocus) oldFocus->SetFocus());

=> wxWindow::SetFocus() internally calls Win32 ::SetFocus, which calls ::SetActiveWindow, which - lord knows why - changes the foreground window to the focus window
    even if the user is currently busy using a different app! More curiosity: this foreground focus stealing happens only during the *first* SetFocus() after app start!
    It also can be avoided by changing focus back and forth with some other app after start => wxWidgets bug or Win32 feature???
*/
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
}

#endif //FOCUS_1084731021985757843

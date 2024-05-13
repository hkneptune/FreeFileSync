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

inline
void setFocusIfActive(wxWindow& win) //don't steal keyboard focus when currently using a different foreground application
{
    if (wxTopLevelWindow* topWin = getTopLevelWindow(&win))
        if (topWin->IsActive()) //Linux/macOS: already behaves just like ::GetForegroundWindow() on Windows!
            win.SetFocus();
}


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
                setFocusIfActive(*oldFocusWin);
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


class WindowLayout
{
public:
    struct Dimensions
    {
        std::optional<wxSize> size;
        std::optional<wxPoint> pos;
        bool isMaximized = false;
    };
    static void setInitial(wxTopLevelWindow& topWin, const Dimensions& dim, wxSize defaultSize)
    {
        initialDims_[&topWin] = dim;

        wxSize newSize = defaultSize;
        std::optional<wxPoint> newPos;
        //set dialog size and position:
        // - width/height are invalid if the window is minimized (eg x,y = -32000; width = 160, height = 28)
        // - multi-monitor setup: dialog may be placed on second monitor which is currently turned off
        if (dim.size &&
            dim.size->GetWidth () > 0 &&
            dim.size->GetHeight() > 0)
        {
            if (dim.pos)
            {
                //calculate how much of the dialog will be visible on screen
                const int dlgArea = dim.size->GetWidth() * dim.size->GetHeight();
                int dlgAreaMaxVisible = 0;

                const int monitorCount = wxDisplay::GetCount();
                for (int i = 0; i < monitorCount; ++i)
                {
                    wxRect overlap = wxDisplay(i).GetClientArea().Intersect(wxRect(*dim.pos, *dim.size));
                    dlgAreaMaxVisible = std::max(dlgAreaMaxVisible, overlap.GetWidth() * overlap.GetHeight());
                }

                if (dlgAreaMaxVisible > 0.1 * dlgArea //at least 10% of the dialog should be visible!
                   )
                {
                    newSize = *dim.size;
                    newPos  = dim.pos;
                }
            }
            else
                newSize = *dim.size;
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

        if (dim.isMaximized) //no real need to support both maximize and full screen functions
        {
            topWin.Maximize(true);
        }


#if 0 //wxWidgets alternative: apparently no benefits (not even on Wayland! but strange decisions: why restore the minimized state!???)
        class GeoSerializer : public wxTopLevelWindow::GeometrySerializer
        {
        public:
            GeoSerializer(const std::string& l)
            {
                split(l, ' ', [&](const std::string_view phrase)
                {
                    assert(phrase.empty() || contains(phrase, '='));
                    if (contains(phrase, '='))
                        valuesByName_[utfTo<wxString>(beforeFirst(phrase, '=', IfNotFoundReturn::none))] =
                            /**/         stringTo<int>(afterFirst(phrase, '=', IfNotFoundReturn::none));
                });
            }

            bool SaveField(const wxString& name, int value) const /*NO, this must not be const!*/ override { return false; }

            bool RestoreField(const wxString& name, int* value) /*const: yes, this MAY(!) be const*/ override
            {
                auto it = valuesByName_.find(name);
                if (it == valuesByName_.end())
                    return false;
                * value = it->second;
                return true;
            }
        private:
            std::unordered_map<wxString, int> valuesByName_;
        } serializer(layout);

        if (!topWin.RestoreToGeometry(serializer)) //apparently no-fail as long as GeometrySerializer::RestoreField is!
            assert(false);
#endif
    }

    //destructive! changes window size!
    static Dimensions getBeforeClose(wxTopLevelWindow& topWin)
    {
        //we need to portably retrieve non-iconized, non-maximized size and position
        //  non-portable: Win32 GetWindowPlacement(); wxWidgets take: wxTopLevelWindow::SaveGeometry/RestoreToGeometry()
        if (topWin.IsIconized())
            topWin.Iconize(false);

        bool isMaximized = false;
        if (topWin.IsMaximized()) //evaluate AFTER uniconizing!
        {
            topWin.Maximize(false);
            isMaximized = true;
        }

        std::optional<wxSize> size = topWin.GetSize();
        std::optional<wxPoint> pos = topWin.GetPosition();

        if (isMaximized)
            if (!topWin.IsShown() //=> Win: can't trust size GetSize()/GetPosition(): still at full screen size!
                //wxGTK: returns full screen size and strange position (65/-4)
                //OS X 10.9 (but NO issue on 10.11!) returns full screen size and strange position (0/-22)
                || pos->y < 0
               )
            {
                size = std::nullopt;
                pos  = std::nullopt;
            }

        //reuse previous values if current ones are not available:
        if (const auto it = initialDims_.find(&topWin);
            it != initialDims_.end())
        {
            if (!size)
                size = it->second.size;

            if (!pos)
                pos = it->second.pos;
        }

        return {size, pos, isMaximized};

#if 0 //wxWidgets alternative: apparently no benefits (not even on Wayland! but strange decisions: why restore the minimized state!???)
        struct : wxTopLevelWindow::GeometrySerializer
        {
            bool SaveField(const wxString& name, int value) const /*NO, this must not be const!*/ override
            {
                layout_ += utfTo<std::string>(name) + '=' + numberTo<std::string>(value) + ' ';
                return true;
            }

            bool RestoreField(const wxString& name, int* value) /*const: yes, this MAY(!) be const*/ override { return false; }

            mutable //wxWidgets people: 1. learn when and when not to use const for input/output functions! see SaveField/RestoreField()
            //                          2. learn flexible software design: why are input/output tied up in a single GeometrySerializer implementation?
            std::string layout_;
        } serializer;

        if (topWin.SaveGeometry(serializer)) //apparently no-fail as long as GeometrySerializer::SaveField is!
            return serializer.layout_;
        else
            assert(false);
#endif
    }

private:
    inline static std::unordered_map<const wxTopLevelWindow* /*don't access! use as key only!*/, Dimensions> initialDims_;
};
}

#endif //FOCUS_1084731021985757843

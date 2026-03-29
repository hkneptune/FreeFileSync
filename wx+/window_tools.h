// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FOCUS_1084731021985757843
#define FOCUS_1084731021985757843

#include <zen/scope_guard.h>
#include <wx/toplevel.h>
#include <wx/display.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/settings.h>
#include "color_tools.h"
#include "dc.h"
    #include <gtk/gtk.h>

namespace zen
{
//set portable font size in multiples of the operating system's default font size
void setRelativeFontSize(wxWindow& control, double factor);
void setMainInstructionFont(wxWindow& control); //following Windows/Gnome/OS X guidelines

void fixSpinCtrl(wxSpinCtrl& m_spinCtrl);

void setText(wxTextCtrl&   control, const wxString& newText, bool* additionalLayoutChange = nullptr);
void setText(wxStaticText& control, const wxString& newText, bool* additionalLayoutChange = nullptr);

void setTextWithUrls(wxRichTextCtrl& richCtrl, const wxString& newText);


bool isComponentOf(const wxWindow* child, const wxWindow* top);
wxWindow& getRootWindow(wxWindow& child);
wxTopLevelWindow* getTopLevelWindow(wxWindow* child);

struct FocusPreserver;

class WindowLayout;












//###################### implementation #####################
inline
void setRelativeFontSize(wxWindow& control, double factor)
{
    wxFont font = control.GetFont();
    font.SetPointSize(std::round(wxNORMAL_FONT->GetPointSize() * factor));
    control.SetFont(font);
}


inline
void setMainInstructionFont(wxWindow& control)
{
    wxFont font = control.GetFont();
    font.SetPointSize(std::round(wxNORMAL_FONT->GetPointSize() * 12.0 / 11));
    font.SetWeight(wxFONTWEIGHT_BOLD);

    control.SetFont(font);
}


inline
void fixSpinCtrl(wxSpinCtrl& spinCtrl)
{
    //select all text after changing selection; macOS: already the default!
    spinCtrl.Bind(wxEVT_SPINCTRL, [&spinCtrl](wxEvent& event)
    {
        //Windows bug: not every mouse scroll or key up/down generates a wxEVT_SPINCTRL!? e.g. the one after manual selecting, and overwriting text field
        spinCtrl.SetSelection(-1, -1); //select all
        event.Skip();
    });
    //-----------------------------------------------------------------------
    //
    //text field <-> spin button out-of-sync bugs:
    //text field editted manually => the spin button uses stale value during next mouse scrolling
    //=> no such issue when clicking +/- buttons
    spinCtrl.Bind(wxEVT_TEXT, [&spinCtrl](wxCommandEvent& event)
    {
        spinCtrl.SetValue(event.GetString()); //=> wxSpinCtrlGTKBase::SetValue()
        event.Skip();
    });


    //-----------------------------------------------------------------------
    //there's no way to set width using GTK's CSS! =>
    spinCtrl.InvalidateBestSize();
    ::gtk_entry_set_width_chars(GTK_ENTRY(spinCtrl.m_widget), 3);

#if 0 //apparently not needed!?
    if (::gtk_check_version(3, 12, 0) == NULL)
        ::gtk_entry_set_max_width_chars(GTK_ENTRY(spinCtrl.m_widget), 3);
#endif

    //get rid of excessive default width on old GTK3 3.14 (Debian);
    //gtk_entry_set_width_chars() not working => mitigate
    spinCtrl.SetMinSize({dipToWxsize(100), -1}); //must be wider than gtk_entry_set_width_chars(), or it breaks newer GTK e.g. 3.22!

#if 0 //generic property syntax:
    GValue bval = G_VALUE_INIT;
    ::g_value_init(&bval, G_TYPE_BOOLEAN);
    ::g_value_set_boolean(&bval, false);
    ZEN_ON_SCOPE_EXIT(::g_value_unset(&bval));
    ::g_object_set_property(G_OBJECT(spinCtrl.m_widget), "visibility", &bval);
#endif

}


inline
void setText(wxTextCtrl& control, const wxString& newText, bool* additionalLayoutChange)
{
    const wxString& label = control.GetValue(); //perf: don't call twice!
    if (additionalLayoutChange && !*additionalLayoutChange && control.IsShown()) //never revert from true to false!
        *additionalLayoutChange = label.length() != newText.length(); //avoid screen flicker: update layout only when necessary

    if (label != newText)
        control.ChangeValue(newText);
}


inline
void setText(wxStaticText& control, const wxString& newText, bool* additionalLayoutChange)
{
    //wxControl::EscapeMnemonics() (& -> &&) =>  wxControl::GetLabelText/SetLabelText
    //e.g. "filenames in the sync progress dialog": https://sourceforge.net/p/freefilesync/bugs/279/

    const wxString& label = control.GetLabelText(); //perf: don't call twice!
    if (additionalLayoutChange && !*additionalLayoutChange && control.IsShown()) //"better" or overkill(?): IsShownOnScreen()
        *additionalLayoutChange = label.length() != newText.length(); //avoid screen flicker: update layout only when necessary

    if (label != newText)
        control.SetLabelText(newText);
}


inline
void setTextWithUrls(wxRichTextCtrl& richCtrl, const wxString& newText)
{
    enum class BlockType
    {
        text,
        url,
    };
    std::vector<std::pair<BlockType, wxString>> blocks;

    for (auto it = newText.begin();;)
    {
        constexpr std::wstring_view urlPrefix = L"https://";
        const auto itUrl = std::search(it, newText.end(), urlPrefix.begin(), urlPrefix.end());
        if (it != itUrl)
            blocks.emplace_back(BlockType::text, wxString(it, itUrl));

        if (itUrl == newText.end())
            break;

        auto itUrlEnd = std::find_if(itUrl, newText.end(), [](wchar_t c) { return isWhiteSpace(c); });
        blocks.emplace_back(BlockType::url, wxString(itUrl, itUrlEnd));
        it = itUrlEnd;
    }
    richCtrl.BeginSuppressUndo();
    ZEN_ON_SCOPE_EXIT(richCtrl.EndSuppressUndo());

    //fix mouse scroll speed: why the FUCK is this even necessary!
    richCtrl.SetLineHeight(richCtrl.GetCharHeight());

    //get rid of margins and space between text blocks/"paragraphs"
    richCtrl.SetMargins({0, 0});
    richCtrl.BeginParagraphSpacing(0, 0);
    ZEN_ON_SCOPE_EXIT(richCtrl.EndParagraphSpacing());

    richCtrl.Clear();

    wxRichTextAttr urlStyle;
    urlStyle.SetTextColour(enhanceContrast(*wxBLUE, //primarily needed for dark mode!
                                           wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW), 5 /*contrastRatioMin*/)); //W3C recommends >= 4.5
    urlStyle.SetFontUnderlined(true);

    for (auto& [type, text] : blocks)
        switch (type)
        {
            case BlockType::text:
                if (endsWith(text, L"\n\n")) //bug: multiple newlines before a URL are condensed to only one;
                    //Why? fuck knows why! no such issue with double newlines *after* URL => hack this shit
                    text.RemoveLast().Append(ZERO_WIDTH_SPACE).Append(L'\n');

                richCtrl.WriteText(text);
                break;

            case BlockType::url:
            {
                richCtrl.BeginStyle(urlStyle);
                ZEN_ON_SCOPE_EXIT(richCtrl.EndStyle());
                richCtrl.BeginURL(text);
                ZEN_ON_SCOPE_EXIT(richCtrl.EndURL());
                richCtrl.WriteText(text);
            }
            break;
        }

    //register only once! => use a global function pointer, so that Unbind() works correctly:
    using LaunchUrlFun = void(*)(wxTextUrlEvent& event);
    static const LaunchUrlFun launchUrl = [](wxTextUrlEvent& event) { wxLaunchDefaultBrowser(event.GetString()); };

    [[maybe_unused]] const bool unbindOk1 = richCtrl.Unbind(wxEVT_TEXT_URL, launchUrl);
    if (std::any_of(blocks.begin(), blocks.end(), [](const auto& item) { return item.first == BlockType::url; }))
    /**/richCtrl.Bind(wxEVT_TEXT_URL, launchUrl);

    struct UserData : public wxObject
    {
        explicit UserData(wxRichTextCtrl& rtc) : richCtrl(rtc) {}
        wxRichTextCtrl& richCtrl;
    };
    using KeyEventsFun = void(*)(wxKeyEvent& event);
    static const KeyEventsFun onKeyEvents = [](wxKeyEvent& event)
    {
        wxRichTextCtrl& richCtrl2 = dynamic_cast<UserData*>(event.GetEventUserData())->richCtrl; //unclear if we can rely on event.GetEventObject() == richCtrl

        //CTRL/SHIFT + INS is broken for wxRichTextCtrl on Windows/Linux (apparently never was a thing on macOS)
        if (event.ControlDown())
            switch (event.GetKeyCode())
            {
                case WXK_INSERT:
                case WXK_NUMPAD_INSERT:
                    assert(richCtrl2.CanCopy()); //except when no selection
                    richCtrl2.Copy();
                    return;
            }

        if (event.ShiftDown())
            switch (event.GetKeyCode())
            {
                case WXK_INSERT:
                case WXK_NUMPAD_INSERT:
                    assert(richCtrl2.CanPaste()); //except wxTE_READONLY
                    richCtrl2.Paste();
                    return;
            }
        event.Skip();
    };
    [[maybe_unused]] const bool unbindOk2 = richCtrl.Unbind(wxEVT_KEY_DOWN, onKeyEvents);
    /**/                                    richCtrl.  Bind(wxEVT_KEY_DOWN, onKeyEvents, wxID_ANY, wxID_ANY, new UserData(richCtrl) /*pass ownership*/);
}


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
            {
                assert(oldFocusWin->IsEnabled()); //only enabled windows can have focus, so why wouldn't it be anymore?
                setFocusIfActive(*oldFocusWin);
            }
    }

    wxWindowID getFocusId() const { return oldFocusId_; }

    void setFocus(wxWindow* win)
    {
        oldFocusId_ = win->GetId();
        assert(oldFocusId_ != wxID_ANY);
    }

    void dismiss() { oldFocusId_ = wxID_ANY; }

private:
    wxWindowID oldFocusId_ = wxID_ANY;
    //don't store wxWindow* which may be dangling during ~FocusPreserver()!
    //test: click on delete folder pair and immediately press F5 => focus window (= FP del button) is defer-deleted during sync
};


class WindowLayout
{
public:
    struct Rect
    {
        wxSize size; //optional! default wxSize(0, 0) = "no size"
        std::optional<wxPoint> pos;
        bool isMaximized = false;
    };

    static void setInitial(wxTopLevelWindow& topWin, const Rect& rect, wxSize defaultSize)
    {
        initialRects_[&topWin] = rect;

        wxSize newSize = defaultSize;
        std::optional<wxPoint> newPos;
        //set dialog size and position:
        // - multi-monitor setup: dialog may be placed on second monitor which is currently turned off
        if (rect.size.GetWidth () > 0 &&
            rect.size.GetHeight() > 0)
        {
            newSize = rect.size;

            if (rect.pos)
            {
                const wxPoint dlgCenter = *rect.pos + rect.size / 2;

                wxRect rectAllDisp;

                const int displayCount = wxDisplay::GetCount();
                for (int i = 0; i < displayCount; ++i)
                {
                    const wxRect rectDisp = wxDisplay(i).GetClientArea();

                    rectAllDisp = getBoundingBox(rectAllDisp, rectDisp);

                    //if dialog center is not part of any display, user probably doesn't want to restore to this location anyway
                    if (rectDisp.Contains(dlgCenter) &&
                        //don't allow position that would hide the title bar: Windows/macOS do not correct invalid y-positions
                        rect.pos->y >= rectDisp.y - dipToWxsize(5) /*some leeway needed?*/)
                        newPos = rect.pos;
                }
                if (newPos)
                    newSize = getIntersection(newSize, rectAllDisp.GetSize());
            }
        }

        if (newPos)
            topWin.SetSize(wxRect(*newPos, newSize), wxSIZE_ALLOW_MINUS_ONE);
        else
        {
            //if dialog height > display height, wxWindow::Center() aligns the dialog to *bottom* of the screen => title bar hidden!!!
            newSize = getIntersection(newSize, wxDisplay(&topWin).GetClientArea().GetSize());

            topWin.SetSize(newSize);
            topWin.Center();
        }

        if (rect.isMaximized) //no real need to support both maximize and full screen functions
            topWin.Maximize(true);

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

    //window-modifying: changes window size!
    static Rect getBeforeClose(wxTopLevelWindow& topWin)
    {
        Rect rect;
        bool validSizePos = true;
        const wxSize sizeOrig = topWin.GetSize();

        if (topWin.IsMaximized())
        {
            topWin.Maximize(false);
            rect.isMaximized = true;
        }

        rect.size = topWin.GetSize();
        rect.pos  = topWin.GetPosition();

        if (rect.isMaximized && rect.size == sizeOrig) //wxGTK3: still returns full screen size due to deferred sizing!?
            validSizePos = false;

        if (!validSizePos) //reuse previous values:
        {
            const auto it = initialRects_.find(&topWin);
            if (it != initialRects_.end())
            {
                rect.size = it->second.size;
                rect.pos  = it->second.pos;
            }
            else
            {
                assert(false);
                rect.size = wxSize();
                rect.pos = std::nullopt;
            }
        }

        return rect;

#if 0 //wxWidgets alternative: apparently no benefits (not even on Wayland! but strange decisions: why restore the minimized state!???)
        struct : wxTopLevelWindow::GeometrySerializer
        {
            bool SaveField(const wxString& name, int value) const /*NO, this must not be const!*/ override
            {
                layout_ += utfTo<std::string>(name) + '=' + numberTo<std::string>(value) + ' ';
                return true;
            }

            bool RestoreField(const wxString& name, int* value) /*const: yes, this MAY(!) be const*/ override { return false; }

            mutable //to wxWidgets people: 1. learn when and when not to use const for input/output functions! see SaveField/RestoreField()
            //                             2. learn flexible software design: why are input/output tied up in a single GeometrySerializer implementation?
            std::string layout_;
        } serializer;

        if (topWin.SaveGeometry(serializer)) //apparently no-fail as long as GeometrySerializer::SaveField is!
            return serializer.layout_;
        else
            assert(false);
#endif
    }

private:
    inline static std::unordered_map<const wxTopLevelWindow* /*don't access! use as key only!*/, Rect> initialRects_;
};
}

#endif //FOCUS_1084731021985757843

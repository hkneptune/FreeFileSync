// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef NO_FLICKER_H_893421590321532
#define NO_FLICKER_H_893421590321532

#include <zen/string_tools.h>
#include <zen/scope_guard.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/wupdlock.h>


namespace zen
{
namespace
{
void setText(wxTextCtrl& control, const wxString& newText, bool* additionalLayoutChange = nullptr)
{
    const wxString& label = control.GetValue(); //perf: don't call twice!
    if (additionalLayoutChange && !*additionalLayoutChange && control.IsShown()) //never revert from true to false!
        *additionalLayoutChange = label.length() != newText.length(); //avoid screen flicker: update layout only when necessary

    if (label != newText)
        control.ChangeValue(newText);
}


void setText(wxStaticText& control, const wxString& newText, bool* additionalLayoutChange = nullptr)
{
    //wxControl::EscapeMnemonics() (& -> &&) =>  wxControl::GetLabelText/SetLabelText
    //e.g. "filenames in the sync progress dialog": https://sourceforge.net/p/freefilesync/bugs/279/

    const wxString& label = control.GetLabelText(); //perf: don't call twice!
    if (additionalLayoutChange && !*additionalLayoutChange && control.IsShown()) //"better" or overkill(?): IsShownOnScreen()
        *additionalLayoutChange = label.length() != newText.length(); //avoid screen flicker: update layout only when necessary

    if (label != newText)
        control.SetLabelText(newText);
}


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
    urlStyle.SetTextColour(*wxBLUE);
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
}
}

#endif //NO_FLICKER_H_893421590321532

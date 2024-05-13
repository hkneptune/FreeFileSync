// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef CONTEXT_MENU_H_18047302153418174632141234
#define CONTEXT_MENU_H_18047302153418174632141234

//#include <map>
#include <vector>
#include <functional>
#include <wx/app.h>
#include <wx/clipbrd.h>
#include <wx/menu.h>
#include "dc.h"


/*  A context menu supporting lambda callbacks!

    Usage:
        ContextMenu menu;
        menu.addItem(L"Some Label", [&]{ ...do something... }); -> capture by reference is fine, as long as captured variables have at least scope of ContextMenu::popup()!
        ...
        menu.popup(wnd);                     */

namespace zen
{
inline
void setImage(wxMenuItem& menuItem, const wxImage& img)
{
    menuItem.SetBitmap(toScaledBitmap(img));
}


class ContextMenu : private wxEvtHandler
{
public:
    ContextMenu() {}

    void addItem(const wxString& label, const std::function<void()>& command, const wxImage& img = wxNullImage, bool enabled = true)
    {
        wxMenuItem* newItem = new wxMenuItem(menu_.get(), wxID_ANY, label); //menu owns item!
        if (img.IsOk())
            setImage(*newItem, img); //do not set AFTER appending item! wxWidgets screws up for yet another crappy reason
        menu_->Append(newItem);
        if (!enabled)
            newItem->Enable(false); //do not enable BEFORE appending item! wxWidgets screws up for yet another crappy reason
        commandList_[newItem->GetId()] = command; //defer event connection, this may be a submenu only!
    }

    void addCheckBox(const wxString& label, const std::function<void()>& command, bool checked, bool enabled = true)
    {
        wxMenuItem* newItem = menu_->AppendCheckItem(wxID_ANY, label);
        newItem->Check(checked);
        if (!enabled)
            newItem->Enable(false);
        commandList_[newItem->GetId()] = command;
    }

    void addRadio(const wxString& label, const std::function<void()>& command, bool selected, bool enabled = true)
    {
        wxMenuItem* newItem = menu_->AppendRadioItem(wxID_ANY, label);
        newItem->Check(selected);
        if (!enabled)
            newItem->Enable(false);
        commandList_[newItem->GetId()] = command;
    }

    void addSeparator() { menu_->AppendSeparator(); }

    void addSubmenu(const wxString& label, ContextMenu& submenu, const wxImage& img = wxNullImage, bool enabled = true) //invalidates submenu!
    {
        //transfer submenu commands:
        commandList_.insert(submenu.commandList_.begin(), submenu.commandList_.end());
        submenu.commandList_.clear();

        submenu.menu_->SetNextHandler(menu_.get()); //on wxGTK submenu events are not propagated to their parent menu by default!

        wxMenuItem* newItem = new wxMenuItem(menu_.get(), wxID_ANY, label, L"", wxITEM_NORMAL, submenu.menu_.release()); //menu owns item, item owns submenu!
        if (img.IsOk())
            setImage(*newItem, img); //do not set AFTER appending item! wxWidgets screws up for yet another crappy reason
        menu_->Append(newItem);
        if (!enabled)
            newItem->Enable(false);
    }

    void popup(wxWindow& wnd, const wxPoint& pos = wxDefaultPosition) //show popup menu + process lambdas
    {
        //eventually all events from submenu items will be received by this menu
        for (const auto& [itemId, command] : commandList_)
            menu_->Bind(wxEVT_COMMAND_MENU_SELECTED, [command /*clang bug*/= command](wxCommandEvent& event) { command(); }, itemId);

        wnd.PopupMenu(menu_.get(), pos);
        wxTheApp->ProcessPendingEvents(); //make sure lambdas are evaluated before going out of scope;
        //although all events seem to be processed within wxWindows::PopupMenu, we shouldn't trust wxWidgets in this regard
    }

private:
    ContextMenu           (const ContextMenu&) = delete;
    ContextMenu& operator=(const ContextMenu&) = delete;

    std::unique_ptr<wxMenu> menu_ = std::make_unique<wxMenu>();
    std::unordered_map<int /*item id*/, std::function<void()> /*command*/> commandList_;
};


//GTK: image must be set *before* adding wxMenuItem to menu or it won't show => workaround:
inline //also needed on Windows + macOS since wxWidgets 3.1.6 (thanks?)
void fixMenuIcons(wxMenu& menu)
{
    std::vector<std::pair<wxMenuItem*, size_t /*pos*/>> itemsWithBmp;
    {
        size_t pos = 0;
        for (wxMenuItem* item : menu.GetMenuItems())
        {
            if (item->GetBitmap().IsOk())
                itemsWithBmp.emplace_back(item, pos);
            ++pos;
        }
    }

    for (const auto& [item, pos] : itemsWithBmp)
        if (!menu.Insert(pos, menu.Remove(item))) //detach + reinsert
            assert(false);
}


//better call wxClipboard::Get()->Flush() *once* during app exit instead of after each setClipboardText()?
//  => OleFlushClipboard: "Carries out the clipboard shutdown sequence"
//  => maybe this helps with clipboard randomly "forgetting" content after app exit?
inline
void setClipboardText(const wxString& txt)
{
    wxClipboard& clip = *wxClipboard::Get();
    if (clip.Open())
    {
        ZEN_ON_SCOPE_EXIT(clip.Close());
        [[maybe_unused]] const bool rv = clip.SetData(new wxTextDataObject(txt)); //ownership passed
        assert(rv);
    }
    else assert(false);
}


inline
std::optional<wxString> getClipboardText()
{
    wxClipboard& clip = *wxClipboard::Get();
    if (clip.Open())
    {
        ZEN_ON_SCOPE_EXIT(clip.Close());

        //if (clip.IsSupported(wxDF_TEXT or wxDF_UNICODETEXT !???)) - superfluous? already handled by wxClipboard::GetData()!?
        wxTextDataObject data;
        if (clip.GetData(data))
            return data.GetText();
    }
    else assert(false);
    return std::nullopt;
}
}

#endif //CONTEXT_MENU_H_18047302153418174632141234

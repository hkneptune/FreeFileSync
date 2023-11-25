// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "folder_history_box.h"
#include <wx+/dc.h>
    #include <gtk/gtk.h>
#include "../afs/concrete.h"

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


FolderHistoryBox::FolderHistoryBox(wxWindow* parent,
                                   wxWindowID id,
                                   const wxString& value,
                                   const wxPoint& pos,
                                   const wxSize& size,
                                   int n,
                                   const wxString choices[],
                                   long style,
                                   const wxValidator& validator,
                                   const wxString& name) :
    wxComboBox(parent, id, value, pos, size, n, choices, style, validator, name)
{
    //#####################################
    /*##*/ SetMinSize({dipToWxsize(150), -1}); //## workaround yet another wxWidgets bug: default minimum size is much too large for a wxComboBox
    //#####################################

    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) { onKeyEvent(event); });

    /*
    we can't attach to wxEVT_COMMAND_TEXT_UPDATED, since setValueAndUpdateList() will implicitly emit wxEVT_COMMAND_TEXT_UPDATED again when calling Clear()!
    => Crash on Suse/X11/wxWidgets 2.9.4 on startup (setting a flag to guard against recursion does not work, still crash)

    On OS X attaching to wxEVT_LEFT_DOWN leads to occasional crashes, especially when double-clicking
    */

    //file drag and drop directly into the text control unhelpfully inserts in format "file://..<cr><nl>"
    //1. this format's implementation is a mess: http://www.lephpfacile.com/manuel-php-gtk/tutorials.filednd.urilist.php
    //2. even if we handle "drag-data-received" for "text/uri-list" this doesn't consider logic in dirname.cpp
    //=> disable all drop events on the text control (disables text drop, too, but not a big loss)
    //=> all drops are nicely propagated as regular file drop events like they should have been in the first place!
    if (GtkWidget* widget = GetConnectWidget())
        ::gtk_drag_dest_unset(widget);
}


void FolderHistoryBox::onRequireHistoryUpdate(wxEvent& event)
{
    setValueAndUpdateList(GetValue());
    event.Skip();
}


//set value and update list are technically entangled: see potential bug description below
void FolderHistoryBox::setValueAndUpdateList(const wxString& folderPathPhrase)
{
    //populate selection list....
    std::vector<wxString> items;
    {
        auto trimTrailingSep = [](Zstring path)
        {
            if (endsWith(path, Zstr('/')) ||
                endsWith(path, Zstr('\\')))
                path.pop_back();
            return path;
        };

        const Zstring& folderPathPhraseTrimmed = trimTrailingSep(trimCpy(utfTo<Zstring>(folderPathPhrase)));

        //path phrase aliases: allow user changing to volume name and back
        for (const Zstring& aliasPhrase : AFS::getPathPhraseAliases(createAbstractPath(utfTo<Zstring>(folderPathPhrase)))) //may block when resolving [<volume name>]
            if (!equalNoCase(folderPathPhraseTrimmed,
                             trimTrailingSep(aliasPhrase))) //don't add redundant aliases
                items.push_back(utfTo<wxString>(aliasPhrase));
    }

    if (sharedHistory_.get())
    {
        std::vector<Zstring> tmp = sharedHistory_->getList();
        std::sort(tmp.begin(), tmp.end(), LessNaturalSort() /*even on Linux*/);

        if (!items.empty() && !tmp.empty())
            items.push_back(HistoryList::separationLine());

        for (const Zstring& str : tmp)
            items.push_back(utfTo<wxString>(str));
    }

    //###########################################################################################

    //attention: if the target value is not part of the dropdown list, SetValue() will look for a string that *starts with* this value:
    //e.g. if the dropdown list contains "222" SetValue("22") will erroneously set and select "222" instead, while "111" would be set correctly!
    // -> by design on Windows!
    if (std::find(items.begin(), items.end(), folderPathPhrase) == items.end())
        items.insert(items.begin(), folderPathPhrase);

    //this->Clear(); -> NO! emits yet another wxEVT_COMMAND_TEXT_UPDATED!!!
    wxItemContainer::Clear(); //suffices to clear the selection items only!
    this->Append(items); //expensive as fuck! => only call when absolutely needed!

    //this->SetSelection(wxNOT_FOUND); //don't select anything
    ChangeValue(folderPathPhrase); //preserve main text!
}


void FolderHistoryBox::onKeyEvent(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();

    if (keyCode == WXK_DELETE ||
        keyCode == WXK_NUMPAD_DELETE)
        //try to delete the currently selected config history item
        if (const int pos = this->GetCurrentSelection();
            0 <= pos && pos < static_cast<int>(this->GetCount()) &&
            //what a mess...:
            (GetValue() != GetString(pos) || //avoid problems when a character shall be deleted instead of list item
             GetValue().empty())) //exception: always allow removing empty entry
        {
            //save old (selected) value: deletion seems to have influence on this
            const wxString currentVal = this->GetValue();
            //this->SetSelection(wxNOT_FOUND);

            //delete selected row
            if (sharedHistory_.get())
                sharedHistory_->delItem(utfTo<Zstring>(GetString(pos)));
            SetString(pos, wxString()); //in contrast to "Delete(pos)", this one does not kill the drop-down list and gives a nice visual feedback!

            this->SetValue(currentVal);
            return; //eat up key event
        }


    event.Skip();
}

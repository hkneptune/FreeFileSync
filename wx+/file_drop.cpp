// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "file_drop.h"
#include <wx/dnd.h>
#include <zen/utf.h>
#include <zen/file_access.h>


using namespace zen;


namespace zen
{
wxDEFINE_EVENT(EVENT_DROP_FILE, FileDropEvent);
}




namespace
{
class WindowDropTarget : public wxFileDropTarget
{
public:
    explicit WindowDropTarget(const wxWindow& dropWindow) : dropWindow_(dropWindow) {}

private:
    wxDragResult OnDragOver(wxCoord x, wxCoord y, wxDragResult def) override
    {
        //why the FUCK I is drag & drop still working while showing another modal dialog!???
        //why the FUCK II is drag & drop working even when dropWindow is disabled!??              [Windows] => we can fix this
        //why the FUCK III is dropWindow NOT disabled while showing another modal dialog!??? [macOS, Linux] => we CANNOT fix this: FUUUUUUUUUUUUUU...
        if (!dropWindow_.IsEnabled())
            return wxDragNone;

        return wxFileDropTarget::OnDragOver(x, y, def);
    }

    //"bool wxDropTarget::GetData() [...] This method may only be called from within OnData()."
    //=> FUUUUUUUUUUUUUU........ a.k.a. no support for DragDropValidator during mouse hover! >:(

    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& fileArray) override
    {
        /*Linux, MTP: we get an empty file array
            => switching to wxTextDropTarget won't help (much): we'd get the format
                mtp://[usb:001,002]/Telefonspeicher/Folder/file.txt
            instead of
                /run/user/1000/gvfs/mtp:host=%5Busb%3A001%2C002%5D/Telefonspeicher/Folder/file.txt                  */

        if (!dropWindow_.IsEnabled())
            return false;

        //wxPoint clientDropPos(x, y)
        std::vector<Zstring> filePaths;
        for (const wxString& file : fileArray)
            filePaths.push_back(utfTo<Zstring>(file));

        //create a custom event on drop window: execute event after file dropping is completed! (after mouse is released)
        dropWindow_.GetEventHandler()->AddPendingEvent(FileDropEvent(filePaths));
        return true;
    }

    const wxWindow& dropWindow_;
};
}


void zen::setupFileDrop(wxWindow& dropWindow)
{
    dropWindow.SetDropTarget(new WindowDropTarget(dropWindow)); /*takes ownership*/
}

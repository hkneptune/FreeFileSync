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


const wxEventType zen::EVENT_DROP_FILE = wxNewEventType();




namespace
{
class WindowDropTarget : public wxFileDropTarget
{
public:
    WindowDropTarget(wxWindow& dropWindow) : dropWindow_(dropWindow) {}

private:
    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& fileArray) override
    {
        /*Linux, MTP: we get an empty file array
            => switching to wxTextDropTarget won't help (much): we'd get the format
                mtp://[usb:001,002]/Telefonspeicher/Folder/file.txt
            instead of
                /run/user/1000/gvfs/mtp:host=%5Busb%3A001%2C002%5D/Telefonspeicher/Folder/file.txt
        */

        //wxPoint clientDropPos(x, y)
        std::vector<Zstring> filePaths;
        for (const wxString& file : fileArray)
            filePaths.push_back(utfTo<Zstring>(file));

        //create a custom event on drop window: execute event after file dropping is completed! (after mouse is released)
        if (wxEvtHandler* handler = dropWindow_.GetEventHandler())
            handler->AddPendingEvent(FileDropEvent(filePaths));
        return true;
    }

    wxWindow& dropWindow_;
};
}


void zen::setupFileDrop(wxWindow& wnd)
{
    wnd.SetDropTarget(new WindowDropTarget(wnd)); /*takes ownership*/
}

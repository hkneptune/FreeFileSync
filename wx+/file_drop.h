// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_DROP_H_09457802957842560325626
#define FILE_DROP_H_09457802957842560325626

#include <vector>
#include <functional>
#include <zen/zstring.h>
#include <wx/window.h>
#include <wx/event.h>


namespace zen
{
//register simple file drop event (without issue of freezing dialogs and without wxFileDropTarget overdesign)
//CAVEAT: a drop target window must not be directly or indirectly contained within a wxStaticBoxSizer until the following wxGTK bug
//is fixed. According to wxWidgets release cycles this is expected to be: never http://trac.wxwidgets.org/ticket/2763

/*
1. setup a window to emit EVENT_DROP_FILE:
    - simple file system paths:        setupFileDrop
    - any shell paths with validation: setupShellItemDrop

2. register events:
wnd.Connect   (EVENT_DROP_FILE, FileDropEventHandler(MyDlg::OnFilesDropped), nullptr, this);
wnd.Disconnect(EVENT_DROP_FILE, FileDropEventHandler(MyDlg::OnFilesDropped), nullptr, this);

3. do something:
void MyDlg::OnFilesDropped(FileDropEvent& event);
*/

extern const wxEventType EVENT_DROP_FILE;


class FileDropEvent : public wxCommandEvent
{
public:
    FileDropEvent(const std::vector<Zstring>& droppedPaths) : wxCommandEvent(EVENT_DROP_FILE), droppedPaths_(droppedPaths) { StopPropagation(); }

    const std::vector<Zstring>& getPaths() const { return droppedPaths_; }

private:
    wxEvent* Clone() const override { return new FileDropEvent(*this); }

    const std::vector<Zstring> droppedPaths_;
};


using FileDropEventFunction = void (wxEvtHandler::*)(FileDropEvent&);

#define FileDropEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(FileDropEventFunction, &func)





void setupFileDrop(wxWindow& wnd);
}

#endif //FILE_DROP_H_09457802957842560325626

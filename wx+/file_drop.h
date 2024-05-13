// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_DROP_H_09457802957842560325626
#define FILE_DROP_H_09457802957842560325626

#include <vector>
//#include <functional>
#include <zen/zstring.h>
#include <wx/window.h>
#include <wx/event.h>


namespace zen
{
/*  register simple file drop event (without issue of freezing dialogs and without wxFileDropTarget overdesign)
    CAVEAT: a drop target window must not be directly or indirectly contained within a wxStaticBoxSizer until the following wxGTK bug
    is fixed. According to wxWidgets release cycles this is expected to be: never https://github.com/wxWidgets/wxWidgets/issues/2763

    1. setup a window to emit EVENT_DROP_FILE:
        - simple file system paths:        setupFileDrop
        - any shell paths with validation: setupShellItemDrop

    2. register events:
        wnd.Bind(EVENT_DROP_FILE, [this](FileDropEvent& event) { onFilesDropped(event); });           */
struct FileDropEvent;
wxDECLARE_EVENT(EVENT_DROP_FILE, FileDropEvent);


struct FileDropEvent : public wxEvent
{
    explicit FileDropEvent(const std::vector<Zstring>& droppedPaths) : wxEvent(0 /*winid*/, EVENT_DROP_FILE), itemPaths_(droppedPaths) {}
    FileDropEvent* Clone() const override { return new FileDropEvent(*this); }

    const std::vector<Zstring> itemPaths_;
};



void setupFileDrop(wxWindow& dropWindow);
}

#endif //FILE_DROP_H_09457802957842560325626

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef CUSTOM_GRID_H_8405817408327894
#define CUSTOM_GRID_H_8405817408327894

#include <wx+/grid.h>
#include "file_view.h"
#include "../icon_buffer.h"


namespace fff
{
//setup grid to show grid view within three components:
namespace filegrid
{
void init(zen::Grid& gridLeft, zen::Grid& gridCenter, zen::Grid& gridRight);
FileView& getDataView(zen::Grid& grid);

void setData(zen::Grid& grid, FolderComparison& folderCmp); //takes (shared) ownership

void setViewType(zen::Grid& gridCenter, GridViewType vt);

void setupIcons(zen::Grid& gridLeft, zen::Grid& gridCenter, zen::Grid& gridRight, bool showFileIcons, IconBuffer::IconSize sz);

void setItemPathForm(zen::Grid& grid, ItemPathFormat fmt); //only for left/right grid

void refresh(zen::Grid& gridLeft, zen::Grid& gridCenter, zen::Grid& gridRight);

void setScrollMaster(zen::Grid& grid);

//mark rows selected in overview panel and navigate to leading object
void setNavigationMarker(zen::Grid& gridLeft, zen::Grid& gridRight,
                         std::unordered_set<const FileSystemObject*>&& markedFilesAndLinks,//mark files/symlinks directly within a container
                         std::unordered_set<const ContainerObject*>&& markedContainer);    //mark full container including child-objects
}

wxImage getSyncOpImage(SyncOperation syncOp);
wxImage getCmpResultImage(CompareFileResult cmpResult);


//grid hover area for file group rendering
enum class HoverAreaGroup
{
    groupName,
    item
};

//---------- custom events for middle grid ----------
struct CheckRowsEvent;
struct SyncDirectionEvent;
wxDECLARE_EVENT(EVENT_GRID_CHECK_ROWS,     CheckRowsEvent);
wxDECLARE_EVENT(EVENT_GRID_SYNC_DIRECTION, SyncDirectionEvent);


struct CheckRowsEvent : public wxEvent
{
    CheckRowsEvent(size_t rowFirst, size_t rowLast, bool setIncluded) : wxEvent(0 /*winid*/, EVENT_GRID_CHECK_ROWS), rowFirst_(rowFirst), rowLast_(rowLast), setActive_(setIncluded) { assert(rowFirst <= rowLast); }
    CheckRowsEvent* Clone() const override { return new CheckRowsEvent(*this); }

    const size_t rowFirst_; //selected range: [rowFirst_, rowLast_)
    const size_t rowLast_;  //range is empty when clearing selection
    const bool setActive_;
};


struct SyncDirectionEvent : public wxEvent
{
    SyncDirectionEvent(size_t rowFirst, size_t rowLast, SyncDirection direction) : wxEvent(0 /*winid*/, EVENT_GRID_SYNC_DIRECTION), rowFirst_(rowFirst), rowLast_(rowLast), direction_(direction) { assert(rowFirst <= rowLast); }
    SyncDirectionEvent* Clone() const override { return new SyncDirectionEvent(*this); }

    const size_t rowFirst_; //see CheckRowsEvent
    const size_t rowLast_;  //
    const SyncDirection direction_;
};
}

#endif //CUSTOM_GRID_H_8405817408327894

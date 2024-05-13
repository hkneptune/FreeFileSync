// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef GRID_VIEW_H_9285028345703475842569
#define GRID_VIEW_H_9285028345703475842569

#include <vector>
#include <variant>
#include <unordered_map>
#include <zen/stl_tools.h>
#include "file_grid_attr.h"
#include "../base/file_hierarchy.h"


namespace fff
{
class FileView //grid view of FolderComparison
{
public:
    FileView() {}
    explicit FileView(FolderComparison& folderCmp); //takes weak (non-owning) references

    size_t rowsOnView() const { return viewRef_  .size(); } //only visible elements
    size_t rowsTotal () const { return sortedRef_.size(); } //total rows available

    //returns nullptr if object is not found; complexity: constant!
    const FileSystemObject* getFsObject(size_t row) const { return row < viewRef_.size() ? FileSystemObject::retrieve(viewRef_[row].objId) : nullptr; }
    /**/  FileSystemObject* getFsObject(size_t row)       { return const_cast<FileSystemObject*>(static_cast<const FileView&>(*this).getFsObject(row)); } //see Meyers Effective C++

    //references to FileSystemObject: no nullptr-check needed! everything is bound
    std::vector<FileSystemObject*> getAllFileRef(const std::vector<size_t>& rows);

    struct PathDrawInfo
    {
        size_t groupFirstRow = 0; //half-open range
        size_t groupLastRow  = 0; //
        const size_t groupIdx = 0;
        uint64_t viewUpdateId = 0; //help detect invalid buffers after updateView()
        FolderPair* folderGroupObj = nullptr; //nullptr if group is BaseFolderPair (or fsObj not found)
        FileSystemObject* fsObj    = nullptr; //nullptr if object is not found
    };
    PathDrawInfo getDrawInfo(size_t row); //complexity: constant!

    struct FileStats
    {
        int fileCount   = 0;
        int folderCount = 0;
        uint64_t bytes  = 0;
    };

    struct DifferenceViewStats
    {
        int excluded = 0;
        int equal    = 0;
        int conflict = 0;

        int leftOnly   = 0;
        int rightOnly  = 0;
        int leftNewer  = 0;
        int rightNewer = 0;
        int different  = 0;

        FileStats fileStatsLeft;
        FileStats fileStatsRight;
    };
    DifferenceViewStats applyDifferenceFilter(bool showExcluded,
                                              bool showLeftOnly,
                                              bool showRightOnly,
                                              bool showLeftNewer,
                                              bool showRightNewer,
                                              bool showDifferent,
                                              bool showEqual,
                                              bool showConflict);

    struct ActionViewStats
    {
        int excluded = 0;
        int equal    = 0;
        int conflict = 0;

        int createLeft  = 0;
        int createRight = 0;
        int deleteLeft  = 0;
        int deleteRight = 0;
        int updateLeft  = 0;
        int updateRight = 0;
        int updateNone  = 0;

        FileStats fileStatsLeft;
        FileStats fileStatsRight;
    };
    ActionViewStats applyActionFilter(bool showExcluded,
                                      bool showCreateLeft,
                                      bool showCreateRight,
                                      bool showDeleteLeft,
                                      bool showDeleteRight,
                                      bool showUpdateLeft,
                                      bool showUpdateRight,
                                      bool showDoNothing,
                                      bool showEqual,
                                      bool showConflict);

    void removeInvalidRows(); //remove references to rows that have been deleted meanwhile: call after manual deletion and synchronization!

    //sorting...
    void sortView(ColumnTypeRim type, ItemPathFormat pathFmt, bool onLeft, bool ascending); //always call these; never sort externally!
    void sortView(ColumnTypeCenter type, bool ascending);                                   //

    struct SortInfo
    {
        std::variant<ColumnTypeRim, ColumnTypeCenter> sortCol;
        bool onLeft    = false; //only use if sortCol is ColumnTypeRim
        bool ascending = false;
    };
    const SortInfo* getSortConfig() const { return zen::get(currentSort_); } //return nullptr if currently not sorted

    ptrdiff_t findRowDirect(FileSystemObject::ObjectIdConst objId) const; //find an object's row position on view list directly, return < 0 if not found
    ptrdiff_t findRowFirstChild(const ContainerObject* conObj)    const; //find first child of FolderPair or BaseFolderPair *on sorted sub view*
    //"conObj" may be invalid, it is NOT dereferenced, return < 0 if not found

    //count non-empty pairs to distinguish single/multiple folder pair cases
    size_t getEffectiveFolderPairCount() const;

private:
    FileView           (const FileView&) = delete;
    FileView& operator=(const FileView&) = delete;

    template <class Predicate> void updateView(Predicate pred);


    std::unordered_map<FileSystemObject::ObjectIdConst, size_t> rowPositions_; //find row positions on viewRef_ directly
    std::unordered_map<const void* /*ContainerObject*/, size_t> rowPositionsFirstChild_; //find first child on sortedRef of a hierarchy object
    //void* instead of ContainerObject*: these are weak pointers and should *never be dereferenced*!

    struct GroupDetail
    {
        size_t groupFirstRow = 0;
    };
    std::vector<GroupDetail> groupDetails_;

    uint64_t viewUpdateId_ = 0; //help clients detect invalid buffers after updateView()

    struct ViewRow
    {
        FileSystemObject::ObjectId objId = nullptr;
        size_t groupIdx = 0; //...into groupDetails_
    };
    std::vector<ViewRow> viewRef_; //partial view on sortedRef_
    /*             /|\
                    | (applyFilterBy...)      */
    std::vector<FileSystemObject::ObjectId> sortedRef_; //flat view of weak pointers on folderCmp; may be sorted
    /*             /|\
                    | (constructor)
           FolderComparison folderCmp         */

    std::vector<std::tuple<const void* /*BaseFolderPair*/, AbstractPath, AbstractPath>> folderPairs_;

    std::optional<SortInfo> currentSort_;
};
}

#endif //GRID_VIEW_H_9285028345703475842569

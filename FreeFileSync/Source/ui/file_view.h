// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef GRID_VIEW_H_9285028345703475842569
#define GRID_VIEW_H_9285028345703475842569

#include <span>
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

    size_t rowsOnView() const { return viewRef_  .size(); } //only visible elements
    size_t rowsTotal () const { return sortedRef_.size(); } //total rows available

    //direct data access via row number
    const FileSystemObject* getFsObject(size_t row) const; //returns nullptr if object is not found; complexity: constant!
    /**/  FileSystemObject* getFsObject(size_t row);       //

    struct PathDrawInfo
    {
        enum
        {
            CONNECT_PREV   = 0x1,
            CONNECT_NEXT   = 0x2,
            DRAW_COMPONENT = 0x4,
        };
        std::span<const unsigned char> pathDrawInfo; //... of path components (including base folder which counts as *single* component)

        const FileSystemObject* fsObj; //nullptr if object is not found
    };
    PathDrawInfo getDrawInfo(size_t row) const; //complexity: constant!

    //get references to FileSystemObject: no nullptr-check needed! Everything's bound.
    std::vector<FileSystemObject*> getAllFileRef(const std::vector<size_t>& rows);

    struct FileStats
    {
        int fileCount = 0;
        int folderCount = 0;
        uint64_t bytes = 0;
    };

    struct CategoryViewStats
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
    CategoryViewStats applyFilterByCategory(bool showExcluded,
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
    ActionViewStats applyFilterByAction(bool showExcluded,
                                        bool showCreateLeft,
                                        bool showCreateRight,
                                        bool showDeleteLeft,
                                        bool showDeleteRight,
                                        bool showUpdateLeft,
                                        bool showUpdateRight,
                                        bool showDoNothing,
                                        bool showEqual,
                                        bool showConflict);

    void setData(FolderComparison& newData);
    void removeInvalidRows(); //remove references to rows that have been deleted meanwhile: call after manual deletion and synchronization!

    //sorting...
    void sortView(ColumnTypeRim type, ItemPathFormat pathFmt, bool onLeft, bool ascending); //always call these; never sort externally!
    void sortView(ColumnTypeCenter type, bool ascending);                                   //

    struct SortInfo
    {
        std::variant<ColumnTypeRim, ColumnTypeCenter> sortCol;
        bool onLeft    = false; //if sortCol is ColumnTypeRim
        bool ascending = false;
    };
    const SortInfo* getSortInfo() const { return zen::get(currentSort_); } //return nullptr if currently not sorted

    ptrdiff_t findRowDirect(FileSystemObject::ObjectIdConst objId) const; // find an object's row position on view list directly, return < 0 if not found
    ptrdiff_t findRowFirstChild(const ContainerObject* hierObj)    const; // find first child of FolderPair or BaseFolderPair *on sorted sub view*
    //"hierObj" may be invalid, it is NOT dereferenced, return < 0 if not found

    //count non-empty pairs to distinguish single/multiple folder pair cases
    size_t getEffectiveFolderPairCount() const;

    //buffer expensive wxDC::GetTextExtent() calls!
    //=> shared between GridDataLeft/GridDataRight
    std::unordered_map<std::wstring, wxSize>& refCompExtentsBuf() { return compExtentsBuf_; }

private:
    FileView           (const FileView&) = delete;
    FileView& operator=(const FileView&) = delete;

    template <class Predicate> void updateView(Predicate pred);


    std::unordered_map<FileSystemObject::ObjectIdConst, size_t> rowPositions_; //find row positions on viewRef_ directly
    std::unordered_map<const void* /*ContainerObject*/, size_t> rowPositionsFirstChild_; //find first child on sortedRef of a hierarchy object
    //void* instead of ContainerObject*: these are weak pointers and should *never be dereferenced*!

    struct ViewRow
    {
        FileSystemObject::ObjectId objId = nullptr;
        size_t pathDrawEndPos; //index into pathDrawBlob_; start position defined by previous row's end position
    };
    std::vector<unsigned char> pathDrawBlob_; //draw info for components of all rows (including base folder which counts as *single* component)


    std::vector<ViewRow> viewRef_; //partial view on sortedRef_
    /*             /|\
                    | (applyFilterBy...)      */
    std::vector<FileSystemObject::ObjectId> sortedRef_; //flat view of weak pointers on folderCmp; may be sorted
    /*             /|\
                    | (setData...)
           FolderComparison folderCmp         */

    std::vector<std::tuple<const void* /*BaseFolderPair*/, AbstractPath, AbstractPath>> folderPairs_;

    class SerializeHierarchy;

    //sorting classes
    template <bool ascending, SelectedSide side>
    struct LessFullPath;

    template <bool ascending>
    struct LessRelativeFolder;

    template <bool ascending, SelectedSide side>
    struct LessFileName;

    template <bool ascending, SelectedSide side>
    struct LessFilesize;

    template <bool ascending, SelectedSide side>
    struct LessFiletime;

    template <bool ascending, SelectedSide side>
    struct LessExtension;

    template <bool ascending>
    struct LessCmpResult;

    template <bool ascending>
    struct LessSyncDirection;

    std::optional<SortInfo> currentSort_;

    std::unordered_map<std::wstring, wxSize> compExtentsBuf_; //buffer expensive wxDC::GetTextExtent() calls!
};







//##################### implementation #########################################

inline
const FileSystemObject* FileView::getFsObject(size_t row) const
{
    return row < viewRef_.size() ?
           FileSystemObject::retrieve(viewRef_[row].objId) : nullptr;
}


inline
FileSystemObject* FileView::getFsObject(size_t row)
{
    //code re-use of const method: see Meyers Effective C++
    return const_cast<FileSystemObject*>(static_cast<const FileView&>(*this).getFsObject(row));
}


inline
FileView::PathDrawInfo FileView::getDrawInfo(size_t row) const
{
    if (row < viewRef_.size())
        if (const FileSystemObject* fsObj = FileSystemObject::retrieve(viewRef_[row].objId))
        {
            const std::span<const unsigned char> pathDrawInfo(&pathDrawBlob_[row == 0 ? 0 : viewRef_[row - 1].pathDrawEndPos],
                                                              &pathDrawBlob_[0] + viewRef_[row].pathDrawEndPos); //WTF: can't use iterators with std::span on clang!?
            return { pathDrawInfo, fsObj };
        }
    return {};
}
}


#endif //GRID_VIEW_H_9285028345703475842569

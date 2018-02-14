// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef GRID_VIEW_H_9285028345703475842569
#define GRID_VIEW_H_9285028345703475842569

#include <vector>
#include <unordered_map>
#include "file_grid_attr.h"
#include "../file_hierarchy.h"


namespace fff
{
class FileView //grid view of FolderComparison
{
public:
    FileView() {}

    //direct data access via row number
    const FileSystemObject* getObject(size_t row) const; //returns nullptr if object is not found; complexity: constant!
    /**/
    FileSystemObject* getObject(size_t row);        //
    size_t rowsOnView() const { return viewRef_  .size(); } //only visible elements
    size_t rowsTotal () const { return sortedRef_.size(); } //total rows available

    //get references to FileSystemObject: no nullptr-check needed! Everything's bound.
    std::vector<FileSystemObject*> getAllFileRef(const std::vector<size_t>& rows);

    struct StatusCmpResult
    {
        bool existsExcluded = false;
        bool existsEqual    = false;
        bool existsConflict = false;

        bool existsLeftOnly   = false;
        bool existsRightOnly  = false;
        bool existsLeftNewer  = false;
        bool existsRightNewer = false;
        bool existsDifferent  = false;

        unsigned int filesOnLeftView    = 0;
        unsigned int foldersOnLeftView  = 0;
        unsigned int filesOnRightView   = 0;
        unsigned int foldersOnRightView = 0;

        uint64_t filesizeLeftView  = 0;
        uint64_t filesizeRightView = 0;
    };

    //comparison results view
    StatusCmpResult updateCmpResult(bool showExcluded,
                                    bool leftOnlyFilesActive,
                                    bool rightOnlyFilesActive,
                                    bool leftNewerFilesActive,
                                    bool rightNewerFilesActive,
                                    bool differentFilesActive,
                                    bool equalFilesActive,
                                    bool conflictFilesActive);

    struct StatusSyncPreview
    {
        bool existsExcluded = false;
        bool existsEqual    = false;
        bool existsConflict = false;

        bool existsSyncCreateLeft  = false;
        bool existsSyncCreateRight = false;
        bool existsSyncDeleteLeft  = false;
        bool existsSyncDeleteRight = false;
        bool existsSyncDirLeft     = false;
        bool existsSyncDirRight    = false;
        bool existsSyncDirNone     = false;

        unsigned int filesOnLeftView    = 0;
        unsigned int foldersOnLeftView  = 0;
        unsigned int filesOnRightView   = 0;
        unsigned int foldersOnRightView = 0;

        uint64_t filesizeLeftView  = 0;
        uint64_t filesizeRightView = 0;
    };

    //synchronization preview
    StatusSyncPreview updateSyncPreview(bool showExcluded,
                                        bool syncCreateLeftActive,
                                        bool syncCreateRightActive,
                                        bool syncDeleteLeftActive,
                                        bool syncDeleteRightActive,
                                        bool syncDirOverwLeftActive,
                                        bool syncDirOverwRightActive,
                                        bool syncDirNoneActive,
                                        bool syncEqualActive,
                                        bool conflictFilesActive);

    void setData(FolderComparison& newData);
    void removeInvalidRows(); //remove references to rows that have been deleted meanwhile: call after manual deletion and synchronization!

    //sorting...
    void sortView(ColumnTypeRim type, ItemPathFormat pathFmt, bool onLeft, bool ascending); //always call this method for sorting, never sort externally!

    struct SortInfo
    {
        ColumnTypeRim type = ColumnTypeRim::ITEM_PATH;
        bool onLeft    = false;
        bool ascending = false;
    };
    const SortInfo* getSortInfo() const { return currentSort_.get(); } //return nullptr if currently not sorted

    ptrdiff_t findRowDirect(FileSystemObject::ObjectIdConst objId) const; // find an object's row position on view list directly, return < 0 if not found
    ptrdiff_t findRowFirstChild(const ContainerObject* hierObj)    const; // find first child of FolderPair or BaseFolderPair *on sorted sub view*
    //"hierObj" may be invalid, it is NOT dereferenced, return < 0 if not found

    size_t getFolderPairCount() const { return folderPairCount_; } //count non-empty pairs to distinguish single/multiple folder pair cases

private:
    FileView           (const FileView&) = delete;
    FileView& operator=(const FileView&) = delete;

    struct RefIndex
    {
        size_t folderIndex = 0; //because of alignment there's no benefit in using "unsigned int" in 64-bit code here!
        FileSystemObject::ObjectId objId = nullptr;
    };

    template <class Predicate> void updateView(Predicate pred);


    std::unordered_map<FileSystemObject::ObjectIdConst, size_t> rowPositions_; //find row positions on sortedRef directly
    std::unordered_map<const void*, size_t> rowPositionsFirstChild_; //find first child on sortedRef of a hierarchy object
    //void* instead of ContainerObject*: these are weak pointers and should *never be dereferenced*!

    std::vector<FileSystemObject::ObjectId> viewRef_; //partial view on sortedRef
    /*             /|\
                    | (update...)
                    |                         */
    std::vector<RefIndex> sortedRef_; //flat view of weak pointers on folderCmp; may be sorted
    /*             /|\
                    | (setData...)
                    |                         */
    //std::shared_ptr<FolderComparison> folderCmp; //actual comparison data: owned by FileView!
    size_t folderPairCount_ = 0; //number of non-empty folder pairs


    class SerializeHierarchy;

    //sorting classes
    template <bool ascending, SelectedSide side>
    struct LessFullPath;

    template <bool ascending>
    struct LessRelativeFolder;

    template <bool ascending, SelectedSide side>
    struct LessShortFileName;

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

    zen::Opt<SortInfo> currentSort_;
};







//##################### implementation #########################################

inline
const FileSystemObject* FileView::getObject(size_t row) const
{
    return row < viewRef_.size() ?
           FileSystemObject::retrieve(viewRef_[row]) : nullptr;
}

inline
FileSystemObject* FileView::getObject(size_t row)
{
    //code re-use of const method: see Meyers Effective C++
    return const_cast<FileSystemObject*>(static_cast<const FileView&>(*this).getObject(row));
}
}


#endif //GRID_VIEW_H_9285028345703475842569

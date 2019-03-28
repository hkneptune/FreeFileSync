// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "file_view.h"
#include <zen/stl_tools.h>
#include <zen/perf.h>
#include "sorting.h"
#include "../base/synchronization.h"

using namespace zen;
using namespace fff;


template <class StatusResult>
void addNumbers(const FileSystemObject& fsObj, StatusResult& result)
{
    visitFSObject(fsObj, [&](const FolderPair& folder)
    {
        if (!folder.isEmpty<LEFT_SIDE>())
            ++result.folderCountLeft;

        if (!folder.isEmpty<RIGHT_SIDE>())
            ++result.folderCountRight;
    },

    [&](const FilePair& file)
    {
        if (!file.isEmpty<LEFT_SIDE>())
        {
            result.bytesLeft += file.getFileSize<LEFT_SIDE>();
            ++result.fileCountLeft;
        }
        if (!file.isEmpty<RIGHT_SIDE>())
        {
            result.bytesRight += file.getFileSize<RIGHT_SIDE>();
            ++result.fileCountRight;
        }
    },

    [&](const SymlinkPair& symlink)
    {
        if (!symlink.isEmpty<LEFT_SIDE>())
            ++result.fileCountLeft;

        if (!symlink.isEmpty<RIGHT_SIDE>())
            ++result.fileCountRight;
    });
}


template <class Predicate>
void FileView::updateView(Predicate pred)
{
    viewRef_.clear();
    rowPositions_.clear();
    rowPositionsFirstChild_.clear();

    for (const RefIndex& ref : sortedRef_)
    {
        if (const FileSystemObject* fsObj = FileSystemObject::retrieve(ref.objId))
            if (pred(*fsObj))
            {
                //save row position for direct random access to FilePair or FolderPair
                this->rowPositions_.emplace(ref.objId, viewRef_.size()); //costs: 0.28 µs per call - MSVC based on std::set
                //"this->" required by two-pass lookup as enforced by GCC 4.7

                //save row position to identify first child *on sorted subview* of FolderPair or BaseFolderPair in case latter are filtered out
                const ContainerObject* parent = &fsObj->parent();
                for (;;) //map all yet unassociated parents to this row
                {
                    const auto [it, inserted] = this->rowPositionsFirstChild_.emplace(parent, viewRef_.size());
                    if (!inserted)
                        break;

                    if (auto folder = dynamic_cast<const FolderPair*>(parent))
                        parent = &(folder->parent());
                    else
                        break;
                }

                //build subview
                this->viewRef_.push_back(ref.objId);
            }
    }
}


ptrdiff_t FileView::findRowDirect(FileSystemObject::ObjectIdConst objId) const
{
    auto it = rowPositions_.find(objId);
    return it != rowPositions_.end() ? it->second : -1;
}


ptrdiff_t FileView::findRowFirstChild(const ContainerObject* hierObj) const
{
    auto it = rowPositionsFirstChild_.find(hierObj);
    return it != rowPositionsFirstChild_.end() ? it->second : -1;
}


FileView::StatusCmpResult FileView::updateCmpResult(bool showExcluded, //maps sortedRef to viewRef
                                                    bool showLeftOnly,
                                                    bool showRightOnly,
                                                    bool showLeftNewer,
                                                    bool showRightNewer,
                                                    bool showDifferent,
                                                    bool showEqual,
                                                    bool showConflict)
{
    StatusCmpResult output;

    updateView([&](const FileSystemObject& fsObj) -> bool
    {
        auto categorize = [&](bool showCategory, bool& existsCategory)
        {
            if (!fsObj.isActive())
            {
                output.existsExcluded = true;
                if (!showExcluded)
                    return false;
            }
            existsCategory = true;
            if (!showCategory)
                return false;

            addNumbers(fsObj, output); //calculate total number of bytes for each side
            return true;
        };

        switch (fsObj.getCategory())
        {
            case FILE_LEFT_SIDE_ONLY:
                return categorize(showLeftOnly, output.existsLeftOnly);
            case FILE_RIGHT_SIDE_ONLY:
                return categorize(showRightOnly, output.existsRightOnly);
            case FILE_LEFT_NEWER:
                return categorize(showLeftNewer, output.existsLeftNewer);
            case FILE_RIGHT_NEWER:
                return categorize(showRightNewer, output.existsRightNewer);
            case FILE_DIFFERENT_CONTENT:
                return categorize(showDifferent, output.existsDifferent);
            case FILE_EQUAL:
            case FILE_DIFFERENT_METADATA: //= sub-category of equal
                return categorize(showEqual, output.existsEqual);
            case FILE_CONFLICT:
                return categorize(showConflict, output.existsConflict);
        }
        assert(false);
        return true;
    });

    return output;
}


FileView::StatusSyncPreview FileView::updateSyncPreview(bool showExcluded, //maps sortedRef to viewRef
                                                        bool showCreateLeft,
                                                        bool showCreateRight,
                                                        bool showDeleteLeft,
                                                        bool showDeleteRight,
                                                        bool showUpdateLeft,
                                                        bool showUpdateRight,
                                                        bool showDoNothing,
                                                        bool showEqual,
                                                        bool showConflict)
{
    StatusSyncPreview output;

    updateView([&](const FileSystemObject& fsObj)
    {
        auto categorize = [&](bool showCategory, bool& existsCategory)
        {
            if (!fsObj.isActive())
            {
                output.existsExcluded = true;
                if (!showExcluded)
                    return false;
            }
            existsCategory = true;
            if (!showCategory)
                return false;

            addNumbers(fsObj, output); //calculate total number of bytes for each side
            return true;
        };

        switch (fsObj.getSyncOperation()) //evaluate comparison result and sync direction
        {
            case SO_CREATE_NEW_LEFT:
                return categorize(showCreateLeft, output.existsSyncCreateLeft);
            case SO_CREATE_NEW_RIGHT:
                return categorize(showCreateRight, output.existsSyncCreateRight);
            case SO_DELETE_LEFT:
                return categorize(showDeleteLeft, output.existsSyncDeleteLeft);
            case SO_DELETE_RIGHT:
                return categorize(showDeleteRight, output.existsSyncDeleteRight);
            case SO_OVERWRITE_RIGHT:
            case SO_COPY_METADATA_TO_RIGHT: //no extra button on screen
            case SO_MOVE_RIGHT_FROM:
            case SO_MOVE_RIGHT_TO:
                return categorize(showUpdateRight, output.existsSyncDirRight);
            case SO_OVERWRITE_LEFT:
            case SO_COPY_METADATA_TO_LEFT: //no extra button on screen
            case SO_MOVE_LEFT_TO:
            case SO_MOVE_LEFT_FROM:
                return categorize(showUpdateLeft, output.existsSyncDirLeft);
            case SO_DO_NOTHING:
                return categorize(showDoNothing, output.existsSyncDirNone);
            case SO_EQUAL:
                return categorize(showEqual, output.existsEqual);
            case SO_UNRESOLVED_CONFLICT:
                return categorize(showConflict, output.existsConflict);
        }
        assert(false);
        return true;
    });

    return output;
}


std::vector<FileSystemObject*> FileView::getAllFileRef(const std::vector<size_t>& rows)
{
    const size_t viewSize = viewRef_.size();

    std::vector<FileSystemObject*> output;

    for (size_t pos : rows)
        if (pos < viewSize)
            if (FileSystemObject* fsObj = FileSystemObject::retrieve(viewRef_[pos]))
                output.push_back(fsObj);

    return output;
}


void FileView::removeInvalidRows()
{
    viewRef_.clear();
    rowPositions_.clear();
    rowPositionsFirstChild_.clear();

    //remove rows that have been deleted meanwhile
    eraseIf(sortedRef_, [&](const RefIndex& refIdx) { return !FileSystemObject::retrieve(refIdx.objId); });
}


class FileView::SerializeHierarchy
{
public:
    static void execute(ContainerObject& hierObj, std::vector<FileView::RefIndex>& sortedRef, size_t index) { SerializeHierarchy(sortedRef, index).recurse(hierObj); }

private:
    SerializeHierarchy(std::vector<FileView::RefIndex>& sortedRef, size_t index) :
        index_(index),
        output_(sortedRef) {}
#if  0
    /*
    Spend additional CPU cycles to sort the standard file list?

        Test case: 690.000 item pairs, Windows 7 x64 (C:\ vs I:\)
        ----------------------
        CmpNaturalSort: 850 ms
        CmpLocalPath:   233 ms
        CmpAsciiNoCase: 189 ms
        No sorting:      30 ms
    */
    template <class ItemPair>
    static std::vector<ItemPair*> getItemsSorted(std::list<ItemPair>& itemList)
    {
        std::vector<ItemPair*> output;
        for (ItemPair& item : itemList)
            output.push_back(&item);

        std::sort(output.begin(), output.end(), [](const ItemPair* lhs, const ItemPair* rhs) { return LessNaturalSort()(lhs->getItemNameAny(), rhs->getItemNameAny()); });
        return output;
    }
#endif
    void recurse(ContainerObject& hierObj)
    {
        for (FilePair& file : hierObj.refSubFiles())
            output_.push_back({ index_, file.getId() });

        for (SymlinkPair& symlink : hierObj.refSubLinks())
            output_.push_back({ index_, symlink.getId() });

        for (FolderPair& folder : hierObj.refSubFolders())
        {
            output_.push_back({ index_, folder.getId() });
            recurse(folder); //add recursion here to list sub-objects directly below parent!
        }
    }

    const size_t index_;
    std::vector<FileView::RefIndex>& output_;
};


void FileView::setData(FolderComparison& folderCmp)
{
    //clear everything
    std::vector<FileSystemObject::ObjectId>().swap(viewRef_); //free mem
    std::vector<RefIndex>().swap(sortedRef_);                 //
    currentSort_ = {};

    folderPairCount_ = std::count_if(begin(folderCmp), end(folderCmp),
                                     [](const BaseFolderPair& baseObj) //count non-empty pairs to distinguish single/multiple folder pair cases
    {
        return !AFS::isNullPath(baseObj.getAbstractPath< LEFT_SIDE>()) ||
               !AFS::isNullPath(baseObj.getAbstractPath<RIGHT_SIDE>());
    });

    for (auto it = begin(folderCmp); it != end(folderCmp); ++it)
        SerializeHierarchy::execute(*it, sortedRef_, it - begin(folderCmp));
}


//------------------------------------ SORTING TEMPLATES ------------------------------------------------
template <bool ascending, SelectedSide side>
struct FileView::LessFullPath
{
    bool operator()(const RefIndex a, const RefIndex b) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(a.objId);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(b.objId);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessFullPath<ascending, side>(*fsObjA, *fsObjB);
    }
};


template <bool ascending>
struct FileView::LessRelativeFolder
{
    bool operator()(const RefIndex a, const RefIndex b) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(a.objId);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(b.objId);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        //presort by folder pair
        if (a.folderIndex != b.folderIndex)
            return ascending ?
                   a.folderIndex < b.folderIndex :
                   a.folderIndex > b.folderIndex;

        return lessRelativeFolder<ascending>(*fsObjA, *fsObjB);
    }
};


template <bool ascending, SelectedSide side>
struct FileView::LessShortFileName
{
    bool operator()(const RefIndex a, const RefIndex b) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(a.objId);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(b.objId);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessShortFileName<ascending, side>(*fsObjA, *fsObjB);
    }
};


template <bool ascending, SelectedSide side>
struct FileView::LessFilesize
{
    bool operator()(const RefIndex a, const RefIndex b) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(a.objId);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(b.objId);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessFilesize<ascending, side>(*fsObjA, *fsObjB);
    }
};


template <bool ascending, SelectedSide side>
struct FileView::LessFiletime
{
    bool operator()(const RefIndex a, const RefIndex b) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(a.objId);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(b.objId);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessFiletime<ascending, side>(*fsObjA, *fsObjB);
    }
};


template <bool ascending, SelectedSide side>
struct FileView::LessExtension
{
    bool operator()(const RefIndex a, const RefIndex b) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(a.objId);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(b.objId);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessExtension<ascending, side>(*fsObjA, *fsObjB);
    }
};


template <bool ascending>
struct FileView::LessCmpResult
{
    bool operator()(const RefIndex a, const RefIndex b) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(a.objId);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(b.objId);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessCmpResult<ascending>(*fsObjA, *fsObjB);
    }
};


template <bool ascending>
struct FileView::LessSyncDirection
{
    bool operator()(const RefIndex a, const RefIndex b) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(a.objId);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(b.objId);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessSyncDirection<ascending>(*fsObjA, *fsObjB);
    }
};

//-------------------------------------------------------------------------------------------------------

void FileView::sortView(ColumnTypeRim type, ItemPathFormat pathFmt, bool onLeft, bool ascending)
{
    viewRef_               .clear();
    rowPositions_          .clear();
    rowPositionsFirstChild_.clear();
    currentSort_ = SortInfo({ type, onLeft, ascending });

    switch (type)
    {
        case ColumnTypeRim::ITEM_PATH:
            switch (pathFmt)
            {
                case ItemPathFormat::FULL_PATH:
                    if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<true,  LEFT_SIDE >());
                    else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<true,  RIGHT_SIDE>());
                    else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<false, LEFT_SIDE >());
                    else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<false, RIGHT_SIDE>());
                    break;

                case ItemPathFormat::RELATIVE_PATH:
                    if      ( ascending) std::sort(sortedRef_.begin(), sortedRef_.end(), LessRelativeFolder<true>());
                    else if (!ascending) std::sort(sortedRef_.begin(), sortedRef_.end(), LessRelativeFolder<false>());
                    break;

                case ItemPathFormat::ITEM_NAME:
                    if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessShortFileName<true,  LEFT_SIDE >());
                    else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessShortFileName<true,  RIGHT_SIDE>());
                    else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessShortFileName<false, LEFT_SIDE >());
                    else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessShortFileName<false, RIGHT_SIDE>());
                    break;
            }
            break;

        case ColumnTypeRim::SIZE:
            if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFilesize<true,  LEFT_SIDE >());
            else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFilesize<true,  RIGHT_SIDE>());
            else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFilesize<false, LEFT_SIDE >());
            else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFilesize<false, RIGHT_SIDE>());
            break;
        case ColumnTypeRim::DATE:
            if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFiletime<true,  LEFT_SIDE >());
            else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFiletime<true,  RIGHT_SIDE>());
            else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFiletime<false, LEFT_SIDE >());
            else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFiletime<false, RIGHT_SIDE>());
            break;
        case ColumnTypeRim::EXTENSION:
            if      ( ascending &&  onLeft) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessExtension<true,  LEFT_SIDE >());
            else if ( ascending && !onLeft) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessExtension<true,  RIGHT_SIDE>());
            else if (!ascending &&  onLeft) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessExtension<false, LEFT_SIDE >());
            else if (!ascending && !onLeft) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessExtension<false, RIGHT_SIDE>());
            break;
    }
}

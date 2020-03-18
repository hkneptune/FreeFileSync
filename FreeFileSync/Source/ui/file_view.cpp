// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "file_view.h"
#include <zen/stl_tools.h>
#include <zen/perf.h>
#include "../base/synchronization.h"

using namespace zen;
using namespace fff;


namespace
{
template <class ViewStats>
void addNumbers(const FileSystemObject& fsObj, ViewStats& stats)
{
    visitFSObject(fsObj, [&](const FolderPair& folder)
    {
        if (!folder.isEmpty<LEFT_SIDE>())
            ++stats.fileStatsLeft.folderCount;

        if (!folder.isEmpty<RIGHT_SIDE>())
            ++stats.fileStatsRight.folderCount;
    },

    [&](const FilePair& file)
    {
        if (!file.isEmpty<LEFT_SIDE>())
        {
            stats.fileStatsLeft.bytes += file.getFileSize<LEFT_SIDE>();
            ++stats.fileStatsLeft.fileCount;
        }
        if (!file.isEmpty<RIGHT_SIDE>())
        {
            stats.fileStatsRight.bytes += file.getFileSize<RIGHT_SIDE>();
            ++stats.fileStatsRight.fileCount;
        }
    },

    [&](const SymlinkPair& symlink)
    {
        if (!symlink.isEmpty<LEFT_SIDE>())
            ++stats.fileStatsLeft.fileCount;

        if (!symlink.isEmpty<RIGHT_SIDE>())
            ++stats.fileStatsRight.fileCount;
    });
}
}


template <class Predicate>
void FileView::updateView(Predicate pred)
{
    viewRef_.clear();
    rowPositions_.clear();
    rowPositionsFirstChild_.clear();

    for (const RefIndex& ref : sortedRef_)
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


FileView::CategoryViewStats FileView::applyFilterByCategory(bool showExcluded, //maps sortedRef to viewRef
                                                            bool showLeftOnly,
                                                            bool showRightOnly,
                                                            bool showLeftNewer,
                                                            bool showRightNewer,
                                                            bool showDifferent,
                                                            bool showEqual,
                                                            bool showConflict)
{
    CategoryViewStats stats;

    updateView([&](const FileSystemObject& fsObj)
    {
        auto categorize = [&](bool showCategory, int& categoryCount)
        {
            if (!fsObj.isActive())
            {
                ++stats.excluded;
                if (!showExcluded)
                    return false;
            }
            ++categoryCount;
            if (!showCategory)
                return false;

            addNumbers(fsObj, stats); //calculate total number of bytes for each side
            return true;
        };

        switch (fsObj.getCategory())
        {
            case FILE_LEFT_SIDE_ONLY:
                return categorize(showLeftOnly, stats.leftOnly);
            case FILE_RIGHT_SIDE_ONLY:
                return categorize(showRightOnly, stats.rightOnly);
            case FILE_LEFT_NEWER:
                return categorize(showLeftNewer, stats.leftNewer);
            case FILE_RIGHT_NEWER:
                return categorize(showRightNewer, stats.rightNewer);
            case FILE_DIFFERENT_CONTENT:
                return categorize(showDifferent, stats.different);
            case FILE_EQUAL:
            case FILE_DIFFERENT_METADATA: //= sub-category of equal
                return categorize(showEqual, stats.equal);
            case FILE_CONFLICT:
                return categorize(showConflict, stats.conflict);
        }
        assert(false);
        return true;
    });

    return stats;
}


FileView::ActionViewStats FileView::applyFilterByAction(bool showExcluded, //maps sortedRef to viewRef
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
    ActionViewStats stats;

    int moveLeft  = 0;
    int moveRight = 0;

    updateView([&](const FileSystemObject& fsObj)
    {
        auto categorize = [&](bool showCategory, int& categoryCount)
        {
            if (!fsObj.isActive())
            {
                ++stats.excluded;
                if (!showExcluded)
                    return false;
            }
            ++categoryCount;
            if (!showCategory)
                return false;

            addNumbers(fsObj, stats); //calculate total number of bytes for each side
            return true;
        };

        switch (fsObj.getSyncOperation()) //evaluate comparison result and sync direction
        {
            case SO_CREATE_NEW_LEFT:
                return categorize(showCreateLeft, stats.createLeft);
            case SO_CREATE_NEW_RIGHT:
                return categorize(showCreateRight, stats.createRight);
            case SO_DELETE_LEFT:
                return categorize(showDeleteLeft, stats.deleteLeft);
            case SO_DELETE_RIGHT:
                return categorize(showDeleteRight, stats.deleteRight);
            case SO_OVERWRITE_LEFT:
            case SO_COPY_METADATA_TO_LEFT: //no extra filter button
                return categorize(showUpdateLeft, stats.updateLeft);
            case SO_MOVE_LEFT_FROM:
            case SO_MOVE_LEFT_TO:
                return categorize(showUpdateLeft, moveLeft);
            case SO_OVERWRITE_RIGHT:
            case SO_COPY_METADATA_TO_RIGHT: //no extra filter button
                return categorize(showUpdateRight, stats.updateRight);
            case SO_MOVE_RIGHT_FROM:
            case SO_MOVE_RIGHT_TO:
                return categorize(showUpdateRight, moveRight);
            case SO_DO_NOTHING:
                return categorize(showDoNothing, stats.updateNone);
            case SO_EQUAL:
                return categorize(showEqual, stats.equal);
            case SO_UNRESOLVED_CONFLICT:
                return categorize(showConflict, stats.conflict);
        }
        assert(false);
        return true;
    });

    assert(moveLeft % 2 == 0 && moveRight % 2 == 0);
    stats.updateLeft  += moveLeft  / 2; //count move operations as single update
    stats.updateRight += moveRight / 2; //=> harmonize with SyncStatistics::processFile()

    return stats;
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
    std::erase_if(sortedRef_, [&](const RefIndex& refIdx) { return !FileSystemObject::retrieve(refIdx.objId); });
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
    /* Spend additional CPU cycles to sort the standard file list?

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


//------------------------------------ SORTING -----------------------------------------
namespace
{
struct CompileTimeReminder : public FSObjectVisitor
{
    void visit(const FilePair&    file   ) override {}
    void visit(const SymlinkPair& symlink) override {}
    void visit(const FolderPair&  folder ) override {}
} checkDymanicCasts; //just a compile-time reminder to manually check dynamic casts in this file if ever needed


inline
bool isDirectoryPair(const FileSystemObject& fsObj)
{
    return dynamic_cast<const FolderPair*>(&fsObj) != nullptr;
}


template <bool ascending, SelectedSide side> inline
bool lessShortFileName(const FileSystemObject& lhs, const FileSystemObject& rhs)
{
    //sort order: first files/symlinks, then directories then empty rows

    //empty rows always last
    if (lhs.isEmpty<side>())
        return false;
    else if (rhs.isEmpty<side>())
        return true;

    //directories after files/symlinks:
    if (isDirectoryPair(lhs))
    {
        if (!isDirectoryPair(rhs))
            return false;
    }
    else if (isDirectoryPair(rhs))
        return true;

    //sort directories and files/symlinks by short name
    return zen::makeSortDirection(LessNaturalSort() /*even on Linux*/, std::bool_constant<ascending>())(lhs.getItemName<side>(), rhs.getItemName<side>());
}


template <bool ascending, SelectedSide side> inline
bool lessFullPath(const FileSystemObject& lhs, const FileSystemObject& rhs)
{
    //empty rows always last
    if (lhs.isEmpty<side>())
        return false;
    else if (rhs.isEmpty<side>())
        return true;

    return zen::makeSortDirection(LessNaturalSort() /*even on Linux*/, std::bool_constant<ascending>())(
               zen::utfTo<Zstring>(AFS::getDisplayPath(lhs.getAbstractPath<side>())),
               zen::utfTo<Zstring>(AFS::getDisplayPath(rhs.getAbstractPath<side>())));
}


template <bool ascending>  inline //side currently unused!
bool lessRelativeFolder(const FileSystemObject& lhs, const FileSystemObject& rhs)
{
    const bool isDirectoryL = isDirectoryPair(lhs);
    const Zstring& relFolderL = isDirectoryL ?
                                lhs.getRelativePathAny() :
                                lhs.parent().getRelativePathAny();

    const bool isDirectoryR = isDirectoryPair(rhs);
    const Zstring& relFolderR = isDirectoryR ?
                                rhs.getRelativePathAny() :
                                rhs.parent().getRelativePathAny();

    //compare relative names without filepaths first
    const int rv = compareNatural(relFolderL, relFolderR);
    if (rv != 0)
        return zen::makeSortDirection(std::less<int>(), std::bool_constant<ascending>())(rv, 0);

    //make directories always appear before contained files
    if (isDirectoryR)
        return false;
    else if (isDirectoryL)
        return true;

    return zen::makeSortDirection(LessNaturalSort(), std::bool_constant<ascending>())(lhs.getItemNameAny(), rhs.getItemNameAny());
}


template <bool ascending, SelectedSide side> inline
bool lessFilesize(const FileSystemObject& lhs, const FileSystemObject& rhs)
{
    //empty rows always last
    if (lhs.isEmpty<side>())
        return false;
    else if (rhs.isEmpty<side>())
        return true;

    //directories second last
    if (isDirectoryPair(lhs))
        return false;
    else if (isDirectoryPair(rhs))
        return true;

    const FilePair* fileL = dynamic_cast<const FilePair*>(&lhs);
    const FilePair* fileR = dynamic_cast<const FilePair*>(&rhs);

    //then symlinks
    if (!fileL)
        return false;
    else if (!fileR)
        return true;

    //return list beginning with largest files first
    return zen::makeSortDirection(std::less<>(), std::bool_constant<ascending>())(fileL->getFileSize<side>(), fileR->getFileSize<side>());
}


template <bool ascending, SelectedSide side> inline
bool lessFiletime(const FileSystemObject& lhs, const FileSystemObject& rhs)
{
    if (lhs.isEmpty<side>())
        return false; //empty rows always last
    else if (rhs.isEmpty<side>())
        return true; //empty rows always last

    const FilePair* fileL = dynamic_cast<const FilePair*>(&lhs);
    const FilePair* fileR = dynamic_cast<const FilePair*>(&rhs);

    const SymlinkPair* symlinkL = dynamic_cast<const SymlinkPair*>(&lhs);
    const SymlinkPair* symlinkR = dynamic_cast<const SymlinkPair*>(&rhs);

    if (!fileL && !symlinkL)
        return false; //directories last
    else if (!fileR && !symlinkR)
        return true; //directories last

    const int64_t dateL = fileL ? fileL->getLastWriteTime<side>() : symlinkL->getLastWriteTime<side>();
    const int64_t dateR = fileR ? fileR->getLastWriteTime<side>() : symlinkR->getLastWriteTime<side>();

    //return list beginning with newest files first
    return zen::makeSortDirection(std::less<>(), std::bool_constant<ascending>())(dateL, dateR);
}


template <bool ascending, SelectedSide side> inline
bool lessExtension(const FileSystemObject& lhs, const FileSystemObject& rhs)
{
    if (lhs.isEmpty<side>())
        return false; //empty rows always last
    else if (rhs.isEmpty<side>())
        return true; //empty rows always last

    if (dynamic_cast<const FolderPair*>(&lhs))
        return false; //directories last
    else if (dynamic_cast<const FolderPair*>(&rhs))
        return true; //directories last

    auto getExtension = [](const FileSystemObject& fsObj)
    {
        return afterLast(fsObj.getItemName<side>(), Zstr('.'), zen::IF_MISSING_RETURN_NONE);
    };

    return zen::makeSortDirection(LessNaturalSort() /*even on Linux*/, std::bool_constant<ascending>())(getExtension(lhs), getExtension(rhs));
}


template <bool ascending> inline
bool lessCmpResult(const FileSystemObject& lhs, const FileSystemObject& rhs)
{
    return zen::makeSortDirection([](CompareFileResult lhs2, CompareFileResult rhs2)
    {
        //presort: equal shall appear at end of list
        if (lhs2 == FILE_EQUAL)
            return false;
        if (rhs2 == FILE_EQUAL)
            return true;
        return lhs2 < rhs2;
    },
    std::bool_constant<ascending>())(lhs.getCategory(), rhs.getCategory());
}


template <bool ascending> inline
bool lessSyncDirection(const FileSystemObject& lhs, const FileSystemObject& rhs)
{
    return zen::makeSortDirection(std::less<>(), std::bool_constant<ascending>())(lhs.getSyncOperation(), rhs.getSyncOperation());
}
}


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
        {
            if constexpr (ascending)
                return a.folderIndex < b.folderIndex;
            else
                return a.folderIndex > b.folderIndex;
        }

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
                    if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<true,   LEFT_SIDE>());
                    else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<true,  RIGHT_SIDE>());
                    else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<false,  LEFT_SIDE>());
                    else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<false, RIGHT_SIDE>());
                    break;

                case ItemPathFormat::RELATIVE_PATH:
                    if      ( ascending) std::sort(sortedRef_.begin(), sortedRef_.end(), LessRelativeFolder<true>());
                    else if (!ascending) std::sort(sortedRef_.begin(), sortedRef_.end(), LessRelativeFolder<false>());
                    break;

                case ItemPathFormat::ITEM_NAME:
                    if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessShortFileName<true,   LEFT_SIDE>());
                    else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessShortFileName<true,  RIGHT_SIDE>());
                    else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessShortFileName<false,  LEFT_SIDE>());
                    else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessShortFileName<false, RIGHT_SIDE>());
                    break;
            }
            break;

        case ColumnTypeRim::SIZE:
            if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFilesize<true,   LEFT_SIDE>());
            else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFilesize<true,  RIGHT_SIDE>());
            else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFilesize<false,  LEFT_SIDE>());
            else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFilesize<false, RIGHT_SIDE>());
            break;
        case ColumnTypeRim::DATE:
            if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFiletime<true,   LEFT_SIDE>());
            else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFiletime<true,  RIGHT_SIDE>());
            else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFiletime<false,  LEFT_SIDE>());
            else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFiletime<false, RIGHT_SIDE>());
            break;
        case ColumnTypeRim::EXTENSION:
            if      ( ascending &&  onLeft) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessExtension<true,   LEFT_SIDE>());
            else if ( ascending && !onLeft) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessExtension<true,  RIGHT_SIDE>());
            else if (!ascending &&  onLeft) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessExtension<false,  LEFT_SIDE>());
            else if (!ascending && !onLeft) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessExtension<false, RIGHT_SIDE>());
            break;
    }
}


void FileView::sortView(ColumnTypeCenter type, bool ascending)
{
    viewRef_               .clear();
    rowPositions_          .clear();
    rowPositionsFirstChild_.clear();
    currentSort_ = SortInfo({ type, false, ascending });

    switch (type)
    {
        case ColumnTypeCenter::CHECKBOX:
            assert(false);
            break;
        case ColumnTypeCenter::CMP_CATEGORY:
            if      ( ascending) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessCmpResult<true >());
            else if (!ascending) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessCmpResult<false>());
            break;
        case ColumnTypeCenter::SYNC_ACTION:
            if      ( ascending) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessSyncDirection<true >());
            else if (!ascending) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessSyncDirection<false>());
            break;
    }
}

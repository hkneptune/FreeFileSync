// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "file_view.h"
#include <span>
#include <zen/stl_tools.h>
//#include <zen/perf.h>
#include <zen/thread.h>
//#include "../base/synchronization.h"

using namespace zen;
using namespace fff;


namespace
{
void serializeHierarchy(ContainerObject& conObj, std::vector<FileSystemObject::ObjectId>& output)
{
    for (FilePair& file : conObj.refSubFiles())
        output.push_back(file.getId());

    for (SymlinkPair& symlink : conObj.refSubLinks())
        output.push_back(symlink.getId());

    for (FolderPair& folder : conObj.refSubFolders())
    {
        output.push_back(folder.getId());
        serializeHierarchy(folder, output); //add recursion here to list sub-objects directly below parent!
    }

#if  0
    /* Spend additional CPU cycles to sort the standard file list?

        Test case: 690.000 item pairs, Windows 7 x64 (C:\ vs I:\)
        ----------------------
        CmpNaturalSort: 850 ms
        CmpLocalPath:   233 ms
        CmpAsciiNoCase: 189 ms
        No sorting:      30 ms                         */

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
}
}


FileView::FileView(FolderComparison& folderCmp)
{
    std::for_each(begin(folderCmp), end(folderCmp), [&](BaseFolderPair& baseObj)
    {
        serializeHierarchy(baseObj, sortedRef_);

        folderPairs_.emplace_back(&baseObj,
                                  baseObj.getAbstractPath<SelectSide::left >(),
                                  baseObj.getAbstractPath<SelectSide::right>());
    });
}


template <class Predicate>
void FileView::updateView(Predicate pred)
{
    viewRef_               .clear();
    groupDetails_          .clear();
    rowPositions_          .clear();
    rowPositionsFirstChild_.clear();

    static uint64_t globalViewUpdateId;
    viewUpdateId_ = ++globalViewUpdateId;
    assert(runningOnMainThread());

    std::vector<const ContainerObject*> parentsBuf; //from bottom to top of hierarchy
    const ContainerObject* groupStartObj = nullptr;

    for (const FileSystemObject::ObjectId& objId : sortedRef_)
        if (const FileSystemObject* const fsObj = FileSystemObject::retrieve(objId))
            if (pred(*fsObj))
            {
                const size_t row = viewRef_.size();

                //save row position for direct random access to FilePair or FolderPair
                rowPositions_.emplace(objId, row); //costs: 0.28 µs per call - MSVC based on std::set

                parentsBuf.clear();
                for (const FileSystemObject* fsObj2 = fsObj;;)
                {
                    const ContainerObject& parent = fsObj2->parent();
                    parentsBuf.push_back(&parent);

                    fsObj2 = dynamic_cast<const FolderPair*>(&parent);
                    if (!fsObj2)
                        break;
                }

                //save row position to identify first child *on sorted subview* of FolderPair or BaseFolderPair in case latter are filtered out
                for (const ContainerObject* parent : parentsBuf)
                    if (const auto [it, inserted] = this->rowPositionsFirstChild_.emplace(parent, row);
                        !inserted) //=> parents further up in hierarchy already inserted!
                        break;

                //------ save info to aggregate rows by parent folders ------
                if (const auto folder = dynamic_cast<const FolderPair*>(fsObj))
                {
                    groupStartObj = folder;
                    groupDetails_.push_back({row});
                }
                else if (&fsObj->parent() != groupStartObj)
                {
                    groupStartObj = &fsObj->parent();
                    groupDetails_.push_back({row});
                }
                assert(!groupDetails_.empty());
                const size_t groupIdx = groupDetails_.size() - 1;
                //-----------------------------------------------------------
                viewRef_.push_back({objId, groupIdx});
            }
}


ptrdiff_t FileView::findRowDirect(FileSystemObject::ObjectIdConst objId) const
{
    auto it = rowPositions_.find(objId);
    return it != rowPositions_.end() ? it->second : -1;
}


ptrdiff_t FileView::findRowFirstChild(const ContainerObject* conObj) const
{
    auto it = rowPositionsFirstChild_.find(conObj);
    return it != rowPositionsFirstChild_.end() ? it->second : -1;
}


namespace
{
template <class ViewStats>
void addNumbers(const FileSystemObject& fsObj, ViewStats& stats)
{
    visitFSObject(fsObj, [&](const FolderPair& folder)
    {
        if (!folder.isEmpty<SelectSide::left>())
            ++stats.fileStatsLeft.folderCount;

        if (!folder.isEmpty<SelectSide::right>())
            ++stats.fileStatsRight.folderCount;
    },

    [&](const FilePair& file)
    {
        if (!file.isEmpty<SelectSide::left>())
        {
            stats.fileStatsLeft.bytes += file.getFileSize<SelectSide::left>();
            ++stats.fileStatsLeft.fileCount;
        }
        if (!file.isEmpty<SelectSide::right>())
        {
            stats.fileStatsRight.bytes += file.getFileSize<SelectSide::right>();
            ++stats.fileStatsRight.fileCount;
        }
    },

    [&](const SymlinkPair& symlink)
    {
        if (!symlink.isEmpty<SelectSide::left>())
            ++stats.fileStatsLeft.fileCount;

        if (!symlink.isEmpty<SelectSide::right>())
            ++stats.fileStatsRight.fileCount;
    });
}
}


FileView::DifferenceViewStats FileView::applyDifferenceFilter(bool showExcluded, //maps sortedRef to viewRef
                                                              bool showLeftOnly,
                                                              bool showRightOnly,
                                                              bool showLeftNewer,
                                                              bool showRightNewer,
                                                              bool showDifferent,
                                                              bool showEqual,
                                                              bool showConflict)
{
    DifferenceViewStats stats;

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
            case FILE_LEFT_ONLY:
                return categorize(showLeftOnly, stats.leftOnly);
            case FILE_RIGHT_ONLY:
                return categorize(showRightOnly, stats.rightOnly);
            case FILE_LEFT_NEWER:
                return categorize(showLeftNewer, stats.leftNewer);
            case FILE_RIGHT_NEWER:
                return categorize(showRightNewer, stats.rightNewer);
            case FILE_DIFFERENT_CONTENT:
                return categorize(showDifferent, stats.different);
            case FILE_EQUAL:
                return categorize(showEqual, stats.equal);
            case FILE_RENAMED:
            case FILE_CONFLICT:
            case FILE_TIME_INVALID:
                return categorize(showConflict, stats.conflict);
        }
        assert(false);
        return true;
    });

    return stats;
}


FileView::ActionViewStats FileView::applyActionFilter(bool showExcluded, //maps sortedRef to viewRef
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
            case SO_CREATE_LEFT:
                return categorize(showCreateLeft, stats.createLeft);
            case SO_CREATE_RIGHT:
                return categorize(showCreateRight, stats.createRight);
            case SO_DELETE_LEFT:
                return categorize(showDeleteLeft, stats.deleteLeft);
            case SO_DELETE_RIGHT:
                return categorize(showDeleteRight, stats.deleteRight);
            case SO_OVERWRITE_LEFT:
            case SO_RENAME_LEFT:
                return categorize(showUpdateLeft, stats.updateLeft);
            case SO_MOVE_LEFT_FROM:
            case SO_MOVE_LEFT_TO:
                return categorize(showUpdateLeft, moveLeft);
            case SO_OVERWRITE_RIGHT:
            case SO_RENAME_RIGHT:
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
            if (FileSystemObject* fsObj = FileSystemObject::retrieve(viewRef_[pos].objId))
                output.push_back(fsObj);

    return output;
}


FileView::PathDrawInfo FileView::getDrawInfo(size_t row)
{
    if (row < viewRef_.size())
    {
        const size_t groupIdx = viewRef_[row].groupIdx;
        assert(groupIdx < groupDetails_.size());

        const size_t groupFirstRow = groupDetails_[groupIdx].groupFirstRow;

        const size_t groupLastRow = groupIdx + 1 < groupDetails_.size() ?
                                    groupDetails_[groupIdx + 1].groupFirstRow :
                                    viewRef_.size();
        FileSystemObject* fsObj = FileSystemObject::retrieve(viewRef_[row].objId);

        FolderPair* folderGroupObj = dynamic_cast<FolderPair*>(fsObj);
        if (fsObj && !folderGroupObj)
            folderGroupObj = dynamic_cast<FolderPair*>(&fsObj->parent());

        return {groupFirstRow, groupLastRow, groupIdx, viewUpdateId_, folderGroupObj, fsObj};
    }
    assert(false); //unexpected: check rowsOnView()!
    return {};
}


void FileView::removeInvalidRows()
{
    //remove rows that have been deleted meanwhile
    std::erase_if(sortedRef_, [&](const FileSystemObject::ObjectId& objId) { return !FileSystemObject::retrieve(objId); });

    viewRef_               .clear();
    groupDetails_          .clear();
    rowPositions_          .clear();
    rowPositionsFirstChild_.clear();
}


size_t FileView::getEffectiveFolderPairCount() const
{
    return std::count_if(folderPairs_.begin(), folderPairs_.end(), [](const auto& folderPair)
    {
        const auto& [baseObj, basePathL, basePathR] = folderPair;

        return !AFS::isNullPath(basePathL) ||
               !AFS::isNullPath(basePathR);
    });
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


template <bool ascending, SelectSide side> inline
bool lessFileName(const FileSystemObject& lhs, const FileSystemObject& rhs)
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

    return zen::makeSortDirection(LessNaturalSort() /*even on Linux*/, std::bool_constant<ascending>())(lhs.getItemName<side>(), rhs.getItemName<side>());
}


template <bool ascending, SelectSide side>  inline
bool lessFilePath(const FileSystemObject::ObjectId& lhs, const FileSystemObject::ObjectId& rhs,
                  const std::unordered_map<const void* /*BaseFolderPair*/, size_t /*position*/>& sortedPos,
                  std::vector<const FolderPair*>& tempBuf)
{
    const FileSystemObject* fsObjL = FileSystemObject::retrieve(lhs);
    const FileSystemObject* fsObjR = FileSystemObject::retrieve(rhs);
    if (!fsObjL) //invalid rows shall appear at the end
        return false;
    else if (!fsObjR)
        return true;

    //------- presort by folder pair ----------
    {
        auto itL = sortedPos.find(&fsObjL->base());
        auto itR = sortedPos.find(&fsObjR->base());
        assert(itL != sortedPos.end() && itR != sortedPos.end());
        if (itL == sortedPos.end()) //invalid rows shall appear at the end
            return false;
        else if (itR == sortedPos.end())
            return true;

        const size_t basePosL = itL->second;
        const size_t basePosR = itR->second;

        if (basePosL != basePosR)
            return zen::makeSortDirection(std::less(), std::bool_constant<ascending>())(basePosL, basePosR);
    }

    //------- sort component-wise ----------
    const auto folderL = dynamic_cast<const FolderPair*>(fsObjL);
    const auto folderR = dynamic_cast<const FolderPair*>(fsObjR);

    std::vector<const FolderPair*>& parentsBuf = tempBuf; //from bottom to top of hierarchy, excluding base
    parentsBuf.clear();

    const auto collectParents = [&](const FileSystemObject* fsObj)
    {
        for (;;)
            if (const auto folder = dynamic_cast<const FolderPair*>(&fsObj->parent())) //perf: most expensive part of this function!
            {
                parentsBuf.push_back(folder);
                fsObj = folder;
            }
            else
                break;
    };
    if (folderL)
        parentsBuf.push_back(folderL);
    collectParents(fsObjL);
    const size_t parentsSizeL = parentsBuf.size();

    if (folderR)
        parentsBuf.push_back(folderR);
    collectParents(fsObjR);

    const std::span<const FolderPair*> parentsL(parentsBuf.data(), parentsSizeL); //no construction via iterator (yet): https://github.com/cplusplus/draft/pull/3456
    const std::span<const FolderPair*> parentsR(parentsBuf.data() + parentsSizeL, parentsBuf.size() - parentsSizeL);

    const auto& [itL, itR] = std::mismatch(parentsL.rbegin(), parentsL.rend(),
                                           parentsR.rbegin(), parentsR.rend());
    if (itL == parentsL.rend())
    {
        if (itR == parentsR.rend())
        {
            //make folders always appear before contained files
            if (folderR)
                return false;
            else if (folderL)
                return true;

            return zen::makeSortDirection(LessNaturalSort(), std::bool_constant<ascending>())(fsObjL->getItemName<side>(), fsObjR->getItemName<side>());
        }
        else
            return true;
    }
    else if (itR == parentsR.rend())
        return false;

    //different components...
    if (const std::weak_ordering cmp = compareNatural((*itL)->getItemName<side>(), (*itR)->getItemName<side>());
        cmp != std::weak_ordering::equivalent)
    {
        if constexpr (ascending)
            return std::is_lt(cmp);
        else
            return std::is_gt(cmp);
    }
    //return zen::makeSortDirection(std::less(), std::bool_constant<ascending>())(rv, 0);

    /*...with equivalent names:
        1. functional correctness => must not compare equal!  e.g. a/a/x and a/A/y
        2. ensure stable sort order                                                            */
    return *itL < *itR;
}


template <bool ascending, SelectSide side> inline
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
    return zen::makeSortDirection(std::less(), std::bool_constant<ascending>())(fileL->getFileSize<side>(), fileR->getFileSize<side>());
}


template <bool ascending, SelectSide side> inline
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
    return zen::makeSortDirection(std::less(), std::bool_constant<ascending>())(dateL, dateR);
}


template <bool ascending, SelectSide side> inline
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
        return afterLast(fsObj.getItemName<side>(), Zstr('.'), zen::IfNotFoundReturn::none);
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
    return zen::makeSortDirection(std::less(), std::bool_constant<ascending>())(lhs.getSyncOperation(), rhs.getSyncOperation());
}


template <bool ascending, SelectSide side>
struct LessFullPath
{
    LessFullPath(std::vector<std::tuple<const void* /*BaseFolderPair*/, AbstractPath, AbstractPath>> folderPairs)
    {
        //calculate positions of base folders sorted by name
        std::sort(folderPairs.begin(), folderPairs.end(), [](const auto& a, const auto& b)
        {
            const auto& [baseObjA, basePathLA, basePathRA] = a;
            const auto& [baseObjB, basePathLB, basePathRB] = b;

            const AbstractPath& basePathA = selectParam<side>(basePathLA, basePathRA);
            const AbstractPath& basePathB = selectParam<side>(basePathLB, basePathRB);

            return LessNaturalSort()/*even on Linux*/(zen::utfTo<Zstring>(AFS::getDisplayPath(basePathA)),
                                                      zen::utfTo<Zstring>(AFS::getDisplayPath(basePathB)));
        });

        size_t pos = 0;
        for (const auto& [baseObj, basePathL, basePathR] : folderPairs)
            sortedPos_.ref().emplace(baseObj, pos++);
    }

    bool operator()(const FileSystemObject::ObjectId& lhs, const FileSystemObject::ObjectId& rhs) const
    {
        return lessFilePath<ascending, side>(lhs, rhs, sortedPos_.ref(), tempBuf_);
    }

private:
    SharedRef<std::unordered_map<const void* /*BaseFolderPair*/, size_t /*position*/>> sortedPos_ = makeSharedRef<std::unordered_map<const void*, size_t>>();
    mutable std::vector<const FolderPair*> tempBuf_; //avoid repeated memory allocation in lessFilePath()
};


template <bool ascending, SelectSide side>
struct LessRelativeFolder
{
    LessRelativeFolder(const std::vector<std::tuple<const void* /*BaseFolderPair*/, AbstractPath, AbstractPath>>& folderPairs)
    {
        //take over positions of base folders as set up by user
        size_t pos = 0;
        for (const auto& [baseObj, basePathL, basePathR] : folderPairs)
            sortedPos_.ref().emplace(baseObj, pos++);
    }

    bool operator()(const FileSystemObject::ObjectId& lhs, const FileSystemObject::ObjectId& rhs) const
    {
        return lessFilePath<ascending, side>(lhs, rhs, sortedPos_.ref(), tempBuf_);
    }

private:
    SharedRef<std::unordered_map<const void* /*BaseFolderPair*/, size_t /*position*/>> sortedPos_ = makeSharedRef<std::unordered_map<const void*, size_t>>();
    mutable std::vector<const FolderPair*> tempBuf_; //avoid repeated memory allocation in lessFilePath()
};


template <bool ascending, SelectSide side>
struct LessFileName
{
    bool operator()(const FileSystemObject::ObjectId& lhs, const FileSystemObject::ObjectId& rhs) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(lhs);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(rhs);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessFileName<ascending, side>(*fsObjA, *fsObjB);
    }
};


template <bool ascending, SelectSide side>
struct LessFilesize
{
    bool operator()(const FileSystemObject::ObjectId& lhs, const FileSystemObject::ObjectId& rhs) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(lhs);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(rhs);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessFilesize<ascending, side>(*fsObjA, *fsObjB);
    }
};


template <bool ascending, SelectSide side>
struct LessFiletime
{
    bool operator()(const FileSystemObject::ObjectId& lhs, const FileSystemObject::ObjectId& rhs) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(lhs);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(rhs);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessFiletime<ascending, side>(*fsObjA, *fsObjB);
    }
};


template <bool ascending, SelectSide side>
struct LessExtension
{
    bool operator()(const FileSystemObject::ObjectId& lhs, const FileSystemObject::ObjectId& rhs) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(lhs);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(rhs);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessExtension<ascending, side>(*fsObjA, *fsObjB);
    }
};


template <bool ascending>
struct LessCmpResult
{
    bool operator()(const FileSystemObject::ObjectId& lhs, const FileSystemObject::ObjectId& rhs) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(lhs);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(rhs);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessCmpResult<ascending>(*fsObjA, *fsObjB);
    }
};


template <bool ascending>
struct LessSyncDirection
{
    bool operator()(const FileSystemObject::ObjectId& lhs, const FileSystemObject::ObjectId& rhs) const
    {
        const FileSystemObject* fsObjA = FileSystemObject::retrieve(lhs);
        const FileSystemObject* fsObjB = FileSystemObject::retrieve(rhs);
        if (!fsObjA) //invalid rows shall appear at the end
            return false;
        else if (!fsObjB)
            return true;

        return lessSyncDirection<ascending>(*fsObjA, *fsObjB);
    }
};
}

//-------------------------------------------------------------------------------------------------------

void FileView::sortView(ColumnTypeRim type, ItemPathFormat pathFmt, bool onLeft, bool ascending)
{
    viewRef_               .clear();
    groupDetails_          .clear();
    rowPositions_          .clear();
    rowPositionsFirstChild_.clear();
    currentSort_ = SortInfo({type, onLeft, ascending});

    switch (type)
    {
        case ColumnTypeRim::path:
            switch (pathFmt)
            {
                case ItemPathFormat::name:
                    if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFileName<true,  SelectSide::left >());
                    else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFileName<true,  SelectSide::right>());
                    else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFileName<false, SelectSide::left >());
                    else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFileName<false, SelectSide::right>());
                    break;

                case ItemPathFormat::relative:
                    if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessRelativeFolder<true,  SelectSide::left >(folderPairs_));
                    else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessRelativeFolder<true,  SelectSide::right>(folderPairs_));
                    else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessRelativeFolder<false, SelectSide::left >(folderPairs_));
                    else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessRelativeFolder<false, SelectSide::right>(folderPairs_));
                    break;

                case ItemPathFormat::full:
                    if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<true,  SelectSide::left >(folderPairs_));
                    else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<true,  SelectSide::right>(folderPairs_));
                    else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<false, SelectSide::left >(folderPairs_));
                    else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<false, SelectSide::right>(folderPairs_));
                    break;
            }
            break;

        case ColumnTypeRim::size:
            if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFilesize<true,  SelectSide::left >());
            else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFilesize<true,  SelectSide::right>());
            else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFilesize<false, SelectSide::left >());
            else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFilesize<false, SelectSide::right>());
            break;
        case ColumnTypeRim::date:
            if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFiletime<true,  SelectSide::left >());
            else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFiletime<true,  SelectSide::right>());
            else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFiletime<false, SelectSide::left >());
            else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFiletime<false, SelectSide::right>());
            break;
        case ColumnTypeRim::extension:
            if      ( ascending &&  onLeft) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessExtension<true,  SelectSide::left >());
            else if ( ascending && !onLeft) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessExtension<true,  SelectSide::right>());
            else if (!ascending &&  onLeft) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessExtension<false, SelectSide::left >());
            else if (!ascending && !onLeft) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessExtension<false, SelectSide::right>());
            break;
    }
}


void FileView::sortView(ColumnTypeCenter type, bool ascending)
{
    viewRef_               .clear();
    groupDetails_          .clear();
    rowPositions_          .clear();
    rowPositionsFirstChild_.clear();
    currentSort_ = SortInfo({type, false, ascending});

    switch (type)
    {
        case ColumnTypeCenter::checkbox:
            assert(false);
            break;
        case ColumnTypeCenter::difference:
            if      ( ascending) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessCmpResult<true >());
            else if (!ascending) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessCmpResult<false>());
            break;
        case ColumnTypeCenter::action:
            if      ( ascending) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessSyncDirection<true >());
            else if (!ascending) std::stable_sort(sortedRef_.begin(), sortedRef_.end(), LessSyncDirection<false>());
            break;
    }
}

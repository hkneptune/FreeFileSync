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
    viewRef_               .clear();
    rowPositions_          .clear();
    rowPositionsFirstChild_.clear();
    pathDrawBlob_          .clear();

    std::vector<const PathInformation*> componentsBlob;
    std::vector<const ContainerObject*> parentsBuf; //from bottom to top of hierarchy

    for (const FileSystemObject::ObjectId& objId : sortedRef_)
        if (const FileSystemObject* const fsObj = FileSystemObject::retrieve(objId))
            if (pred(*fsObj))
            {
                //save row position for direct random access to FilePair or FolderPair
                rowPositions_.emplace(objId, viewRef_.size()); //costs: 0.28 µs per call - MSVC based on std::set
                //"this->" required by two-pass lookup as enforced by GCC 4.7

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
                    if (const auto [it, inserted] = this->rowPositionsFirstChild_.emplace(parent, viewRef_.size());
                        !inserted) //=> parents further up in hierarchy already inserted!
                        break;

                //------ prepare generation of tree render info ------
                componentsBlob.insert(componentsBlob.end(), parentsBuf.rbegin(), parentsBuf.rend());
                componentsBlob.push_back(fsObj);
                //----------------------------------------------------

                //save filtered view
                viewRef_.push_back({ objId, componentsBlob.size() });
            }

    //--------------- generate tree render info ------------------
    size_t startPosPrev = 0;
    size_t   endPosPrev = 0;

    for (auto itV = viewRef_.begin(); itV != viewRef_.end(); ++itV)
    {
        const size_t startPos = endPosPrev;
        const size_t endPos = itV->pathDrawEndPos;

        const std::span<const PathInformation*> componentsPrev(&componentsBlob[startPosPrev], endPosPrev - startPosPrev);
        const std::span<const PathInformation*> components    (&componentsBlob[startPos    ], endPos     - startPos);

        //find first mismatching component to draw
        assert(!components.empty());
        const auto& [it, itPrev] = std::mismatch(components    .begin(), components    .end() - 1 /*no need to check leaf component!*/,
                                                 componentsPrev.begin(), componentsPrev.end()); //but DO check previous row's leaf: might be a folder!
        const size_t iDraw = it - components.begin();

        pathDrawBlob_.resize(pathDrawBlob_.size() + iDraw);
        pathDrawBlob_.resize(pathDrawBlob_.size() + (components.size() - iDraw), PathDrawInfo::DRAW_COMPONENT);

        //connect with first of previous rows' component that is drawn
        if (iDraw != 0) //... not needed for base folder component
        {
            (pathDrawBlob_.end() - components.size())[iDraw] |= PathDrawInfo::CONNECT_PREV;

            assert(itV != viewRef_.begin()); //because iDraw != 0
            for (auto itV2 = itV - 1;; ) //iterate backwards
            {
                const size_t endPos2 = itV2->pathDrawEndPos;

                size_t startPos2 = 0;
                if (itV2 != viewRef_.begin())
                {
                    --itV2;
                    startPos2 = itV2->pathDrawEndPos;
                }
                const std::span<unsigned char> components2(&pathDrawBlob_[startPos2], endPos2 - startPos2);
                assert(iDraw <= components2.size());

                if (iDraw >= components2.size())
                    break; //parent folder!

                components2[iDraw] |= PathDrawInfo::CONNECT_NEXT;

                if (components2[iDraw] & PathDrawInfo::DRAW_COMPONENT)
                    break;

                components2[iDraw] |= PathDrawInfo::CONNECT_PREV;

                assert(startPos2 != 0); //all components of first raw are drawn => expect break!
            }
        }

        startPosPrev = startPos;
        endPosPrev   = endPos;
    }
    //------------------------------------------------------------
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
            if (FileSystemObject* fsObj = FileSystemObject::retrieve(viewRef_[pos].objId))
                output.push_back(fsObj);

    return output;
}


void FileView::removeInvalidRows()
{
    //remove rows that have been deleted meanwhile
    std::erase_if(sortedRef_, [&](const FileSystemObject::ObjectId& objId) { return !FileSystemObject::retrieve(objId); });

    viewRef_               .clear();
    rowPositions_          .clear();
    rowPositionsFirstChild_.clear();
    pathDrawBlob_          .clear();
}


void serializeHierarchy(ContainerObject& hierObj, std::vector<FileSystemObject::ObjectId>& output)
{
    for (FilePair& file : hierObj.refSubFiles())
        output.push_back(file.getId());

    for (SymlinkPair& symlink : hierObj.refSubLinks())
        output.push_back(symlink.getId());

    for (FolderPair& folder : hierObj.refSubFolders())
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


void FileView::setData(FolderComparison& folderCmp)
{
    //clear everything
    std::unordered_map<FileSystemObject::ObjectIdConst, size_t>().swap(rowPositions_);
    std::unordered_map<const void* /*ContainerObject*/, size_t>().swap(rowPositionsFirstChild_);
    std::vector<unsigned char>().swap(pathDrawBlob_);
    std::vector<ViewRow                   >().swap(viewRef_);   //+ free mem
    std::vector<FileSystemObject::ObjectId>().swap(sortedRef_); //
    folderPairs_.clear();
    currentSort_ = {};
    std::unordered_map<std::wstring, wxSize>().swap(compExtentsBuf_); //ensure buffer size does not get out of hand!

    std::for_each(begin(folderCmp), end(folderCmp), [&](BaseFolderPair& baseObj)
    {
        serializeHierarchy(baseObj, sortedRef_);

        folderPairs_.emplace_back(&baseObj,
                                  baseObj.getAbstractPath< LEFT_SIDE>(),
                                  baseObj.getAbstractPath<RIGHT_SIDE>());
    });
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


template <bool ascending, SelectedSide side> inline
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

    //sort directories and files/symlinks by short name
    return zen::makeSortDirection(LessNaturalSort() /*even on Linux*/, std::bool_constant<ascending>())(lhs.getItemName<side>(), rhs.getItemName<side>());
}


template <bool ascending>  inline //side currently unused!
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
            return zen::makeSortDirection(std::less<>(), std::bool_constant<ascending>())(basePosL, basePosR);
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

            return zen::makeSortDirection(LessNaturalSort(), std::bool_constant<ascending>())(fsObjL->getItemNameAny(), fsObjR->getItemNameAny());
        }
        else
            return true;
    }
    else if (itR == parentsR.rend())
        return false;
    else //different components...
    {
        if (const int rv = compareNatural((*itL)->getItemNameAny(), (*itR)->getItemNameAny());
            rv != 0)
            return zen::makeSortDirection(std::less<>(), std::bool_constant<ascending>())(rv, 0);

        /*...with equivalent names:
            1. functional correctness => must not compare equal!  e.g. a/a/x and a/A/y
            2. ensure stable sort order                                                            */
        return *itL < *itR;
    }
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
    LessFullPath(std::vector<std::tuple<const void* /*BaseFolderPair*/, AbstractPath, AbstractPath>> folderPairs)
    {
        //calculate positions of base folders sorted by name
        std::sort(folderPairs.begin(), folderPairs.end(), [](const auto& a, const auto& b)
        {
            const auto& [baseObjA, basePathLA, basePathRA] = a;
            const auto& [baseObjB, basePathLB, basePathRB] = b;

            const AbstractPath& basePathA = SelectParam<side>::ref(basePathLA, basePathRA);
            const AbstractPath& basePathB = SelectParam<side>::ref(basePathLB, basePathRB);

            return LessNaturalSort()/*even on Linux*/(zen::utfTo<Zstring>(AFS::getDisplayPath(basePathA)),
                                                      zen::utfTo<Zstring>(AFS::getDisplayPath(basePathB)));
        });

        size_t pos = 0;
        for (const auto& [baseObj, basePathL, basePathR] : folderPairs)
            sortedPos_.ref().emplace(baseObj, pos++);
    }

    bool operator()(const FileSystemObject::ObjectId& lhs, const FileSystemObject::ObjectId& rhs) const
    {
        return lessFilePath<ascending>(lhs, rhs, sortedPos_.ref(), tempBuf_);
    }

private:
    SharedRef<std::unordered_map<const void* /*BaseFolderPair*/, size_t /*position*/>> sortedPos_ = makeSharedRef<std::unordered_map<const void*, size_t>>();
    mutable std::vector<const FolderPair*> tempBuf_; //avoid repeated memory allocation in lessFilePath()
};


template <bool ascending>
struct FileView::LessRelativeFolder
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
        return lessFilePath<ascending>(lhs, rhs, sortedPos_.ref(), tempBuf_);
    }

private:
    SharedRef<std::unordered_map<const void* /*BaseFolderPair*/, size_t /*position*/>> sortedPos_ = makeSharedRef<std::unordered_map<const void*, size_t>>();
    mutable std::vector<const FolderPair*> tempBuf_; //avoid repeated memory allocation in lessFilePath()
};


template <bool ascending, SelectedSide side>
struct FileView::LessFileName
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


template <bool ascending, SelectedSide side>
struct FileView::LessFilesize
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


template <bool ascending, SelectedSide side>
struct FileView::LessFiletime
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


template <bool ascending, SelectedSide side>
struct FileView::LessExtension
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
struct FileView::LessCmpResult
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
struct FileView::LessSyncDirection
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

//-------------------------------------------------------------------------------------------------------

void FileView::sortView(ColumnTypeRim type, ItemPathFormat pathFmt, bool onLeft, bool ascending)
{
    viewRef_               .clear();
    rowPositions_          .clear();
    rowPositionsFirstChild_.clear();
    pathDrawBlob_          .clear();
    currentSort_ = SortInfo({ type, onLeft, ascending });

    switch (type)
    {
        case ColumnTypeRim::ITEM_PATH:
            switch (pathFmt)
            {
                case ItemPathFormat::FULL_PATH:
                    if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<true,   LEFT_SIDE>(folderPairs_));
                    else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<true,  RIGHT_SIDE>(folderPairs_));
                    else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<false,  LEFT_SIDE>(folderPairs_));
                    else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFullPath<false, RIGHT_SIDE>(folderPairs_));
                    break;

                case ItemPathFormat::RELATIVE_PATH:
                    if      ( ascending) std::sort(sortedRef_.begin(), sortedRef_.end(), LessRelativeFolder<true >(folderPairs_));
                    else if (!ascending) std::sort(sortedRef_.begin(), sortedRef_.end(), LessRelativeFolder<false>(folderPairs_));
                    break;

                case ItemPathFormat::ITEM_NAME:
                    if      ( ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFileName<true,   LEFT_SIDE>());
                    else if ( ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFileName<true,  RIGHT_SIDE>());
                    else if (!ascending &&  onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFileName<false,  LEFT_SIDE>());
                    else if (!ascending && !onLeft) std::sort(sortedRef_.begin(), sortedRef_.end(), LessFileName<false, RIGHT_SIDE>());
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
    pathDrawBlob_          .clear();
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

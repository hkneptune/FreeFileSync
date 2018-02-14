// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include <set>
#include "tree_grid.h"
#include <wx/settings.h>
#include <wx/menu.h>
#include <zen/i18n.h>
#include <zen/utf.h>
#include <zen/stl_tools.h>
#include <zen/format_unit.h>
#include <wx+/rtl.h>
#include <wx+/dc.h>
#include <wx+/context_menu.h>
#include <wx+/image_resources.h>
#include "../lib/icon_buffer.h"

using namespace zen;
using namespace fff;


namespace
{
const int WIDTH_PERCENTAGE_BAR = 60;
}


inline
void TreeView::compressNode(Container& cont) //remove single-element sub-trees -> gain clarity + usability (call *after* inclusion check!!!)
{
    if (cont.subDirs.empty()) //single files node
        cont.firstFileId = nullptr;

#if 0 //let's not go overboard: empty folders should not be condensed => used for file exclusion filter; user expects to see them
    if (cont.firstFileId == nullptr && //single dir node...
        cont.subDirs.size() == 1   && //
        cont.subDirs[0].firstFileId == nullptr && //...that is empty
        cont.subDirs[0].subDirs.empty())          //
        cont.subDirs.clear();
#endif
}


template <class Function> //(const FileSystemObject&) -> bool
void TreeView::extractVisibleSubtree(ContainerObject& hierObj,  //in
                                     TreeView::Container& cont, //out
                                     Function pred)
{
    auto getBytes = [](const FilePair& file) //MSVC screws up miserably if we put this lambda into std::for_each
    {
        ////give accumulated bytes the semantics of a sync preview!
        //if (file.isActive())
        //    switch (file.getSyncDir())
        //    {
        //        case SyncDirection::LEFT:
        //            return file.getFileSize<RIGHT_SIDE>();
        //        case SyncDirection::RIGHT:
        //            return file.getFileSize<LEFT_SIDE>();
        //        case SyncDirection::NONE:
        //            break;
        //    }

        //prefer file-browser semantics over sync preview (=> always show useful numbers, even for SyncDirection::NONE)
        //discussion: https://www.freefilesync.org/forum/viewtopic.php?t=1595
        return std::max(file.getFileSize<LEFT_SIDE>(), file.getFileSize<RIGHT_SIDE>());
    };

    cont.firstFileId = nullptr;
    for (FilePair& file : hierObj.refSubFiles())
        if (pred(file))
        {
            cont.bytesNet += getBytes(file);
            ++cont.itemCountNet;

            if (!cont.firstFileId)
                cont.firstFileId = file.getId();
        }

    for (SymlinkPair& symlink : hierObj.refSubLinks())
        if (pred(symlink))
        {
            ++cont.itemCountNet;

            if (!cont.firstFileId)
                cont.firstFileId = symlink.getId();
        }

    cont.bytesGross     += cont.bytesNet;
    cont.itemCountGross += cont.itemCountNet;

    cont.subDirs.reserve(hierObj.refSubFolders().size()); //avoid expensive reallocations!

    for (FolderPair& folder : hierObj.refSubFolders())
    {
        const bool included = pred(folder);

        cont.subDirs.emplace_back(); //
        auto& subDirCont = cont.subDirs.back();
        TreeView::extractVisibleSubtree(folder, subDirCont, pred);
        if (included)
            ++subDirCont.itemCountGross;

        cont.bytesGross     += subDirCont.bytesGross;
        cont.itemCountGross += subDirCont.itemCountGross;

        if (!included && !subDirCont.firstFileId && subDirCont.subDirs.empty())
            cont.subDirs.pop_back();
        else
        {
            subDirCont.objId = folder.getId();
            compressNode(subDirCont);
        }
    }
}


namespace
{
//generate nice percentage numbers which precisely sum up to 100
void calcPercentage(std::vector<std::pair<uint64_t, int*>>& workList)
{
    const uint64_t total = std::accumulate(workList.begin(), workList.end(), uint64_t(),
    [](uint64_t sum, const std::pair<uint64_t, int*>& pair) { return sum + pair.first; });

    if (total == 0U) //this case doesn't work with the error minimizing algorithm below
    {
        for (auto& pair : workList)
            *pair.second = 0;
        return;
    }

    int remainingPercent = 100;
    for (auto& pair : workList)
    {
        *pair.second = static_cast<int>(pair.first * 100U / total); //round down
        remainingPercent -= *pair.second;
    }
    assert(remainingPercent >= 0);
    assert(remainingPercent < static_cast<int>(workList.size()));

    //distribute remaining percent so that overall error is minimized as much as possible:
    remainingPercent = std::min(remainingPercent, static_cast<int>(workList.size()));
    if (remainingPercent > 0)
    {
        std::nth_element(workList.begin(), workList.begin() + remainingPercent - 1, workList.end(),
                         [total](const std::pair<uint64_t, int*>& lhs, const std::pair<uint64_t, int*>& rhs)
        {
            return lhs.first * 100U % total > rhs.first * 100U % total;
        });

        std::for_each(workList.begin(), workList.begin() + remainingPercent, [&](std::pair<uint64_t, int*>& pair) { ++*pair.second; });
    }
}
}


Zstring fff::getShortDisplayNameForFolderPair(const AbstractPath& itemPathL, const AbstractPath& itemPathR)
{
    Zstring commonTrail;
    AbstractPath tmpPathL = itemPathL;
    AbstractPath tmpPathR = itemPathR;
    for (;;)
    {
        Opt<AbstractPath> parentPathL = AFS::getParentFolderPath(tmpPathL);
        Opt<AbstractPath> parentPathR = AFS::getParentFolderPath(tmpPathR);
        if (!parentPathL || !parentPathR)
            break;

        const Zstring itemNameL = AFS::getItemName(tmpPathL);
        const Zstring itemNameR = AFS::getItemName(tmpPathR);
        if (!strEqual(itemNameL, itemNameR, CmpNaturalSort())) //let's compare case-insensitively even on Linux!
            break;

        tmpPathL = *parentPathL;
        tmpPathR = *parentPathR;

        commonTrail = AFS::appendPaths(itemNameL, commonTrail, FILE_NAME_SEPARATOR);
    }
    if (!commonTrail.empty())
        return commonTrail;

    auto getLastComponent = [](const AbstractPath& itemPath)
    {
        if (!AFS::getParentFolderPath(itemPath)) //= device root
            return utfTo<Zstring>(AFS::getDisplayPath(itemPath));
        return AFS::getItemName(itemPath);
    };

    if (AFS::isNullPath(itemPathL))
        return getLastComponent(itemPathR);
    else if (AFS::isNullPath(itemPathR))
        return getLastComponent(itemPathL);
    else
        return getLastComponent(itemPathL) + utfTo<Zstring>(SPACED_DASH) +
               getLastComponent(itemPathR);
}


template <bool ascending>
struct TreeView::LessShortName
{
    bool operator()(const TreeLine& lhs, const TreeLine& rhs) const
    {
        //files last (irrespective of sort direction)
        if (lhs.type == TreeView::TYPE_FILES)
            return false;
        else if (rhs.type == TreeView::TYPE_FILES)
            return true;

        if (lhs.type != rhs.type)       //
            return lhs.type < rhs.type; //shouldn't happen! root nodes not mixed with files or directories

        switch (lhs.type)
        {
            case TreeView::TYPE_ROOT:
                return makeSortDirection(LessNaturalSort() /*even on Linux*/, Int2Type<ascending>())(static_cast<const RootNodeImpl*>(lhs.node)->displayName,
                        static_cast<const RootNodeImpl*>(rhs.node)->displayName);

            case TreeView::TYPE_DIRECTORY:
            {
                const auto* folderL = dynamic_cast<const FolderPair*>(FileSystemObject::retrieve(static_cast<const DirNodeImpl*>(lhs.node)->objId));
                const auto* folderR = dynamic_cast<const FolderPair*>(FileSystemObject::retrieve(static_cast<const DirNodeImpl*>(rhs.node)->objId));

                if (!folderL)  //might be pathologic, but it's covered
                    return false;
                else if (!folderR)
                    return true;

                return makeSortDirection(LessNaturalSort() /*even on Linux*/, Int2Type<ascending>())(folderL->getPairItemName(), folderR->getPairItemName());
            }

            case TreeView::TYPE_FILES:
                break;
        }
        assert(false);
        return false; //:= all equal
    }
};


template <bool ascending>
void TreeView::sortSingleLevel(std::vector<TreeLine>& items, ColumnTypeTree columnType)
{
    auto getBytes = [](const TreeLine& line) -> uint64_t
    {
        switch (line.type)
        {
            case TreeView::TYPE_ROOT:
            case TreeView::TYPE_DIRECTORY:
                return line.node->bytesGross;
            case TreeView::TYPE_FILES:
                return line.node->bytesNet;
        }
        assert(false);
        return 0U;
    };

    auto getCount = [](const TreeLine& line) -> int
    {
        switch (line.type)
        {
            case TreeView::TYPE_ROOT:
            case TreeView::TYPE_DIRECTORY:
                return line.node->itemCountGross;

            case TreeView::TYPE_FILES:
                return line.node->itemCountNet;
        }
        assert(false);
        return 0;
    };

    const auto lessBytes = [&](const TreeLine& lhs, const TreeLine& rhs) { return getBytes(lhs) < getBytes(rhs); };
    const auto lessCount = [&](const TreeLine& lhs, const TreeLine& rhs) { return getCount(lhs) < getCount(rhs); };

    switch (columnType)
    {
        case ColumnTypeTree::FOLDER_NAME:
            std::sort(items.begin(), items.end(), LessShortName<ascending>());
            break;
        case ColumnTypeTree::ITEM_COUNT:
            std::sort(items.begin(), items.end(), makeSortDirection(lessCount, Int2Type<ascending>()));
            break;
        case ColumnTypeTree::BYTES:
            std::sort(items.begin(), items.end(), makeSortDirection(lessBytes, Int2Type<ascending>()));
            break;
    }
}


void TreeView::getChildren(const Container& cont, unsigned int level, std::vector<TreeLine>& output)
{
    output.clear();
    output.reserve(cont.subDirs.size() + 1); //keep pointers in "workList" valid
    std::vector<std::pair<uint64_t, int*>> workList;

    for (const DirNodeImpl& subDir : cont.subDirs)
    {
        output.push_back({ level, 0, &subDir, TreeView::TYPE_DIRECTORY });
        workList.emplace_back(subDir.bytesGross, &output.back().percent);
    }

    if (cont.firstFileId)
    {
        output.push_back({ level, 0, &cont, TreeView::TYPE_FILES });
        workList.emplace_back(cont.bytesNet, &output.back().percent);
    }
    calcPercentage(workList);

    if (sortAscending_)
        sortSingleLevel<true>(output, sortColumn_);
    else
        sortSingleLevel<false>(output, sortColumn_);
}


void TreeView::applySubView(std::vector<RootNodeImpl>&& newView)
{
    //preserve current node expansion status
    auto getHierAlias = [](const TreeView::TreeLine& tl) -> const ContainerObject*
    {
        switch (tl.type)
        {
            case TreeView::TYPE_ROOT:
                return static_cast<const RootNodeImpl*>(tl.node)->baseFolder.get();

            case TreeView::TYPE_DIRECTORY:
                if (auto folder = dynamic_cast<const FolderPair*>(FileSystemObject::retrieve(static_cast<const DirNodeImpl*>(tl.node)->objId)))
                    return folder;
                break;

            case TreeView::TYPE_FILES:
                break; //none!!!
        }
        return nullptr;
    };

    std::unordered_set<const ContainerObject*> expandedNodes;
    if (!flatTree_.empty())
    {
        auto it = flatTree_.begin();
        for (auto iterNext = flatTree_.begin() + 1; iterNext != flatTree_.end(); ++iterNext, ++it)
            if (it->level < iterNext->level)
                if (auto hierObj = getHierAlias(*it))
                    expandedNodes.insert(hierObj);
    }

    //update view on full data
    folderCmpView_.swap(newView); //newView may be an alias for folderCmpView! see sorting!

    //set default flat tree
    flatTree_.clear();

    if (folderCmp_.size() == 1) //single folder pair case (empty pairs were already removed!) do NOT use folderCmpView for this check!
    {
        if (!folderCmpView_.empty()) //possibly empty!
            getChildren(folderCmpView_[0], 0, flatTree_); //do not show root
    }
    else
    {
        //following is almost identical with TreeView::getChildren(): however we *cannot* reuse code here;
        //this were only possible if we replaced "std::vector<RootNodeImpl>" with "Container"!

        flatTree_.reserve(folderCmpView_.size()); //keep pointers in "workList" valid
        std::vector<std::pair<uint64_t, int*>> workList;

        for (const RootNodeImpl& root : folderCmpView_)
        {
            flatTree_.push_back({ 0, 0, &root, TreeView::TYPE_ROOT });
            workList.emplace_back(root.bytesGross, &flatTree_.back().percent);
        }

        calcPercentage(workList);

        if (sortAscending_)
            sortSingleLevel<true>(flatTree_, sortColumn_);
        else
            sortSingleLevel<false>(flatTree_, sortColumn_);
    }

    //restore node expansion status
    for (size_t row = 0; row < flatTree_.size(); ++row) //flatTree size changes during loop!
    {
        const TreeLine& line = flatTree_[row];

        if (auto hierObj = getHierAlias(line))
            if (expandedNodes.find(hierObj) != expandedNodes.end())
            {
                std::vector<TreeLine> newLines;
                getChildren(*line.node, line.level + 1, newLines);

                flatTree_.insert(flatTree_.begin() + row + 1, newLines.begin(), newLines.end());
            }
    }
}


template <class Predicate>
void TreeView::updateView(Predicate pred)
{
    //update view on full data
    std::vector<RootNodeImpl> newView;
    newView.reserve(folderCmp_.size()); //avoid expensive reallocations!

    for (const std::shared_ptr<BaseFolderPair>& baseObj : folderCmp_)
    {
        newView.emplace_back();
        RootNodeImpl& root = newView.back();
        this->extractVisibleSubtree(*baseObj, root, pred); //"this->" is bogus for a static method, but GCC screws this one up

        //warning: the following lines are almost 1:1 copy from extractVisibleSubtree:
        //however we *cannot* reuse code here; this were only possible if we replaced "std::vector<RootNodeImpl>" with "Container"!
        if (!root.firstFileId && root.subDirs.empty())
            newView.pop_back();
        else
        {
            root.baseFolder = baseObj;
            root.displayName = getShortDisplayNameForFolderPair(baseObj->getAbstractPath< LEFT_SIDE>(),
                                                                baseObj->getAbstractPath<RIGHT_SIDE>());

            this->compressNode(root); //"this->" required by two-pass lookup as enforced by GCC 4.7
        }
    }

    lastViewFilterPred_ = pred;
    applySubView(std::move(newView));
}


void TreeView::setSortDirection(ColumnTypeTree colType, bool ascending) //apply permanently!
{
    sortColumn_    = colType;
    sortAscending_ = ascending;

    //reapply current view
    applySubView(std::move(folderCmpView_));
}


TreeView::NodeStatus TreeView::getStatus(size_t row) const
{
    if (row < flatTree_.size())
    {
        if (row + 1 < flatTree_.size() && flatTree_[row + 1].level > flatTree_[row].level)
            return TreeView::STATUS_EXPANDED;

        //it's either reduced or empty
        switch (flatTree_[row].type)
        {
            case TreeView::TYPE_DIRECTORY:
            case TreeView::TYPE_ROOT:
                return flatTree_[row].node->firstFileId || !flatTree_[row].node->subDirs.empty() ? TreeView::STATUS_REDUCED : TreeView::STATUS_EMPTY;

            case TreeView::TYPE_FILES:
                return TreeView::STATUS_EMPTY;
        }
    }
    return TreeView::STATUS_EMPTY;
}


void TreeView::expandNode(size_t row)
{
    if (getStatus(row) != TreeView::STATUS_REDUCED)
    {
        assert(false);
        return;
    }

    if (row < flatTree_.size())
    {
        std::vector<TreeLine> newLines;

        switch (flatTree_[row].type)
        {
            case TreeView::TYPE_ROOT:
            case TreeView::TYPE_DIRECTORY:
                getChildren(*flatTree_[row].node, flatTree_[row].level + 1, newLines);
                break;
            case TreeView::TYPE_FILES:
                break;
        }
        flatTree_.insert(flatTree_.begin() + row + 1, newLines.begin(), newLines.end());
    }
}


void TreeView::reduceNode(size_t row)
{
    if (row < flatTree_.size())
    {
        const unsigned int parentLevel = flatTree_[row].level;

        bool done = false;
        flatTree_.erase(std::remove_if(flatTree_.begin() + row + 1, flatTree_.end(),
                                       [&](const TreeLine& line) -> bool
        {
            if (done)
                return false;
            if (line.level > parentLevel)
                return true;
            else
            {
                done = true;
                return false;
            }
        }), flatTree_.end());
    }
}


ptrdiff_t TreeView::getParent(size_t row) const
{
    if (row < flatTree_.size())
    {
        const auto level = flatTree_[row].level;

        while (row-- > 0)
            if (flatTree_[row].level < level)
                return row;
    }
    return -1;
}


void TreeView::updateCmpResult(bool showExcluded,
                               bool leftOnlyFilesActive,
                               bool rightOnlyFilesActive,
                               bool leftNewerFilesActive,
                               bool rightNewerFilesActive,
                               bool differentFilesActive,
                               bool equalFilesActive,
                               bool conflictFilesActive)
{
    updateView([showExcluded, //make sure the predicate can be stored safely!
                              leftOnlyFilesActive,
                              rightOnlyFilesActive,
                              leftNewerFilesActive,
                              rightNewerFilesActive,
                              differentFilesActive,
                              equalFilesActive,
                              conflictFilesActive](const FileSystemObject& fsObj) -> bool
    {
        if (!fsObj.isActive() && !showExcluded)
            return false;

        switch (fsObj.getCategory())
        {
            case FILE_LEFT_SIDE_ONLY:
                return leftOnlyFilesActive;
            case FILE_RIGHT_SIDE_ONLY:
                return rightOnlyFilesActive;
            case FILE_LEFT_NEWER:
                return leftNewerFilesActive;
            case FILE_RIGHT_NEWER:
                return rightNewerFilesActive;
            case FILE_DIFFERENT_CONTENT:
                return differentFilesActive;
            case FILE_EQUAL:
            case FILE_DIFFERENT_METADATA: //= sub-category of equal
                return equalFilesActive;
            case FILE_CONFLICT:
                return conflictFilesActive;
        }
        assert(false);
        return true;
    });
}


void TreeView::updateSyncPreview(bool showExcluded,
                                 bool syncCreateLeftActive,
                                 bool syncCreateRightActive,
                                 bool syncDeleteLeftActive,
                                 bool syncDeleteRightActive,
                                 bool syncDirOverwLeftActive,
                                 bool syncDirOverwRightActive,
                                 bool syncDirNoneActive,
                                 bool syncEqualActive,
                                 bool conflictFilesActive)
{
    updateView([showExcluded, //make sure the predicate can be stored safely!
                              syncCreateLeftActive,
                              syncCreateRightActive,
                              syncDeleteLeftActive,
                              syncDeleteRightActive,
                              syncDirOverwLeftActive,
                              syncDirOverwRightActive,
                              syncDirNoneActive,
                              syncEqualActive,
                              conflictFilesActive](const FileSystemObject& fsObj) -> bool
    {
        if (!fsObj.isActive() && !showExcluded)
            return false;

        switch (fsObj.getSyncOperation())
        {
            case SO_CREATE_NEW_LEFT:
                return syncCreateLeftActive;
            case SO_CREATE_NEW_RIGHT:
                return syncCreateRightActive;
            case SO_DELETE_LEFT:
                return syncDeleteLeftActive;
            case SO_DELETE_RIGHT:
                return syncDeleteRightActive;
            case SO_OVERWRITE_RIGHT:
            case SO_COPY_METADATA_TO_RIGHT:
            case SO_MOVE_RIGHT_FROM:
            case SO_MOVE_RIGHT_TO:
                return syncDirOverwRightActive;
            case SO_OVERWRITE_LEFT:
            case SO_COPY_METADATA_TO_LEFT:
            case SO_MOVE_LEFT_FROM:
            case SO_MOVE_LEFT_TO:
                return syncDirOverwLeftActive;
            case SO_DO_NOTHING:
                return syncDirNoneActive;
            case SO_EQUAL:
                return syncEqualActive;
            case SO_UNRESOLVED_CONFLICT:
                return conflictFilesActive;
        }
        assert(false);
        return true;
    });
}


void TreeView::setData(FolderComparison& newData)
{
    std::vector<TreeLine    >().swap(flatTree_);      //free mem
    std::vector<RootNodeImpl>().swap(folderCmpView_); //
    folderCmp_ = newData;

    //remove truly empty folder pairs as early as this: we want to distinguish single/multiple folder pair cases by looking at "folderCmp"
    erase_if(folderCmp_, [](const std::shared_ptr<BaseFolderPair>& baseObj)
    {
        return AFS::isNullPath(baseObj->getAbstractPath< LEFT_SIDE>()) &&
               AFS::isNullPath(baseObj->getAbstractPath<RIGHT_SIDE>());
    });
}


std::unique_ptr<TreeView::Node> TreeView::getLine(size_t row) const
{
    if (row < flatTree_.size())
    {
        const auto level  = flatTree_[row].level;
        const int percent = flatTree_[row].percent;

        switch (flatTree_[row].type)
        {
            case TreeView::TYPE_ROOT:
            {
                const auto& root = *static_cast<const TreeView::RootNodeImpl*>(flatTree_[row].node);
                return std::make_unique<TreeView::RootNode>(percent, root.bytesGross, root.itemCountGross, getStatus(row), *root.baseFolder, root.displayName);
            }
            break;

            case TreeView::TYPE_DIRECTORY:
            {
                const auto* dir = static_cast<const TreeView::DirNodeImpl*>(flatTree_[row].node);
                if (auto folder = dynamic_cast<FolderPair*>(FileSystemObject::retrieve(dir->objId)))
                    return std::make_unique<TreeView::DirNode>(percent, dir->bytesGross, dir->itemCountGross, level, getStatus(row), *folder);
            }
            break;

            case TreeView::TYPE_FILES:
            {
                const auto* parentDir = flatTree_[row].node;
                if (auto firstFile = FileSystemObject::retrieve(parentDir->firstFileId))
                {
                    std::vector<FileSystemObject*> filesAndLinks;
                    ContainerObject& parent = firstFile->parent();

                    //lazy evaluation: recheck "lastViewFilterPred" again rather than buffer and bloat "lastViewFilterPred"
                    for (FileSystemObject& fsObj : parent.refSubFiles())
                        if (lastViewFilterPred_(fsObj))
                            filesAndLinks.push_back(&fsObj);

                    for (FileSystemObject& fsObj : parent.refSubLinks())
                        if (lastViewFilterPred_(fsObj))
                            filesAndLinks.push_back(&fsObj);

                    return std::make_unique<TreeView::FilesNode>(percent, parentDir->bytesNet, parentDir->itemCountNet, level, filesAndLinks);
                }
            }
            break;
        }
    }
    return nullptr;
}

//##########################################################################################################

namespace
{
//let's NOT create wxWidgets objects statically:
inline wxColor getColorPercentBorder    () { return { 198, 198, 198 }; }
inline wxColor getColorPercentBackground() { return { 0xf8, 0xf8, 0xf8 }; }

inline wxColor getColorTreeSelectionGradientFrom() { return Grid::getColorSelectionGradientFrom(); }
inline wxColor getColorTreeSelectionGradientTo  () { return Grid::getColorSelectionGradientTo  (); }

const int iconSizeSmall = IconBuffer::getSize(IconBuffer::SIZE_SMALL);


wxColor getColorForLevel(size_t level)
{
    switch (level % 12)
    {
        case 0:
            return { 0xcc, 0xcc, 0xff };
        case 1:
            return { 0xcc, 0xff, 0xcc };
        case 2:
            return { 0xff, 0xff, 0x99 };
        case 3:
            return { 0xcc, 0xcc, 0xcc };
        case 4:
            return { 0xff, 0xcc, 0xff };
        case 5:
            return { 0x99, 0xff, 0xcc };
        case 6:
            return { 0xcc, 0xcc, 0x99 };
        case 7:
            return { 0xff, 0xcc, 0xcc };
        case 8:
            return { 0xcc, 0xff, 0x99 };
        case 9:
            return { 0xff, 0xff, 0xcc };
        case 10:
            return { 0xcc, 0xff, 0xff };
        case 11:
            return { 0xff, 0xcc, 0x99 };
    }
    assert(false);
    return *wxBLACK;
}


class GridDataTree : private wxEvtHandler, public GridData
{
public:
    GridDataTree(Grid& grid) :
        rootBmp_(getResourceImage(L"rootFolder").ConvertToImage().Scale(iconSizeSmall, iconSizeSmall, wxIMAGE_QUALITY_HIGH)),
        widthNodeIcon_(iconSizeSmall),
        widthLevelStep_(widthNodeIcon_),
        widthNodeStatus_(getResourceImage(L"node_expanded").GetWidth()),
        grid_(grid)
    {
        grid.getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(GridDataTree::onKeyDown), nullptr, this);
        grid.Connect(EVENT_GRID_MOUSE_LEFT_DOWN,       GridClickEventHandler     (GridDataTree::onMouseLeft         ), nullptr, this);
        grid.Connect(EVENT_GRID_MOUSE_LEFT_DOUBLE,     GridClickEventHandler     (GridDataTree::onMouseLeftDouble   ), nullptr, this);
        grid.Connect(EVENT_GRID_COL_LABEL_MOUSE_RIGHT, GridLabelClickEventHandler(GridDataTree::onGridLabelContext  ), nullptr, this);
        grid.Connect(EVENT_GRID_COL_LABEL_MOUSE_LEFT,  GridLabelClickEventHandler(GridDataTree::onGridLabelLeftClick), nullptr, this);
    }

    void setShowPercentage(bool value) { showPercentBar_ = value; grid_.Refresh(); }
    bool getShowPercentage() const { return showPercentBar_; }

    TreeView& getDataView() { return treeDataView_; }

private:
    size_t getRowCount() const override { return treeDataView_.linesTotal(); }

    std::wstring getToolTip(size_t row, ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeTree>(colType))
        {
            case ColumnTypeTree::FOLDER_NAME:
                if (std::unique_ptr<TreeView::Node> node = treeDataView_.getLine(row))
                    if (const TreeView::RootNode* root = dynamic_cast<const TreeView::RootNode*>(node.get()))
                    {
                        const std::wstring& dirLeft  = AFS::getDisplayPath(root->baseFolder_.getAbstractPath< LEFT_SIDE>());
                        const std::wstring& dirRight = AFS::getDisplayPath(root->baseFolder_.getAbstractPath<RIGHT_SIDE>());
                        if (dirLeft.empty())
                            return dirRight;
                        else if (dirRight.empty())
                            return dirLeft;
                        return dirLeft + L" \u2013"/*en dash*/ + L"\n" + dirRight;
                    }
                break;

            case ColumnTypeTree::ITEM_COUNT:
            case ColumnTypeTree::BYTES:
                break;
        }
        return std::wstring();
    }

    std::wstring getValue(size_t row, ColumnType colType) const override
    {
        if (std::unique_ptr<TreeView::Node> node = treeDataView_.getLine(row))
            switch (static_cast<ColumnTypeTree>(colType))
            {
                case ColumnTypeTree::FOLDER_NAME:
                    if (const TreeView::RootNode* root = dynamic_cast<const TreeView::RootNode*>(node.get()))
                        return utfTo<std::wstring>(root->displayName_);
                    else if (const TreeView::DirNode* dir = dynamic_cast<const TreeView::DirNode*>(node.get()))
                        return utfTo<std::wstring>(dir->folder_.getPairItemName());
                    else if (dynamic_cast<const TreeView::FilesNode*>(node.get()))
                        return _("Files");
                    break;

                case ColumnTypeTree::ITEM_COUNT:
                    return formatNumber(node->itemCount_);

                case ColumnTypeTree::BYTES:
                    return formatFilesizeShort(node->bytes_);
            }

        return std::wstring();
    }

    void renderColumnLabel(Grid& tree, wxDC& dc, const wxRect& rect, ColumnType colType, bool highlighted) override
    {
        wxRect rectInside = drawColumnLabelBorder(dc, rect);
        drawColumnLabelBackground(dc, rectInside, highlighted);

        rectInside.x     += COLUMN_GAP_LEFT;
        rectInside.width -= COLUMN_GAP_LEFT;
        drawColumnLabelText(dc, rectInside, getColumnLabel(colType));

        auto sortInfo = treeDataView_.getSortDirection();
        if (colType == static_cast<ColumnType>(sortInfo.first))
        {
            const wxBitmap& marker = getResourceImage(sortInfo.second ? L"sortAscending" : L"sortDescending");
            drawBitmapRtlNoMirror(dc, marker, rectInside, wxALIGN_CENTER_HORIZONTAL);
        }
    }

    static const int GAP_SIZE = 2;

    enum class HoverAreaTree
    {
        NODE,
    };

    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected) override
    {
        if (enabled)
        {
            if (selected)
                dc.GradientFillLinear(rect, getColorTreeSelectionGradientFrom(), getColorTreeSelectionGradientTo(), wxEAST);
            //ignore focus
            else
                clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        }
        else
            clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    }

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover) override
    {
        //wxRect rectTmp= drawCellBorder(dc, rect);
        wxRect rectTmp = rect;

        //  Partitioning:
        //   ________________________________________________________________________________
        //  | space | gap | percentage bar | 2 x gap | node status | gap |icon | gap | rest |
        //   --------------------------------------------------------------------------------
        // -> synchronize renderCell() <-> getBestSize() <-> getRowMouseHover()

        if (static_cast<ColumnTypeTree>(colType) == ColumnTypeTree::FOLDER_NAME)
        {
            if (std::unique_ptr<TreeView::Node> node = treeDataView_.getLine(row))
            {
                ////clear first secion:
                //clearArea(dc, wxRect(rect.GetTopLeft(), wxSize(
                //                         node->level_ * widthLevelStep_ + GAP_SIZE + //width
                //                         (showPercentBar ? WIDTH_PERCENTAGE_BAR + 2 * GAP_SIZE : 0) + //
                //                         widthNodeStatus_ + GAP_SIZE + widthNodeIcon + GAP_SIZE, //
                //                         rect.height)), wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

                //consume space
                rectTmp.x     += static_cast<int>(node->level_) * widthLevelStep_;
                rectTmp.width -= static_cast<int>(node->level_) * widthLevelStep_;

                rectTmp.x     += GAP_SIZE;
                rectTmp.width -= GAP_SIZE;

                if (rectTmp.width > 0)
                {
                    //percentage bar
                    if (showPercentBar_)
                    {

                        const wxRect areaPerc(rectTmp.x, rectTmp.y + 2, WIDTH_PERCENTAGE_BAR, rectTmp.height - 4);
                        {
                            //clear background
                            wxDCPenChanger   dummy (dc, getColorPercentBorder());
                            wxDCBrushChanger dummy2(dc, getColorPercentBackground());
                            dc.DrawRectangle(areaPerc);

                            //inner area
                            const wxColor brushCol = getColorForLevel(node->level_);
                            dc.SetPen  (brushCol);
                            dc.SetBrush(brushCol);

                            wxRect areaPercTmp = areaPerc;
                            areaPercTmp.Deflate(1); //do not include border
                            areaPercTmp.width = numeric::round(areaPercTmp.width * node->percent_ / 100.0);
                            dc.DrawRectangle(areaPercTmp);
                        }

                        wxDCTextColourChanger dummy3(dc, *wxBLACK); //accessibility: always set both foreground AND background colors!
                        drawCellText(dc, areaPerc, numberTo<std::wstring>(node->percent_) + L"%", wxALIGN_CENTER);

                        rectTmp.x     += WIDTH_PERCENTAGE_BAR + 2 * GAP_SIZE;
                        rectTmp.width -= WIDTH_PERCENTAGE_BAR + 2 * GAP_SIZE;
                    }
                    if (rectTmp.width > 0)
                    {
                        //node status
                        auto drawStatus = [&](const wchar_t* image)
                        {
                            const wxBitmap& bmp = getResourceImage(image);

                            wxRect rectStat(rectTmp.GetTopLeft(), wxSize(bmp.GetWidth(), bmp.GetHeight()));
                            rectStat.y += (rectTmp.height - rectStat.height) / 2;

                            //clearArea(dc, rectStat, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
                            clearArea(dc, rectStat, *wxWHITE); //accessibility: always set both foreground AND background colors!
                            drawBitmapRtlMirror(dc, bmp, rectStat, wxALIGN_CENTER, renderBuf_);
                        };

                        const bool drawMouseHover = static_cast<HoverAreaTree>(rowHover) == HoverAreaTree::NODE;
                        switch (node->status_)
                        {
                            case TreeView::STATUS_EXPANDED:
                                drawStatus(drawMouseHover ? L"node_expanded_hover" : L"node_expanded");
                                break;
                            case TreeView::STATUS_REDUCED:
                                drawStatus(drawMouseHover ? L"node_reduced_hover" : L"node_reduced");
                                break;
                            case TreeView::STATUS_EMPTY:
                                break;
                        }

                        rectTmp.x     += widthNodeStatus_ + GAP_SIZE;
                        rectTmp.width -= widthNodeStatus_ + GAP_SIZE;
                        if (rectTmp.width > 0)
                        {
                            wxBitmap nodeIcon;
                            bool isActive = true;
                            //icon
                            if (dynamic_cast<const TreeView::RootNode*>(node.get()))
                                nodeIcon = rootBmp_;
                            else if (auto dir = dynamic_cast<const TreeView::DirNode*>(node.get()))
                            {
                                nodeIcon = dirIcon_;
                                isActive = dir->folder_.isActive();
                            }
                            else if (dynamic_cast<const TreeView::FilesNode*>(node.get()))
                                nodeIcon = fileIcon_;

                            if (!isActive)
                                nodeIcon = wxBitmap(nodeIcon.ConvertToImage().ConvertToGreyscale(1.0 / 3, 1.0 / 3, 1.0 / 3)); //treat all channels equally!

                            drawBitmapRtlNoMirror(dc, nodeIcon, rectTmp, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

                            rectTmp.x     += widthNodeIcon_ + GAP_SIZE;
                            rectTmp.width -= widthNodeIcon_ + GAP_SIZE;

                            if (rectTmp.width > 0)
                            {
                                wxDCTextColourChanger dummy(dc);
                                if (!isActive)
                                    dummy.Set(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));

                                drawCellText(dc, rectTmp, getValue(row, colType), wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
                            }
                        }
                    }
                }
            }
        }
        else
        {
            int alignment = wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL;

            //have file size and item count right-justified (but don't change for RTL languages)
            if ((static_cast<ColumnTypeTree>(colType) == ColumnTypeTree::BYTES ||
                 static_cast<ColumnTypeTree>(colType) == ColumnTypeTree::ITEM_COUNT) && grid_.GetLayoutDirection() != wxLayout_RightToLeft)
            {
                rectTmp.width -= 2 * GAP_SIZE;
                alignment = wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL;
            }
            else //left-justified
            {
                rectTmp.x     += 2 * GAP_SIZE;
                rectTmp.width -= 2 * GAP_SIZE;
            }

            drawCellText(dc, rectTmp, getValue(row, colType), alignment);
        }
    }

    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        // -> synchronize renderCell() <-> getBestSize() <-> getRowMouseHover()

        if (static_cast<ColumnTypeTree>(colType) == ColumnTypeTree::FOLDER_NAME)
        {
            if (std::unique_ptr<TreeView::Node> node = treeDataView_.getLine(row))
                return node->level_ * widthLevelStep_ + GAP_SIZE + (showPercentBar_ ? WIDTH_PERCENTAGE_BAR + 2 * GAP_SIZE : 0) + widthNodeStatus_ + GAP_SIZE
                       + widthNodeIcon_ + GAP_SIZE + dc.GetTextExtent(getValue(row, colType)).GetWidth() +
                       GAP_SIZE; //additional gap from right
            else
                return 0;
        }
        else
            return 2 * GAP_SIZE + dc.GetTextExtent(getValue(row, colType)).GetWidth() +
                   2 * GAP_SIZE; //include gap from right!
    }

    HoverArea getRowMouseHover(size_t row, ColumnType colType, int cellRelativePosX, int cellWidth) override
    {
        switch (static_cast<ColumnTypeTree>(colType))
        {
            case ColumnTypeTree::FOLDER_NAME:
                if (std::unique_ptr<TreeView::Node> node = treeDataView_.getLine(row))
                {
                    const int tolerance = 2;
                    const int nodeStatusXFirst = -tolerance + static_cast<int>(node->level_) * widthLevelStep_ + GAP_SIZE + (showPercentBar_ ? WIDTH_PERCENTAGE_BAR + 2 * GAP_SIZE : 0);
                    const int nodeStatusXLast  = (nodeStatusXFirst + tolerance) + widthNodeStatus_ + tolerance;
                    // -> synchronize renderCell() <-> getBestSize() <-> getRowMouseHover()

                    if (nodeStatusXFirst <= cellRelativePosX && cellRelativePosX < nodeStatusXLast)
                        return static_cast<HoverArea>(HoverAreaTree::NODE);
                }
                break;

            case ColumnTypeTree::ITEM_COUNT:
            case ColumnTypeTree::BYTES:
                break;
        }
        return HoverArea::NONE;
    }

    std::wstring getColumnLabel(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeTree>(colType))
        {
            case ColumnTypeTree::FOLDER_NAME:
                return _("Name");
            case ColumnTypeTree::ITEM_COUNT:
                return _("Items");
            case ColumnTypeTree::BYTES:
                return _("Size");
        }
        return std::wstring();
    }

    void onMouseLeft(GridClickEvent& event)
    {
        switch (static_cast<HoverAreaTree>(event.hoverArea_))
        {
            case HoverAreaTree::NODE:
                switch (treeDataView_.getStatus(event.row_))
                {
                    case TreeView::STATUS_EXPANDED:
                        return reduceNode(event.row_);
                    case TreeView::STATUS_REDUCED:
                        return expandNode(event.row_);
                    case TreeView::STATUS_EMPTY:
                        break;
                }
                break;
        }
        event.Skip();
    }

    void onMouseLeftDouble(GridClickEvent& event)
    {
        switch (treeDataView_.getStatus(event.row_))
        {
            case TreeView::STATUS_EXPANDED:
                return reduceNode(event.row_);
            case TreeView::STATUS_REDUCED:
                return expandNode(event.row_);
            case TreeView::STATUS_EMPTY:
                break;
        }
        event.Skip();
    }

    void onKeyDown(wxKeyEvent& event)
    {
        int keyCode = event.GetKeyCode();
        if (wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft)
        {
            if (keyCode == WXK_LEFT || keyCode == WXK_NUMPAD_LEFT)
                keyCode = WXK_RIGHT;
            else if (keyCode == WXK_RIGHT || keyCode == WXK_NUMPAD_RIGHT)
                keyCode = WXK_LEFT;
        }

        const size_t rowCount = grid_.getRowCount();
        if (rowCount == 0) return;

        const size_t row = grid_.getGridCursor();
        if (event.ShiftDown())
            ;
        else if (event.ControlDown())
            ;
        else
            switch (keyCode)
            {
                case WXK_LEFT:
                case WXK_NUMPAD_LEFT:
                case WXK_NUMPAD_SUBTRACT: //https://msdn.microsoft.com/en-us/library/ms971323#atg_keyboardshortcuts_windows_shortcut_keys
                    switch (treeDataView_.getStatus(row))
                    {
                        case TreeView::STATUS_EXPANDED:
                            return reduceNode(row);
                        case TreeView::STATUS_REDUCED:
                        case TreeView::STATUS_EMPTY:

                            const int parentRow = treeDataView_.getParent(row);
                            if (parentRow >= 0)
                                grid_.setGridCursor(parentRow);
                            break;
                    }
                    return; //swallow event

                case WXK_RIGHT:
                case WXK_NUMPAD_RIGHT:
                case WXK_NUMPAD_ADD:
                    switch (treeDataView_.getStatus(row))
                    {
                        case TreeView::STATUS_EXPANDED:
                            grid_.setGridCursor(std::min(rowCount - 1, row + 1));
                            break;
                        case TreeView::STATUS_REDUCED:
                            return expandNode(row);
                        case TreeView::STATUS_EMPTY:
                            break;
                    }
                    return; //swallow event
            }

        event.Skip();
    }

    void onGridLabelContext(GridLabelClickEvent& event)
    {
        ContextMenu menu;
        //--------------------------------------------------------------------------------------------------------
        menu.addCheckBox(_("Percentage"), [this] { setShowPercentage(!getShowPercentage()); }, getShowPercentage());
        //--------------------------------------------------------------------------------------------------------
        auto toggleColumn = [&](ColumnType ct)
        {
            auto colAttr = grid_.getColumnConfig();

            Grid::ColAttributes* caFolderName = nullptr;
            Grid::ColAttributes* caToggle     = nullptr;

            for (Grid::ColAttributes& ca : colAttr)
                if (ca.type == static_cast<ColumnType>(ColumnTypeTree::FOLDER_NAME))
                    caFolderName = &ca;
                else if (ca.type == ct)
                    caToggle = &ca;

            assert(caFolderName && caFolderName->stretch > 0 && caFolderName->visible);
            assert(caToggle     && caToggle->stretch == 0);

            if (caFolderName && caToggle)
            {
                caToggle->visible = !caToggle->visible;

                //take width of newly visible column from stretched folder name column
                caFolderName->offset -= caToggle->visible ? caToggle->offset : -caToggle->offset;

                grid_.setColumnConfig(colAttr);
            }
        };

        for (const Grid::ColAttributes& ca : grid_.getColumnConfig())
        {
            menu.addCheckBox(getColumnLabel(ca.type), [ct = ca.type, toggleColumn] { toggleColumn(ct); },
                             ca.visible, ca.type != static_cast<ColumnType>(ColumnTypeTree::FOLDER_NAME)); //do not allow user to hide file name column!
        }
        //--------------------------------------------------------------------------------------------------------
        menu.addSeparator();

        auto setDefaultColumns = [&]
        {
            setShowPercentage(treeGridShowPercentageDefault);
            grid_.setColumnConfig(convertColAttributes(getTreeGridDefaultColAttribs(), getTreeGridDefaultColAttribs()));
        };
        menu.addItem(_("&Default"), setDefaultColumns); //'&' -> reuse text from "default" buttons elsewhere
        //--------------------------------------------------------------------------------------------------------

        menu.popup(grid_);
        //event.Skip();
    }

    void onGridLabelLeftClick(GridLabelClickEvent& event)
    {
        const auto colTypeTree = static_cast<ColumnTypeTree>(event.colType_);
        bool sortAscending = getDefaultSortDirection(colTypeTree);

        const auto sortInfo = treeDataView_.getSortDirection();
        if (sortInfo.first == colTypeTree)
            sortAscending = !sortInfo.second;

        treeDataView_.setSortDirection(colTypeTree, sortAscending);
        grid_.clearSelection(ALLOW_GRID_EVENT);
        grid_.Refresh();
    }

    void expandNode(size_t row)
    {
        treeDataView_.expandNode(row);
        grid_.Refresh(); //implicitly clears selection (changed row count after expand)
        grid_.setGridCursor(row);
        //grid_.autoSizeColumns(); -> doesn't look as good as expected
    }

    void reduceNode(size_t row)
    {
        treeDataView_.reduceNode(row);
        grid_.Refresh();
        grid_.setGridCursor(row);
    }

    TreeView treeDataView_;

    const wxBitmap fileIcon_ = IconBuffer::genericFileIcon(IconBuffer::SIZE_SMALL);
    const wxBitmap dirIcon_  = IconBuffer::genericDirIcon (IconBuffer::SIZE_SMALL);

    const wxBitmap rootBmp_;
    Opt<wxBitmap> renderBuf_; //avoid costs of recreating this temporary variable
    const int widthNodeIcon_;
    const int widthLevelStep_;
    const int widthNodeStatus_;
    Grid& grid_;
    bool showPercentBar_ = true;
};
}


void treegrid::init(Grid& grid)
{
    grid.setDataProvider(std::make_shared<GridDataTree>(grid));
    grid.showRowLabel(false);

    const int rowHeight = std::max(IconBuffer::getSize(IconBuffer::SIZE_SMALL), grid.getMainWin().GetCharHeight()) + 2; //allow 1 pixel space on top and bottom; dearly needed on OS X!
    grid.setRowHeight(rowHeight);
}


TreeView& treegrid::getDataView(Grid& grid)
{
    if (auto* prov = dynamic_cast<GridDataTree*>(grid.getDataProvider()))
        return prov->getDataView();

    throw std::runtime_error("treegrid was not initialized! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
}


void treegrid::setShowPercentage(Grid& grid, bool value)
{
    if (auto* prov = dynamic_cast<GridDataTree*>(grid.getDataProvider()))
        prov->setShowPercentage(value);
    else
        assert(false);
}


bool treegrid::getShowPercentage(const Grid& grid)
{
    if (auto* prov = dynamic_cast<const GridDataTree*>(grid.getDataProvider()))
        return prov->getShowPercentage();
    assert(false);
    return true;
}

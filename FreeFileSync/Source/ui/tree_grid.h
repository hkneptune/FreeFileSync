// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TREE_VIEW_H_841703190201835280256673425
#define TREE_VIEW_H_841703190201835280256673425

#include <functional>
#include <wx+/grid.h>
#include "tree_grid_attr.h"
#include "../base/file_hierarchy.h"


namespace fff
{
//tree view of FolderComparison
class TreeView
{
public:
    struct SortInfo
    {
        ColumnTypeOverview sortCol = overviewPanelLastSortColumnDefault;
        bool ascending = getDefaultSortDirection(overviewPanelLastSortColumnDefault);
    };

    TreeView() {}
    TreeView(FolderComparison& folderCmp, const SortInfo& si);

    //apply view filter: comparison results
    void applyDifferenceFilter(bool showExcluded,
                               bool leftOnlyFilesActive,
                               bool rightOnlyFilesActive,
                               bool leftNewerFilesActive,
                               bool rightNewerFilesActive,
                               bool differentFilesActive,
                               bool equalFilesActive,
                               bool conflictFilesActive);

    //apply view filter: synchronization preview
    void applyActionFilter(bool showExcluded,
                           bool syncCreateLeftActive,
                           bool syncCreateRightActive,
                           bool syncDeleteLeftActive,
                           bool syncDeleteRightActive,
                           bool syncDirOverwLeftActive,
                           bool syncDirOverwRightActive,
                           bool syncDirNoneActive,
                           bool syncEqualActive,
                           bool conflictFilesActive);

    enum class NodeStatus
    {
        expanded,
        reduced,
        empty
    };

    //---------------------------------------------------------------------
    struct Node
    {
        Node(int percent, uint64_t bytes, int itemCount, unsigned int level, NodeStatus status) :
            percent_(percent), bytes_(bytes), itemCount_(itemCount), level_(level), status_(status) {}
        virtual ~Node() {}

        const int percent_; //[0, 100]
        const uint64_t bytes_;
        const int itemCount_;
        const unsigned int level_;
        const NodeStatus status_;
    };

    struct FilesNode : public Node
    {
        FilesNode(int percent, uint64_t bytes, int itemCount, unsigned int level, std::vector<FileSystemObject*>&& fsos) :
            Node(percent, bytes, itemCount, level, NodeStatus::empty), filesAndLinks(std::move(fsos))  {}

        std::vector<FileSystemObject*> filesAndLinks; //files and symlinks matching view filter; pointers are bound!
    };

    struct DirNode : public Node
    {
        DirNode(int percent, uint64_t bytes, int itemCount, unsigned int level, NodeStatus status, FolderPair& fp) : Node(percent, bytes, itemCount, level, status), folder(fp) {}
        FolderPair& folder;
    };

    struct RootNode : public Node
    {
        RootNode(int percent, uint64_t bytes, int itemCount, NodeStatus status, BaseFolderPair& bFolder, const std::wstring& dispName) :
            Node(percent, bytes, itemCount, 0, status), baseFolder(bFolder), displayName(dispName) {}

        BaseFolderPair& baseFolder;
        const std::wstring displayName;
    };

    std::unique_ptr<Node> getLine(size_t row) const; //return nullptr on error
    size_t rowsTotal() const { return flatTree_.size(); }

    void expandNode(size_t row);
    void reduceNode(size_t row);
    NodeStatus getStatus(size_t row) const;
    ptrdiff_t getParent(size_t row) const; //return < 0 if none

    void setSortDirection(ColumnTypeOverview colType, bool ascending); //apply permanently!
    SortInfo getSortConfig() { return currentSort_; }

private:
    TreeView           (const TreeView&) = delete;
    TreeView& operator=(const TreeView&) = delete;

    struct Container
    {
        uint64_t bytesGross = 0;
        uint64_t bytesNet   = 0; //bytes for files on view in this directory only
        int itemCountGross  = 0;
        int itemCountNet    = 0; //number of files on view in this directory only

        std::vector<Container> subDirs;
        bool showFilesNode = false; //"compress" algorithm may hide file nodes for directories with a single included file, i.e. itemCountGross == itemCountNet == 1
        std::weak_ptr<ContainerObject> containerRef; //-> BaseFolderPair if NodeType::root,
        //FolderPair if NodeType::folder, and parent ContainerObject if NodeType::files
    };

    struct RootNodeImpl : public Container
    {
        std::wstring displayName;
    };

    enum class NodeType
    {
        root,   //-> RootNodeImpl
        folder, //-> Container
        files   //-> Container
    };

    struct TreeLine
    {
        unsigned int level = 0;
        int percent = 0; //[0, 100]
        const Container* node = nullptr; //
        NodeType type = NodeType::root;  //increase size of "flatTree" using C-style types rather than have a polymorphic "folderCmpView"
    };

    static void compressNode(Container& cont);
    template <class Function>
    static void extractVisibleSubtree(ContainerObject& conObj, Container& cont, Function includeObject);
    void getChildren(const Container& cont, unsigned int level, std::vector<TreeLine>& output);
    template <class Predicate> void updateView(Predicate pred);
    void applySubView(std::vector<RootNodeImpl>&& newView);

    template <bool ascending> static void sortSingleLevel(std::vector<TreeLine>& items, ColumnTypeOverview columnType);
    template <bool ascending> struct LessShortName;

    std::vector<TreeLine> flatTree_; //collapsable/expandable sub-tree of folderCmpView -> always sorted!
    /*             /|\
                    | (update...)             */
    std::vector<RootNodeImpl> folderCmpView_; //partial view on folderCmp -> unsorted (cannot be, because files are not a separate entity)
    std::function<bool(const FileSystemObject& fsObj)> lastViewFilterPred_; //buffer view filter predicate for lazy evaluation of files/symlinks corresponding to a TYPE_FILES node
    /*             /|\
                    | (update...)             */
    std::vector<std::weak_ptr<BaseFolderPair>> folderCmp_; //full raw data

    SortInfo currentSort_;
};


namespace treegrid
{
void init(zen::Grid& grid);
TreeView& getDataView(zen::Grid& grid);
void setData(zen::Grid& grid, FolderComparison& folderCmp);

void setShowPercentage(zen::Grid& grid, bool value);
bool getShowPercentage(const zen::Grid& grid);
}
}

#endif //TREE_VIEW_H_841703190201835280256673425

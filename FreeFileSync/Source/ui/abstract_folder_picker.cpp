// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "abstract_folder_picker.h"
#include <wx+/async_task.h>
#include <wx+/std_button_layout.h>
#include <wx+/image_resources.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_tools.h>
#include "gui_generated.h"
#include "../base/icon_buffer.h"

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


namespace
{
enum class NodeLoadStatus
{
    notLoaded,
    loading,
    loaded
};

struct AfsTreeItemData : public wxTreeItemData
{
    AfsTreeItemData(const AbstractPath& path) : folderPath(path) {}

    const AbstractPath folderPath;
    std::wstring errorMsg; //optional
    NodeLoadStatus loadStatus = NodeLoadStatus::notLoaded;
    std::vector<std::function<void()>> onLoadCompleted; //bound!
};


wxString getNodeDisplayName(const AbstractPath& folderPath)
{
    if (!AFS::getParentPath(folderPath)) //server root
        return utfTo<wxString>(FILE_NAME_SEPARATOR);

    return utfTo<wxString>(AFS::getItemName(folderPath));
}


class AbstractFolderPickerDlg : public AbstractFolderPickerGenerated
{
public:
    AbstractFolderPickerDlg(wxWindow* parent, AbstractPath& folderPath);

private:
    void OnOkay  (wxCommandEvent& event) override;
    void OnCancel(wxCommandEvent& event) override { EndModal(ReturnAfsPicker::BUTTON_CANCEL); }
    void OnClose (wxCloseEvent&   event) override { EndModal(ReturnAfsPicker::BUTTON_CANCEL); }

    void OnKeyPressed(wxKeyEvent& event);
    void OnExpandNode(wxTreeEvent& event) override;
    void OnItemTooltip(wxTreeEvent& event);

    void populateNodeThen(const wxTreeItemId& itemId, const std::function<void()>& evalOnGui /*optional*/, bool popupErrors);

    void findAndNavigateToExistingPath(const AbstractPath& folderPath);
    void        navigateToExistingPath(const wxTreeItemId& itemId, const std::vector<Zstring>& nodeRelPath, AFS::ItemType leafType);

    enum class TreeNodeImage
    {
        root = 0, //used as zero-based wxImageList index!
        folder,
        folderSymlink,
        error
    };

    AsyncGuiQueue guiQueue_{ 25 /*polling [ms]*/ }; //schedule and run long-running tasks asynchronously, but process results on GUI queue

    //output-only parameters:
    AbstractPath& folderPathOut_;
};


AbstractFolderPickerDlg::AbstractFolderPickerDlg(wxWindow* parent, AbstractPath& folderPath) :
    AbstractFolderPickerGenerated(parent),
    folderPathOut_(folderPath)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    m_staticTextStatus->SetLabel(L"");
    m_treeCtrlFileSystem->SetMinSize(wxSize(fastFromDIP(350), fastFromDIP(400)));

    const int iconSize = IconBuffer::getSize(IconBuffer::SIZE_SMALL);
    auto imgList = std::make_unique<wxImageList>(iconSize, iconSize);

    //add images in same sequence like TreeNodeImage enum!!!
    imgList->Add(shrinkImage(getResourceImage(L"server").ConvertToImage(), iconSize));
    imgList->Add(        IconBuffer::genericDirIcon(IconBuffer::SIZE_SMALL));
    imgList->Add(layOver(IconBuffer::genericDirIcon(IconBuffer::SIZE_SMALL), IconBuffer::linkOverlayIcon(IconBuffer::SIZE_SMALL)));
    imgList->Add(shrinkImage(getResourceImage(L"msg_error").ConvertToImage(), iconSize));
    assert(imgList->GetImageCount() == static_cast<int>(TreeNodeImage::error) + 1);

    m_treeCtrlFileSystem->AssignImageList(imgList.release()); //pass ownership

    const AbstractPath rootPath(folderPath.afsDevice, AfsPath());

    const wxTreeItemId rootId = m_treeCtrlFileSystem->AddRoot(getNodeDisplayName(rootPath), static_cast<int>(TreeNodeImage::root), -1,
                                                              new AfsTreeItemData(rootPath));
    m_treeCtrlFileSystem->SetItemHasChildren(rootId);

    if (!AFS::getParentPath(folderPath)) //server root
        populateNodeThen(rootId, [this, rootId] { m_treeCtrlFileSystem->Expand(rootId); }, true /*popupErrors*/);
    else
        try //folder picker has dual responsibility:
        {
            //1. test server connection:
            const AFS::ItemType type = AFS::getItemType(folderPath); //throw FileError
            //2. navigate + select path
            navigateToExistingPath(rootId, split(folderPath.afsPath.value, FILE_NAME_SEPARATOR, SplitType::SKIP_EMPTY), type);
        }
        catch (const FileError& e) //not existing or access error
        {
            findAndNavigateToExistingPath(*AFS::getParentPath(folderPath)); //let's run async while the error message is shown :)

            showNotificationDialog(parent /*"this" not yet shown!*/, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString()));
        }

    //----------------------------------------------------------------------
    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    Connect(wxEVT_CHAR_HOOK,            wxKeyEventHandler (AbstractFolderPickerDlg::OnKeyPressed),  nullptr, this); //dialog-specific local key events
    Connect(wxEVT_TREE_ITEM_GETTOOLTIP, wxTreeEventHandler(AbstractFolderPickerDlg::OnItemTooltip), nullptr, this);

    m_treeCtrlFileSystem->SetFocus();
}


void AbstractFolderPickerDlg::OnKeyPressed(wxKeyEvent& event)
{
    switch (event.GetKeyCode())
    {
        //wxTreeCtrl seems to eat up ENTER without adding any functionality; we can do better:
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
        {
            wxCommandEvent dummy(wxEVT_COMMAND_BUTTON_CLICKED);
            OnOkay(dummy);
            return;
        }
    }
    event.Skip();
}


struct FlatTraverserCallback : public AFS::TraverserCallback
{
    struct Result
    {
        std::map<Zstring, bool /*is symlink*/> folderNames;
        std::wstring errorMsg;
    };

    const Result& getResult() { return result_; }

private:
    void                               onFile   (const AFS::FileInfo&    fi) override {}
    std::shared_ptr<TraverserCallback> onFolder (const AFS::FolderInfo&  fi) override { result_.folderNames.emplace(fi.itemName, fi.symlinkInfo != nullptr); return nullptr; }
    HandleLink                         onSymlink(const AFS::SymlinkInfo& si) override { return LINK_FOLLOW; }
    HandleError reportDirError (const std::wstring& msg, size_t retryNumber)                          override { logError(msg); return ON_ERROR_CONTINUE; }
    HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName) override { logError(msg); return ON_ERROR_CONTINUE; }

    void logError(const std::wstring& msg)
    {
        if (result_.errorMsg.empty())
            result_.errorMsg = msg;
    }

    Result result_;
};


void AbstractFolderPickerDlg::populateNodeThen(const wxTreeItemId& itemId, const std::function<void()>& evalOnGui, bool popupErrors)
{
    if (auto itemData = dynamic_cast<AfsTreeItemData*>(m_treeCtrlFileSystem->GetItemData(itemId)))
    {
        switch (itemData->loadStatus)
        {
            case NodeLoadStatus::notLoaded:
            {
                if (evalOnGui)
                    itemData->onLoadCompleted.push_back(evalOnGui);

                itemData->loadStatus = NodeLoadStatus::loading;

                m_treeCtrlFileSystem->SetItemText(itemId, getNodeDisplayName(itemData->folderPath) +  L" (" + _("Loading...") + L")");

                guiQueue_.processAsync([folderPath = itemData->folderPath] //AbstractPath is thread-safe like an int!
                {
                    auto ft = std::make_shared<FlatTraverserCallback>(); //noexcept, traverse directory one level deep
                    AFS::traverseFolderRecursive(folderPath.afsDevice, {{ folderPath.afsPath, ft }}, 1 /*parallelOps*/);
                    return ft->getResult();
                },

                [this, itemId, popupErrors](const FlatTraverserCallback::Result& result)
                {
                    if (auto itemData2 = dynamic_cast<AfsTreeItemData*>(m_treeCtrlFileSystem->GetItemData(itemId)))
                    {
                        m_treeCtrlFileSystem->SetItemText(itemId, getNodeDisplayName(itemData2->folderPath)); //remove "loading" phrase

                        if (result.folderNames.empty())
                            m_treeCtrlFileSystem->SetItemHasChildren(itemId, false);
                        else
                        {
                            //let's not use the wxTreeCtrl::OnCompareItems() abomination to implement sorting:
                            std::vector<std::pair<Zstring, bool /*is symlink*/>> folderNamesSorted(result.folderNames.begin(), result.folderNames.end());
                            std::sort(folderNamesSorted.begin(), folderNamesSorted.end(), [](const auto& lhs, const auto& rhs) { return LessNaturalSort()(lhs.first, rhs.first); });

                            for (const auto& [childName, isSymlink] : folderNamesSorted)
                            {
                                const AbstractPath childFolderPath = AFS::appendRelPath(itemData2->folderPath, childName);

                                wxTreeItemId childId = m_treeCtrlFileSystem->AppendItem(itemId, getNodeDisplayName(childFolderPath),
                                                                                        static_cast<int>(isSymlink ? TreeNodeImage::folderSymlink : TreeNodeImage::folder), -1,
                                                                                        new AfsTreeItemData(childFolderPath));
                                m_treeCtrlFileSystem->SetItemHasChildren(childId);
                            }
                        }

                        if (!result.errorMsg.empty())
                        {
                            m_treeCtrlFileSystem->SetItemImage(itemId, static_cast<int>(TreeNodeImage::error));
                            itemData2->errorMsg = result.errorMsg;

                            if (popupErrors)
                                showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(result.errorMsg));
                        }

                        itemData2->loadStatus = NodeLoadStatus::loaded; //set status *before* running callbacks
                        for (const auto& evalOnGui2 : itemData2->onLoadCompleted)
                            evalOnGui2();
                    }
                });
            }
            break;

            case NodeLoadStatus::loading:
                if (evalOnGui) itemData->onLoadCompleted.push_back(evalOnGui);
                break;

            case NodeLoadStatus::loaded:
                if (evalOnGui) evalOnGui();
                break;
        }
    }
}


//1. find longest existing/accessible (parent) path
void AbstractFolderPickerDlg::findAndNavigateToExistingPath(const AbstractPath& folderPath)
{
    if (!AFS::getParentPath(folderPath))
        return m_staticTextStatus->SetLabel(L"");

    m_staticTextStatus->SetLabel(_("Scanning...") + L" " + utfTo<std::wstring>(FILE_NAME_SEPARATOR + folderPath.afsPath.value)); //keep it short!

    guiQueue_.processAsync([folderPath]() -> std::optional<AFS::ItemType>
    {
        try
        {
            return AFS::getItemType(folderPath); //throw FileError
        }
        catch (FileError&) { return std::nullopt; } //not existing or access error
    },

    [this, folderPath](std::optional<AFS::ItemType> type)
    {
        if (type)
        {
            m_staticTextStatus->SetLabel(L"");
            navigateToExistingPath(m_treeCtrlFileSystem->GetRootItem(), split(folderPath.afsPath.value, FILE_NAME_SEPARATOR, SplitType::SKIP_EMPTY), *type);
        }
        else //split into multiple small async tasks rather than a single large one!
            findAndNavigateToExistingPath(*AFS::getParentPath(folderPath));
    });
}


//2. navgiate while ignoring any intermediate (access) errors or problems with hidden folders
void AbstractFolderPickerDlg::navigateToExistingPath(const wxTreeItemId& itemId, const std::vector<Zstring>& nodeRelPath, AFS::ItemType leafType)
{
    if (nodeRelPath.empty() ||
        (nodeRelPath.size() == 1 && leafType == AFS::ItemType::FILE)) //let's be *uber* correct
    {
        m_treeCtrlFileSystem->SelectItem(itemId);
        //m_treeCtrlFileSystem->EnsureVisible(itemId); -> not needed: maybe wxTreeCtrl::Expand() does this?
        return;
    }

    populateNodeThen(itemId, [this, itemId, nodeRelPath, leafType]
    {
        const             Zstring  childFolderName   = nodeRelPath.front();
        const std::vector<Zstring> childFolderRelPath{ nodeRelPath.begin() + 1, nodeRelPath.end() };

        wxTreeItemId childIdMatch;
        size_t insertPos = 0; //let's not use the wxTreeCtrl::OnCompareItems() abomination to implement sorting

        wxTreeItemIdValue cookie = nullptr;
        for (wxTreeItemId childId = m_treeCtrlFileSystem->GetFirstChild(itemId, cookie);
             childId.IsOk();
             childId = m_treeCtrlFileSystem->GetNextChild(itemId, cookie))
            if (auto itemData = dynamic_cast<AfsTreeItemData*>(m_treeCtrlFileSystem->GetItemData(childId)))
            {
                const Zstring& itemName = AFS::getItemName(itemData->folderPath);

                if (LessNaturalSort()(itemName, childFolderName))
                    ++insertPos; //assume items are already naturally sorted, see populateNodeThen()

                if (equalNoCase(itemName, childFolderName))
                {
                    childIdMatch = childId;
                    if (itemName == childFolderName)
                        break; //exact match => no need to search further!
                }
            }

        //we *know* that childFolder exists: Maybe it's just hidden during browsing: https://freefilesync.org/forum/viewtopic.php?t=3809
        if (!childIdMatch.IsOk()) //         or access to root folder is denied:     https://freefilesync.org/forum/viewtopic.php?t=5999
            if (auto itemData = dynamic_cast<AfsTreeItemData*>(m_treeCtrlFileSystem->GetItemData(itemId)))
            {
                m_treeCtrlFileSystem->SetItemHasChildren(itemId);

                const AbstractPath childFolderPath = AFS::appendRelPath(itemData->folderPath, childFolderName);

                childIdMatch = m_treeCtrlFileSystem->InsertItem(itemId, insertPos, getNodeDisplayName(childFolderPath),
                                                                static_cast<int>(childFolderRelPath.empty() && leafType == AFS::ItemType::SYMLINK ?
                                                                                 TreeNodeImage::folderSymlink : TreeNodeImage::folder), -1,
                                                                new AfsTreeItemData(childFolderPath));
                m_treeCtrlFileSystem->SetItemHasChildren(childIdMatch);
            }

        m_treeCtrlFileSystem->Expand(itemId); //wxTreeCtr::Expand emits wxTreeEvent!!!

        navigateToExistingPath(childIdMatch, childFolderRelPath, leafType);
    }, false /*popupErrors*/);
}


void AbstractFolderPickerDlg::OnExpandNode(wxTreeEvent& event)
{
    const wxTreeItemId itemId = event.GetItem();

    if (auto itemData = dynamic_cast<AfsTreeItemData*>(m_treeCtrlFileSystem->GetItemData(itemId)))
        if (itemData->loadStatus != NodeLoadStatus::loaded)
            populateNodeThen(itemId, [this, itemId]() { m_treeCtrlFileSystem->Expand(itemId); }, true /*popupErrors*/); //wxTreeCtr::Expand emits wxTreeEvent!!! watch out for recursion!
}


void AbstractFolderPickerDlg::OnItemTooltip(wxTreeEvent& event)
{
    wxString tooltip;
    if (auto itemData = dynamic_cast<AfsTreeItemData*>(m_treeCtrlFileSystem->GetItemData(event.GetItem())))
        tooltip = itemData->errorMsg;
    event.SetToolTip(tooltip);
}


void AbstractFolderPickerDlg::OnOkay(wxCommandEvent& event)
{
    const wxTreeItemId itemId = m_treeCtrlFileSystem->GetFocusedItem();

    auto itemData = dynamic_cast<AfsTreeItemData*>(m_treeCtrlFileSystem->GetItemData(itemId));
    assert(itemData);
    if (itemData)
        folderPathOut_ = itemData->folderPath;

    EndModal(ReturnAfsPicker::BUTTON_OKAY);
}
}


ReturnAfsPicker::ButtonPressed fff::showAbstractFolderPicker(wxWindow* parent, AbstractPath& folderPath)
{
    AbstractFolderPickerDlg pickerDlg(parent, folderPath);
    return static_cast<ReturnAfsPicker::ButtonPressed>(pickerDlg.ShowModal());
}

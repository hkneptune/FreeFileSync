// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FOLDER_PAIR_H_89341750847252345
#define FOLDER_PAIR_H_89341750847252345

#include <wx/event.h>
#include <wx/menu.h>
#include <wx+/context_menu.h>
#include <wx+/bitmap_button.h>
#include <wx+/image_tools.h>
#include <wx+/image_resources.h>
#include "folder_selector.h"
#include "small_dlgs.h"
#include "sync_cfg.h"
#include "../base/norm_filter.h"
#include "../base/structures.h"


namespace fff
{
//basic functionality for handling alternate folder pair configuration: change sync-cfg/filter cfg, right-click context menu, button icons...

template <class GuiPanel>
class FolderPairPanelBasic : private wxEvtHandler
{
public:
    void setConfig(const std::optional<CompConfig>& compConfig, const std::optional<SyncConfig>& syncCfg, const FilterConfig& filter)
    {
        localCmpCfg_  = compConfig;
        localSyncCfg_ = syncCfg;
        localFilter_  = filter;
        refreshButtons();
    }

    std::optional<CompConfig> getCompConfig  () const { return localCmpCfg_;  }
    std::optional<SyncConfig> getSyncConfig  () const { return localSyncCfg_; }
    FilterConfig         getFilterConfig() const { return localFilter_;  }


    FolderPairPanelBasic(GuiPanel& basicPanel) : //takes reference on basic panel to be enhanced
        basicPanel_(basicPanel)
    {
        //register events for removal of alternate configuration
        basicPanel_.m_bpButtonLocalCompCfg ->Connect(wxEVT_RIGHT_DOWN, wxCommandEventHandler(FolderPairPanelBasic::OnLocalCompCfgContext  ), nullptr, this);
        basicPanel_.m_bpButtonLocalSyncCfg ->Connect(wxEVT_RIGHT_DOWN, wxCommandEventHandler(FolderPairPanelBasic::OnLocalSyncCfgContext  ), nullptr, this);
        basicPanel_.m_bpButtonLocalFilter  ->Connect(wxEVT_RIGHT_DOWN, wxCommandEventHandler(FolderPairPanelBasic::OnLocalFilterCfgContext), nullptr, this);

        basicPanel_.m_bpButtonRemovePair->SetBitmapLabel(zen::getResourceImage(L"item_remove"));
    }

private:
    void refreshButtons()
    {
        using namespace zen;

        const wxImage imgCmp    = shrinkImage(getResourceImage(L"cfg_compare").ConvertToImage(), fastFromDIP(20));
        const wxImage imgSync   = shrinkImage(getResourceImage(L"cfg_sync"   ).ConvertToImage(), fastFromDIP(20));
        const wxImage imgFilter = shrinkImage(getResourceImage(L"cfg_filter" ).ConvertToImage(), fastFromDIP(20));

        if (localCmpCfg_)
        {
            setImage(*basicPanel_.m_bpButtonLocalCompCfg, imgCmp);
            basicPanel_.m_bpButtonLocalCompCfg->SetToolTip(_("Local comparison settings") +  L" (" + getVariantName(localCmpCfg_->compareVar) + L")");
        }
        else
        {
            setImage(*basicPanel_.m_bpButtonLocalCompCfg, greyScale(imgCmp));
            basicPanel_.m_bpButtonLocalCompCfg->SetToolTip(_("Local comparison settings"));
        }

        if (localSyncCfg_)
        {
            setImage(*basicPanel_.m_bpButtonLocalSyncCfg, imgSync);
            basicPanel_.m_bpButtonLocalSyncCfg->SetToolTip(_("Local synchronization settings") +  L" (" + getVariantName(localSyncCfg_->directionCfg.var) + L")");
        }
        else
        {
            setImage(*basicPanel_.m_bpButtonLocalSyncCfg, greyScale(imgSync));
            basicPanel_.m_bpButtonLocalSyncCfg->SetToolTip(_("Local synchronization settings"));
        }

        if (!isNullFilter(localFilter_))
        {
            setImage(*basicPanel_.m_bpButtonLocalFilter, imgFilter);
            basicPanel_.m_bpButtonLocalFilter->SetToolTip(_("Local filter") + L" (" + _("Active") + L")");
        }
        else
        {
            setImage(*basicPanel_.m_bpButtonLocalFilter, greyScale(imgFilter));
            basicPanel_.m_bpButtonLocalFilter->SetToolTip(_("Local filter") + L" (" + _("None") + L")");
        }
    }

    void OnLocalCompCfgContext(wxCommandEvent& event)
    {
        auto removeLocalCompCfg = [&]
        {
            this->localCmpCfg_ = {}; //"this->" galore: workaround GCC compiler bugs
            this->refreshButtons();
            this->onLocalCompCfgChange();
        };

        zen::ContextMenu menu;
        menu.addItem(_("Remove local settings"), removeLocalCompCfg, nullptr, static_cast<bool>(localCmpCfg_));
        menu.popup(basicPanel_);
    }

    void OnLocalSyncCfgContext(wxCommandEvent& event)
    {
        auto removeLocalSyncCfg = [&]
        {
            this->localSyncCfg_ = {};
            this->refreshButtons();
            this->onLocalSyncCfgChange();
        };

        zen::ContextMenu menu;
        menu.addItem(_("Remove local settings"), removeLocalSyncCfg, nullptr, static_cast<bool>(localSyncCfg_));
        menu.popup(basicPanel_);
    }

    void OnLocalFilterCfgContext(wxCommandEvent& event)
    {
        auto removeLocalFilterCfg = [&]
        {
            this->localFilter_ = FilterConfig();
            this->refreshButtons();
            this->onLocalFilterCfgChange();
        };

        std::unique_ptr<FilterConfig>& filterCfgOnClipboard = getFilterCfgOnClipboardRef();

        auto copyFilter  = [&] { filterCfgOnClipboard = std::make_unique<FilterConfig>(this->localFilter_); };
        auto pasteFilter = [&]
        {
            if (filterCfgOnClipboard)
            {
                this->localFilter_ = *filterCfgOnClipboard;
                this->refreshButtons();
                this->onLocalFilterCfgChange();
            }
        };

        zen::ContextMenu menu;
        menu.addItem(_("Clear local filter"), removeLocalFilterCfg, nullptr, !isNullFilter(localFilter_));
        menu.addSeparator();
        menu.addItem( _("Copy"),  copyFilter,  nullptr, !isNullFilter(localFilter_));
        menu.addItem( _("Paste"), pasteFilter, nullptr, filterCfgOnClipboard.get() != nullptr);
        menu.popup(basicPanel_);
    }


    virtual MainConfiguration getMainConfig() const = 0;
    virtual wxWindow* getParentWindow() = 0;
    virtual std::unique_ptr<FilterConfig>& getFilterCfgOnClipboardRef() = 0;

    virtual void onLocalCompCfgChange  () = 0;
    virtual void onLocalSyncCfgChange  () = 0;
    virtual void onLocalFilterCfgChange() = 0;

    GuiPanel& basicPanel_; //panel to be enhanced by this template

    //alternate configuration attached to it
    std::optional<CompConfig> localCmpCfg_;
    std::optional<SyncConfig> localSyncCfg_;
    FilterConfig         localFilter_;
};
}


#endif //FOLDER_PAIR_H_89341750847252345

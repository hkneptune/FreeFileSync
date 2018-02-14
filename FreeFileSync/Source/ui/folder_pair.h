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
#include "../lib/norm_filter.h"
#include "../structures.h"


namespace fff
{
//basic functionality for handling alternate folder pair configuration: change sync-cfg/filter cfg, right-click context menu, button icons...

template <class GuiPanel>
class FolderPairPanelBasic : private wxEvtHandler
{
public:
    using AltCompCfgPtr = std::shared_ptr<const CompConfig>;
    using AltSyncCfgPtr = std::shared_ptr<const SyncConfig>;

    void setConfig(AltCompCfgPtr compConfig, AltSyncCfgPtr syncCfg, const FilterConfig& filter)
    {
        altCompConfig_ = compConfig;
        altSyncConfig_ = syncCfg;
        localFilter_   = filter;
        refreshButtons();
    }

    AltCompCfgPtr getAltCompConfig  () const { return altCompConfig_; }
    AltSyncCfgPtr getAltSyncConfig  () const { return altSyncConfig_; }
    FilterConfig  getAltFilterConfig() const { return localFilter_;   }


    FolderPairPanelBasic(GuiPanel& basicPanel) : //takes reference on basic panel to be enhanced
        basicPanel_(basicPanel)
    {
        //register events for removal of alternate configuration
        basicPanel_.m_bpButtonAltCompCfg ->Connect(wxEVT_RIGHT_DOWN, wxCommandEventHandler(FolderPairPanelBasic::OnAltCompCfgContext    ), nullptr, this);
        basicPanel_.m_bpButtonAltSyncCfg ->Connect(wxEVT_RIGHT_DOWN, wxCommandEventHandler(FolderPairPanelBasic::OnAltSyncCfgContext    ), nullptr, this);
        basicPanel_.m_bpButtonLocalFilter->Connect(wxEVT_RIGHT_DOWN, wxCommandEventHandler(FolderPairPanelBasic::OnLocalFilterCfgContext), nullptr, this);

        basicPanel_.m_bpButtonRemovePair->SetBitmapLabel(zen::getResourceImage(L"item_remove"));
    }

private:
    void refreshButtons()
    {
        using namespace zen;

        if (altCompConfig_.get())
        {
            setImage(*basicPanel_.m_bpButtonAltCompCfg, getResourceImage(L"cfg_compare_small"));
            basicPanel_.m_bpButtonAltCompCfg->SetToolTip(_("Local comparison settings") +  L" (" + getVariantName(altCompConfig_->compareVar) + L")");
        }
        else
        {
            setImage(*basicPanel_.m_bpButtonAltCompCfg, greyScale(getResourceImage(L"cfg_compare_small")));
            basicPanel_.m_bpButtonAltCompCfg->SetToolTip(_("Local comparison settings"));
        }

        if (altSyncConfig_.get())
        {
            setImage(*basicPanel_.m_bpButtonAltSyncCfg, getResourceImage(L"cfg_sync_small"));
            basicPanel_.m_bpButtonAltSyncCfg->SetToolTip(_("Local synchronization settings") +  L" (" + getVariantName(altSyncConfig_->directionCfg.var) + L")");
        }
        else
        {
            setImage(*basicPanel_.m_bpButtonAltSyncCfg, greyScale(getResourceImage(L"cfg_sync_small")));
            basicPanel_.m_bpButtonAltSyncCfg->SetToolTip(_("Local synchronization settings"));
        }

        if (!isNullFilter(localFilter_))
        {
            setImage(*basicPanel_.m_bpButtonLocalFilter, getResourceImage(L"filter_small"));
            basicPanel_.m_bpButtonLocalFilter->SetToolTip(_("Local filter") + L" (" + _("Active") + L")");
        }
        else
        {
            setImage(*basicPanel_.m_bpButtonLocalFilter, greyScale(getResourceImage(L"filter_small")));
            basicPanel_.m_bpButtonLocalFilter->SetToolTip(_("Local filter") + L" (" + _("None") + L")");
        }
    }

    void OnAltCompCfgContext(wxCommandEvent& event)
    {
        auto removeAltCompCfg = [&]
        {
            this->altCompConfig_.reset(); //"this->" galore: workaround GCC compiler bugs
            this->refreshButtons();
            this->onAltCompCfgChange();
        };

        zen::ContextMenu menu;
        menu.addItem(_("Remove local settings"), removeAltCompCfg, nullptr, altCompConfig_.get() != nullptr);
        menu.popup(basicPanel_);
    }

    void OnAltSyncCfgContext(wxCommandEvent& event)
    {
        auto removeAltSyncCfg = [&]
        {
            this->altSyncConfig_.reset();
            this->refreshButtons();
            this->onAltSyncCfgChange();
        };

        zen::ContextMenu menu;
        menu.addItem(_("Remove local settings"), removeAltSyncCfg, nullptr, altSyncConfig_.get() != nullptr);
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

    virtual void onAltCompCfgChange() = 0;
    virtual void onAltSyncCfgChange() = 0;
    virtual void onLocalFilterCfgChange() = 0;

    GuiPanel& basicPanel_; //panel to be enhanced by this template

    //alternate configuration attached to it
    AltCompCfgPtr altCompConfig_; //optional
    AltSyncCfgPtr altSyncConfig_; //
    FilterConfig  localFilter_;
};
}


#endif //FOLDER_PAIR_H_89341750847252345

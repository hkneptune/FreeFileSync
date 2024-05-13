// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FOLDER_PAIR_H_89341750847252345
#define FOLDER_PAIR_H_89341750847252345

#include <wx/event.h>
//#include <wx/menu.h>
#include <wx+/context_menu.h>
//#include <wx+/bitmap_button.h>
#include <wx+/image_tools.h>
#include <wx+/image_resources.h>
//#include "folder_selector.h"
//#include "small_dlgs.h"
//#include "sync_cfg.h"
#include "../base/norm_filter.h"
//#include "../base_tools.h"


namespace fff
{
//basic functionality for handling alternate folder pair configuration: change sync-cfg/filter cfg, right-click context menu, button icons...

template <class GuiPanel>
class FolderPairPanelBasic : private wxEvtHandler
{
public:
    explicit FolderPairPanelBasic(GuiPanel& basicPanel) : //takes reference on basic panel to be enhanced
        basicPanel_(basicPanel)
    {
        using namespace zen;

        //register events for removal of alternate configuration
        basicPanel_.m_bpButtonLocalCompCfg ->Bind(wxEVT_RIGHT_DOWN, [this](wxMouseEvent& event) { onLocalCompCfgContext  (event); });
        basicPanel_.m_bpButtonLocalSyncCfg ->Bind(wxEVT_RIGHT_DOWN, [this](wxMouseEvent& event) { onLocalSyncCfgContext  (event); });
        basicPanel_.m_bpButtonLocalFilter  ->Bind(wxEVT_RIGHT_DOWN, [this](wxMouseEvent& event) { onLocalFilterCfgContext(event); });

        setImage(*basicPanel_.m_bpButtonRemovePair, loadImage("item_remove"));
    }


    void setConfig(const std::optional<CompConfig>& compConfig, const std::optional<SyncConfig>& syncCfg, const FilterConfig& filter)
    {
        localCmpCfg_  = compConfig;
        localSyncCfg_ = syncCfg;
        localFilter_  = filter;
        refreshButtons();
    }

    std::optional<CompConfig> getCompConfig  () const { return localCmpCfg_;  }
    std::optional<SyncConfig> getSyncConfig  () const { return localSyncCfg_; }
    FilterConfig              getFilterConfig() const { return localFilter_;  }

private:
    void refreshButtons()
    {
        using namespace zen;

        setImage(*basicPanel_.m_bpButtonLocalCompCfg, greyScaleIfDisabled(imgCmp_, !!localCmpCfg_));
        basicPanel_.m_bpButtonLocalCompCfg->SetToolTip(localCmpCfg_ ?
                                                       _("Local comparison settings") +  L" (" + getVariantName(localCmpCfg_->compareVar) + L')' :
                                                       _("Local comparison settings"));

        setImage(*basicPanel_.m_bpButtonLocalSyncCfg, greyScaleIfDisabled(imgSync_, !!localSyncCfg_));
        basicPanel_.m_bpButtonLocalSyncCfg->SetToolTip(localSyncCfg_ ?
                                                       _("Local synchronization settings") +  L" (" + getVariantName(getSyncVariant(localSyncCfg_->directionCfg)) + L')' :
                                                       _("Local synchronization settings"));

        setImage(*basicPanel_.m_bpButtonLocalFilter, greyScaleIfDisabled(imgFilter_, !isNullFilter(localFilter_)));
        basicPanel_.m_bpButtonLocalFilter->SetToolTip(!isNullFilter(localFilter_) ?
                                                      _("Local filter") + L" (" + _("Active") + L')' :
                                                      _("Local filter") + L" (" + _("None")   + L')');
    }

    void onLocalCompCfgContext(wxEvent& event)
    {
        auto removeLocalCompCfg = [&]
        {
            this->localCmpCfg_ = {}; //"this->" galore: workaround GCC compiler bugs
            this->refreshButtons();
            this->onLocalCompCfgChange();
        };

        zen::ContextMenu menu;
        menu.addItem(_("Remove local settings"), removeLocalCompCfg, wxNullImage, static_cast<bool>(localCmpCfg_));
        menu.popup(*basicPanel_.m_bpButtonLocalCompCfg, {basicPanel_.m_bpButtonLocalCompCfg->GetSize().x, 0});
    }

    void onLocalSyncCfgContext(wxEvent& event)
    {
        auto removeLocalSyncCfg = [&]
        {
            this->localSyncCfg_ = {};
            this->refreshButtons();
            this->onLocalSyncCfgChange();
        };

        zen::ContextMenu menu;
        menu.addItem(_("Remove local settings"), removeLocalSyncCfg, wxNullImage, static_cast<bool>(localSyncCfg_));
        menu.popup(*basicPanel_.m_bpButtonLocalSyncCfg, {basicPanel_.m_bpButtonLocalSyncCfg->GetSize().x, 0});
    }

    void onLocalFilterCfgContext(wxEvent& event)
    {
        using namespace zen;

        std::optional<FilterConfig> filterCfgOnClipboard;
        if (std::optional<wxString> clipTxt = getClipboardText())
            filterCfgOnClipboard = parseFilterBuf(utfTo<std::string>(*clipTxt));

        auto cutFilter = [&]
        {
            setClipboardText(utfTo<wxString>(serializeFilter(this->localFilter_)));
            this->localFilter_ = FilterConfig();
            this->refreshButtons();
            this->onLocalFilterCfgChange();
        };

        auto copyFilter = [&] { setClipboardText(utfTo<wxString>(serializeFilter(this->localFilter_))); };

        auto pasteFilter = [&]
        {
            this->localFilter_ = *filterCfgOnClipboard;
            this->refreshButtons();
            this->onLocalFilterCfgChange();
        };

        zen::ContextMenu menu;
        menu.addItem( _("Cu&t"), cutFilter, loadImage("item_cut_sicon"), !isNullFilter(localFilter_));
        menu.addSeparator();
        menu.addItem( _("&Copy"), copyFilter, loadImage("item_copy_sicon"), !isNullFilter(localFilter_));
        menu.addItem( _("&Paste"), pasteFilter, loadImage("item_paste_sicon"), filterCfgOnClipboard.has_value());

        menu.popup(*basicPanel_.m_bpButtonLocalFilter, {basicPanel_.m_bpButtonLocalFilter->GetSize().x, 0});
    }


    virtual MainConfiguration getMainConfig() const = 0;
    virtual wxWindow* getParentWindow() = 0;

    virtual void onLocalCompCfgChange  () = 0;
    virtual void onLocalSyncCfgChange  () = 0;
    virtual void onLocalFilterCfgChange() = 0;

    GuiPanel& basicPanel_; //panel to be enhanced by this template

    //alternate configuration attached to it
    std::optional<CompConfig> localCmpCfg_;
    std::optional<SyncConfig> localSyncCfg_;
    FilterConfig              localFilter_;

    const wxImage imgCmp_    = zen::loadImage("options_compare", zen::dipToScreen(20));
    const wxImage imgSync_   = zen::loadImage("options_sync",    zen::dipToScreen(20));
    const wxImage imgFilter_ = zen::loadImage("options_filter",  zen::dipToScreen(20));
};
}

#endif //FOLDER_PAIR_H_89341750847252345

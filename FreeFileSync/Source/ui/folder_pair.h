// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FOLDER_PAIR_H_89341750847252345
#define FOLDER_PAIR_H_89341750847252345

#include <wx/event.h>
#include <wx+/context_menu.h>
#include <wx+/image_tools.h>
#include <wx+/image_resources.h>
#include "../base/norm_filter.h"


namespace fff
{
//basic functionality for handling alternate folder pair configuration: change sync-cfg/filter cfg, right-click context menu, button icons...
std::wstring getFilterSummaryForTooltip(const FilterConfig& filterCfg);


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
                                                       _("Local comparison settings") +  L"\n(" + getVariantName(localCmpCfg_->compareVar) + L')' :
                                                       _("Local comparison settings"));

        setImage(*basicPanel_.m_bpButtonLocalSyncCfg, greyScaleIfDisabled(imgSync_, !!localSyncCfg_));
        basicPanel_.m_bpButtonLocalSyncCfg->SetToolTip(localSyncCfg_ ?
                                                       _("Local synchronization settings") +  L"\n(" + getVariantName(getSyncVariant(localSyncCfg_->directionCfg)) + L')' :
                                                       _("Local synchronization settings"));

        setImage(*basicPanel_.m_bpButtonLocalFilter, greyScaleIfDisabled(imgFilter_, !isNullFilter(localFilter_)));
        basicPanel_.m_bpButtonLocalFilter->SetToolTip(_("Local filter") + getFilterSummaryForTooltip(localFilter_));
    }

    void onLocalCompCfgContext(wxEvent& event)
    {
        using namespace zen;

        ContextMenu menu;

        auto setVariant = [&](CompareVariant var)
        {
            if (!this->localCmpCfg_)
                this->localCmpCfg_ = CompConfig();
            this->localCmpCfg_->compareVar = var;

            this->refreshButtons();
            this->onLocalCompCfgChange();
        };

        auto addVariantItem = [&](CompareVariant cmpVar, const char* iconName)
        {
            const wxImage imgSel = loadImage(iconName, -1 /*maxWidth*/, dipToScreen(getMenuIconDipSize()));

            menu.addItem(getVariantName(cmpVar), [&setVariant, cmpVar] { setVariant(cmpVar); },
                         greyScaleIfDisabled(imgSel, this->localCmpCfg_ && this->localCmpCfg_->compareVar == cmpVar));
        };
        addVariantItem(CompareVariant::timeSize, "cmp_time");
        addVariantItem(CompareVariant::content,  "cmp_content");
        addVariantItem(CompareVariant::size,     "cmp_size");

        //----------------------------------------------------------------------------------------
        menu.addSeparator();

        auto removeLocalCompCfg = [&]
        {
            this->localCmpCfg_ = {}; //"this->" galore: workaround GCC compiler bugs
            this->refreshButtons();
            this->onLocalCompCfgChange();
        };
        menu.addItem(_("Remove local settings"), removeLocalCompCfg, wxNullImage, static_cast<bool>(localCmpCfg_));
        menu.popup(*basicPanel_.m_bpButtonLocalCompCfg, {basicPanel_.m_bpButtonLocalCompCfg->GetSize().x, 0});
    }

    void onLocalSyncCfgContext(wxEvent& event)
    {
        using namespace zen;

        ContextMenu menu;

        auto setVariant = [&](SyncVariant var)
        {
            if (!this->localSyncCfg_)
                this->localSyncCfg_ = SyncConfig();
            this->localSyncCfg_->directionCfg = getDefaultSyncCfg(var);

            this->refreshButtons();
            this->onLocalSyncCfgChange();
        };

        auto addVariantItem = [&](SyncVariant syncVar, const char* iconName)
        {
            const wxImage imgSel = mirrorIfRtl(loadImage(iconName, -1 /*maxWidth*/, dipToScreen(getMenuIconDipSize())));

            menu.addItem(getVariantName(syncVar), [&setVariant, syncVar] { setVariant(syncVar); },
                         greyScaleIfDisabled(imgSel, this->localSyncCfg_ && getSyncVariant(this->localSyncCfg_->directionCfg) == syncVar));
        };
        addVariantItem(SyncVariant::twoWay, "sync_twoway");
        addVariantItem(SyncVariant::mirror, "sync_mirror");
        addVariantItem(SyncVariant::update, "sync_update");
        //addVariantItem(SyncVariant::custom, "sync_custom"); -> doesn't make sense, does it?

        //----------------------------------------------------------------------------------------
        menu.addSeparator();

        auto removeLocalSyncCfg = [&]
        {
            this->localSyncCfg_ = {};
            this->refreshButtons();
            this->onLocalSyncCfgChange();
        };
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
        menu.addItem( _("&Copy"), copyFilter, loadImage("item_copy_sicon"), !isNullFilter(localFilter_));
        menu.addItem( _("&Paste"), pasteFilter, loadImage("item_paste_sicon"), filterCfgOnClipboard.has_value());
        menu.addSeparator();
        menu.addItem( _("Cu&t"), cutFilter, loadImage("item_cut_sicon"), !isNullFilter(localFilter_));

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


inline
std::wstring getFilterSummaryForTooltip(const FilterConfig& filterCfg)
{
    using namespace zen;

    auto indentLines = [](Zstring str)
    {
        std::wstring out;
        split(str, Zstr('\n'), [&out](ZstringView block)
        {
            block = trimCpy(block);
            if (!block.empty())
            {
                out += L'\n';
                out += TAB_SPACE;
                out += utfTo<std::wstring>(block);
            }
        });
        return out;
    };

    std::wstring filterSummary;
    if (trimCpy(filterCfg.includeFilter) != Zstr("*")) //harmonize with base/path_filter.cpp NameFilter::isNull
        filterSummary += L"\n\n" + _("Include:") + indentLines(filterCfg.includeFilter);

    if (!trimCpy(filterCfg.excludeFilter).empty())
        filterSummary += L"\n\n" + _("Exclude:") + indentLines(filterCfg.excludeFilter);

    return filterSummary;
}
}

#endif //FOLDER_PAIR_H_89341750847252345

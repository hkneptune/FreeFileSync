// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "sync_cfg.h"
#include <memory>
#include <zen/http.h>
#include <wx/wupdlock.h>
#include <wx/valtext.h>
#include <wx+/rtl.h>
#include <wx+/no_flicker.h>
#include <wx+/choice_enum.h>
#include <wx+/image_tools.h>
#include <wx+/font_size.h>
#include <wx+/std_button_layout.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include <wx+/focus.h>
#include "gui_generated.h"
#include "command_box.h"
#include "folder_selector.h"
#include "../base/file_hierarchy.h"
#include "../base/help_provider.h"
#include "../base/log_file.h"
#include "../base/norm_filter.h"
#include "../afs/concrete.h"



using namespace zen;
using namespace fff;


namespace
{
const int CFG_DESCRIPTION_WIDTH_DIP = 230;


class ConfigDialog : public ConfigDlgGenerated
{
public:
    ConfigDialog(wxWindow* parent,
                 SyncConfigPanel panelToShow,
                 int localPairIndexToShow, bool showMultipleCfgs,
                 GlobalPairConfig& globalPairCfg,
                 std::vector<LocalPairConfig>& localPairConfig,
                 std::vector<Zstring>& versioningFolderHistory,
                 std::vector<Zstring>& logFolderHistory,
                 size_t folderHistoryMax,
                 std::vector<Zstring>& emailHistory,   size_t emailHistoryMax,
                 std::vector<Zstring>& commandHistory, size_t commandHistoryMax);

private:
    void OnOkay  (wxCommandEvent& event) override;
    void OnCancel(wxCommandEvent& event) override { EndModal(ReturnSyncConfig::BUTTON_CANCEL); }
    void OnClose (wxCloseEvent&   event) override { EndModal(ReturnSyncConfig::BUTTON_CANCEL); }

    void onLocalKeyEvent(wxKeyEvent& event);
    void onListBoxKeyEvent(wxKeyEvent& event) override;
    void OnSelectFolderPair(wxCommandEvent& event) override;

    enum class ConfigTypeImage
    {
        COMPARISON = 0, //used as zero-based wxImageList index!
        COMPARISON_GREY,
        FILTER,
        FILTER_GREY,
        SYNC,
        SYNC_GREY,
    };

    //------------- comparison panel ----------------------
    void OnHelpComparisonSettings(wxHyperlinkEvent& event) override { displayHelpEntry(L"comparison-settings",  this); }
    void OnHelpTimeShift         (wxHyperlinkEvent& event) override { displayHelpEntry(L"daylight-saving-time", this); }
    void OnHelpPerformance       (wxHyperlinkEvent& event) override { displayHelpEntry(L"performance",          this); }

    void OnToggleLocalCompSettings(wxCommandEvent& event) override { updateCompGui(); updateSyncGui(); /*affects sync settings, too!*/ }
    void OnToggleIgnoreErrors     (wxCommandEvent& event) override { updateMiscGui(); }
    void OnToggleAutoRetry        (wxCommandEvent& event) override { updateMiscGui(); }

    void OnCompByTimeSize         (wxCommandEvent& event) override { localCmpVar_ = CompareVariant::timeSize; updateCompGui(); updateSyncGui(); } //
    void OnCompByContent          (wxCommandEvent& event) override { localCmpVar_ = CompareVariant::content;   updateCompGui(); updateSyncGui(); } //affects sync settings, too!
    void OnCompBySize             (wxCommandEvent& event) override { localCmpVar_ = CompareVariant::size;      updateCompGui(); updateSyncGui(); } //
    void OnCompByTimeSizeDouble   (wxMouseEvent&   event) override;
    void OnCompBySizeDouble       (wxMouseEvent&   event) override;
    void OnCompByContentDouble    (wxMouseEvent&   event) override;
    void OnChangeCompOption       (wxCommandEvent& event) override { updateCompGui(); }

    std::optional<CompConfig> getCompConfig() const;
    void setCompConfig(const CompConfig* compCfg);

    void updateCompGui();

    CompareVariant localCmpVar_ = CompareVariant::timeSize;

    std::set<AfsDevice>         devicesForEdit_;     //helper data for deviceParallelOps
    std::map<AfsDevice, size_t> deviceParallelOps_;  //

    //------------- filter panel --------------------------
    void OnHelpShowExamples(wxHyperlinkEvent& event) override { displayHelpEntry(L"exclude-items", this); }
    void OnChangeFilterOption(wxCommandEvent& event) override { updateFilterGui(); }
    void OnFilterReset       (wxCommandEvent& event) override { setFilterConfig(FilterConfig()); }

    void onFilterKeyEvent(wxKeyEvent& event);

    FilterConfig getFilterConfig() const;
    void setFilterConfig(const FilterConfig& filter);

    void updateFilterGui();

    EnumDescrList<UnitTime> enumTimeDescr_;
    EnumDescrList<UnitSize> enumSizeDescr_;

    //------------- synchronization panel -----------------
    void OnSyncTwoWay(wxCommandEvent& event) override { directionCfg_.var = DirectionConfig::TWO_WAY; updateSyncGui(); }
    void OnSyncMirror(wxCommandEvent& event) override { directionCfg_.var = DirectionConfig::MIRROR;  updateSyncGui(); }
    void OnSyncUpdate(wxCommandEvent& event) override { directionCfg_.var = DirectionConfig::UPDATE;  updateSyncGui(); }
    void OnSyncCustom(wxCommandEvent& event) override { directionCfg_.var = DirectionConfig::CUSTOM;  updateSyncGui(); }

    void OnToggleLocalSyncSettings(wxCommandEvent& event) override { updateSyncGui(); }
    void OnToggleDetectMovedFiles (wxCommandEvent& event) override { directionCfg_.detectMovedFiles = !directionCfg_.detectMovedFiles; updateSyncGui(); } //parameter NOT owned by checkbox!
    void OnChanegVersioningStyle  (wxCommandEvent& event) override { updateSyncGui(); }
    void OnToggleVersioningLimit  (wxCommandEvent& event) override { updateSyncGui(); }

    void OnSyncTwoWayDouble(wxMouseEvent& event) override;
    void OnSyncMirrorDouble(wxMouseEvent& event) override;
    void OnSyncUpdateDouble(wxMouseEvent& event) override;
    void OnSyncCustomDouble(wxMouseEvent& event) override;

    void OnExLeftSideOnly (wxCommandEvent& event) override;
    void OnExRightSideOnly(wxCommandEvent& event) override;
    void OnLeftNewer      (wxCommandEvent& event) override;
    void OnRightNewer     (wxCommandEvent& event) override;
    void OnDifferent      (wxCommandEvent& event) override;
    void OnConflict       (wxCommandEvent& event) override;

    void OnHelpDetectMovedFiles(wxHyperlinkEvent& event) override { displayHelpEntry(L"synchronization-settings", this); }
    void OnHelpVersioning      (wxHyperlinkEvent& event) override { displayHelpEntry(L"versioning",               this); }

    void OnDeletionPermanent  (wxCommandEvent& event) override { handleDeletion_ = DeletionPolicy::permanent;  updateSyncGui(); }
    void OnDeletionRecycler   (wxCommandEvent& event) override { handleDeletion_ = DeletionPolicy::recycler;   updateSyncGui(); }
    void OnDeletionVersioning (wxCommandEvent& event) override { handleDeletion_ = DeletionPolicy::versioning; updateSyncGui(); }

    void OnToggleMiscOption       (wxCommandEvent& event) override { updateMiscGui(); }
    void OnEmailAlways      (wxCommandEvent& event) override { emailNotifyCondition_ = ResultsNotification::always;       updateMiscGui(); }
    void OnEmailErrorWarning(wxCommandEvent& event) override { emailNotifyCondition_ = ResultsNotification::errorWarning; updateMiscGui(); }
    void OnEmailErrorOnly   (wxCommandEvent& event) override { emailNotifyCondition_ = ResultsNotification::errorOnly;    updateMiscGui(); }

    std::optional<SyncConfig> getSyncConfig() const;
    void setSyncConfig(const SyncConfig* syncCfg);

    void updateSyncGui();

    //parameters with ownership NOT within GUI controls!
    DirectionConfig directionCfg_;
    DeletionPolicy handleDeletion_ = DeletionPolicy::recycler; //use Recycler, delete permanently or move to user-defined location

    const std::function<size_t(const Zstring& folderPathPhrase)>                     getDeviceParallelOps_;
    const std::function<void  (const Zstring& folderPathPhrase, size_t parallelOps)> setDeviceParallelOps_;

    FolderSelector versioningFolder_;
    EnumDescrList<VersioningStyle> enumVersioningStyle_;

    ResultsNotification emailNotifyCondition_ = ResultsNotification::always;

    EnumDescrList<PostSyncCondition> enumPostSyncCondition_;

    FolderSelector logfileDir_;

    //-----------------------------------------------------

    MiscSyncConfig getMiscSyncOptions() const;
    void setMiscSyncOptions(const MiscSyncConfig& miscCfg);

    void updateMiscGui();

    //-----------------------------------------------------

    void selectFolderPairConfig(int newPairIndexToShow);
    bool unselectFolderPairConfig(bool validateParams); //returns false on error: shows message box!

    //output-only parameters
    GlobalPairConfig& globalPairCfgOut_;
    std::vector<LocalPairConfig>& localPairCfgOut_;
    std::vector<Zstring>& versioningFolderHistoryOut_;
    std::vector<Zstring>& logFolderHistoryOut_;
    std::vector<Zstring>& emailHistoryOut_;
    std::vector<Zstring>& commandHistoryOut_;

    //working copy of ALL config parameters: only one folder pair is selected at a time!
    GlobalPairConfig globalPairCfg_;
    std::vector<LocalPairConfig> localPairCfg_;

    int selectedPairIndexToShow_ = EMPTY_PAIR_INDEX_SELECTED;
    static const int EMPTY_PAIR_INDEX_SELECTED = -2;

    const bool enableExtraFeatures_;
    const bool showMultipleCfgs_;
};

//#################################################################################################################

std::wstring getCompVariantDescription(CompareVariant var)
{
    switch (var)
    {
        case CompareVariant::timeSize:
            return _("Identify equal files by comparing modification time and size.");
        case CompareVariant::content:
            return _("Identify equal files by comparing the file content.");
        case CompareVariant::size:
            return _("Identify equal files by comparing their file size.");
    }
    assert(false);
    return _("Error");
}


std::wstring getSyncVariantDescription(DirectionConfig::Variant var)
{
    switch (var)
    {
        case DirectionConfig::TWO_WAY:
            return _("Identify and propagate changes on both sides. Deletions, moves and conflicts are detected automatically using a database.");
        case DirectionConfig::MIRROR:
            return _("Create a mirror backup of the left folder by adapting the right folder to match.");
        case DirectionConfig::UPDATE:
            return _("Copy new and updated files to the right folder.");
        case DirectionConfig::CUSTOM:
            return _("Configure your own synchronization rules.");
    }
    assert(false);
    return _("Error");
}


ConfigDialog::ConfigDialog(wxWindow* parent,
                           SyncConfigPanel panelToShow,
                           int localPairIndexToShow, bool showMultipleCfgs,
                           GlobalPairConfig& globalPairCfg,
                           std::vector<LocalPairConfig>& localPairConfig,
                           std::vector<Zstring>& versioningFolderHistory,
                           std::vector<Zstring>& logFolderHistory,
                           size_t folderHistoryMax,
                           std::vector<Zstring>& emailHistory,   size_t emailHistoryMax,
                           std::vector<Zstring>& commandHistory, size_t commandHistoryMax) :
    ConfigDlgGenerated(parent),

    getDeviceParallelOps_([this](const Zstring& folderPathPhrase)
{
    assert(selectedPairIndexToShow_ == -1 ||  makeUnsigned(selectedPairIndexToShow_) < localPairCfg_.size());
    const auto& deviceParallelOps = selectedPairIndexToShow_ < 0 ? getMiscSyncOptions().deviceParallelOps : globalPairCfg_.miscCfg.deviceParallelOps; //ternary-WTF!

    return getDeviceParallelOps(deviceParallelOps, folderPathPhrase);
}),

setDeviceParallelOps_([this](const Zstring& folderPathPhrase, size_t parallelOps) //setDeviceParallelOps()
{
    assert(selectedPairIndexToShow_ == -1 ||  makeUnsigned(selectedPairIndexToShow_) < localPairCfg_.size());
    if (selectedPairIndexToShow_ < 0)
    {
        MiscSyncConfig miscCfg = getMiscSyncOptions();
        setDeviceParallelOps(miscCfg.deviceParallelOps, folderPathPhrase, parallelOps);
        setMiscSyncOptions(miscCfg);
    }
    else
        setDeviceParallelOps(globalPairCfg_.miscCfg.deviceParallelOps, folderPathPhrase, parallelOps);
}),

versioningFolder_(this, *m_panelVersioning, *m_buttonSelectVersioningFolder, *m_bpButtonSelectVersioningAltFolder, *m_versioningFolderPath,
                  nullptr /*staticText*/, nullptr /*dropWindow2*/, nullptr /*droppedPathsFilter*/, getDeviceParallelOps_, setDeviceParallelOps_),

logfileDir_(this, *m_panelLogfile, *m_buttonSelectLogFolder, *m_bpButtonSelectAltLogFolder, *m_logFolderPath,
            nullptr /*staticText*/, nullptr /*dropWindow2*/, nullptr /*droppedPathsFilter*/, getDeviceParallelOps_, setDeviceParallelOps_),

globalPairCfgOut_(globalPairCfg),
localPairCfgOut_(localPairConfig),
versioningFolderHistoryOut_(versioningFolderHistory),
logFolderHistoryOut_(logFolderHistory),
emailHistoryOut_(emailHistory),
commandHistoryOut_(commandHistory),
globalPairCfg_(globalPairCfg),
localPairCfg_(localPairConfig),
enableExtraFeatures_(false),
showMultipleCfgs_(showMultipleCfgs)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    m_notebook->SetPadding(wxSize(fastFromDIP(2), 0)); //height cannot be changed

    //fill image list to cope with wxNotebook image setting design desaster...
    const int imgListSize = getResourceImage(L"cfg_compare_sicon").GetHeight();
    auto imgList = std::make_unique<wxImageList>(imgListSize, imgListSize);

    auto addToImageList = [&](const wxBitmap& bmp)
    {
        assert(bmp.GetWidth () <= imgListSize);
        assert(bmp.GetHeight() <= imgListSize);
        imgList->Add(bmp);
        imgList->Add(greyScale(bmp));
    };
    //add images in same sequence like ConfigTypeImage enum!!!
    addToImageList(getResourceImage(L"cfg_compare_sicon"));
    addToImageList(getResourceImage(L"cfg_filter_sicon"));
    addToImageList(getResourceImage(L"cfg_sync_sicon"));
    assert(imgList->GetImageCount() == static_cast<int>(ConfigTypeImage::SYNC_GREY) + 1);

    m_notebook->AssignImageList(imgList.release()); //pass ownership

    m_notebook->SetPageText(static_cast<size_t>(SyncConfigPanel::COMPARISON), _("Comparison")      + L" (F6)");
    m_notebook->SetPageText(static_cast<size_t>(SyncConfigPanel::FILTER    ), _("Filter")          + L" (F7)");
    m_notebook->SetPageText(static_cast<size_t>(SyncConfigPanel::SYNC      ), _("Synchronization") + L" (F8)");

    m_notebook->ChangeSelection(static_cast<size_t>(panelToShow));

    //------------- comparison panel ----------------------
    setRelativeFontSize(*m_toggleBtnByTimeSize, 1.25);
    setRelativeFontSize(*m_toggleBtnBySize,     1.25);
    setRelativeFontSize(*m_toggleBtnByContent,  1.25);

    m_toggleBtnByTimeSize->SetToolTip(getCompVariantDescription(CompareVariant::timeSize));
    m_toggleBtnByContent ->SetToolTip(getCompVariantDescription(CompareVariant::content));
    m_toggleBtnBySize    ->SetToolTip(getCompVariantDescription(CompareVariant::size));

    m_staticTextCompVarDescription->SetMinSize({fastFromDIP(CFG_DESCRIPTION_WIDTH_DIP), -1});

    m_scrolledWindowPerf->SetMinSize({fastFromDIP(220), -1});
    m_bitmapPerf->SetBitmap(greyScaleIfDisabled(getResourceImage(L"speed"), enableExtraFeatures_));
    m_panelPerfHeader->Enable(enableExtraFeatures_);

    m_spinCtrlAutoRetryCount->SetMinSize({fastFromDIP(60), -1}); //Hack: set size (why does wxWindow::Size() not work?)
    m_spinCtrlAutoRetryDelay->SetMinSize({fastFromDIP(60), -1}); //

    //ignore invalid input for time shift control:
    wxTextValidator inputValidator(wxFILTER_DIGITS | wxFILTER_INCLUDE_CHAR_LIST);
    inputValidator.SetCharIncludes(L"+-;,: ");
    m_textCtrlTimeShift->SetValidator(inputValidator);

    //------------- filter panel --------------------------
    m_textCtrlInclude->SetMinSize({fastFromDIP(280), -1});

    assert(!contains(m_buttonClear->GetLabel(), L"&C") && !contains(m_buttonClear->GetLabel(), L"&c")); //gazillionth wxWidgets bug on OS X: Command + C mistakenly hits "&C" access key!

    m_textCtrlInclude->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(ConfigDialog::onFilterKeyEvent), nullptr, this);
    m_textCtrlExclude->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(ConfigDialog::onFilterKeyEvent), nullptr, this);

    m_staticTextFilterDescr->Wrap(fastFromDIP(450));

    enumTimeDescr_.
    add(UnitTime::NONE, L"(" + _("None") + L")"). //meta options should be enclosed in parentheses
    add(UnitTime::TODAY,       _("Today")).
    //add(UnitTime::THIS_WEEK,   _("This week")).
    add(UnitTime::THIS_MONTH,  _("This month")).
    add(UnitTime::THIS_YEAR,   _("This year")).
    add(UnitTime::LAST_X_DAYS, _("Last x days:"));

    enumSizeDescr_.
    add(UnitSize::NONE, L"(" + _("None") + L")"). //meta options should be enclosed in parentheses
    add(UnitSize::BYTE, _("Byte")).
    add(UnitSize::KB,   _("KB")).
    add(UnitSize::MB,   _("MB"));

    //------------- synchronization panel -----------------
    m_toggleBtnTwoWay->SetLabel(getVariantName(DirectionConfig::TWO_WAY));
    m_toggleBtnMirror->SetLabel(getVariantName(DirectionConfig::MIRROR));
    m_toggleBtnUpdate->SetLabel(getVariantName(DirectionConfig::UPDATE));
    m_toggleBtnCustom->SetLabel(getVariantName(DirectionConfig::CUSTOM));

    m_toggleBtnTwoWay->SetToolTip(getSyncVariantDescription(DirectionConfig::TWO_WAY));
    m_toggleBtnMirror->SetToolTip(getSyncVariantDescription(DirectionConfig::MIRROR));
    m_toggleBtnUpdate->SetToolTip(getSyncVariantDescription(DirectionConfig::UPDATE));
    m_toggleBtnCustom->SetToolTip(getSyncVariantDescription(DirectionConfig::CUSTOM));

    m_bitmapLeftOnly  ->SetBitmap(mirrorIfRtl(greyScale(getResourceImage(L"cat_left_only"  ))));
    m_bitmapRightOnly ->SetBitmap(mirrorIfRtl(greyScale(getResourceImage(L"cat_right_only" ))));
    m_bitmapLeftNewer ->SetBitmap(mirrorIfRtl(greyScale(getResourceImage(L"cat_left_newer" ))));
    m_bitmapRightNewer->SetBitmap(mirrorIfRtl(greyScale(getResourceImage(L"cat_right_newer"))));
    m_bitmapDifferent ->SetBitmap(mirrorIfRtl(greyScale(getResourceImage(L"cat_different"  ))));
    m_bitmapConflict  ->SetBitmap(mirrorIfRtl(greyScale(getResourceImage(L"cat_conflict"   ))));

    setRelativeFontSize(*m_toggleBtnTwoWay, 1.25);
    setRelativeFontSize(*m_toggleBtnMirror, 1.25);
    setRelativeFontSize(*m_toggleBtnUpdate, 1.25);
    setRelativeFontSize(*m_toggleBtnCustom, 1.25);

    m_staticTextSyncVarDescription->SetMinSize({fastFromDIP(CFG_DESCRIPTION_WIDTH_DIP), -1});

    m_toggleBtnRecycler  ->SetToolTip(_("Retain deleted and overwritten files in the recycle bin"));
    m_toggleBtnPermanent ->SetToolTip(_("Delete and overwrite files permanently"));
    m_toggleBtnVersioning->SetToolTip(_("Move files to a user-defined folder"));

    enumVersioningStyle_.
    add(VersioningStyle::replace,          _("Replace"),    _("Move files and replace if existing")).
    add(VersioningStyle::timestampFolder, _("Time stamp") + L" [" + _("Folder") + L"]", _("Move files into a time-stamped subfolder")).
    add(VersioningStyle::timestampFile,   _("Time stamp") + L" [" + _("File")   + L"]", _("Append a time stamp to each file name"));

    m_spinCtrlVersionMaxDays ->SetMinSize({fastFromDIP(60), -1}); //
    m_spinCtrlVersionCountMin->SetMinSize({fastFromDIP(60), -1}); //Hack: set size (why does wxWindow::Size() not work?)
    m_spinCtrlVersionCountMax->SetMinSize({fastFromDIP(60), -1}); //

    m_versioningFolderPath->setHistory(std::make_shared<HistoryList>(versioningFolderHistory, folderHistoryMax));

    m_logFolderPath->setHistory(std::make_shared<HistoryList>(logFolderHistory, folderHistoryMax));

    m_comboBoxEmail->SetHint(/*_("Example:") + */ L"john.doe@example.com");
    m_comboBoxEmail->setHistory(emailHistory, emailHistoryMax);

    m_checkBoxSendEmail         ->Enable(enableExtraFeatures_);
    m_comboBoxEmail             ->Enable(enableExtraFeatures_);
    m_bpButtonEmailAlways       ->Enable(enableExtraFeatures_);
    m_bpButtonEmailErrorWarning ->Enable(enableExtraFeatures_);
    m_bpButtonEmailErrorOnly    ->Enable(enableExtraFeatures_);

    //m_staticTextPostSync->SetMinSize({fastFromDIP(180), -1});

    enumPostSyncCondition_.
    add(PostSyncCondition::COMPLETION, _("On completion:")).
    add(PostSyncCondition::ERRORS,     _("On errors:")).
    add(PostSyncCondition::SUCCESS,    _("On success:"));

    m_comboBoxPostSyncCommand->SetHint(_("Example:") + L" systemctl poweroff");

    m_comboBoxPostSyncCommand->setHistory(commandHistory, commandHistoryMax);
    //-----------------------------------------------------

    //enable dialog-specific key events
    Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(ConfigDialog::onLocalKeyEvent), nullptr, this);

    assert(!m_listBoxFolderPair->IsSorted());

    m_listBoxFolderPair->Append(_("Main config"));
    for (const LocalPairConfig& lpc : localPairConfig)
    {
        std::wstring fpName = getShortDisplayNameForFolderPair(createAbstractPath(lpc.folderPathPhraseLeft ),
                                                               createAbstractPath(lpc.folderPathPhraseRight));
        if (trimCpy(fpName).empty())
            fpName = L"<" + _("empty") + L">";

        m_listBoxFolderPair->Append(L"     " + fpName);
    }

    if (!showMultipleCfgs)
    {
        m_listBoxFolderPair->Hide();
        m_staticTextFolderPairLabel->Hide();
    }

    //temporarily set main config as reference for window height calculations:
    globalPairCfg_ = GlobalPairConfig();
    globalPairCfg_.syncCfg.directionCfg.var = DirectionConfig::MIRROR;        //
    globalPairCfg_.syncCfg.handleDeletion   = DeletionPolicy::versioning;     //
    globalPairCfg_.syncCfg.versioningFolderPhrase = Zstr("dummy");            //set tentatively for sync dir height calculation below
    globalPairCfg_.syncCfg.versioningStyle  = VersioningStyle::timestampFile; //
    globalPairCfg_.syncCfg.versionMaxAgeDays = 30;                            //
    globalPairCfg_.miscCfg.altLogFolderPathPhrase = Zstr("dummy");            //
    globalPairCfg_.miscCfg.emailNotifyAddress     = Zstr("dummy");            //

    selectFolderPairConfig(-1);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    //keep stable sizer height: "two way" description is smaller than grid of sync directions
    bSizerSyncDirHolder   ->SetMinSize(-1, bSizerSyncDirections  ->GetSize().y);
    bSizerVersioningHolder->SetMinSize(-1, bSizerVersioningHolder->GetSize().y);

    unselectFolderPairConfig(false /*validateParams*/);
    globalPairCfg_ = globalPairCfg; //restore proper value

    //set actual sync config
    selectFolderPairConfig(localPairIndexToShow);

    //more useful and Enter is redirected to m_buttonOkay anyway:
    (m_listBoxFolderPair->IsShown() ? static_cast<wxWindow*>(m_listBoxFolderPair) : m_notebook)->SetFocus();
}


void ConfigDialog::onLocalKeyEvent(wxKeyEvent& event) //process key events without explicit menu entry :)
{
    auto changeSelection = [&](SyncConfigPanel panel)
    {
        m_notebook->ChangeSelection(static_cast<size_t>(panel));
        (m_listBoxFolderPair->IsShown() ? static_cast<wxWindow*>(m_listBoxFolderPair) : m_notebook)->SetFocus(); //GTK ignores F-keys if focus is on hidden item!
    };

    switch (event.GetKeyCode())
    {
        case WXK_F6:
            changeSelection(SyncConfigPanel::COMPARISON);
            return; //handled!
        case WXK_F7:
            changeSelection(SyncConfigPanel::FILTER);
            return;
        case WXK_F8:
            changeSelection(SyncConfigPanel::SYNC);
            return;
    }
    event.Skip();
}


void ConfigDialog::onListBoxKeyEvent(wxKeyEvent& event)
{
    int keyCode = event.GetKeyCode();
    if (m_listBoxFolderPair->GetLayoutDirection() == wxLayout_RightToLeft)
    {
        if (keyCode == WXK_LEFT || keyCode == WXK_NUMPAD_LEFT)
            keyCode = WXK_RIGHT;
        else if (keyCode == WXK_RIGHT || keyCode == WXK_NUMPAD_RIGHT)
            keyCode = WXK_LEFT;
    }

    switch (keyCode)
    {
        case WXK_LEFT:
        case WXK_NUMPAD_LEFT:
            switch (static_cast<SyncConfigPanel>(m_notebook->GetSelection()))
            {
                case SyncConfigPanel::COMPARISON:
                    break;
                case SyncConfigPanel::FILTER:
                    m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::COMPARISON));
                    break;
                case SyncConfigPanel::SYNC:
                    m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::FILTER));
                    break;
            }
            m_listBoxFolderPair->SetFocus(); //needed! wxNotebook::ChangeSelection() leads to focus change!
            return; //handled!

        case WXK_RIGHT:
        case WXK_NUMPAD_RIGHT:
            switch (static_cast<SyncConfigPanel>(m_notebook->GetSelection()))
            {
                case SyncConfigPanel::COMPARISON:
                    m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::FILTER));
                    break;
                case SyncConfigPanel::FILTER:
                    m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::SYNC));
                    break;
                case SyncConfigPanel::SYNC:
                    break;
            }
            m_listBoxFolderPair->SetFocus();
            return; //handled!
    }

    event.Skip();
}


void ConfigDialog::OnSelectFolderPair(wxCommandEvent& event)
{
    assert(!m_listBoxFolderPair->HasMultipleSelection()); //single-choice!
    const int selPos = event.GetSelection();
    assert(0 <= selPos && selPos < makeSigned(m_listBoxFolderPair->GetCount()));

    //m_listBoxFolderPair has no parameter ownership! => selectedPairIndexToShow has!

    if (!unselectFolderPairConfig(true /*validateParams*/))
    {
        //restore old selection:
        m_listBoxFolderPair->SetSelection(selectedPairIndexToShow_ + 1);
        return;
    }
    selectFolderPairConfig(selPos - 1);
}


void ConfigDialog::OnCompByTimeSizeDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnCompByTimeSize(dummy);
    OnOkay(dummy);
}


void ConfigDialog::OnCompBySizeDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnCompBySize(dummy);
    OnOkay(dummy);
}


void ConfigDialog::OnCompByContentDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnCompByContent(dummy);
    OnOkay(dummy);
}


std::optional<CompConfig> ConfigDialog::getCompConfig() const
{
    if (!m_checkBoxUseLocalCmpOptions->GetValue())
        return {};

    CompConfig compCfg;
    compCfg.compareVar = localCmpVar_;
    compCfg.handleSymlinks = !m_checkBoxSymlinksInclude->GetValue() ? SymLinkHandling::EXCLUDE : m_radioBtnSymlinksDirect->GetValue() ? SymLinkHandling::DIRECT : SymLinkHandling::FOLLOW;
    compCfg.ignoreTimeShiftMinutes = fromTimeShiftPhrase(copyStringTo<std::wstring>(m_textCtrlTimeShift->GetValue()));

    return compCfg;
}


void ConfigDialog::setCompConfig(const CompConfig* compCfg)
{
    m_checkBoxUseLocalCmpOptions->SetValue(compCfg);

    //when local settings are inactive, display (current) global settings instead:
    const CompConfig tmpCfg = compCfg ? *compCfg : globalPairCfg_.cmpCfg;

    localCmpVar_ = tmpCfg.compareVar;

    switch (tmpCfg.handleSymlinks)
    {
        case SymLinkHandling::EXCLUDE:
            m_checkBoxSymlinksInclude->SetValue(false);
            m_radioBtnSymlinksFollow ->SetValue(true);
            break;
        case SymLinkHandling::FOLLOW:
            m_checkBoxSymlinksInclude->SetValue(true);
            m_radioBtnSymlinksFollow->SetValue(true);
            break;
        case SymLinkHandling::DIRECT:
            m_checkBoxSymlinksInclude->SetValue(true);
            m_radioBtnSymlinksDirect->SetValue(true);
            break;
    }

    m_textCtrlTimeShift->ChangeValue(toTimeShiftPhrase(tmpCfg.ignoreTimeShiftMinutes));

    updateCompGui();
}


void ConfigDialog::updateCompGui()
{
    const bool compOptionsEnabled = m_checkBoxUseLocalCmpOptions->GetValue();

    m_panelComparisonSettings->Enable(compOptionsEnabled);

    m_notebook->SetPageImage(static_cast<size_t>(SyncConfigPanel::COMPARISON),
                             static_cast<int>(compOptionsEnabled ? ConfigTypeImage::COMPARISON : ConfigTypeImage::COMPARISON_GREY));

    //update toggle buttons -> they have no parameter-ownership at all!
    m_toggleBtnByTimeSize->SetValue(false);
    m_toggleBtnBySize    ->SetValue(false);
    m_toggleBtnByContent ->SetValue(false);

    if (compOptionsEnabled) //help wxWidgets a little to render inactive config state (needed on Windows, NOT on Linux!)
        switch (localCmpVar_)
        {
            case CompareVariant::timeSize:
                m_toggleBtnByTimeSize->SetValue(true);
                break;
            case CompareVariant::content:
                m_toggleBtnByContent->SetValue(true);
                break;
            case CompareVariant::size:
                m_toggleBtnBySize->SetValue(true);
                break;
        }

    switch (localCmpVar_) //unconditionally update image, including "local options off"
    {
        case CompareVariant::timeSize:
            //help wxWidgets a little to render inactive config state (needed on Windows, NOT on Linux!)
            m_bitmapCompVariant->SetBitmap(greyScaleIfDisabled(getResourceImage(L"cmp_file_time"), compOptionsEnabled));
            break;
        case CompareVariant::content:
            m_bitmapCompVariant->SetBitmap(greyScaleIfDisabled(getResourceImage(L"cmp_file_content"), compOptionsEnabled));
            break;
        case CompareVariant::size:
            m_bitmapCompVariant->SetBitmap(greyScaleIfDisabled(getResourceImage(L"cmp_file_size"), compOptionsEnabled));
            break;
    }

    //active variant description:
    setText(*m_staticTextCompVarDescription, getCompVariantDescription(localCmpVar_));
    m_staticTextCompVarDescription->Wrap(fastFromDIP(CFG_DESCRIPTION_WIDTH_DIP)); //needs to be reapplied after SetLabel()

    m_radioBtnSymlinksDirect->Enable(m_checkBoxSymlinksInclude->GetValue() && compOptionsEnabled); //help wxWidgets a little to render inactive config state (needed on Windows, NOT on Linux!)
    m_radioBtnSymlinksFollow->Enable(m_checkBoxSymlinksInclude->GetValue() && compOptionsEnabled); //
}


void ConfigDialog::onFilterKeyEvent(wxKeyEvent& event)
{
    const int keyCode = event.GetKeyCode();

    if (event.ControlDown())
        switch (keyCode)
        {
            case 'A': //CTRL + A
                if (auto textCtrl = dynamic_cast<wxTextCtrl*>(event.GetEventObject()))
                    textCtrl->SetSelection(-1, -1); //select all
                return;
        }

    event.Skip();
}


FilterConfig ConfigDialog::getFilterConfig() const
{
    const Zstring& includeFilter = utfTo<Zstring>(m_textCtrlInclude->GetValue());
    const Zstring& exludeFilter  = utfTo<Zstring>(m_textCtrlExclude->GetValue());

    return FilterConfig(includeFilter, exludeFilter,
                        m_spinCtrlTimespan->GetValue(),
                        getEnumVal(enumTimeDescr_, *m_choiceUnitTimespan),
                        m_spinCtrlMinSize->GetValue(),
                        getEnumVal(enumSizeDescr_, *m_choiceUnitMinSize),
                        m_spinCtrlMaxSize->GetValue(),
                        getEnumVal(enumSizeDescr_, *m_choiceUnitMaxSize));
}


void ConfigDialog::setFilterConfig(const FilterConfig& filter)
{
    m_textCtrlInclude->ChangeValue(utfTo<wxString>(filter.includeFilter));
    m_textCtrlExclude->ChangeValue(utfTo<wxString>(filter.excludeFilter));

    setEnumVal(enumTimeDescr_, *m_choiceUnitTimespan, filter.unitTimeSpan);
    setEnumVal(enumSizeDescr_, *m_choiceUnitMinSize,  filter.unitSizeMin);
    setEnumVal(enumSizeDescr_, *m_choiceUnitMaxSize,  filter.unitSizeMax);

    m_spinCtrlTimespan->SetValue(static_cast<int>(filter.timeSpan));
    m_spinCtrlMinSize ->SetValue(static_cast<int>(filter.sizeMin));
    m_spinCtrlMaxSize ->SetValue(static_cast<int>(filter.sizeMax));

    updateFilterGui();
}


void ConfigDialog::updateFilterGui()
{
    const FilterConfig activeCfg = getFilterConfig();

    m_notebook->SetPageImage(static_cast<size_t>(SyncConfigPanel::FILTER),
                             static_cast<int>(!isNullFilter(activeCfg) ? ConfigTypeImage::FILTER: ConfigTypeImage::FILTER_GREY));

    m_bitmapInclude   ->SetBitmap(greyScaleIfDisabled(getResourceImage(L"filter_include"), !NameFilter::isNull(activeCfg.includeFilter, FilterConfig().excludeFilter)));
    m_bitmapExclude   ->SetBitmap(greyScaleIfDisabled(getResourceImage(L"filter_exclude"), !NameFilter::isNull(FilterConfig().includeFilter, activeCfg.excludeFilter)));
    m_bitmapFilterDate->SetBitmap(greyScaleIfDisabled(getResourceImage(L"cmp_file_time"), activeCfg.unitTimeSpan != UnitTime::NONE));
    m_bitmapFilterSize->SetBitmap(greyScaleIfDisabled(getResourceImage(L"cmp_file_size"), activeCfg.unitSizeMin  != UnitSize::NONE || activeCfg.unitSizeMax != UnitSize::NONE));

    m_spinCtrlTimespan->Enable(activeCfg.unitTimeSpan == UnitTime::LAST_X_DAYS);
    m_spinCtrlMinSize ->Enable(activeCfg.unitSizeMin != UnitSize::NONE);
    m_spinCtrlMaxSize ->Enable(activeCfg.unitSizeMax != UnitSize::NONE);

    m_buttonClear->Enable(!(activeCfg == FilterConfig()));
}


void ConfigDialog::OnSyncTwoWayDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnSyncTwoWay(dummy);
    OnOkay(dummy);
}


void ConfigDialog::OnSyncMirrorDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnSyncMirror(dummy);
    OnOkay(dummy);
}


void ConfigDialog::OnSyncUpdateDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnSyncUpdate(dummy);
    OnOkay(dummy);
}


void ConfigDialog::OnSyncCustomDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    OnSyncCustom(dummy);
    OnOkay(dummy);
}


void toggleSyncDirection(SyncDirection& current)
{
    switch (current)
    {
        case SyncDirection::RIGHT:
            current = SyncDirection::LEFT;
            break;
        case SyncDirection::LEFT:
            current = SyncDirection::NONE;
            break;
        case SyncDirection::NONE:
            current = SyncDirection::RIGHT;
            break;
    }
}


void toggleCustomSyncConfig(DirectionConfig& directionCfg, SyncDirection& custSyncDir)
{
    switch (directionCfg.var)
    {
        case DirectionConfig::TWO_WAY:
            assert(false);
            break;
        case DirectionConfig::MIRROR:
        case DirectionConfig::UPDATE:
            directionCfg.custom = extractDirections(directionCfg);
            break;
        case DirectionConfig::CUSTOM:
            break;
    }
    SyncDirection syncDirOld = custSyncDir;
    toggleSyncDirection(custSyncDir);

    //some config optimization: if custom settings happen to match "mirror" or "update", just switch variant
    const DirectionSet mirrorSet = []
    {
        DirectionConfig mirrorCfg;
        mirrorCfg.var = DirectionConfig::MIRROR;
        return extractDirections(mirrorCfg);
    }();

    const DirectionSet updateSet = []
    {
        DirectionConfig updateCfg;
        updateCfg.var = DirectionConfig::UPDATE;
        return extractDirections(updateCfg);
    }();

    if (directionCfg.custom == mirrorSet)
    {
        directionCfg.var = DirectionConfig::MIRROR;
        custSyncDir = syncDirOld;
    }
    else if (directionCfg.custom == updateSet)
    {
        directionCfg.var = DirectionConfig::UPDATE;
        custSyncDir = syncDirOld;
    }
    else
        directionCfg.var = DirectionConfig::CUSTOM;
}


void ConfigDialog::OnExLeftSideOnly(wxCommandEvent& event)
{
    toggleCustomSyncConfig(directionCfg_, directionCfg_.custom.exLeftSideOnly);
    updateSyncGui();
}


void ConfigDialog::OnExRightSideOnly(wxCommandEvent& event)
{
    toggleCustomSyncConfig(directionCfg_, directionCfg_.custom.exRightSideOnly);
    updateSyncGui();
}


void ConfigDialog::OnLeftNewer(wxCommandEvent& event)
{
    toggleCustomSyncConfig(directionCfg_, directionCfg_.custom.leftNewer);
    updateSyncGui();
}


void ConfigDialog::OnRightNewer(wxCommandEvent& event)
{
    toggleCustomSyncConfig(directionCfg_, directionCfg_.custom.rightNewer);
    updateSyncGui();
}


void ConfigDialog::OnDifferent(wxCommandEvent& event)
{
    toggleCustomSyncConfig(directionCfg_, directionCfg_.custom.different);
    updateSyncGui();
}


void ConfigDialog::OnConflict(wxCommandEvent& event)
{
    toggleCustomSyncConfig(directionCfg_, directionCfg_.custom.conflict);
    updateSyncGui();
}


void updateSyncDirectionIcons(const DirectionConfig& directionCfg,
                              wxBitmapButton& buttonLeftOnly,
                              wxBitmapButton& buttonRightOnly,
                              wxBitmapButton& buttonLeftNewer,
                              wxBitmapButton& buttonRightNewer,
                              wxBitmapButton& buttonDifferent,
                              wxBitmapButton& buttonConflict)
{
    if (directionCfg.var != DirectionConfig::TWO_WAY) //automatic mode needs no sync-directions
    {
        auto updateButton = [](wxBitmapButton& button, SyncDirection dir,
                               const wchar_t* imgNameLeft, const wchar_t* imgNameNone, const wchar_t* imgNameRight,
                               SyncOperation opLeft, SyncOperation opNone, SyncOperation opRight)
        {
            switch (dir)
            {
                case SyncDirection::LEFT:
                    button.SetBitmapLabel(mirrorIfRtl(getResourceImage(imgNameLeft)));
                    button.SetToolTip(getSyncOpDescription(opLeft));
                    break;
                case SyncDirection::NONE:
                    button.SetBitmapLabel(mirrorIfRtl(getResourceImage(imgNameNone)));
                    button.SetToolTip(getSyncOpDescription(opNone));
                    break;
                case SyncDirection::RIGHT:
                    button.SetBitmapLabel(mirrorIfRtl(getResourceImage(imgNameRight)));
                    button.SetToolTip(getSyncOpDescription(opRight));
                    break;
            }
            button.SetBitmapDisabled(greyScale(button.GetBitmap())); //fix wxWidgets' all-too-clever multi-state!
            //=> the disabled bitmap is generated during first SetBitmapLabel() call but never updated again by wxWidgets!
        };

        const DirectionSet dirCfg = extractDirections(directionCfg);

        updateButton(buttonLeftOnly,   dirCfg.exLeftSideOnly,  L"so_delete_left", L"so_none", L"so_create_right", SO_DELETE_LEFT,     SO_DO_NOTHING, SO_CREATE_NEW_RIGHT);
        updateButton(buttonRightOnly,  dirCfg.exRightSideOnly, L"so_create_left", L"so_none", L"so_delete_right", SO_CREATE_NEW_LEFT, SO_DO_NOTHING, SO_DELETE_RIGHT    );
        updateButton(buttonLeftNewer,  dirCfg.leftNewer,       L"so_update_left", L"so_none", L"so_update_right", SO_OVERWRITE_LEFT,  SO_DO_NOTHING, SO_OVERWRITE_RIGHT );
        updateButton(buttonRightNewer, dirCfg.rightNewer,      L"so_update_left", L"so_none", L"so_update_right", SO_OVERWRITE_LEFT,  SO_DO_NOTHING, SO_OVERWRITE_RIGHT );
        updateButton(buttonDifferent,  dirCfg.different,       L"so_update_left", L"so_none", L"so_update_right", SO_OVERWRITE_LEFT,  SO_DO_NOTHING, SO_OVERWRITE_RIGHT );

        switch (dirCfg.conflict)
        {
            case SyncDirection::LEFT:
                buttonConflict.SetBitmapLabel(mirrorIfRtl(getResourceImage(L"so_update_left")));
                buttonConflict.SetToolTip(getSyncOpDescription(SO_OVERWRITE_LEFT));
                break;
            case SyncDirection::NONE:
                buttonConflict.SetBitmapLabel(mirrorIfRtl(getResourceImage(L"cat_conflict"))); //silent dependency from algorithm.cpp::Redetermine!!!
                buttonConflict.SetToolTip(_("Leave as unresolved conflict"));
                break;
            case SyncDirection::RIGHT:
                buttonConflict.SetBitmapLabel(mirrorIfRtl(getResourceImage(L"so_update_right")));
                buttonConflict.SetToolTip(getSyncOpDescription(SO_OVERWRITE_RIGHT));
                break;
        }
        buttonConflict.SetBitmapDisabled(greyScale(buttonConflict.GetBitmap())); //fix wxWidgets' all-too-clever multi-state!
    }
}


std::optional<SyncConfig> ConfigDialog::getSyncConfig() const
{
    if (!m_checkBoxUseLocalSyncOptions->GetValue())
        return {};

    SyncConfig syncCfg;
    syncCfg.directionCfg           = directionCfg_;
    syncCfg.handleDeletion         = handleDeletion_;
    syncCfg.versioningFolderPhrase = versioningFolder_.getPath();
    syncCfg.versioningStyle        = getEnumVal(enumVersioningStyle_, *m_choiceVersioningStyle);
    if (syncCfg.versioningStyle != VersioningStyle::replace)
    {
        syncCfg.versionMaxAgeDays = m_checkBoxVersionMaxDays ->GetValue() ? m_spinCtrlVersionMaxDays->GetValue() : 0;
        syncCfg.versionCountMin   = m_checkBoxVersionCountMin->GetValue() && m_checkBoxVersionMaxDays->GetValue() ? m_spinCtrlVersionCountMin->GetValue() : 0;
        syncCfg.versionCountMax   = m_checkBoxVersionCountMax->GetValue() ? m_spinCtrlVersionCountMax->GetValue() : 0;
    }
    return syncCfg;
}


void ConfigDialog::setSyncConfig(const SyncConfig* syncCfg)
{
    m_checkBoxUseLocalSyncOptions->SetValue(syncCfg);

    //when local settings are inactive, display (current) global settings instead:
    const SyncConfig tmpCfg = syncCfg ? *syncCfg : globalPairCfg_.syncCfg;

    directionCfg_   = tmpCfg.directionCfg; //make working copy; ownership *not* on GUI
    handleDeletion_ = tmpCfg.handleDeletion;
    versioningFolder_.setPath(tmpCfg.versioningFolderPhrase);
    setEnumVal(enumVersioningStyle_, *m_choiceVersioningStyle, tmpCfg.versioningStyle);

    const bool useVersionLimits = tmpCfg.versioningStyle != VersioningStyle::replace;

    m_checkBoxVersionMaxDays ->SetValue(useVersionLimits && tmpCfg.versionMaxAgeDays > 0);
    m_checkBoxVersionCountMin->SetValue(useVersionLimits && tmpCfg.versionCountMin > 0 && tmpCfg.versionMaxAgeDays > 0);
    m_checkBoxVersionCountMax->SetValue(useVersionLimits && tmpCfg.versionCountMax > 0);

    m_spinCtrlVersionMaxDays ->SetValue(m_checkBoxVersionMaxDays ->GetValue() ? tmpCfg.versionMaxAgeDays : 30);
    m_spinCtrlVersionCountMin->SetValue(m_checkBoxVersionCountMin->GetValue() ? tmpCfg.versionCountMin : 1);
    m_spinCtrlVersionCountMax->SetValue(m_checkBoxVersionCountMax->GetValue() ? tmpCfg.versionCountMax : 1);

    updateSyncGui();
}


void ConfigDialog::updateSyncGui()
{

    const bool syncOptionsEnabled = m_checkBoxUseLocalSyncOptions->GetValue();

    m_panelSyncSettings->Enable(syncOptionsEnabled);

    m_notebook->SetPageImage(static_cast<size_t>(SyncConfigPanel::SYNC),
                             static_cast<int>(syncOptionsEnabled ? ConfigTypeImage::SYNC: ConfigTypeImage::SYNC_GREY));

    updateSyncDirectionIcons(directionCfg_,
                             *m_bpButtonLeftOnly,
                             *m_bpButtonRightOnly,
                             *m_bpButtonLeftNewer,
                             *m_bpButtonRightNewer,
                             *m_bpButtonDifferent,
                             *m_bpButtonConflict);

    //selecting "detect move files" does not always make sense:
    m_checkBoxDetectMove->Enable(detectMovedFilesSelectable(directionCfg_));
    m_checkBoxDetectMove->SetValue(detectMovedFilesEnabled(directionCfg_)); //parameter NOT owned by checkbox!

    //display only relevant sync options
    bSizerDatabase      ->Show(directionCfg_.var == DirectionConfig::TWO_WAY);
    bSizerSyncDirections->Show(directionCfg_.var != DirectionConfig::TWO_WAY);

    if (directionCfg_.var == DirectionConfig::TWO_WAY)
        m_bitmapDatabase->SetBitmap(greyScaleIfDisabled(getResourceImage(L"database"), syncOptionsEnabled));
    else
    {
        const CompareVariant activeCmpVar = m_checkBoxUseLocalCmpOptions->GetValue() ? localCmpVar_ : globalPairCfg_.cmpCfg.compareVar;

        m_bitmapLeftNewer   ->Show(activeCmpVar == CompareVariant::timeSize);
        m_bpButtonLeftNewer ->Show(activeCmpVar == CompareVariant::timeSize);
        m_bitmapRightNewer  ->Show(activeCmpVar == CompareVariant::timeSize);
        m_bpButtonRightNewer->Show(activeCmpVar == CompareVariant::timeSize);

        m_bitmapDifferent  ->Show(activeCmpVar == CompareVariant::content || activeCmpVar == CompareVariant::size);
        m_bpButtonDifferent->Show(activeCmpVar == CompareVariant::content || activeCmpVar == CompareVariant::size);
    }

    //active variant description:
    setText(*m_staticTextSyncVarDescription, getSyncVariantDescription(directionCfg_.var));
    m_staticTextSyncVarDescription->Wrap(fastFromDIP(CFG_DESCRIPTION_WIDTH_DIP)); //needs to be reapplied after SetLabel()

    //update toggle buttons -> they have no parameter-ownership at all!
    m_toggleBtnTwoWay->SetValue(false);
    m_toggleBtnMirror->SetValue(false);
    m_toggleBtnUpdate->SetValue(false);
    m_toggleBtnCustom->SetValue(false);

    if (syncOptionsEnabled) //help wxWidgets a little to render inactive config state (needed on Windows, NOT on Linux!)
        switch (directionCfg_.var)
        {
            case DirectionConfig::TWO_WAY:
                m_toggleBtnTwoWay->SetValue(true);
                break;
            case DirectionConfig::MIRROR:
                m_toggleBtnMirror->SetValue(true);
                break;
            case DirectionConfig::UPDATE:
                m_toggleBtnUpdate->SetValue(true);
                break;
            case DirectionConfig::CUSTOM:
                m_toggleBtnCustom->SetValue(true);
                break;
        }

    m_toggleBtnRecycler  ->SetValue(false);
    m_toggleBtnPermanent ->SetValue(false);
    m_toggleBtnVersioning->SetValue(false);

    if (syncOptionsEnabled) //help wxWidgets a little to render inactive config state (needed on Windows, NOT on Linux!)
        switch (handleDeletion_) //unconditionally update image, including "local options off"
        {
            case DeletionPolicy::recycler:
                m_toggleBtnRecycler->SetValue(true);
                break;
            case DeletionPolicy::permanent:
                m_toggleBtnPermanent->SetValue(true);
                break;
            case DeletionPolicy::versioning:
                m_toggleBtnVersioning->SetValue(true);
                break;
        }

    switch (handleDeletion_) //unconditionally update image, including "local options off"
    {
        case DeletionPolicy::recycler:
            m_bitmapDeletionType->SetBitmap(greyScaleIfDisabled(getResourceImage(L"delete_recycler"), syncOptionsEnabled));
            setText(*m_staticTextDeletionTypeDescription, _("Retain deleted and overwritten files in the recycle bin"));
            break;
        case DeletionPolicy::permanent:
            m_bitmapDeletionType->SetBitmap(greyScaleIfDisabled(getResourceImage(L"delete_permanently"), syncOptionsEnabled));
            setText(*m_staticTextDeletionTypeDescription, _("Delete and overwrite files permanently"));
            break;
        case DeletionPolicy::versioning:
            m_bitmapVersioning->SetBitmap(greyScaleIfDisabled(getResourceImage(L"delete_versioning"), syncOptionsEnabled));
            break;
    }
    //m_staticTextDeletionTypeDescription->Wrap(fastFromDIP(200)); //needs to be reapplied after SetLabel()

    const bool versioningSelected = handleDeletion_ == DeletionPolicy::versioning;

    m_bitmapDeletionType               ->Show(!versioningSelected);
    m_staticTextDeletionTypeDescription->Show(!versioningSelected);
    m_panelVersioning                  ->Show( versioningSelected);

    if (versioningSelected)
    {
        updateTooltipEnumVal(enumVersioningStyle_, *m_choiceVersioningStyle);

        const VersioningStyle versioningStyle = getEnumVal(enumVersioningStyle_, *m_choiceVersioningStyle);
        const std::wstring pathSep = utfTo<std::wstring>(FILE_NAME_SEPARATOR);

        switch (versioningStyle)
        {
            case VersioningStyle::replace:
                setText(*m_staticTextNamingCvtPart1, pathSep + _("Folder") + pathSep + _("File") + L".doc");
                setText(*m_staticTextNamingCvtPart2Bold, L"");
                setText(*m_staticTextNamingCvtPart3, L"");
                break;

            case VersioningStyle::timestampFolder:
                setText(*m_staticTextNamingCvtPart1, pathSep);
                setText(*m_staticTextNamingCvtPart2Bold, _("YYYY-MM-DD hhmmss"));
                setText(*m_staticTextNamingCvtPart3, pathSep + _("Folder") + pathSep + _("File") + L".doc ");
                break;

            case VersioningStyle::timestampFile:
                setText(*m_staticTextNamingCvtPart1, pathSep + _("Folder") + pathSep + _("File") + L".doc ");
                setText(*m_staticTextNamingCvtPart2Bold, _("YYYY-MM-DD hhmmss"));
                setText(*m_staticTextNamingCvtPart3, L".doc");
                break;
        }

        const bool enableLimitCtrls = syncOptionsEnabled && versioningStyle != VersioningStyle::replace;
        const bool showLimitCtrls = m_checkBoxVersionMaxDays->GetValue() || m_checkBoxVersionCountMax->GetValue();
        //m_checkBoxVersionCountMin->GetValue() => irrelevant if !m_checkBoxVersionMaxDays->GetValue()!

        if (!m_checkBoxVersionMaxDays->GetValue() && m_checkBoxVersionCountMin->GetValue())
            m_checkBoxVersionCountMin->SetValue(false); //make this dependency cristal-clear (don't just disable)

        m_staticTextLimitVersions->Show(!showLimitCtrls);

        m_spinCtrlVersionMaxDays ->Show(showLimitCtrls);
        m_spinCtrlVersionCountMin->Show(showLimitCtrls);
        m_spinCtrlVersionCountMax->Show(showLimitCtrls);

        m_staticTextLimitVersions->Enable(enableLimitCtrls);
        m_checkBoxVersionMaxDays ->Enable(enableLimitCtrls);
        m_checkBoxVersionCountMin->Enable(enableLimitCtrls && m_checkBoxVersionMaxDays->GetValue());
        m_checkBoxVersionCountMax->Enable(enableLimitCtrls);

        m_spinCtrlVersionMaxDays ->Enable(enableLimitCtrls && m_checkBoxVersionMaxDays ->GetValue());
        m_spinCtrlVersionCountMin->Enable(enableLimitCtrls && m_checkBoxVersionMaxDays->GetValue() && m_checkBoxVersionCountMin->GetValue());
        m_spinCtrlVersionCountMax->Enable(enableLimitCtrls && m_checkBoxVersionCountMax->GetValue());
    }

    m_panelSyncSettings->Layout();

    //Refresh(); //removes a few artifacts when toggling display of versioning folder
}


MiscSyncConfig ConfigDialog::getMiscSyncOptions() const
{
    assert(selectedPairIndexToShow_ == -1);
    MiscSyncConfig miscCfg;

    // Avoid "fake" changed configs! =>
    // - don't touch items corresponding to paths not currently used
    // - don't store parallel ops == 1
    miscCfg.deviceParallelOps = deviceParallelOps_;
    assert(fgSizerPerf->GetItemCount() == 2 * devicesForEdit_.size());
    int i = 0;
    for (const AfsDevice& afsDevice : devicesForEdit_)
    {
        wxSpinCtrl* spinCtrlParallelOps = dynamic_cast<wxSpinCtrl*>(fgSizerPerf->GetItem(i * 2)->GetWindow());
        setDeviceParallelOps(miscCfg.deviceParallelOps, afsDevice, spinCtrlParallelOps->GetValue());
        ++i;
    }
    //----------------------------------------------------------------------------
    miscCfg.ignoreErrors        = m_checkBoxIgnoreErrors  ->GetValue();
    miscCfg.automaticRetryCount = m_checkBoxAutoRetry     ->GetValue() ? m_spinCtrlAutoRetryCount->GetValue() : 0;
    miscCfg.automaticRetryDelay = std::chrono::seconds(m_spinCtrlAutoRetryDelay->GetValue());
    //----------------------------------------------------------------------------
    miscCfg.postSyncCommand   = m_comboBoxPostSyncCommand->getValue();
    miscCfg.postSyncCondition = getEnumVal(enumPostSyncCondition_, *m_choicePostSyncCondition);
    //----------------------------------------------------------------------------
    Zstring altLogPathPhrase = logfileDir_.getPath();
    if (altLogPathPhrase.empty())
        altLogPathPhrase = Zstr(" "); //trigger error message on dialog close
    miscCfg.altLogFolderPathPhrase = m_checkBoxOverrideLogPath->GetValue() ? altLogPathPhrase : Zstring();
    //----------------------------------------------------------------------------
    Zstring emailAddress = m_comboBoxEmail->getValue();
    if (emailAddress.empty())
        emailAddress = Zstr(" "); //trigger error message on dialog close
    miscCfg.emailNotifyAddress = m_checkBoxSendEmail->GetValue() ? emailAddress : Zstring();
    miscCfg.emailNotifyCondition = emailNotifyCondition_;
    //----------------------------------------------------------------------------
    return miscCfg;
}


void ConfigDialog::setMiscSyncOptions(const MiscSyncConfig& miscCfg)
{
    assert(selectedPairIndexToShow_ == -1);

    // Avoid "fake" changed configs! =>
    //- when editting, consider only the deviceParallelOps items corresponding to the currently-used folder paths
    //- keep parallel ops == 1 only temporarily during edit
    deviceParallelOps_  = miscCfg.deviceParallelOps;

    assert(fgSizerPerf->GetItemCount() % 2 == 0);
    const int rowsToCreate = static_cast<int>(devicesForEdit_.size()) - static_cast<int>(fgSizerPerf->GetItemCount() / 2);
    if (rowsToCreate >= 0)
        for (int i = 0; i < rowsToCreate; ++i)
        {
            wxSpinCtrl* spinCtrlParallelOps = new wxSpinCtrl(m_scrolledWindowPerf, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 2000000000, 1);
            spinCtrlParallelOps->SetMinSize({fastFromDIP(60), -1}); //Hack: set size (why does wxWindow::Size() not work?)
            spinCtrlParallelOps->Enable(enableExtraFeatures_);
            fgSizerPerf->Add(spinCtrlParallelOps, 0, wxALIGN_CENTER_VERTICAL);

            wxStaticText* staticTextDevice = new wxStaticText(m_scrolledWindowPerf, wxID_ANY, wxEmptyString);
            staticTextDevice->Enable(enableExtraFeatures_);
            fgSizerPerf->Add(staticTextDevice, 0, wxALIGN_CENTER_VERTICAL);
        }
    else
        for (int i = 0; i < -rowsToCreate * 2; ++i)
            fgSizerPerf->GetItem(size_t(0))->GetWindow()->Destroy();
    assert(fgSizerPerf->GetItemCount() == 2 * devicesForEdit_.size());

    int i = 0;
    for (const AfsDevice& afsDevice : devicesForEdit_)
    {
        wxSpinCtrl*   spinCtrlParallelOps = dynamic_cast<wxSpinCtrl*>  (fgSizerPerf->GetItem(i * 2    )->GetWindow());
        wxStaticText* staticTextDevice    = dynamic_cast<wxStaticText*>(fgSizerPerf->GetItem(i * 2 + 1)->GetWindow());

        spinCtrlParallelOps->SetValue(static_cast<int>(getDeviceParallelOps(deviceParallelOps_, afsDevice)));
        staticTextDevice->SetLabel(AFS::getDisplayPath(AbstractPath(afsDevice, AfsPath())));
        ++i;
    }
    m_staticTextPerfParallelOps->Enable(enableExtraFeatures_ && !devicesForEdit_.empty());

    m_panelComparisonSettings->Layout(); //*after* setting text labels

    //----------------------------------------------------------------------------
    m_checkBoxIgnoreErrors  ->SetValue(miscCfg.ignoreErrors);
    m_checkBoxAutoRetry     ->SetValue(miscCfg.automaticRetryCount > 0);
    m_spinCtrlAutoRetryCount->SetValue(std::max<size_t>(miscCfg.automaticRetryCount, 0));
    m_spinCtrlAutoRetryDelay->SetValue(miscCfg.automaticRetryDelay.count());
    //----------------------------------------------------------------------------
    m_comboBoxPostSyncCommand->setValue(miscCfg.postSyncCommand);
    setEnumVal(enumPostSyncCondition_, *m_choicePostSyncCondition, miscCfg.postSyncCondition);
    //----------------------------------------------------------------------------
    m_checkBoxOverrideLogPath->SetValue(!trimCpy(miscCfg.altLogFolderPathPhrase).empty());
    logfileDir_.setPath(m_checkBoxOverrideLogPath->GetValue() ? miscCfg.altLogFolderPathPhrase : getDefaultLogFolderPath());
    //can't use logfileDir_.setBackgroundText(): no text shown when control is disabled!
    //----------------------------------------------------------------------------
    m_checkBoxSendEmail->SetValue(!trimCpy(miscCfg.emailNotifyAddress).empty());
    m_comboBoxEmail->setValue(miscCfg.emailNotifyAddress);
    emailNotifyCondition_ = miscCfg.emailNotifyCondition;
    //----------------------------------------------------------------------------
    updateMiscGui();
}


void ConfigDialog::updateMiscGui()
{
    const MiscSyncConfig miscCfg = getMiscSyncOptions();

    m_bitmapIgnoreErrors->SetBitmap(greyScaleIfDisabled(getResourceImage(L"error_ignore_active"), miscCfg.ignoreErrors));
    m_bitmapRetryErrors ->SetBitmap(greyScaleIfDisabled(getResourceImage(L"error_retry"), miscCfg.automaticRetryCount > 0 ));

    fgSizerAutoRetry->Show(miscCfg.automaticRetryCount > 0);

    m_panelComparisonSettings->Layout(); //showing "retry count" can affect bSizerPerformance!
    //----------------------------------------------------------------------------
    const bool sendEmailEnabled = m_checkBoxSendEmail->GetValue();
    m_bitmapEmail->SetBitmap(shrinkImage(greyScaleIfDisabled(getResourceImage(L"email"), sendEmailEnabled).ConvertToImage(), fastFromDIP(24)));
    m_comboBoxEmail->Show(sendEmailEnabled);

    auto updateButton = [successIcon = getResourceImage(L"msg_success_sicon").ConvertToImage(),
                                     warningIcon = getResourceImage(L"msg_warning_sicon").ConvertToImage(),
                                     errorIcon   = getResourceImage(L"msg_error_sicon"  ).ConvertToImage(),
                                     sendEmailEnabled, this] (wxBitmapButton& button, ResultsNotification notifyCondition)
    {
        button.Show(sendEmailEnabled);
        if (sendEmailEnabled)
        {
            wxString tooltip = _("Error");
            wxImage label = errorIcon;

            if (notifyCondition == ResultsNotification::always ||
                notifyCondition == ResultsNotification::errorWarning)
            {
                tooltip += (L" | ") + _("Warning");
                label = stackImages(label, warningIcon, ImageStackLayout::horizontal, ImageStackAlignment::center);
            }
            else
                label.Resize({label.GetWidth() + warningIcon.GetWidth(), label.GetHeight()}, {});

            if (notifyCondition == ResultsNotification::always)
            {
                tooltip += (L" | ") + _("Success");
                label = stackImages(label, successIcon, ImageStackLayout::horizontal, ImageStackAlignment::center);
            }
            else
                label.Resize({label.GetWidth() + successIcon.GetWidth(), label.GetHeight()}, {});

            button.SetToolTip(tooltip);
            button.SetBitmapLabel(notifyCondition == emailNotifyCondition_ && sendEmailEnabled ? label : greyScale(label));
            button.SetBitmapDisabled(greyScale(label)); //fix wxWidgets' all-too-clever multi-state!
            //=> the disabled bitmap is generated during first SetBitmapLabel() call but never updated again by wxWidgets!
        }
    };
    updateButton(*m_bpButtonEmailAlways,       ResultsNotification::always);
    updateButton(*m_bpButtonEmailErrorWarning, ResultsNotification::errorWarning);
    updateButton(*m_bpButtonEmailErrorOnly,    ResultsNotification::errorOnly);

    m_staticTextPerfDeRequired2->Show(!enableExtraFeatures_); //required after each bSizerSyncMisc->Show()

    //----------------------------------------------------------------------------
    m_bitmapLogFile->SetBitmap(shrinkImage(greyScaleIfDisabled(getResourceImage(L"log_file"), m_checkBoxOverrideLogPath->GetValue()).ConvertToImage(), fastFromDIP(20)));
    m_logFolderPath             ->Enable(m_checkBoxOverrideLogPath->GetValue()); //
    m_buttonSelectLogFolder     ->Show(m_checkBoxOverrideLogPath->GetValue()); //enabled status can't be derived from resolved config!
    m_bpButtonSelectAltLogFolder->Show(m_checkBoxOverrideLogPath->GetValue()); //

    m_panelSyncSettings->Layout(); //after showing/hiding m_buttonSelectLogFolder
    m_panelLogfile->Refresh(); //removes a few artifacts when toggling email notifications
}


void ConfigDialog::selectFolderPairConfig(int newPairIndexToShow)
{
    assert(selectedPairIndexToShow_ == EMPTY_PAIR_INDEX_SELECTED);
    assert(newPairIndexToShow == -1 || makeUnsigned(newPairIndexToShow) < localPairCfg_.size());
    newPairIndexToShow = std::clamp(newPairIndexToShow, -1, static_cast<int>(localPairCfg_.size()) - 1);

    selectedPairIndexToShow_ = newPairIndexToShow;
    m_listBoxFolderPair->SetSelection(newPairIndexToShow + 1);

    //show/hide controls that are only relevant for main/local config
    const bool mainConfigSelected = newPairIndexToShow < 0;
    //comparison panel:
    m_staticTextMainCompSettings->Show( mainConfigSelected && showMultipleCfgs_);
    m_checkBoxUseLocalCmpOptions->Show(!mainConfigSelected && showMultipleCfgs_);
    m_staticlineCompHeader->Show(showMultipleCfgs_);
    //filter panel
    m_staticTextMainFilterSettings ->Show( mainConfigSelected && showMultipleCfgs_);
    m_staticTextLocalFilterSettings->Show(!mainConfigSelected && showMultipleCfgs_);
    m_staticlineFilterHeader->Show(showMultipleCfgs_);
    //sync panel:
    m_staticTextMainSyncSettings ->Show( mainConfigSelected && showMultipleCfgs_);
    m_checkBoxUseLocalSyncOptions->Show(!mainConfigSelected && showMultipleCfgs_);
    m_staticlineSyncHeader->Show(showMultipleCfgs_);
    //misc
    bSizerPerformance->Show(mainConfigSelected); //caveat: recursively shows hidden child items!
    bSizerCompMisc   ->Show(mainConfigSelected);
    bSizerSyncMisc   ->Show(mainConfigSelected);

    if (mainConfigSelected) m_staticTextPerfDeRequired->Show(!enableExtraFeatures_); //keep after bSizerPerformance->Show()
    if (mainConfigSelected) m_staticlinePerfDeRequired->Show(!enableExtraFeatures_); //

    m_panelCompSettingsTab  ->Layout(); //fix comp panel glitch on Win 7 125% font size + perf panel
    m_panelFilterSettingsTab->Layout();
    m_panelSyncSettingsTab  ->Layout();

    if (mainConfigSelected)
    {
        //update the devices list for "parallel file operations" before calling setMiscSyncOptions():
        //  => should be enough to do this when selecting the main config
        //  => to be "perfect" we'd have to update already when the user drags & drops a different versioning folder
        devicesForEdit_.clear();
        auto addDevicePath = [&](const Zstring& folderPathPhrase)
        {
            const AfsDevice& afsDevice = createAbstractPath(folderPathPhrase).afsDevice;
            if (!AFS::isNullDevice(afsDevice))
                devicesForEdit_.insert(afsDevice);
        };
        for (const LocalPairConfig& fpCfg : localPairCfg_)
        {
            addDevicePath(fpCfg.folderPathPhraseLeft);
            addDevicePath(fpCfg.folderPathPhraseRight);

            if (fpCfg.localSyncCfg && fpCfg.localSyncCfg->handleDeletion == DeletionPolicy::versioning)
                addDevicePath(fpCfg.localSyncCfg->versioningFolderPhrase);
        }
        if (globalPairCfg_.syncCfg.handleDeletion == DeletionPolicy::versioning) //let's always add, even if *all* folder pairs use a local sync config (=> strange!)
            addDevicePath(globalPairCfg_.syncCfg.versioningFolderPhrase);
        //---------------------------------------------------------------------------------------------------------------

        setCompConfig     (&globalPairCfg_.cmpCfg);
        setSyncConfig     (&globalPairCfg_.syncCfg);
        setFilterConfig   (globalPairCfg_.filter);
        setMiscSyncOptions(globalPairCfg_.miscCfg);
    }
    else
    {
        setCompConfig  (get(localPairCfg_[selectedPairIndexToShow_].localCmpCfg ));
        setSyncConfig  (get(localPairCfg_[selectedPairIndexToShow_].localSyncCfg));
        setFilterConfig(localPairCfg_[selectedPairIndexToShow_].localFilter);
    }
}


bool ConfigDialog::unselectFolderPairConfig(bool validateParams)
{
    assert(selectedPairIndexToShow_ == -1 ||  makeUnsigned(selectedPairIndexToShow_) < localPairCfg_.size());

    std::optional<CompConfig> compCfg   = getCompConfig();
    std::optional<SyncConfig> syncCfg   = getSyncConfig();
    FilterConfig              filterCfg = getFilterConfig();

    std::optional<MiscSyncConfig> miscCfg;
    if (selectedPairIndexToShow_ < 0)
        miscCfg = getMiscSyncOptions();

    //------- parameter validation (BEFORE writing output!) -------
    if (validateParams)
    {
        //parameter correction: include filter must not be empty!
        if (trimCpy(filterCfg.includeFilter).empty())
            filterCfg.includeFilter = FilterConfig().includeFilter; //no need to show error message, just correct user input

        if (syncCfg && syncCfg->handleDeletion == DeletionPolicy::versioning)
        {
            if (AFS::isNullPath(createAbstractPath(syncCfg->versioningFolderPhrase)))
            {
                m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::SYNC));
                showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Please enter a target folder for versioning.")));
                //don't show error icon to follow "Windows' encouraging tone"
                m_versioningFolderPath->SetFocus();
                return false;
            }
            m_versioningFolderPath->getHistory()->addItem(syncCfg->versioningFolderPhrase);

            if (syncCfg->versioningStyle != VersioningStyle::replace &&
                syncCfg->versionMaxAgeDays > 0 &&
                syncCfg->versionCountMin   > 0 &&
                syncCfg->versionCountMax   > 0 &&
                syncCfg->versionCountMin >= syncCfg->versionCountMax)
            {
                m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::SYNC));
                showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Minimum version count must be smaller than maximum count.")));
                m_spinCtrlVersionCountMin->SetFocus();
                return false;
            }
        }

        if (selectedPairIndexToShow_ < 0)
        {
            if (!miscCfg->altLogFolderPathPhrase.empty() &&
                trimCpy(miscCfg->altLogFolderPathPhrase).empty())
            {
                m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::SYNC));
                showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Please enter a folder path.")));
                m_logFolderPath->SetFocus();
                return false;
            }
            m_logFolderPath->getHistory()->addItem(miscCfg->altLogFolderPathPhrase);

            if (!miscCfg->emailNotifyAddress.empty() &&
                !isValidEmail(trimCpy(miscCfg->emailNotifyAddress)))
            {
                m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::SYNC));
                showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Please enter a valid email address.")));
                m_comboBoxEmail->SetFocus();
                return false;
            }
            m_comboBoxEmail          ->addItemHistory();
            m_comboBoxPostSyncCommand->addItemHistory();
        }
    }
    //-------------------------------------------------------------

    if (selectedPairIndexToShow_ < 0)
    {
        globalPairCfg_.cmpCfg  = *compCfg;
        globalPairCfg_.syncCfg = *syncCfg;
        globalPairCfg_.filter  = filterCfg;
        globalPairCfg_.miscCfg = *miscCfg;
    }
    else
    {
        localPairCfg_[selectedPairIndexToShow_].localCmpCfg  = compCfg;
        localPairCfg_[selectedPairIndexToShow_].localSyncCfg = syncCfg;
        localPairCfg_[selectedPairIndexToShow_].localFilter  = filterCfg;
    }

    selectedPairIndexToShow_ = EMPTY_PAIR_INDEX_SELECTED;
    //m_listBoxFolderPair->SetSelection(wxNOT_FOUND); not needed, selectedPairIndexToShow has parameter ownership
    return true;
}


void ConfigDialog::OnOkay(wxCommandEvent& event)
{
    if (!unselectFolderPairConfig(true /*validateParams*/))
        return;

    globalPairCfgOut_ = globalPairCfg_;
    localPairCfgOut_  = localPairCfg_;

    versioningFolderHistoryOut_ = m_versioningFolderPath->getHistory()->getList();
    logFolderHistoryOut_        = m_logFolderPath       ->getHistory()->getList();

    commandHistoryOut_ = m_comboBoxPostSyncCommand->getHistory();
    emailHistoryOut_   = m_comboBoxEmail->getHistory();

    EndModal(ReturnSyncConfig::BUTTON_OKAY);
}
}

//########################################################################################

ReturnSyncConfig::ButtonPressed fff::showSyncConfigDlg(wxWindow* parent,
                                                       SyncConfigPanel panelToShow,
                                                       int localPairIndexToShow, bool showMultipleCfgs,

                                                       GlobalPairConfig&             globalPairCfg,
                                                       std::vector<LocalPairConfig>& localPairConfig,

                                                       std::vector<Zstring>& versioningFolderHistory,
                                                       std::vector<Zstring>& logFolderHistory,
                                                       size_t folderHistoryMax,
                                                       std::vector<Zstring>& emailHistory,   size_t emailHistoryMax,
                                                       std::vector<Zstring>& commandHistory, size_t commandHistoryMax)
{

    ConfigDialog syncDlg(parent,
                         panelToShow,
                         localPairIndexToShow, showMultipleCfgs,
                         globalPairCfg,
                         localPairConfig,
                         versioningFolderHistory,
                         logFolderHistory,
                         folderHistoryMax,
                         emailHistory,
                         emailHistoryMax,
                         commandHistory,
                         commandHistoryMax);
    return static_cast<ReturnSyncConfig::ButtonPressed>(syncDlg.ShowModal());
}

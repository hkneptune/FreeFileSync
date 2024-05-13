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
#include <wx+/context_menu.h>
#include <wx+/choice_enum.h>
#include <wx+/image_tools.h>
#include <wx+/window_layout.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "gui_generated.h"
//#include "command_box.h"
#include "folder_selector.h"
#include "../base/norm_filter.h"
#include "../base/file_hierarchy.h"
#include "../base/icon_loader.h"
#include "../afs/concrete.h"
#include "../base_tools.h"



using namespace zen;
using namespace fff;


namespace
{
const int CFG_DESCRIPTION_WIDTH_DIP = 230;
const wchar_t arrowRight[] = L"\u2192"; //"RIGHTWARDS ARROW"


void initBitmapRadioButtons(const std::vector<std::pair<ToggleButton*, std::string /*imgName*/>>& buttons, bool alignLeft)
{
    const bool physicalLeft = alignLeft == (wxTheApp->GetLayoutDirection() != wxLayout_RightToLeft);

    auto generateSelectImage = [physicalLeft](wxButton& btn, const std::string& imgName, bool selected)
    {
        wxImage imgTxt = createImageFromText(btn.GetLabelText(), btn.GetFont(),
                                             selected ? *wxBLACK : //accessibility: always set both foreground AND background colors! see renderSelectedButton()
                                             btn.GetForegroundColour());

        wxImage imgIco = mirrorIfRtl(loadImage(imgName, -1 /*maxWidth*/, dipToScreen(getMenuIconDipSize())));

        if (imgName == "delete_recycler") //use system icon if available (can fail on Linux??)
            try { imgIco = extractWxImage(fff::getTrashIcon(dipToScreen(getMenuIconDipSize()))); /*throw SysError*/ }
            catch (SysError&) { assert(false); }

        if (!selected)
            imgIco = greyScale(imgIco);

        wxImage imgStack = physicalLeft ?
                           stackImages(imgIco, imgTxt, ImageStackLayout::horizontal, ImageStackAlignment::center, dipToScreen(5)) :
                           stackImages(imgTxt, imgIco, ImageStackLayout::horizontal, ImageStackAlignment::center, dipToScreen(5));

        return resizeCanvas(imgStack, imgStack.GetSize() + wxSize(dipToScreen(14), dipToScreen(12)), wxALIGN_CENTER);
    };

    wxSize maxExtent;
    std::unordered_map<const ToggleButton*, wxImage> labelsNotSel;
    for (auto& [btn, imgName] : buttons)
    {
        wxImage img = generateSelectImage(*btn, imgName, false /*selected*/);
        maxExtent.x = std::max(maxExtent.x, img.GetWidth());
        maxExtent.y = std::max(maxExtent.y, img.GetHeight());

        labelsNotSel[btn] = std::move(img);
    }

    for (auto& [btn, imgName] : buttons)
    {
        btn->init(layOver(rectangleImage(maxExtent, getColorToggleButtonFill(), getColorToggleButtonBorder(), dipToScreen(1)),
                          generateSelectImage(*btn, imgName, true /*selected*/), wxALIGN_CENTER_VERTICAL | (physicalLeft ? wxALIGN_LEFT : wxALIGN_RIGHT)),
                  resizeCanvas(labelsNotSel[btn], maxExtent,                     wxALIGN_CENTER_VERTICAL | (physicalLeft ? wxALIGN_LEFT : wxALIGN_RIGHT)));

        btn->SetMinSize({screenToWxsize(maxExtent.x),
                         screenToWxsize(maxExtent.y)}); //get rid of selection border on Windows :)
        //SetMinSize() instead of SetSize() is needed here for wxWindows layout determination to work correctly
    }
}


bool sanitizeFilter(FilterConfig& filterCfg, const std::vector<AbstractPath>& baseFolderPaths, wxWindow* parent)
{
    //include filter must not be empty!
    if (trimCpy(filterCfg.includeFilter).empty())
        filterCfg.includeFilter = FilterConfig().includeFilter; //no need to show error message, just correct user input


    //replace full paths by relative ones: frequent user error => help out: https://freefilesync.org/forum/viewtopic.php?t=9225
    auto normalizeForSearch = [](Zstring str)
    {
        //1. ignore Unicode normalization form 2. ignore case 3. normalize path separator
        str = getUpperCase(str); //getUnicodeNormalForm() is implied by getUpperCase()

        if constexpr (FILE_NAME_SEPARATOR != Zstr('/' )) std::replace(str.begin(), str.end(), Zstr('/'),  FILE_NAME_SEPARATOR);
        if constexpr (FILE_NAME_SEPARATOR != Zstr('\\')) std::replace(str.begin(), str.end(), Zstr('\\'), FILE_NAME_SEPARATOR);

        return str;
    };

    std::vector<Zstring> folderPathsPf; //normalized + postfix path separator
    {
        const Zstring includeFilterNorm = normalizeForSearch(filterCfg.includeFilter);
        const Zstring excludeFilterNorm = normalizeForSearch(filterCfg.excludeFilter);

        for (const AbstractPath& folderPath : baseFolderPaths)
            if (!AFS::isNullPath(folderPath))
                if (const std::wstring& displayPath = AFS::getDisplayPath(folderPath);
                    !displayPath.empty())
                    if (displayPath != L"/") //Linux/macOS: https://freefilesync.org/forum/viewtopic.php?t=9713
                        if (const Zstring pathNormPf = appendSeparator(normalizeForSearch(utfTo<Zstring>(displayPath)));
                            contains(includeFilterNorm, pathNormPf) || //perf!?
                            contains(excludeFilterNorm, pathNormPf))   //
                            folderPathsPf.push_back(pathNormPf);

        removeDuplicates(folderPathsPf);
    }


    std::vector<std::pair<Zstring /*from*/, Zstring /*to*/>> replacements;

    auto replaceFullPaths = [&](Zstring& filterPhrase)
    {
        Zstring filterPhraseNew;
        const Zchar* itFilterOrig = filterPhrase.begin();

        split2(filterPhrase, [](Zchar c) { return c == FILTER_ITEM_SEPARATOR || c == Zstr('\n'); }, //delimiters
        [&](const ZstringView phrase)
        {
            const ZstringView phraseTrm = trimCpy(phrase);
            if (!phraseTrm.empty())
            {
                const Zstring phraseNorm = normalizeForSearch(Zstring{phraseTrm});

                for (const Zstring& pathNormPf : folderPathsPf)
                    if (startsWith(phraseNorm, pathNormPf))
                    {
                        //emulate a "normalized afterFirst()":
                        ptrdiff_t sepCount = std::count(pathNormPf.begin(), pathNormPf.end(), FILE_NAME_SEPARATOR);
                        assert(sepCount > 0);

                        for (auto it = phraseTrm.begin(); it != phraseTrm.end(); ++it)
                            if (*it == Zstr('/') ||
                                *it == Zstr('\\'))
                                if (--sepCount == 0)
                                {
                                    const Zstring relPath(it, phraseTrm.end()); //include first path separator

                                    filterPhraseNew.append(itFilterOrig, phraseTrm.data());
                                    filterPhraseNew += relPath;
                                    itFilterOrig = phraseTrm.data() + phraseTrm.size();

                                    replacements.emplace_back(phraseTrm, relPath);
                                    return; //... to next block
                                }
                        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
                    }
            }
        });

        if (itFilterOrig != filterPhrase.begin()) //perf!?
        {
            filterPhraseNew.append(itFilterOrig, filterPhrase.cend());
            filterPhrase = std::move(filterPhraseNew);
        }
    };
    replaceFullPaths(filterCfg.includeFilter);
    replaceFullPaths(filterCfg.excludeFilter);

    if (!replacements.empty())
    {
        std::wstring detailsMsg;
        for (const auto& [from, to] : replacements)
            if (to.empty())
                detailsMsg += _("Remove:") + L' ' + utfTo<std::wstring>(from) + L'\n';
            else
                detailsMsg += utfTo<std::wstring>(from) + L' ' + arrowRight + L' ' + utfTo<std::wstring>(to) + L'\n';
        detailsMsg.pop_back();

        switch (showConfirmationDialog(parent, DialogInfoType::info, PopupDialogCfg().
                                       setMainInstructions(_("Each filter item must be a path relative to the selected folder pairs. The following changes are suggested:")).
                                       setDetailInstructions(detailsMsg), _("&Change")))
        {
            case ConfirmationButton::accept: //change
                break;

            case ConfirmationButton::cancel:
                return false;
        }
    }
    return true;
}

//==========================================================================

class ConfigDialog : public ConfigDlgGenerated
{
public:
    ConfigDialog(wxWindow* parent,
                 SyncConfigPanel panelToShow,
                 int localPairIndexToShow, bool showMultipleCfgs,
                 GlobalPairConfig& globalPairCfg,
                 std::vector<LocalPairConfig>& localPairCfg,
                 FilterConfig& defaultFilter,
                 std::vector<Zstring>& versioningFolderHistory, Zstring& versioningFolderLastSelected,
                 std::vector<Zstring>& logFolderHistory, Zstring& logFolderLastSelected, const Zstring& globalLogFolderPhrase,
                 size_t folderHistoryMax, Zstring& sftpKeyFileLastSelected,
                 std::vector<Zstring>& emailHistory,   size_t emailHistoryMax,
                 std::vector<Zstring>& commandHistory, size_t commandHistoryMax);

    ~ConfigDialog();

private:
    void onOkay  (wxCommandEvent& event) override;
    void onCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }

    void onAddNotes(wxCommandEvent& event) override;

    void onLocalKeyEvent(wxKeyEvent& event);
    void onListBoxKeyEvent(wxKeyEvent& event) override;
    void onSelectFolderPair(wxCommandEvent& event) override;

    enum class ConfigTypeImage
    {
        compare = 0, //used as zero-based wxImageList index!
        compareGrey,
        filter,
        filterGrey,
        sync,
        syncGrey,
    };

    //------------- comparison panel ----------------------
    void onToggleLocalCompSettings(wxCommandEvent& event) override { updateCompGui(); updateSyncGui(); /*affects sync settings, too!*/ }
    void onToggleIgnoreErrors     (wxCommandEvent& event) override { updateMiscGui(); }
    void onToggleAutoRetry        (wxCommandEvent& event) override { updateMiscGui(); }

    void onCompByTimeSize      (wxCommandEvent& event) override { localCmpVar_ = CompareVariant::timeSize; updateCompGui(); updateSyncGui(); } //
    void onCompByContent       (wxCommandEvent& event) override { localCmpVar_ = CompareVariant::content;  updateCompGui(); updateSyncGui(); } //affects sync settings, too!
    void onCompBySize          (wxCommandEvent& event) override { localCmpVar_ = CompareVariant::size;     updateCompGui(); updateSyncGui(); } //
    void onCompByTimeSizeDouble(wxMouseEvent&   event) override;
    void onCompByContentDouble (wxMouseEvent&   event) override;
    void onCompBySizeDouble    (wxMouseEvent&   event) override;
    void onChangeCompOption    (wxCommandEvent& event) override { updateCompGui(); }

    std::optional<CompConfig> getCompConfig() const;
    void setCompConfig(const CompConfig* compCfg);

    void updateCompGui();

    CompareVariant localCmpVar_ = CompareVariant::timeSize;

    std::set<AfsDevice>         devicesForEdit_;     //helper data for deviceParallelOps
    std::map<AfsDevice, size_t> deviceParallelOps_;  //

    //------------- filter panel --------------------------
    void onChangeFilterOption(wxCommandEvent& event) override { updateFilterGui(); }
    void onFilterClear       (wxCommandEvent& event) override { setFilterConfig(FilterConfig()); }
    void onFilterDefault     (wxCommandEvent& event) override { setFilterConfig(defaultFilterOut_); }

    void onFilterDefaultContext     (wxCommandEvent& event) override { onFilterDefaultContext(static_cast<wxEvent&>(event)); }
    void onFilterDefaultContextMouse(wxMouseEvent&   event) override { onFilterDefaultContext(static_cast<wxEvent&>(event)); }
    void onFilterDefaultContext(wxEvent& event);

    FilterConfig getFilterConfig() const;
    void setFilterConfig(const FilterConfig& filter);

    void updateFilterGui();

    EnumDescrList<UnitTime> enumTimeDescr_;
    EnumDescrList<UnitSize> enumSizeDescr_;

    //------------- synchronization panel -----------------
    void onSyncTwoWay(wxCommandEvent& event) override { directionsCfg_ = getDefaultSyncCfg(SyncVariant::twoWay); updateSyncGui(); }
    void onSyncMirror(wxCommandEvent& event) override { directionsCfg_ = getDefaultSyncCfg(SyncVariant::mirror); updateSyncGui(); }
    void onSyncUpdate(wxCommandEvent& event) override { directionsCfg_ = getDefaultSyncCfg(SyncVariant::update); updateSyncGui(); }
    void onSyncCustom(wxCommandEvent& event) override { directionsCfg_ = getDefaultSyncCfg(SyncVariant::custom); updateSyncGui(); }

    void onToggleLocalSyncSettings(wxCommandEvent& event) override { updateSyncGui(); }
    void onToggleUseDatabase      (wxCommandEvent& event) override;
    void onChanegVersioningStyle  (wxCommandEvent& event) override { updateSyncGui(); }
    void onToggleVersioningLimit  (wxCommandEvent& event) override { updateSyncGui(); }

    void onSyncTwoWayDouble(wxMouseEvent& event) override;
    void onSyncMirrorDouble(wxMouseEvent& event) override;
    void onSyncUpdateDouble(wxMouseEvent& event) override;
    void onSyncCustomDouble(wxMouseEvent& event) override;

    void onLeftOnly  (wxCommandEvent& event) override { toggleSyncDirButton(&DirectionByDiff::leftOnly); }
    void onRightOnly (wxCommandEvent& event) override { toggleSyncDirButton(&DirectionByDiff::rightOnly); }
    void onLeftNewer (wxCommandEvent& event) override;
    void onRightNewer(wxCommandEvent& event) override;
    void onDifferent (wxCommandEvent& event) override;
    void toggleSyncDirButton(SyncDirection DirectionByDiff::* dir);

    void onLeftCreate (wxCommandEvent& event) override { toggleSyncDirButton(&DirectionByChange::left,  &DirectionByChange::Changes::create); }
    void onLeftUpdate (wxCommandEvent& event) override { toggleSyncDirButton(&DirectionByChange::left,  &DirectionByChange::Changes::update); }
    void onLeftDelete (wxCommandEvent& event) override { toggleSyncDirButton(&DirectionByChange::left,  &DirectionByChange::Changes::delete_); }
    void onRightCreate(wxCommandEvent& event) override { toggleSyncDirButton(&DirectionByChange::right, &DirectionByChange::Changes::create); }
    void onRightUpdate(wxCommandEvent& event) override { toggleSyncDirButton(&DirectionByChange::right, &DirectionByChange::Changes::update); }
    void onRightDelete(wxCommandEvent& event) override { toggleSyncDirButton(&DirectionByChange::right, &DirectionByChange::Changes::delete_); }
    void toggleSyncDirButton(DirectionByChange::Changes DirectionByChange::* side, SyncDirection DirectionByChange::Changes::* dir);

    void onDeletionPermanent (wxCommandEvent& event) override { deletionVariant_ = DeletionVariant::permanent;  updateSyncGui(); }
    void onDeletionRecycler  (wxCommandEvent& event) override { deletionVariant_ = DeletionVariant::recycler;   updateSyncGui(); }
    void onDeletionVersioning(wxCommandEvent& event) override { deletionVariant_ = DeletionVariant::versioning; updateSyncGui(); }

    void onToggleMiscOption(wxCommandEvent& event) override { updateMiscGui(); }
    void onToggleMiscEmail (wxCommandEvent& event) override
    {
        onToggleMiscOption(event);
        if (event.IsChecked())           //optimize UX
            m_comboBoxEmail->SetFocus(); //
    }
    void onEmailAlways      (wxCommandEvent& event) override { emailNotifyCondition_ = ResultsNotification::always;       updateMiscGui(); }
    void onEmailErrorWarning(wxCommandEvent& event) override { emailNotifyCondition_ = ResultsNotification::errorWarning; updateMiscGui(); }
    void onEmailErrorOnly   (wxCommandEvent& event) override { emailNotifyCondition_ = ResultsNotification::errorOnly;    updateMiscGui(); }

    void onShowLogFolder(wxCommandEvent& event) override;

    std::optional<SyncConfig> getSyncConfig() const;
    void setSyncConfig(const SyncConfig* syncCfg);

    bool leftRightNewerCombined() const;

    void updateSyncGui();
    //-----------------------------------------------------

    //parameters with ownership NOT within GUI controls!
    SyncDirectionConfig directionsCfg_;
    DeletionVariant deletionVariant_ = DeletionVariant::recycler; //use Recycler, delete permanently or move to user-defined location

    const std::function<size_t(const Zstring& folderPathPhrase)>                     getDeviceParallelOps_;
    const std::function<void  (const Zstring& folderPathPhrase, size_t parallelOps)> setDeviceParallelOps_;

    FolderSelector versioningFolder_;
    EnumDescrList<VersioningStyle> enumVersioningStyle_;

    ResultsNotification emailNotifyCondition_ = ResultsNotification::always;

    EnumDescrList<PostSyncCondition> enumPostSyncCondition_;

    FolderSelector logFolderSelector_;
    //-----------------------------------------------------

    MiscSyncConfig getMiscSyncOptions() const;
    void setMiscSyncOptions(const MiscSyncConfig& miscCfg);

    void updateMiscGui();

    //-----------------------------------------------------

    void selectFolderPairConfig(int newPairIndexToShow);
    bool unselectFolderPairConfig(bool validateParams); //returns false on error: shows message box!

    //output parameters (sync config)
    GlobalPairConfig& globalPairCfgOut_;
    std::vector<LocalPairConfig>& localPairCfgOut_;
    //output parameters (global) -> ignores OK/Cancel
    FilterConfig& defaultFilterOut_;
    std::vector<Zstring>& versioningFolderHistoryOut_;
    std::vector<Zstring>& logFolderHistoryOut_;
    std::vector<Zstring>& emailHistoryOut_;
    std::vector<Zstring>& commandHistoryOut_;

    //working copy of ALL config parameters: only one folder pair is selected at a time!
    GlobalPairConfig globalPairCfg_;
    std::vector<LocalPairConfig> localPairCfg_;

    int selectedPairIndexToShow_ = EMPTY_PAIR_INDEX_SELECTED;
    static constexpr int EMPTY_PAIR_INDEX_SELECTED = -2;

    bool showNotesPanel_ = false;

    const bool enableExtraFeatures_;
    const bool showMultipleCfgs_;

    const Zstring globalLogFolderPhrase_;
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


std::wstring getSyncVariantDescription(SyncVariant var)
{
    switch (var)
    {
        case SyncVariant::twoWay:
            return _("Identify and propagate changes on both sides. Deletions, moves and conflicts are detected automatically using a database.");
        case SyncVariant::mirror:
            return _("Create a mirror backup of the left folder by adapting the right folder to match.");
        case SyncVariant::update:
            return _("Copy new and updated files to the right folder.");
        case SyncVariant::custom:
            return _("Configure your own synchronization rules.");
    }
    assert(false);
    return _("Error");
}

//==========================================================================

ConfigDialog::ConfigDialog(wxWindow* parent,
                           SyncConfigPanel panelToShow,
                           int localPairIndexToShow, bool showMultipleCfgs,
                           GlobalPairConfig& globalPairCfg,
                           std::vector<LocalPairConfig>& localPairCfg,
                           FilterConfig& defaultFilter,
                           std::vector<Zstring>& versioningFolderHistory, Zstring& versioningFolderLastSelected,
                           std::vector<Zstring>& logFolderHistory, Zstring& logFolderLastSelected,
                           const Zstring& globalLogFolderPhrase,
                           size_t folderHistoryMax, Zstring& sftpKeyFileLastSelected,
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
    assert(selectedPairIndexToShow_ == -1 || makeUnsigned(selectedPairIndexToShow_) < localPairCfg_.size());
    if (selectedPairIndexToShow_ < 0)
    {
        MiscSyncConfig miscCfg = getMiscSyncOptions();
        setDeviceParallelOps(miscCfg.deviceParallelOps, folderPathPhrase, parallelOps);
        setMiscSyncOptions(miscCfg);
    }
    else
        setDeviceParallelOps(globalPairCfg_.miscCfg.deviceParallelOps, folderPathPhrase, parallelOps);
}),

versioningFolder_(this, *m_panelVersioning, *m_buttonSelectVersioningFolder, *m_bpButtonSelectVersioningAltFolder, *m_versioningFolderPath, versioningFolderLastSelected, sftpKeyFileLastSelected,
                  nullptr /*staticText*/, nullptr /*dropWindow2*/, nullptr /*droppedPathsFilter*/, getDeviceParallelOps_, setDeviceParallelOps_),

logFolderSelector_(this, *m_panelLogfile, *m_buttonSelectLogFolder, *m_bpButtonSelectAltLogFolder, *m_logFolderPath, logFolderLastSelected, sftpKeyFileLastSelected,
                   nullptr /*staticText*/, nullptr /*dropWindow2*/, nullptr /*droppedPathsFilter*/, getDeviceParallelOps_, setDeviceParallelOps_),

globalPairCfgOut_(globalPairCfg),
localPairCfgOut_(localPairCfg),
defaultFilterOut_(defaultFilter),
versioningFolderHistoryOut_(versioningFolderHistory),
logFolderHistoryOut_(logFolderHistory),
emailHistoryOut_(emailHistory),
commandHistoryOut_(commandHistory),
globalPairCfg_(globalPairCfg),
localPairCfg_(localPairCfg),
showNotesPanel_(!globalPairCfg.miscCfg.notes.empty()),
    enableExtraFeatures_(false),
showMultipleCfgs_(showMultipleCfgs),
globalLogFolderPhrase_(globalLogFolderPhrase)
{
    assert(!AFS::isNullPath(createAbstractPath(globalLogFolderPhrase_)));

    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));


    setBitmapTextLabel(*m_buttonAddNotes, loadImage("notes", dipToScreen(16)), m_buttonAddNotes->GetLabelText());

    setImage(*m_bitmapNotes, loadImage("notes", dipToScreen(20)));

    //set reasonable default height for notes: simplistic algorithm neglecting line-wrap!
    int notesRows = 1;
    for (wchar_t c : trimCpy(globalPairCfg.miscCfg.notes))
        if (c == L'\n')
            ++notesRows;

    double visibleRows = 5;
    if (showNotesPanel_)
        visibleRows = notesRows <= 10 ? notesRows : 10.5; //add half a row as visual hint
    m_textCtrNotes->SetMinSize({-1, getTextCtrlHeight(*m_textCtrNotes, visibleRows)});


    m_notebook->SetPadding(wxSize(dipToWxsize(2), 0)); //height cannot be changed

    //fill image list to cope with wxNotebook image setting design desaster...
    const int imgListSize = dipToWxsize(16); //also required by GTK => don't use getMenuIconDipSize()
    auto imgList = std::make_unique<wxImageList>(imgListSize, imgListSize);

    auto addToImageList = [&](const wxImage& img)
    {
        imgList->Add(toScaledBitmap(img));
        imgList->Add(toScaledBitmap(greyScale(img)));
    };
    //add images in same sequence like ConfigTypeImage enum!!!
    addToImageList(loadImage("options_compare", wxsizeToScreen(imgListSize)));
    addToImageList(loadImage("options_filter",  wxsizeToScreen(imgListSize)));
    addToImageList(loadImage("options_sync",    wxsizeToScreen(imgListSize)));
    assert(imgList->GetImageCount() == static_cast<int>(ConfigTypeImage::syncGrey) + 1);

    m_notebook->AssignImageList(imgList.release()); //pass ownership

    m_notebook->SetPageText(static_cast<size_t>(SyncConfigPanel::compare), _("Comparison")      + L" (F6)");
    m_notebook->SetPageText(static_cast<size_t>(SyncConfigPanel::filter ), _("Filter")          + L" (F7)");
    m_notebook->SetPageText(static_cast<size_t>(SyncConfigPanel::sync   ), _("Synchronization") + L" (F8)");

    m_notebook->ChangeSelection(static_cast<size_t>(panelToShow));

    //------------- comparison panel ----------------------
    setRelativeFontSize(*m_buttonByTimeSize, 1.25);
    setRelativeFontSize(*m_buttonByContent,  1.25);
    setRelativeFontSize(*m_buttonBySize,     1.25);

    initBitmapRadioButtons(
    {
        {m_buttonByTimeSize, "cmp_time"   },
        {m_buttonByContent,  "cmp_content"},
        {m_buttonBySize,     "cmp_size"   },
    }, true /*alignLeft*/);

    m_buttonByTimeSize->SetToolTip(getCompVariantDescription(CompareVariant::timeSize));
    m_buttonByContent ->SetToolTip(getCompVariantDescription(CompareVariant::content));
    m_buttonBySize    ->SetToolTip(getCompVariantDescription(CompareVariant::size));

    m_staticTextCompVarDescription->SetMinSize({dipToWxsize(CFG_DESCRIPTION_WIDTH_DIP), -1});

    m_scrolledWindowPerf->SetMinSize({dipToWxsize(220), -1});
    setImage(*m_bitmapPerf, greyScaleIfDisabled(loadImage("speed"), enableExtraFeatures_));

    const int scrollDelta = GetCharHeight();
    m_scrolledWindowPerf->SetScrollRate(scrollDelta, scrollDelta);

    setDefaultWidth(*m_spinCtrlAutoRetryCount);
    setDefaultWidth(*m_spinCtrlAutoRetryDelay);

    //ignore invalid input for time shift control:
    wxTextValidator inputValidator(wxFILTER_DIGITS | wxFILTER_INCLUDE_CHAR_LIST);
    inputValidator.SetCharIncludes(L"+-;,: ");
    m_textCtrlTimeShift->SetValidator(inputValidator);

    //------------- filter panel --------------------------
    m_textCtrlInclude->SetMinSize({dipToWxsize(280), -1});

    assert(!contains(m_buttonClear->GetLabel(), L"&C") && !contains(m_buttonClear->GetLabel(), L"&c")); //gazillionth wxWidgets bug on OS X: Command + C mistakenly hits "&C" access key!

    setDefaultWidth(*m_spinCtrlMinSize);
    setDefaultWidth(*m_spinCtrlMaxSize);
    setDefaultWidth(*m_spinCtrlTimespan);

    m_staticTextFilterDescr->Wrap(dipToWxsize(450));

    setImage(*m_bpButtonDefaultContext, mirrorIfRtl(loadImage("button_arrow_right")));

    enumTimeDescr_.
    add(UnitTime::none, L'(' + _("None") + L')'). //meta options should be enclosed in parentheses
    add(UnitTime::today,       _("Today")).
    //add(UnitTime::THIS_WEEK,   _("This week")).
    add(UnitTime::thisMonth,  _("This month")).
    add(UnitTime::thisYear,   _("This year")).
    add(UnitTime::lastDays, _("Last x days:"));

    enumSizeDescr_.
    add(UnitSize::none, L'(' + _("None") + L')'). //meta options should be enclosed in parentheses
    add(UnitSize::byte, _("Byte")).
    add(UnitSize::kb,   _("KB")).
    add(UnitSize::mb,   _("MB"));

    //------------- synchronization panel -----------------
    m_buttonTwoWay->SetToolTip(getSyncVariantDescription(SyncVariant::twoWay));
    m_buttonMirror->SetToolTip(getSyncVariantDescription(SyncVariant::mirror));
    m_buttonUpdate->SetToolTip(getSyncVariantDescription(SyncVariant::update));
    m_buttonCustom->SetToolTip(getSyncVariantDescription(SyncVariant::custom));

    const int catSizeMax = loadImage("cat_left_only").GetWidth() * 8 / 10;
    setImage(*m_bitmapLeftOnly,   mirrorIfRtl(greyScale(loadImage("cat_left_only", catSizeMax))));
    setImage(*m_bitmapRightOnly,  mirrorIfRtl(greyScale(loadImage("cat_right_only", catSizeMax))));
    setImage(*m_bitmapLeftNewer,  mirrorIfRtl(greyScale(loadImage("cat_left_newer", catSizeMax))));
    setImage(*m_bitmapRightNewer, mirrorIfRtl(greyScale(loadImage("cat_right_newer", catSizeMax))));
    setImage(*m_bitmapDifferent,  mirrorIfRtl(greyScale(loadImage("cat_different", catSizeMax))));

    setRelativeFontSize(*m_buttonTwoWay, 1.25);
    setRelativeFontSize(*m_buttonMirror, 1.25);
    setRelativeFontSize(*m_buttonUpdate, 1.25);
    setRelativeFontSize(*m_buttonCustom, 1.25);

    initBitmapRadioButtons(
    {
        {m_buttonTwoWay, "sync_twoway"},
        {m_buttonMirror, "sync_mirror"},
        {m_buttonUpdate, "sync_update"},
        {m_buttonCustom, "sync_custom"},
    }, false /*alignLeft*/);

    m_staticTextSyncVarDescription->SetMinSize({dipToWxsize(CFG_DESCRIPTION_WIDTH_DIP), -1});

    m_buttonRecycler  ->SetToolTip(_("Retain deleted and overwritten files in the recycle bin"));
    m_buttonPermanent ->SetToolTip(_("Delete and overwrite files permanently"));
    m_buttonVersioning->SetToolTip(_("Move files to a user-defined folder"));

    initBitmapRadioButtons(
    {
        {m_buttonRecycler,   "delete_recycler"   },
        {m_buttonPermanent,  "delete_permanently"},
        {m_buttonVersioning, "delete_versioning" },
    }, true /*alignLeft*/);

    enumVersioningStyle_.
    add(VersioningStyle::replace,          _("Replace"),    _("Move files and replace if existing")).
    add(VersioningStyle::timestampFolder, _("Time stamp") + L" [" + _("Folder") + L']', _("Move files into a time-stamped subfolder")).
    add(VersioningStyle::timestampFile,   _("Time stamp") + L" [" + _("File")   + L']', _("Append a time stamp to each file name"));

    setDefaultWidth(*m_spinCtrlVersionMaxDays );
    setDefaultWidth(*m_spinCtrlVersionCountMin);
    setDefaultWidth(*m_spinCtrlVersionCountMax);

    m_versioningFolderPath->setHistory(std::make_shared<HistoryList>(versioningFolderHistory, folderHistoryMax));


    const wxImage imgFileManagerSmall_([]
    {
        try { return extractWxImage(fff::getFileManagerIcon(dipToScreen(20))); /*throw SysError*/ }
        catch (SysError&) { assert(false); return loadImage("file_manager", dipToScreen(20)); }
    }());
    setImage(*m_bpButtonShowLogFolder, imgFileManagerSmall_);
    m_bpButtonShowLogFolder->SetToolTip(translate(extCommandFileManager.description));//translate default external apps on the fly: "Show in Explorer"

    m_logFolderPath->SetHint(utfTo<wxString>(globalLogFolderPhrase_));
    //1. no text shown when control is disabled! 2. apparently there's a refresh problem on GTK

    m_logFolderPath->setHistory(std::make_shared<HistoryList>(logFolderHistory, folderHistoryMax));

    m_comboBoxEmail->SetHint(/*_("Example:") + */ L"john.doe@example.com");
    m_comboBoxEmail->setHistory(emailHistory, emailHistoryMax);

    m_comboBoxEmail             ->Enable(enableExtraFeatures_);
    m_bpButtonEmailAlways       ->Enable(enableExtraFeatures_);
    m_bpButtonEmailErrorWarning ->Enable(enableExtraFeatures_);
    m_bpButtonEmailErrorOnly    ->Enable(enableExtraFeatures_);

    //m_staticTextPostSync->SetMinSize({dipToWxsize(180), -1});

    enumPostSyncCondition_.
    add(PostSyncCondition::completion, _("On completion:")).
    add(PostSyncCondition::errors,     _("On errors:")).
    add(PostSyncCondition::success,    _("On success:"));

    m_comboBoxPostSyncCommand->SetHint(_("Example:") + L" systemctl poweroff");

    m_comboBoxPostSyncCommand->setHistory(commandHistory, commandHistoryMax);

    //-----------------------------------------------------
    //
    //enable dialog-specific key events
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); });

    assert(!m_listBoxFolderPair->IsSorted());

    m_listBoxFolderPair->Append(_("All folder pairs"));
    for (const LocalPairConfig& lpc : localPairCfg)
    {
        std::wstring fpName = getShortDisplayNameForFolderPair(createAbstractPath(lpc.folderPathPhraseLeft),
                                                               createAbstractPath(lpc.folderPathPhraseRight));
        if (trimCpy(fpName).empty())
            fpName = L"<" + _("empty") + L">";

        m_listBoxFolderPair->Append(TAB_SPACE + fpName);
    }

    if (!showMultipleCfgs)
    {
        m_listBoxFolderPair->Hide();
        m_staticTextFolderPairLabel->Hide();
    }

    //temporarily set main config as reference for window min size calculations:
    globalPairCfg_ = GlobalPairConfig();
    globalPairCfg_.syncCfg.directionCfg = getDefaultSyncCfg(SyncVariant::twoWay);
    globalPairCfg_.syncCfg.deletionVariant = DeletionVariant::versioning;
    globalPairCfg_.syncCfg.versioningFolderPhrase = Zstr("dummy");
    globalPairCfg_.syncCfg.versioningStyle  = VersioningStyle::timestampFile;
    globalPairCfg_.syncCfg.versionMaxAgeDays = 30;
    globalPairCfg_.miscCfg.autoRetryCount = 1;
    globalPairCfg_.miscCfg.altLogFolderPathPhrase = Zstr("dummy");
    globalPairCfg_.miscCfg.emailNotifyAddress     =      "dummy";

    selectFolderPairConfig(-1);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
    Center(); //needs to be re-applied after a dialog size change!

    //keep stable sizer height: change-based directions are taller than difference-based ones => init with SyncVariant::twoWay
    bSizerSyncDirHolder   ->SetMinSize(-1, bSizerSyncDirsChanges ->GetSize().y);
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
            changeSelection(SyncConfigPanel::compare);
            return; //handled!
        case WXK_F7:
            changeSelection(SyncConfigPanel::filter);
            return;
        case WXK_F8:
            changeSelection(SyncConfigPanel::sync);
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
                case SyncConfigPanel::compare:
                    break;
                case SyncConfigPanel::filter:
                    m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::compare));
                    break;
                case SyncConfigPanel::sync:
                    m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::filter));
                    break;
            }
            m_listBoxFolderPair->SetFocus(); //needed! wxNotebook::ChangeSelection() leads to focus change!
            return; //handled!

        case WXK_RIGHT:
        case WXK_NUMPAD_RIGHT:
            switch (static_cast<SyncConfigPanel>(m_notebook->GetSelection()))
            {
                case SyncConfigPanel::compare:
                    m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::filter));
                    break;
                case SyncConfigPanel::filter:
                    m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::sync));
                    break;
                case SyncConfigPanel::sync:
                    break;
            }
            m_listBoxFolderPair->SetFocus();
            return; //handled!
    }

    event.Skip();
}


void ConfigDialog::onSelectFolderPair(wxCommandEvent& event)
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


void ConfigDialog::onCompByTimeSizeDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    onCompByTimeSize(dummy);
    onOkay(dummy);
}


void ConfigDialog::onCompBySizeDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    onCompBySize(dummy);
    onOkay(dummy);
}


void ConfigDialog::onCompByContentDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    onCompByContent(dummy);
    onOkay(dummy);
}


std::optional<CompConfig> ConfigDialog::getCompConfig() const
{
    if (!m_checkBoxUseLocalCmpOptions->GetValue())
        return {};

    CompConfig compCfg;
    compCfg.compareVar = localCmpVar_;
    compCfg.handleSymlinks = !m_checkBoxSymlinksInclude->GetValue() ? SymLinkHandling::exclude : m_radioBtnSymlinksDirect->GetValue() ? SymLinkHandling::asLink : SymLinkHandling::follow;
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
        case SymLinkHandling::exclude:
            m_checkBoxSymlinksInclude->SetValue(false);
            m_radioBtnSymlinksFollow ->SetValue(true);
            break;
        case SymLinkHandling::follow:
            m_checkBoxSymlinksInclude->SetValue(true);
            m_radioBtnSymlinksFollow->SetValue(true);
            break;
        case SymLinkHandling::asLink:
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

    m_notebook->SetPageImage(static_cast<size_t>(SyncConfigPanel::compare),
                             static_cast<int>(compOptionsEnabled ? ConfigTypeImage::compare : ConfigTypeImage::compareGrey));

    //update toggle buttons -> they have no parameter-ownership at all!
    m_buttonByTimeSize->setActive(CompareVariant::timeSize == localCmpVar_ && compOptionsEnabled);
    m_buttonByContent ->setActive(CompareVariant::content  == localCmpVar_ && compOptionsEnabled);
    m_buttonBySize    ->setActive(CompareVariant::size     == localCmpVar_ && compOptionsEnabled);
    //compOptionsEnabled: nudge wxWidgets to render inactive config state (needed on Windows, NOT on Linux!)

    switch (localCmpVar_) //unconditionally update image, including "local options off"
    {
        case CompareVariant::timeSize:
            //help wxWidgets a little to render inactive config state (needed on Windows, NOT on Linux!)
            setImage(*m_bitmapCompVariant, greyScaleIfDisabled(loadImage("cmp_time"), compOptionsEnabled));
            break;
        case CompareVariant::content:
            setImage(*m_bitmapCompVariant, greyScaleIfDisabled(loadImage("cmp_content"), compOptionsEnabled));
            break;
        case CompareVariant::size:
            setImage(*m_bitmapCompVariant, greyScaleIfDisabled(loadImage("cmp_size"), compOptionsEnabled));
            break;
    }

    //active variant description:
    setText(*m_staticTextCompVarDescription, getCompVariantDescription(localCmpVar_));
    m_staticTextCompVarDescription->Wrap(dipToWxsize(CFG_DESCRIPTION_WIDTH_DIP)); //needs to be reapplied after SetLabel()

    m_radioBtnSymlinksDirect->Enable(m_checkBoxSymlinksInclude->GetValue() && compOptionsEnabled); //help wxWidgets a little to render inactive config state (needed on Windows, NOT on Linux!)
    m_radioBtnSymlinksFollow->Enable(m_checkBoxSymlinksInclude->GetValue() && compOptionsEnabled); //
}


void ConfigDialog::onFilterDefaultContext(wxEvent& event)
{
    const FilterConfig activeCfg = getFilterConfig();
    const FilterConfig defaultFilter = XmlGlobalSettings().defaultFilter;

    ContextMenu menu;
    menu.addItem(_("&Save"), [&] { defaultFilterOut_ = activeCfg; updateFilterGui(); },
                 loadImage("cfg_save", dipToScreen(getMenuIconDipSize())), defaultFilterOut_ != activeCfg);

    menu.addItem(_("&Load factory default"), [&] { setFilterConfig(defaultFilter); }, wxNullImage, activeCfg != defaultFilter);

    menu.popup(*m_bpButtonDefaultContext, {m_bpButtonDefaultContext->GetSize().x, 0});
}


FilterConfig ConfigDialog::getFilterConfig() const
{
    const Zstring& includeFilter = utfTo<Zstring>(m_textCtrlInclude->GetValue());
    const Zstring& exludeFilter  = utfTo<Zstring>(m_textCtrlExclude->GetValue());

    return
    {
        includeFilter, exludeFilter,
        makeUnsigned(m_spinCtrlTimespan->GetValue()),
        getEnumVal(enumTimeDescr_, *m_choiceUnitTimespan),
        makeUnsigned(m_spinCtrlMinSize->GetValue()),
        getEnumVal(enumSizeDescr_, *m_choiceUnitMinSize),
        makeUnsigned(m_spinCtrlMaxSize->GetValue()),
        getEnumVal(enumSizeDescr_, *m_choiceUnitMaxSize)};
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

    m_notebook->SetPageImage(static_cast<size_t>(SyncConfigPanel::filter),
                             static_cast<int>(!isNullFilter(activeCfg) ? ConfigTypeImage::filter: ConfigTypeImage::filterGrey));

    setImage(*m_bitmapInclude,    greyScaleIfDisabled(loadImage("filter_include"), !NameFilter::isNull(activeCfg.includeFilter, FilterConfig().excludeFilter)));
    setImage(*m_bitmapExclude,    greyScaleIfDisabled(loadImage("filter_exclude"), !NameFilter::isNull(FilterConfig().includeFilter, activeCfg.excludeFilter)));
    setImage(*m_bitmapFilterDate, greyScaleIfDisabled(loadImage("cmp_time"), activeCfg.unitTimeSpan != UnitTime::none));
    setImage(*m_bitmapFilterSize, greyScaleIfDisabled(loadImage("cmp_size"), activeCfg.unitSizeMin  != UnitSize::none || activeCfg.unitSizeMax != UnitSize::none));

    m_spinCtrlTimespan->Enable(activeCfg.unitTimeSpan == UnitTime::lastDays);
    m_spinCtrlMinSize ->Enable(activeCfg.unitSizeMin != UnitSize::none);
    m_spinCtrlMaxSize ->Enable(activeCfg.unitSizeMax != UnitSize::none);

    m_buttonDefault->Enable(activeCfg != defaultFilterOut_);
    m_buttonClear  ->Enable(activeCfg != FilterConfig());
}


void ConfigDialog::onToggleUseDatabase(wxCommandEvent& event)
{
    if (const DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&directionsCfg_.dirs))
        directionsCfg_.dirs = getChangesDirDefault(*diffDirs);
    else
    {
        const DirectionByChange& changeDirs = std::get<DirectionByChange>(directionsCfg_.dirs);
        directionsCfg_.dirs = getDiffDirDefault(changeDirs);
    }
    updateSyncGui();
}


void ConfigDialog::onSyncTwoWayDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    onSyncTwoWay(dummy);
    onOkay(dummy);
}


void ConfigDialog::onSyncMirrorDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    onSyncMirror(dummy);
    onOkay(dummy);
}


void ConfigDialog::onSyncUpdateDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    onSyncUpdate(dummy);
    onOkay(dummy);
}


void ConfigDialog::onSyncCustomDouble(wxMouseEvent& event)
{
    wxCommandEvent dummy;
    onSyncCustom(dummy);
    onOkay(dummy);
}


void toggleSyncDirection(SyncDirection& current)
{
    switch (current)
    {
        case SyncDirection::right:
            current = SyncDirection::left;
            break;
        case SyncDirection::left:
            current = SyncDirection::none;
            break;
        case SyncDirection::none:
            current = SyncDirection::right;
            break;
    }
}


void ConfigDialog::toggleSyncDirButton(SyncDirection DirectionByDiff::* dir)
{
    if (DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&directionsCfg_.dirs))
    {
        toggleSyncDirection(diffDirs->*dir);
        updateSyncGui();
    }
    else assert(false);
}


void ConfigDialog::onLeftNewer(wxCommandEvent& event)
{
    toggleSyncDirButton(&DirectionByDiff::leftNewer);
    assert(!leftRightNewerCombined());
}


void ConfigDialog::onRightNewer(wxCommandEvent& event)
{
    toggleSyncDirButton(&DirectionByDiff::rightNewer);
    assert(!leftRightNewerCombined());
}


void ConfigDialog::onDifferent(wxCommandEvent& event)
{
    toggleSyncDirButton(&DirectionByDiff::leftNewer);

    if (DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&directionsCfg_.dirs))
        //simulate category "different" as leftNewer/rightNewer combined:
        diffDirs->rightNewer = diffDirs->leftNewer;
    else assert(false);
    assert(leftRightNewerCombined());
}


void ConfigDialog::toggleSyncDirButton(DirectionByChange::Changes DirectionByChange::* side, SyncDirection DirectionByChange::Changes::* dir)
{
    if (DirectionByChange* changeDirs = std::get_if<DirectionByChange>(&directionsCfg_.dirs))
    {
        toggleSyncDirection(changeDirs->*side.*dir);
        updateSyncGui();
    }
    else assert(false);
}


namespace
{
auto updateDirButton(wxBitmapButton& button, SyncDirection dir,
                     const char* imgNameLeft, const char* imgNameNone, const char* imgNameRight,
                     SyncOperation opLeft, SyncOperation opNone, SyncOperation opRight)
{
    const char* imgName = nullptr;
    switch (dir)
    {
        case SyncDirection::left:
            imgName = imgNameLeft;
            button.SetToolTip(getSyncOpDescription(opLeft));
            break;
        case SyncDirection::none:
            imgName = imgNameNone;
            button.SetToolTip(getSyncOpDescription(opNone));
            break;
        case SyncDirection::right:
            imgName = imgNameRight;
            button.SetToolTip(getSyncOpDescription(opRight));
            break;
    }
    wxImage img = mirrorIfRtl(loadImage(imgName));
    button.SetBitmapLabel   (toScaledBitmap(          img));
    button.SetBitmapDisabled(toScaledBitmap(greyScale(img))); //fix wxWidgets' all-too-clever multi-state!
    //=> the disabled bitmap is generated during first SetBitmapLabel() call but never updated again by wxWidgets!
}


void updateDiffDirButtons(const DirectionByDiff& diffDirs,
                          wxBitmapButton& buttonLeftOnly,
                          wxBitmapButton& buttonRightOnly,
                          wxBitmapButton& buttonLeftNewer,
                          wxBitmapButton& buttonRightNewer,
                          wxBitmapButton& buttonDifferent)
{
    updateDirButton(buttonLeftOnly,   diffDirs.leftOnly,   "so_delete_left", "so_none", "so_create_right", SO_DELETE_LEFT,     SO_DO_NOTHING, SO_CREATE_RIGHT);
    updateDirButton(buttonRightOnly,  diffDirs.rightOnly,  "so_create_left", "so_none", "so_delete_right", SO_CREATE_LEFT, SO_DO_NOTHING, SO_DELETE_RIGHT);
    updateDirButton(buttonLeftNewer,  diffDirs.leftNewer,  "so_update_left", "so_none", "so_update_right", SO_OVERWRITE_LEFT,  SO_DO_NOTHING, SO_OVERWRITE_RIGHT);
    updateDirButton(buttonRightNewer, diffDirs.rightNewer, "so_update_left", "so_none", "so_update_right", SO_OVERWRITE_LEFT,  SO_DO_NOTHING, SO_OVERWRITE_RIGHT);
    //simulate category "different" as leftNewer/rightNewer combined:
    updateDirButton(buttonDifferent,  diffDirs.leftNewer,  "so_update_left", "so_none", "so_update_right", SO_OVERWRITE_LEFT,  SO_DO_NOTHING, SO_OVERWRITE_RIGHT);
}


void updateChangeDirButtons(const DirectionByChange& changeDirs,
                            wxBitmapButton& buttonLeftCreate,
                            wxBitmapButton& buttonLeftUpdate,
                            wxBitmapButton& buttonLeftDelete,
                            wxBitmapButton& buttonRightCreate,
                            wxBitmapButton& buttonRightUpdate,
                            wxBitmapButton& buttonRightDelete)
{
    updateDirButton(buttonLeftCreate, changeDirs.left.create,  "so_delete_left", "so_none", "so_create_right", SO_DELETE_LEFT,     SO_DO_NOTHING, SO_CREATE_RIGHT);
    updateDirButton(buttonLeftUpdate, changeDirs.left.update,  "so_update_left", "so_none", "so_update_right", SO_OVERWRITE_LEFT,  SO_DO_NOTHING, SO_OVERWRITE_RIGHT);
    updateDirButton(buttonLeftDelete, changeDirs.left.delete_, "so_create_left", "so_none", "so_delete_right", SO_CREATE_LEFT, SO_DO_NOTHING, SO_DELETE_RIGHT);

    updateDirButton(buttonRightCreate, changeDirs.right.create,  "so_create_left", "so_none", "so_delete_right", SO_CREATE_LEFT, SO_DO_NOTHING, SO_DELETE_RIGHT);
    updateDirButton(buttonRightUpdate, changeDirs.right.update,  "so_update_left", "so_none", "so_update_right", SO_OVERWRITE_LEFT,  SO_DO_NOTHING, SO_OVERWRITE_RIGHT);
    updateDirButton(buttonRightDelete, changeDirs.right.delete_, "so_delete_left", "so_none", "so_create_right", SO_DELETE_LEFT,     SO_DO_NOTHING, SO_CREATE_RIGHT);
}
}

void ConfigDialog::onShowLogFolder(wxCommandEvent& event)
{
    assert(selectedPairIndexToShow_ < 0);
    if (selectedPairIndexToShow_ < 0)
        try
        {
            AbstractPath logFolderPath = createAbstractPath(getMiscSyncOptions().altLogFolderPathPhrase); //optional
            if (AFS::isNullPath(logFolderPath))
                logFolderPath = createAbstractPath(globalLogFolderPhrase_);

            openFolderInFileBrowser(logFolderPath); //throw FileError
        }
        catch (const FileError& e) { showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString())); }
}


bool ConfigDialog::leftRightNewerCombined() const
{
    assert(std::get_if<DirectionByDiff>(&directionsCfg_.dirs));
    const CompareVariant activeCmpVar = m_checkBoxUseLocalCmpOptions->GetValue() ? localCmpVar_ : globalPairCfg_.cmpCfg.compareVar;
    return activeCmpVar == CompareVariant::content || activeCmpVar == CompareVariant::size;
}


std::optional<SyncConfig> ConfigDialog::getSyncConfig() const
{
    if (!m_checkBoxUseLocalSyncOptions->GetValue())
        return {};

    SyncConfig syncCfg;
    syncCfg.directionCfg           = directionsCfg_;
    syncCfg.deletionVariant        = deletionVariant_;
    syncCfg.versioningFolderPhrase = versioningFolder_.getPath();
    syncCfg.versioningStyle        = getEnumVal(enumVersioningStyle_, *m_choiceVersioningStyle);
    if (syncCfg.versioningStyle != VersioningStyle::replace)
    {
        syncCfg.versionMaxAgeDays = m_checkBoxVersionMaxDays ->GetValue() ? m_spinCtrlVersionMaxDays->GetValue() : 0;
        syncCfg.versionCountMin   = m_checkBoxVersionCountMin->GetValue() && m_checkBoxVersionMaxDays->GetValue() ? m_spinCtrlVersionCountMin->GetValue() : 0;
        syncCfg.versionCountMax   = m_checkBoxVersionCountMax->GetValue() ? m_spinCtrlVersionCountMax->GetValue() : 0;
    }

    //simulate category "different" as leftNewer/rightNewer combined:
    if (DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&syncCfg.directionCfg.dirs))
        if (leftRightNewerCombined())
            diffDirs->rightNewer = diffDirs->leftNewer;

    return syncCfg;
}


void ConfigDialog::setSyncConfig(const SyncConfig* syncCfg)
{
    m_checkBoxUseLocalSyncOptions->SetValue(syncCfg);

    //when local settings are inactive, display (current) global settings instead:
    const SyncConfig tmpCfg = syncCfg ? *syncCfg : globalPairCfg_.syncCfg;

    directionsCfg_    = tmpCfg.directionCfg; //make working copy; ownership *not* on GUI
    deletionVariant_ = tmpCfg.deletionVariant;
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

    m_notebook->SetPageImage(static_cast<size_t>(SyncConfigPanel::sync),
                             static_cast<int>(syncOptionsEnabled ? ConfigTypeImage::sync: ConfigTypeImage::syncGrey));

    const bool setDirsByDifferences = std::get_if<DirectionByDiff>(&directionsCfg_.dirs);

    m_checkBoxUseDatabase->SetValue(!setDirsByDifferences);

    //display only relevant sync options
    bSizerSyncDirsDiff   ->Show( setDirsByDifferences);
    bSizerSyncDirsChanges->Show(!setDirsByDifferences);

    if (const DirectionByDiff* diffDirs = std::get_if<DirectionByDiff>(&directionsCfg_.dirs)) //sync directions by differences
    {
        updateDiffDirButtons(*diffDirs,
                             *m_bpButtonLeftOnly,
                             *m_bpButtonRightOnly,
                             *m_bpButtonLeftNewer,
                             *m_bpButtonRightNewer,
                             *m_bpButtonDifferent);

        //simulate category "different" as leftNewer/rightNewer combined:
        const bool haveLeftRightNewerCombined = leftRightNewerCombined();
        m_bitmapLeftNewer   ->Show(!haveLeftRightNewerCombined);
        m_bpButtonLeftNewer ->Show(!haveLeftRightNewerCombined);
        m_bitmapRightNewer  ->Show(!haveLeftRightNewerCombined);
        m_bpButtonRightNewer->Show(!haveLeftRightNewerCombined);

        m_bitmapDifferent  ->Show(haveLeftRightNewerCombined);
        m_bpButtonDifferent->Show(haveLeftRightNewerCombined);
    }
    else //sync directions by changes
    {
        const DirectionByChange& changeDirs = std::get<DirectionByChange>(directionsCfg_.dirs);

        updateChangeDirButtons(changeDirs,
                               *m_bpButtonLeftCreate,
                               *m_bpButtonLeftUpdate,
                               *m_bpButtonLeftDelete,
                               *m_bpButtonRightCreate,
                               *m_bpButtonRightUpdate,
                               *m_bpButtonRightDelete);
    }

    const bool useDatabaseFile = std::get_if<DirectionByChange>(&directionsCfg_.dirs);

    setImage(*m_bitmapDatabase, greyScaleIfDisabled(loadImage("database", dipToScreen(22)), useDatabaseFile && syncOptionsEnabled));

    //"detect move files" is always active iff database is used:
    setImage(*m_bitmapMoveLeft,  greyScaleIfDisabled(loadImage("so_move_left",  dipToScreen(20)), useDatabaseFile && syncOptionsEnabled));
    setImage(*m_bitmapMoveRight, greyScaleIfDisabled(loadImage("so_move_right", dipToScreen(20)), useDatabaseFile && syncOptionsEnabled));
    m_staticTextDetectMove->Enable(useDatabaseFile);

    const SyncVariant syncVar = getSyncVariant(directionsCfg_);

    //active variant description:
    setText(*m_staticTextSyncVarDescription, getSyncVariantDescription(syncVar));
    m_staticTextSyncVarDescription->Wrap(dipToWxsize(CFG_DESCRIPTION_WIDTH_DIP)); //needs to be reapplied after SetLabel()

    //update toggle buttons -> they have no parameter-ownership at all!
    m_buttonTwoWay->setActive(SyncVariant::twoWay == syncVar && syncOptionsEnabled);
    m_buttonMirror->setActive(SyncVariant::mirror == syncVar && syncOptionsEnabled);
    m_buttonUpdate->setActive(SyncVariant::update == syncVar && syncOptionsEnabled);
    m_buttonCustom->setActive(SyncVariant::custom == syncVar && syncOptionsEnabled);
    //syncOptionsEnabled: nudge wxWidgets to render inactive config state (needed on Windows, NOT on Linux!)

    m_buttonRecycler  ->setActive(DeletionVariant::recycler   == deletionVariant_ && syncOptionsEnabled);
    m_buttonPermanent ->setActive(DeletionVariant::permanent  == deletionVariant_ && syncOptionsEnabled);
    m_buttonVersioning->setActive(DeletionVariant::versioning == deletionVariant_ && syncOptionsEnabled);

    switch (deletionVariant_) //unconditionally update image, including "local options off"
    {
        case DeletionVariant::recycler:
        {
            wxImage imgTrash = loadImage("delete_recycler");
            //use system icon if available (can fail on Linux??)
            try { imgTrash = extractWxImage(fff::getTrashIcon(imgTrash.GetHeight())); /*throw SysError*/ }
            catch (SysError&) { assert(false); }

            setImage(*m_bitmapDeletionType, greyScaleIfDisabled(imgTrash, syncOptionsEnabled));
            setText(*m_staticTextDeletionTypeDescription, _("Retain deleted and overwritten files in the recycle bin"));
        }
        break;
        case DeletionVariant::permanent:
            setImage(*m_bitmapDeletionType, greyScaleIfDisabled(loadImage("delete_permanently"), syncOptionsEnabled));
            setText(*m_staticTextDeletionTypeDescription, _("Delete and overwrite files permanently"));
            break;
        case DeletionVariant::versioning:
            setImage(*m_bitmapVersioning, greyScaleIfDisabled(loadImage("delete_versioning"), syncOptionsEnabled));
            break;
    }
    //m_staticTextDeletionTypeDescription->Wrap(dipToWxsize(200)); //needs to be reapplied after SetLabel()

    const bool versioningSelected = deletionVariant_ == DeletionVariant::versioning;

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
    miscCfg.ignoreErrors   = m_checkBoxIgnoreErrors->GetValue();
    miscCfg.autoRetryCount = m_checkBoxAutoRetry   ->GetValue() ? m_spinCtrlAutoRetryCount->GetValue() : 0;
    miscCfg.autoRetryDelay = std::chrono::seconds(m_spinCtrlAutoRetryDelay->GetValue());
    //----------------------------------------------------------------------------
    miscCfg.postSyncCommand   = m_comboBoxPostSyncCommand->getValue();
    miscCfg.postSyncCondition = getEnumVal(enumPostSyncCondition_, *m_choicePostSyncCondition);
    //----------------------------------------------------------------------------
    Zstring altLogFolderPhrase = logFolderSelector_.getPath();
    if (altLogFolderPhrase.empty()) //"empty" already means "unchecked"
        altLogFolderPhrase = Zstr(' '); //=> trigger error message on dialog close
    miscCfg.altLogFolderPathPhrase = m_checkBoxOverrideLogPath->GetValue() ? altLogFolderPhrase : Zstring();
    //----------------------------------------------------------------------------
    std::string emailAddress = utfTo<std::string>(m_comboBoxEmail->getValue());
    if (emailAddress.empty())
        emailAddress = ' '; //trigger error message on dialog close
    miscCfg.emailNotifyAddress = m_checkBoxSendEmail->GetValue() ? emailAddress : std::string();
    miscCfg.emailNotifyCondition = emailNotifyCondition_;
    //----------------------------------------------------------------------------
    miscCfg.notes = trimCpy(utfTo<std::wstring>(m_textCtrNotes->GetValue()));

    return miscCfg;
}


void ConfigDialog::setMiscSyncOptions(const MiscSyncConfig& miscCfg)
{
    // Avoid "fake" changed configs! =>
    //- when editting, consider only the deviceParallelOps items corresponding to the currently-used folder paths
    //- keep parallel ops == 1 only temporarily during edit
    deviceParallelOps_  = miscCfg.deviceParallelOps;

    assert(fgSizerPerf->GetItemCount() % 2 == 0);
    const int rowsToCreate = static_cast<int>(devicesForEdit_.size()) - static_cast<int>(fgSizerPerf->GetItemCount() / 2);
    if (rowsToCreate >= 0)
        for (int i = 0; i < rowsToCreate; ++i)
        {
            wxSpinCtrl* spinCtrlParallelOps = new wxSpinCtrl(m_scrolledWindowPerf, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 2000'000'000, 1);
            setDefaultWidth(*spinCtrlParallelOps);
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
        staticTextDevice->SetLabelText(AFS::getDisplayPath(AbstractPath(afsDevice, AfsPath())));
        ++i;
    }
    m_staticTextPerfParallelOps->Enable(enableExtraFeatures_ && !devicesForEdit_.empty());

    m_panelComparisonSettings->Layout(); //*after* setting text labels

    //----------------------------------------------------------------------------
    m_checkBoxIgnoreErrors  ->SetValue(miscCfg.ignoreErrors);
    m_checkBoxAutoRetry     ->SetValue(miscCfg.autoRetryCount > 0);
    m_spinCtrlAutoRetryCount->SetValue(std::max<size_t>(miscCfg.autoRetryCount, 0));
    m_spinCtrlAutoRetryDelay->SetValue(miscCfg.autoRetryDelay.count());
    //----------------------------------------------------------------------------
    m_comboBoxPostSyncCommand->setValue(miscCfg.postSyncCommand);
    setEnumVal(enumPostSyncCondition_, *m_choicePostSyncCondition, miscCfg.postSyncCondition);
    //----------------------------------------------------------------------------
    m_checkBoxOverrideLogPath->SetValue(!miscCfg.altLogFolderPathPhrase.empty()); //only "empty path" means unchecked! everything else (e.g. " "): "checked"
    logFolderSelector_.setPath(m_checkBoxOverrideLogPath->GetValue() ? miscCfg.altLogFolderPathPhrase : globalLogFolderPhrase_);
    //----------------------------------------------------------------------------
    Zstring defaultEmail;
    if (const std::vector<Zstring>& history = m_comboBoxEmail->getHistory();
        !history.empty())
        defaultEmail = history[0];

    m_checkBoxSendEmail->SetValue(!trimCpy(miscCfg.emailNotifyAddress).empty());
    m_comboBoxEmail->setValue(m_checkBoxSendEmail->GetValue() ? utfTo<Zstring>(miscCfg.emailNotifyAddress) : defaultEmail);
    emailNotifyCondition_ = miscCfg.emailNotifyCondition;
    //----------------------------------------------------------------------------
    m_textCtrNotes->ChangeValue(utfTo<wxString>(miscCfg.notes));

    updateMiscGui();
}


void ConfigDialog::updateMiscGui()
{
    if (selectedPairIndexToShow_ == -1)
    {
        const MiscSyncConfig miscCfg = getMiscSyncOptions();

        setImage(*m_bitmapIgnoreErrors, greyScaleIfDisabled(loadImage("error_ignore_active"), miscCfg.ignoreErrors));
        setImage(*m_bitmapRetryErrors, greyScaleIfDisabled(loadImage("error_retry"), miscCfg.autoRetryCount > 0 ));

        fgSizerAutoRetry->Show(miscCfg.autoRetryCount > 0);

        m_panelComparisonSettings->Layout(); //showing "retry count" can affect bSizerPerformance!
        //----------------------------------------------------------------------------
        const bool sendEmailEnabled = m_checkBoxSendEmail->GetValue();
        setImage(*m_bitmapEmail, greyScaleIfDisabled(loadImage("email"), sendEmailEnabled));
        m_comboBoxEmail->Show(sendEmailEnabled);

        auto updateButton = [successIcon = loadImage("msg_success", dipToScreen(getMenuIconDipSize())),
                                         warningIcon = loadImage("msg_warning", dipToScreen(getMenuIconDipSize())),
                                         errorIcon   = loadImage("msg_error",   dipToScreen(getMenuIconDipSize())),
                                         sendEmailEnabled, this](wxBitmapButton& button, ResultsNotification notifyCondition)
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
                    label = resizeCanvas(label, {label.GetWidth() + warningIcon.GetWidth(), label.GetHeight()}, wxALIGN_LEFT);

                if (notifyCondition == ResultsNotification::always)
                {
                    tooltip += (L" | ") + _("Success");
                    label = stackImages(label, successIcon, ImageStackLayout::horizontal, ImageStackAlignment::center);
                }
                else
                    label = resizeCanvas(label, {label.GetWidth() + successIcon.GetWidth(), label.GetHeight()}, wxALIGN_LEFT);

                button.SetToolTip(tooltip);
                button.SetBitmapLabel   (toScaledBitmap(notifyCondition == emailNotifyCondition_ && sendEmailEnabled ? label : greyScale(label)));
                button.SetBitmapDisabled(toScaledBitmap(greyScale(label))); //fix wxWidgets' all-too-clever multi-state!
                //=> the disabled bitmap is generated during first SetBitmapLabel() call but never updated again by wxWidgets!
            }
        };
        updateButton(*m_bpButtonEmailAlways,       ResultsNotification::always);
        updateButton(*m_bpButtonEmailErrorWarning, ResultsNotification::errorWarning);
        updateButton(*m_bpButtonEmailErrorOnly,    ResultsNotification::errorOnly);

        m_hyperlinkPerfDeRequired2->Show(!enableExtraFeatures_); //required after each bSizerSyncMisc->Show()

        //----------------------------------------------------------------------------
        setImage(*m_bitmapLogFile, greyScaleIfDisabled(loadImage("log_file", dipToScreen(20)), m_checkBoxOverrideLogPath->GetValue()));
        m_logFolderPath             ->Enable(m_checkBoxOverrideLogPath->GetValue()); //
        m_buttonSelectLogFolder     ->Show(m_checkBoxOverrideLogPath->GetValue()); //enabled status can't be derived from resolved config!
        m_bpButtonSelectAltLogFolder->Show(m_checkBoxOverrideLogPath->GetValue()); //

        m_panelSyncSettings->Layout(); //after showing/hiding m_buttonSelectLogFolder

        m_panelSyncSettings->Refresh(); //removes a few artifacts when toggling email notifications
        m_panelLogfile     ->Refresh();//
    }
    //----------------------------------------------------------------------------
    m_buttonAddNotes->Show(!showNotesPanel_);
    m_panelNotes    ->Show(showNotesPanel_);
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

    if (mainConfigSelected)
    {
        m_hyperlinkPerfDeRequired->Show(!enableExtraFeatures_); //keep after bSizerPerformance->Show()

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

            if (fpCfg.localSyncCfg && fpCfg.localSyncCfg->deletionVariant == DeletionVariant::versioning)
                addDevicePath(fpCfg.localSyncCfg->versioningFolderPhrase);
        }
        if (globalPairCfg_.syncCfg.deletionVariant == DeletionVariant::versioning) //let's always add, even if *all* folder pairs use a local sync config (=> strange!)
            addDevicePath(globalPairCfg_.syncCfg.versioningFolderPhrase);
        //---------------------------------------------------------------------------------------------------------------

        setCompConfig  (&globalPairCfg_.cmpCfg);
        setSyncConfig  (&globalPairCfg_.syncCfg);
        setFilterConfig(globalPairCfg_.filter);
    }
    else
    {
        setCompConfig(get(localPairCfg_[selectedPairIndexToShow_].localCmpCfg));
        setSyncConfig(get(localPairCfg_[selectedPairIndexToShow_].localSyncCfg));
        setFilterConfig  (localPairCfg_[selectedPairIndexToShow_].localFilter);
    }
    setMiscSyncOptions(globalPairCfg_.miscCfg);

    m_panelCompSettingsTab  ->Layout(); //fix comp panel glitch on Win 7 125% font size + perf panel
    m_panelFilterSettingsTab->Layout();
    m_panelSyncSettingsTab  ->Layout();
}


bool ConfigDialog::unselectFolderPairConfig(bool validateParams)
{
    assert(selectedPairIndexToShow_ == -1 ||  makeUnsigned(selectedPairIndexToShow_) < localPairCfg_.size());

    std::optional<CompConfig> compCfg =   getCompConfig();
    std::optional<SyncConfig> syncCfg =   getSyncConfig();
    FilterConfig            filterCfg = getFilterConfig();

    MiscSyncConfig miscCfg = getMiscSyncOptions(); //some "misc" options are always visible, e.g. "notes"

    //------- parameter validation (BEFORE writing output!) -------
    if (validateParams)
    {
        //parameter validation and correction:

        std::vector<AbstractPath> baseFolderPaths; //display paths to fix filter if user pastes full folder paths
        if (selectedPairIndexToShow_ < 0)
            for (const LocalPairConfig& lpc : localPairCfg_)
            {
                baseFolderPaths.push_back(createAbstractPath(lpc.folderPathPhraseLeft));
                baseFolderPaths.push_back(createAbstractPath(lpc.folderPathPhraseRight));
            }
        else
        {
            baseFolderPaths.push_back(createAbstractPath(localPairCfg_[selectedPairIndexToShow_].folderPathPhraseLeft));
            baseFolderPaths.push_back(createAbstractPath(localPairCfg_[selectedPairIndexToShow_].folderPathPhraseRight));
        }
        if (!sanitizeFilter(filterCfg, baseFolderPaths, this))
        {
            m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::filter));
            m_textCtrlExclude->SetFocus();
            return false;
        }

        if (syncCfg && syncCfg->deletionVariant == DeletionVariant::versioning)
        {
            if (AFS::isNullPath(createAbstractPath(syncCfg->versioningFolderPhrase)))
            {
                m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::sync));
                showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Please enter a target folder.")));
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
                m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::sync));
                showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Minimum version count must be smaller than maximum count.")));
                m_spinCtrlVersionCountMin->SetFocus();
                return false;
            }
        }

        if (selectedPairIndexToShow_ < 0)
        {
            if (AFS::isNullPath(createAbstractPath(miscCfg.altLogFolderPathPhrase)) &&
                !miscCfg.altLogFolderPathPhrase.empty())
            {
                m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::sync));
                showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Please enter a folder path.")));
                m_logFolderPath->SetFocus();
                return false;
            }
            m_logFolderPath->getHistory()->addItem(miscCfg.altLogFolderPathPhrase);

            if (!miscCfg.emailNotifyAddress.empty() &&
                !isValidEmail(trimCpy(miscCfg.emailNotifyAddress)))
            {
                m_notebook->ChangeSelection(static_cast<size_t>(SyncConfigPanel::sync));
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
    }
    else
    {
        localPairCfg_[selectedPairIndexToShow_].localCmpCfg  = compCfg;
        localPairCfg_[selectedPairIndexToShow_].localSyncCfg = syncCfg;
        localPairCfg_[selectedPairIndexToShow_].localFilter  = filterCfg;
    }
    globalPairCfg_.miscCfg = miscCfg;

    selectedPairIndexToShow_ = EMPTY_PAIR_INDEX_SELECTED;
    //m_listBoxFolderPair->SetSelection(wxNOT_FOUND); not needed, selectedPairIndexToShow has parameter ownership
    return true;
}


void ConfigDialog::onAddNotes(wxCommandEvent& event)
{
    showNotesPanel_ = true;
    updateMiscGui();

    //=> enlarge dialog height!
    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()

    m_textCtrNotes->SetFocus();
}


void ConfigDialog::onOkay(wxCommandEvent& event)
{
    if (!unselectFolderPairConfig(true /*validateParams*/))
        return;

    globalPairCfgOut_ = globalPairCfg_;
    localPairCfgOut_  = localPairCfg_;

    EndModal(static_cast<int>(ConfirmationButton::accept));
}


//save global settings: should NOT be impacted by OK/Cancel
ConfigDialog::~ConfigDialog()
{
    versioningFolderHistoryOut_ = m_versioningFolderPath->getHistory()->getList();
    logFolderHistoryOut_        = m_logFolderPath       ->getHistory()->getList();

    commandHistoryOut_ = m_comboBoxPostSyncCommand->getHistory();
    emailHistoryOut_   = m_comboBoxEmail          ->getHistory();
}
}

//########################################################################################

ConfirmationButton fff::showSyncConfigDlg(wxWindow* parent,
                                          SyncConfigPanel panelToShow,
                                          int localPairIndexToShow, bool showMultipleCfgs,

                                          GlobalPairConfig&             globalPairCfg,
                                          std::vector<LocalPairConfig>& localPairCfg,

                                          FilterConfig& defaultFilter,
                                          std::vector<Zstring>& versioningFolderHistory, Zstring& versioningFolderLastSelected,
                                          std::vector<Zstring>& logFolderHistory, Zstring& logFolderLastSelected, const Zstring& globalLogFolderPhrase,
                                          size_t folderHistoryMax, Zstring& sftpKeyFileLastSelected,
                                          std::vector<Zstring>& emailHistory,   size_t emailHistoryMax,
                                          std::vector<Zstring>& commandHistory, size_t commandHistoryMax)
{

    ConfigDialog syncDlg(parent,
                         panelToShow,
                         localPairIndexToShow, showMultipleCfgs,
                         globalPairCfg,
                         localPairCfg,
                         defaultFilter,
                         versioningFolderHistory, versioningFolderLastSelected,
                         logFolderHistory, logFolderLastSelected, globalLogFolderPhrase,
                         folderHistoryMax, sftpKeyFileLastSelected,
                         emailHistory,
                         emailHistoryMax,
                         commandHistory,
                         commandHistoryMax);
    return static_cast<ConfirmationButton>(syncDlg.ShowModal());
}

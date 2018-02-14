// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "small_dlgs.h"
#include <zen/time.h>
#include <zen/format_unit.h>
#include <zen/build_info.h>
#include <zen/stl_tools.h>
#include <wx/wupdlock.h>
#include <wx/filedlg.h>
#include <wx/clipbrd.h>
#include <wx+/choice_enum.h>
#include <wx+/bitmap_button.h>
#include <wx+/rtl.h>
#include <wx+/no_flicker.h>
#include <wx+/image_tools.h>
#include <wx+/font_size.h>
#include <wx+/std_button_layout.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include "gui_generated.h"
#include "folder_selector.h"
#include "version_check.h"
#include "../algorithm.h"
#include "../synchronization.h"
#include "../lib/help_provider.h"
#include "../lib/hard_filter.h"
#include "../version/version.h"
#include "../lib/status_handler.h" //updateUiIsAllowed()



using namespace zen;
using namespace fff;


class AboutDlg : public AboutDlgGenerated
{
public:
    AboutDlg(wxWindow* parent);

private:
    void OnOK    (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_OKAY); }
    void OnClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnDonate(wxCommandEvent& event) override { wxLaunchDefaultBrowser(L"https://www.freefilesync.org/donate.php"); }
    void onLocalKeyEvent(wxKeyEvent& event);
};


AboutDlg::AboutDlg(wxWindow* parent) : AboutDlgGenerated(parent)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonClose));

    assert(m_buttonClose->GetId() == wxID_OK); //we cannot use wxID_CLOSE else Esc key won't work: yet another wxWidgets bug??

    m_bitmapHomepage->SetBitmap(getResourceImage(L"website"));
    m_bitmapEmail   ->SetBitmap(getResourceImage(L"email"));
    m_bitmapGpl     ->SetBitmap(getResourceImage(L"gpl"));

    {
        m_panelThankYou->Hide();
        m_bitmapDonate->SetBitmap(getResourceImage(L"freefilesync-heart"));
        setRelativeFontSize(*m_staticTextDonate, 1.25);
        setRelativeFontSize(*m_buttonDonate, 1.25);
    }

    //m_animCtrlWink->SetAnimation(getResourceAnimation(L"wink"));
    //m_animCtrlWink->Play();

    //create language credits
    for (const TranslationInfo& ti : getExistingTranslations())
    {
        //flag
        wxStaticBitmap* staticBitmapFlag = new wxStaticBitmap(m_scrolledWindowTranslators, wxID_ANY, getResourceImage(ti.languageFlag), wxDefaultPosition, wxSize(-1, 11), 0);
        fgSizerTranslators->Add(staticBitmapFlag, 0, wxALIGN_CENTER);

        //translator name
        wxStaticText* staticTextTranslator = new wxStaticText(m_scrolledWindowTranslators, wxID_ANY, ti.translatorName, wxDefaultPosition, wxDefaultSize, 0);
        staticTextTranslator->Wrap(-1);
        fgSizerTranslators->Add(staticTextTranslator, 0, wxALIGN_CENTER_VERTICAL);

        staticBitmapFlag    ->SetToolTip(ti.languageName);
        staticTextTranslator->SetToolTip(ti.languageName);
    }
    fgSizerTranslators->Fit(m_scrolledWindowTranslators);


    std::wstring build = formatTime<std::wstring>(FORMAT_DATE, getCompileTime()) + SPACED_DASH + L"Unicode";
#ifndef wxUSE_UNICODE
#error what is going on?
#endif

    build +=
#ifdef ZEN_BUILD_32BIT
        L" x86";
#elif defined ZEN_BUILD_64BIT
        L" x64";
#endif


    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()

    //generate logo: put *after* first Fit()
    Layout(); //make sure m_panelLogo has final width (required by wxGTK)

    wxImage appnameImg = createImageFromText(wxString(L"FreeFileSync ") + ffsVersion,
                                             wxFont(wxNORMAL_FONT->GetPointSize() * 1.8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, L"Tahoma"),
                                             *wxBLACK); //accessibility: align foreground/background colors!
    wxImage buildImg = createImageFromText(replaceCpy(_("Build: %x"), L"%x", build),
                                           *wxNORMAL_FONT,
                                           *wxBLACK,
                                           ImageStackAlignment::CENTER);
    wxImage versionImage = stackImages(appnameImg, buildImg, ImageStackLayout::VERTICAL, ImageStackAlignment::CENTER, 0);

    const int BORDER_SIZE = 5;
    wxBitmap headerBmp(GetClientSize().GetWidth(), versionImage.GetHeight() + 2 * BORDER_SIZE, 24);
    //attention: *must* pass 24 bits, auto-determination fails on Windows high-contrast colors schemes!!!
    //problem only shows when calling wxDC::DrawBitmap
    {
        wxMemoryDC dc(headerBmp);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();

        const wxBitmap& bmpGradient = getResourceImage(L"logo_gradient");
        dc.DrawBitmap(bmpGradient, wxPoint(0, (headerBmp.GetHeight() - bmpGradient.GetHeight()) / 2));

        const int logoSize = versionImage.GetHeight();
        const wxBitmap logoBmp = getResourceImage(L"FreeFileSync").ConvertToImage().Scale(logoSize, logoSize, wxIMAGE_QUALITY_HIGH);
        dc.DrawBitmap(logoBmp, wxPoint(2 * BORDER_SIZE, (headerBmp.GetHeight() - logoBmp.GetHeight()) / 2));

        dc.DrawBitmap(versionImage, wxPoint((headerBmp.GetWidth () - versionImage.GetWidth ()) / 2,
                                            (headerBmp.GetHeight() - versionImage.GetHeight()) / 2));
    }
    m_bitmapLogo->SetBitmap(headerBmp);

    //enable dialog-specific key local events
    Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(AboutDlg::onLocalKeyEvent), nullptr, this);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonClose->SetFocus(); //on GTK ESC is only associated with wxID_OK correctly if we set at least *any* focus at all!!!
}


void AboutDlg::onLocalKeyEvent(wxKeyEvent& event) //process key events without explicit menu entry :)
{
    event.Skip();
}


void fff::showAboutDialog(wxWindow* parent)
{
    AboutDlg aboutDlg(parent);
    aboutDlg.ShowModal();
}

//########################################################################################


//########################################################################################

class CopyToDialog : public CopyToDlgGenerated
{
public:
    CopyToDialog(wxWindow* parent,
                 const std::vector<const FileSystemObject*>& rowsOnLeft,
                 const std::vector<const FileSystemObject*>& rowsOnRight,
                 Zstring& lastUsedPath,
                 const std::shared_ptr<FolderHistory>& folderHistory,
                 bool& keepRelPaths,
                 bool& overwriteIfExists);

private:
    void OnOK    (wxCommandEvent& event) override;
    void OnCancel(wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

    void onLocalKeyEvent(wxKeyEvent& event);

    std::unique_ptr<FolderSelector> targetFolder; //always bound
    std::shared_ptr<FolderHistory> folderHistory_;

    //output-only parameters:
    Zstring& lastUsedPathOut_;
    bool& keepRelPathsOut_;
    bool& overwriteIfExistsOut_;
};


CopyToDialog::CopyToDialog(wxWindow* parent,
                           const std::vector<const FileSystemObject*>& rowsOnLeft,
                           const std::vector<const FileSystemObject*>& rowsOnRight,
                           Zstring& lastUsedPath,
                           const std::shared_ptr<FolderHistory>& folderHistory,
                           bool& keepRelPaths,
                           bool& overwriteIfExists) :
    CopyToDlgGenerated(parent),
    folderHistory_(folderHistory),
    lastUsedPathOut_(lastUsedPath),
    keepRelPathsOut_(keepRelPaths),
    overwriteIfExistsOut_(overwriteIfExists)
{

    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOK).setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextHeader);

    m_bitmapCopyTo->SetBitmap(getResourceImage(L"copy_to"));

    targetFolder = std::make_unique<FolderSelector>(*this, *m_buttonSelectTargetFolder, *m_bpButtonSelectAltTargetFolder, *m_targetFolderPath, nullptr /*staticText*/, nullptr /*wxWindow*/);

    m_targetFolderPath->init(folderHistory_);

    /*
    There is a nasty bug on wxGTK under Ubuntu: If a multi-line wxTextCtrl contains so many lines that scrollbars are shown,
    it re-enables all windows that are supposed to be disabled during the current modal loop!
    This only affects Ubuntu/wxGTK! No such issue on Debian/wxGTK or Suse/wxGTK
    => another Unity problem like the following?
    http://trac.wxwidgets.org/ticket/14823 "Menu not disabled when showing modal dialogs in wxGTK under Unity"
    */

    const std::pair<std::wstring, int> selectionInfo = getSelectedItemsAsString(rowsOnLeft, rowsOnRight);

    const wxString header = _P("Copy the following item to another folder?",
                               "Copy the following %x items to another folder?", selectionInfo.second);
    m_staticTextHeader->SetLabel(header);
    m_staticTextHeader->Wrap(460); //needs to be reapplied after SetLabel()

    m_textCtrlFileList->ChangeValue(selectionInfo.first);

    //----------------- set config ---------------------------------
    targetFolder               ->setPath(lastUsedPath);
    m_checkBoxKeepRelPath      ->SetValue(keepRelPaths);
    m_checkBoxOverwriteIfExists->SetValue(overwriteIfExists);
    //----------------- /set config --------------------------------

    //enable dialog-specific key local events
    Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(CopyToDialog::onLocalKeyEvent), nullptr, this);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonOK->SetFocus();
}


void CopyToDialog::onLocalKeyEvent(wxKeyEvent& event) //process key events without explicit menu entry :)
{
    event.Skip();
}


void CopyToDialog::OnOK(wxCommandEvent& event)
{
    //------- parameter validation (BEFORE writing output!) -------
    if (trimCpy(targetFolder->getPath()).empty())
    {
        showNotificationDialog(this, DialogInfoType::INFO, PopupDialogCfg().setMainInstructions(_("Please enter a target folder.")));
        //don't show error icon to follow "Windows' encouraging tone"
        m_targetFolderPath->SetFocus();
        return;
    }
    //-------------------------------------------------------------

    lastUsedPathOut_      = targetFolder->getPath();
    keepRelPathsOut_      = m_checkBoxKeepRelPath->GetValue();
    overwriteIfExistsOut_ = m_checkBoxOverwriteIfExists->GetValue();

    folderHistory_->addItem(lastUsedPathOut_);

    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}


ReturnSmallDlg::ButtonPressed fff::showCopyToDialog(wxWindow* parent,
                                                    const std::vector<const FileSystemObject*>& rowsOnLeft,
                                                    const std::vector<const FileSystemObject*>& rowsOnRight,
                                                    Zstring& lastUsedPath,
                                                    std::vector<Zstring>& folderPathHistory,
                                                    size_t historySizeMax,
                                                    bool& keepRelPaths,
                                                    bool& overwriteIfExists)
{

    auto folderHistory = std::make_shared<FolderHistory>(folderPathHistory, historySizeMax);

    CopyToDialog dlg(parent, rowsOnLeft, rowsOnRight, lastUsedPath, folderHistory, keepRelPaths, overwriteIfExists);
    const auto rc = static_cast<ReturnSmallDlg::ButtonPressed>(dlg.ShowModal());

    folderPathHistory = folderHistory->getList(); //unconditionally write path history: support manual item deletion + cancel
    return rc;
}

//########################################################################################

class DeleteDialog : public DeleteDlgGenerated
{
public:
    DeleteDialog(wxWindow* parent,
                 const std::vector<const FileSystemObject*>& rowsOnLeft,
                 const std::vector<const FileSystemObject*>& rowsOnRight,
                 bool& useRecycleBin);

private:
    void OnUseRecycler(wxCommandEvent& event) override { updateGui(); }
    void OnOK         (wxCommandEvent& event) override;
    void OnCancel     (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnClose      (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

    void onLocalKeyEvent(wxKeyEvent& event);

    void updateGui();

    const std::vector<const FileSystemObject*>& rowsToDeleteOnLeft_;
    const std::vector<const FileSystemObject*>& rowsToDeleteOnRight_;
    const std::chrono::steady_clock::time_point dlgStartTime_ = std::chrono::steady_clock::now();

    //output-only parameters:
    bool& useRecycleBinOut_;
};


DeleteDialog::DeleteDialog(wxWindow* parent,
                           const std::vector<const FileSystemObject*>& rowsOnLeft,
                           const std::vector<const FileSystemObject*>& rowsOnRight,
                           bool& useRecycleBin) :
    DeleteDlgGenerated(parent),
    rowsToDeleteOnLeft_(rowsOnLeft),
    rowsToDeleteOnRight_(rowsOnRight),
    useRecycleBinOut_(useRecycleBin)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOK).setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextHeader);

    m_checkBoxUseRecycler->SetValue(useRecycleBin);


    updateGui();

    //enable dialog-specific key local events
    Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(DeleteDialog::onLocalKeyEvent), nullptr, this);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Layout();
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonOK->SetFocus();
}


void DeleteDialog::updateGui()
{

    const std::pair<std::wstring, int> delInfo = getSelectedItemsAsString(rowsToDeleteOnLeft_,
                                                                          rowsToDeleteOnRight_);
    wxString header;
    if (m_checkBoxUseRecycler->GetValue())
    {
        header = _P("Do you really want to move the following item to the recycle bin?",
                    "Do you really want to move the following %x items to the recycle bin?", delInfo.second);
        m_bitmapDeleteType->SetBitmap(getResourceImage(L"delete_recycler"));
        m_buttonOK->SetLabel(_("Move")); //no access key needed: use ENTER!
    }
    else
    {
        header = _P("Do you really want to delete the following item?",
                    "Do you really want to delete the following %x items?", delInfo.second);
        m_bitmapDeleteType->SetBitmap(getResourceImage(L"delete_permanently"));
        m_buttonOK->SetLabel(replaceCpy(_("&Delete"), L"&", L""));
    }
    m_staticTextHeader->SetLabel(header);
    m_staticTextHeader->Wrap(460); //needs to be reapplied after SetLabel()

    m_textCtrlFileList->ChangeValue(delInfo.first);
    /*
    There is a nasty bug on wxGTK under Ubuntu: If a multi-line wxTextCtrl contains so many lines that scrollbars are shown,
    it re-enables all windows that are supposed to be disabled during the current modal loop!
    This only affects Ubuntu/wxGTK! No such issue on Debian/wxGTK or Suse/wxGTK
    => another Unity problem like the following?
    http://trac.wxwidgets.org/ticket/14823 "Menu not disabled when showing modal dialogs in wxGTK under Unity"
    */

    Layout();
    Refresh(); //needed after m_buttonOK label change
}


void DeleteDialog::onLocalKeyEvent(wxKeyEvent& event)
{
    event.Skip();
}


void DeleteDialog::OnOK(wxCommandEvent& event)
{
    //additional safety net, similar to Windows Explorer: time delta between DEL and ENTER must be at least 50ms to avoid accidental deletion!
    if (std::chrono::steady_clock::now() < dlgStartTime_ + std::chrono::milliseconds(50)) //considers chrono-wrap-around!
        return;

    useRecycleBinOut_ = m_checkBoxUseRecycler->GetValue();

    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}


ReturnSmallDlg::ButtonPressed fff::showDeleteDialog(wxWindow* parent,
                                                    const std::vector<const FileSystemObject*>& rowsOnLeft,
                                                    const std::vector<const FileSystemObject*>& rowsOnRight,
                                                    bool& useRecycleBin)
{
    DeleteDialog confirmDeletion(parent, rowsOnLeft, rowsOnRight, useRecycleBin);
    return static_cast<ReturnSmallDlg::ButtonPressed>(confirmDeletion.ShowModal());
}

//########################################################################################

class SyncConfirmationDlg : public SyncConfirmationDlgGenerated
{
public:
    SyncConfirmationDlg(wxWindow* parent,
                        const wxString& variantName,
                        const SyncStatistics& st,
                        bool& dontShowAgain);
private:
    void OnStartSync(wxCommandEvent& event) override;
    void OnCancel   (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnClose    (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

    void onLocalKeyEvent(wxKeyEvent& event);

    //output-only parameters:
    bool& dontShowAgainOut_;
};


SyncConfirmationDlg::SyncConfirmationDlg(wxWindow* parent,
                                         const wxString& variantName,
                                         const SyncStatistics& st,
                                         bool& dontShowAgain) :
    SyncConfirmationDlgGenerated(parent),
    dontShowAgainOut_(dontShowAgain)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonStartSync).setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextHeader);
    m_bitmapSync->SetBitmap(getResourceImage(L"sync"));

    m_staticTextVariant->SetLabel(variantName);
    m_checkBoxDontShowAgain->SetValue(dontShowAgain);

    Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(SyncConfirmationDlg::onLocalKeyEvent), nullptr, this);

    //update preview of item count and bytes to be transferred:
    auto setValue = [](wxStaticText& txtControl, bool isZeroValue, const wxString& valueAsString, wxStaticBitmap& bmpControl, const wchar_t* bmpName)
    {
        wxFont fnt = txtControl.GetFont();
        fnt.SetWeight(isZeroValue ? wxFONTWEIGHT_NORMAL : wxFONTWEIGHT_BOLD);
        txtControl.SetFont(fnt);

        setText(txtControl, valueAsString);

        if (isZeroValue)
            bmpControl.SetBitmap(greyScale(mirrorIfRtl(getResourceImage(bmpName))));
        else
            bmpControl.SetBitmap(mirrorIfRtl(getResourceImage(bmpName)));
    };

    auto setIntValue = [&setValue](wxStaticText& txtControl, int value, wxStaticBitmap& bmpControl, const wchar_t* bmpName)
    {
        setValue(txtControl, value == 0, formatNumber(value), bmpControl, bmpName);
    };

    setValue(*m_staticTextData, st.getBytesToProcess() == 0, formatFilesizeShort(st.getBytesToProcess()), *m_bitmapData,  L"data");
    setIntValue(*m_staticTextCreateLeft,  st.createCount< LEFT_SIDE>(), *m_bitmapCreateLeft,  L"so_create_left_small");
    setIntValue(*m_staticTextUpdateLeft,  st.updateCount< LEFT_SIDE>(), *m_bitmapUpdateLeft,  L"so_update_left_small");
    setIntValue(*m_staticTextDeleteLeft,  st.deleteCount< LEFT_SIDE>(), *m_bitmapDeleteLeft,  L"so_delete_left_small");
    setIntValue(*m_staticTextCreateRight, st.createCount<RIGHT_SIDE>(), *m_bitmapCreateRight, L"so_create_right_small");
    setIntValue(*m_staticTextUpdateRight, st.updateCount<RIGHT_SIDE>(), *m_bitmapUpdateRight, L"so_update_right_small");
    setIntValue(*m_staticTextDeleteRight, st.deleteCount<RIGHT_SIDE>(), *m_bitmapDeleteRight, L"so_delete_right_small");

    m_panelStatistics->Layout();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonStartSync->SetFocus();
}


void SyncConfirmationDlg::onLocalKeyEvent(wxKeyEvent& event)
{
    event.Skip();
}


void SyncConfirmationDlg::OnStartSync(wxCommandEvent& event)
{
    dontShowAgainOut_ = m_checkBoxDontShowAgain->GetValue();
    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}


ReturnSmallDlg::ButtonPressed fff::showSyncConfirmationDlg(wxWindow* parent,
                                                           const wxString& variantName,
                                                           const SyncStatistics& statistics,
                                                           bool& dontShowAgain)
{
    SyncConfirmationDlg dlg(parent,
                            variantName,
                            statistics,
                            dontShowAgain);
    return static_cast<ReturnSmallDlg::ButtonPressed>(dlg.ShowModal());
}

//########################################################################################

class OptionsDlg : public OptionsDlgGenerated
{
public:
    OptionsDlg(wxWindow* parent, XmlGlobalSettings& globalCfg);

private:
    void OnOkay        (wxCommandEvent& event) override;
    void OnResetDialogs(wxCommandEvent& event) override;
    void OnDefault     (wxCommandEvent& event) override;
    void OnCancel      (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnClose       (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnAddRow      (wxCommandEvent& event) override;
    void OnRemoveRow   (wxCommandEvent& event) override;
    void OnHelpShowExamples(wxHyperlinkEvent& event) override { displayHelpEntry(L"external-applications", this); }
    void onResize(wxSizeEvent& event);
    void updateGui();

    //work around defunct keyboard focus on macOS (or is it wxMac?) => not needed for this dialog!
    //void onLocalKeyEvent(wxKeyEvent& event);

    void OnToggleAutoRetryCount(wxCommandEvent& event) override { updateGui(); }

    void setExtApp(const std::vector<ExternalApp>& extApp);
    std::vector<ExternalApp> getExtApp() const;

    std::map<std::wstring, std::wstring> descriptionTransToEng_; //"translated description" -> "english" mapping for external application config

    //parameters NOT owned by GUI:
    ConfirmationDialogs confirmDlgs_;
    WarningDialogs warnDlgs_;
    bool autoCloseProgressDialog_;

    const XmlGlobalSettings defaultCfg_;

    //output-only parameters:
    XmlGlobalSettings& globalCfgOut_;
};


OptionsDlg::OptionsDlg(wxWindow* parent, XmlGlobalSettings& globalSettings) :
    OptionsDlgGenerated(parent),
    confirmDlgs_(globalSettings.confirmDlgs),
    warnDlgs_   (globalSettings.warnDlgs),
    autoCloseProgressDialog_(globalSettings.autoCloseProgressDialog),
    globalCfgOut_(globalSettings)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));


    //setMainInstructionFont(*m_staticTextHeader);

    m_gridCustomCommand->SetTabBehaviour(wxGrid::Tab_Leave);

    m_bitmapSettings   ->SetBitmap     (getResourceImage(L"settings"));
    m_bpButtonAddRow   ->SetBitmapLabel(getResourceImage(L"item_add"));
    m_bpButtonRemoveRow->SetBitmapLabel(getResourceImage(L"item_remove"));
    setBitmapTextLabel(*m_buttonResetDialogs, getResourceImage(L"reset_dialogs").ConvertToImage(), m_buttonResetDialogs->GetLabel());

    m_checkBoxFailSafe       ->SetValue(globalSettings.failSafeFileCopy);
    m_checkBoxCopyLocked     ->SetValue(globalSettings.copyLockedFiles);
    m_checkBoxCopyPermissions->SetValue(globalSettings.copyFilePermissions);

    m_spinCtrlAutoRetryCount->SetValue(globalSettings.automaticRetryCount);
    m_spinCtrlAutoRetryDelay->SetValue(globalSettings.automaticRetryDelay);

    setExtApp(globalSettings.gui.externalApps);

    updateGui();

    bSizerLockedFiles->Show(false);

    const wxString toolTip = wxString(_("Integrate external applications into context menu. The following macros are available:")) + L"\n\n" +
                             L"%item_path%    \t" + _("Full file or folder path") + L"\n" +
                             L"%folder_path%  \t" + _("Parent folder path") + L"\n" +
                             L"%local_path%   \t" + _("Temporary local copy for SFTP and MTP storage") + L"\n" +
                             L"\n" +
                             L"%item_path2%, %folder_path2%, %local_path2% \t" + _("Parameters for opposite side");

    m_gridCustomCommand->GetGridWindow()->SetToolTip(toolTip);
    m_gridCustomCommand->GetGridColLabelWindow()->SetToolTip(toolTip);
    m_gridCustomCommand->SetMargins(0, 0);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Layout();
    Center(); //needs to be re-applied after a dialog size change!

    //automatically fit column width to match total grid width
    Connect(wxEVT_SIZE, wxSizeEventHandler(OptionsDlg::onResize), nullptr, this);
    wxSizeEvent dummy;
    onResize(dummy);

    m_buttonOkay->SetFocus();
}


void OptionsDlg::onResize(wxSizeEvent& event)
{
    const int widthTotal = m_gridCustomCommand->GetGridWindow()->GetClientSize().GetWidth();

    if (widthTotal >= 0 && m_gridCustomCommand->GetNumberCols() == 2)
    {
        const int w0 = widthTotal * 2 / 5; //ratio 2 : 3
        const int w1 = widthTotal - w0;
        m_gridCustomCommand->SetColSize(0, w0);
        m_gridCustomCommand->SetColSize(1, w1);

        m_gridCustomCommand->Refresh(); //required on Ubuntu
    }

    event.Skip();
}


void OptionsDlg::updateGui()
{
    const bool autoRetryActive = m_spinCtrlAutoRetryCount->GetValue() > 0;
    m_staticTextAutoRetryDelay->Enable(autoRetryActive);
    m_spinCtrlAutoRetryDelay  ->Enable(autoRetryActive);

    m_buttonResetDialogs->Enable(confirmDlgs_             != defaultCfg_.confirmDlgs ||
                                 warnDlgs_                != defaultCfg_.warnDlgs    ||
                                 autoCloseProgressDialog_ != defaultCfg_.autoCloseProgressDialog);
}


void OptionsDlg::OnResetDialogs(wxCommandEvent& event)
{
    confirmDlgs_             = defaultCfg_.confirmDlgs;
    warnDlgs_                = defaultCfg_.warnDlgs;
    autoCloseProgressDialog_ = defaultCfg_.autoCloseProgressDialog;
    updateGui();
}


void OptionsDlg::OnDefault(wxCommandEvent& event)
{
    m_checkBoxFailSafe       ->SetValue(defaultCfg_.failSafeFileCopy);
    m_checkBoxCopyLocked     ->SetValue(defaultCfg_.copyLockedFiles);
    m_checkBoxCopyPermissions->SetValue(defaultCfg_.copyFilePermissions);

    m_spinCtrlAutoRetryCount->SetValue(defaultCfg_.automaticRetryCount);
    m_spinCtrlAutoRetryDelay->SetValue(defaultCfg_.automaticRetryDelay);

    setExtApp(defaultCfg_.gui.externalApps);
    updateGui();
}


void OptionsDlg::OnOkay(wxCommandEvent& event)
{
    //write settings only when okay-button is pressed (except hidden dialog reset)!
    globalCfgOut_.failSafeFileCopy    = m_checkBoxFailSafe->GetValue();
    globalCfgOut_.copyLockedFiles     = m_checkBoxCopyLocked->GetValue();
    globalCfgOut_.copyFilePermissions = m_checkBoxCopyPermissions->GetValue();

    globalCfgOut_.automaticRetryCount = m_spinCtrlAutoRetryCount->GetValue();
    globalCfgOut_.automaticRetryDelay = m_spinCtrlAutoRetryDelay->GetValue();

    globalCfgOut_.gui.externalApps = getExtApp();

    globalCfgOut_.confirmDlgs             = confirmDlgs_;
    globalCfgOut_.warnDlgs                = warnDlgs_;
    globalCfgOut_.autoCloseProgressDialog = autoCloseProgressDialog_;

    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}


void OptionsDlg::setExtApp(const std::vector<ExternalApp>& extApps)
{
    auto extAppsTmp = extApps;
    erase_if(extAppsTmp, [](auto& entry) { return entry.description.empty() && entry.cmdLine.empty(); });

    extAppsTmp.emplace_back(); //append empty row to facilitate insertions by user

    const int rowCount = m_gridCustomCommand->GetNumberRows();
    if (rowCount > 0)
        m_gridCustomCommand->DeleteRows(0, rowCount);

    m_gridCustomCommand->AppendRows(static_cast<int>(extAppsTmp.size()));
    for (auto it = extAppsTmp.begin(); it != extAppsTmp.end(); ++it)
    {
        const int row = it - extAppsTmp.begin();

        const std::wstring description = zen::translate(it->description);
        if (description != it->description) //remember english description to save in GlobalSettings.xml later rather than hard-code translation
            descriptionTransToEng_[description] = it->description;

        m_gridCustomCommand->SetCellValue(row, 0, description);
        m_gridCustomCommand->SetCellValue(row, 1, utfTo<wxString>(it->cmdLine)); //commandline
    }
}


std::vector<ExternalApp> OptionsDlg::getExtApp() const
{
    std::vector<ExternalApp> output;
    for (int i = 0; i < m_gridCustomCommand->GetNumberRows(); ++i)
    {
        auto description = copyStringTo<std::wstring>(m_gridCustomCommand->GetCellValue(i, 0));
        auto commandline =             utfTo<Zstring>(m_gridCustomCommand->GetCellValue(i, 1));

        //try to undo translation of description for GlobalSettings.xml
        auto it = descriptionTransToEng_.find(description);
        if (it != descriptionTransToEng_.end())
            description = it->second;

        if (!description.empty() || !commandline.empty())
            output.push_back({ description, commandline });
    }
    return output;
}


void OptionsDlg::OnAddRow(wxCommandEvent& event)
{

    const int selectedRow = m_gridCustomCommand->GetGridCursorRow();
    if (0 <= selectedRow && selectedRow < m_gridCustomCommand->GetNumberRows())
        m_gridCustomCommand->InsertRows(selectedRow);
    else
        m_gridCustomCommand->AppendRows();
}


void OptionsDlg::OnRemoveRow(wxCommandEvent& event)
{
    if (m_gridCustomCommand->GetNumberRows() > 0)
    {

        const int selectedRow = m_gridCustomCommand->GetGridCursorRow();
        if (0 <= selectedRow && selectedRow < m_gridCustomCommand->GetNumberRows())
            m_gridCustomCommand->DeleteRows(selectedRow);
        else
            m_gridCustomCommand->DeleteRows(m_gridCustomCommand->GetNumberRows() - 1);
    }
}


ReturnSmallDlg::ButtonPressed fff::showOptionsDlg(wxWindow* parent, XmlGlobalSettings& globalCfg)
{
    OptionsDlg dlg(parent, globalCfg);
    return static_cast<ReturnSmallDlg::ButtonPressed>(dlg.ShowModal());
}

//########################################################################################

class SelectTimespanDlg : public SelectTimespanDlgGenerated
{
public:
    SelectTimespanDlg(wxWindow* parent, time_t& timeFrom, time_t& timeTo);

private:
    void OnOkay  (wxCommandEvent& event) override;
    void OnCancel(wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

    void OnChangeSelectionFrom(wxCalendarEvent& event) override
    {
        if (m_calendarFrom->GetDate() > m_calendarTo->GetDate())
            m_calendarTo->SetDate(m_calendarFrom->GetDate());
    }
    void OnChangeSelectionTo(wxCalendarEvent& event) override
    {
        if (m_calendarFrom->GetDate() > m_calendarTo->GetDate())
            m_calendarFrom->SetDate(m_calendarTo->GetDate());
    }

    void onLocalKeyEvent(wxKeyEvent& event);

    //output-only parameters:
    time_t& timeFromOut_;
    time_t& timeToOut_;
};


SelectTimespanDlg::SelectTimespanDlg(wxWindow* parent, time_t& timeFrom, time_t& timeTo) :
    SelectTimespanDlgGenerated(parent),
    timeFromOut_(timeFrom),
    timeToOut_(timeTo)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    long style = wxCAL_SHOW_HOLIDAYS | wxCAL_SHOW_SURROUNDING_WEEKS;

        style |= wxCAL_MONDAY_FIRST;

    m_calendarFrom->SetWindowStyleFlag(style);
    m_calendarTo  ->SetWindowStyleFlag(style);

    //set default values
    time_t timeFromTmp = timeFrom;
    time_t timeToTmp   = timeTo;

    if (timeToTmp == 0)
        timeToTmp = std::time(nullptr); //
    if (timeFromTmp == 0)
        timeFromTmp = timeToTmp - 7 * 24 * 3600; //default time span: one week from "now"

    //wxDateTime models local(!) time (in contrast to what documentation says), but it has a constructor taking time_t UTC
    m_calendarFrom->SetDate(timeFromTmp);
    m_calendarTo  ->SetDate(timeToTmp  );

    //enable dialog-specific key local events
    Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(SelectTimespanDlg::onLocalKeyEvent), nullptr, this);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonOkay->SetFocus();
}


void SelectTimespanDlg::onLocalKeyEvent(wxKeyEvent& event) //process key events without explicit menu entry :)
{
    event.Skip();
}


void SelectTimespanDlg::OnOkay(wxCommandEvent& event)
{
    wxDateTime from = m_calendarFrom->GetDate();
    wxDateTime to   = m_calendarTo  ->GetDate();

    //align to full days
    from.ResetTime();
    to.ResetTime(); //reset local(!) time
    to += wxTimeSpan::Day();
    to -= wxTimeSpan::Second(); //go back to end of previous day

    timeFromOut_ = from.GetTicks();
    timeToOut_   = to  .GetTicks();

    /*
    {
        time_t current = zen::to<time_t>(timeFrom_);
        struct tm* tdfewst = ::localtime(&current);
        int budfk = 3;
    }
    {
        time_t current = zen::to<time_t>(timeTo_);
        struct tm* tdfewst = ::localtime(&current);
        int budfk = 3;
    }
    */

    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}


ReturnSmallDlg::ButtonPressed fff::showSelectTimespanDlg(wxWindow* parent, time_t& timeFrom, time_t& timeTo)
{
    SelectTimespanDlg timeSpanDlg(parent, timeFrom, timeTo);
    return static_cast<ReturnSmallDlg::ButtonPressed>(timeSpanDlg.ShowModal());
}

//########################################################################################

class CfgHighlightDlg : public CfgHighlightDlgGenerated
{
public:
    CfgHighlightDlg(wxWindow* parent, int& cfgHistSyncOverdueDays);

private:
    void OnOkay  (wxCommandEvent& event) override;
    void OnCancel(wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

    //work around defunct keyboard focus on macOS (or is it wxMac?) => not needed for this dialog!
    //void onLocalKeyEvent(wxKeyEvent& event);

    //output-only parameters:
    int& cfgHistSyncOverdueDaysOut_;
};


CfgHighlightDlg::CfgHighlightDlg(wxWindow* parent, int& cfgHistSyncOverdueDays) :
    CfgHighlightDlgGenerated(parent),
    cfgHistSyncOverdueDaysOut_(cfgHistSyncOverdueDays)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    m_spinCtrlSyncOverdueDays->SetValue(cfgHistSyncOverdueDays);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_spinCtrlSyncOverdueDays->SetFocus();
}


void CfgHighlightDlg::OnOkay(wxCommandEvent& event)
{
    cfgHistSyncOverdueDaysOut_ = m_spinCtrlSyncOverdueDays->GetValue();
    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}


ReturnSmallDlg::ButtonPressed fff::showCfgHighlightDlg(wxWindow* parent, int& cfgHistSyncOverdueDays)
{
    CfgHighlightDlg cfgHighDlg(parent, cfgHistSyncOverdueDays);
    return static_cast<ReturnSmallDlg::ButtonPressed>(cfgHighDlg.ShowModal());
}

//########################################################################################

class ActivationDlg : public ActivationDlgGenerated
{
public:
    ActivationDlg(wxWindow* parent, const std::wstring& lastErrorMsg, const std::wstring& manualActivationUrl, std::wstring& manualActivationKey);

private:
    void OnActivateOnline (wxCommandEvent& event) override;
    void OnActivateOffline(wxCommandEvent& event) override;
    void OnOfflineActivationEnter(wxCommandEvent& event) override { OnActivateOffline(event); }
    void OnCopyUrl        (wxCommandEvent& event) override;
    void OnCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ReturnActivationDlg::CANCEL)); }
    void OnClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ReturnActivationDlg::CANCEL)); }

    std::wstring& manualActivationKeyOut_; //in/out parameter
};


ActivationDlg::ActivationDlg(wxWindow* parent,
                             const std::wstring& lastErrorMsg,
                             const std::wstring& manualActivationUrl,
                             std::wstring& manualActivationKey) :
    ActivationDlgGenerated(parent),
    manualActivationKeyOut_(manualActivationKey)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setCancel(m_buttonCancel));

    //setMainInstructionFont(*m_staticTextMain);

    m_bitmapActivation->SetBitmap(getResourceImage(L"website"));

    m_textCtrlLastError           ->ChangeValue(lastErrorMsg);
    m_textCtrlManualActivationUrl ->ChangeValue(manualActivationUrl);
    m_textCtrlOfflineActivationKey->ChangeValue(manualActivationKey);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonActivateOnline->SetFocus();
}


void ActivationDlg::OnCopyUrl(wxCommandEvent& event)
{
    if (wxClipboard::Get()->Open())
    {
        ZEN_ON_SCOPE_EXIT(wxClipboard::Get()->Close());
        wxClipboard::Get()->SetData(new wxTextDataObject(m_textCtrlManualActivationUrl->GetValue())); //ownership passed

        m_textCtrlManualActivationUrl->SetFocus(); //[!] otherwise selection is lost
        m_textCtrlManualActivationUrl->SelectAll(); //some visual feedback
    }
}


void ActivationDlg::OnActivateOnline(wxCommandEvent& event)
{
    manualActivationKeyOut_ = m_textCtrlOfflineActivationKey->GetValue();
    EndModal(static_cast<int>(ReturnActivationDlg::ACTIVATE_ONLINE));
}


void ActivationDlg::OnActivateOffline(wxCommandEvent& event)
{
    manualActivationKeyOut_ = m_textCtrlOfflineActivationKey->GetValue();
    EndModal(static_cast<int>(ReturnActivationDlg::ACTIVATE_OFFLINE));
}


ReturnActivationDlg fff::showActivationDialog(wxWindow* parent, const std::wstring& lastErrorMsg, const std::wstring& manualActivationUrl, std::wstring& manualActivationKey)
{
    ActivationDlg dlg(parent, lastErrorMsg, manualActivationUrl, manualActivationKey);
    return static_cast<ReturnActivationDlg>(dlg.ShowModal());
}

//########################################################################################

class DownloadProgressWindow::Impl : public DownloadProgressDlgGenerated
{
public:
    Impl(wxWindow* parent, int64_t fileSizeTotal);

    void notifyNewFile (const Zstring& filePath) { filePath_ = filePath; }
    void notifyProgress(int64_t delta)           { bytesCurrent_ += delta; }

    void requestUiRefresh() //throw CancelPressed
    {
        if (cancelled_)
            throw CancelPressed();

        if (updateUiIsAllowed())
        {
            updateGui();
            //wxTheApp->Yield();
            ::wxSafeYield(this); //disables user input except for "this" (using wxWindowDisabler instead would move the FFS main dialog into the background: why?)
        }
    }

private:
    void OnCancel(wxCommandEvent& event) override { cancelled_ = true; }

    void updateGui()
    {
        const double fraction = bytesTotal_ == 0 ? 0 : 1.0 * bytesCurrent_ / bytesTotal_;
        m_staticTextHeader->SetLabel(_("Downloading update...") + L" " +
                                     numberTo<std::wstring>(numeric::round(fraction * 100)) + L"% (" + formatFilesizeShort(bytesCurrent_) + L")");
        m_gaugeProgress->SetValue(numeric::round(fraction * GAUGE_FULL_RANGE));

        m_staticTextDetails->SetLabel(utfTo<std::wstring>(filePath_));
    }

    bool cancelled_ = false;
    int64_t bytesCurrent_ = 0;
    const int64_t bytesTotal_;
    Zstring filePath_;
    const int GAUGE_FULL_RANGE = 1000000;
};


DownloadProgressWindow::Impl::Impl(wxWindow* parent, int64_t fileSizeTotal) :
    DownloadProgressDlgGenerated(parent),
    bytesTotal_(fileSizeTotal)
{

    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextHeader);

    m_bitmapDownloading->SetBitmap(getResourceImage(L"website"));

    m_gaugeProgress->SetRange(GAUGE_FULL_RANGE);

    updateGui();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!
    Show();

    //clear gui flicker: window must be visible to make this work!
    ::wxSafeYield(); //at least on OS X a real Yield() is required to flush pending GUI updates; Update() is not enough

    m_buttonCancel->SetFocus();
}


DownloadProgressWindow::DownloadProgressWindow(wxWindow* parent, int64_t fileSizeTotal) :
    pimpl_(new DownloadProgressWindow::Impl(parent, fileSizeTotal)) {}

DownloadProgressWindow::~DownloadProgressWindow() { pimpl_->Destroy(); }

void DownloadProgressWindow::notifyNewFile(const Zstring& filePath) { pimpl_->notifyNewFile(filePath); }
void DownloadProgressWindow::notifyProgress(int64_t delta)          { pimpl_->notifyProgress(delta); }
void DownloadProgressWindow::requestUiRefresh()                     { pimpl_->requestUiRefresh(); } //throw CancelPressed

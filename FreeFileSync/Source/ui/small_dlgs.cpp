// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "small_dlgs.h"
#include <variant>
#include <zen/time.h>
#include <zen/format_unit.h>
#include <zen/build_info.h>
#include <zen/stl_tools.h>
#include <zen/shell_execute.h>
#include <wx/wupdlock.h>
#include <wx/filedlg.h>
#include <wx/clipbrd.h>
#include <wx/sound.h>
#include <wx+/choice_enum.h>
#include <wx+/bitmap_button.h>
#include <wx+/rtl.h>
#include <wx+/no_flicker.h>
#include <wx+/image_tools.h>
#include <wx+/font_size.h>
#include <wx+/std_button_layout.h>
#include <wx+/popup_dlg.h>
#include <wx+/async_task.h>
#include <wx+/image_resources.h>
#include "gui_generated.h"
#include "folder_selector.h"
#include "version_check.h"
#include "abstract_folder_picker.h"
#include "../base/algorithm.h"
#include "../base/ffs_paths.h"
#include "../base/synchronization.h"
#include "../base/help_provider.h"
#include "../base/path_filter.h"
#include "../base/status_handler.h" //updateUiIsAllowed()
#include "../base/generate_logfile.h"
#include "../base/icon_buffer.h"
#include "../version/version.h"
#include "../afs/concrete.h"
#include "../afs/gdrive.h"
#include "../afs/sftp.h"
#include "../afs/ftp.h"




using namespace zen;
using namespace fff;


class AboutDlg : public AboutDlgGenerated
{
public:
    AboutDlg(wxWindow* parent);

private:
    void OnOK    (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_OKAY); }
    void OnClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnDonate(wxCommandEvent& event) override { wxLaunchDefaultBrowser(L"https://freefilesync.org/donate.php"); }
    void onLocalKeyEvent(wxKeyEvent& event);
};


AboutDlg::AboutDlg(wxWindow* parent) : AboutDlgGenerated(parent)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonClose));

    assert(m_buttonClose->GetId() == wxID_OK); //we cannot use wxID_CLOSE else Esc key won't work: yet another wxWidgets bug??

    m_bitmapHomepage->SetBitmap(getResourceImage(L"website"));
    m_bitmapForum   ->SetBitmap(getResourceImage(L"forum"));
    m_bitmapEmail   ->SetBitmap(getResourceImage(L"email"));
    m_bitmapGpl     ->SetBitmap(getResourceImage(L"gpl"));

    {
        m_panelThankYou->Hide();
        m_bitmapDonate->SetBitmap(getResourceImage(L"ffs_heart"));
        setRelativeFontSize(*m_staticTextDonate, 1.25);
        setRelativeFontSize(*m_buttonDonate, 1.25);
    }

    //m_animCtrlWink->SetAnimation(getResourceAnimation(L"wink"));
    //m_animCtrlWink->Play();

    m_staticTextThanksForLoc->SetMinSize(wxSize(fastFromDIP(200), -1));
    m_staticTextThanksForLoc->Wrap(fastFromDIP(200));

    //create language credits
    for (const TranslationInfo& ti : getExistingTranslations())
    {
        //flag
        wxStaticBitmap* staticBitmapFlag = new wxStaticBitmap(m_scrolledWindowTranslators, wxID_ANY, getResourceImage(ti.languageFlag));
        fgSizerTranslators->Add(staticBitmapFlag, 0, wxALIGN_CENTER);

        //translator name
        wxStaticText* staticTextTranslator = new wxStaticText(m_scrolledWindowTranslators, wxID_ANY, ti.translatorName, wxDefaultPosition, wxDefaultSize, 0);
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

    const int borderSize = fastFromDIP(5);
    wxBitmap headerBmp(bSizerMainSection->GetSize().x, versionImage.GetHeight() + 2 * borderSize, 24);
    //attention: *must* pass 24 bits, auto-determination fails on Windows high-contrast colors schemes!!!
    //problem only shows when calling wxDC::DrawBitmap
    {
        wxMemoryDC dc(headerBmp);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();

        const wxBitmap& bmpGradient = getResourceImage(L"logo_gradient");
        dc.DrawBitmap(bmpGradient, wxPoint(0, (headerBmp.GetHeight() - bmpGradient.GetHeight()) / 2));

        const int logoSize = versionImage.GetHeight();
        const wxBitmap logoBmp = getResourceImage(L"FreeFileSync").ConvertToImage().Scale(logoSize, logoSize, wxIMAGE_QUALITY_HIGH); //looks smooth unlike wxIMAGE_QUALITY_BILINEAR!
        dc.DrawBitmap(logoBmp, wxPoint(2 * borderSize, (headerBmp.GetHeight() - logoBmp.GetHeight()) / 2));

        dc.DrawBitmap(versionImage, wxPoint((headerBmp.GetWidth () - versionImage.GetWidth ()) / 2,
                                            (headerBmp.GetHeight() - versionImage.GetHeight()) / 2));
    }
    m_bitmapLogo->SetBitmap(headerBmp);

    //enable dialog-specific key events
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

class CloudSetupDlg : public CloudSetupDlgGenerated
{
public:
    CloudSetupDlg(wxWindow* parent, Zstring& folderPathPhrase, size_t& parallelOps, const std::wstring* parallelOpsDisabledReason);

private:
    void OnOkay  (wxCommandEvent& event) override;
    void OnCancel(wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

    void OnGdriveUserAdd   (wxCommandEvent& event) override;
    void OnGdriveUserRemove(wxCommandEvent& event) override;
    void OnGdriveUserSelect(wxCommandEvent& event) override;
    void OnDetectServerChannelLimit(wxCommandEvent& event) override;
    void OnToggleShowPassword(wxCommandEvent& event) override;
    void OnBrowseCloudFolder (wxCommandEvent& event) override;
    void OnHelpFtpPerformance(wxHyperlinkEvent& event) override { displayHelpEntry(L"ftp-setup", this); }

    void OnConnectionGdrive(wxCommandEvent& event) override { type_ = CloudType::gdrive; updateGui(); }
    void OnConnectionSftp  (wxCommandEvent& event) override { type_ = CloudType::sftp;   updateGui(); }
    void OnConnectionFtp   (wxCommandEvent& event) override { type_ = CloudType::ftp;    updateGui(); }

    void OnAuthPassword(wxCommandEvent& event) override { sftpAuthType_ = SftpAuthType::PASSWORD; updateGui(); }
    void OnAuthKeyfile (wxCommandEvent& event) override { sftpAuthType_ = SftpAuthType::KEY_FILE; updateGui(); }
    void OnAuthAgent   (wxCommandEvent& event) override { sftpAuthType_ = SftpAuthType::AGENT;    updateGui(); }

    void OnSelectKeyfile(wxCommandEvent& event) override;

    void updateGui();

    //work around defunct keyboard focus on macOS (or is it wxMac?) => not needed for this dialog!
    //void onLocalKeyEvent(wxKeyEvent& event);

    static bool acceptFileDrop(const std::vector<Zstring>& shellItemPaths);
    void onKeyFileDropped(FileDropEvent& event);

    Zstring getFolderPathPhrase() const;

    enum class CloudType
    {
        gdrive,
        sftp,
        ftp,
    };
    CloudType type_ = CloudType::gdrive;

    SftpAuthType sftpAuthType_ = SftpAuthType::PASSWORD;

    AsyncGuiQueue guiQueue_;

    //output-only parameters:
    Zstring& folderPathPhraseOut_;
    size_t& parallelOpsOut_;
};


CloudSetupDlg::CloudSetupDlg(wxWindow* parent, Zstring& folderPathPhrase, size_t& parallelOps, const std::wstring* parallelOpsDisabledReason) :
    CloudSetupDlgGenerated(parent),
    folderPathPhraseOut_(folderPathPhrase),
    parallelOpsOut_(parallelOps)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    m_toggleBtnGdrive->SetBitmap(getResourceImage(L"google_drive"));
    m_toggleBtnSftp  ->SetBitmap(getTransparentPixel()); //set dummy image (can't be empty!): text-only buttons are rendered smaller on OS X!
    m_toggleBtnFtp   ->SetBitmap(getTransparentPixel()); //

    setRelativeFontSize(*m_toggleBtnGdrive, 1.25);
    setRelativeFontSize(*m_toggleBtnSftp,   1.25);
    setRelativeFontSize(*m_toggleBtnFtp,    1.25);
    setRelativeFontSize(*m_staticTextGdriveUser, 1.25);

    setBitmapTextLabel(*m_buttonGdriveAddUser,    getResourceImage(L"user_add"   ).ConvertToImage(), m_buttonGdriveAddUser   ->GetLabel());
    setBitmapTextLabel(*m_buttonGdriveRemoveUser, getResourceImage(L"user_remove").ConvertToImage(), m_buttonGdriveRemoveUser->GetLabel());

    m_bitmapGdriveSelectedUser->SetBitmap(getResourceImage(L"user_selected"));
    m_bitmapServer->SetBitmap(shrinkImage(getResourceImage(L"server").ConvertToImage(), fastFromDIP(24)));
    m_bitmapCloud ->SetBitmap(getResourceImage(L"cloud"));
    m_bitmapPerf  ->SetBitmap(getResourceImage(L"speed"));
    m_bitmapServerDir->SetBitmap(IconBuffer::genericDirIcon(IconBuffer::SIZE_SMALL));
    m_checkBoxShowPassword->SetValue(false);

    m_textCtrlServer->SetHint(_("Example:") + L"    website.com    66.198.240.22");
    m_textCtrlServer->SetMinSize(wxSize(fastFromDIP(260), -1));

    m_textCtrlPort            ->SetMinSize(wxSize(fastFromDIP(60), -1)); //
    m_spinCtrlConnectionCount ->SetMinSize(wxSize(fastFromDIP(70), -1)); //Hack: set size (why does wxWindow::Size() not work?)
    m_spinCtrlChannelCountSftp->SetMinSize(wxSize(fastFromDIP(70), -1)); //
    m_spinCtrlTimeout         ->SetMinSize(wxSize(fastFromDIP(70), -1)); //

    setupFileDrop(*m_panelAuth);
    m_panelAuth->Connect(EVENT_DROP_FILE, FileDropEventHandler(CloudSetupDlg::onKeyFileDropped), nullptr, this);

    m_staticTextConnectionsLabelSub->SetLabel(L"(" + _("Connections") + L")");

    //use spacer to keep dialog height stable, no matter if key file options are visible
    bSizerAuthInner->Add(0, m_panelAuth->GetSize().GetHeight());

    wxArrayString googleUsers;
    try
    {
        for (const Zstring& googleUser: googleListConnectedUsers()) //throw FileError
            googleUsers.push_back(utfTo<wxString>(googleUser));
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
    }
    m_listBoxGdriveUsers->Append(googleUsers);

    //set default values for Google Drive: use first item of m_listBoxGdriveUsers
    m_staticTextGdriveUser->SetLabel(L"");
    if (!googleUsers.empty())
    {
        m_listBoxGdriveUsers->SetSelection(0);
        m_staticTextGdriveUser->SetLabel(googleUsers[0]);
    }

    m_spinCtrlTimeout->SetValue(FtpLoginInfo().timeoutSec);
    assert(FtpLoginInfo().timeoutSec == SftpLoginInfo().timeoutSec); //make sure the default values are in sync

    if (acceptsItemPathPhraseGdrive(folderPathPhrase))
    {
        type_ = CloudType::gdrive;
        const GdrivePath gdrivePath = getResolvedGooglePath(folderPathPhrase); //noexcept

        const int selIdx = m_listBoxGdriveUsers->FindString(utfTo<wxString>(gdrivePath.userEmail), false /*caseSensitive*/);
        if (selIdx != wxNOT_FOUND)
        {
            m_listBoxGdriveUsers->EnsureVisible(selIdx);
            m_listBoxGdriveUsers->SetSelection(selIdx);
        }
        else
            m_listBoxGdriveUsers->DeselectAll();
        m_staticTextGdriveUser->SetLabel   (utfTo<wxString>(gdrivePath.userEmail));
        m_textCtrlServerPath  ->ChangeValue(utfTo<wxString>(FILE_NAME_SEPARATOR + gdrivePath.itemPath.value));
    }
    else if (acceptsItemPathPhraseSftp(folderPathPhrase))
    {
        type_ = CloudType::sftp;
        const SftpPathInfo pi = getResolvedSftpPath(folderPathPhrase); //noexcept

        if (pi.login.port > 0)
            m_textCtrlPort->ChangeValue(numberTo<wxString>(pi.login.port));
        m_textCtrlServer        ->ChangeValue(utfTo<wxString>(pi.login.server));
        m_textCtrlUserName      ->ChangeValue(utfTo<wxString>(pi.login.username));
        sftpAuthType_ = pi.login.authType;
        m_textCtrlPasswordHidden->ChangeValue(utfTo<wxString>(pi.login.password));
        m_textCtrlKeyfilePath   ->ChangeValue(utfTo<wxString>(pi.login.privateKeyFilePath));
        m_textCtrlServerPath    ->ChangeValue(utfTo<wxString>(FILE_NAME_SEPARATOR + pi.afsPath.value));
        m_spinCtrlChannelCountSftp->SetValue(pi.login.traverserChannelsPerConnection);
        m_spinCtrlTimeout         ->SetValue(pi.login.timeoutSec);
    }
    else if (acceptsItemPathPhraseFtp(folderPathPhrase))
    {
        type_ = CloudType::ftp;
        const FtpPathInfo pi = getResolvedFtpPath(folderPathPhrase); //noexcept

        if (pi.login.port > 0)
            m_textCtrlPort->ChangeValue(numberTo<wxString>(pi.login.port));
        m_textCtrlServer         ->ChangeValue(utfTo<wxString>(pi.login.server));
        m_textCtrlUserName       ->ChangeValue(utfTo<wxString>(pi.login.username));
        m_textCtrlPasswordHidden ->ChangeValue(utfTo<wxString>(pi.login.password));
        m_textCtrlServerPath     ->ChangeValue(utfTo<wxString>(FILE_NAME_SEPARATOR + pi.afsPath.value));
        (pi.login.useSsl ? m_radioBtnEncryptSsl : m_radioBtnEncryptNone)->SetValue(true);
        m_spinCtrlTimeout        ->SetValue(pi.login.timeoutSec);
    }

    m_spinCtrlConnectionCount->SetValue(parallelOps);

    if (parallelOpsDisabledReason)
    {
        //m_staticTextConnectionsLabel   ->Disable();
        //m_staticTextConnectionsLabelSub->Disable();
        m_spinCtrlChannelCountSftp       ->Disable();
        m_buttonChannelCountSftp         ->Disable();
        m_spinCtrlConnectionCount        ->Disable();
        m_staticTextConnectionCountDescr->SetLabel(*parallelOpsDisabledReason);
        //m_staticTextConnectionCountDescr->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }
    else
        m_staticTextConnectionCountDescr->SetLabel(_("Recommended range:") + L" [1" + EN_DASH + L"10]"); //no spaces!

    //set up default view for dialog size calculation
    bSizerGdrive->Show(false);
    bSizerFtpEncrypt->Show(false);
    m_textCtrlPasswordHidden->Hide();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    updateGui(); //*after* SetSizeHints when standard dialog height has been calculated

    m_buttonOkay->SetFocus();
}


void CloudSetupDlg::OnGdriveUserAdd(wxCommandEvent& event)
{
    guiQueue_.processAsync([]() -> std::variant<Zstring, FileError>
    {
        try
        {
            return googleAddUser(nullptr /*updateGui*/); //throw FileError
        }
        catch (const FileError& e) { return e; }
    },
    [this](const std::variant<Zstring, FileError>& result)
    {
        if (const FileError* e = std::get_if<FileError>(&result))
            showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e->toString()));
        else
        {
            const wxString googleUser = utfTo<wxString>(std::get<Zstring>(result));
            int selIdx = m_listBoxGdriveUsers->FindString(googleUser, false /*caseSensitive*/);
            if (selIdx == wxNOT_FOUND)
                selIdx = m_listBoxGdriveUsers->Append(googleUser);

            m_listBoxGdriveUsers->EnsureVisible(selIdx);
            m_listBoxGdriveUsers->SetSelection(selIdx);
            m_staticTextGdriveUser->SetLabel(googleUser);
            updateGui(); //enable remove user button
        }
    });
}


void CloudSetupDlg::OnGdriveUserRemove(wxCommandEvent& event)
{
    const int selIdx = m_listBoxGdriveUsers->GetSelection();
    assert(selIdx != wxNOT_FOUND);
    if (selIdx != wxNOT_FOUND)
        try
        {
            const wxString googleUser = m_listBoxGdriveUsers->GetString(selIdx);
            if (showConfirmationDialog(this, DialogInfoType::WARNING, PopupDialogCfg().
                                       setTitle(_("Confirm")).
                                       setMainInstructions(replaceCpy(_("Do you really want to disconnect from user account %x?"), L"%x", googleUser)),
                                       _("&Disconnect")) != ConfirmationButton::ACCEPT)
                return;

            googleRemoveUser(utfTo<Zstring>(googleUser)); //throw FileError
            m_listBoxGdriveUsers->Delete(selIdx);
            updateGui(); //disable remove user button
            //no need to also clear m_staticTextGdriveUser
        }
        catch (const FileError& e)
        {
            showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
        }
}


void CloudSetupDlg::OnGdriveUserSelect(wxCommandEvent& event)
{
    const int selIdx = m_listBoxGdriveUsers->GetSelection();
    assert(selIdx != wxNOT_FOUND);
    if (selIdx != wxNOT_FOUND)
    {
        m_staticTextGdriveUser->SetLabel(m_listBoxGdriveUsers->GetString(selIdx));
        updateGui(); //enable remove user button
    }
}


void CloudSetupDlg::OnDetectServerChannelLimit(wxCommandEvent& event)
{
    assert (type_ == CloudType::sftp);
    const SftpPathInfo pi = getResolvedSftpPath(getFolderPathPhrase()); //noexcept
    try
    {
        const int channelCountMax = getServerMaxChannelsPerConnection(pi.login); //throw FileError
        m_spinCtrlChannelCountSftp->SetValue(channelCountMax);
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
    }
}


void CloudSetupDlg::OnToggleShowPassword(wxCommandEvent& event)
{
    assert(type_ != CloudType::gdrive);
    if (m_checkBoxShowPassword->GetValue())
        m_textCtrlPasswordVisible->ChangeValue(m_textCtrlPasswordHidden->GetValue());
    else
        m_textCtrlPasswordHidden->ChangeValue(m_textCtrlPasswordVisible->GetValue());
    updateGui();
}


bool CloudSetupDlg::acceptFileDrop(const std::vector<Zstring>& shellItemPaths)
{
    if (shellItemPaths.empty())
        return false;
    const Zstring ext = getFileExtension(shellItemPaths[0]);
    return ext.empty() || equalAsciiNoCase(ext, Zstr("pem"));
}


void CloudSetupDlg::onKeyFileDropped(FileDropEvent& event)
{
    //assert (type_ == CloudType::SFTP); -> no big deal if false
    const auto& itemPaths = event.getPaths();
    if (!itemPaths.empty())
    {
        m_textCtrlKeyfilePath->ChangeValue(utfTo<wxString>(itemPaths[0]));

        sftpAuthType_ = SftpAuthType::KEY_FILE;
        updateGui();
    }
}


void CloudSetupDlg::OnSelectKeyfile(wxCommandEvent& event)
{
    assert (type_ == CloudType::sftp && sftpAuthType_ == SftpAuthType::KEY_FILE);
    wxFileDialog filePicker(this,
                            wxString(), //message
                            beforeLast(m_textCtrlKeyfilePath->GetValue(), utfTo<wxString>(FILE_NAME_SEPARATOR), IF_MISSING_RETURN_NONE), //default folder
                            wxString(), //default file name
                            _("All files") + L" (*.*)|*" + L"|" + L"OpenSSL PEM (*.pem)|*.pem",
                            wxFD_OPEN);
    if (filePicker.ShowModal() == wxID_OK)
        m_textCtrlKeyfilePath->ChangeValue(filePicker.GetPath());
}


void CloudSetupDlg::updateGui()
{

    m_toggleBtnGdrive->SetValue(type_ == CloudType::gdrive);
    m_toggleBtnSftp  ->SetValue(type_ == CloudType::sftp);
    m_toggleBtnFtp   ->SetValue(type_ == CloudType::ftp);

    bSizerGdrive->Show(type_ == CloudType::gdrive);
    bSizerServer->Show(type_ == CloudType::ftp || type_ == CloudType::sftp);
    bSizerAuth  ->Show(type_ == CloudType::ftp || type_ == CloudType::sftp);

    bSizerFtpEncrypt->Show(type_ == CloudType:: ftp);
    bSizerSftpAuth  ->Show(type_ == CloudType::sftp);

    m_staticTextKeyfile->Show(type_ == CloudType::sftp && sftpAuthType_ == SftpAuthType::KEY_FILE);
    bSizerKeyFile      ->Show(type_ == CloudType::sftp && sftpAuthType_ == SftpAuthType::KEY_FILE);

    m_staticTextPassword->Show(type_ == CloudType::ftp || (type_ == CloudType::sftp && sftpAuthType_ != SftpAuthType::AGENT));
    bSizerPassword      ->Show(type_ == CloudType::ftp || (type_ == CloudType::sftp && sftpAuthType_ != SftpAuthType::AGENT));
    if (m_staticTextPassword->IsShown())
    {
        m_textCtrlPasswordVisible->Show( m_checkBoxShowPassword->GetValue());
        m_textCtrlPasswordHidden ->Show(!m_checkBoxShowPassword->GetValue());
    }

    bSizerAccessTimeout->Show(type_ == CloudType::ftp || type_ == CloudType::sftp);

    switch (type_)
    {
        case CloudType::gdrive:
            m_buttonGdriveRemoveUser->Enable(m_listBoxGdriveUsers->GetSelection() != wxNOT_FOUND);
            break;

        case CloudType::sftp:
            m_radioBtnPassword->SetValue(false);
            m_radioBtnKeyfile ->SetValue(false);
            m_radioBtnAgent   ->SetValue(false);

            switch (sftpAuthType_) //*not* owned by GUI controls
            {
                case SftpAuthType::PASSWORD:
                    m_radioBtnPassword->SetValue(true);
                    m_staticTextPassword->SetLabel(_("Password:"));
                    break;
                case SftpAuthType::KEY_FILE:
                    m_radioBtnKeyfile->SetValue(true);
                    m_staticTextPassword->SetLabel(_("Key password:"));
                    break;
                case SftpAuthType::AGENT:
                    m_radioBtnAgent->SetValue(true);
                    break;
            }
            break;

        case CloudType::ftp:
            m_staticTextPassword->SetLabel(_("Password:"));
            break;
    }

    m_staticTextChannelCountSftp->Show(type_ == CloudType::sftp);
    m_spinCtrlChannelCountSftp  ->Show(type_ == CloudType::sftp);
    m_buttonChannelCountSftp    ->Show(type_ == CloudType::sftp);

    Layout(); //needed! hidden items are not considered during resize
    Refresh();
}


Zstring CloudSetupDlg::getFolderPathPhrase() const
{
    switch (type_)
    {
        case CloudType::gdrive:
            return condenseToGoogleFolderPathPhrase(utfTo<Zstring>(m_staticTextGdriveUser->GetLabel()),
                                                    utfTo<Zstring>(m_textCtrlServerPath  ->GetValue())); //noexcept

        case CloudType::sftp:
        {
            SftpLoginInfo login;
            login.server   = utfTo<Zstring>(m_textCtrlServer  ->GetValue());
            login.port     = stringTo<int> (m_textCtrlPort    ->GetValue()); //0 if empty
            login.username = utfTo<Zstring>(m_textCtrlUserName->GetValue());
            login.authType = sftpAuthType_;
            login.privateKeyFilePath = utfTo<Zstring>(m_textCtrlKeyfilePath->GetValue());
            login.password = utfTo<Zstring>((m_checkBoxShowPassword->GetValue() ? m_textCtrlPasswordVisible : m_textCtrlPasswordHidden)->GetValue());
            login.traverserChannelsPerConnection = m_spinCtrlChannelCountSftp->GetValue();
            login.timeoutSec = m_spinCtrlTimeout->GetValue();

            auto serverPath = utfTo<Zstring>(m_textCtrlServerPath->GetValue());
            //clean up (messy) user input:
            return condenseToSftpFolderPathPhrase(login, serverPath); //noexcept
        }

        case CloudType::ftp:
        {
            FtpLoginInfo login;
            login.server   = utfTo<Zstring>(m_textCtrlServer  ->GetValue());
            login.port     = stringTo<int> (m_textCtrlPort    ->GetValue()); //0 if empty
            login.username = utfTo<Zstring>(m_textCtrlUserName->GetValue());
            login.password = utfTo<Zstring>((m_checkBoxShowPassword->GetValue() ? m_textCtrlPasswordVisible : m_textCtrlPasswordHidden)->GetValue());
            login.useSsl = m_radioBtnEncryptSsl->GetValue();
            login.timeoutSec = m_spinCtrlTimeout->GetValue();

            auto serverPath = utfTo<Zstring>(m_textCtrlServerPath->GetValue());
            //clean up (messy) user input:
            return condenseToFtpFolderPathPhrase(login, serverPath); //noexcept
        }
    }
    assert(false);
    return Zstr("");
}


void CloudSetupDlg::OnBrowseCloudFolder(wxCommandEvent& event)
{
    AbstractPath folderPath = createAbstractPath(getFolderPathPhrase()); //noexcept

    if (!AFS::getParentPath(folderPath))
        try //for (S)FTP it makes more sense to start with the home directory rather than root (which often denies access!)
        {
            if (type_ == CloudType::sftp)
                folderPath.afsPath = getSftpHomePath(getResolvedSftpPath(getFolderPathPhrase()).login); //throw FileError
            if (type_ == CloudType::ftp)
                folderPath.afsPath = getFtpHomePath(getResolvedFtpPath(getFolderPathPhrase()).login); //throw FileError
        }
        catch (const FileError& e)
        {
            showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString()));
            return;
        }

    if (showAbstractFolderPicker(this, folderPath) == ReturnAfsPicker::BUTTON_OKAY)
        m_textCtrlServerPath->ChangeValue(utfTo<wxString>(FILE_NAME_SEPARATOR + folderPath.afsPath.value));
}


void CloudSetupDlg::OnOkay(wxCommandEvent& event)
{
    //------- parameter validation (BEFORE writing output!) -------
    if (type_ == CloudType::sftp && sftpAuthType_ == SftpAuthType::KEY_FILE)
        if (trimCpy(m_textCtrlKeyfilePath->GetValue()).empty())
        {
            showNotificationDialog(this, DialogInfoType::INFO, PopupDialogCfg().setMainInstructions(_("Please enter a file path.")));
            //don't show error icon to follow "Windows' encouraging tone"
            m_textCtrlKeyfilePath->SetFocus();
            return;
        }
    //-------------------------------------------------------------

    folderPathPhraseOut_ = getFolderPathPhrase();
    parallelOpsOut_ = m_spinCtrlConnectionCount->GetValue();

    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}


ReturnSmallDlg::ButtonPressed fff::showCloudSetupDialog(wxWindow* parent, Zstring& folderPathPhrase, size_t& parallelOps, const std::wstring* parallelOpsDisabledReason)
{
    CloudSetupDlg setupDlg(parent, folderPathPhrase, parallelOps, parallelOpsDisabledReason);
    return static_cast<ReturnSmallDlg::ButtonPressed>(setupDlg.ShowModal());
}

//########################################################################################

class CopyToDialog : public CopyToDlgGenerated
{
public:
    CopyToDialog(wxWindow* parent,
                 std::span<const FileSystemObject* const> rowsOnLeft,
                 std::span<const FileSystemObject* const> rowsOnRight,
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
                           std::span<const FileSystemObject* const> rowsOnLeft,
                           std::span<const FileSystemObject* const> rowsOnRight,
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

    targetFolder = std::make_unique<FolderSelector>(this, *this, *m_buttonSelectTargetFolder, *m_bpButtonSelectAltTargetFolder, *m_targetFolderPath, nullptr /*staticText*/, nullptr /*wxWindow*/,
                                                    nullptr /*droppedPathsFilter*/,
    [](const Zstring& folderPathPhrase) { return 1; } /*getDeviceParallelOps*/,
    nullptr /*setDeviceParallelOps*/);

    m_targetFolderPath->init(folderHistory_);

    m_textCtrlFileList->SetMinSize(wxSize(fastFromDIP(500), fastFromDIP(200)));

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
    m_staticTextHeader->Wrap(fastFromDIP(460)); //needs to be reapplied after SetLabel()

    m_textCtrlFileList->ChangeValue(selectionInfo.first);

    //----------------- set config ---------------------------------
    targetFolder               ->setPath(lastUsedPath);
    m_checkBoxKeepRelPath      ->SetValue(keepRelPaths);
    m_checkBoxOverwriteIfExists->SetValue(overwriteIfExists);
    //----------------- /set config --------------------------------

    //enable dialog-specific key events
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
                                                    std::span<const FileSystemObject* const> rowsOnLeft,
                                                    std::span<const FileSystemObject* const> rowsOnRight,
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
                 std::span<const FileSystemObject* const> rowsOnLeft,
                 std::span<const FileSystemObject* const> rowsOnRight,
                 bool& useRecycleBin);

private:
    void OnUseRecycler(wxCommandEvent& event) override { updateGui(); }
    void OnOK         (wxCommandEvent& event) override;
    void OnCancel     (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void OnClose      (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

    void onLocalKeyEvent(wxKeyEvent& event);

    void updateGui();

    const std::span<const FileSystemObject* const> rowsToDeleteOnLeft_;
    const std::span<const FileSystemObject* const> rowsToDeleteOnRight_;
    const std::chrono::steady_clock::time_point dlgStartTime_ = std::chrono::steady_clock::now();

    //output-only parameters:
    bool& useRecycleBinOut_;
};


DeleteDialog::DeleteDialog(wxWindow* parent,
                           std::span<const FileSystemObject* const> rowsOnLeft,
                           std::span<const FileSystemObject* const> rowsOnRight,
                           bool& useRecycleBin) :
    DeleteDlgGenerated(parent),
    rowsToDeleteOnLeft_(rowsOnLeft),
    rowsToDeleteOnRight_(rowsOnRight),
    useRecycleBinOut_(useRecycleBin)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOK).setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextHeader);

    m_textCtrlFileList->SetMinSize(wxSize(fastFromDIP(500), fastFromDIP(200)));

    m_checkBoxUseRecycler->SetValue(useRecycleBin);

    updateGui();

    //enable dialog-specific key events
    Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(DeleteDialog::onLocalKeyEvent), nullptr, this);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
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
    m_staticTextHeader->Wrap(fastFromDIP(460)); //needs to be reapplied after SetLabel()

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
                                                    std::span<const FileSystemObject* const> rowsOnLeft,
                                                    std::span<const FileSystemObject* const> rowsOnRight,
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
                        bool syncSelection,
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
                                         bool syncSelection,
                                         const wxString& variantName,
                                         const SyncStatistics& st,
                                         bool& dontShowAgain) :
    SyncConfirmationDlgGenerated(parent),
    dontShowAgainOut_(dontShowAgain)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonStartSync).setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextCaption);
    m_bitmapSync->SetBitmap(getResourceImage(syncSelection ? L"file_sync_selection" : L"file_sync"));

    m_staticTextCaption->SetLabel(syncSelection ?_("Start to synchronize the selection?") : _("Start synchronization now?"));
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

    setValue(*m_staticTextData, st.getBytesToProcess() == 0, formatFilesizeShort(st.getBytesToProcess()), *m_bitmapData, L"data");
    setIntValue(*m_staticTextCreateLeft,  st.createCount< LEFT_SIDE>(), *m_bitmapCreateLeft,  L"so_create_left_sicon");
    setIntValue(*m_staticTextUpdateLeft,  st.updateCount< LEFT_SIDE>(), *m_bitmapUpdateLeft,  L"so_update_left_sicon");
    setIntValue(*m_staticTextDeleteLeft,  st.deleteCount< LEFT_SIDE>(), *m_bitmapDeleteLeft,  L"so_delete_left_sicon");
    setIntValue(*m_staticTextCreateRight, st.createCount<RIGHT_SIDE>(), *m_bitmapCreateRight, L"so_create_right_sicon");
    setIntValue(*m_staticTextUpdateRight, st.updateCount<RIGHT_SIDE>(), *m_bitmapUpdateRight, L"so_update_right_sicon");
    setIntValue(*m_staticTextDeleteRight, st.deleteCount<RIGHT_SIDE>(), *m_bitmapDeleteRight, L"so_delete_right_sicon");

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
                                                           bool syncSelection,
                                                           const wxString& variantName,
                                                           const SyncStatistics& statistics,
                                                           bool& dontShowAgain)
{
    SyncConfirmationDlg dlg(parent,
                            syncSelection,
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
    void OnShowLogFolder   (wxHyperlinkEvent& event) override;
    void OnToggleLogfilesLimit(wxCommandEvent& event) override { updateGui(); }

    void OnSelectSoundCompareDone(wxCommandEvent& event) override { selectSound(*m_textCtrlSoundPathCompareDone); }
    void OnSelectSoundSyncDone   (wxCommandEvent& event) override { selectSound(*m_textCtrlSoundPathSyncDone); }
    void selectSound(wxTextCtrl& txtCtrl);

    void OnChangeSoundFilePath(wxCommandEvent& event) override { updateGui(); }

    void OnPlayCompareDone(wxCommandEvent& event) override { wxSound::Play(trimCpy(m_textCtrlSoundPathCompareDone->GetValue()), wxSOUND_ASYNC); }
    void OnPlaySyncDone   (wxCommandEvent& event) override { wxSound::Play(trimCpy(m_textCtrlSoundPathSyncDone   ->GetValue()), wxSOUND_ASYNC); }

    void onResize(wxSizeEvent& event);
    void updateGui();

    //work around defunct keyboard focus on macOS (or is it wxMac?) => not needed for this dialog!
    //void onLocalKeyEvent(wxKeyEvent& event);

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

    m_spinCtrlLogFilesMaxAge->SetMinSize(wxSize(fastFromDIP(70), -1)); //Hack: set size (why does wxWindow::Size() not work?)
    m_hyperlinkLogFolder->SetLabel(utfTo<wxString>(getDefaultLogFolderPath()));
    setRelativeFontSize(*m_hyperlinkLogFolder, 1.2);

    m_staticTextResetDialogs->Wrap(std::max(fastFromDIP(250), m_buttonResetDialogs->GetMinSize().x));

    m_bitmapSettings          ->SetBitmap     (getResourceImage(L"settings"));
    m_bitmapLogFile           ->SetBitmap(shrinkImage(getResourceImage(L"log_file").ConvertToImage(), fastFromDIP(20)));
    m_bitmapNotificationSounds->SetBitmap     (getResourceImage(L"notification_sounds"));
    m_bitmapCompareDone       ->SetBitmap     (getResourceImage(L"compare_sicon"));
    m_bitmapSyncDone          ->SetBitmap     (getResourceImage(L"file_sync_sicon"));
    m_bpButtonPlayCompareDone ->SetBitmapLabel(getResourceImage(L"play_sound"));
    m_bpButtonPlaySyncDone    ->SetBitmapLabel(getResourceImage(L"play_sound"));
    m_bpButtonAddRow          ->SetBitmapLabel(getResourceImage(L"item_add"));
    m_bpButtonRemoveRow       ->SetBitmapLabel(getResourceImage(L"item_remove"));

    //--------------------------------------------------------------------------------
    m_checkBoxFailSafe       ->SetValue(globalSettings.failSafeFileCopy);
    m_checkBoxCopyLocked     ->SetValue(globalSettings.copyLockedFiles);
    m_checkBoxCopyPermissions->SetValue(globalSettings.copyFilePermissions);

    m_checkBoxLogFilesMaxAge->SetValue(globalSettings.logfilesMaxAgeDays > 0);
    m_spinCtrlLogFilesMaxAge->SetValue(globalSettings.logfilesMaxAgeDays > 0 ? globalSettings.logfilesMaxAgeDays : XmlGlobalSettings().logfilesMaxAgeDays);

    m_textCtrlSoundPathCompareDone->ChangeValue(utfTo<wxString>(globalSettings.soundFileCompareFinished));
    m_textCtrlSoundPathSyncDone   ->ChangeValue(utfTo<wxString>(globalSettings.soundFileSyncFinished));

    setExtApp(globalSettings.gui.externalApps);
    //--------------------------------------------------------------------------------

    updateGui();

    bSizerLockedFiles->Show(false);

    const wxString toolTip = wxString(_("Integrate external applications into context menu. The following macros are available:")) + L"\n\n" +
                             L"%item_path%    \t" + _("Full file or folder path") + L"\n" +
                             L"%folder_path%  \t" + _("Parent folder path") + L"\n" +
                             L"%local_path%   \t" + _("Temporary local copy for SFTP and MTP storage") + L"\n" +
                             L"\n" +
                             L"%item_path2%, %folder_path2%, %local_path2% \t" + _("Parameters for opposite side");

    m_gridCustomCommand->GetGridWindow()        ->SetToolTip(toolTip);
    m_gridCustomCommand->GetGridColLabelWindow()->SetToolTip(toolTip);
    m_gridCustomCommand->SetMargins(0, 0);

    //temporarily set dummy value for window height calculations:
    setExtApp(std::vector<ExternalApp>(globalSettings.gui.externalApps.size() + 1));

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    //restore actual value:
    setExtApp(globalSettings.gui.externalApps);

    //automatically fit column width to match total grid width
    Connect(wxEVT_SIZE, wxSizeEventHandler(OptionsDlg::onResize), nullptr, this);
    wxSizeEvent dummy;
    onResize(dummy);

    m_buttonOkay->SetFocus();
}


void OptionsDlg::onResize(wxSizeEvent& event)
{
    const int widthTotal = m_gridCustomCommand->GetGridWindow()->GetClientSize().GetWidth();
    assert(m_gridCustomCommand->GetNumberCols() == 2);

    const int w0 = widthTotal * 2 / 5; //ratio 2 : 3
    const int w1 = widthTotal - w0;
    m_gridCustomCommand->SetColSize(0, w0);
    m_gridCustomCommand->SetColSize(1, w1);

    m_gridCustomCommand->Refresh(); //required on Ubuntu

    event.Skip();
}


void OptionsDlg::updateGui()
{
    const bool haveHiddenDialogs = confirmDlgs_             != defaultCfg_.confirmDlgs ||
                                   warnDlgs_                != defaultCfg_.warnDlgs    ||
                                   autoCloseProgressDialog_ != defaultCfg_.autoCloseProgressDialog;

    setBitmapTextLabel(*m_buttonResetDialogs, shrinkImage(getResourceImage(L"msg_warning").ConvertToImage(), fastFromDIP(20)),
                       haveHiddenDialogs ? _("Show hidden dialogs again") : _("All dialogs shown"));
    Layout();
    m_buttonResetDialogs->Enable(haveHiddenDialogs);

    m_spinCtrlLogFilesMaxAge->Enable(m_checkBoxLogFilesMaxAge->GetValue());

    m_bpButtonPlayCompareDone->Enable(!trimCpy(m_textCtrlSoundPathCompareDone->GetValue()).empty());
    m_bpButtonPlaySyncDone   ->Enable(!trimCpy(m_textCtrlSoundPathSyncDone   ->GetValue()).empty());
}


void OptionsDlg::OnResetDialogs(wxCommandEvent& event)
{
    confirmDlgs_             = defaultCfg_.confirmDlgs;
    warnDlgs_                = defaultCfg_.warnDlgs;
    autoCloseProgressDialog_ = defaultCfg_.autoCloseProgressDialog;
    updateGui();
}


void OptionsDlg::selectSound(wxTextCtrl& txtCtrl)
{
    wxString defaultFolderPath = beforeLast(txtCtrl.GetValue(), utfTo<wxString>(FILE_NAME_SEPARATOR), IF_MISSING_RETURN_NONE);
    if (defaultFolderPath.empty())
        defaultFolderPath = utfTo<wxString>(beforeLast(getResourceDirPf(), FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL));

    wxFileDialog filePicker(this,
                            wxString(), //message
                            defaultFolderPath,
                            wxString(), //default file name
                            wxString(L"WAVE (*.wav)|*.wav") + L"|" + _("All files") + L" (*.*)|*",
                            wxFD_OPEN);
    if (filePicker.ShowModal() != wxID_OK)
        return;

    txtCtrl.ChangeValue(filePicker.GetPath());
    updateGui();
}


void OptionsDlg::OnDefault(wxCommandEvent& event)
{
    m_checkBoxFailSafe       ->SetValue(defaultCfg_.failSafeFileCopy);
    m_checkBoxCopyLocked     ->SetValue(defaultCfg_.copyLockedFiles);
    m_checkBoxCopyPermissions->SetValue(defaultCfg_.copyFilePermissions);

    m_checkBoxLogFilesMaxAge->SetValue(defaultCfg_.logfilesMaxAgeDays > 0);
    m_spinCtrlLogFilesMaxAge->SetValue(defaultCfg_.logfilesMaxAgeDays > 0 ? defaultCfg_.logfilesMaxAgeDays : 14);

    m_textCtrlSoundPathCompareDone->ChangeValue(utfTo<wxString>(defaultCfg_.soundFileCompareFinished));
    m_textCtrlSoundPathSyncDone   ->ChangeValue(utfTo<wxString>(defaultCfg_.soundFileSyncFinished));

    setExtApp(defaultCfg_.gui.externalApps);

    updateGui();
}


void OptionsDlg::OnOkay(wxCommandEvent& event)
{
    //write settings only when okay-button is pressed (except hidden dialog reset)!
    globalCfgOut_.failSafeFileCopy    = m_checkBoxFailSafe->GetValue();
    globalCfgOut_.copyLockedFiles     = m_checkBoxCopyLocked->GetValue();
    globalCfgOut_.copyFilePermissions = m_checkBoxCopyPermissions->GetValue();

    globalCfgOut_.logfilesMaxAgeDays = m_checkBoxLogFilesMaxAge->GetValue() ? m_spinCtrlLogFilesMaxAge->GetValue() : -1;

    globalCfgOut_.soundFileCompareFinished = utfTo<Zstring>(trimCpy(m_textCtrlSoundPathCompareDone->GetValue()));
    globalCfgOut_.soundFileSyncFinished    = utfTo<Zstring>(trimCpy(m_textCtrlSoundPathSyncDone   ->GetValue()));

    globalCfgOut_.gui.externalApps = getExtApp();

    globalCfgOut_.confirmDlgs             = confirmDlgs_;
    globalCfgOut_.warnDlgs                = warnDlgs_;
    globalCfgOut_.autoCloseProgressDialog = autoCloseProgressDialog_;

    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}


void OptionsDlg::setExtApp(const std::vector<ExternalApp>& extApps)
{
    int rowDiff = static_cast<int>(extApps.size()) - m_gridCustomCommand->GetNumberRows();
    ++rowDiff; //append empty row to facilitate insertions by user

    if (rowDiff >= 0)
        m_gridCustomCommand->AppendRows(rowDiff);
    else
        m_gridCustomCommand->DeleteRows(0, -rowDiff);

    for (auto it = extApps.begin(); it != extApps.end(); ++it)
    {
        const int row = it - extApps.begin();

        const std::wstring description = translate(it->description);
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

    wxSizeEvent dummy2;
    onResize(dummy2);
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

    wxSizeEvent dummy2;
    onResize(dummy2);
}


void OptionsDlg::OnShowLogFolder(wxHyperlinkEvent& event)
{
    try
    {
        openWithDefaultApplication(getDefaultLogFolderPath()); //throw FileError
    }
    catch (const FileError& e) { showNotificationDialog(this, DialogInfoType::ERROR2, PopupDialogCfg().setDetailInstructions(e.toString())); }
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

    //enable dialog-specific key events
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
    to  .ResetTime(); //reset local(!) time
    to += wxTimeSpan::Day();
    to -= wxTimeSpan::Second(); //go back to end of previous day

    timeFromOut_ = from.GetTicks();
    timeToOut_   = to  .GetTicks();

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

    m_staticTextHighlight->Wrap(fastFromDIP(300));

    m_spinCtrlOverdueDays->SetMinSize(wxSize(fastFromDIP(70), -1)); //Hack: set size (why does wxWindow::Size() not work?)

    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    m_spinCtrlOverdueDays->SetValue(cfgHistSyncOverdueDays);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_spinCtrlOverdueDays->SetFocus();
}


void CfgHighlightDlg::OnOkay(wxCommandEvent& event)
{
    cfgHistSyncOverdueDaysOut_ = m_spinCtrlOverdueDays->GetValue();
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

    SetTitle(std::wstring(L"FreeFileSync ") + ffsVersion + L" [" + _("Donation Edition") + L"]");

    //setMainInstructionFont(*m_staticTextMain);

    m_bitmapActivation->SetBitmap(getResourceImage(L"website"));
    m_textCtrlOfflineActivationKey->ForceUpper();

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
    m_staticTextHeader->Wrap(fastFromDIP(460)); //*after* font change!

    m_staticTextDetails->SetMinSize(wxSize(fastFromDIP(550), -1));

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

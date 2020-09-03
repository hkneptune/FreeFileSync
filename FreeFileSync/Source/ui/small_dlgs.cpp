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
#include <zen/file_io.h>
#include <zen/http.h>
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
#include "../afs/concrete.h"
#include "../afs/gdrive.h"
#include "../afs/ftp.h"
#include "../afs/sftp.h"
#include "../base/algorithm.h"
#include "../base/synchronization.h"
#include "../base/path_filter.h"
#include "../status_handler.h" //uiUpdateDue()
#include "../version/version.h"
#include "../log_file.h"
#include "../ffs_paths.h"
#include "../help_provider.h"
#include "../icon_buffer.h"



using namespace zen;
using namespace fff;


namespace
{
class AboutDlg : public AboutDlgGenerated
{
public:
    AboutDlg(wxWindow* parent);

private:
    void onOkay  (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_OKAY); }
    void onClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void onDonate(wxCommandEvent& event) override { wxLaunchDefaultBrowser(L"https://freefilesync.org/donate.php"); }
    void onOpenHomepage(wxCommandEvent& event) override { wxLaunchDefaultBrowser(L"https://freefilesync.org/"); }
    void onOpenForum   (wxCommandEvent& event) override { wxLaunchDefaultBrowser(L"https://freefilesync.org/forum/"); }
    void onSendEmail   (wxCommandEvent& event) override { wxLaunchDefaultBrowser(L"mailto:zenju@" L"freefilesync.org"); }
    void onShowGpl     (wxCommandEvent& event) override { wxLaunchDefaultBrowser(L"https://www.gnu.org/licenses/gpl-3.0"); }

    void onLocalKeyEvent(wxKeyEvent& event);
};


AboutDlg::AboutDlg(wxWindow* parent) : AboutDlgGenerated(parent)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonClose));

    assert(m_buttonClose->GetId() == wxID_OK); //we cannot use wxID_CLOSE else Esc key won't work: yet another wxWidgets bug??

    m_bitmapLogo    ->SetBitmap(loadImage("logo"));
    m_bitmapLogoLeft->SetBitmap(loadImage("logo-left"));


    //------------------------------------
    wxString build = utfTo<wxString>(ffsVersion);
#ifndef wxUSE_UNICODE
#error what is going on?
#endif

    const wchar_t* const SPACED_BULLET = L" \u2022 ";
    build += SPACED_BULLET;

    build += LTR_MARK; //fix Arabic
#ifndef ZEN_BUILD_ARCH
#error include <zen/build_info.h>
#endif
#if ZEN_BUILD_ARCH == ZEN_ARCH_32BIT
    build += L"32 Bit";
#else
    build += L"64 Bit";
#endif

    build += SPACED_BULLET;
    build += utfTo<wxString>(formatTime(formatDateTag, getCompileTime()));

    m_staticTextVersion->SetLabel(replaceCpy(_("Version: %x"), L"%x", build));

    //------------------------------------
    {
        m_panelThankYou->Hide();
        m_bitmapDonate->SetBitmap(loadImage("ffs_heart"));
        setRelativeFontSize(*m_staticTextDonate, 1.25);
        setRelativeFontSize(*m_buttonDonate, 1.25);
    }

    //------------------------------------
    wxImage forumImage = stackImages(loadImage("ffs_forum"),
                                     createImageFromText(L"FreeFileSync Forum", *wxNORMAL_FONT, m_bpButtonForum->GetForegroundColour()),
                                     ImageStackLayout::vertical, ImageStackAlignment::center, fastFromDIP(5));
    m_bpButtonForum->SetBitmapLabel(forumImage);

    setBitmapTextLabel(*m_bpButtonHomepage, loadImage("ffs_homepage"), L"FreeFileSync.org");
    setBitmapTextLabel(*m_bpButtonEmail,    loadImage("ffs_email"   ), L"zenju@" L"freefilesync.org");
    m_bpButtonEmail->SetToolTip(L"mailto:zenju@" L"freefilesync.org");

    //------------------------------------
    m_bpButtonGpl->SetBitmapLabel(loadImage("gpl"));

    //have the GPL text wrap to two lines:
    wxMemoryDC dc;
    dc.SetFont(m_staticTextGpl->GetFont());
    const wxSize gplExt = dc.GetTextExtent(m_staticTextGpl->GetLabelText());
    m_staticTextGpl->Wrap(gplExt.GetWidth() * 6 / 10);

    //------------------------------------
    m_staticTextThanksForLoc->SetMinSize({fastFromDIP(200), -1});
    m_staticTextThanksForLoc->Wrap(fastFromDIP(200));

    for (const TranslationInfo& ti : getExistingTranslations())
    {
        //country flag
        wxStaticBitmap* staticBitmapFlag = new wxStaticBitmap(m_scrolledWindowTranslators, wxID_ANY, wxBitmap(loadImage(ti.languageFlag)));
        fgSizerTranslators->Add(staticBitmapFlag, 0, wxALIGN_CENTER);

        //translator name
        wxStaticText* staticTextTranslator = new wxStaticText(m_scrolledWindowTranslators, wxID_ANY, ti.translatorName, wxDefaultPosition, wxDefaultSize, 0);
        fgSizerTranslators->Add(staticTextTranslator, 0, wxALIGN_CENTER_VERTICAL);

        staticBitmapFlag    ->SetToolTip(ti.languageName);
        staticTextTranslator->SetToolTip(ti.languageName);
    }
    fgSizerTranslators->Fit(m_scrolledWindowTranslators);

    //------------------------------------

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); }); //enable dialog-specific key events

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonClose->SetFocus(); //on GTK ESC is only associated with wxID_OK correctly if we set at least *any* focus at all!!!
}


void AboutDlg::onLocalKeyEvent(wxKeyEvent& event) //process key events without explicit menu entry :)
{
    event.Skip();
}
}

void fff::showAboutDialog(wxWindow* parent)
{
    AboutDlg dlg(parent);
    dlg.ShowModal();
}

//########################################################################################

namespace
{
class CloudSetupDlg : public CloudSetupDlgGenerated
{
public:
    CloudSetupDlg(wxWindow* parent, Zstring& folderPathPhrase, size_t& parallelOps, const std::wstring* parallelOpsDisabledReason);

private:
    void onOkay  (wxCommandEvent& event) override;
    void onCancel(wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void onClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

    void onGdriveUserAdd   (wxCommandEvent& event) override;
    void onGdriveUserRemove(wxCommandEvent& event) override;
    void onGdriveUserSelect(wxCommandEvent& event) override;
    void gdriveUpdateDrivesAndSelect(const std::string& accountEmail, const Zstring& sharedDriveName);

    void onDetectServerChannelLimit(wxCommandEvent& event) override;
    void onToggleShowPassword(wxCommandEvent& event) override;
    void onBrowseCloudFolder (wxCommandEvent& event) override;
    void onHelpFtpPerformance(wxHyperlinkEvent& event) override { displayHelpEntry(L"ftp-setup", this); }

    void onConnectionGdrive(wxCommandEvent& event) override { type_ = CloudType::gdrive; updateGui(); }
    void onConnectionSftp  (wxCommandEvent& event) override { type_ = CloudType::sftp;   updateGui(); }
    void onConnectionFtp   (wxCommandEvent& event) override { type_ = CloudType::ftp;    updateGui(); }

    void onAuthPassword(wxCommandEvent& event) override { sftpAuthType_ = SftpAuthType::password; updateGui(); }
    void onAuthKeyfile (wxCommandEvent& event) override { sftpAuthType_ = SftpAuthType::keyFile;  updateGui(); }
    void onAuthAgent   (wxCommandEvent& event) override { sftpAuthType_ = SftpAuthType::agent;    updateGui(); }

    void onSelectKeyfile(wxCommandEvent& event) override;

    void updateGui();

    //work around defunct keyboard focus on macOS (or is it wxMac?) => not needed for this dialog!
    //void onLocalKeyEvent(wxKeyEvent& event);

    static bool acceptFileDrop(const std::vector<Zstring>& shellItemPaths);
    void onKeyFileDropped(FileDropEvent& event);

    AbstractPath getFolderPath() const;

    enum class CloudType
    {
        gdrive,
        sftp,
        ftp,
    };
    CloudType type_ = CloudType::gdrive;

    const wxString txtLoading_ = L'(' + _("Loading...") + L')';
    const wxString txtMyDrive_ = _("My Drive");

    const SftpLogin sftpDefault_;

    SftpAuthType sftpAuthType_ = sftpDefault_.authType;

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

    m_toggleBtnGdrive->SetBitmap(loadImage("google_drive"));
    m_toggleBtnSftp  ->SetBitmap(getTransparentPixel()); //set dummy image (can't be empty!): text-only buttons are rendered smaller on OS X!
    m_toggleBtnFtp   ->SetBitmap(getTransparentPixel()); //

    setRelativeFontSize(*m_toggleBtnGdrive, 1.25);
    setRelativeFontSize(*m_toggleBtnSftp,   1.25);
    setRelativeFontSize(*m_toggleBtnFtp,    1.25);

    setBitmapTextLabel(*m_buttonGdriveAddUser,    loadImage("user_add",    fastFromDIP(20)), m_buttonGdriveAddUser   ->GetLabel());
    setBitmapTextLabel(*m_buttonGdriveRemoveUser, loadImage("user_remove", fastFromDIP(20)), m_buttonGdriveRemoveUser->GetLabel());

    m_bitmapGdriveUser ->SetBitmap(loadImage("user",   fastFromDIP(20)));
    m_bitmapGdriveDrive->SetBitmap(loadImage("drive",  fastFromDIP(20)));
    m_bitmapServer     ->SetBitmap(loadImage("server", fastFromDIP(20)));
    m_bitmapCloud      ->SetBitmap(loadImage("cloud"));
    m_bitmapPerf       ->SetBitmap(loadImage("speed"));
    m_bitmapServerDir->SetBitmap(IconBuffer::genericDirIcon(IconBuffer::SIZE_SMALL));
    m_checkBoxShowPassword->SetValue(false);

    m_textCtrlServer->SetHint(_("Example:") + L"    website.com    66.198.240.22");
    m_textCtrlServer->SetMinSize({fastFromDIP(260), -1});

    m_textCtrlPort            ->SetMinSize({fastFromDIP(60), -1}); //
    m_spinCtrlConnectionCount ->SetMinSize({fastFromDIP(70), -1}); //Hack: set size (why does wxWindow::Size() not work?)
    m_spinCtrlChannelCountSftp->SetMinSize({fastFromDIP(70), -1}); //
    m_spinCtrlTimeout         ->SetMinSize({fastFromDIP(70), -1}); //

    setupFileDrop(*m_panelAuth);
    m_panelAuth->Bind(EVENT_DROP_FILE, [this](FileDropEvent& event) { onKeyFileDropped(event); });

    m_staticTextConnectionsLabelSub->SetLabel(L'(' + _("Connections") + L')');

    //use spacer to keep dialog height stable, no matter if key file options are visible
    bSizerAuthInner->Add(0, m_panelAuth->GetSize().y);

    //---------------------------------------------------------
    std::vector<wxString> gdriveAccounts;
    try
    {
        for (const std::string& loginEmail : gdriveListAccounts()) //throw FileError
            gdriveAccounts.push_back(utfTo<wxString>(loginEmail));
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString()));
    }
    m_listBoxGdriveUsers->Append(gdriveAccounts);

    //set default values for Google Drive: use first item of m_listBoxGdriveUsers
    if (!gdriveAccounts.empty() && !acceptsItemPathPhraseGdrive(folderPathPhrase))
    {
        m_listBoxGdriveUsers->SetSelection(0);
        gdriveUpdateDrivesAndSelect(utfTo<std::string>(gdriveAccounts[0]), Zstring() /*My Drive*/);
    }

    m_spinCtrlTimeout->SetValue(sftpDefault_.timeoutSec);
    assert(sftpDefault_.timeoutSec == FtpLogin().timeoutSec); //make sure the default values are in sync

    //---------------------------------------------------------
    if (acceptsItemPathPhraseGdrive(folderPathPhrase))
    {
        type_ = CloudType::gdrive;
        const AbstractPath folderPath = createItemPathGdrive(folderPathPhrase);
        const GdriveLogin login = extractGdriveLogin(folderPath.afsDevice); //noexcept

        if (const int selPos = m_listBoxGdriveUsers->FindString(utfTo<wxString>(login.email), false /*caseSensitive*/);
            selPos != wxNOT_FOUND)
        {
            m_listBoxGdriveUsers->EnsureVisible(selPos);
            m_listBoxGdriveUsers->SetSelection(selPos);
            gdriveUpdateDrivesAndSelect(login.email, login.sharedDriveName);
        }
        else
        {
            m_listBoxGdriveUsers->DeselectAll();
            m_listBoxGdriveDrives->Clear();
        }

        m_textCtrlServerPath->ChangeValue(utfTo<wxString>(FILE_NAME_SEPARATOR + folderPath.afsPath.value));
    }
    else if (acceptsItemPathPhraseSftp(folderPathPhrase))
    {
        type_ = CloudType::sftp;
        const AbstractPath folderPath = createItemPathSftp(folderPathPhrase);
        const SftpLogin login = extractSftpLogin(folderPath.afsDevice); //noexcept

        if (login.port > 0)
            m_textCtrlPort->ChangeValue(numberTo<wxString>(login.port));
        m_textCtrlServer        ->ChangeValue(utfTo<wxString>(login.server));
        m_textCtrlUserName      ->ChangeValue(utfTo<wxString>(login.username));
        sftpAuthType_ = login.authType;
        m_textCtrlPasswordHidden->ChangeValue(utfTo<wxString>(login.password));
        m_textCtrlKeyfilePath   ->ChangeValue(utfTo<wxString>(login.privateKeyFilePath));
        m_textCtrlServerPath    ->ChangeValue(utfTo<wxString>(FILE_NAME_SEPARATOR + folderPath.afsPath.value));
        m_checkBoxAllowZlib     ->SetValue(login.allowZlib);
        m_spinCtrlTimeout       ->SetValue(login.timeoutSec);
        m_spinCtrlChannelCountSftp->SetValue(login.traverserChannelsPerConnection);
    }
    else if (acceptsItemPathPhraseFtp(folderPathPhrase))
    {
        type_ = CloudType::ftp;
        const AbstractPath folderPath = createItemPathFtp(folderPathPhrase);
        const FtpLogin login = extractFtpLogin(folderPath.afsDevice); //noexcept

        if (login.port > 0)
            m_textCtrlPort->ChangeValue(numberTo<wxString>(login.port));
        m_textCtrlServer         ->ChangeValue(utfTo<wxString>(login.server));
        m_textCtrlUserName       ->ChangeValue(utfTo<wxString>(login.username));
        m_textCtrlPasswordHidden ->ChangeValue(utfTo<wxString>(login.password));
        m_textCtrlServerPath     ->ChangeValue(utfTo<wxString>(FILE_NAME_SEPARATOR + folderPath.afsPath.value));
        (login.useTls ? m_radioBtnEncryptSsl : m_radioBtnEncryptNone)->SetValue(true);
        m_spinCtrlTimeout        ->SetValue(login.timeoutSec);
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
    //---------------------------------------------------------

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


void CloudSetupDlg::onGdriveUserAdd(wxCommandEvent& event)
{
    guiQueue_.processAsync([]() -> std::variant<std::string /*email*/, FileError>
    {
        try
        {
            return gdriveAddUser(nullptr /*updateGui*/); //throw FileError
        }
        catch (const FileError& e) { return e; }
    },
    [this](const std::variant<std::string, FileError>& result)
    {
        if (const FileError* e = std::get_if<FileError>(&result))
            showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e->toString()));
        else
        {
            const std::string& loginEmail = std::get<std::string>(result);

            int selPos = m_listBoxGdriveUsers->FindString(utfTo<wxString>(loginEmail), false /*caseSensitive*/);
            if (selPos == wxNOT_FOUND)
                selPos = m_listBoxGdriveUsers->Append(utfTo<wxString>(loginEmail));

            m_listBoxGdriveUsers->EnsureVisible(selPos);
            m_listBoxGdriveUsers->SetSelection(selPos);
            updateGui(); //enable remove user button
            gdriveUpdateDrivesAndSelect(loginEmail, Zstring() /*My Drive*/);
        }
    });
}


void CloudSetupDlg::onGdriveUserRemove(wxCommandEvent& event)
{
    const int selPos = m_listBoxGdriveUsers->GetSelection();
    assert(selPos != wxNOT_FOUND);
    if (selPos != wxNOT_FOUND)
        try
        {
            const std::string& loginEmail = utfTo<std::string>(m_listBoxGdriveUsers->GetString(selPos));
            if (showConfirmationDialog(this, DialogInfoType::warning, PopupDialogCfg().
                                       setTitle(_("Confirm")).
                                       setMainInstructions(replaceCpy(_("Do you really want to disconnect from user account %x?"), L"%x", utfTo<std::wstring>(loginEmail))),
                                       _("&Disconnect")) != ConfirmationButton::accept)
                return;

            gdriveRemoveUser(loginEmail); //throw FileError
            m_listBoxGdriveUsers->Delete(selPos);
            updateGui(); //disable remove user button
            m_listBoxGdriveDrives->Clear();
        }
        catch (const FileError& e)
        {
            showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString()));
        }
}


void CloudSetupDlg::onGdriveUserSelect(wxCommandEvent& event)
{
    const int selPos = m_listBoxGdriveUsers->GetSelection();
    assert(selPos != wxNOT_FOUND);
    if (selPos != wxNOT_FOUND)
    {
        const std::string& loginEmail = utfTo<std::string>(m_listBoxGdriveUsers->GetString(selPos));
        updateGui(); //enable remove user button
        gdriveUpdateDrivesAndSelect(loginEmail, Zstring() /*My Drive*/);
    }
}


void CloudSetupDlg::gdriveUpdateDrivesAndSelect(const std::string& accountEmail, const Zstring& sharedDriveName)
{
    m_listBoxGdriveDrives->Clear();
    m_listBoxGdriveDrives->Append(txtLoading_);

    guiQueue_.processAsync([accountEmail]() -> std::variant<std::vector<Zstring /*sharedDriveName*/>, FileError>
    {
        try
        {
            return gdriveListSharedDrives(accountEmail); //throw FileError
        }
        catch (const FileError& e) { return e; }
    },
    [this, accountEmail, sharedDriveName](const std::variant<std::vector<Zstring /*sharedDriveName*/>, FileError>& result)
    {
        if (const int selPos = m_listBoxGdriveUsers->GetSelection();
            selPos == wxNOT_FOUND || utfTo<std::string>(m_listBoxGdriveUsers->GetString(selPos)) != accountEmail)
            return; //different accountEmail selected in the meantime!

        m_listBoxGdriveDrives->Clear();

        if (const FileError* e = std::get_if<FileError>(&result))
            showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e->toString()));
        else
        {
            m_listBoxGdriveDrives->Append(txtMyDrive_);

            for (const Zstring& driveName : std::get<std::vector<Zstring>>(result))
                m_listBoxGdriveDrives->Append(utfTo<wxString>(driveName));

            const wxString driveNameLabel = sharedDriveName.empty() ? txtMyDrive_ : utfTo<wxString>(sharedDriveName);

            if (const int selPos = m_listBoxGdriveDrives->FindString(driveNameLabel, true /*caseSensitive*/);
                selPos != wxNOT_FOUND)
            {
                m_listBoxGdriveDrives->EnsureVisible(selPos);
                m_listBoxGdriveDrives->SetSelection (selPos);
            }
        }
    });
}


void CloudSetupDlg::onDetectServerChannelLimit(wxCommandEvent& event)
{
    assert (type_ == CloudType::sftp);
    try
    {
        const int channelCountMax = getServerMaxChannelsPerConnection(extractSftpLogin(getFolderPath().afsDevice)); //throw FileError
        m_spinCtrlChannelCountSftp->SetValue(channelCountMax);
    }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString()));
    }
}


void CloudSetupDlg::onToggleShowPassword(wxCommandEvent& event)
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
    return ext.empty() ||
           equalAsciiNoCase(ext, "pem") ||
           equalAsciiNoCase(ext, "ppk");
}


void CloudSetupDlg::onKeyFileDropped(FileDropEvent& event)
{
    //assert (type_ == CloudType::SFTP); -> no big deal if false
    if (!event.itemPaths_.empty())
    {
        m_textCtrlKeyfilePath->ChangeValue(utfTo<wxString>(event.itemPaths_[0]));

        sftpAuthType_ = SftpAuthType::keyFile;
        updateGui();
    }
}


void CloudSetupDlg::onSelectKeyfile(wxCommandEvent& event)
{
    assert (type_ == CloudType::sftp && sftpAuthType_ == SftpAuthType::keyFile);
    wxFileDialog filePicker(this,
                            wxString(), //message
                            beforeLast(m_textCtrlKeyfilePath->GetValue(), utfTo<wxString>(FILE_NAME_SEPARATOR), IfNotFoundReturn::none), //default folder
                            wxString(), //default file name
                            _("All files") + L" (*.*)|*" +
                            L"|" + L"OpenSSL PEM (*.pem)|*.pem" +
                            L"|" + L"PuTTY Private Key (*.ppk)|*.ppk",
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

    m_staticTextKeyfile->Show(type_ == CloudType::sftp && sftpAuthType_ == SftpAuthType::keyFile);
    bSizerKeyFile      ->Show(type_ == CloudType::sftp && sftpAuthType_ == SftpAuthType::keyFile);

    m_staticTextPassword->Show(type_ == CloudType::ftp || (type_ == CloudType::sftp && sftpAuthType_ != SftpAuthType::agent));
    bSizerPassword      ->Show(type_ == CloudType::ftp || (type_ == CloudType::sftp && sftpAuthType_ != SftpAuthType::agent));
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
                case SftpAuthType::password:
                    m_radioBtnPassword->SetValue(true);
                    m_staticTextPassword->SetLabel(_("Password:"));
                    break;
                case SftpAuthType::keyFile:
                    m_radioBtnKeyfile->SetValue(true);
                    m_staticTextPassword->SetLabel(_("Key passphrase:"));
                    break;
                case SftpAuthType::agent:
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
    m_checkBoxAllowZlib         ->Show(type_ == CloudType::sftp);
    m_staticTextZlibDescr       ->Show(type_ == CloudType::sftp);

    Layout(); //needed! hidden items are not considered during resize
    Refresh();
}


AbstractPath CloudSetupDlg::getFolderPath() const
{
    //clean up (messy) user input, but no trim: support folders with trailing blanks!
    const AfsPath serverRelPath = sanitizeDeviceRelativePath(utfTo<Zstring>(m_textCtrlServerPath->GetValue()));

    switch (type_)
    {
        case CloudType::gdrive:
        {
            GdriveLogin login;
            if (const int selPos = m_listBoxGdriveUsers->GetSelection();
                selPos != wxNOT_FOUND)
            {
                login.email = utfTo<std::string>(m_listBoxGdriveUsers->GetString(selPos));

                if (const int selPos2 = m_listBoxGdriveDrives->GetSelection();
                    selPos2 != wxNOT_FOUND)
                {
                    if (const wxString& sharedDriveName = m_listBoxGdriveDrives->GetString(selPos2);
                        sharedDriveName != txtMyDrive_ &&
                        sharedDriveName != txtLoading_)
                        login.sharedDriveName = utfTo<Zstring>(sharedDriveName);
                }
            }
            return AbstractPath(condenseToGdriveDevice(login), serverRelPath); //noexcept
        }

        case CloudType::sftp:
        {
            SftpLogin login;
            login.server   = utfTo<Zstring>(m_textCtrlServer  ->GetValue());
            login.port     = stringTo<int> (m_textCtrlPort    ->GetValue()); //0 if empty
            login.username = utfTo<Zstring>(m_textCtrlUserName->GetValue());
            login.authType = sftpAuthType_;
            login.privateKeyFilePath = utfTo<Zstring>(m_textCtrlKeyfilePath->GetValue());
            login.password = utfTo<Zstring>((m_checkBoxShowPassword->GetValue() ? m_textCtrlPasswordVisible : m_textCtrlPasswordHidden)->GetValue());
            login.allowZlib  = m_checkBoxAllowZlib->GetValue();
            login.timeoutSec = m_spinCtrlTimeout->GetValue();
            login.traverserChannelsPerConnection = m_spinCtrlChannelCountSftp->GetValue();
            return AbstractPath(condenseToSftpDevice(login), serverRelPath); //noexcept
        }

        case CloudType::ftp:
        {
            FtpLogin login;
            login.server   = utfTo<Zstring>(m_textCtrlServer  ->GetValue());
            login.port     = stringTo<int> (m_textCtrlPort    ->GetValue()); //0 if empty
            login.username = utfTo<Zstring>(m_textCtrlUserName->GetValue());
            login.password = utfTo<Zstring>((m_checkBoxShowPassword->GetValue() ? m_textCtrlPasswordVisible : m_textCtrlPasswordHidden)->GetValue());
            login.useTls = m_radioBtnEncryptSsl->GetValue();
            login.timeoutSec = m_spinCtrlTimeout->GetValue();
            return AbstractPath(condenseToFtpDevice(login), serverRelPath); //noexcept
        }
    }
    assert(false);
    return createAbstractPath(Zstr(""));
}


void CloudSetupDlg::onBrowseCloudFolder(wxCommandEvent& event)
{
    AbstractPath folderPath = getFolderPath(); //noexcept

    if (!AFS::getParentPath(folderPath))
        try //for (S)FTP it makes more sense to start with the home directory rather than root (which often denies access!)
        {
            if (type_ == CloudType::sftp)
                folderPath.afsPath = getSftpHomePath(extractSftpLogin(folderPath.afsDevice)); //throw FileError

            if (type_ == CloudType::ftp)
                folderPath.afsPath = getFtpHomePath(extractFtpLogin(folderPath.afsDevice)); //throw FileError
        }
        catch (const FileError& e)
        {
            showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString()));
            return;
        }

    if (showAbstractFolderPicker(this, folderPath) == ReturnAfsPicker::BUTTON_OKAY)
        m_textCtrlServerPath->ChangeValue(utfTo<wxString>(FILE_NAME_SEPARATOR + folderPath.afsPath.value));
}


void CloudSetupDlg::onOkay(wxCommandEvent& event)
{
    //------- parameter validation (BEFORE writing output!) -------
    if (type_ == CloudType::sftp && sftpAuthType_ == SftpAuthType::keyFile)
        if (trimCpy(m_textCtrlKeyfilePath->GetValue()).empty())
        {
            showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Please enter a file path.")));
            //don't show error icon to follow "Windows' encouraging tone"
            m_textCtrlKeyfilePath->SetFocus();
            return;
        }
    //-------------------------------------------------------------

    folderPathPhraseOut_ = AFS::getInitPathPhrase(getFolderPath());
    parallelOpsOut_ = m_spinCtrlConnectionCount->GetValue();

    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}
}

ReturnSmallDlg::ButtonPressed fff::showCloudSetupDialog(wxWindow* parent, Zstring& folderPathPhrase, size_t& parallelOps, const std::wstring* parallelOpsDisabledReason)
{
    CloudSetupDlg dlg(parent, folderPathPhrase, parallelOps, parallelOpsDisabledReason);
    return static_cast<ReturnSmallDlg::ButtonPressed>(dlg.ShowModal());
}

//########################################################################################

namespace
{
class CopyToDialog : public CopyToDlgGenerated
{
public:
    CopyToDialog(wxWindow* parent,
                 std::span<const FileSystemObject* const> rowsOnLeft,
                 std::span<const FileSystemObject* const> rowsOnRight,
                 Zstring& lastUsedPath,
                 std::vector<Zstring>& folderHistory, size_t folderHistoryMax,
                 bool& keepRelPaths,
                 bool& overwriteIfExists);

private:
    void onOkay  (wxCommandEvent& event) override;
    void onCancel(wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void onClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

    void onLocalKeyEvent(wxKeyEvent& event);

    std::unique_ptr<FolderSelector> targetFolder; //always bound

    //output-only parameters:
    Zstring& lastUsedPathOut_;
    bool& keepRelPathsOut_;
    bool& overwriteIfExistsOut_;
    std::vector<Zstring>& folderHistoryOut_;
};


CopyToDialog::CopyToDialog(wxWindow* parent,
                           std::span<const FileSystemObject* const> rowsOnLeft,
                           std::span<const FileSystemObject* const> rowsOnRight,
                           Zstring& lastUsedPath,
                           std::vector<Zstring>& folderHistory, size_t folderHistoryMax,
                           bool& keepRelPaths,
                           bool& overwriteIfExists) :
    CopyToDlgGenerated(parent),
    lastUsedPathOut_(lastUsedPath),
    keepRelPathsOut_(keepRelPaths),
    overwriteIfExistsOut_(overwriteIfExists),
    folderHistoryOut_(folderHistory)
{

    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOK).setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextHeader);

    m_bitmapCopyTo->SetBitmap(loadImage("copy_to"));

    targetFolder = std::make_unique<FolderSelector>(this, *this, *m_buttonSelectTargetFolder, *m_bpButtonSelectAltTargetFolder, *m_targetFolderPath, nullptr /*staticText*/, nullptr /*wxWindow*/,
                                                    nullptr /*droppedPathsFilter*/,
    [](const Zstring& folderPathPhrase) { return 1; } /*getDeviceParallelOps*/,
    nullptr /*setDeviceParallelOps*/);

    m_targetFolderPath->setHistory(std::make_shared<HistoryList>(folderHistory, folderHistoryMax));

    m_textCtrlFileList->SetMinSize({fastFromDIP(500), fastFromDIP(200)});

    /*  There is a nasty bug on wxGTK under Ubuntu: If a multi-line wxTextCtrl contains so many lines that scrollbars are shown,
        it re-enables all windows that are supposed to be disabled during the current modal loop!
        This only affects Ubuntu/wxGTK! No such issue on Debian/wxGTK or Suse/wxGTK
        => another Unity problem like the following?
        http://trac.wxwidgets.org/ticket/14823 "Menu not disabled when showing modal dialogs in wxGTK under Unity"        */

    const auto& [itemList, itemCount] = getSelectedItemsAsString(rowsOnLeft, rowsOnRight);

    const wxString header = _P("Copy the following item to another folder?",
                               "Copy the following %x items to another folder?", itemCount);
    m_staticTextHeader->SetLabel(header);
    m_staticTextHeader->Wrap(fastFromDIP(460)); //needs to be reapplied after SetLabel()

    m_textCtrlFileList->ChangeValue(itemList);

    //----------------- set config ---------------------------------
    targetFolder               ->setPath(lastUsedPath);
    m_checkBoxKeepRelPath      ->SetValue(keepRelPaths);
    m_checkBoxOverwriteIfExists->SetValue(overwriteIfExists);
    //----------------- /set config --------------------------------

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); }); //enable dialog-specific key events

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonOK->SetFocus();
}


void CopyToDialog::onLocalKeyEvent(wxKeyEvent& event) //process key events without explicit menu entry :)
{
    event.Skip();
}


void CopyToDialog::onOkay(wxCommandEvent& event)
{
    //------- parameter validation (BEFORE writing output!) -------
    if (trimCpy(targetFolder->getPath()).empty())
    {
        showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Please enter a target folder.")));
        //don't show error icon to follow "Windows' encouraging tone"
        m_targetFolderPath->SetFocus();
        return;
    }
    m_targetFolderPath->getHistory()->addItem(targetFolder->getPath());
    //-------------------------------------------------------------

    lastUsedPathOut_      = targetFolder->getPath();
    keepRelPathsOut_      = m_checkBoxKeepRelPath->GetValue();
    overwriteIfExistsOut_ = m_checkBoxOverwriteIfExists->GetValue();
    folderHistoryOut_     = m_targetFolderPath->getHistory()->getList();

    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}
}

ReturnSmallDlg::ButtonPressed fff::showCopyToDialog(wxWindow* parent,
                                                    std::span<const FileSystemObject* const> rowsOnLeft,
                                                    std::span<const FileSystemObject* const> rowsOnRight,
                                                    Zstring& lastUsedPath,
                                                    std::vector<Zstring>& folderHistory, size_t folderHistoryMax,
                                                    bool& keepRelPaths,
                                                    bool& overwriteIfExists)
{
    CopyToDialog dlg(parent, rowsOnLeft, rowsOnRight, lastUsedPath, folderHistory, folderHistoryMax, keepRelPaths, overwriteIfExists);
    return static_cast<ReturnSmallDlg::ButtonPressed>(dlg.ShowModal());
}

//########################################################################################

namespace
{
class DeleteDialog : public DeleteDlgGenerated
{
public:
    DeleteDialog(wxWindow* parent,
                 std::span<const FileSystemObject* const> rowsOnLeft,
                 std::span<const FileSystemObject* const> rowsOnRight,
                 bool& useRecycleBin);

private:
    void onUseRecycler(wxCommandEvent& event) override { updateGui(); }
    void onOkay       (wxCommandEvent& event) override;
    void onCancel     (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void onClose      (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

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

    m_textCtrlFileList->SetMinSize({fastFromDIP(500), fastFromDIP(200)});

    m_checkBoxUseRecycler->SetValue(useRecycleBin);

    updateGui();

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); }); //enable dialog-specific key events

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonOK->SetFocus();
}


void DeleteDialog::updateGui()
{

    const auto& [itemList, itemCount] = getSelectedItemsAsString(rowsToDeleteOnLeft_, rowsToDeleteOnRight_);
    wxString header;
    if (m_checkBoxUseRecycler->GetValue())
    {
        header = _P("Do you really want to move the following item to the recycle bin?",
                    "Do you really want to move the following %x items to the recycle bin?", itemCount);
        m_bitmapDeleteType->SetBitmap(loadImage("delete_recycler"));
        m_buttonOK->SetLabel(_("Move")); //no access key needed: use ENTER!
    }
    else
    {
        header = _P("Do you really want to delete the following item?",
                    "Do you really want to delete the following %x items?", itemCount);
        m_bitmapDeleteType->SetBitmap(loadImage("delete_permanently"));
        m_buttonOK->SetLabel(replaceCpy(_("&Delete"), L"&", L""));
    }
    m_staticTextHeader->SetLabel(header);
    m_staticTextHeader->Wrap(fastFromDIP(460)); //needs to be reapplied after SetLabel()

    m_textCtrlFileList->ChangeValue(itemList);
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


void DeleteDialog::onOkay(wxCommandEvent& event)
{
    //additional safety net, similar to Windows Explorer: time delta between DEL and ENTER must be at least 50ms to avoid accidental deletion!
    if (std::chrono::steady_clock::now() < dlgStartTime_ + std::chrono::milliseconds(50)) //considers chrono-wrap-around!
        return;

    useRecycleBinOut_ = m_checkBoxUseRecycler->GetValue();

    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}
}

ReturnSmallDlg::ButtonPressed fff::showDeleteDialog(wxWindow* parent,
                                                    std::span<const FileSystemObject* const> rowsOnLeft,
                                                    std::span<const FileSystemObject* const> rowsOnRight,
                                                    bool& useRecycleBin)
{
    DeleteDialog dlg(parent, rowsOnLeft, rowsOnRight, useRecycleBin);
    return static_cast<ReturnSmallDlg::ButtonPressed>(dlg.ShowModal());
}

//########################################################################################

namespace
{
class SyncConfirmationDlg : public SyncConfirmationDlgGenerated
{
public:
    SyncConfirmationDlg(wxWindow* parent,
                        bool syncSelection,
                        std::optional<SyncVariant> syncVar,
                        const SyncStatistics& st,
                        bool& dontShowAgain);
private:
    void onStartSync(wxCommandEvent& event) override;
    void onCancel   (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void onClose    (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

    void onLocalKeyEvent(wxKeyEvent& event);

    //output-only parameters:
    bool& dontShowAgainOut_;
};


SyncConfirmationDlg::SyncConfirmationDlg(wxWindow* parent,
                                         bool syncSelection,
                                         std::optional<SyncVariant> syncVar,
                                         const SyncStatistics& st,
                                         bool& dontShowAgain) :
    SyncConfirmationDlgGenerated(parent),
    dontShowAgainOut_(dontShowAgain)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonStartSync).setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextCaption);
    m_bitmapSync->SetBitmap(loadImage(syncSelection ? "file_sync_selection" : "file_sync"));

    m_staticTextCaption->SetLabel(syncSelection ?_("Start to synchronize the selection?") : _("Start synchronization now?"));
    m_staticTextSyncVar->SetLabel(getVariantName(syncVar));

    const char* varImgName = nullptr;
    if (syncVar)
        switch (*syncVar)
        {
            //*INDENT-OFF*
            case SyncVariant::twoWay: varImgName = "sync_twoway"; break;
            case SyncVariant::mirror: varImgName = "sync_mirror"; break;
            case SyncVariant::update: varImgName = "sync_update"; break;
            case SyncVariant::custom: varImgName = "sync_custom"; break;
            //*INDENT-ON*
        }
    if (varImgName)
        m_bitmapSyncVar->SetBitmap(loadImage(varImgName, -1 /*maxWidth*/, getDefaultMenuIconSize()));

    m_checkBoxDontShowAgain->SetValue(dontShowAgain);

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); });

    //update preview of item count and bytes to be transferred:
    auto setValue = [](wxStaticText& txtControl, bool isZeroValue, const wxString& valueAsString, wxStaticBitmap& bmpControl, const char* imageName)
    {
        wxFont fnt = txtControl.GetFont();
        fnt.SetWeight(isZeroValue ? wxFONTWEIGHT_NORMAL : wxFONTWEIGHT_BOLD);
        txtControl.SetFont(fnt);

        setText(txtControl, valueAsString);

        bmpControl.SetBitmap(greyScaleIfDisabled(mirrorIfRtl(loadImage(imageName)), !isZeroValue));
    };

    auto setIntValue = [&setValue](wxStaticText& txtControl, int value, wxStaticBitmap& bmpControl, const char* imageName)
    {
        setValue(txtControl, value == 0, formatNumber(value), bmpControl, imageName);
    };

    setValue(*m_staticTextData, st.getBytesToProcess() == 0, formatFilesizeShort(st.getBytesToProcess()), *m_bitmapData, "data");
    setIntValue(*m_staticTextCreateLeft,  st.createCount< LEFT_SIDE>(), *m_bitmapCreateLeft,  "so_create_left_sicon");
    setIntValue(*m_staticTextUpdateLeft,  st.updateCount< LEFT_SIDE>(), *m_bitmapUpdateLeft,  "so_update_left_sicon");
    setIntValue(*m_staticTextDeleteLeft,  st.deleteCount< LEFT_SIDE>(), *m_bitmapDeleteLeft,  "so_delete_left_sicon");
    setIntValue(*m_staticTextCreateRight, st.createCount<RIGHT_SIDE>(), *m_bitmapCreateRight, "so_create_right_sicon");
    setIntValue(*m_staticTextUpdateRight, st.updateCount<RIGHT_SIDE>(), *m_bitmapUpdateRight, "so_update_right_sicon");
    setIntValue(*m_staticTextDeleteRight, st.deleteCount<RIGHT_SIDE>(), *m_bitmapDeleteRight, "so_delete_right_sicon");

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonStartSync->SetFocus();
}


void SyncConfirmationDlg::onLocalKeyEvent(wxKeyEvent& event)
{
    event.Skip();
}


void SyncConfirmationDlg::onStartSync(wxCommandEvent& event)
{
    dontShowAgainOut_ = m_checkBoxDontShowAgain->GetValue();
    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}
}

ReturnSmallDlg::ButtonPressed fff::showSyncConfirmationDlg(wxWindow* parent,
                                                           bool syncSelection,
                                                           std::optional<SyncVariant> syncVar,
                                                           const SyncStatistics& statistics,
                                                           bool& dontShowAgain)
{
    SyncConfirmationDlg dlg(parent,
                            syncSelection,
                            syncVar,
                            statistics,
                            dontShowAgain);
    return static_cast<ReturnSmallDlg::ButtonPressed>(dlg.ShowModal());
}

//########################################################################################

namespace
{
class OptionsDlg : public OptionsDlgGenerated
{
public:
    OptionsDlg(wxWindow* parent, XmlGlobalSettings& globalCfg);

private:
    void onOkay          (wxCommandEvent& event) override;
    void onRestoreDialogs(wxCommandEvent& event) override;
    void onDefault       (wxCommandEvent& event) override;
    void onCancel        (wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void onClose         (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void onAddRow        (wxCommandEvent& event) override;
    void onRemoveRow     (wxCommandEvent& event) override;
    void onHelpExternalApps (wxHyperlinkEvent& event) override { displayHelpEntry(L"external-applications", this); }
    void onShowLogFolder    (wxHyperlinkEvent& event) override;
    void onToggleLogfilesLimit(wxCommandEvent& event) override { updateGui(); }

    void onSelectSoundCompareDone(wxCommandEvent& event) override { selectSound(*m_textCtrlSoundPathCompareDone); }
    void onSelectSoundSyncDone   (wxCommandEvent& event) override { selectSound(*m_textCtrlSoundPathSyncDone); }
    void selectSound(wxTextCtrl& txtCtrl);

    void onChangeSoundFilePath(wxCommandEvent& event) override { updateGui(); }

    void onPlayCompareDone(wxCommandEvent& event) override { playSoundWithDiagnostics(trimCpy(m_textCtrlSoundPathCompareDone->GetValue())); }
    void onPlaySyncDone   (wxCommandEvent& event) override { playSoundWithDiagnostics(trimCpy(m_textCtrlSoundPathSyncDone   ->GetValue())); }
    void playSoundWithDiagnostics(const wxString& filePath);

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

    m_spinCtrlLogFilesMaxAge->SetMinSize({fastFromDIP(70), -1}); //Hack: set size (why does wxWindow::Size() not work?)
    m_hyperlinkLogFolder->SetLabel(utfTo<wxString>(getDefaultLogFolderPath()));
    setRelativeFontSize(*m_hyperlinkLogFolder, 1.2);

    m_bitmapSettings          ->SetBitmap     (loadImage("settings"));
    m_bitmapWarnings          ->SetBitmap     (loadImage("msg_warning", fastFromDIP(20)));
    m_bitmapLogFile           ->SetBitmap     (loadImage("log_file",    fastFromDIP(20)));
    m_bitmapNotificationSounds->SetBitmap     (loadImage("notification_sounds"));
    m_bitmapConsole           ->SetBitmap     (loadImage("command_line", fastFromDIP(20)));
    m_bitmapCompareDone       ->SetBitmap     (loadImage("compare_sicon"));
    m_bitmapSyncDone          ->SetBitmap     (loadImage("file_sync_sicon"));
    m_bpButtonPlayCompareDone ->SetBitmapLabel(loadImage("play_sound"));
    m_bpButtonPlaySyncDone    ->SetBitmapLabel(loadImage("play_sound"));
    m_bpButtonAddRow          ->SetBitmapLabel(loadImage("item_add"));
    m_bpButtonRemoveRow       ->SetBitmapLabel(loadImage("item_remove"));

    m_staticTextAllDialogsShown->SetLabel(L'(' + _("No dialogs hidden") + L')');

    m_staticTextResetDialogs->Wrap(std::max(fastFromDIP(250),
                                            m_buttonRestoreDialogs     ->GetSize().x +
                                            m_staticTextAllDialogsShown->GetSize().x));

    //--------------------------------------------------------------------------------
    m_checkBoxFailSafe       ->SetValue(globalSettings.failSafeFileCopy);
    m_checkBoxCopyLocked     ->SetValue(globalSettings.copyLockedFiles);
    m_checkBoxCopyPermissions->SetValue(globalSettings.copyFilePermissions);

    m_checkBoxLogFilesMaxAge->SetValue(globalSettings.logfilesMaxAgeDays > 0);
    m_spinCtrlLogFilesMaxAge->SetValue(globalSettings.logfilesMaxAgeDays > 0 ? globalSettings.logfilesMaxAgeDays : XmlGlobalSettings().logfilesMaxAgeDays);

    switch (globalSettings.logFormat)
    {
        case LogFileFormat::html:
            m_radioBtnLogHtml->SetValue(true);
            break;
        case LogFileFormat::text:
            m_radioBtnLogText->SetValue(true);
            break;
    }

    m_textCtrlSoundPathCompareDone->ChangeValue(utfTo<wxString>(globalSettings.soundFileCompareFinished));
    m_textCtrlSoundPathSyncDone   ->ChangeValue(utfTo<wxString>(globalSettings.soundFileSyncFinished));
    //--------------------------------------------------------------------------------

    bSizerLockedFiles->Show(false);
    m_gridCustomCommand->SetMargins(0, 0);

    //temporarily set dummy value for window height calculations:
    setExtApp(std::vector<ExternalApp>(globalSettings.gui.externalApps.size() + 1));
    confirmDlgs_             = defaultCfg_.confirmDlgs;             //
    warnDlgs_                = defaultCfg_.warnDlgs;                //make sure m_staticTextAllDialogsShown is shown
    autoCloseProgressDialog_ = defaultCfg_.autoCloseProgressDialog; //
    updateGui();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    //restore actual value:
    setExtApp(globalSettings.gui.externalApps);
    confirmDlgs_             = globalSettings.confirmDlgs;
    warnDlgs_                = globalSettings.warnDlgs;
    autoCloseProgressDialog_ = globalSettings.autoCloseProgressDialog;
    updateGui();

    //automatically fit column width to match total grid width
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) { onResize(event); });
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

    m_buttonRestoreDialogs->Enable(haveHiddenDialogs);
    m_staticTextAllDialogsShown->Show(!haveHiddenDialogs);
    Layout();

    m_spinCtrlLogFilesMaxAge->Enable(m_checkBoxLogFilesMaxAge->GetValue());

    m_bpButtonPlayCompareDone->Enable(!trimCpy(m_textCtrlSoundPathCompareDone->GetValue()).empty());
    m_bpButtonPlaySyncDone   ->Enable(!trimCpy(m_textCtrlSoundPathSyncDone   ->GetValue()).empty());
}


void OptionsDlg::onRestoreDialogs(wxCommandEvent& event)
{
    confirmDlgs_             = defaultCfg_.confirmDlgs;
    warnDlgs_                = defaultCfg_.warnDlgs;
    autoCloseProgressDialog_ = defaultCfg_.autoCloseProgressDialog;
    updateGui();
}


void OptionsDlg::selectSound(wxTextCtrl& txtCtrl)
{
    wxString defaultFolderPath = beforeLast(txtCtrl.GetValue(), utfTo<wxString>(FILE_NAME_SEPARATOR), IfNotFoundReturn::none);
    if (defaultFolderPath.empty())
        defaultFolderPath = utfTo<wxString>(beforeLast(getResourceDirPf(), FILE_NAME_SEPARATOR, IfNotFoundReturn::all));

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


void OptionsDlg::playSoundWithDiagnostics(const wxString& filePath)
{
    try
    {
        //wxSOUND_ASYNC: NO failure indication (on Windows)!
        //wxSound::Play(..., wxSOUND_SYNC) can return false, but does not provide details!
        //=> check file access manually first:
        [[maybe_unused]] std::string stream = getFileContent(utfTo<Zstring>(filePath), nullptr /*notifyUnbufferedIO*/); //throw FileError

        /*bool success = */ wxSound::Play(filePath, wxSOUND_ASYNC);
    }
    catch (const FileError& e) { showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString())); }
}


void OptionsDlg::onDefault(wxCommandEvent& event)
{
    m_checkBoxFailSafe       ->SetValue(defaultCfg_.failSafeFileCopy);
    m_checkBoxCopyLocked     ->SetValue(defaultCfg_.copyLockedFiles);
    m_checkBoxCopyPermissions->SetValue(defaultCfg_.copyFilePermissions);

    m_checkBoxLogFilesMaxAge->SetValue(defaultCfg_.logfilesMaxAgeDays > 0);
    m_spinCtrlLogFilesMaxAge->SetValue(defaultCfg_.logfilesMaxAgeDays > 0 ? defaultCfg_.logfilesMaxAgeDays : 14);

    switch (defaultCfg_.logFormat)
    {
        case LogFileFormat::html:
            m_radioBtnLogHtml->SetValue(true);
            break;
        case LogFileFormat::text:
            m_radioBtnLogText->SetValue(true);
            break;
    }

    m_textCtrlSoundPathCompareDone->ChangeValue(utfTo<wxString>(defaultCfg_.soundFileCompareFinished));
    m_textCtrlSoundPathSyncDone   ->ChangeValue(utfTo<wxString>(defaultCfg_.soundFileSyncFinished));

    setExtApp(defaultCfg_.gui.externalApps);

    updateGui();
}


void OptionsDlg::onOkay(wxCommandEvent& event)
{
    //write settings only when okay-button is pressed (except hidden dialog reset)!
    globalCfgOut_.failSafeFileCopy    = m_checkBoxFailSafe->GetValue();
    globalCfgOut_.copyLockedFiles     = m_checkBoxCopyLocked->GetValue();
    globalCfgOut_.copyFilePermissions = m_checkBoxCopyPermissions->GetValue();

    globalCfgOut_.logfilesMaxAgeDays = m_checkBoxLogFilesMaxAge->GetValue() ? m_spinCtrlLogFilesMaxAge->GetValue() : -1;
    globalCfgOut_.logFormat = m_radioBtnLogHtml->GetValue() ? LogFileFormat::html : LogFileFormat::text;

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


void OptionsDlg::onAddRow(wxCommandEvent& event)
{
    const int selectedRow = m_gridCustomCommand->GetGridCursorRow();
    if (0 <= selectedRow && selectedRow < m_gridCustomCommand->GetNumberRows())
        m_gridCustomCommand->InsertRows(selectedRow);
    else
        m_gridCustomCommand->AppendRows();

    wxSizeEvent dummy2;
    onResize(dummy2);

    m_gridCustomCommand->SetFocus(); //make grid cursor visible
}


void OptionsDlg::onRemoveRow(wxCommandEvent& event)
{
    if (m_gridCustomCommand->GetNumberRows() > 0)
    {
        const int selectedRow = m_gridCustomCommand->GetGridCursorRow();
        if (0 <= selectedRow && selectedRow < m_gridCustomCommand->GetNumberRows())
            m_gridCustomCommand->DeleteRows(selectedRow);
        else
            m_gridCustomCommand->DeleteRows(m_gridCustomCommand->GetNumberRows() - 1);

        wxSizeEvent dummy2;
        onResize(dummy2);

        m_gridCustomCommand->SetFocus(); //make grid cursor visible
    }
}


void OptionsDlg::onShowLogFolder(wxHyperlinkEvent& event)
{
    try
    {
        openWithDefaultApp(getDefaultLogFolderPath()); //throw FileError
    }
    catch (const FileError& e) { showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString())); }
}
}

ReturnSmallDlg::ButtonPressed fff::showOptionsDlg(wxWindow* parent, XmlGlobalSettings& globalCfg)
{
    OptionsDlg dlg(parent, globalCfg);
    return static_cast<ReturnSmallDlg::ButtonPressed>(dlg.ShowModal());
}

//########################################################################################

namespace
{
class SelectTimespanDlg : public SelectTimespanDlgGenerated
{
public:
    SelectTimespanDlg(wxWindow* parent, time_t& timeFrom, time_t& timeTo);

private:
    void onOkay  (wxCommandEvent& event) override;
    void onCancel(wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void onClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

    void onChangeSelectionFrom(wxCalendarEvent& event) override
    {
        if (m_calendarFrom->GetDate() > m_calendarTo->GetDate())
            m_calendarTo->SetDate(m_calendarFrom->GetDate());
    }
    void onChangeSelectionTo(wxCalendarEvent& event) override
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

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); }); //enable dialog-specific key events

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonOkay->SetFocus();
}


void SelectTimespanDlg::onLocalKeyEvent(wxKeyEvent& event) //process key events without explicit menu entry :)
{
    event.Skip();
}


void SelectTimespanDlg::onOkay(wxCommandEvent& event)
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
}

ReturnSmallDlg::ButtonPressed fff::showSelectTimespanDlg(wxWindow* parent, time_t& timeFrom, time_t& timeTo)
{
    SelectTimespanDlg dlg(parent, timeFrom, timeTo);
    return static_cast<ReturnSmallDlg::ButtonPressed>(dlg.ShowModal());
}

//########################################################################################

namespace
{
class CfgHighlightDlg : public CfgHighlightDlgGenerated
{
public:
    CfgHighlightDlg(wxWindow* parent, int& cfgHistSyncOverdueDays);

private:
    void onOkay  (wxCommandEvent& event) override;
    void onCancel(wxCommandEvent& event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }
    void onClose (wxCloseEvent&   event) override { EndModal(ReturnSmallDlg::BUTTON_CANCEL); }

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

    m_spinCtrlOverdueDays->SetMinSize({fastFromDIP(70), -1}); //Hack: set size (why does wxWindow::Size() not work?)

    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    m_spinCtrlOverdueDays->SetValue(cfgHistSyncOverdueDays);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_spinCtrlOverdueDays->SetFocus();
}


void CfgHighlightDlg::onOkay(wxCommandEvent& event)
{
    cfgHistSyncOverdueDaysOut_ = m_spinCtrlOverdueDays->GetValue();
    EndModal(ReturnSmallDlg::BUTTON_OKAY);
}
}

ReturnSmallDlg::ButtonPressed fff::showCfgHighlightDlg(wxWindow* parent, int& cfgHistSyncOverdueDays)
{
    CfgHighlightDlg dlg(parent, cfgHistSyncOverdueDays);
    return static_cast<ReturnSmallDlg::ButtonPressed>(dlg.ShowModal());
}

//########################################################################################

namespace
{
class ActivationDlg : public ActivationDlgGenerated
{
public:
    ActivationDlg(wxWindow* parent, const std::wstring& lastErrorMsg, const std::wstring& manualActivationUrl, std::wstring& manualActivationKey);

private:
    void onActivateOnline (wxCommandEvent& event) override;
    void onActivateOffline(wxCommandEvent& event) override;
    void onOfflineActivationEnter(wxCommandEvent& event) override { onActivateOffline(event); }
    void onCopyUrl        (wxCommandEvent& event) override;
    void onCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ReturnActivationDlg::CANCEL)); }
    void onClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ReturnActivationDlg::CANCEL)); }

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

    SetTitle(L"FreeFileSync " + utfTo<std::wstring>(ffsVersion) + L" [" + _("Donation Edition") + L']');

    //setMainInstructionFont(*m_staticTextMain);

    m_bitmapActivation->SetBitmap(loadImage("internet"));
    m_textCtrlOfflineActivationKey->ForceUpper();

    m_textCtrlLastError           ->ChangeValue(lastErrorMsg);
    m_textCtrlManualActivationUrl ->ChangeValue(manualActivationUrl);
    m_textCtrlOfflineActivationKey->ChangeValue(manualActivationKey);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonActivateOnline->SetFocus();
}


void ActivationDlg::onCopyUrl(wxCommandEvent& event)
{
    if (wxClipboard::Get()->Open())
    {
        ZEN_ON_SCOPE_EXIT(wxClipboard::Get()->Close());
        wxClipboard::Get()->SetData(new wxTextDataObject(m_textCtrlManualActivationUrl->GetValue())); //ownership passed

        m_textCtrlManualActivationUrl->SetFocus(); //[!] otherwise selection is lost
        m_textCtrlManualActivationUrl->SelectAll(); //some visual feedback
    }
}


void ActivationDlg::onActivateOnline(wxCommandEvent& event)
{
    manualActivationKeyOut_ = m_textCtrlOfflineActivationKey->GetValue();
    EndModal(static_cast<int>(ReturnActivationDlg::ACTIVATE_ONLINE));
}


void ActivationDlg::onActivateOffline(wxCommandEvent& event)
{
    manualActivationKeyOut_ = m_textCtrlOfflineActivationKey->GetValue();
    EndModal(static_cast<int>(ReturnActivationDlg::ACTIVATE_OFFLINE));
}
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

    void requestUiUpdate() //throw CancelPressed
    {
        if (cancelled_)
            throw CancelPressed();

        if (uiUpdateDue())
        {
            updateGui();
            //wxTheApp->Yield();
            ::wxSafeYield(this); //disables user input except for "this" (using wxWindowDisabler instead would move the FFS main dialog into the background: why?)
        }
    }

private:
    void onCancel(wxCommandEvent& event) override { cancelled_ = true; }

    void updateGui()
    {
        const double fraction = bytesTotal_ == 0 ? 0 : 1.0 * bytesCurrent_ / bytesTotal_;
        m_staticTextHeader->SetLabel(_("Downloading update...") + L' ' +
                                     numberTo<std::wstring>(numeric::round(fraction * 100)) + L"% (" + formatFilesizeShort(bytesCurrent_) + L')');
        m_gaugeProgress->SetValue(numeric::round(fraction * GAUGE_FULL_RANGE));

        m_staticTextDetails->SetLabel(utfTo<std::wstring>(filePath_));
    }

    bool cancelled_ = false;
    int64_t bytesCurrent_ = 0;
    const int64_t bytesTotal_;
    Zstring filePath_;
    const int GAUGE_FULL_RANGE = 1000'000;
};


DownloadProgressWindow::Impl::Impl(wxWindow* parent, int64_t fileSizeTotal) :
    DownloadProgressDlgGenerated(parent),
    bytesTotal_(fileSizeTotal)
{

    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextHeader);
    m_staticTextHeader->Wrap(fastFromDIP(460)); //*after* font change!

    m_staticTextDetails->SetMinSize({fastFromDIP(550), -1});

    m_bitmapDownloading->SetBitmap(loadImage("internet"));

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
void DownloadProgressWindow::requestUiUpdate()                     { pimpl_->requestUiUpdate(); } //throw CancelPressed

//########################################################################################


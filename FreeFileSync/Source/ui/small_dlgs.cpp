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
#include <zen/file_io.h>
//#include <zen/http.h>
#include <wx/wupdlock.h>
#include <wx/filedlg.h>
#include <wx/sound.h>
//#include <wx+/choice_enum.h>
#include <wx+/context_menu.h>
#include <wx+/bitmap_button.h>
#include <wx+/rtl.h>
#include <wx+/no_flicker.h>
#include <wx+/image_tools.h>
#include <wx+/window_layout.h>
#include <wx+/popup_dlg.h>
#include <wx+/async_task.h>
#include <wx+/image_resources.h>
#include "gui_generated.h"
#include "folder_selector.h"
//#include "version_check.h"
#include "abstract_folder_picker.h"
#include "../afs/concrete.h"
#include "../afs/gdrive.h"
#include "../afs/ftp.h"
#include "../afs/sftp.h"
//#include "../base/algorithm.h"
#include "../base/synchronization.h"
//#include "../base/path_filter.h"
#include "../base/icon_loader.h"
#include "../status_handler.h" //uiUpdateDue()
#include "../version/version.h"
#include "../ffs_paths.h"
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
    void onOkay (wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::accept)); }
    void onClose(wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onOpenForum(wxCommandEvent& event) override { wxLaunchDefaultBrowser(L"https://freefilesync.org/forum"); }
    void onDonate   (wxCommandEvent& event) override { wxLaunchDefaultBrowser(L"https://freefilesync.org/donate"); }
    void onSendEmail(wxCommandEvent& event) override
    {
        wxLaunchDefaultBrowser(wxString() + L"mailto:zenju@" +
                               /*don't leave full email in either source or binary*/ L"freefilesync.org");
    }

    void onLocalKeyEvent(wxKeyEvent& event);
};


AboutDlg::AboutDlg(wxWindow* parent) : AboutDlgGenerated(parent)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonClose));

    assert(m_buttonClose->GetId() == wxID_OK); //we cannot use wxID_CLOSE else Esc key won't work: yet another wxWidgets bug??

    setImage(*m_bitmapLogo,     loadImage("logo"));
    setImage(*m_bitmapLogoLeft, loadImage("logo-left"));

    setBitmapTextLabel(*m_bpButtonForum, loadImage("ffs_forum"), L"FreeFileSync Forum");
    setBitmapTextLabel(*m_bpButtonEmail, loadImage("ffs_email"), wxString() + L"zenju@" + /*don't leave full email in either source or binary*/ L"freefilesync.org");
    m_bpButtonEmail->SetToolTip(                          wxString() + L"mailto:zenju@" + /*don't leave full email in either source or binary*/ L"freefilesync.org");

    wxString build = utfTo<wxString>(ffsVersion);

    const wchar_t* const SPACED_BULLET = L" \u2022 ";
    build += SPACED_BULLET;

    build += LTR_MARK; //fix Arabic
    build += utfTo<wxString>(cpuArchName);

    build += SPACED_BULLET;
    build += utfTo<wxString>(formatTime(formatDateTag, getCompileTime()));

    m_staticFfsTextVersion->SetLabelText(replaceCpy(_("Version: %x"), L"%x", build));

    wxString variantName;
    m_staticTextFfsVariant->SetLabelText(variantName);

#ifndef wxUSE_UNICODE
#error what is going on?
#endif

    {
        m_bitmapAnimalBig->Hide();

        setRelativeFontSize(*m_staticTextDonate, 1.20);
        m_staticTextDonate->Hide(); //temporarily! => avoid impact to dialog width

        setRelativeFontSize(*m_buttonDonate1, 1.25);
        setBitmapTextLabel(*m_buttonDonate1, loadImage("ffs_heart", dipToScreen(28)), m_buttonDonate1->GetLabelText());

        m_buttonShowSupporterDetails->Hide();
        m_buttonDonate2->Hide();
    }

    //--------------------------------------------------------------------------
    m_staticTextThanksForLoc->SetMinSize({dipToWxsize(200), -1});
    m_staticTextThanksForLoc->Wrap(dipToWxsize(200));

    const int scrollDelta = GetCharHeight();
    m_scrolledWindowTranslators->SetScrollRate(scrollDelta, scrollDelta);

    for (const TranslationInfo& ti : getAvailableTranslations())
    {
        //country flag
        wxStaticBitmap* staticBitmapFlag = new wxStaticBitmap(m_scrolledWindowTranslators, wxID_ANY, toScaledBitmap(loadImage(ti.languageFlag)));
        fgSizerTranslators->Add(staticBitmapFlag, 0, wxALIGN_CENTER);

        //translator name
        wxStaticText* staticTextTranslator = new wxStaticText(m_scrolledWindowTranslators, wxID_ANY, ti.translatorName, wxDefaultPosition, wxDefaultSize, 0);
        fgSizerTranslators->Add(staticTextTranslator, 0, wxALIGN_CENTER_VERTICAL);

        staticBitmapFlag    ->SetToolTip(ti.languageName);
        staticTextTranslator->SetToolTip(ti.languageName);
    }
    fgSizerTranslators->Fit(m_scrolledWindowTranslators);
    //--------------------------------------------------------------------------

    wxImage::AddHandler(new wxJPEGHandler /*ownership passed*/); //activate support for .jpg files

    wxImage animalImg(utfTo<wxString>(appendPath(getResourceDirPath(), Zstr("Animal.dat"))), wxBITMAP_TYPE_JPEG);
    convertToVanillaImage(animalImg);
    assert(animalImg.IsOk());

    //--------------------------------------------------------------------------
    //have animal + text match *final* dialog width
    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif

    {
        const int imageWidth = (m_panelDonate->GetSize().GetWidth() - 5 - 5 - 5 /* grey border*/) / 2;
        const int textWidth  =  m_panelDonate->GetSize().GetWidth() - 5 - 5 - 5 - imageWidth;

        setImage(*m_bitmapAnimalSmall, shrinkImage(animalImg, wxsizeToScreen(imageWidth), -1 /*maxHeight*/));

        m_staticTextDonate->Show();
        m_staticTextDonate->Wrap(textWidth - 10 /*left gap*/); //wrap *after* changing font size
    }
    //--------------------------------------------------------------------------

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); }); //enable dialog-specific key events

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
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
    CloudSetupDlg(wxWindow* parent, Zstring& folderPathPhrase, Zstring& sftpKeyFileLastSelected, size_t& parallelOps, bool canChangeParallelOp);

private:
    void onOkay  (wxCommandEvent& event) override;
    void onCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }

    void onGdriveUserAdd   (wxCommandEvent& event) override;
    void onGdriveUserRemove(wxCommandEvent& event) override;
    void onGdriveUserSelect(wxCommandEvent& event) override;
    void gdriveUpdateDrivesAndSelect(const std::string& accountEmail, const Zstring& locationToSelect);

    void onDetectServerChannelLimit(wxCommandEvent& event) override;
    void onTypingPassword(wxCommandEvent& event) override;
    void onToggleShowPassword(wxCommandEvent& event) override;
    void onTogglePasswordPrompt(wxCommandEvent& event) override { updateGui(); }
    void onBrowseCloudFolder (wxCommandEvent& event) override;

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

    bool validateParameters();
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

    Zstring& sftpKeyFileLastSelected_;

    //output-only parameters:
    Zstring& folderPathPhraseOut_;
    size_t& parallelOpsOut_;
};


CloudSetupDlg::CloudSetupDlg(wxWindow* parent, Zstring& folderPathPhrase, Zstring& sftpKeyFileLastSelected, size_t& parallelOps, bool canChangeParallelOp) :
    CloudSetupDlgGenerated(parent),
    sftpKeyFileLastSelected_(sftpKeyFileLastSelected),
    folderPathPhraseOut_(folderPathPhrase),
    parallelOpsOut_(parallelOps)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    setImage(*m_toggleBtnGdrive, loadImage("google_drive"));

    setRelativeFontSize(*m_toggleBtnGdrive, 1.25);
    setRelativeFontSize(*m_toggleBtnSftp,   1.25);
    setRelativeFontSize(*m_toggleBtnFtp,    1.25);

    setBitmapTextLabel(*m_buttonGdriveAddUser,    loadImage("user_add",    dipToScreen(20)), m_buttonGdriveAddUser   ->GetLabelText());
    setBitmapTextLabel(*m_buttonGdriveRemoveUser, loadImage("user_remove", dipToScreen(20)), m_buttonGdriveRemoveUser->GetLabelText());

    setImage(*m_bitmapGdriveUser,  loadImage("user",   dipToScreen(20)));
    setImage(*m_bitmapGdriveDrive, loadImage("drive",  dipToScreen(20)));
    setImage(*m_bitmapServer,      loadImage("server", dipToScreen(20)));
    setImage(*m_bitmapCloud,       loadImage("cloud"));
    setImage(*m_bitmapPerf,        loadImage("speed"));
    setImage(*m_bitmapServerDir, IconBuffer::genericDirIcon(IconBuffer::IconSize::small));
    m_checkBoxShowPassword  ->SetValue(false);
    m_checkBoxPasswordPrompt->SetValue(false);

    m_textCtrlServer->SetHint(_("Example:") + L"    website.com    66.198.240.22");
    m_textCtrlServer->SetMinSize({dipToWxsize(260), -1});

    m_textCtrlPort->SetMinSize({dipToWxsize(60), -1});
    setDefaultWidth(*m_spinCtrlConnectionCount);
    setDefaultWidth(*m_spinCtrlChannelCountSftp);
    setDefaultWidth(*m_spinCtrlTimeout);

    setupFileDrop(*m_panelAuth);
    m_panelAuth->Bind(EVENT_DROP_FILE, [this](FileDropEvent& event) { onKeyFileDropped(event); });

    m_staticTextConnectionsLabelSub->SetLabelText(L'(' + _("Connections") + L')');

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
            gdriveUpdateDrivesAndSelect(login.email, login.locationName);
        }
        else
        {
            m_listBoxGdriveUsers->DeselectAll();
            m_listBoxGdriveDrives->Clear();
        }

        m_textCtrlServerPath->ChangeValue(utfTo<wxString>(FILE_NAME_SEPARATOR + folderPath.afsPath.value));
        m_spinCtrlTimeout->SetValue(login.timeoutSec);
    }
    else if (acceptsItemPathPhraseSftp(folderPathPhrase))
    {
        type_ = CloudType::sftp;
        const AbstractPath folderPath = createItemPathSftp(folderPathPhrase);
        const SftpLogin login = extractSftpLogin(folderPath.afsDevice); //noexcept

        if (login.portCfg > 0)
            m_textCtrlPort->ChangeValue(numberTo<wxString>(login.portCfg));
        m_textCtrlServer        ->ChangeValue(utfTo<wxString>(login.server));
        m_textCtrlUserName      ->ChangeValue(utfTo<wxString>(login.username));
        sftpAuthType_ = login.authType;
        if (login.password)
            m_textCtrlPasswordHidden->ChangeValue(utfTo<wxString>(*login.password));
        else
            m_checkBoxPasswordPrompt->SetValue(true);
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

        if (login.portCfg > 0)
            m_textCtrlPort->ChangeValue(numberTo<wxString>(login.portCfg));
        m_textCtrlServer         ->ChangeValue(utfTo<wxString>(login.server));
        m_textCtrlUserName       ->ChangeValue(utfTo<wxString>(login.username));
        if (login.password)
            m_textCtrlPasswordHidden ->ChangeValue(utfTo<wxString>(*login.password));
        else
            m_checkBoxPasswordPrompt->SetValue(true);
        m_textCtrlServerPath     ->ChangeValue(utfTo<wxString>(FILE_NAME_SEPARATOR + folderPath.afsPath.value));
        (login.useTls ? m_radioBtnEncryptSsl : m_radioBtnEncryptNone)->SetValue(true);
        m_spinCtrlTimeout        ->SetValue(login.timeoutSec);
    }

    m_spinCtrlConnectionCount->SetValue(parallelOps);

    m_spinCtrlConnectionCount->Disable();
    m_staticTextConnectionCountDescr->Hide();

    m_spinCtrlChannelCountSftp->Disable();
    m_buttonChannelCountSftp  ->Disable();
    //---------------------------------------------------------

    //set up default view for dialog size calculation
    bSizerGdrive->Show(false);
    bSizerFtpEncrypt->Show(false);
    m_textCtrlPasswordVisible->Hide();
    m_checkBoxPasswordPrompt->Hide();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK with window resizing problems and title bar corruption; e.g. Debian!!!
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
    Center(); //needs to be re-applied after a dialog size change!

    updateGui(); //*after* SetSizeHints when standard dialog height has been calculated

    m_buttonOkay->SetFocus();
}


void CloudSetupDlg::onGdriveUserAdd(wxCommandEvent& event)
{
    guiQueue_.processAsync([timeoutSec = extractGdriveLogin(getFolderPath().afsDevice).timeoutSec]() -> std::variant<std::string /*email*/, FileError>
    {
        try
        {
            return gdriveAddUser(nullptr /*updateGui*/, timeoutSec); //throw FileError
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

            gdriveRemoveUser(loginEmail, extractGdriveLogin(getFolderPath().afsDevice).timeoutSec); //throw FileError
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


void CloudSetupDlg::gdriveUpdateDrivesAndSelect(const std::string& accountEmail, const Zstring& locationToSelect)
{
    m_listBoxGdriveDrives->Clear();
    m_listBoxGdriveDrives->Append(txtLoading_);

    guiQueue_.processAsync([accountEmail, timeoutSec = extractGdriveLogin(getFolderPath().afsDevice).timeoutSec]() ->
                           std::variant<std::vector<Zstring /*locationName*/>, FileError>
    {
        try
        {
            return gdriveListLocations(accountEmail, timeoutSec); //throw FileError
        }
        catch (const FileError& e) { return e; }
    },
    [this, accountEmail, locationToSelect](std::variant<std::vector<Zstring /*locationName*/>, FileError>&& result)
    {
        if (const int selPos = m_listBoxGdriveUsers->GetSelection();
            selPos == wxNOT_FOUND || utfTo<std::string>(m_listBoxGdriveUsers->GetString(selPos)) != accountEmail)
            return; //different accountEmail selected in the meantime!

        m_listBoxGdriveDrives->Clear();

        if (const FileError* e = std::get_if<FileError>(&result))
            showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e->toString()));
        else
        {
            auto& locationNames = std::get<std::vector<Zstring>>(result);
            std::sort(locationNames.begin(), locationNames.end(), LessNaturalSort());

            m_listBoxGdriveDrives->Append(txtMyDrive_); //sort locations, but keep "My Drive" at top

            for (const Zstring& itemLabel : locationNames)
                m_listBoxGdriveDrives->Append(utfTo<wxString>(itemLabel));

            const wxString labelToSelect = locationToSelect.empty() ? txtMyDrive_ : utfTo<wxString>(locationToSelect);

            if (const int selPos = m_listBoxGdriveDrives->FindString(labelToSelect, true /*caseSensitive*/);
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
        m_spinCtrlChannelCountSftp->SetSelection(0, 0); //some visual feedback: clear selection
        m_spinCtrlChannelCountSftp->Refresh(); //both needed for wxGTK: meh!
        m_spinCtrlChannelCountSftp->Update();  //

        AbstractPath folderPath = getFolderPath(); //noexcept
        //-------------------------------------------------------------------
        auto requestPassword = [&, password = Zstring()](const std::wstring& msg, const std::wstring& lastErrorMsg) mutable
        {
            assert(runningOnMainThread());
            if (showPasswordPrompt(this, msg, lastErrorMsg, password) != ConfirmationButton::accept)
                throw CancelProcess();
            return password;
        };
        AFS::authenticateAccess(folderPath.afsDevice, requestPassword); //throw FileError, CancelProcess
        //-------------------------------------------------------------------

        const int channelCountMax = getServerMaxChannelsPerConnection(extractSftpLogin(folderPath.afsDevice)); //throw FileError
        m_spinCtrlChannelCountSftp->SetValue(channelCountMax);

        m_spinCtrlChannelCountSftp->SetFocus(); //[!] otherwise selection is lost
        m_spinCtrlChannelCountSftp->SetSelection(-1, -1); //some visual feedback: select all
    }
    catch (CancelProcess&) { return; }
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

    wxTextCtrl& textCtrl = *(m_checkBoxShowPassword->GetValue() ? m_textCtrlPasswordVisible : m_textCtrlPasswordHidden);
    textCtrl.SetFocus(); //macOS: selects text as unwanted side effect => *before* SetInsertionPointEnd()
    textCtrl.SetInsertionPointEnd();
}


void CloudSetupDlg::onTypingPassword(wxCommandEvent& event)
{
    assert(m_staticTextPassword->IsShown());
    const wxString password = (m_checkBoxShowPassword->GetValue() ? m_textCtrlPasswordVisible : m_textCtrlPasswordHidden)->GetValue();
    if (m_checkBoxShowPassword  ->IsShown() != !password.empty() || //let's avoid some minor flicker
        m_checkBoxPasswordPrompt->IsShown() !=  password.empty())   //in updateGui() Dimensions()
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

    std::optional<Zstring> defaultFolderPath = getParentFolderPath(sftpKeyFileLastSelected_);

    wxFileDialog fileSelector(this, wxString() /*message*/, utfTo<wxString>(defaultFolderPath ? *defaultFolderPath : Zstr("")), wxString() /*default file name*/,
                              _("All files") + L" (*.*)|*" +
                              L"|" + L"OpenSSL PEM (*.pem)|*.pem" +
                              L"|" + L"PuTTY Private Key (*.ppk)|*.ppk",
                              wxFD_OPEN);
    if (fileSelector.ShowModal() != wxID_OK)
        return;
    m_textCtrlKeyfilePath->ChangeValue(fileSelector.GetPath());
    sftpKeyFileLastSelected_ = utfTo<Zstring>(fileSelector.GetPath());
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

        m_textCtrlPasswordVisible->Enable(!m_checkBoxPasswordPrompt->GetValue());
        m_textCtrlPasswordHidden ->Enable(!m_checkBoxPasswordPrompt->GetValue());

        const wxString password = (m_checkBoxShowPassword->GetValue() ? m_textCtrlPasswordVisible : m_textCtrlPasswordHidden)->GetValue();
        m_checkBoxShowPassword  ->Show(!password.empty());
        m_checkBoxPasswordPrompt->Show( password.empty());
    }

    switch (type_)
    {
        case CloudType::gdrive:
            m_buttonGdriveRemoveUser->Enable(m_listBoxGdriveUsers->GetSelection() != wxNOT_FOUND);
            break;

        case CloudType::sftp:
            m_radioBtnPassword->SetValue(false);
            m_radioBtnKeyfile ->SetValue(false);
            m_radioBtnAgent   ->SetValue(false);

            m_textCtrlPort->SetHint(numberTo<wxString>(DEFAULT_PORT_SFTP));

            switch (sftpAuthType_) //*not* owned by GUI controls
            {
                case SftpAuthType::password:
                    m_radioBtnPassword->SetValue(true);
                    m_staticTextPassword->SetLabelText(_("Password:"));
                    break;
                case SftpAuthType::keyFile:
                    m_radioBtnKeyfile->SetValue(true);
                    m_staticTextPassword->SetLabelText(_("Key passphrase:"));
                    break;
                case SftpAuthType::agent:
                    m_radioBtnAgent->SetValue(true);
                    break;
            }
            break;

        case CloudType::ftp:
            m_textCtrlPort->SetHint(numberTo<wxString>(DEFAULT_PORT_FTP));
            m_staticTextPassword->SetLabelText(_("Password:"));
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


bool CloudSetupDlg::validateParameters()
{
    if (type_ == CloudType::sftp ||
        type_ == CloudType::ftp)
    {
        if (trimCpy(m_textCtrlServer->GetValue()).empty())
        {
            showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Server name must not be empty.")));
            m_textCtrlServer->SetFocus();
            return false;
        }
    }

    switch (type_)
    {
        case CloudType::gdrive:
            if (m_listBoxGdriveUsers->GetSelection() == wxNOT_FOUND)
            {
                showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Please select a user account first.")));
                return false;
            }
            break;

        case CloudType::sftp:
            //username *required* for SFTP, but optional for FTP: libcurl will use "anonymous"
            if (trimCpy(m_textCtrlUserName->GetValue()).empty())
            {
                showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Please enter a username.")));
                m_textCtrlUserName->SetFocus();
                return false;
            }

            if (sftpAuthType_ == SftpAuthType::keyFile)
                if (trimCpy(m_textCtrlKeyfilePath->GetValue()).empty())
                {
                    showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Please enter a file path.")));
                    //don't show error icon to follow "Windows' encouraging tone"
                    m_textCtrlKeyfilePath->SetFocus();
                    return false;
                }
            break;

        case CloudType::ftp:
            break;
    }
    return true;
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
                    if (const wxString& locationName = m_listBoxGdriveDrives->GetString(selPos2);
                        locationName != txtMyDrive_ &&
                        locationName != txtLoading_)
                        login.locationName = utfTo<Zstring>(locationName);
                }
            }
            login.timeoutSec = m_spinCtrlTimeout->GetValue();
            return AbstractPath(condenseToGdriveDevice(login), serverRelPath); //noexcept
        }

        case CloudType::sftp:
        {
            SftpLogin login;
            login.server   = utfTo<Zstring>(m_textCtrlServer  ->GetValue());
            login.portCfg  = stringTo<int> (m_textCtrlPort    ->GetValue()); //0 if empty
            login.username = utfTo<Zstring>(m_textCtrlUserName->GetValue());
            login.authType = sftpAuthType_;
            login.privateKeyFilePath = utfTo<Zstring>(m_textCtrlKeyfilePath->GetValue());
            if (m_checkBoxPasswordPrompt->GetValue())
                login.password = std::nullopt;
            else
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
            login.portCfg  = stringTo<int> (m_textCtrlPort    ->GetValue()); //0 if empty
            login.username = utfTo<Zstring>(m_textCtrlUserName->GetValue());
            if (m_checkBoxPasswordPrompt->GetValue())
                login.password = std::nullopt;
            else
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
    if (!validateParameters())
        return;

    AbstractPath folderPath = getFolderPath(); //noexcept
    try
    {
        //-------------------------------------------------------------------
        auto requestPassword = [&, password = Zstring()](const std::wstring& msg, const std::wstring& lastErrorMsg) mutable
        {
            assert(runningOnMainThread());
            if (showPasswordPrompt(this, msg, lastErrorMsg, password) != ConfirmationButton::accept)
                throw CancelProcess();
            return password;
        };
        AFS::authenticateAccess(folderPath.afsDevice, requestPassword); //throw FileError, CancelProcess
        //caveat: this could block *indefinitely* for Google Drive, but luckily already authenticated in this context
        //-------------------------------------------------------------------
        //
        //for (S)FTP it makes more sense to start with the home directory rather than root (which often denies access!)
        if (!AFS::getParentPath(folderPath))
        {
            if (type_ == CloudType::sftp)
                folderPath.afsPath = getSftpHomePath(extractSftpLogin(folderPath.afsDevice)); //throw FileError

            if (type_ == CloudType::ftp)
                folderPath.afsPath = getFtpHomePath(extractFtpLogin(folderPath.afsDevice)); //throw FileError
        }
    }
    catch (CancelProcess&) { return; }
    catch (const FileError& e)
    {
        showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString()));
        return;
    }

    if (showAbstractFolderPicker(this, folderPath) == ConfirmationButton::accept)
        m_textCtrlServerPath->ChangeValue(utfTo<wxString>(FILE_NAME_SEPARATOR + folderPath.afsPath.value));
}


void CloudSetupDlg::onOkay(wxCommandEvent& event)
{
    //------- parameter validation (BEFORE writing output!) -------
    if (!validateParameters())
        return;
    //-------------------------------------------------------------

    folderPathPhraseOut_ = AFS::getInitPathPhrase(getFolderPath());
    parallelOpsOut_ = m_spinCtrlConnectionCount->GetValue();

    EndModal(static_cast<int>(ConfirmationButton::accept));
}
}

ConfirmationButton fff::showCloudSetupDialog(wxWindow* parent, Zstring& folderPathPhrase, Zstring& sftpKeyFileLastSelected, size_t& parallelOps, bool canChangeParallelOp)
{
    CloudSetupDlg dlg(parent, folderPathPhrase, sftpKeyFileLastSelected, parallelOps, canChangeParallelOp);
    return static_cast<ConfirmationButton>(dlg.ShowModal());
}

//########################################################################################

namespace
{
class CopyToDialog : public CopyToDlgGenerated
{
public:
    CopyToDialog(wxWindow* parent,
                 const std::wstring& itemList, int itemCount,
                 Zstring& targetFolderPath, Zstring& targetFolderLastSelected,
                 std::vector<Zstring>& folderHistory, size_t folderHistoryMax,
                 Zstring& sftpKeyFileLastSelected,
                 bool& keepRelPaths,
                 bool& overwriteIfExists);

private:
    void onOkay  (wxCommandEvent& event) override;
    void onCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }

    void onLocalKeyEvent(wxKeyEvent& event);

    std::unique_ptr<FolderSelector> targetFolder; //always bound

    //output-only parameters:
    Zstring& targetFolderPathOut_;
    bool& keepRelPathsOut_;
    bool& overwriteIfExistsOut_;
    std::vector<Zstring>& folderHistoryOut_;
};


CopyToDialog::CopyToDialog(wxWindow* parent,
                           const std::wstring& itemList, int itemCount,
                           Zstring& targetFolderPath, Zstring& targetFolderLastSelected,
                           std::vector<Zstring>& folderHistory, size_t folderHistoryMax,
                           Zstring& sftpKeyFileLastSelected,
                           bool& keepRelPaths,
                           bool& overwriteIfExists) :
    CopyToDlgGenerated(parent),
    targetFolderPathOut_(targetFolderPath),
    keepRelPathsOut_(keepRelPaths),
    overwriteIfExistsOut_(overwriteIfExists),
    folderHistoryOut_(folderHistory)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOK).setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextHeader);

    setImage(*m_bitmapCopyTo, loadImage("copy_to"));

    targetFolder = std::make_unique<FolderSelector>(this, *this, *m_buttonSelectTargetFolder, *m_bpButtonSelectAltTargetFolder, *m_targetFolderPath,
                                                    targetFolderLastSelected, sftpKeyFileLastSelected, nullptr /*staticText*/, nullptr /*wxWindow*/, nullptr /*droppedPathsFilter*/,
    [](const Zstring& folderPathPhrase) { return 1; } /*getDeviceParallelOps*/, nullptr /*setDeviceParallelOps*/);

    m_targetFolderPath->setHistory(std::make_shared<HistoryList>(folderHistory, folderHistoryMax));

    m_textCtrlFileList->SetMinSize({dipToWxsize(500), dipToWxsize(200)});

    /*  There is a nasty bug on wxGTK under Ubuntu: If a multi-line wxTextCtrl contains so many lines that scrollbars are shown,
        it re-enables all windows that are supposed to be disabled during the current modal loop!
        This only affects Ubuntu/wxGTK! No such issue on Debian/wxGTK or Suse/wxGTK
        => another Unity problem like the following?
        https://github.com/wxWidgets/wxWidgets/issues/14823 "Menu not disabled when showing modal dialogs in wxGTK under Unity"        */

    m_staticTextHeader->SetLabelText(_P("Copy the following item to another folder?",
                                        "Copy the following %x items to another folder?", itemCount));
    m_staticTextHeader->Wrap(dipToWxsize(460)); //needs to be reapplied after SetLabel()

    m_textCtrlFileList->ChangeValue(itemList);

    //----------------- set config ---------------------------------
    targetFolder               ->setPath(targetFolderPath);
    m_checkBoxKeepRelPath      ->SetValue(keepRelPaths);
    m_checkBoxOverwriteIfExists->SetValue(overwriteIfExists);
    //----------------- /set config --------------------------------

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); }); //enable dialog-specific key events

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
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

    targetFolderPathOut_  = targetFolder->getPath();
    keepRelPathsOut_      = m_checkBoxKeepRelPath->GetValue();
    overwriteIfExistsOut_ = m_checkBoxOverwriteIfExists->GetValue();
    folderHistoryOut_     = m_targetFolderPath->getHistory()->getList();

    EndModal(static_cast<int>(ConfirmationButton::accept));
}
}

ConfirmationButton fff::showCopyToDialog(wxWindow* parent,
                                         const std::wstring& itemList, int itemCount,
                                         Zstring& targetFolderPath, Zstring& targetFolderLastSelected,
                                         std::vector<Zstring>& folderHistory, size_t folderHistoryMax,
                                         Zstring& sftpKeyFileLastSelected,
                                         bool& keepRelPaths,
                                         bool& overwriteIfExists)
{
    CopyToDialog dlg(parent, itemList, itemCount, targetFolderPath, targetFolderLastSelected, folderHistory, folderHistoryMax, sftpKeyFileLastSelected, keepRelPaths, overwriteIfExists);
    return static_cast<ConfirmationButton>(dlg.ShowModal());
}

//########################################################################################

namespace
{
class DeleteDialog : public DeleteDlgGenerated
{
public:
    DeleteDialog(wxWindow* parent,
                 const std::wstring& itemList, int itemCount,
                 bool& useRecycleBin);

private:
    void onUseRecycler(wxCommandEvent& event) override { updateGui(); }
    void onOkay       (wxCommandEvent& event) override;
    void onCancel     (wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onClose      (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }

    void onLocalKeyEvent(wxKeyEvent& event);

    void updateGui();

    const int itemCount_ = 0;
    const std::chrono::steady_clock::time_point dlgStartTime_ = std::chrono::steady_clock::now();

    const wxImage imgTrash_ = []
    {
        wxImage imgDefault = loadImage("delete_recycler");

        //use system icon if available (can fail on Linux??)
        try { return extractWxImage(fff::getTrashIcon(imgDefault.GetHeight())); /*throw SysError*/ }
        catch (SysError&) { assert(false); return imgDefault; }
    }();

    //output-only parameters:
    bool& useRecycleBinOut_;
};


DeleteDialog::DeleteDialog(wxWindow* parent,
                           const std::wstring& itemList, int itemCount,
                           bool& useRecycleBin) :
    DeleteDlgGenerated(parent),
    itemCount_(itemCount),
    useRecycleBinOut_(useRecycleBin)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOK).setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextHeader);

    m_textCtrlFileList->SetMinSize({dipToWxsize(500), dipToWxsize(200)});

    wxString itemList2(itemList);
    trim(itemList2); //remove trailing newline
    m_textCtrlFileList->ChangeValue(itemList2);
    /*  There is a nasty bug on wxGTK under Ubuntu: If a multi-line wxTextCtrl contains so many lines that scrollbars are shown,
        it re-enables all windows that are supposed to be disabled during the current modal loop!
        This only affects Ubuntu/wxGTK! No such issue on Debian/wxGTK or Suse/wxGTK
        => another Unity problem like the following?
        https://github.com/wxWidgets/wxWidgets/issues/14823 "Menu not disabled when showing modal dialogs in wxGTK under Unity"             */

    m_checkBoxUseRecycler->SetValue(useRecycleBin);

    updateGui();

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); }); //enable dialog-specific key events

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonOK->SetFocus();
}


void DeleteDialog::updateGui()
{
    if (m_checkBoxUseRecycler->GetValue())
    {
        setImage(*m_bitmapDeleteType, imgTrash_);
        m_staticTextHeader->SetLabelText(_P("Do you really want to move the following item to the recycle bin?",
                                            "Do you really want to move the following %x items to the recycle bin?", itemCount_));
        m_buttonOK->SetLabelText(_("Move")); //no access key needed: use ENTER!
    }
    else
    {
        setImage(*m_bitmapDeleteType, loadImage("delete_permanently"));
        m_staticTextHeader->SetLabelText(_P("Do you really want to delete the following item?",
                                            "Do you really want to delete the following %x items?", itemCount_));
        m_buttonOK->SetLabelText(wxControl::RemoveMnemonics(_("&Delete"))); //no access key needed: use ENTER!
    }
    m_staticTextHeader->Wrap(dipToWxsize(460)); //needs to be reapplied after SetLabel()

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

    EndModal(static_cast<int>(ConfirmationButton::accept));
}
}

ConfirmationButton fff::showDeleteDialog(wxWindow* parent,
                                         const std::wstring& itemList, int itemCount,
                                         bool& useRecycleBin)
{
    DeleteDialog dlg(parent, itemList, itemCount, useRecycleBin);
    return static_cast<ConfirmationButton>(dlg.ShowModal());
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
    void onCancel   (wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onClose    (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }

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
    setImage(*m_bitmapSync, loadImage(syncSelection ? "start_sync_selection" : "start_sync"));

    m_staticTextCaption->SetLabelText(syncSelection ?_("Start to synchronize the selection?") : _("Start synchronization now?"));
    m_staticTextSyncVar->SetLabelText(getVariantName(syncVar));

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
        setImage(*m_bitmapSyncVar, loadImage(varImgName, -1 /*maxWidth*/, dipToScreen(getMenuIconDipSize())));

    m_checkBoxDontShowAgain->SetValue(dontShowAgain);

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); });

    //update preview of item count and bytes to be transferred:
    auto setValue = [](wxStaticText& txtControl, bool isZeroValue, const wxString& valueAsString, wxStaticBitmap& bmpControl, const char* imageName)
    {
        wxFont fnt = txtControl.GetFont();
        fnt.SetWeight(isZeroValue ? wxFONTWEIGHT_NORMAL : wxFONTWEIGHT_BOLD);
        txtControl.SetFont(fnt);

        setText(txtControl, valueAsString);

        setImage(bmpControl, greyScaleIfDisabled(mirrorIfRtl(loadImage(imageName)), !isZeroValue));
    };

    auto setIntValue = [&setValue](wxStaticText& txtControl, int value, wxStaticBitmap& bmpControl, const char* imageName)
    {
        setValue(txtControl, value == 0, formatNumber(value), bmpControl, imageName);
    };

    setValue(*m_staticTextData, st.getBytesToProcess() == 0, formatFilesizeShort(st.getBytesToProcess()), *m_bitmapData, "data");
    setIntValue(*m_staticTextCreateLeft,  st.createCount<SelectSide::left >(), *m_bitmapCreateLeft,  "so_create_left_sicon");
    setIntValue(*m_staticTextUpdateLeft,  st.updateCount<SelectSide::left >(), *m_bitmapUpdateLeft,  "so_update_left_sicon");
    setIntValue(*m_staticTextDeleteLeft,  st.deleteCount<SelectSide::left >(), *m_bitmapDeleteLeft,  "so_delete_left_sicon");
    setIntValue(*m_staticTextCreateRight, st.createCount<SelectSide::right>(), *m_bitmapCreateRight, "so_create_right_sicon");
    setIntValue(*m_staticTextUpdateRight, st.updateCount<SelectSide::right>(), *m_bitmapUpdateRight, "so_update_right_sicon");
    setIntValue(*m_staticTextDeleteRight, st.deleteCount<SelectSide::right>(), *m_bitmapDeleteRight, "so_delete_right_sicon");

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
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
    EndModal(static_cast<int>(ConfirmationButton::accept));
}
}

ConfirmationButton fff::showSyncConfirmationDlg(wxWindow* parent,
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
    return static_cast<ConfirmationButton>(dlg.ShowModal());
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
    void onShowHiddenDialogs   (wxCommandEvent& event) override { expandConfigArea(ConfigArea::hidden); };
    void onShowContextCustomize(wxCommandEvent& event) override { expandConfigArea(ConfigArea::context); };
    void onDefault       (wxCommandEvent& event) override;
    void onCancel        (wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onClose         (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onAddRow        (wxCommandEvent& event) override;
    void onRemoveRow     (wxCommandEvent& event) override;
    void onShowLogFolder      (wxCommandEvent& event) override;
    void onToggleLogfilesLimit(wxCommandEvent& event) override { updateGui(); }
    void onToggleHiddenDialog (wxCommandEvent& event) override { updateGui(); }

    void onSelectSoundCompareDone (wxCommandEvent& event) override { selectSound(*m_textCtrlSoundPathCompareDone); }
    void onSelectSoundSyncDone    (wxCommandEvent& event) override { selectSound(*m_textCtrlSoundPathSyncDone); }
    void onSelectSoundAlertPending(wxCommandEvent& event) override { selectSound(*m_textCtrlSoundPathAlertPending); }
    void selectSound(wxTextCtrl& txtCtrl);

    void onChangeSoundFilePath(wxCommandEvent& event) override { updateGui(); }

    void onPlayCompareDone (wxCommandEvent& event) override { playSoundWithDiagnostics(trimCpy(m_textCtrlSoundPathCompareDone ->GetValue())); }
    void onPlaySyncDone    (wxCommandEvent& event) override { playSoundWithDiagnostics(trimCpy(m_textCtrlSoundPathSyncDone    ->GetValue())); }
    void onPlayAlertPending(wxCommandEvent& event) override { playSoundWithDiagnostics(trimCpy(m_textCtrlSoundPathAlertPending->GetValue())); }
    void playSoundWithDiagnostics(const wxString& filePath);

    void onGridResize(wxEvent& event);
    void updateGui();

    enum class ConfigArea
    {
        hidden,
        context
    };
    void expandConfigArea(ConfigArea area);

    //work around defunct keyboard focus on macOS (or is it wxMac?) => not needed for this dialog!
    //void onLocalKeyEvent(wxKeyEvent& event);

    void setExtApp(const std::vector<ExternalApp>& extApp);
    std::vector<ExternalApp> getExtApp() const;

    std::unordered_map<std::wstring, std::wstring> descriptionTransToEng_; //"translated description" -> "english" mapping for external application config

    const XmlGlobalSettings defaultCfg_;

    std::vector<std::tuple<std::function<bool(const XmlGlobalSettings& gs)> /*get dialog shown status*/,
        std::function<void(XmlGlobalSettings& gs, bool show)> /*set dialog shown status*/,
        wxString /*dialog message*/>> hiddenDialogCfgMapping_
    {
        //*INDENT-OFF*
        {[](const XmlGlobalSettings& gs){     return gs.confirmDlgs.confirmSyncStart; },
         [](      XmlGlobalSettings& gs, bool show){ gs.confirmDlgs.confirmSyncStart = show; }, _("Start synchronization now?")},
        {[](const XmlGlobalSettings& gs){     return gs.confirmDlgs.confirmSaveConfig; },
         [](      XmlGlobalSettings& gs, bool show){ gs.confirmDlgs.confirmSaveConfig = show; }, _("Do you want to save changes to %x?")},
        {[](const XmlGlobalSettings& gs){    return !gs.progressDlgAutoClose; },
         [](      XmlGlobalSettings& gs, bool show){ gs.progressDlgAutoClose = !show; }, _("Leave progress dialog open after synchronization. (don't auto-close)")},
        {[](const XmlGlobalSettings& gs){     return gs.confirmDlgs.confirmSwapSides; },
         [](      XmlGlobalSettings& gs, bool show){ gs.confirmDlgs.confirmSwapSides = show; }, _("Please confirm you want to swap sides.")},
        {[](const XmlGlobalSettings& gs){     return gs.confirmDlgs.confirmCommandMassInvoke; }, 
         [](      XmlGlobalSettings& gs, bool show){ gs.confirmDlgs.confirmCommandMassInvoke = show; }, _P("Do you really want to execute the command %y for one item?",
                                                                                                           "Do you really want to execute the command %y for %x items?", 42)},
        {[](const XmlGlobalSettings& gs){     return gs.warnDlgs.warnFolderNotExisting; },
         [](      XmlGlobalSettings& gs, bool show){ gs.warnDlgs.warnFolderNotExisting = show; }, _("The following folders do not yet exist:") + L" [...] " + _("The folders are created automatically when needed.")},
        {[](const XmlGlobalSettings& gs){     return gs.warnDlgs.warnFoldersDifferInCase; },
         [](      XmlGlobalSettings& gs, bool show){ gs.warnDlgs.warnFoldersDifferInCase = show; }, _("The following folder paths differ in case. Please use a single form in order to avoid duplicate accesses.")},
        {[](const XmlGlobalSettings& gs){     return gs.warnDlgs.warnDependentFolderPair; },
         [](      XmlGlobalSettings& gs, bool show){ gs.warnDlgs.warnDependentFolderPair = show; }, _("One folder of the folder pair is a subfolder of the other.") + L' ' + _("The folder should be excluded via filter.")},
        {[](const XmlGlobalSettings& gs){     return gs.warnDlgs.warnDependentBaseFolders; },
         [](      XmlGlobalSettings& gs, bool show){ gs.warnDlgs.warnDependentBaseFolders = show; }, _("Some files will be synchronized as part of multiple folder pairs.") + L' ' + _("To avoid conflicts, set up exclude filters so that each updated file is included by only one folder pair.")},
        {[](const XmlGlobalSettings& gs){     return gs.warnDlgs.warnSignificantDifference; },
         [](      XmlGlobalSettings& gs, bool show){ gs.warnDlgs.warnSignificantDifference = show; }, _("The following folders are significantly different. Please check that the correct folders are selected for synchronization.")},
        {[](const XmlGlobalSettings& gs){     return gs.warnDlgs.warnNotEnoughDiskSpace; },
         [](      XmlGlobalSettings& gs, bool show){ gs.warnDlgs.warnNotEnoughDiskSpace = show; }, _("Not enough free disk space available in:")},
        {[](const XmlGlobalSettings& gs){     return gs.warnDlgs.warnUnresolvedConflicts; },
         [](      XmlGlobalSettings& gs, bool show){ gs.warnDlgs.warnUnresolvedConflicts = show; }, _("The following items have unresolved conflicts and will not be synchronized:")},
        {[](const XmlGlobalSettings& gs){     return gs.warnDlgs.warnRecyclerMissing; },
         [](      XmlGlobalSettings& gs, bool show){ gs.warnDlgs.warnRecyclerMissing = show; }, _("The recycle bin is not available for %x.") + L' ' + _("Ignore and delete permanently each time recycle bin is unavailable?")},
        {[](const XmlGlobalSettings& gs){     return gs.warnDlgs.warnInputFieldEmpty; },
         [](      XmlGlobalSettings& gs, bool show){ gs.warnDlgs.warnInputFieldEmpty = show; }, _("A folder input field is empty.") + L' ' + _("The corresponding folder will be considered as empty.")},
        {[](const XmlGlobalSettings& gs){     return gs.warnDlgs.warnDirectoryLockFailed; },
         [](      XmlGlobalSettings& gs, bool show){ gs.warnDlgs.warnDirectoryLockFailed = show; }, _("Cannot set directory locks for the following folders:")},
        {[](const XmlGlobalSettings& gs){     return gs.warnDlgs.warnVersioningFolderPartOfSync; },
         [](      XmlGlobalSettings& gs, bool show){ gs.warnDlgs.warnVersioningFolderPartOfSync = show; }, _("The versioning folder is part of the synchronization.") + L' ' +  _("The folder should be excluded via filter.")},
        //*INDENT-ON*
    };

    FolderSelector logFolderSelector_;

    //output-only parameters:
    XmlGlobalSettings& globalCfgOut_;
};


OptionsDlg::OptionsDlg(wxWindow* parent, XmlGlobalSettings& globalCfg) :
    OptionsDlgGenerated(parent),

    logFolderSelector_(this, *m_panelLogfile, *m_buttonSelectLogFolder, *m_bpButtonSelectAltLogFolder, *m_logFolderPath, globalCfg.logFolderLastSelected, globalCfg.sftpKeyFileLastSelected,
                       nullptr /*staticText*/, nullptr /*dropWindow2*/, nullptr /*droppedPathsFilter*/,
                       [](const Zstring& folderPathPhrase) { return 1; } /*getDeviceParallelOps_*/, nullptr /*setDeviceParallelOps_*/),
                   globalCfgOut_(globalCfg)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    //setMainInstructionFont(*m_staticTextHeader);
    m_gridCustomCommand->SetTabBehaviour(wxGrid::Tab_Leave);

    const wxImage imgFileManagerSmall_([]
    {
        try { return extractWxImage(fff::getFileManagerIcon(dipToScreen(20))); /*throw SysError*/ }
        catch (SysError&) { assert(false); return loadImage("file_manager", dipToScreen(20)); }
    }());
    setImage(*m_bpButtonShowLogFolder, imgFileManagerSmall_);
    m_bpButtonShowLogFolder->SetToolTip(translate(extCommandFileManager.description));//translate default external apps on the fly: "Show in Explorer"

    m_logFolderPath->SetHint(utfTo<wxString>(defaultCfg_.logFolderPhrase));
    //1. no text shown when control is disabled! 2. apparently there's a refresh problem on GTK

    m_logFolderPath->setHistory(std::make_shared<HistoryList>(globalCfg.logFolderHistory, globalCfg.folderHistoryMax));

    logFolderSelector_.setPath(globalCfg.logFolderPhrase);

    setDefaultWidth(*m_spinCtrlLogFilesMaxAge);

    setImage(*m_bitmapSettings,           loadImage("settings"));
    setImage(*m_bitmapWarnings,           loadImage("msg_warning", dipToScreen(20)));
    setImage(*m_bitmapLogFile,            loadImage("log_file",    dipToScreen(20)));
    setImage(*m_bitmapNotificationSounds, loadImage("notification_sounds"));
    setImage(*m_bitmapConsole,            loadImage("command_line", dipToScreen(20)));
    setImage(*m_bitmapCompareDone,        loadImage("compare",      dipToScreen(20)));
    setImage(*m_bitmapSyncDone,           loadImage("start_sync",   dipToScreen(20)));
    setImage(*m_bitmapAlertPending,       loadImage("msg_error",    dipToScreen(20)));
    setImage(*m_bpButtonPlayCompareDone,  loadImage("play_sound"));
    setImage(*m_bpButtonPlaySyncDone,     loadImage("play_sound"));
    setImage(*m_bpButtonPlayAlertPending, loadImage("play_sound"));
    setImage(*m_bpButtonAddRow,           loadImage("item_add"));
    setImage(*m_bpButtonRemoveRow,        loadImage("item_remove"));

    //--------------------------------------------------------------------------------
    m_checkListHiddenDialogs->Hide();
    m_buttonShowCtxCustomize->Hide();

    //fix wxCheckListBox's stupid "per-item toggle" when multiple items are selected
    m_checkListHiddenDialogs->Bind(wxEVT_KEY_DOWN, [&checklist = *m_checkListHiddenDialogs](wxKeyEvent& event)
    {
        switch (event.GetKeyCode())
        {
            case WXK_SPACE:
            case WXK_NUMPAD_SPACE:
                assert(checklist.HasMultipleSelection());

                if (wxArrayInt selection;
                    checklist.GetSelections(selection), !selection.empty())
                {
                    const bool checkedNew = !checklist.IsChecked(selection[0]);

                    for (const int itemPos : selection)
                        checklist.Check(itemPos, checkedNew);

                    wxCommandEvent chkEvent(wxEVT_CHECKLISTBOX);
                    chkEvent.SetInt(selection[0]);
                    checklist.GetEventHandler()->ProcessEvent(chkEvent);
                }
                return;
        }
        event.Skip();
    });

    std::stable_partition(hiddenDialogCfgMapping_.begin(), hiddenDialogCfgMapping_.end(), [&](const auto& item)
    {
        const auto& [dlgShown, dlgSetShown, msg] = item;
        return !dlgShown(globalCfg); //move hidden dialogs to the top
    });

    std::vector<wxString> dialogMessages;
    for (const auto& [dlgShown, dlgSetShown, msg] : hiddenDialogCfgMapping_)
        dialogMessages.push_back(msg);

    m_checkListHiddenDialogs->Append(dialogMessages);

    unsigned int itemPos = 0;
    for (const auto& [dlgShown, dlgSetShown, msg] : hiddenDialogCfgMapping_)
    {
        if (dlgShown(globalCfg))
            m_checkListHiddenDialogs->Check(itemPos);
        ++itemPos;
    }

    //--------------------------------------------------------------------------------
    m_checkBoxFailSafe       ->SetValue(globalCfg.failSafeFileCopy);
    m_checkBoxCopyLocked     ->SetValue(globalCfg.copyLockedFiles);
    m_checkBoxCopyPermissions->SetValue(globalCfg.copyFilePermissions);

    m_checkBoxLogFilesMaxAge->SetValue(globalCfg.logfilesMaxAgeDays > 0);
    m_spinCtrlLogFilesMaxAge->SetValue(globalCfg.logfilesMaxAgeDays > 0 ? globalCfg.logfilesMaxAgeDays : XmlGlobalSettings().logfilesMaxAgeDays);

    switch (globalCfg.logFormat)
    {
        case LogFileFormat::html:
            m_radioBtnLogHtml->SetValue(true);
            break;
        case LogFileFormat::text:
            m_radioBtnLogText->SetValue(true);
            break;
    }

    m_textCtrlSoundPathCompareDone ->ChangeValue(utfTo<wxString>(globalCfg.soundFileCompareFinished));
    m_textCtrlSoundPathSyncDone    ->ChangeValue(utfTo<wxString>(globalCfg.soundFileSyncFinished));
    m_textCtrlSoundPathAlertPending->ChangeValue(utfTo<wxString>(globalCfg.soundFileAlertPending));
    //--------------------------------------------------------------------------------

    bSizerLockedFiles->Show(false);
    m_gridCustomCommand->SetMargins(0, 0);

    //automatically fit column width to match total grid width
    m_gridCustomCommand->GetGridWindow()->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) { onGridResize(event); });

    //temporarily set dummy value for window height calculations:
    setExtApp(std::vector<ExternalApp>(globalCfg.externalApps.size() + 1));
    updateGui();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
    Center(); //needs to be re-applied after a dialog size change!

    //restore actual value:
    setExtApp(globalCfg.externalApps);
    updateGui();

    m_buttonOkay->SetFocus();
}


void OptionsDlg::onGridResize(wxEvent& event)
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
    m_spinCtrlLogFilesMaxAge->Enable(m_checkBoxLogFilesMaxAge->GetValue());

    m_bpButtonPlayCompareDone ->Enable(!trimCpy(m_textCtrlSoundPathCompareDone ->GetValue()).empty());
    m_bpButtonPlaySyncDone    ->Enable(!trimCpy(m_textCtrlSoundPathSyncDone    ->GetValue()).empty());
    m_bpButtonPlayAlertPending->Enable(!trimCpy(m_textCtrlSoundPathAlertPending->GetValue()).empty());

    int hiddenDialogs = 0;
    for (unsigned int itemPos = 0; itemPos < static_cast<unsigned int>(hiddenDialogCfgMapping_.size()); ++itemPos)
        if (!m_checkListHiddenDialogs->IsChecked(itemPos))
            ++hiddenDialogs;
    assert(hiddenDialogCfgMapping_.size() == m_checkListHiddenDialogs->GetCount());

    m_staticTextHiddenDialogsCount->SetLabelText(L'(' + (hiddenDialogs == 0 ? _("No dialogs hidden") :
                                                         _P("1 dialog hidden", "%x dialogs hidden", hiddenDialogs)) + L')');
    Layout();
}


void OptionsDlg::expandConfigArea(ConfigArea area)
{
    //only show one expanded area at a time (wxGTK even crashes when showing both: not worth debugging)
    m_buttonShowHiddenDialogs->Show(area != ConfigArea::hidden);
    m_buttonShowCtxCustomize ->Show(area != ConfigArea::context);

    m_checkListHiddenDialogs->Show(area == ConfigArea::hidden);
    bSizerContextCustomize  ->Show(area == ConfigArea::context);

    Layout();
    Refresh(); //required on Windows
}


void OptionsDlg::selectSound(wxTextCtrl& txtCtrl)
{
    std::optional<Zstring> defaultFolderPath = getParentFolderPath(utfTo<Zstring>(txtCtrl.GetValue()));
    if (!defaultFolderPath)
        defaultFolderPath = getResourceDirPath();

    wxFileDialog fileSelector(this, wxString() /*message*/, utfTo<wxString>(*defaultFolderPath), wxString() /*default file name*/,
                              wxString(L"WAVE (*.wav)|*.wav") + L"|" + _("All files") + L" (*.*)|*",
                              wxFD_OPEN);
    if (fileSelector.ShowModal() != wxID_OK)
        return;

    txtCtrl.ChangeValue(fileSelector.GetPath());
    updateGui();
}


void OptionsDlg::playSoundWithDiagnostics(const wxString& filePath)
{
    try
    {
        //::PlaySound() on Windows does not set last error!
        //wxSound::Play(..., wxSOUND_SYNC) can return "false", but also without details!
        //=> check file access manually:
        [[maybe_unused]] const std::string& stream = getFileContent(utfTo<Zstring>(filePath), nullptr /*notifyUnbufferedIO*/); //throw FileError

        if (!wxSound::Play(filePath, wxSOUND_ASYNC))
            throw FileError(L"Sound playback failed. No further diagnostics available.");
    }
    catch (const FileError& e) { showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString())); }
}


void OptionsDlg::onDefault(wxCommandEvent& event)
{
    m_checkBoxFailSafe       ->SetValue(defaultCfg_.failSafeFileCopy);
    m_checkBoxCopyLocked     ->SetValue(defaultCfg_.copyLockedFiles);
    m_checkBoxCopyPermissions->SetValue(defaultCfg_.copyFilePermissions);

    unsigned int itemPos = 0;
    for (const auto& [dlgShown, dlgSetShown, msg] : hiddenDialogCfgMapping_)
        m_checkListHiddenDialogs->Check(itemPos++, dlgShown(defaultCfg_));

    logFolderSelector_.setPath(defaultCfg_.logFolderPhrase);

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

    m_textCtrlSoundPathCompareDone ->ChangeValue(utfTo<wxString>(defaultCfg_.soundFileCompareFinished));
    m_textCtrlSoundPathSyncDone    ->ChangeValue(utfTo<wxString>(defaultCfg_.soundFileSyncFinished));
    m_textCtrlSoundPathAlertPending->ChangeValue(utfTo<wxString>(defaultCfg_.soundFileAlertPending));

    setExtApp(defaultCfg_.externalApps);

    updateGui();
}


void OptionsDlg::onOkay(wxCommandEvent& event)
{
    //------- parameter validation (BEFORE writing output!) -------
    Zstring logFolderPhrase = logFolderSelector_.getPath();
    if (AFS::isNullPath(createAbstractPath(logFolderPhrase))) //no need to show an error: just set default!
        logFolderPhrase = defaultCfg_.logFolderPhrase;
    //-------------------------------------------------------------

    //write settings only when okay-button is pressed (except hidden dialog reset)!
    globalCfgOut_.failSafeFileCopy    = m_checkBoxFailSafe->GetValue();
    globalCfgOut_.copyLockedFiles     = m_checkBoxCopyLocked->GetValue();
    globalCfgOut_.copyFilePermissions = m_checkBoxCopyPermissions->GetValue();

    globalCfgOut_.logFolderPhrase = logFolderPhrase;
    m_logFolderPath->getHistory()->addItem(logFolderPhrase);
    globalCfgOut_.logFolderHistory = m_logFolderPath->getHistory()->getList();
    globalCfgOut_.logfilesMaxAgeDays = m_checkBoxLogFilesMaxAge->GetValue() ? m_spinCtrlLogFilesMaxAge->GetValue() : -1;
    globalCfgOut_.logFormat = m_radioBtnLogHtml->GetValue() ? LogFileFormat::html : LogFileFormat::text;

    globalCfgOut_.soundFileCompareFinished = utfTo<Zstring>(trimCpy(m_textCtrlSoundPathCompareDone ->GetValue()));
    globalCfgOut_.soundFileSyncFinished    = utfTo<Zstring>(trimCpy(m_textCtrlSoundPathSyncDone    ->GetValue()));
    globalCfgOut_.soundFileAlertPending    = utfTo<Zstring>(trimCpy(m_textCtrlSoundPathAlertPending->GetValue()));

    globalCfgOut_.externalApps = getExtApp();

    unsigned int itemPos = 0;
    for (const auto& [dlgShown, dlgSetShown, msg] : hiddenDialogCfgMapping_)
        dlgSetShown(globalCfgOut_, m_checkListHiddenDialogs->IsChecked(itemPos++));

    EndModal(static_cast<int>(ConfirmationButton::accept));
}


void OptionsDlg::setExtApp(const std::vector<ExternalApp>& extApps)
{
    int rowDiff = static_cast<int>(extApps.size()) - m_gridCustomCommand->GetNumberRows();
    ++rowDiff; //append empty row to facilitate insertions by user

    if (rowDiff >= 0)
        m_gridCustomCommand->AppendRows(rowDiff);
    else
        m_gridCustomCommand->DeleteRows(0, -rowDiff);

    int row = 0;
    for (const auto& [descriptionEng, cmdLine] : extApps)
    {
        const std::wstring description = translate(descriptionEng);
        //remember english description to save in GlobalSettings.xml later rather than hard-code translation
        descriptionTransToEng_[description] = descriptionEng;

        m_gridCustomCommand->SetCellValue(row, 0, description);
        m_gridCustomCommand->SetCellValue(row, 1, utfTo<wxString>(cmdLine));
        ++row;
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
            output.push_back({description, commandline});
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

        m_gridCustomCommand->SetFocus(); //make grid cursor visible
    }
}


void OptionsDlg::onShowLogFolder(wxCommandEvent& event)
{
    try
    {
        AbstractPath logFolderPath = createAbstractPath(logFolderSelector_.getPath());
        if (AFS::isNullPath(logFolderPath))
            logFolderPath = createAbstractPath(defaultCfg_.logFolderPhrase);

        openFolderInFileBrowser(logFolderPath); //throw FileError
    }
    catch (const FileError& e) { showNotificationDialog(this, DialogInfoType::error, PopupDialogCfg().setDetailInstructions(e.toString())); }
}
}

ConfirmationButton fff::showOptionsDlg(wxWindow* parent, XmlGlobalSettings& globalCfg)
{
    OptionsDlg dlg(parent, globalCfg);
    return static_cast<ConfirmationButton>(dlg.ShowModal());
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
    void onCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }

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

    assert(m_calendarFrom->GetWindowStyleFlag() == m_calendarTo->GetWindowStyleFlag());
    assert(m_calendarFrom->HasFlag(wxCAL_SHOW_HOLIDAYS)); //caveat: for some stupid reason this is not honored when set by SetWindowStyleFlag()
    assert(m_calendarFrom->HasFlag(wxCAL_SHOW_SURROUNDING_WEEKS));
    assert(!m_calendarFrom->HasFlag(wxCAL_MONDAY_FIRST) &&
           !m_calendarFrom->HasFlag(wxCAL_SUNDAY_FIRST)); //...because we set it in the following:
    long style = m_calendarFrom->GetWindowStyleFlag();

    style |= getFirstDayOfWeek() == WeekDay::sunday ? wxCAL_SUNDAY_FIRST : wxCAL_MONDAY_FIRST; //seems to be ignored on CentOS

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
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
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

    EndModal(static_cast<int>(ConfirmationButton::accept));
}
}

ConfirmationButton fff::showSelectTimespanDlg(wxWindow* parent, time_t& timeFrom, time_t& timeTo)
{
    SelectTimespanDlg dlg(parent, timeFrom, timeTo);
    return static_cast<ConfirmationButton>(dlg.ShowModal());
}

//########################################################################################

namespace
{
class PasswordPromptDlg : public PasswordPromptDlgGenerated
{
public:
    PasswordPromptDlg(wxWindow* parent, const std::wstring& msg, const std::wstring& lastErrorMsg /*optional*/, Zstring& password);

private:
    void onOkay  (wxCommandEvent& event) override;
    void onCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }

    void onToggleShowPassword(wxCommandEvent& event) override;

    void updateGui();

    //work around defunct keyboard focus on macOS (or is it wxMac?) => not needed for this dialog!
    //void onLocalKeyEvent(wxKeyEvent& event);

    //output-only parameters:
    Zstring& passwordOut_;
};


PasswordPromptDlg::PasswordPromptDlg(wxWindow* parent, const std::wstring& msg, const std::wstring& lastErrorMsg /*optional*/, Zstring& password) :
    PasswordPromptDlgGenerated(parent),
    passwordOut_(password)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOkay).setCancel(m_buttonCancel));

    wxString titleTmp;
    if (!parent || !parent->IsShownOnScreen())
        titleTmp = wxTheApp->GetAppDisplayName();
    SetTitle(titleTmp);

    const int maxWidthDip = 600;

    m_staticTextMain->SetLabelText(msg);
    m_staticTextMain->Wrap(dipToWxsize(maxWidthDip));

    m_checkBoxShowPassword->SetValue(false);

    m_textCtrlPasswordHidden->ChangeValue(utfTo<wxString>(password));

    bSizerError->Show(!lastErrorMsg.empty());
    if (!lastErrorMsg.empty())
    {
        setImage(*m_bitmapError, loadImage("msg_error", dipToWxsize(32)));

        m_staticTextError->SetLabelText(lastErrorMsg);
        m_staticTextError->Wrap(dipToWxsize(maxWidthDip) - m_bitmapError->GetSize().x - 10 /*border in non-DIP pixel*/);
    }

    //set up default view for dialog size calculation
    m_textCtrlPasswordVisible->Hide();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
    Center(); //needs to be re-applied after a dialog size change!

    updateGui(); //*after* SetSizeHints when standard dialog height has been calculated

    //m_textCtrlPasswordHidden->SelectAll(); -> apparantly implicitly caused by SetFocus!?
    m_textCtrlPasswordHidden->SetFocus();
}


void PasswordPromptDlg::onToggleShowPassword(wxCommandEvent& event)
{
    if (m_checkBoxShowPassword->GetValue())
        m_textCtrlPasswordVisible->ChangeValue(m_textCtrlPasswordHidden->GetValue());
    else
        m_textCtrlPasswordHidden->ChangeValue(m_textCtrlPasswordVisible->GetValue());

    updateGui();

    wxTextCtrl& textCtrl = *(m_checkBoxShowPassword->GetValue() ? m_textCtrlPasswordVisible : m_textCtrlPasswordHidden);
    textCtrl.SetFocus(); //macOS: selects text as unwanted side effect => *before* SetInsertionPointEnd()
    textCtrl.SetInsertionPointEnd();
}


void PasswordPromptDlg::updateGui()
{
    m_textCtrlPasswordVisible->Show( m_checkBoxShowPassword->GetValue());
    m_textCtrlPasswordHidden ->Show(!m_checkBoxShowPassword->GetValue());

    Layout();
    Refresh();
}


void PasswordPromptDlg::onOkay(wxCommandEvent& event)
{
    passwordOut_ = utfTo<Zstring>((m_checkBoxShowPassword->GetValue() ? m_textCtrlPasswordVisible : m_textCtrlPasswordHidden)->GetValue());
    EndModal(static_cast<int>(ConfirmationButton::accept));
}
}

ConfirmationButton fff::showPasswordPrompt(wxWindow* parent, const std::wstring& msg, const std::wstring& lastErrorMsg /*optional*/, Zstring& password)
{
    PasswordPromptDlg dlg(parent, msg, lastErrorMsg, password);
    return static_cast<ConfirmationButton>(dlg.ShowModal());
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
    void onCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }

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

    m_staticTextHighlight->Wrap(dipToWxsize(300));

    setDefaultWidth(*m_spinCtrlOverdueDays);

    m_spinCtrlOverdueDays->SetValue(cfgHistSyncOverdueDays);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
    Center(); //needs to be re-applied after a dialog size change!

    m_spinCtrlOverdueDays->SetFocus();
}


void CfgHighlightDlg::onOkay(wxCommandEvent& event)
{
    cfgHistSyncOverdueDaysOut_ = m_spinCtrlOverdueDays->GetValue();
    EndModal(static_cast<int>(ConfirmationButton::accept));
}
}

ConfirmationButton fff::showCfgHighlightDlg(wxWindow* parent, int& cfgHistSyncOverdueDays)
{
    CfgHighlightDlg dlg(parent, cfgHistSyncOverdueDays);
    return static_cast<ConfirmationButton>(dlg.ShowModal());
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
    void onCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ActivationDlgButton::cancel)); }
    void onClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ActivationDlgButton::cancel)); }

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

    std::wstring title = L"FreeFileSync " + utfTo<std::wstring>(ffsVersion);
    SetTitle(title);

    //setMainInstructionFont(*m_staticTextMain);

    m_richTextLastError           ->SetMinSize({-1, m_richTextLastError          ->GetCharHeight() * 8});
    m_richTextManualActivationUrl ->SetMinSize({-1, m_richTextManualActivationUrl->GetCharHeight() * 4});
    m_textCtrlOfflineActivationKey->SetMinSize({dipToWxsize(260), -1});

    setImage(*m_bitmapActivation, loadImage("internet"));
    m_textCtrlOfflineActivationKey->ForceUpper();

    setTextWithUrls(*m_richTextLastError, lastErrorMsg);
    setTextWithUrls(*m_richTextManualActivationUrl, manualActivationUrl);

    m_textCtrlOfflineActivationKey->ChangeValue(manualActivationKey);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonActivateOnline->SetFocus();
}


void ActivationDlg::onCopyUrl(wxCommandEvent& event)
{
    setClipboardText(m_richTextManualActivationUrl->GetValue());

    m_richTextManualActivationUrl->SetFocus(); //[!] otherwise selection is lost
    m_richTextManualActivationUrl->SelectAll(); //some visual feedback
}


void ActivationDlg::onActivateOnline(wxCommandEvent& event)
{
    manualActivationKeyOut_ = utfTo<std::wstring>(m_textCtrlOfflineActivationKey->GetValue());
    EndModal(static_cast<int>(ActivationDlgButton::activateOnline));
}


void ActivationDlg::onActivateOffline(wxCommandEvent& event)
{
    manualActivationKeyOut_ = utfTo<std::wstring>(m_textCtrlOfflineActivationKey->GetValue());
    if (trimCpy(manualActivationKeyOut_).empty()) //alternative: disable button? => user thinks option is not available!
    {
        showNotificationDialog(this, DialogInfoType::info, PopupDialogCfg().setMainInstructions(_("Please enter a key for offline activation.")));
        m_textCtrlOfflineActivationKey->SetFocus();
        return;
    }

    EndModal(static_cast<int>(ActivationDlgButton::activateOffline));
}
}

ActivationDlgButton fff::showActivationDialog(wxWindow* parent, const std::wstring& lastErrorMsg, const std::wstring& manualActivationUrl, std::wstring& manualActivationKey)
{
    ActivationDlg dlg(parent, lastErrorMsg, manualActivationUrl, manualActivationKey);
    return static_cast<ActivationDlgButton>(dlg.ShowModal());
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
        m_staticTextHeader->SetLabelText(_("Downloading update...") + L' ' + formatProgressPercent(fraction) + L" (" + formatFilesizeShort(bytesCurrent_) + L')');
        m_gaugeProgress->SetValue(std::round(fraction * GAUGE_FULL_RANGE));

        m_staticTextDetails->SetLabelText(utfTo<std::wstring>(filePath_));
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
    m_staticTextHeader->Wrap(dipToWxsize(460)); //*after* font change!

    m_staticTextDetails->SetMinSize({dipToWxsize(550), -1});

    setImage(*m_bitmapDownloading, loadImage("internet"));

    m_gaugeProgress->SetRange(GAUGE_FULL_RANGE);

    updateGui();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
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
void DownloadProgressWindow::requestUiUpdate()                      { pimpl_->requestUiUpdate(); } //throw CancelPressed

//########################################################################################


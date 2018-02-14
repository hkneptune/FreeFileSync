// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "batch_config.h"
#include <wx/wupdlock.h>
#include <wx+/std_button_layout.h>
#include <wx+/font_size.h>
#include <wx+/image_resources.h>
#include <wx+/image_tools.h>
#include <wx+/choice_enum.h>
#include "gui_generated.h"
#include "folder_selector.h"
#include "../lib/help_provider.h"
#include "../lib/generate_logfile.h"


using namespace zen;
using namespace fff;


namespace
{
struct BatchDialogConfig
{
    BatchExclusiveConfig batchExCfg;
    bool ignoreErrors = false;
};


class BatchDialog : public BatchDlgGenerated
{
public:
    BatchDialog(wxWindow* parent, BatchDialogConfig& dlgCfg);

private:
    void OnClose       (wxCloseEvent&   event) override { EndModal(ReturnBatchConfig::BUTTON_CANCEL); }
    void OnCancel      (wxCommandEvent& event) override { EndModal(ReturnBatchConfig::BUTTON_CANCEL); }
    void OnSaveBatchJob(wxCommandEvent& event) override;

    void OnToggleIgnoreErrors(wxCommandEvent& event) override { updateGui(); }
    void OnToggleRunMinimized(wxCommandEvent& event) override
    {
        m_checkBoxAutoClose->SetValue(m_checkBoxRunMinimized->GetValue()); //usually user wants to change both
        updateGui();
    }

    void OnHelpScheduleBatch(wxHyperlinkEvent& event) override { displayHelpEntry(L"schedule-a-batch-job", this); }

    void OnToggleGenerateLogfile(wxCommandEvent& event) override { updateGui(); }
    void OnToggleLogfilesLimit  (wxCommandEvent& event) override { updateGui(); }

    void onLocalKeyEvent(wxKeyEvent& event);

    void updateGui(); //re-evaluate gui after config changes

    void setConfig(const BatchDialogConfig& batchCfg);
    BatchDialogConfig getConfig() const;

    //output-only parameters
    BatchDialogConfig& dlgCfgOut_;

    std::unique_ptr<FolderSelector> logfileDir_; //always bound, solve circular compile-time dependency

    EnumDescrList<PostSyncAction> enumPostSyncAction_;
};

//###################################################################################################################################

BatchDialog::BatchDialog(wxWindow* parent, BatchDialogConfig& dlgCfg) :
    BatchDlgGenerated(parent),
    dlgCfgOut_(dlgCfg)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonSaveAs).setCancel(m_buttonCancel));

    m_staticTextDescr->SetLabel(replaceCpy(m_staticTextDescr->GetLabel(), L"%x", L"FreeFileSync.exe <" + _("job name") + L">.ffs_batch"));

    m_bitmapBatchJob->SetBitmap(getResourceImage(L"batch"));

    logfileDir_ = std::make_unique<FolderSelector>(*m_panelLogfile, *m_buttonSelectLogFolder, *m_bpButtonSelectAltLogFolder, *m_logFolderPath, nullptr /*staticText*/, nullptr /*wxWindow*/);

    logfileDir_->setBackgroundText(utfTo<std::wstring>(getDefaultLogFolderPath()));

    enumPostSyncAction_.
    add(PostSyncAction::NONE,     L"").
    add(PostSyncAction::SLEEP,    _("System: Sleep")).
    add(PostSyncAction::SHUTDOWN, _("System: Shut down"));

    setConfig(dlgCfg);

    //enable dialog-specific key local events
    Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(BatchDialog::onLocalKeyEvent), nullptr, this);

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonSaveAs->SetFocus();
}


void BatchDialog::updateGui() //re-evaluate gui after config changes
{
    const BatchDialogConfig dlgCfg = getConfig(); //resolve parameter ownership: some on GUI controls, others member variables

    m_bitmapIgnoreErrors->SetBitmap(getResourceImage(dlgCfg.ignoreErrors ? L"msg_error_medium_ignored" : L"msg_error_medium"));

    m_radioBtnErrorDialogShow  ->Enable(!dlgCfg.ignoreErrors);
    m_radioBtnErrorDialogCancel->Enable(!dlgCfg.ignoreErrors);


    m_bitmapMinimizeToTray->SetBitmap(dlgCfg.batchExCfg.runMinimized ? getResourceImage(L"minimize_to_tray") : greyScale(getResourceImage(L"minimize_to_tray")));


    m_panelLogfile->Enable(m_checkBoxSaveLog->GetValue()); //enabled status is *not* directly dependent from resolved config! (but transitively)
    m_bitmapLogFile->SetBitmap(m_checkBoxSaveLog->GetValue() ? getResourceImage(L"log_file") : greyScale(getResourceImage(L"log_file")));
    m_checkBoxLogfilesLimit->Enable(m_checkBoxSaveLog->GetValue());
    m_spinCtrlLogfileLimit ->Enable(m_checkBoxSaveLog->GetValue() && m_checkBoxLogfilesLimit->GetValue());
}


void BatchDialog::setConfig(const BatchDialogConfig& dlgCfg)
{

    m_checkBoxIgnoreErrors->SetValue(dlgCfg.ignoreErrors);

    //transfer parameter ownership to GUI
    m_radioBtnErrorDialogShow  ->SetValue(false);
    m_radioBtnErrorDialogCancel->SetValue(false);

    switch (dlgCfg.batchExCfg.batchErrorDialog)
    {
        case BatchErrorDialog::SHOW:
            m_radioBtnErrorDialogShow->SetValue(true);
            break;
        case BatchErrorDialog::CANCEL:
            m_radioBtnErrorDialogCancel->SetValue(true);
            break;
    }

    m_checkBoxRunMinimized->SetValue(dlgCfg.batchExCfg.runMinimized);
    m_checkBoxAutoClose   ->SetValue(dlgCfg.batchExCfg.autoCloseSummary);
    setEnumVal(enumPostSyncAction_, *m_choicePostSyncAction, dlgCfg.batchExCfg.postSyncAction);
    logfileDir_->setPath(dlgCfg.batchExCfg.logFolderPathPhrase);

    //map single parameter "logfiles limit" to all three checkboxs and spin ctrl:
    m_checkBoxSaveLog      ->SetValue(dlgCfg.batchExCfg.logfilesCountLimit != 0);
    m_checkBoxLogfilesLimit->SetValue(dlgCfg.batchExCfg.logfilesCountLimit > 0);
    m_spinCtrlLogfileLimit ->SetValue(dlgCfg.batchExCfg.logfilesCountLimit > 0 ? dlgCfg.batchExCfg.logfilesCountLimit : 100 /*XmlBatchConfig().logfilesCountLimit*/);
    //attention: emits a "change value" event!! => updateGui() called implicitly!

    updateGui(); //re-evaluate gui after config changes
}


BatchDialogConfig BatchDialog::getConfig() const
{
    BatchDialogConfig dlgCfg = {};

    dlgCfg.ignoreErrors = m_checkBoxIgnoreErrors->GetValue();

    dlgCfg.batchExCfg.batchErrorDialog    = m_radioBtnErrorDialogCancel->GetValue() ? BatchErrorDialog::CANCEL : BatchErrorDialog::SHOW;
    dlgCfg.batchExCfg.runMinimized        = m_checkBoxRunMinimized->GetValue();
    dlgCfg.batchExCfg.autoCloseSummary    = m_checkBoxAutoClose   ->GetValue();
    dlgCfg.batchExCfg.postSyncAction = getEnumVal(enumPostSyncAction_, *m_choicePostSyncAction);
    dlgCfg.batchExCfg.logFolderPathPhrase = utfTo<Zstring>(logfileDir_->getPath());

    dlgCfg.batchExCfg.logfilesCountLimit  = m_checkBoxSaveLog->GetValue() ? (m_checkBoxLogfilesLimit->GetValue() ? m_spinCtrlLogfileLimit->GetValue() : -1) : 0;
    //get single parameter "logfiles limit" from all three checkboxes and spin ctrl

    return dlgCfg;
}


void BatchDialog::onLocalKeyEvent(wxKeyEvent& event)
{
    event.Skip();
}


void BatchDialog::OnSaveBatchJob(wxCommandEvent& event)
{
    dlgCfgOut_ = getConfig();
    EndModal(ReturnBatchConfig::BUTTON_SAVE_AS);
}
}


ReturnBatchConfig::ButtonPressed fff::showBatchConfigDialog(wxWindow* parent,
                                                            BatchExclusiveConfig& batchExCfg,
                                                            bool& ignoreErrors)
{
    BatchDialogConfig dlgCfg = { batchExCfg, ignoreErrors };

    BatchDialog batchDlg(parent, dlgCfg);

    const auto rv = static_cast<ReturnBatchConfig::ButtonPressed>(batchDlg.ShowModal());
    if (rv != ReturnBatchConfig::BUTTON_CANCEL)
    {
        batchExCfg   = dlgCfg.batchExCfg;
        ignoreErrors = dlgCfg.ignoreErrors;
    }
    return rv;
}

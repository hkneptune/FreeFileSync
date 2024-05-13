// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "batch_config.h"
#include <wx/wupdlock.h>
//#include <wx+/window_layout.h>
#include <wx+/image_resources.h>
#include <wx+/image_tools.h>
#include <wx+/choice_enum.h>
#include <wx+/popup_dlg.h>
#include "gui_generated.h"
//#include "folder_selector.h"


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
    void onClose       (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onCancel      (wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onSaveBatchJob(wxCommandEvent& event) override;

    void onToggleIgnoreErrors(wxCommandEvent& event) override { updateGui(); }
    void onToggleRunMinimized(wxCommandEvent& event) override
    {
        m_checkBoxAutoClose->SetValue(m_checkBoxRunMinimized->GetValue()); //usually user wants to change both
        updateGui();
    }

    void onLocalKeyEvent(wxKeyEvent& event);

    void updateGui(); //re-evaluate gui after config changes

    void setConfig(const BatchDialogConfig& batchCfg);
    BatchDialogConfig getConfig() const;

    //output-only parameters
    BatchDialogConfig& dlgCfgOut_;

    EnumDescrList<PostBatchAction> enumPostBatchAction_;
};

//###################################################################################################################################

BatchDialog::BatchDialog(wxWindow* parent, BatchDialogConfig& dlgCfg) :
    BatchDlgGenerated(parent),
    dlgCfgOut_(dlgCfg)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonSaveAs).setCancel(m_buttonCancel));

    m_staticTextHeader->SetLabelText(replaceCpy(m_staticTextHeader->GetLabelText(), L"%x", L"FreeFileSync.exe <" + _("configuration file") + L">.ffs_batch"));
    m_staticTextHeader->Wrap(dipToWxsize(520));

    setImage(*m_bitmapBatchJob, loadImage("cfg_batch"));

    enumPostBatchAction_.
    add(PostBatchAction::none,     L"").
    add(PostBatchAction::sleep,    _("System: Sleep")).
    add(PostBatchAction::shutdown, _("System: Shut down"));

    setConfig(dlgCfg);

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); }); //enable dialog-specific key events

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
    Center(); //needs to be re-applied after a dialog size change!

    m_buttonSaveAs->SetFocus();
}


void BatchDialog::updateGui() //re-evaluate gui after config changes
{
    const BatchDialogConfig dlgCfg = getConfig(); //resolve parameter ownership: some on GUI controls, others member variables

    setImage(*m_bitmapIgnoreErrors, greyScaleIfDisabled(loadImage("error_ignore_active"), dlgCfg.ignoreErrors));

    m_radioBtnErrorDialogShow  ->Enable(!dlgCfg.ignoreErrors);
    m_radioBtnErrorDialogCancel->Enable(!dlgCfg.ignoreErrors);

    setImage(*m_bitmapMinimizeToTray, greyScaleIfDisabled(loadImage("minimize_to_tray"), dlgCfg.batchExCfg.runMinimized));
}


void BatchDialog::setConfig(const BatchDialogConfig& dlgCfg)
{
    m_checkBoxIgnoreErrors->SetValue(dlgCfg.ignoreErrors);

    //transfer parameter ownership to GUI
    m_radioBtnErrorDialogShow  ->SetValue(false);
    m_radioBtnErrorDialogCancel->SetValue(false);

    switch (dlgCfg.batchExCfg.batchErrorHandling)
    {
        case BatchErrorHandling::showPopup:
            m_radioBtnErrorDialogShow->SetValue(true);
            break;
        case BatchErrorHandling::cancel:
            m_radioBtnErrorDialogCancel->SetValue(true);
            break;
    }

    m_checkBoxRunMinimized->SetValue(dlgCfg.batchExCfg.runMinimized);
    m_checkBoxAutoClose   ->SetValue(dlgCfg.batchExCfg.autoCloseSummary);
    setEnumVal(enumPostBatchAction_, *m_choicePostSyncAction, dlgCfg.batchExCfg.postBatchAction);

    updateGui(); //re-evaluate gui after config changes
}


BatchDialogConfig BatchDialog::getConfig() const
{
    return
    {
        .batchExCfg
        {
            .runMinimized        = m_checkBoxRunMinimized->GetValue(),
            .autoCloseSummary    = m_checkBoxAutoClose   ->GetValue(),
            .batchErrorHandling  = m_radioBtnErrorDialogCancel->GetValue() ? BatchErrorHandling::cancel : BatchErrorHandling::showPopup,
            .postBatchAction = getEnumVal(enumPostBatchAction_, *m_choicePostSyncAction),
        },
        .ignoreErrors = m_checkBoxIgnoreErrors->GetValue(),
    };
}


void BatchDialog::onLocalKeyEvent(wxKeyEvent& event)
{
    event.Skip();
}


void BatchDialog::onSaveBatchJob(wxCommandEvent& event)
{
    //BatchDialogConfig dlgCfg = getConfig();

    //------- parameter validation (BEFORE writing output!) -------

    //-------------------------------------------------------------

    dlgCfgOut_ = getConfig();
    EndModal(static_cast<int>(ConfirmationButton::accept));
}
}


ConfirmationButton fff::showBatchConfigDialog(wxWindow* parent,
                                              BatchExclusiveConfig& batchExCfg,
                                              bool& ignoreErrors)
{
    BatchDialogConfig dlgCfg = {batchExCfg, ignoreErrors};

    BatchDialog batchDlg(parent, dlgCfg);

    const auto rv = static_cast<ConfirmationButton>(batchDlg.ShowModal());
    if (rv == ConfirmationButton::accept)
    {
        batchExCfg   = dlgCfg.batchExCfg;
        ignoreErrors = dlgCfg.ignoreErrors;
    }
    return rv;
}

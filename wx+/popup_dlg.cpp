// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "popup_dlg.h"
#include <wx/app.h>
#include <wx/display.h>
#include <wx+/std_button_layout.h>
#include <wx+/font_size.h>
#include <wx+/image_resources.h>
#include "popup_dlg_generated.h"


using namespace zen;


namespace
{
void setBestInitialSize(wxTextCtrl& ctrl, const wxString& text, wxSize maxSize)
{
    const int scrollbarWidth = fastFromDIP(20);
    if (maxSize.x <= scrollbarWidth) //implicitly checks for non-zero, too!
        return;
    maxSize.x -= scrollbarWidth;

    int bestWidth = 0;
    int rowCount  = 0;
    int rowHeight = 0;

    auto evalLineExtent = [&](const wxSize& sz) -> bool //return true when done
    {
        if (sz.x > bestWidth)
            bestWidth = std::min(maxSize.x, sz.x);

        rowCount += numeric::integerDivideRoundUp(sz.x, maxSize.x); //integer round up: consider line-wraps!
        rowHeight = std::max(rowHeight, sz.y); //all rows *should* have same height

        return rowCount * rowHeight >= maxSize.y;
    };

    for (auto it = text.begin();;)
    {
        auto itEnd = std::find(it, text.end(), L'\n');
        wxString line(it, itEnd);
        if (line.empty())
            line = L" "; //GetTextExtent() returns (0, 0) for empty strings!

        wxSize sz = ctrl.GetTextExtent(line); //exactly gives row height, but does *not* consider newlines
        if (evalLineExtent(sz))
            break;

        if (itEnd == text.end())
            break;
        it = itEnd + 1;
    }

    const int rowGap = 0;
    const wxSize bestSize(bestWidth + scrollbarWidth, std::min(rowCount * (rowHeight + rowGap), maxSize.y));
    ctrl.SetMinSize(bestSize); //alas, SetMinClientSize() is just not working!
}
}


class zen::StandardPopupDialog : public PopupDialogGenerated
{
public:
    StandardPopupDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg,
                        const wxString& labelAccept,    //
                        const wxString& labelAcceptAll, //optional, except: if "decline" or "acceptAll" is passed, so must be "accept"
                        const wxString& labelDecline) : //
        PopupDialogGenerated(parent),
        checkBoxValue_(cfg.checkBoxValue),
        buttonToDisableWhenChecked_(cfg.buttonToDisableWhenChecked)
    {

        wxBitmap iconTmp;
        wxString titleTmp;
        switch (type)
        {
            case DialogInfoType::info:
                //"Information" is meaningless as caption text!
                //confirmation doesn't use info icon
                //iconTmp = getResourceImage(L"msg_info");
                break;
            case DialogInfoType::warning:
                iconTmp  = getResourceImage(L"msg_warning");
                titleTmp = _("Warning");
                break;
            case DialogInfoType::error:
                iconTmp  = getResourceImage(L"msg_error");
                titleTmp = _("Error");
                break;
        }
        if (cfg.icon.IsOk())
            iconTmp = cfg.icon;

        if (!cfg.title.empty())
            titleTmp = cfg.title;
        //-----------------------------------------------
        m_bitmapMsgType->SetBitmap(iconTmp);

        if (titleTmp.empty())
            SetTitle(wxTheApp->GetAppDisplayName());
        else
        {
            if (parent && parent->IsShownOnScreen())
                SetTitle(titleTmp);
            else
                SetTitle(wxTheApp->GetAppDisplayName() + SPACED_DASH + titleTmp);
        }

        int maxWidth  = fastFromDIP(500);
        int maxHeight = fastFromDIP(400); //try to determine better value based on actual display resolution:

        if (parent)
        {
            const int disPos = wxDisplay::GetFromWindow(parent); //window must be visible
            if (disPos != wxNOT_FOUND)
                maxHeight = wxDisplay(disPos).GetClientArea().GetHeight() *  2 / 3;
        }

        assert(!cfg.textMain.empty() || !cfg.textDetail.empty());
        if (!cfg.textMain.empty())
        {
            setMainInstructionFont(*m_staticTextMain);
            m_staticTextMain->SetLabel(cfg.textMain);
            m_staticTextMain->Wrap(maxWidth); //call *after* SetLabel()
        }
        else
            m_staticTextMain->Hide();

        if (!cfg.textDetail.empty())
        {
            wxString text;
            if (!cfg.textMain.empty())
                text += L"\n";
            text += trimCpy(cfg.textDetail) + L"\n"; //add empty top/bottom lines *instead* of using border space!
            setBestInitialSize(*m_textCtrlTextDetail, text, wxSize(maxWidth, maxHeight));
            m_textCtrlTextDetail->ChangeValue(text);
        }
        else
            m_textCtrlTextDetail->Hide();

        if (checkBoxValue_)
        {
            assert(contains(cfg.checkBoxLabel, L"&"));
            m_checkBoxCustom->SetLabel(cfg.checkBoxLabel);
            m_checkBoxCustom->SetValue(*checkBoxValue_);
        }
        else
            m_checkBoxCustom->Hide();

        Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(StandardPopupDialog::OnKeyPressed), nullptr, this); //dialog-specific local key events

        //------------------------------------------------------------------------------
        StdButtons stdBtns;
        stdBtns.setAffirmative(m_buttonAccept);
        if (labelAccept.empty()) //notification dialog
        {
            assert(labelAcceptAll.empty() && labelDecline.empty());
            m_buttonAccept->SetLabel(_("Close")); //UX Guide: use "Close" for errors, warnings and windows in which users can't make changes (no ampersand!)
            m_buttonAcceptAll->Hide();
            m_buttonDecline->Hide();
            m_buttonCancel->Hide();
        }
        else
        {
            assert(contains(labelAccept, L"&"));
            m_buttonAccept->SetLabel(labelAccept);
            stdBtns.setCancel(m_buttonCancel);

            if (labelDecline.empty()) //confirmation dialog(YES/CANCEL)
                m_buttonDecline->Hide();
            else //confirmation dialog(YES/NO/CANCEL)
            {
                assert(contains(labelDecline, L"&"));
                m_buttonDecline->SetLabel(labelDecline);
                stdBtns.setNegative(m_buttonDecline);

                //m_buttonConfirm->SetId(wxID_IGNORE); -> setting id after button creation breaks "mouse snap to" functionality
                //m_buttonDecline->SetId(wxID_RETRY);  -> also wxWidgets docs seem to hide some info: "Normally, the identifier should be provided on creation and should not be modified subsequently."
            }

            if (labelAcceptAll.empty())
                m_buttonAcceptAll->Hide();
            else
            {
                assert(contains(labelAcceptAll, L"&"));
                m_buttonAcceptAll->SetLabel(labelAcceptAll);
                stdBtns.setAffirmativeAll(m_buttonAcceptAll);
            }
        }
        updateGui();

        //set std order after button visibility was set
        setStandardButtonLayout(*bSizerStdButtons, stdBtns);

        GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
        Center(); //needs to be re-applied after a dialog size change!

        m_buttonAccept->SetFocus();
    }

private:
    void OnClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton3::cancel)); }
    void OnCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton3::cancel)); }

    void OnButtonAccept(wxCommandEvent& event) override
    {
        if (checkBoxValue_)
            *checkBoxValue_ = m_checkBoxCustom->GetValue();
        EndModal(static_cast<int>(ConfirmationButton3::accept));
    }

    void OnButtonAcceptAll(wxCommandEvent& event) override
    {
        if (checkBoxValue_)
            *checkBoxValue_ = m_checkBoxCustom->GetValue();
        EndModal(static_cast<int>(ConfirmationButton3::acceptAll));
    }

    void OnButtonDecline(wxCommandEvent& event) override
    {
        if (checkBoxValue_)
            *checkBoxValue_ = m_checkBoxCustom->GetValue();
        EndModal(static_cast<int>(ConfirmationButton3::decline));
    }

    void OnKeyPressed(wxKeyEvent& event)
    {
        switch (event.GetKeyCode())
        {
            case WXK_RETURN:
            case WXK_NUMPAD_ENTER:
            {
                wxCommandEvent dummy(wxEVT_COMMAND_BUTTON_CLICKED);
                OnButtonAccept(dummy);
                return;
            }

            case WXK_ESCAPE: //handle case where cancel button is hidden!
                EndModal(static_cast<int>(ConfirmationButton3::cancel));
                return;
        }
        event.Skip();
    }

    void OnCheckBoxClick(wxCommandEvent& event) override { updateGui(); event.Skip(); }

    void updateGui()
    {
        switch (buttonToDisableWhenChecked_)
        {
            case QuestionButton2::yes:
                m_buttonAccept   ->Enable(!m_checkBoxCustom->GetValue());
                m_buttonAcceptAll->Enable(!m_checkBoxCustom->GetValue());
                break;
            case QuestionButton2::no:
                m_buttonDecline->Enable(!m_checkBoxCustom->GetValue());
                break;
            case QuestionButton2::cancel:
                break;
        }
    }

    bool* checkBoxValue_;
    const QuestionButton2 buttonToDisableWhenChecked_;
};

//########################################################################################

void zen::showNotificationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg)
{
    StandardPopupDialog dlg(parent, type, cfg, wxString() /*labelAccept*/, wxString() /*labelAcceptAll*/, wxString() /*labelDecline*/);
    dlg.ShowModal();
}


ConfirmationButton zen::showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelAccept)
{
    StandardPopupDialog dlg(parent, type, cfg, labelAccept, wxString() /*labelAcceptAll*/, wxString() /*labelDecline*/);
    return static_cast<ConfirmationButton>(dlg.ShowModal());
}


ConfirmationButton2 zen::showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelAccept, const wxString& labelAcceptAll)
{
    StandardPopupDialog dlg(parent, type, cfg, labelAccept, labelAcceptAll, wxString() /*labelDecline*/);
    return static_cast<ConfirmationButton2>(dlg.ShowModal());
}


ConfirmationButton3 zen::showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelAccept, const wxString& labelAcceptAll, const wxString& labelDecline)
{
    StandardPopupDialog dlg(parent, type, cfg, labelAccept, labelAcceptAll, labelDecline);
    return static_cast<ConfirmationButton3>(dlg.ShowModal());
}


QuestionButton2 zen::showQuestionDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelYes, const wxString& labelNo)
{
    StandardPopupDialog dlg(parent, type, cfg, labelYes, wxString() /*labelAcceptAll*/, labelNo);
    return static_cast<QuestionButton2>(dlg.ShowModal());
}

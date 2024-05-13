// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "popup_dlg.h"
#include <zen/basic_math.h>
#include <zen/utf.h>
#include <wx/app.h>
#include <wx/display.h>
#include <wx/sound.h>
//#include "app_main.h"
#include "bitmap_button.h"
#include "no_flicker.h"
#include "window_layout.h"
#include "image_resources.h"
#include "popup_dlg_generated.h"
#include "taskbar.h"
#include "window_tools.h"


using namespace zen;


namespace
{
void setBestInitialSize(wxRichTextCtrl& ctrl, const wxString& text, wxSize maxSize)
{
    const int scrollbarWidth = dipToWxsize(25); /*not only scrollbar, but also left/right padding (on macOS)!
    better use slightly larger than exact value (Windows: 17, Linux(CentOS): 14, macOS: 25)
    => worst case: minor increase in rowCount (no big deal) + slightly larger bestSize.x (good!)  */

    if (maxSize.x <= scrollbarWidth) //implicitly checks for non-zero, too!
        return;

    const int rowGap = 0;
    int maxLineWidth = 0;
    int rowHeight = 0; //alternative: just call ctrl.GetCharHeight()!?
    int rowCount  = 0;
    bool haveLineWrap = false;

    auto evalLineExtent = [&](const wxSize& sz) -> bool //return true when done
    {
        assert(rowHeight == 0 || rowHeight == sz.y + rowGap); //all rows *should* have same height
        rowHeight    = std::max(rowHeight,    sz.y + rowGap);
        maxLineWidth = std::max(maxLineWidth, sz.x);

        const int wrappedRows = numeric::intDivCeil(sz.x, maxSize.x - scrollbarWidth); //round up: consider line-wraps!
        rowCount += wrappedRows;
        if (wrappedRows > 1)
            haveLineWrap = true;

        return rowCount * rowHeight >= maxSize.y;
    };

    for (auto it = text.begin();;)
    {
        auto itEnd = std::find(it, text.end(), L'\n');
        wxString line(it, itEnd);
        if (line.empty())
            line = L' '; //GetTextExtent() returns (0, 0) for empty strings!

        wxSize sz = ctrl.GetTextExtent(line); //exactly gives row height, but does *not* consider newlines
        if (evalLineExtent(sz))
            break;

        if (itEnd == text.end())
            break;
        it = itEnd + 1;
    }

    int extraWidth = 0;
    if (haveLineWrap) //compensate for trivial intDivCeil() not...
        extraWidth += ctrl.GetTextExtent(L"FreeFileSync").x / 2; //...understanding line wrap algorithm

    const wxSize bestSize(std::min(maxLineWidth + scrollbarWidth /*1*/+ extraWidth, maxSize.x),
                          std::min(rowHeight * (rowCount + 1 /*2*/), maxSize.y));
    //1: wxWidgets' layout algorithm sucks: e.g. shows scrollbar *nedlessly* => extra line wrap increases height => scrollbar suddenly *needed*: catch 22!
    //2: add some vertical space just for looks (*instead* of using border gap)! Extra space needed anyway to avoid scrollbars on Windows (2 px) and macOS (11 px)

    ctrl.SetMinSize(bestSize); //alas, SetMinClientSize() is just not working!
#if 0
    std::cout << "rowCount       " << rowCount << "\n" <<
              "maxLineWidth   " << maxLineWidth << "\n" <<
              "rowHeight      " << rowHeight << "\n" <<
              "haveLineWrap   " << haveLineWrap << "\n" <<
              "scrollbarWidth " << scrollbarWidth << "\n\n";
#endif
}
}


int zen::getTextCtrlHeight(wxTextCtrl& ctrl, double rowCount)
{
    const int rowHeight =
        ctrl.GetTextExtent(L"X").GetHeight();

    return std::round(
               2 +
               rowHeight * rowCount);
}


class zen::StandardPopupDialog : public PopupDialogGenerated
{
public:
    StandardPopupDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg,
                        const wxString& labelAccept,    //
                        const wxString& labelAccept2,   //optional, except: if "decline" or "accept2" is passed, so must be "accept"
                        const wxString& labelDecline) : //
        PopupDialogGenerated(parent),
        checkBoxValue_(cfg.checkBoxValue),
        buttonToDisableWhenChecked_(cfg.buttonToDisableWhenChecked)
    {

        if (type != DialogInfoType::info)
            try
            {
                taskbar_ = std::make_unique<Taskbar>(parent); //throw TaskbarNotAvailable
                switch (type)
                {
                    case DialogInfoType::info:
                        break;
                    case DialogInfoType::warning:
                        taskbar_->setStatus(Taskbar::Status::warning);
                        break;
                    case DialogInfoType::error:
                        taskbar_->setStatus(Taskbar::Status::error);
                        break;
                }
            }
            catch (TaskbarNotAvailable&) {}


        wxImage iconTmp;
        wxString titleTmp;
        switch (type)
        {
            case DialogInfoType::info:
                //"Information" is meaningless as caption text!
                //confirmation doesn't use info icon
                //iconTmp = loadImage("msg_info");
                break;
            case DialogInfoType::warning:
                iconTmp  = loadImage("msg_warning");
                titleTmp = _("Warning");
                break;
            case DialogInfoType::error:
                iconTmp  = loadImage("msg_error");
                titleTmp = _("Error");
                break;
        }
        if (cfg.icon.IsOk())
            iconTmp = cfg.icon;

        if (!cfg.title.empty())
            titleTmp = cfg.title;
        //-----------------------------------------------
        if (iconTmp.IsOk())
            setImage(*m_bitmapMsgType, iconTmp);

        if (!parent || !parent->IsShownOnScreen())
            titleTmp = wxTheApp->GetAppDisplayName() + (!titleTmp.empty() ? SPACED_DASH + titleTmp : wxString());
        SetTitle(titleTmp);

        int maxWidth  = dipToWxsize(500);
        int maxHeight = dipToWxsize(400); //try to determine better value based on actual display resolution:
        if (parent)
            if (const int disPos = wxDisplay::GetFromWindow(parent); //window must be visible
                disPos != wxNOT_FOUND)
                maxHeight = wxDisplay(disPos).GetClientArea().GetHeight() *  2 / 3;

        assert(!cfg.textMain.empty() || !cfg.textDetail.empty());
        if (!cfg.textMain.empty())
        {
            setMainInstructionFont(*m_staticTextMain);
            m_staticTextMain->SetLabelText(cfg.textMain);
            m_staticTextMain->Wrap(maxWidth); //call *after* SetLabel()
        }
        else
            m_staticTextMain->Hide();

        if (!cfg.textDetail.empty())
        {
            const wxString& text = trimCpy(cfg.textDetail);
            setBestInitialSize(*m_richTextDetail, text, wxSize(maxWidth, maxHeight));
            setTextWithUrls(*m_richTextDetail, text);
        }
        else
            m_richTextDetail->Hide();

        if (checkBoxValue_)
        {
            assert(contains(cfg.checkBoxLabel, L'&'));
            m_checkBoxCustom->SetLabel(cfg.checkBoxLabel);
            m_checkBoxCustom->SetValue(*checkBoxValue_);
        }
        else
            m_checkBoxCustom->Hide();

        Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); }); //dialog-specific local key events

        //play sound reminder when waiting for user confirmation
        if (!cfg.soundFileAlertPending.empty())
        {
            timer_.Bind(wxEVT_TIMER, [this, parent, alertSoundPath = cfg.soundFileAlertPending](wxTimerEvent& event)
            {
                //wxWidgets shows modal error dialog by default => "no, wxWidgets, NO!"
                wxLog* oldLogTarget = wxLog::SetActiveTarget(new wxLogStderr); //transfer and receive ownership!
                ZEN_ON_SCOPE_EXIT(delete wxLog::SetActiveTarget(oldLogTarget));

                wxSound::Play(utfTo<wxString>(alertSoundPath), wxSOUND_ASYNC);

                RequestUserAttention(wxUSER_ATTENTION_INFO);
                /*  wxUSER_ATTENTION_INFO:  flashes window 3 times, unconditionally
                    wxUSER_ATTENTION_ERROR: flashes without limit, but *only* if not in foreground (FLASHW_TIMERNOFG) :( */
                if (parent)
                    if (auto tlw = dynamic_cast<wxTopLevelWindow*>(&getRootWindow(*parent)))
                        tlw->RequestUserAttention(wxUSER_ATTENTION_INFO); //top-level window needed for the taskbar flash!
            });
            timer_.Start(60'000 /*unit: [ms]*/);
        }

        //------------------------------------------------------------------------------

        auto setButtonImage = [&](wxButton& button, ConfirmationButton3 btnType)
        {
            auto it = cfg.buttonImages.find(btnType);
            if (it != cfg.buttonImages.end())
                setImage(button, it->second); //caveat: image + text at the same time not working on GTK < 2.6
        };
        setButtonImage(*m_buttonAccept,  ConfirmationButton3::accept);
        setButtonImage(*m_buttonAccept2, ConfirmationButton3::accept2);
        setButtonImage(*m_buttonDecline, ConfirmationButton3::decline);
        setButtonImage(*m_buttonCancel,  ConfirmationButton3::cancel);


        if (cfg.disabledButtons.contains(ConfirmationButton3::accept )) m_buttonAccept ->Disable();
        if (cfg.disabledButtons.contains(ConfirmationButton3::accept2)) m_buttonAccept2->Disable();
        if (cfg.disabledButtons.contains(ConfirmationButton3::decline)) m_buttonDecline->Disable();
        assert(!cfg.disabledButtons.contains(ConfirmationButton3::cancel));
        assert(!cfg.disabledButtons.contains(cfg.buttonToDisableWhenChecked));


        StdButtons stdBtns;
        stdBtns.setAffirmative(m_buttonAccept);
        if (labelAccept.empty()) //notification dialog
        {
            assert(labelAccept2.empty() && labelDecline.empty());
            m_buttonAccept->SetLabel(_("Close")); //UX Guide: use "Close" for errors, warnings and windows in which users can't make changes (no ampersand!)
            m_buttonAccept2->Hide();
            m_buttonDecline->Hide();
            m_buttonCancel ->Hide();
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

            if (labelAccept2.empty())
                m_buttonAccept2->Hide();
            else
            {
                assert(contains(labelAccept2, L"&"));
                m_buttonAccept2->SetLabel(labelAccept2);
                stdBtns.setAffirmativeAll(m_buttonAccept2);
            }
        }
        //set std order after button visibility was set
        setStandardButtonLayout(*bSizerStdButtons, stdBtns);

        updateGui();


        GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
        Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
        //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
        Center(); //needs to be re-applied after a dialog size change!


        Raise(); //[!] popup may be triggered by ffs_batch job running in the background!

        if (m_buttonAccept->IsEnabled())
            m_buttonAccept->SetFocus();
        else if (m_buttonAccept2->IsEnabled())
            m_buttonAccept2->SetFocus();
        else
            m_buttonCancel->SetFocus();
    }

private:
    void onClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton3::cancel)); }
    void onCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton3::cancel)); }

    void onButtonAccept(wxCommandEvent& event) override
    {
        if (checkBoxValue_)
            *checkBoxValue_ = m_checkBoxCustom->GetValue();
        EndModal(static_cast<int>(ConfirmationButton3::accept));
    }

    void onButtonAccept2(wxCommandEvent& event) override
    {
        if (checkBoxValue_)
            *checkBoxValue_ = m_checkBoxCustom->GetValue();
        EndModal(static_cast<int>(ConfirmationButton3::accept2));
    }

    void onButtonDecline(wxCommandEvent& event) override
    {
        if (checkBoxValue_)
            *checkBoxValue_ = m_checkBoxCustom->GetValue();
        EndModal(static_cast<int>(ConfirmationButton3::decline));
    }

    void onLocalKeyEvent(wxKeyEvent& event)
    {
        switch (event.GetKeyCode())
        {

            case WXK_ESCAPE: //handle case where cancel button is hidden!
                EndModal(static_cast<int>(ConfirmationButton3::cancel));
                return;
        }
        event.Skip();
    }

    void onCheckBoxClick(wxCommandEvent& event) override { updateGui(); event.Skip(); }

    void updateGui()
    {
        switch (buttonToDisableWhenChecked_)
        {
            //*INDENT-OFF*
            case ConfirmationButton3::accept:  m_buttonAccept ->Enable(!m_checkBoxCustom->GetValue()); break;
            case ConfirmationButton3::accept2: m_buttonAccept2->Enable(!m_checkBoxCustom->GetValue()); break;
            case ConfirmationButton3::decline: m_buttonDecline->Enable(!m_checkBoxCustom->GetValue()); break;
            case ConfirmationButton3::cancel: break;
            //*INDENT-ON*
        }
    }

    bool* checkBoxValue_;
    const ConfirmationButton3 buttonToDisableWhenChecked_;
    std::unique_ptr<Taskbar> taskbar_;
    wxTimer timer_;
};

//########################################################################################

void zen::showNotificationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg)
{
    StandardPopupDialog dlg(parent, type, cfg, wxString() /*labelAccept*/, wxString() /*labelAccept2*/, wxString() /*labelDecline*/);
    dlg.ShowModal();
}


ConfirmationButton zen::showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelAccept)
{
    StandardPopupDialog dlg(parent, type, cfg, labelAccept, wxString() /*labelAccept2*/, wxString() /*labelDecline*/);
    return static_cast<ConfirmationButton>(dlg.ShowModal());
}


ConfirmationButton2 zen::showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelAccept, const wxString& labelAccept2)
{
    StandardPopupDialog dlg(parent, type, cfg, labelAccept, labelAccept2, wxString() /*labelDecline*/);
    return static_cast<ConfirmationButton2>(dlg.ShowModal());
}


ConfirmationButton3 zen::showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelAccept, const wxString& labelAccept2, const wxString& labelDecline)
{
    StandardPopupDialog dlg(parent, type, cfg, labelAccept, labelAccept2, labelDecline);
    return static_cast<ConfirmationButton3>(dlg.ShowModal());
}


QuestionButton2 zen::showQuestionDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelYes, const wxString& labelNo)
{
    StandardPopupDialog dlg(parent, type, cfg, labelYes, wxString() /*labelAccept2*/, labelNo);
    return static_cast<QuestionButton2>(dlg.ShowModal());
}

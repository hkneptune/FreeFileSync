// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef POPUP_DLG_H_820780154723456
#define POPUP_DLG_H_820780154723456

#include <unordered_set>
#include <unordered_map>
#include <zen/zstring.h>
#include <wx/window.h>
#include <wx/image.h>
#include <wx/string.h>
#include <wx/textctrl.h>


namespace zen
{
//parent window, optional: support correct dialog placement above parent on multiple monitor systems
//this module requires error, warning and info image files in Icons.zip, see <wx+/image_resources.h>

enum class DialogInfoType
{
    info,
    warning,
    error,
};

enum class ConfirmationButton3
{
    cancel,
    accept,
    accept2,
    decline,
};
enum class ConfirmationButton
{
    cancel = static_cast<int>(ConfirmationButton3::cancel), //[!] Clang requires "static_cast"
    accept = static_cast<int>(ConfirmationButton3::accept), //
};
enum class ConfirmationButton2
{
    cancel  = static_cast<int>(ConfirmationButton3::cancel),
    accept  = static_cast<int>(ConfirmationButton3::accept),
    accept2 = static_cast<int>(ConfirmationButton3::accept2),
};
enum class QuestionButton2
{
    cancel = static_cast<int>(ConfirmationButton3::cancel),
    yes    = static_cast<int>(ConfirmationButton3::accept),
    no     = static_cast<int>(ConfirmationButton3::decline),
};

struct PopupDialogCfg;

void                showNotificationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg);
ConfirmationButton  showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelAccept);
ConfirmationButton2 showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelAccept, const wxString& labelAccept2);
ConfirmationButton3 showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelAccept, const wxString& labelAccept2, const wxString& labelDecline);
QuestionButton2     showQuestionDialog    (wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelYes,    const wxString& labelNo);

//----------------------------------------------------------------------------------------------------------------
class StandardPopupDialog;

struct PopupDialogCfg
{
    PopupDialogCfg& setIcon              (const  wxImage& bmp  ) { icon       = bmp;   return *this; }
    PopupDialogCfg& setTitle             (const wxString& label) { title      = label; return *this; }
    PopupDialogCfg& setMainInstructions  (const wxString& label) { textMain   = label; return *this; } //set at least one of these!
    PopupDialogCfg& setDetailInstructions(const wxString& label) { textDetail = label; return *this; } //
    PopupDialogCfg& disableButton(ConfirmationButton3 button) { disabledButtons.insert(button); return *this; }
    PopupDialogCfg& setButtonImage(ConfirmationButton3 button, const wxImage& img) { buttonImages.emplace(button, img); return *this; }
    PopupDialogCfg& alertWhenPending(const Zstring& soundFilePath) { soundFileAlertPending = soundFilePath; return *this; }
    PopupDialogCfg& setCheckBox(bool& value, const wxString& label, ConfirmationButton3 disableWhenChecked = ConfirmationButton3::cancel)
    {
        checkBoxValue = &value;
        checkBoxLabel = label;
        buttonToDisableWhenChecked = disableWhenChecked;
        return *this;
    }

private:
    friend class StandardPopupDialog;

    wxImage  icon;
    wxString title;
    wxString textMain;
    wxString textDetail;
    std::unordered_set<ConfirmationButton3> disabledButtons;
    std::unordered_map<ConfirmationButton3, wxImage> buttonImages;
    Zstring soundFileAlertPending;
    bool* checkBoxValue = nullptr; //in/out
    wxString checkBoxLabel;
    ConfirmationButton3 buttonToDisableWhenChecked = ConfirmationButton3::cancel;
};


int getTextCtrlHeight(wxTextCtrl& ctrl, double rowCount);
}

#endif //POPUP_DLG_H_820780154723456

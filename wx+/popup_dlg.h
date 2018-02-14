// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef POPUP_DLG_H_820780154723456
#define POPUP_DLG_H_820780154723456

#include <wx/window.h>
#include <wx/bitmap.h>
#include <wx/string.h>


namespace zen
{
//parent window, optional: support correct dialog placement above parent on multiple monitor systems
//this module requires error, warning and info image files in resources.zip, see <wx+/image_resources.h>

struct PopupDialogCfg;

enum class DialogInfoType
{
    INFO,
    WARNING,
    ERROR2, //fuck the ERROR macro in WinGDI.h!
};

enum class ConfirmationButton3
{
    ACCEPT,
    ACCEPT_ALL,
    DECLINE,
    CANCEL,
};
enum class ConfirmationButton
{
    ACCEPT = static_cast<int>(ConfirmationButton3::ACCEPT), //[!] Clang requires a "static_cast"
    CANCEL = static_cast<int>(ConfirmationButton3::CANCEL), //
};
enum class ConfirmationButton2
{
    ACCEPT     = static_cast<int>(ConfirmationButton3::ACCEPT),
    ACCEPT_ALL = static_cast<int>(ConfirmationButton3::ACCEPT_ALL),
    CANCEL     = static_cast<int>(ConfirmationButton3::CANCEL),
};
enum class QuestionButton2
{
    YES    = static_cast<int>(ConfirmationButton3::ACCEPT),
    NO     = static_cast<int>(ConfirmationButton3::DECLINE),
    CANCEL = static_cast<int>(ConfirmationButton3::CANCEL),
};

void                showNotificationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg);
ConfirmationButton  showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelAccept);
ConfirmationButton2 showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelAccept, const wxString& labelAcceptAll);
ConfirmationButton3 showConfirmationDialog(wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelAccept, const wxString& labelAcceptAll, const wxString& labelDecline);
QuestionButton2     showQuestionDialog    (wxWindow* parent, DialogInfoType type, const PopupDialogCfg& cfg, const wxString& labelYes, const wxString& labelNo);

//----------------------------------------------------------------------------------------------------------------
class StandardPopupDialog;

struct PopupDialogCfg
{
    PopupDialogCfg& setIcon              (const wxBitmap& bmp  ) { icon       = bmp;   return *this; }
    PopupDialogCfg& setTitle             (const wxString& label) { title      = label; return *this; }
    PopupDialogCfg& setMainInstructions  (const wxString& label) { textMain   = label; return *this; } //set at least one of these!
    PopupDialogCfg& setDetailInstructions(const wxString& label) { textDetail = label; return *this; } //
    PopupDialogCfg& setCheckBox(bool& value, const wxString& label, QuestionButton2 disableWhenChecked = QuestionButton2::CANCEL)
    {
        checkBoxValue = &value;
        checkBoxLabel = label;
        buttonToDisableWhenChecked = disableWhenChecked;
        return *this;
    }

private:
    friend class StandardPopupDialog;

    wxBitmap icon;
    wxString title;
    wxString textMain;
    wxString textDetail;
    bool* checkBoxValue = nullptr; //in/out
    wxString checkBoxLabel;
    QuestionButton2 buttonToDisableWhenChecked = QuestionButton2::CANCEL;
};
}

#endif //POPUP_DLG_H_820780154723456

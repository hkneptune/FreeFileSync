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
//this module requires error, warning and info image files in Icons.zip, see <wx+/image_resources.h>

struct PopupDialogCfg;

enum class DialogInfoType
{
    info,
    warning,
    error,
};

enum class ConfirmationButton3
{
    accept,
    acceptAll,
    decline,
    cancel,
};
enum class ConfirmationButton
{
    accept = static_cast<int>(ConfirmationButton3::accept), //[!] Clang requires a "static_cast"
    cancel = static_cast<int>(ConfirmationButton3::cancel), //
};
enum class ConfirmationButton2
{
    accept    = static_cast<int>(ConfirmationButton3::accept),
    acceptAll = static_cast<int>(ConfirmationButton3::acceptAll),
    cancel    = static_cast<int>(ConfirmationButton3::cancel),
};
enum class QuestionButton2
{
    yes    = static_cast<int>(ConfirmationButton3::accept),
    no     = static_cast<int>(ConfirmationButton3::decline),
    cancel = static_cast<int>(ConfirmationButton3::cancel),
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
    PopupDialogCfg& setCheckBox(bool& value, const wxString& label, QuestionButton2 disableWhenChecked = QuestionButton2::cancel)
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
    QuestionButton2 buttonToDisableWhenChecked = QuestionButton2::cancel;
};
}

#endif //POPUP_DLG_H_820780154723456

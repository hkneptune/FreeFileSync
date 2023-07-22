// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "rename_dlg.h"
#include "gui_generated.h"
#include <wx+/window_layout.h>
#include <wx+/image_resources.h>
#include "../base/multi_rename.h"


using namespace zen;
using namespace fff;


namespace
{
enum class ColumnTypeRename
{
    oldName,
    newName,
};


class GridDataRename : public GridData
{
public:
    GridDataRename(const std::vector<std::wstring>& fileNamesOld,
                   const SharedRef<const RenameBuf>& renameBuf) :
        fileNamesOld_(fileNamesOld),
        renameBuf_(renameBuf) {}

    void updatePreview(const std::wstring& renamePhrase)
    {
        fileNamesNew_ = resolvePlaceholderPhrase(renamePhrase, renameBuf_.ref());
        assert(fileNamesNew_.size() == fileNamesOld_.size());
    }

    const std::vector<std::wstring>& getNewNames() const { return fileNamesNew_; }

    size_t getRowCount() const override { return fileNamesOld_.size(); }

    std::wstring getValue(size_t row, ColumnType colType) const override
    {
        if (row < fileNamesOld_.size())
            switch (static_cast<ColumnTypeRename>(colType))
            {
                case ColumnTypeRename::oldName:
                    return fileNamesOld_[row];

                case ColumnTypeRename::newName:
                    return fileNamesNew_[row];
            }
        return std::wstring();
    }

    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected, HoverArea rowHover) override
    {
        //clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); -> already the default
    }

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover) override
    {
        //draw border on right
        clearArea(dc, {rect.x + rect.width - fastFromDIP(1), rect.y, fastFromDIP(1), rect.height}, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));
        wxRect rectTmp = wxRect(rect.GetTopLeft(), wxSize(rect.width - fastFromDIP(1), rect.height));

        rectTmp.x     += getColumnGapLeft();
        rectTmp.width -= getColumnGapLeft();
        drawCellText(dc, rectTmp, getValue(row, colType));
    }

    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        // -> synchronize renderCell() <-> getBestSize()
        return dc.GetTextExtent(getValue(row, colType)).GetWidth() + 2 * getColumnGapLeft() + fastFromDIP(1); //gap on left and right side + border
    }

    std::wstring getToolTip(size_t row, ColumnType colType, HoverArea rowHover) override { return std::wstring(); }

    std::wstring getColumnLabel(ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeRename>(colType))
        {
            case ColumnTypeRename::oldName:
                return _("Old name");
            case ColumnTypeRename::newName:
                return _("New name");
        }
        //assert(false); may be ColumnType::none
        return std::wstring();
    }

private:
    const std::vector<std::wstring> fileNamesOld_;
    std::vector<std::wstring> fileNamesNew_;
    const SharedRef<const RenameBuf> renameBuf_;
};


class RenameDialog : public RenameDlgGenerated
{
public:
    RenameDialog(wxWindow* parent, const std::vector<std::wstring>& fileNamesOld, std::vector<Zstring>& fileNamesNew);

private:
    void onTypingName(wxCommandEvent& event) override { updatePreview(); }
    void onOkay      (wxCommandEvent& event) override;
    void onCancel    (wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onClose     (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }

    void onLocalKeyEvent(wxKeyEvent& event);

    void updatePreview()
    {
        getDataView().updatePreview(copyStringTo<std::wstring>(trimCpy(m_textCtrlNewName->GetValue())));
        m_gridRenamePreview->Refresh();
    }

    GridDataRename& getDataView()
    {
        if (auto* prov = dynamic_cast<GridDataRename*>(m_gridRenamePreview->getDataProvider()))
            return *prov;
        throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] m_gridRenamePreview was not initialized.");
    }

    //output-only parameters:
    std::vector<Zstring>& fileNamesNewOut_;
};


RenameDialog::RenameDialog(wxWindow* parent,
                           const std::vector<std::wstring>& fileNamesOld,
                           std::vector<Zstring>& fileNamesNew) :
    RenameDlgGenerated(parent),
    fileNamesNewOut_(fileNamesNew)
{
    setStandardButtonLayout(*bSizerStdButtons, StdButtons().setAffirmative(m_buttonOK).setCancel(m_buttonCancel));

    setMainInstructionFont(*m_staticTextHeader);

    setImage(*m_bitmapRename, loadImage("rename"));

    m_staticTextHeader->SetLabelText(_P("Do you really want to rename the following item?",
                                        "Do you really want to rename the following %x items?", fileNamesOld.size()));

    m_staticTextHeader->Wrap(fastFromDIP(460)); //needs to be reapplied after SetLabel()

    m_buttonOK->SetLabelText(wxControl::RemoveMnemonics(_("&Rename"))); //no access key needed: use ENTER!

    const auto& [renamePhrase, renameBuf] = getPlaceholderPhrase(fileNamesOld);

    //-----------------------------------------------------------
    m_gridRenamePreview->showRowLabel(false);
    m_gridRenamePreview->setRowHeight(m_gridRenamePreview->getMainWin().GetCharHeight() + fastFromDIP(1) /*extra space*/);
    m_gridRenamePreview->setColumnConfig(
    {
        {static_cast<ColumnType>(ColumnTypeRename::oldName), 0, 1, true},
        {static_cast<ColumnType>(ColumnTypeRename::newName), 0, 1, true},
    });

    m_gridRenamePreview->setDataProvider(std::make_shared<GridDataRename>(fileNamesOld, renameBuf));

    warn_static("make smarter!")
    m_gridRenamePreview->SetMinSize({fastFromDIP(500), fastFromDIP(200)});
    //-----------------------------------------------------------

    m_textCtrlNewName->ChangeValue(renamePhrase);

    updatePreview();

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); }); //enable dialog-specific key events

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    Hide(); //avoid old position flash when Center() moves window (asynchronously?)
#endif
    Center(); //needs to be re-applied after a dialog size change!

    m_textCtrlNewName->SetFocus(); //[!] required *before* SetSelection() on wxGTK

    //pre-select name part that user will most likely change
    assert(contains(renamePhrase, L'\u2776') == fileNamesOld.size() > 1);
    auto it = fileNamesOld.size() == 1 ?
              findLast (renamePhrase.begin(), renamePhrase.end(), L'.') : //select everything except file extension
              std::find(renamePhrase.begin(), renamePhrase.end(), L'\u2776'); //❶
    if (it == renamePhrase.end())
        m_textCtrlNewName->SelectAll();
    else
        m_textCtrlNewName->SetSelection(0, static_cast<long>(it - renamePhrase.begin()));
}


void RenameDialog::onLocalKeyEvent(wxKeyEvent& event)
{
    event.Skip();
}


void RenameDialog::onOkay(wxCommandEvent& event)
{
    fileNamesNewOut_.clear();
    for (const std::wstring& newName : getDataView().getNewNames())
        fileNamesNewOut_.push_back(utfTo<Zstring>(newName));

    EndModal(static_cast<int>(ConfirmationButton::accept));
}
}


ConfirmationButton fff::showRenameDialog(wxWindow* parent,
                                         const std::vector<Zstring>& fileNamesOld,
                                         std::vector<Zstring>& fileNamesNew)
{
    std::vector<std::wstring> namesOld;
    for (const Zstring& name : fileNamesOld)
        namesOld.push_back(utfTo<std::wstring>(name));

    RenameDialog dlg(parent, namesOld, fileNamesNew);
    return static_cast<ConfirmationButton>(dlg.ShowModal());
}

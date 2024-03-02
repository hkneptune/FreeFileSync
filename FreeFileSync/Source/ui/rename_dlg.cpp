// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "rename_dlg.h"
#include <chrono>
#include <wx/valtext.h>
#include <wx+/window_layout.h>
#include <wx+/image_resources.h>
#include "gui_generated.h"
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

    bool updatePreview(std::wstring_view renamePhrase, size_t selectBegin, size_t selectEnd) //support polling
    {
        //normalize input: trim and adapt selection
        {
            const std::wstring_view renamePhraseTrm = trimCpy(renamePhrase);

            if (selectBegin <= selectEnd && selectEnd <= renamePhrase.size())
            {
                selectBegin -= std::min(selectBegin, makeUnsigned(renamePhraseTrm.data() - renamePhrase.data())); //careful:
                selectEnd   -= std::min(selectEnd,   makeUnsigned(renamePhraseTrm.data() - renamePhrase.data())); //avoid underflow

                selectBegin = std::min(selectBegin, renamePhraseTrm.size());
                selectEnd   = std::min(selectEnd,   renamePhraseTrm.size());
            }
            else
            {
                assert(false);
                selectBegin = selectEnd = 0;
            }

            renamePhrase = renamePhraseTrm;
        }

        auto currentPhrase = std::make_tuple(renamePhrase, selectBegin, selectEnd);
        if (currentPhrase != lastUsedPhrase_) //only update when needed
        {
            lastUsedPhrase_ = currentPhrase;

            fileNamesNewSelectBefore_ = resolvePlaceholderPhrase(renamePhrase.substr(0, selectBegin), renameBuf_.ref());
            fileNamesNewSelected_     = resolvePlaceholderPhrase(renamePhrase.substr(selectBegin, selectEnd - selectBegin), renameBuf_.ref());
            fileNamesNewSelectAfter_  = resolvePlaceholderPhrase(renamePhrase.substr(selectEnd), renameBuf_.ref());

            assert(fileNamesNewSelectBefore_.size() == fileNamesOld_.size());
            assert(fileNamesNewSelected_    .size() == fileNamesOld_.size());
            assert(fileNamesNewSelectAfter_ .size() == fileNamesOld_.size());

            previewChangeTime_ = std::chrono::steady_clock::now();
            return true;
        }
        else
            return false;
    }

    std::vector<std::wstring> getNewNames() const { return resolvePlaceholderPhrase(std::get<std::wstring>(lastUsedPhrase_), renameBuf_.ref()); }

    size_t getRowCount() const override { return fileNamesOld_.size(); }

    std::wstring getValue(size_t row, ColumnType colType) const override
    {
        if (row < fileNamesOld_.size())
            switch (static_cast<ColumnTypeRename>(colType))
            {
                case ColumnTypeRename::oldName:
                    return fileNamesOld_[row];

                case ColumnTypeRename::newName:
                    return fileNamesNewSelectBefore_[row] + fileNamesNewSelected_[row] + fileNamesNewSelectAfter_[row];
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
        clearArea(dc, {rect.x + rect.width - dipToWxsize(1), rect.y, dipToWxsize(1), rect.height}, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));

        wxRect rectTmp = rect;
        rectTmp.x     += getColumnGapLeft();
        rectTmp.width -= getColumnGapLeft() + dipToWxsize(1);

        switch (static_cast<ColumnTypeRename>(colType))
        {
            case ColumnTypeRename::oldName:
                drawCellText(dc, rectTmp, getValue(row, colType));
                break;

            case ColumnTypeRename::newName:
            {
                const std::wstring& fulltext = fileNamesNewSelectBefore_[row] + fileNamesNewSelected_[row] + fileNamesNewSelectAfter_[row];
                //macOS: drawCellText() is not accurate for partial strings => draw full text + calculate deltas:
                const wxSize extentBefore   = dc.GetTextExtent(fileNamesNewSelectBefore_[row]);
                const wxSize extentFullText = dc.GetTextExtent(fulltext);

                drawCellText(dc, rectTmp, fulltext, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, &extentFullText);

                if (!fileNamesNewSelected_[row].empty()) //highlight text selection:
                {
                    const wxSize extentBeforeAndSel = dc.GetTextExtent(fileNamesNewSelectBefore_[row] + fileNamesNewSelected_[row]);

                    const wxRect rectSel{rectTmp.x + extentBefore.GetWidth(),
                                         rectTmp.y,
                                         extentBeforeAndSel.GetWidth() - extentBefore.GetWidth(),
                                         rectTmp.height};

                    clearArea(dc, rectSel, wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));

                    RecursiveDcClipper dummy(dc, rectSel);

                    wxDCTextColourChanger textColor(dc, wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT)); //accessibility: always set both foreground AND background colors!
                    drawCellText(dc, rectTmp, fulltext, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, &extentFullText); //draw everything: might fix partially cleared character
                }
                else //draw input cursor
                    if (showCursor_ || std::chrono::steady_clock::now() < previewChangeTime_ + std::chrono::milliseconds(400))
                    {
                        const wxRect rectLine{rectTmp.x + extentBefore.GetWidth(),
                                              rectTmp.y,
                                              dipToWxsize(1),
                                              rectTmp.height};
                        clearArea(dc, rectLine, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
                    }
            }
            break;
        }
    }

    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        // -> synchronize renderCell() <-> getBestSize()
        return dc.GetTextExtent(getValue(row, colType)).GetWidth() + 2 * getColumnGapLeft() + dipToWxsize(1); //gap on left and right side + border
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

    void setCursorShown(bool show) { showCursor_ = show; }

private:
    const std::vector<std::wstring> fileNamesOld_;

    std::tuple<std::wstring /*renamePhrase*/, size_t /*selectBegin*/, size_t /*selectEnd*/> lastUsedPhrase_;

    std::vector<std::wstring> fileNamesNewSelectBefore_{fileNamesOld_.size()};
    std::vector<std::wstring> fileNamesNewSelected_    {fileNamesOld_.size()};
    std::vector<std::wstring> fileNamesNewSelectAfter_ {fileNamesOld_.size()};

    bool showCursor_ = false;
    std::chrono::steady_clock::time_point previewChangeTime_ = std::chrono::steady_clock::now();

    const SharedRef<const RenameBuf> renameBuf_;
};


class RenameDialog : public RenameDlgGenerated
{
public:
    RenameDialog(wxWindow* parent, const std::vector<std::wstring>& fileNamesOld, std::vector<Zstring>& fileNamesNew);

private:
    void onOkay  (wxCommandEvent& event) override;
    void onCancel(wxCommandEvent& event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }
    void onClose (wxCloseEvent&   event) override { EndModal(static_cast<int>(ConfirmationButton::cancel)); }

    void onLocalKeyEvent(wxKeyEvent& event);

    void updatePreview()
    {
        const std::wstring renamePhrase = copyStringTo<std::wstring>(m_textCtrlNewName->GetValue());

        long selectBegin = 0;
        long selectEnd   = 0;
        m_textCtrlNewName->GetSelection(&selectBegin, &selectEnd);

        assert(selectBegin == m_textCtrlNewName->GetInsertionPoint()); //apparently this is true for all Win/macOS/Linux

        if (getDataView().updatePreview(renamePhrase, selectBegin, selectEnd))
            m_gridRenamePreview->Refresh();
    }

    GridDataRename& getDataView()
    {
        if (auto* prov = dynamic_cast<GridDataRename*>(m_gridRenamePreview->getDataProvider()))
            return *prov;
        throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] m_gridRenamePreview was not initialized.");
    }

    wxTimer timer_; //poll for text selection changes
    wxTimer timerCursor_; //second timer just for cursor blinking

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

    m_buttonOK->SetLabelText(wxControl::RemoveMnemonics(_("&Rename"))); //no access key needed: use ENTER!

    auto [renamePhrase, renameBuf] = getPlaceholderPhrase(fileNamesOld);
    const std::wstring renamePhraseOld = renamePhrase; //save copy *before* trimming

    trim(renamePhrase); //leading/trailing whitespace makes no sense for file names


    std::wstring placeholders;
    for (const wchar_t c : renamePhrase)
        if (isRenamePlaceholderChar(c))
            placeholders += c;

    m_staticTextPlaceholderDescription->SetLabelText(placeholders + L": " + m_staticTextPlaceholderDescription->GetLabelText());

    //-----------------------------------------------------------
    m_gridRenamePreview->setDataProvider(std::make_shared<GridDataRename>(fileNamesOld, renameBuf));
    m_gridRenamePreview->showRowLabel(false);
    m_gridRenamePreview->setRowHeight(m_gridRenamePreview->getMainWin().GetCharHeight() + dipToWxsize(1) /*extra space*/);

    //-----------------------------------------------------------
    if (fileNamesOld.size() > 1) //calculate reasonable default preview grid size
    {
        //quick and dirty: get (likely) maximum string width while avoiding excessive wxDC::GetTextExtent() calls
        std::vector<std::wstring> names = fileNamesOld;
        auto itMax10 = names.end() - std::min<size_t>(10, names.size()); //find the 10 longest strings according to std::wstring::size()
        if (itMax10 != names.begin())
            std::nth_element(names.begin(), itMax10, names.end(),
            /**/[](const std::wstring& lhs, const std::wstring& rhs) { return lhs.size() < rhs.size(); }); //complexity: O(n)

        wxMemoryDC dc; //the context used for bitmaps
        setScaleFactor(dc, getScreenDpiScale());
        dc.SetFont(m_gridRenamePreview->GetFont()); //the font parameter of GetTextExtent() is not evaluated on OS X, wxWidgets 2.9.5, so apply it to the DC directly!

        int maxStringWidth = 0;
        std::for_each(itMax10, names.end(), [&](const std::wstring& str)
        {
            maxStringWidth = std::max(maxStringWidth, dc.GetTextExtent(str).GetWidth());
        });

        const int defaultColWidthOld = maxStringWidth + 2 * GridData::getColumnGapLeft() + dipToWxsize(1) /*border*/ + dipToWxsize(10) /*extra space: less cramped*/;
        const int defaultColWidthNew = maxStringWidth + 2 * GridData::getColumnGapLeft() + dipToWxsize(1) /*border*/ + dipToWxsize(50) /*extra space: for longer new name*/;

        m_gridRenamePreview->setColumnConfig(
        {
            {static_cast<ColumnType>(ColumnTypeRename::oldName),  defaultColWidthOld, 0, true}, //"old name" is fixed =>
            {static_cast<ColumnType>(ColumnTypeRename::newName), -defaultColWidthOld, 1, true}, //stretch "new name" only
        });

        const int previewDefaultWidth = std::min(defaultColWidthOld + defaultColWidthNew + dipToWxsize(25), //scroll bar width (guess!)
                                                 dipToWxsize(900));

        const int previewDefaultHeight = std::min(m_gridRenamePreview->getColumnLabelHeight() +
                                                  static_cast<int>(fileNamesOld.size()) * m_gridRenamePreview->getRowHeight(),
                                                  dipToWxsize(400));

        m_gridRenamePreview->SetMinSize({previewDefaultWidth, previewDefaultHeight});

        m_staticTextHeader->Wrap(std::max(previewDefaultWidth, dipToWxsize(400))); //needs to be reapplied after SetLabel()
    }
    else //renaming single file
    {
        m_gridRenamePreview               ->Hide();
        m_staticlinePreview               ->Hide();
        m_staticTextPlaceholderDescription->Hide();

        wxMemoryDC dc; //the context used for bitmaps
        setScaleFactor(dc, getScreenDpiScale());
        dc.SetFont(m_textCtrlNewName->GetFont()); //the font parameter of GetTextExtent() is not evaluated on OS X, wxWidgets 2.9.5, so apply it to the DC directly!

        const int textCtrlDefaultWidth = std::min(dc.GetTextExtent(renamePhrase).GetWidth() + 20 /*borders (non-DIP!)*/ +
                                                  dipToWxsize(50) /*extra space: for longer new name*/,
                                                  dipToWxsize(900));
        m_textCtrlNewName->SetMinSize({textCtrlDefaultWidth, -1});

        m_staticTextHeader->Wrap(std::max(textCtrlDefaultWidth, dipToWxsize(400))); //needs to be reapplied after SetLabel()
    }
    //-----------------------------------------------------------

    m_textCtrlNewName->Bind(wxEVT_COMMAND_TEXT_UPDATED, [this, renamePhraseOld, needPreview = fileNamesOld.size() > 1](wxCommandEvent& event)
    {
        if (needPreview)
            updatePreview(); //(almost?) redundant, considering timer_ is doing the same!?

        //disable OK button, until user changes input
        const std::wstring renamePhraseNew = trimCpy(copyStringTo<std::wstring>(m_textCtrlNewName->GetValue()));
        m_buttonOK->Enable(!renamePhraseNew.empty() && renamePhraseNew != renamePhraseOld); //supports polling
    });

    wxTextValidator inputValidator(wxFILTER_EXCLUDE_CHAR_LIST);
    inputValidator.SetCharExcludes(LR"(<>:"/\|?*)"); //chars forbidden for file names (at least on Windows)
    //https://docs.microsoft.com/de-de/windows/win32/fileio/naming-a-file#naming-conventions
    m_textCtrlNewName->SetValidator(inputValidator);
    m_textCtrlNewName->SetValue(renamePhrase); //SetValue() generates a text change event, unlike ChangeValue()


    if (fileNamesOld.size() > 1)
    {
        timer_.Bind(wxEVT_TIMER, [this](wxTimerEvent& event) { updatePreview(); }); //poll to detect text selection changes
        timer_.Start(100 /*unit: [ms]*/);

        timerCursor_.Bind(wxEVT_TIMER, [this, show = true](wxTimerEvent& event) mutable //trigger blinking cursor
        {
            getDataView().setCursorShown(show);
            m_gridRenamePreview->Refresh();
            show = !show;
        });
        timerCursor_.Start(wxCaret::GetBlinkTime() /*unit: [ms]*/);
    }

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); }); //enable dialog-specific key events

    //-----------------------------------------------------------
    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
    Center(); //needs to be re-applied after a dialog size change!

    m_textCtrlNewName->SetFocus(); //[!] required *before* SetSelection() on wxGTK
    //-----------------------------------------------------------

    //macOS issue: the *whole* text control is selected by default, unless we SetSelection() *after* wxDialog::Show()!
    CallAfter([this, nameCount = fileNamesOld.size(), renamePhrase = renamePhrase]
    {
        //pre-select name part that user will most likely change
        //assert(contains(renamePhrase, L'\u2776') == nameCount > 1); -> fails, if user selects same item on left and right grid
        auto it = std::find_if(renamePhrase.begin(), renamePhrase.end(), isRenamePlaceholderChar); //❶
        if (it == renamePhrase.end())
            it = findLast(renamePhrase.begin(), renamePhrase.end(), L'.'); //select everything except file extension

        if (it == renamePhrase.end())
            m_textCtrlNewName->SelectAll();
        else
        {
            const long selectEnd = static_cast<long>(it - renamePhrase.begin());
            m_textCtrlNewName->SetSelection(0, selectEnd);
        }

        updatePreview(); //consider new selection
    });
}


void RenameDialog::onLocalKeyEvent(wxKeyEvent& event)
{
    event.Skip();
}


void RenameDialog::onOkay(wxCommandEvent& event)
{
    updatePreview(); //ensure GridDataRename::getNewNames() is current

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
        namesOld.push_back(utfTo<std::wstring>(getUnicodeNormalForm(name))); //[!] don't care about Normalization form differences!

    RenameDialog dlg(parent, namesOld, fileNamesNew);
    return static_cast<ConfirmationButton>(dlg.ShowModal());
}

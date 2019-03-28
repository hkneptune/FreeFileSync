// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "log_panel.h"
#include <wx/clipbrd.h>
#include <wx+/focus.h>
#include <wx+/image_resources.h>
#include <wx+/rtl.h>
#include <wx+/context_menu.h>
#include <wx+/popup_dlg.h>

using namespace zen;
using namespace fff;


namespace
{
inline wxColor getColorGridLine() { return { 192, 192, 192 }; } //light grey


inline
wxBitmap getImageButtonPressed(const wchar_t* name)
{
    return layOver(getResourceImage(L"msg_button_pressed"), getResourceImage(name));
}


inline
wxBitmap getImageButtonReleased(const wchar_t* name)
{
    return greyScale(getResourceImage(name)).ConvertToImage();
    //getResourceImage(utfTo<wxString>(name)).ConvertToImage().ConvertToGreyscale(1.0/3, 1.0/3, 1.0/3); //treat all channels equally!
    //brighten(output, 30);

    //moveImage(output, 1, 0); //move image right one pixel
    //return output;
}


enum class ColumnTypeMsg
{
    TIME,
    CATEGORY,
    TEXT,
};
}


//a vector-view on ErrorLog considering multi-line messages: prepare consumption by Grid
class fff::MessageView
{
public:
    MessageView(const std::shared_ptr<const ErrorLog>& log /*bound*/) : log_(log) {}

    size_t rowsOnView() const { return viewRef_.size(); }

    struct LogEntryView
    {
        time_t      time = 0;
        MessageType type = MSG_TYPE_INFO;
        Zstringw    messageLine;
        bool firstLine = false; //if LogEntry::message spans multiple rows
    };

    std::optional<LogEntryView> getEntry(size_t row) const
    {
        if (row < viewRef_.size())
        {
            const Line& line = viewRef_[row];

            LogEntryView output;
            output.time = line.logIt_->time;
            output.type = line.logIt_->type;
            output.messageLine = extractLine(line.logIt_->message, line.rowNumber_);
            output.firstLine = line.rowNumber_ == 0; //this is virtually always correct, unless first line of the original message is empty!
            return output;
        }
        return {};
    }

    void updateView(int includedTypes) //MSG_TYPE_INFO | MSG_TYPE_WARNING, etc. see error_log.h
    {
        viewRef_.clear();

        for (auto it = log_->begin(); it != log_->end(); ++it)
            if (it->type & includedTypes)
            {
                static_assert(std::is_same_v<GetCharTypeT<Zstringw>, wchar_t>);
                assert(!startsWith(it->message, L'\n'));

                size_t rowNumber = 0;
                bool lastCharNewline = true;
                for (const wchar_t c : it->message)
                    if (c == L'\n')
                    {
                        if (!lastCharNewline) //do not reference empty lines!
                            viewRef_.emplace_back(it, rowNumber);
                        ++rowNumber;
                        lastCharNewline = true;
                    }
                    else
                        lastCharNewline = false;

                if (!lastCharNewline)
                    viewRef_.emplace_back(it, rowNumber);
            }
    }

private:
    static Zstringw extractLine(const Zstringw& message, size_t textRow)
    {
        auto it1 = message.begin();
        for (;;)
        {
            auto it2 = std::find_if(it1, message.end(), [](wchar_t c) { return c == L'\n'; });
            if (textRow == 0)
                return it1 == message.end() ? Zstringw() : Zstringw(&*it1, it2 - it1); //must not dereference iterator pointing to "end"!

            if (it2 == message.end())
            {
                assert(false);
                return Zstringw();
            }

            it1 = it2 + 1; //skip newline
            --textRow;
        }
    }

    struct Line
    {
        Line(ErrorLog::const_iterator logIt, size_t rowNumber) : logIt_(logIt), rowNumber_(rowNumber) {}

        ErrorLog::const_iterator logIt_; //always bound!
        size_t rowNumber_; //LogEntry::message may span multiple rows
    };

    std::vector<Line> viewRef_; //partial view on log_
    /*          /|\
                 | updateView()
                 |                      */
    const std::shared_ptr<const ErrorLog> log_;
};

//-----------------------------------------------------------------------------
namespace
{
//Grid data implementation referencing MessageView
class GridDataMessages : public GridData
{
public:
    GridDataMessages(const std::shared_ptr<const ErrorLog>& log /*bound!*/) : msgView_(log) {}

    MessageView& getDataView() { return msgView_; }

    size_t getRowCount() const override { return msgView_.rowsOnView(); }

    std::wstring getValue(size_t row, ColumnType colType) const override
    {
        if (std::optional<MessageView::LogEntryView> entry = msgView_.getEntry(row))
            switch (static_cast<ColumnTypeMsg>(colType))
            {
                case ColumnTypeMsg::TIME:
                    if (entry->firstLine)
                        return formatTime<std::wstring>(FORMAT_TIME, getLocalTime(entry->time));
                    break;

                case ColumnTypeMsg::CATEGORY:
                    if (entry->firstLine)
                        switch (entry->type)
                        {
                            case MSG_TYPE_INFO:
                                return _("Info");
                            case MSG_TYPE_WARNING:
                                return _("Warning");
                            case MSG_TYPE_ERROR:
                                return _("Error");
                            case MSG_TYPE_FATAL_ERROR:
                                return _("Serious Error");
                        }
                    break;

                case ColumnTypeMsg::TEXT:
                    return copyStringTo<std::wstring>(entry->messageLine);
            }
        return std::wstring();
    }

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover) override
    {
        wxRect rectTmp = rect;

        //-------------- draw item separation line -----------------
        {
            wxDCPenChanger dummy2(dc, getColorGridLine());
            const bool drawBottomLine = [&] //don't separate multi-line messages
            {
                if (std::optional<MessageView::LogEntryView> nextEntry = msgView_.getEntry(row + 1))
                    return nextEntry->firstLine;
                return true;
            }();

            if (drawBottomLine)
            {
                dc.DrawLine(rect.GetBottomLeft(), rect.GetBottomRight() + wxPoint(1, 0));
                --rectTmp.height;
            }
        }
        //--------------------------------------------------------

        if (std::optional<MessageView::LogEntryView> entry = msgView_.getEntry(row))
            switch (static_cast<ColumnTypeMsg>(colType))
            {
                case ColumnTypeMsg::TIME:
                    drawCellText(dc, rectTmp, getValue(row, colType), wxALIGN_CENTER);
                    break;

                case ColumnTypeMsg::CATEGORY:
                    if (entry->firstLine)
                    {
                        wxBitmap msgTypeIcon = [&]
                        {
                            switch (entry->type)
                            {
                                case MSG_TYPE_INFO:
                                    return getResourceImage(L"msg_info_sicon");
                                case MSG_TYPE_WARNING:
                                    return getResourceImage(L"msg_warning_sicon");
                                case MSG_TYPE_ERROR:
                                case MSG_TYPE_FATAL_ERROR:
                                    return getResourceImage(L"msg_error_sicon");
                            }
                            assert(false);
                            return wxNullBitmap;
                        }();
                        drawBitmapRtlNoMirror(dc, enabled ? msgTypeIcon : msgTypeIcon.ConvertToDisabled(), rectTmp, wxALIGN_CENTER);
                    }
                    break;

                case ColumnTypeMsg::TEXT:
                    rectTmp.x     += getColumnGapLeft();
                    rectTmp.width -= getColumnGapLeft();
                    drawCellText(dc, rectTmp, getValue(row, colType));
                    break;
            }
    }

    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected) override
    {
        GridData::renderRowBackgound(dc, rect, row, true /*enabled*/, enabled && selected);
    }

    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        // -> synchronize renderCell() <-> getBestSize()

        if (msgView_.getEntry(row))
            switch (static_cast<ColumnTypeMsg>(colType))
            {
                case ColumnTypeMsg::TIME:
                    return 2 * getColumnGapLeft() + dc.GetTextExtent(getValue(row, colType)).GetWidth();

                case ColumnTypeMsg::CATEGORY:
                    return getResourceImage(L"msg_info_sicon").GetWidth();

                case ColumnTypeMsg::TEXT:
                    return getColumnGapLeft() + dc.GetTextExtent(getValue(row, colType)).GetWidth();
            }
        return 0;
    }

    static int getColumnTimeDefaultWidth(Grid& grid)
    {
        wxClientDC dc(&grid.getMainWin());
        dc.SetFont(grid.getMainWin().GetFont());
        return 2 * getColumnGapLeft() + dc.GetTextExtent(formatTime<wxString>(FORMAT_TIME)).GetWidth();
    }

    static int getColumnCategoryDefaultWidth()
    {
        return getResourceImage(L"msg_info_sicon").GetWidth();
    }

    static int getRowDefaultHeight(const Grid& grid)
    {
        return std::max(getResourceImage(L"msg_info_sicon").GetHeight(), grid.getMainWin().GetCharHeight() + fastFromDIP(2)) + 1; //+ some space + bottom border
    }

    std::wstring getToolTip(size_t row, ColumnType colType) const override
    {
        switch (static_cast<ColumnTypeMsg>(colType))
        {
            case ColumnTypeMsg::TIME:
            case ColumnTypeMsg::TEXT:
                break;

            case ColumnTypeMsg::CATEGORY:
                return getValue(row, colType);
        }
        return std::wstring();
    }

    std::wstring getColumnLabel(ColumnType colType) const override { return std::wstring(); }

private:
    MessageView msgView_;
};
}

//########################################################################################

void LogPanel::setLog(const std::shared_ptr<const ErrorLog>& log)
{
    std::shared_ptr<const zen::ErrorLog> newLog = log;
    if (!newLog)
    {
        auto placeHolderLog = std::make_shared<ErrorLog>();
        placeHolderLog->logMsg(_("No log entries"), MSG_TYPE_INFO);
        newLog = placeHolderLog;
    }

    const int errorCount   = newLog->getItemCount(MSG_TYPE_ERROR | MSG_TYPE_FATAL_ERROR);
    const int warningCount = newLog->getItemCount(MSG_TYPE_WARNING);
    const int infoCount    = newLog->getItemCount(MSG_TYPE_INFO);

    auto initButton = [](ToggleButton& btn, const wchar_t* imgName, const wxString& tooltip)
    {
        btn.init(getImageButtonPressed(imgName), getImageButtonReleased(imgName));
        btn.SetToolTip(tooltip);
    };

    initButton(*m_bpButtonErrors,   L"msg_error",   _("Error"  ) + L" (" + formatNumber(errorCount)   + L")");
    initButton(*m_bpButtonWarnings, L"msg_warning", _("Warning") + L" (" + formatNumber(warningCount) + L")");
    initButton(*m_bpButtonInfo,     L"msg_info",    _("Info"   ) + L" (" + formatNumber(infoCount)    + L")");

    m_bpButtonErrors  ->setActive(true);
    m_bpButtonWarnings->setActive(true);
    m_bpButtonInfo    ->setActive(errorCount + warningCount == 0);

    m_bpButtonErrors  ->Show(errorCount   != 0);
    m_bpButtonWarnings->Show(warningCount != 0);
    m_bpButtonInfo    ->Show(infoCount    != 0);

    //init grid, determine default sizes
    const int rowHeight           = GridDataMessages::getRowDefaultHeight(*m_gridMessages);
    const int colMsgTimeWidth     = GridDataMessages::getColumnTimeDefaultWidth(*m_gridMessages);
    const int colMsgCategoryWidth = GridDataMessages::getColumnCategoryDefaultWidth();

    m_gridMessages->setDataProvider(std::make_shared<GridDataMessages>(newLog));
    m_gridMessages->setColumnLabelHeight(0);
    m_gridMessages->showRowLabel(false);
    m_gridMessages->setRowHeight(rowHeight);
    m_gridMessages->setColumnConfig(
    {
        { static_cast<ColumnType>(ColumnTypeMsg::TIME    ), colMsgTimeWidth,                        0, true },
        { static_cast<ColumnType>(ColumnTypeMsg::CATEGORY), colMsgCategoryWidth,                    0, true },
        { static_cast<ColumnType>(ColumnTypeMsg::TEXT    ), -colMsgTimeWidth - colMsgCategoryWidth, 1, true },
    });

    //support for CTRL + C
    m_gridMessages->getMainWin().Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(LogPanel::onGridButtonEvent), nullptr, this);

    m_gridMessages->Connect(EVENT_GRID_MOUSE_RIGHT_UP, GridClickEventHandler(LogPanel::onMsgGridContext), nullptr, this);

    //enable dialog-specific key events
    Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(LogPanel::onLocalKeyEvent), nullptr, this);

    updateGrid();
}


MessageView& LogPanel::getDataView()
{
    if (auto* prov = dynamic_cast<GridDataMessages*>(m_gridMessages->getDataProvider()))
        return prov->getDataView();
    throw std::runtime_error(std::string(__FILE__) + "[" + numberTo<std::string>(__LINE__) + "] m_gridMessages was not initialized.");
}



void LogPanel::updateGrid()
{
    int includedTypes = 0;
    if (m_bpButtonErrors->isActive())
        includedTypes |= MSG_TYPE_ERROR | MSG_TYPE_FATAL_ERROR;

    if (m_bpButtonWarnings->isActive())
        includedTypes |= MSG_TYPE_WARNING;

    if (m_bpButtonInfo->isActive())
        includedTypes |= MSG_TYPE_INFO;

    getDataView().updateView(includedTypes); //update MVC "model"
    m_gridMessages->Refresh();          //update MVC "view"
}

void LogPanel::OnErrors(wxCommandEvent& event)
{
    m_bpButtonErrors->toggle();
    updateGrid();
}


void LogPanel::OnWarnings(wxCommandEvent& event)
{
    m_bpButtonWarnings->toggle();
    updateGrid();
}


void LogPanel::OnInfo(wxCommandEvent& event)
{
    m_bpButtonInfo->toggle();
    updateGrid();
}


void LogPanel::onGridButtonEvent(wxKeyEvent& event)
{
    int keyCode = event.GetKeyCode();

    if (event.ControlDown())
        switch (keyCode)
        {
            //case 'A': -> "select all" is already implemented by Grid!

            case 'C':
            case WXK_INSERT: //CTRL + C || CTRL + INS
                copySelectionToClipboard();
                return; // -> swallow event! don't allow default grid commands!
        }

    //else
    //switch (keyCode)
    //{
    //  case WXK_RETURN:
    //  case WXK_NUMPAD_ENTER:
    //      return;
    //}

    event.Skip(); //unknown keypress: propagate
}


void LogPanel::onMsgGridContext(GridClickEvent& event)
{
    const std::vector<size_t> selection = m_gridMessages->getSelectedRows();

    const size_t rowCount = [&]() -> size_t
    {
        if (auto prov = m_gridMessages->getDataProvider())
            return prov->getRowCount();
        return 0;
    }();

    ContextMenu menu;
    menu.addItem(_("Copy") + L"\tCtrl+C", [this] { copySelectionToClipboard(); }, nullptr, !selection.empty());
    menu.addSeparator();

    menu.addItem(_("Select all") + L"\tCtrl+A", [this] { m_gridMessages->selectAllRows(GridEventPolicy::ALLOW); }, nullptr, rowCount > 0);
    menu.popup(*m_gridMessages, event.mousePos_);
}


void LogPanel::onLocalKeyEvent(wxKeyEvent& event) //process key events without explicit menu entry :)
{
    if (processingKeyEventHandler_) //avoid recursion
    {
        event.Skip();
        return;
    }
    processingKeyEventHandler_ = true;
    ZEN_ON_SCOPE_EXIT(processingKeyEventHandler_ = false);


    const int keyCode = event.GetKeyCode();

    if (event.ControlDown())
        switch (keyCode)
        {
            case 'A':
                m_gridMessages->SetFocus();
                m_gridMessages->selectAllRows(GridEventPolicy::ALLOW);
                return; // -> swallow event! don't allow default grid commands!

                //case 'C': -> already implemented by "Grid" class
        }
    else
        switch (keyCode)
        {
            //redirect certain (unhandled) keys directly to grid!
            case WXK_UP:
            case WXK_DOWN:
            case WXK_LEFT:
            case WXK_RIGHT:
            case WXK_PAGEUP:
            case WXK_PAGEDOWN:
            case WXK_HOME:
            case WXK_END:

            case WXK_NUMPAD_UP:
            case WXK_NUMPAD_DOWN:
            case WXK_NUMPAD_LEFT:
            case WXK_NUMPAD_RIGHT:
            case WXK_NUMPAD_PAGEUP:
            case WXK_NUMPAD_PAGEDOWN:
            case WXK_NUMPAD_HOME:
            case WXK_NUMPAD_END:
                if (!isComponentOf(wxWindow::FindFocus(), m_gridMessages) && //don't propagate keyboard commands if grid is already in focus
                    m_gridMessages->IsEnabled())
                    if (wxEvtHandler* evtHandler = m_gridMessages->getMainWin().GetEventHandler())
                    {
                        m_gridMessages->SetFocus();

                        event.SetEventType(wxEVT_KEY_DOWN); //the grid event handler doesn't expect wxEVT_CHAR_HOOK!
                        evtHandler->ProcessEvent(event); //propagating event catched at wxTheApp to child leads to recursion, but we prevented it...
                        event.Skip(false); //definitively handled now!
                        return;
                    }
                break;
        }

    event.Skip();
}


void LogPanel::copySelectionToClipboard()
{
    try
    {
        Zstringw clipboardString; //guaranteed exponential growth, unlike wxString

        if (auto prov = m_gridMessages->getDataProvider())
        {
            std::vector<Grid::ColAttributes> colAttr = m_gridMessages->getColumnConfig();
            eraseIf(colAttr, [](const Grid::ColAttributes& ca) { return !ca.visible; });
            if (!colAttr.empty())
                for (size_t row : m_gridMessages->getSelectedRows())
                {
                    std::for_each(colAttr.begin(), --colAttr.end(),
                                  [&](const Grid::ColAttributes& ca)
                    {
                        clipboardString += copyStringTo<Zstringw>(prov->getValue(row, ca.type));
                        clipboardString += L'\t';
                    });
                    clipboardString += copyStringTo<Zstringw>(prov->getValue(row, colAttr.back().type));
                    clipboardString += L'\n';
                }
        }

        //finally write to clipboard
        if (!clipboardString.empty())
            if (wxClipboard::Get()->Open())
            {
                ZEN_ON_SCOPE_EXIT(wxClipboard::Get()->Close());
                wxClipboard::Get()->SetData(new wxTextDataObject(copyStringTo<wxString>(clipboardString))); //ownership passed
            }
    }
    catch (const std::bad_alloc& e)
    {
        showNotificationDialog(nullptr, DialogInfoType::ERROR2, PopupDialogCfg().setMainInstructions(_("Out of memory.") + L" " + utfTo<std::wstring>(e.what())));
    }
}

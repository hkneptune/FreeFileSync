// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "log_panel.h"
#include <wx/clipbrd.h>
#include <wx+/window_tools.h>
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
wxImage getImageButtonPressed(const char* imageName)
{
    return layOver(loadImage("msg_button_pressed"),
                   loadImage(imageName));
}


inline
wxImage getImageButtonReleased(const char* imageName)
{
    return greyScale(loadImage(imageName));
    //loadImage(utfTo<wxString>(imageName)).ConvertToGreyscale(1.0/3, 1.0/3, 1.0/3); //treat all channels equally!
    //brighten(output, 30);

    //moveImage(output, 1, 0); //move image right one pixel
    //return output;
}


enum class ColumnTypeLog
{
    time,
    severity,
    text,
};
}


//a vector-view on ErrorLog considering multi-line messages: prepare consumption by Grid
class fff::MessageView
{
public:
    MessageView(const SharedRef<const ErrorLog>& log) : log_(log) {}

    size_t rowsOnView() const { return viewRef_.size(); }

    struct LogEntryView
    {
        time_t      time = 0;
        MessageType type = MSG_TYPE_INFO;
        std::string_view messageLine;
        bool firstLine = false; //if LogEntry::message spans multiple rows
    };

    std::optional<LogEntryView> getEntry(size_t row) const
    {
        if (row < viewRef_.size())
        {
            const Line& line = viewRef_[row];

            LogEntryView output;
            output.time = line.logIt->time;
            output.type = line.logIt->type;
            output.messageLine = extractLine(line.logIt->message, line.row);
            output.firstLine = line.row == 0; //this is virtually always correct, unless first line of the original message is empty!
            return output;
        }
        return {};
    }

    void updateView(int includedTypes) //MSG_TYPE_INFO | MSG_TYPE_WARNING, etc. see error_log.h
    {
        viewRef_.clear();

        for (auto it = log_.ref().begin(); it != log_.ref().end(); ++it)
            if (it->type & includedTypes)
            {
                assert(!startsWith(it->message, '\n'));

                size_t rowNumber = 0;
                bool lastCharNewline = true;
                for (const char c : it->message)
                    if (c == '\n')
                    {
                        if (!lastCharNewline) //do not reference empty lines!
                            viewRef_.push_back({it, rowNumber});
                        ++rowNumber;
                        lastCharNewline = true;
                    }
                    else
                        lastCharNewline = false;

                if (!lastCharNewline)
                    viewRef_.push_back({it, rowNumber});
            }
    }

private:
    static std::string_view extractLine(const Zstringc& message, size_t textRow)
    {
        auto it1 = message.begin();
        for (;;)
        {
            auto it2 = std::find_if(it1, message.end(), [](char c) { return c == '\n'; });
            if (textRow == 0)
                return makeStringView(it1, it2 - it1);

            if (it2 == message.end())
            {
                assert(false);
                return makeStringView(it1, 0);
            }

            it1 = it2 + 1; //skip newline
            --textRow;
        }
    }

    struct Line
    {
        ErrorLog::const_iterator logIt; //always bound!
        size_t row; //LogEntry::message may span multiple rows
    };

    std::vector<Line> viewRef_; //partial view on log_
    /*          /|\
                 | updateView()
                 |                      */
    const SharedRef<const ErrorLog> log_;
};

//-----------------------------------------------------------------------------
namespace
{
//Grid data implementation referencing MessageView
class GridDataMessages : public GridData
{
public:
    GridDataMessages(const SharedRef<const ErrorLog>& log) : msgView_(log) {}

    MessageView& getDataView() { return msgView_; }

    size_t getRowCount() const override { return msgView_.rowsOnView(); }

    std::wstring getValue(size_t row, ColumnType colType) const override
    {
        if (const std::optional<MessageView::LogEntryView> entry = msgView_.getEntry(row))
            switch (static_cast<ColumnTypeLog>(colType))
            {
                case ColumnTypeLog::time:
                    if (entry->firstLine)
                        return utfTo<std::wstring>(formatTime(formatTimeTag, getLocalTime(entry->time)));
                    break;

                case ColumnTypeLog::severity:
                    if (entry->firstLine)
                        switch (entry->type)
                        {
                            case MSG_TYPE_INFO:
                                return _("Info");
                            case MSG_TYPE_WARNING:
                                return _("Warning");
                            case MSG_TYPE_ERROR:
                                return _("Error");
                        }
                    break;

                case ColumnTypeLog::text:
                    return utfTo<std::wstring>(entry->messageLine);
            }
        return std::wstring();
    }

    void renderRowBackgound(wxDC& dc, const wxRect& rect, size_t row, bool enabled, bool selected, HoverArea rowHover) override
    {
        if (!enabled || !selected)
            ; //clearArea(dc, rect, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); -> already the default
        else
            GridData::renderRowBackgound(dc, rect, row, true /*enabled*/, true /*selected*/, rowHover);

        //-------------- draw item separation line -----------------
        wxDCPenChanger dummy2(dc, wxPen(getColorGridLine(), fastFromDIP(1)));
        const bool drawBottomLine = [&] //don't separate multi-line messages
        {
            if (std::optional<MessageView::LogEntryView> nextEntry = msgView_.getEntry(row + 1))
                return nextEntry->firstLine;
            return true;
        }();

        if (drawBottomLine)
            dc.DrawLine(rect.GetBottomLeft(), rect.GetBottomRight() + wxPoint(1, 0));
        //--------------------------------------------------------
    }

    void renderCell(wxDC& dc, const wxRect& rect, size_t row, ColumnType colType, bool enabled, bool selected, HoverArea rowHover) override
    {
        wxDCTextColourChanger textColor(dc);
        if (enabled && selected) //accessibility: always set *both* foreground AND background colors!
            textColor.Set(*wxBLACK);

        wxRect rectTmp = rect;

        if (std::optional<MessageView::LogEntryView> entry = msgView_.getEntry(row))
            switch (static_cast<ColumnTypeLog>(colType))
            {
                case ColumnTypeLog::time:
                    drawCellText(dc, rectTmp, getValue(row, colType), wxALIGN_CENTER);
                    break;

                case ColumnTypeLog::severity:
                    if (entry->firstLine)
                    {
                        wxImage msgTypeIcon = [&]
                        {
                            switch (entry->type)
                            {
                                case MSG_TYPE_INFO:
                                    return loadImage("msg_info", getDefaultMenuIconSize());
                                case MSG_TYPE_WARNING:
                                    return loadImage("msg_warning", getDefaultMenuIconSize());
                                case MSG_TYPE_ERROR:
                                    return loadImage("msg_error", getDefaultMenuIconSize());
                            }
                            assert(false);
                            return wxNullImage;
                        }();
                        drawBitmapRtlNoMirror(dc, enabled ? msgTypeIcon : msgTypeIcon.ConvertToDisabled(), rectTmp, wxALIGN_CENTER);
                    }
                    break;

                case ColumnTypeLog::text:
                    rectTmp.x     += getColumnGapLeft();
                    rectTmp.width -= getColumnGapLeft();
                    drawCellText(dc, rectTmp, getValue(row, colType));
                    break;
            }
    }

    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        // -> synchronize renderCell() <-> getBestSize()

        if (msgView_.getEntry(row))
            switch (static_cast<ColumnTypeLog>(colType))
            {
                case ColumnTypeLog::time:
                    return 2 * getColumnGapLeft() + dc.GetTextExtent(getValue(row, colType)).GetWidth();

                case ColumnTypeLog::severity:
                    return getDefaultMenuIconSize();

                case ColumnTypeLog::text:
                    return getColumnGapLeft() + dc.GetTextExtent(getValue(row, colType)).GetWidth();
            }
        return 0;
    }

    static int getColumnTimeDefaultWidth(Grid& grid)
    {
        wxClientDC dc(&grid.getMainWin());
        dc.SetFont(grid.getMainWin().GetFont());
        return 2 * getColumnGapLeft() + dc.GetTextExtent(utfTo<wxString>(formatTime(formatTimeTag))).GetWidth();
    }

    static int getColumnSeverityDefaultWidth()
    {
        return getDefaultMenuIconSize();
    }

    static int getRowDefaultHeight(const Grid& grid)
    {
        return std::max(getDefaultMenuIconSize(), grid.getMainWin().GetCharHeight() + fastFromDIP(2)) + 1; //+ some space + bottom border
    }

    std::wstring getToolTip(size_t row, ColumnType colType, HoverArea rowHover) override
    {
        switch (static_cast<ColumnTypeLog>(colType))
        {
            case ColumnTypeLog::time:
            case ColumnTypeLog::text:
                break;

            case ColumnTypeLog::severity:
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

LogPanel::LogPanel(wxWindow* parent) : LogPanelGenerated(parent)
{
    const int rowHeight           = GridDataMessages::getRowDefaultHeight(*m_gridMessages);
    const int colMsgTimeWidth     = GridDataMessages::getColumnTimeDefaultWidth(*m_gridMessages);
    const int colMsgSeverityWidth = GridDataMessages::getColumnSeverityDefaultWidth();

    m_gridMessages->setColumnLabelHeight(0);
    m_gridMessages->showRowLabel(false);
    m_gridMessages->setRowHeight(rowHeight);
    m_gridMessages->setColumnConfig(
    {
        { static_cast<ColumnType>(ColumnTypeLog::time    ), colMsgTimeWidth,                        0, true },
        { static_cast<ColumnType>(ColumnTypeLog::severity), colMsgSeverityWidth,                    0, true },
        { static_cast<ColumnType>(ColumnTypeLog::text    ), -colMsgTimeWidth - colMsgSeverityWidth, 1, true },
    });

    //support for CTRL + C
    m_gridMessages->getMainWin().Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) { onGridButtonEvent(event); });

    m_gridMessages->Bind(EVENT_GRID_CONTEXT_MENU, [this](GridContextMenuEvent& event) { onMsgGridContext(event); });

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) { onLocalKeyEvent(event); }); //enable dialog-specific key events

    setLog(nullptr);
}


void LogPanel::setLog(const std::shared_ptr<const ErrorLog>& log)
{
    SharedRef<const ErrorLog> newLog = [&]
    {
        if (log)
            return SharedRef<const ErrorLog>(log);

        ErrorLog dummyLog;
        dummyLog.logMsg(_("No log entries"), MSG_TYPE_INFO);
        return makeSharedRef<const ErrorLog>(std::move(dummyLog));
    }();

    const ErrorLog::Stats logCount = newLog.ref().getStats();

    auto initButton = [](ToggleButton& btn, const char* imgName, const wxString& tooltip)
    {
        btn.init(getImageButtonPressed(imgName), getImageButtonReleased(imgName));
        btn.SetToolTip(tooltip);
    };

    initButton(*m_bpButtonErrors,   "msg_error",   _("Error"  ) + L" (" + formatNumber(logCount.error)   + L')');
    initButton(*m_bpButtonWarnings, "msg_warning", _("Warning") + L" (" + formatNumber(logCount.warning) + L')');
    initButton(*m_bpButtonInfo,     "msg_info",    _("Info"   ) + L" (" + formatNumber(logCount.info)    + L')');

    m_bpButtonErrors  ->setActive(true);
    m_bpButtonWarnings->setActive(true);
    m_bpButtonInfo    ->setActive(logCount.warning + logCount.error == 0);

    m_bpButtonErrors  ->Show(logCount.error   != 0);
    m_bpButtonWarnings->Show(logCount.warning != 0);
    m_bpButtonInfo    ->Show(logCount.info    != 0);

    m_gridMessages->setDataProvider(std::make_shared<GridDataMessages>(newLog));

    updateGrid();
}


MessageView& LogPanel::getDataView()
{
    if (auto* prov = dynamic_cast<GridDataMessages*>(m_gridMessages->getDataProvider()))
        return prov->getDataView();
    throw std::runtime_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] m_gridMessages was not initialized.");
}


void LogPanel::updateGrid()
{
    int includedTypes = 0;
    if (m_bpButtonErrors->isActive())
        includedTypes |= MSG_TYPE_ERROR;

    if (m_bpButtonWarnings->isActive())
        includedTypes |= MSG_TYPE_WARNING;

    if (m_bpButtonInfo->isActive())
        includedTypes |= MSG_TYPE_INFO;

    getDataView().updateView(includedTypes); //update MVC "model"
    m_gridMessages->Refresh();               //update MVC "view"
}

void LogPanel::onErrors(wxCommandEvent& event)
{
    m_bpButtonErrors->toggle();
    updateGrid();
}


void LogPanel::onWarnings(wxCommandEvent& event)
{
    m_bpButtonWarnings->toggle();
    updateGrid();
}


void LogPanel::onInfo(wxCommandEvent& event)
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


void LogPanel::onMsgGridContext(GridContextMenuEvent& event)
{
    const std::vector<size_t> selection = m_gridMessages->getSelectedRows();

    const size_t rowCount = [&]() -> size_t
    {
        if (auto prov = m_gridMessages->getDataProvider())
            return prov->getRowCount();
        return 0;
    }();

    ContextMenu menu;
    menu.addItem(_("Copy") + L"\tCtrl+C", [this] { copySelectionToClipboard(); }, wxNullImage, !selection.empty());
    menu.addSeparator();

    menu.addItem(_("Select all") + L"\tCtrl+A", [this] { m_gridMessages->selectAllRows(GridEventPolicy::allow); }, wxNullImage, rowCount > 0);
    menu.popup(*m_gridMessages);
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
                m_gridMessages->selectAllRows(GridEventPolicy::allow);
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
        std::wstring clipBuf; //guaranteed exponential growth, unlike wxString

        if (auto prov = m_gridMessages->getDataProvider())
        {
            std::vector<Grid::ColAttributes> colAttr = m_gridMessages->getColumnConfig();
            std::erase_if(colAttr, [](const Grid::ColAttributes& ca) { return !ca.visible; });
            if (!colAttr.empty())
                for (size_t row : m_gridMessages->getSelectedRows())
                {
                    std::for_each(colAttr.begin(), --colAttr.end(),
                                  [&](const Grid::ColAttributes& ca)
                    {
                        clipBuf += prov->getValue(row, ca.type);
                        clipBuf += L'\t';
                    });
                    clipBuf += prov->getValue(row, colAttr.back().type);
                    clipBuf += L'\n';
                }
        }

        //finally write to clipboard
        if (!clipBuf.empty())
            if (wxClipboard::Get()->Open())
            {
                ZEN_ON_SCOPE_EXIT(wxClipboard::Get()->Close());
                wxClipboard::Get()->SetData(new wxTextDataObject(std::move(clipBuf))); //ownership passed
            }
    }
    catch (const std::bad_alloc& e)
    {
        showNotificationDialog(nullptr, DialogInfoType::error, PopupDialogCfg().setMainInstructions(_("Out of memory.") + L' ' + utfTo<std::wstring>(e.what())));
    }
}

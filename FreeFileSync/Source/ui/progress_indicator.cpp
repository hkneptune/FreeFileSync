// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "progress_indicator.h"
#include <memory>
#include <wx/imaglist.h>
#include <wx/wupdlock.h>
#include <wx/sound.h>
#include <wx/clipbrd.h>
#include <wx/dcclient.h>
#include <wx/dataobj.h> //wxTextDataObject
#include <zen/basic_math.h>
#include <zen/format_unit.h>
#include <zen/scope_guard.h>
#include <wx+/grid.h>
#include <wx+/toggle_button.h>
#include <wx+/image_tools.h>
#include <wx+/graph.h>
#include <wx+/context_menu.h>
#include <wx+/no_flicker.h>
#include <wx+/font_size.h>
#include <wx+/std_button_layout.h>
#include <wx+/popup_dlg.h>
#include <wx+/image_resources.h>
#include <zen/file_access.h>
#include <zen/thread.h>
#include <wx+/rtl.h>
#include <wx+/choice_enum.h>
#include "gui_generated.h"
#include "../lib/ffs_paths.h"
#include "../lib/perf_check.h"
#include "tray_icon.h"
#include "taskbar.h"
#include "app_icon.h"


using namespace zen;
using namespace fff;


namespace
{
//window size used for statistics
const std::chrono::seconds WINDOW_REMAINING_TIME(60); //USB memory stick scenario can have drop outs of 40 seconds => 60 sec. window size handles it
const std::chrono::seconds WINDOW_BYTES_PER_SEC  (5); //

inline wxColor getColorGridLine() { return { 192, 192, 192 }; } //light grey

inline wxColor getColorBytes() { return { 111, 255,  99 }; } //light green
inline wxColor getColorItems() { return { 127, 147, 255 }; } //light blue

inline wxColor getColorBytesRim() { return { 20, 200,   0 }; } //medium green
inline wxColor getColorItemsRim() { return { 90, 120, 255 }; } //medium blue

inline wxColor getColorBytesBackground() { return { 205, 255, 202 }; } //faint green
inline wxColor getColorItemsBackground() { return { 198, 206, 255 }; } //faint blue

inline wxColor getColorBytesBackgroundRim() { return { 12, 128,   0 }; } //dark green
inline wxColor getColorItemsBackgroundRim() { return { 53,  25, 255 }; } //dark blue


//don't use wxStopWatch for long-running measurements: internally it uses ::QueryPerformanceCounter() which can overflow after only a few days:
//https://www.freefilesync.org/forum/viewtopic.php?t=1426
//    std::chrono::system_clock is not a steady clock, but at least doesn't overflow! (wraps ::GetSystemTimePreciseAsFileTime())
//    std::chrono::steady_clock also wraps ::QueryPerformanceCounter() => same flaw like wxStopWatch???

class StopWatch
{
public:
    bool isPaused() const { return paused_; }

    void pause()
    {
        if (!paused_)
        {
            paused_ = true;
            elapsedUntilPause_ += std::chrono::system_clock::now() - startTime_;
        }
    }

    void resume()
    {
        if (paused_)
        {
            paused_ = false;
            startTime_ = std::chrono::system_clock::now();
        }
    }

    void restart()
    {
        paused_ = false;
        startTime_ = std::chrono::system_clock::now();
        elapsedUntilPause_ = std::chrono::nanoseconds::zero();
    }

    std::chrono::nanoseconds elapsed() const
    {
        auto elapsedTotal = elapsedUntilPause_;
        if (!paused_)
            elapsedTotal += std::chrono::system_clock::now() - startTime_;
        return elapsedTotal;
    }

private:
    bool paused_ = false;
    std::chrono::system_clock::time_point startTime_ = std::chrono::system_clock::now();
    std::chrono::nanoseconds elapsedUntilPause_{}; //std::chrono::duration is uninitialized by default! WTF! When will this stupidity end???
};


std::wstring getDialogPhaseText(const Statistics* syncStat, bool paused, SyncProgressDialog::SyncResult finalResult)
{
    if (syncStat) //sync running
    {
        if (paused)
            return _("Paused");

        if (syncStat->getAbortStatus())
            return _("Stop requested...");
        else
            switch (syncStat->currentPhase())
            {
                case ProcessCallback::PHASE_NONE:
                    return _("Initializing..."); //dialog is shown *before* sync starts, so this text may be visible!
                case ProcessCallback::PHASE_SCANNING:
                    return _("Scanning...");
                case ProcessCallback::PHASE_COMPARING_CONTENT:
                    return _("Comparing content...");
                case ProcessCallback::PHASE_SYNCHRONIZING:
                    return _("Synchronizing...");
            }
    }
    else //sync finished
        switch (finalResult)
        {
            case SyncProgressDialog::RESULT_ABORTED:
                return _("Stopped");
            case SyncProgressDialog::RESULT_FINISHED_WITH_ERROR:
                return _("Completed with errors");
            case SyncProgressDialog::RESULT_FINISHED_WITH_WARNINGS:
                return _("Completed with warnings");
            case SyncProgressDialog::RESULT_FINISHED_WITH_SUCCESS:
                return _("Completed");
        }
    return std::wstring();
}


class CurveDataProgressBar : public CurveData
{
public:
    CurveDataProgressBar(bool drawTop) : drawTop_(drawTop) {}

    void setFraction(double fraction) { fraction_ = fraction; } //value between [0, 1]

private:
    std::pair<double, double> getRangeX() const override { return { 0, 1 }; }

    std::vector<CurvePoint> getPoints(double minX, double maxX, const wxSize& areaSizePx) const override
    {
        const double yHigh = drawTop_ ? 3 :  1; //draw partially out of vertical bounds to not render top/bottom borders of the bars
        const double yLow  = drawTop_ ? 1 : -1; //

        return
        {
            { 0,         yHigh },
            { fraction_, yHigh },
            { fraction_, yLow  },
            { 0,         yLow  },
        };
    }

    double fraction_ = 0;
    const bool drawTop_;
};

class CurveDataProgressSeparatorLine : public CurveData
{
    std::pair<double, double> getRangeX() const override { return { 0, 1 }; }

    std::vector<CurvePoint> getPoints(double minX, double maxX, const wxSize& areaSizePx) const override
    {
        return
        {
            { 0, 1 },
            { 1, 1 },
        };
    }
};
}


class CompareProgressDialog::Impl : public CompareProgressDlgGenerated
{
public:
    Impl(wxFrame& parentWindow);

    void init(const Statistics& syncStat, bool ignoreErrors); //constructor/destructor semantics, but underlying Window is reused
    void teardown();                                          //

    void initNewPhase();
    void updateProgressGui();

    bool getOptionIgnoreErrors() const            { return m_checkBoxIgnoreErrors->GetValue(); }
    void setOptionIgnoreErrors(bool ignoreErrors) { m_checkBoxIgnoreErrors->SetValue(ignoreErrors); updateStaticGui(); }

private:
    void OnToggleIgnoreErrors(wxCommandEvent& event) override { updateStaticGui(); }

    void updateStaticGui();

    wxFrame& parentWindow_;
    wxString parentTitleBackup_;

    StopWatch stopWatch_;
    std::chrono::nanoseconds binCompStart_{}; //begin of binary comparison phase

    const Statistics* syncStat_ = nullptr; //only bound while sync is running

    std::unique_ptr<Taskbar> taskbar_;
    std::unique_ptr<PerfCheck> perf_; //estimate remaining time

    std::chrono::nanoseconds timeLastSpeedEstimate_ = std::chrono::seconds(-100); //used for calculating intervals between showing and collecting perf samples
    //initial value: just some big number

    std::shared_ptr<CurveDataProgressBar> curveDataBytes_{ std::make_shared<CurveDataProgressBar>(true  /*drawTop*/) };
    std::shared_ptr<CurveDataProgressBar> curveDataItems_{ std::make_shared<CurveDataProgressBar>(false /*drawTop*/) };
};


CompareProgressDialog::Impl::Impl(wxFrame& parentWindow) :
    CompareProgressDlgGenerated(&parentWindow),
    parentWindow_(parentWindow)
{
    //make sure that standard height matches PHASE_COMPARING_CONTENT statistics layout
    m_staticTextItemsFoundLabel->Hide();
    m_staticTextItemsFound     ->Hide();

    //init graph
    m_panelProgressGraph->setAttributes(Graph2D::MainAttributes().setMinY(0).setMaxY(2).
                                        setLabelX(Graph2D::LABEL_X_NONE).
                                        setLabelY(Graph2D::LABEL_Y_NONE).
                                        setBackgroundColor(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE)).
                                        setSelectionMode(Graph2D::SELECT_NONE));

    m_panelProgressGraph->addCurve(curveDataBytes_, Graph2D::CurveAttributes().setLineWidth(1).fillPolygonArea(getColorBytes()).setColor(Graph2D::getBorderColor()));
    m_panelProgressGraph->addCurve(curveDataItems_, Graph2D::CurveAttributes().setLineWidth(1).fillPolygonArea(getColorItems()).setColor(Graph2D::getBorderColor()));

    m_panelProgressGraph->addCurve(std::make_shared<CurveDataProgressSeparatorLine>(), Graph2D::CurveAttributes().setLineWidth(1).setColor(Graph2D::getBorderColor()));

    m_panelStatistics->Layout();
    Layout();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
}


void CompareProgressDialog::Impl::init(const Statistics& syncStat, bool ignoreErrors)
{
    syncStat_ = &syncStat;
    parentTitleBackup_ = parentWindow_.GetTitle();

    try //try to get access to Windows 7/Ubuntu taskbar
    {
        taskbar_ = std::make_unique<Taskbar>(parentWindow_);
    }
    catch (const TaskbarNotAvailable&) {}

    //initialize progress indicator
    bSizerProgressGraph->Show(false);

    perf_.reset();
    stopWatch_.restart(); //measure total time

    //initially hide status that's relevant for comparing bytewise only
    m_staticTextItemsFoundLabel->Show();
    m_staticTextItemsFound     ->Show();

    m_staticTextItemsRemainingLabel->Hide();
    bSizerItemsRemaining           ->Show(false);

    m_staticTextTimeRemainingLabel->Hide();
    m_staticTextTimeRemaining     ->Hide();

    //allow changing a few options dynamically during sync
    m_checkBoxIgnoreErrors->SetValue(ignoreErrors);

    updateStaticGui();
    updateProgressGui();

    m_panelStatistics->Layout();
    Layout();
}


void CompareProgressDialog::Impl::teardown()
{
    syncStat_ = nullptr;
    parentWindow_.SetTitle(parentTitleBackup_);
    taskbar_.reset();
}


void CompareProgressDialog::Impl::initNewPhase()
{
    switch (syncStat_->currentPhase())
    {
        case ProcessCallback::PHASE_NONE:
            assert(false);
        case ProcessCallback::PHASE_SCANNING:
            break;

        case ProcessCallback::PHASE_COMPARING_CONTENT:
        case ProcessCallback::PHASE_SYNCHRONIZING:
            //start to measure perf
            perf_ = std::make_unique<PerfCheck>(WINDOW_REMAINING_TIME, WINDOW_BYTES_PER_SEC);
            timeLastSpeedEstimate_ = std::chrono::seconds(-100); //make sure estimate is updated upon next check

            binCompStart_ = stopWatch_.elapsed();

            bSizerProgressGraph->Show(true);

            //show status for comparing bytewise
            m_staticTextItemsFoundLabel->Hide();
            m_staticTextItemsFound     ->Hide();

            m_staticTextItemsRemainingLabel->Show();
            bSizerItemsRemaining           ->Show(true);

            m_staticTextTimeRemainingLabel->Show();
            m_staticTextTimeRemaining     ->Show();

            m_panelStatistics->Layout();
            Layout();
            break;
    }

    updateProgressGui();
}


void CompareProgressDialog::Impl::updateStaticGui()
{
    m_bitmapIgnoreErrors->SetBitmap(getResourceImage(m_checkBoxIgnoreErrors->GetValue() ? L"msg_error_medium_ignored" : L"msg_error_medium"));
}


void CompareProgressDialog::Impl::updateProgressGui()
{
    if (!syncStat_) //no comparison running!!
        return;

    auto setTitle = [&](const wxString& title)
    {
        if (parentWindow_.GetTitle() != title)
            parentWindow_.SetTitle(title);
    };

    bool layoutChanged = false; //avoid screen flicker by calling layout() only if necessary
    const std::chrono::nanoseconds timeElapsed = stopWatch_.elapsed();

    //status texts
    setText(*m_staticTextStatus, replaceCpy(syncStat_->currentStatusText(), L'\n', L' ')); //no layout update for status texts!

    //write status information to taskbar, parent title ect.
    switch (syncStat_->currentPhase())
    {
        case ProcessCallback::PHASE_NONE:
        case ProcessCallback::PHASE_SCANNING:
        {
            const wxString& scannedObjects = formatNumber(syncStat_->getItemsCurrent(ProcessCallback::PHASE_SCANNING));

            //dialog caption, taskbar
            setTitle(scannedObjects + SPACED_DASH + getDialogPhaseText(syncStat_, false /*paused*/, SyncProgressDialog::RESULT_ABORTED));
            if (taskbar_.get()) //support Windows 7 taskbar
                taskbar_->setStatus(Taskbar::STATUS_INDETERMINATE);

            //nr of scanned objects
            setText(*m_staticTextItemsFound, scannedObjects, &layoutChanged);
        }
        break;

        case ProcessCallback::PHASE_SYNCHRONIZING:
        case ProcessCallback::PHASE_COMPARING_CONTENT:
        {
            const int     itemsCurrent = syncStat_->getItemsCurrent(syncStat_->currentPhase());
            const int     itemsTotal   = syncStat_->getItemsTotal  (syncStat_->currentPhase());
            const int64_t bytesCurrent = syncStat_->getBytesCurrent(syncStat_->currentPhase());
            const int64_t bytesTotal   = syncStat_->getBytesTotal  (syncStat_->currentPhase());

            //add both bytes + item count, to handle "deletion-only" cases
            const double fractionTotal = bytesTotal + itemsTotal == 0 ? 0 : 1.0 * (bytesCurrent + itemsCurrent) / (bytesTotal + itemsTotal);
            const double fractionBytes = bytesTotal == 0 ? 0 : 1.0 * bytesCurrent / bytesTotal;
            const double fractionItems = itemsTotal == 0 ? 0 : 1.0 * itemsCurrent / itemsTotal;

            //dialog caption, taskbar
            setTitle(formatFraction(fractionTotal) + SPACED_DASH + getDialogPhaseText(syncStat_, false /*paused*/, SyncProgressDialog::RESULT_ABORTED));
            if (taskbar_.get())
            {
                taskbar_->setProgress(fractionTotal);
                taskbar_->setStatus(Taskbar::STATUS_NORMAL);
            }

            //progress indicator, shown for binary comparison only
            curveDataBytes_->setFraction(fractionBytes);
            curveDataItems_->setFraction(fractionItems);

            //remaining item and byte count
            setText(*m_staticTextItemsRemaining, formatNumber(itemsTotal - itemsCurrent), &layoutChanged);
            setText(*m_staticTextBytesRemaining, L"(" + formatFilesizeShort(bytesTotal - bytesCurrent) + L")", &layoutChanged);

            //remaining time and speed: only visible during binary comparison
            assert(perf_);
            if (perf_)
                if (numeric::dist(timeLastSpeedEstimate_, timeElapsed) >= std::chrono::milliseconds(500))
                {
                    timeLastSpeedEstimate_ = timeElapsed;

                    if (numeric::dist(binCompStart_, timeElapsed) >= std::chrono::seconds(1)) //discard stats for first second: probably messy
                        perf_->addSample(timeElapsed, itemsCurrent, bytesCurrent);

                    //current speed -> Win 7 copy uses 1 sec update interval instead
                    Opt<std::wstring> bps = perf_->getBytesPerSecond();
                    Opt<std::wstring> ips = perf_->getItemsPerSecond();
                    m_panelProgressGraph->setAttributes(m_panelProgressGraph->getAttributes().setCornerText(bps ? *bps : L"", Graph2D::CORNER_TOP_LEFT));
                    m_panelProgressGraph->setAttributes(m_panelProgressGraph->getAttributes().setCornerText(ips ? *ips : L"", Graph2D::CORNER_BOTTOM_LEFT));

                    //remaining time: display with relative error of 10% - based on samples taken every 0.5 sec only
                    //-> call more often than once per second to correctly show last few seconds countdown, but don't call too often to avoid occasional jitter
                    Opt<double> remTimeSec = perf_->getRemainingTimeSec(bytesTotal - bytesCurrent);
                    setText(*m_staticTextTimeRemaining, remTimeSec ? formatRemainingTime(*remTimeSec) : L"-", &layoutChanged);
                }

            m_panelProgressGraph->Refresh();
        }
        break;
    }

    const int64_t timeElapSec = std::chrono::duration_cast<std::chrono::seconds>(timeElapsed).count();

    setText(*m_staticTextTimeElapsed,
            timeElapSec < 3600 ?
            wxTimeSpan::Seconds(timeElapSec).Format(   L"%M:%S") :
            wxTimeSpan::Seconds(timeElapSec).Format(L"%H:%M:%S"), &layoutChanged);

    if (layoutChanged)
    {
        m_panelStatistics->Layout();
        Layout();
    }

    //do the ui update
    wxTheApp->Yield();
}

//########################################################################################

//redirect to implementation
CompareProgressDialog::CompareProgressDialog(wxFrame& parentWindow) : pimpl_(new Impl(parentWindow)) {} //owned by parentWindow
wxWindow* CompareProgressDialog::getAsWindow() { return pimpl_; }
void CompareProgressDialog::init(const Statistics& syncStat, bool ignoreErrors) { pimpl_->init(syncStat, ignoreErrors); }
void CompareProgressDialog::teardown()     { pimpl_->teardown(); }
void CompareProgressDialog::initNewPhase() { pimpl_->initNewPhase(); }
void CompareProgressDialog::updateGui()    { pimpl_->updateProgressGui(); }
bool CompareProgressDialog::getOptionIgnoreErrors() const { return pimpl_->getOptionIgnoreErrors(); }
void CompareProgressDialog::setOptionIgnoreErrors(bool ignoreErrors) { pimpl_->setOptionIgnoreErrors(ignoreErrors); }

//########################################################################################

namespace
{
//pretty much the same like "bool wxWindowBase::IsDescendant(wxWindowBase* child) const" but without the obvious misnomer
inline
bool isComponentOf(const wxWindow* child, const wxWindow* top)
{
    for (const wxWindow* wnd = child; wnd != nullptr; wnd = wnd->GetParent())
        if (wnd == top)
            return true;
    return false;
}


inline
wxBitmap getImageButtonPressed(const wchar_t* name)
{
    return layOver(getResourceImage(L"log button pressed"), getResourceImage(name));
}


inline
wxBitmap getImageButtonReleased(const wchar_t* name)
{
    return greyScale(getResourceImage(name)).ConvertToImage();
    //getResourceImage(utfTo<wxString>(name)).ConvertToImage().ConvertToGreyscale(1.0/3, 1.0/3, 1.0/3); //treat all channels equally!
    //brighten(output, 30);

    //zen::moveImage(output, 1, 0); //move image right one pixel
    //return output;
}


//a vector-view on ErrorLog considering multi-line messages: prepare consumption by Grid
class MessageView
{
public:
    MessageView(const ErrorLog& log) : log_(log) {}

    size_t rowsOnView() const { return viewRef_.size(); }

    struct LogEntryView
    {
        time_t      time = 0;
        MessageType type = MSG_TYPE_INFO;
        MsgString   messageLine;
        bool firstLine = false; //if LogEntry::message spans multiple rows
    };

    Opt<LogEntryView> getEntry(size_t row) const
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
        return NoValue();
    }

    void updateView(int includedTypes) //MSG_TYPE_INFO | MSG_TYPE_WARNING, ect. see error_log.h
    {
        viewRef_.clear();

        for (auto it = log_.begin(); it != log_.end(); ++it)
            if (it->type & includedTypes)
            {
                static_assert(IsSameType<GetCharType<MsgString>::Type, wchar_t>::value, "");
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
    static MsgString extractLine(const MsgString& message, size_t textRow)
    {
        auto it1 = message.begin();
        for (;;)
        {
            auto it2 = std::find_if(it1, message.end(), [](wchar_t c) { return c == L'\n'; });
            if (textRow == 0)
                return it1 == message.end() ? MsgString() : MsgString(&*it1, it2 - it1); //must not dereference iterator pointing to "end"!

            if (it2 == message.end())
            {
                assert(false);
                return MsgString();
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
    const ErrorLog log_;
};

//-----------------------------------------------------------------------------

enum class ColumnTypeMsg
{
    TIME,
    CATEGORY,
    TEXT,
};

//Grid data implementation referencing MessageView
class GridDataMessages : public GridData
{
public:
    GridDataMessages(const ErrorLog& log) : msgView_(log) {}

    MessageView& getDataView() { return msgView_; }

    size_t getRowCount() const override { return msgView_.rowsOnView(); }

    std::wstring getValue(size_t row, ColumnType colType) const override
    {
        if (Opt<MessageView::LogEntryView> entry = msgView_.getEntry(row))
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
                if (Opt<MessageView::LogEntryView> nextEntry = msgView_.getEntry(row + 1))
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

        if (Opt<MessageView::LogEntryView> entry = msgView_.getEntry(row))
            switch (static_cast<ColumnTypeMsg>(colType))
            {
                case ColumnTypeMsg::TIME:
                    drawCellText(dc, rectTmp, getValue(row, colType), wxALIGN_CENTER);
                    break;

                case ColumnTypeMsg::CATEGORY:
                    if (entry->firstLine)
                        switch (entry->type)
                        {
                            case MSG_TYPE_INFO:
                                drawBitmapRtlNoMirror(dc, getResourceImage(L"msg_info_small"), rectTmp, wxALIGN_CENTER);
                                break;
                            case MSG_TYPE_WARNING:
                                drawBitmapRtlNoMirror(dc, getResourceImage(L"msg_warning_small"), rectTmp, wxALIGN_CENTER);
                                break;
                            case MSG_TYPE_ERROR:
                            case MSG_TYPE_FATAL_ERROR:
                                drawBitmapRtlNoMirror(dc, getResourceImage(L"msg_error_small"), rectTmp, wxALIGN_CENTER);
                                break;
                        }
                    break;

                case ColumnTypeMsg::TEXT:
                    rectTmp.x     += COLUMN_GAP_LEFT;
                    rectTmp.width -= COLUMN_GAP_LEFT;
                    drawCellText(dc, rectTmp, getValue(row, colType));
                    break;
            }
    }

    int getBestSize(wxDC& dc, size_t row, ColumnType colType) override
    {
        // -> synchronize renderCell() <-> getBestSize()

        if (msgView_.getEntry(row))
            switch (static_cast<ColumnTypeMsg>(colType))
            {
                case ColumnTypeMsg::TIME:
                    return 2 * COLUMN_GAP_LEFT + dc.GetTextExtent(getValue(row, colType)).GetWidth();

                case ColumnTypeMsg::CATEGORY:
                    return getResourceImage(L"msg_info_small").GetWidth();

                case ColumnTypeMsg::TEXT:
                    return COLUMN_GAP_LEFT + dc.GetTextExtent(getValue(row, colType)).GetWidth();
            }
        return 0;
    }

    static int getColumnTimeDefaultWidth(Grid& grid)
    {
        wxClientDC dc(&grid.getMainWin());
        dc.SetFont(grid.getMainWin().GetFont());
        return 2 * COLUMN_GAP_LEFT + dc.GetTextExtent(formatTime<wxString>(FORMAT_TIME)).GetWidth();
    }

    static int getColumnCategoryDefaultWidth()
    {
        return getResourceImage(L"msg_info_small").GetWidth();
    }

    static int getRowDefaultHeight(const Grid& grid)
    {
        return std::max(getResourceImage(L"msg_info_small").GetHeight(), grid.getMainWin().GetCharHeight() + 2) + 1; //+ some space + bottom border
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


class LogPanel : public LogPanelGenerated
{
public:
    LogPanel(wxWindow* parent, const ErrorLog& log) : LogPanelGenerated(parent)
    {
        const int errorCount   = log.getItemCount(MSG_TYPE_ERROR | MSG_TYPE_FATAL_ERROR);
        const int warningCount = log.getItemCount(MSG_TYPE_WARNING);
        const int infoCount    = log.getItemCount(MSG_TYPE_INFO);

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

        m_gridMessages->setDataProvider(std::make_shared<GridDataMessages>(log));
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

        //enable dialog-specific key local events
        Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(LogPanel::onLocalKeyEvent), nullptr, this);

        updateGrid();
    }

private:
    MessageView& getDataView()
    {
        if (auto* prov = dynamic_cast<GridDataMessages*>(m_gridMessages->getDataProvider()))
            return prov->getDataView();

        throw std::runtime_error("m_gridMessages was not initialized! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
    }

    void OnErrors(wxCommandEvent& event) override
    {
        m_bpButtonErrors->toggle();
        updateGrid();
    }

    void OnWarnings(wxCommandEvent& event) override
    {
        m_bpButtonWarnings->toggle();
        updateGrid();
    }

    void OnInfo(wxCommandEvent& event) override
    {
        m_bpButtonInfo->toggle();
        updateGrid();
    }

    void updateGrid()
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

    void onGridButtonEvent(wxKeyEvent& event)
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

    void onMsgGridContext(GridClickEvent& event)
    {
        const std::vector<size_t> selection = m_gridMessages->getSelectedRows();

        const size_t rowCount = [&]() -> size_t
        {
            if (auto prov = m_gridMessages->getDataProvider())
                return prov->getRowCount();
            return 0;
        }();

        ContextMenu menu;
        menu.addItem(_("Select all") + L"\tCtrl+A", [this] { m_gridMessages->selectAllRows(ALLOW_GRID_EVENT); }, nullptr, rowCount > 0);
        menu.addSeparator();

        menu.addItem(_("Copy") + L"\tCtrl+C", [this] { copySelectionToClipboard(); }, nullptr, !selection.empty());
        menu.popup(*this);
    }

    void onLocalKeyEvent(wxKeyEvent& event) //process key events without explicit menu entry :)
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
                    m_gridMessages->selectAllRows(ALLOW_GRID_EVENT);
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

    void copySelectionToClipboard()
    {
        try
        {
            using zxString = Zbase<wchar_t>; //guaranteed exponential growth, unlike wxString
            zxString clipboardString;

            if (auto prov = m_gridMessages->getDataProvider())
            {
                std::vector<Grid::ColAttributes> colAttr = m_gridMessages->getColumnConfig();
                erase_if(colAttr, [](const Grid::ColAttributes& ca) { return !ca.visible; });
                if (!colAttr.empty())
                    for (size_t row : m_gridMessages->getSelectedRows())
                    {
                        std::for_each(colAttr.begin(), --colAttr.end(),
                                      [&](const Grid::ColAttributes& ca)
                        {
                            clipboardString += copyStringTo<zxString>(prov->getValue(row, ca.type));
                            clipboardString += L'\t';
                        });
                        clipboardString += copyStringTo<zxString>(prov->getValue(row, colAttr.back().type));
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

    bool processingKeyEventHandler_ = false;
};

//########################################################################################

namespace
{
class CurveDataStatistics : public SparseCurveData
{
public:
    CurveDataStatistics() : SparseCurveData(true /*addSteps*/) {}

    void clear() { samples_.clear(); lastSample_ = {}; }

    void addRecord(std::chrono::nanoseconds timeElapsed, double value)
    {
        assert(!samples_.empty() || (lastSample_ == std::pair<std::chrono::nanoseconds, double>()));

        lastSample_ = { timeElapsed, value };

        //allow for at most one sample per 100ms (handles duplicate inserts, too!) => this is unrelated to UI_UPDATE_INTERVAL!
        if (!samples_.empty()) //always unconditionally insert first sample!
            if (numeric::dist(timeElapsed, samples_.rbegin()->first) < std::chrono::milliseconds(100))
                return;

        samples_.insert(samples_.end(), { timeElapsed, value }); //time is "expected" to be monotonously ascending
        //documentation differs about whether "hint" should be before or after the to be inserted element!
        //however "std::map<>::end()" is interpreted correctly by GCC and VS2010

        if (samples_.size() > MAX_BUFFER_SIZE) //limit buffer size
            samples_.erase(samples_.begin());
    }

private:
    std::pair<double, double> getRangeX() const override
    {
        if (samples_.empty())
            return {};

        const std::chrono::nanoseconds upperEnd = std::max(samples_.rbegin()->first, lastSample_.first);

        /*
        //report some additional width by 5% elapsed time to make graph recalibrate before hitting the right border
        //caveat: graph for batch mode binary comparison does NOT start at elapsed time 0!! PHASE_COMPARING_CONTENT and PHASE_SYNCHRONIZING!
        //=> consider width of current sample set!
        upperEndMs += 0.05 *(upperEndMs - samples.begin()->first);
        */

        return { std::chrono::duration<double>(samples_.begin()->first).count(), //need not start with 0, e.g. "binary comparison, graph reset, followed by sync"
                 std::chrono::duration<double>(upperEnd).count() };
    }

    Opt<CurvePoint> getLessEq(double x) const override //x: seconds since begin
    {
        const auto timeX = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(x)); //round down

        //------ add artifical last sample value -------
        if (!samples_.empty() && samples_.rbegin()->first < lastSample_.first)
            if (lastSample_.first <= timeX)
                return CurvePoint(std::chrono::duration<double>(lastSample_.first).count(), lastSample_.second);
        //--------------------------------------------------

        //find first key > x, then go one step back: => samples must be a std::map, NOT std::multimap!!!
        auto it = samples_.upper_bound(timeX);
        if (it == samples_.begin())
            return NoValue();
        //=> samples not empty in this context
        --it;
        return CurvePoint(std::chrono::duration<double>(it->first).count(), it->second);
    }

    Opt<CurvePoint> getGreaterEq(double x) const override
    {
        const std::chrono::nanoseconds timeX(static_cast<std::chrono::nanoseconds::rep>(std::ceil(x * (1000 * 1000 * 1000)))); //round up!

        //------ add artifical last sample value -------
        if (!samples_.empty() && samples_.rbegin()->first < lastSample_.first)
            if (samples_.rbegin()->first < timeX && timeX <= lastSample_.first)
                return CurvePoint(std::chrono::duration<double>(lastSample_.first).count(), lastSample_.second);
        //--------------------------------------------------

        auto it = samples_.lower_bound(timeX);
        if (it == samples_.end())
            return NoValue();
        return CurvePoint(std::chrono::duration<double>(it->first).count(), it->second);
    }

    static const size_t MAX_BUFFER_SIZE = 2500000; //sizeof(single node) worst case ~ 3 * 8 byte ptr + 16 byte key/value = 40 byte

    std::map <std::chrono::nanoseconds, double> samples_;    //[!] don't use std::multimap, see getLessEq()
    std::pair<std::chrono::nanoseconds, double> lastSample_; //artificial most current record at the end of samples to visualize current time!
};


class CurveDataTotalBlock : public CurveData
{
public:
    void setValue (double x, double y) { x_ = x; y_ = y; }
    void setValueX(double x)           { x_ = x; }
    double getValueX() const { return x_; }

private:
    std::pair<double, double> getRangeX() const override { return { x_, x_ }; } //conceptually just a vertical line!

    std::vector<CurvePoint> getPoints(double minX, double maxX, const wxSize& areaSizePx) const override
    {
        return
        {
            { 0,  y_ },
            { x_, y_ },
            { x_, 0  },
        };
    }

    double x_ = 0; //time elapsed in seconds
    double y_ = 0; //items/bytes processed
};


class CurveDataProcessedBlock : public CurveData
{
public:
    void setValue(double x1, double x2, double y) { x1_ = x1; x2_ = x2; y_ = y; }

private:
    std::pair<double, double> getRangeX() const override { return { x1_, x2_ }; }

    std::vector<CurvePoint> getPoints(double minX, double maxX, const wxSize& areaSizePx) const override
    {
        return
        {
            { 0,   y_ },
            { x1_, y_ },
            { x1_, 0  },
            { x1_, y_ },
            { x2_, y_ },
        };
    }

    double x1_ = 0; //time elapsed in seconds
    double x2_ = 0; //total time (estimated)
    double y_ = 0; //items/bytes processed
};


const double stretchDefaultBlockSize = 1.4; //enlarge block default size


struct LabelFormatterBytes : public LabelFormatter
{
    double getOptimalBlockSize(double bytesProposed) const override
    {
        bytesProposed *= stretchDefaultBlockSize; //enlarge block default size

        if (bytesProposed <= 1) //never smaller than 1 byte
            return 1;

        //round to next number which is a convenient to read block size
        const double k = std::floor(std::log(bytesProposed) / std::log(2.0));
        const double e = std::pow(2.0, k);
        if (numeric::isNull(e))
            return 0;
        const double a = bytesProposed / e; //bytesProposed = a * 2^k with a in [1, 2)
        assert(1 <= a && a < 2);
        const double steps[] = { 1, 2 };
        return e * numeric::nearMatch(a, std::begin(steps), std::end(steps));
    }

    wxString formatText(double value, double optimalBlockSize) const override { return formatFilesizeShort(static_cast<int64_t>(value)); }
};


struct LabelFormatterItemCount : public LabelFormatter
{
    double getOptimalBlockSize(double itemsProposed) const override
    {
        itemsProposed *= stretchDefaultBlockSize; //enlarge block default size

        const double steps[] = { 1, 2, 5, 10 };
        if (itemsProposed <= 10)
            return numeric::nearMatch(itemsProposed, std::begin(steps), std::end(steps)); //like nextNiceNumber(), but without the 2.5 step!
        return nextNiceNumber(itemsProposed);
    }

    wxString formatText(double value, double optimalBlockSize) const override
    {
        return formatNumber(numeric::round(value)); //not enough room for a "%x items" representation
    }
};


struct LabelFormatterTimeElapsed : public LabelFormatter
{
    LabelFormatterTimeElapsed(bool drawLabel) : drawLabel_(drawLabel) {}

    double getOptimalBlockSize(double secProposed) const override
    {
        //5 sec minimum block size
        const double stepsSec[] = { 5, 10, 20, 30, 60 }; //nice numbers for seconds
        if (secProposed <= 60)
            return numeric::nearMatch(secProposed, std::begin(stepsSec), std::end(stepsSec));

        const double stepsMin[] = { 1, 2, 5, 10, 15, 20, 30, 60 }; //nice numbers for minutes
        if (secProposed <= 3600)
            return 60 * numeric::nearMatch(secProposed / 60, std::begin(stepsMin), std::end(stepsMin));

        if (secProposed <= 3600 * 24)
            return 3600 * nextNiceNumber(secProposed / 3600); //round up to full hours

        return 24 * 3600 * nextNiceNumber(secProposed / (24 * 3600)); //round to full days
    }

    wxString formatText(double timeElapsed, double optimalBlockSize) const override
    {
        if (!drawLabel_)
            return wxString();
        return timeElapsed < 60 ?
               wxString(_P("1 sec", "%x sec", numeric::round(timeElapsed))) :
               timeElapsed < 3600 ?
               wxTimeSpan::Seconds(timeElapsed).Format(   L"%M:%S") :
               wxTimeSpan::Seconds(timeElapsed).Format(L"%H:%M:%S");
    }

private:
    const bool drawLabel_;
};
}


template <class TopLevelDialog> //can be a wxFrame or wxDialog
class SyncProgressDialogImpl : public TopLevelDialog, public SyncProgressDialog
/*we need derivation, not composition!
      1. SyncProgressDialogImpl IS a wxFrame/wxDialog
      2. implement virtual ~wxFrame()
      3. event handling below assumes lifetime is larger-equal than wxFrame's
*/
{
public:
    SyncProgressDialogImpl(long style, //wxFrame/wxDialog style
                           const std::function<wxFrame*(TopLevelDialog& progDlg)>& getTaskbarFrame,
                           AbortCallback& abortCb,
                           const std::function<void()>& notifyWindowTerminate,
                           const Statistics& syncStat,
                           wxFrame* parentFrame,
                           bool showProgress,
                           bool autoCloseDialog,
                           const wxString& jobName,
                           const Zstring& soundFileSyncComplete,
                           bool ignoreErrors,
                           PostSyncAction2 postSyncAction);
    ~SyncProgressDialogImpl() override;

    //call this in StatusUpdater derived class destructor at the LATEST(!) to prevent access to currentStatusUpdater
    void showSummary(SyncResult resultId, const ErrorLog& log) override;
    void closeDirectly(bool restoreParentFrame) override;

    wxWindow* getWindowIfVisible() override { return this->IsShown() ? this : nullptr; }
    //workaround OS X bug: if "this" is used as parent window for a modal dialog then this dialog will erroneously un-hide its parent!

    void initNewPhase        () override;
    void notifyProgressChange() override;
    void updateGui           () override { updateProgressGui(true /*allowYield*/); }

    bool getOptionIgnoreErrors()                 const override { return pnl_.m_checkBoxIgnoreErrors->GetValue(); }
    void    setOptionIgnoreErrors(bool ignoreErrors)   override { pnl_.m_checkBoxIgnoreErrors->SetValue(ignoreErrors); updateStaticGui(); }
    PostSyncAction2 getOptionPostSyncAction()    const override { return getEnumVal(enumPostSyncAction_, *pnl_.m_choicePostSyncAction); }
    bool getOptionAutoCloseDialog()              const override { return pnl_.m_checkBoxAutoClose->GetValue(); }

    void timerSetStatus(bool active) override
    {
        if (active)
            stopWatch_.resume();
        else
            stopWatch_.pause();
    }

    bool timerIsRunning() const override
    {
        return !stopWatch_.isPaused();
    }

private:
    void onLocalKeyEvent (wxKeyEvent& event);
    void onParentKeyEvent(wxKeyEvent& event);
    void OnOkay   (wxCommandEvent& event);
    void OnPause  (wxCommandEvent& event);
    void OnCancel (wxCommandEvent& event);
    void OnClose  (wxCloseEvent& event);
    void OnIconize(wxIconizeEvent& event);
    void OnMinimizeToTray(wxCommandEvent& event) { minimizeToTray(); }
    void OnToggleIgnoreErrors(wxCommandEvent& event) { updateStaticGui(); }

    void minimizeToTray();
    void resumeFromSystray();

    void updateStaticGui();
    void updateProgressGui(bool allowYield);

    void setExternalStatus(const wxString& status, const wxString& progress); //progress may be empty!

    SyncProgressPanelGenerated& pnl_; //wxPanel containing the GUI controls of *this

    const wxString jobName_;
    const Zstring soundFileSyncComplete_;
    StopWatch stopWatch_;

    wxFrame* parentFrame_; //optional

    std::function<void()> notifyWindowTerminate_; //call once in OnClose(), NOT in destructor which is called far too late somewhere in wxWidgets main loop!

    bool wereDead_ = false; //set after wxWindow::Delete(), which equals "delete this" on OS X!

    //status variables
    const Statistics* syncStat_;                  //
    AbortCallback*    abortCb_;                   //valid only while sync is running
    bool paused_ = false; //valid only while sync is running
    SyncResult finalResult_ = RESULT_ABORTED; //set after sync

    //remaining time
    std::unique_ptr<PerfCheck> perf_;
    std::chrono::nanoseconds timeLastSpeedEstimate_ = std::chrono::seconds(-100); //used for calculating intervals between collecting perf samples

    //help calculate total speed
    std::chrono::nanoseconds phaseStart_{}; //begin of current phase

    std::shared_ptr<CurveDataStatistics    > curveDataBytes_       { std::make_shared<CurveDataStatistics>() };
    std::shared_ptr<CurveDataStatistics    > curveDataItems_       { std::make_shared<CurveDataStatistics>() };
    std::shared_ptr<CurveDataProcessedBlock> curveDataBytesCurrent_{ std::make_shared<CurveDataProcessedBlock>() };
    std::shared_ptr<CurveDataProcessedBlock> curveDataItemsCurrent_{ std::make_shared<CurveDataProcessedBlock>() };
    std::shared_ptr<CurveDataTotalBlock    > curveDataBytesTotal_  { std::make_shared<CurveDataTotalBlock>() };
    std::shared_ptr<CurveDataTotalBlock    > curveDataItemsTotal_  { std::make_shared<CurveDataTotalBlock>() };

    wxString parentTitleBackup_;
    std::unique_ptr<FfsTrayIcon> trayIcon_; //optional: if filled all other windows should be hidden and conversely
    std::unique_ptr<Taskbar> taskbar_;

    EnumDescrList<PostSyncAction2> enumPostSyncAction_;
};


template <class TopLevelDialog>
SyncProgressDialogImpl<TopLevelDialog>::SyncProgressDialogImpl(long style, //wxFrame/wxDialog style
                                                               const std::function<wxFrame*(TopLevelDialog& progDlg)>& getTaskbarFrame,
                                                               AbortCallback& abortCb,
                                                               const std::function<void()>& notifyWindowTerminate,
                                                               const Statistics& syncStat,
                                                               wxFrame* parentFrame,
                                                               bool showProgress,
                                                               bool autoCloseDialog,
                                                               const wxString& jobName,
                                                               const Zstring& soundFileSyncComplete,
                                                               bool ignoreErrors,
                                                               PostSyncAction2 postSyncAction) :
    TopLevelDialog(parentFrame, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, style), //title is overwritten anyway in setExternalStatus()
    pnl_(*new SyncProgressPanelGenerated(this)), //ownership passed to "this"
    jobName_  (jobName),
    soundFileSyncComplete_(soundFileSyncComplete),
    parentFrame_(parentFrame),
    notifyWindowTerminate_(notifyWindowTerminate),
    syncStat_ (&syncStat),
    abortCb_  (&abortCb)
{
    static_assert(IsSameType<TopLevelDialog, wxFrame >::value ||
                  IsSameType<TopLevelDialog, wxDialog>::value, "");
    assert((IsSameType<TopLevelDialog, wxFrame>::value == !parentFrame));

    //finish construction of this dialog:
    this->SetMinSize(wxSize(470, 280)); //== minimum size! no idea why SetMinSize() is not used...
    wxBoxSizer* bSizer170 = new wxBoxSizer(wxVERTICAL);
    bSizer170->Add(&pnl_, 1, wxEXPAND);
    this->SetSizer(bSizer170); //pass ownership

    //lifetime of event sources is subset of this instance's lifetime => no wxEvtHandler::Disconnect() needed
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler  (SyncProgressDialogImpl<TopLevelDialog>::OnClose));
    this->Connect(wxEVT_ICONIZE,      wxIconizeEventHandler(SyncProgressDialogImpl<TopLevelDialog>::OnIconize));
    this->Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(SyncProgressDialogImpl::onLocalKeyEvent), nullptr, this);
    pnl_.m_buttonClose->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(SyncProgressDialogImpl::OnOkay  ), NULL, this);
    pnl_.m_buttonPause->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(SyncProgressDialogImpl::OnPause ), NULL, this);
    pnl_.m_buttonStop ->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(SyncProgressDialogImpl::OnCancel), NULL, this);
    pnl_.m_bpButtonMinimizeToTray->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(SyncProgressDialogImpl::OnMinimizeToTray), NULL, this);
    pnl_.m_checkBoxIgnoreErrors->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(SyncProgressDialogImpl::OnToggleIgnoreErrors), NULL, this);

    if (parentFrame_)
        parentFrame_->Connect(wxEVT_CHAR_HOOK, wxKeyEventHandler(SyncProgressDialogImpl::onParentKeyEvent), nullptr, this);


    assert(pnl_.m_buttonClose->GetId() == wxID_OK); //we cannot use wxID_CLOSE else Esc key won't work: yet another wxWidgets bug??

    setRelativeFontSize(*pnl_.m_staticTextPhase, 1.5);

    if (parentFrame_)
        parentTitleBackup_ = parentFrame_->GetTitle(); //save old title (will be used as progress indicator)

    //pnl.m_animCtrlSyncing->SetAnimation(getResourceAnimation(L"working"));
    //pnl.m_animCtrlSyncing->Play();

    //this->EnableCloseButton(false); //this is NOT honored on OS X or with ALT+F4 on Windows! -> why disable button at all??

    stopWatch_.restart(); //measure total time

    if (wxFrame* frame = getTaskbarFrame(*this))
        try //try to get access to Windows 7/Ubuntu taskbar
        {
            taskbar_ = std::make_unique<Taskbar>(*frame); //throw TaskbarNotAvailable
        }
        catch (const TaskbarNotAvailable&) {}

    //hide "processed" statistics until end of process
    pnl_.m_notebookResult     ->Hide();
    pnl_.m_panelItemsProcessed->Hide();
    pnl_.m_buttonClose        ->Show(false);
    //set std order after button visibility was set
    setStandardButtonLayout(*pnl_.bSizerStdButtons, StdButtons().setAffirmative(pnl_.m_buttonPause).setCancel(pnl_.m_buttonStop));

    pnl_.m_bpButtonMinimizeToTray->SetBitmapLabel(getResourceImage(L"minimize_to_tray"));

    //init graph
    const int xLabelHeight = this->GetCharHeight() + 2 * 1 /*border*/; //use same height for both graphs to make sure they stretch evenly
    const int yLabelWidth  = 70;
    pnl_.m_panelGraphBytes->setAttributes(Graph2D::MainAttributes().
                                          setLabelX(Graph2D::LABEL_X_TOP,    xLabelHeight, std::make_shared<LabelFormatterTimeElapsed>(true)).
                                          setLabelY(Graph2D::LABEL_Y_RIGHT,  yLabelWidth,  std::make_shared<LabelFormatterBytes>()).
                                          setBackgroundColor(wxColor(208, 208, 208)). //light grey
                                          setSelectionMode(Graph2D::SELECT_NONE));

    pnl_.m_panelGraphItems->setAttributes(Graph2D::MainAttributes().
                                          setLabelX(Graph2D::LABEL_X_BOTTOM, xLabelHeight, std::make_shared<LabelFormatterTimeElapsed>(true)).
                                          setLabelY(Graph2D::LABEL_Y_RIGHT,  yLabelWidth,  std::make_shared<LabelFormatterItemCount>()).
                                          setBackgroundColor(wxColor(208, 208, 208)). //light grey
                                          setSelectionMode(Graph2D::SELECT_NONE));

    pnl_.m_panelGraphBytes->setCurve(curveDataBytesTotal_, Graph2D::CurveAttributes().setLineWidth(1).fillCurveArea(*wxWHITE).setColor(wxColor(192, 192, 192))); //medium grey
    pnl_.m_panelGraphItems->setCurve(curveDataItemsTotal_, Graph2D::CurveAttributes().setLineWidth(1).fillCurveArea(*wxWHITE).setColor(wxColor(192, 192, 192))); //medium grey

    pnl_.m_panelGraphBytes->addCurve(curveDataBytesCurrent_, Graph2D::CurveAttributes().setLineWidth(1).fillCurveArea(getColorBytesBackground()).setColor(getColorBytesBackgroundRim()));
    pnl_.m_panelGraphItems->addCurve(curveDataItemsCurrent_, Graph2D::CurveAttributes().setLineWidth(1).fillCurveArea(getColorItemsBackground()).setColor(getColorItemsBackgroundRim()));

    pnl_.m_panelGraphBytes->addCurve(curveDataBytes_, Graph2D::CurveAttributes().setLineWidth(2).fillCurveArea(getColorBytes()).setColor(getColorBytesRim()));
    pnl_.m_panelGraphItems->addCurve(curveDataItems_, Graph2D::CurveAttributes().setLineWidth(2).fillCurveArea(getColorItems()).setColor(getColorItemsRim()));

    //graph legend:
    auto generateSquareBitmap = [&](const wxColor& fillCol, const wxColor& borderCol)
    {
        wxBitmap bmpSquare(this->GetCharHeight(), this->GetCharHeight()); //seems we don't need to pass 24-bit depth here even for high-contrast color schemes
        {
            wxMemoryDC dc(bmpSquare);
            wxDCBrushChanger dummy(dc, fillCol);
            wxDCPenChanger  dummy2(dc, borderCol);
            dc.DrawRectangle(wxPoint(), bmpSquare.GetSize());
        }
        return bmpSquare;
    };
    pnl_.m_bitmapGraphKeyBytes->SetBitmap(generateSquareBitmap(getColorBytes(), getColorBytesRim()));
    pnl_.m_bitmapGraphKeyItems->SetBitmap(generateSquareBitmap(getColorItems(), getColorItemsRim()));

    //allow changing a few options dynamically during sync
    pnl_.m_checkBoxIgnoreErrors->SetValue(ignoreErrors);

    enumPostSyncAction_.add(PostSyncAction2::NONE, L"");
    if (parentFrame_) //enable EXIT option for gui mode sync
        enumPostSyncAction_.add(PostSyncAction2::EXIT, replaceCpy(_("E&xit"), L"&", L"")); //reuse translation
    enumPostSyncAction_.add(PostSyncAction2::SLEEP,    _("System: Sleep"));
    enumPostSyncAction_.add(PostSyncAction2::SHUTDOWN, _("System: Shut down"));

    setEnumVal(enumPostSyncAction_, *pnl_.m_choicePostSyncAction, postSyncAction);

    pnl_.m_checkBoxAutoClose->SetValue(autoCloseDialog);

    updateStaticGui(); //null-status will be shown while waiting for dir locks

    this->GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    pnl_.Layout();
    this->Center(); //call *after* dialog layout update and *before* wxWindow::Show()!

    if (showProgress)
    {
        this->Show();
        pnl_.m_buttonStop->SetFocus(); //don't steal focus when starting in sys-tray!

        //clear gui flicker, remove dummy texts: window must be visible to make this work!
        updateProgressGui(true /*allowYield*/); //at least on OS X a real Yield() is required to flush pending GUI updates; Update() is not enough
    }
    else
        minimizeToTray();
}


template <class TopLevelDialog>
SyncProgressDialogImpl<TopLevelDialog>::~SyncProgressDialogImpl()
{
    if (parentFrame_)
    {
        parentFrame_->Disconnect(wxEVT_CHAR_HOOK, wxKeyEventHandler(SyncProgressDialogImpl::onParentKeyEvent), nullptr, this);

        parentFrame_->SetTitle(parentTitleBackup_); //restore title text

        //make sure main dialog is shown again if still "minimized to systray"! see SyncProgressDialog::closeDirectly()
        parentFrame_->Show();
        //if (parentFrame_->IsIconized()) //caveat: if window is maximized calling Iconize(false) will erroneously un-maximize!
        //    parentFrame_->Iconize(false);
    }

    //our client is NOT expecting a second call via notifyWindowTerminate_()!
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::onLocalKeyEvent(wxKeyEvent& event)
{
    switch (event.GetKeyCode())
    {
        case WXK_ESCAPE:
        {
            wxButton& activeButton = pnl_.m_buttonStop->IsShown() ? *pnl_.m_buttonStop : *pnl_.m_buttonClose;

            wxCommandEvent dummy(wxEVT_COMMAND_BUTTON_CLICKED);
            activeButton.Command(dummy); //simulate click
            return;
        }
        break;
    }

    event.Skip();
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::onParentKeyEvent(wxKeyEvent& event)
{
    //redirect keys from main dialog to progress dialog
    switch (event.GetKeyCode())
    {
        case WXK_ESCAPE:
            this->SetFocus();
            this->onLocalKeyEvent(event); //event will be handled => no event recursion to parent dialog!
            return;
    }

    event.Skip();
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::initNewPhase()
{
    updateStaticGui(); //evaluates "syncStat_->currentPhase()"

    //reset graphs (e.g. after binary comparison)
    curveDataBytesCurrent_->setValue(0, 0, 0);
    curveDataItemsCurrent_->setValue(0, 0, 0);
    curveDataBytesTotal_  ->setValue(0, 0);
    curveDataItemsTotal_  ->setValue(0, 0);
    curveDataBytes_       ->clear();
    curveDataItems_       ->clear();

    notifyProgressChange(); //make sure graphs get initial values

    //start new measurement
    perf_ = std::make_unique<PerfCheck>(WINDOW_REMAINING_TIME, WINDOW_BYTES_PER_SEC);
    timeLastSpeedEstimate_ = std::chrono::seconds(-100); //make sure estimate is updated upon next check

    phaseStart_ = stopWatch_.elapsed();

    updateProgressGui(false /*allowYield*/);
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::notifyProgressChange() //noexcept!
{
    if (syncStat_) //sync running
        switch (syncStat_->currentPhase())
        {
            case ProcessCallback::PHASE_NONE:
            //assert(false); -> can happen: e.g. batch run, log file creation failed, throw in BatchStatusHandler constructor
            case ProcessCallback::PHASE_SCANNING:
                break;
            case ProcessCallback::PHASE_COMPARING_CONTENT:
            case ProcessCallback::PHASE_SYNCHRONIZING:
            {
                const int64_t bytesCurrent = syncStat_->getBytesCurrent(syncStat_->currentPhase());
                const int     itemsCurrent = syncStat_->getItemsCurrent(syncStat_->currentPhase());

                curveDataBytes_->addRecord(stopWatch_.elapsed(), bytesCurrent);
                curveDataItems_->addRecord(stopWatch_.elapsed(), itemsCurrent);
            }
            break;
        }
}


namespace
{
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::setExternalStatus(const wxString& status, const wxString& progress) //progress may be empty!
{
    //sys tray: order "top-down": jobname, status, progress
    wxString systrayTooltip = jobName_.empty() ? status : L"\"" + jobName_ + L"\"\n" + status;
    if (!progress.empty())
        systrayTooltip += L" " + progress;

    //window caption/taskbar; inverse order: progress, status, jobname
    wxString title = progress.empty() ? status : progress + SPACED_DASH + status;
    if (!jobName_.empty())
        title += wxString(SPACED_DASH) + L"\"" + jobName_ + L"\"";

    //systray tooltip, if window is minimized
    if (trayIcon_.get())
        trayIcon_->setToolTip(systrayTooltip);

    //show text in dialog title (and at the same time in taskbar)
    if (parentFrame_)
        if (parentFrame_->GetTitle() != title)
            parentFrame_->SetTitle(title);

    //always set a title: we don't wxGTK to show "nameless window" instead
    if (this->GetTitle() != title)
        this->SetTitle(title);
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::updateProgressGui(bool allowYield)
{
    if (!syncStat_) //sync not running
        return;

    //normally we don't update the "static" GUI components here, but we have to make an exception
    //if sync is cancelled (by user or error handling option)
    if (syncStat_->getAbortStatus())
        updateStaticGui(); //called more than once after cancel... ok


    bool layoutChanged = false; //avoid screen flicker by calling layout() only if necessary
    const std::chrono::nanoseconds timeElapsed = stopWatch_.elapsed();
    const double timeElapsedDouble = std::chrono::duration<double>(timeElapsed).count();

    //sync status text
    setText(*pnl_.m_staticTextStatus, replaceCpy(syncStat_->currentStatusText(), L'\n', L' ')); //no layout update for status texts!

    switch (syncStat_->currentPhase()) //no matter if paused or not
    {
        case ProcessCallback::PHASE_NONE:
        case ProcessCallback::PHASE_SCANNING:
            //dialog caption, taskbar, systray tooltip
            setExternalStatus(getDialogPhaseText(syncStat_, paused_, finalResult_), formatNumber(syncStat_->getItemsCurrent(ProcessCallback::PHASE_SCANNING))); //status text may be "paused"!

            //progress indicators
            if (trayIcon_.get()) trayIcon_->setProgress(1); //100% = regular FFS logo

            //ignore graphs: should already have been cleared in initNewPhase()

            //remaining objects and data
            setText(*pnl_.m_staticTextItemsRemaining, L"-", &layoutChanged);
            setText(*pnl_.m_staticTextBytesRemaining, L"", &layoutChanged);

            //remaining time and speed
            setText(*pnl_.m_staticTextTimeRemaining, L"-", &layoutChanged);
            pnl_.m_panelGraphBytes->setAttributes(pnl_.m_panelGraphBytes->getAttributes().setCornerText(wxString(), Graph2D::CORNER_TOP_LEFT));
            pnl_.m_panelGraphItems->setAttributes(pnl_.m_panelGraphItems->getAttributes().setCornerText(wxString(), Graph2D::CORNER_TOP_LEFT));
            break;

        case ProcessCallback::PHASE_COMPARING_CONTENT:
        case ProcessCallback::PHASE_SYNCHRONIZING:
        {
            const int64_t bytesCurrent  = syncStat_->getBytesCurrent(syncStat_->currentPhase());
            const int64_t bytesTotal    = syncStat_->getBytesTotal  (syncStat_->currentPhase());
            const int     itemsCurrent  = syncStat_->getItemsCurrent(syncStat_->currentPhase());
            const int     itemsTotal    = syncStat_->getItemsTotal  (syncStat_->currentPhase());

            //add both data + obj-count, to handle "deletion-only" cases
            const double fractionTotal = bytesTotal + itemsTotal == 0 ? 0 : 1.0 * (bytesCurrent + itemsCurrent) / (bytesTotal + itemsTotal);
            //----------------------------------------------------------------------------------------------------

            //dialog caption, taskbar, systray tooltip
            setExternalStatus(getDialogPhaseText(syncStat_, paused_, finalResult_), formatFraction(fractionTotal)); //status text may be "paused"!

            //progress indicators
            if (trayIcon_.get()) trayIcon_->setProgress(fractionTotal);
            if (taskbar_ .get()) taskbar_ ->setProgress(fractionTotal);

            const double timeTotalSecTentative = bytesTotal == bytesCurrent ? timeElapsedDouble : std::max(curveDataBytesTotal_->getValueX(), timeElapsedDouble);

            //constant line graph
            curveDataBytesCurrent_->setValue(timeElapsedDouble, timeTotalSecTentative, bytesCurrent);
            curveDataItemsCurrent_->setValue(timeElapsedDouble, timeTotalSecTentative, itemsCurrent);

            //tentatively update total time, may be improved on below:
            curveDataBytesTotal_->setValue(timeTotalSecTentative, bytesTotal);
            curveDataItemsTotal_->setValue(timeTotalSecTentative, itemsTotal);

            //even though notifyProgressChange() already set the latest data, let's add another sample to have all curves consider "timeNowMs"
            //no problem with adding too many records: CurveDataStatistics will remove duplicate entries!
            curveDataBytes_->addRecord(timeElapsed, bytesCurrent);
            curveDataItems_->addRecord(timeElapsed, itemsCurrent);

            //remaining item and byte count
            setText(*pnl_.m_staticTextItemsRemaining, formatNumber(itemsTotal - itemsCurrent), &layoutChanged);
            setText(*pnl_.m_staticTextBytesRemaining, L"(" + formatFilesizeShort(bytesTotal - bytesCurrent) + L")", &layoutChanged);
            //it's possible data remaining becomes shortly negative if last file synced has ADS data and the bytesTotal was not yet corrected!

            //remaining time and speed
            assert(perf_);
            if (perf_)
                if (numeric::dist(timeLastSpeedEstimate_, timeElapsed) >= std::chrono::milliseconds(500))
                {
                    timeLastSpeedEstimate_ = timeElapsed;

                    if (numeric::dist(phaseStart_, timeElapsed) >= std::chrono::seconds(1)) //discard stats for first second: probably messy
                        perf_->addSample(timeElapsed, itemsCurrent, bytesCurrent);

                    //current speed -> Win 7 copy uses 1 sec update interval instead
                    Opt<std::wstring> bps = perf_->getBytesPerSecond();
                    Opt<std::wstring> ips = perf_->getItemsPerSecond();
                    pnl_.m_panelGraphBytes->setAttributes(pnl_.m_panelGraphBytes->getAttributes().setCornerText(bps ? *bps : L"", Graph2D::CORNER_TOP_LEFT));
                    pnl_.m_panelGraphItems->setAttributes(pnl_.m_panelGraphItems->getAttributes().setCornerText(ips ? *ips : L"", Graph2D::CORNER_TOP_LEFT));

                    //remaining time: display with relative error of 10% - based on samples taken every 0.5 sec only
                    //-> call more often than once per second to correctly show last few seconds countdown, but don't call too often to avoid occasional jitter
                    Opt<double> remTimeSec = perf_->getRemainingTimeSec(bytesTotal - bytesCurrent);
                    setText(*pnl_.m_staticTextTimeRemaining, remTimeSec ? formatRemainingTime(*remTimeSec) : L"-", &layoutChanged);

                    //update estimated total time marker with precision of "10% remaining time" only to avoid needless jumping around:
                    const double timeRemainingSec = remTimeSec ? *remTimeSec : 0;
                    const double timeTotalSec = timeElapsedDouble + timeRemainingSec;
                    if (numeric::dist(curveDataBytesTotal_->getValueX(), timeTotalSec) > 0.1 * timeRemainingSec)
                    {
                        curveDataBytesTotal_->setValueX(timeTotalSec);
                        curveDataItemsTotal_->setValueX(timeTotalSec);
                        //don't forget to update these, too:
                        curveDataBytesCurrent_->setValue(timeElapsedDouble, timeTotalSec, bytesCurrent);
                        curveDataItemsCurrent_->setValue(timeElapsedDouble, timeTotalSec, itemsCurrent);
                    }
                }
            break;
        }
    }

    pnl_.m_panelGraphBytes->Refresh();
    pnl_.m_panelGraphItems->Refresh();

    const int64_t timeElapSec = std::chrono::duration_cast<std::chrono::seconds>(timeElapsed).count();

    setText(*pnl_.m_staticTextTimeElapsed,
            timeElapSec < 3600 ?
            wxTimeSpan::Seconds(timeElapSec).Format(   L"%M:%S") :
            wxTimeSpan::Seconds(timeElapSec).Format(L"%H:%M:%S"), &layoutChanged);

    //adapt layout after content changes above
    if (layoutChanged)
    {
        pnl_.m_panelProgress->Layout();
        //small statistics panels:
        //pnl.m_panelItemsProcessed->Layout();
        pnl_.m_panelItemsRemaining->Layout();
        pnl_.m_panelTimeRemaining ->Layout();
        //pnl.m_panelTimeElapsed->Layout(); -> needed?
    }


    if (allowYield)
    {
        //support for pause button
        if (paused_)
        {
            /*
            ZEN_ON_SCOPE_EXIT(resumeTimer()); -> crashes on Fedora; WHY???
            => likely compiler bug!!!
               1. no crash on Fedora for: ZEN_ON_SCOPE_EXIT(this->resumeTimer());
               1. no crash if we derive from wxFrame instead of template "TopLevelDialog"
               2. no crash on Ubuntu GCC
               3. following makes GCC crash already during compilation: auto dfd = zen::makeGuard([this]{ resumeTimer(); });
            */
            timerSetStatus(false /*active*/);

            while (paused_)
            {
                wxTheApp->Yield(); //receive UI message that end pause OR forceful termination!
                //*first* refresh GUI (removing flicker) before sleeping!
                std::this_thread::sleep_for(UI_UPDATE_INTERVAL);
            }
            //after SyncProgressDialogImpl::OnClose() called wxWindow::Destroy() on OS X this instance is instantly toast!
            if (wereDead_)
                return; //GTFO and don't call this->timerSetStatus()

            timerSetStatus(true /*active*/);
        }
        else
            /*
                /|\
                 |   keep this sequence to ensure one full progress update before entering pause mode!
                \|/
            */
            wxTheApp->Yield(); //receive UI message that sets pause status OR forceful termination!
    }
    else
        this->Update(); //don't wait until next idle event (who knows what blocking process comes next?)
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::updateStaticGui() //depends on "syncStat_, paused_, finalResult"
{
    const wxString dlgPhaseTxt = getDialogPhaseText(syncStat_, paused_, finalResult_);

    pnl_.m_staticTextPhase->SetLabel(dlgPhaseTxt);
    //pnl_.m_bitmapStatus->SetToolTip(dlgPhaseTxt); -> redundant

    auto setStatusBitmap = [&](const wchar_t* bmpName)
    {
        pnl_.m_bitmapStatus->SetBitmap(getResourceImage(bmpName));
        pnl_.m_bitmapStatus->Show();
    };

    //status bitmap
    if (syncStat_) //sync running
    {
        if (paused_)
            setStatusBitmap(L"status_pause");
        else
        {
            if (syncStat_->getAbortStatus())
                setStatusBitmap(L"status_aborted");
            else
                switch (syncStat_->currentPhase())
                {
                    case ProcessCallback::PHASE_NONE:
                        pnl_.m_bitmapStatus->Hide();
                        break;

                    case ProcessCallback::PHASE_SCANNING:
                        setStatusBitmap(L"status_scanning");
                        break;

                    case ProcessCallback::PHASE_COMPARING_CONTENT:
                        setStatusBitmap(L"status_binary_compare");
                        break;

                    case ProcessCallback::PHASE_SYNCHRONIZING:
                        setStatusBitmap(L"status_syncing");
                        break;
                }
        }
    }
    else //sync finished
        switch (finalResult_)
        {
            case RESULT_ABORTED:
                setStatusBitmap(L"status_aborted");
                break;

            case RESULT_FINISHED_WITH_ERROR:
                setStatusBitmap(L"status_finished_errors");
                break;

            case RESULT_FINISHED_WITH_WARNINGS:
                setStatusBitmap(L"status_finished_warnings");
                break;

            case RESULT_FINISHED_WITH_SUCCESS:
                setStatusBitmap(L"status_finished_success");
                break;
        }

    //show status on Windows 7 taskbar
    if (taskbar_.get())
    {
        if (syncStat_) //sync running
        {
            if (paused_)
                taskbar_->setStatus(Taskbar::STATUS_PAUSED);
            else
                switch (syncStat_->currentPhase())
                {
                    case ProcessCallback::PHASE_NONE:
                    case ProcessCallback::PHASE_SCANNING:
                        taskbar_->setStatus(Taskbar::STATUS_INDETERMINATE);
                        break;

                    case ProcessCallback::PHASE_COMPARING_CONTENT:
                    case ProcessCallback::PHASE_SYNCHRONIZING:
                        taskbar_->setStatus(Taskbar::STATUS_NORMAL);
                        break;
                }
        }
        else //sync finished
            switch (finalResult_)
            {
                case RESULT_ABORTED:
                case RESULT_FINISHED_WITH_ERROR:
                    taskbar_->setStatus(Taskbar::STATUS_ERROR);
                    break;

                case RESULT_FINISHED_WITH_WARNINGS:
                case RESULT_FINISHED_WITH_SUCCESS:
                    taskbar_->setStatus(Taskbar::STATUS_NORMAL);
                    break;
            }
    }

    //pause button
    if (syncStat_) //sync running
        pnl_.m_buttonPause->SetLabel(paused_ ? _("&Continue") : _("&Pause"));


    pnl_.m_bitmapIgnoreErrors->SetBitmap(getResourceImage(pnl_.m_checkBoxIgnoreErrors->GetValue() ? L"msg_error_medium_ignored" : L"msg_error_medium"));

    pnl_.Layout();
    this->Refresh(); //a few pixels below the status text need refreshing
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::closeDirectly(bool restoreParentFrame) //this should really be called: do not call back + schedule deletion
{
    assert(syncStat_ && abortCb_);

    if (!restoreParentFrame)
        parentFrame_ = nullptr; //avoid destructor calls like parentFrame_->Show(), ::TransformProcessType(&psn, kProcessTransformToForegroundApplication);

    paused_ = false; //you never know?

    //ATTENTION: dialog may live a little longer, so watch callbacks!
    //e.g. wxGTK calls OnIconize after wxWindow::Close() (better not ask why) and before physical destruction! => indirectly calls updateStaticGui(), which reads syncStat_!!!
    syncStat_ = nullptr;
    abortCb_  = nullptr;

    //resumeFromSystray(); -> NO, instead ~SyncProgressDialogImpl() makes sure that main dialog is shown again! e.g. avoid calls to this/parentFrame_->Raise()

    this->Close(); //generate close event: do NOT destroy window unconditionally!
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::showSummary(SyncResult resultId, const ErrorLog& log) //essential to call this in StatusHandler derived class destructor
{
    assert(syncStat_ && abortCb_);
    //at the LATEST(!) to prevent access to currentStatusHandler
    //enable okay and close events; may be set in this method ONLY

    //In wxWidgets 2.9.3 upwards, the wxWindow::Reparent() below fails on GTK and OS X if window is frozen! http://forums.codeblocks.org/index.php?topic=13388.45

    paused_ = false; //you never know?

    //update numbers one last time (as if sync were still running)
    notifyProgressChange(); //make one last graph entry at the *current* time
    updateProgressGui(false /*allowYield*/);

    switch (syncStat_->currentPhase()) //no matter if paused or not
    {
        case ProcessCallback::PHASE_NONE:
        case ProcessCallback::PHASE_SCANNING:
            //set overall speed -> not needed
            //items processed -> not needed
            break;

        case ProcessCallback::PHASE_COMPARING_CONTENT:
        case ProcessCallback::PHASE_SYNCHRONIZING:
        {
            const int     itemsCurrent = syncStat_->getItemsCurrent(syncStat_->currentPhase());
            const int     itemsTotal   = syncStat_->getItemsTotal  (syncStat_->currentPhase());
            const int64_t bytesCurrent = syncStat_->getBytesCurrent(syncStat_->currentPhase());
            const int64_t bytesTotal   = syncStat_->getBytesTotal  (syncStat_->currentPhase());
            assert(bytesCurrent <= bytesTotal);

            //set overall speed (instead of current speed)
            const double timeDelta = std::chrono::duration<double>(stopWatch_.elapsed() - phaseStart_).count();
            //we need to consider "time within current phase" not total "timeElapsed"!

            const wxString overallBytesPerSecond = numeric::isNull(timeDelta) ? std::wstring() : formatFilesizeShort(numeric::round(bytesCurrent / timeDelta)) + _("/sec");
            const wxString overallItemsPerSecond = numeric::isNull(timeDelta) ? std::wstring() : replaceCpy(_("%x items/sec"), L"%x", formatThreeDigitPrecision(itemsCurrent / timeDelta));

            pnl_.m_panelGraphBytes->setAttributes(pnl_.m_panelGraphBytes->getAttributes().setCornerText(overallBytesPerSecond, Graph2D::CORNER_TOP_LEFT));
            pnl_.m_panelGraphItems->setAttributes(pnl_.m_panelGraphItems->getAttributes().setCornerText(overallItemsPerSecond, Graph2D::CORNER_TOP_LEFT));

            //show new element "items processed"
            pnl_.m_panelItemsProcessed->Show();
            pnl_.m_staticTextItemsProcessed->SetLabel(formatNumber(itemsCurrent));
            pnl_.m_staticTextBytesProcessed->SetLabel(L"(" + formatFilesizeShort(bytesCurrent) + L")");

            //hide remaining elements...
            if (itemsCurrent == itemsTotal && //...if everything was processed successfully
                bytesCurrent == bytesTotal)
                pnl_.m_panelItemsRemaining->Hide();
        }
        break;
    }

    //------- change class state -------
    finalResult_ = resultId;

    syncStat_ = nullptr;
    abortCb_  = nullptr;
    //----------------------------------

    updateStaticGui();
    setExternalStatus(getDialogPhaseText(syncStat_, paused_, finalResult_), wxString());

    resumeFromSystray(); //if in tray mode...

    //this->EnableCloseButton(true);

    pnl_.m_bpButtonMinimizeToTray->Hide();
    pnl_.m_buttonStop->Disable();
    pnl_.m_buttonStop->Hide();
    pnl_.m_buttonPause->Disable();
    pnl_.m_buttonPause->Hide();
    pnl_.m_buttonClose->Show();
    pnl_.m_buttonClose->Enable();

    pnl_.m_buttonClose->SetFocus();

    pnl_.bSizerProgressFooter->Show(false);

    if (!parentFrame_) //hide checkbox for batch mode sync (where value won't be retrieved after close)
        pnl_.m_checkBoxAutoClose->Hide();

    //set std order after button visibility was set
    setStandardButtonLayout(*pnl_.bSizerStdButtons, StdButtons().setAffirmative(pnl_.m_buttonClose));

    //hide current operation status
    pnl_.bSizerStatusText->Show(false);

    pnl_.m_staticlineFooter->Hide(); //win: m_notebookResult already has a window frame

    //hide remaining time
    pnl_.m_panelTimeRemaining->Hide();

    //-------------------------------------------------------------

    pnl_.m_notebookResult->SetPadding(wxSize(2, 0)); //height cannot be changed

    const size_t pagePosProgress = 0;
    const size_t pagePosLog      = 1;

    //1. re-arrange graph into results listbook
    const bool wasDetached = pnl_.bSizerRoot->Detach(pnl_.m_panelProgress);
    assert(wasDetached);
    (void)wasDetached;
    pnl_.m_panelProgress->Reparent(pnl_.m_notebookResult);
    pnl_.m_notebookResult->AddPage(pnl_.m_panelProgress, _("Progress"), true /*bSelect*/);

    //2. log file
    assert(pnl_.m_notebookResult->GetPageCount() == 1);
    LogPanel* logPanel = new LogPanel(pnl_.m_notebookResult, log); //owned by m_notebookResult
    pnl_.m_notebookResult->AddPage(logPanel, _("Log"), false /*bSelect*/);
    //bSizerHoldStretch->Insert(0, logPanel, 1, wxEXPAND);

    //show log instead of graph if errors occurred! (not required for ignored warnings)
    if (log.getItemCount(MSG_TYPE_ERROR | MSG_TYPE_FATAL_ERROR) > 0)
        pnl_.m_notebookResult->ChangeSelection(pagePosLog);

    //fill image list to cope with wxNotebook image setting design desaster...
    const int imgListSize = getResourceImage(L"log_file_small").GetHeight();
    assert(imgListSize == 16); //Windows default size for panel caption
    auto imgList = std::make_unique<wxImageList>(imgListSize, imgListSize);

    auto addToImageList = [&](const wxBitmap& bmp)
    {
        assert(bmp.GetWidth () <= imgListSize);
        assert(bmp.GetHeight() <= imgListSize);
        imgList->Add(bmp);
    };
    addToImageList(getResourceImage(L"progress_small"));
    addToImageList(getResourceImage(L"log_file_small"));

    pnl_.m_notebookResult->AssignImageList(imgList.release()); //pass ownership

    pnl_.m_notebookResult->SetPageImage(pagePosProgress, pagePosProgress);
    pnl_.m_notebookResult->SetPageImage(pagePosLog,      pagePosLog);

    //Caveat: we need "Show()" *after" the above wxNotebook::ChangeSelection() to get the correct selection on Linux
    pnl_.m_notebookResult->Show();

    //GetSizer()->SetSizeHints(this); //~=Fit() //not a good idea: will shrink even if window is maximized or was enlarged by the user
    pnl_.Layout();

    pnl_.m_panelProgress->Layout();
    //small statistics panels:
    pnl_.m_panelItemsProcessed->Layout();
    pnl_.m_panelItemsRemaining->Layout();
    //pnl.m_panelTimeRemaining->Layout();
    //pnl.m_panelTimeElapsed->Layout(); -> needed?

    //play (optional) sound notification after sync has completed -> only play when waiting on results dialog, seems to be pointless otherwise!
    switch (finalResult_)
    {
        case SyncProgressDialog::RESULT_ABORTED:
            break;
        case SyncProgressDialog::RESULT_FINISHED_WITH_ERROR:
        case SyncProgressDialog::RESULT_FINISHED_WITH_WARNINGS:
        case SyncProgressDialog::RESULT_FINISHED_WITH_SUCCESS:
            if (!soundFileSyncComplete_.empty())
            {
                const Zstring soundFilePath = getResourceDirPf() + soundFileSyncComplete_;
                if (fileAvailable(soundFilePath))
                    wxSound::Play(utfTo<wxString>(soundFilePath), wxSOUND_ASYNC);
                //warning: this may fail and show a wxWidgets error message! => must not play when running FFS without user interaction!
            }
            //if (::GetForegroundWindow() != GetHWND())
            //  RequestUserAttention(); -> probably too much since task bar already alreay is colorized with Taskbar::STATUS_ERROR or STATUS_NORMAL
            break;
    }

    //Raise(); -> don't! user may be watching a movie in the meantime ;) note: resumeFromSystray() also calls Raise()!
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::OnOkay(wxCommandEvent& event)
{
    this->Close(); //generate close event: do NOT destroy window unconditionally!
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::OnCancel(wxCommandEvent& event)
{
    if (abortCb_) abortCb_->userRequestAbort();

    paused_ = false;
    updateStaticGui(); //update status + pause button
    //no Layout() or UI-update here to avoid cascaded Yield()-call!
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::OnPause(wxCommandEvent& event)
{
    paused_ = !paused_;
    updateStaticGui(); //update status + pause button
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::OnClose(wxCloseEvent& event)
{
    //this event handler may be called *during* sync!
    //=> try to stop sync gracefully and cross fingers:
    if (abortCb_)
        abortCb_->userRequestAbort();
    //Note: we must NOT veto dialog destruction, else we will cancel system shutdown if this dialog is application main window (like in batch mode)

    notifyWindowTerminate_(); //don't wait until delayed "Destroy()" finally calls destructor -> avoid calls to showSummary()/closeDirectly()

    paused_ = false; //[!] we could be pausing here!

    //now that we notified window termination prematurely, and since showSummary()/closeDirectly() won't be called, make sure we don't call back, too!
    //e.g. a second notifyWindowTerminate_() in ~SyncProgressDialogImpl()!!!
    syncStat_ = nullptr;
    abortCb_  = nullptr;

    wereDead_ = true;
    this->Destroy(); //wxWidgets OS X: simple "delete"!!!!!!!
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::OnIconize(wxIconizeEvent& event)
{
    /*
        propagate progress dialog minimize/maximize to parent
        -----------------------------------------------------
        Fedora/Debian/Ubuntu:
            - wxDialog cannot be minimized
            - worse, wxGTK sends stray iconize events *after* wxDialog::Destroy()
            - worse, on Fedora an iconize event is issued directly after calling Close()
            - worse, even wxDialog::Hide() causes iconize event!
                => nothing to do
        SUSE:
            - wxDialog can be minimized (it just vanishes!) and in general also minimizes parent: except for our progress wxDialog!!!
            - worse, wxDialog::Hide() causes iconize event
            - probably the same issues with stray iconize events like Fedora/Debian/Ubuntu
            - minimize button is always shown, even if wxMINIMIZE_BOX is omitted!
                => nothing to do
        Mac OS X:
            - wxDialog can be minimized and automatically minimizes parent
            - no iconize events seen by wxWidgets!
                => nothing to do
        Windows:
            - wxDialog can be minimized but does not also minimize parent
            - iconize events only seen for manual minimize
                => propagate event to parent
    */
    event.Skip();
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::minimizeToTray()
{
    if (!trayIcon_.get())
    {
        trayIcon_ = std::make_unique<FfsTrayIcon>([this] { this->resumeFromSystray(); }); //FfsTrayIcon lifetime is a subset of "this"'s lifetime!
        //we may destroy FfsTrayIcon even while in the FfsTrayIcon callback!!!!

        updateProgressGui(false /*allowYield*/); //set tray tooltip + progress: e.g. no updates while paused

        this->Hide();
        if (parentFrame_)
            parentFrame_->Hide();
    }
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::resumeFromSystray()
{
    if (trayIcon_)
    {
        trayIcon_.reset();

        if (parentFrame_)
        {
            //if (parentFrame_->IsIconized()) //caveat: if window is maximized calling Iconize(false) will erroneously un-maximize!
            //    parentFrame_->Iconize(false);
            parentFrame_->Show();
            parentFrame_->Raise();
        }

        //if (IsIconized()) //caveat: if window is maximized calling Iconize(false) will erroneously un-maximize!
        //    Iconize(false);
        this->Show();
        this->Raise();
        this->SetFocus();

        updateStaticGui();                        //restore Windows 7 task bar status   (e.g. required in pause mode)
        updateProgressGui(false  /*allowYield*/); //restore Windows 7 task bar progress (e.g. required in pause mode)

    }
}

//########################################################################################

SyncProgressDialog* fff::createProgressDialog(AbortCallback& abortCb,
                                              const std::function<void()>& notifyWindowTerminate, //note: user closing window cannot be prevented on OS X! (And neither on Windows during system shutdown!)
                                              const Statistics& syncStat,
                                              wxFrame* parentWindow, //may be nullptr
                                              bool showProgress,
                                              bool autoCloseDialog,
                                              const wxString& jobName,
                                              const Zstring& soundFileSyncComplete,
                                              bool ignoreErrors,
                                              PostSyncAction2 postSyncAction)
{
    if (parentWindow) //sync from GUI
    {
        //due to usual "wxBugs", wxDialog on OS X does not float on its parent; wxFrame OTOH does => hack!
        //https://groups.google.com/forum/#!topic/wx-users/J5SjjLaBOQE
        return new SyncProgressDialogImpl<wxDialog>(wxDEFAULT_DIALOG_STYLE | wxMAXIMIZE_BOX | wxMINIMIZE_BOX | wxRESIZE_BORDER,
        [&](wxDialog& progDlg) { return parentWindow; },
        abortCb, notifyWindowTerminate, syncStat, parentWindow, showProgress, autoCloseDialog, jobName, soundFileSyncComplete, ignoreErrors, postSyncAction);
    }
    else //FFS batch job
    {
        auto dlg = new SyncProgressDialogImpl<wxFrame>(wxDEFAULT_FRAME_STYLE,
        [](wxFrame& progDlg) { return &progDlg; },
        abortCb, notifyWindowTerminate, syncStat, parentWindow, showProgress, autoCloseDialog, jobName, soundFileSyncComplete, ignoreErrors, postSyncAction);

        //only top level windows should have an icon:
        dlg->SetIcon(getFfsIcon());
        return dlg;
    }
}

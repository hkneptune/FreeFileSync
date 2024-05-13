// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "progress_indicator.h"
#include <memory>
//#include <wx/imaglist.h>
#include <wx/wupdlock.h>
#include <wx/app.h>
#include <zen/format_unit.h>
//#include <zen/scope_guard.h>
//#include <wx+/toggle_button.h>
#include <wx+/image_tools.h>
#include <wx+/graph.h>
#include <wx+/no_flicker.h>
#include <wx+/window_layout.h>
//#include <zen/file_access.h>
//#include <zen/thread.h>
#include <zen/perf.h>
#include <wx+/choice_enum.h>
#include "wx+/taskbar.h"
#include "gui_generated.h"
#include "tray_icon.h"
#include "log_panel.h"
#include "app_icon.h"
//#include "../ffs_paths.h"
#include "../icon_buffer.h"
#include "../base/speed_test.h"


using namespace zen;
using namespace fff;


namespace
{
constexpr std::chrono::seconds PERF_WINDOW_BYTES_PER_SEC  (4); //window size used for statistics
constexpr std::chrono::seconds PERF_WINDOW_REMAINING_TIME(60); //USB memory stick can have 40-second-hangs
constexpr std::chrono::seconds      SPEED_ESTIMATE_SAMPLE_SKIP(1);
constexpr std::chrono::milliseconds SPEED_ESTIMATE_UPDATE_INTERVAL(500);
constexpr std::chrono::seconds      GRAPH_TOTAL_TIME_UPDATE_INTERVAL(2);

const size_t PROGRESS_GRAPH_SAMPLE_SIZE_MAX = 2'500'000; //sizeof(CurveDataStatistics::Sample) == 16 byte key/value

inline wxColor getColorBytes() { return {111, 255,  99}; } //light green
inline wxColor getColorItems() { return {127, 147, 255}; } //light blue

inline wxColor getColorBytesRim() { return {20, 200,   0}; } //medium green
inline wxColor getColorItemsRim() { return {90, 120, 255}; } //medium blue

//inline wxColor getColorBytesFaint() { return {205, 255, 202}; } //faint green
//inline wxColor getColorItemsFaint() { return {198, 206, 255}; } //faint blue

inline wxColor getColorBytesDark() { return {12, 128,   0}; } //dark green
inline wxColor getColorItemsDark() { return {53,  25, 255}; } //dark blue

inline wxColor getColorLightGrey() { return {0xf2, 0xf2, 0xf2}; }
inline wxColor getColorDarkGrey () { return {0x8f, 0x8f, 0x8f}; }


std::wstring getDialogPhaseText(const Statistics& syncStat, bool paused)
{
    if (paused)
        return _("Paused");

    if (syncStat.taskCancelled())
        return _("Stop requested...");

    switch (syncStat.currentPhase())
    {
        case ProcessPhase::none:
            return _("Initializing..."); //dialog is shown *before* sync starts, so this text may be visible!
        case ProcessPhase::scan:
            return _("Scanning...");
        case ProcessPhase::binaryCompare:
            return _("Comparing content...");
        case ProcessPhase::sync:
            return _("Synchronizing...");
    }
    assert(false);
    return std::wstring();
}


class CurveDataProgressBar : public CurveData
{
public:
    CurveDataProgressBar(bool drawTop) : drawTop_(drawTop) {}

    void setFraction(double fraction) { fraction_ = fraction; } //value between [0, 1]

private:
    std::pair<double, double> getRangeX() const override { return {0, 1}; }

    std::vector<CurvePoint> getPoints(double minX, double maxX, const wxSize& areaSizePx) const override
    {
        const double yLow  = drawTop_ ? 1 : -1; //draw partially out of vertical bounds to not render top/bottom borders of the bars
        const double yHigh = drawTop_ ? 3 :  1; //

        return
        {
            {0,         yHigh},
            {fraction_, yHigh},
            {fraction_, yLow },
            {0,         yLow },
        };
    }

    double fraction_ = 0;
    const bool drawTop_;
};

class CurveDataProgressSeparatorLine : public CurveData
{
    std::pair<double, double> getRangeX() const override { return {0, 1}; }

    std::vector<CurvePoint> getPoints(double minX, double maxX, const wxSize& areaSizePx) const override
    {
        return
        {
            {0, 1},
            {1, 1},
        };
    }
};
}


class CompareProgressPanel::Impl : public CompareProgressDlgGenerated
{
public:
    explicit Impl(wxFrame& parentWindow);

    void init(const Statistics& syncStat, bool ignoreErrors, size_t autoRetryCount); //constructor/destructor semantics, but underlying Window is reused
    void teardown();                                                                 //

    void initNewPhase();
    void updateProgressGui(bool allowYield);

    bool getOptionIgnoreErrors() const            { return ignoreErrors_; }
    void setOptionIgnoreErrors(bool ignoreErrors) { ignoreErrors_ = ignoreErrors; updateStaticGui(); }

    void timerSetStatus(bool active)
    {
        if (active)
            stopWatch_.resume();
        else
            stopWatch_.pause();
    }
    bool timerIsRunning() const { return !stopWatch_.isPaused(); }

    std::chrono::milliseconds pauseAndGetTotalTime()
    {
        stopWatch_.pause();
        return std::chrono::duration_cast<std::chrono::milliseconds>(stopWatch_.elapsed());
    }

private:
    //void onToggleIgnoreErrors(wxCommandEvent& event) override { updateStaticGui(); }

    void updateStaticGui();

    wxFrame& parentWindow_;
    wxString parentTitleBackup_;

    StopWatch stopWatch_;
    std::chrono::nanoseconds phaseStart_{}; //begin of current phase

    const Statistics* syncStat_ = nullptr; //only bound while sync is running

    std::unique_ptr<Taskbar> taskbar_;
    SpeedTest remTimeTest_{PERF_WINDOW_REMAINING_TIME};
    SpeedTest speedTest_  {PERF_WINDOW_BYTES_PER_SEC};

    std::chrono::nanoseconds timeLastSpeedEstimate_ = std::chrono::seconds(-100); //used for calculating intervals between showing and collecting perf samples
    //initial value: just some big number

    SharedRef<CurveDataProgressBar> curveDataBytes_{makeSharedRef<CurveDataProgressBar>(true  /*drawTop*/)};
    SharedRef<CurveDataProgressBar> curveDataItems_{makeSharedRef<CurveDataProgressBar>(false /*drawTop*/)};

    bool ignoreErrors_ = false;
};


CompareProgressPanel::Impl::Impl(wxFrame& parentWindow) :
    CompareProgressDlgGenerated(&parentWindow),
    parentWindow_(parentWindow)
{
    setImage(*m_bitmapItemStat, IconBuffer::genericFileIcon(IconBuffer::IconSize::small));
    setImage(*m_bitmapTimeStat, loadImage("time", -1 /*maxWidth*/, IconBuffer::getPixSize(IconBuffer::IconSize::small)));
    m_bitmapTimeStat->SetMinSize({-1, screenToWxsize(IconBuffer::getPixSize(IconBuffer::IconSize::small))});

    setImage(*m_bitmapErrors,   loadImage("msg_error",   dipToScreen(getMenuIconDipSize())));
    setImage(*m_bitmapWarnings, loadImage("msg_warning", dipToScreen(getMenuIconDipSize())));

    setImage(*m_bitmapIgnoreErrors, loadImage("error_ignore_active", dipToScreen(getMenuIconDipSize())));
    setImage(*m_bitmapRetryErrors,  loadImage("error_retry",         dipToScreen(getMenuIconDipSize())));

    //make sure standard height matches ProcessPhase::binaryCompare statistics layout (== largest)

    //init graph
    m_panelProgressGraph->setAttributes(Graph2D::MainAttributes().setMinY(0).setMaxY(2).
                                        setLabelX(XLabelPos::none).
                                        setLabelY(YLabelPos::none).
                                        setBaseColors(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT), wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE)).
                                        setSelectionMode(GraphSelMode::none));

    m_panelProgressGraph->addCurve(curveDataBytes_, Graph2D::CurveAttributes().setLineWidth(dipToWxsize(1)).fillPolygonArea(getColorBytes()).setColor(Graph2D::getBorderColor()));
    m_panelProgressGraph->addCurve(curveDataItems_, Graph2D::CurveAttributes().setLineWidth(dipToWxsize(1)).fillPolygonArea(getColorItems()).setColor(Graph2D::getBorderColor()));

    m_panelProgressGraph->addCurve(makeSharedRef<CurveDataProgressSeparatorLine>(), Graph2D::CurveAttributes().setLineWidth(dipToWxsize(1)).setColor(Graph2D::getBorderColor()));

    Layout();
    m_panelItemStats->Layout();
    m_panelTimeStats->Layout();
    m_panelErrorStats->Layout();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    //Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
}


void CompareProgressPanel::Impl::init(const Statistics& syncStat, bool ignoreErrors, size_t autoRetryCount)
{
    assert(!syncStat_);
    syncStat_ = &syncStat;
    parentTitleBackup_ = parentWindow_.GetTitle();

    try //try to get access to Windows 7/Ubuntu taskbar
    {
        taskbar_ = std::make_unique<Taskbar>(this); //throw TaskbarNotAvailable
    }
    catch (const TaskbarNotAvailable&) {}

    stopWatch_ = StopWatch(); //reset to measure total time

    setText(*m_staticTextRetryCount, L'(' + formatNumber(autoRetryCount) + MULT_SIGN + L')');
    bSizerErrorsRetry->Show(autoRetryCount > 0);

    //allow changing a few options dynamically during sync
    ignoreErrors_ = ignoreErrors;

    updateStaticGui();

    initNewPhase();
}


void CompareProgressPanel::Impl::teardown()
{
    assert(stopWatch_.isPaused()); //why wasn't pauseAndGetTotalTime() called?

    syncStat_ = nullptr;
    parentWindow_.SetTitle(parentTitleBackup_);
    taskbar_.reset();
}


void CompareProgressPanel::Impl::initNewPhase()
{
    //start new measurement
    remTimeTest_.clear();
    speedTest_  .clear();
    timeLastSpeedEstimate_ = std::chrono::seconds(-100); //make sure estimate is updated upon next check
    phaseStart_ = stopWatch_.elapsed();

    const int     itemsTotal = syncStat_->getTotalStats().items;
    const int64_t bytesTotal = syncStat_->getTotalStats().bytes;

    const bool haveTotalStats = itemsTotal >= 0 || bytesTotal >= 0;

    if (taskbar_.get()) taskbar_->setStatus(haveTotalStats ? Taskbar::Status::normal : Taskbar::Status::indeterminate);

    m_staticTextProcessed     ->Show(haveTotalStats);
    m_staticTextRemaining     ->Show(haveTotalStats);
    m_staticTextItemsRemaining->Show(haveTotalStats);
    m_staticTextBytesRemaining->Show(haveTotalStats);
    m_staticTextTimeRemaining ->Show(haveTotalStats);
    bSizerProgressGraph       ->Show(haveTotalStats);

    Layout();                   //
    m_panelItemStats->Layout(); //redundant? can we trust updateProgressGui() to do the same after detecting "layoutChanged"?
    m_panelTimeStats->Layout(); //

    updateProgressGui(false /*allowYield*/);
}


void CompareProgressPanel::Impl::updateStaticGui()
{
    bSizerErrorsIgnore->Show(ignoreErrors_);
    Layout();
}


void CompareProgressPanel::Impl::updateProgressGui(bool allowYield)
{
    assert(syncStat_);
    if (!syncStat_) //no comparison running!?
        return;

    auto setTitle = [&](const wxString& title)
    {
        if (parentWindow_.GetTitle() != title)
            parentWindow_.SetTitle(title);
    };

    bool layoutChanged = false; //avoid screen flicker by calling layout() only if necessary
    const std::chrono::nanoseconds timeElapsed = stopWatch_.elapsed();

    const int     itemsCurrent = syncStat_->getCurrentStats().items;
    const int64_t bytesCurrent = syncStat_->getCurrentStats().bytes;
    const int     itemsTotal   = syncStat_->getTotalStats  ().items;
    const int64_t bytesTotal   = syncStat_->getTotalStats  ().bytes;

    const bool haveTotalStats = itemsTotal >= 0 || bytesTotal >= 0;

    //status texts
    setText(*m_staticTextStatus, replaceCpy(syncStat_->currentStatusText(), L'\n', L' ')); //no layout update for status texts!

    if (!haveTotalStats)
    {
        //dialog caption, taskbar
        setTitle(formatNumber(itemsCurrent) + L' ' + getDialogPhaseText(*syncStat_, false /*paused*/));

        //progress indicators
        //taskbar_ already set to STATUS_INDETERMINATE by initNewPhase()
    }
    else
    {
        //add both bytes + item count, to handle "deletion-only" cases
        const double fractionTotal = bytesTotal + itemsTotal == 0 ? 0 : 1.0 * (bytesCurrent + itemsCurrent) / (bytesTotal + itemsTotal);
        const double fractionBytes = bytesTotal == 0 ? 0 : 1.0 * bytesCurrent / bytesTotal;
        const double fractionItems = itemsTotal == 0 ? 0 : 1.0 * itemsCurrent / itemsTotal;

        //dialog caption, taskbar
        setTitle(formatProgressPercent(fractionTotal) + L' ' + getDialogPhaseText(*syncStat_, false /*paused*/));

        //progress indicators
        if (taskbar_.get()) taskbar_->setProgress(fractionTotal);

        curveDataBytes_.ref().setFraction(fractionBytes);
        curveDataItems_.ref().setFraction(fractionItems);
    }

    //item and data stats
    if (!haveTotalStats)
    {
        setText(*m_staticTextItemsProcessed, formatNumber(itemsCurrent), &layoutChanged);
        setText(*m_staticTextBytesProcessed, L"", &layoutChanged);
    }
    else
    {
        setText(*m_staticTextItemsProcessed,               formatNumber(itemsCurrent), &layoutChanged);
        setText(*m_staticTextBytesProcessed, L'(' + formatFilesizeShort(bytesCurrent) + L')', &layoutChanged);

        setText(*m_staticTextItemsRemaining, formatNumber(itemsTotal - itemsCurrent), &layoutChanged);
        setText(*m_staticTextBytesRemaining, L'(' + formatFilesizeShort(bytesTotal - bytesCurrent) + L')', &layoutChanged);
    }

    auto showIfNeeded = [&](wxWindow& wnd, bool show)
    {
        if (wnd.IsShown() != show)
        {
            wnd.Show(show);
            layoutChanged = true;
        }
    };

    //errors and warnings (pop up dynamically)
    const Statistics::ErrorStats errorStats = syncStat_->getErrorStats();

    showIfNeeded(*m_staticTextErrors,   errorStats.errorCount != 0);
    showIfNeeded(*m_staticTextWarnings, errorStats.warningCount != 0);
    showIfNeeded(*m_panelErrorStats, errorStats.errorCount != 0 || errorStats.warningCount != 0);

    if (m_panelErrorStats->IsShown())
    {
        showIfNeeded(*m_bitmapErrors,         errorStats.errorCount != 0);
        showIfNeeded(*m_staticTextErrorCount, errorStats.errorCount != 0);

        if (m_staticTextErrorCount->IsShown())
            setText(*m_staticTextErrorCount, formatNumber(errorStats.errorCount), &layoutChanged);

        showIfNeeded(*m_bitmapWarnings,         errorStats.warningCount != 0);
        showIfNeeded(*m_staticTextWarningCount, errorStats.warningCount != 0);

        if (m_staticTextWarningCount->IsShown())
            setText(*m_staticTextWarningCount, formatNumber(errorStats.warningCount), &layoutChanged);
    }

    //current time elapsed
    const int64_t timeElapSec = std::chrono::duration_cast<std::chrono::seconds>(timeElapsed).count();

    setText(*m_staticTextTimeElapsed, utfTo<wxString>(formatTimeSpan(timeElapSec, true /*hourOptional*/)), &layoutChanged);

    if (haveTotalStats) //remaining time and speed: only visible during binary comparison
        if (numeric::dist(timeLastSpeedEstimate_, timeElapsed) >= SPEED_ESTIMATE_UPDATE_INTERVAL)
        {
            timeLastSpeedEstimate_ = timeElapsed;

            if (numeric::dist(phaseStart_, timeElapsed) >= SPEED_ESTIMATE_SAMPLE_SKIP) //discard stats for first second: probably messy
            {
                remTimeTest_.addSample(timeElapsed, itemsCurrent, bytesCurrent);
                speedTest_  .addSample(timeElapsed, itemsCurrent, bytesCurrent);
            }

            //current speed -> Win 7 copy uses 1 sec update interval instead
            m_panelProgressGraph->setAttributes(m_panelProgressGraph->getAttributes().setCornerText(speedTest_.getBytesPerSecFmt(), GraphCorner::topL));
            m_panelProgressGraph->setAttributes(m_panelProgressGraph->getAttributes().setCornerText(speedTest_.getItemsPerSecFmt(), GraphCorner::bottomL));

            //remaining time: display with relative error of 10% - based on samples taken every 0.5 sec only
            //-> call more often than once per second to correctly show last few seconds countdown, but don't call too often to avoid occasional jitter
            std::optional<double> remTimeSec = remTimeTest_.getRemainingSec(itemsTotal - itemsCurrent, bytesTotal - bytesCurrent);
            setText(*m_staticTextTimeRemaining, remTimeSec ? formatRemainingTime(*remTimeSec) : std::wstring(1, EM_DASH), &layoutChanged);
        }

    if (haveTotalStats)
        m_panelProgressGraph->Refresh();

    //adapt layout after content changes above
    if (layoutChanged)
    {
        Layout();
        m_panelItemStats->Layout();
        m_panelTimeStats->Layout();
        if (m_panelErrorStats->IsShown())
            m_panelErrorStats->Layout();
    }

    //do the ui update
    if (allowYield)
        wxTheApp->Yield(); //pump GUI messages
    else
        this->Update(); //don't wait until next idle event (who knows what blocking process comes next?)
}

//########################################################################################

//redirect to implementation
CompareProgressPanel::CompareProgressPanel(wxFrame& parentWindow) : pimpl_(new Impl(parentWindow)) {} //owned by parentWindow
wxWindow* CompareProgressPanel::getAsWindow() { return pimpl_; }
void CompareProgressPanel::init(const Statistics& syncStat, bool ignoreErrors, size_t autoRetryCount) { pimpl_->init(syncStat, ignoreErrors, autoRetryCount); }
void CompareProgressPanel::teardown()    { pimpl_->teardown(); }
void CompareProgressPanel::initNewPhase() { pimpl_->initNewPhase(); }
void CompareProgressPanel::updateGui()   { pimpl_->updateProgressGui(true /*allowYield*/); }
bool CompareProgressPanel::getOptionIgnoreErrors() const { return pimpl_->getOptionIgnoreErrors(); }
void CompareProgressPanel::setOptionIgnoreErrors(bool ignoreErrors) { pimpl_->setOptionIgnoreErrors(ignoreErrors); }
void CompareProgressPanel::timerSetStatus(bool active) { pimpl_->timerSetStatus(active); }
bool CompareProgressPanel::timerIsRunning() const { return pimpl_->timerIsRunning(); }
std::chrono::milliseconds CompareProgressPanel::pauseAndGetTotalTime() { return pimpl_->pauseAndGetTotalTime(); }
//########################################################################################

namespace
{
class CurveDataStatistics : public SparseCurveData
{
public:
    CurveDataStatistics() : SparseCurveData(true /*addSteps*/) {}

    void clear() { samples_.clear(); lastSample_ = {}; }

    void addSample(double timeElapsed /*[sec]*/, double value /*[items|bytes]*/)
    {
        assert(( samples_.empty() && lastSample_.x == 0 && lastSample_.y == 0) ||
               (!samples_.empty() && samples_.back().x <= lastSample_.x));

        if (timeElapsed < lastSample_.x) //time *required* to be monotonously ascending for std::partition_point
        {
            assert(false);
            return;
        }

        lastSample_ = {timeElapsed, value};

        //allow for at most one sample per 100ms (handles duplicate inserts, too!) => unrelated to UI_UPDATE_INTERVAL!
        if (!samples_.empty() && timeElapsed - samples_.back().x < 0.1)
            return;

        samples_.push_back(CurvePoint{timeElapsed, value});

        if (samples_.size() > PROGRESS_GRAPH_SAMPLE_SIZE_MAX) //limit buffer size
            samples_.pop_front();
    }

private:
    std::pair<double, double> getRangeX() const override
    {
        if (samples_.empty())
            return {};
        /*
            //report some additional width by 5% elapsed time to make graph recalibrate before hitting the right border
            //caveat: graph for batch mode binary comparison does NOT start at elapsed time 0!! ProcessPhase::binaryCompare and ProcessPhase::sync!
            //=> consider width of current sample set!
            upperEndMs += 0.05 *(upperEndMs - samples.begin()->first);
        */
        return {samples_.front().x, //need not start with 0, e.g. "binary comparison, graph reset, followed by sync"
                lastSample_.x};
    }

    std::optional<CurvePoint> getLessEq(double x) const override //x: seconds since begin
    {
        //--------- add artifical last sample value --------
        if (!samples_.empty() && lastSample_.x <= x)
            return lastSample_;
        //--------------------------------------------------

        //find first item > x, then go one step back:
        auto it = std::partition_point(samples_.begin(), samples_.end(),
        /*find first item for which "!pred"*/ [x](const CurvePoint& p) { return p.x <= x; });
        if (it == samples_.begin())
            return std::nullopt;
        --it; //bound!
        return *it;
    }

    std::optional<CurvePoint> getGreaterEq(double x) const override
    {
        //find first item >= x
        const auto it = std::partition_point(samples_.begin(), samples_.end(),
        /*find first item for which "!pred"*/ [x](const CurvePoint& p) { return p.x < x; });
        if (it != samples_.end())
            return *it;

        //--------- add artifical last sample value --------
        if (!samples_.empty() && x <= lastSample_.x)
            return lastSample_;
        //--------------------------------------------------
        return std::nullopt;
    }

    RingBuffer<CurvePoint> samples_; //x: monotonously ascending with time!
    CurvePoint lastSample_; //artificial record after end of samples to visualize current time!
};


class CurveDataEstimate : public CurveData
{
public:
    void setValue(double x1, double x2, double y1, double y2) { x1_ = x1; x2_ = x2; y1_ = y1; y2_ = y2; }
    void setTotalTime(double x2) { x2_ = x2; }
    double getTotalTime() const { return x2_; }

private:
    std::pair<double, double> getRangeX() const override { return {x1_, x2_}; }

    std::vector<CurvePoint> getPoints(double minX, double maxX, const wxSize& areaSizePx) const override
    {
        return
        {
            {x1_, y1_},
            {x2_, y2_},
        };
    }

    double x1_ = 0; //elapsed time [s]
    double x2_ = 0; //total time [s] (estimated)
    double y1_ = 0; //items/bytes processed
    double y2_ = 0; //items/bytes total
};


class CurveDataTimeMarker : public CurveData
{
public:
    void setValue(double x, double y) { x_ = x; y_ = y; }
    void setTime(double x) { x_ = x; }

private:
    std::pair<double, double> getRangeX() const override { return {x_, x_}; }

    std::vector<CurvePoint> getPoints(double minX, double maxX, const wxSize& areaSizePx) const override
    {
        return
        {
            {x_, y_},
            {x_, 0 },
        };
    }

    double x_ = 0; //time [s]
    double y_ = 0; //items/bytes
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
        const double k = std::floor(std::log(bytesProposed) / std::numbers::ln2);
        const double e = std::pow(2.0, k);
        if (numeric::isNull(e))
            return 0;
        const double a = bytesProposed / e; //bytesProposed = a * 2^k with a in [1, 2)
        assert(1 <= a && a < 2);
        const double steps[] = {1, 2};
        return e * numeric::roundToGrid(a, std::begin(steps), std::end(steps));
    }

    wxString formatText(double value, double optimalBlockSize) const override { return formatFilesizeShort(static_cast<int64_t>(value)); }
};


struct LabelFormatterItemCount : public LabelFormatter
{
    double getOptimalBlockSize(double itemsProposed) const override
    {
        itemsProposed *= stretchDefaultBlockSize; //enlarge block default size

        const double steps[] = {1, 2, 5, 10};
        if (itemsProposed <= 10)
            return numeric::roundToGrid(itemsProposed, std::begin(steps), std::end(steps)); //like nextNiceNumber(), but without the 2.5 step!
        return nextNiceNumber(itemsProposed);
    }

    wxString formatText(double value, double optimalBlockSize) const override
    {
        return formatNumber(std::round(value)); //not enough room for a "%x items" representation
    }
};


struct LabelFormatterTimeElapsed : public LabelFormatter
{
    double getOptimalBlockSize(double secProposed) const override
    {
        //5 sec minimum block size
        const double stepsSec[] = {5, 10, 20, 30, 60}; //nice numbers for seconds
        if (secProposed <= 60)
            return numeric::roundToGrid(secProposed, std::begin(stepsSec), std::end(stepsSec));

        const double stepsMin[] = {1, 2, 5, 10, 15, 20, 30, 60}; //nice numbers for minutes
        if (secProposed <= 3600)
            return 60 * numeric::roundToGrid(secProposed / 60, std::begin(stepsMin), std::end(stepsMin));

        if (secProposed <= 3600 * 24)
            return 3600 * nextNiceNumber(secProposed / 3600); //round to full hours

        return 24 * 3600 * nextNiceNumber(secProposed / (24 * 3600)); //round to full days
    }

    wxString formatText(double timeElapsed, double optimalBlockSize) const override
    {
        const int64_t timeElapsedSec = std::round(timeElapsed);
        if (timeElapsedSec < 60)
            return _P("1 sec", "%x sec", timeElapsedSec);

        return utfTo<wxString>(formatTimeSpan(timeElapsedSec, true /*hourOptional*/));
    }
};
}


template <class TopLevelDialog> //can be a wxFrame or wxDialog
class SyncProgressDialogImpl : public TopLevelDialog, public SyncProgressDialog
/*  we need derivation, not composition:
      1. SyncProgressDialogImpl IS a wxFrame/wxDialog
      2. implement virtual ~wxFrame()
      3. event handling below assumes lifetime is larger-equal than wxFrame's      */
{
public:
    SyncProgressDialogImpl(long style, //wxFrame/wxDialog style
                           const WindowLayout::Dimensions& dim,
                           const std::function<void()>& userRequestCancel,
                           const Statistics& syncStat,
                           wxFrame* parentFrame,
                           bool showProgress,
                           bool autoCloseDialog,
                           const std::vector<std::wstring>& jobNames,
                           time_t syncStartTime,
                           bool ignoreErrors,
                           size_t autoRetryCount,
                           PostSyncAction postSyncAction);

    Result destroy(bool autoClose, bool restoreParentFrame, TaskResult syncResult, const SharedRef<const zen::ErrorLog>& log) override;

    wxWindow* getWindowIfVisible() override { return this->IsShown() ? this : nullptr; }
    //workaround macOS bug: if "this" is used as parent window for a modal dialog then this dialog will erroneously un-hide its parent!

    void initNewPhase        () override;
    void notifyProgressChange() override;
    void updateGui           () override { updateProgressGui(true /*allowYield*/); }

    bool getOptionIgnoreErrors()                 const override { return ignoreErrors_; }
    void setOptionIgnoreErrors(bool ignoreErrors)      override { ignoreErrors_ = ignoreErrors; updateStaticGui(); }
    PostSyncAction getOptionPostSyncAction()    const override { return getEnumVal(enumPostSyncAction_, *pnl_.m_choicePostSyncAction); }
    bool getOptionAutoCloseDialog()              const override { return pnl_.m_checkBoxAutoClose->GetValue(); }

    void timerSetStatus(bool active) override
    {
        if (active)
            stopWatch_.resume();
        else
            stopWatch_.pause();
    }

    bool timerIsRunning() const override { return !stopWatch_.isPaused(); }

    std::chrono::milliseconds pauseAndGetTotalTime() override
    {
        stopWatch_.pause();
        return std::chrono::duration_cast<std::chrono::milliseconds>(stopWatch_.elapsed());
    }

private:
    void onLocalKeyEvent (wxKeyEvent& event);
    void onParentKeyEvent(wxKeyEvent& event);
    void onPause  (wxCommandEvent& event);
    void onCancel (wxCommandEvent& event);
    void onClose(wxCloseEvent& event);
    void onIconize(wxIconizeEvent& event);
    //void onToggleIgnoreErrors(wxCommandEvent& event) { updateStaticGui(); }

    void showSummary(TaskResult syncResult, const SharedRef<const ErrorLog>& log);

    void minimizeToTray();
    void resumeFromSystray(bool userRequested);

    void updateStaticGui();
    void updateProgressGui(bool allowYield);

    void setExternalStatus(const wxString& status, const wxString& progress); //progress may be empty!

    SyncProgressPanelGenerated& pnl_; //wxPanel containing the GUI controls of *this

    const TimeComp syncStartTime_;
    const wxString jobName_;
    StopWatch stopWatch_;

    wxFrame* parentFrame_; //optional

    const std::function<void()> userRequestAbort_; //cancel button or dialog close

    //status variables
    const Statistics* syncStat_; //valid only while sync is running
    bool paused_ = false;
    bool closePressed_ = false;

    //remaining time
    SpeedTest remTimeTest_{PERF_WINDOW_REMAINING_TIME};
    SpeedTest speedTest_  {PERF_WINDOW_BYTES_PER_SEC};
    std::chrono::nanoseconds timeLastSpeedEstimate_    = std::chrono::seconds(-100); //used for calculating intervals between collecting perf samples
    std::chrono::nanoseconds timeLastGraphTotalUpdate_ = std::chrono::seconds(-100);

    //help calculate total speed
    std::chrono::nanoseconds phaseStart_{}; //begin of current phase

    SharedRef<CurveDataStatistics> curveBytes_          = makeSharedRef<CurveDataStatistics>();
    SharedRef<CurveDataStatistics> curveItems_          = makeSharedRef<CurveDataStatistics>();
    SharedRef<CurveDataEstimate  > curveBytesEstim_     = makeSharedRef<CurveDataEstimate  >();
    SharedRef<CurveDataEstimate  > curveItemsEstim_     = makeSharedRef<CurveDataEstimate  >();
    SharedRef<CurveDataTimeMarker> curveBytesTimeNow_   = makeSharedRef<CurveDataTimeMarker>();
    SharedRef<CurveDataTimeMarker> curveItemsTimeNow_   = makeSharedRef<CurveDataTimeMarker>();
    SharedRef<CurveDataTimeMarker> curveBytesTimeEstim_ = makeSharedRef<CurveDataTimeMarker>();
    SharedRef<CurveDataTimeMarker> curveItemsTimeEstim_ = makeSharedRef<CurveDataTimeMarker>();

    wxString parentTitleBackup_;
    std::unique_ptr<FfsTrayIcon> trayIcon_; //optional: if filled all other windows should be hidden and conversely
    std::unique_ptr<Taskbar> taskbar_;

    bool ignoreErrors_ = false;
    EnumDescrList<PostSyncAction> enumPostSyncAction_;
};


template <class TopLevelDialog>
SyncProgressDialogImpl<TopLevelDialog>::SyncProgressDialogImpl(long style, //wxFrame/wxDialog style
                                                               const WindowLayout::Dimensions& dim,
                                                               const std::function<void()>& userRequestCancel,
                                                               const Statistics& syncStat,
                                                               wxFrame* parentFrame,
                                                               bool showProgress,
                                                               bool autoCloseDialog,
                                                               const std::vector<std::wstring>& jobNames,
                                                               time_t syncStartTime,
                                                               bool ignoreErrors,
                                                               size_t autoRetryCount,
                                                               PostSyncAction postSyncAction) :
    TopLevelDialog(parentFrame, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, style), //title is overwritten anyway in setExternalStatus()
    pnl_(*new SyncProgressPanelGenerated(this)), //ownership passed to "this"
    syncStartTime_(getLocalTime(syncStartTime)), //returns TimeComp() on error
    jobName_([&]
{
    std::wstring tmp;
    if (!jobNames.empty())
    {
        tmp = jobNames[0];
        std::for_each(jobNames.begin() + 1, jobNames.end(), [&](const std::wstring& jobName)
        { tmp += L" + " + jobName; });
    }
    return tmp;
}
()),
parentFrame_(parentFrame),
userRequestAbort_(userRequestCancel),
syncStat_(&syncStat)
{
    static_assert(std::is_same_v<TopLevelDialog, wxFrame > ||
                  std::is_same_v<TopLevelDialog, wxDialog>);
    assert((std::is_same_v<TopLevelDialog, wxFrame> == !parentFrame));

    //finish construction of this dialog:
    this->pnl_.m_panelProgress->SetMinSize({dipToWxsize(550), dipToWxsize(340)});

    wxBoxSizer* bSizer170 = new wxBoxSizer(wxVERTICAL);
    bSizer170->Add(&pnl_, 1, wxEXPAND);
    this->SetSizer(bSizer170); //pass ownership

    //lifetime of event sources is subset of this instance's lifetime => no wxEvtHandler::Unbind() needed
    this->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&   event) { onClose(event); });
    this->Bind(wxEVT_ICONIZE,      [this](wxIconizeEvent& event) { onIconize(event); });
    this->Bind(wxEVT_CHAR_HOOK,    [this](wxKeyEvent&     event) { onLocalKeyEvent(event); });

    pnl_.m_buttonClose           ->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](wxCommandEvent& event) { closePressed_ = true; });
    pnl_.m_buttonPause           ->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](wxCommandEvent& event) { onPause(event); });
    pnl_.m_buttonStop            ->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](wxCommandEvent& event) { onCancel(event); });
    pnl_.m_bpButtonMinimizeToTray->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](wxCommandEvent& event) { minimizeToTray(); });

    if (parentFrame_)
        parentFrame_->Bind(wxEVT_CHAR_HOOK, &SyncProgressDialogImpl::onParentKeyEvent, this);


    assert(pnl_.m_buttonClose->GetId() == wxID_OK); //we cannot use wxID_CLOSE else ESC key won't work: yet another wxWidgets bug??

    setRelativeFontSize(*pnl_.m_staticTextPhase, 1.5);
    setRelativeFontSize(*pnl_.m_staticTextPercentTotal, 1.5);

    if (parentFrame_)
        parentTitleBackup_ = parentFrame_->GetTitle(); //save old title (will be used as progress indicator)

    //pnl.m_animCtrlSyncing->SetAnimation(getResourceAnimation(L"working"));
    //pnl.m_animCtrlSyncing->Play();

    //this->EnableCloseButton(false); //this is NOT honored on OS X or with ALT+F4 on Windows! -> why disable button at all??

    try //try to get access to Windows 7/Ubuntu taskbar
    {
        taskbar_ = std::make_unique<Taskbar>(this); //throw TaskbarNotAvailable
    }
    catch (const TaskbarNotAvailable&) {}

    //hide until end of process:
    pnl_.m_notebookResult     ->Hide();
    pnl_.m_buttonClose        ->Show(false);
    //set std order after button visibility was set
    setStandardButtonLayout(*pnl_.bSizerStdButtons, StdButtons().setAffirmative(pnl_.m_buttonPause).setCancel(pnl_.m_buttonStop));

    setImage(*pnl_.m_bpButtonMinimizeToTray, loadImage("minimize_to_tray"));

    setImage(*pnl_.m_bitmapItemStat, IconBuffer::genericFileIcon(IconBuffer::IconSize::small));
    setImage(*pnl_.m_bitmapTimeStat, loadImage("time", -1 /*maxWidth*/, IconBuffer::getPixSize(IconBuffer::IconSize::small)));
    pnl_.m_bitmapTimeStat->SetMinSize({-1, screenToWxsize(IconBuffer::getPixSize(IconBuffer::IconSize::small))});

    setImage(*pnl_.m_bitmapErrors,   loadImage("msg_error",   dipToScreen(getMenuIconDipSize())));
    setImage(*pnl_.m_bitmapWarnings, loadImage("msg_warning", dipToScreen(getMenuIconDipSize())));

    setImage(*pnl_.m_bitmapIgnoreErrors, loadImage("error_ignore_active", dipToScreen(getMenuIconDipSize())));
    setImage(*pnl_.m_bitmapRetryErrors,  loadImage("error_retry",         dipToScreen(getMenuIconDipSize())));

    //init graph
    const int xLabelHeight = this->GetCharHeight() + dipToWxsize(2) /*margin*/; //use same height for both graphs to make sure they stretch evenly
    const int yLabelWidth  = dipToWxsize(70);
    pnl_.m_panelGraphBytes->setAttributes(Graph2D::MainAttributes().
                                          setLabelX(XLabelPos::top,   xLabelHeight, std::make_shared<LabelFormatterTimeElapsed>()).
                                          setLabelY(YLabelPos::right, yLabelWidth,  std::make_shared<LabelFormatterBytes>()).
                                          setBaseColors(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT), wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)).
                                          setSelectionMode(GraphSelMode::none));

    pnl_.m_panelGraphItems->setAttributes(Graph2D::MainAttributes().
                                          setLabelX(XLabelPos::bottom, xLabelHeight, std::make_shared<LabelFormatterTimeElapsed>()).
                                          setLabelY(YLabelPos::right,  yLabelWidth,  std::make_shared<LabelFormatterItemCount>()).
                                          setBaseColors(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT), wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)).
                                          setSelectionMode(GraphSelMode::none));

    pnl_.m_panelGraphBytes->addCurve(curveBytes_, Graph2D::CurveAttributes().setLineWidth(dipToWxsize(1)).fillCurveArea(getColorBytes()).setColor(getColorBytesRim()));
    pnl_.m_panelGraphItems->addCurve(curveItems_, Graph2D::CurveAttributes().setLineWidth(dipToWxsize(1)).fillCurveArea(getColorItems()).setColor(getColorItemsRim()));

    pnl_.m_panelGraphBytes->addCurve(curveBytesEstim_, Graph2D::CurveAttributes().setLineWidth(dipToWxsize(1)).fillCurveArea(getColorLightGrey()).setColor(getColorDarkGrey()));
    pnl_.m_panelGraphItems->addCurve(curveItemsEstim_, Graph2D::CurveAttributes().setLineWidth(dipToWxsize(1)).fillCurveArea(getColorLightGrey()).setColor(getColorDarkGrey()));

    pnl_.m_panelGraphBytes->addCurve(curveBytesTimeNow_, Graph2D::CurveAttributes().setLineWidth(dipToWxsize(2)).setColor(getColorBytesDark()));
    pnl_.m_panelGraphItems->addCurve(curveItemsTimeNow_, Graph2D::CurveAttributes().setLineWidth(dipToWxsize(2)).setColor(getColorItemsDark()));

    pnl_.m_panelGraphBytes->addCurve(curveBytesTimeEstim_, Graph2D::CurveAttributes().setLineWidth(dipToWxsize(2)).setColor(getColorDarkGrey()));
    pnl_.m_panelGraphItems->addCurve(curveItemsTimeEstim_, Graph2D::CurveAttributes().setLineWidth(dipToWxsize(2)).setColor(getColorDarkGrey()));

    //graph legend:
    const wxSize squareSize{this->GetCharHeight(), this->GetCharHeight()};
    setImage(*pnl_.m_bitmapGraphKeyBytes, rectangleImage({wxsizeToScreen(squareSize.x), wxsizeToScreen(squareSize.y)}, getColorBytes(), getColorBytesRim(), dipToScreen(1)));
    setImage(*pnl_.m_bitmapGraphKeyItems, rectangleImage({wxsizeToScreen(squareSize.x), wxsizeToScreen(squareSize.y)}, getColorItems(), getColorItemsRim(), dipToScreen(1)));

    pnl_.bSizerDynSpace->SetMinSize(yLabelWidth, -1); //ensure item/time stats are nicely centered

    setText(*pnl_.m_staticTextRetryCount, L'(' + formatNumber(autoRetryCount) + MULT_SIGN + L')');
    pnl_.bSizerErrorsRetry->Show(autoRetryCount > 0);

    //allow changing a few options dynamically during sync
    ignoreErrors_ = ignoreErrors;

    enumPostSyncAction_.add(PostSyncAction::none, L"");
    if (parentFrame_) //enable EXIT option for gui mode sync
        enumPostSyncAction_.add(PostSyncAction::exit, wxControl::RemoveMnemonics(_("E&xit"))); //reuse label translation
    enumPostSyncAction_.add(PostSyncAction::sleep,    _("System: Sleep"));
    enumPostSyncAction_.add(PostSyncAction::shutdown, _("System: Shut down"));

    setEnumVal(enumPostSyncAction_, *pnl_.m_choicePostSyncAction, postSyncAction);

    pnl_.m_checkBoxAutoClose->SetValue(autoCloseDialog);

    updateStaticGui(); //null-status will be shown while waiting for dir locks

    //make sure that standard height matches ProcessPhase::binaryCompare statistics layout (== largest)

    this->GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
#ifdef __WXGTK3__
    this->Show(); //GTK3 size calculation requires visible window: https://github.com/wxWidgets/wxWidgets/issues/16088
    //Hide(); -> avoids old position flash before Center() on GNOME but causes hang on KDE? https://freefilesync.org/forum/viewtopic.php?t=10103#p42404
#endif
    pnl_.Layout();
    this->Center(); //call *after* dialog layout update and *before* wxWindow::Show()!

    WindowLayout::setInitial(*this, dim, this->GetSize() /*defaultSize*/);

    pnl_.m_buttonStop->SetDefault();

    if (showProgress)
    {
        this->Show();
        //clear gui flicker, remove dummy texts: window must be visible to make this work!
        updateProgressGui(true /*allowYield*/); //at least on OS X a real Yield() is required to flush pending GUI updates; Update() is not enough

        setFocusIfActive(*pnl_.m_buttonStop); //don't steal focus when starting in sys-tray!
    }
    else
        minimizeToTray();
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
    curveBytes_         .ref().clear();
    curveItems_         .ref().clear();
    curveBytesEstim_    .ref().setValue(0, 0, 0, 0);
    curveItemsEstim_    .ref().setValue(0, 0, 0, 0);
    curveBytesTimeNow_  .ref().setValue(0, 0);
    curveItemsTimeNow_  .ref().setValue(0, 0);
    curveBytesTimeEstim_.ref().setValue(0, 0);
    curveItemsTimeEstim_.ref().setValue(0, 0);

    notifyProgressChange(); //make sure graphs get initial values

    //start new measurement
    remTimeTest_.clear();
    speedTest_  .clear();
    timeLastGraphTotalUpdate_ = timeLastSpeedEstimate_ = std::chrono::seconds(-100); //make sure estimate is updated upon next check
    phaseStart_ = stopWatch_.elapsed();

    updateProgressGui(false /*allowYield*/);
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::notifyProgressChange() //noexcept!
{
    if (syncStat_) //sync running
    {
        const double timeElapsedDouble = std::chrono::duration<double>(stopWatch_.elapsed()).count();
        const ProgressStats stats = syncStat_->getCurrentStats();
        curveBytes_.ref().addSample(timeElapsedDouble, stats.bytes);
        curveItems_.ref().addSample(timeElapsedDouble, stats.items);
    }
}


namespace
{
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::setExternalStatus(const wxString& status, const wxString& progress) //progress may be empty!
{
    //sys tray: order "top-down": jobname, status, progress
    wxString tooltip = L"FreeFileSync";
    if (!jobName_.empty())
        tooltip += SPACED_DASH + jobName_;

    tooltip += L'\n' + status;

    if (!progress.empty())
        tooltip += L' ' + progress;

    //window caption/taskbar; inverse order: progress, status, jobname
    wxString title;
    if (!progress.empty())
        title += progress + L' ';

    title += status;

    if (!jobName_.empty() && !parentFrame_ /*job name already visible in sync config panel, unlike with batch jobs*/)
        title += SPACED_DASH + jobName_;

#if 0 //why again does start time have to be visible in the titel!?
    const Zchar* format = [&tc = syncStartTime_]
    {
        if (const TimeComp& tcNow = getLocalTime();
            tc.day   == tcNow.day &&
            tc.month == tcNow.month &&
            tc.year  == tcNow.year)
            return formatTimeTag;
        return formatDateTimeTag;
    }();
    title += SPACED_DASH + utfTo<std::wstring>(formatTime(format, syncStartTime_));
#endif
    //---------------------------------------------------------------------------

    //systray tooltip, if window is minimized
    if (trayIcon_)
        trayIcon_->setToolTip(tooltip);

    //top level dialog title also shows in Windows taskbar!
    if (parentFrame_)
    {
        if (parentFrame_->GetTitle() != title)
            parentFrame_->SetTitle(title);
    }
    else if (this->GetTitle() != title)
        this->SetTitle(title);
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::updateProgressGui(bool allowYield)
{
    assert(syncStat_);
    if (!syncStat_) //sync not running!?
        return;

    //normally we don't update the "static" GUI components here, but we have to make an exception
    //if sync is cancelled (by user or error handling option)
    if (syncStat_->taskCancelled())
        updateStaticGui(); //called more than once after cancel... ok


    const std::chrono::nanoseconds timeElapsed = stopWatch_.elapsed();
    const double timeElapsedDouble = std::chrono::duration<double>(timeElapsed).count();

    const int     itemsCurrent = syncStat_->getCurrentStats().items;
    const int64_t bytesCurrent = syncStat_->getCurrentStats().bytes;
    const int     itemsTotal   = syncStat_->getTotalStats  ().items;
    const int64_t bytesTotal   = syncStat_->getTotalStats  ().bytes;

    const bool haveTotalStats = itemsTotal >= 0 || bytesTotal >= 0;

    bool headerLayoutChanged = false;

    //status texts
    setText(*pnl_.m_staticTextStatus, replaceCpy(syncStat_->currentStatusText(), L'\n', L' ')); //no layout update for status texts!

    if (!haveTotalStats)
    {
        //dialog caption, taskbar, systray tooltip
        setExternalStatus(getDialogPhaseText(*syncStat_, paused_), formatNumber(itemsCurrent)); //status text may be "paused"!

        //progress indicators
        setText(*pnl_.m_staticTextPercentTotal, L"", &headerLayoutChanged);

        if (trayIcon_.get()) trayIcon_->setProgress(1); //100% = fully visible FFS logo
        //taskbar_ already set to STATUS_INDETERMINATE by initNewPhase()
    }
    else
    {
        //dialog caption, taskbar, systray tooltip

        const double fractionTotal = bytesTotal + itemsTotal == 0 ? 0 : 1.0 * (bytesCurrent + itemsCurrent) / (bytesTotal + itemsTotal);
        //add both data + obj-count, to handle "deletion-only" cases

        const std::wstring percentTotal = formatProgressPercent(fractionTotal);

        setExternalStatus(getDialogPhaseText(*syncStat_, paused_), percentTotal); //status text may be "paused"!

        //progress indicators
        setText(*pnl_.m_staticTextPercentTotal, L' ' + percentTotal, &headerLayoutChanged);

        if (trayIcon_.get()) trayIcon_->setProgress(fractionTotal);
        if (taskbar_ .get()) taskbar_ ->setProgress(fractionTotal);

        const double timeTotalSecTentative = bytesCurrent == bytesTotal ? timeElapsedDouble : std::max(curveBytesEstim_.ref().getTotalTime(), timeElapsedDouble);

        curveBytesEstim_.ref().setValue(timeElapsedDouble, timeTotalSecTentative, bytesCurrent, bytesTotal);
        curveItemsEstim_.ref().setValue(timeElapsedDouble, timeTotalSecTentative, itemsCurrent, itemsTotal);

        //tentatively update total time, may be improved on below:
        curveBytesTimeNow_.ref().setValue(timeElapsedDouble, bytesCurrent);
        curveItemsTimeNow_.ref().setValue(timeElapsedDouble, itemsCurrent);

        curveBytesTimeEstim_.ref().setValue(timeTotalSecTentative, bytesTotal);
        curveItemsTimeEstim_.ref().setValue(timeTotalSecTentative, itemsTotal);
    }

    //even though notifyProgressChange() already set the latest data, let's add another sample to have all curves consider "timeNowMs"
    //no problem with adding too many records: CurveDataStatistics will remove duplicate entries!
    curveBytes_.ref().addSample(timeElapsedDouble, bytesCurrent);
    curveItems_.ref().addSample(timeElapsedDouble, itemsCurrent);

    bool layoutChanged = false; //avoid screen flicker by calling layout() only if necessary
    auto showIfNeeded = [&](wxWindow& wnd, bool show)
    {
        if (wnd.IsShown() != show)
        {
            wnd.Show(show);
            layoutChanged = true;
        }
    };

    //item and data stats
    if (!haveTotalStats)
    {
        setText(*pnl_.m_staticTextItemsProcessed, formatNumber(itemsCurrent), &layoutChanged);
        setText(*pnl_.m_staticTextBytesProcessed, L"", &layoutChanged);

        setText(*pnl_.m_staticTextItemsRemaining, std::wstring(1, EM_DASH), &layoutChanged);
        setText(*pnl_.m_staticTextBytesRemaining, L"",  &layoutChanged);
    }
    else
    {
        setText(*pnl_.m_staticTextItemsProcessed,               formatNumber(itemsCurrent), &layoutChanged);
        setText(*pnl_.m_staticTextBytesProcessed, L'(' + formatFilesizeShort(bytesCurrent) + L')', &layoutChanged);

        setText(*pnl_.m_staticTextItemsRemaining,               formatNumber(itemsTotal - itemsCurrent), &layoutChanged);
        setText(*pnl_.m_staticTextBytesRemaining, L'(' + formatFilesizeShort(bytesTotal - bytesCurrent) + L')', &layoutChanged);
        //it's possible data remaining becomes shortly negative if last file synced has ADS data and the bytesTotal was not yet corrected!
    }


    //errors and warnings (pop up dynamically)
    const Statistics::ErrorStats errorStats = syncStat_->getErrorStats();

    showIfNeeded(*pnl_.m_staticTextErrors,   errorStats.errorCount != 0);
    showIfNeeded(*pnl_.m_staticTextWarnings, errorStats.warningCount != 0);
    showIfNeeded(*pnl_.m_panelErrorStats, errorStats.errorCount != 0 || errorStats.warningCount != 0);

    if (pnl_.m_panelErrorStats->IsShown())
    {
        showIfNeeded(*pnl_.m_bitmapErrors,         errorStats.errorCount != 0);
        showIfNeeded(*pnl_.m_staticTextErrorCount, errorStats.errorCount != 0);

        if (pnl_.m_staticTextErrorCount->IsShown())
            setText(*pnl_.m_staticTextErrorCount, formatNumber(errorStats.errorCount), &layoutChanged);

        showIfNeeded(*pnl_.m_bitmapWarnings,         errorStats.warningCount != 0);
        showIfNeeded(*pnl_.m_staticTextWarningCount, errorStats.warningCount != 0);

        if (pnl_.m_staticTextWarningCount->IsShown())
            setText(*pnl_.m_staticTextWarningCount, formatNumber(errorStats.warningCount), &layoutChanged);
    }

    //current time elapsed
    const int64_t timeElapSec = std::chrono::duration_cast<std::chrono::seconds>(timeElapsed).count();

    setText(*pnl_.m_staticTextTimeElapsed, utfTo<wxString>(formatTimeSpan(timeElapSec, true /*hourOptional*/)), &layoutChanged);

    //remaining time and speed
    if (numeric::dist(timeLastSpeedEstimate_, timeElapsed) >= SPEED_ESTIMATE_UPDATE_INTERVAL)
    {
        timeLastSpeedEstimate_ = timeElapsed;

        if (numeric::dist(phaseStart_, timeElapsed) >= SPEED_ESTIMATE_SAMPLE_SKIP) //discard stats for first second: probably messy
        {
            remTimeTest_.addSample(timeElapsed, itemsCurrent, bytesCurrent);
            speedTest_  .addSample(timeElapsed, itemsCurrent, bytesCurrent);
        }

        //current speed -> Win 7 copy uses 1 sec update interval instead
        pnl_.m_panelGraphBytes->setAttributes(pnl_.m_panelGraphBytes->getAttributes().setCornerText(speedTest_.getBytesPerSecFmt(), GraphCorner::topL));
        pnl_.m_panelGraphItems->setAttributes(pnl_.m_panelGraphItems->getAttributes().setCornerText(speedTest_.getItemsPerSecFmt(), GraphCorner::topL));

        //remaining time
        if (!haveTotalStats)
        {
            setText(*pnl_.m_staticTextTimeRemaining, std::wstring(1, EM_DASH), &layoutChanged);
            //ignore graphs: should already have been cleared in initNewPhase()
        }
        else
        {
            //remaining time: display with relative error of 10% - based on samples taken every 0.5 sec only
            //-> call more often than once per second to correctly show last few seconds countdown, but don't call too often to avoid occasional jitter
            std::optional<double> remTimeSec = remTimeTest_.getRemainingSec(itemsTotal - itemsCurrent, bytesTotal - bytesCurrent);
            setText(*pnl_.m_staticTextTimeRemaining, remTimeSec ? formatRemainingTime(*remTimeSec) : std::wstring(1, EM_DASH), &layoutChanged);

            const double timeRemainingSec = remTimeSec ? *remTimeSec : 0;
            const double timeTotalSec = timeElapsedDouble + timeRemainingSec;
            //update estimated total time marker only with precision of "20% remaining time" to avoid needless jumping around:
            if (numeric::dist(curveBytesEstim_.ref().getTotalTime(), timeTotalSec) > 0.2 * timeRemainingSec)
            {
                //avoid needless flicker and don't update total time graph too often:
                static_assert(std::chrono::duration_cast<std::chrono::milliseconds>(GRAPH_TOTAL_TIME_UPDATE_INTERVAL).count() % SPEED_ESTIMATE_UPDATE_INTERVAL.count() == 0);
                if (numeric::dist(timeLastGraphTotalUpdate_, timeElapsed) >= GRAPH_TOTAL_TIME_UPDATE_INTERVAL)
                {
                    timeLastGraphTotalUpdate_ = timeElapsed;

                    curveBytesEstim_.ref().setTotalTime(timeTotalSec);
                    curveItemsEstim_.ref().setTotalTime(timeTotalSec);

                    curveBytesTimeEstim_.ref().setTime(timeTotalSec);
                    curveItemsTimeEstim_.ref().setTime(timeTotalSec);
                }
            }
        }
    }

    pnl_.m_panelGraphBytes->Refresh();
    pnl_.m_panelGraphItems->Refresh();

    //adapt layout after content changes above
    if (headerLayoutChanged)
        pnl_.Layout();

    if (layoutChanged)
    {
        pnl_.m_panelProgress->Layout();
        //small statistics panels:
        pnl_.m_panelItemStats->Layout();
        pnl_.m_panelTimeStats->Layout();
        if (pnl_.m_panelErrorStats->IsShown())
            pnl_.m_panelErrorStats->Layout();
    }


    if (allowYield)
    {
        if (paused_) //support for pause button
        {
            PauseTimers dummy(*this);

            while (paused_)
            {
                wxTheApp->Yield(); //receive UI message that ends pause
                //*first* refresh GUI (removing flicker) before sleeping!
                std::this_thread::sleep_for(UI_UPDATE_INTERVAL);
            }
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
void SyncProgressDialogImpl<TopLevelDialog>::updateStaticGui() //depends on "syncStat_, paused_"
{
    assert(syncStat_);
    if (!syncStat_)
        return;

    pnl_.m_staticTextPhase->SetLabelText(getDialogPhaseText(*syncStat_, paused_));
    //pnl_.m_bitmapStatus->SetToolTip(); -> redundant

    const wxImage statusImage = [&]
    {
        if (paused_)
            return loadImage("status_pause");

        if (syncStat_->taskCancelled())
            return loadImage("result_error");

        switch (syncStat_->currentPhase())
        {
            case ProcessPhase::none:
            case ProcessPhase::scan:
                return loadImage("status_scanning");
            case ProcessPhase::binaryCompare:
                return loadImage("status_binary_compare");
            case ProcessPhase::sync:
                return loadImage("status_syncing");
        }
        assert(false);
        return wxNullImage;
    }();
    setImage(*pnl_.m_bitmapStatus, statusImage);

    //show status on Windows 7 taskbar
    if (taskbar_.get())
    {
        if (paused_)
            taskbar_->setStatus(Taskbar::Status::paused);
        else
        {
            const int     itemsTotal = syncStat_->getTotalStats().items;
            const int64_t bytesTotal = syncStat_->getTotalStats().bytes;

            const bool haveTotalStats = itemsTotal >= 0 || bytesTotal >= 0;

            taskbar_->setStatus(haveTotalStats ? Taskbar::Status::normal : Taskbar::Status::indeterminate);
        }
    }

    //pause button
    pnl_.m_buttonPause->SetLabel(paused_ ? _("&Continue") : _("&Pause"));

    pnl_.bSizerErrorsIgnore->Show(ignoreErrors_);

    pnl_.Layout();
    pnl_.m_panelProgress->Layout(); //for bSizerErrorsIgnore
    //this->Refresh(); //a few pixels below the status text need refreshing -> still needed?
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::showSummary(TaskResult syncResult, const SharedRef<const ErrorLog>& log)
{
    assert(syncStat_);
    //at the LATEST(!) to prevent access to currentStatusHandler
    //enable okay and close events; may be set in this method ONLY

    paused_ = false; //you never know?

    //update numbers one last time (as if sync were still running)
    notifyProgressChange(); //make one last graph entry at the *current* time
    updateProgressGui(false /*allowYield*/);
    //===================================================================================

    const int     itemsProcessed = syncStat_->getCurrentStats().items;
    const int64_t bytesProcessed = syncStat_->getCurrentStats().bytes;
    const int     itemsTotal     = syncStat_->getTotalStats  ().items;
    const int64_t bytesTotal     = syncStat_->getTotalStats  ().bytes;

    //set overall speed (instead of current speed)
    const double timeDelta = std::chrono::duration<double>(stopWatch_.elapsed() - phaseStart_).count();
    //we need to consider "time within current phase" not total "timeElapsed"!

    const wxString overallBytesPerSecond = numeric::isNull(timeDelta) ? std::wstring() :
                                           replaceCpy(_("%x/sec"), L"%x", formatFilesizeShort(std::round(bytesProcessed / timeDelta)));
    const wxString overallItemsPerSecond = numeric::isNull(timeDelta) ? std::wstring() :
                                           replaceCpy(_("%x/sec"), L"%x", replaceCpy(_("%x items"), L"%x", formatThreeDigitPrecision(itemsProcessed / timeDelta)));

    pnl_.m_panelGraphBytes->setAttributes(pnl_.m_panelGraphBytes->getAttributes().setCornerText(overallBytesPerSecond, GraphCorner::topL));
    pnl_.m_panelGraphItems->setAttributes(pnl_.m_panelGraphItems->getAttributes().setCornerText(overallItemsPerSecond, GraphCorner::topL));

    //...if everything was processed successfully
    if (itemsTotal >= 0 && bytesTotal >= 0 && //itemsTotal < 0 && bytesTotal < 0 => e.g. cancel during folder comparison
        itemsProcessed == itemsTotal &&
        bytesProcessed == bytesTotal)
    {
        pnl_.m_staticTextPercentTotal->Hide();

        pnl_.m_staticTextProcessed     ->Hide();
        pnl_.m_staticTextRemaining     ->Hide();
        pnl_.m_staticTextItemsRemaining->Hide();
        pnl_.m_staticTextBytesRemaining->Hide();
        pnl_.m_staticTextTimeRemaining ->Hide();
    }

    //generally not interesting anymore (e.g. items > 0 due to skipped errors)
    pnl_.m_staticTextTimeRemaining->Hide();

    const int64_t totalTimeSec = std::chrono::duration_cast<std::chrono::seconds>(stopWatch_.elapsed()).count();
    pnl_.m_staticTextTimeElapsed->SetLabelText(utfTo<wxString>(formatTimeSpan(totalTimeSec)));
    //hourOptional? -> let's use full precision for max. clarity: https://freefilesync.org/forum/viewtopic.php?t=6308


    resumeFromSystray(false /*userRequested*/); //if in tray mode...

    //------- change class state -------
    syncStat_ = nullptr;
    //----------------------------------

    const wxImage statusImage = [&]
    {
        switch (syncResult)
        {
            case TaskResult::success:
                return loadImage("result_success");
            case TaskResult::warning:
                return loadImage("result_warning");
            case TaskResult::error:
            case TaskResult::cancelled:
                return loadImage("result_error");
        }
        assert(false);
        return wxNullImage;
    }();
    setImage(*pnl_.m_bitmapStatus, statusImage);

    pnl_.m_staticTextPhase->SetLabelText(getSyncResultLabel(syncResult));

    //pnl_.m_bitmapStatus->SetToolTip(); -> redundant

    //show status on Windows 7 taskbar
    if (taskbar_.get())
        switch (syncResult)
        {
            case TaskResult::success:
                taskbar_->setStatus(Taskbar::Status::normal);
                break;

            case TaskResult::warning:
                taskbar_->setStatus(Taskbar::Status::warning);
                break;

            case TaskResult::error:
            case TaskResult::cancelled:
                taskbar_->setStatus(Taskbar::Status::error);
                break;
        }
    //----------------------------------

    setExternalStatus(getSyncResultLabel(syncResult), wxString());

    //this->EnableCloseButton(true);

    pnl_.m_bpButtonMinimizeToTray->Hide();
    pnl_.m_buttonStop->Disable();
    pnl_.m_buttonStop->Hide();
    pnl_.m_buttonPause->Disable();
    pnl_.m_buttonPause->Hide();
    pnl_.m_buttonClose->Show();
    pnl_.m_buttonClose->Enable();

    pnl_.bSizerProgressFooter->Show(false);

    if (!parentFrame_) //hide checkbox for batch mode sync (where value won't be retrieved after close)
        pnl_.m_checkBoxAutoClose->Hide();

    //set std order after button visibility was set
    setStandardButtonLayout(*pnl_.bSizerStdButtons, StdButtons().setAffirmative(pnl_.m_buttonClose));

    //hide current operation status
    pnl_.bSizerStatusText->Show(false);

    pnl_.m_staticlineFooter->Hide(); //win: m_notebookResult already has a window frame

    //-------------------------------------------------------------

    pnl_.m_notebookResult->SetPadding(wxSize(dipToWxsize(2), 0)); //height cannot be changed

    //1. re-arrange graph into results listbook
    const size_t pagePosProgress = 0;
    const size_t pagePosLog      = 1;

    [[maybe_unused]] const bool wasDetached = pnl_.bSizerRoot->Detach(pnl_.m_panelProgress);
    assert(wasDetached);
    pnl_.m_panelProgress->Reparent(pnl_.m_notebookResult);
    pnl_.m_notebookResult->AddPage(pnl_.m_panelProgress, _("Progress"), true /*bSelect*/);

    //2. log file
    assert(pnl_.m_notebookResult->GetPageCount() == 1);
    LogPanel* logPanel = new LogPanel(pnl_.m_notebookResult); //owned by m_notebookResult
    logPanel->setLog(log.ptr());
    pnl_.m_notebookResult->AddPage(logPanel, _("Log"), false /*bSelect*/);

    //show log instead of graph if errors occurred! (not required for ignored warnings)
    const ErrorLogStats logCount = getStats(log.ref());
    if (logCount.error > 0)
        pnl_.m_notebookResult->ChangeSelection(pagePosLog);

    //fill image list to cope with wxNotebook image setting design desaster...
    const int imgListSize = dipToWxsize(16); //also required by GTK => don't use getMenuIconDipSize()
    auto imgList = std::make_unique<wxImageList>(imgListSize, imgListSize);

    imgList->Add(toScaledBitmap(loadImage("progress", wxsizeToScreen(imgListSize))));
    imgList->Add(toScaledBitmap(loadImage("log_file", wxsizeToScreen(imgListSize))));

    pnl_.m_notebookResult->AssignImageList(imgList.release()); //pass ownership

    pnl_.m_notebookResult->SetPageImage(pagePosProgress, pagePosProgress);
    pnl_.m_notebookResult->SetPageImage(pagePosLog,      pagePosLog);

    //Caveat: we need "Show()" *after" the above wxNotebook::ChangeSelection() to get the correct selection on Linux
    pnl_.m_notebookResult->Show();

    //GetSizer()->SetSizeHints(this); //~=Fit() //not a good idea: will shrink even if window is maximized or was enlarged by the user
    pnl_.Layout();

    pnl_.m_panelProgress->Layout();
    //small statistics panels:
    pnl_.m_panelItemStats->Layout();
    pnl_.m_panelTimeStats->Layout();
    if (pnl_.m_panelErrorStats->IsShown())
        pnl_.m_panelErrorStats->Layout();

    //this->Raise(); -> don't! user may be watching a movie in the meantime ;)

    pnl_.m_buttonClose->SetDefault();
    setFocusIfActive(*pnl_.m_buttonClose);
}


template <class TopLevelDialog>
auto SyncProgressDialogImpl<TopLevelDialog>::destroy(bool autoClose, bool restoreParentFrame, TaskResult syncResult, const SharedRef<const ErrorLog>& log) -> Result
{
    assert(stopWatch_.isPaused()); //why wasn't pauseAndGetTotalTime() called?

    if (autoClose)
    {
        assert(syncStat_);

        //ATTENTION: dialog may live a little longer, so watch callbacks!
        //e.g. wxGTK calls onIconize after wxWindow::Close() (better not ask why) and before physical destruction! => indirectly calls updateStaticGui(), which reads syncStat_!!!
        syncStat_ = nullptr;
    }
    else
    {
        showSummary(syncResult, log);

        //wait until user closes the dialog by pressing "Close"
        while (!closePressed_)
        {
            wxTheApp->Yield(); //refresh GUI *first* before sleeping! (remove flicker)
            std::this_thread::sleep_for(UI_UPDATE_INTERVAL);
        }
        restoreParentFrame = true;
    }
    //------------------------------------------------------------------------

    if (parentFrame_)
    {
        [[maybe_unused]] bool ubOk = parentFrame_->Unbind(wxEVT_CHAR_HOOK, &SyncProgressDialogImpl::onParentKeyEvent, this);
        assert(ubOk);

        parentFrame_->SetTitle(parentTitleBackup_); //restore title text

        if (restoreParentFrame)
        {
            //make sure main dialog is shown again if still "minimized to systray"!
            parentFrame_->Show();
            //if (parentFrame_->IsIconized()) //caveat: if window is maximized calling Iconize(false) will erroneously un-maximize!
            //    parentFrame_->Iconize(false);
        }
    }
    //else: don't call transformAppType(): consider "switch to main dialog" option during silent batch run

    //------------------------------------------------------------------------
    const bool autoCloseDialog = getOptionAutoCloseDialog();

    const WindowLayout::Dimensions dims = WindowLayout::getBeforeClose(*this);

    this->Destroy(); //wxWidgets macOS: simple "delete"!!!!!!!

    return {autoCloseDialog, dims};
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::onClose(wxCloseEvent& event)
{
    assert(event.CanVeto()); //this better be true: if "this" is parent of a modal error dialog, there is NO way (in hell) we allow destruction here!!!
    //wxEVT_END_SESSION is already handled by application.cpp::onSystemShutdown()!
    event.Veto();

    closePressed_ = true; //"temporary" auto-close: preempt closing results dialog

    if (syncStat_)
    {
        //user closing dialog => cancel sync + auto-close dialog
        userRequestAbort_();

        paused_ = false; //[!] we could be pausing here!
        updateStaticGui(); //update status + pause button
    }
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::onCancel(wxCommandEvent& event)
{
    userRequestAbort_();

    paused_ = false;
    updateStaticGui(); //update status + pause button
    //no UI-update here to avoid cascaded Yield()-call!
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::onPause(wxCommandEvent& event)
{
    paused_ = !paused_;
    updateStaticGui(); //update status + pause button
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::onIconize(wxIconizeEvent& event)
{
    /*  propagate progress dialog minimize/maximize to parent
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
        macOS:
            - wxDialog can be minimized but does not also minimize parent
                => propagate event to parent
        Windows:
            - wxDialog can be minimized but does not also minimize parent
            - iconize events only seen for manual minimize
                => propagate event to parent                                          */
    event.Skip();
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::minimizeToTray()
{
    if (!trayIcon_.get())
    {
        trayIcon_ = std::make_unique<FfsTrayIcon>([this] { this->resumeFromSystray(true /*userRequested*/); }); //FfsTrayIcon lifetime is a subset of "this"'s lifetime!
        //we may destroy FfsTrayIcon even while in the FfsTrayIcon callback!!!!

        updateProgressGui(false /*allowYield*/); //set tray tooltip + progress: e.g. no updates while paused

        this->Hide();
        if (parentFrame_)
            parentFrame_->Hide();
    }
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::resumeFromSystray(bool userRequested)
{
    if (trayIcon_)
    {
        trayIcon_.reset();

        if (parentFrame_)
        {
            //if (parentFrame_->IsIconized()) //caveat: if window is maximized calling Iconize(false) will erroneously un-maximize!
            //    parentFrame_->Iconize(false);
            parentFrame_->Show();
        }

        //if (IsIconized()) //caveat: if window is maximized calling Iconize(false) will erroneously un-maximize!
        //    Iconize(false);
        this->Show();

        updateStaticGui();                       //restore Windows 7 task bar status   (e.g. required in pause mode)
        updateProgressGui(false /*allowYield*/); //restore Windows 7 task bar progress (e.g. required in pause mode)

        if (userRequested)
        {
            if (parentFrame_)
                parentFrame_->Raise();
            this->Raise();
            pnl_.m_bpButtonMinimizeToTray->SetFocus();
        }
    }
}

//########################################################################################

SyncProgressDialog* SyncProgressDialog::create(const WindowLayout::Dimensions& dim,
                                               const std::function<void()>& userRequestCancel,
                                               const Statistics& syncStat,
                                               wxFrame* parentWindow, //may be nullptr
                                               bool showProgress,
                                               bool autoCloseDialog,
                                               const std::vector<std::wstring>& jobNames,
                                               time_t syncStartTime,
                                               bool ignoreErrors,
                                               size_t autoRetryCount,
                                               PostSyncAction postSyncAction)
{
    if (parentWindow) //FFS GUI sync
        return new SyncProgressDialogImpl<wxDialog>(wxDEFAULT_DIALOG_STYLE | wxMAXIMIZE_BOX | wxMINIMIZE_BOX | wxRESIZE_BORDER,
                                                    dim, userRequestCancel, syncStat, parentWindow, showProgress,
                                                    autoCloseDialog, jobNames, syncStartTime, ignoreErrors, autoRetryCount, postSyncAction);
    else //FFS batch job
    {
        auto dlg = new SyncProgressDialogImpl<wxFrame>(wxDEFAULT_FRAME_STYLE,
                                                       dim, userRequestCancel, syncStat, parentWindow, showProgress,
                                                       autoCloseDialog, jobNames, syncStartTime, ignoreErrors, autoRetryCount, postSyncAction);
        dlg->SetIcon(getFfsIcon()); //only top level windows should have an icon
        return dlg;
    }
}

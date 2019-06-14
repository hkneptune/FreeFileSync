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
#include <wx/app.h>
#include <zen/basic_math.h>
#include <zen/format_unit.h>
#include <zen/scope_guard.h>
#include <wx+/toggle_button.h>
#include <wx+/image_tools.h>
#include <wx+/graph.h>
#include <wx+/no_flicker.h>
#include <wx+/font_size.h>
#include <wx+/std_button_layout.h>
#include <zen/file_access.h>
#include <zen/thread.h>
#include <zen/perf.h>
#include <wx+/choice_enum.h>
#include "gui_generated.h"
#include "../base/ffs_paths.h"
#include "../base/perf_check.h"
#include "tray_icon.h"
#include "taskbar.h"
#include "log_panel.h"
#include "app_icon.h"


using namespace zen;
using namespace fff;


namespace
{
//window size used for statistics
const std::chrono::seconds WINDOW_REMAINING_TIME(60); //USB memory stick scenario can have drop outs of 40 seconds => 60 sec. window size handles it
const std::chrono::seconds WINDOW_BYTES_PER_SEC  (5); //
const std::chrono::milliseconds SPEED_ESTIMATE_UPDATE_INTERVAL(500);
const std::chrono::seconds      SPEED_ESTIMATE_SAMPLE_INTERVAL(1);

const size_t PROGRESS_GRAPH_SAMPLE_SIZE_MAX = 2500000; //sizeof(single node) worst case ~ 3 * 8 byte ptr + 16 byte key/value = 40 byte

inline wxColor getColorBytes() { return { 111, 255,  99 }; } //light green
inline wxColor getColorItems() { return { 127, 147, 255 }; } //light blue

inline wxColor getColorBytesRim() { return { 20, 200,   0 }; } //medium green
inline wxColor getColorItemsRim() { return { 90, 120, 255 }; } //medium blue

inline wxColor getColorBytesBackground() { return { 205, 255, 202 }; } //faint green
inline wxColor getColorItemsBackground() { return { 198, 206, 255 }; } //faint blue

inline wxColor getColorBytesBackgroundRim() { return { 12, 128,   0 }; } //dark green
inline wxColor getColorItemsBackgroundRim() { return { 53,  25, 255 }; } //dark blue


std::wstring getDialogPhaseText(const Statistics& syncStat, bool paused)
{
    if (paused)
        return _("Paused");

    if (syncStat.getAbortStatus())
        return _("Stop requested...");

    switch (syncStat.currentPhase())
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
    assert(false);
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

    void init(const Statistics& syncStat, bool ignoreErrors, size_t automaticRetryCount); //constructor/destructor semantics, but underlying Window is reused
    void teardown();                                                                      //

    void initNewPhase();
    void updateProgressGui();

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

private:
    //void OnToggleIgnoreErrors(wxCommandEvent& event) override { updateStaticGui(); }

    void updateStaticGui();

    wxFrame& parentWindow_;
    wxString parentTitleBackup_;

    StopWatch stopWatch_;
    std::chrono::nanoseconds phaseStart_{}; //begin of current phase

    const Statistics* syncStat_ = nullptr; //only bound while sync is running

    std::unique_ptr<Taskbar> taskbar_;
    PerfCheck perf_{ WINDOW_REMAINING_TIME, WINDOW_BYTES_PER_SEC }; //estimate remaining time

    std::chrono::nanoseconds timeLastSpeedEstimate_ = std::chrono::seconds(-100); //used for calculating intervals between showing and collecting perf samples
    //initial value: just some big number

    std::shared_ptr<CurveDataProgressBar> curveDataBytes_{ std::make_shared<CurveDataProgressBar>(true  /*drawTop*/) };
    std::shared_ptr<CurveDataProgressBar> curveDataItems_{ std::make_shared<CurveDataProgressBar>(false /*drawTop*/) };

    bool ignoreErrors_ = false;
};


CompareProgressDialog::Impl::Impl(wxFrame& parentWindow) :
    CompareProgressDlgGenerated(&parentWindow),
    parentWindow_(parentWindow)
{
    m_bitmapIgnoreErrors->SetBitmap(getResourceImage(L"error_ignore_active"));
    m_bitmapRetryErrors ->SetBitmap(getResourceImage(L"error_retry"));

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

    Layout();
    m_panelStatistics->Layout();

    GetSizer()->SetSizeHints(this); //~=Fit() + SetMinSize()
    //=> works like a charm for GTK2 with window resizing problems and title bar corruption; e.g. Debian!!!
}


void CompareProgressDialog::Impl::init(const Statistics& syncStat, bool ignoreErrors, size_t automaticRetryCount)
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

    perf_ = PerfCheck(WINDOW_REMAINING_TIME, WINDOW_BYTES_PER_SEC);
    stopWatch_.restart(); //measure total time

    //initially hide status that's relevant for comparing bytewise only
    m_staticTextItemsFoundLabel->Show();
    m_staticTextItemsFound     ->Show();

    m_staticTextItemsRemainingLabel->Hide();
    bSizerItemsRemaining           ->Show(false);

    m_staticTextTimeRemainingLabel->Hide();
    m_staticTextTimeRemaining     ->Hide();

    setText(*m_staticTextRetryCount, std::wstring(L"(") + formatNumber(automaticRetryCount) + MULT_SIGN + L")");
    bSizerErrorsRetry->Show(automaticRetryCount > 0);

    //allow changing a few options dynamically during sync
    ignoreErrors_ = ignoreErrors;

    updateStaticGui();
    updateProgressGui();

    Layout();
    m_panelStatistics->Layout();
}


void CompareProgressDialog::Impl::teardown()
{
    syncStat_ = nullptr;
    parentWindow_.SetTitle(parentTitleBackup_);
    taskbar_.reset();
}


void CompareProgressDialog::Impl::initNewPhase()
{
    //start new measurement
    perf_ = PerfCheck(WINDOW_REMAINING_TIME, WINDOW_BYTES_PER_SEC);
    timeLastSpeedEstimate_ = std::chrono::seconds(-100); //make sure estimate is updated upon next check
    phaseStart_ = stopWatch_.elapsed();

    switch (syncStat_->currentPhase())
    {
        case ProcessCallback::PHASE_NONE:
            assert(false);
        case ProcessCallback::PHASE_SCANNING:
            break;

        case ProcessCallback::PHASE_COMPARING_CONTENT:
        case ProcessCallback::PHASE_SYNCHRONIZING:
            bSizerProgressGraph->Show(true);

            //show status for comparing bytewise
            m_staticTextItemsFoundLabel->Hide();
            m_staticTextItemsFound     ->Hide();

            m_staticTextItemsRemainingLabel->Show();
            bSizerItemsRemaining           ->Show(true);

            m_staticTextTimeRemainingLabel->Show();
            m_staticTextTimeRemaining     ->Show();

            Layout();
            m_panelStatistics->Layout();
            break;
    }

    updateProgressGui();
}


void CompareProgressDialog::Impl::updateStaticGui()
{
    bSizerErrorsIgnore->Show(ignoreErrors_);
    Layout();
}


void CompareProgressDialog::Impl::updateProgressGui()
{
    assert(syncStat_);
    if (!syncStat_) //no comparison running!!
        return;

    auto setTitle = [&](const wxString& title)
    {
        if (parentWindow_.GetTitle() != title)
            parentWindow_.SetTitle(title);
    };

    bool layoutChanged = false; //avoid screen flicker by calling layout() only if necessary
    const std::chrono::nanoseconds timeElapsed = stopWatch_.elapsed();

    const int     itemsCurrent = syncStat_->getStatsCurrent(syncStat_->currentPhase()).items;
    const int64_t bytesCurrent = syncStat_->getStatsCurrent(syncStat_->currentPhase()).bytes;
    const int     itemsTotal   = syncStat_->getStatsTotal  (syncStat_->currentPhase()).items;
    const int64_t bytesTotal   = syncStat_->getStatsTotal  (syncStat_->currentPhase()).bytes;

    //status texts
    setText(*m_staticTextStatus, replaceCpy(syncStat_->currentStatusText(), L'\n', L' ')); //no layout update for status texts!

    //write status information to taskbar, parent title etc.
    switch (syncStat_->currentPhase())
    {
        case ProcessCallback::PHASE_NONE:
        case ProcessCallback::PHASE_SCANNING:
        {
            const wxString& scannedObjects = formatNumber(itemsCurrent);

            //dialog caption, taskbar
            setTitle(scannedObjects + L" | " + getDialogPhaseText(*syncStat_, false /*paused*/));
            if (taskbar_.get()) //support Windows 7 taskbar
                taskbar_->setStatus(Taskbar::STATUS_INDETERMINATE);

            //nr of scanned objects
            setText(*m_staticTextItemsFound, scannedObjects, &layoutChanged);
        }
        break;

        case ProcessCallback::PHASE_SYNCHRONIZING:
        case ProcessCallback::PHASE_COMPARING_CONTENT:
        {
            //add both bytes + item count, to handle "deletion-only" cases
            const double fractionTotal = bytesTotal + itemsTotal == 0 ? 0 : 1.0 * (bytesCurrent + itemsCurrent) / (bytesTotal + itemsTotal);
            const double fractionBytes = bytesTotal == 0 ? 0 : 1.0 * bytesCurrent / bytesTotal;
            const double fractionItems = itemsTotal == 0 ? 0 : 1.0 * itemsCurrent / itemsTotal;

            //dialog caption, taskbar
            setTitle(formatFraction(fractionTotal) + L" | " + getDialogPhaseText(*syncStat_, false /*paused*/));
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
            if (numeric::dist(timeLastSpeedEstimate_, timeElapsed) >= SPEED_ESTIMATE_UPDATE_INTERVAL)
            {
                timeLastSpeedEstimate_ = timeElapsed;

                if (numeric::dist(phaseStart_, timeElapsed) >= SPEED_ESTIMATE_SAMPLE_INTERVAL) //discard stats for first second: probably messy
                    perf_.addSample(timeElapsed, itemsCurrent, bytesCurrent);

                //current speed -> Win 7 copy uses 1 sec update interval instead
                std::optional<std::wstring> bps = perf_.getBytesPerSecond();
                std::optional<std::wstring> ips = perf_.getItemsPerSecond();
                m_panelProgressGraph->setAttributes(m_panelProgressGraph->getAttributes().setCornerText(bps ? *bps : L"", Graph2D::CORNER_TOP_LEFT));
                m_panelProgressGraph->setAttributes(m_panelProgressGraph->getAttributes().setCornerText(ips ? *ips : L"", Graph2D::CORNER_BOTTOM_LEFT));

                //remaining time: display with relative error of 10% - based on samples taken every 0.5 sec only
                //-> call more often than once per second to correctly show last few seconds countdown, but don't call too often to avoid occasional jitter
                std::optional<double> remTimeSec = perf_.getRemainingTimeSec(bytesTotal - bytesCurrent);
                setText(*m_staticTextTimeRemaining, remTimeSec ? formatRemainingTime(*remTimeSec) : L"-", &layoutChanged);
            }

            m_panelProgressGraph->Refresh();
        }
        break;
    }

    const int64_t timeElapSec = std::chrono::duration_cast<std::chrono::seconds>(timeElapsed).count();

    setText(*m_staticTextTimeElapsed, timeElapSec < 3600 ?
            wxTimeSpan::Seconds(timeElapSec).Format(   L"%M:%S") :
            wxTimeSpan::Seconds(timeElapSec).Format(L"%H:%M:%S"), &layoutChanged);

    if (layoutChanged)
    {
        Layout();
        m_panelStatistics->Layout();
    }

    //do the ui update
    wxTheApp->Yield();
}

//########################################################################################

//redirect to implementation
CompareProgressDialog::CompareProgressDialog(wxFrame& parentWindow) : pimpl_(new Impl(parentWindow)) {} //owned by parentWindow
wxWindow* CompareProgressDialog::getAsWindow() { return pimpl_; }
void CompareProgressDialog::init(const Statistics& syncStat, bool ignoreErrors, size_t automaticRetryCount) { pimpl_->init(syncStat, ignoreErrors, automaticRetryCount); }
void CompareProgressDialog::teardown()     { pimpl_->teardown(); }
void CompareProgressDialog::initNewPhase() { pimpl_->initNewPhase(); }
void CompareProgressDialog::updateGui()    { pimpl_->updateProgressGui(); }
bool CompareProgressDialog::getOptionIgnoreErrors() const { return pimpl_->getOptionIgnoreErrors(); }
void CompareProgressDialog::setOptionIgnoreErrors(bool ignoreErrors) { pimpl_->setOptionIgnoreErrors(ignoreErrors); }
void CompareProgressDialog::timerSetStatus(bool active) { pimpl_->timerSetStatus(active); }
bool CompareProgressDialog::timerIsRunning() const { return pimpl_->timerIsRunning(); }

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

        if (samples_.size() > PROGRESS_GRAPH_SAMPLE_SIZE_MAX) //limit buffer size
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

    std::optional<CurvePoint> getLessEq(double x) const override //x: seconds since begin
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
            return {};
        //=> samples not empty in this context
        --it;
        return CurvePoint(std::chrono::duration<double>(it->first).count(), it->second);
    }

    std::optional<CurvePoint> getGreaterEq(double x) const override
    {
        const std::chrono::nanoseconds timeX(static_cast<std::chrono::nanoseconds::rep>(std::ceil(x * (1000 * 1000 * 1000)))); //round up!

        //------ add artifical last sample value -------
        if (!samples_.empty() && samples_.rbegin()->first < lastSample_.first)
            if (samples_.rbegin()->first < timeX && timeX <= lastSample_.first)
                return CurvePoint(std::chrono::duration<double>(lastSample_.first).count(), lastSample_.second);
        //--------------------------------------------------

        auto it = samples_.lower_bound(timeX);
        if (it == samples_.end())
            return {};
        return CurvePoint(std::chrono::duration<double>(it->first).count(), it->second);
    }

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
                           const std::chrono::system_clock::time_point& syncStartTime,
                           const wxString& jobName,
                           const Zstring& soundFileSyncComplete,
                           bool ignoreErrors,
                           size_t automaticRetryCount,
                           PostSyncAction2 postSyncAction);
    ~SyncProgressDialogImpl() override;

    //call this in StatusUpdater derived class destructor at the LATEST(!) to prevent access to currentStatusUpdater
    void showSummary(SyncResult finalStatus, const std::shared_ptr<const ErrorLog>& log /*bound!*/) override;
    void closeDirectly(bool restoreParentFrame) override;

    wxWindow* getWindowIfVisible() override { return this->IsShown() ? this : nullptr; }
    //workaround OS X bug: if "this" is used as parent window for a modal dialog then this dialog will erroneously un-hide its parent!

    void initNewPhase        () override;
    void notifyProgressChange() override;
    void updateGui           () override { updateProgressGui(true /*allowYield*/); }

    bool getOptionIgnoreErrors()                 const override { return ignoreErrors_; }
    void setOptionIgnoreErrors(bool ignoreErrors)      override { ignoreErrors_ = ignoreErrors; updateStaticGui(); }
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
    //void OnToggleIgnoreErrors(wxCommandEvent& event) { updateStaticGui(); }

    void minimizeToTray();
    void resumeFromSystray();

    void updateStaticGui();
    void updateProgressGui(bool allowYield);

    void setExternalStatus(const wxString& status, const wxString& progress); //progress may be empty!

    SyncProgressPanelGenerated& pnl_; //wxPanel containing the GUI controls of *this

    const std::chrono::system_clock::time_point& syncStartTime_;
    const wxString jobName_;
    const Zstring soundFileSyncComplete_;
    StopWatch stopWatch_;

    wxFrame* parentFrame_; //optional

    std::function<void()> notifyWindowTerminate_; //call once in OnClose(), NOT in destructor which is called far too late somewhere in wxWidgets main loop!

    //status variables
    const Statistics* syncStat_;                  //
    AbortCallback*    abortCb_;                   //valid only while sync is running
    bool paused_ = false;
    const std::shared_ptr<int> lifeSign_ = std::make_shared<int>(42); //only bound while instance exists, see pause handling in updateProgressGui()
    //wxWindow::Delete(), equals "delete this" on OS X!

    //remaining time
    PerfCheck perf_{ WINDOW_REMAINING_TIME, WINDOW_BYTES_PER_SEC };
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

    bool ignoreErrors_ = false;
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
                                                               const std::chrono::system_clock::time_point& syncStartTime,
                                                               const wxString& jobName,
                                                               const Zstring& soundFileSyncComplete,
                                                               bool ignoreErrors,
                                                               size_t automaticRetryCount,
                                                               PostSyncAction2 postSyncAction) :
    TopLevelDialog(parentFrame, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, style), //title is overwritten anyway in setExternalStatus()
    pnl_(*new SyncProgressPanelGenerated(this)), //ownership passed to "this"
    syncStartTime_(syncStartTime),
    jobName_  (jobName),
    soundFileSyncComplete_(soundFileSyncComplete),
    parentFrame_(parentFrame),
    notifyWindowTerminate_(notifyWindowTerminate),
    syncStat_ (&syncStat),
    abortCb_  (&abortCb)
{
    static_assert(std::is_same_v<TopLevelDialog, wxFrame > ||
                  std::is_same_v<TopLevelDialog, wxDialog>);
    assert((std::is_same_v<TopLevelDialog, wxFrame> == !parentFrame));

    //finish construction of this dialog:
    this->pnl_.m_panelProgress->SetMinSize(wxSize(fastFromDIP(550), fastFromDIP(340)));

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

    pnl_.m_bitmapIgnoreErrors->SetBitmap(getResourceImage(L"error_ignore_active"));
    pnl_.m_bitmapRetryErrors ->SetBitmap(getResourceImage(L"error_retry"));

    //init graph
    const int xLabelHeight = this->GetCharHeight() + fastFromDIP(2) /*margin*/; //use same height for both graphs to make sure they stretch evenly
    const int yLabelWidth  = fastFromDIP(70);
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

    setText(*pnl_.m_staticTextRetryCount, std::wstring(L"(") + formatNumber(automaticRetryCount) + MULT_SIGN + L")");
    pnl_.bSizerErrorsRetry->Show(automaticRetryCount > 0);

    //allow changing a few options dynamically during sync
    ignoreErrors_ = ignoreErrors;

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
    //else: don't call "TransformProcessType": consider "switch to main dialog" option during silent batch run

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
    curveDataBytesTotal_  ->setValue(0, 0);
    curveDataItemsTotal_  ->setValue(0, 0);
    curveDataBytesCurrent_->setValue(0, 0, 0);
    curveDataItemsCurrent_->setValue(0, 0, 0);
    curveDataBytes_       ->clear();
    curveDataItems_       ->clear();

    notifyProgressChange(); //make sure graphs get initial values

    //start new measurement
    perf_ = PerfCheck(WINDOW_REMAINING_TIME, WINDOW_BYTES_PER_SEC);
    timeLastSpeedEstimate_ = std::chrono::seconds(-100); //make sure estimate is updated upon next check
    phaseStart_ = stopWatch_.elapsed();

    updateProgressGui(false /*allowYield*/);
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::notifyProgressChange() //noexcept!
{
    if (syncStat_) //sync running
    {
        const ProgressStats stats = syncStat_->getStatsCurrent(syncStat_->currentPhase());
        curveDataBytes_->addRecord(stopWatch_.elapsed(), stats.bytes);
        curveDataItems_->addRecord(stopWatch_.elapsed(), stats.items);
    }
}


namespace
{
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::setExternalStatus(const wxString& status, const wxString& progress) //progress may be empty!
{
    //sys tray: order "top-down": jobname, status, progress
    wxString systrayTooltip = jobName_.empty() ? status : jobName_ + L"\n" + status;
    if (!progress.empty())
        systrayTooltip += L" " + progress;

    //window caption/taskbar; inverse order: progress, status, jobname
    wxString title = progress.empty() ? status : progress + L" | " + status;

    if (!jobName_.empty())
        title += L" | " + jobName_;

    const TimeComp tc = getLocalTime(std::chrono::system_clock::to_time_t(syncStartTime_)); //returns empty string on failure
    title += L" | " + formatTime<std::wstring>(FORMAT_DATE_TIME, tc);
    //---------------------------------------------------------------------------

    //systray tooltip, if window is minimized
    if (trayIcon_)
        trayIcon_->setToolTip(systrayTooltip);

    //show text in dialog title (and at the same time in taskbar)
    if (parentFrame_)
        if (parentFrame_->GetTitle() != title)
            parentFrame_->SetTitle(title);

    //always set a title: we don't want wxGTK to show "nameless window" instead
    if (this->GetTitle() != title)
        this->SetTitle(title);
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::updateProgressGui(bool allowYield)
{
    assert(syncStat_);
    if (!syncStat_) //sync not running
        return;

    //normally we don't update the "static" GUI components here, but we have to make an exception
    //if sync is cancelled (by user or error handling option)
    if (syncStat_->getAbortStatus())
        updateStaticGui(); //called more than once after cancel... ok


    bool layoutChanged = false; //avoid screen flicker by calling layout() only if necessary
    const std::chrono::nanoseconds timeElapsed = stopWatch_.elapsed();
    const double timeElapsedDouble = std::chrono::duration<double>(timeElapsed).count();

    const int     itemsCurrent = syncStat_->getStatsCurrent(syncStat_->currentPhase()).items;
    const int64_t bytesCurrent = syncStat_->getStatsCurrent(syncStat_->currentPhase()).bytes;
    const int     itemsTotal   = syncStat_->getStatsTotal  (syncStat_->currentPhase()).items;
    const int64_t bytesTotal   = syncStat_->getStatsTotal  (syncStat_->currentPhase()).bytes;

    //sync status text
    setText(*pnl_.m_staticTextStatus, replaceCpy(syncStat_->currentStatusText(), L'\n', L' ')); //no layout update for status texts!


    if (itemsTotal < 0 && bytesTotal < 0)
    {
        //dialog caption, taskbar, systray tooltip
        setExternalStatus(getDialogPhaseText(*syncStat_, paused_), formatNumber(itemsCurrent)); //status text may be "paused"!

        //progress indicators
        if (trayIcon_.get()) trayIcon_->setProgress(1); //100% = regular FFS logo
        //taskbar_ already set to STATUS_INDETERMINATE within initNewPhase()
    }
    else
    {
        //dialog caption, taskbar, systray tooltip

        const double fractionTotal = bytesTotal + itemsTotal == 0 ? 0 : 1.0 * (bytesCurrent + itemsCurrent) / (bytesTotal + itemsTotal);
        //add both data + obj-count, to handle "deletion-only" cases

        setExternalStatus(getDialogPhaseText(*syncStat_, paused_), formatFraction(fractionTotal)); //status text may be "paused"!

        //progress indicators
        if (trayIcon_.get()) trayIcon_->setProgress(fractionTotal);
        if (taskbar_ .get()) taskbar_ ->setProgress(fractionTotal);

        //----------------------------------------------------------------------------------------------------
        const double timeTotalSecTentative = bytesCurrent == bytesTotal ? timeElapsedDouble : std::max(curveDataBytesTotal_->getValueX(), timeElapsedDouble);

        //constant line graph
        curveDataBytesCurrent_->setValue(timeElapsedDouble, timeTotalSecTentative, bytesCurrent);
        curveDataItemsCurrent_->setValue(timeElapsedDouble, timeTotalSecTentative, itemsCurrent);

        //tentatively update total time, may be improved on below:
        curveDataBytesTotal_->setValue(timeTotalSecTentative, bytesTotal);
        curveDataItemsTotal_->setValue(timeTotalSecTentative, itemsTotal);
    }

    //even though notifyProgressChange() already set the latest data, let's add another sample to have all curves consider "timeNowMs"
    //no problem with adding too many records: CurveDataStatistics will remove duplicate entries!
    curveDataBytes_->addRecord(timeElapsed, bytesCurrent);
    curveDataItems_->addRecord(timeElapsed, itemsCurrent);


    //remaining objects and data
    if (itemsTotal < 0 && bytesTotal < 0)
    {
        setText(*pnl_.m_staticTextItemsRemaining, L"-", &layoutChanged);
        setText(*pnl_.m_staticTextBytesRemaining, L"",  &layoutChanged);
    }
    else
    {
        setText(*pnl_.m_staticTextItemsRemaining,               formatNumber(itemsTotal - itemsCurrent), &layoutChanged);
        setText(*pnl_.m_staticTextBytesRemaining, L"(" + formatFilesizeShort(bytesTotal - bytesCurrent) + L")", &layoutChanged);
        //it's possible data remaining becomes shortly negative if last file synced has ADS data and the bytesTotal was not yet corrected!
    }

    //remaining time and speed
    if (numeric::dist(timeLastSpeedEstimate_, timeElapsed) >= SPEED_ESTIMATE_UPDATE_INTERVAL)
    {
        timeLastSpeedEstimate_ = timeElapsed;

        if (numeric::dist(phaseStart_, timeElapsed) >= SPEED_ESTIMATE_SAMPLE_INTERVAL) //discard stats for first second: probably messy
            perf_.addSample(timeElapsed, itemsCurrent, bytesCurrent);

        //current speed -> Win 7 copy uses 1 sec update interval instead
        std::optional<std::wstring> bps = perf_.getBytesPerSecond();
        std::optional<std::wstring> ips = perf_.getItemsPerSecond();
        pnl_.m_panelGraphBytes->setAttributes(pnl_.m_panelGraphBytes->getAttributes().setCornerText(bps ? *bps : L"", Graph2D::CORNER_TOP_LEFT));
        pnl_.m_panelGraphItems->setAttributes(pnl_.m_panelGraphItems->getAttributes().setCornerText(ips ? *ips : L"", Graph2D::CORNER_TOP_LEFT));

        //remaining time
        if (bytesTotal < 0)
        {
            setText(*pnl_.m_staticTextTimeRemaining, L"-", &layoutChanged);
            //ignore graphs: should already have been cleared in initNewPhase()
        }
        else
        {
            //remaining time: display with relative error of 10% - based on samples taken every 0.5 sec only
            //-> call more often than once per second to correctly show last few seconds countdown, but don't call too often to avoid occasional jitter
            std::optional<double> remTimeSec = perf_.getRemainingTimeSec(bytesTotal - bytesCurrent);
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
    }


    pnl_.m_panelGraphBytes->Refresh();
    pnl_.m_panelGraphItems->Refresh();

    const int64_t timeElapSec = std::chrono::duration_cast<std::chrono::seconds>(timeElapsed).count();

    setText(*pnl_.m_staticTextTimeElapsed, timeElapSec < 3600 ?
            wxTimeSpan::Seconds(timeElapSec).Format(   L"%M:%S") :
            wxTimeSpan::Seconds(timeElapSec).Format(L"%H:%M:%S"), &layoutChanged);

    //adapt layout after content changes above
    if (layoutChanged)
    {
        pnl_.m_panelProgress->Layout();
        //small statistics panels:
        //pnl.m_panelItemsProcessed ->Layout(); -> hidden
        pnl_.m_panelItemsRemaining->Layout();
        pnl_.m_panelTimeRemaining ->Layout();
        //pnl.m_panelTimeElapsed->Layout(); -> needed?
    }


    if (allowYield)
    {
        //support for pause button
        if (paused_)
        {
            timerSetStatus(false /*active*/);

            std::weak_ptr<int> lifeSignWeak(lifeSign_);
            while (paused_)
            {
                wxTheApp->Yield(); //receive UI message that end pause OR forceful termination!
                //*first* refresh GUI (removing flicker) before sleeping!
                std::this_thread::sleep_for(UI_UPDATE_INTERVAL);

                //after SyncProgressDialogImpl::OnClose() called wxWindow::Destroy() on OS X this instance is instantly toast!
                if (!lifeSignWeak.lock())
                    return; //GTFO and don't call this->timerSetStatus(); we're fine: https://isocpp.org/wiki/faq/freestore-mgmt#delete-this
            }

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
void SyncProgressDialogImpl<TopLevelDialog>::updateStaticGui() //depends on "syncStat_, paused_"
{
    assert(syncStat_);
    if (!syncStat_)
        return;

    pnl_.m_staticTextPhase->SetLabel(getDialogPhaseText(*syncStat_, paused_));
    //pnl_.m_bitmapStatus->SetToolTip(); -> redundant

    const wxBitmap statusImage = [&]
    {
        if (paused_)
            return getResourceImage(L"status_pause");

        if (syncStat_->getAbortStatus())
            return getResourceImage(L"status_aborted");

        switch (syncStat_->currentPhase())
        {
            case ProcessCallback::PHASE_NONE:
            case ProcessCallback::PHASE_SCANNING:
                return getResourceImage(L"status_scanning");
            case ProcessCallback::PHASE_COMPARING_CONTENT:
                return getResourceImage(L"status_binary_compare");
            case ProcessCallback::PHASE_SYNCHRONIZING:
                return getResourceImage(L"status_syncing");
        }
        assert(false);
        return wxNullBitmap;
    }();
    pnl_.m_bitmapStatus->SetBitmap(statusImage);


    //show status on Windows 7 taskbar
    if (taskbar_.get())
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

    //pause button
    pnl_.m_buttonPause->SetLabel(paused_ ? _("&Continue") : _("&Pause"));

    pnl_.bSizerErrorsIgnore->Show(ignoreErrors_);

    pnl_.Layout();
    pnl_.m_panelProgress->Layout(); //for bSizerErrorsIgnore
    //this->Refresh(); //a few pixels below the status text need refreshing -> still needed?
}


template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::closeDirectly(bool restoreParentFrame) //this should really be called "do not call back + schedule deletion"
{
    assert(syncStat_ && abortCb_);

    if (!restoreParentFrame)
        parentFrame_ = nullptr; //avoid destructor calls like parentFrame_->Show(), ::TransformProcessType(&psn, kProcessTransformToForegroundApplication);

    paused_ = false; //you never know?

    //resumeFromSystray(); -> NO, instead ~SyncProgressDialogImpl() makes sure that main dialog is shown again! e.g. avoid calls to this/parentFrame_->Raise()

    //ATTENTION: dialog may live a little longer, so watch callbacks!
    //e.g. wxGTK calls OnIconize after wxWindow::Close() (better not ask why) and before physical destruction! => indirectly calls updateStaticGui(), which reads syncStat_!!!
    syncStat_ = nullptr;
    abortCb_  = nullptr;

    this->Close(); //generate close event: do NOT destroy window unconditionally!
}


//essential to call this in StatusHandler derived class destructor
template <class TopLevelDialog>
void SyncProgressDialogImpl<TopLevelDialog>::showSummary(SyncResult finalStatus, const std::shared_ptr<const ErrorLog>& log /*bound!*/)
{
    assert(syncStat_ && abortCb_);
    //at the LATEST(!) to prevent access to currentStatusHandler
    //enable okay and close events; may be set in this method ONLY

    //In wxWidgets 2.9.3 upwards, the wxWindow::Reparent() below fails on GTK and OS X if window is frozen! http://forums.codeblocks.org/index.php?topic=13388.45

    paused_ = false; //you never know?

    //update numbers one last time (as if sync were still running)
    notifyProgressChange(); //make one last graph entry at the *current* time
    updateProgressGui(false /*allowYield*/);
    //===================================================================================

    const int     itemsProcessed = syncStat_->getStatsCurrent(syncStat_->currentPhase()).items;
    const int64_t bytesProcessed = syncStat_->getStatsCurrent(syncStat_->currentPhase()).bytes;
    const int     itemsTotal     = syncStat_->getStatsTotal  (syncStat_->currentPhase()).items;
    const int64_t bytesTotal     = syncStat_->getStatsTotal  (syncStat_->currentPhase()).bytes;

    //set overall speed (instead of current speed)
    const double timeDelta = std::chrono::duration<double>(stopWatch_.elapsed() - phaseStart_).count();
    //we need to consider "time within current phase" not total "timeElapsed"!

    const wxString overallBytesPerSecond = numeric::isNull(timeDelta) ? std::wstring() :
                                           replaceCpy(_("%x/sec"), L"%x", formatFilesizeShort(numeric::round(bytesProcessed / timeDelta)));
    const wxString overallItemsPerSecond = numeric::isNull(timeDelta) ? std::wstring() :
                                           replaceCpy(_("%x/sec"), L"%x", replaceCpy(_("%x items"), L"%x", formatThreeDigitPrecision(itemsProcessed / timeDelta)));

    pnl_.m_panelGraphBytes->setAttributes(pnl_.m_panelGraphBytes->getAttributes().setCornerText(overallBytesPerSecond, Graph2D::CORNER_TOP_LEFT));
    pnl_.m_panelGraphItems->setAttributes(pnl_.m_panelGraphItems->getAttributes().setCornerText(overallItemsPerSecond, Graph2D::CORNER_TOP_LEFT));


    //show new info box "items processed"
    pnl_.m_panelItemsProcessed->Show();
    pnl_.m_staticTextItemsProcessed->SetLabel(              formatNumber(itemsProcessed));
    pnl_.m_staticTextBytesProcessed->SetLabel(L"(" + formatFilesizeShort(bytesProcessed) + L")");


    if ((itemsTotal < 0 && bytesTotal < 0) || //no total items/bytes: e.g. for pure folder comparison
        (itemsProcessed == itemsTotal && //
         bytesProcessed == bytesTotal))  //...if everything was processed successfully
        pnl_.m_panelItemsRemaining->Hide();
    else
    {
        pnl_.m_staticTextItemsRemaining->SetLabel(              formatNumber(itemsTotal - itemsProcessed));
        pnl_.m_staticTextBytesRemaining->SetLabel(L"(" + formatFilesizeShort(bytesTotal - bytesProcessed) + L")");
    }

    //hide remaining time
    pnl_.m_panelTimeRemaining->Hide();

    const int64_t totalTimeSec = std::chrono::duration_cast<std::chrono::seconds>(stopWatch_.elapsed()).count();
    setText(*pnl_.m_staticTextTimeElapsed, wxTimeSpan::Seconds(totalTimeSec).Format(L"%H:%M:%S"));
    //totalTimeSec < 3600 ? wxTimeSpan::Seconds(totalTimeSec).Format(L"%M:%S") -> let's use full precision for max. clarity: https://freefilesync.org/forum/viewtopic.php?t=6308
    //maybe also should rename to "Total time"!?

    resumeFromSystray(); //if in tray mode...

    //------- change class state -------
    syncStat_ = nullptr;
    abortCb_  = nullptr;
    //----------------------------------

    const wxBitmap statusImage = [&]
    {
        switch (finalStatus)
        {
            case SyncResult::FINISHED_WITH_SUCCESS:
                return getResourceImage(L"status_finished_success");
            case SyncResult::FINISHED_WITH_WARNINGS:
                return getResourceImage(L"status_finished_warnings");
            case SyncResult::FINISHED_WITH_ERROR:
                return getResourceImage(L"status_finished_errors");
            case SyncResult::ABORTED:
                return getResourceImage(L"status_aborted");
        }
        assert(false);
        return wxNullBitmap;
    }();
    pnl_.m_bitmapStatus->SetBitmap(statusImage);

    pnl_.m_staticTextPhase->SetLabel(getFinalStatusLabel(finalStatus));
    //pnl_.m_bitmapStatus->SetToolTip(); -> redundant

    //show status on Windows 7 taskbar
    if (taskbar_.get())
        switch (finalStatus)
        {
            case SyncResult::FINISHED_WITH_SUCCESS:
            case SyncResult::FINISHED_WITH_WARNINGS:
                taskbar_->setStatus(Taskbar::STATUS_NORMAL);
                break;

            case SyncResult::FINISHED_WITH_ERROR:
            case SyncResult::ABORTED:
                taskbar_->setStatus(Taskbar::STATUS_ERROR);
                break;
        }
    //----------------------------------

    setExternalStatus(getFinalStatusLabel(finalStatus), wxString());

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

    //-------------------------------------------------------------

    pnl_.m_notebookResult->SetPadding(wxSize(fastFromDIP(2), 0)); //height cannot be changed

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
    logPanel->setLog(log);
    pnl_.m_notebookResult->AddPage(logPanel, _("Log"), false /*bSelect*/);

    //show log instead of graph if errors occurred! (not required for ignored warnings)
    if (log->getItemCount(MSG_TYPE_ERROR | MSG_TYPE_FATAL_ERROR) > 0)
        pnl_.m_notebookResult->ChangeSelection(pagePosLog);

    //fill image list to cope with wxNotebook image setting design desaster...
    const int imgListSize = getResourceImage(L"log_file_sicon").GetHeight();
    auto imgList = std::make_unique<wxImageList>(imgListSize, imgListSize);

    auto addToImageList = [&](const wxBitmap& bmp)
    {
        assert(bmp.GetWidth () <= imgListSize);
        assert(bmp.GetHeight() <= imgListSize);
        imgList->Add(bmp);
    };
    addToImageList(getResourceImage(L"progress_sicon"));
    addToImageList(getResourceImage(L"log_file_sicon"));

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
    //pnl.m_panelTimeRemaining->Layout(); -> hidden
    //pnl.m_panelTimeElapsed->Layout(); -> needed?

    //play (optional) sound notification after sync has completed -> only play when waiting on results dialog, seems to be pointless otherwise!
    switch (finalStatus)
    {
        case SyncResult::ABORTED:
            break;
        case SyncResult::FINISHED_WITH_ERROR:
        case SyncResult::FINISHED_WITH_WARNINGS:
        case SyncResult::FINISHED_WITH_SUCCESS:
            if (!soundFileSyncComplete_.empty() && fileAvailable(soundFileSyncComplete_))
                wxSound::Play(utfTo<wxString>(soundFileSyncComplete_), wxSOUND_ASYNC);
            //warning: this may fail and show a wxWidgets error message! => must not play when running FFS without user interaction!

            //if (::GetForegroundWindow() != GetHWND())
            //  RequestUserAttention(); -> probably too much since task bar is already colorized with Taskbar::STATUS_ERROR or STATUS_NORMAL
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
    //no UI-update here to avoid cascaded Yield()-call!
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
                                              const std::chrono::system_clock::time_point& syncStartTime,
                                              const wxString& jobName,
                                              const Zstring& soundFileSyncComplete,
                                              bool ignoreErrors,
                                              size_t automaticRetryCount,
                                              PostSyncAction2 postSyncAction)
{
    if (parentWindow) //sync from GUI
    {
        //due to usual "wxBugs", wxDialog on OS X does not float on its parent; wxFrame OTOH does => hack!
        //https://groups.google.com/forum/#!topic/wx-users/J5SjjLaBOQE
        return new SyncProgressDialogImpl<wxDialog>(wxDEFAULT_DIALOG_STYLE | wxMAXIMIZE_BOX | wxMINIMIZE_BOX | wxRESIZE_BORDER,
        [&](wxDialog& progDlg) { return parentWindow; },
        abortCb, notifyWindowTerminate, syncStat, parentWindow, showProgress, autoCloseDialog, syncStartTime, jobName, soundFileSyncComplete, ignoreErrors, automaticRetryCount, postSyncAction);
    }
    else //FFS batch job
    {
        auto dlg = new SyncProgressDialogImpl<wxFrame>(wxDEFAULT_FRAME_STYLE,
        [](wxFrame& progDlg) { return &progDlg; },
        abortCb, notifyWindowTerminate, syncStat, parentWindow, showProgress, autoCloseDialog, syncStartTime, jobName, soundFileSyncComplete, ignoreErrors, automaticRetryCount, postSyncAction);

        //only top level windows should have an icon:
        dlg->SetIcon(getFfsIcon());
        return dlg;
    }
}

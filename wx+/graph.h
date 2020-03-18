// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef GRAPH_H_234425245936567345799
#define GRAPH_H_234425245936567345799

#include <map>
#include <vector>
#include <memory>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/bitmap.h>
#include <zen/string_tools.h>


//elegant 2D graph as wxPanel specialization
namespace zen
{
/*
Example:
    //init graph (optional)
    m_panelGraph->setAttributes(Graph2D::MainAttributes().
                                setLabelX(Graph2D::LABEL_X_BOTTOM, 20, std::make_shared<LabelFormatterTimeElapsed>()).
                                setLabelY(Graph2D::LABEL_Y_RIGHT,  60, std::make_shared<LabelFormatterBytes>()));
    //set graph data
    std::shared_ptr<CurveData> curveDataBytes_ = ...
    m_panelGraph->setCurve(curveDataBytes_, Graph2D::CurveAttributes().setLineWidth(2).setColor(wxColor(0, 192, 0)));
*/

struct CurvePoint
{
    CurvePoint() {}
    CurvePoint(double xVal, double yVal) : x(xVal), y(yVal) {}
    double x = 0;
    double y = 0;
};
inline bool operator==(const CurvePoint& lhs, const CurvePoint& rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }
inline bool operator!=(const CurvePoint& lhs, const CurvePoint& rhs) { return !(lhs == rhs); }


struct CurveData
{
    virtual ~CurveData() {}

    virtual std::pair<double, double> getRangeX() const = 0;
    virtual std::vector<CurvePoint> getPoints(double minX, double maxX, const wxSize& areaSizePx) const = 0; //points outside the draw area are automatically trimmed!
};

//special curve types:
struct ContinuousCurveData : public CurveData
{
    virtual double getValue(double x) const = 0;

private:
    std::vector<CurvePoint> getPoints(double minX, double maxX, const wxSize& areaSizePx) const override;
};

struct SparseCurveData : public CurveData
{
    SparseCurveData(bool addSteps = false) : addSteps_(addSteps) {} //addSteps: add points to get a staircase effect or connect points via a direct line

    virtual std::optional<CurvePoint> getLessEq   (double x) const = 0;
    virtual std::optional<CurvePoint> getGreaterEq(double x) const = 0;

private:
    std::vector<CurvePoint> getPoints(double minX, double maxX, const wxSize& areaSizePx) const override;
    const bool addSteps_;
};


struct ArrayCurveData : public SparseCurveData
{
    virtual double getValue(size_t pos) const = 0;
    virtual size_t getSize ()           const = 0;

private:
    std::pair<double, double> getRangeX() const override { const size_t sz = getSize(); return { 0.0, sz == 0 ? 0.0 : sz - 1.0}; }

    std::optional<CurvePoint> getLessEq(double x) const override
    {
        const size_t sz = getSize();
        const size_t pos = std::min<ptrdiff_t>(std::floor(x), sz - 1); //[!] expect unsigned underflow if empty!
        if (pos < sz)
            return CurvePoint(pos, getValue(pos));
        return {};
    }

    std::optional<CurvePoint> getGreaterEq(double x) const override
    {
        const size_t pos = std::max<ptrdiff_t>(std::ceil(x), 0); //[!] use std::max with signed type!
        if (pos < getSize())
            return CurvePoint(pos, getValue(pos));
        return {};
    }
};


struct VectorCurveData : public ArrayCurveData
{
    std::vector<double>& refData() { return data_; }
private:
    double getValue(size_t pos) const override { return pos < data_.size() ? data_[pos] : 0; }
    size_t getSize()            const override { return data_.size(); }

    std::vector<double> data_;
};

//------------------------------------------------------------------------------------------------------------

struct LabelFormatter
{
    virtual ~LabelFormatter() {}

    //determine convenient graph label block size in unit of data: usually some small deviation on "sizeProposed"
    virtual double getOptimalBlockSize(double sizeProposed) const = 0;

    //create human-readable text for x or y-axis position
    virtual wxString formatText(double value, double optimalBlockSize) const = 0;
};


double nextNiceNumber(double blockSize); //round to next number which is convenient to read, e.g. 2.13 -> 2; 2.7 -> 2.5

struct DecimalNumberFormatter : public LabelFormatter
{
    double   getOptimalBlockSize(double sizeProposed                  ) const override { return nextNiceNumber(sizeProposed); }
    wxString formatText         (double value, double optimalBlockSize) const override { return numberTo<wxString>(value); }
};

//------------------------------------------------------------------------------------------------------------

//emit data selection event
//Usage: wnd.Connect(wxEVT_GRAPH_SELECTION, GraphSelectEventHandler(MyDlg::OnGraphSelection), nullptr, this);
//       void MyDlg::OnGraphSelection(GraphSelectEvent& event);

extern const wxEventType wxEVT_GRAPH_SELECTION;

struct SelectionBlock
{
    CurvePoint from;
    CurvePoint to;
};

class GraphSelectEvent : public wxCommandEvent
{
public:
    GraphSelectEvent(const SelectionBlock& selBlock) : wxCommandEvent(wxEVT_GRAPH_SELECTION), selBlock_(selBlock) {}
    wxEvent* Clone() const override { return new GraphSelectEvent(selBlock_); }

    SelectionBlock getSelection() { return selBlock_; }

private:
    SelectionBlock selBlock_;
};

using GraphSelectEventFunction = void (wxEvtHandler::*)(GraphSelectEvent&);

#define GraphSelectEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(GraphSelectEventFunction, &func)

//------------------------------------------------------------------------------------------------------------

class Graph2D : public wxPanel
{
public:
    Graph2D(wxWindow* parent,
            wxWindowID winid     = wxID_ANY,
            const wxPoint& pos   = wxDefaultPosition,
            const wxSize& size   = wxDefaultSize,
            long style           = wxTAB_TRAVERSAL | wxNO_BORDER,
            const wxString& name = wxPanelNameStr);

    class CurveAttributes
    {
    public:
        CurveAttributes() {} //required by GCC
        CurveAttributes& setColor       (const wxColor& col) { color = col; autoColor = false; return *this; }
        CurveAttributes& fillCurveArea  (const wxColor& col) { fillColor = col; fillMode = FILL_CURVE;   return *this; }
        CurveAttributes& fillPolygonArea(const wxColor& col) { fillColor = col; fillMode = FILL_POLYGON; return *this; }
        CurveAttributes& setLineWidth(size_t width) { lineWidth = static_cast<int>(width); return *this; }

    private:
        friend class Graph2D;

        bool autoColor = true;
        wxColor color;

        enum FillMode
        {
            FILL_NONE,
            FILL_CURVE,
            FILL_POLYGON
        };

        FillMode fillMode = FILL_NONE;
        wxColor fillColor;

        int lineWidth = 2;
    };

    void setCurve(const std::shared_ptr<CurveData>& data, const CurveAttributes& ca = CurveAttributes());
    void addCurve(const std::shared_ptr<CurveData>& data, const CurveAttributes& ca = CurveAttributes());

    static wxColor getBorderColor() { return { 130, 135, 144 }; } //medium grey, the same Win7 uses for other frame borders => not accessible! but no big deal...

    enum PosLabelY
    {
        LABEL_Y_LEFT,
        LABEL_Y_RIGHT,
        LABEL_Y_NONE
    };

    enum PosLabelX
    {
        LABEL_X_TOP,
        LABEL_X_BOTTOM,
        LABEL_X_NONE
    };

    enum PosCorner
    {
        CORNER_TOP_LEFT,
        CORNER_TOP_RIGHT,
        CORNER_BOTTOM_LEFT,
        CORNER_BOTTOM_RIGHT,
    };

    enum SelMode
    {
        SELECT_NONE,
        SELECT_RECTANGLE,
        SELECT_X_AXIS,
        SELECT_Y_AXIS,
    };

    class MainAttributes
    {
    public:
        MainAttributes& setMinX(double newMinX) { minX = newMinX; return *this; }
        MainAttributes& setMaxX(double newMaxX) { maxX = newMaxX; return *this; }

        MainAttributes& setMinY(double newMinY) { minY = newMinY; return *this; }
        MainAttributes& setMaxY(double newMaxY) { maxY = newMaxY; return *this; }

        MainAttributes& setAutoSize() { minX = maxX = minY = maxY = {}; return *this; }

        MainAttributes& setLabelX(PosLabelX posX, int height = -1, std::shared_ptr<LabelFormatter> newLabelFmt = nullptr)
        {
            labelposX = posX;
            if (height >= 0) xLabelHeight = height;
            if (newLabelFmt) labelFmtX = newLabelFmt;
            return *this;
        }
        MainAttributes& setLabelY(PosLabelY posY, int width = -1, std::shared_ptr<LabelFormatter> newLabelFmt = nullptr)
        {
            labelposY = posY;
            if (width >= 0) yLabelWidth = width;
            if (newLabelFmt) labelFmtY = newLabelFmt;
            return *this;
        }

        MainAttributes& setCornerText(const wxString& txt, PosCorner pos) { cornerTexts[pos] = txt; return *this; }

        //accessibility: always set both colors
        MainAttributes& setBaseColors(const wxColor& text, const wxColor& back) { colorText = text; colorBack = back; return *this; }

        MainAttributes& setSelectionMode(SelMode mode) { mouseSelMode = mode; return *this; }

    private:
        friend class Graph2D;

        std::optional<double> minX; //x-range to visualize
        std::optional<double> maxX; //

        std::optional<double> minY; //y-range to visualize
        std::optional<double> maxY; //

        PosLabelX labelposX = LABEL_X_BOTTOM;
        std::optional<int> xLabelHeight;
        std::shared_ptr<LabelFormatter> labelFmtX = std::make_shared<DecimalNumberFormatter>();

        PosLabelY labelposY = LABEL_Y_LEFT;
        std::optional<int> yLabelWidth;
        std::shared_ptr<LabelFormatter> labelFmtY = std::make_shared<DecimalNumberFormatter>();

        std::map<PosCorner, wxString> cornerTexts;

        wxColor colorText = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
        wxColor colorBack = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

        SelMode mouseSelMode = SELECT_RECTANGLE;
    };


    void setAttributes(const MainAttributes& newAttr) { attr_ = newAttr; Refresh(); }
    MainAttributes getAttributes() const { return attr_; }

    std::vector<SelectionBlock> getSelections() const { return oldSel_; }
    void setSelections(const std::vector<SelectionBlock>& sel)
    {
        oldSel_ = sel;
        activeSel_.reset();
        Refresh();
    }
    void clearSelection() { oldSel_.clear(); Refresh(); }

private:
    void OnMouseLeftDown(wxMouseEvent& event);
    void OnMouseMovement(wxMouseEvent& event);
    void OnMouseLeftUp  (wxMouseEvent& event);
    void OnMouseCaptureLost(wxMouseCaptureLostEvent& event);

    void onPaintEvent(wxPaintEvent& event);
    void onSizeEvent(wxSizeEvent& event) { Refresh(); event.Skip(); }

    void render(wxDC& dc) const;

    class MouseSelection
    {
    public:
        MouseSelection(wxWindow& wnd, const wxPoint& posDragStart) : wnd_(wnd), posDragStart_(posDragStart), posDragCurrent(posDragStart) { wnd_.CaptureMouse(); }
        ~MouseSelection() { if (wnd_.HasCapture()) wnd_.ReleaseMouse(); }

        wxPoint getStartPos() const { return posDragStart_; }
        wxPoint& refCurrentPos() { return posDragCurrent; }

        SelectionBlock& refSelection() { return selBlock; } //updated in Graph2d::render(): this is fine, since only what's shown is selected!

    private:
        wxWindow& wnd_;
        const wxPoint posDragStart_;
        wxPoint posDragCurrent;
        SelectionBlock selBlock;
    };
    std::vector<SelectionBlock>     oldSel_; //applied selections
    std::shared_ptr<MouseSelection> activeSel_; //set during mouse selection

    MainAttributes attr_; //global attributes

    std::optional<wxBitmap> doubleBuffer_;

    using CurveList = std::vector<std::pair<std::shared_ptr<CurveData>, CurveAttributes>>;
    CurveList curves_;

    //perf!!! generating the font is *very* expensive! => buffer for Graph2D::render()!
    const wxFont labelFont_ { wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, L"Arial" };
};
}

#endif //GRAPH_H_234425245936567345799

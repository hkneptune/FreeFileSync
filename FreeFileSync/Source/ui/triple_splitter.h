// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TRIPLE_SPLITTER_H_8257804292846842573942534254
#define TRIPLE_SPLITTER_H_8257804292846842573942534254

#include <cassert>
#include <memory>
#include <optional>
#include <wx/window.h>
#include <wx/bitmap.h>
//#include <wx/dcclient.h>


/* manage three contained windows:
    1. left and right window are stretched
    2. middle window is fixed size
    3. middle window position can be changed via mouse with two sash lines
    -----------------
    |      | |      |
    |      | |      |
    |      | |      |
    -----------------                */
namespace fff
{
class TripleSplitter : public wxWindow
{
public:
    TripleSplitter(wxWindow* parent,
                   wxWindowID id      = wxID_ANY,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = 0);

    ~TripleSplitter();

    void setupWindows(wxWindow* winL, wxWindow* winC, wxWindow* winR)
    {
        assert(winL->GetParent() == this && winC->GetParent() == this && winR->GetParent() == this && !GetSizer());
        windowL_ = winL;
        windowC_ = winC;
        windowR_ = winR;
        updateWindowSizes();
    }

    int getSashOffset() const { return centerOffset_; }
    void setSashOffset(int off) { centerOffset_ = off; updateWindowSizes(); }

private:
    void updateWindowSizes();
    int getCenterWidth() const;
    int getCenterPosX() const; //return normalized posX
    int getCenterPosXOptimal() const;

    void onPaintEvent(wxPaintEvent& event);
    bool hitOnSashLine(int posX) const;

    void onMouseLeftDown(wxMouseEvent& event);
    void onMouseLeftUp(wxMouseEvent& event);
    void onMouseMovement(wxMouseEvent& event);
    void onLeaveWindow(wxMouseEvent& event);
    void onMouseCaptureLost(wxMouseCaptureLostEvent& event);
    void onMouseLeftDouble(wxMouseEvent& event);

    class SashMove;
    std::unique_ptr<SashMove> activeMove_;

    int centerOffset_ = 0; //offset to add after "gravity" stretching
    const int sashSize_;
    const int childWindowMinSize_;

    wxWindow* windowL_ = nullptr;
    wxWindow* windowC_ = nullptr;
    wxWindow* windowR_ = nullptr;

    std::optional<wxBitmap> doubleBuffer_;
};
}

#endif //TRIPLE_SPLITTER_H_8257804292846842573942534254

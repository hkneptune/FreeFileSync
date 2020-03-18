// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TOOLTIP_H_8912740832170515
#define TOOLTIP_H_8912740832170515

#include <wx/window.h>


namespace zen
{
class Tooltip
{
public:
    Tooltip(wxWindow& parent) : parent_(parent) {} //parent needs to live at least as long as this instance!

    void show(const wxString& text,
              wxPoint mousePos, //absolute screen coordinates
              const wxBitmap* bmp = nullptr);
    void hide();

private:
    class TooltipDlgGenerated;
    TooltipDlgGenerated* tipWindow_ = nullptr;
    wxWindow& parent_;
};
}

#endif //TOOLTIP_H_8912740832170515

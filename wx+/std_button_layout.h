// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STD_BUTTON_LAYOUT_H_183470321478317214
#define STD_BUTTON_LAYOUT_H_183470321478317214

#include <algorithm>
#include <wx/sizer.h>
#include <wx/button.h>
#include "dc.h"


namespace zen
{
struct StdButtons
{
    StdButtons& setAffirmative   (wxButton* btn) { btnYes    = btn; return *this; }
    StdButtons& setAffirmativeAll(wxButton* btn) { btnYesAll = btn; return *this; }
    StdButtons& setNegative      (wxButton* btn) { btnNo     = btn; return *this; }
    StdButtons& setCancel        (wxButton* btn) { btnCancel = btn; return *this; }

    wxButton* btnYes    = nullptr;
    wxButton* btnYesAll = nullptr;
    wxButton* btnNo     = nullptr;
    wxButton* btnCancel = nullptr;
};

void setStandardButtonLayout(wxBoxSizer& sizer, const StdButtons& buttons = StdButtons());
//sizer width will change! => call wxWindow::Fit and wxWindow::Layout











//--------------- impelementation -------------------------------------------
inline
void setStandardButtonLayout(wxBoxSizer& sizer, const StdButtons& buttons)
{
    assert(sizer.GetOrientation() == wxHORIZONTAL);

    StdButtons buttonsTmp = buttons;

    auto detach = [&](wxButton*& btn)
    {
        if (btn)
        {
            assert(btn->GetContainingSizer() == &sizer);
            if (btn->IsShown())
            {
                bool rv = sizer.Detach(btn);
                assert(rv);
                if (!rv)
                    btn = nullptr;
            }
            else
                btn = nullptr;
        }
    };

    detach(buttonsTmp.btnYes);
    detach(buttonsTmp.btnYesAll);
    detach(buttonsTmp.btnNo);
    detach(buttonsTmp.btnCancel);

    //GNOME Human Interface Guidelines: https://developer.gnome.org/hig-book/3.2/hig-book.html#alert-spacing
    const int spaceH    = fastFromDIP( 6); //OK
    const int spaceRimH = fastFromDIP(12); //OK
    const int spaceRimV = fastFromDIP(12); //OK

    bool settingFirstButton = true;
    auto attach = [&](wxButton* btn)
    {
        if (btn)
        {
            assert(btn->GetMinSize().GetHeight() == -1); //let OS or this routine do the sizing! note: OS X does not allow changing the (visible!) button height!
            const int defaultHeight = wxButton::GetDefaultSize().GetHeight(); //buffered by wxWidgets
            btn->SetMinSize({-1, std::max(defaultHeight, fastFromDIP(31))}); //default button height is much too small => increase!

            if (settingFirstButton)
                settingFirstButton = false;
            else
                sizer.Add(spaceH, 0);
            sizer.Add(btn, 0, wxTOP | wxBOTTOM | wxALIGN_CENTER_VERTICAL, spaceRimV);
        }
    };

    //set border on left considering existing items
    if (sizer.GetChildren().GetCount() > 0) //for yet another retarded reason wxWidgets will have wxSizer::GetItem(0) cause an assert rather than just return nullptr as documented
        if (wxSizerItem* item = sizer.GetItem(static_cast<size_t>(0)))
        {
            assert(item->GetBorder() <= spaceRimV); //pragmatic check: other controls in the sizer should not have a larger border
            int flag = item->GetFlag();
            if (flag & wxLEFT)
            {
                flag &= ~wxLEFT;
                item->SetFlag(flag);
            }
            sizer.Insert(static_cast<size_t>(0), spaceRimH, 0);
        }

    sizer.Add(spaceRimH, 0);
    attach(buttonsTmp.btnNo);
    attach(buttonsTmp.btnCancel);
    attach(buttonsTmp.btnYesAll);
    attach(buttonsTmp.btnYes);

    sizer.Add(spaceRimH, 0);

    //OS X: there should be at least one button following the gap after the "dangerous" no-button
    assert(buttonsTmp.btnYes || buttonsTmp.btnCancel);
}
}

#endif //STD_BUTTON_LAYOUT_H_183470321478317214

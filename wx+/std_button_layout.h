// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef STD_BUTTON_LAYOUT_H_183470321478317214
#define STD_BUTTON_LAYOUT_H_183470321478317214

//#include <algorithm>
#include <wx/sizer.h>
#include <wx/button.h>
#include "dc.h"


namespace zen
{
struct StdButtons
{
    StdButtons& setAffirmative   (wxButton* btn) { btnYes    = btn; return *this; }
    StdButtons& setAffirmativeAll(wxButton* btn) { btnYes2   = btn; return *this; }
    StdButtons& setNegative      (wxButton* btn) { btnNo     = btn; return *this; }
    StdButtons& setCancel        (wxButton* btn) { btnCancel = btn; return *this; }

    wxButton* btnYes    = nullptr;
    wxButton* btnYes2   = nullptr;
    wxButton* btnNo     = nullptr;
    wxButton* btnCancel = nullptr;
};

void setStandardButtonLayout(wxBoxSizer& sizer, const StdButtons& buttons = StdButtons());
//sizer width will change! => call wxWindow::Fit and wxWindow::Dimensions


inline
constexpr int getMenuIconDipSize()
{
    return 20;
}


inline
int getDefaultButtonHeight()
{
    const int defaultHeight = wxButton::GetDefaultSize().GetHeight(); //buffered by wxWidgets
    return std::max(defaultHeight, dipToWxsize(31)); //default button height is much too small => increase!
}










//--------------- impelementation -------------------------------------------
inline
void setStandardButtonLayout(wxBoxSizer& sizer, const StdButtons& buttons)
{
    assert(sizer.GetOrientation() == wxHORIZONTAL);

    //GNOME Human Interface Guidelines: https://developer.gnome.org/hig-book/3.2/hig-book.html#alert-spacing
    const int spaceH    = dipToWxsize( 6); //OK
    const int spaceRimH = dipToWxsize(12); //OK
    const int spaceRimV = dipToWxsize(12); //OK

    StdButtons buttonsTmp = buttons;

    auto detach = [&](wxButton*& btn)
    {
        if (btn)
        {
            assert(btn->GetContainingSizer() == &sizer);
            if (btn->IsShown() && sizer.Detach(btn))
                return;

            assert(false); //why is it hidden!?
            btn = nullptr;
        }
    };

    detach(buttonsTmp.btnYes);
    detach(buttonsTmp.btnYes2);
    detach(buttonsTmp.btnNo);
    detach(buttonsTmp.btnCancel);


    //"All your fixed-size spacers are belong to us!" => have a clean slate: consider repeated setStandardButtonLayout() calls
    for (size_t pos = sizer.GetItemCount(); pos-- > 0;)
        if (wxSizerItem& item = *sizer.GetItem(pos);
            item.IsSpacer() && item.GetProportion() == 0 && item.GetSize().y == 0)
        {
            [[maybe_unused]] const bool rv = sizer.Detach(pos);
            assert(rv);
        }

    //set border on left considering existing items
    if (!sizer.IsEmpty()) //for yet another retarded reason wxWidgets will have wxSizer::GetItem(0) cause an assert rather than just return nullptr as documented
        if (wxSizerItem& item = *sizer.GetItem(static_cast<size_t>(0));
            item.IsShown())
        {
            assert(item.GetBorder() <= spaceRimV); //pragmatic check: other controls in the sizer should not have a larger border

            if (const int flag = item.GetFlag();
                flag & wxLEFT)
                item.SetFlag(flag & ~wxLEFT);

            sizer.Prepend(spaceRimH, 0);
        }


    bool settingFirstButton = true;
    auto attach = [&](wxButton* btn)
    {
        if (btn)
        {
            assert(btn->GetMinSize().GetHeight() == -1); //let OS or this routine do the sizing! note: OS X does not allow changing the (visible!) button height!
            btn->SetMinSize({-1, getDefaultButtonHeight()});

            if (settingFirstButton)
                settingFirstButton = false;
            else
                sizer.Add(spaceH, 0);
            sizer.Add(btn, 0, wxTOP | wxBOTTOM | wxALIGN_CENTER_VERTICAL, spaceRimV);
        }
    };

    sizer.Add(spaceRimH, 0);
    attach(buttonsTmp.btnNo);
    attach(buttonsTmp.btnCancel);
    attach(buttonsTmp.btnYes2);
    attach(buttonsTmp.btnYes);

    sizer.Add(spaceRimH, 0);

    //OS X: there should be at least one button following the gap after the "dangerous" no-button
    assert(buttonsTmp.btnYes || buttonsTmp.btnCancel);
}
}

#endif //STD_BUTTON_LAYOUT_H_183470321478317214

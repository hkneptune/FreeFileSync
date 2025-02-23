// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef COLOR_TOOLS_H_18301239864123785613
#define COLOR_TOOLS_H_18301239864123785613

#include <zen/basic_math.h>
#include <wx/colour.h>


namespace zen
{
inline
double srgbDecode(unsigned char c) //https://en.wikipedia.org/wiki/SRGB
{
    const double c_ = c / 255.0;
    return c_ <= 0.04045 ? c_ / 12.92 : std::pow((c_ + 0.055) / 1.055, 2.4);
}


inline
unsigned char srgbEncode(double c)
{
    const double c_ = c <= 0.0031308 ? c * 12.92 : std::pow(c, 1 / 2.4) * 1.055 - 0.055;
    return std::clamp<int>(std::round(c_ * 255), 0, 255);
}


inline //https://www.w3.org/WAI/GL/wiki/Relative_luminance
double relLuminance(double r, double g, double b) //input: gamma-decoded sRGB
{
    return 0.2126 * r + 0.7152 * g + 0.0722 * b; //= the Y part of CIEXYZ
}


inline
double relativeLuminance(const wxColor& col) //[0, 1]
{
    assert(col.Alpha() == wxALPHA_OPAQUE);
    return relLuminance(srgbDecode(col.Red()), srgbDecode(col.Green()), srgbDecode(col.Blue()));
}


inline
double relativeContrast(const wxColor& c1, const wxColor& c2)
{
    //https://www.w3.org/TR/2008/REC-WCAG20-20081211/#contrast-ratiodef
    //https://snook.ca/technical/colour_contrast/colour.html
    double lum1 = relativeLuminance(c1);
    double lum2 = relativeLuminance(c2);
    if (lum1 < lum2)
        std::swap(lum1, lum2);
    return (lum1 + 0.05) / (lum2 + 0.05);
}


namespace
{
//get first color between [col1, white/black] (assuming direct line in decoded sRGB) where minimum contrast is satisfied against col2
wxColor enhanceContrast(wxColor col1, const wxColor& col2, double contrastRatioMin)
{
    const wxColor colMax = relativeLuminance(col2) < 0.17912878474779204 /* = sqrt(0.05 * 1.05) - 0.05 */ ? 0xffffff : 0;
    //equivalent to: relativeContrast(col2, *wxWHITE) > relativeContrast(col2, *wxBLACK) ? *wxWHITE : *wxBLACK

    assert(col2.Alpha() == wxALPHA_OPAQUE);
    if (col2.Alpha() != wxALPHA_OPAQUE)
        return *wxRED; //make some noise

    /*  Caveat: macOS uses partially-transparent colors! e.g. in #RGBA:
        wxSYS_COLOUR_GRAYTEXT   #FFFFFF3F
        wxSYS_COLOUR_WINDOWTEXT #FFFFFFD8
        wxSYS_COLOUR_WINDOW     #171717FF      */
    if (col1.Alpha() != wxALPHA_OPAQUE)
    {
        auto calcChannel = [a = col1.Alpha()](unsigned char f, unsigned char b)
        {
            return static_cast<unsigned char>(numeric::intDivRound(f * a + b * (255 - a), 255));
        };

        col1 = wxColor(calcChannel(col1.Red  (), col2.Red  ()),
                       calcChannel(col1.Green(), col2.Green()),
                       calcChannel(col1.Blue (), col2.Blue ()));
    }

    //---------------------------------------------------------------
    assert(contrastRatioMin >= 3); //lower values (especially near 1) probably aren't sensible mathematically, also: W3C recommends >= 4.5 for base AA compliance
    auto contrast = [](double lum1, double lum2) //input: relative luminance
    {
        if (lum1 < lum2)
            std::swap(lum1, lum2);
        return (lum1 + 0.05) / (lum2 + 0.05);
    };
    const double r_1 = srgbDecode(col1.Red());
    const double g_1 = srgbDecode(col1.Green());
    const double b_1 = srgbDecode(col1.Blue());
    const double r_m = srgbDecode(colMax.Red());
    const double g_m = srgbDecode(colMax.Green());
    const double b_m = srgbDecode(colMax.Blue());

    const double lum_1 = relLuminance(r_1, g_1, b_1);
    const double lum_m = relLuminance(r_m, g_m, b_m);
    const double lum_2 = relativeLuminance(col2);

    if (contrast(lum_1, lum_2) >= contrastRatioMin)
        return col1; //nothing to do!

    if (contrast(lum_m, lum_2) <= contrastRatioMin)
    {
        assert(false); //problem!
        return colMax;
    }

    if (lum_m < lum_2)
        contrastRatioMin = 1 / contrastRatioMin;

    const double lum_t = contrastRatioMin * (lum_2 + 0.05) - 0.05; //target luminance
    const double t = (lum_t - lum_1) / (lum_m - lum_1);

    return wxColor(srgbEncode(t * (r_m - r_1) + r_1),
                   srgbEncode(t * (g_m - g_1) + g_1),
                   srgbEncode(t * (b_m - b_1) + b_1));
}
}

#if 0
//toy sample code: gamma-encoded sRGB -> CIEXYZ -> CIELAB and back: input === output RGB color (verified)
wxColor colorConversion(const wxColor& col)
{
    assert(col.GetAlpha() == wxALPHA_OPAQUE);
    const double r = srgbDecode(col.Red());
    const double g = srgbDecode(col.Green());
    const double b = srgbDecode(col.Blue());

    //https://en.wikipedia.org/wiki/SRGB#Correspondence_to_CIE_XYZ_stimulus
    const double x = 0.4124 * r + 0.3576 * g + 0.1805 * b;
    const double y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    const double z = 0.0193 * r + 0.1192 * g + 0.9505 * b;
    //-----------------------------------------------
    //https://en.wikipedia.org/wiki/CIELAB_color_space#Converting_between_CIELAB_and_CIEXYZ_coordinates
    using numeric::power;
    auto f = [](double t)
    {
        constexpr double delta = 6.0 / 29;
        return t > power<3>(delta) ?
               std::pow(t, 1.0 / 3) :
               t / (3 * power<2>(delta)) + 4.0 / 29;
    };
    const double L_ = 116 * f(y) - 16;                //[   0, 100]
    const double a_ = 500 * (f(x / 0.950489) - f(y)); //[-128, 127]
    const double b_ = 200 * (f(y) - f(z / 1.088840)); //[-128, 127]
    //-----------------------------------------------
    auto f_1 = [](double t)
    {
        constexpr double delta = 6.0 / 29;
        return t > delta ?
               power<3>(t) :
               3 * power<2>(delta) * (t - 4.0 / 29);
    };
    const double x2 = 0.950489 * f_1((L_ + 16) / 116 + a_ / 500);
    const double y2 =            f_1((L_ + 16) / 116);
    const double z2 = 1.088840 * f_1((L_ + 16) / 116 - b_ / 200);
    //-----------------------------------------------
    const double r2 =  3.2406255 * x2 + -1.5372080 * y2 + -0.4986286 * z2;
    const double g2 = -0.9689307 * x2 +  1.8757561 * y2 +  0.0415175 * z2;
    const double b2 =  0.0557101 * x2 + -0.2040211 * y2 +  1.0569959 * z2;

    return wxColor(srgbEncode(r2), srgbEncode(g2), srgbEncode(b2));
}


//https://en.wikipedia.org/wiki/HSL_and_HSV
wxColor hsvColor(double h, double s, double v) //h within [0, 360), s, v within [0, 1]
{
    //make input values fit into bounds
    if (h > 360)
        h -= static_cast<int>(h / 360) * 360;
    else if (h < 0)
        h -= static_cast<int>(h / 360) * 360 - 360;
    s = std::clamp(s, 0.0, 1.0);
    v = std::clamp(v, 0.0, 1.0);
    //------------------------------------
    const int h_i = h / 60;
    const float f = h / 60 - h_i;

    auto to8Bit = [](double val) -> unsigned char
    {
        return std::clamp<int>(std::round(val * 255), 0, 255);
    };

    const unsigned char p  = to8Bit(v * (1 - s));
    const unsigned char q  = to8Bit(v * (1 - s * f));
    const unsigned char t  = to8Bit(v * (1 - s * (1 - f)));
    const unsigned char vi = to8Bit(v);

    switch (h_i)
    {
        case 0:
            return wxColor(vi, t, p);
        case 1:
            return wxColor(q, vi, p);
        case 2:
            return wxColor(p, vi, t);
        case 3:
            return wxColor(p, q, vi);
        case 4:
            return wxColor(t, p, vi);
        case 5:
            return wxColor(vi, p, q);
    }
    assert(false);
    return *wxBLACK;
}
#endif
}

#endif //COLOR_TOOLS_H_18301239864123785613

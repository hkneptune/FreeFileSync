// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef IMAGE_TOOLS_H_45782456427634254
#define IMAGE_TOOLS_H_45782456427634254

#include <numeric>
#include <wx/image.h>
#include <zen/basic_math.h>
#include <wx/colour.h>


namespace zen
{
enum class ImageStackLayout
{
    horizontal,
    vertical
};

enum class ImageStackAlignment //one-dimensional unlike wxAlignment
{
    center,
    left,
    right,
    top = left,
    bottom = right,
};
wxImage stackImages(const wxImage& img1, const wxImage& img2, ImageStackLayout dir, ImageStackAlignment align, int gap = 0);

wxImage createImageFromText(const wxString& text, const wxFont& font, const wxColor& col);

wxImage layOver(const wxImage& back, const wxImage& front, int alignment = wxALIGN_CENTER);

wxImage greyScale(const wxImage&  img); //greyscale + brightness adaption
wxImage greyScaleIfDisabled(const wxImage& img, bool enabled);

void adjustBrightness(wxImage& img, int targetLevel);
double getAvgBrightness(const wxImage& img); //in [0, 255]
void brighten(wxImage& img, int level); //level: delta per channel in points

void convertToVanillaImage(wxImage& img); //add alpha channel if missing + remove mask if existing

//wxColor gradient(const wxColor& from, const wxColor& to, double fraction); //maps fraction within [0, 1] to an intermediate color

//wxColor hsvColor(double h, double s, double v); //h within [0, 360), s, v within [0, 1]

//does *not* fuck up alpha channel like naive bilinear implementations, e.g. wxImage::Scale()
wxImage bilinearScale(const wxImage& img, int width, int height);

wxImage shrinkImage(const wxImage& img, int maxWidth /*optional*/, int maxHeight /*optional*/);
inline wxImage shrinkImage(const wxImage& img, int maxSize) { return shrinkImage(img, maxSize, maxSize); }

wxImage resizeCanvas(const wxImage& img, wxSize newSize, int alignment);

wxImage rectangleImage(wxSize size, const wxColor& col);
wxImage rectangleImage(wxSize size, const wxColor& innerCol, const wxColor& borderCol, int borderWidth);










//################################### implementation ###################################

inline
wxImage greyScale(const wxImage& img) //TODO support gamma-decoding and perceptual colors!?
{
    wxImage output = img.ConvertToGreyscale(1.0 / 3, 1.0 / 3, 1.0 / 3); //treat all channels equally
    adjustBrightness(output, 160);
    return output;
}


inline
wxImage greyScaleIfDisabled(const wxImage& img, bool enabled)
{
    if (enabled) //avoid ternary WTF
        return img; //ref-counted
    else
        return greyScale(img);
}


inline
double getAvgBrightness(const wxImage& img) //TODO: consider gamma-encoded sRGB!?
{
    const int width  = img.GetWidth ();
    const int height = img.GetHeight();
    if (width <= 0 || height <= 0) return 0;

    const unsigned char* rgb = img.GetData();
    const unsigned char* rgbEnd = rgb + 3 * width * height;

    if (img.HasAlpha())
    {
        const unsigned char* alpha = img.GetAlpha();

        const double divisor = 3.0 * std::accumulate(alpha, alpha + width * height, 0.0);
        if (numeric::isNull(divisor))
            return 0;

        double dividend = 0;
        while (rgb < rgbEnd)
        {
            const double a = static_cast<double>(*alpha++);
            dividend += *rgb++ * a;
            dividend += *rgb++ * a;
            dividend += *rgb++ * a;
        }
        return dividend / divisor; //average weighted by alpha channel
    }
    else
        return std::accumulate(rgb, rgbEnd, 0.0) / (3.0 * width * height);
}


inline
void brighten(wxImage& img, int level)
{
    if (img.GetWidth() <= 0 || img.GetHeight() <= 0)
        return;

    if (level > 0)
        for (unsigned char& c : std::span(img.GetData(), 3 * img.GetWidth() * img.GetHeight()))
            c = static_cast<unsigned char>(std::min(c + level, 255));
    else
        for (unsigned char& c : std::span(img.GetData(), 3 * img.GetWidth() * img.GetHeight()))
            c = static_cast<unsigned char>(std::max(c + level, 0));
}


inline
void adjustBrightness(wxImage& img, int targetLevel)
{
    brighten(img, targetLevel - getAvgBrightness(img));
}
}

#endif //IMAGE_TOOLS_H_45782456427634254

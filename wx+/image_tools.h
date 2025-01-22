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

wxImage createImageFromText(const wxString& text, const wxFont& font, const wxColor& col, ImageStackAlignment textAlign = ImageStackAlignment::left); //center/left/right

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
        return img;
    else
        return greyScale(img);
}


inline
double getAvgBrightness(const wxImage& img) //TODO: consider gamma-encoded sRGB!?
{
    const int pixelCount = img.GetWidth() * img.GetHeight();
    auto pixBegin = img.GetData();

    if (pixelCount > 0 && pixBegin)
    {
        auto pixEnd = pixBegin + 3 * pixelCount; //RGB

        if (img.HasAlpha())
        {
            const unsigned char* alphaFirst = img.GetAlpha();

            //calculate average weighted by alpha channel
            double dividend = 0;
            for (auto it = pixBegin; it != pixEnd; ++it)
                dividend += *it * static_cast<double>(alphaFirst[(it - pixBegin) / 3]);

            const double divisor = 3.0 * std::accumulate(alphaFirst, alphaFirst + pixelCount, 0.0);

            return numeric::isNull(divisor) ? 0 : dividend / divisor;
        }
        else
            return std::accumulate(pixBegin, pixEnd, 0.0) / (3.0 * pixelCount);
    }
    return 0;
}


inline
void brighten(wxImage& img, int level)
{
    if (auto pixBegin = img.GetData())
    {
        const int pixelCount = img.GetWidth() * img.GetHeight();
        auto pixEnd = pixBegin + 3 * pixelCount; //RGB
        if (level > 0)
            std::for_each(pixBegin, pixEnd, [level](unsigned char& c) { c = static_cast<unsigned char>(std::min(c + level, 255)); });
        else
            std::for_each(pixBegin, pixEnd, [level](unsigned char& c) { c = static_cast<unsigned char>(std::max(c + level, 0)); });
    }
}


inline
void adjustBrightness(wxImage& img, int targetLevel)
{
    brighten(img, targetLevel - getAvgBrightness(img));
}
}

#endif //IMAGE_TOOLS_H_45782456427634254

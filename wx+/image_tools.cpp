// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "image_tools.h"
#include <zen/string_tools.h>
#include <zen/zstring.h>
#include <wx/app.h>


using namespace zen;

namespace
{
void writeToImage(const wxImage& source, wxImage& target, const wxPoint& pos)
{
    const int srcWidth  = source.GetWidth ();
    const int srcHeight = source.GetHeight();
    const int trgWidth  = target.GetWidth ();

    if (srcWidth > 0 && srcHeight > 0)
    {
        assert(0 <= pos.x && pos.x + srcWidth  <= trgWidth          ); //draw area must be a
        assert(0 <= pos.y && pos.y + srcHeight <= target.GetHeight()); //subset of target image!
        assert(target.HasAlpha());

        {
            const unsigned char* sourcePtr = source.GetData();
            unsigned char*       targetPtr = target.GetData() + 3 * (pos.x + pos.y * trgWidth);

            for (int row = 0; row < srcHeight; ++row)
                ::memcpy(targetPtr + 3 * row * trgWidth, sourcePtr + 3 * row * srcWidth, 3 * srcWidth);
        }

        //handle alpha channel
        {
            unsigned char* targetPtr = target.GetAlpha() + pos.x + pos.y * trgWidth;
            if (source.HasAlpha())
            {
                const unsigned char* sourcePtr = source.GetAlpha();
                for (int row = 0; row < srcHeight; ++row)
                    ::memcpy(targetPtr + row * trgWidth, sourcePtr + row * srcWidth, srcWidth);
            }
            else
                for (int row = 0; row < srcHeight; ++row)
                    ::memset(targetPtr + row * trgWidth, wxIMAGE_ALPHA_OPAQUE, srcWidth);
        }
    }
}
}


wxImage zen::stackImages(const wxImage& img1, const wxImage& img2, ImageStackLayout dir, ImageStackAlignment align, int gap)
{
    assert(gap >= 0);
    gap = std::max(0, gap);

    const int img1Width  = img1.GetWidth ();
    const int img1Height = img1.GetHeight();
    const int img2Width  = img2.GetWidth ();
    const int img2Height = img2.GetHeight();

    int width  = std::max(img1Width,  img2Width);
    int height = std::max(img1Height, img2Height);
    switch (dir)
    {
        case ImageStackLayout::HORIZONTAL:
            width  = img1Width + gap + img2Width;
            break;

        case ImageStackLayout::VERTICAL:
            height = img1Height + gap + img2Height;
            break;
    }
    wxImage output(width, height);
    output.SetAlpha();
    ::memset(output.GetAlpha(), wxIMAGE_ALPHA_TRANSPARENT, width * height);

    auto calcPos = [&](int imageExtent, int totalExtent)
    {
        switch (align)
        {
            case ImageStackAlignment::CENTER:
                return (totalExtent - imageExtent) / 2;
            case ImageStackAlignment::LEFT:
                return 0;
            case ImageStackAlignment::RIGHT:
                return totalExtent - imageExtent;
        }
        assert(false);
        return 0;
    };

    switch (dir)
    {
        case ImageStackLayout::HORIZONTAL:
            writeToImage(img1, output, wxPoint(0,               calcPos(img1Height, height)));
            writeToImage(img2, output, wxPoint(img1Width + gap, calcPos(img2Height, height)));
            break;

        case ImageStackLayout::VERTICAL:
            writeToImage(img1, output, wxPoint(calcPos(img1Width, width), 0));
            writeToImage(img2, output, wxPoint(calcPos(img2Width, width), img1Height + gap));
            break;
    }
    return output;
}


namespace
{
void calcAlphaForBlackWhiteImage(wxImage& image) //assume black text on white background
{
    assert(image.HasAlpha());
    if (unsigned char* alphaPtr = image.GetAlpha())
    {
        const int pixelCount = image.GetWidth() * image.GetHeight();
        const unsigned char* dataPtr = image.GetData();
        for (int i = 0; i < pixelCount; ++ i)
        {
            const unsigned char r = *dataPtr++;
            const unsigned char g = *dataPtr++;
            const unsigned char b = *dataPtr++;

            //black(0,0,0) becomes fully opaque(255), while white(255,255,255) becomes transparent(0)
            alphaPtr[i] = static_cast<unsigned char>((255 - r + 255 - g + 255 - b) / 3); //mixed mode arithmetics!
        }
    }
}


std::vector<std::pair<wxString, wxSize>> getTextExtentInfo(const wxString& text, const wxFont& font)
{
    wxMemoryDC dc; //the context used for bitmaps
    dc.SetFont(font); //the font parameter of GetMultiLineTextExtent() is not evalated on OS X, wxWidgets 2.9.5, so apply it to the DC directly!

    std::vector<std::pair<wxString, wxSize>> lineInfo; //text + extent
    for (const wxString& line : split(text, L"\n", SplitType::ALLOW_EMPTY))
        lineInfo.emplace_back(line, line.empty() ? wxSize() : dc.GetTextExtent(line));

    return lineInfo;
}
}

wxImage zen::createImageFromText(const wxString& text, const wxFont& font, const wxColor& col, ImageStackAlignment textAlign)
{
    //assert(!contains(text, L"&")); //accelerator keys not supported here
    wxString textFmt = replaceCpy(text, L"&", L"", false);

    //for some reason wxDC::DrawText messes up "weak" bidi characters even when wxLayout_RightToLeft is set! (--> arrows in hebrew/arabic)
    //=> use mark characters instead:
    if (wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft)
        textFmt = RTL_MARK + textFmt + RTL_MARK;

    const std::vector<std::pair<wxString, wxSize>> lineInfo = getTextExtentInfo(textFmt, font);

    int maxWidth   = 0;
    int lineHeight = 0;
    for (const auto& li : lineInfo)
    {
        maxWidth   = std::max(maxWidth,   li.second.GetWidth());
        lineHeight = std::max(lineHeight, li.second.GetHeight()); //wxWidgets comment "GetTextExtent will return 0 for empty string"
    }
    if (maxWidth == 0 || lineHeight == 0)
        return wxImage();

    wxBitmap newBitmap(maxWidth, lineHeight * lineInfo.size()); //seems we don't need to pass 24-bit depth here even for high-contrast color schemes
    {
        wxMemoryDC dc(newBitmap);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();

        dc.SetTextForeground(*wxBLACK); //for use in calcAlphaForBlackWhiteImage
        dc.SetTextBackground(*wxWHITE); //
        dc.SetFont(font);

        int posY = 0;
        for (const auto& li : lineInfo)
        {
            if (!li.first.empty())
                switch (textAlign)
                {
                    case ImageStackAlignment::LEFT:
                        dc.DrawText(li.first, wxPoint(0, posY));
                        break;
                    case ImageStackAlignment::RIGHT:
                        dc.DrawText(li.first, wxPoint(maxWidth - li.second.GetWidth(), posY));
                        break;
                    case ImageStackAlignment::CENTER:
                        dc.DrawText(li.first, wxPoint((maxWidth - li.second.GetWidth()) / 2, posY));
                        break;
                }

            posY += lineHeight;
        }
    }

    //wxDC::DrawLabel() doesn't respect alpha channel => calculate alpha values manually:
    wxImage output(newBitmap.ConvertToImage());
    output.SetAlpha();

    calcAlphaForBlackWhiteImage(output);

    //apply actual text color
    unsigned char* dataPtr = output.GetData();
    const int pixelCount = output.GetWidth() * output.GetHeight();
    for (int i = 0; i < pixelCount; ++ i)
    {
        *dataPtr++ = col.Red();
        *dataPtr++ = col.Green();
        *dataPtr++ = col.Blue();
    }
    return output;
}


void zen::convertToVanillaImage(wxImage& img)
{
    if (!img.HasAlpha())
    {
        const int width  = img.GetWidth ();
        const int height = img.GetHeight();
        if (width <= 0 || height <= 0) return;

        unsigned char mask_r = 0;
        unsigned char mask_g = 0;
        unsigned char mask_b = 0;
        const bool haveMask = img.HasMask() && img.GetOrFindMaskColour(&mask_r, &mask_g, &mask_b);
        //check for mask before calling wxImage::GetOrFindMaskColour() to skip needlessly searching for new mask color

        img.SetAlpha();
        ::memset(img.GetAlpha(), wxIMAGE_ALPHA_OPAQUE, width * height);

        //wxWidgets, as always, tries to be more clever than it really is and fucks up wxStaticBitmap if wxBitmap is fully opaque:
        img.GetAlpha()[width * height - 1] = 254;

        if (haveMask)
        {
            img.SetMask(false);
            unsigned char*       alphaPtr = img.GetAlpha();
            const unsigned char* dataPtr  = img.GetData();

            const int pixelCount = width * height;
            for (int i = 0; i < pixelCount; ++ i)
            {
                const unsigned char r = *dataPtr++;
                const unsigned char g = *dataPtr++;
                const unsigned char b = *dataPtr++;

                if (r == mask_r &&
                    g == mask_g &&
                    b == mask_b)
                    alphaPtr[i] = wxIMAGE_ALPHA_TRANSPARENT;
            }
        }
    }
    else
    {
        assert(!img.HasMask());
    }
}

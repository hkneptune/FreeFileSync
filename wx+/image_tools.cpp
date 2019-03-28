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
void writeToImage(wxImage& output, const wxImage& top, const wxPoint& pos)
{
    const int topWidth  = top.GetWidth ();
    const int topHeight = top.GetHeight();
    const int outWidth  = output.GetWidth();

    assert(0 <= pos.x && pos.x + topWidth  <= outWidth          ); //draw area must be a
    assert(0 <= pos.y && pos.y + topHeight <= output.GetHeight()); //subset of output image!
    assert(top.HasAlpha() && output.HasAlpha());

    //https://en.wikipedia.org/wiki/Alpha_compositing
    const unsigned char* topRgb   = top.GetData();
    const unsigned char* topAlpha = top.GetAlpha();

    for (int y = 0; y < topHeight; ++y)
    {
        unsigned char* outRgb   = output.GetData () + 3 * (pos.x + (pos.y + y) * outWidth);
        unsigned char* outAlpha = output.GetAlpha() +      pos.x + (pos.y + y) * outWidth;

        for (int x = 0; x < topWidth; ++x)
        {
            const int w1 = *topAlpha; //alpha-composition interpreted as weighted average
            const int w2 = *outAlpha * (255 - w1) / 255;
            const int wSum = w1 + w2;

            auto calcColor = [w1, w2, wSum](unsigned char colTop, unsigned char colBot)
            {
                return static_cast<unsigned char>(wSum == 0 ? 0 : (colTop * w1 + colBot * w2) / wSum);
            };
            outRgb[0] = calcColor(topRgb[0], outRgb[0]);
            outRgb[1] = calcColor(topRgb[1], outRgb[1]);
            outRgb[2] = calcColor(topRgb[2], outRgb[2]);

            *outAlpha = static_cast<unsigned char>(wSum);

            topRgb += 3;
            outRgb += 3;
            ++topAlpha;
            ++outAlpha;
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

    if (dir == ImageStackLayout::HORIZONTAL)
        width = img1Width + gap + img2Width;
    else
        height = img1Height + gap + img2Height;

    wxImage output(width, height);
    output.SetAlpha();
    ::memset(output.GetAlpha(), wxIMAGE_ALPHA_TRANSPARENT, width * height);

    auto calcPos = [&](int imageExtent, int totalExtent)
    {
        switch (align)
        {
            case ImageStackAlignment::CENTER:
                return static_cast<int>(std::floor((totalExtent - imageExtent) / 2.0)); //consistency: round down negative values, too!
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
            writeToImage(output, img1, wxPoint(0,               calcPos(img1Height, height)));
            writeToImage(output, img2, wxPoint(img1Width + gap, calcPos(img2Height, height)));
            break;

        case ImageStackLayout::VERTICAL:
            writeToImage(output, img1, wxPoint(calcPos(img1Width, width), 0));
            writeToImage(output, img2, wxPoint(calcPos(img2Width, width), img1Height + gap));
            break;
    }
    return output;
}


namespace
{
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
    for (const auto& [lineText, lineSize] : lineInfo)
    {
        maxWidth   = std::max(maxWidth,   lineSize.GetWidth());
        lineHeight = std::max(lineHeight, lineSize.GetHeight()); //wxWidgets comment "GetTextExtent will return 0 for empty string"
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
        for (const auto& [lineText, lineSize] : lineInfo)
        {
            if (!lineText.empty())
                switch (textAlign)
                {
                    case ImageStackAlignment::LEFT:
                        dc.DrawText(lineText, wxPoint(0, posY));
                        break;
                    case ImageStackAlignment::RIGHT:
                        dc.DrawText(lineText, wxPoint(maxWidth - lineSize.GetWidth(), posY));
                        break;
                    case ImageStackAlignment::CENTER:
                        dc.DrawText(lineText, wxPoint((maxWidth - lineSize.GetWidth()) / 2, posY));
                        break;
                }

            posY += lineHeight;
        }
    }

    //wxDC::DrawLabel() doesn't respect alpha channel => calculate alpha values manually:
    wxImage output(newBitmap.ConvertToImage());
    output.SetAlpha();

    unsigned char* rgb   = output.GetData();
    unsigned char* alpha = output.GetAlpha();
    const int pixelCount = output.GetWidth() * output.GetHeight();

    for (int i = 0; i < pixelCount; ++i)
    {
        //black(0,0,0) becomes wxIMAGE_ALPHA_OPAQUE(255), while white(255,255,255) becomes wxIMAGE_ALPHA_TRANSPARENT(0)
        *alpha++ = static_cast<unsigned char>((255 - rgb[0] + 255 - rgb[1] + 255 - rgb[2]) / 3); //mixed mode arithmetics!

        rgb[0] = col.Red  (); //
        rgb[1] = col.Green(); //apply actual text color
        rgb[2] = col.Blue (); //

        rgb += 3;
    }
    return output;
}


wxBitmap zen::layOver(const wxBitmap& back, const wxBitmap& front,  int alignment)
{
    if (!front.IsOk()) return back;

    const int width  = std::max(back.GetWidth(),  front.GetWidth());
    const int height = std::max(back.GetHeight(), front.GetHeight());

    assert(front.HasAlpha() == back.HasAlpha()); //we don't support mixed-mode brittleness!
    const int offsetX = [&]
    {
        if (alignment & wxALIGN_RIGHT)
            return back.GetWidth() - front.GetWidth();
        if (alignment & wxALIGN_CENTER_HORIZONTAL)
            return (back.GetWidth() - front.GetWidth()) / 2;

        static_assert(wxALIGN_LEFT == 0);
        return 0;
    }();

    const int offsetY = [&]
    {
        if (alignment & wxALIGN_BOTTOM)
            return back.GetHeight() - front.GetHeight();
        if (alignment & wxALIGN_CENTER_VERTICAL)
            return (back.GetHeight() - front.GetHeight()) / 2;

        static_assert(wxALIGN_TOP == 0);
        return 0;
    }();

    //can't use wxMemoryDC and wxDC::DrawBitmap(): no alpha channel support on wxGTK!
    wxImage output(width, height);
    output.SetAlpha();
    ::memset(output.GetAlpha(), wxIMAGE_ALPHA_TRANSPARENT, width * height);

    const wxPoint posBack(std::max(-offsetX, 0), std::max(-offsetY, 0));
    writeToImage(output, back .ConvertToImage(), posBack);
    writeToImage(output, front.ConvertToImage(), posBack + wxPoint(offsetX, offsetY));
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

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "image_tools.h"
#include <zen/string_tools.h>
#include <zen/scope_guard.h>
#include <wx/app.h>
#include <wx/dcmemory.h>
#include <wx+/dc.h>
#include <xBRZ/src/xbrz_tools.h>

using namespace zen;


namespace
{
template <int PixBytes>
void copyImageBlock(const unsigned char* src, int srcWidth,
                    /**/  unsigned char* trg, int trgWidth, int blockWidth, int blockHeight)
{
    assert(srcWidth >= blockWidth && trgWidth >= blockWidth);
    const int srcPitch = srcWidth * PixBytes;
    const int trgPitch = trgWidth * PixBytes;
    const int blockPitch = blockWidth * PixBytes;
    for (int y = 0; y < blockHeight; ++y)
        std::memcpy(trg + y * trgPitch, src + y * srcPitch, blockPitch);
}


//...what wxImage::Resize() wants to be when it grows up
void copySubImage(const wxImage& src, wxPoint srcPos,
                  /**/  wxImage& trg, wxPoint trgPos, wxSize blockSize)
{
    auto pointClamp = [](const wxPoint& pos, const wxImage& img) -> wxPoint
    {
        return {
            std::clamp(pos.x, 0, img.GetWidth ()),
            std::clamp(pos.y, 0, img.GetHeight())};
    };
    auto subtract = [](const wxPoint& lhs, const wxPoint& rhs) { return wxSize{lhs.x - rhs.x, lhs.y - rhs.y}; };
    //work around yet another wxWidgets screw up: WTF does "operator-(wxPoint, wxPoint)" return wxPoint instead of wxSize!??

    const wxPoint trgPos2    = pointClamp(trgPos,             trg);
    const wxPoint trgPos2End = pointClamp(trgPos + blockSize, trg);

    blockSize = subtract(trgPos2End, trgPos2);
    srcPos += subtract(trgPos2, trgPos);
    trgPos = trgPos2;
    if (blockSize.x <= 0 || blockSize.y <= 0)
        return;

    const wxPoint srcPos2    = pointClamp(srcPos,             src);
    const wxPoint srcPos2End = pointClamp(srcPos + blockSize, src);

    blockSize = subtract(srcPos2End, srcPos2);
    trgPos += subtract(srcPos2, srcPos);
    srcPos = srcPos2;
    if (blockSize.x <= 0 || blockSize.y <= 0)
        return;
    //what if target block size is bigger than source block size? should we clear the area that is not copied from source?

    copyImageBlock<3>(src.GetData() + 3 * (srcPos.x + srcPos.y * src.GetWidth()), src.GetWidth(),
                      trg.GetData() + 3 * (trgPos.x + trgPos.y * trg.GetWidth()), trg.GetWidth(),
                      blockSize.x, blockSize.y);

    copyImageBlock<1>(src.GetAlpha() + srcPos.x + srcPos.y * src.GetWidth(), src.GetWidth(),
                      trg.GetAlpha() + trgPos.x + trgPos.y * trg.GetWidth(), trg.GetWidth(),
                      blockSize.x, blockSize.y);
}


void copyImageLayover(const wxImage& src,
                      /**/  wxImage& trg, wxPoint trgPos)
{
    const int srcWidth  = src.GetWidth ();
    const int srcHeight = src.GetHeight();
    const int trgWidth  = trg.GetWidth();

    assert(0 <= trgPos.x && trgPos.x + srcWidth  <= trgWidth       ); //draw area must be a
    assert(0 <= trgPos.y && trgPos.y + srcHeight <= trg.GetHeight()); //subset of target image!

    //https://en.wikipedia.org/wiki/Alpha_compositing
    //TODO!? gamma correction: https://en.wikipedia.org/wiki/Alpha_compositing#Gamma_correction
    const unsigned char* srcRgb   = src.GetData();
    const unsigned char* srcAlpha = src.GetAlpha();

    for (int y = 0; y < srcHeight; ++y)
    {
        unsigned char* trgRgb   = trg.GetData () + 3 * (trgPos.x + (trgPos.y + y) * trgWidth);
        unsigned char* trgAlpha = trg.GetAlpha() +      trgPos.x + (trgPos.y + y) * trgWidth;

        for (int x = 0; x < srcWidth; ++x)
        {
            const int w1 = *srcAlpha; //alpha-composition interpreted as weighted average
            const int w2 = numeric::intDivRound(*trgAlpha * (255 - w1), 255);
            const int wSum = w1 + w2;

            auto calcColor = [w1, w2, wSum](unsigned char colsrc, unsigned char colTrg)
            {
                return static_cast<unsigned char>(wSum == 0 ? 0 : numeric::intDivRound(colsrc * w1 + colTrg * w2, wSum));
            };
            trgRgb[0] = calcColor(srcRgb[0], trgRgb[0]);
            trgRgb[1] = calcColor(srcRgb[1], trgRgb[1]);
            trgRgb[2] = calcColor(srcRgb[2], trgRgb[2]);

            *trgAlpha = static_cast<unsigned char>(wSum);

            srcRgb += 3;
            trgRgb += 3;
            ++srcAlpha;
            ++trgAlpha;
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

    const wxSize newSize = dir == ImageStackLayout::horizontal ?
                           wxSize(img1Width + gap + img2Width, std::max(img1Height, img2Height)) :
                           wxSize(std::max(img1Width, img2Width), img1Height + gap + img2Height);

    wxImage output(newSize);
    output.SetAlpha();
    std::memset(output.GetAlpha(), wxIMAGE_ALPHA_TRANSPARENT, newSize.x * newSize.y);

    auto calcPos = [&](int imageExtent, int totalExtent)
    {
        switch (align)
        {
            case ImageStackAlignment::center:
                return (totalExtent - imageExtent) / 2;
            case ImageStackAlignment::left: //or top
                return 0;
            case ImageStackAlignment::right: //or bottom
                return totalExtent - imageExtent;
        }
        assert(false);
        return 0;
    };

    switch (dir)
    {
        case ImageStackLayout::horizontal:
            copySubImage(img1, wxPoint(), output, wxPoint(0,               calcPos(img1Height, newSize.y)), img1.GetSize());
            copySubImage(img2, wxPoint(), output, wxPoint(img1Width + gap, calcPos(img2Height, newSize.y)), img2.GetSize());
            break;

        case ImageStackLayout::vertical:
            copySubImage(img1, wxPoint(), output, wxPoint(calcPos(img1Width, newSize.x), 0),                img1.GetSize());
            copySubImage(img2, wxPoint(), output, wxPoint(calcPos(img2Width, newSize.x), img1Height + gap), img2.GetSize());
            break;
    }
    return output;
}


wxImage zen::createImageFromText(const wxString& text, const wxFont& font, const wxColor& col, ImageStackAlignment textAlign)
{
    wxMemoryDC dc; //the context used for bitmaps
    setScaleFactor(dc, getScreenDpiScale());
    dc.SetFont(font); //the font parameter of GetTextExtent() is not evaluated on OS X, wxWidgets 2.9.5, so apply it to the DC directly!

    std::vector<std::pair<wxString, wxSize>> lineInfo; //text + extent
    for (const wxString& line : splitCpy(text, L'\n', SplitOnEmpty::allow))
        lineInfo.emplace_back(line, dc.GetTextExtent(line)); //GetTextExtent() returns (0, 0) for empty string!
    //------------------------------------------------------------------------------------------------

    int maxWidth   = 0;
    int lineHeight = 0;
    for (const auto& [lineText, lineSize] : lineInfo)
    {
        maxWidth   = std::max(maxWidth,   lineSize.GetWidth());
        lineHeight = std::max(lineHeight, lineSize.GetHeight());
    }
    if (maxWidth == 0 || lineHeight == 0)
        return wxNullImage;

    wxBitmap newBitmap(wxsizeToScreen(maxWidth),
                       wxsizeToScreen(static_cast<int>(lineHeight * lineInfo.size()))); //seems we don't need to pass 24-bit depth here even for high-contrast color schemes
    newBitmap.SetScaleFactor(getScreenDpiScale());
    {
        dc.SelectObject(newBitmap); //copies scale factor from wxBitmap
        ZEN_ON_SCOPE_EXIT(dc.SelectObject(wxNullBitmap));

        if (wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft)
            dc.SetLayoutDirection(wxLayout_RightToLeft); //handle e.g. "weak" bidi characters: -> arrows in hebrew/arabic

        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();

        dc.SetTextForeground(*wxBLACK); //for proper alpha-channel calculation
        dc.SetTextBackground(*wxWHITE); //

        int posY = 0;
        for (const auto& [lineText, lineSize] : lineInfo)
        {
            if (!lineText.empty())
                switch (textAlign)
                {
                    case ImageStackAlignment::left:
                        dc.DrawText(lineText, wxPoint(0, posY));
                        break;
                    case ImageStackAlignment::right:
                        dc.DrawText(lineText, wxPoint(maxWidth - lineSize.GetWidth(), posY));
                        break;
                    case ImageStackAlignment::center:
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
        //gamma correction? does not seem to apply here!
        *alpha++ = static_cast<unsigned char>(numeric::intDivRound(3 * 255 - rgb[0] - rgb[1] - rgb[2], 3)); //mixed-mode arithmetics!
        *rgb++ = col.Red  (); //
        *rgb++ = col.Green(); //apply actual text color
        *rgb++ = col.Blue (); //
    }
    return output;
}


wxImage zen::layOver(const wxImage& back, const wxImage& front, int alignment)
{
    if (!front.IsOk()) return back;
    assert(front.HasAlpha() && back.HasAlpha());

    const wxSize newSize(std::max(back.GetWidth(),  front.GetWidth()),
                         std::max(back.GetHeight(), front.GetHeight()));

    auto calcNewPos = [&](const wxImage& img)
    {
        wxPoint newPos;
        if (alignment & wxALIGN_RIGHT) //note: wxALIGN_LEFT == 0!
            newPos.x = newSize.GetWidth() - img.GetWidth();
        else if (alignment & wxALIGN_CENTER_HORIZONTAL)
            newPos.x = (newSize.GetWidth() - img.GetWidth()) / 2;

        if (alignment & wxALIGN_BOTTOM) //note: wxALIGN_TOP == 0!
            newPos.y = newSize.GetHeight() - img.GetHeight();
        else if (alignment & wxALIGN_CENTER_VERTICAL)
            newPos.y = (newSize.GetHeight() - img.GetHeight()) / 2;

        return newPos;
    };

    wxImage output(newSize);
    output.SetAlpha();
    std::memset(output.GetAlpha(), wxIMAGE_ALPHA_TRANSPARENT, newSize.x * newSize.y);

    copySubImage(back, wxPoint(), output, calcNewPos(back), back.GetSize());
    //use resizeCanvas()? might return ref-counted copy!

    //can't use wxMemoryDC and wxDC::DrawBitmap(): no alpha channel support on wxGTK!
    copyImageLayover(front, output, calcNewPos(front));

    return output;
}


wxImage zen::resizeCanvas(const wxImage& img, wxSize newSize, int alignment)
{
    if (newSize == img.GetSize())
        return img; //caveat: wxImage is ref-counted *without* copy on write

    wxPoint newPos;
    if (alignment & wxALIGN_RIGHT) //note: wxALIGN_LEFT == 0!
        newPos.x = newSize.GetWidth() - img.GetWidth();
    else if (alignment & wxALIGN_CENTER_HORIZONTAL)
        newPos.x = numeric::intDivFloor(newSize.GetWidth() - img.GetWidth(), 2); //consistency: round down negative values, too!

    if (alignment & wxALIGN_BOTTOM) //note: wxALIGN_TOP == 0!
        newPos.y = newSize.GetHeight() - img.GetHeight();
    else if (alignment & wxALIGN_CENTER_VERTICAL)
        newPos.y = numeric::intDivFloor(newSize.GetHeight() - img.GetHeight(), 2); //consistency: round down negative values, too!

    wxImage output(newSize);
    output.SetAlpha();
    std::memset(output.GetAlpha(), wxIMAGE_ALPHA_TRANSPARENT, newSize.x * newSize.y);

    copySubImage(img, wxPoint(), output, newPos, img.GetSize());
    //about 50x faster than e.g. wxImage::Resize!!! suprise :>
    return output;
}


wxImage zen::bilinearScale(const wxImage& img, int width, int height)
{
    assert(img.HasAlpha());
    const auto imgReader = [rgb = img.GetData(), alpha = img.GetAlpha(), srcWidth = img.GetSize().x](int x, int y, xbrz::BytePixel& pix)
    {
        const int idx = y * srcWidth + x;
        const unsigned char* const ptr = rgb + idx * 3;

        //TODO!? gamma correction: https://en.wikipedia.org/wiki/Alpha_compositing#Gamma_correction
        const unsigned char a = alpha[idx];
        pix[0] = a;
        pix[1] = xbrz::premultiply(ptr[0], a); //r
        pix[2] = xbrz::premultiply(ptr[1], a); //g
        pix[3] = xbrz::premultiply(ptr[2], a); //b
    };

    wxImage imgOut(width, height);
    imgOut.SetAlpha();

    const auto imgWriter = [rgb = imgOut.GetData(), alpha = imgOut.GetAlpha()](const xbrz::BytePixel& pix) mutable
    {
        const unsigned char a = pix[0];
        * alpha++ = a;
        * rgb++ = xbrz::demultiply(pix[1], a); //r
        *rgb++ = xbrz::demultiply(pix[2], a); //g
        *rgb++ = xbrz::demultiply(pix[3], a); //b
    };

    xbrz::bilinearScaleSimple(imgReader,       //PixReader srcReader
                              img.GetSize().x, //int srcWidth
                              img.GetSize().y, //int srcHeight
                              imgWriter,       //PixWriter trgWriter
                              width,           //int trgWidth
                              height,          //int trgHeight
                              0,               //int yFirst
                              height);         //int yLast
    return imgOut;
    //return img.Scale(width, height, wxIMAGE_QUALITY_BILINEAR);
}


wxImage zen::shrinkImage(const wxImage& img, int maxWidth /*optional*/, int maxHeight /*optional*/)
{
    wxSize newSize = img.GetSize();

    if (0 <= maxWidth && maxWidth < newSize.x)
    {
        newSize.x = maxWidth;
        newSize.y = numeric::intDivRound(maxWidth * img.GetHeight(), img.GetWidth());
    }
    if (0 <= maxHeight && maxHeight < newSize.y)
    {
        newSize.x = numeric::intDivRound(maxHeight * img.GetWidth(), img.GetHeight()); //avoid loss of precision
        newSize.y = maxHeight;
    }

    if (newSize == img.GetSize())
        return img;

    return bilinearScale(img, newSize.x, newSize.y); //looks sharper than wxIMAGE_QUALITY_HIGH!
}


void zen::convertToVanillaImage(wxImage& img)
{
    if (!img.HasAlpha())
    {
        const int width  = img.GetWidth ();
        const int height = img.GetHeight();
        if (width <= 0 || height <= 0) return;

        unsigned char maskR = 0;
        unsigned char maskG = 0;
        unsigned char maskB = 0;
        const bool haveMask = img.HasMask() && img.GetOrFindMaskColour(&maskR, &maskG, &maskB);
        //check for mask before calling wxImage::GetOrFindMaskColour() to skip needlessly searching for new mask color

        img.SetAlpha();
        ::memset(img.GetAlpha(), wxIMAGE_ALPHA_OPAQUE, width * height);

        //wxWidgets, as always, tries to be more clever than it really is and fucks up wxStaticBitmap if wxBitmap is fully opaque:
        img.GetAlpha()[width * height - 1] = 254;

        if (haveMask)
        {
            img.SetMask(false);
            unsigned char*       alpha = img.GetAlpha();
            const unsigned char* rgb   = img.GetData();

            const int pixelCount = width * height;
            for (int i = 0; i < pixelCount; ++i)
            {
                const unsigned char r = *rgb++;
                const unsigned char g = *rgb++;
                const unsigned char b = *rgb++;

                if (r == maskR &&
                    g == maskG &&
                    b == maskB)
                    alpha[i] = wxIMAGE_ALPHA_TRANSPARENT;
            }
        }
    }
    else
    {
        assert(!img.HasMask());
    }
}

wxImage zen::rectangleImage(wxSize size, const wxColor& col)
{
    assert(col.IsSolid());
    wxImage img(size);

    unsigned char* rgb = img.GetData();
    const int pixelCount = size.GetWidth() * size.GetHeight();
    for (int i = 0; i < pixelCount; ++i)
    {
        *rgb++ = col.GetRed();
        *rgb++ = col.GetGreen();
        *rgb++ = col.GetBlue();
    }
    convertToVanillaImage(img);
    return img;
}


wxImage zen::rectangleImage(wxSize size, const wxColor& innerCol, const wxColor& borderCol, int borderWidth)
{
    assert(innerCol.IsSolid() && borderCol.IsSolid());
    wxImage img = rectangleImage(size, borderCol);

    const int heightInner = size.GetHeight() - 2 * borderWidth;
    const int widthInner  = size.GetWidth () - 2 * borderWidth;

    if (widthInner > 0 && heightInner > 0 && innerCol != borderCol)
        //copyImageLayover(rectangleImage({widthInner, heightInner}, innerCol), img, {borderWidth, borderWidth}); => inline:
        for (int y = 0; y < heightInner; ++y)
        {
            unsigned char* rgb = img.GetData () + 3 * (borderWidth + (borderWidth + y) * size.GetWidth());

            for (int x = 0; x < widthInner; ++x)
            {
                *rgb++ = innerCol.GetRed();
                *rgb++ = innerCol.GetGreen();
                *rgb++ = innerCol.GetBlue();
            }
        }

    return img;
}

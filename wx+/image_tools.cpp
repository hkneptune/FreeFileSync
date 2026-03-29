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
#include <wx+/color_tools.h>
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

    const unsigned char* srcRgb   = src.GetData();
    const unsigned char* srcAlpha = src.GetAlpha();

    for (int y = 0; y < srcHeight; ++y)
    {
        unsigned char* trgRgb   = trg.GetData () + 3 * (trgPos.x + (trgPos.y + y) * trgWidth);
        unsigned char* trgAlpha = trg.GetAlpha() +      trgPos.x + (trgPos.y + y) * trgWidth;

        for (int x = 0; x < srcWidth; ++x)
        {
            const unsigned char w1 = *srcAlpha; //alpha-composition interpreted as weighted average
            const unsigned char w2 = numeric::intDivRound(*trgAlpha * (255 - w1), 255);
            const unsigned char wSum = w1 + w2;

            auto calcColor = [w1, w2, wSum](unsigned char colsrc, unsigned char colTrg)
            {
                if (w1 == 0) return colTrg;
                if (w2 == 0) return colsrc;

                //https://en.wikipedia.org/wiki/Alpha_compositing
                //Limitation: alpha should be applied in gamma-decoded linear RGB space: https://ssp.impulsetrain.com/gamma-premult.html
                // => srgbEncode((srgbDecode(colsrc) * w1 + srgbDecode(colTrg) * w2) / wSum)
                return static_cast<unsigned char>(numeric::intDivRound(colsrc * w1 + colTrg * w2, int(wSum)));
            };
            trgRgb[0] = calcColor(srcRgb[0], trgRgb[0]);
            trgRgb[1] = calcColor(srcRgb[1], trgRgb[1]);
            trgRgb[2] = calcColor(srcRgb[2], trgRgb[2]);

            *trgAlpha = wSum;

            srcRgb += 3;
            trgRgb += 3;
            ++srcAlpha;
            ++trgAlpha;
        }
    }
}


wxRect getVisibleBox(const unsigned char* alpha, int width, int height)
{
    assert(width >= 0 && height >= 0);
    if (width <= 0 || height <= 0)
        return {};

    int i = width * height - 1;
    while (alpha[i] == wxIMAGE_ALPHA_TRANSPARENT)
        if (--i < 0)
			return {};
    int yMax = i / width;
    int xMax = i % width;

    int j = 0;
    while (alpha[j] == wxIMAGE_ALPHA_TRANSPARENT)
        ++j;
    int yMin = j / width;
    int xMin = j % width;

    const unsigned char* alphaRow = alpha + (yMin + 1) * width;

    for (int y = yMin + 1; y <= yMax; ++y, alphaRow += width)
        for (int x = 0; x < xMin; ++x)
            if (alphaRow[x] != wxIMAGE_ALPHA_TRANSPARENT)
            {
                xMin = x;
                break;
            }

    alphaRow -= 2 * width;

    for (int y = yMax - 1; y >= yMin; --y, alphaRow -= width)
        for (int x = width - 1; x > xMax; --x)
            if (alphaRow[x] != wxIMAGE_ALPHA_TRANSPARENT)
            {
                xMax = x;
                break;
            }

    return {xMin, yMin, xMax - xMin + 1, yMax - yMin + 1};
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


wxImage zen::createImageFromText(const wxString& text, const wxFont& font, const wxColor& col)
{
    assert(!contains(text, L'\n'));

    wxMemoryDC dc; //the context used for bitmaps
    setScaleFactor(dc, getScreenDpiScale());
    dc.SetFont(font); //the font parameter of GetTextExtent() is not used on macOS, wxWidgets 2.9.5, so apply it to the DC directly!

    wxSize size = dc.GetTextExtent(text); //GetTextExtent() returns (0, 0) for empty string!
    if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
        return wxNullImage;

    //GetTextExtent() is slightly incorrect on GTK3 with 200% scaling: apparently returns "extent for 100% scaling" x 2 => GTK3 bug?
    size.x += std::max(size.x / 4, 10); //enlarge by 25%, then trim later via getVisibleBox()

    //------------------------------------------------------------------------------------------------
    const bool darkMode = relativeContrast(col, *wxBLACK) > //wxSystemSettings::GetAppearance().IsDark() ?
                          relativeContrast(col, *wxWHITE);  //=> no, make it text color-dependent
    //small but noticeable difference; due to "ClearType"?

    wxBitmap newBitmap(wxsizeToScreen(size.GetWidth()),
                       wxsizeToScreen(size.GetHeight())); //seems we don't need to pass 24-bit depth here even for high-contrast color schemes
    newBitmap.SetScaleFactor(getScreenDpiScale());
    {
        dc.SelectObject(newBitmap); //copies scale factor from wxBitmap
        ZEN_ON_SCOPE_EXIT(dc.SelectObject(wxNullBitmap));

        if (wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft)
            dc.SetLayoutDirection(wxLayout_RightToLeft); //handle e.g. "weak" bidi characters: -> arrows in hebrew/arabic

        dc.SetBackground(darkMode ? *wxBLACK_BRUSH : *wxWHITE_BRUSH);
        dc.Clear();

        dc.SetTextBackground(darkMode ? *wxBLACK : *wxWHITE); //for proper alpha-channel calculation
        dc.SetTextForeground(darkMode ? *wxWHITE : *wxBLACK); //

        dc.DrawText(text, 0, 0);
    }

    wxImage img(newBitmap.ConvertToImage());
    //calculate alpha values manually:
    //Limitation: alpha should be applied in gamma-decoded linear RGB space: https://ssp.impulsetrain.com/gamma-premult.html
    //=> however wxDC::DrawText most likely applied alpha in gamma-encoded sRGB => following simple calculations should be fine:
    img.SetAlpha();
    {
        unsigned char* alpha = img.GetAlpha();
        unsigned char* rgb   = img.GetData();
        unsigned char* rgbEnd = rgb + 3 * img.GetWidth() * img.GetHeight();
        
        if (darkMode) //black(0,0,0) becomes wxIMAGE_ALPHA_TRANSPARENT(0), white(255,255,255) becomes wxIMAGE_ALPHA_OPAQUE(255)
            for (; rgb < rgbEnd; rgb += 3)
                *alpha++ = static_cast<unsigned char>(numeric::intDivRound(rgb[0] + rgb[1] + rgb[2], 3)); //mixed-mode arithmetics!
        else //black(0,0,0) becomes wxIMAGE_ALPHA_OPAQUE(255), white(255,255,255) becomes wxIMAGE_ALPHA_TRANSPARENT(0)
            for (; rgb < rgbEnd; rgb += 3)
                *alpha++ = static_cast<unsigned char>(numeric::intDivRound(3 * 255 - rgb[0] - rgb[1] - rgb[2], 3)); //mixed-mode arithmetics!
    }

    const wxRect box = getVisibleBox(img.GetAlpha(), img.GetWidth(), img.GetHeight());
    assert(box.x + box.width < img.GetWidth()); //we enlarged wxDC::GetTextExtent(), so last colum should be empty!

    if (box.width <= 0 || box.height <= 0)
    {
        assert(false);
        return wxNullImage;
    }

    wxImage output(box.width, img.GetHeight()); //let's keep wxDC::GetTextExtent height until further
    output.SetAlpha();

    copyImageBlock<1>(img.GetAlpha() + box.x, img.GetWidth(),
                      output.GetAlpha(), output.GetWidth(),
                      output.GetWidth(), output.GetHeight());

    const unsigned char r = col.Red  (); //
    const unsigned char g = col.Green(); //getting RGB involves virtual function calls!
    const unsigned char b = col.Blue (); //

    unsigned char* rgb = output.GetData();
    unsigned char* rgbEnd = rgb + 3 * output.GetWidth() * output.GetHeight();
    while (rgb < rgbEnd)
    {
        *rgb++ = r; //
        *rgb++ = g; //apply actual text color
        *rgb++ = b; //
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
    //about 50x faster than wxImage::Resize!!! surprise :>
    return output;
}


wxImage zen::bilinearScale(const wxImage& img, int width, int height)
{
    assert(img.HasAlpha());

    const auto pixRead = [rgb = img.GetData(), alpha = img.GetAlpha(), srcWidth = img.GetSize().x](int x, int y)
    {
        const int idx = y * srcWidth + x;

        return [a = int(alpha[idx]), pix = rgb + idx * 3](int channel)
        {
            if (channel == 3)
                return a;
            //Limitation: alpha should be applied in gamma-decoded linear RGB space: https://ssp.impulsetrain.com/gamma-premult.html
            return pix[channel] * a;
        };
    };

    wxImage imgOut(width, height);
    imgOut.SetAlpha();

    const auto pixWrite = [rgb = imgOut.GetData(), alpha = imgOut.GetAlpha()](const auto& interpolate) mutable
    {
        const double a = interpolate(3);
        if (a <= 0.0)
        {
            *alpha++ = 0;
            rgb += 3; //don't care about color
        }
        else
        {
            *alpha++ = xbrz::byteRound(a);
            *rgb++   = xbrz::byteRound(interpolate(0) / a); //r
            *rgb++   = xbrz::byteRound(interpolate(1) / a); //g
            *rgb++   = xbrz::byteRound(interpolate(2) / a); //b
        }
    };

    xbrz::bilinearScale(pixRead,         //PixReader pixRead
                        img.GetSize().x, //int srcWidth
                        img.GetSize().y, //int srcHeight
                        pixWrite,        //PixWriter pixWrite
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
    const int width  = img.GetWidth ();
    const int height = img.GetHeight();
    if (width <= 0 || height <= 0) return;

    if (!img.HasAlpha())
    {
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
            const unsigned char* rgb = img.GetData();

            for (unsigned char& a : std::span(img.GetAlpha(), width * height))
            {
                const unsigned char r = *rgb++;
                const unsigned char g = *rgb++;
                const unsigned char b = *rgb++;

                if (r == maskR &&
                    g == maskG &&
                    b == maskB)
                    a = wxIMAGE_ALPHA_TRANSPARENT;
            }
        }
    }
    else
        assert(!img.HasMask());
}


wxImage zen::rectangleImage(wxSize size, const wxColor& col)
{
    assert(col.IsSolid());
    wxImage img(size);

    const unsigned char r = col.Red  (); //
    const unsigned char g = col.Green(); //getting RGB involves virtual function calls!
    const unsigned char b = col.Blue (); //

    unsigned char* rgb = img.GetData();
    unsigned char* rgbEnd = rgb + 3 * size.GetWidth() * size.GetHeight();

    while (rgb < rgbEnd)
    {
        *rgb++ = r;
        *rgb++ = g;
        *rgb++ = b;
    }
    convertToVanillaImage(img);
    return img;
}


wxImage zen::rectangleImage(wxSize size, const wxColor& innerCol, const wxColor& borderCol, int borderWidth)
{
    assert(innerCol.IsSolid() && borderCol.IsSolid());
    assert(borderWidth > 0);
    wxImage img = rectangleImage(size, borderCol);

    const int heightInner = size.GetHeight() - 2 * borderWidth;
    const int widthInner  = size.GetWidth () - 2 * borderWidth;

    const unsigned char r = innerCol.Red  (); //
    const unsigned char g = innerCol.Green(); //getting RGB involves virtual function calls!
    const unsigned char b = innerCol.Blue (); //

    if (widthInner > 0 && heightInner > 0 && innerCol != borderCol)
        //copyImageLayover(rectangleImage({widthInner, heightInner}, innerCol), img, {borderWidth, borderWidth}); => inline:
        for (int y = 0; y < heightInner; ++y)
        {
            unsigned char* rgb = img.GetData () + 3 * (borderWidth + (borderWidth + y) * size.GetWidth());
            unsigned char* rgbEnd = rgb + 3 * widthInner;

            while (rgb < rgbEnd)
            {
                *rgb++ = r;
                *rgb++ = g;
                *rgb++ = b;
            }
        }

    return img;
}

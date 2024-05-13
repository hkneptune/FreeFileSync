// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "image_resources.h"
//#include <map>
#include <zen/utf.h>
#include <zen/thread.h>
#include <zen/file_io.h>
#include <zen/file_traverser.h>
#include <wx/zipstrm.h>
#include <wx/mstream.h>
#include <xBRZ/src/xbrz.h>
#include <xBRZ/src/xbrz_tools.h>
#include "image_tools.h"
#include "image_holder.h"
#include "dc.h"

using namespace zen;


namespace
{
ImageHolder xbrzScale(int width, int height, const unsigned char* imageRgb, const unsigned char* imageAlpha, int hqScale)
{
    assert(imageRgb && imageAlpha && width > 0 && height > 0); //see convertToVanillaImage()
    if (width <= 0 || height <= 0)
        return ImageHolder(0, 0, true /*withAlpha*/);

    const int hqWidth  = width  * hqScale;
    const int hqHeight = height * hqScale;

    //get rid of allocation and buffer std::vector<> at thread-level? => no discernable perf improvement
    std::vector<uint32_t> buf(hqWidth * hqHeight + width * height);
    uint32_t* const argbSrc = buf.data() + hqWidth * hqHeight;
    uint32_t* const xbrTrg  = buf.data();

    //convert RGB (RGB byte order) to ARGB (BGRA byte order)
    {
        const unsigned char* rgb = imageRgb;
        const unsigned char* rgbEnd = rgb + 3 * width * height;
        const unsigned char* alpha  = imageAlpha;
        uint32_t* out = argbSrc;

        for (; rgb < rgbEnd; rgb += 3)
            *out++ = xbrz::makePixel(*alpha++, rgb[0], rgb[1], rgb[2]);
    }
    //-----------------------------------------------------
    xbrz::scale(hqScale,       //size_t factor - valid range: 2 - SCALE_FACTOR_MAX
                argbSrc,       //const uint32_t* src
                xbrTrg,        //uint32_t* trg
                width, height, //int srcWidth, int srcHeight
                xbrz::ColorFormat::argbUnbuffered); //ColorFormat colFmt
    //test: total xBRZ scaling time with ARGB: 300ms, ARGB unbuffered: 50ms
    //-----------------------------------------------------
    //convert BGRA to RGB + alpha
    ImageHolder trgImg(hqWidth, hqHeight, true /*withAlpha*/);

    std::for_each(xbrTrg, xbrTrg + hqWidth * hqHeight, [rgb = trgImg.getRgb(), alpha = trgImg.getAlpha()](uint32_t col) mutable
    {
        *alpha++ = xbrz::getAlpha(col);
        *rgb++   = xbrz::getRed  (col);
        *rgb++   = xbrz::getGreen(col);
        *rgb++   = xbrz::getBlue (col);
    });
    return trgImg;
}


auto createScalerTask(const std::string& imageName, const wxImage& img, int hqScale, Protected<std::vector<std::pair<std::string, ImageHolder>>>& protResult)
{
    assert(runningOnMainThread());
    return [imageName,
            width  = img.GetWidth(),  //
            height = img.GetHeight(), //don't call wxWidgets functions from worker thread
            rgb    = img.GetData(),   //
            alpha  = img.GetAlpha(),  //
            hqScale, &protResult]
    {
        ImageHolder ih = xbrzScale(width, height, rgb, alpha, hqScale);
        protResult.access([&](std::vector<std::pair<std::string, ImageHolder>>& result) { result.emplace_back(imageName, std::move(ih)); });
    };
}


class HqParallelScaler
{
public:
    explicit HqParallelScaler(int hqScale) : hqScale_(hqScale) { assert(hqScale > 1); }

    ~HqParallelScaler() { threadGroup_ = {}; } //imgKeeper_ must out-live threadGroup!!!

    void add(const std::string& imageName, const wxImage& img)
    {
        assert(runningOnMainThread());
        imgKeeper_.push_back(img); //retain (ref-counted) wxImage so that the rgb/alpha pointers remain valid after passed to threads
        threadGroup_->run(createScalerTask(imageName, img, hqScale_, protResult_));
    }

    std::unordered_map<std::string, wxImage> waitAndGetResult()
    {
        assert(runningOnMainThread());
        threadGroup_->wait();

        std::unordered_map<std::string, wxImage> output;

        protResult_.access([&](std::vector<std::pair<std::string, ImageHolder>>& result)
        {
            for (auto& [imageName, ih] : result)
            {
                wxImage img(ih.getWidth(), ih.getHeight(), ih.releaseRgb(), false /*static_data*/); //pass ownership
                img.SetAlpha(ih.releaseAlpha(), false /*static_data*/);

                output.emplace(imageName, std::move(img));
            }
        });
        return output;
    }

private:
    const int hqScale_;
    std::vector<wxImage> imgKeeper_;
    Protected<std::vector<std::pair<std::string, ImageHolder>>> protResult_;

    using TaskType = FunctionReturnTypeT<decltype(&createScalerTask)>;
    std::optional<ThreadGroup<TaskType>> threadGroup_{ThreadGroup<TaskType>(std::max<int>(std::thread::hardware_concurrency(), 1), Zstr("xBRZ Scaler"))};
    //hardware_concurrency() == 0 if "not computable or well defined"
};

//================================================================================================
//================================================================================================

class ImageBuffer
{
public:
    explicit ImageBuffer(const Zstring& filePath); //throw FileError

    const wxImage& getImage(const std::string& name, int maxWidth /*optional*/, int maxHeight /*optional*/);

private:
    ImageBuffer           (const ImageBuffer&) = delete;
    ImageBuffer& operator=(const ImageBuffer&) = delete;

    const wxImage& getRawImage   (const std::string& name);
    const wxImage& getHqScaledImage(const std::string& name);

    std::unordered_map<std::string, wxImage> imagesRaw_;
    std::unordered_map<std::string, wxImage> imagesScaled_;

    std::unique_ptr<HqParallelScaler> hqScaler_;

    using OutImageKey = std::tuple<std::string /*name*/, int /*height*/>;

    struct OutImageKeyHash
    {
        size_t operator()(const OutImageKey& imKey) const
        {
            const auto& [name, height] = imKey;

            FNV1aHash<size_t> hash;
            for (const char c : name)
                hash.add(c);

            hash.add(height);

            return hash.get();
        }
    };
    std::unordered_map<OutImageKey, wxImage, OutImageKeyHash> imagesOut_;
};


ImageBuffer::ImageBuffer(const Zstring& zipPath) //throw FileError
{
    std::vector<std::pair<Zstring /*file name*/, std::string /*byte stream*/>> streams;

    try //to load from ZIP first:
    {
        //wxFFileInputStream/wxZipInputStream loads in junks of 512 bytes => WTF!!! => implement sane file loading:
        const std::string rawStream = getFileContent(zipPath, nullptr /*notifyUnbufferedIO*/); //throw FileError
        wxMemoryInputStream memStream(rawStream.c_str(), rawStream.size()); //does not take ownership
        wxZipInputStream zipStream(memStream, wxConvUTF8);
        //do NOT rely on wxConvLocal! On failure shows unhelpful popup "Cannot convert from the charset 'Unknown encoding (-1)'!"

        while (const auto& entry = std::unique_ptr<wxZipEntry>(zipStream.GetNextEntry())) //take ownership!
            if (std::string stream(entry->GetSize(), '\0');
                zipStream.ReadAll(stream.data(), stream.size()))
                streams.emplace_back(utfTo<Zstring>(entry->GetName()), std::move(stream));
            else
                assert(false);
    }
    catch (FileError&) //fall back to folder: dev build (only!?)
    {
        const Zstring fallbackFolder = beforeLast(zipPath, Zstr(".zip"), IfNotFoundReturn::none);
        if (!itemExists(fallbackFolder)) //throw FileError
            throw;

        traverseFolder(fallbackFolder, [&](const FileInfo& fi)
        {
            if (endsWith(fi.fullPath, Zstr(".png")))
            {
                std::string stream = getFileContent(fi.fullPath, nullptr /*notifyUnbufferedIO*/); //throw FileError
                streams.emplace_back(fi.itemName, std::move(stream));
            }
        }, nullptr, nullptr); //throw FileError
    }
    //--------------------------------------------------------------------

    wxImage::AddHandler(new wxPNGHandler/*ownership passed*/); //activate support for .png files

    //do we need xBRZ scaling for high quality DPI images?
    const int hqScale = std::clamp(static_cast<int>(std::ceil(getScreenDpiScale())), 1, xbrz::SCALE_FACTOR_MAX);
    //even for 125% DPI scaling, "2xBRZ + bilinear downscale" gives a better result than mere "125% bilinear upscale"!
    if (hqScale > 1)
        hqScaler_ = std::make_unique<HqParallelScaler>(hqScale);

    for (const auto& [fileName, stream] : streams)
        if (endsWith(fileName, Zstr(".png")))
        {
            wxMemoryInputStream wxstream(stream.c_str(), stream.size()); //stream does not take ownership of data

            wxImage img(wxstream, wxBITMAP_TYPE_PNG);
            assert(img.IsOk());

            //end this alpha/no-alpha/mask/wxDC::DrawBitmap/RTL/high-contrast-scheme interoperability nightmare here and now!!!!
            //=> there's only one type of wxImage: with alpha channel, no mask!!!
            convertToVanillaImage(img);

            const std::string imageName = utfTo<std::string>(beforeLast(fileName, Zstr("."), IfNotFoundReturn::none));

            imagesRaw_.emplace(imageName, img);
            if (hqScaler_)
                hqScaler_->add(imageName, img); //scale in parallel!
            else
                imagesScaled_.emplace(imageName, img);

            //wxBitmap::NewFromPNGData(stream.c_str(), stream.size())?
            //  => Windows: just a (slow!) wrapper for wxBitmap(wxImage())!
        }
        else
            assert(false);
}


const wxImage& ImageBuffer::getRawImage(const std::string& name)
{
    if (auto it = imagesRaw_.find(name);
        it != imagesRaw_.end())
        return it->second;

    assert(false);
    return wxNullImage;
}


const wxImage& ImageBuffer::getHqScaledImage(const std::string& name)
{
    //test: this function is first called about 220ms after ImageBuffer::ImageBuffer() has ended
    //      => should be enough time to finish xBRZ scaling in parallel (which takes 50ms)
    //debug perf: extra 800-1000ms during startup
    if (hqScaler_)
    {
        imagesScaled_ = hqScaler_->waitAndGetResult();
        hqScaler_.reset();
    }

    if (auto it = imagesScaled_.find(name);
        it != imagesScaled_.end())
        return it->second;

    assert(false);
    return wxNullImage;
}


const wxImage& ImageBuffer::getImage(const std::string& name, int maxWidth /*optional*/, int maxHeight /*optional*/)
{
    const wxImage& rawImg = getRawImage(name);

    const wxSize dpiSize(dipToScreen(rawImg.GetWidth ()),
                         dipToScreen(rawImg.GetHeight()));

    int outHeight = dpiSize.y;
    if (maxWidth >= 0 && maxWidth < dpiSize.x)
        outHeight = numeric::intDivRound(maxWidth * rawImg.GetHeight(), rawImg.GetWidth());

    if (maxHeight >= 0 && maxHeight < outHeight)
        outHeight = maxHeight;

    const OutImageKey imgKey{name, outHeight};

    auto it = imagesOut_.find(imgKey);
    if (it == imagesOut_.end())
    {
        if (rawImg.GetHeight() >= outHeight) //=> skip needless xBRZ upscaling
            it = imagesOut_.emplace(imgKey, shrinkImage(rawImg, -1 /*maxWidth*/, outHeight)).first;
        else if (rawImg.GetHeight() >= 0.9 * outHeight) //almost there: also no need for xBRZ-scale
            it = imagesOut_.emplace(imgKey, bilinearScale(rawImg, numeric::intDivRound(outHeight * rawImg.GetWidth(), rawImg.GetHeight()), outHeight)).first;
        else //however: for 125% DPI scaling, "2xBRZ + bilinear downscale" gives a better result than mere "125% bilinear upscale"
            it = imagesOut_.emplace(imgKey, shrinkImage(getHqScaledImage(name), -1 /*maxWidth*/, outHeight)).first;
    }
    return it->second;
}


std::unique_ptr<ImageBuffer> globalImageBuffer;
}


void zen::imageResourcesInit(const Zstring& zipPath) //throw FileError
{
    assert(runningOnMainThread()); //wxWidgets is not thread-safe!
    assert(!globalImageBuffer);
    globalImageBuffer = std::make_unique<ImageBuffer>(zipPath); //throw FileError
}


void zen::imageResourcesCleanup()
{
    assert(runningOnMainThread()); //wxWidgets is not thread-safe!
    assert(globalImageBuffer);
    globalImageBuffer.reset();
}


const wxImage& zen::loadImage(const std::string& name, int maxWidth /*optional*/, int maxHeight /*optional*/)
{
    assert(runningOnMainThread()); //wxWidgets is not thread-safe!
    assert(globalImageBuffer);
    if (globalImageBuffer)
        return globalImageBuffer->getImage(name, maxWidth, maxHeight);
    return wxNullImage;
}


const wxImage& zen::loadImage(const std::string& name, int maxSize)
{
    return loadImage(name, maxSize, maxSize);
}

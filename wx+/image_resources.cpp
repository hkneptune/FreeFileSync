// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "image_resources.h"
#include <memory>
#include <map>
#include <zen/utf.h>
#include <zen/globals.h>
#include <zen/perf.h>
#include <zen/thread.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <xBRZ/src/xbrz.h>
#include <xBRZ/src/xbrz_tools.h>
#include "image_tools.h"
#include "image_holder.h"
#include "dc.h"

using namespace zen;


namespace
{


ImageHolder dpiScale(int width, int height, int dpiWidth, int dpiHeight, const unsigned char* imageRgb, const unsigned char* imageAlpha, int hqScale)
{
    assert(imageRgb && imageAlpha); //see convertToVanillaImage()
    if (width <= 0 || height <= 0 || dpiWidth <= 0 || dpiHeight <= 0)
        return ImageHolder(0, 0, true /*withAlpha*/);

    const int hqWidth  = width  * hqScale;
    const int hqHeight = height * hqScale;

    //get rid of allocation and buffer std::vector<> at thread-level? => no discernable perf improvement
    std::vector<uint32_t> buf(hqWidth * hqHeight + std::max(width * height, dpiWidth * dpiHeight));
    uint32_t* const argbSrc = &buf[0] + hqWidth * hqHeight;
    uint32_t* const xbrTrg  = &buf[0];
    uint32_t* const dpiTrg  = argbSrc;

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
    xbrz::scale(hqScale,       //size_t factor, //valid range: 2 - SCALE_FACTOR_MAX
                argbSrc,       //const uint32_t* src,
                xbrTrg,        //uint32_t* trg,
                width, height, //int srcWidth, int srcHeight,
                xbrz::ColorFormat::ARGB_UNBUFFERED); //ColorFormat colFmt,
    //test: total xBRZ scaling time with ARGB: 300ms, ARGB_UNBUFFERED: 50ms
    //-----------------------------------------------------
    xbrz::bilinearScale(xbrTrg,               //const uint32_t* src,
                        hqWidth, hqHeight,    //int srcWidth, int srcHeight,
                        dpiTrg,               //uint32_t* trg,
                        dpiWidth, dpiHeight); //int trgWidth, int trgHeight
    //-----------------------------------------------------
    //convert BGRA to RGB + alpha
    ImageHolder trgImg(dpiWidth, dpiHeight, true /*withAlpha*/);
    {
        unsigned char* rgb   = trgImg.getRgb();
        unsigned char* alpha = trgImg.getAlpha();

        std::for_each(dpiTrg, dpiTrg + dpiWidth * dpiHeight, [&](uint32_t col)
        {
            *alpha++ = xbrz::getAlpha(col);
            *rgb++   = xbrz::getRed  (col);
            *rgb++   = xbrz::getGreen(col);
            *rgb++   = xbrz::getBlue (col);
        });
    }
    return trgImg;
}


struct WorkItem
{
    Zbase<wchar_t> name;
    int width     = 0;
    int height    = 0;
    int dpiWidth  = 0;
    int dpiHeight = 0;
    const unsigned char* rgb   = nullptr;
    const unsigned char* alpha = nullptr;
};


class WorkLoad
{
public:
    void add(const WorkItem& wi) //context of main thread
    {
        assert(std::this_thread::get_id() == mainThreadId);
        {
            std::lock_guard<std::mutex> dummy(lockWork_);
            workLoad_.push_back(wi);
        }
        conditionNewWork_.notify_all();
    }

    void noMoreWork()
    {
        assert(std::this_thread::get_id() == mainThreadId);
        {
            std::lock_guard<std::mutex> dummy(lockWork_);
            expectMoreWork_ = false;
        }
        conditionNewWork_.notify_all();
    }

    //context of worker thread, blocking:
    Opt<WorkItem> extractNext() //throw ThreadInterruption
    {
        assert(std::this_thread::get_id() != mainThreadId);
        std::unique_lock<std::mutex> dummy(lockWork_);

        interruptibleWait(conditionNewWork_, dummy, [this] { return !workLoad_.empty() || !expectMoreWork_; }); //throw ThreadInterruption

        if (workLoad_.empty())
            return NoValue();

        WorkItem wi = workLoad_.back(); //
        workLoad_.pop_back();           //yes, no strong exception guarantee (std::bad_alloc)
        return wi;                      //
    }

private:
    bool expectMoreWork_ = true;
    std::vector<WorkItem>   workLoad_;
    std::mutex              lockWork_;
    std::condition_variable conditionNewWork_; //signal event: data for processing available
};


class DpiParallelScaler
{
public:
    DpiParallelScaler(int hqScale)
    {
        assert(hqScale > 1);
        const int threadCount = std::max<int>(std::thread::hardware_concurrency(), 1); //hardware_concurrency() == 0 if "not computable or well defined"

        for (int i = 0; i < threadCount; ++i)
            worker_.push_back([hqScale, &workload = workload_, &result = result_]
        {
            setCurrentThreadName("xBRZ Scaler");
            while (Opt<WorkItem> wi = workload.extractNext()) //throw ThreadInterruption
            {
                ImageHolder ih = dpiScale(wi->width,    wi->height,
                                          wi->dpiWidth, wi->dpiHeight,
                                          wi->rgb, wi->alpha, hqScale);
                result.access([&](std::vector<std::pair<Zbase<wchar_t>, ImageHolder>>& r) { r.emplace_back(wi->name, std::move(ih)); });
            }
        });
    }

    ~DpiParallelScaler()
    {
        for (InterruptibleThread& w : worker_)
            w.interrupt();

        for (InterruptibleThread& w : worker_)
            if (w.joinable())
                w.join();
    }

    void add(const wxString& name, const wxImage& img)
    {
        imgKeeper_.push_back(img); //retain (ref-counted) wxImage so that the rgb/alpha pointers remain valid after passed to threads
        workload_.add({ copyStringTo<Zbase<wchar_t>>(name),
                        img.GetWidth(), img.GetHeight(),
                        fastFromDIP(img.GetWidth()), fastFromDIP(img.GetHeight()), //don't call fastFromDIP() from worker thread (wxWidgets function!)
                        img.GetData(), img.GetAlpha() });
    }

    std::map<wxString, wxBitmap> waitAndGetResult()
    {
        workload_.noMoreWork();

        for (InterruptibleThread& w : worker_)
            w.join();

        std::map<wxString, wxBitmap> output;

        result_.access([&](std::vector<std::pair<Zbase<wchar_t>, ImageHolder>>& r)
        {
            for (auto& item : r)
            {
                ImageHolder& ih = item.second;

                wxImage img(ih.getWidth(), ih.getHeight(), ih.releaseRgb(), false /*static_data*/); //pass ownership
                img.SetAlpha(ih.releaseAlpha(), false /*static_data*/);

                output[utfTo<wxString>(item.first)] = wxBitmap(img);
            }
        });
        return output;
    }

private:
    std::vector<InterruptibleThread> worker_;
    WorkLoad workload_;
    Protected<std::vector<std::pair<Zbase<wchar_t>, ImageHolder>>> result_;
    std::vector<wxImage> imgKeeper_;
};


void loadAnimFromZip(wxZipInputStream& zipInput, wxAnimation& anim)
{
    //work around wxWidgets bug:
    //construct seekable input stream (zip-input stream is not seekable) for wxAnimation::Load()
    //luckily this method call is very fast: below measurement precision!
    std::vector<char> data;
    data.reserve(10000);

    int newValue = 0;
    while ((newValue = zipInput.GetC()) != wxEOF)
        data.push_back(newValue);

    wxMemoryInputStream seekAbleStream(&data.front(), data.size()); //stream does not take ownership of data

    anim.Load(seekAbleStream, wxANIMATION_TYPE_GIF);
}

//================================================================================================
//================================================================================================

class GlobalBitmaps
{
public:
    static std::shared_ptr<GlobalBitmaps> instance()
    {
        static Global<GlobalBitmaps> inst(std::make_unique<GlobalBitmaps>());
        assert(std::this_thread::get_id() == mainThreadId); //wxWidgets is not thread-safe!
        return inst.get();
    }

    GlobalBitmaps() {}
    ~GlobalBitmaps() { assert(bitmaps_.empty() && anims_.empty()); } //don't leave wxWidgets objects for static destruction!

    void init(const Zstring& filepath);
    void cleanup()
    {
        bitmaps_.clear();
        anims_  .clear();
        dpiScaler_.reset();
    }

    const wxBitmap&    getImage    (const wxString& name);
    const wxAnimation& getAnimation(const wxString& name) const;

private:
    GlobalBitmaps           (const GlobalBitmaps&) = delete;
    GlobalBitmaps& operator=(const GlobalBitmaps&) = delete;

    std::map<wxString, wxBitmap> bitmaps_;
    std::map<wxString, wxAnimation> anims_;

    std::unique_ptr<DpiParallelScaler> dpiScaler_;
};


void GlobalBitmaps::init(const Zstring& filePath)
{
    assert(bitmaps_.empty() && anims_.empty());

    wxFFileInputStream input(utfTo<wxString>(filePath));
    if (input.IsOk()) //if not... we don't want to react too harsh here
    {
        //activate support for .png files
        wxImage::AddHandler(new wxPNGHandler); //ownership passed

        wxZipInputStream streamIn(input, wxConvUTF8);
        //do NOT rely on wxConvLocal! On failure shows unhelpful popup "Cannot convert from the charset 'Unknown encoding (-1)'!"

        //do we need xBRZ scaling for high quality DPI images?
        const int hqScale = numeric::clampCpy<int>(std::ceil(fastFromDIP(1000) / 1000.0), 1, xbrz::SCALE_FACTOR_MAX);
        //even for 125% DPI scaling, "2xBRZ + bilinear downscale" gives a better result than mere "125% bilinear upscale"!
        if (hqScale > 1)
            dpiScaler_ = std::make_unique<DpiParallelScaler>(hqScale);

        while (const auto& entry = std::unique_ptr<wxZipEntry>(streamIn.GetNextEntry())) //take ownership!)
        {
            const wxString name = entry->GetName();

            if (endsWith(name, L".png"))
            {
                wxImage img(streamIn, wxBITMAP_TYPE_PNG);

                //end this alpha/no-alpha/mask/wxDC::DrawBitmap/RTL/high-contrast-scheme interoperability nightmare here and now!!!!
                //=> there's only one type of wxImage: with alpha channel, no mask!!!
                convertToVanillaImage(img);

                if (dpiScaler_)
                    dpiScaler_->add(name, img); //scale in parallel!
                else
                    bitmaps_.emplace(name, img);
            }
            else if (endsWith(name, L".gif"))
                loadAnimFromZip(streamIn, anims_[name]);
        }
    }
}


const wxBitmap& GlobalBitmaps::getImage(const wxString& name)
{
    //test: this function is first called about 220ms after GlobalBitmaps::init() has ended
    //      => should be enough time to finish xBRZ scaling in parallel (which takes 50ms)
    //debug perf: extra 800-1000ms during startup
    if (dpiScaler_)
    {
        bitmaps_ = dpiScaler_->waitAndGetResult();
        dpiScaler_.reset();
    }

    auto it = bitmaps_.find(contains(name, L'.') ? name : name + L".png"); //assume .png ending if nothing else specified
    if (it != bitmaps_.end())
        return it->second;
    assert(false);
    return wxNullBitmap;
}


const wxAnimation& GlobalBitmaps::getAnimation(const wxString& name) const
{
    auto it = anims_.find(contains(name, L'.') ? name : name + L".gif");
    if (it != anims_.end())
        return it->second;
    assert(false);
    return wxNullAnimation;
}
}


void zen::initResourceImages(const Zstring& filepath)
{
    if (std::shared_ptr<GlobalBitmaps> inst = GlobalBitmaps::instance())
        inst->init(filepath);
    else
        assert(false);
}


void zen::cleanupResourceImages()
{
    if (std::shared_ptr<GlobalBitmaps> inst = GlobalBitmaps::instance())
        inst->cleanup();
    else
        assert(false);
}


const wxBitmap& zen::getResourceImage(const wxString& name)
{
    if (std::shared_ptr<GlobalBitmaps> inst = GlobalBitmaps::instance())
        return inst->getImage(name);
    assert(false);
    return wxNullBitmap;
}


const wxAnimation& zen::getResourceAnimation(const wxString& name)
{
    if (std::shared_ptr<GlobalBitmaps> inst = GlobalBitmaps::instance())
        return inst->getAnimation(name);
    assert(false);
    return wxNullAnimation;
}

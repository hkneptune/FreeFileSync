// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "icon_buffer.h"
#include <map>
#include <set>
#include <zen/thread.h> //includes <std/thread.hpp>
#include <zen/scope_guard.h>
#include <wx+/image_resources.h>
#include <wx+/dc.h>
#include "icon_loader.h"


using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


namespace
{
const size_t BUFFER_SIZE_MAX = 800; //maximum number of icons to hold in buffer: must be big enough to hold visible icons + preload buffer! Consider OS limit on GDI resources (wxBitmap)!!!


//destroys raw icon! Call from GUI thread only!
wxBitmap extractWxBitmap(ImageHolder&& ih)
{
    assert(runningMainThread());

    if (!ih.getRgb())
        return wxNullBitmap;

    wxImage img(ih.getWidth(), ih.getHeight(), ih.releaseRgb(), false /*static_data*/); //pass ownership
    if (ih.getAlpha())
        img.SetAlpha(ih.releaseAlpha(), false /*static_data*/);
    return wxBitmap(img);
}


}

//################################################################################################################################################

ImageHolder getDisplayIcon(const AbstractPath& itemPath, IconBuffer::IconSize sz)
{
    //1. try to load thumbnails
    switch (sz)
    {
        case IconBuffer::SIZE_SMALL:
            break;
        case IconBuffer::SIZE_MEDIUM:
        case IconBuffer::SIZE_LARGE:
            if (ImageHolder img = AFS::getThumbnailImage(itemPath, IconBuffer::getSize(sz)))
                return img;
            //else: fallback to non-thumbnail icon
            break;
    }

    const Zstring& templateName = AFS::getItemName(itemPath);

    //2. retrieve file icons
        if (ImageHolder ih = AFS::getFileIcon(itemPath, IconBuffer::getSize(sz)))
            return ih;

    //3. fallbacks
    if (ImageHolder ih = getIconByTemplatePath(templateName, IconBuffer::getSize(sz)))
        return ih;

    return genericFileIcon(IconBuffer::getSize(sz));
}

//################################################################################################################################################

//---------------------- Shared Data -------------------------
class WorkLoad
{
public:
    //context of main thread
    void set(const std::vector<AbstractPath>& newLoad)
    {
        assert(runningMainThread());
        {
            std::lock_guard dummy(lockFiles_);

            workLoad_.clear();
            for (const AbstractPath& filePath : newLoad)
                workLoad_.emplace_back(filePath);
        }
        conditionNewWork_.notify_all(); //instead of notify_one(); workaround bug: https://svn.boost.org/trac/boost/ticket/7796
        //condition handling, see: https://www.boost.org/doc/libs/1_43_0/doc/html/thread/synchronization.html#thread.synchronization.condvar_ref
    }

    void add(const AbstractPath& filePath) //context of main thread
    {
        assert(runningMainThread());
        {
            std::lock_guard dummy(lockFiles_);
            workLoad_.emplace_back(filePath); //set as next item to retrieve
        }
        conditionNewWork_.notify_all();
    }

    //context of worker thread, blocking:
    AbstractPath extractNext() //throw ThreadInterruption
    {
        assert(!runningMainThread());
        std::unique_lock dummy(lockFiles_);

        interruptibleWait(conditionNewWork_, dummy, [this] { return !workLoad_.empty(); }); //throw ThreadInterruption

        AbstractPath filePath = workLoad_.    back(); //yes, no strong exception guarantee (std::bad_alloc)
        /**/                    workLoad_.pop_back(); //
        return filePath;
    }

private:
    //AbstractPath is thread-safe like an int!
    std::mutex                lockFiles_;
    std::condition_variable   conditionNewWork_; //signal event: data for processing available
    std::vector<AbstractPath> workLoad_; //processes last elements of vector first!
};


class Buffer
{
public:
    //called by main and worker thread:
    bool hasIcon(const AbstractPath& filePath) const
    {
        std::lock_guard dummy(lockIconList_);
        return iconList.find(filePath) != iconList.end();
    }

    //must be called by main thread only! => wxBitmap is NOT thread-safe like an int (non-atomic ref-count!!!)
    std::optional<wxBitmap> retrieve(const AbstractPath& filePath)
    {
        assert(runningMainThread());
        std::lock_guard dummy(lockIconList_);

        auto it = iconList.find(filePath);
        if (it == iconList.end())
            return {};

        markAsHot(it);

        IconData& idata = refData(it);
        if (idata.iconRaw) //if not yet converted...
        {
            idata.iconFmt = std::make_unique<wxBitmap>(extractWxBitmap(std::move(idata.iconRaw))); //convert in main thread!
            assert(!idata.iconRaw);
        }
        return idata.iconFmt ? *idata.iconFmt : wxNullBitmap; //idata.iconRaw may be inserted as empty from worker thread!
    }

    //called by main and worker thread:
    void insert(const AbstractPath& filePath, ImageHolder&& icon)
    {
        std::lock_guard dummy(lockIconList_);

        //thread safety: moving ImageHolder is free from side effects, but ~wxBitmap() is NOT! => do NOT delete items from iconList here!
        auto rc = iconList.emplace(filePath, IconData());
        assert(rc.second); //insertion took place
        if (rc.second)
        {
            refData(rc.first).iconRaw = std::move(icon);
            priorityListPushBack(rc.first);
        }
    }

    //must be called by main thread only! => ~wxBitmap() is NOT thread-safe!
    //call at an appropriate time, e.g. after Workload::set()
    void limitSize()
    {
        assert(runningMainThread());
        std::lock_guard dummy(lockIconList_);

        while (iconList.size() > BUFFER_SIZE_MAX)
        {
            auto itDelPos = firstInsertPos_;
            priorityListPopFront();
            iconList.erase(itDelPos); //remove oldest element
        }
    }

private:
    struct IconData;
    using FileIconMap = std::map<AbstractPath, IconData>;
    IconData& refData(FileIconMap::iterator it) { return it->second; }

    //call while holding lock:
    void priorityListPopFront()
    {
        assert(firstInsertPos_!= iconList.end());
        firstInsertPos_ = refData(firstInsertPos_).next;

        if (firstInsertPos_ != iconList.end())
            refData(firstInsertPos_).prev = iconList.end();
        else //priority list size > BUFFER_SIZE_MAX in this context, but still for completeness:
            lastInsertPos_ = iconList.end();
    }

    //call while holding lock:
    void priorityListPushBack(FileIconMap::iterator it)
    {
        if (lastInsertPos_ == iconList.end())
        {
            assert(firstInsertPos_ == iconList.end());
            firstInsertPos_ = lastInsertPos_ = it;
            refData(it).prev = refData(it).next = iconList.end();
        }
        else
        {
            refData(it).next = iconList.end();
            refData(it).prev = lastInsertPos_;
            refData(lastInsertPos_).next = it;
            lastInsertPos_ = it;
        }
    }

    //call while holding lock:
    void markAsHot(FileIconMap::iterator it) //mark existing buffer entry as if newly inserted
    {
        assert(it != iconList.end());
        if (refData(it).next != iconList.end())
        {
            if (refData(it).prev != iconList.end())
            {
                refData(refData(it).prev).next = refData(it).next; //remove somewhere from the middle
                refData(refData(it).next).prev = refData(it).prev; //
            }
            else
            {
                assert(it == firstInsertPos_);
                priorityListPopFront();
            }
            priorityListPushBack(it);
        }
        else
        {
            if (refData(it).prev != iconList.end())
                assert(it == lastInsertPos_); //nothing to do
            else
                assert(iconList.size() == 1 && it == firstInsertPos_ && it == lastInsertPos_); //nothing to do
        }
    }

    struct IconData
    {
        IconData() {}
        IconData(IconData&& tmp) noexcept : iconRaw(std::move(tmp.iconRaw)), iconFmt(std::move(tmp.iconFmt)), prev(tmp.prev), next(tmp.next) {}

        ImageHolder iconRaw; //native icon representation: may be used by any thread

        std::unique_ptr<wxBitmap> iconFmt; //use ONLY from main thread!
        //wxBitmap is NOT thread-safe: non-atomic ref-count just to begin with...
        //- prohibit implicit calls to wxBitmap(const wxBitmap&)
        //- prohibit calls to ~wxBitmap() and transitively ~IconData()
        //- prohibit even wxBitmap() default constructor - better be safe than sorry!

        FileIconMap::iterator prev; //store list sorted by time of insertion into buffer
        FileIconMap::iterator next; //
    };

    mutable std::mutex lockIconList_;
    FileIconMap iconList; //shared resource; Zstring is thread-safe like an int
    FileIconMap::iterator firstInsertPos_ = iconList.end();
    FileIconMap::iterator lastInsertPos_  = iconList.end();
};

//################################################################################################################################################


//#########################  redirect to impl  #####################################################

struct IconBuffer::Impl
{
    //communication channel used by threads:
    WorkLoad workload; //manage life time: enclose InterruptibleThread's (until joined)!!!
    Buffer   buffer;   //

    InterruptibleThread worker;
    //-------------------------
    //-------------------------
    std::map<Zstring, wxBitmap, LessAsciiNoCase> extensionIcons; //no item count limit!? Test case C:\ ~ 3800 unique file extensions
};


IconBuffer::IconBuffer(IconSize sz) : pimpl_(std::make_unique<Impl>()), iconSizeType_(sz)
{
    pimpl_->worker = InterruptibleThread([&workload = pimpl_->workload, &buffer = pimpl_->buffer, sz]
    {
        setCurrentThreadName("Icon Buffer");

        for (;;)
        {
            //start work: blocks until next icon to load is retrieved:
            const AbstractPath itemPath = workload.extractNext(); //throw ThreadInterruption

            if (!buffer.hasIcon(itemPath)) //perf: workload may contain duplicate entries?
                buffer.insert(itemPath, getDisplayIcon(itemPath, sz));
        }
    });
}


IconBuffer::~IconBuffer()
{
    setWorkload({}); //make sure interruption point is always reached! needed???
    pimpl_->worker.interrupt();
    pimpl_->worker.join();
}


int IconBuffer::getSize(IconSize sz)
{
    //coordinate with getIconByIndexImpl() and linkOverlayIcon()!
    switch (sz)
    {
        case IconBuffer::SIZE_SMALL:
            return fastFromDIP(24);
        case IconBuffer::SIZE_MEDIUM:
            return fastFromDIP(48);

        case IconBuffer::SIZE_LARGE:
            return fastFromDIP(128);
    }
    assert(false);
    return 0;
}


bool IconBuffer::readyForRetrieval(const AbstractPath& filePath)
{
    return pimpl_->buffer.hasIcon(filePath);
}


std::optional<wxBitmap> IconBuffer::retrieveFileIcon(const AbstractPath& filePath)
{
    if (std::optional<wxBitmap> ico = pimpl_->buffer.retrieve(filePath))
        return ico;

    //since this icon seems important right now, we don't want to wait until next setWorkload() to start retrieving
    pimpl_->workload.add(filePath);
    pimpl_->buffer.limitSize();
    return {};
}


void IconBuffer::setWorkload(const std::vector<AbstractPath>& load)
{
    assert(load.size() < BUFFER_SIZE_MAX / 2);

    pimpl_->workload.set(load); //since buffer can only increase due to new workload,
    pimpl_->buffer.limitSize(); //this is the place to impose the limit from main thread!
}


wxBitmap IconBuffer::getIconByExtension(const Zstring& filePath)
{
    const Zstring& ext = getFileExtension(filePath);

    assert(runningMainThread());

    auto it = pimpl_->extensionIcons.find(ext);
    if (it == pimpl_->extensionIcons.end())
    {
        const Zstring& templateName(ext.empty() ? Zstr("file") : Zstr("file.") + ext);
        //don't pass actual file name to getIconByTemplatePath(), e.g. "AUTHORS" has own mime type on Linux!!!
        //=> we want to buffer by extension only to minimize buffer-misses!

        it = pimpl_->extensionIcons.emplace(ext, extractWxBitmap(getIconByTemplatePath(templateName, getSize(iconSizeType_)))).first;
    }
    //need buffer size limit???
    return it->second;
}


wxBitmap IconBuffer::genericFileIcon(IconSize sz)
{
    return extractWxBitmap(fff::genericFileIcon(IconBuffer::getSize(sz)));
}


wxBitmap IconBuffer::genericDirIcon(IconSize sz)
{
    return extractWxBitmap(fff::genericDirIcon(IconBuffer::getSize(sz)));
}


wxBitmap IconBuffer::linkOverlayIcon(IconSize sz)
{
    //coordinate with IconBuffer::getSize()!
    return getResourceImage([sz]
    {
        const int pixelSize = IconBuffer::getSize(sz);

        if (pixelSize >= fastFromDIP(128)) return L"link_128";
        if (pixelSize >=  fastFromDIP(48)) return L"link_48";
        if (pixelSize >=  fastFromDIP(24)) return L"link_24";
        return L"link_16";
    }());
}


bool fff::hasLinkExtension(const Zstring& filepath)
{
    const Zstring& ext = getFileExtension(filepath);
    return ext == "desktop";

}

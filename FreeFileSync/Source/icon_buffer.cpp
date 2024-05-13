// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "icon_buffer.h"
#include <map>
//#include <set>
#include <variant>
#include <zen/thread.h> //includes <std/thread.hpp>
//#include <zen/scope_guard.h>
#include <wx+/dc.h>
#include <wx+/image_resources.h>
//#include <wx+/image_tools.h>
#include <wx+/std_button_layout.h>
#include "base/icon_loader.h"


using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


namespace
{
const size_t BUFFER_SIZE_MAX = 1000; //maximum number of icons to hold in buffer: must be big enough to hold visible icons + preload buffer!


}

//################################################################################################################################################

std::variant<ImageHolder, FileIconHolder> getDisplayIcon(const AbstractPath& itemPath, IconBuffer::IconSize sz)
{
    //1. try to load thumbnails
    switch (sz)
    {
        case IconBuffer::IconSize::small:
            break;
        case IconBuffer::IconSize::medium:
        case IconBuffer::IconSize::large:
            try
            {
                if (ImageHolder ih = AFS::getThumbnailImage(itemPath, IconBuffer::getPixSize(sz))) //throw FileError; optional return value
                    return ih;
            }
            catch (FileError&) {}

            //else: fallback to non-thumbnail icon
            break;
    }

    //2. retrieve file icons
        try
        {
            if (FileIconHolder fih = AFS::getFileIcon(itemPath, IconBuffer::getPixSize(sz))) //throw FileError; optional return value
                return fih;
        }
        catch (FileError&) {}

    //run getIconByTemplatePath()/genericFileIcon() fallbacks on main thread:
    //extractWxImage() might fail if icon theme is missing a MIME type!
    return ImageHolder();
}

//################################################################################################################################################

//---------------------- Shared Data -------------------------
class WorkLoad
{
public:
    //context of main thread
    void set(const std::vector<AbstractPath>& newLoad)
    {
        assert(runningOnMainThread());
        {
            std::lock_guard dummy(lockFiles_);
            workLoad_ = newLoad;
        }
        conditionNewWork_.notify_all(); //instead of notify_one(); work around bug: https://svn.boost.org/trac/boost/ticket/7796
        //condition handling, see: https://www.boost.org/doc/libs/1_43_0/doc/html/thread/synchronization.html#thread.synchronization.condvar_ref
    }

    void add(const AbstractPath& filePath) //context of main thread
    {
        assert(runningOnMainThread());
        {
            std::lock_guard dummy(lockFiles_);
            workLoad_.emplace_back(filePath); //set as next item to retrieve
        }
        conditionNewWork_.notify_all();
    }

    //context of worker thread, blocking:
    AbstractPath extractNext() //throw ThreadStopRequest
    {
        assert(!runningOnMainThread());
        std::unique_lock dummy(lockFiles_);

        interruptibleWait(conditionNewWork_, dummy, [this] { return !workLoad_.empty(); }); //throw ThreadStopRequest

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
        return iconList.contains(filePath);
    }

    //- must be called by main thread only! => wxImage is NOT thread-safe like an int (non-atomic ref-count!!!)
    //- check wxImage::IsOk() + implement fallback if needed
    std::optional<wxImage> retrieve(const AbstractPath& filePath)
    {
        assert(runningOnMainThread());
        std::lock_guard dummy(lockIconList_);

        auto it = iconList.find(filePath);
        if (it == iconList.end())
            return {};

        markAsHot(it);

        IconData& idata = refData(it);

        if (ImageHolder* ih = std::get_if<ImageHolder>(&idata.iconHolder))
        {
            if (*ih) //if not yet converted...
            {
                idata.iconImg = std::make_unique<wxImage>(extractWxImage(std::move(*ih))); //convert in main thread!
                assert(!*ih);
            }
        }
        else
        {
            if (FileIconHolder& fih = std::get<FileIconHolder>(idata.iconHolder)) //if not yet converted...
            {
                idata.iconImg = std::make_unique<wxImage>(extractWxImage(std::move(fih))); //convert in main thread!
                assert(!fih);
                //!idata.iconImg->IsOk(): extractWxImage() might fail if icon theme is missing a MIME type!
            }
        }

        return idata.iconImg ? *idata.iconImg : wxNullImage; //idata.iconHolder may be inserted as empty from worker thread!
    }

    //called by main and worker thread:
    void insert(const AbstractPath& filePath, std::variant<ImageHolder, FileIconHolder>&& ih)
    {
        std::lock_guard dummy(lockIconList_);

        //thread safety: moving ImageHolder is free from side effects, but ~wxImage() is NOT! => do NOT delete items from iconList here!
        const auto [it, inserted] = iconList.try_emplace(filePath);
        assert(inserted);
        if (inserted)
        {
            refData(it).iconHolder = std::move(ih);
            priorityListPushBack(it);
        }
    }

    //must be called by main thread only! => ~wxImage() is NOT thread-safe!
    //call at an appropriate time, e.g. after Workload::set()
    void limitSize()
    {
        assert(runningOnMainThread());
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
        IconData(IconData&& tmp) noexcept : iconHolder(std::move(tmp.iconHolder)), iconImg(std::move(tmp.iconImg)), prev(tmp.prev), next(tmp.next) {}

        std::variant<ImageHolder, FileIconHolder> iconHolder; //native icon representation: may be used by any thread

        std::unique_ptr<wxImage> iconImg; //use ONLY from main thread!
        //wxImage is NOT thread-safe: non-atomic ref-count just to begin with...
        //- prohibit implicit calls to wxImage()
        //- prohibit calls to ~wxImage() and transitively ~IconData()
        //- prohibit even wxImage() default constructor - better be safe than sorry!

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
    std::unordered_map<Zstring, wxImage, StringHashAsciiNoCase, StringEqualAsciiNoCase> extensionIcons; //no item count limit!? Test case C:\ ~ 3800 unique file extensions
};


IconBuffer::IconBuffer(IconSize sz) : pimpl_(std::make_unique<Impl>()), iconSizeType_(sz)
{
    pimpl_->worker = InterruptibleThread([&workload = pimpl_->workload, &buffer = pimpl_->buffer, sz]
    {
        setCurrentThreadName(Zstr("Icon Buffer"));

        for (;;)
        {
            //start work: blocks until next icon to load is retrieved:
            const AbstractPath itemPath = workload.extractNext(); //throw ThreadStopRequest

            if (!buffer.hasIcon(itemPath)) //perf: workload may contain duplicate entries?
                buffer.insert(itemPath, getDisplayIcon(itemPath, sz));
        }
    });
}


IconBuffer::~IconBuffer()
{
    setWorkload({}); //make sure interruption point is always reached! needed???
    pimpl_->worker.requestStop(); //end thread life time *before*
    pimpl_->worker.join();        //IconBuffer::Impl member clean up!
}


int IconBuffer::getPixSize(IconSize sz)
{
    //coordinate with getIconByIndexImpl() and linkOverlayIcon()!
    switch (sz)
    {
        case IconSize::small:
            return dipToScreen(getMenuIconDipSize());
        case IconSize::medium:
            return dipToScreen(48);
        case IconSize::large:
            return dipToScreen(128);
    }
    assert(false);
    return 0;
}


bool IconBuffer::readyForRetrieval(const AbstractPath& filePath)
{
    return pimpl_->buffer.hasIcon(filePath);
}


std::optional<wxImage> IconBuffer::retrieveFileIcon(const AbstractPath& filePath)
{
    const Zstring fileName = AFS::getItemName(filePath);
    if (std::optional<wxImage> ico = pimpl_->buffer.retrieve(filePath))
    {
        if (ico->IsOk())
            return ico;
        else //fallback
            return this->getIconByExtension(fileName); //buffered!
    }

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


wxImage IconBuffer::getIconByExtension(const Zstring& filePath)
{
    const Zstring& ext = getFileExtension(filePath);

    assert(runningOnMainThread());

    auto it = pimpl_->extensionIcons.find(ext);
    if (it == pimpl_->extensionIcons.end())
    {
        const Zstring& templateName(ext.empty() ? Zstr("file") : Zstr("file.") + ext);
        //don't pass actual file name to getIconByTemplatePath(), e.g. "AUTHORS" has own mime type on Linux!!!
        //=> buffer by extension to minimize buffer-misses!

        wxImage img;
        try
        {
            img = extractWxImage(getIconByTemplatePath(templateName, getPixSize(iconSizeType_))); //throw SysError
        }
        catch (SysError&) {}
        if (!img.IsOk()) //Linux: not all MIME types have icons!
            img = IconBuffer::genericFileIcon(iconSizeType_);

        it = pimpl_->extensionIcons.emplace(ext, img).first;
    }
    //need buffer size limit???
    return it->second;
}


wxImage IconBuffer::genericFileIcon(IconSize sz)
{
    try
    {
        return extractWxImage(fff::genericFileIcon(IconBuffer::getPixSize(sz))); //throw SysError
    }
    catch (SysError&) { assert(false); return wxNullImage; }
}


wxImage IconBuffer::genericDirIcon(IconSize sz)
{
    try
    {
        return extractWxImage(fff::genericDirIcon(IconBuffer::getPixSize(sz))); //throw SysError
    }
    catch (SysError&) { assert(false); return wxNullImage; }
}


wxImage IconBuffer::linkOverlayIcon(IconSize sz)
{
    //coordinate with IconBuffer::getPixSize()!
    return loadImage([sz]
    {
        const int iconSize = IconBuffer::getPixSize(sz);

        if (iconSize >= dipToScreen(128)) return "file_link_128";
        if (iconSize >= dipToScreen( 48)) return "file_link_48";
        if (iconSize >= dipToScreen( 20)) return "file_link_20";
        return "file_link_16";
    }());
}


wxImage IconBuffer::plusOverlayIcon(IconSize sz)
{
    //coordinate with IconBuffer::getPixSize()!
    return loadImage([sz]
    {
        const int iconSize = IconBuffer::getPixSize(sz);

        if (iconSize >= dipToScreen(128)) return "file_plus_128";
        if (iconSize >= dipToScreen( 48)) return "file_plus_48";
        if (iconSize >= dipToScreen( 20)) return "file_plus_20";
        return "file_plus_16";
    }());
}


wxImage IconBuffer::minusOverlayIcon(IconSize sz)
{
    //coordinate with IconBuffer::getPixSize()!
    return loadImage([sz]
    {
        const int iconSize = IconBuffer::getPixSize(sz);

        if (iconSize >= dipToScreen(128)) return "file_minus_128";
        if (iconSize >= dipToScreen( 48)) return "file_minus_48";
        if (iconSize >= dipToScreen( 20)) return "file_minus_20";
        return "file_minus_16";
    }());
}


bool fff::hasLinkExtension(const Zstring& filepath)
{
    const Zstring& ext = getFileExtension(filepath);
    return ext == "desktop";

}

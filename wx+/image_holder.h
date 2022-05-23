// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef IMAGE_HOLDER_H_284578426342567457
#define IMAGE_HOLDER_H_284578426342567457

#include <memory>
    #include <gio/gio.h>
//used by fs/abstract.h => check carefully before adding dependencies!
//DO NOT add any wx/wx+ includes!

namespace zen
{
struct ImageHolder //prepare conversion to wxImage as much as possible while staying thread-safe (in contrast to wxIcon/wxBitmap)
{
    ImageHolder() {}

    ImageHolder(int w, int h, bool withAlpha) : //init with memory allocated
        width_(w), height_(h),
        rgb_(              static_cast<unsigned char*>(::malloc(w * h * 3))),
        alpha_(withAlpha ? static_cast<unsigned char*>(::malloc(w * h)) : nullptr) {}

    ImageHolder           (ImageHolder&&) noexcept = default; //
    ImageHolder& operator=(ImageHolder&&) noexcept = default; //move semantics only!
    ImageHolder           (const ImageHolder&)     = delete;  //
    ImageHolder& operator=(const ImageHolder&)     = delete;  //

    explicit operator bool() const { return rgb_.get() != nullptr; }

    int getWidth () const { return width_;  }
    int getHeight() const { return height_; }

    unsigned char* getRgb  () { return rgb_  .get(); }
    unsigned char* getAlpha() { return alpha_.get(); }

    unsigned char* releaseRgb  () { return rgb_  .release(); }
    unsigned char* releaseAlpha() { return alpha_.release(); }

private:
    struct CLibFree { void operator()(unsigned char* p) const { ::free(p); } }; //use malloc/free to allow direct move into wxImage!

    int width_  = 0;
    int height_ = 0;
    std::unique_ptr<unsigned char, CLibFree> rgb_;   //optional
    std::unique_ptr<unsigned char, CLibFree> alpha_; //
};


struct FileIconHolder
{
    //- GTK is NOT thread-safe! The most we can do from worker threads is retrieve a GIcon and later *try*(!) to convert it on the MAIN THREAD! >:( what a waste
    //- at least g_file_query_info() *always* returns G_IS_THEMED_ICON(gicon) for native file systems => main thread won't block https://gitlab.gnome.org/GNOME/glib/blob/master/gio/glocalfileinfo.c#L1733
    //- what about G_IS_FILE_ICON(gicon), G_IS_LOADABLE_ICON(gicon)? => may block! => do NOT convert on main thread! (no big deal: doesn't seem to occur in practice)
    FileIconHolder() {};

    FileIconHolder(GIcon* icon, int maxSz) : //takes ownership!
        gicon(icon),
        maxSize(maxSz) {}

    struct GiconFree { void operator()(GIcon* icon) const { ::g_object_unref(icon); } };

    std::unique_ptr<GIcon, GiconFree> gicon;
    int maxSize = 0;

    explicit operator bool() const { return static_cast<bool>(gicon); }
};
}

#endif //IMAGE_HOLDER_H_284578426342567457

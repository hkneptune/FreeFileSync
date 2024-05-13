// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "icon_loader.h"
//#include <zen/scope_guard.h>
#include <zen/thread.h> //includes <std/thread.hpp>

    #include <gtk/gtk.h>
    #include <sys/stat.h>
    #include <zen/sys_error.h>
    #include <zen/basic_math.h>
    #include <xBRZ/src/xbrz_tools.h>


using namespace zen;
using namespace fff;


namespace
{
ImageHolder copyToImageHolder(const GdkPixbuf& pixBuf, int maxSize) //throw SysError
{
    //see: https://developer.gnome.org/gdk-pixbuf/stable/gdk-pixbuf-The-GdkPixbuf-Structure.html
    if (const GdkColorspace cs = ::gdk_pixbuf_get_colorspace(&pixBuf);
        cs != GDK_COLORSPACE_RGB)
        throw SysError(formatSystemError("gdk_pixbuf_get_colorspace", L"", L"Unexpected color space: " + numberTo<std::wstring>(static_cast<int>(cs))));

    if (const int bitCount = ::gdk_pixbuf_get_bits_per_sample(&pixBuf);
        bitCount != 8)
        throw SysError(formatSystemError("gdk_pixbuf_get_bits_per_sample", L"", L"Unexpected bits per sample: " + numberTo<std::wstring>(bitCount)));

    const int channels = ::gdk_pixbuf_get_n_channels(&pixBuf);
    if (channels != 3 && channels != 4)
        throw SysError(formatSystemError("gdk_pixbuf_get_n_channels", L"", L"Unexpected number of channels: " + numberTo<std::wstring>(channels)));

    assert(::gdk_pixbuf_get_has_alpha(&pixBuf) == (channels == 4));

    const unsigned char* srcBytes = ::gdk_pixbuf_read_pixels(&pixBuf);
    const int srcWidth  = ::gdk_pixbuf_get_width (&pixBuf);
    const int srcHeight = ::gdk_pixbuf_get_height(&pixBuf);
    const int srcStride = ::gdk_pixbuf_get_rowstride(&pixBuf);

    //don't stretch small images, shrink large ones only!
    int targetWidth  = srcWidth;
    int targetHeight = srcHeight;

    const int maxExtent = std::max(targetWidth, targetHeight);
    if (maxSize < maxExtent)
    {
        targetWidth  = numeric::intDivRound(targetWidth  * maxSize, maxExtent);
        targetHeight = numeric::intDivRound(targetHeight * maxSize, maxExtent);

#if 0 //alternative to xbrz::bilinearScaleSimple()? does it support alpha-channel?
        GdkPixbuf* pixBufShrinked = ::gdk_pixbuf_scale_simple(pixBuf,               //const GdkPixbuf* src
                                                              targetWidth,          //int dest_width
                                                              targetHeight,         //int dest_height
                                                              GDK_INTERP_BILINEAR); //GdkInterpType interp_type
        if (!pixBufShrinked)
            throw SysError(formatSystemError("gdk_pixbuf_scale_simple", L"", L"Not enough memory."));
        ZEN_ON_SCOPE_EXIT(::g_object_unref(pixBufShrinked));
#endif
    }

    const auto imgReader = [srcBytes, srcStride, channels](int x, int y, xbrz::BytePixel& pix)
    {
        const unsigned char* const ptr = srcBytes + y * srcStride + channels * x;

        const unsigned char a = channels == 4 ? ptr[3] : 255;
        pix[0] = a;
        pix[1] = xbrz::premultiply(ptr[0], a); //r
        pix[2] = xbrz::premultiply(ptr[1], a); //g
        pix[3] = xbrz::premultiply(ptr[2], a); //b
    };

    ImageHolder imgOut(targetWidth, targetHeight, true /*withAlpha*/);

    const auto imgWriter = [rgb = imgOut.getRgb(), alpha = imgOut.getAlpha()](const xbrz::BytePixel& pix) mutable
    {
        const unsigned char a = pix[0];
        * alpha++ = a;
        * rgb++   = xbrz::demultiply(pix[1], a); //r
        *rgb++   = xbrz::demultiply(pix[2], a); //g
        *rgb++   = xbrz::demultiply(pix[3], a); //b
    };

    if (srcWidth  == targetWidth &&
        srcHeight == targetHeight)
        xbrz::unscaledCopy(imgReader, imgWriter, srcWidth, srcHeight); //perf: going overboard?
    else
        xbrz::bilinearScaleSimple(imgReader,     //PixReader srcReader
                                  srcWidth,      //int srcWidth
                                  srcHeight,     //int srcHeight
                                  imgWriter,     //PixWriter trgWriter
                                  targetWidth,   //int trgWidth
                                  targetHeight,  //int trgHeight
                                  0,             //int yFirst
                                  targetHeight); //int yLast
    return imgOut;
}


ImageHolder imageHolderFromGicon(GIcon& gicon, int maxSize) //throw SysError
{
    assert(runningOnMainThread()); //GTK is NOT thread safe!!!
    assert(!G_IS_FILE_ICON(&gicon) && !G_IS_LOADABLE_ICON(&gicon)); //see comment in image_holder.h => icon loading must not block main thread

    GtkIconTheme* const defaultTheme = ::gtk_icon_theme_get_default(); //not owned!
    ASSERT_SYSERROR(defaultTheme); //no more error details

    GtkIconInfo* const iconInfo = ::gtk_icon_theme_lookup_by_gicon(defaultTheme,                 //GtkIconTheme* icon_theme
                                                                   &gicon,                       //GIcon* icon
                                                                   maxSize,                      //gint size
                                                                   GTK_ICON_LOOKUP_USE_BUILTIN); //GtkIconLookupFlags flags
    if (!iconInfo)
        throw SysError(formatSystemError("gtk_icon_theme_lookup_by_gicon", L"", L"Icon not available."));
#if GTK_MAJOR_VERSION == 2
    ZEN_ON_SCOPE_EXIT(::gtk_icon_info_free(iconInfo));
#elif GTK_MAJOR_VERSION == 3
    ZEN_ON_SCOPE_EXIT(::g_object_unref(iconInfo));
#else
#error unknown GTK version!
#endif
    GError* error = nullptr;
    ZEN_ON_SCOPE_EXIT(if (error) ::g_error_free(error));

    GdkPixbuf* const pixBuf = ::gtk_icon_info_load_icon(iconInfo, &error);
    if (!pixBuf)
        throw SysError(formatGlibError("gtk_icon_info_load_icon", error));
    ZEN_ON_SCOPE_EXIT(::g_object_unref(pixBuf));

    //we may have to shrink (e.g. GTK3, openSUSE): "an icon theme may have icons that differ slightly from their nominal sizes"
    return copyToImageHolder(*pixBuf, maxSize); //throw SysError
}
}


FileIconHolder fff::getIconByTemplatePath(const Zstring& templatePath, int maxSize) //throw SysError
{
    //uses full file name, e.g. "AUTHORS" has own mime type on Linux:
    gchar* const contentType = ::g_content_type_guess(templatePath.c_str(), //const gchar* filename
                                                      nullptr,              //const guchar* data
                                                      0,                    //gsize data_size
                                                      nullptr);             //gboolean* result_uncertain
    if (!contentType)
        throw SysError(formatSystemError("g_content_type_guess(" + copyStringTo<std::string>(templatePath) + ')', L"", L"Unknown content type."));
    ZEN_ON_SCOPE_EXIT(::g_free(contentType));

    GIcon* const fileIcon = ::g_content_type_get_icon(contentType);
    if (!fileIcon)
        throw SysError(formatSystemError("g_content_type_get_icon(" + std::string(contentType) + ')', L"", L"Icon not available."));

    return FileIconHolder(fileIcon /*pass ownership*/, maxSize);

}


FileIconHolder fff::genericFileIcon(int maxSize) //throw SysError
{
    //we're called by getDisplayIcon()! -> avoid endless recursion!
    GIcon* const fileIcon = ::g_content_type_get_icon("text/plain");
    if (!fileIcon)
        throw SysError(formatSystemError("g_content_type_get_icon(text/plain)", L"", L"Icon not available."));

    return FileIconHolder(fileIcon /*pass ownership*/, maxSize);

}


FileIconHolder fff::genericDirIcon(int maxSize) //throw SysError
{
    GIcon* const dirIcon = ::g_content_type_get_icon("inode/directory"); //should contain fallback to GTK_STOCK_DIRECTORY ("gtk-directory")
    if (!dirIcon)
        throw SysError(formatSystemError("g_content_type_get_icon(inode/directory)", L"", L"Icon not available."));

    return FileIconHolder(dirIcon /*pass ownership*/, maxSize);

}


FileIconHolder fff::getTrashIcon(int maxSize) //throw SysError
{
    GIcon* const trashIcon = ::g_themed_icon_new("user-trash-full"); //empty: "user-trash"
    if (!trashIcon)
        throw SysError(formatSystemError("g_themed_icon_new(user-trash-full)", L"", L"Icon not available."));

    return FileIconHolder(trashIcon /*pass ownership*/, maxSize);

}


FileIconHolder fff::getFileManagerIcon(int maxSize) //throw SysError
{
    GIcon* const trashIcon = ::g_themed_icon_new("system-file-manager"); //empty: "user-trash"
    if (!trashIcon)
        throw SysError(formatSystemError("g_themed_icon_new(system-file-manager)", L"", L"Icon not available."));

    return FileIconHolder(trashIcon /*pass ownership*/, maxSize);

}


FileIconHolder fff::getFileIcon(const Zstring& filePath, int maxSize) //throw SysError
{
    GFile* file = ::g_file_new_for_path(filePath.c_str()); //documented to "never fail"
    ZEN_ON_SCOPE_EXIT(::g_object_unref(file));

    GError* error = nullptr;
    ZEN_ON_SCOPE_EXIT(if (error) ::g_error_free(error));

    GFileInfo* const fileInfo = ::g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_ICON, G_FILE_QUERY_INFO_NONE, nullptr /*cancellable*/, &error);
    if (!fileInfo)
        throw SysError(formatGlibError("g_file_query_info", error));
    ZEN_ON_SCOPE_EXIT(::g_object_unref(fileInfo));

    GIcon* const gicon = ::g_file_info_get_icon(fileInfo); //no ownership transfer!
    if (!gicon)
        throw SysError(formatSystemError("g_file_info_get_icon", L"", L"Icon not available."));

    //https://github.com/GNOME/gtk/blob/master/gtk/gtkicontheme.c#L4082
    if (G_IS_FILE_ICON(gicon) || G_IS_LOADABLE_ICON(gicon)) //see comment in image_holder.h
        throw SysError(L"Icon loading might block main thread.");
    //shouldn't be a problem for native file systems -> G_IS_THEMED_ICON(gicon)

    //the remaining icon types won't block!
    assert(GDK_IS_PIXBUF(gicon) || G_IS_THEMED_ICON(gicon) || G_IS_EMBLEMED_ICON(gicon));

    ::g_object_ref(gicon);                 //pass ownership
    return FileIconHolder(gicon, maxSize); //

}


ImageHolder fff::getThumbnailImage(const Zstring& filePath, int maxSize) //throw SysError
{
    struct stat fileInfo = {};
    if (::stat(filePath.c_str(), &fileInfo) != 0)
        THROW_LAST_SYS_ERROR("stat");

    if (!S_ISREG(fileInfo.st_mode)) //skip blocking file types, e.g. named pipes, see file_io.cpp
        throw SysError(_("Unsupported item type.") + L" [" + printNumber<std::wstring>(L"0%06o", fileInfo.st_mode & S_IFMT) + L']');

    GError* error = nullptr;
    ZEN_ON_SCOPE_EXIT(if (error) ::g_error_free(error));

    GdkPixbuf* const pixBuf = ::gdk_pixbuf_new_from_file(filePath.c_str(), &error);
    if (!pixBuf)
        throw SysError(formatGlibError("gdk_pixbuf_new_from_file", error));
    ZEN_ON_SCOPE_EXIT(::g_object_unref(pixBuf));

    return copyToImageHolder(*pixBuf, maxSize); //throw SysError

}


wxImage fff::extractWxImage(ImageHolder&& ih)
{
    assert(runningOnMainThread());
    if (!ih.getRgb())
        return wxNullImage;

    wxImage img(ih.getWidth(), ih.getHeight(), ih.releaseRgb(), false /*static_data*/); //pass ownership
    if (ih.getAlpha())
        img.SetAlpha(ih.releaseAlpha(), false /*static_data*/);
    else
    {
        assert(false);
        img.SetAlpha();
        ::memset(img.GetAlpha(), wxIMAGE_ALPHA_OPAQUE, ih.getWidth() * ih.getHeight());
    }
    return img;
}


wxImage fff::extractWxImage(zen::FileIconHolder&& fih)
{
    assert(runningOnMainThread());

    wxImage img;
    if (GIcon* gicon = fih.gicon.get())
        try
        {
            img = extractWxImage(imageHolderFromGicon(*gicon, fih.maxSize)); //throw SysError
        }
        catch (SysError&) {} //might fail if icon theme is missing a MIME type!

    fih.gicon.reset();
    return img;

}

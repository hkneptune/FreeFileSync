// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "icon_loader.h"
#include <zen/scope_guard.h>

    #include <gtk/gtk.h>
    #include <sys/stat.h>
    #include <zen/sys_error.h>


using namespace zen;
using namespace fff;


namespace
{
static_assert(GTK_MAJOR_VERSION == 2, "FreeFileSync does NOT (currently) support GTK3! The GTK calls below will lead to crashes due to not being thread-safe on GTK3.");
//gdk_threads_enter(); + ZEN_ON_SCOPE_EXIT(::gdk_threads_leave); is NOT enough; e.g. GTK3 + openSuse still crashes with:
//  Gtk:ERROR:gtkicontheme.c:4026:proxy_pixbuf_destroy: assertion failed: (icon_info->proxy_pixbuf != NULL)
//GTK icon theme internals: https://github.com/GNOME/gtk/blob/master/gtk/gtkicontheme.c
//Alternative: https://developer.gnome.org/gtk3/stable/GtkIconTheme.html#gtk-icon-info-load-icon-async
//GTK2 OTOH crashes only extremely rarely, still this must be fixed
warn_static("TODO: GTK / GDK not thread-safe! in particular gtk_icon_info_load_icon() is not!!!")


ImageHolder copyToImageHolder(const GdkPixbuf* pixBuf)
{
    //see: https://developer.gnome.org/gdk-pixbuf/stable/gdk-pixbuf-The-GdkPixbuf-Structure.html
    if (pixBuf &&
        ::gdk_pixbuf_get_colorspace(pixBuf) == GDK_COLORSPACE_RGB &&
        ::gdk_pixbuf_get_bits_per_sample(pixBuf) == 8)
    {
        const int channels = ::gdk_pixbuf_get_n_channels(pixBuf);
        if (channels == 3 || channels == 4)
        {
            const int stride = ::gdk_pixbuf_get_rowstride(pixBuf);
            const unsigned char* rgbaSrc = ::gdk_pixbuf_get_pixels(pixBuf);

            if (channels == 3)
            {
                assert(!::gdk_pixbuf_get_has_alpha(pixBuf));

                ImageHolder out(::gdk_pixbuf_get_width(pixBuf), ::gdk_pixbuf_get_height(pixBuf), false /*withAlpha*/);
                unsigned char* rgbTrg = out.getRgb();

                for (int y = 0; y < out.getHeight(); ++y)
                {
                    const unsigned char* srcLine = rgbaSrc + y * stride;
                    for (int x = 0; x < out.getWidth(); ++x)
                    {
                        *rgbTrg++ = *srcLine++;
                        *rgbTrg++ = *srcLine++;
                        *rgbTrg++ = *srcLine++;
                    }
                }
                return out;
            }
            else if (channels == 4)
            {
                assert(::gdk_pixbuf_get_has_alpha(pixBuf));

                ImageHolder out(::gdk_pixbuf_get_width(pixBuf), ::gdk_pixbuf_get_height(pixBuf), true /*withAlpha*/);
                unsigned char* rgbTrg   = out.getRgb();
                unsigned char* alphaTrg = out.getAlpha();

                for (int y = 0; y < out.getHeight(); ++y)
                {
                    const unsigned char* srcLine = rgbaSrc + y * stride;
                    for (int x = 0; x < out.getWidth(); ++x)
                    {
                        *rgbTrg++   = *srcLine++;
                        *rgbTrg++   = *srcLine++;
                        *rgbTrg++   = *srcLine++;
                        *alphaTrg++ = *srcLine++;
                    }
                }
                return out;
            }
        }
    }
    return ImageHolder();
}


ImageHolder imageHolderFromGicon(GIcon* gicon, int pixelSize)
{
    if (gicon)
        if (GtkIconTheme* defaultTheme = ::gtk_icon_theme_get_default()) //not owned!
            if (GtkIconInfo* iconInfo = ::gtk_icon_theme_lookup_by_gicon(defaultTheme, gicon, pixelSize, GTK_ICON_LOOKUP_USE_BUILTIN)) //this may fail if icon is not installed on system
            {
#if GTK_MAJOR_VERSION == 2
                ZEN_ON_SCOPE_EXIT(::gtk_icon_info_free(iconInfo));
#elif GTK_MAJOR_VERSION == 3
                ZEN_ON_SCOPE_EXIT(::g_object_unref(iconInfo));
#else
#error unknown GTK version!
#endif
                if (GdkPixbuf* pixBuf = ::gtk_icon_info_load_icon(iconInfo, nullptr /*error*/))
                {
                    ZEN_ON_SCOPE_EXIT(::g_object_unref(pixBuf));

                    //we might have to shrink the image (e.g. GTK3, openSUSE): "an icon theme may have icons that differ slightly from their nominal sizes"
                    const int width  = ::gdk_pixbuf_get_width (pixBuf);
                    const int height = ::gdk_pixbuf_get_height(pixBuf);

                    const int maxExtent = std::max(width, height);
                    if (pixelSize < maxExtent)
                    {
                        GdkPixbuf* pixBufShrinked = ::gdk_pixbuf_scale_simple(pixBuf,                         //const GdkPixbuf* src,
                                                                              width  * pixelSize / maxExtent, //int dest_width,
                                                                              height * pixelSize / maxExtent, //int dest_height,
                                                                              GDK_INTERP_BILINEAR);           //GdkInterpType interp_type
                        if (!pixBufShrinked)
                            return ImageHolder();
                        ZEN_ON_SCOPE_EXIT(::g_object_unref(pixBufShrinked));

                        return copyToImageHolder(pixBufShrinked);
                    }
                    else
                        return copyToImageHolder(pixBuf);
                }
            }
    return ImageHolder();
}
}


ImageHolder fff::getIconByTemplatePath(const Zstring& templatePath, int pixelSize)
{
    //uses full file name, e.g. "AUTHORS" has own mime type on Linux:
    if (gchar* contentType = ::g_content_type_guess(templatePath.c_str(), //const gchar* filename,
                                                    nullptr,              //const guchar* data,
                                                    0,                    //gsize data_size,
                                                    nullptr))             //gboolean* result_uncertain
    {
        ZEN_ON_SCOPE_EXIT(::g_free(contentType));
        if (GIcon* dirIcon = ::g_content_type_get_icon(contentType))
        {
            ZEN_ON_SCOPE_EXIT(::g_object_unref(dirIcon));
            return imageHolderFromGicon(dirIcon, pixelSize);
        }
    }
    return ImageHolder();

}


ImageHolder fff::genericFileIcon(int pixelSize)
{
    //we're called by getDisplayIcon()! -> avoid endless recursion!
    if (GIcon* fileIcon = ::g_content_type_get_icon("text/plain"))
    {
        ZEN_ON_SCOPE_EXIT(::g_object_unref(fileIcon));
        return imageHolderFromGicon(fileIcon, pixelSize);
    }
    return ImageHolder();

}


ImageHolder fff::genericDirIcon(int pixelSize)
{
    if (GIcon* dirIcon = ::g_content_type_get_icon("inode/directory")) //should contain fallback to GTK_STOCK_DIRECTORY ("gtk-directory")
    {
        ZEN_ON_SCOPE_EXIT(::g_object_unref(dirIcon));
        return imageHolderFromGicon(dirIcon, pixelSize);
    }
    return ImageHolder();

}


ImageHolder fff::getFileIcon(const Zstring& filePath, int pixelSize)
{
    try
    {
        GFile* file = ::g_file_new_for_path(filePath.c_str()); //documented to "never fail"
        ZEN_ON_SCOPE_EXIT(::g_object_unref(file));

        if (GFileInfo* fileInfo = ::g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_ICON, G_FILE_QUERY_INFO_NONE, nullptr, nullptr))
        {
            ZEN_ON_SCOPE_EXIT(::g_object_unref(fileInfo));
            if (GIcon* gicon = ::g_file_info_get_icon(fileInfo)) //not owned!
                return imageHolderFromGicon(gicon, pixelSize);
        }
        //need fallback: icon lookup may fail because some icons are currently not present on system

    }
    catch (SysError&) { assert(false); }

    return ImageHolder();
}


ImageHolder fff::getThumbnailImage(const Zstring& filePath, int pixelSize) //return null icon on failure
{
    try
    {
        struct ::stat fileInfo = {};
        if (::stat(filePath.c_str(), &fileInfo) == 0)
            if (!S_ISFIFO(fileInfo.st_mode)) //skip named pipes: else gdk_pixbuf_get_file_info() would hang forever!
            {
                gint width  = 0;
                gint height = 0;
                if (/*GdkPixbufFormat* fmt =*/ ::gdk_pixbuf_get_file_info(filePath.c_str(), &width, &height))
                    if (width > 0 && height > 0 && pixelSize > 0)
                    {
                        int trgWidth  = width;
                        int trgHeight = height;

                        const int maxExtent = std::max(width, height); //don't stretch small images, shrink large ones only!
                        if (pixelSize < maxExtent)
                        {
                            trgWidth  = width  * pixelSize / maxExtent;
                            trgHeight = height * pixelSize / maxExtent;
                        }
                        if (GdkPixbuf* pixBuf = ::gdk_pixbuf_new_from_file_at_size(filePath.c_str(), trgWidth, trgHeight, nullptr))
                        {
                            ZEN_ON_SCOPE_EXIT(::g_object_unref(pixBuf));
                            return copyToImageHolder(pixBuf);
                        }
                    }
            }

    }
    catch (SysError&) {}

    return ImageHolder();
}

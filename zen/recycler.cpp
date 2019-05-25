// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "recycler.h"
#include "file_access.h"

    #include <sys/stat.h>
    #include <gio/gio.h>
    #include "scope_guard.h"


using namespace zen;




bool zen::recycleOrDeleteIfExists(const Zstring& itemPath) //throw FileError
{
    GFile* file = ::g_file_new_for_path(itemPath.c_str()); //never fails according to docu
    ZEN_ON_SCOPE_EXIT(g_object_unref(file);)

    GError* error = nullptr;
    ZEN_ON_SCOPE_EXIT(if (error) ::g_error_free(error););

    if (!::g_file_trash(file, nullptr, &error))
    {
        const std::optional<ItemType> type = itemStillExists(itemPath); //throw FileError
        if (!type)
            return false;

        const std::wstring errorMsg = replaceCpy(_("Unable to move %x to the recycle bin."), L"%x", fmtPath(itemPath));
        if (!error)
            throw FileError(errorMsg, L"g_file_trash: unknown error."); //user should never see this

        //implement same behavior as in Windows: if recycler is not existing, delete permanently
        if (error->code == G_IO_ERROR_NOT_SUPPORTED)
        {
            if (*type == ItemType::FOLDER)
                removeDirectoryPlainRecursion(itemPath); //throw FileError
            else
                removeFilePlain(itemPath); //throw FileError
            return true;
        }

        throw FileError(errorMsg, formatSystemError(L"g_file_trash", replaceCpy(_("Error Code %x"), L"%x", numberTo<std::wstring>(error->code)), utfTo<std::wstring>(error->message)));
        //g_quark_to_string(error->domain)
    }
    return true;

}


/*
We really need access to a similar function to check whether a directory supports trashing and emit a warning if it does not!

The following function looks perfect, alas it is restricted to local files and to the implementation of GIO only:

    gboolean _g_local_file_has_trash_dir(const char* dirpath, dev_t dir_dev);
    See: http://www.netmite.com/android/mydroid/2.0/external/bluetooth/glib/gio/glocalfileinfo.h

    Just checking for "G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH" is not correct, since we find in
    http://www.netmite.com/android/mydroid/2.0/external/bluetooth/glib/gio/glocalfileinfo.c

            g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH,
                                           writable && parent_info->has_trash_dir);

    => We're NOT interested in whether the specified folder can be trashed, but whether it supports thrashing its child elements! (Only support, not actual write access!)
    This renders G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH useless for this purpose.
*/

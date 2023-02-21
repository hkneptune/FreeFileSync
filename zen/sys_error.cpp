// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "sys_error.h"
    #include <gio/gio.h>

using namespace zen;


namespace
{
std::wstring formatSystemErrorCode(ErrorCode ec)
{
    switch (ec) //pretty much all codes currently used on CentOS 7 and macOS 10.15
    {
            ZEN_CHECK_CASE_FOR_CONSTANT(EPERM);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOENT);
            ZEN_CHECK_CASE_FOR_CONSTANT(ESRCH);
            ZEN_CHECK_CASE_FOR_CONSTANT(EINTR);
            ZEN_CHECK_CASE_FOR_CONSTANT(EIO);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENXIO);
            ZEN_CHECK_CASE_FOR_CONSTANT(E2BIG);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOEXEC);
            ZEN_CHECK_CASE_FOR_CONSTANT(EBADF);
            ZEN_CHECK_CASE_FOR_CONSTANT(ECHILD);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAGAIN);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOMEM);
            ZEN_CHECK_CASE_FOR_CONSTANT(EACCES);
            ZEN_CHECK_CASE_FOR_CONSTANT(EFAULT);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOTBLK);
            ZEN_CHECK_CASE_FOR_CONSTANT(EBUSY);
            ZEN_CHECK_CASE_FOR_CONSTANT(EEXIST);
            ZEN_CHECK_CASE_FOR_CONSTANT(EXDEV);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENODEV);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOTDIR);
            ZEN_CHECK_CASE_FOR_CONSTANT(EISDIR);
            ZEN_CHECK_CASE_FOR_CONSTANT(EINVAL);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENFILE);
            ZEN_CHECK_CASE_FOR_CONSTANT(EMFILE);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOTTY);
            ZEN_CHECK_CASE_FOR_CONSTANT(ETXTBSY);
            ZEN_CHECK_CASE_FOR_CONSTANT(EFBIG);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOSPC);
            ZEN_CHECK_CASE_FOR_CONSTANT(ESPIPE);
            ZEN_CHECK_CASE_FOR_CONSTANT(EROFS);
            ZEN_CHECK_CASE_FOR_CONSTANT(EMLINK);
            ZEN_CHECK_CASE_FOR_CONSTANT(EPIPE);
            ZEN_CHECK_CASE_FOR_CONSTANT(EDOM);
            ZEN_CHECK_CASE_FOR_CONSTANT(ERANGE);
            ZEN_CHECK_CASE_FOR_CONSTANT(EDEADLK);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENAMETOOLONG);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOLCK);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOSYS);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOTEMPTY);
            ZEN_CHECK_CASE_FOR_CONSTANT(ELOOP);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOMSG);
            ZEN_CHECK_CASE_FOR_CONSTANT(EIDRM);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOSTR);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENODATA);
            ZEN_CHECK_CASE_FOR_CONSTANT(ETIME);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOSR);
            ZEN_CHECK_CASE_FOR_CONSTANT(EREMOTE);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOLINK);
            ZEN_CHECK_CASE_FOR_CONSTANT(EPROTO);
            ZEN_CHECK_CASE_FOR_CONSTANT(EMULTIHOP);
            ZEN_CHECK_CASE_FOR_CONSTANT(EBADMSG);
            ZEN_CHECK_CASE_FOR_CONSTANT(EOVERFLOW);
            ZEN_CHECK_CASE_FOR_CONSTANT(EILSEQ);
            ZEN_CHECK_CASE_FOR_CONSTANT(EUSERS);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOTSOCK);
            ZEN_CHECK_CASE_FOR_CONSTANT(EDESTADDRREQ);
            ZEN_CHECK_CASE_FOR_CONSTANT(EMSGSIZE);
            ZEN_CHECK_CASE_FOR_CONSTANT(EPROTOTYPE);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOPROTOOPT);
            ZEN_CHECK_CASE_FOR_CONSTANT(EPROTONOSUPPORT);
            ZEN_CHECK_CASE_FOR_CONSTANT(ESOCKTNOSUPPORT);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOTSUP);
            ZEN_CHECK_CASE_FOR_CONSTANT(EPFNOSUPPORT);
            ZEN_CHECK_CASE_FOR_CONSTANT(EAFNOSUPPORT);
            ZEN_CHECK_CASE_FOR_CONSTANT(EADDRINUSE);
            ZEN_CHECK_CASE_FOR_CONSTANT(EADDRNOTAVAIL);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENETDOWN);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENETUNREACH);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENETRESET);
            ZEN_CHECK_CASE_FOR_CONSTANT(ECONNABORTED);
            ZEN_CHECK_CASE_FOR_CONSTANT(ECONNRESET);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOBUFS);
            ZEN_CHECK_CASE_FOR_CONSTANT(EISCONN);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOTCONN);
            ZEN_CHECK_CASE_FOR_CONSTANT(ESHUTDOWN);
            ZEN_CHECK_CASE_FOR_CONSTANT(ETOOMANYREFS);
            ZEN_CHECK_CASE_FOR_CONSTANT(ETIMEDOUT);
            ZEN_CHECK_CASE_FOR_CONSTANT(ECONNREFUSED);
            ZEN_CHECK_CASE_FOR_CONSTANT(EHOSTDOWN);
            ZEN_CHECK_CASE_FOR_CONSTANT(EHOSTUNREACH);
            ZEN_CHECK_CASE_FOR_CONSTANT(EALREADY);
            ZEN_CHECK_CASE_FOR_CONSTANT(EINPROGRESS);
            ZEN_CHECK_CASE_FOR_CONSTANT(ESTALE);
            ZEN_CHECK_CASE_FOR_CONSTANT(EDQUOT);
            ZEN_CHECK_CASE_FOR_CONSTANT(ECANCELED);
            ZEN_CHECK_CASE_FOR_CONSTANT(EOWNERDEAD);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOTRECOVERABLE);

            ZEN_CHECK_CASE_FOR_CONSTANT(ECHRNG);
            ZEN_CHECK_CASE_FOR_CONSTANT(EL2NSYNC);
            ZEN_CHECK_CASE_FOR_CONSTANT(EL3HLT);
            ZEN_CHECK_CASE_FOR_CONSTANT(EL3RST);
            ZEN_CHECK_CASE_FOR_CONSTANT(ELNRNG);
            ZEN_CHECK_CASE_FOR_CONSTANT(EUNATCH);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOCSI);
            ZEN_CHECK_CASE_FOR_CONSTANT(EL2HLT);
            ZEN_CHECK_CASE_FOR_CONSTANT(EBADE);
            ZEN_CHECK_CASE_FOR_CONSTANT(EBADR);
            ZEN_CHECK_CASE_FOR_CONSTANT(EXFULL);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOANO);
            ZEN_CHECK_CASE_FOR_CONSTANT(EBADRQC);
            ZEN_CHECK_CASE_FOR_CONSTANT(EBADSLT);
            ZEN_CHECK_CASE_FOR_CONSTANT(EBFONT);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENONET);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOPKG);
            ZEN_CHECK_CASE_FOR_CONSTANT(EADV);
            ZEN_CHECK_CASE_FOR_CONSTANT(ESRMNT);
            ZEN_CHECK_CASE_FOR_CONSTANT(ECOMM);
            ZEN_CHECK_CASE_FOR_CONSTANT(EDOTDOT);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOTUNIQ);
            ZEN_CHECK_CASE_FOR_CONSTANT(EBADFD);
            ZEN_CHECK_CASE_FOR_CONSTANT(EREMCHG);
            ZEN_CHECK_CASE_FOR_CONSTANT(ELIBACC);
            ZEN_CHECK_CASE_FOR_CONSTANT(ELIBBAD);
            ZEN_CHECK_CASE_FOR_CONSTANT(ELIBSCN);
            ZEN_CHECK_CASE_FOR_CONSTANT(ELIBMAX);
            ZEN_CHECK_CASE_FOR_CONSTANT(ELIBEXEC);
            ZEN_CHECK_CASE_FOR_CONSTANT(ERESTART);
            ZEN_CHECK_CASE_FOR_CONSTANT(ESTRPIPE);
            ZEN_CHECK_CASE_FOR_CONSTANT(EUCLEAN);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOTNAM);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENAVAIL);
            ZEN_CHECK_CASE_FOR_CONSTANT(EISNAM);
            ZEN_CHECK_CASE_FOR_CONSTANT(EREMOTEIO);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOMEDIUM);
            ZEN_CHECK_CASE_FOR_CONSTANT(EMEDIUMTYPE);
            ZEN_CHECK_CASE_FOR_CONSTANT(ENOKEY);
            ZEN_CHECK_CASE_FOR_CONSTANT(EKEYEXPIRED);
            ZEN_CHECK_CASE_FOR_CONSTANT(EKEYREVOKED);
            ZEN_CHECK_CASE_FOR_CONSTANT(EKEYREJECTED);
            ZEN_CHECK_CASE_FOR_CONSTANT(ERFKILL);
            ZEN_CHECK_CASE_FOR_CONSTANT(EHWPOISON);
        default:
            return replaceCpy(_("Error code %x"), L"%x", numberTo<std::wstring>(ec));
    }
}
}


std::wstring zen::formatGlibError(const std::string& functionName, GError* error)
{
    if (!error)
        return formatSystemError(functionName, L"", _("Error description not available.") + L" null GError");

    if (error->domain == G_FILE_ERROR) //"values corresponding to errno codes"
        return formatSystemError(functionName, error->code);

    std::wstring errorCode;
    if (error->domain == G_IO_ERROR)
        errorCode = [&]() -> std::wstring
    {
        switch (error->code) //GIOErrorEnum: https://gitlab.gnome.org/GNOME/glib/-/blob/master/gio/gioenums.h#L530
        {
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_FAILED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_NOT_FOUND);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_EXISTS);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_IS_DIRECTORY);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_NOT_DIRECTORY);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_NOT_EMPTY);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_NOT_REGULAR_FILE);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_NOT_SYMBOLIC_LINK);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_NOT_MOUNTABLE_FILE);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_FILENAME_TOO_LONG);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_INVALID_FILENAME);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_TOO_MANY_LINKS);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_NO_SPACE);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_INVALID_ARGUMENT);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_PERMISSION_DENIED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_NOT_SUPPORTED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_NOT_MOUNTED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_ALREADY_MOUNTED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_CLOSED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_CANCELLED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_PENDING);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_READ_ONLY);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_CANT_CREATE_BACKUP);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_WRONG_ETAG);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_TIMED_OUT);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_WOULD_RECURSE);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_BUSY);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_WOULD_BLOCK);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_HOST_NOT_FOUND);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_WOULD_MERGE);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_FAILED_HANDLED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_TOO_MANY_OPEN_FILES);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_NOT_INITIALIZED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_ADDRESS_IN_USE);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_PARTIAL_INPUT);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_INVALID_DATA);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_DBUS_ERROR);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_HOST_UNREACHABLE);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_NETWORK_UNREACHABLE);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_CONNECTION_REFUSED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_PROXY_FAILED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_PROXY_AUTH_FAILED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_PROXY_NEED_AUTH);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_PROXY_NOT_ALLOWED);
#ifndef GLIB_CHECK_VERSION //e.g Debian 8 (GLib 2.42)     CentOS 7 (GLib 2.56)
#error Where is GLib?
#endif
#if GLIB_CHECK_VERSION(2, 44, 0)
                static_assert(G_IO_ERROR_BROKEN_PIPE == G_IO_ERROR_CONNECTION_CLOSED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_CONNECTION_CLOSED);
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_NOT_CONNECTED);
#endif
#if GLIB_CHECK_VERSION(2, 48, 0)
                ZEN_CHECK_CASE_FOR_CONSTANT(G_IO_ERROR_MESSAGE_TOO_LARGE);
#endif
            default:
                return replaceCpy<std::wstring>(L"GIO error %x", L"%x", numberTo<std::wstring>(error->code));
        }
    }();
    else
    {
        //g-file-error-quark => g-file-error
        //g-io-error-quark   => g-io-error
        std::wstring domain = utfTo<std::wstring>(::g_quark_to_string(error->domain)); //e.g. "g-io-error-quark"
        if (endsWith(domain, L"-quark"))
            domain = beforeLast(domain, L"-", IfNotFoundReturn::none);

        errorCode = domain + L' ' + numberTo<std::wstring>(error->code); //e.g. "g-io-error 15"
    }

    const std::wstring errorMsg = utfTo<std::wstring>(error->message); //e.g. "Unable to find or create trash directory for file.txt"

    return formatSystemError(functionName, errorCode, errorMsg);
}



std::wstring zen::getSystemErrorDescription(ErrorCode ec) //return empty string on error
{
    const ErrorCode ecCurrent = getLastError(); //not necessarily == ec
    ZEN_ON_SCOPE_EXIT(errno = ecCurrent);

    std::wstring errorMsg = utfTo<std::wstring>(::g_strerror(ec)); //... vs strerror(): "marginally improves thread safety, and marginally improves consistency"

    trim(errorMsg); //Windows messages seem to end with a space...
    return errorMsg;
}


std::wstring zen::formatSystemError(const std::string& functionName, ErrorCode ec)
{
    return formatSystemError(functionName, formatSystemErrorCode(ec), getSystemErrorDescription(ec));
}


std::wstring zen::formatSystemError(const std::string& functionName, const std::wstring& errorCode, const std::wstring& errorMsg)
{
    std::wstring output = trimCpy(errorCode);

    const std::wstring errorMsgFmt = trimCpy(errorMsg);
    if (!output.empty() && !errorMsgFmt.empty())
        output += L": ";

    output += errorMsgFmt;

    if (!functionName.empty())
        output += L" [" + utfTo<std::wstring>(functionName) + L']';

    return trimCpy(output);
}

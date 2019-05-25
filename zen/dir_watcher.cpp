// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "dir_watcher.h"
#include <algorithm>
#include <set>
#include "thread.h"
#include "scope_guard.h"
#include "basic_math.h"

    #include <map>
    #include <sys/inotify.h>
    #include <fcntl.h> //fcntl
    #include <unistd.h> //close
    #include <limits.h> //NAME_MAX
    #include "file_traverser.h"


using namespace zen;


struct DirWatcher::Impl
{
    int notifDescr = 0;
    std::map<int, Zstring> watchedPaths; //watch descriptor and (sub-)directory paths -> owned by "notifDescr"
};


DirWatcher::DirWatcher(const Zstring& dirPath) : //throw FileError
    baseDirPath_(dirPath),
    pimpl_(std::make_unique<Impl>())
{
    //get all subdirectories
    std::vector<Zstring> fullFolderList { baseDirPath_ };
    {
        std::function<void (const Zstring& path)> traverse;

        traverse = [&traverse, &fullFolderList](const Zstring& path)
        {
            traverseFolder(path, nullptr,
            [&](const FolderInfo& fi ) { fullFolderList.push_back(fi.fullPath); traverse(fi.fullPath); },
            nullptr, //don't traverse into symlinks (analog to windows build)
            [&](const std::wstring& errorMsg) { throw FileError(errorMsg); });
        };

        traverse(baseDirPath_);
    }

    //init
    pimpl_->notifDescr  = ::inotify_init();
    if (pimpl_->notifDescr == -1)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtPath(baseDirPath_)), L"inotify_init");

    ZEN_ON_SCOPE_FAIL( ::close(pimpl_->notifDescr); );

    //set non-blocking mode
    bool initSuccess = false;
    {
        int flags = ::fcntl(pimpl_->notifDescr, F_GETFL);
        if (flags != -1)
            initSuccess = ::fcntl(pimpl_->notifDescr, F_SETFL, flags | O_NONBLOCK) != -1;
    }
    if (!initSuccess)
        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtPath(baseDirPath_)), L"fcntl");

    //add watches
    for (const Zstring& subDirPath : fullFolderList)
    {
        int wd = ::inotify_add_watch(pimpl_->notifDescr, subDirPath.c_str(),
                                     IN_ONLYDIR     | //"Only watch pathname if it is a directory."
                                     IN_DONT_FOLLOW | //don't follow symbolic links
                                     IN_CREATE      |
                                     IN_MODIFY      |
                                     IN_CLOSE_WRITE |
                                     IN_DELETE      |
                                     IN_DELETE_SELF |
                                     IN_MOVED_FROM  |
                                     IN_MOVED_TO    |
                                     IN_MOVE_SELF);
        if (wd == -1)
        {
            const ErrorCode ec = getLastError(); //copy before directly/indirectly making other system calls!
            if (ec == ENOSPC) //fix misleading system message "No space left on device"
                throw FileError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtPath(subDirPath)),
                                formatSystemError(L"inotify_add_watch", numberTo<std::wstring>(ec), L"The user limit on the total number of inotify watches was reached or the kernel failed to allocate a needed resource."));

            throw FileError(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtPath(subDirPath)), formatSystemError(L"inotify_add_watch", ec));
        }

        pimpl_->watchedPaths.emplace(wd, subDirPath);
    }
}


DirWatcher::~DirWatcher()
{
    ::close(pimpl_->notifDescr); //associated watches are removed automatically!
}


std::vector<DirWatcher::Entry> DirWatcher::getChanges(const std::function<void()>& requestUiRefresh, std::chrono::milliseconds cbInterval) //throw FileError
{
    std::vector<std::byte> buffer(512 * (sizeof(struct ::inotify_event) + NAME_MAX + 1));

    ssize_t bytesRead = 0;
    do
    {
        //non-blocking call, see O_NONBLOCK
        bytesRead = ::read(pimpl_->notifDescr, &buffer[0], buffer.size());
    }
    while (bytesRead < 0 && errno == EINTR); //"Interrupted function call; When this happens, you should try the call again."

    if (bytesRead < 0)
    {
        if (errno == EAGAIN)  //this error is ignored in all inotify wrappers I found
            return std::vector<Entry>();

        THROW_LAST_FILE_ERROR(replaceCpy(_("Cannot monitor directory %x."), L"%x", fmtPath(baseDirPath_)), L"read");
    }

    std::vector<Entry> output;

    ssize_t bytePos = 0;
    while (bytePos < bytesRead)
    {
        struct ::inotify_event& evt = reinterpret_cast<struct ::inotify_event&>(buffer[bytePos]);

        if (evt.len != 0) //exclude case: deletion of "self", already reported by parent directory watch
        {
            auto it = pimpl_->watchedPaths.find(evt.wd);
            if (it != pimpl_->watchedPaths.end())
            {
                //Note: evt.len is NOT the size of the evt.name c-string, but the array size including all padding 0 characters!
                //It may be even 0 in which case evt.name must not be used!
                const Zstring itemPath = appendSeparator(it->second) + evt.name;

                if ((evt.mask & IN_CREATE) ||
                    (evt.mask & IN_MOVED_TO))
                    output.push_back({ ACTION_CREATE, itemPath });
                else if ((evt.mask & IN_MODIFY) ||
                         (evt.mask & IN_CLOSE_WRITE))
                    output.push_back({ ACTION_UPDATE, itemPath });
                else if ((evt.mask & IN_DELETE     ) ||
                         (evt.mask & IN_DELETE_SELF) ||
                         (evt.mask & IN_MOVE_SELF  ) ||
                         (evt.mask & IN_MOVED_FROM))
                    output.push_back({ ACTION_DELETE, itemPath });
            }
        }
        bytePos += sizeof(struct ::inotify_event) + evt.len;
    }

    return output;
}


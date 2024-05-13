// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef DIR_WATCHER_348577025748023458
#define DIR_WATCHER_348577025748023458

#include <vector>
#include <chrono>
#include <functional>
#include "file_error.h"


namespace zen
{
//Windows: ReadDirectoryChangesW https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw
//Linux:   inotify               https://linux.die.net/man/7/inotify
//macOS:   kqueue                https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man2/kqueue.2.html

//watch directory including subdirectories
/*
!Note handling of directories!:
    Windows: removal of top watched directory is NOT notified when watching the dir handle, e.g. brute force usb stick removal,
             (watchting for GUID_DEVINTERFACE_WPD OTOH works fine!)
             however manual unmount IS notified (e.g. USB stick removal, then re-insert), but watching is stopped!
             Renaming of top watched directory handled incorrectly: Not notified(!) + additional changes in subfolders
             now do report FILE_ACTION_MODIFIED for directory (check that should prevent this fails!)

    Linux: newly added subdirectories are reported but not automatically added for watching! -> reset Dirwatcher!
           removal of base directory is NOT notified!

    macOS: everything works as expected; renaming of base directory is also detected

    Overcome all issues portably: check existence of top watched directory externally + reinstall watch after changes in directory structure (added directories) are detected
*/
class DirWatcher
{
public:
    explicit DirWatcher(const Zstring& dirPath); //throw FileError
    ~DirWatcher();

    enum class ChangeType
    {
        create, //
        update, //informal: use for debugging/logging only!
        remove, //
        baseFolderUnavailable, //1. not existing or 2. can't access
    };

    struct Change
    {
        ChangeType type = ChangeType::create;
        Zstring itemPath;
    };

    //extract accumulated changes since last call
    std::vector<Change> fetchChanges(const std::function<void()>& requestUiUpdate, std::chrono::milliseconds cbInterval); //throw FileError

private:
    DirWatcher           (const DirWatcher&) = delete;
    DirWatcher& operator=(const DirWatcher&) = delete;

    const Zstring baseDirPath_;

    struct Impl;
    const std::unique_ptr<Impl> pimpl_;
};
}

#endif

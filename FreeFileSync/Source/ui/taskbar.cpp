// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "taskbar.h"

#if   defined HAVE_UBUNTU_UNITY
    #include <unity/unity/unity.h>

#endif

using namespace zen;
using namespace fff;


#if   defined HAVE_UBUNTU_UNITY //Ubuntu unity
namespace
{
const char FFS_DESKTOP_FILE[] = "freefilesync.desktop";
}

class Taskbar::Impl //throw (TaskbarNotAvailable)
{
public:
    Impl(const wxFrame& window) :
        tbEntry(unity_launcher_entry_get_for_desktop_id(FFS_DESKTOP_FILE))
        //tbEntry(unity_launcher_entry_get_for_app_uri("application://freefilesync.desktop"))
    {
        if (!tbEntry)
            throw TaskbarNotAvailable();
    }

    ~Impl() { setStatus(STATUS_INDETERMINATE); } //it seems UnityLauncherEntry* does not need destruction

    void setStatus(Status status)
    {
        switch (status)
        {
            case Taskbar::STATUS_ERROR:
                unity_launcher_entry_set_urgent(tbEntry, true);
                break;

            case Taskbar::STATUS_INDETERMINATE:
                unity_launcher_entry_set_urgent(tbEntry, false);
                unity_launcher_entry_set_progress_visible(tbEntry, false);
                break;

            case Taskbar::STATUS_NORMAL:
                unity_launcher_entry_set_urgent(tbEntry, false);
                unity_launcher_entry_set_progress_visible(tbEntry, true);
                break;

            case Taskbar::STATUS_PAUSED:
                unity_launcher_entry_set_urgent(tbEntry, false);
                break;
        }
    }

    void setProgress(double fraction)
    {
        unity_launcher_entry_set_progress(tbEntry, fraction);
    }

private:
    UnityLauncherEntry* tbEntry;
};

#else //no taskbar support
class Taskbar::Impl
{
public:
    Impl(const wxFrame& window) { throw TaskbarNotAvailable(); }
    void setStatus(Status status) {}
    void setProgress(double fraction) {}
};
#endif

//########################################################################################################

Taskbar::Taskbar(const wxFrame& window) : pimpl_(std::make_unique<Impl>(window)) {} //throw TaskbarNotAvailable
Taskbar::~Taskbar() {}

void Taskbar::setStatus(Status status) { pimpl_->setStatus(status); }
void Taskbar::setProgress(double fraction) { pimpl_->setProgress(fraction); }

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "process_priority.h"
#include "i18n.h"


using namespace zen;


struct PreventStandby::Impl {};
PreventStandby::PreventStandby() {}
PreventStandby::~PreventStandby() {}

//solution for GNOME?: https://people.gnome.org/~mccann/gnome-session/docs/gnome-session.html#org.gnome.SessionManager.Inhibit

struct ScheduleForBackgroundProcessing::Impl {};
ScheduleForBackgroundProcessing::ScheduleForBackgroundProcessing() {}
ScheduleForBackgroundProcessing::~ScheduleForBackgroundProcessing() {}

/*
struct ScheduleForBackgroundProcessing
{
    - required functions ioprio_get/ioprio_set are not part of glibc: https://linux.die.net/man/2/ioprio_set
    - and probably never will: https://sourceware.org/bugzilla/show_bug.cgi?id=4464
    - /usr/include/linux/ioprio.h not available on Ubuntu, so we can't use it instead

    ScheduleForBackgroundProcessing() : oldIoPrio(getIoPriority(IOPRIO_WHO_PROCESS, ::getpid()))
    {
        if (oldIoPrio != -1)
            setIoPriority(::getpid(), IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 0));
    }
    ~ScheduleForBackgroundProcessing()
    {
        if (oldIoPrio != -1)
            setIoPriority(::getpid(), oldIoPrio);
    }

private:
    static int getIoPriority(pid_t pid)
    {
        return ::syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, pid);
    }
    static int setIoPriority(pid_t pid, int ioprio)
    {
        return ::syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, pid, ioprio);
    }

    const int oldIoPrio;
};
*/

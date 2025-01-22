// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "process_priority.h"

    #include <sys/resource.h> //setpriority

using namespace zen;


namespace
{
#if 0
//https://linux.die.net/man/2/getpriority
//CPU priority from highest to lowest range: [-NZERO, NZERO -1]  usually: [-20, 19]
enum //with values from CentOS 7
{
    CPU_PRIO_VERYHIGH = -NZERO,
    CPU_PRIO_HIGH     = -5,
    CPU_PRIO_NORMAL   = 0,
    CPU_PRIO_LOW      = 5,
    CPU_PRIO_VERYLOW  = NZERO - 1,
};

int getCpuPriority() //throw SysError
{
    errno = 0;
    const int prio = getpriority(PRIO_PROCESS, 0 /* = the calling process */);
    if (prio == -1 && errno != 0) //"can legitimately return the value -1"
        THROW_LAST_SYS_ERROR("getpriority");
    return prio;
}


//lowering is allowed, but increasing CPU prio requires admin rights >:(
void setCpuPriority(int prio) //throw SysError
{
    if (setpriority(PRIO_PROCESS, 0 /* = the calling process */, prio) != 0)
        THROW_LAST_SYS_ERROR("setpriority(" + numberTo<std::string>(prio) + ')');
}
#endif
//---------------------------------------------------------------------------------------------------

//- required functions ioprio_get/ioprio_set are not part of glibc: https://linux.die.net/man/2/ioprio_set
//- and probably never will: https://sourceware.org/bugzilla/show_bug.cgi?id=4464
//https://github.com/torvalds/linux/blob/master/include/uapi/linux/ioprio.h
#define IOPRIO_CLASS_SHIFT  13

#define IOPRIO_PRIO_VALUE(prioclass, priolevel)         \
    (((prioclass) << IOPRIO_CLASS_SHIFT) | (priolevel))

#define IOPRIO_NORM 4

enum
{
    IOPRIO_WHO_PROCESS = 1,
    IOPRIO_WHO_PGRP,
    IOPRIO_WHO_USER,
};

enum
{
    IOPRIO_CLASS_NONE = 0,
    IOPRIO_CLASS_RT   = 1,
    IOPRIO_CLASS_BE   = 2,
    IOPRIO_CLASS_IDLE = 3,
};


int getIoPriority() //throw SysError
{
    const int rv = ::syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, ::getpid());
    if (rv == -1)
        THROW_LAST_SYS_ERROR("ioprio_get");

    //fix Linux kernel fuck up: bogus system default value
    if (rv == IOPRIO_PRIO_VALUE(IOPRIO_CLASS_NONE, IOPRIO_NORM))
        return IOPRIO_PRIO_VALUE(IOPRIO_CLASS_BE, IOPRIO_NORM);

    return rv;
}


void setIoPriority(int ioPrio) //throw SysError
{
    if (::syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, ::getpid(), ioPrio) != 0)
        THROW_LAST_SYS_ERROR("ioprio_set(0x" + printNumber<std::string>("%x", static_cast<unsigned int>(ioPrio)) + ')');
}
}


struct SetProcessPriority::Impl
{
    std::optional<int> oldIoPrio;
};


SetProcessPriority::SetProcessPriority(ProcessPriority prio) : //throw FileError
    pimpl_(new Impl)
{
    if (prio == ProcessPriority::background)
        try
        {
            pimpl_->oldIoPrio = getIoPriority(); //throw SysError

            setIoPriority(IOPRIO_PRIO_VALUE(IOPRIO_CLASS_BE, 6 /*0 (highest) to 7 (lowest)*/)); //throw SysError
            //maybe even IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 0)  ? nope: "only served when no one else is using the disk"
        }
        catch (const SysError& e) { throw FileError(_("Cannot change process I/O priorities."), e.toString()); }
}


SetProcessPriority::~SetProcessPriority()
{
    if (pimpl_->oldIoPrio)
        try
        {
            setIoPriority(*pimpl_->oldIoPrio); //throw SysError
        }
        catch (const SysError& e) { logExtraError(_("Cannot change process I/O priorities.") + L"\n\n" + e.toString()); }
}

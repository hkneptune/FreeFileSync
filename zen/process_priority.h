// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PROCESS_PRIORITY_H_83421759082143245
#define PROCESS_PRIORITY_H_83421759082143245

#include <memory>
#include "file_error.h"


namespace zen
{
//signal a "busy" state to the operating system
class PreventStandby
{
public:
    PreventStandby(); //throw FileError
    ~PreventStandby();
private:
    struct Impl;
    const std::unique_ptr<Impl> pimpl_;
};

//lower CPU and file I/O priorities
class ScheduleForBackgroundProcessing
{
public:
    ScheduleForBackgroundProcessing(); //throw FileError
    ~ScheduleForBackgroundProcessing();
private:
    struct Impl;
    const std::unique_ptr<Impl> pimpl_;
};
}

#endif //PROCESS_PRIORITY_H_83421759082143245

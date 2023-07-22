// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef TASKBAR_H_98170845709124456
#define TASKBAR_H_98170845709124456

#include <memory>
#include <wx/window.h>


namespace zen
{
class TaskbarNotAvailable {};

class Taskbar
{
public:
    Taskbar(wxWindow* window); //throw TaskbarNotAvailable
    ~Taskbar();

    enum class Status
    {
        normal,
        indeterminate,
        warning,
        error,
        paused,
    };

    void setStatus(Status status); //noexcept
    void setProgress(double fraction); //between [0, 1]; noexcept

private:
    class Impl;
    const std::unique_ptr<Impl> pimpl_;
};
}

#endif //TASKBAR_H_98170845709124456

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "taskbar.h"


using namespace zen;
using namespace fff;


class Taskbar::Impl
{
public:
    Impl(const wxFrame& window) { throw TaskbarNotAvailable(); }
    void setStatus(Status status) {}
    void setProgress(double fraction) {}
};

//########################################################################################################

Taskbar::Taskbar(const wxFrame& window) : pimpl_(std::make_unique<Impl>(window)) {} //throw TaskbarNotAvailable
Taskbar::~Taskbar() {}

void Taskbar::setStatus(Status status) { pimpl_->setStatus(status); }
void Taskbar::setProgress(double fraction) { pimpl_->setProgress(fraction); }

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "taskbar.h"


using namespace zen;


class Taskbar::Impl {};

Taskbar::Taskbar(wxWindow* window) { throw TaskbarNotAvailable(); }
Taskbar::~Taskbar() {}

void Taskbar::setStatus(Status status) {}
void Taskbar::setProgress(double fraction) {}

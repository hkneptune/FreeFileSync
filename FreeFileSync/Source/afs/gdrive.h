// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FS_GDRIVE_9238425018342701356
#define FS_GDRIVE_9238425018342701356

#include "abstract.h"

namespace fff
{
bool  acceptsItemPathPhraseGdrive(const Zstring& itemPathPhrase); //noexcept
AbstractPath createItemPathGdrive(const Zstring& itemPathPhrase); //noexcept

void googleDriveInit(const Zstring& configDirPath, //directory to store Google-Drive-specific files
                     const Zstring& caCertFilePath); //cacert.pem
void googleDriveTeardown();

//-------------------------------------------------------

Zstring /*Google user email*/ googleAddUser(const std::function<void()>& updateGui /*throw X*/); //throw FileError, X
void                          googleRemoveUser(const Zstring& googleUserEmail); //throw FileError
std::vector<Zstring> /*Google user email*/ googleListConnectedUsers(); //throw FileError

AfsDevice condenseToGdriveDevice(const Zstring& userEmail); //noexcept; potentially messy user input
Zstring extractGdriveEmail(const AfsDevice& afsDevice); //noexcept
}

#endif //FS_GDRIVE_9238425018342701356

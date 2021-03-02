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
bool         acceptsItemPathPhraseGdrive(const Zstring& itemPathPhrase); //noexcept
AbstractPath createItemPathGdrive       (const Zstring& itemPathPhrase); //noexcept

void gdriveInit(const Zstring& configDirPath,   //directory to store Google-Drive-specific files
                const Zstring& caCertFilePath); //cacert.pem
void gdriveTeardown();

//-------------------------------------------------------

std::string /*account email*/ gdriveAddUser(const std::function<void()>& updateGui /*throw X*/); //throw FileError, X
void                          gdriveRemoveUser(const std::string& accountEmail);                 //throw FileError

std::vector<std::string /*account email*/> gdriveListAccounts(); //throw FileError
std::vector<Zstring /*sharedDriveName*/> gdriveListSharedDrives(const std::string& accountEmail); //throw FileError

struct GdriveLogin
{
    std::string email;
    Zstring sharedDriveName; //empty for "My Drive"
};
AfsDevice   condenseToGdriveDevice(const GdriveLogin& login); //noexcept; potentially messy user input
GdriveLogin extractGdriveLogin(const AfsDevice& afsDevice);   //noexcept

//return empty, if not a Google Drive path
Zstring getGoogleDriveFolderUrl(const AbstractPath& folderPath); //throw FileError
}

#endif //FS_GDRIVE_9238425018342701356

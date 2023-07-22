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

//caveat: gdriveAddUser() blocks indefinitely if user doesn't log in with Google! timeoutSec is only regarding HTTP requests
std::string /*account email*/ gdriveAddUser(const std::function<void()>& updateGui /*throw X*/, int timeoutSec); //throw FileError, X
void                          gdriveRemoveUser(const std::string& accountEmail, int timeoutSec);                 //throw FileError

std::vector<std::string /*account email*/> gdriveListAccounts(); //throw FileError
std::vector<Zstring /*locationName*/> gdriveListLocations(const std::string& accountEmail, int timeoutSec); //throw FileError

struct GdriveLogin
{
    std::string email;
    Zstring locationName; //empty for "My Drive"; can be a shared drive or starred folder name
    int timeoutSec = 10; //Gdrive can "hang" for 20 seconds when "scanning for viruses": https://freefilesync.org/forum/viewtopic.php?t=9116
};

AfsDevice condenseToGdriveDevice(const GdriveLogin& login); //noexcept; potentially messy user input
GdriveLogin extractGdriveLogin(const AfsDevice& afsDevice); //noexcept

//return empty, if not a Google Drive path
Zstring getGoogleDriveFolderUrl(const AbstractPath& folderPath); //throw FileError
}

#endif //FS_GDRIVE_9238425018342701356

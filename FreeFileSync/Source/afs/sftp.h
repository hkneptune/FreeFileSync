// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SFTP_H_5392187498172458215426
#define SFTP_H_5392187498172458215426

#include "abstract.h"


namespace fff
{
bool  acceptsItemPathPhraseSftp(const Zstring& itemPathPhrase); //noexcept
AbstractPath createItemPathSftp(const Zstring& itemPathPhrase); //noexcept

//-------------------------------------------------------
enum class SftpAuthType
{
    password,
    keyFile,
    agent,
};
//-------------------------------------------------------

void sftpInit();
void sftpTeardown();

struct SftpLoginInfo
{
    Zstring server;
    int     port = 0; // > 0 if set
    Zstring username;

    SftpAuthType authType = SftpAuthType::password;
    Zstring password;           //authType == password or keyFile
    Zstring privateKeyFilePath; //authType == keyFile: use PEM-encoded private key (protected by password) for authentication

    //other settings not specific to SFTP session:
    int timeoutSec = 15;                    //valid range: [1, inf)
    int traverserChannelsPerConnection = 1; //valid range: [1, inf)
};


struct SftpPathInfo
{
    SftpLoginInfo login;
    AfsPath afsPath; //server-relative path
};
SftpPathInfo getResolvedSftpPath(const Zstring& folderPathPhrase); //noexcept

//expects (potentially messy) user input:
Zstring condenseToSftpFolderPathPhrase(const SftpLoginInfo& login, const Zstring& relPath); //noexcept

int getServerMaxChannelsPerConnection(const SftpLoginInfo& login); //throw FileError

AfsPath getSftpHomePath(const SftpLoginInfo& login); //throw FileError
}

#endif //SFTP_H_5392187498172458215426

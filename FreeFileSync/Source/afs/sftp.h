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

void sftpInit();
void sftpTeardown();

//-------------------------------------------------------

enum class SftpAuthType
{
    password,
    keyFile,
    agent,
};

const int DEFAULT_PORT_SFTP = 22;
//SFTP default port: 22, see %WINDIR%\system32\drivers\etc\services
//=> we could use the "ssh" alias, but let's be explicit

struct SftpLogin
{
    Zstring server;
    int portCfg = 0; //use if > 0, DEFAULT_PORT_SFTP otherwise
    Zstring username;
    SftpAuthType authType = SftpAuthType::password;
    std::optional<Zstring> password = Zstr(""); //authType == password or keyFile: none given => prompt during AFS::authenticateAccess()
    Zstring privateKeyFilePath;                 //authType == keyFile: use PEM-encoded private key (protected by password) for authentication
    bool allowZlib = false;
    //other settings not specific to SFTP session:
    int timeoutSec = 10;                    //valid range: [1, inf)
    int traverserChannelsPerConnection = 1; //valid range: [1, inf)
};
AfsDevice condenseToSftpDevice(const SftpLogin& login); //noexcept; potentially messy user input
SftpLogin extractSftpLogin(const AfsDevice& afsDevice); //noexcept

int getServerMaxChannelsPerConnection(const SftpLogin& login); //throw FileError

AfsPath getSftpHomePath(const SftpLogin& login); //throw FileError
}

#endif //SFTP_H_5392187498172458215426

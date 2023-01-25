// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FTP_H_745895742383425326568678
#define FTP_H_745895742383425326568678

#include "abstract.h"


namespace fff
{
bool  acceptsItemPathPhraseFtp(const Zstring& itemPathPhrase); //noexcept
AbstractPath createItemPathFtp(const Zstring& itemPathPhrase); //noexcept

void ftpInit();
void ftpTeardown();

//-------------------------------------------------------

const int DEFAULT_PORT_FTP = 21; //TLS enabled? => same for explicit FTP, but *implicit* FTP uses port 990

struct FtpLogin
{
    Zstring server;
    int portCfg = 0; //use if > 0, DEFAULT_PORT_FTP otherwise
    Zstring username;
    std::optional<Zstring> password = Zstr(""); //none given => prompt during AFS::authenticateAccess()
    bool useTls = false;
    //other settings not specific to FTP session:
    int timeoutSec = 10;
};
AfsDevice condenseToFtpDevice(const FtpLogin& login); //noexcept; potentially messy user input
FtpLogin extractFtpLogin(const AfsDevice& afsDevice); //noexcept

AfsPath getFtpHomePath(const FtpLogin& login); //throw FileError
}

#endif //FTP_H_745895742383425326568678

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef INIT_CURL_LIBSSH2_H_4570285702375915765
#define INIT_CURL_LIBSSH2_H_4570285702375915765

#include <memory>
#include <zen/globals.h>


namespace zen
{
//(S)FTP initialization/shutdown dance:

//1. create "Global<UniSessionCounter> globalSftpSessionCount(createUniSessionCounter());" to have a waitable counter of existing (S)FTP sessions
struct UniSessionCounter
{
    UniSessionCounter();
    ~UniSessionCounter();

    class Impl;
    const std::unique_ptr<Impl> pimpl;
};
std::unique_ptr<UniSessionCounter> createUniSessionCounter();


//2. count number of existing (S)FTP sessions => tie to (S)FTP session instances!
class UniCounterCookie;
std::shared_ptr<UniCounterCookie> getLibsshCurlUnifiedInitCookie(Global<UniSessionCounter>& globalSftpSessionCount); //throw SysError


//3. Create static "UniInitializer globalInitSftp(*globalSftpSessionCount.get());" instance *before* constructing objects like "SftpSessionManager"
// => ~SftpSessionManager will run first and all remaining sessions are on non-main threads => can be waited on in ~UniInitializer
class UniInitializer
{
public:
    UniInitializer(UniSessionCounter& sessionCount);
    ~UniInitializer();

private:
    UniInitializer           (const UniInitializer&) = delete;
    UniInitializer& operator=(const UniInitializer&) = delete;

    UniSessionCounter& sessionCount_;
};
}

#endif //INIT_CURL_LIBSSH2_H_4570285702375915765

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "concrete.h"
#include "native.h"
#include "ftp.h"
#include "sftp.h"
#include "gdrive.h"

using namespace fff;
using namespace zen;


void fff::initAfs(const AfsConfig& cfg)
{
    ftpInit();
    sftpInit();
    gdriveInit(appendPath(cfg.configDirPath,   Zstr("GoogleDrive")),
               appendPath(cfg.resourceDirPath, Zstr("cacert.pem")));
}


void fff::teardownAfs()
{
    gdriveTeardown();
    sftpTeardown();
    ftpTeardown();
}


AbstractPath fff::getNullPath()
{
    return createItemPathNativeNoFormatting(Zstring());
}


AbstractPath fff::createAbstractPath(const Zstring& itemPathPhrase) //noexcept
{
    //greedy: try native evaluation first
    if (acceptsItemPathPhraseNative(itemPathPhrase)) //noexcept
        return createItemPathNative(itemPathPhrase); //noexcept

    //then the rest:
    if (acceptsItemPathPhraseFtp(itemPathPhrase)) //noexcept
        return createItemPathFtp(itemPathPhrase); //noexcept

    if (acceptsItemPathPhraseSftp(itemPathPhrase)) //noexcept
        return createItemPathSftp(itemPathPhrase); //noexcept

    if (acceptsItemPathPhraseGdrive(itemPathPhrase)) //noexcept
        return createItemPathGdrive(itemPathPhrase); //noexcept


    //no idea? => native!
    return createItemPathNative(itemPathPhrase);
}

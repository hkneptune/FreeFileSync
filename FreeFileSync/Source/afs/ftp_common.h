// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FTP_COMMON_H_92889457091324321454
#define FTP_COMMON_H_92889457091324321454

#include <zen/base64.h>
#include "abstract.h"


namespace fff
{
inline
Zstring encodePasswordBase64(const ZstringView pass)
{
    using namespace zen;
    return utfTo<Zstring>(stringEncodeBase64(utfTo<std::string>(pass))); //nothrow
}


inline
Zstring decodePasswordBase64(const ZstringView pass)
{
    using namespace zen;
    return utfTo<Zstring>(stringDecodeBase64(utfTo<std::string>(pass))); //nothrow
}


//according to the SFTP path syntax, the username must not contain raw @ and :
//-> we don't need a full urlencode!
inline
Zstring encodeFtpUsername(Zstring name)
{
    using namespace zen;
    replace(name, Zstr('%'), Zstr("%25")); //first!
    replace(name, Zstr('@'), Zstr("%40"));
    replace(name, Zstr(':'), Zstr("%3A"));
    return name;
}


inline
Zstring decodeFtpUsername(Zstring name)
{
    using namespace zen;
    replace(name, Zstr("%40"), Zstr('@'));
    replace(name, Zstr("%3A"), Zstr(':'));
    replace(name, Zstr("%3a"), Zstr(':'));
    replace(name, Zstr("%25"), Zstr('%')); //last!
    return name;
}


//(S)FTP path relative to server root using Unix path separators and with leading slash
inline
Zstring getServerRelPath(const AfsPath& itemPath)
{
    using namespace zen;
    if constexpr (FILE_NAME_SEPARATOR != Zstr('/' ))
        return Zstr('/') + replaceCpy(itemPath.value, FILE_NAME_SEPARATOR, Zstr('/'));
    else
        return Zstr('/') + itemPath.value;
}
}

#endif //FTP_COMMON_H_92889457091324321454

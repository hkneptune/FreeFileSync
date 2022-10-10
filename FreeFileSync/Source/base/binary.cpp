// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "binary.h"

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


bool fff::filesHaveSameContent(const AbstractPath& filePath1, const AbstractPath& filePath2, const IoCallback& notifyUnbufferedIO /*throw X*/) //throw FileError, X
{
    int64_t totalBytesNotified = 0;
    IoCallback /*[!] as expected by InputStream::tryRead()*/ notifyIoDiv = IOCallbackDivider(notifyUnbufferedIO, totalBytesNotified);

    const std::unique_ptr<AFS::InputStream> stream1 = AFS::getInputStream(filePath1); //throw FileError
    const std::unique_ptr<AFS::InputStream> stream2 = AFS::getInputStream(filePath2); //

    const size_t blockSize1 = stream1->getBlockSize(); //throw FileError
    const size_t blockSize2 = stream2->getBlockSize(); //

    const size_t bufCapacity = blockSize2 - 1 + blockSize1 + blockSize2;

    const std::unique_ptr<std::byte[]> buf(new std::byte[bufCapacity]);

    std::byte* const buf1 = buf.get() + blockSize2; //capacity: blockSize2 - 1 + blockSize1
    std::byte* const buf2 = buf.get();              //capacity: blockSize2

    size_t buf1PosEnd = 0;
    for (;;)
    {
        const size_t bytesRead1 = stream1->tryRead(buf1 + buf1PosEnd, blockSize1, notifyIoDiv); //throw FileError, X; may return short; only 0 means EOF

        if (bytesRead1 == 0) //end of file
        {
            size_t buf1Pos = 0;
            while (buf1Pos < buf1PosEnd)
            {
                const size_t bytesRead2 = stream2->tryRead(buf2, blockSize2, notifyIoDiv); //throw FileError, X; may return short; only 0 means EOF

                if (bytesRead2 == 0 ||//end of file
                    bytesRead2 > buf1PosEnd - buf1Pos)
                    return false;

                if (std::memcmp(buf1 + buf1Pos, buf2, bytesRead2) != 0)
                    return false;

                buf1Pos += bytesRead2;
            }
            return stream2->tryRead(buf2, blockSize2, notifyIoDiv) == 0; //throw FileError, X; expect EOF
        }
        else
        {
            buf1PosEnd += bytesRead1;

            size_t buf1Pos = 0;
            while (buf1PosEnd - buf1Pos >= blockSize2)
            {
                const size_t bytesRead2 = stream2->tryRead(buf2, blockSize2, notifyIoDiv); //throw FileError, X; may return short; only 0 means EOF

                if (bytesRead2 == 0) //end of file
                    return false;

                if (std::memcmp(buf1 + buf1Pos, buf2, bytesRead2) != 0)
                    return false;

                buf1Pos += bytesRead2;
            }
            if (buf1Pos > 0)
            {
                buf1PosEnd -= buf1Pos;
                std::memmove(buf1, buf1 + buf1Pos, buf1PosEnd);
            }
        }
    }
}

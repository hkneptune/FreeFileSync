// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_ID_DEF_H_013287632486321493
#define FILE_ID_DEF_H_013287632486321493

    #include <sys/stat.h>


namespace zen
{
namespace impl { typedef struct ::stat StatDummy; } //sigh...

using VolumeId  = decltype(impl::StatDummy::st_dev);
using FileIndex = decltype(impl::StatDummy::st_ino);


struct FileId //always available on Linux, and *generally* available on Windows)
{
    FileId() {}
    FileId(VolumeId volId, FileIndex fIdx) : volumeId(volId), fileIndex(fIdx)
    {
        if (volId == 0 || fIdx == 0)
        {
            volumeId  = 0;
            fileIndex = 0;
        }
    }
    VolumeId  volumeId  = 0;
    FileIndex fileIndex = 0;
};
inline bool operator==(const FileId& lhs, const FileId& rhs) { return lhs.volumeId == rhs.volumeId && lhs.fileIndex == rhs.fileIndex; }


inline
FileId generateFileId(const struct ::stat& fileInfo)
{
    return FileId(fileInfo.st_dev, fileInfo.st_ino);
}
}

#endif //FILE_ID_DEF_H_013287632486321493

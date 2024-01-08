// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef VERSIONING_H_8760247652438056
#define VERSIONING_H_8760247652438056

#include <functional>
#include <zen/time.h>
#include <zen/file_error.h>
#include "structures.h"
#include "algorithm.h"
#include "../afs/abstract.h"


namespace fff
{
/* e.g. move C:\Source\subdir\Sample.txt -> D:\Revisions\subdir\Sample.txt 2012-05-15 131513.txt
    scheme: <revisions directory>\<relpath>\<filename>.<ext> YYYY-MM-DD HHMMSS.<ext>

    - ignores missing source files/dirs
    - creates missing intermediate directories
    - does not create empty directories
    - handles symlinks
    - multi-threading: internally synchronized
    - replaces already existing target files/dirs (supports retry)
        => (unlikely) risk of data loss for naming convention "versioning":
        race-condition if multiple folder pairs process the same filepath!!                */

class FileVersioner
{
public:
    FileVersioner(const AbstractPath& versioningFolderPath, //throw FileError
                  VersioningStyle versioningStyle,
                  time_t syncStartTime) :
        versioningFolderPath_(versioningFolderPath),
        versioningStyle_(versioningStyle),
        syncStartTime_(syncStartTime)
    {
        using namespace zen;

        if (AbstractFileSystem::isNullPath(versioningFolderPath_))
            throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");

        if (timeStamp_.size() != 17) //formatTime() returns empty string on error; unexpected length: e.g. problem in year 10,000!
            throw FileError(_("Unable to create time stamp for versioning:") + L" \"" + utfTo<std::wstring>(timeStamp_) + L'"');
    }

    //multi-threaded access: internally synchronized!
    void revisionFile(const FileDescriptor& fileDescr, //throw FileError, X
                      const Zstring& relativePath,
                      //called frequently if move has to revert to copy + delete => see zen::copyFile for limitations when throwing exceptions!
                      const zen::IoCallback& notifyUnbufferedIO /*throw X*/) const;

    void revisionSymlink(const AbstractPath& linkPath, const Zstring& relativePath) const; //throw FileError

    void revisionFolder(const AbstractPath& folderPath, const Zstring& relPath, //throw FileError, X
                        const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFileMove,   /*throw X*/
                        const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFolderMove, /*throw X*/
                        //called frequently if move has to revert to copy + delete => see zen::copyFile for limitations when throwing exceptions!
                        const zen::IoCallback& notifyUnbufferedIO /*throw X*/) const;

private:
    FileVersioner           (const FileVersioner&) = delete;
    FileVersioner& operator=(const FileVersioner&) = delete;

    void checkPathConflict(const AbstractPath& itemPath, const Zstring& relativePath) const; //throw FileError

    void revisionFileImpl(const FileDescriptor& fileDescr, const Zstring& relativePath, //throw FileError, X
                          const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeMove,
                          const zen::IoCallback& notifyUnbufferedIO) const;

    void revisionSymlinkImpl(const AbstractPath& linkPath, const Zstring& relativePath, //throw FileError
                             const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeMove) const;

    void revisionFolderImpl(const AbstractPath& folderPath, const Zstring& relativePath,
                            const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFileMove,
                            const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFolderMove,
                            const zen::IoCallback& notifyUnbufferedIO) const; //throw FileError, X

    AbstractPath generateVersionedPath(const Zstring& relativePath) const;

    const AbstractPath versioningFolderPath_;
    const VersioningStyle versioningStyle_;
    const time_t syncStartTime_;
    const Zstring timeStamp_{zen::formatTime(Zstr("%Y-%m-%d %H%M%S"), zen::getLocalTime(syncStartTime_))}; //e.g. "2012-05-15 131513"
};

//--------------------------------------------------------------------------------

struct VersioningLimitFolder
{
    AbstractPath versioningFolderPath;
    int versionMaxAgeDays = 0; //<= 0 := no limit
    int versionCountMin   = 0; //only used if versionMaxAgeDays > 0 => < versionCountMax (if versionCountMax > 0)
    int versionCountMax   = 0; //<= 0 := no limit
};
std::weak_ordering operator<=>(const VersioningLimitFolder& lhs, const VersioningLimitFolder& rhs);


void applyVersioningLimit(const std::set<VersioningLimitFolder>& folderLimits,
                          PhaseCallback& callback /*throw X*/);


namespace impl //declare for unit tests:
{
std::pair<time_t, Zstring> parseVersionedFileName  (const Zstring& fileName);
time_t                     parseVersionedFolderName(const Zstring& folderName);
}
}

#endif //VERSIONING_H_8760247652438056

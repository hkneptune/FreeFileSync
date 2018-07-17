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
#include "../fs/abstract.h"


namespace fff
{
//e.g. move C:\Source\subdir\Sample.txt -> D:\Revisions\subdir\Sample.txt 2012-05-15 131513.txt
//scheme: <revisions directory>\<relpath>\<filename>.<ext> YYYY-MM-DD HHMMSS.<ext>
/*
    - ignores missing source files/dirs
    - creates missing intermediate directories
    - does not create empty directories
    - handles symlinks
    - multi-threading: internally synchronized
    - replaces already existing target files/dirs (supports retry)
        => (unlikely) risk of data loss for naming convention "versioning":
        race-condition if multiple folder pairs process the same filepath!!
*/

class FileVersioner
{
public:
    FileVersioner(const AbstractPath& versioningFolderPath, //throw FileError
                  VersioningStyle versioningStyle,
                  const zen::TimeComp& timeStamp) :
        versioningFolderPath_(versioningFolderPath),
        versioningStyle_(versioningStyle),
        timeStamp_(zen::formatTime<Zstring>(Zstr("%Y-%m-%d %H%M%S"), timeStamp)) //e.g. "2012-05-15 131513"
    {
        using namespace zen;

        if (AbstractFileSystem::isNullPath(versioningFolderPath_))
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));

        if (timeStamp_.size() != 17) //formatTime() returns empty string on error; unexpected length: e.g. problem in year 10,000!
            throw FileError(_("Unable to create time stamp for versioning:") + L" \"" + utfTo<std::wstring>(timeStamp_) + L"\"");
    }

    //multi-threaded access: internally synchronized!
    bool revisionFile(const FileDescriptor& fileDescr, //throw FileError; return "false" if file is not existing
                      const Zstring& relativePath,
                      //called frequently if move has to revert to copy + delete => see zen::copyFile for limitations when throwing exceptions!
                      const zen::IOCallback& notifyUnbufferedIO) const; //may be nullptr

    bool revisionSymlink(const AbstractPath& linkPath, const Zstring& relativePath) const; //throw FileError; return "false" if file is not existing

    void revisionFolder(const AbstractPath& folderPath, const Zstring& relativePath, //throw FileError

                        //optional callbacks: may be nullptr
                        const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFileMove,   //one call for each object!
                        const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFolderMove, //
                        //called frequently if move has to revert to copy + delete => see zen::copyFile for limitations when throwing exceptions!
                        const zen::IOCallback& notifyUnbufferedIO) const;

    //multi-threaded access: ?
    //void limitVersions(std::function<void()> updateUI); //throw FileError; call when done revisioning!

private:
    FileVersioner           (const FileVersioner&) = delete;
    FileVersioner& operator=(const FileVersioner&) = delete;

    void revisionFolderImpl(const AbstractPath& folderPath, const Zstring& relativePath,
                            const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFileMove,
                            const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFolderMove,
                            const zen::IOCallback& notifyUnbufferedIO) const; //throw FileError

    AbstractPath generateVersionedPath(const Zstring& relativePath) const;

    const AbstractPath versioningFolderPath_;
    const VersioningStyle versioningStyle_;
    const Zstring timeStamp_;

    //Protected<std::vector<Zstring>> fileRelNames_; //list of revisioned file and symlink relative names for limitVersions()
};

namespace impl //declare for unit tests:
{
bool isMatchingVersion(const Zstring& shortname, const Zstring& shortnameVersion);
}
}

#endif //VERSIONING_H_8760247652438056

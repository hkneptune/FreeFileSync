#include "versioning.h"
#include <cstddef> //required by GCC 4.8.1 to find ptrdiff_t

using namespace zen;
using namespace fff;


namespace
{
inline
Zstring getDotExtension(const Zstring& relativePath) //including "." if extension is existing, returns empty string otherwise
{
    const Zstring& extension = getFileExtension(relativePath);
    return extension.empty() ? extension : Zstr('.') + extension;
};
}

bool fff::impl::isMatchingVersion(const Zstring& shortname, const Zstring& shortnameVersioned) //e.g. ("Sample.txt", "Sample.txt 2012-05-15 131513.txt")
{
    auto it = shortnameVersioned.begin();
    auto itLast = shortnameVersioned.end();

    auto nextDigit = [&]() -> bool
    {
        if (it == itLast || !isDigit(*it))
            return false;
        ++it;
        return true;
    };
    auto nextDigits = [&](size_t count) -> bool
    {
        while (count-- > 0)
            if (!nextDigit())
                return false;
        return true;
    };
    auto nextChar = [&](Zchar c) -> bool
    {
        if (it == itLast || *it != c)
            return false;
        ++it;
        return true;
    };
    auto nextStringI = [&](const Zstring& str) -> bool //windows: ignore case!
    {
        if (itLast - it < static_cast<ptrdiff_t>(str.size()) || !equalFilePath(str, Zstring(&*it, str.size())))
            return false;
        it += str.size();
        return true;
    };

    return nextStringI(shortname) && //versioned file starts with original name
           nextChar(Zstr(' ')) && //validate timestamp: e.g. "2012-05-15 131513"; Regex: \d{4}-\d{2}-\d{2} \d{6}
           nextDigits(4)       && //YYYY
           nextChar(Zstr('-')) && //
           nextDigits(2)       && //MM
           nextChar(Zstr('-')) && //
           nextDigits(2)       && //DD
           nextChar(Zstr(' ')) && //
           nextDigits(6)       && //HHMMSS
           nextStringI(getDotExtension(shortname)) &&
           it == itLast;
}


AbstractPath FileVersioner::generateVersionedPath(const Zstring& relativePath) const
{
    assert(!startsWith(relativePath, FILE_NAME_SEPARATOR));
    assert(!endsWith  (relativePath, FILE_NAME_SEPARATOR));
    assert(!relativePath.empty());

    Zstring versionedRelPath;
    switch (versioningStyle_)
    {
        case VersioningStyle::REPLACE:
            versionedRelPath = relativePath;
            break;
        case VersioningStyle::ADD_TIMESTAMP: //assemble time-stamped version name
            versionedRelPath = relativePath + Zstr(' ') + timeStamp_ + getDotExtension(relativePath);
            assert(impl::isMatchingVersion(afterLast(relativePath,     FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL),
                                           afterLast(versionedRelPath, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL))); //paranoid? no!
            break;
    }

    return AFS::appendRelPath(versioningFolderPath_, versionedRelPath);
}


namespace
{
/*
move source to target across volumes:
- source is expected to exist
- if target already exists, it is overwritten, unless it is of a different type, e.g. a directory!
- target parent directories are created if missing
*/
template <class Function>
void moveExistingItemToVersioning(const AbstractPath& sourcePath, const AbstractPath& targetPath, //throw FileError
                                  Function copyNewItemPlain /*throw FileError*/)
{
    //start deleting existing target as required by copyFileTransactional()/renameItem():
    //best amortized performance if "target existing" is the most common case
    std::exception_ptr deletionError;
    try { AFS::removeFilePlain(targetPath); /*throw FileError*/ }
    catch (FileError&) { deletionError = std::current_exception(); } //probably "not existing" error, defer evaluation
    //overwrite AFS::ItemType::FOLDER with FILE? => highly dubious, do not allow

    auto fixedTargetPathIssues = [&] //throw FileError
    {
        Opt<AFS::PathStatus> pd;
        try { pd = AFS::getPathStatus(targetPath); /*throw FileError*/ }
        catch (FileError&)
        {
            //previous exception is more relevant in general
            //BUT, we might be hiding a second unrelated issue: https://www.freefilesync.org/forum/viewtopic.php?t=4765#p16016
            //=> FFS considers session faulty and tries to create a new one, which might fail with: LIBSSH2_ERROR_AUTHENTICATION_FAILED
        }

        if (pd)
        {
            if (pd->relPath.empty()) //already existing
            {
                if (deletionError)
                    std::rethrow_exception(deletionError);
            }
            else if (pd->relPath.size() > 1) //parent folder missing
            {
                AbstractPath intermediatePath = pd->existingPath;
                for (const Zstring& itemName : std::vector<Zstring>(pd->relPath.begin(), pd->relPath.end() - 1))
                    AFS::createFolderPlain(intermediatePath = AFS::appendRelPath(intermediatePath, itemName)); //throw FileError
                return true;
            }
        }
        return false;
    };

    try //first try to move directly without copying
    {
        AFS::renameItem(sourcePath, targetPath); //throw FileError, ErrorDifferentVolume
        //great, we get away cheaply!
    }
    catch (ErrorDifferentVolume&)
    {
        try
        {
            copyNewItemPlain(); //throw FileError
        }
        catch (FileError&)
        {
            if (!fixedTargetPathIssues()) //throw FileError
                throw;
            //retry
            copyNewItemPlain(); //throw FileError
        }
        //[!] remove source file AFTER handling target path errors!
        AFS::removeFilePlain(sourcePath); //throw FileError
    }
    catch (FileError&)
    {
        if (!fixedTargetPathIssues()) //throw FileError
            throw;

        try //retry
        {
            AFS::renameItem(sourcePath, targetPath); //throw FileError, ErrorDifferentVolume
        }
        catch (ErrorDifferentVolume&)
        {
            copyNewItemPlain(); //throw FileError
            AFS::removeFilePlain(sourcePath); //throw FileError
        }
    }
}


struct FlatTraverserCallback: public AFS::TraverserCallback
{
    FlatTraverserCallback(const AbstractPath& folderPath) : folderPath_(folderPath) {}

    const std::vector<FileInfo>&    refFiles   () const { return files_; }
    const std::vector<FolderInfo>&  refFolders () const { return folders_; }
    const std::vector<SymlinkInfo>& refSymlinks() const { return symlinks_; }

private:
    void                               onFile   (const FileInfo&    fi) override { files_   .push_back(fi); }
    std::unique_ptr<TraverserCallback> onFolder (const FolderInfo&  fi) override { folders_ .push_back(fi); return nullptr; }
    HandleLink                         onSymlink(const SymlinkInfo& si) override { symlinks_.push_back(si); return TraverserCallback::LINK_SKIP; }

    HandleError reportDirError (const std::wstring& msg, size_t retryNumber)                          override { throw FileError(msg); }
    HandleError reportItemError(const std::wstring& msg, size_t retryNumber, const Zstring& itemName) override { throw FileError(msg); }

    const AbstractPath folderPath_;
    std::vector<FileInfo>    files_;
    std::vector<FolderInfo>  folders_;
    std::vector<SymlinkInfo> symlinks_;
};
}


bool FileVersioner::revisionFile(const FileDescriptor& fileDescr, const Zstring& relativePath, const IOCallback& notifyUnbufferedIO) //throw FileError
{
    const AbstractPath& filePath = fileDescr.path;
    const AFS::StreamAttributes fileAttr{ fileDescr.attr.modTime, fileDescr.attr.fileSize, fileDescr.attr.fileId };

    if (Opt<AFS::ItemType> type = AFS::getItemTypeIfExists(filePath)) //throw FileError
    {
        const AbstractPath targetPath = generateVersionedPath(relativePath);

        if (*type == AFS::ItemType::SYMLINK)
            moveExistingItemToVersioning(filePath, targetPath, [&] { AFS::copySymlink(filePath, targetPath, false /*copy filesystem permissions*/); }); //throw FileError
        else
            moveExistingItemToVersioning(filePath, targetPath, [&] //throw FileError
        {
            //target existing: copyFileTransactional() undefined behavior! (fail/overwrite/auto-rename) => not expected here:
            /*const AFS::FileCopyResult result =*/ AFS::copyFileTransactional(filePath, fileAttr, targetPath, //throw FileError, ErrorFileLocked
                                                                              false, //copyFilePermissions
                                                                              false,  //transactionalCopy: not needed for versioning!
                                                                              nullptr /*onDeleteTargetFile*/, notifyUnbufferedIO);
            //result.errorModTime? => irrelevant for versioning!
        });
        return true;
    }
    else
        return false; //missing source item is not an error => check BEFORE overwriting target
}


bool FileVersioner::revisionSymlink(const AbstractPath& linkPath, const Zstring& relativePath) //throw FileError
{
    if (AFS::getItemTypeIfExists(linkPath)) //throw FileError
    {
        const AbstractPath targetPath = generateVersionedPath(relativePath);
        moveExistingItemToVersioning(linkPath, targetPath, [&] { AFS::copySymlink(linkPath, targetPath, false /*copy filesystem permissions*/); }); //throw FileError
        return true;
    }
    else
        return false;
}


void FileVersioner::revisionFolder(const AbstractPath& folderPath, const Zstring& relativePath, //throw FileError
                                   const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFileMove,
                                   const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFolderMove,
                                   const IOCallback& notifyUnbufferedIO)
{
    if (Opt<AFS::ItemType> type = AFS::getItemTypeIfExists(folderPath)) //throw FileError
    {
        if (*type == AFS::ItemType::SYMLINK) //on Linux there is just one type of symlink, and since we do revision file symlinks, we should revision dir symlinks as well!
        {
            const AbstractPath targetPath = generateVersionedPath(relativePath);
            if (onBeforeFileMove)
                onBeforeFileMove(AFS::getDisplayPath(folderPath), AFS::getDisplayPath(targetPath));

            moveExistingItemToVersioning(folderPath, targetPath, [&] { AFS::copySymlink(folderPath, targetPath, false /*copy filesystem permissions*/); }); //throw FileError
        }
        else
            revisionFolderImpl(folderPath, relativePath, onBeforeFileMove, onBeforeFolderMove, notifyUnbufferedIO); //throw FileError
    }
    //no error situation if directory is not existing! manual deletion relies on it!
}


void FileVersioner::revisionFolderImpl(const AbstractPath& folderPath, const Zstring& relativePath, //throw FileError
                                       const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFileMove,
                                       const std::function<void(const std::wstring& displayPathFrom, const std::wstring& displayPathTo)>& onBeforeFolderMove,
                                       const IOCallback& notifyUnbufferedIO)
{

    //create target directories only when needed in moveFileToVersioning(): avoid empty directories!

    FlatTraverserCallback ft(folderPath); //traverse source directory one level deep
    AFS::traverseFolder(folderPath, ft); //throw FileError

    const Zstring relPathPf = appendSeparator(relativePath);

    for (const auto& fileInfo: ft.refFiles())
    {
        const AbstractPath sourcePath = AFS::appendRelPath(folderPath, fileInfo.itemName);
        const AbstractPath targetPath = generateVersionedPath(relPathPf + fileInfo.itemName);
        const AFS::StreamAttributes sourceAttr{ fileInfo.modTime, fileInfo.fileSize, fileInfo.fileId };

        if (onBeforeFileMove)
            onBeforeFileMove(AFS::getDisplayPath(sourcePath), AFS::getDisplayPath(targetPath));

        moveExistingItemToVersioning(sourcePath, targetPath, [&] //throw FileError
        {
            //target existing: copyFileTransactional() undefined behavior! (fail/overwrite/auto-rename) => not expected here:
            /*const AFS::FileCopyResult result =*/ AFS::copyFileTransactional(sourcePath, sourceAttr, targetPath, //throw FileError, ErrorFileLocked
                                                                              false, //copyFilePermissions
                                                                              false,  //transactionalCopy: not needed for versioning!
                                                                              nullptr /*onDeleteTargetFile*/, notifyUnbufferedIO);
            //result.errorModTime? => irrelevant for versioning!
        });
    }

    for (const auto& linkInfo: ft.refSymlinks())
    {
        const AbstractPath sourcePath = AFS::appendRelPath(folderPath, linkInfo.itemName);
        const AbstractPath targetPath = generateVersionedPath(relPathPf + linkInfo.itemName);

        if (onBeforeFileMove)
            onBeforeFileMove(AFS::getDisplayPath(sourcePath), AFS::getDisplayPath(targetPath));

        moveExistingItemToVersioning(sourcePath, targetPath, [&] { AFS::copySymlink(sourcePath, targetPath, false /*copy filesystem permissions*/); }); //throw FileError
    }

    //move folders recursively
    for (const auto& folderInfo : ft.refFolders())
        revisionFolderImpl(AFS::appendRelPath(folderPath, folderInfo.itemName), //throw FileError
                           relPathPf + folderInfo.itemName,
                           onBeforeFileMove, onBeforeFolderMove, notifyUnbufferedIO);
    //delete source
    if (onBeforeFolderMove)
        onBeforeFolderMove(AFS::getDisplayPath(folderPath), AFS::getDisplayPath(AFS::appendRelPath(versioningFolderPath_, relativePath)));

    AFS::removeFolderPlain(folderPath); //throw FileError
}


/*
void FileVersioner::limitVersions(std::function<void()> updateUI) //throw FileError
{
    if (versionCountLimit_ < 0) //no limit!
        return;

    //buffer map "directory |-> list of immediate child file and symlink short names"
    std::map<Zstring, std::vector<Zstring>, LessFilePath> dirBuffer;

    auto getVersionsBuffered = [&](const Zstring& dirpath) -> const std::vector<Zstring>&
    {
        auto it = dirBuffer.find(dirpath);
        if (it != dirBuffer.end())
            return it->second;

        std::vector<Zstring> fileShortNames;
        TraverseVersionsOneLevel tol(fileShortNames, updateUI); //throw FileError
        traverseFolder(dirpath, tol);

        auto& newEntry = dirBuffer[dirpath]; //transactional behavior!!!
        newEntry.swap(fileShortNames);       //-> until C++11 emplace is available

        return newEntry;
    };

    std::for_each(fileRelNames.begin(), fileRelNames.end(),
                  [&](const Zstring& relativePath) //e.g. "subdir\Sample.txt"
    {
        const Zstring filepath = appendSeparator(versioningDirectory_) + relativePath; //e.g. "D:\Revisions\subdir\Sample.txt"
        const Zstring parentDir = beforeLast(filepath, FILE_NAME_SEPARATOR);    //e.g. "D:\Revisions\subdir"
        const Zstring shortname = afterLast(relativePath, FILE_NAME_SEPARATOR); //e.g. "Sample.txt"; returns the whole string if seperator not found

        const std::vector<Zstring>& allVersions = getVersionsBuffered(parentDir);

        //filter out only those versions that match the given relative name
        std::vector<Zstring> matches; //e.g. "Sample.txt 2012-05-15 131513.txt"

        std::copy_if(allVersions.begin(), allVersions.end(), std::back_inserter(matches),
        [&](const Zstring& shortnameVer) { return impl::isMatchingVersion(shortname, shortnameVer); });

        //take advantage of version naming convention to find oldest versions
        if (matches.size() <= static_cast<size_t>(versionCountLimit_))
            return;
        std::nth_element(matches.begin(), matches.end() - versionCountLimit_, matches.end(), LessFilePath()); //windows: ignore case!

        //delete obsolete versions
        std::for_each(matches.begin(), matches.end() - versionCountLimit_,
                      [&](const Zstring& shortnameVer)
        {
            updateUI();
            const Zstring fullnameVer = parentDir + FILE_NAME_SEPARATOR + shortnameVer;
            try
            {
                removeFile(fullnameVer); //throw FileError
            }
            catch (FileError&)
            {
#ifdef ZEN_WIN //if it's a directory symlink:
                if (symlinkExists(fullnameVer) && dirExists(fullnameVer))
                    removeDirectory(fullnameVer); //throw FileError
                else
#endif
                    throw;
            }
        });
    });
}
*/

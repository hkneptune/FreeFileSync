// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "file_hierarchy.h"
#include <zen/i18n.h>
#include <zen/utf.h>
#include <zen/file_error.h>

using namespace zen;
using namespace fff;




void ContainerObject::removeEmptyRec()
{
    bool emptyExisting = false;
    auto isEmpty = [&](const FileSystemObject& fsObj) -> bool
    {
        const bool objEmpty = fsObj.isPairEmpty();
        if (objEmpty)
            emptyExisting = true;
        return objEmpty;
    };

    refSubFiles  ().remove_if(isEmpty);
    refSubLinks  ().remove_if(isEmpty);
    refSubFolders().remove_if(isEmpty);

    if (emptyExisting) //notify if actual deletion happened
        notifySyncCfgChanged(); //mustn't call this in ~FileSystemObject(), since parent, usually a FolderPair, may already be partially destroyed and existing as a pure ContainerObject!

    for (FolderPair& folder : refSubFolders())
        folder.removeEmptyRec(); //recurse
}


namespace
{
SyncOperation getIsolatedSyncOperation(bool itemExistsLeft,
                                       bool itemExistsRight,
                                       CompareFilesResult cmpResult,
                                       bool selectedForSync,
                                       SyncDirection syncDir,
                                       bool hasDirectionConflict) //perf: std::wstring was wasteful here
{
    assert(( itemExistsLeft &&  itemExistsRight && cmpResult != FILE_LEFT_SIDE_ONLY && cmpResult != FILE_RIGHT_SIDE_ONLY) ||
           ( itemExistsLeft && !itemExistsRight && cmpResult == FILE_LEFT_SIDE_ONLY ) ||
           (!itemExistsLeft &&  itemExistsRight && cmpResult == FILE_RIGHT_SIDE_ONLY) ||
           (!itemExistsLeft && !itemExistsRight && cmpResult == FILE_EQUAL && syncDir == SyncDirection::NONE && !hasDirectionConflict) ||
           cmpResult == FILE_CONFLICT);

    assert(!hasDirectionConflict || syncDir == SyncDirection::NONE);

    if (!selectedForSync)
        return cmpResult == FILE_EQUAL ?
               SO_EQUAL :
               SO_DO_NOTHING;

    switch (cmpResult)
    {
        case FILE_EQUAL:
            assert(syncDir == SyncDirection::NONE);
            return SO_EQUAL;

        case FILE_LEFT_SIDE_ONLY:
            switch (syncDir)
            {
                case SyncDirection::LEFT:
                    return SO_DELETE_LEFT; //delete files on left
                case SyncDirection::RIGHT:
                    return SO_CREATE_NEW_RIGHT; //copy files to right
                case SyncDirection::NONE:
                    return hasDirectionConflict ? SO_UNRESOLVED_CONFLICT : SO_DO_NOTHING;
            }
            break;

        case FILE_RIGHT_SIDE_ONLY:
            switch (syncDir)
            {
                case SyncDirection::LEFT:
                    return SO_CREATE_NEW_LEFT; //copy files to left
                case SyncDirection::RIGHT:
                    return SO_DELETE_RIGHT; //delete files on right
                case SyncDirection::NONE:
                    return hasDirectionConflict ? SO_UNRESOLVED_CONFLICT : SO_DO_NOTHING;
            }
            break;

        case FILE_LEFT_NEWER:
        case FILE_RIGHT_NEWER:
        case FILE_DIFFERENT_CONTENT:
            switch (syncDir)
            {
                case SyncDirection::LEFT:
                    return SO_OVERWRITE_LEFT; //copy from right to left
                case SyncDirection::RIGHT:
                    return SO_OVERWRITE_RIGHT; //copy from left to right
                case SyncDirection::NONE:
                    return hasDirectionConflict ? SO_UNRESOLVED_CONFLICT : SO_DO_NOTHING;
            }
            break;

        case FILE_DIFFERENT_METADATA:
            switch (syncDir)
            {
                case SyncDirection::LEFT:
                    return SO_COPY_METADATA_TO_LEFT;
                case SyncDirection::RIGHT:
                    return SO_COPY_METADATA_TO_RIGHT;
                case SyncDirection::NONE:
                    return hasDirectionConflict ? SO_UNRESOLVED_CONFLICT : SO_DO_NOTHING;
            }
            break;

        case FILE_CONFLICT:
            switch (syncDir)
            {
                case SyncDirection::LEFT:
                    return itemExistsLeft && itemExistsRight ? SO_OVERWRITE_LEFT : itemExistsLeft ? SO_DELETE_LEFT: SO_CREATE_NEW_LEFT;
                case SyncDirection::RIGHT:
                    return itemExistsLeft && itemExistsRight ? SO_OVERWRITE_RIGHT : itemExistsLeft ? SO_CREATE_NEW_RIGHT : SO_DELETE_RIGHT;
                case SyncDirection::NONE:
                    return hasDirectionConflict ? SO_UNRESOLVED_CONFLICT : SO_DO_NOTHING;
            }
            break;
    }

    assert(false);
    return SO_DO_NOTHING; //dummy
}


template <class Predicate> inline
bool hasDirectChild(const ContainerObject& hierObj, Predicate p)
{
    return std::any_of(hierObj.refSubFiles  ().begin(), hierObj.refSubFiles  ().end(), p) ||
           std::any_of(hierObj.refSubLinks  ().begin(), hierObj.refSubLinks  ().end(), p) ||
           std::any_of(hierObj.refSubFolders().begin(), hierObj.refSubFolders().end(), p);
}
}


SyncOperation FileSystemObject::testSyncOperation(SyncDirection testSyncDir) const //semantics: "what if"! assumes "active, no conflict, no recursion (directory)!
{
    return getIsolatedSyncOperation(!isEmpty<LEFT_SIDE>(), !isEmpty<RIGHT_SIDE>(), getCategory(), true, testSyncDir, false);
}


SyncOperation FileSystemObject::getSyncOperation() const
{
    return getIsolatedSyncOperation(!isEmpty<LEFT_SIDE>(), !isEmpty<RIGHT_SIDE>(), getCategory(), selectedForSync_, getSyncDir(), syncDirectionConflict_.get() != nullptr);
    //do *not* make a virtual call to testSyncOperation()! See FilePair::testSyncOperation()! <- better not implement one in terms of the other!!!
}


//SyncOperation FolderPair::testSyncOperation() const -> no recursion: we do NOT want to consider child elements when testing!


SyncOperation FolderPair::getSyncOperation() const
{
    if (!syncOpBuffered_) //redetermine...
    {
        //suggested operation *not* considering child elements
        syncOpBuffered_ = FileSystemObject::getSyncOperation();

        //action for child elements may occassionally have to overwrite parent task:
        switch (*syncOpBuffered_)
        {
            case SO_MOVE_LEFT_FROM:
            case SO_MOVE_LEFT_TO:
            case SO_MOVE_RIGHT_FROM:
            case SO_MOVE_RIGHT_TO:
                assert(false);
            case SO_CREATE_NEW_LEFT:
            case SO_CREATE_NEW_RIGHT:
            case SO_OVERWRITE_LEFT:
            case SO_OVERWRITE_RIGHT:
            case SO_COPY_METADATA_TO_LEFT:
            case SO_COPY_METADATA_TO_RIGHT:
            case SO_EQUAL:
                break; //take over suggestion, no problem for child-elements
            case SO_DELETE_LEFT:
            case SO_DELETE_RIGHT:
            case SO_DO_NOTHING:
            case SO_UNRESOLVED_CONFLICT:
                if (isEmpty<LEFT_SIDE>())
                {
                    //1. if at least one child-element is to be created, make sure parent folder is created also
                    //note: this automatically fulfills "create parent folders even if excluded"
                    if (hasDirectChild(*this,
                                       [](const FileSystemObject& fsObj)
                {
                    const SyncOperation op = fsObj.getSyncOperation();
                        return op == SO_CREATE_NEW_LEFT ||
                               op == SO_MOVE_LEFT_TO;
                    }))
                    syncOpBuffered_ = SO_CREATE_NEW_LEFT;
                    //2. cancel parent deletion if only a single child is not also scheduled for deletion
                    else if (*syncOpBuffered_ == SO_DELETE_RIGHT &&
                             hasDirectChild(*this,
                                            [](const FileSystemObject& fsObj)
                {
                    if (fsObj.isPairEmpty())
                            return false; //fsObj may already be empty because it once contained a "move source"
                        const SyncOperation op = fsObj.getSyncOperation();
                        return op != SO_DELETE_RIGHT &&
                               op != SO_MOVE_RIGHT_FROM;
                    }))
                    syncOpBuffered_ = SO_DO_NOTHING;
                }
                else if (isEmpty<RIGHT_SIDE>())
                {
                    if (hasDirectChild(*this,
                                       [](const FileSystemObject& fsObj)
                {
                    const SyncOperation op = fsObj.getSyncOperation();
                        return  op == SO_CREATE_NEW_RIGHT ||
                                op == SO_MOVE_RIGHT_TO;
                    }))
                    syncOpBuffered_ = SO_CREATE_NEW_RIGHT;
                    else if (*syncOpBuffered_ == SO_DELETE_LEFT &&
                             hasDirectChild(*this,
                                            [](const FileSystemObject& fsObj)
                {
                    if (fsObj.isPairEmpty())
                            return false;
                        const SyncOperation op = fsObj.getSyncOperation();
                        return op != SO_DELETE_LEFT &&
                               op != SO_MOVE_LEFT_FROM;
                    }))
                    syncOpBuffered_ = SO_DO_NOTHING;
                }
                break;
        }
    }
    return *syncOpBuffered_;
}


inline //it's private!
SyncOperation FilePair::applyMoveOptimization(SyncOperation op) const
{
    /*
        check whether we can optimize "create + delete" via "move":
        note: as long as we consider "create + delete" cases only, detection of renamed files, should be fine even for "binary" comparison variant!
    */
    if (moveFileRef_)
        if (auto refFile = dynamic_cast<const FilePair*>(FileSystemObject::retrieve(moveFileRef_))) //we expect a "FilePair", but only need a "FileSystemObject"
        {
            SyncOperation opRef = refFile->FileSystemObject::getSyncOperation(); //do *not* make a virtual call!

            if (op    == SO_CREATE_NEW_LEFT &&
                opRef == SO_DELETE_LEFT)
                op = SO_MOVE_LEFT_TO;
            else if (op    == SO_DELETE_LEFT &&
                     opRef == SO_CREATE_NEW_LEFT)
                op = SO_MOVE_LEFT_FROM;
            else if (op    == SO_CREATE_NEW_RIGHT &&
                     opRef == SO_DELETE_RIGHT)
                op = SO_MOVE_RIGHT_TO;
            else if (op    == SO_DELETE_RIGHT &&
                     opRef == SO_CREATE_NEW_RIGHT)
                op = SO_MOVE_RIGHT_FROM;
        }
    return op;
}


SyncOperation FilePair::testSyncOperation(SyncDirection testSyncDir) const
{
    return applyMoveOptimization(FileSystemObject::testSyncOperation(testSyncDir));
}


SyncOperation FilePair::getSyncOperation() const
{
    return applyMoveOptimization(FileSystemObject::getSyncOperation());
}


std::wstring fff::getCategoryDescription(CompareFilesResult cmpRes)
{
    switch (cmpRes)
    {
        case FILE_LEFT_SIDE_ONLY:
            return _("Item exists on left side only");
        case FILE_RIGHT_SIDE_ONLY:
            return _("Item exists on right side only");
        case FILE_LEFT_NEWER:
            return _("Left side is newer");
        case FILE_RIGHT_NEWER:
            return _("Right side is newer");
        case FILE_DIFFERENT_CONTENT:
            return _("Items have different content");
        case FILE_EQUAL:
            return _("Both sides are equal");
        case FILE_DIFFERENT_METADATA:
            return _("Items differ in attributes only");
        case FILE_CONFLICT:
            return _("Conflict/item cannot be categorized");
    }
    assert(false);
    return std::wstring();
}


namespace
{
const wchar_t arrowLeft [] = L"<-";
const wchar_t arrowRight[] = L"->";
}


std::wstring fff::getCategoryDescription(const FileSystemObject& fsObj)
{
    const std::wstring footer = L"\n[" + utfTo<std::wstring>(fsObj. getPairItemName()) + L"]";

    const CompareFilesResult cmpRes = fsObj.getCategory();
    switch (cmpRes)
    {
        case FILE_LEFT_SIDE_ONLY:
        case FILE_RIGHT_SIDE_ONLY:
        case FILE_DIFFERENT_CONTENT:
        case FILE_EQUAL:
            return getCategoryDescription(cmpRes) + footer; //use generic description

        case FILE_LEFT_NEWER:
        case FILE_RIGHT_NEWER:
        {
            std::wstring descr = getCategoryDescription(cmpRes);

            visitFSObject(fsObj, [](const FolderPair& folder) {},
            [&](const FilePair& file)
            {
                descr += std::wstring(L"\n") +
                         arrowLeft  + L" " + formatUtcToLocalTime(file.getLastWriteTime< LEFT_SIDE>()) + L"\n" +
                         arrowRight + L" " + formatUtcToLocalTime(file.getLastWriteTime<RIGHT_SIDE>());
            },
            [&](const SymlinkPair& symlink)
            {
                descr += std::wstring(L"\n") +
                         arrowLeft  + L" " + formatUtcToLocalTime(symlink.getLastWriteTime< LEFT_SIDE>()) + L"\n" +
                         arrowRight + L" " + formatUtcToLocalTime(symlink.getLastWriteTime<RIGHT_SIDE>());
            });
            return descr + footer;
        }

        case FILE_DIFFERENT_METADATA:
        case FILE_CONFLICT:
            return fsObj.getCatExtraDescription() + footer;
    }
    assert(false);
    return std::wstring();
}


std::wstring fff::getSyncOpDescription(SyncOperation op)
{
    switch (op)
    {
        case SO_CREATE_NEW_LEFT:
            return _("Copy new item to left");
        case SO_CREATE_NEW_RIGHT:
            return _("Copy new item to right");
        case SO_DELETE_LEFT:
            return _("Delete left item");
        case SO_DELETE_RIGHT:
            return _("Delete right item");
        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_LEFT_TO:
            return _("Move file on left"); //move only supported for files
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_RIGHT_TO:
            return _("Move file on right");
        case SO_OVERWRITE_LEFT:
            return _("Update left item");
        case SO_OVERWRITE_RIGHT:
            return _("Update right item");
        case SO_DO_NOTHING:
            return _("Do nothing");
        case SO_EQUAL:
            return _("Both sides are equal");
        case SO_COPY_METADATA_TO_LEFT:
            return _("Update attributes on left");
        case SO_COPY_METADATA_TO_RIGHT:
            return _("Update attributes on right");
        case SO_UNRESOLVED_CONFLICT: //not used on GUI, but in .csv
            return _("Conflict/item cannot be categorized");
    }
    assert(false);
    return std::wstring();
}


std::wstring fff::getSyncOpDescription(const FileSystemObject& fsObj)
{
    const std::wstring footer = L"\n[" + utfTo<std::wstring>(fsObj. getPairItemName()) + L"]";

    const SyncOperation op = fsObj.getSyncOperation();
    switch (op)
    {
        case SO_CREATE_NEW_LEFT:
        case SO_CREATE_NEW_RIGHT:
        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
        case SO_OVERWRITE_LEFT:
        case SO_OVERWRITE_RIGHT:
        case SO_DO_NOTHING:
        case SO_EQUAL:
            return getSyncOpDescription(op) + footer; //use generic description

        case SO_COPY_METADATA_TO_LEFT:
        case SO_COPY_METADATA_TO_RIGHT:
            //harmonize with synchronization.cpp::SynchronizeFolderPair::synchronizeFileInt, ect!!
        {
            Zstring shortNameOld = fsObj.getItemName<RIGHT_SIDE>();
            Zstring shortNameNew = fsObj.getItemName< LEFT_SIDE>();
            if (op == SO_COPY_METADATA_TO_LEFT)
                std::swap(shortNameOld, shortNameNew);

            if (shortNameOld != shortNameNew) //detected change in case
                return getSyncOpDescription(op) + L"\n" +
                       fmtPath(shortNameOld) + L" " + arrowRight + L"\n" + //show short name only
                       fmtPath(shortNameNew) /*+ footer -> redundant */;
        }
        return getSyncOpDescription(op) + footer; //fallback

        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_LEFT_TO:
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_RIGHT_TO:
            if (auto sourceFile = dynamic_cast<const FilePair*>(&fsObj))
                if (auto targetFile = dynamic_cast<const FilePair*>(FileSystemObject::retrieve(sourceFile->getMoveRef())))
                {
                    const bool onLeft   = op == SO_MOVE_LEFT_FROM || op == SO_MOVE_LEFT_TO;
                    const bool isSource = op == SO_MOVE_LEFT_FROM || op == SO_MOVE_RIGHT_FROM;

                    if (!isSource)
                        std::swap(sourceFile, targetFile);

                    auto getRelName = [&](const FileSystemObject& fso, bool leftSide) { return leftSide ? fso.getRelativePath<LEFT_SIDE>() : fso.getRelativePath<RIGHT_SIDE>(); };

                    const Zstring relSource = getRelName(*sourceFile, onLeft);
                    const Zstring relTarget = getRelName(*targetFile, onLeft);

                    //attention: ::SetWindowText() doesn't handle tab characters correctly in combination with certain file names, so don't use them
                    return getSyncOpDescription(op) + L"\n" +
                           (equalFilePath(beforeLast(relSource, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE),
                                          beforeLast(relTarget, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_NONE)) ?
                            //detected pure "rename"
                            fmtPath(afterLast(relSource, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL)) + L" " + arrowRight + L"\n" + //show short name only
                            fmtPath(afterLast(relTarget, FILE_NAME_SEPARATOR, IF_MISSING_RETURN_ALL)) :
                            //"move" or "move + rename"
                            fmtPath(relSource) + L" " + arrowRight + L"\n" +
                            fmtPath(relTarget)) /*+ footer -> redundant */;
                }
            break;

        case SO_UNRESOLVED_CONFLICT:
            return fsObj.getSyncOpConflict() + footer;
    }

    assert(false);
    return std::wstring();
}

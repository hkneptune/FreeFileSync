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


std::wstring fff::getShortDisplayNameForFolderPair(const AbstractPath& itemPathL, const AbstractPath& itemPathR)
{
    Zstring commonTrail;
    AbstractPath tmpPathL = itemPathL;
    AbstractPath tmpPathR = itemPathR;
    for (;;)
    {
        const std::optional<AbstractPath> parentPathL = AFS::getParentPath(tmpPathL);
        const std::optional<AbstractPath> parentPathR = AFS::getParentPath(tmpPathR);
        if (!parentPathL || !parentPathR)
            break;

        const Zstring itemNameL = AFS::getItemName(tmpPathL);
        const Zstring itemNameR = AFS::getItemName(tmpPathR);
        if (!equalNoCase(itemNameL, itemNameR)) //let's compare case-insensitively (even on Linux!)
            break;

        tmpPathL = *parentPathL;
        tmpPathR = *parentPathR;

        commonTrail = appendPath(itemNameL, commonTrail);
    }
    if (!commonTrail.empty())
        return utfTo<std::wstring>(commonTrail);

    auto getLastComponent = [](const AbstractPath& itemPath)
    {
        if (!AFS::getParentPath(itemPath)) //= device root
            return AFS::getDisplayPath(itemPath);
        return utfTo<std::wstring>(AFS::getItemName(itemPath));
    };

    if (AFS::isNullPath(itemPathL))
        return getLastComponent(itemPathR);
    else if (AFS::isNullPath(itemPathR))
        return getLastComponent(itemPathL);
    else
        return getLastComponent(itemPathL) + L" | " +
               getLastComponent(itemPathR);
}


void ContainerObject::removeDoubleEmpty()
{
    auto isEmpty = [](const FileSystemObject& fsObj) { return fsObj.isPairEmpty(); };

    refSubFiles  ().remove_if(isEmpty);
    refSubLinks  ().remove_if(isEmpty);
    refSubFolders().remove_if(isEmpty);

    for (FolderPair& folder : refSubFolders())
        folder.removeDoubleEmpty();
}


namespace
{
SyncOperation getIsolatedSyncOperation(const FileSystemObject& fsObj,
                                       bool selectedForSync,
                                       SyncDirection syncDir,
                                       bool hasDirectionConflict)
{
    assert(!hasDirectionConflict || syncDir == SyncDirection::none);

    if (fsObj.isEmpty<SelectSide::left>() || fsObj.isEmpty<SelectSide::right>())
    {
        if (!selectedForSync)
            return SO_DO_NOTHING;

        if (hasDirectionConflict)
            return SO_UNRESOLVED_CONFLICT;

        if (fsObj.isEmpty<SelectSide::left>())
        {
            if (fsObj.isEmpty<SelectSide::right>()) //both sides empty: should only occur temporarily, if ever
                return SO_EQUAL;
            else //right-only
                switch (syncDir)
                {
                    //*INDENT-OFF*
                    case SyncDirection::left:  return SO_CREATE_LEFT;
                    case SyncDirection::right: return SO_DELETE_RIGHT;
                    case SyncDirection::none:  return SO_DO_NOTHING;
                    //*INDENT-ON*
                }
        }
        else //left-only
            switch (syncDir)
            {
                //*INDENT-OFF*
                case SyncDirection::left:  return SO_DELETE_LEFT;
                case SyncDirection::right: return SO_CREATE_RIGHT;
                case SyncDirection::none:  return SO_DO_NOTHING;
                //*INDENT-ON*
            }
    }
    //--------------------------------------------------------------
    std::optional<SyncOperation> result;

    visitFSObject(fsObj,
                  [&](const FolderPair& folder) //see FolderPair::getCategory()
    {
        if (folder.hasEquivalentItemNames()) //a.k.a. DIR_EQUAL
        {
            assert(syncDir == SyncDirection::none);
            return result = SO_EQUAL; //no matter if "conflict" (e.g. traversal error) or "not selected"
        }

        if (!selectedForSync)
            return result = SO_DO_NOTHING;

        if (hasDirectionConflict)
            return result = SO_UNRESOLVED_CONFLICT;

        switch (syncDir)
        {
            //*INDENT-OFF*
            case SyncDirection::left:  return result = SO_RENAME_LEFT;
            case SyncDirection::right: return result = SO_RENAME_RIGHT;
            case SyncDirection::none:  return result = SO_DO_NOTHING;
            //*INDENT-ON*
        }
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
    },
    //--------------------------------------------------------------
    [&](const FilePair& file) //see FilePair::getCategory()
    {
        if (file.getContentCategory() == FileContentCategory::equal && file.hasEquivalentItemNames()) //a.k.a. FILE_EQUAL
        {
            assert(syncDir == SyncDirection::none);
            return result = SO_EQUAL; //no matter if "conflict" (e.g. traversal error) or "not selected"
        }

        if (!selectedForSync)
            return result = SO_DO_NOTHING;

        if (hasDirectionConflict)
            return result = SO_UNRESOLVED_CONFLICT;

        switch (file.getContentCategory())
        {
            case FileContentCategory::unknown:
            case FileContentCategory::leftNewer:
            case FileContentCategory::rightNewer:
            case FileContentCategory::invalidTime:
            case FileContentCategory::different:
            case FileContentCategory::conflict:
                switch (syncDir)
                {
                    //*INDENT-OFF*
                    case SyncDirection::left:  return result = SO_OVERWRITE_LEFT;
                    case SyncDirection::right: return result = SO_OVERWRITE_RIGHT;
                    case SyncDirection::none:  return result = SO_DO_NOTHING;
                    //*INDENT-ON*
                }
                break;

            case FileContentCategory::equal:
                switch (syncDir)
                {
                    //*INDENT-OFF*
                    case SyncDirection::left:  return result = SO_RENAME_LEFT;
                    case SyncDirection::right: return result = SO_RENAME_RIGHT;
                    case SyncDirection::none:  return result = SO_DO_NOTHING;
                    //*INDENT-ON*
                }
                break;
        }
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
    },
    //--------------------------------------------------------------
    [&](const SymlinkPair& symlink) //see SymlinkPair::getCategory()
    {
        if (symlink.getContentCategory() == FileContentCategory::equal && symlink.hasEquivalentItemNames()) //a.k.a. SYMLINK_EQUAL
        {
            assert(syncDir == SyncDirection::none);
            return result = SO_EQUAL; //no matter if "conflict" (e.g. traversal error) or "not selected"
        }

        if (!selectedForSync)
            return result = SO_DO_NOTHING;

        if (hasDirectionConflict)
            return result = SO_UNRESOLVED_CONFLICT;

        switch (symlink.getContentCategory())
        {
            case FileContentCategory::unknown:
            case FileContentCategory::leftNewer:
            case FileContentCategory::rightNewer:
            case FileContentCategory::invalidTime:
            case FileContentCategory::different:
            case FileContentCategory::conflict:
                switch (syncDir)
                {
                    //*INDENT-OFF*
                    case SyncDirection::left:  return result = SO_OVERWRITE_LEFT;
                    case SyncDirection::right: return result = SO_OVERWRITE_RIGHT;
                    case SyncDirection::none:  return result = SO_DO_NOTHING;
                    //*INDENT-ON*
                }
                break;

            case FileContentCategory::equal:
                switch (syncDir)
                {
                    //*INDENT-OFF*
                    case SyncDirection::left:  return result = SO_RENAME_LEFT;
                    case SyncDirection::right: return result = SO_RENAME_RIGHT;
                    case SyncDirection::none:  return result = SO_DO_NOTHING;
                    //*INDENT-ON*
                }
                break;
        }
        throw std::logic_error(std::string(__FILE__) + '[' + numberTo<std::string>(__LINE__) + "] Contract violation!");
    });
    return *result;
}


template <class Predicate> inline
bool hasDirectChild(const ContainerObject& conObj, Predicate p)
{
    return std::any_of(conObj.refSubFiles  ().begin(), conObj.refSubFiles  ().end(), p) ||
           std::any_of(conObj.refSubLinks  ().begin(), conObj.refSubLinks  ().end(), p) ||
           std::any_of(conObj.refSubFolders().begin(), conObj.refSubFolders().end(), p);
}
}


SyncOperation FileSystemObject::testSyncOperation(SyncDirection testSyncDir) const //semantics: "what if"! assumes "active, no conflict, no recursion (directory)!
{
    return getIsolatedSyncOperation(*this, true, testSyncDir, false);
}

//SyncOperation FolderPair::testSyncOperation() const -> no recursion: we do NOT consider child elements when testing!


SyncOperation FileSystemObject::getSyncOperation() const
{
    return getIsolatedSyncOperation(*this, selectedForSync_, syncDir_, !syncDirectionConflict_.empty());
    //do *not* make a virtual call to testSyncOperation()! See FilePair::testSyncOperation()! <- better not implement one in terms of the other!!!
}


SyncOperation FolderPair::getSyncOperation() const
{
    if (!syncOpBuffered_) //redetermine...
    {
        //suggested operation *not* considering child elements
        syncOpBuffered_ = FileSystemObject::getSyncOperation();

        //action for child elements may occassionally have to overwrite parent task:
        switch (*syncOpBuffered_)
        {
            case SO_OVERWRITE_LEFT:
            case SO_OVERWRITE_RIGHT:
            case SO_MOVE_LEFT_FROM:
            case SO_MOVE_LEFT_TO:
            case SO_MOVE_RIGHT_FROM:
            case SO_MOVE_RIGHT_TO:
                assert(false);
                [[fallthrough]];
            case SO_CREATE_LEFT:
            case SO_CREATE_RIGHT:
            case SO_RENAME_LEFT:
            case SO_RENAME_RIGHT:
            case SO_EQUAL:
                break; //take over suggestion, no problem for child-elements
            case SO_DELETE_LEFT:
            case SO_DELETE_RIGHT:
            case SO_DO_NOTHING:
            case SO_UNRESOLVED_CONFLICT:
                if (isEmpty<SelectSide::left>())
                {
                    //1. if at least one child-element is to be created, make sure parent folder is created also
                    //note: this automatically fulfills "create parent folders even if excluded"
                    if (hasDirectChild(*this, [](const FileSystemObject& fsObj)
                {
                    assert(!fsObj.isPairEmpty() || fsObj.getSyncOperation() == SO_DO_NOTHING);
                        const SyncOperation op = fsObj.getSyncOperation();
                        return op == SO_CREATE_LEFT ||
                               op == SO_MOVE_LEFT_TO;
                    }))
                    syncOpBuffered_ = SO_CREATE_LEFT;
                    //2. cancel parent deletion if only a single child is not also scheduled for deletion
                    else if (*syncOpBuffered_ == SO_DELETE_RIGHT &&
                             hasDirectChild(*this, [](const FileSystemObject& fsObj)
                {
                    if (fsObj.isPairEmpty())
                            return false; //fsObj may already be empty because it once contained a "move source"
                        const SyncOperation op = fsObj.getSyncOperation();
                        return op != SO_DELETE_RIGHT &&
                               op != SO_MOVE_RIGHT_FROM;
                    }))
                    syncOpBuffered_ = SO_DO_NOTHING;
                }
                else if (isEmpty<SelectSide::right>())
                {
                    if (hasDirectChild(*this, [](const FileSystemObject& fsObj)
                {
                    assert(!fsObj.isPairEmpty() || fsObj.getSyncOperation() == SO_DO_NOTHING);
                        const SyncOperation op = fsObj.getSyncOperation();
                        return  op == SO_CREATE_RIGHT ||
                                op == SO_MOVE_RIGHT_TO;
                    }))
                    syncOpBuffered_ = SO_CREATE_RIGHT;
                    else if (*syncOpBuffered_ == SO_DELETE_LEFT &&
                             hasDirectChild(*this, [](const FileSystemObject& fsObj)
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


inline //called by private only!
SyncOperation FilePair::applyMoveOptimization(SyncOperation op) const
{
    /* check whether we can optimize "create + delete" via "move":
       note: as long as we consider "create + delete" cases only, detection of renamed files, should be fine even for "binary" comparison variant!       */
    if (moveFileRef_)
        if (auto refFile = dynamic_cast<const FilePair*>(FileSystemObject::retrieve(moveFileRef_)))
        {
            if (refFile->moveFileRef_ == getId()) //both ends should agree...
            {
                const SyncOperation opRef = refFile->FileSystemObject::getSyncOperation(); //do *not* make a virtual call!
                if (op    == SO_CREATE_LEFT &&
                    opRef == SO_DELETE_LEFT)
                    op = SO_MOVE_LEFT_TO;
                else if (op    == SO_DELETE_LEFT &&
                         opRef == SO_CREATE_LEFT)
                    op = SO_MOVE_LEFT_FROM;
                else if (op    == SO_CREATE_RIGHT &&
                         opRef == SO_DELETE_RIGHT)
                    op = SO_MOVE_RIGHT_TO;
                else if (op    == SO_DELETE_RIGHT &&
                         opRef == SO_CREATE_RIGHT)
                    op = SO_MOVE_RIGHT_FROM;
            }
            else assert(false); //...and why shouldn't they?
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


std::wstring fff::getCategoryDescription(CompareFileResult cmpRes)
{
    switch (cmpRes)
    {
        case FILE_EQUAL:
            return _("Both sides are equal");
        case FILE_RENAMED:
            return _("Items differ in name only");
        case FILE_LEFT_ONLY:
            return _("Item exists on left side only");
        case FILE_RIGHT_ONLY:
            return _("Item exists on right side only");
        case FILE_LEFT_NEWER:
            return _("Left side is newer");
        case FILE_RIGHT_NEWER:
            return _("Right side is newer");
        case FILE_DIFFERENT_CONTENT:
            return _("Items have different content");
        case FILE_TIME_INVALID:
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
//const wchar_t arrowRight[] = L"\u2192"; unicode arrows -> too small
}


std::wstring fff::getCategoryDescription(const FileSystemObject& fsObj)
{
    const std::wstring footer = [&]
    {
        if (fsObj.hasEquivalentItemNames())
            return L'\n' + fmtPath(fsObj.getItemName<SelectSide::left>());
        else
            return std::wstring(L"\n") +
            fmtPath(fsObj.getItemName<SelectSide::left >()) + L' ' + arrowLeft + L'\n' +
            fmtPath(fsObj.getItemName<SelectSide::right>()) + L' ' + arrowRight;
    }();

    if (const Zstringc descr = fsObj.getCategoryCustomDescription();
        !descr.empty())
        return utfTo<std::wstring>(descr) + footer;

    const CompareFileResult cmpRes = fsObj.getCategory();
    switch (cmpRes)
    {
        case FILE_EQUAL:
        case FILE_RENAMED:
        case FILE_LEFT_ONLY:
        case FILE_RIGHT_ONLY:
        case FILE_DIFFERENT_CONTENT:
            return getCategoryDescription(cmpRes) + footer; //use generic description

        case FILE_LEFT_NEWER:
        case FILE_RIGHT_NEWER:
        {
            std::wstring descr = getCategoryDescription(cmpRes);

            visitFSObject(fsObj, [](const FolderPair& folder) {},
            [&](const FilePair& file)
            {
                descr += std::wstring(L"\n") +
                         formatUtcToLocalTime(file.getLastWriteTime<SelectSide::left >()) + L' ' + arrowLeft + L'\n' +
                         formatUtcToLocalTime(file.getLastWriteTime<SelectSide::right>()) + L' ' + arrowRight ;
            },
            [&](const SymlinkPair& symlink)
            {
                descr += std::wstring(L"\n") +
                         formatUtcToLocalTime(symlink.getLastWriteTime<SelectSide::left >()) + L' ' + arrowLeft + L'\n' +
                         formatUtcToLocalTime(symlink.getLastWriteTime<SelectSide::right>()) + L' ' + arrowRight ;
            });
            return descr + footer;
        }

        case FILE_TIME_INVALID:
        case FILE_CONFLICT:
            assert(false); //should have getCategoryDescription()!
            return _("Error") + footer;
    }
    assert(false);
    return std::wstring();
}


std::wstring fff::getSyncOpDescription(SyncOperation op)
{
    switch (op)
    {
        case SO_CREATE_LEFT:
            return _("Copy new item to left");
        case SO_CREATE_RIGHT:
            return _("Copy new item to right");
        case SO_DELETE_LEFT:
            return _("Delete left item");
        case SO_DELETE_RIGHT:
            return _("Delete right item");
        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_LEFT_TO:
            return _("Move left file"); //move only supported for files
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_RIGHT_TO:
            return _("Move right file");
        case SO_OVERWRITE_LEFT:
            return _("Update left item");
        case SO_OVERWRITE_RIGHT:
            return _("Update right item");
        case SO_DO_NOTHING:
            return _("Do nothing");
        case SO_EQUAL:
            return _("Both sides are equal");
        case SO_RENAME_LEFT:
            return _("Rename left item");
        case SO_RENAME_RIGHT:
            return _("Rename right item");
        case SO_UNRESOLVED_CONFLICT: //not used on GUI, but in .csv
            return _("Conflict/item cannot be categorized");
    }
    assert(false);
    return std::wstring();
}


std::wstring fff::getSyncOpDescription(const FileSystemObject& fsObj)
{
    const SyncOperation op = fsObj.getSyncOperation();

    const std::wstring rightArrowDown = languageLayoutIsRtl() ?
                                        std::wstring() + RTL_MARK + LEFT_ARROW_ANTICLOCK :
                                        std::wstring() + LTR_MARK + RIGHT_ARROW_CURV_DOWN;
    //Windows bug: RIGHT_ARROW_CURV_DOWN rendering and extent calculation is buggy (see wx+\tooltip.cpp) => need LTR mark!

    auto generateFooter = [&]
    {
        if (fsObj.hasEquivalentItemNames())
            return L'\n' + fmtPath(fsObj.getItemName<SelectSide::left>());

        Zstring itemNameNew = fsObj.getItemName<SelectSide::left >();
        Zstring itemNameOld = fsObj.getItemName<SelectSide::right>();

        if (const SyncDirection dir = getEffectiveSyncDir(op);
            dir != SyncDirection::none)
        {
            if (dir == SyncDirection::left)
                std::swap(itemNameNew, itemNameOld);

            return L'\n' + fmtPath(itemNameOld) + L' ' + rightArrowDown + L'\n' + fmtPath(itemNameNew);
        }
        else
            return  L'\n' +
            fmtPath(itemNameNew) + L' ' + arrowLeft + L'\n' +
            fmtPath(itemNameOld) + L' ' + arrowRight;
    };

    switch (op)
    {
        case SO_CREATE_LEFT:
        case SO_CREATE_RIGHT:
        case SO_DELETE_LEFT:
        case SO_DELETE_RIGHT:
        case SO_OVERWRITE_LEFT:
        case SO_OVERWRITE_RIGHT:
        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_RENAME_LEFT:
        case SO_RENAME_RIGHT:
            return getSyncOpDescription(op) + generateFooter();

        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_LEFT_TO:
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_RIGHT_TO:
            if (auto fileFrom = dynamic_cast<const FilePair*>(&fsObj))
                if (auto fileTo = dynamic_cast<const FilePair*>(FileSystemObject::retrieve(fileFrom->getMoveRef())))
                {
                    assert(fileTo->getMoveRef() == fileFrom->getId());
                    const bool onLeft       = op == SO_MOVE_LEFT_FROM || op == SO_MOVE_LEFT_TO;
                    const bool isMoveSource = op == SO_MOVE_LEFT_FROM || op == SO_MOVE_RIGHT_FROM;

                    if (!isMoveSource)
                        std::swap(fileFrom, fileTo);

                    auto getRelPath = [&](const FileSystemObject& fso) { return onLeft ? fso.getRelativePath<SelectSide::left>() : fso.getRelativePath<SelectSide::right>(); };

                    const Zstring relPathFrom = getRelPath(*fileFrom);
                    const Zstring relPathTo   = getRelPath(*fileTo);

                    //attention: ::SetWindowText() doesn't handle tab characters correctly in combination with certain file names, so don't use
                    return getSyncOpDescription(op) + L'\n' +
                           (beforeLast(relPathFrom, FILE_NAME_SEPARATOR, IfNotFoundReturn::none) ==
                            beforeLast(relPathTo,   FILE_NAME_SEPARATOR, IfNotFoundReturn::none) ?
                            //detected pure "rename"
                            fmtPath(getItemName(relPathFrom)) + L' ' + rightArrowDown + L'\n' + //show file name only
                            fmtPath(getItemName(relPathTo)) :
                            //"move" or "move + rename"
                            fmtPath(relPathFrom) + L' ' + rightArrowDown + L'\n' +
                            fmtPath(relPathTo));
                }
            break;

        case SO_UNRESOLVED_CONFLICT:
            return fsObj.getSyncOpConflict() + generateFooter();
    }

    assert(false);
    return std::wstring();
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_HIERARCHY_H_257235289645296
#define FILE_HIERARCHY_H_257235289645296

#include <map>
#include <cstddef> //required by GCC 4.8.1 to find ptrdiff_t
#include <string>
#include <memory>
#include <functional>
#include <unordered_set>
#include <zen/zstring.h>
#include <zen/fixed_list.h>
#include <zen/stl_tools.h>
#include <zen/file_id_def.h>
#include "structures.h"
#include "lib/hard_filter.h"
#include "fs/abstract.h"


namespace fff
{
using AFS = AbstractFileSystem;

struct FileAttributes
{
    FileAttributes() {}
    FileAttributes(time_t modTimeIn,
                   uint64_t fileSizeIn,
                   const AFS::FileId& idIn,
                   bool isSymlink) :
        modTime(modTimeIn),
        fileSize(fileSizeIn),
        fileId(idIn),
        isFollowedSymlink(isSymlink)
    {
        static_assert(std::is_signed<time_t>::value, "... and signed!");
    }

    time_t modTime = 0; //number of seconds since Jan. 1st 1970 UTC
    uint64_t fileSize = 0;
    AFS::FileId fileId; //optional!
    bool isFollowedSymlink = false;
};


struct LinkAttributes
{
    LinkAttributes() {}
    explicit LinkAttributes(time_t modTimeIn) : modTime(modTimeIn) {}

    time_t modTime = 0; //number of seconds since Jan. 1st 1970 UTC
};


struct FolderAttributes
{
    FolderAttributes() {}
    FolderAttributes(bool isSymlink) :
        isFollowedSymlink(isSymlink) {}

    bool isFollowedSymlink = false;
};


enum SelectedSide
{
    LEFT_SIDE,
    RIGHT_SIDE
};

template <SelectedSide side>
struct OtherSide;

template <>
struct OtherSide<LEFT_SIDE> { static const SelectedSide result = RIGHT_SIDE; };

template <>
struct OtherSide<RIGHT_SIDE> { static const SelectedSide result = LEFT_SIDE; };


template <SelectedSide side>
struct SelectParam;

template <>
struct SelectParam<LEFT_SIDE>
{
    template <class T>
    static T& ref(T& left, T& right) { return left; }
};

template <>
struct SelectParam<RIGHT_SIDE>
{
    template <class T>
    static T& ref(T& left, T& right) { return right; }
};

//------------------------------------------------------------------

struct FolderContainer
{
    //------------------------------------------------------------------
    using FolderList  = std::map<Zstring, std::pair<FolderAttributes, FolderContainer>, LessFilePath>; //
    using FileList    = std::map<Zstring, FileAttributes,  LessFilePath>; //key: file name
    using SymlinkList = std::map<Zstring, LinkAttributes,  LessFilePath>; //
    //------------------------------------------------------------------

    FolderContainer() = default;
    FolderContainer           (const FolderContainer&) = delete; //catch accidental (and unnecessary) copying
    FolderContainer& operator=(const FolderContainer&) = delete; //

    FileList    files;
    SymlinkList symlinks; //non-followed symlinks
    FolderList  folders;

    void addSubFile(const Zstring& itemName, const FileAttributes& attr)
    {
        auto rv = files.emplace(itemName, attr);
        if (!rv.second) //update entry if already existing (e.g. during folder traverser "retry") => does not handle different item name case (irrelvant!..)
            rv.first->second = attr;
    }

    void addSubLink(const Zstring& itemName, const LinkAttributes& attr)
    {
        auto rv = symlinks.emplace(itemName, attr);
        if (!rv.second)
            rv.first->second = attr;
    }

    FolderContainer& addSubFolder(const Zstring& itemName, const FolderAttributes& attr)
    {
        auto& p = folders[itemName]; //value default-construction is okay here
        p.first = attr;
        return p.second;

        //auto rv = folders.emplace(itemName, std::pair<FolderAttributes, FolderContainer>(attr, FolderContainer()));
        //if (!rv.second)
        //  rv.first->second.first = attr;
        //return rv.first->second.second;
    }
};

class BaseFolderPair;
class FolderPair;
class FilePair;
class SymlinkPair;
class FileSystemObject;

/*------------------------------------------------------------------
    inheritance diagram:

         ObjectMgr        PathInformation
            /|\                 /|\
             |________  _________|_________
                      ||                   |
               FileSystemObject     ContainerObject
                     /|\                  /|\
           ___________|___________   ______|______
          |           |           | |             |
     SymlinkPair   FilePair    FolderPair   BaseFolderPair

------------------------------------------------------------------*/

struct PathInformation //diamond-shaped inheritence!
{
    virtual ~PathInformation() {}

    template <SelectedSide side> AbstractPath getAbstractPath() const;
    template <SelectedSide side> Zstring      getRelativePath() const; //get path relative to base sync dir (without leading/trailing FILE_NAME_SEPARATOR)

    Zstring getPairRelativePath() const;

private:
    virtual AbstractPath getAbstractPathL() const = 0; //implemented by FileSystemObject + BaseFolderPair
    virtual AbstractPath getAbstractPathR() const = 0; //

    virtual Zstring getRelativePathL() const = 0; //implemented by SymlinkPair/FilePair + ContainerObject
    virtual Zstring getRelativePathR() const = 0; //
};

template <> inline AbstractPath PathInformation::getAbstractPath<LEFT_SIDE >() const { return getAbstractPathL(); }
template <> inline AbstractPath PathInformation::getAbstractPath<RIGHT_SIDE>() const { return getAbstractPathR(); }

template <> inline Zstring PathInformation::getRelativePath<LEFT_SIDE >() const { return getRelativePathL(); }
template <> inline Zstring PathInformation::getRelativePath<RIGHT_SIDE>() const { return getRelativePathR(); }

inline Zstring PathInformation::getPairRelativePath() const { return getRelativePathL(); } //side doesn't matter

//------------------------------------------------------------------

class ContainerObject : public virtual PathInformation
{
    friend class FolderPair;
    friend class FileSystemObject;

public:
    using FileList    = zen::FixedList<FilePair>;    //MergeSides::execute() requires a structure that doesn't invalidate pointers after push_back()
    using SymlinkList = zen::FixedList<SymlinkPair>; //
    using FolderList  = zen::FixedList<FolderPair>;

    FolderPair& addSubFolder(const Zstring&          itemNameL,
                             const FolderAttributes& left,    //file exists on both sides
                             CompareDirResult        defaultCmpResult,
                             const Zstring&          itemNameR,
                             const FolderAttributes& right);

    template <SelectedSide side>
    FolderPair& addSubFolder(const Zstring& itemName, //dir exists on one side only
                             const FolderAttributes& attr);

    FilePair& addSubFile(const Zstring&        itemNameL,
                         const FileAttributes& left,          //file exists on both sides
                         CompareFilesResult    defaultCmpResult,
                         const Zstring&        itemNameR,
                         const FileAttributes& right);

    template <SelectedSide side>
    FilePair& addSubFile(const Zstring&        itemName, //file exists on one side only
                         const FileAttributes& attr);

    SymlinkPair& addSubLink(const Zstring&        itemNameL,
                            const LinkAttributes& left, //link exists on both sides
                            CompareSymlinkResult  defaultCmpResult,
                            const Zstring&        itemNameR,
                            const LinkAttributes& right);

    template <SelectedSide side>
    SymlinkPair& addSubLink(const Zstring&        itemName, //link exists on one side only
                            const LinkAttributes& attr);

    const FileList& refSubFiles() const { return subFiles_; }
    /**/  FileList& refSubFiles()       { return subFiles_; }

    const SymlinkList& refSubLinks() const { return subLinks_; }
    /**/  SymlinkList& refSubLinks()       { return subLinks_; }

    const FolderList& refSubFolders() const { return subFolders_; }
    /**/  FolderList& refSubFolders()       { return subFolders_; }

    BaseFolderPair& getBase() { return base_; }

protected:
    ContainerObject(BaseFolderPair& baseFolder) : //used during BaseFolderPair constructor
        base_(baseFolder) {} //take reference only: baseFolder *not yet* fully constructed at this point!

    ContainerObject(const FileSystemObject& fsAlias); //used during FolderPair constructor

    virtual ~ContainerObject() {} //don't need polymorphic deletion, but we have a vtable anyway

    virtual void flip();

    void removeEmptyRec();

    template <SelectedSide side>
    void updateRelPathsRecursion(const FileSystemObject& fsAlias);

private:
    ContainerObject           (const ContainerObject&) = delete; //this class is referenced by its child elements => make it non-copyable/movable!
    ContainerObject& operator=(const ContainerObject&) = delete;

    virtual void notifySyncCfgChanged() {}

    Zstring getRelativePathL() const override { return relPathL_; }
    Zstring getRelativePathR() const override { return relPathR_; }

    FileList    subFiles_;
    SymlinkList subLinks_;
    FolderList  subFolders_;

    Zstring relPathL_; //path relative to base sync dir (without leading/trailing FILE_NAME_SEPARATOR)
    Zstring relPathR_; //

    BaseFolderPair& base_;
};

//------------------------------------------------------------------

class BaseFolderPair : public ContainerObject //synchronization base directory
{
public:
    BaseFolderPair(const AbstractPath& folderPathLeft,
                   bool folderAvailableLeft,
                   const AbstractPath& folderPathRight,
                   bool folderAvailableRight,
                   const HardFilter::FilterRef& filter,
                   CompareVariant cmpVar,
                   int fileTimeTolerance,
                   const std::vector<unsigned int>& ignoreTimeShiftMinutes) :
        ContainerObject(*this), //trust that ContainerObject knows that *this is not yet fully constructed!
        filter_(filter), cmpVar_(cmpVar), fileTimeTolerance_(fileTimeTolerance), ignoreTimeShiftMinutes_(ignoreTimeShiftMinutes),
        folderAvailableLeft_ (folderAvailableLeft),
        folderAvailableRight_(folderAvailableRight),
        folderPathLeft_(folderPathLeft),
        folderPathRight_(folderPathRight) {}

    static void removeEmpty(BaseFolderPair& baseFolder) { baseFolder.removeEmptyRec(); } //physically remove all invalid entries (where both sides are empty) recursively

    template <SelectedSide side> bool isAvailable() const; //base folder status at the time of comparison!
    template <SelectedSide side> void setAvailable(bool value); //update after creating the directory in FFS

    //get settings which were used while creating BaseFolderPair
    const HardFilter&   getFilter() const { return *filter_; }
    CompareVariant getCompVariant() const { return cmpVar_; }
    int  getFileTimeTolerance() const { return fileTimeTolerance_; }
    const std::vector<unsigned int>& getIgnoredTimeShift() const { return ignoreTimeShiftMinutes_; }

    void flip() override;

private:
    AbstractPath getAbstractPathL() const override { return folderPathLeft_; }
    AbstractPath getAbstractPathR() const override { return folderPathRight_; }

    const HardFilter::FilterRef filter_; //filter used while scanning directory: represents sub-view of actual files!
    const CompareVariant cmpVar_;
    const int fileTimeTolerance_;
    const std::vector<unsigned int> ignoreTimeShiftMinutes_;

    bool folderAvailableLeft_;
    bool folderAvailableRight_;

    AbstractPath folderPathLeft_;
    AbstractPath folderPathRight_;
};


//get rid of shared_ptr indirection
template <class IterImpl, //underlying iterator type
          class V>        //target value type
class DerefIter : public std::iterator<std::bidirectional_iterator_tag, V>
{
public:
    DerefIter() {}
    DerefIter(IterImpl it) : it_(it) {}
    DerefIter(const DerefIter& other) : it_(other.it_) {}
    DerefIter& operator++() { ++it_; return *this; }
    DerefIter& operator--() { --it_; return *this; }
    inline friend DerefIter operator++(DerefIter& it, int) { return it++; }
    inline friend DerefIter operator--(DerefIter& it, int) { return it--; }
    inline friend ptrdiff_t operator-(const DerefIter& lhs, const DerefIter& rhs) { return lhs.it_ - rhs.it_; }
    inline friend bool operator==(const DerefIter& lhs, const DerefIter& rhs) { return lhs.it_ == rhs.it_; }
    inline friend bool operator!=(const DerefIter& lhs, const DerefIter& rhs) { return !(lhs == rhs); }
    V& operator* () const { return  **it_; }
    V* operator->() const { return &** it_; }
private:
    IterImpl it_{};
};

/*
C++17: specialize std::iterator_traits instead of inherting from std::iterator
namespace std
{
template <class IterImpl, class V>
struct iterator_traits<zen::DerefIter<IterImpl, V>>
{
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = V;
};
}
*/

using FolderComparison = std::vector<std::shared_ptr<BaseFolderPair>>; //make sure pointers to sub-elements remain valid
//don't change this back to std::vector<BaseFolderPair> too easily: comparison uses push_back to add entries which may result in a full copy!

DerefIter<typename FolderComparison::iterator,             BaseFolderPair> inline begin(      FolderComparison& vect) { return vect.begin(); }
DerefIter<typename FolderComparison::iterator,             BaseFolderPair> inline end  (      FolderComparison& vect) { return vect.end  (); }
DerefIter<typename FolderComparison::const_iterator, const BaseFolderPair> inline begin(const FolderComparison& vect) { return vect.begin(); }
DerefIter<typename FolderComparison::const_iterator, const BaseFolderPair> inline end  (const FolderComparison& vect) { return vect.end  (); }

//------------------------------------------------------------------
struct FSObjectVisitor
{
    virtual ~FSObjectVisitor() {}
    virtual void visit(const FilePair&    file   ) = 0;
    virtual void visit(const SymlinkPair& symlink) = 0;
    virtual void visit(const FolderPair&  folder ) = 0;
};


//inherit from this class to allow safe random access by id instead of unsafe raw pointer
//allow for similar semantics like std::weak_ptr without having to use std::shared_ptr
template <class T>
class ObjectMgr
{
public:
    using ObjectId      = ObjectMgr* ;
    using ObjectIdConst = const ObjectMgr*;

    ObjectIdConst  getId() const { return this; }
    /**/  ObjectId getId()       { return this; }

    static const T* retrieve(ObjectIdConst id) //returns nullptr if object is not valid anymore
    {
        const auto& aObj = activeObjects();
        return static_cast<const T*>(aObj.find(id) == aObj.end() ? nullptr : id);
    }
    static T* retrieve(ObjectId id) { return const_cast<T*>(retrieve(static_cast<ObjectIdConst>(id))); }

protected:
    ObjectMgr () { activeObjects().insert(this); }
    ~ObjectMgr() { activeObjects().erase (this); }

private:
    ObjectMgr           (const ObjectMgr& rhs) = delete;
    ObjectMgr& operator=(const ObjectMgr& rhs) = delete; //it's not well-defined what copying an objects means regarding object-identity in this context

    static std::unordered_set<const ObjectMgr*>& activeObjects()
    {
        static std::unordered_set<const ObjectMgr*> inst;
        return inst; //external linkage (even in header file!)
    }

};

//------------------------------------------------------------------

class FileSystemObject : public ObjectMgr<FileSystemObject>, public virtual PathInformation
{
public:
    virtual void accept(FSObjectVisitor& visitor) const = 0;

    Zstring getPairItemName() const; //like getItemName() but without bias to which side is returned
    bool isPairEmpty() const; //true, if both sides are empty

    //path getters always return valid values, even if isEmpty<side>()!
    template <SelectedSide side> Zstring getItemName() const; //case sensitive!
    template <SelectedSide side> bool isEmpty() const;

    //comparison result
    CompareFilesResult getCategory() const { return cmpResult_; }
    std::wstring getCatExtraDescription() const; //only filled if getCategory() == FILE_CONFLICT or FILE_DIFFERENT_METADATA

    //sync settings
    SyncDirection getSyncDir() const { return syncDir_; }
    void setSyncDir(SyncDirection newDir);
    void setSyncDirConflict(const std::wstring& description); //set syncDir = SyncDirection::NONE + fill conflict description

    bool isActive() const { return selectedForSync_; }
    void setActive(bool active);

    //sync operation
    virtual SyncOperation testSyncOperation(SyncDirection testSyncDir) const; //semantics: "what if"! assumes "active, no conflict, no recursion (directory)!
    virtual SyncOperation getSyncOperation() const;
    std::wstring getSyncOpConflict() const; //return conflict when determining sync direction or (still unresolved) conflict during categorization

    template <SelectedSide side> void removeObject();    //removes file or directory (recursively!) without physically removing the element: used by manual deletion

    const ContainerObject& parent() const { return parent_; }
    /**/  ContainerObject& parent()       { return parent_; }
    const BaseFolderPair& base() const  { return parent_.getBase(); }
    /**/  BaseFolderPair& base()        { return parent_.getBase(); }

    //for use during init in "CompareProcess" only:
    template <CompareFilesResult res> void setCategory();
    void setCategoryConflict    (const std::wstring& description);
    void setCategoryDiffMetadata(const std::wstring& description);

protected:
    FileSystemObject(const Zstring& itemNameL,
                     const Zstring& itemNameR,
                     ContainerObject& parentObj,
                     CompareFilesResult defaultCmpResult) :
        cmpResult_(defaultCmpResult),
        itemNameL_(itemNameL),
        itemNameR_(itemNameL == itemNameR ? itemNameL : itemNameR), //perf: no measurable speed drawback; -3% peak memory => further needed by ContainerObject construction!
        parent_(parentObj)
    {
        assert(itemNameL_.c_str() == itemNameR_.c_str() || itemNameL_ != itemNameR_); //also checks ref-counted string precondition
        parent_.notifySyncCfgChanged();
    }

    virtual ~FileSystemObject() {} //don't need polymorphic deletion, but we have a vtable anyway
    //must not call parent here, it is already partially destroyed and nothing more than a pure ContainerObject!

    virtual void flip();
    virtual void notifySyncCfgChanged() { parent().notifySyncCfgChanged(); /*propagate!*/ }

    void setSynced(const Zstring& itemName);

private:
    FileSystemObject           (const FileSystemObject&) = delete;
    FileSystemObject& operator=(const FileSystemObject&) = delete;

    AbstractPath getAbstractPathL() const override { return AFS::appendRelPath(base().getAbstractPath<LEFT_SIDE >(), getRelativePath<LEFT_SIDE >()); }
    AbstractPath getAbstractPathR() const override { return AFS::appendRelPath(base().getAbstractPath<RIGHT_SIDE>(), getRelativePath<RIGHT_SIDE>()); }

    virtual void removeObjectL() = 0;
    virtual void removeObjectR() = 0;

    template <SelectedSide side>
    void propagateChangedItemName(const Zstring& itemNameOld); //required after any itemName changes

    //categorization
    std::unique_ptr<std::wstring> cmpResultDescr_; //only filled if getCategory() == FILE_CONFLICT or FILE_DIFFERENT_METADATA
    CompareFilesResult cmpResult_; //although this uses 4 bytes there is currently *no* space wasted in class layout!

    bool selectedForSync_ = true;

    //Note: we model *four* states with following two variables => "syncDirectionConflict is empty or syncDir == NONE" is a class invariant!!!
    SyncDirection syncDir_ = SyncDirection::NONE; //1 byte: optimize memory layout!
    std::unique_ptr<std::wstring> syncDirectionConflict_; //non-empty if we have a conflict setting sync-direction
    //get rid of std::wstring small string optimization (consumes 32/48 byte on VS2010 x86/x64!)

    Zstring itemNameL_; //slightly redundant under Linux, but on Windows the "same" file paths can differ in case
    Zstring itemNameR_; //use as indicator: an empty name means: not existing on this side!

    ContainerObject& parent_;
};

//------------------------------------------------------------------


class FolderPair : public FileSystemObject, public ContainerObject
{
    friend class ContainerObject;

public:
    void accept(FSObjectVisitor& visitor) const override;

    CompareDirResult getDirCategory() const; //returns actually used subset of CompareFilesResult

    FolderPair(const Zstring& itemNameL, //use empty itemName if "not existing"
               const FolderAttributes& attrL,
               CompareDirResult defaultCmpResult,
               const Zstring& itemNameR,
               const FolderAttributes& attrR,
               ContainerObject& parentObj) :
        FileSystemObject(itemNameL, itemNameR, parentObj, static_cast<CompareFilesResult>(defaultCmpResult)),
        ContainerObject(static_cast<FileSystemObject&>(*this)), //FileSystemObject fully constructed at this point!
        attrL_(attrL),
        attrR_(attrR) {}

    template <SelectedSide side> bool isFollowedSymlink() const;

    SyncOperation getSyncOperation() const override;

    template <SelectedSide sideTrg>
    void setSyncedTo(const Zstring& itemName, bool isSymlinkTrg, bool isSymlinkSrc); //call after sync, sets DIR_EQUAL

private:
    void flip         () override;
    void removeObjectL() override;
    void removeObjectR() override;
    void notifySyncCfgChanged() override { syncOpBuffered_ = zen::NoValue(); FileSystemObject::notifySyncCfgChanged(); ContainerObject::notifySyncCfgChanged(); }

    mutable zen::Opt<SyncOperation> syncOpBuffered_; //determining sync-op for directory may be expensive as it depends on child-objects => buffer

    FolderAttributes attrL_;
    FolderAttributes attrR_;
};


//------------------------------------------------------------------

class FilePair : public FileSystemObject
{
    friend class ContainerObject; //construction

public:
    void accept(FSObjectVisitor& visitor) const override;

    FilePair(const Zstring&        itemNameL, //use empty string if "not existing"
             const FileAttributes& attrL,
             CompareFilesResult    defaultCmpResult,
             const Zstring&        itemNameR, //
             const FileAttributes& attrR,
             ContainerObject& parentObj) :
        FileSystemObject(itemNameL, itemNameR, parentObj, defaultCmpResult),
        attrL_(attrL),
        attrR_(attrR) {}

    template <SelectedSide side> time_t      getLastWriteTime() const;
    template <SelectedSide side> uint64_t         getFileSize() const;
    template <SelectedSide side> AFS::FileId        getFileId() const;
    template <SelectedSide side> bool       isFollowedSymlink() const;
    template <SelectedSide side> FileAttributes getAttributes() const;

    void setMoveRef(ObjectId refId) { moveFileRef_ = refId; } //reference to corresponding renamed file
    ObjectId getMoveRef() const { return moveFileRef_; } //may be nullptr

    CompareFilesResult getFileCategory() const;

    SyncOperation testSyncOperation(SyncDirection testSyncDir) const override; //semantics: "what if"! assumes "active, no conflict, no recursion (directory)!
    SyncOperation getSyncOperation() const override;

    template <SelectedSide sideTrg>
    void setSyncedTo(const Zstring& itemName, //call after sync, sets FILE_EQUAL
                     uint64_t fileSize,
                     int64_t lastWriteTimeTrg,
                     int64_t lastWriteTimeSrc,
                     const AFS::FileId& fileIdTrg,
                     const AFS::FileId& fileIdSrc,
                     bool isSymlinkTrg,
                     bool isSymlinkSrc);

private:
    Zstring getRelativePathL() const override { return AFS::appendPaths(parent().getRelativePath<LEFT_SIDE >(), getItemName<LEFT_SIDE >(), FILE_NAME_SEPARATOR); }
    Zstring getRelativePathR() const override { return AFS::appendPaths(parent().getRelativePath<RIGHT_SIDE>(), getItemName<RIGHT_SIDE>(), FILE_NAME_SEPARATOR); }

    SyncOperation applyMoveOptimization(SyncOperation op) const;

    void flip         () override;
    void removeObjectL() override { attrL_ = FileAttributes(); }
    void removeObjectR() override { attrR_ = FileAttributes(); }

    FileAttributes attrL_;
    FileAttributes attrR_;

    ObjectId moveFileRef_ = nullptr; //optional, filled by redetermineSyncDirection()
};

//------------------------------------------------------------------

class SymlinkPair : public FileSystemObject //this class models a TRUE symbolic link, i.e. one that is NEVER dereferenced: deref-links should be directly placed in class File/FolderPair
{
    friend class ContainerObject; //construction

public:
    void accept(FSObjectVisitor& visitor) const override;

    template <SelectedSide side> time_t getLastWriteTime() const; //write time of the link, NOT target!

    CompareSymlinkResult getLinkCategory()   const; //returns actually used subset of CompareFilesResult

    SymlinkPair(const Zstring&         itemNameL, //use empty string if "not existing"
                const LinkAttributes&  attrL,
                CompareSymlinkResult   defaultCmpResult,
                const Zstring&         itemNameR, //use empty string if "not existing"
                const LinkAttributes&  attrR,
                ContainerObject& parentObj) :
        FileSystemObject(itemNameL, itemNameR, parentObj, static_cast<CompareFilesResult>(defaultCmpResult)),
        attrL_(attrL),
        attrR_(attrR) {}

    template <SelectedSide sideTrg>
    void setSyncedTo(const Zstring& itemName, //call after sync, sets SYMLINK_EQUAL
                     int64_t lastWriteTimeTrg,
                     int64_t lastWriteTimeSrc);

private:
    Zstring getRelativePathL() const override { return AFS::appendPaths(parent().getRelativePath<LEFT_SIDE >(), getItemName<LEFT_SIDE >(), FILE_NAME_SEPARATOR); }
    Zstring getRelativePathR() const override { return AFS::appendPaths(parent().getRelativePath<RIGHT_SIDE>(), getItemName<RIGHT_SIDE>(), FILE_NAME_SEPARATOR); }

    void flip()          override;
    void removeObjectL() override { attrL_ = LinkAttributes(); }
    void removeObjectR() override { attrR_ = LinkAttributes(); }

    LinkAttributes attrL_;
    LinkAttributes attrR_;
};

//------------------------------------------------------------------

//generic type descriptions (usecase CSV legend, sync config)
std::wstring getCategoryDescription(CompareFilesResult cmpRes);
std::wstring getSyncOpDescription  (SyncOperation op);

//item-specific type descriptions
std::wstring getCategoryDescription(const FileSystemObject& fsObj);
std::wstring getSyncOpDescription  (const FileSystemObject& fsObj);

//------------------------------------------------------------------

template <class Function1, class Function2, class Function3>
struct FSObjectLambdaVisitor : public FSObjectVisitor
{
    FSObjectLambdaVisitor(Function1 onFolder,
                          Function2 onFile,
                          Function3 onSymlink) : onFolder_(onFolder), onFile_(onFile), onSymlink_(onSymlink) {}
private:
    void visit(const FolderPair&  folder) override { onFolder_ (folder); }
    void visit(const FilePair&    file  ) override { onFile_   (file); }
    void visit(const SymlinkPair& link  ) override { onSymlink_(link); }

    Function1 onFolder_;
    Function2 onFile_;
    Function3 onSymlink_;
};

template <class Function1, class Function2, class Function3> inline
void visitFSObject(const FileSystemObject& fsObj, Function1 onFolder, Function2 onFile, Function3 onSymlink)
{
    FSObjectLambdaVisitor<Function1, Function2, Function3> visitor(onFolder, onFile, onSymlink);
    fsObj.accept(visitor);
}




















//--------------------- implementation ------------------------------------------

//inline virtual... admittedly its use may be limited
inline void FilePair   ::accept(FSObjectVisitor& visitor) const { visitor.visit(*this); }
inline void FolderPair ::accept(FSObjectVisitor& visitor) const { visitor.visit(*this); }
inline void SymlinkPair::accept(FSObjectVisitor& visitor) const { visitor.visit(*this); }


inline
CompareFilesResult FilePair::getFileCategory() const
{
    return getCategory();
}


inline
CompareDirResult FolderPair::getDirCategory() const
{
    return static_cast<CompareDirResult>(getCategory());
}


inline
std::wstring FileSystemObject::getCatExtraDescription() const
{
    assert(getCategory() == FILE_CONFLICT || getCategory() == FILE_DIFFERENT_METADATA);
    if (cmpResultDescr_) //avoid ternary-WTF! (implicit copy-constructor call!!!!!!)
        return *cmpResultDescr_;
    return std::wstring();
}


inline
void FileSystemObject::setSyncDir(SyncDirection newDir)
{
    syncDir_ = newDir;
    syncDirectionConflict_.reset();

    notifySyncCfgChanged();
}


inline
void FileSystemObject::setSyncDirConflict(const std::wstring& description)
{
    syncDir_ = SyncDirection::NONE;
    syncDirectionConflict_ = std::make_unique<std::wstring>(description);

    notifySyncCfgChanged();
}


inline
std::wstring FileSystemObject::getSyncOpConflict() const
{
    assert(getSyncOperation() == SO_UNRESOLVED_CONFLICT);
    if (syncDirectionConflict_) //avoid ternary-WTF! (implicit copy-constructor call!!!!!!)
        return *syncDirectionConflict_;
    return std::wstring();
}


inline
void FileSystemObject::setActive(bool active)
{
    selectedForSync_ = active;
    notifySyncCfgChanged();
}


template <SelectedSide side> inline
bool FileSystemObject::isEmpty() const
{
    return SelectParam<side>::ref(itemNameL_, itemNameR_).empty();
}


inline
bool FileSystemObject::isPairEmpty() const
{
    return isEmpty<LEFT_SIDE>() && isEmpty<RIGHT_SIDE>();
}


template <SelectedSide side> inline
Zstring FileSystemObject::getItemName() const
{
    //assert(!itemNameL_.empty() || !itemNameR_.empty()); -> file pair might be empty (until removed after sync)

    const Zstring& itemName = SelectParam<side>::ref(itemNameL_, itemNameR_); //empty if not existing
    if (!itemName.empty()) //avoid ternary-WTF! (implicit copy-constructor call!!!!!!)
        return itemName;
    return SelectParam<OtherSide<side>::result>::ref(itemNameL_, itemNameR_); //empty if not existing
}


inline
Zstring FileSystemObject::getPairItemName() const
{
    return getItemName<LEFT_SIDE>(); //side doesn't matter
}


template <> inline
void FileSystemObject::removeObject<LEFT_SIDE>()
{
    const Zstring itemNameOld = getItemName<LEFT_SIDE>();

    cmpResult_ = isEmpty<RIGHT_SIDE>() ? FILE_EQUAL : FILE_RIGHT_SIDE_ONLY;
    itemNameL_.clear();
    removeObjectL();

    setSyncDir(SyncDirection::NONE); //calls notifySyncCfgChanged()
    propagateChangedItemName<LEFT_SIDE>(itemNameOld);
}


template <> inline
void FileSystemObject::removeObject<RIGHT_SIDE>()
{
    const Zstring itemNameOld = getItemName<RIGHT_SIDE>();

    cmpResult_ = isEmpty<LEFT_SIDE>() ? FILE_EQUAL : FILE_LEFT_SIDE_ONLY;
    itemNameR_.clear();
    removeObjectR();

    setSyncDir(SyncDirection::NONE); //calls notifySyncCfgChanged()
    propagateChangedItemName<RIGHT_SIDE>(itemNameOld);
}


inline
void FileSystemObject::setSynced(const Zstring& itemName)
{
    const Zstring itemNameOldL = getItemName<LEFT_SIDE>();
    const Zstring itemNameOldR = getItemName<RIGHT_SIDE>();

    assert(!isPairEmpty());
    itemNameR_ = itemNameL_ = itemName;
    cmpResult_ = FILE_EQUAL;
    setSyncDir(SyncDirection::NONE);

    propagateChangedItemName<LEFT_SIDE >(itemNameOldL);
    propagateChangedItemName<RIGHT_SIDE>(itemNameOldR);
}


template <CompareFilesResult res> inline
void FileSystemObject::setCategory()
{
    cmpResult_ = res;
}
template <> void FileSystemObject::setCategory<FILE_CONFLICT>          () = delete; //
template <> void FileSystemObject::setCategory<FILE_DIFFERENT_METADATA>() = delete; //deny use
template <> void FileSystemObject::setCategory<FILE_LEFT_SIDE_ONLY>    () = delete; //
template <> void FileSystemObject::setCategory<FILE_RIGHT_SIDE_ONLY>   () = delete; //

inline
void FileSystemObject::setCategoryConflict(const std::wstring& description)
{
    cmpResult_ = FILE_CONFLICT;
    cmpResultDescr_ = std::make_unique<std::wstring>(description);
}

inline
void FileSystemObject::setCategoryDiffMetadata(const std::wstring& description)
{
    cmpResult_ = FILE_DIFFERENT_METADATA;
    cmpResultDescr_ = std::make_unique<std::wstring>(description);
}

inline
void FileSystemObject::flip()
{
    std::swap(itemNameL_, itemNameR_);

    switch (cmpResult_)
    {
        case FILE_LEFT_SIDE_ONLY:
            cmpResult_ = FILE_RIGHT_SIDE_ONLY;
            break;
        case FILE_RIGHT_SIDE_ONLY:
            cmpResult_ = FILE_LEFT_SIDE_ONLY;
            break;
        case FILE_LEFT_NEWER:
            cmpResult_ = FILE_RIGHT_NEWER;
            break;
        case FILE_RIGHT_NEWER:
            cmpResult_ = FILE_LEFT_NEWER;
            break;
        case FILE_DIFFERENT_CONTENT:
        case FILE_EQUAL:
        case FILE_DIFFERENT_METADATA:
        case FILE_CONFLICT:
            break;
    }

    notifySyncCfgChanged();
}


template <SelectedSide side> inline
void FileSystemObject::propagateChangedItemName(const Zstring& itemNameOld)
{
    if (itemNameL_.empty() && itemNameR_.empty()) return; //both sides might just have been deleted by removeObject<>

    if (itemNameOld != getItemName<side>()) //perf: premature optimization?
        if (auto hierObj = dynamic_cast<ContainerObject*>(this))
            hierObj->updateRelPathsRecursion<side>(*this);
}


template <SelectedSide side> inline
void ContainerObject::updateRelPathsRecursion(const FileSystemObject& fsAlias)
{
    assert(SelectParam<side>::ref(relPathL_, relPathR_) != //perf: only call if actual item name changed!
           AFS::appendPaths(fsAlias.parent().getRelativePath<side>(), fsAlias.getItemName<side>(), FILE_NAME_SEPARATOR));

    SelectParam<side>::ref(relPathL_, relPathR_) = AFS::appendPaths(fsAlias.parent().getRelativePath<side>(), fsAlias.getItemName<side>(), FILE_NAME_SEPARATOR);

    for (FolderPair& folder : subFolders_)
        folder.updateRelPathsRecursion<side>(folder);
}


inline
ContainerObject::ContainerObject(const FileSystemObject& fsAlias) :
    relPathL_(AFS::appendPaths(fsAlias.parent().relPathL_, fsAlias.getItemName<LEFT_SIDE>(), FILE_NAME_SEPARATOR)),
    relPathR_(
        fsAlias.parent().relPathL_.c_str() ==        //
        fsAlias.parent().relPathR_.c_str() &&        //take advantage of FileSystemObject's Zstring reuse:
        fsAlias.getItemName<LEFT_SIDE >().c_str() == //=> perf: 12% faster merge phase; -4% peak memory
        fsAlias.getItemName<RIGHT_SIDE>().c_str() ?  //
        relPathL_ : //ternary-WTF! (implicit copy-constructor call!!) => no big deal for a Zstring
        AFS::appendPaths(fsAlias.parent().relPathR_, fsAlias.getItemName<RIGHT_SIDE>(), FILE_NAME_SEPARATOR)),
    base_(fsAlias.parent().base_)
{
    assert(relPathL_.c_str() == relPathR_.c_str() || relPathL_ != relPathR_);
}


inline
void ContainerObject::flip()
{
    for (FilePair& file : refSubFiles())
        file.flip();
    for (SymlinkPair& link : refSubLinks())
        link.flip();
    for (FolderPair& folder : refSubFolders())
        folder.flip();

    std::swap(relPathL_, relPathR_);
}


inline
FolderPair& ContainerObject::addSubFolder(const Zstring& itemNameL,
                                          const FolderAttributes& left,
                                          CompareDirResult defaultCmpResult,
                                          const Zstring& itemNameR,
                                          const FolderAttributes& right)
{
    subFolders_.emplace_back(itemNameL, left, defaultCmpResult, itemNameR, right, *this);
    return subFolders_.back();
}


template <> inline
FolderPair& ContainerObject::addSubFolder<LEFT_SIDE>(const Zstring& itemName, const FolderAttributes& attr)
{
    subFolders_.emplace_back(itemName, attr, DIR_LEFT_SIDE_ONLY, Zstring(), FolderAttributes(), *this);
    return subFolders_.back();
}


template <> inline
FolderPair& ContainerObject::addSubFolder<RIGHT_SIDE>(const Zstring& itemName, const FolderAttributes& attr)
{
    subFolders_.emplace_back(Zstring(), FolderAttributes(), DIR_RIGHT_SIDE_ONLY, itemName, attr, *this);
    return subFolders_.back();
}


inline
FilePair& ContainerObject::addSubFile(const Zstring&        itemNameL,
                                      const FileAttributes& left,          //file exists on both sides
                                      CompareFilesResult    defaultCmpResult,
                                      const Zstring&        itemNameR,
                                      const FileAttributes& right)
{
    subFiles_.emplace_back(itemNameL, left, defaultCmpResult, itemNameR, right, *this);
    return subFiles_.back();
}


template <> inline
FilePair& ContainerObject::addSubFile<LEFT_SIDE>(const Zstring& itemName, const FileAttributes& attr)
{
    subFiles_.emplace_back(itemName, attr, FILE_LEFT_SIDE_ONLY, Zstring(), FileAttributes(), *this);
    return subFiles_.back();
}


template <> inline
FilePair& ContainerObject::addSubFile<RIGHT_SIDE>(const Zstring& itemName, const FileAttributes& attr)
{
    subFiles_.emplace_back(Zstring(), FileAttributes(), FILE_RIGHT_SIDE_ONLY, itemName, attr, *this);
    return subFiles_.back();
}


inline
SymlinkPair& ContainerObject::addSubLink(const Zstring&        itemNameL,
                                         const LinkAttributes& left, //link exists on both sides
                                         CompareSymlinkResult  defaultCmpResult,
                                         const Zstring&        itemNameR,
                                         const LinkAttributes& right)
{
    subLinks_.emplace_back(itemNameL, left, defaultCmpResult, itemNameR, right, *this);
    return subLinks_.back();
}


template <> inline
SymlinkPair& ContainerObject::addSubLink<LEFT_SIDE>(const Zstring& itemName, const LinkAttributes& attr)
{
    subLinks_.emplace_back(itemName, attr, SYMLINK_LEFT_SIDE_ONLY, Zstring(), LinkAttributes(), *this);
    return subLinks_.back();
}


template <> inline
SymlinkPair& ContainerObject::addSubLink<RIGHT_SIDE>(const Zstring& itemName, const LinkAttributes& attr)
{
    subLinks_.emplace_back(Zstring(), LinkAttributes(), SYMLINK_RIGHT_SIDE_ONLY, itemName, attr, *this);
    return subLinks_.back();
}


inline
void BaseFolderPair::flip()
{
    ContainerObject::flip();
    std::swap(folderAvailableLeft_, folderAvailableRight_);
    std::swap(folderPathLeft_,      folderPathRight_);
}


inline
void FolderPair::flip()
{
    ContainerObject ::flip(); //call base class versions
    FileSystemObject::flip(); //
    std::swap(attrL_, attrR_);
}


inline
void FolderPair::removeObjectL()
{
    for (FilePair& file : refSubFiles())
        file.removeObject<LEFT_SIDE>();
    for (SymlinkPair& link : refSubLinks())
        link.removeObject<LEFT_SIDE>();
    for (FolderPair& folder : refSubFolders())
        folder.removeObject<LEFT_SIDE>();

    attrL_ = FolderAttributes();
}


inline
void FolderPair::removeObjectR()
{
    for (FilePair& file : refSubFiles())
        file.removeObject<RIGHT_SIDE>();
    for (SymlinkPair& link : refSubLinks())
        link.removeObject<RIGHT_SIDE>();
    for (FolderPair& folder : refSubFolders())
        folder.removeObject<RIGHT_SIDE>();

    attrR_ = FolderAttributes();
}


template <SelectedSide side> inline
bool BaseFolderPair::isAvailable() const
{
    return SelectParam<side>::ref(folderAvailableLeft_, folderAvailableRight_);
}


template <SelectedSide side> inline
void BaseFolderPair::setAvailable(bool value)
{
    SelectParam<side>::ref(folderAvailableLeft_, folderAvailableRight_) = value;
}


inline
void FilePair::flip()
{
    FileSystemObject::flip(); //call base class version
    std::swap(attrL_, attrR_);
}


template <SelectedSide side> inline
FileAttributes FilePair::getAttributes() const
{
    return SelectParam<side>::ref(attrL_, attrR_);
}


template <SelectedSide side> inline
time_t FilePair::getLastWriteTime() const
{
    return SelectParam<side>::ref(attrL_, attrR_).modTime;
}


template <SelectedSide side> inline
uint64_t FilePair::getFileSize() const
{
    return SelectParam<side>::ref(attrL_, attrR_).fileSize;
}


template <SelectedSide side> inline
AFS::FileId FilePair::getFileId() const
{
    return SelectParam<side>::ref(attrL_, attrR_).fileId;
}


template <SelectedSide side> inline
bool FilePair::isFollowedSymlink() const
{
    return SelectParam<side>::ref(attrL_, attrR_).isFollowedSymlink;
}


template <SelectedSide side> inline
bool FolderPair::isFollowedSymlink() const
{
    return SelectParam<side>::ref(attrL_, attrR_).isFollowedSymlink;
}


template <SelectedSide sideTrg> inline
void FilePair::setSyncedTo(const Zstring& itemName,
                           uint64_t fileSize,
                           int64_t lastWriteTimeTrg,
                           int64_t lastWriteTimeSrc,
                           const AFS::FileId& fileIdTrg,
                           const AFS::FileId& fileIdSrc,
                           bool isSymlinkTrg,
                           bool isSymlinkSrc)
{
    //FILE_EQUAL is only allowed for same short name and file size: enforced by this method!
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    SelectParam<sideTrg>::ref(attrL_, attrR_) = FileAttributes(lastWriteTimeTrg, fileSize, fileIdTrg, isSymlinkTrg);
    SelectParam<sideSrc>::ref(attrL_, attrR_) = FileAttributes(lastWriteTimeSrc, fileSize, fileIdSrc, isSymlinkSrc);

    moveFileRef_ = nullptr;
    FileSystemObject::setSynced(itemName); //set FileSystemObject specific part
}


template <SelectedSide sideTrg> inline
void SymlinkPair::setSyncedTo(const Zstring& itemName,
                              int64_t lastWriteTimeTrg,
                              int64_t lastWriteTimeSrc)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    SelectParam<sideTrg>::ref(attrL_, attrR_) = LinkAttributes(lastWriteTimeTrg);
    SelectParam<sideSrc>::ref(attrL_, attrR_) = LinkAttributes(lastWriteTimeSrc);

    FileSystemObject::setSynced(itemName); //set FileSystemObject specific part
}


template <SelectedSide sideTrg> inline
void FolderPair::setSyncedTo(const Zstring& itemName,
                             bool isSymlinkTrg,
                             bool isSymlinkSrc)
{
    static const SelectedSide sideSrc = OtherSide<sideTrg>::result;

    SelectParam<sideTrg>::ref(attrL_, attrR_) = FolderAttributes(isSymlinkTrg);
    SelectParam<sideSrc>::ref(attrL_, attrR_) = FolderAttributes(isSymlinkSrc);

    FileSystemObject::setSynced(itemName); //set FileSystemObject specific part
}


template <SelectedSide side> inline
time_t SymlinkPair::getLastWriteTime() const
{
    return SelectParam<side>::ref(attrL_, attrR_).modTime;
}


inline
CompareSymlinkResult SymlinkPair::getLinkCategory() const
{
    return static_cast<CompareSymlinkResult>(getCategory());
}


inline
void SymlinkPair::flip()
{
    FileSystemObject::flip(); //call base class versions
    std::swap(attrL_, attrR_);
}
}

#endif //FILE_HIERARCHY_H_257235289645296

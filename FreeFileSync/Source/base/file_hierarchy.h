// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_HIERARCHY_H_257235289645296
#define FILE_HIERARCHY_H_257235289645296

#include <string>
//#include <memory>
#include <list>
//#include <functional>
#include <unordered_set>
#include <unordered_map>
#include "structures.h"
#include "path_filter.h"
#include "../afs/abstract.h"


namespace fff
{
struct FileAttributes
{
    time_t modTime = 0; //number of seconds since Jan. 1st 1970 GMT
    uint64_t fileSize = 0;
    AFS::FingerPrint filePrint = 0; //optional
    bool isFollowedSymlink = false;

    static_assert(std::is_signed_v<time_t>, "we need time_t to be signed");

    std::strong_ordering operator<=>(const FileAttributes&) const = default;
};


struct LinkAttributes
{
    time_t modTime = 0; //number of seconds since Jan. 1st 1970 GMT
};


struct FolderAttributes
{
    bool isFollowedSymlink = false;
};


struct FolderContainer
{
    //------------------------------------------------------------------
    //key: raw file name, without any (Unicode) normalization, preserving original upper-/lower-case
    //"Changing data [...] to NFC would cause interoperability problems. Always leave data as it is."
    using FolderList  = std::unordered_map<Zstring, std::pair<FolderAttributes, FolderContainer>>;
    using FileList    = std::unordered_map<Zstring, FileAttributes>;
    using SymlinkList = std::unordered_map<Zstring, LinkAttributes>;
    //------------------------------------------------------------------

    FolderContainer() = default;
    FolderContainer           (const FolderContainer&) = delete; //catch accidental (and unnecessary) copying
    FolderContainer& operator=(const FolderContainer&) = delete; //

    FileList    files;
    SymlinkList symlinks; //non-followed symlinks
    FolderList  folders;

    void addFile(const Zstring& itemName, const FileAttributes& attr)
    {
        files.insert_or_assign(itemName, attr); //update entry if already existing (e.g. during folder traverser "retry")
    }

    void addLink(const Zstring& itemName, const LinkAttributes& attr)
    {
        symlinks.insert_or_assign(itemName, attr);
    }

    FolderContainer& addFolder(const Zstring& itemName, const FolderAttributes& attr)
    {
        auto& p = folders[itemName]; //value default-construction is okay here
        p.first = attr;
        return p.second;
    }
};

//------------------------------------------------------------------

enum class SelectSide
{
    left,
    right
};


template <SelectSide side>
constexpr SelectSide getOtherSide = side == SelectSide::left ? SelectSide::right : SelectSide::left;


template <SelectSide side, class T> inline
T& selectParam(T& left, T& right)
{
    if constexpr (side == SelectSide::left)
        return left;
    else
        return right;
}


enum class FileContentCategory : unsigned char
{
    unknown,
    equal,
    leftNewer,
    rightNewer,
    invalidTime,
    different,
    conflict,
};


inline
SyncDirection getEffectiveSyncDir(SyncOperation syncOp)
{
    switch (syncOp)
    {
        case SO_CREATE_LEFT:
        case SO_DELETE_LEFT:
        case SO_OVERWRITE_LEFT:
        case SO_RENAME_LEFT:
        case SO_MOVE_LEFT_FROM:
        case SO_MOVE_LEFT_TO:
            return SyncDirection::left;

        case SO_CREATE_RIGHT:
        case SO_DELETE_RIGHT:
        case SO_OVERWRITE_RIGHT:
        case SO_RENAME_RIGHT:
        case SO_MOVE_RIGHT_FROM:
        case SO_MOVE_RIGHT_TO:
            return SyncDirection::right;

        case SO_DO_NOTHING:
        case SO_EQUAL:
        case SO_UNRESOLVED_CONFLICT:
            break; //nothing to do
    }
    return SyncDirection::none;
}


std::wstring getShortDisplayNameForFolderPair(const AbstractPath& itemPathL, const AbstractPath& itemPathR);


class FileSystemObject;
class SymlinkPair;
class FilePair;
class FolderPair;
class BaseFolderPair;

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

struct PathInformation //diamond-shaped inheritance!
{
    virtual ~PathInformation() {}

    template <SelectSide side> AbstractPath getAbstractPath() const;
    template <SelectSide side> Zstring      getRelativePath() const; //get path relative to base sync dir (without leading/trailing FILE_NAME_SEPARATOR)

private:
    virtual AbstractPath getAbstractPathL() const = 0; //implemented by FileSystemObject + BaseFolderPair
    virtual AbstractPath getAbstractPathR() const = 0; //

    virtual Zstring getRelativePathL() const = 0; //implemented by SymlinkPair/FilePair + ContainerObject
    virtual Zstring getRelativePathR() const = 0; //
};

template <> inline AbstractPath PathInformation::getAbstractPath<SelectSide::left >() const { return getAbstractPathL(); }
template <> inline AbstractPath PathInformation::getAbstractPath<SelectSide::right>() const { return getAbstractPathR(); }

template <> inline Zstring PathInformation::getRelativePath<SelectSide::left >() const { return getRelativePathL(); }
template <> inline Zstring PathInformation::getRelativePath<SelectSide::right>() const { return getRelativePathR(); }

//------------------------------------------------------------------

class ContainerObject : public virtual PathInformation
{
    friend class FileSystemObject; //access to updateRelPathsRecursion()

public:
    using FileList    = std::list<FilePair>;    //MergeSides::execute() requires a structure that doesn't invalidate pointers after push_back()
    using SymlinkList = std::list<SymlinkPair>; //
    using FolderList  = std::list<FolderPair>;

    FolderPair& addFolder(const Zstring&          itemNameL, //file exists on both sides
                          const FolderAttributes& left,
                          const Zstring&          itemNameR,
                          const FolderAttributes& right);

    template <SelectSide side>
    FolderPair& addFolder(const Zstring& itemName, //dir exists on one side only
                          const FolderAttributes& attr);

    FilePair& addFile(const Zstring&        itemNameL, //file exists on both sides
                      const FileAttributes& left,
                      const Zstring&        itemNameR,
                      const FileAttributes& right);

    template <SelectSide side>
    FilePair& addFile(const Zstring&        itemName, //file exists on one side only
                      const FileAttributes& attr);

    SymlinkPair& addLink(const Zstring&        itemNameL, //link exists on both sides
                         const LinkAttributes& left,
                         const Zstring&        itemNameR,
                         const LinkAttributes& right);

    template <SelectSide side>
    SymlinkPair& addLink(const Zstring&        itemName, //link exists on one side only
                         const LinkAttributes& attr);

    const FileList& refSubFiles() const { return subFiles_; }
    /**/  FileList& refSubFiles()       { return subFiles_; }

    const SymlinkList& refSubLinks() const { return subLinks_; }
    /**/  SymlinkList& refSubLinks()       { return subLinks_; }

    const FolderList& refSubFolders() const { return subFolders_; }
    /**/  FolderList& refSubFolders()       { return subFolders_; }

    const BaseFolderPair& getBase() const { return base_; }
    /**/  BaseFolderPair& getBase()       { return base_; }

    void removeDoubleEmpty(); //remove all invalid entries (where both sides are empty) recursively

    virtual void flip();

protected:
    explicit ContainerObject(BaseFolderPair& baseFolder) : //used during BaseFolderPair constructor
        base_(baseFolder) //take reference only: baseFolder *not yet* fully constructed at this point!
    { assert(relPathL_.c_str() == relPathR_.c_str()); } //expected by the following contructor!

    explicit ContainerObject(const FileSystemObject& fsAlias); //used during FolderPair constructor

    virtual ~ContainerObject() //don't need polymorphic deletion, but we have a vtable anyway
    { assert(relPathL_.c_str() == relPathR_.c_str() || relPathL_ != relPathR_); }

    template <SelectSide side>
    void updateRelPathsRecursion(const FileSystemObject& fsAlias);

private:
    ContainerObject           (const ContainerObject&) = delete; //this class is referenced by its child elements => make it non-copyable/movable!
    ContainerObject& operator=(const ContainerObject&) = delete;

    Zstring getRelativePathL() const override { return relPathL_; }
    Zstring getRelativePathR() const override { return relPathR_; }

    FileList    subFiles_;
    SymlinkList subLinks_;
    FolderList  subFolders_;

    Zstring relPathL_; //path relative to base sync dir (without leading/trailing FILE_NAME_SEPARATOR)
    Zstring relPathR_; //class invariant: shared Zstring iff equal!

    BaseFolderPair& base_;
};

//------------------------------------------------------------------

enum class BaseFolderStatus
{
    existing,
    notExisting,
    failure,
};

class BaseFolderPair : public ContainerObject
{
public:
    BaseFolderPair(const AbstractPath& folderPathLeft,
                   BaseFolderStatus folderStatusLeft,
                   const AbstractPath& folderPathRight,
                   BaseFolderStatus folderStatusRight,
                   const FilterRef& filter,
                   CompareVariant cmpVar,
                   unsigned int fileTimeTolerance,
                   const std::vector<unsigned int>& ignoreTimeShiftMinutes) :
        ContainerObject(*this), //trust that ContainerObject knows that *this is not yet fully constructed!
        filter_(filter), cmpVar_(cmpVar), fileTimeTolerance_(fileTimeTolerance), ignoreTimeShiftMinutes_(ignoreTimeShiftMinutes),
        folderStatusLeft_ (folderStatusLeft),
        folderStatusRight_(folderStatusRight),
        folderPathLeft_(folderPathLeft),
        folderPathRight_(folderPathRight) {}

    template <SelectSide side> BaseFolderStatus getFolderStatus() const; //base folder status at the time of comparison!
    template <SelectSide side> void setFolderStatus(BaseFolderStatus value); //update after creating the directory in FFS

    //get settings which were used while creating BaseFolderPair:
    const PathFilter&   getFilter() const { return filter_.ref(); }
    CompareVariant getCompVariant() const { return cmpVar_; }
    unsigned int getFileTimeTolerance() const { return fileTimeTolerance_; }
    const std::vector<unsigned int>& getIgnoredTimeShift() const { return ignoreTimeShiftMinutes_; }

    void flip() override;

private:
    AbstractPath getAbstractPathL() const override { return folderPathLeft_; }
    AbstractPath getAbstractPathR() const override { return folderPathRight_; }

    const FilterRef filter_; //filter used while scanning directory: represents sub-view of actual files!
    const CompareVariant cmpVar_;
    const unsigned int fileTimeTolerance_;
    const std::vector<unsigned int> ignoreTimeShiftMinutes_;

    BaseFolderStatus folderStatusLeft_;
    BaseFolderStatus folderStatusRight_;

    AbstractPath folderPathLeft_;
    AbstractPath folderPathRight_;
};


//get rid of SharedRef<> indirection
template <class IterImpl, //underlying iterator type
          class T>        //target value type
class DerefIter
{
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = ptrdiff_t;
    using pointer   = T*;
    using reference = T&;

    DerefIter() {}
    DerefIter(IterImpl it) : it_(it) {}
    //DerefIter(const DerefIter& other) : it_(other.it_) {}
    DerefIter& operator++() { ++it_; return *this; }
    DerefIter& operator--() { --it_; return *this; }
    inline friend DerefIter operator++(DerefIter& it, int) { return it++; }
    inline friend DerefIter operator--(DerefIter& it, int) { return it--; }
    inline friend ptrdiff_t operator-(const DerefIter& lhs, const DerefIter& rhs) { return lhs.it_ - rhs.it_; }
    bool operator==(const DerefIter&) const = default;
    T& operator* () const { return  it_->ref(); }
    T* operator->() const { return &it_->ref(); }
private:
    IterImpl it_{};
};


using FolderComparison = std::vector<zen::SharedRef<BaseFolderPair>>; //make sure pointers to sub-elements remain valid
//don't change this back to std::vector<BaseFolderPair> inconsiderately: comparison uses push_back to add entries which may result in a full copy!

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
    using ObjectId      =       ObjectMgr*;
    using ObjectIdConst = const ObjectMgr*;

    ObjectIdConst  getId() const { return this; }
    /**/  ObjectId getId()       { return this; }

    static const T* retrieve(ObjectIdConst id) //returns nullptr if object is not valid anymore
    {
        return static_cast<const T*>(activeObjects_.contains(id) ? id : nullptr);
    }
    static T* retrieve(ObjectId id) { return const_cast<T*>(retrieve(static_cast<ObjectIdConst>(id))); }

protected:
    ObjectMgr () { activeObjects_.insert(this); }
    ~ObjectMgr() { activeObjects_.erase (this); }

private:
    ObjectMgr           (const ObjectMgr& rhs) = delete;
    ObjectMgr& operator=(const ObjectMgr& rhs) = delete; //it's not well-defined what copying an objects means regarding object-identity in this context

    //our global ObjectMgr is not thread-safe (and currently does not need to be!)
    //assert(runningOnMainThread()); -> still, may be accessed by synchronization worker threads, one thread at a time
    static inline std::unordered_set<const ObjectMgr*> activeObjects_; //external linkage!
};

//------------------------------------------------------------------

class FileSystemObject : public ObjectMgr<FileSystemObject>, public virtual PathInformation
{
public:
    virtual void accept(FSObjectVisitor& visitor) const = 0;

    bool isPairEmpty() const; //true, if both sides are empty
    template <SelectSide side> bool isEmpty() const;

    //path getters always return valid values, even if isEmpty<side>()!
    template <SelectSide side> Zstring getItemName() const; //case sensitive!

    bool hasEquivalentItemNames() const; //*quick* check if left/right names are equivalent when ignoring Unicode normalization forms

    //for use during compare() only:
    virtual void setCategoryConflict(const Zstringc& description) = 0;

    //comparison result
    virtual CompareFileResult getCategory() const = 0;
    virtual Zstringc getCategoryCustomDescription() const = 0; //optional

    //sync settings
    void setSyncDir(SyncDirection newDir);
    void setSyncDirConflict(const Zstringc& description); //set syncDir = SyncDirection::none + fill conflict description

    bool isActive() const { return selectedForSync_; }
    void setActive(bool active);

    //sync operation
    virtual SyncOperation testSyncOperation(SyncDirection testSyncDir) const; //"what if" semantics! assumes "active, no conflict, no recursion (directory)!
    virtual SyncOperation getSyncOperation() const;
    std::wstring getSyncOpConflict() const; //return conflict when determining sync direction or (still unresolved) conflict during categorization

    const ContainerObject& parent() const { return parent_; }
    /**/  ContainerObject& parent()       { return parent_; }
    const BaseFolderPair& base() const { return parent_.getBase(); }
    /**/  BaseFolderPair& base()       { return parent_.getBase(); }

    bool passFileFilter(const PathFilter& filter) const; //optimized for perf!

    virtual void flip();

    template <SelectSide side>
    void setItemName(const Zstring& itemName);

protected:
    FileSystemObject(const Zstring& itemNameL,
                     const Zstring& itemNameR,
                     ContainerObject& parentObj) :
        itemNameL_(itemNameL),
        itemNameR_(itemNameL == itemNameR ? itemNameL : itemNameR), //perf: no measurable speed drawback; -3% peak memory => further needed by ContainerObject construction!
        parent_(parentObj)
    {
        assert(itemNameL_.c_str() == itemNameR_.c_str() || itemNameL_ != itemNameR_); //also checks ref-counted string precondition
        FileSystemObject::notifySyncCfgChanged(); //non-virtual call! (=> anyway in a constructor!)
    }

    virtual ~FileSystemObject() //don't need polymorphic deletion, but we have a vtable anyway
    { assert(itemNameL_.c_str() == itemNameR_.c_str() || itemNameL_ != itemNameR_); }

    virtual void notifySyncCfgChanged()
    {
        if (auto fsParent = dynamic_cast<FileSystemObject*>(&parent_))
            fsParent->notifySyncCfgChanged(); //propagate!
    }

    template <SelectSide side> void removeFsObject();

private:
    FileSystemObject           (const FileSystemObject&) = delete;
    FileSystemObject& operator=(const FileSystemObject&) = delete;

    AbstractPath getAbstractPathL() const override { return AFS::appendRelPath(base().getAbstractPath<SelectSide::left >(), getRelativePath<SelectSide::left >()); }
    AbstractPath getAbstractPathR() const override { return AFS::appendRelPath(base().getAbstractPath<SelectSide::right>(), getRelativePath<SelectSide::right>()); }

    template <SelectSide side>
    void propagateChangedItemName(); //required after any itemName changes

    bool selectedForSync_ = true;

    SyncDirection syncDir_ = SyncDirection::none;
    Zstringc syncDirectionConflict_; //non-empty if we have a conflict setting sync-direction
    //conserve memory (avoid std::string SSO overhead + allow ref-counting!)

    Zstring itemNameL_; //use as indicator: empty means "not existing on this side"
    Zstring itemNameR_; //class invariant: shared Zstring iff equal!

    ContainerObject& parent_;
};

//------------------------------------------------------------------


class FolderPair : public FileSystemObject, public ContainerObject
{
public:
    void accept(FSObjectVisitor& visitor) const override;

    CompareFileResult getCategory() const override;
    CompareDirResult getDirCategory() const { return static_cast<CompareDirResult>(getCategory()); }

    FolderPair(const Zstring& itemNameL, //use empty itemName if "not existing"
               const FolderAttributes& attrL,
               const Zstring& itemNameR,
               const FolderAttributes& attrR,
               ContainerObject& parentObj) :
        FileSystemObject(itemNameL, itemNameR, parentObj),
        ContainerObject(static_cast<FileSystemObject&>(*this)), //FileSystemObject fully constructed at this point!
        attrL_(attrL),
        attrR_(attrR) {}

    template <SelectSide side> bool isFollowedSymlink() const;

    SyncOperation getSyncOperation() const override;

    template <SelectSide sideTrg>
    void setSyncedTo(bool isSymlinkTrg, bool isSymlinkSrc); //call after successful sync

    bool passDirFilter(const PathFilter& filter, bool* childItemMightMatch) const; //optimized for perf!

    void flip() override;

    void setCategoryConflict(const Zstringc& description) override;
    Zstringc getCategoryCustomDescription() const override; //optional

    template <SelectSide side> void removeItem();

private:
    void notifySyncCfgChanged() override { syncOpBuffered_ = {}; FileSystemObject::notifySyncCfgChanged(); }

    mutable std::optional<SyncOperation> syncOpBuffered_; //determining sync-op for directory may be expensive as it depends on child-objects => buffer

    FolderAttributes attrL_;
    FolderAttributes attrR_;

    Zstringc categoryConflict_; //conserve memory (avoid std::string SSO overhead + allow ref-counting!
};


//------------------------------------------------------------------

class FilePair : public FileSystemObject
{
public:
    void accept(FSObjectVisitor& visitor) const override;

    FilePair(const Zstring&        itemNameL, //use empty string if "not existing"
             const FileAttributes& attrL,
             const Zstring&        itemNameR, //
             const FileAttributes& attrR,
             ContainerObject& parentObj) :
        FileSystemObject(itemNameL, itemNameR, parentObj),
        attrL_(attrL),
        attrR_(attrR) {}

    CompareFileResult getCategory() const override;

    template <SelectSide side> time_t       getLastWriteTime() const;
    template <SelectSide side> uint64_t          getFileSize() const;
    template <SelectSide side> bool        isFollowedSymlink() const;
    template <SelectSide side> FileAttributes  getAttributes() const;
    template <SelectSide side> AFS::FingerPrint getFilePrint() const;
    template <SelectSide side> void clearFilePrint();

    void setMoveRef(ObjectId refId) { moveFileRef_ = refId; } //reference to corresponding renamed file
    ObjectId getMoveRef() const { assert(!moveFileRef_ || (isEmpty<SelectSide::left>() != isEmpty<SelectSide::right>())); return moveFileRef_; } //may be nullptr

    SyncOperation testSyncOperation(SyncDirection testSyncDir) const override; //semantics: "what if"! assumes "active, no conflict, no recursion (directory)!
    SyncOperation getSyncOperation() const override;

    template <SelectSide sideTrg>
    void setSyncedTo(uint64_t fileSize,
                     time_t lastWriteTimeTrg,
                     time_t lastWriteTimeSrc,
                     AFS::FingerPrint filePrintTrg,
                     AFS::FingerPrint filePrintSrc,
                     bool isSymlinkTrg,
                     bool isSymlinkSrc); //call after successful sync

    void flip() override;

    void setCategoryConflict(const Zstringc& description) override;
    void setCategoryInvalidTime(const Zstringc& description);
    Zstringc getCategoryCustomDescription() const override; //optional

    void setContentCategory(FileContentCategory category);
    FileContentCategory getContentCategory() const;

    template <SelectSide side> void removeItem();

private:
    Zstring getRelativePathL() const override { return appendPath(parent().getRelativePath<SelectSide::left >(), getItemName<SelectSide::left >()); }
    Zstring getRelativePathR() const override { return appendPath(parent().getRelativePath<SelectSide::right>(), getItemName<SelectSide::right>()); }

    SyncOperation applyMoveOptimization(SyncOperation op) const;

    FileAttributes attrL_;
    FileAttributes attrR_;

    ObjectId moveFileRef_ = nullptr; //optional, filled by redetermineSyncDirection()

    FileContentCategory contentCategory_ = FileContentCategory::unknown;
    Zstringc categoryDescr_; //optional: custom category description (e.g. FileContentCategory::conflict or invalidTime)
};

//------------------------------------------------------------------

class SymlinkPair : public FileSystemObject //this class models a TRUE symbolic link, i.e. one that is NEVER dereferenced: deref-links should be directly placed in class File/FolderPair
{
public:
    void accept(FSObjectVisitor& visitor) const override;

    SymlinkPair(const Zstring&        itemNameL, //use empty string if "not existing"
                const LinkAttributes& attrL,
                const Zstring&        itemNameR, //use empty string if "not existing"
                const LinkAttributes& attrR,
                ContainerObject& parentObj) :
        FileSystemObject(itemNameL, itemNameR, parentObj),
        attrL_(attrL),
        attrR_(attrR) {}

    CompareFileResult getCategory() const override;
    CompareSymlinkResult getLinkCategory() const { return static_cast<CompareSymlinkResult>(getCategory()); }

    template <SelectSide side> time_t getLastWriteTime() const; //write time of the link, NOT target!

    template <SelectSide sideTrg>
    void setSyncedTo(time_t lastWriteTimeTrg,
                     time_t lastWriteTimeSrc); //call after successful sync

    void flip() override;

    void setCategoryConflict(const Zstringc& description) override;
    void setCategoryInvalidTime(const Zstringc& description);
    Zstringc getCategoryCustomDescription() const override; //optional

    void setContentCategory(FileContentCategory category);
    FileContentCategory getContentCategory() const;

    template <SelectSide side> void removeItem();

private:
    Zstring getRelativePathL() const override { return appendPath(parent().getRelativePath<SelectSide::left >(), getItemName<SelectSide::left >()); }
    Zstring getRelativePathR() const override { return appendPath(parent().getRelativePath<SelectSide::right>(), getItemName<SelectSide::right>()); }

    LinkAttributes attrL_;
    LinkAttributes attrR_;

    FileContentCategory contentCategory_ = FileContentCategory::unknown;
    Zstringc categoryDescr_; //optional: custom category description (e.g. FileContentCategory::conflict or invalidTime)
};

//------------------------------------------------------------------

//generic descriptions (usecase CSV legend, sync config)
std::wstring getCategoryDescription(CompareFileResult cmpRes);
std::wstring getSyncOpDescription(SyncOperation op);

//item-specific descriptions
std::wstring getCategoryDescription(const FileSystemObject& fsObj);
std::wstring getSyncOpDescription(const FileSystemObject& fsObj);

//------------------------------------------------------------------

namespace impl
{
template <class Function1, class Function2, class Function3>
struct FSObjectLambdaVisitor : public FSObjectVisitor
{
    FSObjectLambdaVisitor(Function1 onFolder,
                          Function2 onFile,
                          Function3 onSymlink) : //unifying assignment
        onFolder_(std::move(onFolder)), onFile_(std::move(onFile)), onSymlink_(std::move(onSymlink)) {}
private:
    void visit(const FolderPair&  folder ) override { onFolder_ (folder);  }
    void visit(const FilePair&    file   ) override { onFile_   (file);    }
    void visit(const SymlinkPair& symlink) override { onSymlink_(symlink); }

    const Function1 onFolder_;
    const Function2 onFile_;
    const Function3 onSymlink_;
};
}

template <class Function1, class Function2, class Function3> inline
void visitFSObject(const FileSystemObject& fsObj, Function1 onFolder, Function2 onFile, Function3 onSymlink)
{
    impl::FSObjectLambdaVisitor<Function1, Function2, Function3> visitor(onFolder, onFile, onSymlink);
    fsObj.accept(visitor);
}


template <class Function1, class Function2, class Function3> inline
void visitFSObject(FileSystemObject& fsObj, Function1 onFolder, Function2 onFile, Function3 onSymlink)
{
    visitFSObject(static_cast<const FileSystemObject&>(fsObj),
    [onFolder ](const FolderPair&   folder) { onFolder (const_cast<FolderPair& >(folder));  },  //
    [onFile   ](const FilePair&       file) { onFile   (const_cast<FilePair&   >(file   )); },  //physical object is not const anyway
    [onSymlink](const SymlinkPair& symlink) { onSymlink(const_cast<SymlinkPair&>(symlink)); }); //
}

//------------------------------------------------------------------

namespace impl
{
template <class Function1, class Function2, class Function3>
class RecursiveObjectVisitor
{
public:
    RecursiveObjectVisitor(Function1 onFolder,
                           Function2 onFile,
                           Function3 onSymlink) : //unifying assignment
        onFolder_ (std::move(onFolder)),
        onFile_   (std::move(onFile)),
        onSymlink_(std::move(onSymlink)) {}

    void execute(ContainerObject& conObj)
    {
        for (FilePair& file : conObj.refSubFiles())
            onFile_(file);
        for (SymlinkPair& symlink : conObj.refSubLinks())
            onSymlink_(symlink);
        for (FolderPair& subFolder : conObj.refSubFolders())
        {
            onFolder_(subFolder);
            execute(subFolder);
        }
    }

private:
    RecursiveObjectVisitor           (const RecursiveObjectVisitor&) = delete;
    RecursiveObjectVisitor& operator=(const RecursiveObjectVisitor&) = delete;

    const Function1 onFolder_;
    const Function2 onFile_;
    const Function3 onSymlink_;
};
}

template <class Function1, class Function2, class Function3> inline
void visitFSObjectRecursively(ContainerObject& conObj, //consider contained items only
                              Function1 onFolder,
                              Function2 onFile,
                              Function3 onSymlink)
{
    impl::RecursiveObjectVisitor(onFolder, onFile, onSymlink).execute(conObj);
}

template <class Function1, class Function2, class Function3> inline
void visitFSObjectRecursively(FileSystemObject& fsObj, //consider item and contained items (if folder)
                              Function1 onFolder,
                              Function2 onFile,
                              Function3 onSymlink)
{
    visitFSObject(fsObj, [onFolder, onFile, onSymlink](FolderPair& folder)
    {
        onFolder(folder);
        impl::RecursiveObjectVisitor(onFolder, onFile, onSymlink).execute(const_cast<FolderPair&>(folder));
    }, onFile, onSymlink);
}


















//--------------------- implementation ------------------------------------------

//inline virtual... admittedly its use may be limited
inline void FolderPair ::accept(FSObjectVisitor& visitor) const { visitor.visit(*this); }
inline void FilePair   ::accept(FSObjectVisitor& visitor) const { visitor.visit(*this); }
inline void SymlinkPair::accept(FSObjectVisitor& visitor) const { visitor.visit(*this); }


inline
void FileSystemObject::setSyncDir(SyncDirection newDir)
{
    syncDir_ = newDir;
    syncDirectionConflict_.clear();

    notifySyncCfgChanged();
}


inline
void FileSystemObject::setSyncDirConflict(const Zstringc& description)
{
    assert(!description.empty());
    syncDir_ = SyncDirection::none;
    syncDirectionConflict_ = description;

    notifySyncCfgChanged();
}


inline
std::wstring FileSystemObject::getSyncOpConflict() const
{
    assert(getSyncOperation() == SO_UNRESOLVED_CONFLICT);
    return zen::utfTo<std::wstring>(syncDirectionConflict_);
}


inline
void FileSystemObject::setActive(bool active)
{
    selectedForSync_ = active;
    notifySyncCfgChanged();
}


template <SelectSide side> inline
bool FileSystemObject::isEmpty() const
{
    return selectParam<side>(itemNameL_, itemNameR_).empty();
}


inline
bool FileSystemObject::isPairEmpty() const
{
    return isEmpty<SelectSide::left>() && isEmpty<SelectSide::right>();
}


template <SelectSide side> inline
Zstring FileSystemObject::getItemName() const
{
    //assert(!itemNameL_.empty() || !itemNameR_.empty()); //-> file pair might be temporarily empty (until removed after sync)

    const Zstring& itemName = selectParam<side>(itemNameL_, itemNameR_); //empty if not existing
    if (!itemName.empty()) //avoid ternary-WTF! (implicit copy-constructor call!!!!!!)
        return itemName;
    return selectParam<getOtherSide<side>>(itemNameL_, itemNameR_);
}


inline
bool FileSystemObject::hasEquivalentItemNames() const
{
    if (itemNameL_.c_str() == itemNameR_.c_str() || //most likely case
        itemNameL_.empty() || itemNameR_.empty())   //
        return true;

    assert(itemNameL_ != itemNameR_); //class invariant
    return getUnicodeNormalForm(itemNameL_) == getUnicodeNormalForm(itemNameR_);
}


template <SelectSide side> inline
void FileSystemObject::removeFsObject()
{
    if (isEmpty<getOtherSide<side>>())
        itemNameL_ = itemNameR_ = Zstring(); //ensure (c_str) class invariant!
    else
        selectParam<side>(itemNameL_, itemNameR_).clear();

    if (isPairEmpty())
        setSyncDir(SyncDirection::none); //calls notifySyncCfgChanged()
    else //keep current syncDir_
        notifySyncCfgChanged(); //needed!?

    propagateChangedItemName<side>();
}


template <SelectSide side> inline
void FilePair::removeItem()
{
    selectParam<side>(attrL_, attrR_) = FileAttributes();
    contentCategory_ = FileContentCategory::unknown;
    removeFsObject<side>();

    //cut ties between "move" pairs
    if (isPairEmpty())
    {
        if (moveFileRef_)
            if (auto refFile = dynamic_cast<FilePair*>(FileSystemObject::retrieve(moveFileRef_)))
            {
                if (refFile->moveFileRef_ == getId()) //both ends should agree...
                    refFile->moveFileRef_ = nullptr;
                else assert(false); //...and why shouldn't they?
            }
        moveFileRef_ = nullptr;
    }
}


template <SelectSide side> inline
void SymlinkPair::removeItem()
{
    selectParam<side>(attrL_, attrR_) = LinkAttributes();
    contentCategory_ = FileContentCategory::unknown;
    removeFsObject<side>();
}


template <SelectSide side> inline
void FolderPair::removeItem()
{
    for (FilePair& file : refSubFiles())
        file.removeItem<side>();
    for (SymlinkPair& symlink : refSubLinks())
        symlink.removeItem<side>();
    for (FolderPair& folder : refSubFolders())
        folder.removeItem<side>();

    selectParam<side>(attrL_, attrR_) = FolderAttributes();
    removeFsObject<side>();
}


template <SelectSide side> inline
void FileSystemObject::setItemName(const Zstring& itemName)
{
    assert(!itemName.empty());
    assert(!isPairEmpty());

    selectParam<side>(itemNameL_, itemNameR_) = itemName;

    if (itemNameL_.c_str() != itemNameR_.c_str() &&
        itemNameL_ == itemNameR_)
        itemNameL_ = itemNameR_; //preserve class invariant
    assert(itemNameL_.c_str() == itemNameR_.c_str() || itemNameL_ != itemNameR_);

    propagateChangedItemName<side>();
}


template <SelectSide side> inline
void FileSystemObject::propagateChangedItemName()
{
    if (itemNameL_.empty() && itemNameR_.empty()) return; //both sides might just have been deleted by removeItem<>

    if (auto conObj = dynamic_cast<ContainerObject*>(this))
    {
        const Zstring& itemNameOld = zen::getItemName(conObj->getRelativePath<side>());
        if (itemNameOld != getItemName<side>()) //perf: premature optimization?
            conObj->updateRelPathsRecursion<side>(*this);
    }
}


template <SelectSide side> inline
void ContainerObject::updateRelPathsRecursion(const FileSystemObject& fsAlias)
{
    //perf: only call if actual item name changed:
    assert(selectParam<side>(relPathL_, relPathR_) != appendPath(fsAlias.parent().getRelativePath<side>(), fsAlias.getItemName<side>()));

    constexpr SelectSide otherSide = getOtherSide<side>;

    if (fsAlias.isEmpty<otherSide>()) //=> 1. other side's relPath also needs updating! 2. both sides have same name
        selectParam<otherSide>(relPathL_, relPathR_) = appendPath(selectParam<otherSide>(fsAlias.parent().relPathL_,
                                                                  fsAlias.parent().relPathR_), fsAlias.getItemName<otherSide>());
    else //assume relPath on other side is up to date!
        assert(selectParam<otherSide>(relPathL_, relPathR_) == appendPath(fsAlias.parent().getRelativePath<otherSide>(), fsAlias.getItemName<otherSide>()));

    if (fsAlias.parent().relPathL_.c_str() ==               //
        fsAlias.parent().relPathR_.c_str() &&               //see ContainerObject constructor and setItemName()
        fsAlias.getItemName<SelectSide::left >().c_str() == //
        fsAlias.getItemName<SelectSide::right>().c_str())   //
        selectParam<side>(relPathL_, relPathR_) = selectParam<otherSide>(relPathL_, relPathR_);
    else
        selectParam<side>(relPathL_, relPathR_) = appendPath(selectParam<side>(fsAlias.parent().relPathL_,
                                                                               fsAlias.parent().relPathR_), fsAlias.getItemName<side>());
    assert(relPathL_.c_str() == relPathR_.c_str() || relPathL_ != relPathR_);

    for (FolderPair& folder : subFolders_)
        folder.updateRelPathsRecursion<side>(folder);
}


inline
ContainerObject::ContainerObject(const FileSystemObject& fsAlias) :
    relPathL_(appendPath(fsAlias.parent().relPathL_, fsAlias.getItemName<SelectSide::left>())),
    relPathR_(fsAlias.parent().relPathL_.c_str() ==               //
              fsAlias.parent().relPathR_.c_str() &&               //take advantage of FileSystemObject's Zstring reuse:
              fsAlias.getItemName<SelectSide::left >().c_str() == //=> perf: 12% faster merge phase; -4% peak memory
              fsAlias.getItemName<SelectSide::right>().c_str() ?  //
              relPathL_ : //ternary-WTF! (implicit copy-constructor call!!) => no big deal for a Zstring
              appendPath(fsAlias.parent().relPathR_, fsAlias.getItemName<SelectSide::right>())),
    base_(fsAlias.parent().base_)
{
    assert(relPathL_.c_str() == relPathR_.c_str() || relPathL_ != relPathR_);
}


inline
FolderPair& ContainerObject::addFolder(const Zstring& itemNameL,
                                       const FolderAttributes& left,
                                       const Zstring& itemNameR,
                                       const FolderAttributes& right)
{
    subFolders_.emplace_back(itemNameL, left, itemNameR, right, *this);
    return subFolders_.back();
}


template <> inline
FolderPair& ContainerObject::addFolder<SelectSide::left>(const Zstring& itemName, const FolderAttributes& attr)
{
    subFolders_.emplace_back(itemName, attr, Zstring(), FolderAttributes(), *this);
    return subFolders_.back();
}


template <> inline
FolderPair& ContainerObject::addFolder<SelectSide::right>(const Zstring& itemName, const FolderAttributes& attr)
{
    subFolders_.emplace_back(Zstring(), FolderAttributes(), itemName, attr, *this);
    return subFolders_.back();
}


inline
FilePair& ContainerObject::addFile(const Zstring&        itemNameL,
                                   const FileAttributes& left,
                                   const Zstring&        itemNameR,
                                   const FileAttributes& right) //file exists on both sides
{
    subFiles_.emplace_back(itemNameL, left, itemNameR, right, *this);
    return subFiles_.back();
}


template <> inline
FilePair& ContainerObject::addFile<SelectSide::left>(const Zstring& itemName, const FileAttributes& attr)
{
    subFiles_.emplace_back(itemName, attr, Zstring(), FileAttributes(), *this);
    return subFiles_.back();
}


template <> inline
FilePair& ContainerObject::addFile<SelectSide::right>(const Zstring& itemName, const FileAttributes& attr)
{
    subFiles_.emplace_back(Zstring(), FileAttributes(), itemName, attr, *this);
    return subFiles_.back();
}


inline
SymlinkPair& ContainerObject::addLink(const Zstring&        itemNameL,
                                      const LinkAttributes& left,
                                      const Zstring&        itemNameR,
                                      const LinkAttributes& right) //link exists on both sides
{
    subLinks_.emplace_back(itemNameL, left, itemNameR, right, *this);
    return subLinks_.back();
}


template <> inline
SymlinkPair& ContainerObject::addLink<SelectSide::left>(const Zstring& itemName, const LinkAttributes& attr)
{
    subLinks_.emplace_back(itemName, attr, Zstring(), LinkAttributes(), *this);
    return subLinks_.back();
}


template <> inline
SymlinkPair& ContainerObject::addLink<SelectSide::right>(const Zstring& itemName, const LinkAttributes& attr)
{
    subLinks_.emplace_back(Zstring(), LinkAttributes(), itemName, attr, *this);
    return subLinks_.back();
}


inline
void FileSystemObject::flip()
{
    std::swap(itemNameL_, itemNameR_);
    notifySyncCfgChanged();
}


inline
void ContainerObject::flip()
{
    for (FilePair& file : refSubFiles())
        file.flip();
    for (SymlinkPair& symlink : refSubLinks())
        symlink.flip();
    for (FolderPair& folder : refSubFolders())
        folder.flip();

    std::swap(relPathL_, relPathR_);
}


inline
void BaseFolderPair::flip()
{
    ContainerObject::flip();
    std::swap(folderStatusLeft_, folderStatusRight_);
    std::swap(folderPathLeft_,   folderPathRight_);
}


inline
void FolderPair::flip() //this overrides both ContainerObject/FileSystemObject::flip!
{
    ContainerObject ::flip(); //call base class versions
    FileSystemObject::flip(); //
    std::swap(attrL_, attrR_);
}


inline
void FilePair::flip()
{
    FileSystemObject::flip(); //call base class version
    std::swap(attrL_, attrR_);

    switch (contentCategory_)
    {
        //*INDENT-OFF*
        case FileContentCategory::unknown:
        case FileContentCategory::equal:
        case FileContentCategory::invalidTime:
        case FileContentCategory::different:
        case FileContentCategory::conflict: break;
        case FileContentCategory::leftNewer:  contentCategory_ = FileContentCategory::rightNewer; break;
        case FileContentCategory::rightNewer: contentCategory_ = FileContentCategory::leftNewer; break;
        //*INDENT-ON*
    }
}


inline
void SymlinkPair::flip()
{
    FileSystemObject::flip(); //call base class versions
    std::swap(attrL_, attrR_);

    switch (contentCategory_)
    {
        //*INDENT-OFF*
        case FileContentCategory::unknown:
        case FileContentCategory::equal:
        case FileContentCategory::invalidTime:
        case FileContentCategory::different:
        case FileContentCategory::conflict: break;
        case FileContentCategory::leftNewer:  contentCategory_ = FileContentCategory::rightNewer; break;
        case FileContentCategory::rightNewer: contentCategory_ = FileContentCategory::leftNewer; break;
        //*INDENT-ON*
    }
}


template <SelectSide side> inline
BaseFolderStatus BaseFolderPair::getFolderStatus() const
{
    return selectParam<side>(folderStatusLeft_, folderStatusRight_);
}


template <SelectSide side> inline
void BaseFolderPair::setFolderStatus(BaseFolderStatus value)
{
    selectParam<side>(folderStatusLeft_, folderStatusRight_) = value;
}


inline
void FolderPair::setCategoryConflict(const Zstringc& description)
{
    assert(!description.empty());
    categoryConflict_ = description;
}


inline
void FilePair::setCategoryConflict(const Zstringc& description)
{
    assert(!description.empty());
    categoryDescr_ = description;
    contentCategory_ = FileContentCategory::conflict;
}


inline
void SymlinkPair::setCategoryConflict(const Zstringc& description)
{
    assert(!description.empty());
    categoryDescr_ = description;
    contentCategory_ = FileContentCategory::conflict;
}


inline
void FilePair::setCategoryInvalidTime(const Zstringc& description)
{
    assert(!description.empty());
    categoryDescr_ = description;
    contentCategory_ = FileContentCategory::invalidTime;
}


inline
void SymlinkPair::setCategoryInvalidTime(const Zstringc& description)
{
    assert(!description.empty());
    categoryDescr_ = description;
    contentCategory_ = FileContentCategory::invalidTime;
}


inline Zstringc FolderPair ::getCategoryCustomDescription() const { return categoryConflict_; }
inline Zstringc FilePair   ::getCategoryCustomDescription() const { return categoryDescr_; }
inline Zstringc SymlinkPair::getCategoryCustomDescription() const { return categoryDescr_; }


inline
void FilePair::setContentCategory(FileContentCategory category)
{
    assert(!isEmpty<SelectSide::left>() &&!isEmpty<SelectSide::right>());
    assert(category != FileContentCategory::unknown);
    contentCategory_ = category;
}


inline
void SymlinkPair::setContentCategory(FileContentCategory category)
{
    assert(!isEmpty<SelectSide::left>() &&!isEmpty<SelectSide::right>());
    assert(category != FileContentCategory::unknown);
    contentCategory_ = category;
}


inline
FileContentCategory FilePair::getContentCategory() const
{
    assert(!isEmpty<SelectSide::left>() &&!isEmpty<SelectSide::right>());
    return contentCategory_;
}


inline
FileContentCategory SymlinkPair::getContentCategory() const
{
    assert(!isEmpty<SelectSide::left>() &&!isEmpty<SelectSide::right>());
    return contentCategory_;
}


inline
CompareFileResult FolderPair::getCategory() const
{
    if (!categoryConflict_.empty())
        return FILE_CONFLICT;

    if (isEmpty<SelectSide::left>())
    {
        if (isEmpty<SelectSide::right>())
            return FILE_EQUAL;
        else
            return FILE_RIGHT_ONLY;
    }
    else
    {
        if (isEmpty<SelectSide::right>())
            return FILE_LEFT_ONLY;
        else
            return hasEquivalentItemNames() ? FILE_EQUAL : FILE_RENAMED;
    }
}


inline
CompareFileResult FilePair::getCategory() const
{
    assert(contentCategory_ == FileContentCategory::conflict ||
           (isEmpty<SelectSide::left>() || isEmpty<SelectSide::right>()) == (contentCategory_ == FileContentCategory::unknown));
    assert(contentCategory_ != FileContentCategory::conflict || !categoryDescr_.empty());

    if (contentCategory_ == FileContentCategory::conflict)
    {
        assert(!categoryDescr_.empty());
        return FILE_CONFLICT;
    }

    if (isEmpty<SelectSide::left>())
    {
        if (isEmpty<SelectSide::right>())
            return FILE_EQUAL;
        else
            return FILE_RIGHT_ONLY;
    }
    else
    {
        if (isEmpty<SelectSide::right>())
            return FILE_LEFT_ONLY;
        else
            //Caveat:
            //1. FILE_EQUAL may only be set if names match in case: InSyncFolder's mapping tables use file name as a key! see db_file.cpp
            //2. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::setSyncedTo() in file_hierarchy.h
            //3. FILE_EQUAL is expected to mean identical file sizes! See InSyncFile
            switch (contentCategory_)
            {
                //*INDENT-OFF*
                case FileContentCategory::unknown:
                case FileContentCategory::conflict: assert(false); return FILE_CONFLICT;
                case FileContentCategory::equal:       return hasEquivalentItemNames() ? FILE_EQUAL : FILE_RENAMED;
                case FileContentCategory::leftNewer:   return FILE_LEFT_NEWER;
                case FileContentCategory::rightNewer:  return FILE_RIGHT_NEWER;
                case FileContentCategory::invalidTime: return FILE_TIME_INVALID;
                case FileContentCategory::different:   return FILE_DIFFERENT_CONTENT;
                //*INDENT-ON*
            }
    }
    throw std::logic_error(std::string(__FILE__) + '[' + zen::numberTo<std::string>(__LINE__) + "] Contract violation!");
}


inline
CompareFileResult SymlinkPair::getCategory() const
{
    assert(contentCategory_ == FileContentCategory::conflict ||
           (isEmpty<SelectSide::left>() || isEmpty<SelectSide::right>()) == (contentCategory_ == FileContentCategory::unknown));
    assert(contentCategory_ != FileContentCategory::conflict || !categoryDescr_.empty());

    if (contentCategory_ == FileContentCategory::conflict)
    {
        assert(!categoryDescr_.empty());
        return FILE_CONFLICT;
    }

    if (isEmpty<SelectSide::left>())
    {
        if (isEmpty<SelectSide::right>())
            return FILE_EQUAL;
        else
            return FILE_RIGHT_ONLY;
    }
    else
    {
        if (isEmpty<SelectSide::right>())
            return FILE_LEFT_ONLY;
        else
            //Caveat:
            //1. SYMLINK_EQUAL may only be set if names match in case: InSyncFolder's mapping tables use link name as a key! see db_file.cpp
            //2. harmonize with "bool stillInSync()" in algorithm.cpp, FilePair::setSyncedTo() in file_hierarchy.h
            switch (contentCategory_)
            {
                //*INDENT-OFF*
                case FileContentCategory::unknown:
                case FileContentCategory::conflict: assert(false); return FILE_CONFLICT;
                case FileContentCategory::equal:       return hasEquivalentItemNames() ? FILE_EQUAL : FILE_RENAMED;
                case FileContentCategory::leftNewer:   return FILE_LEFT_NEWER;
                case FileContentCategory::rightNewer:  return FILE_RIGHT_NEWER;
                case FileContentCategory::invalidTime: return FILE_TIME_INVALID;
                case FileContentCategory::different:   return FILE_DIFFERENT_CONTENT;
                //*INDENT-ON*
            }
    }
    throw std::logic_error(std::string(__FILE__) + '[' + zen::numberTo<std::string>(__LINE__) + "] Contract violation!");
}


template <SelectSide side> inline
FileAttributes FilePair::getAttributes() const
{
    assert(!isEmpty<side>());
    return selectParam<side>(attrL_, attrR_);
}


template <SelectSide side> inline
time_t FilePair::getLastWriteTime() const
{
    assert(!isEmpty<side>());
    return selectParam<side>(attrL_, attrR_).modTime;
}


template <SelectSide side> inline
uint64_t FilePair::getFileSize() const
{
    assert(!isEmpty<side>());
    return selectParam<side>(attrL_, attrR_).fileSize;
}


template <SelectSide side> inline
bool FolderPair::isFollowedSymlink() const
{
    assert(!isEmpty<side>());
    return selectParam<side>(attrL_, attrR_).isFollowedSymlink;
}


template <SelectSide side> inline
bool FilePair::isFollowedSymlink() const
{
    assert(!isEmpty<side>());
    return selectParam<side>(attrL_, attrR_).isFollowedSymlink;
}


template <SelectSide side> inline
AFS::FingerPrint FilePair::getFilePrint() const
{
    assert(!isEmpty<side>());
    return selectParam<side>(attrL_, attrR_).filePrint;
}


template <SelectSide side> inline
void FilePair::clearFilePrint()
{
    selectParam<side>(attrL_, attrR_).filePrint = 0;
}


template <SelectSide sideTrg> inline
void FolderPair::setSyncedTo(bool isSymlinkTrg,
                             bool isSymlinkSrc)
{
    selectParam<             sideTrg >(attrL_, attrR_) = {.isFollowedSymlink = isSymlinkTrg};
    selectParam<getOtherSide<sideTrg>>(attrL_, attrR_) = {.isFollowedSymlink = isSymlinkSrc};

    setItemName<sideTrg>(getItemName<getOtherSide<sideTrg>>());

    categoryConflict_.clear();
    setSyncDir(SyncDirection::none);
}


template <SelectSide sideTrg> inline
void FilePair::setSyncedTo(uint64_t fileSize,
                           time_t lastWriteTimeTrg,
                           time_t lastWriteTimeSrc,
                           AFS::FingerPrint filePrintTrg,
                           AFS::FingerPrint filePrintSrc,
                           bool isSymlinkTrg,
                           bool isSymlinkSrc)
{
    selectParam<             sideTrg >(attrL_, attrR_) = {lastWriteTimeTrg, fileSize, filePrintTrg, isSymlinkTrg};
    selectParam<getOtherSide<sideTrg>>(attrL_, attrR_) = {lastWriteTimeSrc, fileSize, filePrintSrc, isSymlinkSrc};

    //cut ties between "move" pairs
    if (moveFileRef_)
        if (auto refFile = dynamic_cast<FilePair*>(FileSystemObject::retrieve(moveFileRef_)))
        {
            if (refFile->moveFileRef_ == getId()) //both ends should agree...
                refFile->moveFileRef_ = nullptr;
            else assert(false); //...and why shouldn't they?
        }
    moveFileRef_ = nullptr;

    setItemName<sideTrg>(getItemName<getOtherSide<sideTrg>>());

    contentCategory_ = FileContentCategory::equal;
    categoryDescr_.clear();
    setSyncDir(SyncDirection::none);
}


template <SelectSide sideTrg> inline
void SymlinkPair::setSyncedTo(time_t lastWriteTimeTrg,
                              time_t lastWriteTimeSrc)
{
    selectParam<             sideTrg >(attrL_, attrR_) = {.modTime = lastWriteTimeTrg};
    selectParam<getOtherSide<sideTrg>>(attrL_, attrR_) = {.modTime = lastWriteTimeSrc};

    setItemName<sideTrg>(getItemName<getOtherSide<sideTrg>>());

    contentCategory_ = FileContentCategory::equal;
    categoryDescr_.clear();
    setSyncDir(SyncDirection::none);
}


inline
bool FolderPair::passDirFilter(const PathFilter& filter, bool* childItemMightMatch) const
{
    const Zstring& relPathL = getRelativePath<SelectSide::left >();
    const Zstring& relPathR = getRelativePath<SelectSide::right>();
    assert(relPathL.c_str() == relPathR.c_str() || relPathL!= relPathR);

    if (filter.passDirFilter(relPathL, childItemMightMatch))
        return relPathL.c_str() == relPathR.c_str() /*perf!*/ || equalNoCase(relPathL, relPathR) ||
               filter.passDirFilter(relPathR, childItemMightMatch);
    else
    {
        if (childItemMightMatch && *childItemMightMatch &&
            relPathL.c_str() != relPathR.c_str() /*perf!*/ && !equalNoCase(relPathL, relPathR))
            filter.passDirFilter(relPathR, childItemMightMatch);
        return false;
    }
}


inline
bool FileSystemObject::passFileFilter(const PathFilter& filter) const
{
    assert(!dynamic_cast<const FolderPair*>(this));
    assert(parent().getRelativePath<SelectSide::left >().c_str() ==
           parent().getRelativePath<SelectSide::right>().c_str() ||
           parent().getRelativePath<SelectSide::left >()!=
           parent().getRelativePath<SelectSide::right>());
    assert(getItemName<SelectSide::left >().c_str() ==
           getItemName<SelectSide::right>().c_str() ||
           getItemName<SelectSide::left >() !=
           getItemName<SelectSide::right>());

    const Zstring relPathL = getRelativePath<SelectSide::left>();

    if (!filter.passFileFilter(relPathL))
        return false;

    if (parent().getRelativePath<SelectSide::left >().c_str() == //
        parent().getRelativePath<SelectSide::right>().c_str() && //perf! see ContainerObject constructor
        getItemName<SelectSide::left >().c_str() ==              //
        getItemName<SelectSide::right>().c_str())                //
        return true;

    const Zstring relPathR = getRelativePath<SelectSide::right>();

    if (equalNoCase(relPathL, relPathR))
        return true;

    return filter.passFileFilter(relPathR);
}


template <SelectSide side> inline
time_t SymlinkPair::getLastWriteTime() const
{
    return selectParam<side>(attrL_, attrR_).modTime;
}
}

#endif //FILE_HIERARCHY_H_257235289645296

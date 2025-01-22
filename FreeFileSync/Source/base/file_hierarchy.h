// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FILE_HIERARCHY_H_257235289645296
#define FILE_HIERARCHY_H_257235289645296

#include <string>
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

    void addSymlink(const Zstring& itemName, const LinkAttributes& attr)
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

std::enable_shared_from_this   PathInformation
           /|\                      /|\
            |____________   _________|_________
                         | |                   |
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
    using FileList    = std::vector<zen::SharedRef<FilePair>>;    //MergeSides::execute() requires a structure that doesn't invalidate pointers after push_back()
    using SymlinkList = std::vector<zen::SharedRef<SymlinkPair>>; //
    using FolderList  = std::vector<zen::SharedRef<FolderPair>>;

    FolderPair& addFolder(const Zstring& itemNameL, const FolderAttributes& attribL,
                          const Zstring& itemNameR, const FolderAttributes& attribR); //exists on both sides

    template <SelectSide side>
    FolderPair& addFolder(const Zstring& itemName, const FolderAttributes& attr); //exists on one side only

    FilePair& addFile(const Zstring& itemNameL, const FileAttributes& attribL,
                      const Zstring& itemNameR, const FileAttributes& attribR); //exists on both sides

    template <SelectSide side>
    FilePair& addFile(const Zstring& itemName, const FileAttributes& attr); //exists on one side only

    SymlinkPair& addSymlink(const Zstring& itemNameL, const LinkAttributes& attribL,
                            const Zstring& itemNameR, const LinkAttributes& attribR); //exists on both sides

    template <SelectSide side>
    SymlinkPair& addSymlink(const Zstring& itemName, const LinkAttributes& attr); //exists on one side only

    zen::Range<zen::DerefIter<FileList::const_iterator, const FilePair>> files() const { return {files_.begin(), files_.end()}; }
    zen::Range<zen::DerefIter<FileList::      iterator,       FilePair>> files()       { return {files_.begin(), files_.end()}; }

    zen::Range<zen::DerefIter<SymlinkList::const_iterator, const SymlinkPair>> symlinks() const { return {symlinks_.begin(), symlinks_.end()}; }
    zen::Range<zen::DerefIter<SymlinkList::      iterator,       SymlinkPair>> symlinks()       { return {symlinks_.begin(), symlinks_.end()}; }

    zen::Range<zen::DerefIter<FolderList::const_iterator, const FolderPair>> subfolders() const { return {subfolders_.begin(), subfolders_.end()}; }
    zen::Range<zen::DerefIter<FolderList::      iterator,       FolderPair>> subfolders()       { return {subfolders_.begin(), subfolders_.end()}; }

    void clearFiles     () { files_     .clear(); }
    void clearSymlinks  () { symlinks_  .clear(); }
    void clearSubfolders() { subfolders_.clear(); }

    template <class Function>
    void foldersRemoveIf(Function fun) { zen::eraseIf(subfolders_, [fun](auto& fsObj) { return fun(fsObj.ref()); }); }

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

    FileList    files_;
    SymlinkList symlinks_;
    FolderList  subfolders_;

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


using FolderComparison = std::vector<zen::SharedRef<BaseFolderPair>>; //make sure pointers to sub-elements remain valid
//don't change this back to std::vector<BaseFolderPair> inconsiderately: comparison uses push_back to add entries which may result in a full copy!

zen::Range<zen::DerefIter<FolderComparison::iterator,             BaseFolderPair>> inline asRange(      FolderComparison& vect) { return {vect.begin(), vect.end()}; }
zen::Range<zen::DerefIter<FolderComparison::const_iterator, const BaseFolderPair>> inline asRange(const FolderComparison& vect) { return {vect.begin(), vect.end()}; }

//------------------------------------------------------------------
struct FSObjectVisitor
{
    virtual ~FSObjectVisitor() {}
    virtual void visit(const FilePair&    file   ) = 0;
    virtual void visit(const SymlinkPair& symlink) = 0;
    virtual void visit(const FolderPair&  folder ) = 0;
};

//------------------------------------------------------------------

class FileSystemObject : public std::enable_shared_from_this<FileSystemObject>, public virtual PathInformation
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
    Zstring itemNameR_; //class invariant: same Zstring.c_str() pointer iff equal!

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


    void setMovePair(FilePair* ref); //reference to corresponding moved/renamed file
    FilePair* getMovePair() const; //may be nullptr

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

    std::weak_ptr<FilePair> moveFileRef_; //optional, filled by DetectMovedFiles::findAndSetMovePair()

    FileContentCategory contentCategory_ = FileContentCategory::unknown;
    Zstringc categoryDescr_; //optional: custom category description (e.g. FileContentCategory::conflict or invalidTime)
};

//------------------------------------------------------------------

class SymlinkPair : public FileSystemObject //models an unresolved symbolic link: followed-links should go in FilePair/FolderPair
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
        for (FilePair& file : conObj.files())
            onFile_(file);
        for (SymlinkPair& symlink : conObj.symlinks())
            onSymlink_(symlink);
        for (FolderPair& subFolder : conObj.subfolders())
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
    {
        selectParam<side>(itemNameL_, itemNameR_) = selectParam<getOtherSide<side>>(itemNameL_, itemNameR_); //ensure (c_str) class invariant!
        setSyncDir(SyncDirection::none); //calls notifySyncCfgChanged()
    }
    else
    {
        selectParam<side>(itemNameL_, itemNameR_).clear();
        //keep current syncDir_
        notifySyncCfgChanged(); //needed!?
    }

    propagateChangedItemName<side>();
}


template <SelectSide side> inline
void FilePair::removeItem()
{
    if (isEmpty<getOtherSide<side>>())
        setMovePair(nullptr); //cut ties between "move" pairs

    selectParam<side>(attrL_, attrR_) = FileAttributes();
    contentCategory_ = FileContentCategory::unknown;
    removeFsObject<side>();
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
    for (FilePair& file : files())
        file.removeItem<side>();
    for (SymlinkPair& symlink : symlinks())
        symlink.removeItem<side>();
    for (FolderPair& folder : subfolders())
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

    for (FolderPair& folder : subfolders())
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
FolderPair& ContainerObject::addFolder(const Zstring& itemNameL, const FolderAttributes& attribL,
                                       const Zstring& itemNameR, const FolderAttributes& attribR)
{
    subfolders_.push_back(makeSharedRef<FolderPair>(itemNameL, attribL, itemNameR, attribR, *this));
    return subfolders_.back().ref();
}


template <> inline
FolderPair& ContainerObject::addFolder<SelectSide::left>(const Zstring& itemName, const FolderAttributes& attr)
{
    return addFolder(itemName, attr, Zstring(), FolderAttributes());
}


template <> inline
FolderPair& ContainerObject::addFolder<SelectSide::right>(const Zstring& itemName, const FolderAttributes& attr)
{
    return addFolder(Zstring(), FolderAttributes(), itemName, attr);
}


inline
FilePair& ContainerObject::addFile(const Zstring& itemNameL, const FileAttributes& attribL,
                                   const Zstring& itemNameR, const FileAttributes& attribR)
{
    files_.push_back(makeSharedRef<FilePair>(itemNameL, attribL, itemNameR, attribR, *this));
    return files_.back().ref();
}


template <> inline
FilePair& ContainerObject::addFile<SelectSide::left>(const Zstring& itemName, const FileAttributes& attr)
{
    return addFile(itemName, attr, Zstring(), FileAttributes());
}


template <> inline
FilePair& ContainerObject::addFile<SelectSide::right>(const Zstring& itemName, const FileAttributes& attr)
{
    return addFile(Zstring(), FileAttributes(), itemName, attr);
}


inline
SymlinkPair& ContainerObject::addSymlink(const Zstring& itemNameL, const LinkAttributes& attribL,
                                         const Zstring& itemNameR, const LinkAttributes& attribR)
{
    symlinks_.push_back(makeSharedRef<SymlinkPair>(itemNameL, attribL, itemNameR, attribR, *this));
    return symlinks_.back().ref();
}


template <> inline
SymlinkPair& ContainerObject::addSymlink<SelectSide::left>(const Zstring& itemName, const LinkAttributes& attr)
{
    return addSymlink(itemName, attr, Zstring(), LinkAttributes());
}


template <> inline
SymlinkPair& ContainerObject::addSymlink<SelectSide::right>(const Zstring& itemName, const LinkAttributes& attr)
{
    return addSymlink(Zstring(), LinkAttributes(), itemName, attr);
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
    for (FilePair& file : files())
        file.flip();
    for (SymlinkPair& symlink : symlinks())
        symlink.flip();
    for (FolderPair& folder : subfolders())
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


inline
void FilePair::setMovePair(FilePair* ref)
{
    FilePair* refOld = getMovePair();
    if (ref != refOld)
    {
        if (refOld)
            refOld->moveFileRef_.reset();

        if (ref)
        {
            FilePair* refOld2 = ref->getMovePair();
            assert(!refOld2); //destroying already exising pair!? why?
            if (refOld2)
                refOld2 ->moveFileRef_.reset();

            /**/ moveFileRef_ = std::static_pointer_cast<FilePair>(ref->shared_from_this());
            ref->moveFileRef_ = std::static_pointer_cast<FilePair>(     shared_from_this());
        }
        else
            moveFileRef_.reset();
    }
    else
        assert(!ref); //are we called needlessly!?
}


inline
FilePair* FilePair::getMovePair() const
{
    if (moveFileRef_.expired()) //skip std::shared_ptr construction => premature optimization?
        return nullptr;

    FilePair* ref = moveFileRef_.lock().get();
    assert(!ref || (isEmpty<SelectSide::left>() != isEmpty<SelectSide::right>()));
    assert(!ref || ref->moveFileRef_.lock().get() == this); //both ends should agree
    return ref;
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
    setMovePair(nullptr); //cut ties between "move" pairs

    selectParam<             sideTrg >(attrL_, attrR_) = {lastWriteTimeTrg, fileSize, filePrintTrg, isSymlinkTrg};
    selectParam<getOtherSide<sideTrg>>(attrL_, attrR_) = {lastWriteTimeSrc, fileSize, filePrintSrc, isSymlinkSrc};

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

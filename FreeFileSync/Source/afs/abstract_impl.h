// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef IMPL_HELPER_H_873450978453042524534234
#define IMPL_HELPER_H_873450978453042524534234

#include "abstract.h"
#include <zen/thread.h>
#include <zen/stream_buffer.h>


namespace fff
{
template <class Function> inline //return ignored error message if available
std::wstring tryReportingDirError(Function cmd /*throw FileError, X*/, AbstractFileSystem::TraverserCallback& cb /*throw X*/)
{
    for (size_t retryNumber = 0;; ++retryNumber)
        try
        {
            cmd(); //throw FileError, X
            return std::wstring();
        }
        catch (const zen::FileError& e)
        {
            assert(!e.toString().empty());
            switch (cb.reportDirError({e.toString(), std::chrono::steady_clock::now(), retryNumber})) //throw X
            {
                case AbstractFileSystem::TraverserCallback::HandleError::ignore:
                    return e.toString();
                case AbstractFileSystem::TraverserCallback::HandleError::retry:
                    break; //continue with loop
            }
        }
}

template <class Command> inline
bool tryReportingItemError(Command cmd, AbstractFileSystem::TraverserCallback& callback, const Zstring& itemName) //throw X, return "true" on success, "false" if error was ignored
{
    for (size_t retryNumber = 0;; ++retryNumber)
        try
        {
            cmd(); //throw FileError
            return true;
        }
        catch (const zen::FileError& e)
        {
            switch (callback.reportItemError({e.toString(), std::chrono::steady_clock::now(), retryNumber}, itemName)) //throw X
            {
                case AbstractFileSystem::TraverserCallback::HandleError::retry:
                    break;
                case AbstractFileSystem::TraverserCallback::HandleError::ignore:
                    return false;
            }
        }
}

//==========================================================================================

//Google Drive/MTP happily create duplicate files/folders with the same names, without failing
//=> however, FFS's "check if already exists after failure" idiom *requires* failure
//=> best effort: serialize access (at path level) so that GdriveFileState existence check and file/folder creation act as a single operation
template <class NativePath>
class PathAccessLocker
{
    struct BlockInfo
    {
        std::mutex m;
        bool itemInUse = false; //protected by mutex!
        /* can we get rid of BlockType::fail and save "bool itemInUse" "somewhere else"?
            Google Drive => put dummy entry in GdriveFileState? problem: there is no fail-free removal: accessGlobalFileState() can throw!
            MTP          => no (buffered) state                                                   */
    };
public:
    PathAccessLocker() {}

    //how to handle *other* access attempts while holding the lock:
    enum class BlockType
    {
        otherWait,
        otherFail
    };

    class Lock
    {
    public:
        Lock(const NativePath& nativePath, BlockType blockType) : blockType_(blockType) //throw SysError
        {
            using namespace zen;

            if (const std::shared_ptr<PathAccessLocker> pal = getGlobalInstance())
                pal->protPathLocks_.access([&](std::map<NativePath, std::weak_ptr<BlockInfo>>& pathLocks)
            {
                //clean up obsolete entries
                std::erase_if(pathLocks, [](const auto& v) { return !v.second.lock(); });

                //get or create:
                std::weak_ptr<BlockInfo>& weakPtr = pathLocks[nativePath];
                blockInfo_ = weakPtr.lock();
                if (!blockInfo_)
                    weakPtr = blockInfo_ = std::make_shared<BlockInfo>();
            });
            else
                throw SysError(L"PathAccessLocker::Lock() function call not allowed during init/shutdown.");

            blockInfo_->m.lock();

            if (blockInfo_->itemInUse)
            {
                blockInfo_->m.unlock();
                throw SysError(replaceCpy(_("The item %x is currently in use."), L"%x", fmtPath(getItemName(nativePath))));
            }

            if (blockType == BlockType::otherFail)
            {
                blockInfo_->itemInUse = true;
                blockInfo_->m.unlock();
            }
        }

        ~Lock()
        {
            if (blockType_ == BlockType::otherFail)
            {
                blockInfo_->m.lock();
                blockInfo_->itemInUse = false;
            }

            blockInfo_->m.unlock();
        }

    private:
        Lock           (const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;

        const BlockType blockType_; //[!] needed: we can't instead check "itemInUse" (without locking first)
        std::shared_ptr<BlockInfo> blockInfo_;
    };

private:
    PathAccessLocker           (const PathAccessLocker&) = delete;
    PathAccessLocker& operator=(const PathAccessLocker&) = delete;

    static std::shared_ptr<PathAccessLocker> getGlobalInstance();
    static Zstring getItemName(const NativePath& nativePath);

    zen::Protected<std::map<NativePath, std::weak_ptr<BlockInfo>>> protPathLocks_;
};

}

#endif //IMPL_HELPER_H_873450978453042524534234

#ifndef LOCK_HOLDER_H_489572039485723453425
#define LOCK_HOLDER_H_489572039485723453425

#include <set>
#include <zen/zstring.h>
#include <zen/stl_tools.h>
#include "dir_lock.h"
#include "status_handler.h"


namespace fff
{
//intermediate locks created by DirLock use this extension, too:
const Zchar LOCK_FILE_ENDING[] = Zstr(".ffs_lock"); //don't use Zstring as global constant: avoid static initialization order problem in global namespace!

//Attention: 1. call after having checked directory existence!
//           2. perf: remove folder aliases (e.g. case differences) *before* calling this function!!!

//hold locks for a number of directories without blocking during lock creation
class LockHolder
{
public:
    LockHolder(const std::set<Zstring>& folderPaths, bool& warnDirectoryLockFailed, ProcessCallback& pcb /*throw X*/)
    {
        using namespace zen;

        std::map<Zstring, FileError> failedLocks;

        for (const Zstring& folderPath : folderPaths)
            try
            {
                //lock file creation is synchronous and may block noticeably for very slow devices (usb sticks, mapped cloud storages)
                lockHolder_.emplace_back(appendSeparator(folderPath) + Zstr("sync") + LOCK_FILE_ENDING,
                [&](const std::wstring& msg) { pcb.reportStatus(msg); /*throw X*/ },
                UI_UPDATE_INTERVAL / 2); //throw FileError
            }
            catch (const FileError& e) { failedLocks.emplace(folderPath, e); }

        if (!failedLocks.empty())
        {
            std::wstring msg = _("Cannot set directory locks for the following folders:");

            for (const auto& [folderPath, error] : failedLocks)
            {
                msg += L"\n\n" + fmtPath(folderPath);
                msg += L"\n" + replaceCpy(error.toString(), L"\n\n", L"\n");
            }

            pcb.reportWarning(msg, warnDirectoryLockFailed); //throw X
        }
    }

private:
    std::vector<DirLock> lockHolder_;
};
}

#endif //LOCK_HOLDER_H_489572039485723453425

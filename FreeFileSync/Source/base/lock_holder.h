#ifndef LOCK_HOLDER_H_489572039485723453425
#define LOCK_HOLDER_H_489572039485723453425

#include <set>
#include <zen/zstring.h>
#include <zen/stl_tools.h>
#include "dir_lock.h"
#include "process_callback.h"


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
    LockHolder(const std::set<Zstring>& folderPaths, bool& warnDirectoryLockFailed, PhaseCallback& pcb /*throw X*/)
    {
        using namespace zen;

        std::vector<std::pair<Zstring, FileError>> failedLocks;

        for (const Zstring& folderPath : folderPaths)
            try
            {
                //lock file creation is synchronous and may block noticeably for very slow devices (USB sticks, mapped cloud storage)
                lockHolder_.emplace_back(appendSeparator(folderPath) + Zstr("sync") + LOCK_FILE_ENDING,
                [&](std::wstring&& msg) { pcb.updateStatus(std::move(msg)); /*throw X*/ },
                UI_UPDATE_INTERVAL / 2); //throw FileError
            }
            catch (const FileError& e) { failedLocks.emplace_back(folderPath, e); }

        if (!failedLocks.empty())
        {
            std::wstring msg = _("Cannot set directory locks for the following folders:");

            for (const auto& [folderPath, error] : failedLocks)
            {
                msg += L"\n\n";
                //msg += fmtPath(folderPath) + L'\n' -> seems redundant
                msg += replaceCpy(error.toString(), L"\n\n", L'\n');
            }

            pcb.reportWarning(msg, warnDirectoryLockFailed); //throw X
        }
    }

private:
    std::vector<DirLock> lockHolder_;
};
}

#endif //LOCK_HOLDER_H_489572039485723453425

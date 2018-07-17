// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef GENERATE_LOGFILE_H_931726432167489732164
#define GENERATE_LOGFILE_H_931726432167489732164

#include <zen/error_log.h>
#include "ffs_paths.h"
#include "file_hierarchy.h"


namespace fff
{
struct LogSummary
{
    std::wstring jobName; //may be empty
    std::wstring finalStatus;
    int     itemsProcessed = 0;
    int64_t bytesProcessed = 0;
    int     itemsTotal = 0;
    int64_t bytesTotal = 0;
    int64_t totalTime = 0; //unit: [sec]
};

void streamToLogFile(const LogSummary& summary, //throw FileError
                     const zen::ErrorLog& log,
                     AFS::OutputStream& streamOut);

void saveToLastSyncsLog(const LogSummary& summary, //throw FileError
                        const zen::ErrorLog& log,
                        size_t maxBytesToWrite,
                        const std::function<void(const std::wstring& msg)>& notifyStatus);


inline Zstring getDefaultLogFolderPath() { return getConfigDirPathPf() + Zstr("Logs") ; }

void limitLogfileCount(const AbstractPath& logFolderPath, const std::wstring& jobname, size_t maxCount, //throw FileError
                       const std::function<void(const std::wstring& msg)>& notifyStatus);
}

#endif //GENERATE_LOGFILE_H_931726432167489732164

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef GENERATE_LOGFILE_H_931726432167489732164
#define GENERATE_LOGFILE_H_931726432167489732164

#include <chrono>
#include <zen/error_log.h>
#include "return_codes.h"
#include "status_handler.h"
#include "../afs/abstract.h"


namespace fff
{
Zstring getDefaultLogFolderPath();


AbstractPath generateLogFilePath(const ProcessSummary& summary, const Zstring& altLogFolderPathPhrase /*optional*/);

void saveLogFile(const AbstractPath& logFilePath, //throw FileError, X
                 const ProcessSummary& summary,
                 const zen::ErrorLog& log,
                 int logfilesMaxAgeDays,
                 const std::set<AbstractPath>& logFilePathsToKeep,
                 const std::function<void(const std::wstring& msg)>& notifyStatus /*throw X*/);

void sendLogAsEmail(const Zstring& email, //throw FileError, X
                    const ProcessSummary& summary,
                    const zen::ErrorLog& log,
                    const AbstractPath& logFilePath,
                    const std::function<void(const std::wstring& msg)>& notifyStatus /*throw X*/);
}

#endif //GENERATE_LOGFILE_H_931726432167489732164

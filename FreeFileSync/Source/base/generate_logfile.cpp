// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "generate_logfile.h"
#include <zen/file_io.h>
#include <wx/datetime.h>
#include "ffs_paths.h"
#include "../fs/concrete.h"

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


namespace
{
std::wstring generateLogHeader(const ProcessSummary& s, const ErrorLog& log, const std::wstring& finalStatusMsg)
{
    //assemble summary box
    std::vector<std::wstring> summary;

    const std::wstring tabSpace(4, L' '); //4, the one true space count for tabs

    std::wstring headerLine = formatTime<std::wstring>(FORMAT_DATE); //+ L" [" + formatTime<std::wstring>(FORMAT_TIME, startTime + L"]";
    if (!s.jobName.empty())
        headerLine += L"  " + s.jobName;

    summary.push_back(headerLine);
    summary.push_back(L"");
    summary.push_back(tabSpace +  finalStatusMsg);


    const int errorCount   = log.getItemCount(MSG_TYPE_ERROR | MSG_TYPE_FATAL_ERROR);
    const int warningCount = log.getItemCount(MSG_TYPE_WARNING);

    if (errorCount   > 0) summary.push_back(tabSpace + _("Errors:")   + L" " + formatNumber(errorCount));
    if (warningCount > 0) summary.push_back(tabSpace + _("Warnings:") + L" " + formatNumber(warningCount));


    std::wstring itemsProc = tabSpace + _("Items processed:") + L" " + formatNumber(s.statsProcessed.items); //show always, even if 0!
    itemsProc += L" (" + formatFilesizeShort(s.statsProcessed.bytes) + L")";
    summary.push_back(itemsProc);

    if ((s.statsTotal.items < 0 && s.statsTotal.bytes < 0) || //no total items/bytes: e.g. for pure folder comparison
        s.statsProcessed == s.statsTotal) //...if everything was processed successfully
        ;
    else
        summary.push_back(tabSpace + _("Items remaining:") +
                          L" "  + formatNumber       (s.statsTotal.items - s.statsProcessed.items) +
                          L" (" + formatFilesizeShort(s.statsTotal.bytes - s.statsProcessed.bytes) + L")");

    const int64_t totalTimeSec = std::chrono::duration_cast<std::chrono::seconds>(s.totalTime).count();
    summary.push_back(tabSpace + _("Total time:") + L" " + copyStringTo<std::wstring>(wxTimeSpan::Seconds(totalTimeSec).Format()));

    //calculate max width, this considers UTF-16, not Unicode code points...but maybe good idea? those 2-byte-UTF16 chars are usually wider than fixed-width chars anyway!
    size_t sepLineLen = 0;
    for (const std::wstring& str : summary) sepLineLen = std::max(sepLineLen, str.size());

    std::wstring output(sepLineLen + 1, L'_');
    output += L'\n';

    for (const std::wstring& str : summary) { output += L'|'; output += str; output += L'\n'; }

    output += L'|';
    output.append(sepLineLen, L'_');
    output += L'\n';

    return output;
}


void streamToLogFile(const ProcessSummary& summary, //throw FileError
                     const ErrorLog& log,
                     const std::wstring& finalStatusLabel,
                     AFS::OutputStream& streamOut)
{
    auto fmtForTxtFile = [needLbReplace = !equalString(LINE_BREAK, '\n')](const std::wstring& str)
    {
        std::string utfStr = utfTo<std::string>(str);
        if (needLbReplace)
            replace(utfStr, '\n', LINE_BREAK);
        return utfStr;
    };

    std::string buffer = fmtForTxtFile(generateLogHeader(summary, log, finalStatusLabel)); //don't replace line break any earlier

    streamOut.write(&buffer[0], buffer.size()); //throw FileError, X
    buffer.clear(); //flush out header if entry.empty()

    buffer += LINE_BREAK;

    for (const LogEntry& entry : log)
    {
        buffer += fmtForTxtFile(formatMessage(entry));
        buffer += LINE_BREAK;

        //write log items in blocks instead of creating one big string: memory allocation might fail; think 1 million entries!
        streamOut.write(&buffer[0], buffer.size()); //throw FileError, X
        buffer.clear();
    }
}


const int TIME_STAMP_LENGTH = 21;
const Zchar STATUS_BEGIN_TOKEN[] = Zstr(" [");
const Zchar STATUS_END_TOKEN     = Zstr(']');

//"Backup FreeFileSync 2013-09-15 015052.123.log" ->
//"Backup FreeFileSync 2013-09-15 015052.123 [Error].log"
AbstractPath saveNewLogFile(const ProcessSummary& summary, //throw FileError
                            const ErrorLog& log,
                            const AbstractPath& logFolderPath,
                            const std::chrono::system_clock::time_point& syncStartTime,
                            const std::function<void(const std::wstring& msg)>& notifyStatus /*throw X*/)
{
    //create logfile folder if required
    AFS::createFolderIfMissingRecursion(logFolderPath); //throw FileError

    //const std::string colon = "\xcb\xb8"; //="modifier letter raised colon" => regular colon is forbidden in file names on Windows and OS X
    //=> too many issues, most notably cmd.exe is not Unicode-aware: https://freefilesync.org/forum/viewtopic.php?t=1679

    //assemble logfile name
    const TimeComp tc = getLocalTime(std::chrono::system_clock::to_time_t(syncStartTime));
    if (tc == TimeComp())
        throw FileError(L"Failed to determine current time: " + numberTo<std::wstring>(syncStartTime.time_since_epoch().count()));

    const auto timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(syncStartTime.time_since_epoch()).count() % 1000;
    assert(std::chrono::duration_cast<std::chrono::seconds>(syncStartTime.time_since_epoch()).count() == std::chrono::system_clock::to_time_t(syncStartTime));

    Zstring logFileName;

    if (!summary.jobName.empty())
        logFileName += utfTo<Zstring>(summary.jobName) + Zstr(' ');

    logFileName += formatTime<Zstring>(Zstr("%Y-%m-%d %H%M%S"), tc) +
                   Zstr(".") + printNumber<Zstring>(Zstr("%03d"), static_cast<int>(timeMs)); //[ms] should yield a fairly unique name
    static_assert(TIME_STAMP_LENGTH == 21);

    const std::wstring failStatus = [&]
    {
        switch (summary.finalStatus)
        {
            case SyncResult::FINISHED_WITH_SUCCESS:
                break;
            case SyncResult::FINISHED_WITH_WARNINGS:
                return _("Warning");
            case SyncResult::FINISHED_WITH_ERROR:
                return _("Error");
            case SyncResult::ABORTED:
                return _("Stopped");
        }
        return std::wstring();
    }();

    if (!failStatus.empty())
        logFileName += STATUS_BEGIN_TOKEN + utfTo<Zstring>(failStatus) + STATUS_END_TOKEN;
    logFileName += Zstr(".log");

    const AbstractPath logFilePath = AFS::appendRelPath(logFolderPath, logFileName);

    auto notifyUnbufferedIO = [notifyStatus,
                               bytesWritten_ = int64_t(0),
                               msg_ = replaceCpy(_("Saving file %x..."), L"%x", fmtPath(AFS::getDisplayPath(logFilePath)))]
         (int64_t bytesDelta) mutable
    {
        if (notifyStatus)
            notifyStatus(msg_ + L" (" + formatFilesizeShort(bytesWritten_ += bytesDelta) + L")"); //throw X
    };

    const std::wstring& finalStatusLabel = getFinalStatusLabel(summary.finalStatus);

    std::unique_ptr<AFS::OutputStream> logFileStream = AFS::getOutputStream(logFilePath, std::nullopt /*streamSize*/, std::nullopt /*modTime*/, notifyUnbufferedIO); //throw FileError
    streamToLogFile(summary, log, finalStatusLabel, *logFileStream); //throw FileError, X
    logFileStream->finalize();                                       //throw FileError, X

    return logFilePath;
}


struct LogFileInfo
{
    AbstractPath filePath;
    time_t       timeStamp;
    std::wstring jobName; //may be empty
};
std::vector<LogFileInfo> getLogFiles(const AbstractPath& logFolderPath) //throw FileError
{
    std::vector<LogFileInfo> logfiles;

    AFS::traverseFolderFlat(logFolderPath, [&](const AFS::FileInfo& fi) //throw FileError
    {
        //"Backup FreeFileSync 2013-09-15 015052.123.log"
        //"2013-09-15 015052.123 [Error].log"
        static_assert(TIME_STAMP_LENGTH == 21);

        if (endsWith(fi.itemName, Zstr(".log"))) //case-sensitive: e.g. ".LOG" is not from FFS, right?
        {
            auto tsBegin = fi.itemName.begin();
            auto tsEnd   = fi.itemName.end() - 4;

            if (tsBegin != tsEnd && tsEnd[-1] == STATUS_END_TOKEN)
                tsEnd = searchLast(tsBegin, tsEnd,
                                   std::begin(STATUS_BEGIN_TOKEN), std::end(STATUS_BEGIN_TOKEN) - 1);

            if (tsEnd - tsBegin >= TIME_STAMP_LENGTH &&
                tsEnd[-4] == Zstr('.') &&
                isdigit(tsEnd[-3]) &&
                isdigit(tsEnd[-2]) &&
                isdigit(tsEnd[-1]))
            {
                tsBegin = tsEnd - TIME_STAMP_LENGTH;
                const TimeComp tc = parseTime(Zstr("%Y-%m-%d %H%M%S"), StringRef<const Zchar>(tsBegin, tsBegin + 17)); //returns TimeComp() on error
                const time_t t = localToTimeT(tc); //returns -1 on error
                if (t != -1)
                {
                    Zstring jobName(fi.itemName.begin(), tsBegin);
                    if (!jobName.empty())
                    {
                        assert(jobName.size() >= 2 && jobName.end()[-1] == Zstr(' '));
                        jobName.pop_back();
                    }

                    logfiles.push_back({ AFS::appendRelPath(logFolderPath, fi.itemName), t, utfTo<std::wstring>(jobName) });
                }
            }
        }
    },
    nullptr /*onFolder*/, //traverse only one level deep
    nullptr /*onSymlink*/);

    return logfiles;
}


void limitLogfileCount(const AbstractPath& logFolderPath, //throw FileError
                       int logfilesMaxAgeDays, //<= 0 := no limit
                       const std::set<AbstractPath>& logFilePathsToKeep,
                       const std::function<void(const std::wstring& msg)>& notifyStatus)
{
    if (logfilesMaxAgeDays > 0)
    {
        if (notifyStatus) notifyStatus(_("Cleaning up log files:") + L" " + fmtPath(AFS::getDisplayPath(logFolderPath)));

        std::vector<LogFileInfo> logFiles = getLogFiles(logFolderPath); //throw FileError

        const time_t lastMidnightTime = []
        {
            TimeComp tc = getLocalTime(); //returns TimeComp() on error
            tc.second = 0;
            tc.minute = 0;
            tc.hour   = 0;
            return localToTimeT(tc); //returns -1 on error => swallow => no versions trimmed by versionMaxAgeDays
        }();
        const time_t cutOffTime = lastMidnightTime - static_cast<time_t>(logfilesMaxAgeDays) * 24 * 3600;

        std::exception_ptr firstError;

        for (const LogFileInfo& lfi : logFiles)
            if (lfi.timeStamp < cutOffTime &&
                logFilePathsToKeep.find(lfi.filePath) == logFilePathsToKeep.end()) //don't trim latest log files corresponding to last used config files!
                //nitpicker's corner: what about path differences due to case? e.g. user-overriden log file path changed in case
            {
                if (notifyStatus) notifyStatus(_("Cleaning up log files:") + L" " + fmtPath(AFS::getDisplayPath(lfi.filePath)));
                try
                {
                    AFS::removeFilePlain(lfi.filePath); //throw FileError
                }
                catch (const FileError&) { if (!firstError) firstError = std::current_exception(); };
            }

        if (firstError) //late failure!
            std::rethrow_exception(firstError);
    }
}
}


Zstring fff::getDefaultLogFolderPath() { return getConfigDirPathPf() + Zstr("Logs") ; }


AbstractPath fff::saveLogFile(const ProcessSummary& summary, //throw FileError
                              const ErrorLog& log,
                              const std::chrono::system_clock::time_point& syncStartTime,
                              const Zstring& altLogFolderPathPhrase, //optional
                              int logfilesMaxAgeDays,
                              const std::set<AbstractPath>& logFilePathsToKeep,
                              const std::function<void(const std::wstring& msg)>& notifyStatus /*throw X*/)
{
    AbstractPath logFolderPath = createAbstractPath(altLogFolderPathPhrase);
    if (AFS::isNullPath(logFolderPath))
        logFolderPath = createAbstractPath(getDefaultLogFolderPath());

    std::optional<AbstractPath> logFilePath;
    std::exception_ptr firstError;
    try
    {
        logFilePath = saveNewLogFile(summary, log, logFolderPath, syncStartTime, notifyStatus); //throw FileError, X
    }
    catch (const FileError&) { if (!firstError) firstError = std::current_exception(); };

    try
    {
        limitLogfileCount(logFolderPath, logfilesMaxAgeDays, logFilePathsToKeep, notifyStatus); //throw FileError, X
    }
    catch (const FileError&) { if (!firstError) firstError = std::current_exception(); };

    if (firstError) //late failure!
        std::rethrow_exception(firstError);

    return *logFilePath;
}

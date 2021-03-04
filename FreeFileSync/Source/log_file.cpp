// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "log_file.h"
#include <zen/file_io.h>
#include <zen/http.h>
#include <zen/sys_info.h>
#include <wx/datetime.h>
#include "ffs_paths.h"
#include "afs/concrete.h"

using namespace zen;
using namespace fff;
using AFS = AbstractFileSystem;


namespace
{
const int LOG_PREVIEW_FAIL_MAX = 25;
const int SEPARATION_LINE_LEN = 40;


std::string generateLogHeaderTxt(const ProcessSummary& s, const ErrorLog& log, int logPreviewFailsMax)
{
    const std::string tabSpace(4, ' '); //4: the only sensible space count for tabs

    std::string headerLine;
    for (const std::wstring& jobName : s.jobNames)
        headerLine += (headerLine.empty() ? "" : " + ") + utfTo<std::string>(jobName);

    if (!headerLine.empty())
        headerLine += ' ';

    const TimeComp tc = getLocalTime(std::chrono::system_clock::to_time_t(s.startTime)); //returns empty string on failure
    headerLine += utfTo<std::string>(formatTime(formatDateTag, tc) + Zstr(" [") + formatTime(formatTimeTag, tc) + Zstr(']'));

    //assemble summary box
    std::vector<std::string> summary;
    summary.emplace_back();
    summary.push_back(tabSpace + utfTo<std::string>(getSyncResultLabel(s.syncResult)));
    summary.emplace_back();

    const ErrorLog::Stats logCount = log.getStats();

    if (logCount.error   > 0) summary.push_back(tabSpace + utfTo<std::string>(_("Errors:")   + L' ' + formatNumber(logCount.error)));
    if (logCount.warning > 0) summary.push_back(tabSpace + utfTo<std::string>(_("Warnings:") + L' ' + formatNumber(logCount.warning)));

    summary.push_back(tabSpace + utfTo<std::string>(_("Items processed:") + L' ' + formatNumber(s.statsProcessed.items) + //show always, even if 0!
                                                    L" (" + formatFilesizeShort(s.statsProcessed.bytes) + L')'));

    if ((s.statsTotal.items < 0 && s.statsTotal.bytes < 0) || //no total items/bytes: e.g. for pure folder comparison
        s.statsProcessed == s.statsTotal) //...if everything was processed successfully
        ;
    else
        summary.push_back(tabSpace + utfTo<std::string>(_("Items remaining:") +
                                                        L' '  + formatNumber       (s.statsTotal.items - s.statsProcessed.items) +
                                                        L" (" + formatFilesizeShort(s.statsTotal.bytes - s.statsProcessed.bytes) + L')'));

    const int64_t totalTimeSec = std::chrono::duration_cast<std::chrono::seconds>(s.totalTime).count();
    summary.push_back(tabSpace + utfTo<std::string>(_("Total time:")) + ' ' + utfTo<std::string>(wxTimeSpan::Seconds(totalTimeSec).Format()));

    size_t sepLineLen = 0; //calculate max width (considering Unicode!)
    for (const std::string& str : summary) sepLineLen = std::max(sepLineLen, unicodeLength(str));

    std::string output = headerLine + '\n';
    output += std::string(sepLineLen + 1, '_') + '\n';

    for (const std::string& str : summary)
        output += '|' + str + '\n';

    output += '|' + std::string(sepLineLen, '_') + "\n\n";

    //------------ warnings/errors preview ----------------
    const int logFailTotal = logCount.warning + logCount.error;
    if (logFailTotal > 0)
    {
        output += '\n' + utfTo<std::string>(_("Errors and warnings:")) + '\n';
        output += std::string(SEPARATION_LINE_LEN, '_') + '\n';

        int previewCount = 0;
        if (logPreviewFailsMax > 0)
            for (const LogEntry& entry : log)
                if (entry.type & (MSG_TYPE_WARNING | MSG_TYPE_ERROR))
                {
                    output += utfTo<std::string>(formatMessage(entry));
                    if (++previewCount >= logPreviewFailsMax)
                        break;
                }
        if (logFailTotal > previewCount)
            output += "  [...]  " + utfTo<std::string>(replaceCpy(_P("Showing %y of 1 item", "Showing %y of %x items", logFailTotal), //%x used as plural form placeholder!
                                                                  L"%y", formatNumber(previewCount))) + '\n';
        output += std::string(SEPARATION_LINE_LEN, '_') + "\n\n\n";
    }
    return output;
}


std::string generateLogFooterTxt(const std::wstring& logFilePath, int logItemsTotal, int logPreviewItemsMax) //throw FileError
{
    const ComputerModel cm = getComputerModel(); //throw FileError

    std::string output;
    if (logItemsTotal > logPreviewItemsMax)
        output += "  [...]  " + utfTo<std::string>(replaceCpy(_P("Showing %y of 1 item", "Showing %y of %x items", logItemsTotal), //%x used as plural form placeholder!
                                                              L"%y", formatNumber(logPreviewItemsMax))) + '\n';

    return output += '\n' + std::string(SEPARATION_LINE_LEN, '_') + '\n' +

                     utfTo<std::string>(getOsDescription() + /*throw FileError*/ +
                                        L" [" +  utfTo<std::wstring>(getUserName()) /*throw FileError*/ + L']' +
                                        (!cm.model .empty() ? L" - " + cm.model  : L"") +
                                        (!cm.vendor.empty() ? L" - " + cm.vendor : L"") + L'\n' +

                                        _("Log file:") + L' ' + logFilePath) + '\n';
}


std::string htmlTxt(const std::string_view& str)
{
    std::string msg = htmlSpecialChars(str);
    trim(msg);
    if (!contains(msg, '\n'))
        return msg;

    std::string msgFmt;
    for (auto it = msg.begin(); it != msg.end(); )
        if (*it == '\n')
        {
            msgFmt += "<br>\n";
            ++it;

            //skip duplicate newlines
            for (; it != msg.end() && *it == L'\n'; ++it)
                ;

            //preserve leading spaces
            for (; it != msg.end() && *it == L' '; ++it)
                msgFmt += "&nbsp;";
        }
        else
            msgFmt += *it++;

    return msgFmt;
}

std::string htmlTxt(const      Zstring& str) { return htmlTxt(utfTo<std::string>(str)); }
std::string htmlTxt(const std::wstring& str) { return htmlTxt(utfTo<std::string>(str)); }
std::string htmlTxt(const      wchar_t* str) { return htmlTxt(utfTo<std::string>(str)); }


//Astyle screws up royally with the following raw string literals!
//*INDENT-OFF*
std::string formatMessageHtml(const LogEntry& entry)
{
    const std::string typeLabel = htmlTxt(getMessageTypeLabel(entry.type));
    const char* typeImage = nullptr;
    switch (entry.type)
    {
        case MSG_TYPE_INFO:    typeImage = "msg-info.png";    break;
        case MSG_TYPE_WARNING: typeImage = "msg-warning.png"; break;
        case MSG_TYPE_ERROR:   typeImage = "msg-error.png";   break;
    }

    return R"(		<tr>
            <td valign="top">)" + htmlTxt(formatTime(formatTimeTag, getLocalTime(entry.time))) + R"(</td>
            <td valign="top"><img src="https://freefilesync.org/images/log/)" + typeImage + R"(" height="16" alt=")" + typeLabel + R"(:"></td>
            <td>)" + htmlTxt(makeStringView(entry.message.begin(), entry.message.end())) + R"(</td>
        </tr>
)";
}


std::wstring generateLogTitle(const ProcessSummary& s)
{
    std::wstring jobNamesFmt;
    for (const std::wstring& jobName : s.jobNames)
        jobNamesFmt += (jobNamesFmt.empty() ? L"" : L" + ") + jobName;

    std::wstring title = L"[FreeFileSync] ";

    if (!jobNamesFmt.empty())
        title += jobNamesFmt + L' ';

    switch (s.syncResult)
    {
        case SyncResult::finishedSuccess: title += utfTo<std::wstring>("\xe2\x9c\x94" "\xef\xb8\x8f"); break; //✔️
        case SyncResult::finishedWarning: title += utfTo<std::wstring>("\xe2\x9a\xa0" "\xef\xb8\x8f"); break; //⚠️
        case SyncResult::finishedError: //efb88f (U+FE0F): variation selector-16 to prefer emoji over text rendering
        case SyncResult::aborted:         title += utfTo<std::wstring>("\xe2\x9d\x8c" "\xef\xb8\x8f"); break; //❌️
    }
    return title;
}


std::string generateLogHeaderHtml(const ProcessSummary& s, const ErrorLog& log, int logPreviewFailsMax)
{
    std::string output = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" + htmlTxt(generateLogTitle(s)) + R"(</title>
    <style>
)" +   /*caveat: non-inline CSS is often ignored by email clients!*/ R"(
        .summary-table td:nth-child(1) { padding-right: 10px; }
        .summary-table td:nth-child(2) { padding-right:  5px; }
        .summary-table img { display: block; }

        .log-items img { display: block; }
        .log-items td { padding-bottom: 0.1em; }
        .log-items td:nth-child(1) { padding-right: 10px; white-space: nowrap; }
        .log-items td:nth-child(2) { padding-right: 10px; }
    </style>
</head>
<body style="font-family: -apple-system, 'Segoe UI', Arial, Tahoma, Helvetica, sans-serif;">
)";

    std::string jobNamesFmt;
    for (const std::wstring& jobName : s.jobNames)
        jobNamesFmt += (jobNamesFmt.empty() ? "" : " + ") + htmlTxt(jobName);

    const TimeComp tc = getLocalTime(std::chrono::system_clock::to_time_t(s.startTime)); //returns empty string on failure
    output += R"(	<div><span style="font-weight:600; color:gray;">)" + jobNamesFmt + R"(</span> &nbsp;<span style="white-space:nowrap">)" +
              htmlTxt(formatTime(formatDateTag, tc)) + " &nbsp;" + htmlTxt(formatTime(formatTimeTag, tc)) + "</span></div>\n";

    std::string resultsStatusImage;
    switch (s.syncResult)
    {
        case SyncResult::finishedSuccess: resultsStatusImage = "result-succes.png"; break;
        case SyncResult::finishedWarning: resultsStatusImage = "result-warning.png"; break;
        case SyncResult::finishedError:
        case SyncResult::aborted:         resultsStatusImage = "result-error.png"; break;
    }
    output += R"(
    <div style="margin:10px 0; display:inline-block; border-radius:7px; background:#f8f8f8; box-shadow:1px 1px 4px #888; overflow:hidden;">
        <div style="background-color:white; border-bottom:1px solid #AAA; font-size:larger; padding:10px;">
            <img src="https://freefilesync.org/images/log/)" + resultsStatusImage + R"(" width="32" height="32" alt="" style="vertical-align:middle;">
            <span style="font-weight:600; vertical-align:middle;">)" + htmlTxt(getSyncResultLabel(s.syncResult)) + R"(</span>
        </div>
        <table role="presentation" class="summary-table" style="border-spacing:0; margin-left:10px; padding:5px 10px;">)";

    const ErrorLog::Stats logCount = log.getStats();

    if (logCount.error > 0) 
        output += R"(
            <tr>
                <td>)" + htmlTxt(_("Errors:")) + R"(</td>
                <td><img src="https://freefilesync.org/images/log/msg-error.png" width="24" height="24" alt=""></td>
                <td><span style="font-weight:600;">)" + htmlTxt(formatNumber(logCount.error)) + R"(</span></td>
            </tr>)";

    if (logCount.warning > 0)
        output += R"(
            <tr>
                <td>)" + htmlTxt(_("Warnings:")) + R"(</td>
                <td><img src="https://freefilesync.org/images/log/msg-warning.png" width="24" height="24" alt=""></td>
                <td><span style="font-weight:600;">)" + htmlTxt(formatNumber(logCount.warning)) + R"(</span></td>
            </tr>)";

    output += R"(
            <tr>
                <td>)" + htmlTxt(_("Items processed:")) + R"(</td>
                <td><img src="https://freefilesync.org/images/log/file.png" width="24" height="24" alt=""></td>
                <td><span style="font-weight:600;">)" + htmlTxt(formatNumber(s.statsProcessed.items)) + "</span> (" + 
                                          htmlTxt(formatFilesizeShort(s.statsProcessed.bytes)) + R"()</td>
            </tr>)";

    if ((s.statsTotal.items < 0 && s.statsTotal.bytes < 0) || //no total items/bytes: e.g. for pure folder comparison
        s.statsProcessed == s.statsTotal) //...if everything was processed successfully
        ;
    else
        output += R"(
            <tr>
                <td>)" + htmlTxt(_("Items remaining:")) + R"(</td>
                <td></td>
                <td><span style="font-weight:600;">)" + htmlTxt(formatNumber(s.statsTotal.items - s.statsProcessed.items)) + "</span> (" + 
                                          htmlTxt(formatFilesizeShort(s.statsTotal.bytes - s.statsProcessed.bytes)) + R"()</td>
            </tr>)";

    const int64_t totalTimeSec = std::chrono::duration_cast<std::chrono::seconds>(s.totalTime).count();
    output += R"(
            <tr>
                <td>)" + htmlTxt(_("Total time:")) + R"(</td>
                <td><img src="https://freefilesync.org/images/log/clock.png" width="24" height="24" alt=""></td>
                <td><span style="font-weight: 600;">)" + htmlTxt(wxTimeSpan::Seconds(totalTimeSec).Format()) + R"(</span></td>
            </tr>
        </table>
    </div>
)";

    //------------ warnings/errors preview ----------------
    const int logFailTotal = logCount.warning + logCount.error;
    if (logFailTotal > 0)
    {
        output += R"(
    <div style="font-weight:600; font-size: large;">)" + htmlTxt(_("Errors and warnings:")) + R"(</div>
    <div style="border-bottom: 1px solid #AAA; margin: 5px 0;"></div>
    <table class="log-items" style="line-height:1em; border-spacing:0;">
)";
        int previewCount = 0;
        if (logPreviewFailsMax > 0)
            for (const LogEntry& entry : log)
                if (entry.type & (MSG_TYPE_WARNING | MSG_TYPE_ERROR))
                {
                    output += formatMessageHtml(entry);
                    if (++previewCount >= logPreviewFailsMax)
                        break;
                }
        output += R"(	</table>
)";
        if (logFailTotal > previewCount)
            output += R"(	<div><span style="font-weight:600; padding:0 10px;">[&hellip;]</span>)" + 
                      htmlTxt(replaceCpy(_P("Showing %y of 1 item", "Showing %y of %x items", logFailTotal), //%x used as plural form placeholder!
                      L"%y", formatNumber(previewCount))) + "</div>\n";

        output += R"(	<div style="border-bottom: 1px solid #AAA; margin: 5px 0;"></div><br>
)";
    }

        output += R"(
    <table class="log-items" style="line-height:1em; border-spacing:0;">
)";
    return output;
}


std::string generateLogFooterHtml(const std::wstring& logFilePath, int logItemsTotal, int logPreviewItemsMax) //throw FileError
{
    const std::string osImage = "os-linux.png";
    const ComputerModel cm = getComputerModel(); //throw FileError

    std::string output = R"(	</table>
)";

    if (logItemsTotal > logPreviewItemsMax)
        output += R"(	<div><span style="font-weight:600; padding:0 10px;">[&hellip;]</span>)" + 
                  htmlTxt(replaceCpy(_P("Showing %y of 1 item", "Showing %y of %x items", logItemsTotal), //%x used as plural form placeholder!
                          L"%y", formatNumber(logPreviewItemsMax))) + "</div>\n";

    return output += R"(	<br>

    <div style="border-bottom:1px solid #AAA; margin:5px 0;"></div>
    <div style="font-size:small;">
        <img src="https://freefilesync.org/images/log/)" + osImage + R"(" width="24" height="24" alt="" style="vertical-align:middle;">
        <span style="vertical-align:middle;">)" + htmlTxt(getOsDescription()) + /*throw FileError*/ + 
            " [" + htmlTxt(getUserName()) /*throw FileError*/ + ']' +
            (!cm.model .empty() ? " &ndash; " + htmlTxt(cm.model ) : "") +
            (!cm.vendor.empty() ? " &ndash; " + htmlTxt(cm.vendor) : "") + R"(</span>
    </div>
    <div style="font-size:small;">
        <img src="https://freefilesync.org/images/log/log.png" width="24" height="24" alt=")" + htmlTxt(_("Log file:")) + R"(" style="vertical-align:middle;">
        <span style="font-family: Consolas,'Courier New',Courier,monospace; vertical-align:middle;">)" + htmlTxt(logFilePath) + R"(</span>
    </div>
</body>
</html>
)";
}

//-> Astyle fucks up! => no INDENT-ON


void streamToLogFile(const ProcessSummary& summary, //throw FileError
                     const ErrorLog& log,
                     AFS::OutputStream& streamOut,
                     const AbstractPath& logFilePath,
                     LogFileFormat logFormat)
{
    const int logItemsTotal = log.end() - log.begin();
    const int logPreviewItemsMax = std::numeric_limits<int>::max();

    std::string buffer = logFormat == LogFileFormat::html ? 
                         generateLogHeaderHtml(summary, log, LOG_PREVIEW_FAIL_MAX) :
                         generateLogHeaderTxt (summary, log, LOG_PREVIEW_FAIL_MAX);

    //write log items in blocks instead of creating one big string: memory allocation might fail; think 1 million entries!
    for (const LogEntry& entry : log)
    {
        buffer += logFormat == LogFileFormat::html ?
                  formatMessageHtml(entry) :
                  formatMessage    (entry);

        streamOut.write(&buffer[0], buffer.size()); //throw FileError, X
        buffer.clear();
    }

    buffer += logFormat == LogFileFormat::html ? 
              generateLogFooterHtml(AFS::getDisplayPath(logFilePath), logItemsTotal, logPreviewItemsMax) : //throw FileError
              generateLogFooterTxt (AFS::getDisplayPath(logFilePath), logItemsTotal, logPreviewItemsMax);  //throw FileError

    //don't forget to flush:
    streamOut.write(&buffer[0], buffer.size()); //throw FileError, X
}


void saveNewLogFile(const AbstractPath& logFilePath, //throw FileError, X
                    LogFileFormat logFormat,
                    const ProcessSummary& summary,
                    const ErrorLog& log,
                    const std::function<void(std::wstring&& msg)>& notifyStatus /*throw X*/)
{
    //create logfile folder if required
    if (const std::optional<AbstractPath> parentPath = AFS::getParentPath(logFilePath))
        try
        {
            AFS::createFolderIfMissingRecursion(*parentPath); //throw FileError
        }
        catch (const FileError& e) //add context info regarding log file!
        {
            throw FileError(replaceCpy(_("Cannot write file %x."), L"%x", fmtPath(AFS::getDisplayPath(logFilePath))), e.toString());
        }
    //-----------------------------------------------------------------------

    auto notifyUnbufferedIO = [notifyStatus,
                               bytesWritten_ = int64_t(0),
                               msg_ = replaceCpy(_("Saving file %x..."), L"%x", fmtPath(AFS::getDisplayPath(logFilePath)))]
         (int64_t bytesDelta) mutable
    {
        if (notifyStatus)
            notifyStatus(msg_ + L" (" + formatFilesizeShort(bytesWritten_ += bytesDelta) + L')'); //throw X
    };

    //already existing: undefined behavior! (e.g. fail/overwrite/auto-rename)
    std::unique_ptr<AFS::OutputStream> logFileStream = AFS::getOutputStream(logFilePath, std::nullopt /*streamSize*/, std::nullopt /*modTime*/, notifyUnbufferedIO); //throw FileError
    streamToLogFile(summary, log, *logFileStream, logFilePath, logFormat); //throw FileError, X
    logFileStream->finalize();                     //throw FileError, X
}


const int TIME_STAMP_LENGTH = 21;
const Zchar STATUS_BEGIN_TOKEN[] = Zstr(" [");
const Zchar STATUS_END_TOKEN     = Zstr(']');


struct LogFileInfo
{
    AbstractPath filePath;
    time_t       timeStamp;
    std::wstring jobNames; //may be empty
};
std::vector<LogFileInfo> getLogFiles(const AbstractPath& logFolderPath) //throw FileError
{
    std::vector<LogFileInfo> logfiles;

    AFS::traverseFolderFlat(logFolderPath, [&](const AFS::FileInfo& fi) //throw FileError
    {
        //"Backup FreeFileSync 2013-09-15 015052.123.html"
        //"Jobname1 + Jobname2 2013-09-15 015052.123.log"
        //"2013-09-15 015052.123 [Error].log"
        static_assert(TIME_STAMP_LENGTH == 21);

        if (endsWith(fi.itemName, Zstr(".log")) || //case-sensitive: e.g. ".LOG" is not from FFS, right?
            endsWith(fi.itemName, Zstr(".html")))
        {
            auto tsBegin = fi.itemName.begin();
            auto tsEnd   = tsBegin + fi.itemName.rfind('.');

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
                const TimeComp tc = parseTime(Zstr("%Y-%m-%d %H%M%S"), makeStringView(tsBegin, 17)); //returns TimeComp() on error
                const time_t t = localToTimeT(tc); //returns -1 on error
                if (t != -1)
                {
                    Zstring jobNames(fi.itemName.begin(), tsBegin);
                    if (!jobNames.empty())
                    {
                        assert(jobNames.size() >= 2 && endsWith(jobNames, Zstr(' ')));
                        jobNames.pop_back();
                    }

                    logfiles.push_back({AFS::appendRelPath(logFolderPath, fi.itemName), t, utfTo<std::wstring>(jobNames)});
                }
            }
        }
    },
    nullptr /*onFolder*/, //traverse only one level deep
    nullptr /*onSymlink*/);

    return logfiles;
}


void limitLogfileCount(const AbstractPath& logFolderPath, //throw FileError, X
                       int logfilesMaxAgeDays, //<= 0 := no limit
                       const std::set<AbstractPath>& logFilePathsToKeep,
                       const std::function<void(std::wstring&& msg)>& notifyStatus /*throw X*/)
{
    if (logfilesMaxAgeDays > 0)
    {
        const std::wstring statusPrefix = _("Cleaning up log files:") + L" [" + _P("1 day", "%x days", logfilesMaxAgeDays) + L"] ";

        if (notifyStatus) notifyStatus(statusPrefix + fmtPath(AFS::getDisplayPath(logFolderPath))); //throw X

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
                !logFilePathsToKeep.contains(lfi.filePath)) //don't trim latest log files corresponding to last used config files!
                //nitpicker's corner: what about path differences due to case? e.g. user-overriden log file path changed in case
            {
                if (notifyStatus) notifyStatus(statusPrefix + fmtPath(AFS::getDisplayPath(lfi.filePath))); //throw X
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


Zstring fff::getLogFolderDefaultPath() { return getConfigDirPathPf() + Zstr("Logs") ; }


//"Backup FreeFileSync 2013-09-15 015052.123.html"
//"Backup FreeFileSync 2013-09-15 015052.123 [Error].html"
//"Backup FreeFileSync + RealTimeSync 2013-09-15 015052.123 [Error].log"
AbstractPath fff::generateLogFilePath(LogFileFormat logFormat, const ProcessSummary& summary, const Zstring& altLogFolderPathPhrase /*optional*/)
{
    //const std::string colon = "\xcb\xb8"; //="modifier letter raised colon" => regular colon is forbidden in file names on Windows and OS X
    //=> too many issues, most notably cmd.exe is not Unicode-aware: https://freefilesync.org/forum/viewtopic.php?t=1679

    Zstring jobNamesFmt;
    if (!summary.jobNames.empty())
    {
        for (const std::wstring& jobName : summary.jobNames)
            if (const Zstring jobNameZ = utfTo<Zstring>(jobName);
                jobNamesFmt.size() + jobNameZ.size() > 200)
            {
                jobNamesFmt += Zstr("[...] + "); //avoid hitting file system name length limitations: "lpMaximumComponentLength is commonly 255 characters"
                break;                           //https://freefilesync.org/forum/viewtopic.php?t=7113
            }
            else
                jobNamesFmt += jobNameZ + Zstr(" + ");

        jobNamesFmt.resize(jobNamesFmt.size() - 3);
    }

    const TimeComp tc = getLocalTime(std::chrono::system_clock::to_time_t(summary.startTime));
    if (tc == TimeComp())
        throw FileError(L"Failed to determine current time: " + numberTo<std::wstring>(summary.startTime.time_since_epoch().count()));

    const auto timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(summary.startTime.time_since_epoch()).count() % 1000;
    assert(std::chrono::duration_cast<std::chrono::seconds>(summary.startTime.time_since_epoch()).count() == std::chrono::system_clock::to_time_t(summary.startTime));

    const std::wstring failStatus = [&]
    {
        switch (summary.syncResult)
        {
            case SyncResult::finishedSuccess: break;
            case SyncResult::finishedWarning: return _("Warning");
            case SyncResult::finishedError:   return _("Error");
            case SyncResult::aborted:         return _("Stopped");
        }
        return std::wstring();
    }();
    //------------------------------------------------------------------

    Zstring logFileName = jobNamesFmt;
    if (!logFileName.empty())
        logFileName += Zstr(' ');

    logFileName += formatTime(Zstr("%Y-%m-%d %H%M%S"), tc) +
                   Zstr(".") + printNumber<Zstring>(Zstr("%03d"), static_cast<int>(timeMs)); //[ms] should yield a fairly unique name
    static_assert(TIME_STAMP_LENGTH == 21);

    if (!failStatus.empty())
        logFileName += STATUS_BEGIN_TOKEN + utfTo<Zstring>(failStatus) + STATUS_END_TOKEN;

    if (logFormat == LogFileFormat::html)
        logFileName += Zstr(".html");
    else
        logFileName += Zstr(".log");

    AbstractPath logFolderPath = createAbstractPath(altLogFolderPathPhrase);
    if (AFS::isNullPath(logFolderPath))
        logFolderPath = createAbstractPath(getLogFolderDefaultPath());

    return AFS::appendRelPath(logFolderPath, logFileName);
}


void fff::saveLogFile(const AbstractPath& logFilePath, //throw FileError, X
                      const ProcessSummary& summary,
                      const ErrorLog& log,
                      int logfilesMaxAgeDays,
                      LogFileFormat logFormat,
                      const std::set<AbstractPath>& logFilePathsToKeep,
                      const std::function<void(std::wstring&& msg)>& notifyStatus /*throw X*/)
{
    std::exception_ptr firstError;
    try
    {
        saveNewLogFile(logFilePath, logFormat, summary, log, notifyStatus); //throw FileError, X
    }
    catch (const FileError&) { if (!firstError) firstError = std::current_exception(); };

    try
    {
        const std::optional<AbstractPath> logFolderPath = AFS::getParentPath(logFilePath);
        assert(logFolderPath);
        if (logFolderPath) //else: logFilePath == device root; not possible with generateLogFilePath()
            limitLogfileCount(*logFolderPath, logfilesMaxAgeDays, logFilePathsToKeep, notifyStatus); //throw FileError, X
    }
    catch (const FileError&) { if (!firstError) firstError = std::current_exception(); };

    if (firstError) //late failure!
        std::rethrow_exception(firstError);
}




void fff::sendLogAsEmail(const std::string& email, //throw FileError, X
                         const ProcessSummary& summary,
                         const ErrorLog& log,
                         const AbstractPath& logFilePath,
                         const std::function<void(std::wstring&& msg)>& notifyStatus /*throw X*/)
{
    try
    {
        throw SysError(_("Requires FreeFileSync Donation Edition"));
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot send notification email to %x."), L"%x", L'"' + utfTo<std::wstring>(email) + L'"'), e.toString()); }
}

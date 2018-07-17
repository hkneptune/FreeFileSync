// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "generate_logfile.h"
#include <zen/file_io.h>
#include <wx/datetime.h>

using namespace zen;
using namespace fff;


namespace
{
std::wstring generateLogHeader(const LogSummary& s)
{
    assert(s.itemsProcessed <= s.itemsTotal);
    assert(s.bytesProcessed <= s.bytesTotal);

    std::wstring output;

    //write header
    std::wstring headerLine = formatTime<std::wstring>(FORMAT_DATE);
    if (!s.jobName.empty())
        headerLine += L" | " + s.jobName;
    headerLine += L" | " + s.finalStatus;

    //assemble results box
    std::vector<std::wstring> results;
    results.push_back(headerLine);
    results.push_back(L"");

    const wchar_t tabSpace[] = L"    ";

    std::wstring itemsProc = tabSpace + _("Items processed:") + L" " + formatNumber(s.itemsProcessed); //show always, even if 0!
    if (s.itemsProcessed != 0 || s.bytesProcessed != 0) //[!] don't show 0 bytes processed if 0 items were processed
        itemsProc += + L" (" + formatFilesizeShort(s.bytesProcessed) + L")";
    results.push_back(itemsProc);

    if (s.itemsTotal != 0 || s.bytesTotal != 0) //=: sync phase was reached and there were actual items to sync
    {
        if (s.itemsProcessed != s.itemsTotal ||
            s.bytesProcessed  != s.bytesTotal)
            results.push_back(tabSpace + _("Items remaining:") + L" " + formatNumber(s.itemsTotal - s.itemsProcessed) + L" (" + formatFilesizeShort(s.bytesTotal - s.bytesProcessed) + L")");
    }

    results.push_back(tabSpace + _("Total time:") + L" " + copyStringTo<std::wstring>(wxTimeSpan::Seconds(s.totalTime).Format()));

    //calculate max width, this considers UTF-16 only, not true Unicode...but maybe good idea? those 2-char-UTF16 codes are usually wider than fixed width chars anyway!
    size_t sepLineLen = 0;
    for (const std::wstring& str : results) sepLineLen = std::max(sepLineLen, str.size());

    output.resize(output.size() + sepLineLen + 1, L'_');
    output += L'\n';

    for (const std::wstring& str : results) { output += L'|'; output += str; output += L'\n'; }

    output += L'|';
    output.resize(output.size() + sepLineLen, L'_');
    output += L'\n';

    return output;
}
}


void fff::streamToLogFile(const LogSummary& summary, //throw FileError
                          const zen::ErrorLog& log,
                          AFS::OutputStream& streamOut)
{
    const std::string header = replaceCpy(utfTo<std::string>(generateLogHeader(summary)), '\n', LINE_BREAK); //don't replace line break any earlier

    streamOut.write(&header[0], header.size()); //throw FileError, X

    //write log items in blocks instead of creating one big string: memory allocation might fail; think 1 million entries!
    std::string buffer;
    buffer += LINE_BREAK;

    for (const LogEntry& entry : log)
    {
        buffer += replaceCpy(utfTo<std::string>(formatMessage<std::wstring>(entry)), '\n', LINE_BREAK);
        buffer += LINE_BREAK;

        streamOut.write(&buffer[0], buffer.size()); //throw FileError, X
        buffer.clear();
    }
}


void fff::saveToLastSyncsLog(const LogSummary& summary, //throw FileError
                             const zen::ErrorLog& log,
                             size_t maxBytesToWrite, //log may be *huge*, e.g. 1 million items; LastSyncs.log *must not* create performance problems!
                             const std::function<void(const std::wstring& msg)>& notifyStatus)
{
    const Zstring filePath = getConfigDirPathPf() + Zstr("LastSyncs.log");

    Utf8String newStream = utfTo<Utf8String>(generateLogHeader(summary));
    replace(newStream, '\n', LINE_BREAK); //don't replace line break any earlier
    newStream += LINE_BREAK;

    //check size of "newStream": memory allocation might fail - think 1 million entries!
    for (const LogEntry& entry : log)
    {
        newStream += replaceCpy(utfTo<Utf8String>(formatMessage<std::wstring>(entry)), '\n', LINE_BREAK);
        newStream += LINE_BREAK;

        if (newStream.size() > maxBytesToWrite)
        {
            newStream += "[...]";
            newStream += LINE_BREAK;
            break;
        }
    }

    auto notifyUnbufferedIOLoad = [notifyStatus,
                                   bytesRead_ = int64_t(0),
                                   msg_ = replaceCpy(_("Loading file %x..."), L"%x", fmtPath(filePath))]
         (int64_t bytesDelta) mutable
    {
        if (notifyStatus)
            notifyStatus(msg_ + L" (" + formatFilesizeShort(bytesRead_ += bytesDelta) + L")"); /*throw X*/
    };

    auto notifyUnbufferedIOSave = [notifyStatus,
                                   bytesWritten_ = int64_t(0),
                                   msg_ = replaceCpy(_("Saving file %x..."), L"%x", fmtPath(filePath))]
         (int64_t bytesDelta) mutable
    {
        if (notifyStatus)
            notifyStatus(msg_ + L" (" + formatFilesizeShort(bytesWritten_ += bytesDelta) + L")"); /*throw X*/
    };

    //fill up the rest of permitted space by appending old log
    if (newStream.size() < maxBytesToWrite)
    {
        Utf8String oldStream;
        try
        {
            oldStream = loadBinContainer<Utf8String>(filePath, notifyUnbufferedIOLoad); //throw FileError, X
            //Note: we also report the loaded bytes via onUpdateSaveStatus()!
        }
        catch (FileError&) {}

        if (!oldStream.empty())
        {
            newStream += LINE_BREAK;
            newStream += LINE_BREAK;
            newStream += oldStream; //implicitly limited by "maxBytesToWrite"!

            //truncate size if required
            if (newStream.size() > maxBytesToWrite)
            {
                //but do not cut in the middle of a row
                auto it = std::search(newStream.cbegin() + maxBytesToWrite, newStream.cend(), std::begin(LINE_BREAK), std::end(LINE_BREAK) - 1);
                if (it != newStream.cend())
                {
                    newStream.resize(it - newStream.cbegin());
                    newStream += LINE_BREAK;

                    newStream += "[...]";
                    newStream += LINE_BREAK;
                }
            }
        }
    }

    saveBinContainer(filePath, newStream, notifyUnbufferedIOSave); //throw FileError, X
}


void fff::limitLogfileCount(const AbstractPath& logFolderPath, const std::wstring& jobname, size_t maxCount, //throw FileError
                            const std::function<void(const std::wstring& msg)>& notifyStatus)
{
    const std::wstring cleaningMsg = _("Cleaning up log files:");
    const Zstring prefix = utfTo<Zstring>(jobname);

    //traverse source directory one level deep
    if (notifyStatus) notifyStatus(cleaningMsg + L" " + fmtPath(AFS::getDisplayPath(logFolderPath)));

    std::vector<Zstring> logFileNames;

    AFS::traverseFolderFlat(logFolderPath, [&](const AFS::FileInfo& fi) //throw FileError
    {
        if (startsWith(fi.itemName, prefix, CmpFilePath() /*even on Linux!*/) && endsWith(fi.itemName, Zstr(".log"), CmpFilePath()))
            logFileNames.push_back(fi.itemName);
    },
    nullptr /*onFolder*/,
    nullptr /*onSymlink*/);

    Opt<FileError> lastError;

    if (logFileNames.size() > maxCount)
    {
        //delete oldest logfiles: take advantage of logfile naming convention to find them
        std::nth_element(logFileNames.begin(), logFileNames.end() - maxCount, logFileNames.end(), LessFilePath());

        std::for_each(logFileNames.begin(), logFileNames.end() - maxCount, [&](const Zstring& logFileName)
        {
            const AbstractPath filePath = AFS::appendRelPath(logFolderPath, logFileName);
            if (notifyStatus) notifyStatus(cleaningMsg + L" " + fmtPath(AFS::getDisplayPath(filePath)));

            try
            {
                AFS::removeFilePlain(filePath); //throw FileError
            }
            catch (const FileError& e) { if (!lastError) lastError = e; };
        });
    }

    if (lastError) //late failure!
        throw* lastError;
}

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ERROR_LOG_H_8917590832147915
#define ERROR_LOG_H_8917590832147915

#include <cassert>
#include <vector>
#include "time.h"
#include "i18n.h"
#include "zstring.h"


namespace zen
{
enum MessageType
{
    MSG_TYPE_INFO    = 0x1,
    MSG_TYPE_WARNING = 0x2,
    MSG_TYPE_ERROR   = 0x4,
};

struct LogEntry
{
    time_t      time = 0;
    MessageType type = MSG_TYPE_ERROR;
    Zstringc message; //conserve memory (=> avoid std::string SSO overhead!)
};

std::string formatMessage(const LogEntry& entry);

using ErrorLog = std::vector<LogEntry>;

void logMsg(ErrorLog& log, const std::wstring& msg, MessageType type, time_t time = std::time(nullptr));

struct ErrorLogStats
{
    int info    = 0;
    int warning = 0;
    int error   = 0;
};
ErrorLogStats getStats(const ErrorLog& log);







//######################## implementation ##########################
inline
void logMsg(ErrorLog& log, const std::wstring& msg, MessageType type, time_t time)
{
    log.push_back({time, type, utfTo<Zstringc>(msg)});
}


inline
ErrorLogStats getStats(const ErrorLog& log)
{
    ErrorLogStats count;
    for (const LogEntry& entry : log)
        switch (entry.type)
        {
            case MSG_TYPE_INFO:
                ++count.info;
                break;
            case MSG_TYPE_WARNING:
                ++count.warning;
                break;
            case MSG_TYPE_ERROR:
                ++count.error;
                break;
        }
    assert(std::ssize(log) == count.info + count.warning + count.error);
    return count;
}


inline
std::wstring getMessageTypeLabel(MessageType type)
{
    switch (type)
    {
        case MSG_TYPE_INFO:
            return _("Info");
        case MSG_TYPE_WARNING:
            return _("Warning");
        case MSG_TYPE_ERROR:
            return _("Error");
    }
    assert(false);
    return std::wstring();
}


inline
std::string formatMessage(const LogEntry& entry)
{
    std::string msgFmt = '[' + utfTo<std::string>(formatTime(formatTimeTag, getLocalTime(entry.time))) + "]  " + utfTo<std::string>(getMessageTypeLabel(entry.type)) + ":  ";
    const size_t prefixLen = unicodeLength(msgFmt); //consider Unicode!

    const Zstringc msg = trimCpy(entry.message);
    static_assert(std::is_same_v<decltype(msg), const Zstringc>, "no worries about copying as long as we're using a ref-counted string!");
    assert(msg == entry.message); //trimming shouldn't be needed usually!?

    for (auto it = msg.begin(); it != msg.end(); )
        if (*it == '\n')
        {
            msgFmt += *it++;
            msgFmt.append(prefixLen, ' ');
            //skip duplicate newlines
            for (; it != msg.end() && *it == '\n'; ++it)
                ;
        }
        else
            msgFmt += *it++;

    msgFmt += '\n';
    return msgFmt;
}
}

#endif //ERROR_LOG_H_8917590832147915

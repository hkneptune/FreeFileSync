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
#include "utf.h"
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


class ErrorLog
{
public:
    void logMsg(const std::wstring& msg, MessageType type);

    struct Stats
    {
        int info    = 0;
        int warning = 0;
        int error   = 0;
    };
    Stats getStats() const;

    //subset of std::vector<> interface:
    using const_iterator = std::vector<LogEntry>::const_iterator;
    const_iterator begin() const { return entries_.begin(); }
    const_iterator end  () const { return entries_.end  (); }
    bool           empty() const { return entries_.empty(); }

private:
    std::vector<LogEntry> entries_;
};








//######################## implementation ##########################
inline
void ErrorLog::logMsg(const std::wstring& msg, MessageType type)
{
    entries_.push_back({ std::time(nullptr), type, utfTo<Zstringc>(msg) });
}



inline
ErrorLog::Stats ErrorLog::getStats() const
{
    Stats count;
    for (const LogEntry& entry : entries_)
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
    assert(static_cast<int>(entries_.size()) == count.info + count.warning + count.error);
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
    static_assert(std::is_same_v<decltype(msg), const Zstringc>, "don't worry about copying as long as we're using a ref-counted string!");

    for (auto it = msg.begin(); it != msg.end(); )
        if (*it == '\n')
        {
            msgFmt += '\n';
            msgFmt.append(prefixLen, ' ');
            ++it;
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

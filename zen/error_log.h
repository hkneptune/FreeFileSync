// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef ERROR_LOG_H_8917590832147915
#define ERROR_LOG_H_8917590832147915

#include <cassert>
#include <algorithm>
#include <vector>
//#include <string>
#include "time.h"
#include "i18n.h"
#include "utf.h"
#include "zstring.h"


namespace zen
{
enum MessageType
{
    MSG_TYPE_INFO        = 0x1,
    MSG_TYPE_WARNING     = 0x2,
    MSG_TYPE_ERROR       = 0x4,
    MSG_TYPE_FATAL_ERROR = 0x8,
};

struct LogEntry
{
    time_t      time = 0;
    MessageType type = MSG_TYPE_FATAL_ERROR;
    Zstringw    message; //std::wstring may employ small string optimization: we cannot accept bloating the "ErrorLog::entries_" memory block below (think 1 million items)
};

std::wstring formatMessage(const LogEntry& entry);


class ErrorLog
{
public:
    void logMsg(const std::wstring& msg, MessageType type);

    int getItemCount(int typeFilter = MSG_TYPE_INFO | MSG_TYPE_WARNING | MSG_TYPE_ERROR | MSG_TYPE_FATAL_ERROR) const;

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
    entries_.push_back({ std::time(nullptr), type, copyStringTo<Zstringw>(msg) });
}


inline
int ErrorLog::getItemCount(int typeFilter) const
{
    return static_cast<int>(std::count_if(entries_.begin(), entries_.end(), [typeFilter](const LogEntry& e) { return e.type & typeFilter; }));
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
            case MSG_TYPE_FATAL_ERROR:
                return _("Serious Error");
        }
        assert(false);
        return std::wstring();
}


inline
std::wstring formatMessage(const LogEntry& entry)
{
    std::wstring msgFmt = L"[" + formatTime<std::wstring>(FORMAT_TIME, getLocalTime(entry.time)) + L"]  " + getMessageTypeLabel(entry.type) + L":  ";
    const size_t prefixLen = unicodeLength(msgFmt); //consider Unicode!

    const Zstringw msg = trimCpy(entry.message);
    static_assert(std::is_same_v<decltype(msg), const Zstringw>, "don't worry about copying as long as we're using a ref-counted string!");

    for (auto it = msg.begin(); it != msg.end(); )
        if (*it == L'\n')
        {
            msgFmt += L'\n';
            msgFmt.append(prefixLen, L' ');
            ++it;
            //skip duplicate newlines
            for (;it != msg.end() && *it == L'\n'; ++it)
                ;
        }
        else
            msgFmt += *it++;

    return msgFmt += L'\n';
}
}

#endif //ERROR_LOG_H_8917590832147915

// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef EXTRA_LOG_H_601673246392441846218957402563
#define EXTRA_LOG_H_601673246392441846218957402563

#include "error_log.h"
#include "thread.h"

/*  log errors in "exceptional situations" when no other means are available, e.g.
    - while an exception is in flight
    - cleanup errors
    - nothrow GUI functions                                */

namespace zen
{
namespace impl
{
class ExtraLog
{
public:
    ~ExtraLog()
    {
        assert(reportOutstandingLog_);
        if (!log_.empty() && reportOutstandingLog_)
            reportOutstandingLog_(log_);
    }

    void init(const std::function<void(const ErrorLog& log)>& reportOutstandingLog)
    {
        assert(!reportOutstandingLog_);
        reportOutstandingLog_ = reportOutstandingLog;
    }

    ErrorLog fetchLog() { return std::exchange(log_, ErrorLog()); }

    void logError(const std::wstring& msg) { logMsg(log_, msg, MessageType::MSG_TYPE_ERROR); } //nothrow!

private:
    ErrorLog log_;
    std::function<void(const ErrorLog& log)> reportOutstandingLog_;
};

inline constinit Global<Protected<ExtraLog>> globalExtraLog;

template <class Function>
auto accessExtraLog(Function fun)
{
    globalExtraLog.setOnce([] { return std::make_unique<Protected<ExtraLog>>(); });

    if (auto protExtraLog = impl::globalExtraLog.get())
        protExtraLog->access([&](ExtraLog& log) { fun(log); });
    else
        assert(false); //access after global shutdown!? => SOL!
}
}

inline
void initExtraLog(const std::function<void(const ErrorLog& log)>& reportOutstandingLog /*nothrow! runs during global shutdown!*/)
{
    impl::accessExtraLog([&](impl::ExtraLog& el) { el.init(reportOutstandingLog); });
}


inline
ErrorLog fetchExtraLog()
{
    ErrorLog output;
    impl::accessExtraLog([&](impl::ExtraLog& el) { output = el.fetchLog(); });
    return output;
}


inline
void logExtraError(const std::wstring& msg) //nothrow!
{
    impl::accessExtraLog([&](impl::ExtraLog& el) { el.logError(msg); });
}
}

#endif //EXTRA_LOG_H_601673246392441846218957402563

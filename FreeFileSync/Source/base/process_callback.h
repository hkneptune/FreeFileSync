// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PROCESS_CALLBACK_H_48257827842345454545
#define PROCESS_CALLBACK_H_48257827842345454545

#include <string>
#include <cstdint>
#include <chrono>


namespace fff
{
struct PhaseCallback
{
    virtual ~PhaseCallback() {}

    //note: this one must NOT throw in order to properly allow undoing setting of statistics!
    //it is in general paired with a call to requestUiUpdate() to compensate!
    virtual void updateDataProcessed(int itemsDelta, int64_t bytesDelta) = 0; //noexcept!
    virtual void updateDataTotal    (int itemsDelta, int64_t bytesDelta) = 0; //
    /* the estimated and actual total workload may change *during* sync:
            1. file cannot be moved -> fallback to copy + delete
            2. file copy, actual size changed after comparison
            3. file contains significant ADS data, is sparse or compressed
            4. file/directory already deleted externally: nothing to do, 0 logical operations and data
            5. auto-resolution for failed create operations due to missing source
            6. directory deletion: may contain more items than scanned by FFS (excluded by filter) or less (contains followed symlinks)
            7. delete directory to recycler: no matter how many child-elements exist, this is only 1 item to process!
            8. user-defined deletion directory on different volume: full file copy required (instead of move)
            9. Binary file comparison: short-circuit behavior after first difference is found
           10. Error during file copy, retry: bytes were copied => increases total workload!                        */

    //opportunity to abort must be implemented in a frequently-executed method like requestUiUpdate()
    virtual void requestUiUpdate(bool force = false) = 0; //throw X

    //UI info only, should *not* be logged: called periodically after data was processed: expected(!) to request GUI update
    virtual void updateStatus(std::wstring&& msg) = 0; //throw X

    enum class MsgType
    {
        info,
        warning,
        error,
    };
    //log only; must *not* call updateStatus()!
    virtual void logMessage(const std::wstring& msg, MsgType type) = 0; //throw X

    virtual void reportWarning(const std::wstring& msg, bool& warningActive) = 0; //throw X

    struct ErrorInfo
    {
        std::wstring msg;
        std::chrono::steady_clock::time_point failTime;
        size_t retryNumber = 0;
    };
    enum Response
    {
        ignore,
        retry
    };
    virtual Response reportError(const ErrorInfo& errorInfo) = 0; //throw X; recoverable error

    virtual void reportFatalError(const std::wstring& msg)   = 0; //throw X; non-recoverable error
};


//interface for comparison and synchronization process status updates (used by GUI or Batch mode)
constexpr std::chrono::milliseconds UI_UPDATE_INTERVAL(100); //perform ui updates not more often than necessary,
//100 ms seems to be a good value with only a minimal performance loss; also used by Win 7 copy progress bar
//this one is required by async directory existence check!

enum class ProcessPhase
{
    none, //initial status
    scan,
    binaryCompare,
    sync
};

//report status during comparison and synchronization
struct ProcessCallback : public PhaseCallback
{
    //informs about the estimated amount of data that will be processed in the next synchronization phase
    virtual void initNewPhase(int itemsTotal, int64_t bytesTotal, ProcessPhase phaseId) = 0; //throw X
};
}

#endif //PROCESS_CALLBACK_H_48257827842345454545

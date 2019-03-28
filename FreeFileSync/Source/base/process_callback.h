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
//interface for comparison and synchronization process status updates (used by GUI or Batch mode)
const std::chrono::milliseconds UI_UPDATE_INTERVAL(100); //perform ui updates not more often than necessary,
//100 ms seems to be a good value with only a minimal performance loss; also used by Win 7 copy progress bar
//this one is required by async directory existence check!

//report status during comparison and synchronization
struct ProcessCallback
{
    virtual ~ProcessCallback() {}

    //these methods have to be implemented in the derived classes to handle error and status information

    //notify synchronization phases
    enum Phase
    {
        PHASE_NONE, //initial status
        PHASE_SCANNING,
        PHASE_COMPARING_CONTENT,
        PHASE_SYNCHRONIZING
    };
    virtual void initNewPhase(int itemsTotal, int64_t bytesTotal, Phase phaseId) = 0; //throw X; informs about the estimated amount of data that will be processed in this phase

    //note: this one must NOT throw in order to properly allow undoing setting of statistics!
    //it is in general paired with a call to requestUiRefresh() to compensate!
    virtual void updateDataProcessed(int itemsDelta, int64_t bytesDelta) = 0; //noexcept!!
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
           10. Error during file copy, retry: bytes were copied => increases total workload!
    */

    //opportunity to abort must be implemented in a frequently-executed method like requestUiRefresh()
    virtual void requestUiRefresh() = 0; //throw X
    virtual void forceUiRefresh  () = 0; //throw X - called before starting long running tasks which don't update regularly

    //UI info only, should not be logged: called periodically after data was processed: expected(!) to request GUI update
    virtual void reportStatus(const std::wstring& text) = 0; //throw X

    //logging only, no status update!
    virtual void logInfo(const std::wstring& msg) = 0;

    //called periodically after data was processed
    void reportInfo(const std::wstring& msg) //throw X
    {
        logInfo(msg);
        reportStatus(msg); //throw X
    }

    virtual void reportWarning(const std::wstring& msg, bool& warningActive) = 0; //throw X

    //error handling:
    enum Response
    {
        IGNORE_ERROR,
        RETRY
    };
    virtual Response reportError     (const std::wstring& msg, size_t retryNumber) = 0; //throw X;     recoverable error situation
    virtual void     reportFatalError(const std::wstring& msg)                     = 0; //throw X; non-recoverable error situation

    virtual void abortProcessNow() = 0; //will throw an exception => don't call while in a C GUI callstack
};
}

#endif //PROCESS_CALLBACK_H_48257827842345454545

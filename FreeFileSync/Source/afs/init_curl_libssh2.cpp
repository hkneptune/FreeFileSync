// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "init_curl_libssh2.h"
#include <cassert>
#include <zen/thread.h>
#include <zen/file_error.h>
#include <libcurl/curl_wrap.h> //DON'T include <curl/curl.h> directly!
#include <zen/open_ssl.h>
#include "libssh2/libssh2_wrap.h" //DON'T include <libssh2_sftp.h> directly!


using namespace zen;


namespace
{
int uniInitLevel = 0; //support interleaving initialization calls! (e.g. use for libssh2 and libCurl)
//zero-initialized POD => not subject to static initialization order fiasco

void libsshCurlUnifiedInit()
{
    assert(runningMainThread()); //all OpenSSL/libssh2/libcurl require init on main thread!
    assert(uniInitLevel >= 0);
    if (++uniInitLevel != 1) //non-atomic => also require call from main thread
        return;


    openSslInit();

    [[maybe_unused]] const int rc2 = ::libssh2_init(0);
    /*
        we need libssh2's crypto init:
        - there is other OpenSSL-related initialization which might be needed (and hopefully won't hurt...)

        2019-02-26: following reasons are obsolete due to HAVE_EVP_AES_128_CTR:
        // - initializes a few statically allocated constants => avoid (minor) race condition if these were initialized by worker threads
        // - enable proper clean up of these variables in libssh2_exit() (otherwise: memory leaks!)
    */
    assert(rc2 == 0); //libssh2 unconditionally returns 0 => why then have a return value in first place???


    [[maybe_unused]] const CURLcode rc3 = ::curl_global_init(CURL_GLOBAL_NOTHING /*CURL_GLOBAL_DEFAULT = CURL_GLOBAL_SSL|CURL_GLOBAL_WIN32*/);
    assert(rc3 == CURLE_OK);
}


void libsshCurlUnifiedTearDown()
{
    assert(runningMainThread()); //symmetry with libsshCurlUnifiedInit
    assert(uniInitLevel >= 1);
    if (--uniInitLevel != 0)
        return;

    ::curl_global_cleanup();
    ::libssh2_exit();
    openSslTearDown();

}
}


class zen::UniSessionCounter::Impl
{
public:
    void inc() //throw SysError
    {
        {
            std::unique_lock dummy(lockCount_);
            assert(sessionCount_ >= 0);

            if (!newSessionsAllowed_)
                throw SysError(L"UniSessionCounter::inc() function call not allowed during init/shutdown.");

            ++sessionCount_;
        }
        conditionCountChanged_.notify_all();
    }

    void dec() //noexcept
    {
        {
            std::unique_lock dummy(lockCount_);
            assert(sessionCount_ >= 1);
            --sessionCount_;
        }
        conditionCountChanged_.notify_all();
    }

    void onInitCompleted() //noexcept
    {
        std::unique_lock dummy(lockCount_);
        newSessionsAllowed_ = true;
    }

    void onBeforeTearDown() //noexcept
    {
        std::unique_lock dummy(lockCount_);
        newSessionsAllowed_ = false;
        conditionCountChanged_.wait(dummy, [this] { return sessionCount_ == 0; });
    }

    Impl() {}
    ~Impl()
    {
    }

private:
    Impl           (const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    std::mutex              lockCount_;
    int                     sessionCount_ = 0;
    std::condition_variable conditionCountChanged_;

    bool newSessionsAllowed_ = false;
};


UniSessionCounter::UniSessionCounter() : pimpl(std::make_unique<Impl>()) {}
UniSessionCounter::~UniSessionCounter() {}


std::unique_ptr<UniSessionCounter> zen::createUniSessionCounter()
{
    return std::make_unique<UniSessionCounter>();
}


class zen::UniCounterCookie
{
public:
    UniCounterCookie(const std::shared_ptr<UniSessionCounter>& sessionCounter) :  sessionCounter_(sessionCounter) {}
    ~UniCounterCookie() { sessionCounter_->pimpl->dec(); }

private:
    UniCounterCookie           (const UniCounterCookie&) = delete;
    UniCounterCookie& operator=(const UniCounterCookie&) = delete;

    const std::shared_ptr<UniSessionCounter> sessionCounter_;
};


std::shared_ptr<UniCounterCookie> zen::getLibsshCurlUnifiedInitCookie(Global<UniSessionCounter>& globalSftpSessionCount) //throw SysError
{
    std::shared_ptr<UniSessionCounter> sessionCounter = globalSftpSessionCount.get();
    if (!sessionCounter)
        throw SysError(L"getLibsshCurlUnifiedInitCookie() function call not allowed during init/shutdown."); //=> ~UniCounterCookie() *not* called!
    sessionCounter->pimpl->inc(); //throw SysError                                                           //

    //pass "ownership" of having to call UniSessionCounter::dec()
    return std::make_shared<UniCounterCookie>(sessionCounter); //throw SysError
}


UniInitializer::UniInitializer(UniSessionCounter& sessionCount) : sessionCount_(sessionCount)
{
    libsshCurlUnifiedInit();
    sessionCount_.pimpl->onInitCompleted();
}


UniInitializer::~UniInitializer()
{
    //wait until all (S)FTP sessions running on detached threads have ended! otherwise they'll crash during ::WSACleanup()!
    sessionCount_.pimpl->onBeforeTearDown();
    /*
      alternatively we could use a Global<UniInitializer> and have each session own a shared_ptr<UniInitializer>:
      drawback 1: SFTP clean-up may happen on worker thread => probably not supported!!!
      drawback 2: cleanup will not happen when the C++ runtime on Windows kills all worker threads during shutdown
    */
    libsshCurlUnifiedTearDown();
}

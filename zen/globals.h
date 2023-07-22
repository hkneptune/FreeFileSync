// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef GLOBALS_H_8013740213748021573485
#define GLOBALS_H_8013740213748021573485

#include <atomic>
#include <memory>
#include <utility>
#include "scope_guard.h"


namespace zen
{
/*  Solve static destruction order fiasco by providing shared ownership and serialized access to global variables

    => e.g. accesses to "Global<T>::get()" during process shutdown: _("") used by message in debug_minidump.cpp or by some detached thread assembling an error message!
    => use trivially-destructible POD only!!!

    ATTENTION: function-static globals have the compiler generate "magic statics" == compiler-genenerated locking code which will crash or leak memory when accessed after global is "dead"
               => "solved" by FunStatGlobal, but we can't have "too many" of these...                  */
class PodSpinMutex
{
public:
    bool tryLock();
    void lock();
    void unlock();
    bool isLocked();

private:
    std::atomic_flag flag_{}; /* => avoid potential contention with worker thread during Global<> construction!
    - "For an atomic_flag with static storage duration, this guarantees static initialization:" => just what the doctor ordered!
    - "[default initialization] initializes std::atomic_flag to clear state" - since C++20      =>
    - "std::atomic_flag is [...] guaranteed to be lock-free"
    - interestingly, is_trivially_constructible_v<> is false, thanks to constexpr! https://developercommunity.visualstudio.com/content/problem/416343/stdatomic-no-longer-is-trivially-constructible.html    */
};


#define GLOBAL_RUN_ONCE(X)                               \
    struct ZEN_CONCAT(GlobalInitializer, __LINE__)       \
    {                                                    \
        ZEN_CONCAT(GlobalInitializer, __LINE__)() { X; } \
    } ZEN_CONCAT(globalInitializer, __LINE__)


template <class T>
class Global //don't use for function-scope statics!
{
public:
    consteval Global() {}; //demand static zero-initialization!

    ~Global()
    {
        static_assert(std::is_trivially_destructible_v<Pod>, "this memory needs to live forever");

        pod_.spinLock.lock();
        std::shared_ptr<T>* oldInst = std::exchange(pod_.inst, nullptr);
        pod_.destroyed = true;
        pod_.spinLock.unlock();

        delete oldInst;
    }

    std::shared_ptr<T> get() //=> return std::shared_ptr to let instance life time be handled by caller (MT usage!)
    {
        pod_.spinLock.lock();
        ZEN_ON_SCOPE_EXIT(pod_.spinLock.unlock());

        if (pod_.inst)
            return *pod_.inst;
        return nullptr;
    }

    void set(std::unique_ptr<T>&& newInst)
    {
        std::shared_ptr<T>* tmpInst = nullptr;
        if (newInst)
            tmpInst = new std::shared_ptr<T>(std::move(newInst));
        {
            pod_.spinLock.lock();
            ZEN_ON_SCOPE_EXIT(pod_.spinLock.unlock());

            if (!pod_.destroyed)
                std::swap(pod_.inst, tmpInst);
            else
                assert(false);

            pod_.initialized = true;
        }
        delete tmpInst;
    }

    //for initialization via a frequently-called function (which may be running on parallel threads)
    template <class Function>
    void setOnce(Function getInitialValue /*-> std::unique_ptr<T>*/)
    {
        pod_.spinLock.lock();
        ZEN_ON_SCOPE_EXIT(pod_.spinLock.unlock());

        if (!pod_.initialized)
        {
            assert(!pod_.inst);
            if (!pod_.destroyed)
            {
                if (std::unique_ptr<T> newInst = getInitialValue()) //throw ?
                    pod_.inst = new std::shared_ptr<T>(std::move(newInst));
            }
            else
                assert(false);

            pod_.initialized = true;
        }
    }

private:
    struct Pod
    {
        PodSpinMutex spinLock; //rely entirely on static zero-initialization! => avoid potential contention with worker thread during Global<> construction!
        //serialize access: can't use std::mutex: has non-trival destructor
        std::shared_ptr<T>* inst = nullptr;
        bool initialized = false;
        bool destroyed = false;
    } pod_;
};

//===================================================================================================================
//===================================================================================================================

struct CleanUpEntry
{
    using CleanUpFunction = void (*)(void* callbackData);
    CleanUpFunction cleanUpFun   = nullptr;
    void*           callbackData = nullptr;
    CleanUpEntry*   prev         = nullptr;
};
void registerGlobalForDestruction(CleanUpEntry& entry);


template <class T>
class FunStatGlobal
{
public:
    consteval FunStatGlobal() {}; //demand static zero-initialization!

    //No ~FunStatGlobal(): required to avoid generation of magic statics code for a function-scope static!

    std::shared_ptr<T> get()
    {
        static_assert(std::is_trivially_destructible_v<FunStatGlobal>, "this class must not generate code for magic statics!");

        pod_.spinLock.lock();
        ZEN_ON_SCOPE_EXIT(pod_.spinLock.unlock());

        if (pod_.inst)
            return *pod_.inst;
        return nullptr;
    }

    void set(std::unique_ptr<T>&& newInst)
    {
        std::shared_ptr<T>* tmpInst = nullptr;
        if (newInst)
            tmpInst = new std::shared_ptr<T>(std::move(newInst));
        {
            pod_.spinLock.lock();
            ZEN_ON_SCOPE_EXIT(pod_.spinLock.unlock());

            if (!pod_.destroyed)
                std::swap(pod_.inst, tmpInst);
            else
                assert(false);

            registerDestruction();
        }
        delete tmpInst;
    }

    template <class Function>
    void setOnce(Function getInitialValue /*-> std::unique_ptr<T>*/)
    {
        pod_.spinLock.lock();
        ZEN_ON_SCOPE_EXIT(pod_.spinLock.unlock());

        if (!pod_.cleanUpEntry.cleanUpFun)
        {
            assert(!pod_.inst);
            if (!pod_.destroyed)
            {
                if (std::unique_ptr<T> newInst = getInitialValue()) //throw ?
                    pod_.inst = new std::shared_ptr<T>(std::move(newInst));
            }
            else
                assert(false);

            registerDestruction();
        }
    }

private:
    void destruct()
    {
        static_assert(std::is_trivially_destructible_v<Pod>, "this memory needs to live forever");

        pod_.spinLock.lock();
        std::shared_ptr<T>* oldInst = std::exchange(pod_.inst, nullptr);
        pod_.destroyed = true;
        pod_.spinLock.unlock();

        delete oldInst;
    }

    //call while holding pod_.spinLock
    void registerDestruction()
    {
        assert(pod_.spinLock.isLocked());

        if (!pod_.cleanUpEntry.cleanUpFun)
        {
            pod_.cleanUpEntry.callbackData = this;
            pod_.cleanUpEntry.cleanUpFun = [](void* callbackData)
            {
                static_cast<FunStatGlobal*>(callbackData)->destruct();
            };

            registerGlobalForDestruction(pod_.cleanUpEntry);
        }
    }

    struct Pod
    {
        PodSpinMutex spinLock; //rely entirely on static zero-initialization! => avoid potential contention with worker thread during Global<> construction!
        //serialize access; can't use std::mutex: has non-trival destructor
        std::shared_ptr<T>* inst = nullptr;
        CleanUpEntry cleanUpEntry;
        bool destroyed = false;
    } pod_;
};


inline
void registerGlobalForDestruction(CleanUpEntry& entry)
{
    static struct
    {
        PodSpinMutex  spinLock;
        CleanUpEntry* head = nullptr;
    } cleanUpList;

    static_assert(std::is_trivially_destructible_v<decltype(cleanUpList)>, "we must not generate code for magic statics!");

    cleanUpList.spinLock.lock();
    ZEN_ON_SCOPE_EXIT(cleanUpList.spinLock.unlock());

    std::atexit([]
    {
        cleanUpList.spinLock.lock();
        ZEN_ON_SCOPE_EXIT(cleanUpList.spinLock.unlock());

        (*cleanUpList.head->cleanUpFun)(cleanUpList.head->callbackData);
        cleanUpList.head = cleanUpList.head->prev; //nicely clean up in reverse order of construction
    });

    entry.prev = cleanUpList.head;
    cleanUpList.head = &entry;

}

//------------------------------------------------------------------------------------------

inline
bool PodSpinMutex::tryLock()
{
    return !flag_.test_and_set(std::memory_order_acquire);
}




inline
void PodSpinMutex::lock()
{
    while (!tryLock())
        flag_.wait(true, std::memory_order_relaxed);
}


inline
void PodSpinMutex::unlock()
{
    flag_.clear(std::memory_order_release);
    flag_.notify_one();
}


inline
bool PodSpinMutex::isLocked()
{
    if (!tryLock())
        return true;
    unlock();
    return false;
}
}

#endif //GLOBALS_H_8013740213748021573485

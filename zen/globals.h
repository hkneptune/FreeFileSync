// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef GLOBALS_H_8013740213748021573485
#define GLOBALS_H_8013740213748021573485

#include <atomic>
#include <memory>
#include "scope_guard.h"


namespace zen
{
/*
Solve static destruction order fiasco by providing shared ownership and serialized access to global variables

=>there may be accesses to "Global<T>::get()" during process shutdown e.g. _("") used by message in debug_minidump.cpp or by some detached thread assembling an error message!
=> use trivially-destructible POD only!!!

ATTENTION: function-static globals have the compiler generate "magic statics" == compiler-genenerated locking code which will crash or leak memory when accessed after global is "dead"
           => "solved" by FunStatGlobal, but we can't have "too many" of these...
*/
template <class T>
class Global //don't use for function-scope statics!
{
public:
    Global()
    {
        static_assert(std::is_trivially_constructible_v<Pod>&& std::is_trivially_destructible_v<Pod>, "this memory needs to live forever");
        assert(!pod_.inst && !pod_.spinLock); //we depend on static zero-initialization!
    }

    explicit Global(std::unique_ptr<T>&& newInst) { set(std::move(newInst)); }

    ~Global() { set(nullptr); }

    std::shared_ptr<T> get() //=> return std::shared_ptr to let instance life time be handled by caller (MT usage!)
    {
        while (pod_.spinLock.exchange(true)) ;
        ZEN_ON_SCOPE_EXIT(pod_.spinLock = false);
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
            while (pod_.spinLock.exchange(true)) ;
            ZEN_ON_SCOPE_EXIT(pod_.spinLock = false);
            std::swap(pod_.inst, tmpInst);
        }
        delete tmpInst;
    }

private:
    struct Pod
    {
        std::atomic<bool> spinLock; // { false }; rely entirely on static zero-initialization! => avoid potential contention with worker thread during Global<> construction!
        //serialize access; can't use std::mutex: has non-trival destructor
        std::shared_ptr<T>* inst;   // = nullptr;
    } pod_;
};

//===================================================================================================================
//===================================================================================================================

struct CleanUpEntry
{
    using CleanUpFunction = void (*)(void* callbackData);
    CleanUpFunction cleanUpFun;
    void*           callbackData;
    CleanUpEntry*   prev;
};
void registerGlobalForDestruction(CleanUpEntry& entry);


template <class T>
class FunStatGlobal
{
public:
    //No FunStatGlobal() or ~FunStatGlobal()!

    std::shared_ptr<T> get()
    {
        static_assert(std::is_trivially_constructible_v<FunStatGlobal>&&
                      std::is_trivially_destructible_v<FunStatGlobal>, "this class must not generate code for magic statics!");

        while (pod_.spinLock.exchange(true)) ;
        ZEN_ON_SCOPE_EXIT(pod_.spinLock = false);
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
            while (pod_.spinLock.exchange(true)) ;
            ZEN_ON_SCOPE_EXIT(pod_.spinLock = false);

            std::swap(pod_.inst, tmpInst);
            registerDestruction();
        }
        delete tmpInst;
    }

    void initOnce(std::unique_ptr<T> (*getInitialValue)())
    {
        while (pod_.spinLock.exchange(true)) ;
        ZEN_ON_SCOPE_EXIT(pod_.spinLock = false);

        if (!pod_.cleanUpEntry.cleanUpFun)
        {
            assert(!pod_.inst);
            if (std::unique_ptr<T> newInst = (*getInitialValue)())
                pod_.inst = new std::shared_ptr<T>(std::move(newInst));
            registerDestruction();
        }
    }

private:
    //call while holding pod_.spinLock
    void registerDestruction()
    {
        assert(pod_.spinLock);

        if (!pod_.cleanUpEntry.cleanUpFun)
        {
            pod_.cleanUpEntry.callbackData = this;
            pod_.cleanUpEntry.cleanUpFun = [](void* callbackData)
            {
                auto thisPtr = static_cast<FunStatGlobal*>(callbackData);
                thisPtr->set(nullptr);
            };

            registerGlobalForDestruction(pod_.cleanUpEntry);
        }
    }

    struct Pod
    {
        std::atomic<bool> spinLock; // { false }; rely entirely on static zero-initialization! => avoid potential contention with worker thread during Global<> construction!
        //serialize access; can't use std::mutex: has non-trival destructor
        std::shared_ptr<T>* inst;   // = nullptr;
        CleanUpEntry cleanUpEntry;
    } pod_;
};


inline
void registerGlobalForDestruction(CleanUpEntry& entry)
{
    static struct
    {
        std::atomic<bool> spinLock;
        CleanUpEntry*     head;
    } cleanUpList;

    static_assert(std::is_trivially_constructible_v<decltype(cleanUpList)>&&
                  std::is_trivially_destructible_v<decltype(cleanUpList)>, "we must not generate code for magic statics!");

    while (cleanUpList.spinLock.exchange(true)) ;
    ZEN_ON_SCOPE_EXIT(cleanUpList.spinLock = false);

    std::atexit([]
    {
        while (cleanUpList.spinLock.exchange(true)) ;
        ZEN_ON_SCOPE_EXIT(cleanUpList.spinLock = false);

        (*cleanUpList.head->cleanUpFun)(cleanUpList.head->callbackData);
        cleanUpList.head = cleanUpList.head->prev; //nicely clean up in reverse order of construction
    });

    entry.prev = cleanUpList.head;
    cleanUpList.head = &entry;

}
}

#endif //GLOBALS_H_8013740213748021573485

/**
 * This file declares a mutex like std::mutex but one that keeps track of the
 * lock state and the owning thread.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: MyMutex.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_MYMUTEX_H_
#define MAIN_MISC_MYMUTEX_H_

#include <mutex>

namespace hycast {

class MyMutex final : public std::mutex
{
    bool            isLocked;
    std::thread::id threadId;
    std::mutex      mutex;
public:
    MyMutex()
        : isLocked{false}
        , threadId{}
        , mutex{}
    {}
    void lock() {
        std::lock_guard<decltype(mutex)> lock{mutex};
        std::mutex::lock();
        isLocked = true;
        threadId = std::this_thread::get_id();
    }
    void try_lock() {
        std::lock_guard<decltype(mutex)> lock{mutex};
        if (std::mutex::try_lock()) {
            isLocked = true;
            threadId = std::this_thread::get_id();
        }
    }
    void unlock() {
        std::lock_guard<decltype(mutex)> lock{mutex};
        std::mutex::unlock();
        isLocked = false;
    }
    bool currentThreadHasLock() const {
        std::lock_guard<decltype(mutex)> lock{mutex};
        return isLocked && threadId == std::this_thread::get_id();
    }
};

} // namespace

#endif /* MAIN_MISC_MYMUTEX_H_ */

/**
 * This file implements a mutex like std::mutex but one that keeps track of the
 * lock state and the owning thread.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: MyMutex.cpp
 * @author: Steven R. Emmerson
 */

#include "MyMutex.h"

namespace hycast {

MyMutex::MyMutex()
    : isLocked{false}
    , threadId{}
    , mutex{}
{}

void MyMutex::lock()
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    std::mutex::lock();
    isLocked = true;
    threadId = std::this_thread::get_id();
}

void MyMutex::try_lock()
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    if (std::mutex::try_lock()) {
        isLocked = true;
        threadId = std::this_thread::get_id();
    }
}

void MyMutex::unlock()
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    std::mutex::unlock();
    isLocked = false;
}

bool MyMutex::currentThreadHasLock() const
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    return isLocked && threadId == std::this_thread::get_id();
}

} // namespace

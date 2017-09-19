/**
 * This file implements a synchronous queue: one that provides an exchange point
 * between threads.
 *
 * Copyright 2017 University Corporation for Atmospheric Research
 * See file "Copying" in the top-level source-directory for terms and
 * conditions.
 *
 * SyncQueue.cpp
 *
 *  Created on: Apr 27, 2017
 *      Author: Steven R. Emmerson
 */
#include "config.h"

#include "SyncQueue.h"

#include <condition_variable>
#include <mutex>

namespace hycast {

template<class T>
class SyncQueue<T>::Impl final
{
    std::mutex              mutex;
    std::condition_variable cond;
    T                       obj;
    bool                    haveObj;

public:
    Impl()
        : haveObj{false}
    {}

    void push(T obj)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        this->obj = obj;
        haveObj = true;
        cond.notify_one();
        while (haveObj) {
            Canceler canceler{};
            cond.wait(lock);
        }
    }

    T pop()
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (!haveObj) {
            Canceler canceler{};
            cond.wait(lock);
        }
        haveObj = false;
        cond.notify_one();
        return obj;
    }
};

} // namespace

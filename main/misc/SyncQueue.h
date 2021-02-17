/**
 * This file declares a synchronous queue: one that provides an exchange point
 * between threads.
 *
 * SyncQueue.h
 *
 *  Created on: Apr 27, 2017
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MAIN_MISC_SYNCQUEUE_H_
#define MAIN_MISC_SYNCQUEUE_H_

#include <condition_variable>
#include <memory>
#include <mutex>

namespace hycast {

template<class T>
class SyncQueue final
{
    std::mutex              mutex;
    std::condition_variable cond;
    T                       obj;
    bool                    haveObj;

public:
    SyncQueue()
        : haveObj{false}
    {}

    SyncQueue(const SyncQueue& that) = delete;
    SyncQueue(const SyncQueue&& that) = delete;
    SyncQueue& operator =(const SyncQueue& rhs) = delete;
    SyncQueue& operator =(const SyncQueue&& rhs) = delete;

    void push(T obj)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (haveObj) {
            Canceler canceler{};
            cond.wait(lock);
        }
        this->obj = obj;
        haveObj = true;
        cond.notify_one();
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

#endif /* MAIN_MISC_SYNCQUEUE_H_ */

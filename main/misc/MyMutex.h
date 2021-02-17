/**
 * This file declares a mutex like std::mutex but one that keeps track of the
 * lock state and the owning thread.
 *
 *   @file: MyMutex.h
 * @author: Steven R. Emmerson
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

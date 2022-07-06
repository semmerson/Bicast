/**
 * This file declares a thread-safe, hybrid, unordered set and queue.
 *
 *        File: HashSetQueue.h
 *  Created on: July 6, 2022
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
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

#ifndef MAIN_MISC_HASHSETQUEUE_H_
#define MAIN_MISC_HASHSETQUEUE_H_

#include "error.h"

#include <exception>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace hycast {

/**
 * @tparam VALUE  Value to be stored. Must have default constructor and functions `operator=()`,
 *                `hash()`, and `operator==()`. Smaller values are better.
 */
template<class VALUE>
class HashSetQueue
{
    using Mutex = std::mutex;
    using Guard = std::lock_guard<Mutex>;

    struct Links {
        VALUE prev;
        VALUE next;

        Links(
            const VALUE& prev,
            const VALUE& next)
            : prev{prev}
            , next{next}
        {}

        Links(const VALUE& prev)
            : Links(prev, VALUE())
        {}
    };

    using Hash = std::function<size_t(const VALUE&)>;
    using Equal = std::function<bool(const VALUE&, const VALUE&)>;
    using Map = std::unordered_map<VALUE, Links, Hash, Equal>;

    Mutex mutex;
    Hash  myHash;
    Equal myEqual;
    Map   map;
    VALUE head;
    VALUE tail;

public:
    explicit HashSetQueue(const size_t initialSize = 10)
        : mutex{}
        , myHash([](const VALUE& value){return value.hash();})
        , myEqual([](const VALUE& value1, const VALUE& value2){return value1 == value2;})
        , map(initialSize, myHash, myEqual)
        , head{}
        , tail{}
    {}

    size_t size() const {
        return map.size();
    }

    /**
     * Adds a value to the queue.
     *
     * @param[in] value  Value
     * @retval `true`    Value added
     * @retval `false`   Value not added because it's already in the queue
     * @throw            Exceptions related to construction of `Value`
     * @exceptionSafety  Strong guarantee
     * @threadsafety     Safe
     */
    bool push(const VALUE& value) {
        Guard lock{mutex};
        auto  pair = map.emplace(value, Links{tail});
        try {
            if (!pair.second)
                return false;
            tail = value;
            if (map.size() == 1)
                head = tail;
        }
        catch (const std::exception& ex) {
            map.erase(value);
            throw;
        }
        return true;
    }

    /**
     * Returns a reference to the front value.
     *
     * @throw OutOfRange  Map is empty
     * @exceptionSafety   Nothrow
     * @threadsafety      Safe
     */
    const VALUE& front() noexcept {
        Guard lock{mutex};
        if (map.size() == 0)
            throw OUT_OF_RANGE("Queue is empty");
        return head;
    }

    /**
     * Deletes the front value.
     *
     * @throw OutOfRange  Map is empty
     * @threadsafety      Safe
     */
    void pop() noexcept {
        Guard lock{mutex};
        if (map.size() == 0)
            throw OUT_OF_RANGE("Queue is empty");
        auto& next = map.at(head).next;
        map.erase(head);
        head = next;
        if (map.size() == 0) {
            tail = VALUE();
        }
        else {
            map.at(head).prev = VALUE();
        }
    }

    /**
     * Deletes a value.
     *
     * @param[in] value   Value to be deleted
     * @retval `true`     Value existed
     * @retval `false`    Value did not exist
     * @exceptionSafety   Nothrow
     * @threadsafety      Safe
     */
    bool erase(const VALUE& value) noexcept {
        Guard lock{mutex};

        auto iter = map.find(value);
        if (iter == map.end())
            return false;

        auto& links = iter->second;

        if (head == value) {
            head = links.next;
        }
        else {
            map.at(links.prev).next = links.next;
        }

        if (tail == value) {
            tail = links.prev;
        }
        else {
            map.at(links.next).prev = links.prev;
        }

        map.erase(iter);
        return true;
    }
};

} // namespace

#endif /* MAIN_MISC_HASHSETQUEUE_H_ */

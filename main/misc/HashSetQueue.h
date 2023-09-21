/**
 * @file: HashSetQueue.h
 * This file declares a thread-safe, hybrid, unordered set and queue.
 *
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
 * A thread-safe combination of hash table and queue.
 * @tparam VALUE  Value to be stored. Must have default constructor, copy assignment,  `hash()`, and
 *                `operator==()`. Smaller values are better.
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

    using Hash = std::function<size_t(const VALUE&)>; ///< Hash function
    using Map = std::unordered_map<VALUE, Links, Hash>; ///< Hash table from values to links

    mutable Mutex mutex;    ///< Mutex for maintaining consistency
    Hash          myHash;   ///< Hash function
    Map           linksMap; ///< Map of value to links
    VALUE         head;     ///< First value
    VALUE         tail;     ///< Last value

public:
    /**
     * Constructs.
     * @param[in] initialSize  Initial capacity of the queue
     */
    explicit HashSetQueue(const size_t initialSize = 10)
        : mutex{}
        , myHash([](const VALUE& value){return value.hash();})
        , linksMap(initialSize, myHash)
        , head{}
        , tail{}
    {}

    /**
     * Indicates if this instance is empty.
     * @retval true  This instance is empty
     * @retval false This instance is not empty
     */
    bool empty() const {
        Guard guard{mutex};
        return linksMap.empty();
    }

    /**
     * Returns the number of entries.
     * @return The number of entries
     */
    size_t size() const {
        Guard guard{mutex};
        return linksMap.size();
    }

    /**
     * Adds a value to the queue to the back of the queue.
     *
     * @param[in] value  Value
     * @retval true      Value added
     * @retval false     Value not added because it's already in the queue
     * @throw            Exceptions related to construction of `Value`
     * @exceptionSafety  Strong guarantee
     * @threadsafety     Safe
     */
    bool push(const VALUE& value) {
        Guard lock{mutex};

        auto  pair = linksMap.emplace(value, Links{tail});
        if (!pair.second)
            return false;

        if (linksMap.size() == 1) {
            head = value;
        }
        else {
            linksMap.at(tail).next = value;
        }
        tail = value;

        return true;
    }

    /**
     * Returns a reference to the front value.
     *
     * @throw OutOfRange  Map is empty
     * @exceptionSafety   Nothrow
     * @threadsafety      Safe
     * @throw OutOfRange  Queue is empty
     */
    const VALUE& front() {
        Guard lock{mutex};
        if (linksMap.size() == 0)
            throw OUT_OF_RANGE("Queue is empty");
        //LOG_DEBUG("Returning head=" + head.to_string());
        return head;
    }

    /**
     * Deletes the front value.
     *
     * @throw OutOfRange  Queue is empty
     * @threadsafety      Safe
     */
    void pop() {
        Guard lock{mutex};
        if (linksMap.size() == 0)
            throw OUT_OF_RANGE("Queue is empty");
        auto next = linksMap.at(head).next;
        linksMap.erase(head);
        head = next;
        if (linksMap.size() == 0) {
            tail = VALUE();
        }
        else {
            linksMap.at(head).prev = VALUE();
        }
    }

    /**
     * Deletes a value.
     *
     * @param[in] value   Value to be deleted
     * @retval true       Value existed
     * @retval false      Value did not exist
     * @exceptionSafety   Nothrow
     * @threadsafety      Safe
     */
    bool erase(const VALUE& value) noexcept {
        Guard lock{mutex};

        auto iter = linksMap.find(value);
        if (iter == linksMap.end())
            return false;

        auto& links = iter->second;

        if (head == value) {
            head = links.next;
        }
        else {
            linksMap.at(links.prev).next = links.next;
        }

        if (tail == value) {
            tail = links.prev;
        }
        else {
            linksMap.at(links.next).prev = links.prev;
        }

        linksMap.erase(iter);
        return true;
    }
};

} // namespace

#endif /* MAIN_MISC_HASHSETQUEUE_H_ */

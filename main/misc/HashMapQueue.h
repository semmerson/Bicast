/**
 * This file declares a thread-safe, hybrid, unordered-map and queue.
 *
 *        File: HashMapQueue.h
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

#ifndef MAIN_MISC_HASHMAPQUEUE_H_
#define MAIN_MISC_HASHMAPQUEUE_H_

#include <mutex>
#include <unordered_map>

namespace hycast {

/**
 * @tparam KEY    Key that references the stored values.  Smaller types are better.
 * @tparam VALUE  Value to be stored in the queue. Must have default constructor and functions
 *                `operator=()`, `hash()`, and `operator==()`.
 */
template<class KEY, class VALUE>
class HashMapQueue
{
    using Mutex = std::mutex;
    using Guard = std::lock_guard<Mutex>;
    using Cond  = std::condition_variable;

    struct MappedValue
    {
        VALUE  value;
        KEY    prev;
        KEY    next;

        inline MappedValue(
                const KEY&   prev,
                const VALUE& value)
            : value{value}
            , prev{prev}
            , next{}
        {}
    };

    using Hash = std::function<size_t(const KEY&)>;
    using Equal = std::function<bool(const KEY&, const KEY&)>;
    using Map = std::unordered_map<KEY, MappedValue, Hash, Equal>;

    mutable Mutex  mutex;
    mutable Cond   cond;
    Hash           myHash;
    Equal          myEqual;
    Map            map;
    KEY            head;
    KEY            tail;

public:
    explicit HashMapQueue(const size_t initialSize = 10)
        : mutex{}
        , cond()
        , myHash([](const KEY& key){return key.hash();})
        , myEqual([](const KEY& key1, const KEY& key2){return key1 == key2;})
        , map{initialSize, myHash, myEqual}
        , head{}
        , tail{}
    {}


    /**
     * Adds an entry to the back of the queue.
     *
     * @param[in] key        Key
     * @param[in] value      Value
     * @return               Pointer to the added value
     * @retval    `nullptr`  Value wasn't added because the key already exists
     * @throw                Exceptions related to construction of the key and value
     * @exceptionSafety      Strong guarantee
     * @threadsafety         Safe
     */
    VALUE* push(
            const KEY&   key,
            const VALUE& value) {
        Guard guard{mutex};
        auto  pair = map.emplace(key, MappedValue{tail, value});
        try {
            if (!pair.second)
                return nullptr;
            tail = key;
            if (map.size() == 1)
                head = tail;
        }
        catch (const std::exception& ex) {
            map.erase(key);
            throw;
        }
        return &pair.first->second.value;
    }

    bool empty() const {
        Guard guard(mutex);
        return map.empty();
    }

    /**
     * Returns the front entry.
     *
     * @param[out] key    Key
     * @return            Pointer to value referenced by key
     * @throw OutOfRange  Queue is empty
     * @threadsafety      Safe
    VALUE& front(const KEY*& key) {
        Guard guard(mutex);
        if (map.size() == 0)
            throw OUT_OF_RANGE("Queue is empty");

        key = &head;
        return map.at(head).value;
    }
    const KEY& front(VALUE*& value) {
        Guard guard(mutex);
        if (map.size() == 0)
            throw OUT_OF_RANGE("Queue is empty");

        value = &map.at(head).value;
        return head;
    }
     */
    std::pair<const KEY&, VALUE&> front() {
        Guard guard(mutex);
        if (map.size() == 0)
            throw OUT_OF_RANGE("Queue is empty");

        return {head, map.at(head).value};
    }

    /**
     * Returns a pointer to a given value.
     *
     * @param[in] key        Key
     * @return               Pointer to value associated with key
     * @retval    `nullptr`  No such value
     */
    VALUE* get(const KEY& key) {
        auto iter = map.find(key);
        return (iter == map.end()) ? nullptr : &iter->second.value;
    }

    /**
     * Deletes the front value.
     *
     * @threadsafety    Safe
     */
    void pop() noexcept {
        Guard guard(mutex);
        if (map.size()) {
            auto& next = map.at(head).next;
            map.erase(head);
            head = next;
            if (map.size() == 0) {
                tail = KEY();
            }
            else {
                map.at(head).prev = KEY();
            }
        }
    }
    /**
     * Deletes an entry.
     *
     * @param[in]  key    Key
     * @retval `true`     Entry existed
     * @retval `false`    Entry did not exist
     * @exceptionSafety   Nothrow
     * @threadsafety      Safe
     */
    bool erase(KEY& key) noexcept {
        Guard guard(mutex);

        auto iter = map.find(key);
        if (iter == map.end())
            return false;

        auto& mappedValue = iter->second;

        if (head == key) {
            head = mappedValue.next;
        }
        else {
            map.at(mappedValue.prev).next = mappedValue.next;
        }

        if (tail == key) {
            tail = mappedValue.prev;
        }
        else {
            map.at(mappedValue.next).prev = mappedValue.prev;
        }

        map.erase(iter);
        return true;
    }
};

} // namespace

#endif /* MAIN_MISC_HASHMAPQUEUE_H_ */

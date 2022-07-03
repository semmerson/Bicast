/**
 * This file declares a thread-safe, combined doubly-linked list and hash table.
 *
 *        File: LinkedHashMap.h
 *  Created on: Dec 16, 2017
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

#ifndef MAIN_MISC_LINKEDHASHMAP_H_
#define MAIN_MISC_LINKEDHASHMAP_H_

#include <exception>
#include <mutex>
#include <unordered_map>

namespace hycast {

template<class KEY, class VALUE>
class LinkedHashMap
{
    using Mutex = std::mutex;
    using Guard = std::lock_guard<Mutex>;

    struct MapValue
    {
        VALUE  value;
        KEY    prev;
        KEY    next;

        inline MapValue(
                const KEY&   prev,
                const VALUE& value)
            : value{value}
            , prev{prev}
            , next{}
        {}
    };

    std::unordered_map<KEY, MapValue> map;
    Mutex                             mutex;
    KEY                               head;
    KEY                               tail;

public:
    LinkedHashMap()
        : map{}
        , mutex{}
        , head{}
        , tail{}
    {}

    LinkedHashMap(const ssize_t initialSize)
        : map{initialSize}
        , mutex{}
        , head{}
        , tail{}
    {}

    /**
     * Adds a key/value pair to the back of the map.
     *
     * @param[in] key    Key
     * @param[in] value  Value
     * @retval `true`    Previous entry under `key` did not exist
     * @retval `false`   Previous entry under `key` did exist. Entry wasn't replaced.
     * @throw            Exceptions related to construction of `Key` and `Value`
     * @exceptionSafety  Strong guarantee
     * @threadsafety     Safe
     */
    bool pushBack(
            const KEY&   key,
            const VALUE& value);

    /**
     * Returns a reference to the oldest value.
     *
     * @throw OutOfRange  Map is empty
     * @exceptionSafety   Nothrow
     * @threadsafety      Safe
     */
    VALUE& front() noexcept;

    /**
     * Returns a pointer to a given value.
     *
     * @param[in] key  Key
     * @return         Pointer to value associated with key
     */
    VALUE* get(const KEY& key) const;

    /**
     * Deletes the oldest value.
     *
     * @throw OutOfRange  Map is empty
     * @threadsafety      Safe
     */
    void pop() noexcept;

    /**
     * Removes the entry corresponding to a key.
     *
     * @param[in]  key    Key
     * @retval `true`     Entry existed
     * @retval `false`    Entry did not exist
     * @exceptionSafety   Nothrow
     * @threadsafety      Safe
     */
    bool remove(KEY& key) noexcept;
};

} // namespace

#endif /* MAIN_MISC_LINKEDHASHMAP_H_ */

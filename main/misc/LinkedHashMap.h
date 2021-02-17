/**
 * This file declares a thread-safe, combined doubly-linked list and hash table.
 *
 *        File: LinkedHashMap.h
 *  Created on: Dec 16, 2017
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

#ifndef MAIN_MISC_LINKEDHASHMAP_H_
#define MAIN_MISC_LINKEDHASHMAP_H_

#include <exception>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace hycast {

template<class Key, class Value>
class LinkedHashMap
{
    typedef std::shared_ptr<Key> KeyPtr;
    struct MapValue
    {
        KeyPtr prev;
        KeyPtr next;
        Value  value;

        inline MapValue(
                const KeyPtr& prev,
                const Value&  value)
            : prev{prev}
            , next{}
            , value{value}
        {}
    };
    typedef std::mutex             Mutex;
    typedef std::lock_guard<Mutex> LockGuard;

    std::unordered_map<Key, MapValue> map;
    Mutex                          mutex;
    KeyPtr                         front;
    KeyPtr                         back;

public:
    LinkedHashMap()
        : map{}
        , mutex{}
        , front{}
        , back{}
    {}

    /**
     * Inserts a key/value pair.
     * @param[in] key    Key
     * @param[in] value  Value
     * @retval `true`    Previous entry under `key` did not exist
     * @retval `false`   Previous entry under `key` did exist. Entry wasn't
     *                   replaced.
     * @throw            Exceptions related to construction of `Key` and `Value`
     * @exceptionSafety  Strong guarantee
     * @threadsafety     Safe
     */
    bool insert(
            const Key&   key,
            const Value& value)
    {
        LockGuard lock{mutex};
        auto      pair = map.emplace(key, MapValue{back, value});
        try {
            if (!pair.second)
                return false;
            back = KeyPtr{new Key{key}};
            if (!front)
                front = back;
        }
        catch (const std::exception& ex) {
            map.erase(key);
            throw;
        }
        return true;
    }

    /**
     * Removes the oldest inserted entry and returns its value.
     * @param[out] value  Value
     * @retval `true`     Oldest entry existed
     * @retval `false`    No entries
     * @exceptionSafety   Nothrow
     * @threadsafety      Safe
     */
    bool pop(Value& value) noexcept
    {
        LockGuard lock{mutex};
        if (!front)
            return false;
        KeyPtr    keyPtr = front;
        MapValue& mapValue = map.at(*keyPtr);
        value = mapValue.value;
        front = mapValue.next;
        front
            ? map.at(*front).prev.reset()
            : back.reset();
        map.erase(*keyPtr);
        return true;
    }

    /**
     * Removes the entry corresponding to a key.
     * @param[in]  key    Key
     * @retval `true`     Entry existed
     * @retval `false`    Entry did not exist
     * @exceptionSafety   Nothrow
     * @threadsafety      Safe
     */
    bool remove(Key& key) noexcept
    {
        LockGuard lock{mutex};
        auto iter = map.find(key);
        if (iter == map.end())
            return false;
        MapValue& mapValue = iter->second;
        KeyPtr keyPtr = mapValue.prev;
        keyPtr
            ? (map.at(*keyPtr).next = mapValue.next)
            : (front = mapValue.next);
        keyPtr = mapValue.next;
        keyPtr
            ? (map.at(*keyPtr).prev = mapValue.prev)
            : (back = mapValue.prev);
        map.erase(iter);
        return true;
    }
};

} // namespace

#endif /* MAIN_MISC_LINKEDHASHMAP_H_ */

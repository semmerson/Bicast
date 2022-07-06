/**
 * This file defines a thread-safe, combined doubly-linked list and hash table.
 *
 *  @file:  LinkedHashMap.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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

#include "config.h"

#include "error.h"
#include "LinkedHashMap.h"

namespace hycast {

template<class KEY, class VALUE>
bool HashMapQueue<KEY, VALUE>::pushBack(
        const KEY&   key,
        const VALUE& value)
{
    Guard lock{mutex};
    auto  pair = map.emplace(key, MapValue{tail, value});
    try {
        if (!pair.second)
            return false;
        tail = key;
        if (map.size() == 1)
            head = tail;
    }
    catch (const std::exception& ex) {
        map.erase(key);
        throw;
    }
    return true;
}

template<class KEY, class VALUE>
VALUE& HashMapQueue<KEY, VALUE>::front() noexcept
{
    Guard lock{mutex};
    if (map.size() == 0)
        throw OUT_OF_RANGE("Map is empty");
    return map.at(head).value;
}

template<class KEY, class VALUE>
void HashMapQueue<KEY, VALUE>::pop() noexcept
{
    Guard lock{mutex};
    if (map.size() == 0)
        throw OUT_OF_RANGE("Map is empty");
    if (map.size() == 1) {
        map.erase(head);
        head = tail = KEY();
    }
    else {
        auto origHead = head;
        head = map.at(head).next;
        map.erase(origHead);
    }
}

template<class KEY, class VALUE>
bool HashMapQueue<KEY, VALUE>::remove(KEY& key) noexcept
{
    Guard lock{mutex};
    auto iter = map.find(key);
    if (iter == map.end())
        return false;
    MapValue& mapValue = iter->second;
    if (map.size() == 1) {
        head = tail = KEY();
    }
    else {
        if (head == key) {
            head = mapValue.next;
            map.at(mapValue.next).prev = mapValue.prev;
        }
        else if (tail == key) {
            tail = mapValue.prev;
            map.at(mapValue.prev).next = mapValue.next;
        }
        else {
            map.at(mapValue.next).prev = mapValue.prev;
            map.at(mapValue.prev).next = mapValue.next;
        }
    }
    map.erase(iter);
    return true;
}

} // namespace

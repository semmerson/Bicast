/**
 * This file defines a thread-safe, combined doubly-linked list and hash table.
 *
 *  @file:  LinkedHashSet.cpp
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
#include "LinkedHashSet.h"

namespace hycast {

template<class VALUE>
bool LinkedHashSet<VALUE>::pushBack(const VALUE& value)
{
    Guard lock{mutex};
    auto  pair = set.emplace(Elt{tail, value});
    try {
        if (!pair.second)
            return false;
        tail = value;
        if (set.size() == 1)
            head = tail;
    }
    catch (const std::exception& ex) {
        set.erase(value);
        throw;
    }
    return true;
}

template<class VALUE>
VALUE& LinkedHashSet<VALUE>::front() noexcept
{
    Guard lock{mutex};
    if (set.size() == 0)
        throw OUT_OF_RANGE("Map is empty");
    return set.at(head).value;
}

template<class VALUE>
void LinkedHashSet<VALUE>::pop() noexcept
{
    Guard lock{mutex};
    if (set.size() == 0)
        throw OUT_OF_RANGE("Map is empty");
    if (set.size() == 1) {
        set.erase(head);
        head = tail = VALUE();
    }
    else {
        auto origHead = head;
        head = set.at(head).next;
        set.erase(origHead);
    }
}

template<class VALUE>
bool LinkedHashSet<VALUE>::remove(const VALUE& value) noexcept
{
    Guard lock{mutex};
    auto iter = set.find(value);
    if (iter == set.end())
        return false;
    auto& elt = iter->second;
    if (set.size() == 1) {
        head = tail = VALUE();
    }
    else {
        if (head == value) {
            head = elt.next;
            set.at(elt.next).prev = elt.prev;
        }
        else if (tail == value) {
            tail = elt.prev;
            set.at(elt.prev).next = elt.next;
        }
        else {
            set.at(elt.next).prev = elt.prev;
            set.at(elt.prev).next = elt.next;
        }
    }
    set.erase(iter);
    return true;
}

} // namespace

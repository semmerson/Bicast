/**
 * This file implements a mapping from a key to a list of values. It is similar
 * to `std::unordered_multimap` with the additional guarantee that the values in
 * each list are ordered by their time of addition (earliest to latest).
 *
 *   @file: MapOfLists.cpp
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

#include "MapOfLists.h"

namespace bicast {

template<class K, class V>
MapOfLists<K, V>::MapOfLists(unsigned numKeys)
    : map{numKeys},
      mutex{},
      cond{}
{}

template<class K, class V>
void MapOfLists<K, V>::add(
        const K& key,
        V&       value)
{
    std::lock_guard<std::mutex> lock(mutex);
    map[key].push_back(value);
}

template<class K, class V>
typename MapOfLists<K, V>::ValueBounds MapOfLists<K, V>::getValues(const K& key)
        const
{
    std::lock_guard<std::mutex> lock(mutex);
    typename Map::const_iterator listIter{map.find(key)};
    if (listIter == map.end()) {
        typename Map::mapped_type::const_iterator emptyIter{};
        return ValueBounds{emptyIter, emptyIter};
    }
    return ValueBounds{listIter->second.begin(), listIter->second.end()};
}

template<class K, class V>
void MapOfLists<K, V>::remove(const K& key)
{
    std::lock_guard<std::mutex> lock(mutex);
    map.erase(key);
}

template<class K, class V>
void MapOfLists<K, V>::remove(
        const K& key,
        const V& value)
{
    std::lock_guard<std::mutex> lock(mutex);
    typename Map::iterator listIter{map.find(key)};
    if (listIter != map.end())
        listIter->second.remove(value);
}

} // namespace

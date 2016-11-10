/**
 * This file implements a mapping from a key to a list of values. It is similar
 * to `std::unordered_multimap` with the additional guarantee that the values in
 * each list are ordered by their time of addition (earliest to latest).
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: MapOfLists.cpp
 * @author: Steven R. Emmerson
 */

#include "ChunkInfo.h"
#include "MapOfLists.h"
#include "Peer.h"

namespace hycast {

template<class K, class V>
MapOfLists<K, V>::MapOfLists(unsigned numKeys)
    : map{numKeys, &K::hash, &K::areEqual},
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

template class MapOfLists<ChunkInfo, Peer>;

} // namespace

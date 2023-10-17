/**
 * This file declares a mapping from a key to a list of values. It is similar to
 * `std::unordered_multimap` with the additional guarantee that the values in
 * each list are ordered by their time of addition (earliest to latest).
 *
 *   @file: MapOfLists.h
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

#ifndef MAP_OF_LISTS_H_
#define MAP_OF_LISTS_H_

#include <condition_variable>
#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace bicast {

/**
 * @tparam K  Type of key. The classes `std::hash<K>` and `std::equal_to<K>`
 *            must exist.
 * @tparam V  Type of value
 */
template<class K, class V>
class MapOfLists final {
    typedef std::unordered_map<K, std::list<V>> Map;

    Map                             map;
    mutable std::mutex              mutex;
    mutable std::condition_variable cond;

    MapOfLists(const MapOfLists& map);
    MapOfLists& operator=(const MapOfLists& map);
public:
    /// Type of iterator over values in a list
    typedef typename Map::mapped_type::const_iterator ValueIterator;
    /// Range of values in a list
    typedef std::pair<ValueIterator, ValueIterator>   ValueBounds;
    /**
     * Constructs from the initial number of keys.
     * @param[in] numKeys  Initial number of keys
     */
    explicit MapOfLists(unsigned numKeys = 0);
    /**
     * Adds a value to the list of values that have a particular key-of-data.
     * The list is ordered by insertion with the first added value at the front.
     * @param[in] key    Key
     * @param[in] value  Value
     * @exceptionsafety Basic guarantee
     * @threadsafety    Safe
     */
    void add(
            const K& key,
            V&       value);
    /**
     * Returns the bounds of a range that includes all the values that have a
     * given key. The values are in the same order in which they were added.
     * @param[in] key   Key
     * @return          All the values that have the key in the order of
     *                  their addition. The values go from `pair::first`
     *                  (inclusive) to `pair::second` (exclusive).
     * @exceptionsafety Basic guarantee
     * @threadsafety    Safe
     */
    std::pair<ValueIterator, ValueIterator> getValues(const K& key) const;
    /**
     * Removes a particular key and all the values associated with it.
     * @param[in] key   Key
     * @exceptionsafety Basic guarantee
     * @threadsafety    Safe
     */
    void remove(const K& key);
    /**
     * Removes a value from from the list of values associated with a particular
     * key.
     * @param[in] key    The key
     * @param[in] value  The value to be removed from the list of values
     *                   associated with the key
     * @exceptionsafety Basic guarantee
     * @threadsafety    Safe
     */
    void remove(
            const K& key,
            const V& value);
};

} // namespace

#endif /* MAP_OF_LISTS_H_ */

/**
 * This file declares a mapping from a key to a list of values. It is similar to
 * `std::unordered_multimap` with the additional guarantee that the values in
 * each list are ordered by their time of addition (earliest to latest).
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: MapOfLists.h
 * @author: Steven R. Emmerson
 */

#ifndef MAP_OF_LISTS_H_
#define MAP_OF_LISTS_H_

#include <condition_variable>
#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace hycast {

/**
 * @tparam K  Type of key. Must have the static methods
 *              - `size_t K::hash(const K&)` Returns the hash code of a key
 *              - `bool K::areEqual(const K& k1, const K& k2)` Returns `true`
 *                 iff `k1` and `k2` are considered equal
 * @tparam V  Type of value
 */
template<class K, class V>
class MapOfLists final {
    typedef std::unordered_map<K, std::list<V>,
            decltype(&K::hash), decltype(&K::areEqual)> Map;

    Map                             map;
    mutable std::mutex              mutex;
    mutable std::condition_variable cond;

    MapOfLists(const MapOfLists& map);
    MapOfLists& operator=(const MapOfLists& map);
public:
    typedef typename Map::mapped_type::const_iterator ValueIterator;
    typedef std::pair<ValueIterator, ValueIterator>   ValueBounds;
    /**
     * Constructs from the initial number of keys.
     * @param[in] numKeys  Initial number of keys
     */
    MapOfLists(unsigned numKeys = 0);
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

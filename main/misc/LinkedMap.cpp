/**
 * Map whose entries are also linked together into a list.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: LinkedMap.cpp
 *  Created on: May 13, 2020
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "LinkedMap.h"

#include "error.h"

#include <unordered_map>

namespace hycast {

/**
 * @tparam KEY    Key type. Must have:
 *                  - Default constructor;
 *                  - `operator bool()`, which must return false if and only if
 *                    the instance was default constructed; and
 *                  - `operator ==(const KEY& rhs)`
 * @tparam VALUE  Value type
 */
template<class KEY, class VALUE>
class Impl final
{
    /**
     * Mapped-to entry in the map.
     */
    struct Entry final
    {
        VALUE value; ///< User's value
        KEY   prev;  ///< Key of previous entry (towards the head)
        KEY   next;  ///< Key of subsequent entry (towards the tail)

        Entry(VALUE& value, KEY& prev)
            : value(value)
            , prev(prev)
            , next()
        {}
    };

    std::unordered_map<KEY, Entry> map;  ///< Map from user's key to entry
    KEY                            head; ///< Key of head of list
    KEY                            tail; ///< Key of tail of list

public:
    /**
     * Default constructs.
     */
    Impl()
        : map()
        , head()
        , tail()
    {}

    /**
     * Constructs with an initial number of buckets for the map.
     *
     * @param[in] initSize  Initial size
     */
    Impl(const size_t initSize)
        : map(initSize)
        , head()
        , tail()
    {}

    /**
     * Returns the number of entries.
     *
     * @return Number of entries
     */
    size_t size()
    {
        return map.size();
    }

    /**
     * Adds an entry. If a new entry is created, then it is added to the tail of
     * the list.
     *
     * @param[in] key    Key
     * @param[in] value  Value mapped-to by key
     * @return           Pair whose first element is reference to value
     *                   associated with key and whose second element is true if
     *                   and only if new entry was created.
     */
    std::pair<VALUE&, bool> add(const KEY& key, VALUE& value)
    {
        if (!key)
            throw INVALID_ARGUMENT("Key is invalid");

        auto pair = map.insert({key, Entry(value, tail)});

        if (pair.second) {
            // New entry
            if (tail) {
                map[tail].next = key;
            }
            else {
                head = key;
            }
            tail = key;
        }

        return {pair.first->second, pair.second};
    }

    /**
     * Returns the value that corresponds to a key.
     *
     * @param[in] key        Key
     * @retval    `nullptr`  No such value
     * @return               Pointer to value. Valid only while no changes are
     *                       made.
     */
    VALUE* find(const KEY& key)
    {
        auto iter = map.find(key);
        return (iter == map.end())
                ? nullptr
                : &iter->second.value;
    }

    /**
     * Removes an entry.
     *
     * @param[in] key           Key of entry to be removed
     * @return                  Value associated with key
     * @throws InvalidArgument  No such entry
     */
    VALUE remove(const KEY& key)
    {
        auto iter = map.find(key);

        if (iter == map.end())
            throw INVALID_ARGUMENT("No such entry");

        Entry& entry = iter->second;

        if (entry.prev) {
            map[entry.prev].next = entry.next;
        }
        else {
            head = entry.next;
        }

        if (entry.next) {
            map[entry.next].prev = entry.prev;
        }
        else {
            tail = entry.prev;
        }

        return entry.value;
    }

    /**
     * Returns the key of the head of the list.
     *
     * @return Key of head of list. Will test false if the list is empty.
     */
    KEY getHead()
    {
        return head;
    }

    /**
     * Returns the key of the tail of the list.
     *
     * @return Key of tail of list. Will test false if the list is empty.
     */
    KEY getTail()
    {
        return tail;
    }
};

template<class KEY, class VALUE>
LinkedMap<KEY,VALUE>::LinkedMap(Impl* impl)
    : pImpl(impl)
{}

template<class KEY, class VALUE>
LinkedMap<KEY,VALUE>::LinkedMap()
    : LinkedMap(new Impl())
{}

template<class KEY, class VALUE>
LinkedMap<KEY,VALUE>::LinkedMap(const size_t initSize)
    : LinkedMap(new Impl(initSize))
{}

template<class KEY, class VALUE>
size_t LinkedMap<KEY,VALUE>::size() const noexcept
{
    return pImpl->size();
}

template<class KEY, class VALUE>
std::pair<VALUE&, bool> LinkedMap<KEY,VALUE>::add(const KEY& key, VALUE& value)
{
    return pImpl->add(key, value);
}

template<class KEY, class VALUE>
VALUE* LinkedMap<KEY,VALUE>::find(const KEY& key)
{
    return pImpl->find(key);
}

template<class KEY, class VALUE>
VALUE LinkedMap<KEY,VALUE>::remove(const KEY& key)
{
    return pImpl->remove(key);
}

template<class KEY, class VALUE>
KEY LinkedMap<KEY,VALUE>::getHead()
{
    return pImpl->getHead();
}

template<class KEY, class VALUE>
KEY LinkedMap<KEY,VALUE>::getTail()
{
    return pImpl->getTail();
}

} // namespace

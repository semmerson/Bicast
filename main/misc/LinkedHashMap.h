/**
 * This file declares a thread-safe, combined linked-list and hash table.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: LinkedHashMap.h
 *  Created on: Dec 16, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_LINKEDHASHMAP_H_
#define MAIN_MISC_LINKEDHASHMAP_H_

#include <exception>
#include <list>
#include <memory>
#include <unordered_map>

namespace hycast {

template<class Key, class Value>
class LinkedHashMap
{
    typedef std::shared_ptr<Key> KeyPtr;
    class Entry
    {
        KeyPtr prev;
        KeyPtr next;
        Value  value;
    public:
        inline Entry(
                const KeyPtr& prev,
                const Value&  value)
            : prev{prev}
            , next{}
            , value{value}
        {}
        inline Key getNext() const
        {
            return next;
        }
        inline Key getValue() const
        {
            return value;
        }
    };
    typedef std::mutex             Mutex;
    typedef std::lock_guard<Mutex> LockGuard;

    std::unordered_map<Key, Entry> map;
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
        auto pair = map.emplace(key, Entry{back, value});
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
     * Returns the oldest inserted value and removes it.
     * @param[out] value  Value
     * @retval `true`     Oldest entry existed
     * @retval `false`    No entries
     * @exceptionSafety   Strong guarantee
     * @threadsafety      Safe
     */
    bool pop(Value& value)
    {
        LockGuard lock{mutex};
        if (!front)
            return false;
        auto key = *front;
        auto entry = map.at(key);
        value = entry.getValue();
        front = entry.getNext();
        if (!front)
            back.reset();
        map.erase(key);
        return true;
    }
};

} // namespace

#endif /* MAIN_MISC_LINKEDHASHMAP_H_ */

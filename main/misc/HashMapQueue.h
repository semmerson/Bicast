/**
 * This file declares a combination of unordered-map and queue. The implementation is
 * thread-compatible, but not thread-safe.
 *
 *        File: HashMapQueue.h
 *  Created on: July 6, 2022
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */

#ifndef MAIN_MISC_HASHMAPQUEUE_H_
#define MAIN_MISC_HASHMAPQUEUE_H_

#include "error.h"

#include <functional>
#include <iterator>
#include <unordered_map>

namespace hycast {

/**
 * A combination of queue and hash table.
 * @tparam KEY    Key that references the stored values. Must have default constructor, assignment
 *                operator, `hash()`, and `operator==()`. Smaller types are better.
 * @tparam VALUE  Value to be stored in the queue. Must have default constructor and assignment
 *                operator.
 */
template<class KEY, class VALUE>
class HashMapQueue
{
    /// Value of the hash table with links to previous and subsequent values
    struct LinkedValue
    {
        VALUE  value;
        KEY    prev;
        KEY    next;

        inline LinkedValue(
                const KEY&   prev,
                const VALUE& value)
            : value{value}
            , prev{prev}
            , next{}
        {}
    };

    using Hash         = std::function<size_t(const KEY&)>; ///< Hash function
    using Equal        = std::function<bool(const KEY&, const KEY&)>; ///< Equality function
    using Map          = std::unordered_map<KEY, LinkedValue, Hash, Equal>; ///< Hash table
    using LogicalEntry = std::pair<const KEY&, VALUE&>; ///< Logical representation of an entry

    class Iterator : public std::iterator<std::input_iterator_tag, LogicalEntry>
    {
        KEY next;

    public:
        Iterator(KEY next) : next(next) {}
        Iterator(const Iterator& iter) : next(iter.next) {}
        Iterator& operator=(const Iterator& rhs) {next=rhs.next;}
        Iterator& operator++() {if (next) next = next->next; return *this;}
        Iterator operator++(int) {Iterator tmp(*this); operator++(); return tmp;}
        bool operator==(const Iterator& rhs) const {return next==rhs.next;}
        bool operator!=(const Iterator& rhs) const {return next!=rhs.next;}
        LogicalEntry operator*() {return LogicalEntry{next->first, next->second.value};}
    };

    Hash  myHash;  ///< Hash function
    Equal myEqual; ///< Equality function
    Map   map;     ///< Map from key to linked value
    KEY   head;    ///< Key of first linked value
    KEY   tail;    ///< Key of last linked value

public:
    /**
     * Constructs. The queue will be empty.
     * @param[in] initialSize  Initial capacity of the queue
     */
    explicit HashMapQueue(const size_t initialSize = 10)
        : myHash([](const KEY& key){return key.hash();})
        , myEqual([](const KEY& key1, const KEY& key2){return key1 == key2;})
        , map{initialSize, myHash, myEqual}
        , head{}
        , tail{}
    {}


    /**
     * Adds an entry to the back of the queue.
     *
     * @param[in] key        Key
     * @param[in] value      Value
     * @return               Pointer to the added value
     * @retval    `nullptr`  Value wasn't added because the key already exists
     * @throw                Exceptions related to construction of the key and value
     * @exceptionSafety      Strong guarantee
     * @threadsafety         Safe
     */
    VALUE* pushBack(
            const KEY&   key,
            const VALUE& value) {
        //LOG_DEBUG("Inserting {key=" + key.to_string() + "}");
        auto  pair = map.emplace(key, LinkedValue{tail, value});
        if (!pair.second)
            return nullptr;

        if (map.size() == 1) {
            head = key;
        }
        else {
            map.at(tail).next = key;
        }
        tail = key;

        return &pair.first->second.value;
    }

    /**
     * Indicates if this instance is empty.
     * @retval true   This instance is empty
     * @retval true   This instance is not empty
     */
    bool empty() const {
        return map.empty();
    }

    size_t size() const {
        return map.size();
    }

    /**
     * Returns the front entry.
     *
     * @return            Pointer to value referenced by key
     * @throw OutOfRange  Queue is empty
     * @threadsafety      Safe
     */
    LogicalEntry front() {
        if (map.size() == 0)
            throw OUT_OF_RANGE("Queue is empty");

        return LogicalEntry{head, map.at(head).value};
    }

    /**
     * Returns a pointer to a given value.
     *
     * @param[in] key        Key
     * @return               Pointer to value associated with key
     * @retval    `nullptr`  No such value
     */
    VALUE* get(const KEY& key) {
        //LOG_DEBUG("Finding key=" + key.to_string());
        auto iter = map.find(key);
        return (iter == map.end()) ? nullptr : &iter->second.value;
    }

    /**
     * Deletes the front value.
     *
     * @threadsafety    Safe
     */
    void pop() noexcept {
        if (map.size()) {
            auto& next = map.at(head).next;
            map.erase(head);
            head = next;
            if (map.size() == 0) {
                tail = KEY();
            }
            else {
                map.at(head).prev = KEY();
            }
        }
    }

    /**
     * Deletes an entry.
     *
     * @param[in]  key    Key
     * @retval true       Entry existed
     * @retval false      Entry did not exist
     * @exceptionSafety   Nothrow
     * @threadsafety      Safe
     */
    bool erase(KEY& key) noexcept {
        auto iter = map.find(key);
        if (iter == map.end())
            return false;

        auto& mappedValue = iter->second;

        if (head == key) {
            head = mappedValue.next;
        }
        else {
            map.at(mappedValue.prev).next = mappedValue.next;
        }

        if (tail == key) {
            tail = mappedValue.prev;
        }
        else {
            map.at(mappedValue.next).prev = mappedValue.prev;
        }

        map.erase(iter);
        return true;
    }

    Iterator begin() {
        return Iterator(head);
    }

    Iterator end() {
        return ++Iterator{tail};
    }
};

} // namespace

#endif /* MAIN_MISC_HASHMAPQUEUE_H_ */

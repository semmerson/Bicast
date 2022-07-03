/**
 * This file declares a thread-safe, combined doubly-linked list and hash table.
 *
 *        File: LinkedHashSet.h
 *  Created on: Dec 16, 2017
 *      Author: Steven R. Emmerson
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

#ifndef MAIN_MISC_LINKEDHASHSET_H_
#define MAIN_MISC_LINKEDHASHSET_H_

#include <exception>
#include <functional>
#include <mutex>
#include <unordered_set>

namespace hycast {

template<class VALUE>
class LinkedHashSet
{
    using Mutex = std::mutex;
    using Guard = std::lock_guard<Mutex>;

    struct Elt
    {
        VALUE value;
        VALUE prev;
        VALUE next;

        Elt(    const VALUE& prev,
                const VALUE& value)
            : value{value}
            , prev{prev}
            , next{}
        {}

        size_t hash() const {
            return value.hash();
        }

        bool operator==(const Elt& rhs) const {
            return value == rhs.value;
        }
    };

    std::function<size_t(const Elt&)>           myHash =
            [](const Elt& elt){return elt.hash();};
    std::function<bool(const Elt&, const Elt&)> myEqual =
            [](const Elt& elt1, const Elt& elt2){return elt1 == elt2;};
    std::unordered_set<Elt, decltype(myHash), decltype(myEqual)> set;
    Mutex                   mutex;
    VALUE                   head;
    VALUE                   tail;

public:
    LinkedHashSet(const size_t initialSize = 10)
        : set(initialSize, myHash, myEqual)
        , mutex{}
        , head{}
        , tail{}
    {}

    size_t size() const {
        return set.size();
    }

    /**
     * Adds a value to the back.
     *
     * @param[in] value  Value
     * @retval `true`    Previous entry did not exist
     * @retval `false`   Previous entry did exist. Entry wasn't replaced.
     * @throw            Exceptions related to construction of `Value`
     * @exceptionSafety  Strong guarantee
     * @threadsafety     Safe
     */
    bool pushBack(const VALUE& value);

    /**
     * Returns a pointer to a given value.
     *
     * @param[in] value      The value to be found
     * @return               Pointer to the given value
     * @retval    `nullptr`  No such entry
     */
    const VALUE* get(const VALUE& value) const noexcept;

    /**
     * Returns a reference to the front value.
     *
     * @throw OutOfRange  Map is empty
     * @exceptionSafety   Nothrow
     * @threadsafety      Safe
     */
    VALUE& front() noexcept;

    /**
     * Deletes the oldest value.
     *
     * @throw OutOfRange  Map is empty
     * @threadsafety      Safe
     */
    void pop() noexcept;

    /**
     * Removes a value.
     *
     * @param[in] value   Value to be removed
     * @retval `true`     Entry existed
     * @retval `false`    Entry did not exist
     * @exceptionSafety   Nothrow
     * @threadsafety      Safe
     */
    bool remove(const VALUE& value) noexcept;
};

} // namespace

#endif /* MAIN_MISC_LINKEDHASHMAP_H_ */

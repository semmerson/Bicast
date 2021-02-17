/**
 * Map whose entries are also linked together into a list.
 *
 *        File: LinkedMap.h
 *  Created on: May 13, 2020
 *      Author: Steven R. Emmerson
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

#ifndef MAIN_MISC_LINKEDMAP_H_
#define MAIN_MISC_LINKEDMAP_H_

#include <memory>

namespace hycast {

/**
 * @tparam KEY    Key type. Must have:
 *                  - Default constructor;
 *                  - `operator bool()`, which must return false if and only if
 *                    default constructed; and
 *                  - `operator ==(const Key& rhs)`
 * @tparam VALUE  Value type
 */
template<class KEY, class VALUE>
class LinkedMap
{
    class Impl;

    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    LinkedMap();

    /**
     * Constructs with an initial size.
     *
     * @param[in] initSize  Initial size
     */
    LinkedMap(const size_t initSize);

    /**
     * Returns the number of entries.
     *
     * @return Number of entries
     */
    size_t size() const noexcept;

    /**
     * Indicates if this instance is empty.
     *
     * @retval `true`   This instance is empty
     * @retval `false`  This instance is not empty
     */
    bool empty() const noexcept;

    /**
     * Adds an entry. If a new entry is created, then it is added to the tail of
     * the list.
     *
     * @param[in] key      Key
     * @param[in] value    Value mapped-to by key
     * @return             Pair whose second element is true if the value didn't
     *                     already exist and false, otherwise, and whose first
     *                     element is a reference to the value in the set
     */
    std::pair<VALUE&, bool> add(const KEY& key, VALUE& value);

    /**
     * Returns the value that corresponds to a key.
     *
     * @param[in] key        Key
     * @retval    `nullptr`  No such value
     * @return               Pointer to value. Valid only while no changes are
     *                       made.
     */
    VALUE* find(const KEY& key);

    /**
     * Removes an entry.
     *
     * @param[in] key           Key of entry to be removed
     * @return                  Value associated with key
     * @throws InvalidArgument  No such entry
     */
    VALUE remove(const KEY& key);

    /**
     * Removes the value at the head of the list.
     *
     * @return Value at head of list
     * @throws InvalidArgument  No such entry
     */
    VALUE pop();

    /**
     * Returns the key of the head of the list.
     *
     * @return                  Key of head of list. Will test false if the list
     *                          is empty.
     */
    KEY getHead();

    /**
     * Returns the key of the tail of the list.
     *
     * @return                  Key of tail of list. Will test false if the list
     *                          is empty.
     */
    KEY getTail();
};

} // namespace

#endif /* MAIN_MISC_LINKEDMAP_H_ */

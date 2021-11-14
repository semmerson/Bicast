/**
 * Set whose entries are also linked together into a queue.
 *
 *        File: LinkedSet.h
 *  Created on: August 20, 2020
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

#ifndef MAIN_MISC_LINKEDSET_H_
#define MAIN_MISC_LINKEDSET_H_

#include <memory>

namespace hycast {

/**
 * @tparam T  Value type
 */
template<class T, class COMP, class HASH = std::hash<T>>
class LinkedSet
{
    class Impl;

    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    LinkedSet();

    /**
     * Constructs with an initial size.
     *
     * @param[in] initSize  Initial size
     */
    LinkedSet(const size_t initSize);

    /**
     * Returns the number of entries.
     *
     * @return Number of entries
     */
    size_t size() const noexcept;

    /**
     * Adds an entry. If a new entry is created, then it is added to the tail of
     * the list.
     *
     * @param[in] value  Value to be added
     * @return           Pointer to value in map
     */
    T* add(T& value);

    /**
     * Returns a pointer to a value
     *
     * @param[in] value      Value to find
     * @retval    `nullptr`  Value doesn't exist
     * @retval    `false`    Pointer to existing value
     */
    T* find(const T& value);

    /**
     * Removes a value.
     *
     * @param[in] value         Value to be removed
     * @throws InvalidArgument  No such entry
     */
    T remove(const T& value);

    /**
     * Returns a pointer to the value at the head of the list.
     *
     * @retval `nullptr`  Set is empty
     * @return            Pointer to value at head of list.
     */
    T* getHead();
};

} // namespace

#endif /* MAIN_MISC_LINKEDSET_H_ */

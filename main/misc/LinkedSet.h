/**
 * Set whose entries are also linked together into a list.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: LinkedSet.h
 *  Created on: August 20, 2020
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_LINKEDSET_H_
#define MAIN_MISC_LINKEDSET_H_

#include <memory>

namespace hycast {

/**
 * @tparam VALUE  Value type
 */
template<class VALUE>
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
    VALUE* add(VALUE& value);

    /**
     * Returns a pointer to a value
     *
     * @param[in] value      Value to find
     * @retval    `nullptr`  Value doesn't exist
     * @retval    `false`    Pointer to existing value
     */
    VALUE* find(const VALUE& value);

    /**
     * Removes a value.
     *
     * @param[in] value         Value to be removed
     * @throws InvalidArgument  No such entry
     */
    VALUE remove(const VALUE& value);

    /**
     * Returns a pointer to the value at the head of the list.
     *
     * @retval `nullptr`  Set is empty
     * @return            Pointer to value at head of list.
     */
    VALUE* getHead();
};

} // namespace

#endif /* MAIN_MISC_LINKEDSET_H_ */

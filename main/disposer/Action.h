/**
 * This file declares handle classes for local processing of data-products.
 *
 *  @file:  Action.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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

#ifndef MAIN_DISPOSER_ACTION_H_
#define MAIN_DISPOSER_ACTION_H_

#include "CommonTypes.h"
#include "ProdFile.h"

#include <memory>
#include <vector>

namespace hycast {

/**
 * Base handle class for processing data-products.
 */
class Action
{
public:
    class Impl;

private:
    std::shared_ptr<Impl> pImpl;

protected:
    Action(Impl* const pImpl);

public:
    Action() =default;

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     *
     * @retval `true`  This instance is valid
     * @retval `false` This instance is not valid
     */
    operator bool() const noexcept;

    String to_string() const;

    /**
     * Indicates whether or not this instance should persist between data-products.
     *
     * @retval `true`   This instance should persist
     * @retval `true`   This instance should not persist
     */
    bool shouldPersist() const noexcept;

    /**
     * Returns the hash value of this instance.
     *
     * @return  This instance's hash value
     */
    size_t hash() const noexcept;

    /**
     * Indicates if this instance is considered equal to another.
     *
     * @param[in] rhs      The other instance
     * @retval    `true`   This instance is equal to the other
     * @retval    `false`  This instance is not equal to the other
     */
    bool operator==(const Action& rhs) const noexcept;

    /**
     * Processes data.
     *
     * @param[in] data      The data to be processed
     * @param[in] nbytes    The amount of data in bytes
     * @retval    `true`    Success
     * @retval    `false`   Too many file descriptors are open
     * @throw SystemError   System failure
     */
    bool process(
            const char* data,
            size_t      nbytes);
};

/**
 * Derived handle class for piping a data-product to a decoder. A decoder is a program that reads
 * data-products from its standard input and does something with them.
 */
class PipeAction final : public Action
{
public:
    /**
     * Constructs.
     *
     * @param[in] args      Command-line arguments. The first argument is the pathname of the
     *                      decoder.
     * @param[in] keepOpen  Should the pipe to the decoder be kept open?
     */
    PipeAction(
            const std::vector<String>& args,
            const bool                 keepOpen);
};

/**
 * Derived handle class for writing a data-product into a file. The contents of the file will be
 * overwritten each time.
 */
class FileAction final : public Action
{
public:
    /**
     * Constructs.
     *
     * @param[in] args      Command-line arguments. The first argument is the pathname of the
     *                      decoder.
     * @param[in] keepOpen  Should the pipe to the decoder be kept open?
     */
    FileAction(
            const std::vector<String>& args,
            const bool                 keepOpen);
};

/**
 * Derived handle class for appending a data-product to a file.
 */
class AppendAction final : public Action
{
public:
    /**
     * Constructs.
     *
     * @param[in] args      Command-line arguments. The first argument is the pathname of the
     *                      decoder.
     * @param[in] keepOpen  Should the pipe to the decoder be kept open?
     */
    AppendAction(
            const std::vector<String>& args,
            const bool                 keepOpen);
};

} // namespace

namespace std {
    template<>
    struct hash<Action> {
        size_t operator()(const Action action) const {
            return action.hash();
        }
    };
}

#endif /* MAIN_DISPOSER_ACTION_H_ */

/**
 * A watcher of a directory hierarchy.
 *
 *        File: Watcher.h
 *  Created on: May 4, 2020
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
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

#ifndef MAIN_REPOSITORY_WATCHER_H_
#define MAIN_REPOSITORY_WATCHER_H_

#include "CommonTypes.h"

#include <memory>
#include <string>

namespace bicast {

/**
 * Watches a repository.
 */
class Watcher final
{
protected:
    class                 Impl;
    /// Smart pointer to the implementation
    std::shared_ptr<Impl> pImpl;

    /**
     * Constructs.
     * @param[in] impl  Pointer to an implementation
     */
    Watcher(Impl* const impl);

public:
    /// Interface for the client of a `Watcher`.
    class Client
    {
    public:
        /// Destroys
        virtual ~Client() =default;

        /**
         * Processes a newly-created directory. Called by a Watcher.
         * @param[in] pathname  Absolute pathname of the newly-created directory
         */
        virtual void dirAdded(const String& pathname) =0;

        /**
         * Processes a newly-added, regular file. Called by a Watcher.
         * @param[in] pathname  Absolute pathname of the newly-added, regular file
         */
        virtual void fileAdded(const String& pathname) =0;
    };

    /**
     * Constructs from the pathname of the root of a directory hierarchy to be
     * watched.
     *
     * @param[in] rootDir   Pathname of root directory
     * @param[in] client    Client of this instance
     */
    Watcher(const std::string& rootDir,
            Client&            client);

    /**
     * Starts calling the Client when appropriate.
     * @throw SystemError   Couldn't read the inotify(7) file-descriptor
     * @throw RuntimeError  A watched file-system was unmounted
     */
    void operator()() const;

    /**
     * Adds a directory to be watched if it's not already being watched.
     *
     * @param[in] dirPath          Directory pathname
     * @throws    InvalidArgument  The pathname doesn't reference a directory
     * @throws    SystemError      System failure
     * @threadsafety               Safe
     */
    void tryAdd(const std::string& dirPath) const;
};

} // namespace

#endif /* MAIN_REPOSITORY_WATCHER_H_ */

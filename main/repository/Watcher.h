/**
 * Watches a directory hierarchy for files.
 *
 *        File: Watcher.h
 *  Created on: May 4, 2020
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

#ifndef MAIN_REPOSITORY_WATCHER_H_
#define MAIN_REPOSITORY_WATCHER_H_

#include <memory>
#include <string>

namespace hycast {

/**
 * Watcher of a publisher's repository.
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
    /// A watched-for event
    struct WatchEvent {
        /// Pathname of new file. Will have pathname of root-directory prefix.
        std::string pathname;
    };

    /**
     * Constructs from the pathname of the root of a directory hierarchy to be
     * watched.
     *
     * @param[in] rootDir  Pathname of root directory
     */
    Watcher(const std::string& rootDir);

    /**
     * Returns a watched-for event.
     *
     * @param[out] event  The watched-for event
     * @threadsafety      Compatible but unsafe
     */
    void getEvent(WatchEvent& event);
};

} // namespace

#endif /* MAIN_REPOSITORY_WATCHER_H_ */

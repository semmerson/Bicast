/**
 * Watches a directory hierarchy for files.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Watcher.h
 *  Created on: May 4, 2020
 *      Author: Steven R. Emmerson
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
    std::shared_ptr<Impl> pImpl;

    Watcher(Impl* const impl);

public:
    struct WatchEvent {
        std::string pathname; ///< Pathname of a new file
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
     * @param[out] watchEvent  The watched-for event
     * @threadsafety           Compatible but unsafe
     */
    void getEvent(WatchEvent& event);
};

} // namespace

#endif /* MAIN_REPOSITORY_WATCHER_H_ */

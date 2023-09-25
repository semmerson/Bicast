/**
 * A thread-safe watcher of a directory hierarchy.
 *
 *        File: Watcher.cpp
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

#include "config.h"

#include "CommonTypes.h"
#include "error.h"
#include "FileUtil.h"
#include "Watcher.h"

#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <queue>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

namespace hycast {

/// An implementation of a Watcher
class Watcher::Impl final
{
    using IntToStringMap = std::unordered_map<int, String>;
    using StringSet      = std::unordered_set<String>;

    static constexpr size_t BUFSIZE = 100*(sizeof(struct inotify_event)+NAME_MAX+1);

    mutable Mutex     mutex;        ///< For thread safety
    const String      rootDir;      ///< Root directory of watched hierarchy
    Client&           client;       ///< Client of the watcher
    const int         fd ;          ///< inotify(7) file-descriptor
    IntToStringMap    wdToDirs;     ///< Watch descriptors to directory pathnames map
    StringSet         watchedDirs;  ///< Pathnames of watched directories
    alignas(struct inotify_event)
    char              buf[BUFSIZE]; ///< inotify(7) event-buffer
    char*             nextEvent;    ///< Next event to access in event-buffer
    char*             endEvent;     ///< End of events in event-buffer

    /**
     * Indicates if a pathname references a directory, either directly or via symbolic links. NB:
     * both "." and ".." return true.
     *
     * @param[in] pathname  Pathname to examine
     * @retval    true      Pathname references directory
     * @retval    false     Pathname doesn't reference directory
     * @threadsafety        Safe
     */
    bool isDir(const String& pathname)
    {
        struct stat stat;

        if (::stat(pathname.data(), &stat)) // Follow symlinks
            throw SYSTEM_ERROR("Couldn't stat() \"" + pathname + "\"");

        return S_ISDIR(stat.st_mode);
    }

    /**
     * Indicates if a pathname is a symbolic link or hard link.
     *
     * @param[in] pathname  Pathname to examine
     * @retval    true      Pathname is a symbolic or hard link
     * @retval    false     Pathname is not a symbolic or hard link
     * @threadsafety        Safe
     */
    bool isLink(const String& pathname)
    {
        struct stat stat;

        if (::lstat(pathname.data(), &stat)) // lstat() won't follow a symlink
            throw SYSTEM_ERROR("Couldn't lstat() \"" + pathname + "\"");

        return S_ISLNK(stat.st_mode) || (stat.st_nlink > 1);
    }

    /**
     * Determines if a file descriptor is ready for reading. Blocks.
     *
     * @retval true         File descriptor is ready for reading
     * @retval false        File descriptor is closed
     * @throw SystemError   poll(2) failure
     */
    inline bool hasInput(const int fd) {
        struct pollfd pollfd = {};
        pollfd.fd = fd;
        pollfd.events = POLLIN;

        if (::poll(&pollfd, 1, -1) == -1) // Blocks
            throw SYSTEM_ERROR("poll() failure on file descriptor %d", fd);
        if (pollfd.revents & POLLHUP) {
            LOG_TRACE("EOF on file descriptor %d", fd);
            return false; // EOF
        }
        if ((pollfd.revents & (POLLIN | POLLERR)) == 0)
            throw SYSTEM_ERROR("poll() failure on file descriptor %d", fd);

        ssize_t nbytes = ::read(fd, buf, sizeof(buf)); // Won't block

        if (nbytes == -1)
            throw SYSTEM_ERROR("Couldn't read inotify(7) file descriptor");

        if (nbytes == 0) {
            LOG_TRACE("EOF on inotify(7) file descriptor");
            return false; // EOF
        }

        return true;
    }

    /**
     * Blocks.
     *
     * @retval true         Success
     * @retval false        inotify(7) file descriptor has been closed
     * @throw SystemError   poll(2) failure
     * @throw SystemError   Couldn't read inotify(7) file-descriptor
     */
    bool readEvents() {
        // if (hasInput(fd))
        {
            ssize_t nbytes = ::read(fd, buf, sizeof(buf)); // Won't block

            if (nbytes == -1)
                throw SYSTEM_ERROR("Couldn't read inotify(7) file descriptor");

            if (nbytes == 0) {
                LOG_TRACE("EOF on inotify(7) file descriptor");
                return false; // EOF
            }

            nextEvent = buf;
            endEvent = buf + nbytes;

            return true;
        }
    }

    /**
     * Processes an inotify(7) event. Calls the client if the event is recognized; otherwise, does
     * nothing.
     *
     * @param[in] event         Pointer to the inotify(7) event
     * @throws    RuntimeError  A watched file-system was unmounted
     */
    inline void processEvent(struct inotify_event* event)
    {
        const auto wd = event->wd;
        const auto mask = event->mask;

        if (mask & IN_UNMOUNT)
            throw RUNTIME_ERROR("Watched file-system was unmounted");
        if (mask & IN_Q_OVERFLOW) {
            LOG_WARN("Inotify(7) event-queue overflowed");
        }
        else {
            const auto pathname = FileUtil::pathname(wdToDirs.at(wd), event->name);

            if (mask & IN_ISDIR) {
                if (mask & IN_CREATE) {
                    client.dirAdded(pathname);
                }
                else if (mask & IN_DELETE) {
                    LOG_DEBUG("Unwatching directory \"" + pathname + '"');
                    wdToDirs.erase(wd);
                    watchedDirs.erase(pathname);
                    ::inotify_rm_watch(fd, wd);
                }
            }
            else {
                const uint32_t maskTemplate = isLink(pathname) ? IN_CREATE : IN_CLOSE_WRITE;

                if (mask & maskTemplate)
                    client.fileAdded(pathname);
            }
        }
    }

    /**
     * Processes inotify(7) events in the input buffer
     * @throw RuntimeError  A watched file-system was unmounted
     */
    void processEvents() {
        while (nextEvent < endEvent) {
            struct inotify_event* event = reinterpret_cast<struct inotify_event*>(nextEvent);

            processEvent(event);
            nextEvent += sizeof(struct inotify_event) + event->len;
        } // While event-buffer needs processing
    }

public:
    /**
     * Constructs from the root directory to be watched.
     *
     * @param[in] rootDir          Pathname of root directory
     * @param[in] client           Client of the watcher
     * @throws    SystemError      `inotify_init()` failure
     * @throws    InvalidArgument  The pathname doesn't reference a directory
     * @throws    SystemError      Couldn't set `inotify_init(2)` file-descriptor to close-on-exec
     * @throws    SystemError      Couldn't open directory
     */
    Impl(   const String& rootDir,
            Client&       client)
        : mutex()
        , rootDir(rootDir)
        , client(client)
        , fd(::inotify_init())
        , wdToDirs()
        , watchedDirs()
        , buf()
        , nextEvent(buf)
        , endEvent(nextEvent)
    {
        if (fd == -1)
            throw SYSTEM_ERROR("inotify_init() failure");

        if (::fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
            throw SYSTEM_ERROR("Couldn't set inotify(7) file-descriptor to close-on-exec");
    }

    ~Impl() noexcept
    {
        (void)::close(fd);
    }

    /**
     * Starts calling the Client when appropriate.
     */
    void operator()() {
        for (;;) {
            readEvents(); // Blocks
            processEvents();
        }
    }

    /**
     * Adds a directory to be watched if it's not already being watched.
     *
     * @param[in] dirPath          Directory pathname
     * @throws    InvalidArgument  The pathname doesn't reference a directory
     * @throws    SystemError      System failure
     * @threadsafety               Safe
     */
    void tryAdd(const String& dirPath) {
        Guard guard{mutex};

        if (watchedDirs.count(dirPath) == 0) {
            if (!isDir(dirPath))
                throw INVALID_ARGUMENT("\"" + dirPath + "\" is not a directory");

            // The directory is not being watched

            /*
             * Watch for
             *   - Link-creation;
             *   - Being renamed to a watched directory
             *   - Closing of regular file; and
             *   - File deletion (including directories)
             */
            const int wd = ::inotify_add_watch(fd, dirPath.data(),
                    IN_CLOSE_WRITE|IN_MOVED_TO|IN_CREATE|IN_DELETE);
            if (wd == -1)
                throw SYSTEM_ERROR("Couldn't watch directory \"" + dirPath + "\"");
            LOG_DEBUG("Added directory \"" + dirPath + "\"");

            wdToDirs[wd] = dirPath;
            watchedDirs.insert(dirPath);
        }
    }
};

/******************************************************************************/

Watcher::Watcher(
        const String& rootDir,
        Client&       client)
    : pImpl(new Impl(rootDir, client))
{}

void Watcher::operator()() const
{
    pImpl->operator()();
}

void Watcher::tryAdd(const String& dirPath) const
{
    pImpl->tryAdd(dirPath);
}

} // namespace

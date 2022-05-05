/**
 * Watcher of a publisher's directory hierarchy.
 *
 *        File: Watcher.cpp
 *  Created on: May 4, 2020
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

#include "config.h"

#include "error.h"
#include "Watcher.h"

#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <queue>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

namespace hycast {

class Watcher::Impl final
{
    using PathMap   = std::unordered_map<int, std::string>;
    using WdMap     = std::unordered_map<std::string, int>;
    using PathQueue = std::queue<std::string>;
    using PathSet   = std::unordered_set<std::string>;
    using FilesMap  = std::unordered_map<int, PathSet>;

    std::string rootDir;      ///< Root directory of watched hierarchy
    int         fd ;          ///< inotify(7) file-descriptor
    PathMap     dirPaths;     ///< Pathnames of watched directories
    WdMap       wds;          ///< inotify(7) watch descriptors
    FilesMap    scannedFiles; ///< Regular files found by scanning
    PathQueue   regFiles;     ///< Queue of pre-existing but new regular files
    /// inotify(7) event-buffer
    union {
        struct inotify_event event; ///< For alignment
        char                 buf[100*(sizeof(struct inotify_event)+NAME_MAX+1)];
    }           eventBuf;
    char*       nextEvent;    ///< Next event to access in event-buffer
    char*       endEvent;     ///< End of events in event-buffer

    /**
     * Indicates if a pathname references a directory, either directly or via
     * symbolic links. NB: both "." and ".." return true.
     *
     * @param[in] pathname  Pathname to examine
     * @retval    `true`    Pathname references directory
     * @retval    `false`   Pathname doesn't reference directory
     * @threadsafety        Safe
     */
    bool isDir(const std::string& pathname)
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
     * @retval    `true`    Pathname is a symbolic or hard link
     * @retval    `false`   Pathname is not a symbolic or hard link
     * @threadsafety        Safe
     */
    bool isLink(const std::string& pathname)
    {
        struct stat stat;

        if (::lstat(pathname.data(), &stat)) // lstat() won't follow a symlink
            throw SYSTEM_ERROR("Couldn't lstat() \"" + pathname + "\"");

        return S_ISLNK(stat.st_mode) || (stat.st_nlink > 1);
    }

    /**
     * Initializes watching a directory if the directory isn't already being watched. Scans a
     * watched directory for regular files and adds them if appropriate. Recursively descends into
     * sub-directories.
     *
     * @param[in] dirPath          Directory pathname
     * @param[in] addRegFiles      Add regular files encountered to `regFiles`?
     * @throws    InvalidArgument  The pathname doesn't reference a directory
     * @throws    SystemError      System failure
     * @threadsafety               Compatible but unsafe
     */
    void watchIfNew(
            const std::string& dirPath,
            const bool         addRegFiles = false)
    {
        if (wds.count(dirPath) == 0) {
            if (!isDir(dirPath))
                throw INVALID_ARGUMENT("\"" + dirPath + "\" is not a directory");

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

            dirPaths[wd] = dirPath;
            wds[dirPath] = wd;

            try {
                DIR* const dirStream = ::opendir(dirPath.data());

                if (dirStream == nullptr)
                    throw SYSTEM_ERROR(std::string("Couldn't open directory \"") + dirPath + "\"");
                try {
                    struct dirent  fileEntry;
                    struct dirent* entry = &fileEntry;

                    for (;;) {
                        int status = ::readdir_r(dirStream, entry, &entry);

                        if (status)
                            throw SYSTEM_ERROR("readdir_r() failure", status);
                        if (entry == NULL)
                            break; // End of directory stream

                        if (::strcmp(entry->d_name, ".") && ::strcmp(entry->d_name, "..")) {
                            const auto pathname = dirPath + "/" + entry->d_name;

                            if (isDir(pathname)) {
                                LOG_DEBUG("Watching directory \"%s\"", pathname.data());
                                watchIfNew(pathname, addRegFiles);
                            }
                            else if (addRegFiles) {
                                LOG_DEBUG("Pushing regular file \"%s\"", pathname.data());
                                regFiles.push(pathname);
                                scannedFiles[wd].insert(pathname);
                            }
                        }
                    }

                    ::closedir(dirStream);
                } // `dirStream` is open
                catch (const std::exception& ex) {
                    ::closedir(dirStream);
                    throw;
                }
            } // `wd`, `dirPaths[wd]`, and `wds[dir]` are set
            catch (const std::exception& ex) {
                scannedFiles.erase(wd);
                dirPaths.erase(wd);
                wds.erase(dirPath);
                (void)inotify_rm_watch(fd, wd);
                throw;
            }
        }
    }

    /**
     * Blocks.
     *
     * @throws SystemError  Couldn't read inotify(7) file-descriptor
     */
    void readEvents() {
        ssize_t nbytes = ::read(fd, eventBuf.buf, sizeof(eventBuf)); // Blocks

        if (nbytes == -1)
            throw SYSTEM_ERROR("Couldn't read inotify(7) file-descriptor");

        nextEvent = eventBuf.buf;
        endEvent = eventBuf.buf + nbytes;
    }

    /**
     * Indicates if a regular file was added because its parent directory was scanned.
     *
     * @param wd        Associated watch descriptor
     * @param pathname  Pathname of the file
     * @retval `true`   File was added because it was scanned
     * @retval `false`  File was not added because it was scanned
     * @see `watch()`
     */
    inline bool isScannedFile(
            const int          wd,
            const std::string& pathname) {
        return scannedFiles.count(wd) && scannedFiles.at(wd).count(pathname);
    }

    /**
     * Processes an event. Does nothing if the event isn't recognized.
     *
     * @param[in] wd        Event's watch descriptor
     * @param[in] mask      Event's mask
     * @param[in] pathname  Event's pathname
     */
    inline void processEvent(
            const int          wd,
            const uint32_t     mask,
            const std::string& pathname)
    {
        if (mask & IN_ISDIR) {
            if (mask & IN_CREATE) {
                LOG_DEBUG("Watching directory \"%s\"", pathname.data());
                watchIfNew(pathname, true); // Might add to `regFiles`
            }
            else if (mask & IN_DELETE) {
                LOG_DEBUG("Unwatching directory \"%s\"", pathname.data());
                const auto wd = wds.at(pathname); // Eclipses parameter `wd`
                if (scannedFiles.count(wd)) {
                    auto& pathnames = scannedFiles.at(wd);
                    if (pathnames.erase(pathname) && pathnames.empty())
                        scannedFiles.erase(wd);
                }
                dirPaths.erase(wd);
                wds.erase(pathname);
                ::inotify_rm_watch(fd, wd);
            }
        }
        else if (isLink(pathname)) {
            if ((mask & IN_CREATE) && !isScannedFile(wd, pathname)) {
                LOG_DEBUG("Pushing link \"%s\"", pathname.data());
                regFiles.push(pathname);
            }
        }
        else if ((mask & IN_CLOSE_WRITE) && !isScannedFile(wd, pathname)) {
            LOG_DEBUG("Pushing regular file \"%s\"", pathname.data());
            regFiles.push(pathname);
        }
    }

    /**
     * @throws       RuntimeError  A watched file-system was unmounted
     * @throws       RuntimeError  The inotify(7) event-queue overflowed
     */
    void processEvents() {
        while (nextEvent < endEvent) {
            struct inotify_event* event = reinterpret_cast<struct inotify_event*>(nextEvent);
            nextEvent += sizeof(struct inotify_event) + event->len;

            const auto            mask = event->mask;

            if (mask & IN_UNMOUNT)
                throw RUNTIME_ERROR("Watched file-system was unmounted");
            if (mask & IN_Q_OVERFLOW)
                throw RUNTIME_ERROR("Inotify(7) event-queue overflowed");

            const auto  wd = event->wd;
            const auto& pathname = dirPaths.at(wd) + "/" + event->name;
            processEvent(wd, mask, pathname);
        } // While event-buffer needs processing
    }

public:
    /**
     * Constructs from the root directory to be watched.
     *
     * @param[in] rootDir          Pathname of root directory
     * @throws    SystemError      `inotify_init()` failure
     * @throws    InvalidArgument  The pathname doesn't reference a directory
     * @throws    SystemError      Couldn't set `inotify_init(2)` file-descriptor to close-on-exec
     * @throws    SystemError      Couldn't open directory
     */
    Impl(const std::string& rootDir)
        : rootDir(rootDir)
        , fd(::inotify_init())
        , dirPaths()
        , wds()
        , scannedFiles()
        , regFiles()
        , nextEvent(eventBuf.buf)
        , endEvent(nextEvent)
    {
        if (fd == -1)
            throw SYSTEM_ERROR("inotify_init() failure");

        if (::fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
            throw SYSTEM_ERROR("Couldn't set inotify(7) file-descriptor to close-on-exec");

        LOG_DEBUG("Watching directory %s", rootDir.data());
        watchIfNew(rootDir, true);
    }

    ~Impl() noexcept
    {
        (void)::close(fd);
    }

    /**
     * Returns a watched-for event.  Reads the `inotify(7)` file-descriptor.
     * Recurses into new directories. Follows symbolic links. Blocks.
     *
     * @param[out] watchEvent  The watched-for event
     * @threadsafety           Compatible but unsafe
     * @throws       SystemError   Couldn't read inotify(7) file-descriptor
     * @throws       RuntimeError  A watched file-system was unmounted
     * @throws       RuntimeError  The inotify(7) event-queue overflowed
     */
    void getEvent(WatchEvent& watchEvent)
    {
        while (regFiles.empty()) {
            readEvents(); // Blocks
            processEvents();
        }

        watchEvent.pathname = regFiles.front();
        regFiles.pop();
        LOG_DEBUG("Returning pathname \"%s\"", watchEvent.pathname.data());
    }
};

/******************************************************************************/

Watcher::Watcher(const std::string& rootDir)
    : pImpl{new Impl(rootDir)}
{}

void Watcher::getEvent(WatchEvent& watchEvent)
{
    pImpl->getEvent(watchEvent);
}

} // namespace

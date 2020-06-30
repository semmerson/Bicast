/**
 * Watcher of a publisher's directory hierarchy.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Watcher.cpp
 *  Created on: May 4, 2020
 *      Author: Steven R. Emmerson
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

namespace hycast {

class Watcher::Impl final
{
    typedef std::unordered_map<int, std::string> PathMap;
    typedef std::unordered_map<std::string, int> WdMap;
    typedef std::queue<std::string>              PathQueue;

    std::string rootDir;  ///< Root directory of watched hierarchy
    int         fd ;      ///< inotify(7) file-descriptor
    PathMap     dirPaths; ///< Pathnames of watched directories
    WdMap       wds;      ///< inotify(7) watch descriptors
    PathQueue   regFiles; ///< Queue of pre-existing but new regular files
    /// inotify(7) event-buffer
    union {
        struct inotify_event event; ///< For alignment
        char                 buf[10*(sizeof(struct inotify_event)+NAME_MAX+1)];
    }           eventBuf;
    char*       nextEvent;   ///< Next event to access in event-buffer
    char*       endEvent;    ///< End of events in event-buffer

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

        return (stat.st_mode & S_IFMT) == S_IFDIR;
    }

    /**
     * Indicates if a pathname is a symbolic link or hard link.
     *
     * @param[in] pathname  Pathname to examine
     * @retval    `true`    Pathname is link
     * @retval    `false`   Pathname is not link
     * @threadsafety        Safe
     */
    bool isLink(const std::string& pathname)
    {
        struct stat stat;

        if (::lstat(pathname.data(), &stat)) // Don't follow symlinks
            throw SYSTEM_ERROR("Couldn't lstat() \"" + pathname + "\"");

        return ((stat.st_mode & S_IFMT) == S_IFLNK) ||
                (stat.st_nlink > 1);
    }

    /**
     * Initializes watching a directory hierarchy. Recursively descends into
     * sub-directories.
     *
     * @param[in] dirPath      Directory pathname
     * @param[in] addRegFiles  Add regular files encountered to `regFiles`?
     * @throws    SystemError  System failure
     * @threadsafety           Unsafe
     */
    void watch(
            const std::string& dirPath,
            const bool         addRegFiles = false)
    {
        /*
         * Watch for
         *   - File renaming;
         *   - Link-creation;
         *   - Closing of regular file; and
         *   - Directory deletion.
         */
        int wd = ::inotify_add_watch(fd, dirPath.data(),
                IN_CLOSE_WRITE|IN_MOVED_TO|IN_CREATE|IN_DELETE_SELF);
        if (wd == -1)
            throw SYSTEM_ERROR("Couldn't watch directory \"" + dirPath + "\"");

        dirPaths[wd] = dirPath;
        wds[dirPath] = wd;

        try {
            DIR* const dirStream = ::opendir(dirPath.data());

            if (dirStream == NULL)
                throw SYSTEM_ERROR(std::string("Couldn't open directory \"") +
                        dirPath + "\"");
            try {
                for (struct dirent* fileEntry;;) {
                    errno = 0; // readdir() returns NULL on both EOF and error
                    if ((fileEntry = ::readdir(dirStream)) == NULL)
                        break;

                    if (::strcmp(fileEntry->d_name, ".") &&
                            ::strcmp(fileEntry->d_name, "..")) {
                        const std::string pathname(dirPath + "/" +
                                fileEntry->d_name);

                        if (isDir(pathname)) {
                            watch(pathname, addRegFiles);
                        }
                        else if (addRegFiles) {
                            regFiles.push(pathname);
                        }
                    }
                }
                if (errno)
                    throw SYSTEM_ERROR("readdir() failure");

                ::closedir(dirStream);
            } // `stream` is open
            catch (const std::exception& ex) {
                ::closedir(dirStream);
                throw;
            }
        } // `wd`, `dirpaths[wd]`, and `wds[dir]` are set
        catch (const std::exception& ex) {
            (void)inotify_rm_watch(fd, wd);
            dirPaths.erase(wd);
            wds.erase(dirPath);
            throw;
        }
    }

    /**
     * Returns the pathname of the next next link or closed regular-file if
     * appropriate. Reads the `inotify(7)` file-descriptor if necessary. If the
     * next file is a directory, then `watch()` is called and pre-existing links
     * and regular files are added to `regFiles`. Follows symbolic links. The
     * returned pathname will have the pathname given to the constructor as a
     * prefix.
     *
     * @param[out] filePath  Pathname of link or closed, regular file
     * @retval     `true`    Next event satisfied criteria. `filePath` is set.
     * @retval     `false`   Next event didn't satisfy criteria. Try again.
     *                       `filePath` is not set.
     * @threadsafety         Unsafe
     */
    bool getNextEventPath(std::string& filePath)
    {
        if (nextEvent >= endEvent) {
            // Blocks
            ssize_t nbytes = ::read(fd, eventBuf.buf, sizeof(eventBuf));

            if (nbytes == -1)
                throw SYSTEM_ERROR("Couldn't read inotify file-descriptor");

            nextEvent = eventBuf.buf;
            endEvent = eventBuf.buf + nbytes;
        }

        struct inotify_event* event =
                reinterpret_cast<struct inotify_event*>(nextEvent);
        nextEvent += sizeof(struct inotify_event) + event->len;

        if (event->mask & IN_UNMOUNT)
            throw RUNTIME_ERROR("Watched file-system was unmounted");
        if (event->mask & IN_Q_OVERFLOW)
            throw RUNTIME_ERROR("Inotify(7) event-queue overflowed");

        const std::string pathname = dirPaths.at(event->wd) + "/" + event->name;
        bool              success = false; // true => link or closed reg file

        if (event->mask & IN_DELETE_SELF) { // Only directories are watched
            inotify_rm_watch(fd, event->wd);
            dirPaths.erase(event->wd);
            wds.erase(pathname);
        }
        else if (isDir(pathname)) {
            watch(pathname, true); // Might add to `regFiles`
        }
        else if (isLink(pathname)
                ? (event->mask & IN_CREATE)
                : (event->mask & IN_CLOSE_WRITE)) {
            // `pathname` is link or closed regular file
            filePath = pathname;
            success = true;
        }

        return success;
    }

public:
    /**
     * Constructs from the root directory to be watched.
     *
     * @param[in] rootDir      Pathname of root directory
     * @throws    SystemError  `inotify_init()` failure
     * @throws    SystemError  Couldn't set `inotify_init(2)` file-descriptor to
     *                         close-on-exec
     * @throws    SystemError  Couldn't open directory
     */
    Impl(const std::string& rootDir)
        : rootDir(rootDir)
        , fd(::inotify_init())
        , dirPaths()
        , wds()
        , regFiles()
        , nextEvent(eventBuf.buf)
        , endEvent(nextEvent)
    {
        if (fd == -1)
            throw SYSTEM_ERROR("inotify_init() failure");

        if (::fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
            throw SYSTEM_ERROR("Couldn't set inotify(7) file-descriptor to "
                    "close-on-exec");

        watch(rootDir);
    }

    ~Impl() noexcept
    {
        (void)::close(fd);
    }

    /**
     * Returns a watched-for event. Blocks.
     *
     * @param[out] watchEvent  The watched-for event
     * @threadsafety           Compatible but unsafe
     */
    void getEvent(WatchEvent& watchEvent)
    {
        for (;;) {
            if (!regFiles.empty()) {
                watchEvent.pathname = regFiles.front();
                regFiles.pop();
                break;
            }
            else if (getNextEventPath(watchEvent.pathname)) {
                break;
            }
        }
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

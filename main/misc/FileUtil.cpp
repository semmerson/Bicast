/**
 * Utility functions for files.
 *
 *        File: FileUtil.cpp
 *  Created on: Jan 2, 2020
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

#include "config.h"

#include "CommonTypes.h"
#include "FileUtil.h"

#include "error.h"

#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace hycast {

bool FileUtil::isAbsolute(const String& pathname)
{
    return pathname.at(0) == '/';
}

String FileUtil::makeAbsolute(const String& pathname)
{
    if (pathname.size() && pathname.at(0) == '/')
        return String(pathname);

    char cwd[PATH_MAX];
    return String(::getcwd(cwd, sizeof(cwd))) + "/" + pathname;
}

String FileUtil::filename(const String& pathname) noexcept
{
    char buf[PATH_MAX];
    ::strncpy(buf, pathname.data(), sizeof(buf))[sizeof(buf)-1] = 0;
    return String(::basename(buf));
}

String FileUtil::ensureTrailingSep(const String& pathname)
{
    if (pathname.size() == 0)
        return pathname;

    return pathname.back() == separator
            ? pathname
            : pathname + separator;
}

String FileUtil::pathname(
        const String& dirPath,
        const String& filename)
{
    if (dirPath.size() == 0)
        return filename;

    return dirPath[dirPath.size()-1] == separator
            ? dirPath + filename
            : dirPath + separator + filename;
}

String FileUtil::dirname(const String& pathname) noexcept
{
    // Workaround `dirname()` not being re-entrant
    char buf[PATH_MAX];
    ::strncpy(buf, pathname.data(), sizeof(buf))[sizeof(buf)-1] = 0;
    return String(::dirname(buf));
}

void FileUtil::trimPathname(String& pathname) noexcept
{
    pathname = dirname(pathname) + '/' +  filename(pathname);
}

bool FileUtil::exists(const String& pathname) noexcept
{
    struct ::stat    statBuf;

    return ::stat(pathname.data(), &statBuf) == 0;
}

size_t FileUtil::getSize(const String& pathname)
{
    struct ::stat    statBuf;

    if (::stat(pathname.data(), &statBuf))
        throw SYSTEM_ERROR(String("stat() failure on \"") + pathname + "\"");

    return statBuf.st_size;
}

struct ::stat& FileUtil::getStat(
        const int      rootFd,
        const String&  pathname,
        struct ::stat& statBuf,
        const bool     follow)
{
    if (fstatat(rootFd, pathname.data(), &statBuf, follow ? 0 : AT_SYMLINK_NOFOLLOW))
        throw SYSTEM_ERROR("Couldn't fstatat() file \"" + pathname + "\"");
    return statBuf;
}

struct ::stat& FileUtil::getStat(
        const String&  pathname,
        struct ::stat& statBuf,
        const bool     follow)
{
    if (::fstatat(AT_FDCWD, pathname.data(), &statBuf, follow ? 0 : AT_SYMLINK_NOFOLLOW))
        throw SYSTEM_ERROR("::fstatat() failure on file \"" + pathname + "\"");
    return statBuf;
}

void FileUtil::getStat(
        const int      rootFd,
        const String&  pathname,
        struct ::stat& statBuf)
{
    int status = fstatat(rootFd, pathname.data(), &statBuf, 0); // NB: Follows symlinks
    if (status == -1)
        throw SYSTEM_ERROR("Couldn't fstatat() file \"" + pathname + "\"");
}

struct ::stat FileUtil::getStat(
        const int     rootFd,
        const String& pathname)
{
    struct ::stat statBuf;
    getStat(rootFd, pathname, statBuf, true);
    return statBuf;
}

void FileUtil::setOwnership(
        const String& pathname,
        const uid_t   uid,
        const gid_t   gid)
{
    if (::chown(pathname.data(), uid, gid))
        throw SYSTEM_ERROR("::chown() failure on file \"" + pathname + "\"");
}

void FileUtil::setProtection(
        const String& pathname,
        const mode_t  protMask)
{
    if (::chmod(pathname.data(), protMask))
        throw SYSTEM_ERROR("::chmod() failure on file \"" + pathname + "\"");
}

SysTimePoint FileUtil::getModTime(
        const int     dirFd,
        const String& pathname,
        const bool    follow)
{
    struct ::stat statBuf;
    getStat(dirFd, pathname, statBuf, follow);
    return SysClock::from_time_t(statBuf.st_mtim.tv_sec) +
            std::chrono::nanoseconds(statBuf.st_mtim.tv_nsec);
}

SysTimePoint FileUtil::getModTime(
        const String& pathname,
        const bool    follow)
{
    struct ::stat statBuf;
    FileUtil::getStat(pathname, statBuf, follow);
    return SysClock::from_time_t(statBuf.st_mtim.tv_sec) +
            std::chrono::nanoseconds(statBuf.st_mtim.tv_nsec);
}

void FileUtil::setModTime(
        const int           rootFd,
        const String&       pathname,
        const SysTimePoint& modTime,
        const bool          followSymLinks) {
    struct timespec times[2];
    times[0].tv_nsec = UTIME_OMIT;
    times[1].tv_sec = SysClock::to_time_t(modTime);
    times[1].tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(
            modTime - SysClock::from_time_t(times[1].tv_sec)).count();
    if (::utimensat(rootFd, pathname.data(), times, followSymLinks ? 0 : AT_SYMLINK_NOFOLLOW))
        throw SYSTEM_ERROR("::utimensat() failure");
}

void FileUtil::setModTime(
        const String&       pathname,
        const SysTimePoint& modTime,
        const bool          followSymLinks) {
    setModTime(AT_FDCWD, pathname, modTime, followSymLinks);
}

off_t FileUtil::getFileSize(const String& pathname)
{
    struct ::stat statBuf;
    return getStat(pathname, statBuf, true).st_size;
}

off_t FileUtil::getFileSize(
        const int     rootFd,
        const String& pathname)
{
    return getStat(rootFd, pathname).st_size;
}

void FileUtil::rename(
        const int     rootFd,
        const String& oldPathname,
        const String& newPathname)
{
    if (::renameat(rootFd, oldPathname.data(), rootFd, newPathname.data()))
        throw SYSTEM_ERROR("::renameat() failure: from=\"" + oldPathname + "\", to=\"" +
                newPathname + "\"");
}

const String& FileUtil::ensureDir(
        const String& pathname,
        const mode_t  mode)
{
    struct ::stat statBuf;

    if (::stat(pathname.data(), &statBuf)) {
        if (errno != ENOENT)
            throw SYSTEM_ERROR(String("stat() failure on \"") + pathname + "\"");

        ensureDir(dirname(pathname), mode);

        if (::mkdir(pathname.data(), mode))
            throw SYSTEM_ERROR(String("mkdir() failure on \"") + pathname + "\"");
    }

    return pathname;
}

const String& FileUtil::ensureDir(
        const int     fd,
        const String& pathname,
        const mode_t  mode)
{
    struct ::stat statBuf;

    if (::fstatat(fd, pathname.data(), &statBuf, 0)) {
        if (errno != ENOENT)
            throw SYSTEM_ERROR(String("fstatat() failure on \"") + pathname + "\"; fd=" +
                    std::to_string(fd));

        ensureDir(fd, dirname(pathname), mode);

        if (::mkdirat(fd, pathname.data(), mode))
            throw SYSTEM_ERROR(String("mkdir() failure on \"") + pathname + "\"");
    }

    return pathname;
}

void FileUtil::ensureParent(
        const String& pathname,
        const mode_t  mode)
{
    ensureDir(dirname(pathname), mode);
}

void FileUtil::hardLink(
        const String& extantPath,
        const String& linkPath)
{
    if (::link(extantPath.data(), linkPath.data()))
        throw SYSTEM_ERROR("::link() failure from \"" + linkPath + "\" to \"" + linkPath + "\"");
}

void FileUtil::rmDirTree(const String& dirPath)
{
    DIR* dir = ::opendir(dirPath.data());

    if (dir) {
        try {
            struct dirent  entry;
            struct dirent* result;
            int            status;

            for (status = ::readdir_r(dir, &entry, &result);
                    status == 0 && result != nullptr;
                    status = ::readdir_r(dir, &entry, &result)) {
                const char* name = entry.d_name;

                if (::strcmp(".", name) && ::strcmp("..", name)) {
                    const String  subName = dirPath + "/" + name;
                    struct ::stat statBuf;

                    getStat(subName, statBuf, false); // Don't follow symbolic links

                    if (S_ISDIR(statBuf.st_mode)) {
                        rmDirTree(subName);
                    }
                    else if (::unlink(subName.data())) {
                        throw SYSTEM_ERROR("Couldn't delete file \"" + subName + "\"");
                    }
                }
            }
            if (status && status != ENOENT)
                throw SYSTEM_ERROR("Couldn't read directory \"" + dirPath + "\"");

            ::closedir(dir);
            dir = nullptr;

            struct stat statBuf;
            getStat(dirPath, statBuf, false);
            if (S_ISDIR(statBuf.st_mode)) {
                // Not a symbolic link
                if (::rmdir(dirPath.data()))
                    throw SYSTEM_ERROR("Couldn't delete directory \"" + dirPath + "\"");
            }
        } // `dir` is set
        catch (...) {
            if (dir != nullptr)
                ::closedir(dir);
            throw;
        }
    }
}

void FileUtil::pruneEmptyDirPath(
        const String& rootPathname,
        const String& dirPathname)
{
    struct ::stat statBuf;
    const auto    rootInode = getStat(rootPathname, statBuf, false).st_ino;
    String   dirPath(dirPathname);

    for (;;) {
        getStat(dirPath, statBuf, false);

        if (statBuf.st_ino == rootInode)
            break; // Stop because at root directory

        if (!S_ISDIR(statBuf.st_mode))
            // Must be a symbolic link. Should first be removed by `removeFileAndPrune()` instead.
            break;

        auto status = ::rmdir(dirPath.data());
        if (status) {
            if (status == EEXIST || status == ENOTEMPTY)
                break; // Stop because directory isn't empty
            throw SYSTEM_ERROR(String("::rmdir() failure on directory \"") + dirPath + "\"");
        }

        dirPath = dirname(dirPath); // Repeat with parent directory
    }
}

void FileUtil::pruneEmptyDirPath(
        const int fd,
        String&&  dirPath)
{
    struct ::stat statBuf;
    if (::fstat(fd, &statBuf))
        throw SYSTEM_ERROR("fstat() failure on file-descriptor " + std::to_string(fd));
    const auto rootInode = statBuf.st_ino;

    for (;;) {
        if (::fstatat(fd, dirPath.data(), &statBuf, AT_SYMLINK_NOFOLLOW))
            throw SYSTEM_ERROR("fstatat() failure on fd-relative directory \"" + dirPath + "\"");

        if (statBuf.st_ino == rootInode)
            break; // Stop because at root directory

        auto status = unlinkat(fd, dirPath.data(), AT_REMOVEDIR);
        if (status) {
            if (status == EEXIST || status == ENOTEMPTY)
                break; // Stop because directory isn't empty
            throw SYSTEM_ERROR(String("unlinkat() failure on fd-relative directory \"") +
                    dirPath + "\"");
        }
        else {
            dirPath = dirname(dirPath); // Repeat with parent directory
        }
    }
}

void FileUtil::removeFileAndPrune(
        const String& rootPathname,
        const String& pathname)
{
    if (::unlink(pathname.data()))
        throw SYSTEM_ERROR("::unlink() failure on file \"" + pathname + "\"");

    pruneEmptyDirPath(rootPathname, dirname(pathname));
}

void FileUtil::removeFileAndPrune(
        const int     fd,
        const String& pathname)
{
    if (::unlinkat(fd, pathname.data(), 0))
        throw SYSTEM_ERROR("::unlinkat() failure on fd-relative file \"" + pathname + "\"");

    pruneEmptyDirPath(fd, dirname(pathname));
}

void FileUtil::closeOnExec(const int fd)
{
    int flags = ::fcntl(fd, F_GETFD);

    if (-1 == flags)
        throw SYSTEM_ERROR("Couldn't get flags for file descriptor %d", fd);

    if (!(flags & FD_CLOEXEC) && (-1 == ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC)))
        throw SYSTEM_ERROR("Couldn't set file descriptor %d to close-on-exec()", fd);
}

} // namespace

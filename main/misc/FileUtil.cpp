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

bool FileUtil::isAbsolute(const std::string& pathname)
{
    return pathname.at(0) == '/';
}

std::string FileUtil::makeAbsolute(const std::string& pathname)
{
    if (pathname.size() && pathname.at(0) == '/')
        return std::string(pathname);

    char cwd[PATH_MAX];
    return std::string(::getcwd(cwd, sizeof(cwd))) + "/" + pathname;
}

std::string FileUtil::filename(const std::string& pathname) noexcept
{
    char buf[PATH_MAX];
    ::strncpy(buf, pathname.data(), sizeof(buf))[sizeof(buf)-1] = 0;
    return std::string(::basename(buf));
}

std::string FileUtil::dirname(const std::string& pathname) noexcept
{
    // Workaround `dirname()` not being re-entrant
    char buf[PATH_MAX];
    ::strncpy(buf, pathname.data(), sizeof(buf))[sizeof(buf)-1] = 0;
    return std::string(::dirname(buf));
}

void FileUtil::trimPathname(std::string& pathname) noexcept
{
    pathname = dirname(pathname) + '/' +  filename(pathname);
}

size_t FileUtil::getSize(const std::string& pathname)
{
    struct ::stat    statBuf;

    if (::stat(pathname.data(), &statBuf))
        throw SYSTEM_ERROR(std::string("stat() failure on \"") + pathname + "\"");

    return statBuf.st_size;
}

struct ::stat& FileUtil::statNoFollow(
        const std::string& pathname,
        struct ::stat&     statBuf)
{
    if (::stat(pathname.data(), &statBuf))
        throw SYSTEM_ERROR("::stat() failure on file \"" + pathname + "\"");
    return statBuf;
}

/**
 * Returns the statistics of a file.
 *
 * @param[in] rootFd        File descriptor open on root-directory
 * @param[in] pathname      Pathname of existing file. May be absolute or relative to the root-
 *                          directory.
 * @return                  Statistics of the file
 * @throws    SYSTEM_ERROR  Couldn't open file
 * @throws    SYSTEM_ERROR  Couldn't get information on the file
 * @threadsafety            Safe
 * @exceptionsafety         Strong guarantee
 * @cancellationpoint       No
 */
struct stat FileUtil::getStat(
        const int          rootFd,
        const std::string& pathname)
{
    const int fd = ::openat(rootFd, pathname.data(), O_RDONLY);
    if (fd == -1)
        throw SYSTEM_ERROR("Couldn't open file \"" + pathname + "\"");

    try {
        struct ::stat statBuf;
        int           status = ::fstat(fd, &statBuf);

        if (status)
            throw SYSTEM_ERROR("stat() failure");

        ::close(fd);
        return statBuf;
    } // `fd` is open
    catch (...) {
        ::close(fd);
        throw;
    }
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

SysTimePoint& FileUtil::getModTime(
        const std::string& pathname,
        SysTimePoint&      modTime)
{
    struct ::stat statBuf;
    FileUtil::statNoFollow(pathname, statBuf); // Doesn't follow symlinks
    modTime = SysClock::from_time_t(statBuf.st_mtim.tv_sec) +
            std::chrono::nanoseconds(statBuf.st_mtim.tv_nsec);
    return modTime;
}

SysTimePoint FileUtil::getModTime(
        const int          rootFd,
        const std::string& pathname)
{
    auto statBuf = getStat(rootFd, pathname);
    return SysClock::from_time_t(statBuf.st_mtim.tv_sec) +
            std::chrono::nanoseconds(statBuf.st_mtim.tv_nsec);
}

void FileUtil::setModTime(
        const int           rootFd,
        const std::string&  pathname,
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
        const std::string&  pathname,
        const SysTimePoint& modTime,
        const bool          followSymLinks) {
    setModTime(AT_FDCWD, pathname, modTime, followSymLinks);
}

off_t FileUtil::getFileSize(const std::string& pathname)
{
    struct ::stat statBuf;
    return statNoFollow(pathname, statBuf).st_size;
}

off_t FileUtil::getFileSize(
        const int          rootFd,
        const std::string& pathname)
{
    return getStat(rootFd, pathname).st_size;
}

void FileUtil::rename(
        const int          rootFd,
        const std::string& oldPathname,
        const std::string& newPathname)
{
    if (::renameat(rootFd, oldPathname.data(), rootFd, newPathname.data()))
        throw SYSTEM_ERROR("::renameat() failure: from=\"" + oldPathname + "\", to=\"" +
                newPathname + "\"");
}

const std::string& FileUtil::ensureDir(
        const std::string& pathname,
        const mode_t       mode)
{
    struct ::stat statBuf;

    if (::stat(pathname.data(), &statBuf)) {
        if (errno != ENOENT)
            throw SYSTEM_ERROR(std::string("stat() failure on \"") + pathname + "\"");

        ensureDir(dirname(pathname), mode);

        if (::mkdir(pathname.data(), mode))
            throw SYSTEM_ERROR(std::string("mkdir() failure on \"") + pathname + "\"");
    }

    return pathname;
}

const std::string& FileUtil::ensureDir(
        const int          fd,
        const std::string& pathname,
        const mode_t       mode)
{
    struct ::stat statBuf;

    if (::fstatat(fd, pathname.data(), &statBuf, 0)) {
        if (errno != ENOENT)
            throw SYSTEM_ERROR(std::string("fstatat() failure on \"") + pathname + "\"; fd=" +
                    std::to_string(fd));

        ensureDir(fd, dirname(pathname), mode);

        if (::mkdirat(fd, pathname.data(), mode))
            throw SYSTEM_ERROR(std::string("mkdir() failure on \"") + pathname + "\"");
    }

    return pathname;
}

void FileUtil::ensureParent(
        const std::string& pathname,
        const mode_t       mode)
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

void FileUtil::rmDirTree(const std::string& dirPath)
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
                    const std::string subName = dirPath + "/" + name;
                    struct ::stat     statBuf;

                    statNoFollow(subName, statBuf);

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

            if (::rmdir(dirPath.data()))
                throw SYSTEM_ERROR("Couldn't delete directory \"" + dirPath + "\"");
        } // `dir` is set
        catch (...) {
            ::closedir(dir);
            throw;
        }
    }
}

void FileUtil::pruneEmptyDirPath(
        const std::string& rootPathname,
        const std::string& dirPathname)
{
    struct ::stat statBuf;
    const auto    rootInode = statNoFollow(rootPathname, statBuf).st_ino;
    std::string   dirPath(dirPathname);

    for (;;) {
        statNoFollow(dirPath, statBuf);

        if (statBuf.st_ino == rootInode)
            break; // Stop because at root directory

        if (!S_ISDIR(statBuf.st_mode))
            // Must be a symbolic link. Should first be removed by `removeFileAndPrune()` instead.
            break;

        auto status = ::rmdir(dirPath.data());
        if (status) {
            if (status == EEXIST || status == ENOTEMPTY)
                break; // Stop because directory isn't empty
            throw SYSTEM_ERROR(std::string("::rmdir() failure on directory \"") + dirPath + "\"");
        }

        dirPath = dirname(dirPath); // Repeat with parent directory
    }
}

void FileUtil::pruneEmptyDirPath(
        const int     fd,
        std::string&& dirPath)
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
            throw SYSTEM_ERROR(std::string("unlinkat() failure on fd-relative directory \"") +
                    dirPath + "\"");
        }
        else {
            dirPath = dirname(dirPath); // Repeat with parent directory
        }
    }
}

void FileUtil::removeFileAndPrune(
        const std::string& rootPathname,
        const std::string& pathname)
{
    if (::unlink(pathname.data()))
        throw SYSTEM_ERROR("::unlink() failure on file \"" + pathname + "\"");

    pruneEmptyDirPath(rootPathname, dirname(pathname));
}

void FileUtil::removeFileAndPrune(
        const int          fd,
        const std::string& pathname)
{
    if (::unlinkat(fd, pathname.data(), 0))
        throw SYSTEM_ERROR("::unlinkat() failure on fd-relative file \"" + pathname + "\"");

    pruneEmptyDirPath(fd, dirname(pathname));
}

} // namespace

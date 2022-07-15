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
#include "FileUtil.h"

#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace hycast {

std::string makeAbsolute(const std::string& pathname)
{
    if (pathname.at(0) == '/')
        return std::string(pathname);

    char cwd[PATH_MAX];
    return std::string(::getcwd(cwd, sizeof(cwd))) + "/" + pathname;
}

std::string basename(const std::string& pathname) noexcept
{
    char buf[PATH_MAX];
    ::strncpy(buf, pathname.data(), sizeof(buf))[sizeof(buf)-1] = 0;
    return std::string(::basename(buf));
}

std::string dirname(const std::string& pathname) noexcept
{
    char buf[PATH_MAX];
    ::strncpy(buf, pathname.data(), sizeof(buf))[sizeof(buf)-1] = 0;
    return std::string(::dirname(buf));
}

void trimPathname(std::string& pathname) noexcept
{
    pathname = dirname(pathname) + '/' +  basename(pathname);
}

size_t getSize(const std::string& pathname)
{
    struct stat statBuf;

    if (::stat(pathname.data(), &statBuf))
        throw SYSTEM_ERROR(std::string("stat() failure on \"") + pathname + "\"");

    return statBuf.st_size;
}

void ensureDir(
        const std::string& pathname,
        const mode_t       mode)
{
    struct stat statBuf;

    if (::stat(pathname.data(), &statBuf)) {
        if (errno != ENOENT)
            throw SYSTEM_ERROR(std::string("stat() failure on \"") + pathname + "\"");

        ensureDir(dirname(pathname), mode);

        if (::mkdir(pathname.data(), mode))
            throw SYSTEM_ERROR(std::string("mkdir() failure on \"") + pathname + "\"");
    }
}

void ensureDir(
        const int          fd,
        const std::string& pathname,
        const mode_t       mode)
{
    struct stat statBuf;

    if (::fstatat(fd, pathname.data(), &statBuf, 0)) {
        if (errno != ENOENT)
            throw SYSTEM_ERROR(std::string("fstatat() failure on \"") + pathname + "\"; fd=" +
                    std::to_string(fd));

        ensureDir(fd, dirname(pathname), mode);

        if (::mkdirat(fd, pathname.data(), mode))
            throw SYSTEM_ERROR(std::string("mkdir() failure on \"") + pathname + "\"");
    }
}

void ensureParent(
        const std::string& pathname,
        const mode_t       mode)
{
    ensureDir(dirname(pathname), mode);
}

void rmDirTree(const std::string& dirPath)
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
                    struct stat       statBuf;

                    if (::lstat(subName.data(), &statBuf))
                        throw SYSTEM_ERROR(std::string("lstat() failure on \"") + subName + "\"");

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

void pruneEmptyDirPath(
        const int     fd,
        std::string&& dirPath)
{
    struct stat statBuf;
    if (::fstatat(fd, ".", &statBuf, 0))
        throw SYSTEM_ERROR("fstatat() failure on fd-relative directory \".\"");
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

void removeFileAndPrune(
        const int    fd,
        std::string& pathname)
{
    if (::unlinkat(fd, pathname.data(), 0))
        throw SYSTEM_ERROR("::unlinkat() failure on fd-relative file \"" + pathname + "\"");

    pruneEmptyDirPath(fd, dirname(pathname));
}

} // namespace

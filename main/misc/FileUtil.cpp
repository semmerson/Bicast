/**
 * Utility functions for files.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: FileUtil.cpp
 *  Created on: Jan 2, 2020
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "FileUtil.h"

#include "error.h"

#include <dirent.h>
#include <libgen.h>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace hycast {

std::string filename(const std::string& pathname)
{
    char buf[PATH_MAX];
    ::strncpy(buf, pathname.data(), sizeof(buf))[sizeof(buf)-1] = 0;
    return std::string(::basename(buf));
}

std::string dirPath(const std::string& pathname)
{
    char buf[PATH_MAX];
    ::strncpy(buf, pathname.data(), sizeof(buf))[sizeof(buf)-1] = 0;
    return std::string(::dirname(buf));
}

void ensureDir(
        const std::string& pathname,
        const mode_t       mode = 0700)
{
    struct stat statBuf;

    if (::stat(pathname.data(), &statBuf)) {
        if (errno != ENOENT)
            throw SYSTEM_ERROR(std::string("stat() failure on \"") +
                    pathname + "\"");

        ensureDir(dirPath(pathname), mode);

        if (::mkdir(pathname.data(), mode))
            throw SYSTEM_ERROR(std::string("mkdir() failure on \"") +
                    pathname + "\"");
    }
}

void pruneDir(const std::string& pathname)
{
    DIR* dir = ::opendir(pathname.data());
    if (dir == nullptr)
        throw SYSTEM_ERROR("Couldn't open directory \"" + pathname + "\"");

    try {
        struct dirent  entry;
        struct dirent* result;
        int            status;
        bool           shouldDelete = true;

        for (status = ::readdir_r(dir, &entry, &result);
                status == 0 && result != nullptr;
                status = ::readdir_r(dir, &entry, &result)) {
            const char* name = entry.d_name;

            if (::strcmp(".", name) && ::strcmp("..", name)) {
                const std::string subdir = pathname + "/" + name;
                struct stat       statBuf;

                if (::stat(subdir.data(), &statBuf))
                    throw SYSTEM_ERROR(std::string("stat() failure on \"") +
                            subdir + "\"");

                if (!S_ISDIR(statBuf.st_mode)) {
                    shouldDelete = false;
                }
                else {
                    pruneDir(subdir);
                }
            }
        }
        if (status && status != ENOENT)
            throw SYSTEM_ERROR("Couldn't read directory \"" + pathname + "\"");

        if (shouldDelete && ::rmdir(pathname.data()))
            throw SYSTEM_ERROR("Couldn't delete directory \"" + pathname +
                    "\"");

        ::closedir(dir);
    }
    catch (...) {
        ::closedir(dir);
        throw;
    }
}

size_t getSize(const std::string& pathname)
{
    struct stat statBuf;

    if (::stat(pathname.data(), &statBuf))
        throw SYSTEM_ERROR(std::string("stat() failure on \"") + pathname +
                "\"");

    return statBuf.st_size;
}

} // namespace

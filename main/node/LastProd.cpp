/**
 * @file LastProd.cpp
 * Manages access to the time of the last product transmitted, received, or locally processed. A
 * product has  an associated creation-time (i.e., the time that the publisher created it). This
 * time is the modification-time of the corresponding product-file and is promulgated as such to
 * subscribers. This creation-time is used to determine if a product needs to be transmitted, has
 * been received, or has been locally processed. Obviously, this time must persist between sessions
 * and be available at the start of a new session. That is the job of this component.
 *
 *  Created on: Aug 26, 2023
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

#include "FileUtil.h"
#include "LastProd.h"
#include "error.h"

#include <fcntl.h>
#include <unistd.h>

namespace bicast {

/// Saves a time persistently.
class LastProdImpl : public LastProd
{
    String path;    ///< Pathname of the file
    String relPath; ///< Pathname of the file relative to dirFd
    int    dirFd;   ///< File descriptor open on the file's parent directory

public:
    /**
     * Constructs.
     * @param[in] path Pathname of file to contain the time
     */
    explicit LastProdImpl(const String& path)
        : path(path)
        , relPath(FileUtil::filename(path))
        , dirFd(-1)
    {
        FileUtil::ensureDir(FileUtil::dirname(path));
        dirFd = ::open(FileUtil::dirname(path).data(), O_CLOEXEC | O_DIRECTORY);
        if (dirFd == -1)
            throw SYSTEM_ERROR("Couldn't open directory \"" + FileUtil::dirname(path) + "\"");

        const auto fd = ::openat(dirFd, relPath.data(), O_WRONLY | O_CREAT | O_CLOEXEC | O_TRUNC,
                0666);
        if (fd == -1)
            throw SYSTEM_ERROR("Couldn't open file \"" + path + "\"");
        ::close(fd);
    }

    ~LastProdImpl() {
        if (dirFd >= 0)
            ::close(dirFd);
    }

    /**
     * Saves a time.
     * @param[in] time  Time to be saved
     */
    void save(const SysTimePoint time) override {
        // The time is saved as the modification-time of the file.
        FileUtil::setModTime(dirFd, relPath, time, false); // false => won't follow symlinks
    }

    /**
     * Returns the previously-saved time.
     * @return             Previously-saved time or SysTimePoint::min() if no such time
     *                     exists
     * @throw SystemError  The time couldn't be obtained
     */
    SysTimePoint recall() override {
        try {
            return (dirFd < 0)
                    ? SysTimePoint::min()
                    : FileUtil::getModTime(dirFd, relPath, false);
        }
        catch (const SystemError& ex) {
            if (ex.code().value() == ENOENT)
                return SysTimePoint::min();
            throw;
        }
    }
};

/// Implementation of a dummy LastProd.
class DummyLastProd : public LastProd
{
public:
    /**
     * Constructs.
     * @throw SystemError       Couldn't create a necessary directory
     */
    DummyLastProd() =default;

    void save(const SysTimePoint time) override {
    }

    SysTimePoint recall() override {
        return SysTimePoint::min();
    }
};

LastProdPtr LastProd::create()
{
    return LastProdPtr(new DummyLastProd());
}

LastProdPtr LastProd::create(const String& pathTemplate)
{
    return LastProdPtr(new LastProdImpl(pathTemplate));
}

} // namespace





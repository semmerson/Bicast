/**
 * @file LastProc.cpp
 * Manages access to the time of the last, successfully-processed product.
 * A locally processed product (either sent by a publisher or received by a subscriber) has  an
 * associated creation-time (i.e., the time that the publisher created it). This time is the
 * modification-time of the corresponding product-file and is promulgated as such to subscribers.
 * This creation-time is used to determine if a product file needs to be locally processed based on
 * the creation-time of the last, successfully-processed product. Obviously, this latter time must
 * persist between sessions and be available at the start of a new session. That is the job of this
 * component.
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

#include "LastProc.h"

#include "FileUtil.h"
#include "error.h"

#include <unistd.h>

namespace hycast {

/// File for saving a time.
class TimeFile
{
    /* TODO: Use a symlink to the last, successfuly-modified file and the modification-time of that
     * file
     */
    String symLink; ///< Pathname of the symbolic link

public:
    /**
     * Default constructs.
     */
    TimeFile()
        : symLink("")
    {}

    /**
     * Constructs.
     * @param[in] symLink Pathname of symbolic link
     */
    explicit TimeFile(const String& symLink)
        : symLink(symLink)
    {}

    /**
     * Swaps this instance with another.
     * @param[in] that  The other instance
     */
    void swap(TimeFile& that) {
        if (this != &that)
            this->symLink.swap(that.symLink);
    }

    /**
     * Saves a reference to the last, successfully-processed product-file.
     * @param[in] pathname  Pathname of the last, successfully-processed product-file
     */
    void save(const String& pathname) {
        if (::unlink(symLink.data()) && errno != ENOENT)
            throw SYSTEM_ERROR("Couldn't unlink symbolic link \"" + symLink + "\"");
        if (::symlink(pathname.data(), symLink.data()))
            throw SYSTEM_ERROR("Couldn't link \"" + symLink + "\" to \"" + pathname + "\"");
        /*
         * The modification-time of the symlink is set to that of the product-file because the
         * product-file could be a symlink itself. See recall().
         */
        SysTimePoint modTime;
        FileUtil::getModTime(pathname, modTime, false);
        FileUtil::setModTime(symLink, modTime, false);
    }

    /**
     * Returns the modification-time of the last, successfully-processed file.
     * @return             Modification-file of the file or SysTimePoint::min() if no such time
     *                     exists
     * @throw SystemError  The file exists but its modification-time couldn't be obtained
     */
    SysTimePoint& recall(SysTimePoint& modTime) {
        try {
            FileUtil::getModTime(symLink, modTime, false); // Don't follow symbolic links
        }
        catch (const std::exception& ex) {
            if (errno == ENOENT) {
                modTime = SysTimePoint::min();
            }
            else {
                throw;
            }
        }
        return modTime;
    }
};

/// Implementation of LastProc.
class LastProcImpl : public LastProc
{
    TimeFile timeFiles[2]; ///< Symbolic links for storing information
    int      last;         ///< Index of last, successfully-processed product-file
    int      next;         ///< Index of next, successfully-processed product-file

public:
    /**
     * Constructs.
     * @param[in] lastProcDir  Pathname of directory to hold information
     * @param[in] feedName     Name of data-product feed
     */
    LastProcImpl(
            const String& lastProcDir,
            const String& feedName)
        : timeFiles()
        , last(0)
        , next(1)
    {
        const String baseFilename = lastProcDir + FileUtil::separator + feedName;
        SysTimePoint modTimes[2];

        for (int i = 0; i < 2; ++i) {
            TimeFile timeFile(baseFilename + "." + std::to_string(i));
            timeFiles[i].swap(timeFile);
            timeFiles[i].recall(modTimes[i]);
        }

        if (modTimes[0] >= modTimes[1]) {
            last = 0;
            next = 1;
        }
        else {
            last = 1;
            next = 0;
        }
    }

    void save(const String& pathname) override {
        timeFiles[next].save(pathname);
        next = last;
        last ^= 1;
    }

    SysTimePoint recall() override {
        SysTimePoint modTime{};
        return timeFiles[last].recall(modTime);
    }
};

LastProcPtr LastProc::create(
        const String& lastProcDir,
        const String& feedName)
{
    return LastProcPtr(new LastProcImpl(lastProcDir, feedName));
}

} // namespace





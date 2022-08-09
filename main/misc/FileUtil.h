/**
 * Utility functions for files.
 *
 *        File: FileUtil.h
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

#ifndef MAIN_MISC_FILEUTIL_H_
#define MAIN_MISC_FILEUTIL_H_

#include "HycastProto.h"

#include <string>
#include <chrono>

namespace hycast {

class FileUtil
{
    /**
     * Indicates if a pathname is absolute or not.
     *
     * @param[in] pathname  Pathname
     * @return    `true`    Pathname is absolute
     * @return    `false`   Pathname is relative
     */
    static bool isAbsolute(const std::string& pathname);

    /**
     * Returns the absolute pathname of a generic (i.e., relative or absolute)
     * pathname. The current working directory is used if the pathname is
     * relative.
     *
     * @param[in] pathname  Pathname to have its absolute form returned
     * @return              Corresponding absolute pathname. Will be the same
     *                      if the pathname is absolute
     * @throws OutOfRange   Pathname is empty
     */
    static std::string makeAbsolute(const std::string& pathname);

    static std::string basename(const std::string& pathname) noexcept;

    static std::string dirname(const std::string& pathname) noexcept;

    static void trimPathname(std::string& pathname) noexcept;

    static size_t getSize(const std::string& pathname);

    /**
     * Returns the metadata of a file. Doesn't follow symbolic links.
     *
     * @param[in]  pathname     Pathname of existing file
     * @param[out] statBuf      Metadata of the file
     * @throws     SystemError  `::stat()` failure
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static void lstat(
            const std::string& pathname,
            struct stat&       statBuf);

    /**
     * Returns the statistics of a file.
     *
     * @param[in] rootFd        File descriptor open on root-directory
     * @param[in] pathname      Pathname of existing file
     * @return                  Statistics of the file
     * @throws    SYSTEM_ERROR  `::openat()` failure
     * @throws    SYSTEM_ERROR  `::stat()` failure
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static struct stat getStat(
            const int          rootFd,
            const std::string& pathname);

    /**
     * Returns the modification time of a file.
     *
     * @param[in] pathname      Pathname of existing file
     * @return                  Modification time of the file
     * @throws    SYSTEM_ERROR  `stat()` failure
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static SysTimePoint& getModTime(
            const std::string& pathname,
            SysTimePoint&      modTime);

    /**
     * Returns the modification time of a file.
     *
     * @param[in] rootFd        File descriptor open on root-directory
     * @param[in] pathname      Pathname of existing file
     * @return                  Modification time of the file
     * @throws    SYSTEM_ERROR  `::openat()` failure
     * @throws    SYSTEM_ERROR  `stat()` failure
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static SysTimePoint getModTime(
            const int          rootFd,
            const std::string& pathname);

    /**
     * Sets the modification time of a file.
     *
     * @param[in] pathname        Pathname of file
     * @param[in] modTime         New modification time
     * @param[in] followSymLinks  Follow symbolic links?
     */
    static void setModTime(
            const std::string&  pathname,
            const SysTimePoint& modTime,
            const bool          followSymLinks);

    /**
     * Sets the modification time of a file.
     *
     * @param[in] rootFd          File descriptor open on root directory
     * @param[in] pathname        Pathname relative to root file descriptor
     * @param[in] modTime         New modification time
     * @param[in] followSymLinks  Follow symbolic links?
     */
    static void setModTime(
            const int           rootFd,
            const std::string&  pathname,
            const SysTimePoint& modTime,
            const bool          followSymLinks);

    /**
     * Returns the size of a file in bytes.
     *
     * @param[in] pathname      Pathname of existing file
     * @return                  Size of file in bytes
     * @throws    SYSTEM_ERROR  `stat()` failure
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static off_t getFileSize(const std::string& pathname);

    /**
     * Returns the size of a file in bytes.
     *
     * @param[in] rootFd        File descriptor open on root-directory
     * @param[in] pathname      Pathname of existing file
     * @return                  Size of file in bytes
     * @throws    SYSTEM_ERROR  `::openat()` failure
     * @throws    SYSTEM_ERROR  `stat()` failure
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static off_t getFileSize(
            const int          rootFd,
            const std::string& pathname);

    /**
     * Renames a file.
     *
     * @param[in] rootFd       File descriptor open on root-directory
     * @param[in] oldPathname  Old pathname relative to root directory
     * @param[in] newPathname  New pathname relative to root directory
     * @throw SystemError      `::renameat()` failure
     */
    static void rename(
            const int          rootFd,
            const std::string& oldPathname,
            const std::string& newPathname);

    /**
     * Follows symbolic links.
     *
     * @param[in] pathname  Pathname of directory
     * @param[in] mode      Permission mode for directory
     * @return              `pathname`
     */
    static const std::string& ensureDir(
            const std::string& pathname,
            const mode_t       mode = 0777);

    /**
     * Follows symbolic links.
     *
     * @param fd            File descriptor open on root directory
     * @param pathname      Pathname relative to root directory
     * @param mode          Permission mode for directory
     * @return              `pathname`
     */
    static const std::string& ensureDir(
            const int          fd,
            const std::string& pathname,
            const mode_t       mode = 0777);

    /**
     * Ensures that the parent directory of a file exists.
     *
     * @param[in] pathname  Pathname of file
     * @param[in] mode      Directory creation mode
     */
    static void ensureParent(
            const std::string& pathname,
            const mode_t       mode = 0777);

    /**
     * Removes a directory tree. The given directory tree is recursively traversed depth first. All
     * files in the tree are removed, including the root directory. Symbolic links are removed but not
     * the file the link references.
     *
     * @param[in] pathname  Root of directory hierarchy
     */
    static void rmDirTree(const std::string& dirPath);

    /**
     * Deletes empty directories starting with a leaf directory and progressing towards a root
     * directory. Stops when a non-empty directory or the root directory is encountered. The root
     * directory is not deleted.
     *
     * @param[in] fd           File descriptor open on the root directory
     * @param[in,out] dirPath  Pathname of the leaf directory at which to start
     * @throw SystemError      A directory couldn't be `stat()`ed
     * @throw SystemError      A directory couldn't be deleted
     */
    static void pruneEmptyDirPath(
            const int      fd,
            std::string&& dirPath);

    /**
     * Deletes a file and any empty directories on the path from a root directory to the file. The
     * root directory is not deleted.
     *
     * @param fd           File descriptor open on the root directory
     * @param pathname     Pathname of the file to be deleted
     * @throw SystemError  The file couldn't be deleted
     * @throw SystemError  A directory couldn't be `stat()`ed
     * @throw SystemError  A directory couldn't be deleted
     * @see `pruneEmptyDirPath()`
     */
    static void removeFileAndPrune(
            const int          fd,
            const std::string& pathname);
};

} // namespace

#endif /* MAIN_MISC_FILEUTIL_H_ */

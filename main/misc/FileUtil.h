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

#include "CommonTypes.h"

#include <chrono>
#include <string>
#include <sys/stat.h>

namespace hycast {

/// A class with utility functions for files
class FileUtil
{
public:
    static constexpr char separator = '/'; ///< Separator between pathname components

    /**
     * Indicates if a pathname is absolute or not.
     *
     * @param[in] pathname  Pathname
     * @return    true      Pathname is absolute
     * @return    false     Pathname is relative
     */
    static bool isAbsolute(const String& pathname);

    /**
     * Returns the absolute pathname of a generic (i.e., relative or absolute) pathname. The current
     * working directory is used if the pathname is relative.
     *
     * @param[in] pathname  Pathname to have its absolute form returned
     * @return              Corresponding absolute pathname. Will be the same
     *                      if the pathname is absolute
     * @throws OutOfRange   Pathname is empty
     */
    static String makeAbsolute(const String& pathname);

    /**
     * Returns the filename portion of a pathname.
     * @param[in] pathname  The pathname
     * @return The filename portion of the pathname
     */
    static String filename(const String& pathname) noexcept;

    /**
     * Returns a pathname with a trailing separator. If the input pathname already has a trailing
     * separator, then a copy of it is returned.
     * @param[in] pathname  The pathname
     * @return              The pathname with a trailing separator.
     */
    static String ensureTrailingSep(const String& pathname);

    /**
     * Returns the pathname of a file given the pathname of its directory and the name of the file.
     * @param[in] dirPath   The pathname of the parent directory. May be empty.
     * @param[in] filename  The name of the file
     * @return              The pathname of the file
     */
    static String pathname(
            const String& dirPath,
            const String& filename);

    /**
     * Returns the directory portion of a pathname.
     * @param[in] pathname  The pathname
     * @return The directory portion of the pathname
     */
    static String dirname(const String& pathname) noexcept;

    /**
     * Trims a pathname by removing any trailing slashes.
     * @param[in,out] pathname  The pathname to be trimmed
     */
    static void trimPathname(String& pathname) noexcept;

    /**
     * Indicates if a file exists.
     * @param[in] pathname  Pathname of the file
     * @retval    true      File exists
     * @retval    false     File doesn't exist
     */
    static bool exists(const String& pathname) noexcept;

    /**
     * Returns the size of the regular file referenced by a pathname.
     * @param[in] pathname  The pathname of the file
     * @return The size, in bytes, of the regular file referenced by the pathname
     */
    static size_t getSize(const String& pathname);

    /**
     * Returns the metadata of a file. Doesn't follow symbolic links.
     *
     * @param[in]  pathname     Pathname of existing file
     * @param[out] statBuf      Metadata of the file
     * @return                  Reference to `statBuf`
     * @throws     SystemError  Couldn't get information on the file
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static struct ::stat& statNoFollow(
            const String& pathname,
            struct ::stat&     statBuf);

    /**
     * Returns the status of a file. Follows symbolic links.
     *
     * @param[in]  rootFd       File descriptor open on root-directory
     * @param[in]  pathname     Pathname of existing file. May be absolute or relative to the root-
     *                          directory.
     * @param[out] statBuf      Status buffer
     * @throws    SYSTEM_ERROR  Couldn't open file
     * @throws    SYSTEM_ERROR  Couldn't get information on the file
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static void getStat(
            const int      rootFd,
            const String&  pathname,
            struct ::stat& statBuf);

    /**
     * Returns the statistics of a file.
     *
     * @param[in] rootFd        File descriptor open on root-directory
     * @param[in] pathname      Pathname of existing file. May be absolute or relative to the
     *                          root-directory.
     * @return                  Statistics of the file
     * @throws    SYSTEM_ERROR  Couldn't open the file.
     * @throws    SYSTEM_ERROR  Couldn't get information on the file
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static struct stat getStat(
            const int     rootFd,
            const String& pathname);

    /**
     * Sets the ownership of an existing file.
     *
     * @param[in] pathname     Pathname of file
     * @param[in] uid          User ID
     * @param[in] gid          Group ID
     * @throw     SystemError  Couldn't change ownership of file
     */
    static void setOwnership(
            const String& pathname,
            const uid_t   uid,
            const gid_t   gid);

    /**
     * Sets the file-protection bits of an existing file.
     *
     * @param[in] pathname     Pathname of file
     * @param[in] protMask     Protection mask (e.g., 0644)
     * @throw     SystemError  Couldn't change mode of file
     */
    static void setProtection(
            const String& pathname,
            const mode_t  protMask);

    /**
     * Returns the modification time of a file. Doesn't follow symbolic links.
     *
     * @param[in]  pathname     Pathname of existing file
     * @param[out] modTime      Modification time of the file
     * @return                  Reference to `modtime`
     * @throws    SYSTEM_ERROR  `stat()` failure
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static SysTimePoint& getModTime(
            const String& pathname,
            SysTimePoint&      modTime);

    /**
     * Returns the modification time of a file. Follows symbolic links.
     *
     * @param[in] rootFd        File descriptor open on root-directory
     * @param[in] pathname      Pathname of existing file. May be absolute or relative to the
     *                          root-directory.
     * @return                  Modification time of the file
     * @throws    SYSTEM_ERROR  Couldn't open the file
     * @throws    SYSTEM_ERROR  Couldn't get information on the file
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static SysTimePoint getModTime(
            const int          rootFd,
            const String& pathname);

    /**
     * Sets the modification time of a file.
     *
     * @param[in] pathname        Pathname of file
     * @param[in] modTime         New modification time
     * @param[in] followSymLinks  Follow symbolic links?
     */
    static void setModTime(
            const String&  pathname,
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
            const String&  pathname,
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
    static off_t getFileSize(const String& pathname);

    /**
     * Returns the size of a file in bytes.
     *
     * @param[in] rootFd        File descriptor open on root-directory
     * @param[in] pathname      Pathname of existing file. Couldn't be absolute or relative to the
     *                          root-directory.
     * @return                  Size of file in bytes
     * @throws    SYSTEM_ERROR  Couldn't open file
     * @throws    SYSTEM_ERROR  Couldn't obtain information on the file
     * @threadsafety            Safe
     * @exceptionsafety         Strong guarantee
     * @cancellationpoint       No
     */
    static off_t getFileSize(
            const int          rootFd,
            const String& pathname);

    /**
     * Renames a file.
     *
     * @param[in] rootFd       File descriptor open on root-directory
     * @param[in] oldPathname  Old pathname relative to root directory
     * @param[in] newPathname  New pathname relative to root directory
     * @throw SystemError      Couldn't rename the file
     */
    static void rename(
            const int          rootFd,
            const String& oldPathname,
            const String& newPathname);

    /**
     * Follows symbolic links.
     *
     * @param[in] pathname  Pathname of directory
     * @param[in] mode      Permission mode for directory
     * @return              `pathname`
     */
    static const String& ensureDir(
            const String& pathname,
            const mode_t       mode = 0777);

    /**
     * Follows symbolic links.
     *
     * @param fd            File descriptor open on root directory
     * @param pathname      Pathname relative to root directory
     * @param mode          Permission mode for directory
     * @return              `pathname`
     */
    static const String& ensureDir(
            const int          fd,
            const String& pathname,
            const mode_t       mode = 0777);

    /**
     * Ensures that the parent directory of a file exists.
     *
     * @param[in] pathname  Pathname of file
     * @param[in] mode      Directory creation mode
     */
    static void ensureParent(
            const String& pathname,
            const mode_t       mode = 0777);

    /**
     * Creates a hard link to a file.
     *
     * @param[in] extantPath  Pathname of existing file
     * @param[in] linkPath    Pathname of link. Its parent directory must exist.
     * @see `ensureParent()`
     */
    static void hardLink(
            const String& extantPath,
            const String& linkPath);

    /**
     * Removes a directory tree. The given directory tree is recursively traversed depth first. All
     * files in the tree are removed, including the root directory. Symbolic links are removed but
     * not the file the link references.
     *
     * @param[in] dirPath  Root of directory hierarchy
     */
    static void rmDirTree(const String& dirPath);

    /**
     * Deletes empty directories starting with a leaf directory and progressing towards a root
     * directory. Stops when a non-empty directory or the root directory is encountered. The root
     * directory is not deleted.
     *
     * @param[in] rootPathname     Pathname of the root directory
     * @param[in,out] dirPathname  Pathname of the leaf directory at which to start
     * @throw SystemError          A directory couldn't be `stat()`ed
     * @throw SystemError          A directory couldn't be deleted
     */
    static void pruneEmptyDirPath(
            const String& rootPathname,
            const String& dirPathname);

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
            String&& dirPath);

    /**
     * Deletes a file and any empty directories on the path from the file to a root directory. The
     * root directory is not deleted.
     *
     * @param rootPathname  Pathname of the root directory
     * @param pathname      Pathname of the file to be deleted
     * @throw SystemError   The file couldn't be deleted
     * @throw SystemError   A directory couldn't be `stat()`ed
     * @throw SystemError   A directory couldn't be deleted
     * @see `pruneEmptyDirPath()`
     */
    static void removeFileAndPrune(
            const String& rootPathname,
            const String& pathname);

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
            const String& pathname);

    /**
     * Set a file descriptor to close-on-exec.
     * @param[in] fd       File descriptor
     * @throw SystemError  Couldn't get file descriptor flags
     * @throw SystemError  Couldn't set file descriptor to close-on-exec
     */
    static void closeOnExec(const int fd);
};

} // namespace

#endif /* MAIN_MISC_FILEUTIL_H_ */

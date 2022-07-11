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

#include <string>

namespace hycast {

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
std::string makeAbsolute(const std::string& pathname);

std::string basename(const std::string& pathname) noexcept;

std::string dirname(const std::string& pathname) noexcept;

void trimPathname(std::string& pathname) noexcept;

size_t getSize(const std::string& pathname);

/**
 * Follows symbolic links.
 *
 * @param pathname
 * @param mode
 */
void ensureDir(
        const std::string& pathname,
        const mode_t       mode = 0777);

/**
 * Follows symbolic links.
 *
 * @param fd
 * @param pathname
 * @param mode
 */
void ensureDir(
        const int          fd,
        const std::string& pathname,
        const mode_t       mode = 0777);

/**
 * Ensures that the parent directory of a file exists.
 *
 * @param[in] pathname  Pathname of file
 * @param[in] mode      Directory creation mode
 */
void ensureParent(
        const std::string& pathname,
        const mode_t       mode = 0777);

/**
 * Removes a directory tree. The given directory tree is recursively traversed depth first. All
 * files in the tree are removed, including the root directory. Symbolic links are removed but not
 * the file the link references.
 *
 * @param[in] pathname  Root of directory hierarchy
 */
void rmDirTree(const std::string& dirPath);

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
void pruneEmptyDirPath(
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
void removeFileAndPrune(
        const int    fd,
        std::string& pathname);

} // namespace

#endif /* MAIN_MISC_FILEUTIL_H_ */

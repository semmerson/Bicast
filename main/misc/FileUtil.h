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

std::string filename(const std::string& pathname);

std::string dirPath(const std::string& pathname);

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
 * Removes a directory hierarchy. All files in the hierarchy -- including the
 * root directory -- are removed.
 *
 * @param[in] pathname  Root of directory hierarchy
 */
void rmDirTree(const std::string& dirPath);

/**
 * Deletes a directory. The given directory hierarchy is recursively
 * traversed depth first. All files are deleted, including the given directory.
 *
 * @param[in] pathname  Root of directory hierarchy. Will be deleted.
 */
void deleteDir(const std::string& pathname);

size_t getSize(const std::string& pathname);

} // namespace

#endif /* MAIN_MISC_FILEUTIL_H_ */

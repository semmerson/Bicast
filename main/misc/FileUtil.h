/**
 * Utility functions for files.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: FileUtil.h
 *  Created on: Jan 2, 2020
 *      Author: Steven R. Emmerson
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
        const mode_t       mode = 0700);

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
        const mode_t       mode = 0700);

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

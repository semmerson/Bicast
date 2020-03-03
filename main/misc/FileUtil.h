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

std::string filename(const std::string& pathname);

std::string dirPath(const std::string& pathname);

void ensureDir(
        const std::string& pathname,
        const mode_t       mode);

/**
 * Prunes a directory. The given directory hierarchy is recursively
 * traversed depth first. All empty directories are deleted.
 *
 * @param[in] pathname  Root of directory hierarchy. Will be deleted if
 *                      empty after recursive traversal.
 */
void pruneDir(const std::string& pathname);

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

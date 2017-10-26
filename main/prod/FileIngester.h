/**
 * This file implements a file-based ingester of data-products. One that
 * uses inotify(7) to monitor a directory hierarchy and return all existing,
 * newly-closed, and moved-in files as data-products.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: FileIngester.h
 *  Created on: Oct 24, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PROD_FILEINGESTER_H_
#define MAIN_PROD_FILEINGESTER_H_

#include "Ingester.h"

#include <string>

namespace hycast {

class FileIngester : public Ingester
{
    class Impl;

public:
    FileIngester();

    /**
     * Constructs.
     * @param[in] dirPathname  Pathname of directory to monitor
     * @throw SystemError      `::opendir()` failure on `dirPathName`
     * @throw SystemError      `::inotify_init1()` failure
     * @throw SystemError      `::inotify_add_watch()` failure
     */
    explicit FileIngester(const std::string& dirPathname);
};

} // namespace

#endif /* MAIN_PROD_FILEINGESTER_H_ */

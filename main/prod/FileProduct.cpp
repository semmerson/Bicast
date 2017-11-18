/**
 * This file 
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: FileProduct.cpp
 *  Created on: Oct 27, 2017
 *      Author: Steven R. Emmerson
 */

#include "config.h"





#if 0
    /**
     * Constructs from a file.
     * @param[in] pathname  Pathname of the file
     * @param[in] index     Product index
     */
    Impl(   const std::string& pathname,
            const ProdIndex    index)
        : Impl{ProdInfo(pathname, index)}
    {
        numChunks = prodInfo.getNumChunks();
        complete = true;
        auto fd = ::open(pathname, O_RDONLY);
        if (fd == -1)
            throw SYSTEM_ERROR("open() failure on \"" + pathname + "\"");
        auto status = ::read(fd, data, prodInfo.getSize());
        if (status) {
            ::close(fd);
            throw SYSTEM_ERROR(std::string{"read() failure on \""} + pathname +
                    "\"");
        }
        ::close(fd);
    }
#endif

#if 0
Product::Product(
        const std::string& pathname,
        const ProdIndex    index)
    : pImpl{new Impl(pathname, index)}
{}
#endif

/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetAddrImpl.h
 * @author: Steven R. Emmerson
 *
 * This file defines an abstract base class for an Internet address.
 */

#ifndef INETADDRIMPL_H_
#define INETADDRIMPL_H_

#include "InetAddr.h"

#include <cstddef>
#include <memory>
#include <string>

namespace hycast {

class InetAddrImpl {
public:
    static std::shared_ptr<InetAddrImpl>
                        create();
    static std::shared_ptr<InetAddrImpl>
                        create(const std::string ipAddr);
    virtual             ~InetAddrImpl() {};
    virtual int         get_family() const = 0;
    virtual size_t      hash() const = 0;
    int                 compare(const InetAddrImpl& that) const;
    int                 equals(const InetAddrImpl& that) const {
                            return compare(that) == 0;
                        }
    virtual std::string to_string() const = 0;
};

} // namespace

#endif /* INETADDRIMPL_H_ */

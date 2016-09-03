/**
 * This file declares an abstract base class for an immutable Internet address.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetAddrImpl.h
 * @author: Steven R. Emmerson
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
    /**
     * Factory method that returns a default instance.
     * @return A default instance
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    static std::shared_ptr<InetAddrImpl>
                        create();
    /**
     * Factory method that returns a new instance based on the string representation
     * of an Internet address.
     * @param[in] ip_addr  The string representation of an Internet address.
     * @return A new instance
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    static std::shared_ptr<InetAddrImpl>
                        create(const std::string ipAddr);
    /**
     * Destructor.
     */
    virtual             ~InetAddrImpl() {};
    /**
     * Returns the address family.
     * @retval AF_INET   IPv4
     * @retval AF_INET6  IPv6
     * @exceptionsafety Nothrow
     */
    virtual int         get_family() const noexcept = 0;
    /**
     * Returns the hash code for this instance.
     * @return Hash code for this instance
     * @exceptionsafety Nothrow
     */
    virtual size_t      hash() const noexcept = 0;
    /**
     * Compares this instance with another.
     * @param[in] that  Other instance
     * @retval <0  This instance is less than the other
     * @retval  0  This instance is equal to the other
     * @retval >0  This instance is greater than the other
     * @exceptionsafety Nothrow
     */
    int                 compare(const InetAddrImpl& that) const noexcept;
    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  Other instance
     * @retval `true`   This instance is equal to the other
     * @retval `false`  This instance isn't equal to the other
     * @exceptionsafety Nothrow
     */
    int                 equals(const InetAddrImpl& that) const noexcept {
                            return compare(that) == 0;
                        }
    /**
     * Returns the string representation of the Internet address.
     * @return The string representation of the Internet address
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    virtual std::string to_string() const = 0;
};

} // namespace
/**
 * Returns a string representation of this instance.
 * @return A string representation of this instance
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */

#endif /* INETADDRIMPL_H_ */

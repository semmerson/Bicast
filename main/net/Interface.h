/**
 * This file declares a network interface.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 * Interface.h
 *
 *  Created on: May 8, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_NET_INTERFACE_H_
#define MAIN_NET_INTERFACE_H_

#include "InetAddr.h"

#include <memory>
#include <string>
#include <sys/socket.h>

namespace hycast {

class Interface final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    Interface() =default;

    /**
     * Constructs.
     * @param[in] name  Name of the interface (e.g., "eth0")
     */
    Interface(const std::string& name);

    /**
     * Constructs.
     * @param[in] index  Index of the interface (e.g., 2)
     */
    Interface(const int index);

    /**
     * Returns an Internet address of the interface.
     * @param[in] family       Address family. One of `AF_INET` or `AF_INET6`.
     * @return                 Corresponding Internet address of interface
     * @throw InvalidArgument  `family` is invalid
     * @throw SystemError      Error getting information on interfaces
     * @throw RuntimeError     Couldn't construct Internet address
     * @throw LogicError       No information on interface
     * @exceptionsafety        Strong guarantee
     * @threadsafety           Probably compatible but not safe
     */
    InetAddr getInetAddr(const int family) const;
};

} // namespace

#endif /* MAIN_NET_INTERFACE_H_ */

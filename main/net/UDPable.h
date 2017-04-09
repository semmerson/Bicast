/**
 * This file declares an interface for classes that can be transmitted in a
 * UDP packet.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: UDPable.h
 * @author: Steven R. Emmerson
 */

#ifndef UDPABLE_H_
#define UDPABLE_H_

#include "UdpSock.h"

#include <cstddef>
#include <cstdint>

namespace hycast {

class UDPable {
protected:
    static uint_fast16_t protoVers; /// Protocol version

public:
    virtual ~UDPable() {}

    /**
     * Transmits this instance on a UDP socket.
     * @param[in]  sock  UDP socket on which to transmit this instance
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    virtual char* transmit(UdpSock& sock) =0;

    /**
     * Returns the size, in bytes, of a serialized representation of this
     * instance.
     * @param[in] version  Protocol version
     * @return the size, in bytes, of a serialized representation of this
     *         instance
     * @exceptionsafety Nothrow
     * @threadsafety    Safe
     */
    virtual size_t getSerialSize(unsigned version) const noexcept =0;
};

} // namespace

#endif /* UDPABLE_H_ */

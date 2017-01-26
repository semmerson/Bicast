/**
 * This file declares an implementation of an I/O channel.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChannelImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef CHANNELIMPL_H_
#define CHANNELIMPL_H_

#include "ChunkInfo.h"
#include "ProdIndex.h"
#include "ProdInfo.h"
#include "Serializable.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include "../net/SctpSock.h"

namespace hycast {

class ChannelImpl {
protected:
    SctpSock             sock;
    unsigned           streamId;
    unsigned           version;
public:
    ChannelImpl(
            SctpSock&            sock,
            const unsigned     streamId,
            const unsigned     version);
    /**
     * Returns the associated SCTP socket.
     * @returns the associated SCTP socket
     */
    SctpSock& getSocket() {
        return sock;
    }
    /**
     * Returns the SCTP stream ID of the current, incoming message. Waits for
     * the message if necessary.
     * @return the SCTP stream ID of the current message
     */
    unsigned getStreamId() {
        return sock.getStreamId();
    }
    /**
     * Returns the amount of available input in bytes.
     * @return The amount of available input in bytes
     */
    size_t getSize() {
        return static_cast<size_t>(sock.getSize());
    }
};

} // namespace

#endif /* CHANNELIMPL_H_ */

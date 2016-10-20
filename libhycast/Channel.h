/**
 * This file declares an interface for an I/O channel.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Channel.h
 * @author: Steven R. Emmerson
 */

#ifndef CHANNEL_H_
#define CHANNEL_H_

#include "Socket.h"

#include <cstddef>
#include <memory>

namespace hycast {

class ChannelImpl; // Forward declaration of implementation

class Channel {
public:
    virtual ~Channel() {};
    /**
     * Returns the associated SCTP socket.
     * @returns the associated SCTP socket
     */
    virtual Socket& getSocket() const =0;
    /**
     * Returns the SCTP stream ID of the current message. Waits for the message
     * if necessary. The message is left in the input buffer.
     * @return the SCTP stream ID of the current message
     */
    virtual unsigned getStreamId() const =0;
    /**
     * Returns the size of the current message in bytes.
     * @return The size of the current message in bytes
     */
    virtual size_t getSize() const =0;
};

} // namespace

#endif /* CHANNEL_H_ */

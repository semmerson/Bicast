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

#include "ChunkInfo.h"
#include "ProdInfo.h"
#include "Serializable.h"

#include <sys/uio.h>
#include <memory>

namespace hycast {

template <class U> class ChannelImpl; // Forward declaration of implementation

template <class T>
class Channel {
    std::shared_ptr<ChannelImpl<T>> pImpl;
public:
    /**
     * Constructs from an SCTP socket, a stream identifier, and a protocol
     * version.
     * @param[in] sock      SCTP Socket
     * @param[in] streamId  Stream identifier
     * @param[in] version   Protocol version
     */
    Channel(
            Socket&            sock,
            const unsigned     streamId,
            const unsigned     version);
    /**
     * Returns the associated SCTP socket.
     * @returns the associated SCTP socket
     */
    Socket& getSocket() const;
    /**
     * Returns the SCTP stream ID of the current message. Waits for the message
     * if necessary. The message is left in the input buffer.
     * @return the SCTP stream ID of the current message
     */
    unsigned getStreamId() const;
    /**
     * Sends a serializable object.
     * @param[in] obj  Serializable object
     */
    void send(const Serializable& obj) const;
    /**
     * Returns the object contained in the current message.
     * @return the object contained in the current message
     */
    std::shared_ptr<T> recv();
    /**
     * Returns the product-information contained in the current message.
     * @return the product-information contained in the current message
     */
    std::shared_ptr<ProdInfo> recvProdInfo();
    /**
     * Returns the chunk-information contained in the current message.
     * @return the chunk-information contained in the current message
     */
    std::shared_ptr<ChunkInfo> recvChunkInfo();
    /**
     * Returns the size of the current message in bytes.
     * @return The size of the current message in bytes
     */
    size_t getSize() const;
};

} // namespace

#endif /* CHANNEL_H_ */

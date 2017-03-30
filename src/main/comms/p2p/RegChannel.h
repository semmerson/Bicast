/**
 * This file declares an I/O channel for `Serializable` objects.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: RegChannel.h
 * @author: Steven R. Emmerson
 */

#ifndef REGCHANNEL_H_
#define REGCHANNEL_H_

#include "Serializable.h"

#include <memory>
#include "../p2p/Channel.h"

namespace hycast {

template <class V> class RegChannelImpl; // Forward declaration of implementation

template <class T>
class RegChannel final : public Channel {
    std::shared_ptr<RegChannelImpl<T>> pImpl;
public:
    /**
     * Constructs from nothing. Any attempt to use the resulting object will
     * throw an exception.
     */
    RegChannel() = default;
    /**
     * Constructs from an SCTP socket, a stream identifier, and a protocol
     * version.
     * @param[in] sock      SCTP Socket
     * @param[in] streamId  Stream identifier
     * @param[in] version   Protocol version
     */
    RegChannel(
            SctpSock&          sock,
            const unsigned     streamId,
            const unsigned     version);
    /**
     * Returns the associated SCTP socket.
     * @returns the associated SCTP socket
     */
    SctpSock& getSocket() const;
    /**
     * Returns the SCTP stream ID of the current message. Waits for the message
     * if necessary. The message is left in the input buffer.
     * @return the SCTP stream ID of the current message
     */
    unsigned getStreamId() const;
    /**
     * Returns the size of the current message in bytes.
     * @return The size of the current message in bytes
     */
    size_t getSize() const;
    /**
     * Sends a serializable object.
     * @param[in] obj  Serializable object
     * @throws std::system_error if an I/O error occurred
     * @execptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void send(const T& obj) const;
    /**
     * Returns the serialized object in the current message.
     * @return the serialized object in the current message
     */
    typename std::result_of<decltype(&T::deserialize)
            (const char*, size_t, unsigned)>::type recv();
};

} // namespace

#endif /* REGCHANNEL_H_ */

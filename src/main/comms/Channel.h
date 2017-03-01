/**
 * This file declares a handle for a Peer's type-specific I/O channel.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Channel.h
 * @author: Steven R. Emmerson
 */

#ifndef CHANNEL_H_
#define CHANNEL_H_

#include "SctpSock.h"

#include <cstddef>
#include <memory>

namespace hycast {

/**
 * A Peer's type-specific I/O channel.
 * @tparam S  Type of sent object
 * @tparam R  Type of received object
 */
template<class S, class R = S>
class Channel final
{
protected:
    class                 Impl;     // Forward declaration of implementation

    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     * @param[in] sock      SCTP socket
     * @param[in] streamId  SCTP stream identifier
     * @param[in] version   Protocol version
     */
    Channel(SctpSock&      sock,
            const unsigned streamId,
            const unsigned version);

    /**
     * Returns the associated SCTP socket.
     * @returns the associated SCTP socket
     */
    SctpSock& getSocket() const;

    /**
     * Returns the size of the current message in bytes.
     * @return The size of the current message in bytes
     */
    size_t getSize() const;

    /**
     * Sends a value.
     * @param[in] value  Value to be sent
     * @throws std::system_error if an I/O error occurred
     * @execptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void send(const unsigned short value) const;

    /**
     * Sends a value.
     * @param[in] value  Value to be sent
     * @throws std::system_error if an I/O error occurred
     * @execptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void send(const unsigned value) const;

    /**
     * Sends an object.
     * @param[in] obj  Object to be sent
     * @throws std::system_error if an I/O error occurred
     * @execptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void send(S& obj) const;

    /**
     * Returns the value in the current message. Waits for a value if necessary.
     * @return Value in the current message
     */
    void recv(unsigned& value) const;

    /**
     * Returns the object in the current message. Waits for an object if
     * necessary.
     * @return Object in the current message
     */
    R recv() const;
};

} // namespace

#endif /* CHANNEL_H_ */

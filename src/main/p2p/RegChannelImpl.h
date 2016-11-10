/**
 * This file declares the implementation of an I/O channel for serializable
 * objects.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: RegChannelImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef REGCHANNELIMPL_H_
#define REGCHANNELIMPL_H_

#include "ChannelImpl.h"
#include "Serializable.h"
#include "Socket.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace hycast {

template <class T>
class RegChannelImpl final : public ChannelImpl {
public:
    /**
     * Constructs from nothing. Any attempt to use the resulting object will
     * throw an exception.
     */
    RegChannelImpl() = default;
    /**
     * Constructs from an SCTP socket, SCTP stream identifier, and protocol
     * version.
     * @param[in] sock      SCTP socket
     * @param[in] streamId  SCTP stream ID
     * @param[in] version   Protocol version
     */
    RegChannelImpl(
            Socket&            sock,
            const unsigned     streamId,
            const unsigned     version);
    /**
     * Sends a serializable object.
     * @param[in] obj  Serializable object.
     * @throws std::system_error if an I/O error occurred
     * @exceptionsafety  Basic
     * @threadsafety     Compatible but not safe
     */
    void send(const Serializable& obj);
    /**
     * Returns the object in the current message.
     * @return the object in the current message
     */
    typename std::result_of<decltype(&T::deserialize)
            (const char*, size_t, unsigned)>::type recv();
};

} // namespace

#endif /* REGCHANNELIMPL_H_ */

/**
 * This file declares a server-side SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ClientSocket.h
 * @author: Steven R. Emmerson
 */

#ifndef SERVERSOCKET_H_
#define SERVERSOCKET_H_

#include "InetSockAddr.h"
#include "SctpSock.h"

#include <memory>

namespace hycast {

class SrvrSctpSock final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs. On return
     * - `getSock()` will return `-1`
     * - `getNumStreams() will return `0`
     * - `accept()` will throw an exception
     */
    SrvrSctpSock();

    /**
     * Constructs from an Internet socket address and the number of SCTP
     * streams.
     * @param[in] addr        Internet socket address on which to listen
     * @param[in] numStreams  Number of SCTP streams
     * @param[in] queueSize   Length of `accept()` queue
     * @throw RuntimeError    Couldn't construct server-size SCTP socket
     */
    SrvrSctpSock(
            const InetSockAddr& addr,
            const int           numStreams = 1,
            const int           queueSize = 5);

    /**
     * Copy assigns.
     * @param[in] rhs  Other instance
     * @return         This instance
     */
    SrvrSctpSock& operator=(const SrvrSctpSock& rhs);

    /**
     * Indicates if this instance is considered equal to another instance.
     * @param[in] that  Other instance
     * @retval `true`   The instances are equal
     * @retval `false`  The instances are not equal
     */
    bool operator==(const SrvrSctpSock& that) const noexcept;

    /**
     * Returns the underlying BSD socket.
     * @return Underlying BSD socket
     */
    int getSock() const noexcept;

    /**
     * Returns the number of SCTP streams.
     * @return Number of SCTP streams
     */
    int getNumStreams() const noexcept;

    /**
     * Accepts an incoming connection on the socket.
     * @return The accepted connection
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    SctpSock accept() const;

    std::string to_string() const;
};

} // namespace

#endif /* SERVERSOCKET_H_ */

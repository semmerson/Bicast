/**
 * This file declares a handle class for an SCTP socket.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYIING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: SctpSocket.h
 * @author: Steven R. Emmerson
 */

#ifndef SCTPSOCK_H_
#define SCTPSOCK_H_

#include "InetSockAddr.h"

#include <memory>

namespace hycast {

/**
 * Abstract base class for an SCTP socket.
 */
class BaseSctpSock
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    /**
     * Constructs from the implementation of a derived class.
     * @param[in] impl  Implementation of a derived class
     */
    explicit BaseSctpSock(Impl* impl);

public:
    /**
     * Creates an SCTP-compatible BSD socket.
     * @return SCTP-compatible socket descriptor
     * @throw SystemError  Socket couldn't be created
     */
    static int createSocket();

    /**
     * Default constructs.
     */
    BaseSctpSock() =default;

    /**
     * Destroys. Closes the underlying BSD socket if this instance holds the
     * last reference to it.
     */
    virtual ~BaseSctpSock() noexcept =0;

    /**
     * Copy assigns.
     * @param[in] rhs  Other instance
     * @return         This instance
     */
    BaseSctpSock& operator=(const BaseSctpSock& rhs);

    /**
     * Returns the number of SCTP streams.
     * @return the number of SCTP streams
     */
    uint16_t getNumStreams() const;

    /**
     * Returns the size of the send buffer.
     * @return  Size of the send buffer in bytes
     */
    int getSendBufSize() const;

    /**
     * Sets the size of the send buffer.
     * @param[in] size     Send buffer size in bytes
     * @return             Reference to this instance
     * @throw SystemError  Size couldn't be set
     */
    BaseSctpSock& setSendBufSize(const int size);

    /**
     * Returns the size of the receive buffer.
     * @return  Size of the receive buffer in bytes
     */
    int getRecvBufSize() const;

    /**
     * Sets the size of the receive buffer.
     * @param[in] size     Receive buffer size in bytes
     * @return             Reference to this instance
     * @throw SystemError  Size couldn't be set
     */
    BaseSctpSock& setRecvBufSize(const int size);

    /**
     * Returns the socket descriptor.
     * @return socket descriptor
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    int getSock() const noexcept;

    /**
     * Indicates if this instance equals another.
     * @param[in] that  Other instance
     * @retval `true`   This instance equals the other
     * @retval `false`  This instance doesn't equal the other
     * @exceptionsafety Nothrow
     */
    bool operator==(const BaseSctpSock& that) const noexcept;

    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    std::string to_string() const;
};

/******************************************************************************/

/**
 * An established SCTP socket (i.e., one with an SCTP association).
 */
class SctpSock final : public BaseSctpSock
{
    class Impl;

public:
    /**
     * Default constructs.
     */
    SctpSock();

    /**
     * Constructs an SCTP socket from the client side. The caller must
     * eventually call `connect()` to establish an SCTP association. `connect()`
     * is not called here to allow socket configurations that must occur before
     * that function is called (e.g., setting buffer sizes).
     * @param[in] numStreams   Number of SCTP streams
     * @return                 Corresponding SCTP socket
     * @throw InvalidArgument  `numStreams <= 0`
     * @throw SystemError      Socket couldn't be created
     * @throw SystemError      Required memory couldn't be allocated
     * @see `connect(InetSockAddr& addr)`
     * @see `setSendBufSize()`
     * @see `setRecvBufSize()`
     */
    explicit SctpSock(const int numStreams);

    /**
     * Constructs an SCTP socket from the server side.
     * @param[in] sd         SCTP socket descriptor from `accept()`
     * @param[in] addr       Address of remote SCTP socket
     * @param[in] numStream  Number of SCTP streams
     * @throws SystemError   Required memory can't be allocated
     */
    SctpSock(
            const int              sd,
            const struct sockaddr& addr,
            const int              numStreams);

    /**
     * Destroys. Closes the underlying BSD socket if this instance holds the
     * last reference to it.
     */
    ~SctpSock() noexcept =default;

    /**
     * Copy assigns.
     * @param[in] rhs  Other instance
     * @return         This instance
     */
    SctpSock& operator=(const SctpSock& rhs);

    /**
     * Connects to a server. This function is separate from the constructor to
     * allow socket configurations that must occur before `::connect()` is
     * called (e.g., setting buffer sizes). Blocks until the association is
     * established.
     * @return              This instance
     * @throws SystemError  Connection failure
     * @exceptionsafety     Strong
     * @threadsafety        Safe
     * @see `Impl(int numStreams)`
     * @see `setSendBufSize()`
     * @see `setRecvBufSize()`
     */
    SctpSock& connect(const InetSockAddr& addr);

    /**
     * Returns the Internet socket address of the remote end.
     * @return Internet socket address of the remote end
     */
    InetSockAddr getRemoteAddr() const;

    /**
     * Sends a message.
     * @param[in] streamId  SCTP stream number
     * @param[in] msg       Message to be sent
     * @param[in] len       Size of message in bytes
     * @throws std::system_error if an I/O error occurred
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not safe
     */
    void send(
            const unsigned streamId,
            const void*    msg,
            const size_t   len) const;

    /**
     * Sends a message. This is a cancellation point.
     * @param[in] streamId  SCTP stream number
     * @param[in] iovec     Vector comprising message to send
     * @param[in] iovcnt    Number of elements in `iovec`
     */
    void sendv(
            const unsigned      streamId,
            const struct iovec* iovec,
            const int           iovcnt) const;

    /**
     * Returns the SCTP stream number of the current message. Waits for the
     * message if necessary. The message is left in the socket's input buffer.
     * @returns SCTP stream number
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     */
    unsigned getStreamId() const;

    /**
     * Returns the size, in bytes, of the current SCTP message. Waits for the
     * message if necessary. The message is left in the socket's input buffer.
     * @returns Size of message in bytes. Will equal 0 when socket is closed by
     *          remote peer.
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     */
    uint32_t getSize() const;

    /**
     * Receives a message.
     * @param[out] msg     Receive buffer
     * @param[in]  len     Size of receive buffer in bytes
     * @param[in]  flags   Type of message reception. Logical OR of zero or
     *                     more of
     *                       - `MSG_OOB`  Requests out-of-band data
     *                       - `MSG_PEEK` Peeks at the incoming message
     * @throws std::system_error on I/O failure or if number of bytes read
     *     doesn't equal `len`
     */
    void recv(
            void*        msg,
            const size_t len,
            const int    flags = 0) const;

    /**
     * Receives a message.
     * @param[in] iovec     Vector comprising message to receive
     * @param[in] iovcnt    Number of elements in `iovec`
     * @param[in] flags     Type of message reception. Logical OR of zero or
     *                      more of
     *                      - `MSG_OOB`  Requests out-of-band data
     *                      - `MSG_PEEK` Peeks at the incoming message
     * @return              Number of bytes actually read
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety Safe
     */
    size_t recvv(
            const struct iovec* iovec,
            const int           iovcnt,
            const int           flags = 0) const;

    /**
     * Indicates if this instance has a current message.
     * @retval true   Yes
     * @retval false  No
     */
    bool hasMessage() const;

    /**
     * Discards the current message.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    void discard() const;
};

/******************************************************************************/

/**
 * Server-side SCTP socket. One that listens for incoming association attempts.
 */
class SrvrSctpSock final : public BaseSctpSock
{
    class                 Impl;

public:
    /**
     * Default constructs. On return
     * - `getSock()` will return `-1`
     * - `getNumStreams() will return `0`
     * - `accept()` will throw an exception
     */
    SrvrSctpSock();

    /**
     * Constructs. The socket is bound to the Internet socket address but
     * `listen()` is not called to allow certain options to be set (e.g., buffer
     * sizes).
     * @param[in] addr        Internet socket address on which to listen
     * @param[in] numStreams  Number of SCTP streams
     * @throw RuntimeError    Couldn't construct server-size SCTP socket
     * @see   `listen()`
     */
    SrvrSctpSock(
            const InetSockAddr& addr,
            const int           numStreams = 1);

    /**
     * Copy assigns.
     * @param[in] rhs  Other instance
     * @return         This instance
     */
    SrvrSctpSock& operator=(const SrvrSctpSock& rhs);

    /**
     * Configures the socket for accepting incoming association attempts.
     * @param[in] queueSize  Size of the backlog queue
     * @throw LogicError     Socket already configured for listening
     * @throw SystemError    `::listen()` failure
     */
    void listen(const int queueSize = 5) const;

    /**
     * Accepts an incoming connection on the socket.
     * @return          The accepted connection
     * @exceptionsafety Basic guarantee
     * @threadsafety    Thread-compatible but not thread-safe
     */
    SctpSock accept() const;
};

} // namespace

namespace std {
    inline string to_string(const hycast::BaseSctpSock& sock) {
        return sock.to_string();
    }
}

#endif

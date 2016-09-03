/**
 * This file declares a RAII object for a socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYIING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Socket.h
 * @author: Steven R. Emmerson
 */

#ifndef SOCKET_H_
#define SOCKET_H_

#include <memory>
#include <string>

namespace hycast {

class Socket final {
    int                  sock;
    std::shared_ptr<int> sptr;
    /**
     * Unconditionally assigns another instance to this instance. Closes this
     * instance's socket before the assignment if this instance holds the last
     * reference to it.
     * @param[in] that  The other instance
     * @exceptionsafety Nothrow
     */
    void copy_assign(const Socket& that) noexcept;
public:
    /**
     * Constructs from nothing.
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    Socket();
    /**
     * Constructs from a socket. Only do this once per socket because the destructor
     * might close the socket.
     * @see Socket::~Socket()
     * @see Socket::operator=(Socket& socket)
     * @see Socket::operator=(Socket&& socket)
     * @param[in] sock  The socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @throws std::invalid_argument if `sock < 0`
     */
    Socket(const int sock);
    /**
     * Constructs from another instance. The returned instance will close the socket
     * upon destruction if it's the last one that references the socket.
     * @see Socket::~Socket()
     * @see Socket::operator=(Socket& socket)
     * @see Socket::operator=(Socket&& socket)
     * @param[in] socket  The other instance.
     * @exceptionsafety Nothrow
     */
    Socket(const Socket& socket) noexcept;
    /**
     * Constructs from a temporary instance. Move constructor. The returned instance
     * will close the socket upon destruction if it's the last one that references
     * the socket.
     * @see Socket::~Socket()
     * @see Socket::operator=(Socket& socket)
     * @see Socket::operator=(Socket&& socket)
     * @param[in] socket  The other instance.
     * @exceptionsafety Nothrow
     */
    Socket(const Socket&& socket) noexcept;
    /**
     * Destroys an instance. Closes the underlying socket if this is the last
     * instance that references it.
     * @exceptionsafety Nothrow
     * @see Socket::Socket(int sock)
     * @see Socket::Socket(Socket& socket)
     * @see Socket::Socket(Socket&& socket)
     * @see Socket::operator=(Socket& socket)
     * @see Socket::operator=(Socket&& socket)
     */
    ~Socket();
    /**
     * Assigns from another instance. The returned instance will close the socket
     * upon destruction if it's the last one that references the socket.
     * @see Socket::Socket(int sock)
     * @see Socket::Socket(Socket& socket)
     * @see Socket::Socket(Socket&& socket)
     * @see Socket::~Socket()
     * @param[in] that  The other instance
     * @return This instance
     * @exceptionsafety Nothrow
     */
    Socket& operator=(const Socket& socket) noexcept;
    /**
     * Assigns from another instance. The returned instance will close the socket
     * upon destruction if it's the last one that references the socket.
     * @see Socket::Socket(int sock)
     * @see Socket::Socket(Socket& socket)
     * @see Socket::Socket(Socket&& socket)
     * @see Socket::~Socket()
     * @param[in] that  The other instance
     * @return This instance
     * @exceptionsafety Nothrow
     */
    Socket& operator=(const Socket&& socket) noexcept;
    /**
     * Indicates if this instance equals another.
     * @param[in] that  Other instance
     * @retval `true`   This instance equals the other
     * @retval `false`  This instance doesn't equal the other
     * @exceptionsafety Nothrow
     */
    bool operator==(const Socket& that) const noexcept {
        return sock == that.sock;
    }
    /**
     * Returns a string representation of this instance's socket.
     * @return String representation of this instance's socket
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    std::string to_string() const {
        return std::to_string(sock);
    }
};

} // namespace

#endif
